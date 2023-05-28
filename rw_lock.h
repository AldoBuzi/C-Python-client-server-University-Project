#define _GNU_SOURCE   // permette di usare estensioni GNU
#include <stdio.h>    // permette di usare scanf printf etc ...
#include <stdlib.h>   // conversioni stringa exit() etc ...
#include <stdbool.h>  // gestisce tipo bool
#include <assert.h>   // permette di usare la funzione ass
#include <string.h>   // funzioni per stringhe
#include <errno.h>    // richiesto per usare errno
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <pthread.h>
#ifndef PROGETTO_FINALE_SERVER_RW_LOCK_H
#define PROGETTO_FINALE_SERVER_RW_LOCK_H

#endif //PROGETTO_FINALE_SERVER_RW_LOCK_H


typedef struct {
    int readers;
    bool writing;
    pthread_cond_t cond;   // condition variable
    pthread_mutex_t mutex; // mutex associato alla condition variable
} rw;

void read_lock(rw *z);

void read_unlock(rw *z);

void write_lock(rw *z);

void write_unlock(rw *z);


void rw_init(rw *z);
void rw_destroy(rw *z);