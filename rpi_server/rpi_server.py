import json
import logging as lo
import socket


class Handler(object):

    def __init__(self):
        #init logging
        self.root_log = lo.getLogger()
        formatter = lo.Formatter("%(asctime)s %(levelname)s %(message)s")
        #logging to file
        file_handler = lo.FileHandler("/home/pi/gihas_control.log")
        file_handler.setFormatter(formatter)
        self.root_log.addHandler(file_handler)
        #logging to stdout
        console_handler = lo.StreamHandler()
        console_handler.setFormatter(formatter)
        self.root_log.addHandler(console_handler)
        #set log level
        self.root_log.setLevel(lo.INFO)

        self.server_address = "192.168.2.10"
        self.port = 4000 

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) # correct default settings?
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((self.server_address, self.port))
        
    def getConn(self):
        self.sock.listen(1)
        print("Waiting for incoming connection...")
        conn, addr = self.sock.accept()
        print("Connected address is: {}".format(addr))
        return conn

    def listen(self, conn):
        with conn:
            f = conn.makefile(mode="r")

            while True:
                line = f.readline().strip()
                self.root_log.info(line)

    def __call__(self):
        mconn = self.getConn()
        self.listen(mconn)
            
if __name__=="__main__":

    gihas_handler = Handler()
    gihas_handler()
