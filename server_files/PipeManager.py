import os

class PipeManager:
    
    def __init__(self, pipeName):
        self.name = pipeName
        self.pipeDescriptor = None
    def create(self):
        try:
            #Rimuovo la pipe se già esiste
            if os.path.exists(self.name):
                os.unlink(self.name)
            os.mkfifo(self.name,0o666)
        except OSError as e:
            print ("Failed to create FIFO: %s", e)

    def open(self,accessType = os.O_WRONLY):
        self.pipeDescriptor = os.open(self.name, accessType)
        
    def write(self,bytes):
        return os.write(self.pipeDescriptor, bytes)
    
    def close(self):
        os.close(self.pipeDescriptor)
        
    def unlink(self):
        os.unlink(self.name)

