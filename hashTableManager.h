#define _GNU_SOURCE   // permette di usare estensioni GNU
#define _BSD_SOURCE 1

#include "xerrori.h"
#include <search.h>


ENTRY *crea_entry(char *s, int n) ;


void distruggi_entry(ENTRY *e);

void aggiungi(char *s);

int conta(char *s);

void destroyHashTable();

void createHashTable();