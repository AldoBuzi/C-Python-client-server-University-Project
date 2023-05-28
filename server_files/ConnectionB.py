from ServerProtocol import ServerProtocol
import struct
import logging
import os

#
# CLASS AREA ConnectionB (extends ServerProtocol)
#
class ConnectionB(ServerProtocol):

    pipe = None

    def __init__(self,conn,addr):
        super().__init__(conn,addr)

    def handleConnection(self):
        while True:
            try:
                # leggo uno short come lunghezza della stringa
                data = self.recv_all(self.conn,2)
                assert len(data)==2
                length = struct.unpack("!h",data)[0]
                if length == 0: #il client ha scritto una stringa di lunghezza 0
                    break
                data = self.recv_all(self.conn,length)
                assert len(data)==length
                inizio = data.decode("utf-8") 
                g = ConnectionB.pipe.write(struct.pack("<h", length) + str.encode(inizio))
                #funzione ereditata dal padre
                self.updateBytes(len(struct.pack("<h", length) + str.encode(inizio)))
                if len(struct.pack("<h", length) + str.encode(inizio)) != g:
                    raise "Errore durante la scrittura in pipe"
            except Exception as e:
                print(e)
                break
        self.writeLog()
        #uso il metodo ereditato dal padre
        self.closeConnection()
    def writeLog(self):
        #uso la variabile logger ereditata dal padre per scrivere sul file di log
        self.logger.log(logging.DEBUG, f"Connesione di tipo A, scritti {self.numberOfBytes} bytes su capolet")


# FINE CLASS AREA Connection B