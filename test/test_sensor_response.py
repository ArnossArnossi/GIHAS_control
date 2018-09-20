import serial
import gpiozero as go
import json
import time
import pytest


def reset_serial_and_controllino(ser):
    print("resetting serial connection")
    ser.close()
    time.sleep(1)
    ser.open()
    # this should work faster. does the control really need 10 sec to reset?
    time.sleep(10)

def enable_connection():
    #readline reads characters until endl is found OR until timeout is reached
    return serial.Serial(port="/dev/ttyACM0", baudrate=9600, timeout=1)


def init_outputs():
    for key, val in sim_inputs.items():
        sim_inputs[key] = go.DigitalOutputDevice(val, active_high=False)

def disable_outputs():
    for key in sim_inputs:
        sim_inputs[key].off()

def close_outputs():
    for val in sim_inputs.values():
        val.close()

def compare_output(expected_output: dict, received_output: dict):
    """ compares dictionaries of ouptuts received from controllino with
        predefined expected outputs.
        Checks weather expected keys are all present and values are set correctly
        Only checks the outputs defined in expected_output and nothing else.
        Parameters: expected_output: dict, ouputs to be checked
                    received_output: dict, raw dict received from controllino
    """
    for key, val in expected_output.items():
        assert key in received_output, "key {} not found in received output".format(key)
        msg = "received output {} is set to {}".format(key, received_output[key])
        assert val == received_output[key], msg

sim_inputs = {
    # values are output pins on RPi board
    # the will be switched out with corresponding output objects

    # pin number correspond to wireing of relayboard
    # see controllino_relay_RPi.txt
        "sourceFlow_1" : 2,         # k1 on relay board
        "sourceFlow_2" : 3,         # k2 on relay board
        "chamberFlow_1" : 4,        # k3 on relay board
        "chamberFlow_2" : 14,       # etc
        "detectorFlow_1" : 15,
        "detectorFlow_2" : 18,
        "sourceGroundH2O" : 17,
        "chamberGroundH2O" : 27,
        "detectorGroundH2O" : 22,
        }

source_outputs = { 
        "heliumSource" : 0,
        "sourcePump_1" : 0,
        "sourcePump_2" : 0,
        "sourceH2O" : 0,
        "vacuumChamberValve" : 0
        }

chamber_outputs = {
        "heliumSource" : 0,
        "chamberPump_1" : 0,
        "chamberPump_2" : 0,
        "chamberH2O" : 0,
        "vacuumChamberValve" : 0,
        "vacuumDetectorValve" : 0
        }

detector_outputs = {
        "heliumSource" : 0,
        "detectorPump_1" : 0,
        "detectorPump_2" : 0,
        "detectorH2O" : 0,
        "vacuumDetectorValve" : 0
        }

class TestSensorResponse(object):
    """
    Start with tests of sensor input from normal operation
    => set contrl state to normal before each test...
        - reseting control paramaters?
        - controlparameters are used to decide which sensors are still read out
          because controlino should never reenable outputs automatically

    """

    def capture_response(self, message_type):
        # read response from controllino and return unpacked json dict
        # NOTE: This will only listen for x seconds.
        #       Not suitable for timed shutdown processes.
        timeout = time.time() + 5 # timeout after 5 seconds
        while time.time() < timeout:
            # this reads from serial until endl character is found or timeout of Serial
            if self.ser.in_waiting > 0:
                line = self.ser.readline()
                try:
                    print(line)
                    data = json.loads(str(line, "ascii"))
                    if data["type"] == message_type: return data
                except ValueError as e:
                    print(e)

    def setup_class(self):
        self.ser = enable_connection()
        init_outputs()

    def teardown_class(self):
        close_outputs()
        self.ser.close()

    def setup_method(self):
        # this should reset the controllino -> wait a bit for reseting
        reset_serial_and_controllino(self.ser)

    def teardown_method(self):
        # set all output pins to off (meaning high with active_low enabled
        disable_outputs()

    @pytest.mark.parametrize("sensor,response_type,expected_output", [
        ("sourceGroundH2O", "shutdownSource", source_outputs),
        ("chamberGroundH2O", "shutdownChamber", chamber_outputs),
        ("detectorGroundH2O", "shutdownDetector", detector_outputs),
        ])
    def test_waterspill(self, sensor, response_type, expected_output):
        """ simulate water spill for all groundwatersensors
            expected response:  - emergency shutdown signal
                                - check if all the right outputs are low
        """
        sim_inputs[sensor].on()
        received_data = self.capture_response(response_type)
        assert type(received_data) is dict, "correct response was not found until timeout"
        compare_output(expected_output, received_data)
        



        



















                                
    
