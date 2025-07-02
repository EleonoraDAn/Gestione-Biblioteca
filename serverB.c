#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "tessera_bibliotecaria.h"
#include "settings_Client_Server.h"


void connessioni(void *arg);//funzione che gestisce una singola connessione al client
void submitTask(Task);//inserisce una task nella coda
void executeTask(Task*);//esegue una task
void *routine(void* arg);//funzione che eseguono i thread del thread pool
void sigint_handler(int);//gestore del segnale SIGINT(CTRL+C)

volatile sig_atomic_t running = 1; //indica se il server deve continuare a girare
Task taskQueue[256]; //viene creata una coda di task che devono essere eseguite da ogni thread
int task_count = 0; //questo contatore servirà per scorrere nella coda delle task (conta il numero di task presenti nella coda)
pthread_mutex_t mutexSB; //protegge l'accesso alla coda delle task
pthread_cond_t condSB; //una variabile di condizione per notificare nuove task
pthread_mutex_t mutex_file; //protegge l'accesso concorrente al file delle tessere ("tessere_bibliotecarie.txt")
int socketSB; //socket del server(resa globale per poter essere chiusa nel gestore SIGINT)


int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);
    /*inizializzo tutto: variabile di condizione e i vari mutex*/
    pthread_t thread_pool[THREAD_NUM]; //dichiaro il thread pool
    pthread_mutex_init(&mutexSB, NULL);
    pthread_mutex_init(&mutex_file, NULL);
    pthread_cond_init(&condSB, NULL);
    struct sockaddr_in indSB; //indirizzo serverB
    if((socketSB=socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("[+]Error in socket");
        exit(1);
    }
    /* +++ imposto le varie informazioni dell'indirizzo del serverB +++*/
    indSB.sin_port = htons(SERVERB); //porta
    indSB.sin_family = AF_INET; //famiglia dell'indirizzo (indirizzi IPV4)
    indSB.sin_addr.s_addr = htonl(INADDR_ANY); //accetta connessioni da qualsiasi IP a cui questa macchina e' connessa
    if((bind(socketSB, (struct sockaddr*)&indSB, sizeof(indSB))<0)){ //binding...
        perror("[+]Error in bind");
        return 0;
    }
    if((listen(socketSB, SOMAXCONN))<0){//listening...
        perror("[+]Error in listen");
        exit(EXIT_FAILURE);
    }

    int i;
    for(i = 0; i<THREAD_NUM; i++){ //vengono generati i thread del threadpool
        
        if(pthread_create(&thread_pool[i], NULL, &routine, NULL) != 0){
            perror("[+]Error in pthread_create");
        }
    }
    while(running){//il server accetta connessioni finchè non viene ricevuto SIGINT(CTRL+C) 
        struct sockaddr_in indClient;
        socklen_t client_len = sizeof(indClient);
        int client_sock = accept(socketSB, (struct sockaddr*)&indClient, &client_len);
        if(client_sock < 0){
            if(!running) break;
            perror("[+]Error in accept");
            continue;
        }
        int *cptr = malloc(sizeof(int));
        *cptr = client_sock;
        Task t = {
            .taskFunction = &connessioni,
            .arg = cptr
        };
        submitTask(t); //invia il task alla coda del thread pool
    }
    
    for(i = 0; i<THREAD_NUM; i++){ 
        if(pthread_join(thread_pool[i], NULL) != 0){
            perror("[+]Error in pthread_join");
        }
    }
    /*Pulizia finale del codice*/
    close(socketSB);
    pthread_mutex_destroy(&mutexSB);
    pthread_mutex_destroy(&mutex_file);
    pthread_cond_destroy(&condSB);
    return 0;
}

void connessioni(void *arg){ //gestisce una connessione con un client e viene eseguita da un threadpool

    int cs = (*(int*)arg); //estrae il descrittore della socket dal puntatore 
    struct TesseraBibliotecaria tb;
    if(recv(cs, &tb, sizeof(tb), 0) < 0){ // riceve la tessera bibliotecaria
        perror("[+]Error in recv");
        close(cs);
        free(arg);
        return;
    }
    
    pthread_mutex_lock(&mutex_file);
    FILE * filestream = fopen("tessere_bibliotecarie.txt", "r+"); //apertura in lettura/scrittura del file delle tessere bibliotecarie
    
    if(filestream == NULL){
        perror("[+]Error in fopen");
        close(cs);
        free(arg);
        return;
    }
    fseek(filestream, 0, SEEK_SET); //riporta il cursore del file dall'inizio perche' si deve leggere o modificare tutto il file
    switch (tb.servizio) {
            case 0: { // inserimento con controllo duplicati
                int risposta = 1;
                char buffer[512];
                char codice_file[TESSERA_LUNG + 1];
                rewind(filestream); //torna all'inizio del file
                while (fgets(buffer, sizeof(buffer), filestream)) { //ciclo per leggere tutte le righe e verificare i duplicati
                    // leggo solo il codice prima della virgola
                    if (sscanf(buffer, "%[^,]", codice_file) == 1) {
                        if (strncmp(tb.codice_tb, codice_file, TESSERA_LUNG) == 0) { // se si inserisce un codice uguale a quello presente nel file...
                            risposta = 0; 
                            break;
                        }
                    }
                }
    
                if (risposta == 1) { // se non e' duplicato
                    
                    struct tm validInizio = *localtime(&tb.data_emissione); //viene impostata la data di emissione
                    struct tm validFine = *localtime(&tb.data_scadenza);//viene impostata la data di scadenza ad un anno dalla data di emissione (vedere "biblioteca.c")
                    fprintf(filestream, "%s, emissione: %02d-%02d-%04d, scadenza: %02d-%02d-%04d, stato: %d\n",
                        tb.codice_tb,
                        validInizio.tm_mday, validInizio.tm_mon + 1, validInizio.tm_year + 1900,
                        validFine.tm_mday, validFine.tm_mon + 1, validFine.tm_year + 1900,
                        tb.stato); //vengono inserite tutte le informazioni raccolte nel file 
                    fflush(filestream);
                }
                if(send(cs, &risposta, sizeof(risposta), 0)<0){ //viene inviata la risposta al client
                    perror("[+]Error in send");
                    close(cs);
                    exit(1);
                }
                break;
            }
    
            case 1: { // servizio di verifica della validita' (effettuato da "clientV.c")
                char buffer[512];
                char codice_file[TESSERA_LUNG + 1];
                int gg_s, mm_s, aa_s, statoAttuale;
                int trovata = 0; //codice trovato oppure no
                int risposta = 0; //risposta da inviare al client
                time_t oggi = time(NULL); //data del momento di esecuzione del codice
    
                rewind(filestream); //il file viene letto dall'inizio
                while (fgets(buffer, sizeof(buffer), filestream)) {
                    // leggo codice, data scadenza e stato(tessera attiva oppure no)
                    if (sscanf(buffer, "%[^,], emissione: %*d-%*d-%*d, scadenza: %d-%d-%d, stato: %d",
                               codice_file, &gg_s, &mm_s, &aa_s, &statoAttuale) == 5) { //se gli argomenti sono 5: codice tessera, giorno, mese e anno di scadenza, stato ...
                        if (strncmp(tb.codice_tb, codice_file, TESSERA_LUNG) == 0) {
                            trovata = 1; //tessera trovata!
                            struct tm data_scadenza = {0};
                            data_scadenza.tm_mday = gg_s;
                            data_scadenza.tm_mon = mm_s - 1;
                            data_scadenza.tm_year = aa_s - 1900;
                            time_t ds = mktime(&data_scadenza); //si converte la data di scadenza in time_t
    
                            // controllo stato e scadenza
                            if (statoAttuale == 1 && difftime(ds, oggi) > 0) {//se lo stato attuale e' 1 e la tessera non e' scaduta...
                                risposta = 1; // tessera valida
                            } else {
                                risposta = 0; // tessera non valida
                            }
                            break;
                        }
                    }
                }
                if (!trovata) risposta = -2; // codice non trovato
                if(send(cs, &risposta, sizeof(risposta), 0)<0){ //viene inviata la risposta al client
                    perror("[+]Error in send");
                    close(cs);
                    exit(1);
                }
                break;
            }
    
            case 2: { // sospendi / riattiva la tessera (servizio effettuato dal "clientU.c")
                char buffer[512];
                char codice_file[TESSERA_LUNG + 1];
                int gg_e, mm_e, aa_e, gg_s, mm_s, aa_s, statoAttuale;
                int trovata = 0;
                long pos_inizio_riga;
    
                rewind(filestream);//il file viene letto dall'inizio
                while (fgets(buffer, sizeof(buffer), filestream)) {
                    pos_inizio_riga = ftell(filestream) - strlen(buffer); //si calcola la posizione di inizio riga usando ftell e strlen
                    if (sscanf(buffer, "%[^,], emissione: %d-%d-%d, scadenza: %d-%d-%d, stato: %d",
                               codice_file, &gg_e, &mm_e, &aa_e, &gg_s, &mm_s, &aa_s, &statoAttuale) == 8) {
                        if (strncmp(tb.codice_tb, codice_file, TESSERA_LUNG) == 0) {
                            trovata = 1; //tessera trovata!
                            int nuovoStato = (statoAttuale == 1) ? 0 : 1; //se lo stato attuale e' 1, il nuovo stato sara' 0 (e viceversa)

    // sposto il puntatore all'inizio della riga per sovrascriverla
                            fseek(filestream, pos_inizio_riga, SEEK_SET); //ci si posiziona all'inizio della riga nel file
                            fprintf(filestream, "%s, emissione: %02d-%02d-%04d, scadenza: %02d-%02d-%04d, stato: %d\n",
                                    codice_file,
                                    gg_e, mm_e, aa_e,
                                    gg_s, mm_s, aa_s,
                                    nuovoStato); 
                            fflush(filestream); //si aggiorna il file
                            if(send(cs, &nuovoStato, sizeof(nuovoStato), 0)<0){ //viene inviato il nuovo stato al client
                                perror("[+]Error in send");
                                close(cs);
                                exit(1);
                            }
                            break;
                        }
                    }
                }
                if (!trovata) { // tessera non trovata
                    int risposta = -2;
                    if(send(cs, &risposta, sizeof(risposta), 0)<0){ //viene inviata la risposta al client
                        perror("[+]Error in send");
                        close(cs);
                        exit(1);
                    }
                }
                break;
            }
    
        }
/* +++ pulizia del codice +++*/
fclose(filestream);
pthread_mutex_unlock(&mutex_file);
close(cs);
free(arg);
return;
}
//* +++ inserisce una task nella coda in modo thread-safe e segnala ai thread in attesa +++ */
void submitTask(Task task){
    pthread_mutex_lock(&mutexSB);
    taskQueue[task_count] = task; 
    task_count ++;
    pthread_cond_signal(&condSB);
    pthread_mutex_unlock(&mutexSB);
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
        pthread_mutex_lock(&mutexSB);
        while(task_count == 0 && running){
            pthread_cond_wait(&condSB, &mutexSB);
        }
        if(!running){
            pthread_mutex_unlock(&mutexSB);
            break;
        }
        task = taskQueue[0];
        
        for(int i=0; i<task_count-1; i++){
            taskQueue[i]=taskQueue[i+1];
        }
        task_count--;
        pthread_mutex_unlock(&mutexSB);
        executeTask(&task);
        
    }
    
    return NULL;
}

void sigint_handler(int sig){
    running = 0; //imposta il flag a 0 per fermare il ciclo principale e i thread
    close(socketSB); // chiude la socket per forzare l'uscita da accept()
    pthread_cond_broadcast(&condSB); //sveglia eventuali thread in attesa 
}