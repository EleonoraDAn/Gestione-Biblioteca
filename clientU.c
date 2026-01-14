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
    if(argc < 2){ //controlla se nella linea di comando si ha anche il codice della tessera
        printf("Argomenti insufficienti. Riprova con: ./clientU <codiceTessera>\n");
        exit(1);
    }
    if(strlen(argv[1]) != TESSERA_LUNG){ //si controlla se il codice inserito sia della lunghezza giusta
        printf("Errore: il codice deve essere lungo %d caratteri.\n", TESSERA_LUNG);
        return 1;
    }
    int socketCU, risposta;
    struct sockaddr_in indSG;
    struct TesseraBibliotecaria tb;
    socketCU = socket(AF_INET, SOCK_STREAM, 0); //socket per il clientU
    if(socketCU < 0){
        perror("[+]Error in socket");
        exit(1);
    }
    /*vengono definite le informazioni riguardo l'indirizzo del server con il quale il clientU comunica*/
    indSG.sin_addr.s_addr = inet_addr(LOCALHOST); //stabilisce che l'indirizzo al quale connettersi e' "LOCALHOST" (vedi "settings_Client_Server.h")
    indSG.sin_port = htons(SERVERG); //definisce la porta del server
    indSG.sin_family = AF_INET; //definisce la famiglia dell'indirizzo (IPV4)

    if(connect(socketCU,(struct sockaddr*)&indSG,sizeof(indSG))<0){ //connessione al serverG
        perror("[+]Error in connect");
        close(socketCU);
        exit(1);
    }
    memset(&tb, 0, sizeof(tb)); //pulizia della struct per evitare valori garbage
    strncpy(tb.codice_tb, argv[1], TESSERA_LUNG); //copiamo la stringa inserita dall'utente nella linea di comando in "tb.codice_tb"
    for(size_t i=0; tb.codice_tb[i]!='\0';i++){ //i caratteri del codice della tessera vengono convertiti in maiuscolo
        tb.codice_tb[i] = toupper((unsigned char) tb.codice_tb[i]);
    }
    tb.servizio = 2; //servizio di sospensione/attivazione tessera

    if(send(socketCU, &tb, sizeof(tb), 0)<0){ //viene inviata la tessera bibliotecaria al serverG
        perror("[+]Error in send");
        close(socketCU);
        exit(1);
    }

    if(recv(socketCU, &risposta, sizeof(risposta), 0)<0){//il client riceve una risposta
        perror("[+]Error in recv");
        exit(1);
    }

    if(risposta == 0){ //risposta ricevuta dal server
        printf("La tessera bibliotecaria e' stata invalidata.\n");
    }else if(risposta == -2){
        printf("La tessera bibliotecaria non e' stata trovata.\n");
    }else{
        printf("La tessera bibliotecaria e' valida.\n");
    }

    close(socketCU);
   return 0;
}
