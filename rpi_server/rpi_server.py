import json
import logging as lo
import socket
import sys
import time
from GSM_Handler import GSM



class Handler(object):

    def __init__(self, server, port, log_file_path, sms_sender_func = None, binding_timeout = 20):
        #init logging
        self.root_log = lo.getLogger()
        formatter = lo.Formatter("%(asctime)s %(levelname)s %(message)s")
        #logging to file
        file_handler = lo.FileHandler(log_file_path)
        file_handler.setFormatter(formatter)
        self.root_log.addHandler(file_handler)
        #logging to stdout
        console_handler = lo.StreamHandler()
        console_handler.setFormatter(formatter)
        self.root_log.addHandler(console_handler)
        #set log level
        self.root_log.setLevel(lo.INFO)
        self.root_log.info("Initialized logging.")

        self.binding_timeout = binding_timeout
        self.server = server
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.bind_socket()

        if sms_sender_func is not None:
            self.sms_func = sms_sender_func
        else:
            self.sms_func = self.default_sms_func
    
    def __call__(self, conn):
        with conn.makefile(mode="r") as f:
            while True:
                line = f.readline().strip()
                self.root_log.info(line)
                success = self.sms_func(line) # send entire json as sms
                self.root_log.info("Tried to send SMS. Success: {}".format(success))


    def default_sms_func(self, message):
        pass

    def bind_socket(self):
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.root_log.info("Trying to bind socket...")
        for i in range(self.binding_timeout + 1):
            try:
                self.sock.bind((self.server, self.port))
                self.root_log.info("Socket binding successfull with address: {} on port {}".format(self.server, self.port))
                break
            except OSError:
                time.sleep(1)
                if i==self.binding_timeout:
                    self.root_log.warn("Socket binding to adress: {} on port {} failed. Exiting programm...".format(self.server, self.port))
                    sys.exit()

    def get_connection(self):
        self.sock.listen(1)
        self.root_log.info("Waiting for incoming connection...")
        self.sock.settimeout(40)
        conn, addr = self.sock.accept()
        self.root_log.info("Connected address is: {}".format(addr))
        self.sock.settimeout(None)
        return conn

            
if __name__=="__main__":

    gsm_handler= GSM("/dev/ttyAMA0", "+4917256187924")
    gihas_handler = Handler("192.168.2.10", 4000,
                            "/home/pi/gihas_control.log",
                            sms_sender_func=gsm_handler.send_sms)

    try:
        conn = gihas_handler.get_connection()
    except socket.timeout:
        gihas_handler.root_log.warn("Could not find connection with client after {} seconds. Exiting program...".format(gihas_handler.sock.gettimeout()))
        sys.exit()

    with conn:
        gihas_handler(conn)
    
