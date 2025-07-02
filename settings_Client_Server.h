#ifndef SETTINGS_CLIENT_SERVER_H
#define SETTINGS_CLIENT_SERVER_H

#define BIBLIOTECA 55000
#define SERVERG 50000
#define SERVERB 60000
#define LOCALHOST "127.0.0.1"
#define THREAD_NUM 10 //lunghezza del threadpool

/*rappresenta un compito da eseguire all'interno del threadpool
ogni task e' composta da:*/
typedef struct Task{
    void (*taskFunction)(void*); //puntatore a funzione che accetta un argomento generico (void*) e non restituisce nulla. Sara' la funzione da eseguire
    void* arg;
}Task;
/*questo schema consente di avere una coda di task eterogenee ma gestibili in modo uniforme dai thread del threadpool*/

#endif