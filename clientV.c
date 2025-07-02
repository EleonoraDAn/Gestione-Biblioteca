#include <stdio.h>
#include <stdlib.h> //per funzioni standard(es. exit)
#include <string.h> //per operazioni su stringhe(es. strncpy, memset)
#include <arpa/inet.h> //per funzioni e strutture legate agli indirizzi di rete
#include <sys/socket.h> //per funzioni e strutture dei socket
#include <unistd.h> //per close() e altre funzioni POSIX

#include "tessera_bibliotecaria.h"
#include "settings_Client_Server.h"

int main(int argc, char const *argv[])
{
    if(argc < 2){ //controlla se si inserisce anche il codice della tessera nella linea di comando
        printf("Argomenti insufficienti. Riprova con: ./clientV <codiceTessera>\n");
        exit(1);
    }
    if(strlen(argv[1]) != TESSERA_LUNG){ //verifica se il codice inserito sia lungo quanto stabilito nella struct
        printf("Errore: il codice deve essere lungo %d caratteri.\n", TESSERA_LUNG);
        return 1;
    }
    int socketCV, risposta;
    struct sockaddr_in indSG;
    struct TesseraBibliotecaria tb;
    if((socketCV=socket(AF_INET, SOCK_STREAM, 0)) < 0){ //creazione socket per il client
        perror("[+]Error in socket");
        exit(1);
    }
    /*di seguito, sono impostate le informazioni riguardo il serverG*/
    indSG.sin_port = htons(SERVERG); //porta(veder settings_Client_Server.h)
    indSG.sin_family = AF_INET;//famiglia(indirizzo IPV4)
    indSG.sin_addr.s_addr = inet_addr(LOCALHOST);//indirizzo 

    if((connect(socketCV, (struct sockaddr*)&indSG, sizeof(indSG)))<0){ //connessione al server
        perror("[+]Error in connect");
        exit(1);
    }
    memset(&tb, 0, sizeof(tb)); //pulizia della struct per evitare valori garbage
    strncpy(tb.codice_tb, argv[1], TESSERA_LUNG); //si copia il codice della tessera bibliotecaria della linea di comando nella variabile "tb.codice_tb"
    for(size_t i=0; tb.codice_tb[i]!='\0';i++){ //i caratteri vengono convertiti in maiuscolo
        tb.codice_tb[i] = toupper(tb.codice_tb[i]);
    }
    tb.servizio = 1; //servizio di controllo della validita'

    if(send(socketCV, &tb, sizeof(tb), 0)<0){ //viene inviata la tessera al server
        perror("[+]Error in send");
        close(socketCV);
        exit(1);
    }

    if(recv(socketCV, &risposta, sizeof(risposta), 0)<0){//si riceve una risposta
        perror("[+]Error in recv");
        exit(1);
    }

    if(risposta == 0){//risposta del server
        printf("La tessera bibliotecaria non e' valida (scaduta o mancato rinnovo).\n");
    }else if(risposta == 1){
        printf("La tessera bibliotecaria e' valida.\n");
    }else if(risposta == -2){
        printf("La tessera non e' stata trovata.\n");
    }

    close(socketCV);
    return 0;
}
