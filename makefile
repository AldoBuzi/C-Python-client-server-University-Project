# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite

CC=gcc
CFLAGS=-std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread


# su https://www.gnu.org/software/make/manual/make.html#Implicit-Rules
# sono elencate le regole implicite e le variabili 
# usate dalle regole implicite 

# Variabili automatiche: https://www.gnu.org/software/make/manual/make.html#Automatic-Variables
# nei comandi associati ad ogni regola:
#  $@ viene sostituito con il nome del target
#  $< viene sostituito con il primo prerequisito
#  $^ viene sostituito con tutti i prerequisiti

# elenco degli eseguibili da creare


EXEC=archivio client1 client2

# se si scrive solo make di default compila main 
all: $(EXEC)

# regola per la creazioni degli eseguibili utilizzando xerrori.o
archivio: archivio.o hashTableManager.o xerrori.o rw_lock.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
client1: client1.o xerrori.o 
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
client2: client2.o xerrori.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	rm -f *.o

# regola per la creazione di file oggetto che dipendono da xerrori.h
%.o: %.c hashTableManager.h xerrori.h rw_lock.h
	$(CC) $(CFLAGS) -c $<

 
# esempio di target che non corrisponde a una compilazione
# ma esegue la cancellazione dei file oggetto e degli eseguibili
clean: 
	rm -f *.o $(EXECS)
run_server:
	python3 server.py 5 -r 2 -w 4 -v & 
	sleep 2
	./client2.out file1.txt file2.txt
	sleep 1
	./client1.out file3.txt
	pkill -SIGINT -f python3
	

