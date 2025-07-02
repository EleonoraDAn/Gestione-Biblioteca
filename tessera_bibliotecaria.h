#ifndef TESSERA_BIBLIOTECARIA_H
#define TESSERA_BIBLIOTECARIA_H

#include <time.h>

#define TESSERA_LUNG 10 //lunghezza del codice della tessera bibliotecaria
#define ANNO 31536000 //quantita' di secondi che equivale ad un anno, viene utilizzato per creare la "data_scadenza" 

struct TesseraBibliotecaria {
    char codice_tb[TESSERA_LUNG+1]; //codice della tessera bibliotecaria di tipo "char"
    time_t data_emissione; //data emissione della tessera bibliotecaria
    time_t data_scadenza; //data di scadenza della tessera bibliotecaria
    int servizio; //0: inserimento tessera, 1: controllo validita', 2: sospende/abilita la tessera
    int stato; // 1: attiva, 0: sospesa
};

#endif