#include "Controllino.h"
#include "ArduinoJson.h"
#include "ModbusRtu.h" 
#include <Ethernet.h>


const int MAX_INPUT_SIZE = 16; // number of input elements
const int MAX_OUTPUT_SIZE = 10; // number of outputs elements


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC address for controllino.
IPAddress ip(192, 168, 2, 5); // the static IP address of controllino
IPAddress server(192,168,2,10);  // numeric IP of the server i.e. rpi
EthernetClient client; // start ethernet client


// array in which the digital states of all inputs and outputs of the slave controllino are stored
// Modbus sends and receives any data to the slave only from this array
uint16_t ModbusSlaveRegisters[52];
Modbus ControllinoModbusMaster(0, 3, 0);
modbus_t query;
// receiving (slave) adress
query.u8id = 1;
// always only read one register at a time
query.u16CoilsNo = 1;

// lookup array for index of pin in slave.
int slavePinLookup[69] = {-1, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, -1, -1, -1, -1, -1, -1, -1,
                          -1, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, 
                          -1, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,
                           7,  8,  9, 10, 11, 12, 13, 14, 15
                         };


// enough for 72 bytes from output array with
// "changedIndex":4 and "Type": "sensorevent" but not for totalIn
const size_t capacity = 650;
JsonObject & jsonBuild (String type){
  // the returned object should never live in the global scope
  // or else it wont be destroyed by garbage collection => memory leak
  // Parameter: type of json message
  // Return: JSON Object where data can be appended to

  StaticJsonBuffer<capacity> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = type;
  return root;
}


void jsonSend(JsonObject & root){
  // if the JSONbuffer is to small the program hangs at printTo()
  root.printTo(client);
  client.println();
}


template <size_t N> void jsonAddArray(JsonObject & someRoot, String name, byte (&ar)[N]) {
  // Add an int array to a json object.
  // Parameters: someRoot, json object
  //             name, name of the array
  //             ar, reference to in array to be stored          
  JsonArray& someArray = someRoot.createNestedArray(name);
  for (int i=0; i<N; i++) {
    someArray.add(ar[i]);
  }
}


struct Pin {
  String pinName;
  byte pinNumber;
  byte pinState;
};


class Inputs {

  private:
    Pin inputData[MAX_INPUT_SIZE];
    // Timeout for waiting for an answer from slave
    int slaveInputTimeout = 1000;
    // expected values for input. Will be set at initiation of Input
    byte normalInput[MAX_INPUT_SIZE];


  public:
    // should be initialized to array of zeros.
    byte inputRepr[MAX_INPUT_SIZE] = {};


    Inputs() {}


    void begin(Pin pinLayout[]) {
      // constructor method with different name to call in setup()
      for (int i = 0; i < MAX_INPUT_SIZE; i++) {
        if (pinLayout[i].pinName[0] == 'M') {
          pinMode(pinLayout[i].pinNumber, INPUT);
        }
        inputData[i] = pinLayout[i];
        normalInput[i] = pinLayout[i].pinState;
        inputRepr[i] = inputData[i].pinState;
      }
    }


    int readInput(int inputNumber) {
      // Read state of input at postion inputNumber in inputLayout,
      // check if request has to be send via rs485 first
      // Return -1 if error is detected.
      Pin mpin = inputData[inputNumber];
      if (mpin.pinName[0] == 'M') {
        return digitalRead(inputData[inputNumber].pinNumber);
      }
      else if (mpin.pinName[0] == 'S') {
        int registerIndex = slavePinLookup[mpin.pinNumber - 1];

        if (registerIndex == -1) {
          String errorMessage = F("Slave has no pin with this pin number. Ignoring this input.");
          JsonObject & root = jsonBuild(F("error"));
          root[F("error")] = errorMessage;
          root[F("pinNumber")] = mpin.pinNumber;
          jsonSend(root);
          return -1;
        }
      
        query.u8fct = 3;
        query.u16RegAdd = registerIndex;
        query.au16reg = ModbusSlaveRegisters + registerIndex;

        ControllinoModbusMaster.query(query);

        unsigned long startTime = millis();
        while (ControllinoModbusMaster.getState() != COM_IDLE && millis() - startTime < slaveInputTimeout) {
          ControllinoModbusMaster.poll();
        }
        return ModbusSlaveRegisters[registerIndex];
      }
      else  {
          String errorMessage = F("Name not starting with M or S. Ignoring this input");
          JsonObject & root = jsonBuild(F("error"));
          root[F("error")] = errorMessage;
          root[F("pinNumber")] = mpin.pinName;
          jsonSend(root);
          return -1;
      }
    }


    void update() {
      // Update the states of all pins in inputData to current value.

      for (int i = 0; i < MAX_INPUT_SIZE; i++) {
        inputRepr[i] = inputData[i].pinState = readInput(i);
      }
    }


    int getChanges() {
      // Check if any input state has changed to something different than the
      // expected input. If so return the index of that input pin
      // if input changed: wait x ms and recheck the reading. If it changed again, Dont do anything
      // this allows for flipping of the switches from manual to auto without triggering something

      // Returns: int, index of changed input pin or -1 if no input has changed.

      for (int i=0; i<MAX_INPUT_SIZE; i++) {
        int tempInput = readInput(i);
        if (inputData[i].pinState != tempInput) {
          delay(200);
          if (inputData[i].pinState == readInput(i)){
            continue;
          }
          inputRepr[i] = inputData[i].pinState = tempInput;
          if (tempInput!=normalInput[i]) {
            return i;
          }
        }
      }
      return -1;
    }


    int checknormalInput(){
      // Check if input is the same as normalInput defined in setup()
      // if any input is not as expected send index of that input otherwise send -1

      for (int i=0; i<MAX_INPUT_SIZE; i++) {
        if (readInput(i) != normalInput[i]){
          return i;
        }
      }
      return -1;
    }
};


class Outputs {

  private:
    Pin outputData[MAX_OUTPUT_SIZE];
    byte normalOutput[MAX_OUTPUT_SIZE];
    // timeout length before response from slave is ignored
    int slaveOutputTimeout = 1000;


  public:
    // should be initialized to array of zeros.
    byte outputRepr[MAX_OUTPUT_SIZE] = {};


    Outputs() {}
    

    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time
      for (int i = 0; i < MAX_OUTPUT_SIZE; i++) {
        outputData[i] = pinLayout[i];
        normalOutput[i] = pinLayout[i].pinState;


        outputRepr[i] = outputData[i].pinState = 0; // make sure all outputs are off initially
        if (pinLayout[i].pinName[0] == 'M') {
          pinMode(pinLayout[i].pinNumber, OUTPUT);
          digitalWrite(outputData[i].pinNumber, 0);
        }
      }
    }


    void writeOutput(int outputNumber, byte outputState) {
      // Check if output on postion outputnumber in outputPinLayout
      // is in master or slave, write data or send query accordingly
      Pin mpin = outputData[outputNumber];
      if (mpin.pinName[0] == 'M') {
        digitalWrite(mpin.pinNumber, outputState);
      }
      else if (mpin.pinName[0] == 'S') {
        int registerIndex = slavePinLookup[mpin.pinNumber - 1];

        if (registerIndex == -1) {
          String errorMessage = "Slave has no pin with pin number: " + String(mpin.pinNumber)
                                 + "Ignoring this query.";
          JsonObject & root = jsonBuild("error");
          root["error"] = errorMessage;
          jsonSend(root);
          return;
        }

        query.u8fct = 6;
        query.u16RegAdd = registerIndex;
        ModbusSlaveRegisters[registerIndex] = outputState;
        query.au16reg = ModbusSlaveRegisters + registerIndex;

        ControllinoModbusMaster.query(query);

        unsigned long startTime = millis();
        while(ControllinoModbusMaster.getState() != COM_IDLE &&  millis() - startTime < slaveOutputTimeout) {
          ControllinoModbusMaster.poll();
        }
      }
      else  {
        String errorMessage = F("Name not starting with M or S. Ignoring this input");
        JsonObject & root = jsonBuild(F("error"));
        root[F("error")] = errorMessage;
        root[F("pinNumber")] = mpin.pinName;
        jsonSend(root);
        return;
      }
    }


    void setOutput(byte (&configArray)[MAX_OUTPUT_SIZE]) {
      // Allow setting the entire output state with one array. 
      // Must have same size AND ORDER as outputPinLayout.
      // Elements can be  0: set outputpin with same index to 0
      //                  1: set outputpin with same index to 1
      //                  2: dont do anything with that pin
      // Parameters: configArray: reference to int array with
      //                          MAX_OUTPUT_SIZE as size

      for (int i = 0; i < MAX_OUTPUT_SIZE; i++) {
        if (configArray[i] < 2) {
          writeOutput(i, configArray[i]);
          outputData[i].pinState = configArray[i];
          outputRepr[i] = configArray[i];
        }
      }
      JsonObject & root = jsonBuild("setAllOutputs");
      jsonAddArray(root, "changedOut", configArray);
      jsonSend(root);
    }


    void setNormalOutput() {
      setOutput(normalOutput);
    }


    bool isAlreadySet(byte (&toBeChecked)[MAX_OUTPUT_SIZE]) {
      // Check if the desired output array "toBeChecked" is allready
      // set in the current output.
      // Parameters:  toBeChecked: ref to int array of MAX_OUTPUT_SIZE in length
      //              can only have elements: 0: pin set to 0
      //                                      1: pin set to 1
      //                                      2: do not check this pin
      // Returns: false: if output is different or if only 2s are in toBeChecked
      //          true: if output is same at positions with 0s and 1s,

      bool isSet = false;

      for (int i = 0; i < MAX_OUTPUT_SIZE; i++) {
        if (toBeChecked[i] == 2) {
          continue;
        }
        if (toBeChecked[i] != outputData[i].pinState) {
          return false;
        } else {
          isSet = true;
        }
      }
      return isSet;
    }
};

class Machine {

  private:
    Inputs input;
    Outputs output;
    byte mmapping[MAX_INPUT_SIZE][MAX_OUTPUT_SIZE];


  public:
    // timing start for rechecking of ethernet connection in milliseconds
    unsigned long startTimeReconnect = 0;
    // time between recheckings of ethernet connection in milliseconds
    int waitReconnect = 20000;


    Machine() {}


    void begin(Pin inputPinLayout[],
               Pin outputPinLayout[],
               byte mapping[MAX_INPUT_SIZE][MAX_OUTPUT_SIZE]) {
      // constructor method with different name to call in setup()
      input.begin(inputPinLayout);
      output.begin(outputPinLayout);

      for (int i=0; i<MAX_INPUT_SIZE; i++) {
        for (int j=0; j<MAX_OUTPUT_SIZE; j++) {
          mmapping[i][j] = mapping[i][j];
        }
      }
    }


    void checkAndEnableNormalOutput() {
      // Check if input is different from normalInput, i.e. the expected input states.
      int index = input.checknormalInput();
      if (index == -1) {
        output.setNormalOutput();
      } else {
        JsonObject & root = jsonBuild(F("startupError"));
        root[F("changedIn")] = index;
        jsonSend(root);
      }
    }


    void runAllMappings() {
      // Check if input has changed. If so set output to defined output in mapping.
      int changedIndex = input.getChanges();
      if (changedIndex != -1) {
        if (!output.isAlreadySet(mmapping[changedIndex])){
          output.setOutput(mmapping[changedIndex]);
        }

        Serial.println("sensorEvent on Pin: " + changedIndex);
        JsonObject & root = jsonBuild(F("sensorEvent"));
        root[F("changedIn")] = changedIndex;
        jsonAddArray(root,F("totalOut"), output.outputRepr);
        jsonSend(root);
      }      
    }

    void checkConnection() {
      // Check if ethernet is connected.
      // If not and timer for retry has run down: try to reconnect

      if (!client.connected() && millis() - startTimeReconnect >= waitReconnect) {
        startTimeReconnect = millis();
        Serial.println(F("Trying to reconnect..."));
        client.stop();
        client.connect(server, 4000);
        Serial.print(F("Connectionstatus: "));
        Serial.println(client.connected());
      }
    }
};


Machine controller;


void resetWrapper(){
  // for some reason non static member function cannot be used as interrupt routines
  controller.checkAndEnableNormalOutput();
}


void setup() {

  // Setup the whole machine. All pin configurations go here!

  // The in/out-putPinLayout describes the normal (i.e. expected) input and output state
  // Remember to set MAX_INPUT_SIZE and MAX_OUTPUT_SIZE at the beginning of this
  // script correctly, i.e. length of input and output layout respectively.
  // Pin Name has to start with M or S depending on master or slave device
  // or pin will be ignored otherwise. 
  Pin inputPinLayout[] = {
    {F("M:B01"), CONTROLLINO_A0, 0},
    {F("M:B02"), CONTROLLINO_A1, 0},
    {F("M:B03"), CONTROLLINO_A2, 0},
    {F("M:B04"), CONTROLLINO_A3, 0},
    {F("M:B05"), CONTROLLINO_A4, 0},
    {F("M:B06"), CONTROLLINO_A5, 0},
    {F("M:B07"), CONTROLLINO_A6, 0},
    {F("M:B07"), CONTROLLINO_A7, 0},
    {F("S:B01"), CONTROLLINO_A0, 0},
    {F("S:B02"), CONTROLLINO_A1, 0},
    {F("S:B03"), CONTROLLINO_A2, 0},
    {F("S:B04"), CONTROLLINO_A3, 0},
    {F("S:B05"), CONTROLLINO_A4, 0},
    {F("S:B06"), CONTROLLINO_A5, 0},
    {F("S:B07"), CONTROLLINO_A6, 0},
    {F("S:B08"), CONTROLLINO_A7, 0},
  };

  Pin outputPinLayout[] = {
    {F("M:G31"), CONTROLLINO_R2, 1},
    {F("M:G67"), CONTROLLINO_R3, 1},
    {F("M:G32"), CONTROLLINO_R4, 1},
    {F("M:G33"), CONTROLLINO_R5, 1},
    {F("M:G68"), CONTROLLINO_R6, 1},
    {F("M:G34"), CONTROLLINO_R7, 1},
    {F("M:G64"), CONTROLLINO_R8, 1},
    {F("M:G35"), CONTROLLINO_R9, 1},
    {F("M:G36"), CONTROLLINO_R10, 1},
    {F("M:G42"), CONTROLLINO_R11, 1},
  };

// Each row coresponds to input with same index in inputLayout.
// And order of Elements in one row has to be the same as outputLayout.
// Note: Remember to update MAX_INPUT_SIZE and MAX_OUTPUT_SIZE at BEGINNING of script
byte mapping[MAX_INPUT_SIZE][MAX_OUTPUT_SIZE] = {
//Input\Output :R02,R03,R04,R05,R06,R07,R08,R09,R10,R11
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A0, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A1, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A2, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A3, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A4,
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A5, 
                {0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0}, // CONTROLLINO_A6, 
                {0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0}, // CONTROLLINO_A7, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A0, slave inputs starting here
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A1, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A2, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A3, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A4, 
                {0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2}, // CONTROLLINO_A5, 
                {0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0}, // CONTROLLINO_A6, 
                {0 , 0 , 2 , 2 , 0 , 0 , 0 , 2 , 2 , 0}, // CONTROLLINO_A7, 
};


 //interrupt routine for restarting all outputs after sensorevent has been fixed
  const int interruptPin = CONTROLLINO_IN0;
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), resetWrapper, RISING);


  // start Serial and check if its connected correctly
  Serial.begin(9600);
  int startTime = millis();
  while(!Serial) {
    if (millis() - startTime >= 5000) {
      break;
    }
  }


  // start the Ethernet connection
  Ethernet.begin(mac, ip);
  // give the Ethernet a second to initialize
  delay(1000);


  // setup rs485 query and connection
  ControllinoModbusMaster.begin(19200); // baud-rate at 19200
  ControllinoModbusMaster.setTimeOut(5000); 


  // enable connection with server (rpi) over port 4000
  Serial.println(F("Start connecting with ethernet..."));
  if (client.connect(server, 4000)) {
    //TODO: change this to log message
    Serial.println(F("Connected."));
  } else {
    Serial.println(F("Connection failed."));
    controller.startTimeReconnect = millis();
  }


  // initilize controller with input and output pins
  // set inputs to expected inputs tempoarily, set outputs to 0
  controller.begin(inputPinLayout, outputPinLayout, mapping);
  // Wait a bit. seems to be needed to set everything up correctly
  delay(500);

  // Check if inputs do not show an error before enableing normal operation
  controller.checkAndEnableNormalOutput();
}


void loop() {

  controller.runAllMappings();

  controller.checkConnection();

}
