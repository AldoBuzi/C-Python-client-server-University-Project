#include "hashTableManager.h"
#define _BSD_SOURCE 1


ENTRY *crea_entry(char *s, int n) {
    ENTRY *e = malloc(sizeof(ENTRY));
    if(e==NULL) termina("errore malloc entry 1");
    char *new_pointer = strdup(s);
    if(new_pointer <=0) termina("Errore strdup");
    e->key = new_pointer; // salva copia di s
    e->data = (int *) malloc(sizeof(int));
    if(e->key==NULL || e->data==NULL)
        termina("errore malloc entry 2");
    *((int *)e->data) = n;
    return e;
}

void distruggi_entry(ENTRY *e)
{
    free(e->key); free(e->data); free(e);
}
void aggiungi(char *s){
    ENTRY *e = crea_entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    if(r==NULL) { // la entry è nuova
        r = hsearch(*e,ENTER);
        if(r==NULL) termina("errore o tabella piena");
        *s = '\0';
    }
    else {
        // la stringa è gia' presente incremento il valore
        assert(strcmp(e->key,r->key)==0);
        int *d = (int *) r->data;
        *d +=1;
        distruggi_entry(e); // questa non la devo memorizzare
    }
}
int conta(char *s){
    ENTRY *e = crea_entry(s, 1);
    ENTRY *r = hsearch(*e,FIND);
    distruggi_entry(e);
    if(r == NULL) return 0;
    return *((int *) r->data );
}
