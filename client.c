#include <stdio.h>
#include <stdlib.h> //per funzioni standard(es. exit)
#include <string.h> //per operazioni su stringhe(es. strncpy, memset)
#include <arpa/inet.h> //per funzioni e strutture legate agli indirizzi di rete
#include <sys/socket.h> //per funzioni e strutture dei socket
#include <unistd.h> //per close() e altre funzioni POSIX
#include <ctype.h>

#include "tessera_bibliotecaria.h"
#include "settings_Client_Server.h"

int main(int argc, char const *argv[])
{
    if(argc < 2){ //controllo sul numero di argomenti presenti nella linea di comando
        printf("Argomenti insufficienti. Riprova con: ./client <codiceTessera>\n");
        exit(1);
    }
    if(strlen(argv[1]) != TESSERA_LUNG){ //verifica se il codice inserito nella linea di comando sia della lunghezza giusta
        printf("Errore: il codice deve essere lungo %d caratteri.\n", TESSERA_LUNG);
        return 1;
    }
    int socketFD, risposta;
    struct sockaddr_in indB;//struttura per indirizzo del server biblioteca
    struct TesseraBibliotecaria tb;
    socketFD = socket(AF_INET, SOCK_STREAM, 0); //socket per comunicare 
    if(socketFD < 0){
        perror("[+]Error in socket");
        exit(1);
    }
/*il server della biblioteca e' caratterizzato dalle seguenti informazioni:*/
    indB.sin_addr.s_addr = inet_addr(LOCALHOST); //indirizzo(127.0.0.1, vedere settings_Client_Server.h)
    indB.sin_family = AF_INET; //un indirizzo IPV4
    indB.sin_port = htons(BIBLIOTECA); //porta del server della biblioteca

    if(connect(socketFD, (struct sockaddr*)&indB, sizeof(indB))<0){ //connessione al server della biblioteca
        perror("[+]Error in connect");
        exit(1);
    }

    memset(&tb, 0, sizeof(tb)); //pulizia della struct per evitare valori garbage
    strncpy(tb.codice_tb, argv[1], TESSERA_LUNG); //copia del codice della tessera ricevuto da linea di comando nella struct
    tb.codice_tb[TESSERA_LUNG]='\0'; //aggiunta del terminatore di stringa 
    for(size_t i=0; tb.codice_tb[i]!='\0'; i++){//conversione del codice della tessera in maiuscolo per la standardizzazione
        tb.codice_tb[i] = toupper((unsigned char) tb.codice_tb[i]);
    }
   
    if(send(socketFD, &tb, sizeof(tb), 0)<0){//invio della tessera al server
        perror("[+]Error in send");
        exit(1);
    }

    if(recv(socketFD, &risposta, sizeof(risposta), 0) < 0){ //ricezione della risposta dal server
        perror("[+]Error in recv");
        exit(1);
    }

    if(risposta == 0){//risposta ricevuta
        printf("La tessera bibliotecaria e' gia' stata inserita.\n");
    }else{
        printf("La tessera bibliotecaria e' valida.\n");
    }

    close(socketFD); //chiusura del socket

    return 0;
}
