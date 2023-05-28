#
# IMPORT AREA
#
import socket
import argparse
import concurrent.futures
import os
import subprocess
from ConnectionA import ConnectionA 
from ConnectionB import ConnectionB 
from ServerProtocol import ServerProtocol
from PipeManager import PipeManager
import signal

# END IMPORT AREA 


#
# AREA funzione che avvia il server e crea i thread per gestire i vari client
#
def serverStart(threadMax, processArchivio):
    HOST = "127.0.0.1"
    PORT = 55116
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        try:
            # permette di riutilizzare la porta se il server viene chiuso
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)            
            server.bind((HOST, PORT))
            server.listen()
            with concurrent.futures.ThreadPoolExecutor(max_workers=threadMax) as executor:
                while True:
                    # mi metto in attesa di una connessione
                    conn, addr = server.accept()
                    executor.submit(gestisci_connessione, conn,addr)
        except KeyboardInterrupt:
            ConnectionA.pipe.close()
            ConnectionB.pipe.close()
            # mando un segnale SIGTERM a main che lo farà terminare            
            processArchivio.send_signal(signal.SIGTERM)
            processArchivio.wait() #aspetto il processo archivio che termini
            ConnectionA.pipe.unlink()
            ConnectionB.pipe.unlink()
            server.shutdown(socket.SHUT_RDWR)

# FINE AREA SERVER

#
# AREA gestisci connessione
#
def gestisci_connessione(conn,addr): 
  with conn:  
    try:
        #Determino tramite la super classe il tipo di connessione, mi restituirà un oggetto di tipo ConncectionA o ConnectionB (ovvero sottoclasse)
        obj = ServerProtocol(conn,addr).findConnectionType(conn,addr,(ConnectionA,ConnectionB))
    except BaseException as e:
       print(e) #sollevo un eccezione se la connessione non è ne di tipo A o B
    
    # dico al nostro oggetto di occuparsi interamente della gestione del client
    obj.handleConnection()

# FINE AREA gestisci connessione

#
# AREA Main
#

def main(args = None,port=55116):
    # Creo le pipe tramie PipeManager e le assegno staticamente ai due tipi di connessione
    ConnectionA.pipe = PipeManager("capolet")
    ConnectionB.pipe = PipeManager("caposc")
    ConnectionA.pipe.create()
    ConnectionB.pipe.create()

    #se vale 1 allora devo usare valgrind
    if args.valgrind == 1:
        processArchivio = subprocess.Popen(["valgrind","--leak-check=full", 
                      "--show-leak-kinds=all", 
                      "--log-file=valgrind-%p.log", 
                      "./archivio", args.writers_number, args.readers_number])
    else:
        processArchivio = subprocess.Popen(["./archivio",(args.writers_number), args.readers_number])
    # Apro le pipe in scrittura (posso specificare il tipo di apertura ma in automatico se non previsto apre in scrittura)
    ConnectionB.pipe.open()
    ConnectionA.pipe.open()
    #chiamo la funzione che avvia il server
    serverStart(threadMax=args.thread,processArchivio = processArchivio)
    return 0

#FINE AREA Main







if __name__ == '__main__':
    
    parser = argparse.ArgumentParser(
                    prog='Server Python Esame Laboratorio II',
                    epilog='Text at the bottom of help')
    parser.add_argument('thread', nargs='?' , default=-1, type=int)
    parser.add_argument('-w','--writers_number', type=str, default="3")           # numero di writers da passare ad archivio
    parser.add_argument('-r', '--readers_number',type=str, default="3")      # numero di readers da passare ad archivio
    parser.add_argument('-v', '--valgrind', nargs='?', type=int, const=1)  # flag valgrind se 1 allora uso valgrind
    args = parser.parse_args()
    if(args.thread <=0):
        print("Passare numero positivo di thread gestori della connessione")
        exit(1)
    #chiamo main
    main(args)


