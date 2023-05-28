from ServerProtocol import ServerProtocol 
from PipeManager import PipeManager
import logging
import struct
import struct

#
# AREA CLASS ConnectionA (extends ServerProtocol)
#
class ConnectionA(ServerProtocol):
    pipe =  None
    def __init__(self,conn,addr):
        self.conn = conn
        super().__init__(conn,addr)
    
    def handleConnection(self):
        try:
            #funzione del padre, leggo 2 byte (uno short per la lunghezza della stringa)
            data = self.recv_all(self.conn,2)
            assert len(data)==2
            length = struct.unpack("!h",data)[0]
            if length == -1:
                return
            data = self.recv_all(self.conn,length)
            assert len(data)==length
            inizio = data.decode("utf8") 
            g = ConnectionA.pipe.write(struct.pack("<h", length)+str.encode(inizio))
            self.updateBytes(len(struct.pack("<h", length) + str.encode(inizio)))
            self.writeLog()
            self.closeConnection()
            if g != len(struct.pack("<h", length) + str.encode(inizio)):
                raise "Errore durante la scrittura nella pipe"
        except Exception as e:
            print(e)

            
    def writeLog(self):
        self.logger.log(logging.DEBUG, f"Connesione di tipo B, scritti {self.numberOfBytes} bytes su caposc")


# FINE CLASS AREA ConnectionA