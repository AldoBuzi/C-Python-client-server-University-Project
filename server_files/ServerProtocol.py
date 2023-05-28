from abc import ABC, abstractmethod
import logging

#
# CLASS AREA ServerProtocol
#
class ServerProtocol(ABC):
    def __init__(self, conn,addr):
        self.conn = conn
        self.addr = addr
        #config base del modulo logging
        logging.basicConfig(filename="server.log",
                    filemode='a',
                    format='%(asctime)s,%(msecs)d %(name)s %(levelname)s %(message)s',
                    datefmt='%H:%M:%S',
                    level=logging.DEBUG)
        #assegno il logger
        self.logger = logging.getLogger(__name__)
        self.numberOfBytes = 0
    def handleConnection(self):
        pass
    
    #Funzione per riceve i dati dal client
    def recv_all(self,conn,n):
        chunks = b''
        bytes_recd = 0
        while bytes_recd < n:
            chunk = conn.recv(min(n - bytes_recd, 1024))
            if len(chunk) == 0:
               raise RuntimeError("socket connection broken")
            chunks += chunk
            bytes_recd = bytes_recd + len(chunk)
        return chunks

    #Aggiorno il numero di byte scritti
    def updateBytes(self,val):
        self.numberOfBytes += val
    
    #Si occupa di trovare il tipo di connessione
    def findConnectionType(self,conn,addr, classType):
        data = self.recv_all(conn,1)
        assert len(data)==1
        inizio = data.decode("utf8")
        if inizio == 'a':
            return classType[0](conn, addr)
        elif inizio == 'b':
            return classType[1](conn,addr)
        else: raise "Tipo di connessione non riconosciuta"
    

    def closeConnection(self):
        self.conn.close()

# FINE CLASS AREA ServerProtocol