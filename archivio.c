#define _BSD_SOURCE 1
#include <search.h>
#include "hashTableManager.h"
#include "xerrori.h"
#include <sys/signal.h>
#include "rw_lock.h"
#define PC_buffer_len 10
#define here __LINE__,__FILE__
#define Num_elem 1000000

/*
 *
 * INIZIO REGIONE STRUCT
 *
 */

// sruct capo scrittore & capo lettore
typedef struct {
    char **buffer;
    sem_t *full_slots;
    sem_t *empty_slots;
    int *p_index;
    int *termination_code;
    char *pipeName;
    
} struct_capo;
//struct scrittore consumatore
typedef struct {
    char **buffer;
    pthread_mutex_t *writer_consumer_mutex;
    sem_t *full_slots;
    sem_t *empty_slots;
    int *c_index;
    int *termination_code;
    rw *rw_lock;
    int *counterStringAdded;

} writer_consumer;
//struct lettore consumatore
typedef struct {
    char **buffer;
    pthread_mutex_t *writer_consumer_mutex;
    sem_t *full_slots;
    sem_t *empty_slots;
    int *c_index;
    int *termination_code;
    rw *rw_lock;
    FILE *logFile;
    pthread_mutex_t *file_lock;
} reader_consumer;
/*
 *
 * FINE REGIONE STRUCT
 *
 */



/*
 *
 * REGIONE DEI CAPI
 *
*/

//INIZIO REGIONE SCRITTORE CAPO & REGIONE LETTORE CAPO

void *writer_body(void *args){
    //PARSE Args to correct type
    struct_capo *obj = (struct_capo *) args;
    int fd = open (obj->pipeName, O_RDONLY);
    if(fd <= 0) {
        fprintf(stderr,"Errore apertura named pipe %s",obj->pipeName);
        *(obj -> termination_code) = -1;
        xsem_post(obj->empty_slots, here);
        pthread_exit(0);
    }
    while(true){
        /*
         * Fase di lettura dalla pipe
         */
        //lunghezza
        short length;
        ssize_t errNo = read(fd,&length,sizeof(short));
        if(errNo <0) termina("Errore lettura");
        if(errNo == 0) { //pipe chiusa, invio terminazione ai consumatori
            puts("Finito di leggere");
            *(obj -> termination_code) = -1;
            xsem_post(obj->empty_slots, here);
            break;
        }
        //stringa effettiva
        char *text = malloc(sizeof (char) * length);
        errNo = read(fd,text,sizeof(char) * length);
        if(errNo<0) termina("Errore lettura da pipe");
        if(errNo == 0) {
            free(text);
            puts("Finito di leggere");
            *(obj -> termination_code) = -1;
            xsem_post(obj->empty_slots, here);
            break;
        }
        /*
         * Fase di scrittura nel buffer
         */
        text[strlen(text)] = '\0';
        char* token = strtok(text, ".,:; \n\r\t");
        while (token != NULL) {
            xsem_wait(obj->full_slots, here);
            token[strlen(token)] = '\0';
            char *token_copy = strdup(token);
            obj->buffer[*(obj->p_index) % PC_buffer_len] =  token_copy;
            *(obj->p_index) = *(obj->p_index) + 1;
            xsem_post(obj->empty_slots, here);
            token = strtok(NULL, ".,:; \n\r\t");
        }
        free(text);
    }
    close(fd); //chiudo la pipe
    pthread_exit(0);
}

//FINE REGIONE SCRITTORE CAPO & REGIONE LETTORE CAPO

/*
 *
 * FINE REGIONE DEI CAPI
 *
*/


/*
 *
 * INIZIO REGIONE DEGLI SCRITTORI CONSUMATORI
 *
*/

void *writer_consumer_body(void * args){
    //PARSE Args to correct type
    writer_consumer *obj = (writer_consumer *) args;
    while(true){
        xsem_wait(obj->empty_slots, here);
        xpthread_mutex_lock( obj->writer_consumer_mutex, here );
        if(*(obj->termination_code) == -1){
            xpthread_mutex_unlock(obj->writer_consumer_mutex, here);
            xsem_post(obj->empty_slots, here);
            break;
        }
        char *stringa =  obj->buffer[*(obj->c_index) % PC_buffer_len];
        *(obj->c_index) = *(obj->c_index) + 1;
        xpthread_mutex_unlock(obj->writer_consumer_mutex, here);
        xsem_post(obj->full_slots,here);
        write_lock(obj->rw_lock);
        aggiungi(stringa);
        if(stringa[0] == '\0'){ //Aggiungo \0 all'inizio della stringa se ho inserito una nuova stringa distinta in hash table
            *obj->counterStringAdded = *obj->counterStringAdded + 1;
            stringa[0] = 'a'; // riprstino la prima posizione con un carattere diverso da \0
        }
        free(stringa);
        write_unlock(obj->rw_lock);
    }
    pthread_exit(0);
}
/*
 *
 * FINE REGIONE SCRITTORI CONSUMATORI
 *
 */



/*
 *
 * REGIONE DEI LETTORI CONSUMATORI
 *
*/

void *reader_consumer_body(void * args){
    //PARSE Args to correct type
    reader_consumer *obj = (reader_consumer *) args;
    while(true){
        xsem_wait(obj->empty_slots, here);
        xpthread_mutex_lock( obj->writer_consumer_mutex, here );
        if(*(obj->termination_code) == -1){
            xpthread_mutex_unlock(obj->writer_consumer_mutex, here);
            xsem_post(obj->empty_slots, here);
            break;
        }
        char *stringa =  obj->buffer[*(obj->c_index) % PC_buffer_len];
        *(obj->c_index) = *(obj->c_index) + 1;

        xpthread_mutex_unlock(obj->writer_consumer_mutex, here);
        xsem_post(obj->full_slots,here);
        read_lock(obj->rw_lock);
        int val = conta(stringa);
        read_unlock(obj->rw_lock);
        pthread_mutex_lock(obj->file_lock);
        fprintf(obj->logFile,"%s %d\n", stringa, val);
        fflush (obj->logFile);
        pthread_mutex_unlock(obj->file_lock);
        free(stringa);
    }
    pthread_exit(0);
}

/*
 *
 * FINE REGIONE DEI LETTORI CONSUMATORI
 *
*/



/*
 *
 * SIGNAL HANDLER THREAD REGION
 *
*/

//INIZIO REGIONE Funzione convertitore
char *formatString(int val, char *toFormat){
    int maxNum = 10000;
    int startFrom = 31;
    while(maxNum != 1){
        if(val >= maxNum){
            toFormat[startFrom] = '0'+ val/maxNum  ;
            val = val % maxNum;
            if(val == 0) {
                while(startFrom <=34)
                    toFormat[++startFrom] = '0';
                break;
            }
            maxNum = maxNum/10;
            while(maxNum > val){
                maxNum = maxNum/10;
                toFormat[++startFrom] = '0';
            }
        }
        else {
            toFormat[startFrom] = ' ';
            maxNum = maxNum/10;
        }
        startFrom ++;
    }
    toFormat[35] = '0'+ val;
    return toFormat;
}
//struct per l'handler
typedef struct{
    int *counterStringAdded;
    pthread_t *writer_master;
    pthread_t *reader_master;
    pthread_t *writer_array;
    int *writer_length;
    pthread_t *reader_array;
    int *reader_length;
}signal_handler_data;

//INIZIO REGIONE HANDLER SEGNALI
void *signalHandlerBody(void *args){
    signal_handler_data *obj = (signal_handler_data *) args;
    sigset_t mascheraSegnali;
    sigemptyset(&mascheraSegnali);
    sigaddset(&mascheraSegnali, SIGTERM);
    sigaddset(&mascheraSegnali, SIGINT);
    sig_atomic_t s;
    while(true) {
        int e = sigwait(&mascheraSegnali,&s);
        sigfillset(&mascheraSegnali);
        if(e!=0) perror("Errore sigwait");
        if(s == SIGINT){
            char c[] = "valore differenti in hashTable xxxxx\n";
            e = write(STDERR_FILENO, formatString(*obj->counterStringAdded, c),37);
            if(e != 37) termina("Errore write");
        }
        if(s==SIGTERM) {
            xpthread_join(*obj->writer_master,NULL,here);
            xpthread_join(*obj->reader_master,NULL,here);
            for(int key = 0; key < *obj->writer_length; key++)
                xpthread_join(obj->writer_array[key],NULL,here);
            for(int key = 0; key < *obj->reader_length; key++)
                xpthread_join(obj->reader_array[key],NULL,here);
            char c[] = "valore differenti in hashTable xxxxx\n";
            e = write(STDOUT_FILENO, formatString(*obj->counterStringAdded, c),37);
            if(e != 37) termina("Errore write");
            break;
        }
    }
    pthread_exit(0);
}
//FINE REGIONE HANDLER SEGNALI

/*
 *
 * END SIGNAL HANDLER THREAD REGION
 *
*/



/*
 *
 * INIZIO REGIONE MAIN
 *
 * */
int main(int argc, char *argv[]) {

    if(argc != 3) termina("Passare -r e -w");
    /*
     * HASH TABLE E STRUTTURE INIZIALI
     */
    int hashTable = hcreate(Num_elem);
    if( hashTable==0 ) termina("Errore creazione Hash Table");
    int numberOfWriters = atoi(argv[1]);
    int numberOfReaders = atoi(argv[2]);
    char **writer_buffer = malloc(PC_buffer_len * sizeof (char *));
    int counterStringAdded = 0;
    rw *lock = malloc(sizeof (rw));
    rw_init(lock);

    /*
     * REGIONE DICHIARAZIONE SCRITTORE CAPO, SCRITTORE CONSUMATORE E STRUCT NECESSARIE
     */
    pthread_t scrittore;
    pthread_mutex_t writer_consumer_mutex = PTHREAD_MUTEX_INITIALIZER;
    sem_t writer_full_slots;
    sem_t  writer_empty_slots;
    sem_init(&writer_full_slots,0,PC_buffer_len);
    sem_init(&writer_empty_slots,0,0);
    pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;
    char caposc[] = "caposc";
    int writer_p_index = 0, writer_c_index = 0, writer_termination_code = 0;
    struct_capo obj_producer = {writer_buffer, &writer_full_slots, &writer_empty_slots, &writer_p_index, &writer_termination_code, caposc};
    writer_consumer  obj_consumer = {writer_buffer, &writer_consumer_mutex, &writer_full_slots, &writer_empty_slots,&writer_c_index,&writer_termination_code, lock, &counterStringAdded};
    pthread_t writer_consumer_array[numberOfWriters];

    /*
     * REGIONE DICHIARAZIONE LETTORE CAPO, LETTORE CONSUMATORE E STRUCT NECESSARIE
     */

    pthread_mutex_t reader_consumer_mutex = PTHREAD_MUTEX_INITIALIZER;
    sem_t reader_full_slots;
    sem_t  reader_empty_slots;
    FILE *f = fopen("lettori.log","w+");
    if(f == NULL) termina("Errore apertura file di log");
    char **reader_buffer = malloc(PC_buffer_len * sizeof (char *));
    sem_init(&reader_full_slots,0,PC_buffer_len);
    sem_init(&reader_empty_slots,0,0);
    char capolet[] = "capolet";
    int reader_p_index = 0, reader_c_index = 0;
    int  reader_termination_code = 0;
    pthread_t lettore;
    pthread_t reader_consumer_array[numberOfWriters];
    struct_capo robj_producer = {reader_buffer, &reader_full_slots, &reader_empty_slots, &reader_p_index, &reader_termination_code, capolet};
    reader_consumer robj_consumer = {reader_buffer, &reader_consumer_mutex, &reader_full_slots, &reader_empty_slots,&reader_c_index,&reader_termination_code, lock, f, &file_lock};

    /*
    * REGIONE DICHIARAZIONE MASCHERA SEGNALI, THREAD HANDLER E STRUCT NECESSARIA
    */

    sigset_t mascheraMain;
    sigemptyset(&mascheraMain);
    sigaddset(&mascheraMain, SIGTERM);
    sigaddset(&mascheraMain, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mascheraMain, NULL);
    signal_handler_data data = {&counterStringAdded, &scrittore, &lettore,writer_consumer_array,&numberOfWriters,reader_consumer_array, &numberOfReaders};
    pthread_t signalHandler;

    /*
     * REGIONE DI AVVIO DEI THREAD
     */

    //signal handler
    xpthread_create(&signalHandler,NULL,&signalHandlerBody,(void * )&data,here);
    //capo scrittore
    xpthread_create(&scrittore,NULL,&writer_body,(void * )&obj_producer, here);
    //scrittori consumatori
    for(int key = 0; key < numberOfWriters; key++){
        xpthread_create(&writer_consumer_array[key],NULL,&writer_consumer_body, (void *) &obj_consumer, here);
    }
    //capo lettore
    xpthread_create(&lettore,NULL,&writer_body,(void * )&robj_producer, here);
    //lettori consumatori
    for(int key = 0; key < numberOfReaders; key++){
        xpthread_create(&reader_consumer_array[key],NULL,&reader_consumer_body, (void *) &robj_consumer, here);
    }
    //join del signal hanlder, lui si occuperà di fare le altre join quando arriverà SIGTERM
    xpthread_join(signalHandler,NULL,here);

    /*
     * REGIONE DEALLOCAZIONE MEMORIA
     */

    //se sono arrivato qua vuol dire che tutte le join necessarie hanno terminato, inizia la fase di deallocazione
    hdestroy();
    xpthread_mutex_destroy(&writer_consumer_mutex,here);
    xpthread_mutex_destroy(&reader_consumer_mutex,here);
    xpthread_mutex_destroy(&file_lock,here);
    sem_destroy(&writer_full_slots);
    sem_destroy(&writer_empty_slots);
    sem_destroy(&reader_full_slots);
    sem_destroy(&reader_empty_slots);

    rw_destroy(lock);
    free(lock);
    fclose(f);
    free(reader_buffer);
    free(writer_buffer);


    return  0;
}

/*
 *
 * FINE REGIONE MAIN
 *
 */