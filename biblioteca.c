#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "tessera_bibliotecaria.h"
#include "settings_Client_Server.h"

void connessioni(void *arg);//funzione che gestisce una singola connessione al client
void submitTask(Task);//inserisce una task nella coda
void executeTask(Task*);//esegue una task
void *routine(void* arg);//funzione che eseguono i thread del thread pool
void sigint_handler(int);//gestore del segnale SIGINT(CTRL+C)

pthread_mutex_t mutexBib;//mutex per proteggere l'accesso alla coda taskQueue
pthread_cond_t condBib;//variabile di condizione per sincronizzare i thread e il main thread
Task taskQueue[256];//coda di task(max 256)
int task_count = 0;//numero di task presenti in coda
volatile sig_atomic_t running = 1; //indica se il server deve continuare a girare
int socketBiblioteca;//socket principale del server biblioteca


int main(int argc, char const *argv[])
{
    int i;
    signal(SIGINT, sigint_handler);//collega il gestore per il segnale SIGINT
    pthread_t th[THREAD_NUM];
    struct sockaddr_in bibliotecaInd;
    //inziailizza: mutex e variabile di condizione
    pthread_mutex_init(&mutexBib, NULL);
    pthread_cond_init(&condBib, NULL);
    socketBiblioteca = socket(AF_INET, SOCK_STREAM, 0);

    if(socketBiblioteca < 0){//crea il socket TCP IPV4
        perror("[+]Error in socket");
        exit(1);
    }
/*definizione dell'indirizzo del server*/
    bibliotecaInd.sin_addr.s_addr = htonl(INADDR_ANY);//accetta connessioni da qualsiai IP
    bibliotecaInd.sin_family = AF_INET; //indirizzo IPV4
    bibliotecaInd.sin_port = htons(BIBLIOTECA);//porta del server biblioteca

    if(bind(socketBiblioteca, (struct sockaddr*)&bibliotecaInd, sizeof(bibliotecaInd))<0){//associa il socket all'indirizzo e porta
        perror("[+]Error in bind");
        exit(1);
    }

    if(listen(socketBiblioteca, SOMAXCONN)<0){//pone il socket in ascolto 
        perror("[+]Error in listen");
        exit(EXIT_FAILURE);
    }

    for(i = 0; i<THREAD_NUM; i++){//creazione del pool 

        
        if(pthread_create(&th[i], NULL, &routine, NULL) != 0){
            perror("[+]Error in pthread_create");
        }
    }
        while(running){//accetta connessioni in arrivo e crea task per gestirle
        struct sockaddr_in indClient;
        socklen_t client_len = sizeof(indClient);
        int client_sock = accept(socketBiblioteca, (struct sockaddr*)&indClient, &client_len);
        if(client_sock < 0){
            if(!running) break; //se il server e' in chiusura esce dal ciclo
            perror("[+]Error in accept");
            continue;
        }
        int *cptr = malloc(sizeof(int)); //alloca dinamicamente spazio per passare il socket alla task
        *cptr = client_sock;

            Task t = {
                .taskFunction = &connessioni, //funzione da eseguire (gestione connessione)
                .arg = cptr //argomento passato alla funzione (socket client)
            };
            submitTask(t); //inserisce la task in coda 
    
    }
    close(socketBiblioteca); //chiude il socket del server
    //distrugge mutex e variabile di condizione
    pthread_mutex_destroy(&mutexBib);
    pthread_cond_destroy(&condBib);

    return 0;
}

void connessioni(void* arg){
    int cs = *(int*)arg;
    struct TesseraBibliotecaria tb;
    int risposta;
    ssize_t bytesReceived = recv(cs, &tb, sizeof(tb), 0); //riceve la struct TesseraBibliotecaria dal client
    if(bytesReceived <= 0){//se si verifica un errore o connessione chiusa, risponde con 0 e chiude tutto
        risposta = 0;
        perror("[+]Error in recv");
        send(cs, &risposta, sizeof(risposta), 0);
        close(cs);
        free(arg);
        return;
    }
    //imposta i dati di emissione e scadenza della tessera 
    tb.data_emissione = time(NULL);
    tb.data_scadenza = tb.data_emissione + ANNO;//aggiunge un anno alla data di emissione
    tb.servizio = 0; //servizio di scrittura
    tb.stato = 1;//imposta lo stato della tessera(attiva)

    int serverBfd = socket(AF_INET, SOCK_STREAM, 0); //crea un socket per comunicare con il serverB
    if(serverBfd < 0){
        perror("[+]Error in socket");
        return;
    }
    //indirizzo del serverB
    struct sockaddr_in indServB;
    indServB.sin_family = AF_INET;//indirizzo IPV4
    indServB.sin_port = htons(SERVERB); //porta
    indServB.sin_addr.s_addr = htonl(INADDR_ANY);

    if(connect(serverBfd, (struct sockaddr*)&indServB, sizeof(indServB)) < 0){//connessione al serverB
        perror("[+]Error in connect");
        close(serverBfd);
        free(arg);
        return;
    }

    if(send(serverBfd, &tb, sizeof(tb), 0) < 0){//invio della tessera al serverB
        perror("[+]Error in send");
        close(serverBfd);
        free(arg);
        return;
    }

    if(recv(serverBfd, &risposta, sizeof(risposta), 0) < 0){//riceve la risposta dal serverB
        perror("[+]Error in recv");
        close(serverBfd);
        free(arg);
        return;
    }

    if(send(cs, &risposta, sizeof(risposta), 0)<0) {//invia la risposta ricevuta dal serverB al client originale
        perror("[+]Error in send");
        close(cs);
        free(arg);
        return;
    }
    //chiude socket e libera memoria
    close(cs);
    close(serverBfd);
    free(arg);
}
/*+++ esegue la task richiamando la funzione passata nel campo taskFunction +++*/
void executeTask(Task * task){
    task->taskFunction(task->arg);
}

/* +++ routine dei thread: eseguita da ogni thread
- attende una task disponibile nella coda (usando pthread_cond_wait e la variabile di condizione)
- rimuove la prima task e lo esegue */
void *routine(void *arg){
    
    while(running){
        Task task;
        pthread_mutex_lock(&mutexBib);
        while(task_count == 0 && running){//se non ci sono task, il thread si mette in attesa sulla variabile di condizione
            pthread_cond_wait(&condBib, &mutexBib);
        }
        if(!running){//se il server e' in chiusura, esce
            pthread_mutex_unlock(&mutexBib);
            break;
        }
        task = taskQueue[0]; //prende la prima task
        int i;
        for(i = 0; i<task_count-1; i++){ 
            taskQueue[i] = taskQueue[i+1];
        }
        task_count --;
        pthread_mutex_unlock(&mutexBib);
        executeTask(&task); //esegue la task
    }
    
    return NULL;
}



void submitTask(Task task){
    pthread_mutex_lock(&mutexBib);
    taskQueue[task_count] = task;//inserisce la task in fondo alla coda
    task_count++;
    pthread_cond_signal(&condBib);//sveglia un thread in attesa
    pthread_mutex_unlock(&mutexBib);
}

void sigint_handler(int sig){
    running = 0; //imposta il flag a 0 per fermare il ciclo principale e i thread
    close(socketBiblioteca); // chiude la socket per forzare l'uscita da accept()
    pthread_cond_broadcast(&condBib); //sveglia eventuali thread in attesa 
}
