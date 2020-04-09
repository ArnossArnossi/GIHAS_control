import serial
import time


class GSM:
    def __init__(self, port_path, receiving_number, baud_rate=9600, timeout=5):
        """ 
        Parameters: port_path: (string) full file path
                        to active serial connection, e.g.: "/dev/ttyAMA0"
                    receiving_number: (string) number to send to in
                        international format, e.g.: +4917212312312
                    baud_rate: (int) baudrate of gsm module
                    timeout: (int) time in seconds handler waits for a response
        """
        self.ser = serial.Serial(port_path, baudrate = baud_rate, timeout=timeout)
        time.sleep(1)
        self.receiving_number = receiving_number

        # set module to sms text mode
        self.write('AT+CMGF=1')

        # choose sim card as primary sms storage space
        self.write('AT+CPMS="SM","SM","SM"')

    def read_response(self):
        """
        Listen for response from GSM module. Only expect one response per given command.
        readline() is a blocking call. readline timeout is set during __init__.
        Returns "" if timeout was reached. 
        """
        return self.ser.readline()

    def write(self, command):
        self.ser.write(bytes(command, "utf-8") + b'\r')
        # wait for module to process command
        time.sleep(3)

    def send_sms(self, message):
        self.write('AT+CMGS="{}"'.format(self.receiving_number)) # prepare sms
        self.write(message)
        self.ser.write(b'\x1a') # end sms
        return self.read_response().startswith("+CMGS")
