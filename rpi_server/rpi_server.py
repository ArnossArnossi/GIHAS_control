import json
import logging as lo
import socket
import sys
import time


class Handler(object):

    def __init__(self, server, port):
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
        self.root_log.info("Initialized logging.")

        self.server = server
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.bind_socket()


    def bind_socket(self):
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.root_log.info("Trying to bind socket...")
        for i in range(11):
            try:
                self.sock.bind((self.server, self.port))
                self.root_log.info("Socket binding successfull with address: {} on port {}".format(self.server, self.port))
                break
            except OSError:
                time.sleep(1)
                if i==10:
                    self.root_log.warn("Socket binding to adress: {} on port {} failed. Exiting programm...".format(self.server, self.port))
                    sys.exit()

    def getConn(self):
        self.sock.listen(1)
        self.root_log.info("Waiting for incoming connection...")
        self.sock.settimeout(30)
        conn, addr = self.sock.accept()
        self.root_log.info("Connected address is: {}".format(addr))
        self.sock.settimeout(None)
        return conn

    def __call__(self, conn):
        with conn.makefile(mode="r") as f:
            while True:
                line = f.readline().strip()
                self.root_log.info(line)

            
if __name__=="__main__":

    gihas_handler = Handler("192.168.2.10", 4000)
    try:
        conn = gihas_handler.getConn()
    except socket.timeout:
        gihas_handler.root_log.warn("Could not find connection with client after {} seconds. Exiting program...".format(gihas_handler.sock.gettimeout()))
        sys.exit()
    with conn:
        gihas_handler(conn)
    
