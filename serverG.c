#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "tessera_bibliotecaria.h"
#include "settings_Client_Server.h"



void connessioni(void *arg);//funzione che gestisce una singola connessione al client
void submitTask(Task);//inserisce una task nella coda
void executeTask(Task*);//esegue una task
void *routine(void* arg);//funzione che eseguono i thread del thread pool
void sigint_handler(int);//gestore del segnale SIGINT(CTRL+C)

volatile sig_atomic_t running = 1; //indica se il server deve continuare a girare
Task taskQueue[256]; //coda dei task da eseguire
int task_count = 0; //numero corrente di task nella coda
int sockSG; //socket del serverG
/*strutture per la sincronizzazione tra thread*/
pthread_mutex_t mutexQueue;
pthread_cond_t condQueue; 

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);//associa il gestore del segnale
    pthread_t thread_pool[THREAD_NUM];
    /*vengono inzializzati: il mutex e la variabile di condizione*/
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    sockSG = socket(AF_INET, SOCK_STREAM, 0); //crea il socket per il serverG
    if(sockSG < 0){
        perror("[+]Error in socket");
        exit(1);
    }
    //configurazione dell'indirizzo del serverG
    struct sockaddr_in indSG; 
    indSG.sin_family = AF_INET;
    indSG.sin_port = htons(SERVERG);
    indSG.sin_addr.s_addr = htonl(INADDR_ANY);
//associa il socket all'indirizzo
    if(bind(sockSG, (struct sockaddr*)&indSG, sizeof(indSG)) < 0){
        perror("[+]Error in bind");
        close(sockSG);
        exit(1);
    }

    if(listen(sockSG, SOMAXCONN) < 0){//server in ascolto 
        perror("[+]Error in listen");
        close(sockSG);
        exit(1);
    }
    int i;
    for(i = 0; i<THREAD_NUM; i++){ //crea il threadpool
        if(pthread_create(&thread_pool[i], NULL, &routine, NULL) != 0){
            perror("[+]Error in pthread_create");
        }

    }

    while(running){//accetta connessioni e crea task 
        struct sockaddr_in indClient;
        socklen_t client_len = sizeof(indClient);
        int client_sock = accept(sockSG, (struct sockaddr*)&indClient, &client_len);
        
        if(client_sock < 0){
            if(!running) break;//se è stato ricevuto SIGINT, esce dal ciclo
            perror("[+]Error in accept");
            continue;
        }
        int *cptr = malloc(sizeof(int)); //alloca memoria per il descrittore del client
        *cptr = client_sock;
        Task t = { //crea una task e la inserisce nella coda
            .taskFunction = &connessioni,
            .arg = cptr
        };
        submitTask(t);
    }
//attende la termiazione dei thread
    for(i = 0; i<THREAD_NUM; i++){
        if(pthread_join(thread_pool[i], NULL) != 0){
            perror("[+]Error in pthread_join");
        }

    }
    //pulisce le risorse
    close(sockSG);
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    return 0;
}

void connessioni(void *arg){
    
    int clientFD = *((int *)arg);//cast e deferenziazione dell'argomento che e' il socket del client
    struct TesseraBibliotecaria tb;
    if(recv(clientFD, &tb, sizeof(tb), 0) < 0){//riceve la tessera dal client
    perror("[+]Error in recv");
    close(clientFD);
    free(arg);
    return;}

    int sockSB = socket(AF_INET, SOCK_STREAM, 0); //crea il socket per connettersi a serverB
    if(sockSB < 0){
        perror("[+]Error in socket");
        free(arg);
        return;
    } 
    //configura indirizzo di destinazione(serverB)
    struct sockaddr_in indSB;
    indSB.sin_addr.s_addr = htonl(INADDR_ANY);//accetta qualsiasi interfaccia locale
    indSB.sin_port = htons(SERVERB);//porta
    indSB.sin_family = AF_INET;//indirizzo IPV4

    if(connect(sockSB, (struct sockaddr*)&indSB, sizeof(indSB)) < 0){//connessione al serverB
        perror("[+]Error in connect");
        close(sockSB);
        free(arg);
        return;
    }
    
    if(send(sockSB, &tb, sizeof(tb), 0) < 0){//invia la tessera al serverB
        perror("[+]Error in send");
        close(sockSB);
        free(arg);
        return;
    }

    int risposta; //riceve la risposta da serverB
    if(recv(sockSB, &risposta, sizeof(risposta), 0) < 0){
        perror("[+]Error in recv");
        close(sockSB);
        free(arg);
        return;
    }
//invia la risposta al client
    if(send(clientFD, &risposta, sizeof(risposta), 0) < 0){
        perror("[+]Error in send");
        close(clientFD);
        free(arg);
        return;
    } 
//chiude le connessioni e libera la memoria
    close(clientFD);
    close(sockSB);
    free(arg);
}

void submitTask(Task task){
    pthread_mutex_lock(&mutexQueue);
    taskQueue[task_count] = task; //aggiunge la task in coda
    task_count++;
    pthread_cond_signal(&condQueue); //segnala a un thread che è disponibile un nuovo task
    pthread_mutex_unlock(&mutexQueue);
}
/*+++ esegue la task richiamando la funzione passata nel campo taskFunction +++*/
void executeTask(Task * task){
    task->taskFunction(task->arg); //chiama la funzione del task con il suo argomento
}
/* +++ routine dei thread: eseguita da ogni thread
- attende una task disponibile nella coda (usando pthread_cond_wait e la variabile di condizione)
- rimuove la prima task e lo esegue */
void *routine(void * arg){
    while(running){
        Task task;
        pthread_mutex_lock(&mutexQueue);
        while(task_count == 0 && running){
            pthread_cond_wait(&condQueue, &mutexQueue); //attende finche' non arriva un task
        }
        if(!running){
            pthread_mutex_unlock(&mutexQueue);
            break;
        }
        task = taskQueue[0]; //estrae la prima task dalla coda(FIFO)
        int i;
        for(i = 0; i<task_count-1; i++){
            taskQueue[i]=taskQueue[i+1];
        }
        task_count--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);//esegue la task estratta
    }
    return NULL;
}
void sigint_handler(int sig){
    running = 0; //imposta il flag a 0 per fermare il ciclo principale e i thread
    close(sockSG); // chiude la socket per forzare l'uscita da accept()
    pthread_cond_broadcast(&condQueue); //sveglia eventuali thread in attesa 
}