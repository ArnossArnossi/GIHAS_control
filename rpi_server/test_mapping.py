# on RPi this script is located in /home/pi not .../test
from .rpi_server import Handler
import gpiozero as go
import pytest
import socket
import sys
import time
import json
import signal

# setup timeout for test loop
class TimeoutException(Exception):
    pass

def timeout_handler(signum, frame):   # Custom signal handler
    raise TimeoutException 

# Change the behavior of SIGALRM
signal.signal(signal.SIGALRM, timeout_handler)


sim_inputs = [
    # GPIO pin numbers of RPi
    # order of elements correpsonds to
    # order of inputs in mapping matrix
    2, # M0
    3, # M1
    4, # M2
    14, # M3
    15, # M4
    18, # M5
    17, # M6
    27, # M7
    22, # S0
    23, # S1
    24, # S2
    10, # S3
    9, # S4
    25, # S5
    8, # S6
    7 # S7
    ]

mapping = [
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A0, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A1, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A2, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A3, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A4,
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A5, 
    [0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0], # CONTROLLINO_A6, 
    [0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0], # CONTROLLINO_A7, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A0, slave inputs starting here
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A1, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A2, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A3, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A4, 
    [0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2], # CONTROLLINO_A5, 
    [0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0], # CONTROLLINO_A6, 
    [0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0], # CONTROLLINO_A7, 
]

reset_pin = go.DigitalOutputDevice(21, active_high=False)

def init_outputs():
    for i, val in enumerate(sim_inputs):
        sim_inputs[i] = go.DigitalOutputDevice(val, active_high=False)

def disable_outputs():
    for val in sim_inputs:
        val.off()

def close_outputs():
    for val in sim_inputs:
        val.close()

def reset_plc():
    reset_pin.on()
    time.sleep(.5)
    reset_pin.off()


class TestMapping(object):

    def setup_class(self):
        init_outputs()
        self.handler = Handler("192.168.2.10", 4000, "/home/pi/testing_plc.log")
        self.msg_timeout = 10

    def teardown_class(self):
        close_outputs()

    def setup_method(self):
        self.test_success = False
        reset_plc()
        try:
            self.conn = self.handler.get_connection()
        except socket.timeout:
            self.handler.root_log.warn("Could not find connection with client after {} seconds. Exiting test...".format(self.handler.sock.gettimeout()))
            sys.exit()
        # wait for plc to finish starting up
        time.sleep(5)

    def teardown_method(self):
        disable_outputs()
        self.conn.close()

    @pytest.mark.parametrize("sensor_index, expected_output", [
        (0, mapping[0]),
        (1, mapping[1]),
        (2, mapping[2]),
        (3, mapping[3]),
        (4, mapping[4]),
        (5, mapping[5]),
        (6, mapping[6]),
        (7, mapping[7]),
        (8, mapping[8]),
        (9, mapping[9]),
        (10, mapping[10]),
        (11, mapping[11]),
        (12, mapping[12]),
        (13, mapping[13]),
        (14, mapping[14]),
        (15, mapping[15]),
    ])
    def test_map(self, sensor_index, expected_output):
        sim_inputs[sensor_index].on()
        with pytest.raises(TimeoutException):
            with self.conn.makefile(mode="r") as f:
                signal.alarm(self.msg_timeout)
                while True:
                    try:
                        line = f.readline().strip()
                        self.handler.root_log.info(line)
                    except TimeoutException:
                        if self.test_success:
                            raise TimeoutException
                        else:
                            raise AssertionError("No message type:sensorError received from PLC after {} sec.".format(self.msg_timeout))
                    
                    try:
                        msg = json.loads(line)
                    except (ValueError):
                        self.handler.root_log.warn("Could not translate message to json for sensor: {}, message: {}".format(sensor_index, line))
                    
                    if msg["type"] == "sensorEvent":
                        # actual testing
                        assert msg["changedIn"] == sensor_index , "Wrong input has been triggered: {}".format(msg["changedIn"])
                        for i, val in enumerate(expected_output):
                            if val != 2:
                                assert val == msg["totalOut"][i], "Wrong output state on index: {}".format(i)
                        self.test_success = True
