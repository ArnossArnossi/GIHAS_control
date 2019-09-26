#include "Controllino.h"
#include "ArduinoJson.h"
#include "ModbusRtu.h" 
#include <Ethernet.h>


const int MAX_INPUT_SIZE = 32; //18; // number of input elements
const int MAX_OUTPUT_SIZE = 35; //33; // number of outputs elements



byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC address for controllino.
IPAddress ip(192, 168, 2, 5); // the static IP address of controllino
IPAddress server(192,168,2,10);  // numeric IP of the server i.e. rpi

EthernetClient client; // start ethernet client


// array in which the digital states of all inputs and outputs of the slave controllino are stored
// Modbus sends and receives any data to the slave only from this array
uint16_t ModbusSlaveRegisters[52];

// The object ControllinoModbuSlave of the class Modbus is initialized with three parameters.
// The first specifies the address of the Modbus master device.
// The second specifies type of the interface used for communication between devices, RS485.
// The third can be any number. During the initialization of the object this parameter has no effect.
Modbus ControllinoModbusMaster(0, 3, 0);
modbus_t query;
// lookup array for index of pin in slave.
int slavePinLookup[69] = {-1, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, -1, -1, -1, -1, -1, -1, -1,
                          -1, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, 
                          -1, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,
                           7,  8,  9, 10, 11, 12, 13, 14, 15
                         };



// enough for 72 bytes from output array with "changedIndex":4 and "Type": "sensorevent" but not for totalIn
const size_t capacity = 650;
JsonObject & jsonBuild (String type){
  // build JsonObject with type denoting type of json message
  // the returned object should never live in the global scope
  // or else it wont be destroyed by garbage collection => memory leak 

  // initilize Json object with certain site. Size has to be
  // changed according to the size of the json document
  // 1200 is enough for 32 key value <str, int> pairs for the output and a bit more
  StaticJsonBuffer<capacity> jsonBuffer;
  // root is now the object which holds the json data an where data can
  // be appended and sent to Serial
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = type;
  return root;
}

void jsonSend(JsonObject & root){
  if (!Serial) {
    return;
  }
  // if the JSONbuffer is to small the program hangs at printTo() and the endl;
  // charecter after that never gets send
  root.printTo(client);
  // only needed if we seperate incoming streams via readline()f
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

// struct Dict {
//   String key;
//   byte value;

//   Dict(String key = "", byte value = 0) : key(key), value(value) {}
// };


// template <size_t N>
// struct ArInit {
//   // helper struct for "easy" input of parameters to map method
//   // easy here means needlesly complicated and fucking stupid
//     byte ar[N];
  
//   // custom constructor cannot be used because implicit list initialization
//   // would be overwritten and cannot be reimplemented because std cant be used

//     byte& operator[](byte i) {
//       return ar[i];
//     }
// };


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
      // constructor method with different name to call at a later time
      for (int i = 0; i < MAX_INPUT_SIZE; i++) {
        if (pinLayout[i].pinName[0] == 'M') {
          pinMode(pinLayout[i].pinNumber, INPUT);
        }
        inputData[i] = pinLayout[i];
        inputRepr[i] = inputData[i].pinState;
        // pinState is getting misused here a bit. The defined pinState
        // in pinLayout is the expected pinstate for normalInput
        // And the saved pinState could be different from the acctual state right now
        // which is needed so the checking befor normalOutput is set can be done
        normalInput[i] = pinLayout[i].pinState;
      }
    }

    int readInput(int inputNumber) {
      // Read state of input at postion inputNumber in inputLayout, check if request has to be send via rs485 first
      // get Name: if slave: 
      //              - lookup postion of pin on slave device in dict/hashfunction
      //                goal: order in which slave inputs are defined in pinLayout should not matter 
      //              - build request 
      //              - send request if no response: log no response event -> should be send out via sms
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
      // if any input is not as expected send index of that input otherwise send-1

      for (int i=0; i<MAX_INPUT_SIZE; i++) {
        if (readInput(i) != normalInput[i]){
          return i;
        }
      }
      return -1;
    }

    // int getPinID(String pinName) {
    //   // Return the position (i.e. id) of pin with name pinName
    //   // in inputData
    //   // Parameters: String pinName: name of pin to be found
    //   // Return: int, # element in inputData
    //   for (int i=0; i<MAX_INPUT_SIZE; i++) {
    //     if (inputData[i].pinName == pinName) {
    //       return i;
    //   String errorMessage = "No Input with name: " + pinName + " found.";
    //   JsonObject & root = jsonBuild("error");
    //   root["error"] = errorMessage;
    //   jsonSend(root);
    //   // no error catching is attempted
    //   // if the name is not found the program should fail horribly and obviously
    //   // NOTE: all mapings need to be tested beforehand in simulation
    //     }
    //   }
    // }

    // bool compare(byte (&toBeChecked)[MAX_INPUT_SIZE]) {
    //   //NOTE: the rechecking of changed readings is not implemented here because of all the overloading
    //   //      and because we dont use these function anyhow at the moment.
    //   //
    //   // Compares a given inputState with the current state of all inputpins.
    //   // Parameters: toBeChecked: reference to int array[MAX_INPUT_SIZE], can only have elments 0, 1 or 2.
    //   //                          0: check if pin on same position has state 0
    //   //                          1: check if pin on same position has state 1
    //   //                          2: do not check pin at all
    //   // Returns: true, if all 0s and 1s have corresponding state in correct position
    //   //          false, in all other cases. Also if only 2s are given in toBeChecked
      
    //   bool sensorEvent = false;
    //   update(); //update all inputs

    //   for (int i=0; i<MAX_INPUT_SIZE; i++) {
    //     if (toBeChecked[i] == 2) {
    //       // TODO check if this clause is actually stept into
    //       continue;
    //     }
    //     if (toBeChecked[i] != inputData[i].pinState) {
    //       return false;
    //     } else {
    //       sensorEvent = true;
    //     }
    //   }
    //   return sensorEvent;
    // }

    // bool compare(Dict toBeChecked) {
    //   // Overloaded function
    //   // Parameters: toBeChecked: Dict structure with key: name of pin
    //   //                                              value: expected state
    //   // Returns: true: if pin name with same state is found in inputData
    //   //          false: all other cases
    //   update();

    //   for (auto & j : inputData) {
    //     if (toBeChecked.key == j.pinName && toBeChecked.value == j.pinState) {
    //       return true;
    //     }
    //   }
    //   return false;
    // }

    // template <size_t N>
    // bool compare(Dict (&toBeChecked)[N]) {
    //   // Overloaded function
    //   // Parameters: toBeChecked: Dict array with key: name of pin
    //   //                                               value: expected state
    //   // Returns: true: if all pin names with same state are found in inputData
    //   //          false: all other cases, also if no names are found in inputData 

      
    //   bool sensorEvent = false;
    //   update();

    //   for (auto & i : toBeChecked) {
    //     for (auto & j : inputData) {

    //       if (i.key != j.pinName) continue;
    //       if (i.value != j.pinState) {
    //         return false;
    //       } else {
    //         sensorEvent = true;
    //       }
    //     }
    //   }
    //   return sensorEvent;
    // }

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
        // as in Inputs pinState is getting misused a bit
        normalOutput[i] = pinLayout[i].pinState;
        outputData[i].pinState = 0; // make sure all outputs are off initially

        if (pinLayout[i].pinName[0] == 'M') {
          pinMode(pinLayout[i].pinNumber, OUTPUT);
          digitalWrite(outputData[i].pinNumber, 0);
        }
      }
    }


    void writeOutput(int outputNumber, byte outputState) {
      // Check if output on postion outputnumber in outputPinLayout is in master or slave
      // write data or send query accordingly
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
    }


    void setOutput(byte (&configArray)[MAX_OUTPUT_SIZE]) {
      // Overloaded function to allow setting the entire output state with
      // one array. Must have same size AND ORDER as outputPinLayout.
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

    bool isAllreadySet(byte (&toBeChecked)[MAX_OUTPUT_SIZE]) {
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

    // int getPinID(String pinName) {
    //   // Return the position (i.e. id) of pin with name pinName
    //   // in in/ouputdata
    //   // Parameters: String pinName: name of pin to be found
    //   // Return: int, # element in input ot ouputData
    //   for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
    //     if (outputData[i].pinName == pinName) {
    //       return i;
    //   String errorMessage = "No Output with name: " + pinName + " found.";
    //   JsonObject & root = jsonBuild("error");
    //   root["error"] = errorMessage;
    //   jsonSend(root);
    //   // no error catching is attempted
    //   // if the name is not found the program should fail horribly and obviously
    //   // NOTE: all mapings need to be tested beforehand in simulation
    //     }
    //   }
    // }
    
    // void setOutput(String outputName, byte stateValue) {
    //   // set state of the Pin with outputName to stateValue
    //   // TODO: does not work with char pinName right now

    //   for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
    //     if (outputData[i].pinName == outputName) {
    //       writeOutput(i, stateValue);
    //       outputData[i].pinState = stateValue;
    //       outputRepr[i] = stateValue;
    //       JsonObject & root = jsonBuild("setOut");
    //       root[outputData[i].pinName] = stateValue;
    //       jsonSend(root);
    //       return;
    //     }
    //   }
    //   String errorMessage = "No Output with name: " + outputName + " found. Nothing is set.";
    //   JsonObject & root = jsonBuild("error");
    //   root["error"] = errorMessage;
    //   jsonSend(root);
    // }

    // template <size_t N>
    // void setOutput(Dict (&config)[N]) {
    //   // set multiple outputs at once 
    //   // Parameters:  config: reference to Dict array with names and desired states of muliple pins

    //   for (int i = 0; i<N; i++) {
    //     setOutput(config[i].key, config[i].value);
    //   }
    // }

//     bool isAllreadySet(Dict toBeChecked) {

//       byte tempArray[MAX_OUTPUT_SIZE] = {};
//       for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
//         if (toBeChecked.key == outputData[i].pinName) {
//           tempArray[i] = toBeChecked.value;
//         } else {
//           tempArray[i] = 2;
//         }
//       }
//       return isAllreadySet(tempArray);
//     }

//     template <size_t N>
//     bool isAllreadySet(Dict (&toBeChecked)[N]) {

//       byte tempArray[MAX_OUTPUT_SIZE] = {};
//       for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
//         for (auto & j : toBeChecked) {
//           if (j.key == outputData[i]) {
//             tempArray[i] = j.val;
//           } else {
//             tempArray[i] = 2;
//           }
//         }
//       }
//       return isAllreadySet(tempArray);

//     }

class Machine {

  private:

    Inputs input;
    Outputs output;

    byte mmapping[MAX_INPUT_SIZE][MAX_OUTPUT_SIZE];

  public:

    // flag to indicate wether a sensor is showing an error
    bool sensorError = false;
    // flag for connection status of ethernet (0 not connected, 1 connected, -1 timed out,
    // -2 invalid server, -3truncated, -4 invalid response)
    int ethernetConnected = 0;
    // timing start for rechecking of ethernet connection in milliseconds
    unsigned long timerReconnect = 0;
    // time between recheckings of ethernet connection in milliseconds
    int waitReconnect = 20000;

    Machine() {}

    void begin(Pin inputPinLayout[], Pin outputPinLayout[], byte mapping[MAX_INPUT_SIZE][MAX_OUTPUT_SIZE]) {
      // constructor method with different name to call after initialzation
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
      // This only works if the inputData is set to the normalInput in the beginning
      // and not the acctual state of the inputs pins.
      int index = input.checknormalInput();
      if (index == -1) {
        output.setNormalOutput();
      } else {
        sensorError = true;
        JsonObject & root = jsonBuild(F("startupError"));
        root[F("changedIn")] = index;
        jsonSend(root);
      }
    }

    void runAllMappings() {
      // Check if input has changed. If so set output to defined output in mapping.
      // Carefull: the output is also set if the input changes back to normal
      int changedIndex = input.getChanges();
      if (changedIndex != -1) {
        sensorError = true;
        if (!output.isAllreadySet(mmapping[changedIndex])){
          // i'm not sure if this has any benefit because right now everthing is only
          // set to 0s and when we use delayed responces thing will change again anyway
          output.setOutput(mmapping[changedIndex]);
        }

        JsonObject & root = jsonBuild(F("sensorEvent"));
        root[F("changedIn")] = changedIndex;
        jsonAddArray(root,F("totalOut"), output.outputRepr);
        jsonSend(root);
      }      
    }

    void checkConnection() {
      // Check if ethernet is connected.
      // If not and timer for retry has run down: try to reconnect

      // TODO: check the implementation of the timeout agains rollover of millis()
      if (!ethernetConnected && millis() >= timerReconnect) {
        Serial.println(F("Trying to reconnect..."));
        ethernetConnected = client.connect(server, 4000);
        timerReconnect = millis() + waitReconnect;
        Serial.print(F("Connectionstatus: "));
        Serial.println(ethernetConnected);
      }
    }

};

    // bool mapImpl(byte (&inputArray)[MAX_INPUT_SIZE], byte (&outputArray)[MAX_OUTPUT_SIZE]) {
    //   // Set all outputs to a given state if all inputs are in a given state.
    //   // Parameters: inputArray: int array of length MAX_INPUT_SIZE
    //   //             elements can be: 0: check if state of pin on same position
    //   //                                 in the inputPinLayout is 0
    //   //                              1: check if state of pin is 1
    //   //                              2: do not check state of pin
    //   //             outputArray: int array of length MAX_OUTPUT_SIZE
    //   //             elements can be: 0: set state to 0
    //   //                              1: set state to 1
    //   //                              2: do not set state
    //   // Returns: false: if output is not changed
    //   //          true: if output is changed
    //   //
    //   // Usage example:
    //   //    Dict in;
    //   //    ArInit<MAX_OUTPUT_SIZE> out;
    //   //    controller.map(in = {"sourceFlow_2", 1}, out = {0,0,0,2,2,2,2,2,2,2,0,2});

    //   if (output.isAllreadySet(outputArray)) {
    //     // do not repeat test if output is allready set anyway
    //     return false;
    //   }

    //   if (input.compare(inputArray)) {
    //     output.setOutput(outputArray);
    //     sensorError = true;
    //     JsonObject & root = jsonBuild("sensorEvent");
    //     jsonAddArray(root, "input", inputArray);
    //     jsonAddArray(root, "output", outputArray);
    //     jsonSend(root);
    //     return true;
    //   }
    //   return false;
    // }

    // bool map(ArInit<MAX_INPUT_SIZE> in, ArInit<MAX_OUTPUT_SIZE> out) {
    //   // NOTE: the rechecking of changed readings is not implemented here because of all the overloading
    //   //       and because we dont use these functions anyhow at the moment. 
    //   //
    //   // map for "easy" usage, meaning initialsation of in and out array inside map call
    //   return mapImpl(in.ar, out.ar);
    // }

    // bool map(Dict in, ArInit<MAX_OUTPUT_SIZE> out) {
    //   // Overoading of map. All overloaded mappings just refer
    //   // the logic to mapping-method with complete in and out arrays
    //   ArInit<MAX_INPUT_SIZE> inArray;
    //   for (int i=0; i<MAX_INPUT_SIZE; i++) inArray[i] = 2;
    //   inArray[input.getPinID(in.key)] = in.value;
    //   return mapImpl(inArray.ar, out.ar);
    // }

    // bool map(ArInit<MAX_INPUT_SIZE> in, Dict out) {
    //   // Overoading of map.
    //   ArInit<MAX_OUTPUT_SIZE> outArray;
    //   for (int i=0; i<MAX_OUTPUT_SIZE; i++) outArray[i] = 2;
    //   outArray[output.getPinID(out.key)] = out.value;
    //   return mapImpl(in.ar, outArray.ar);
    // }

    // bool map(Dict in, Dict out) {
    //   // Overoading of map.
    //   ArInit<MAX_INPUT_SIZE> inArray;
    //   for (int i=0; i<MAX_INPUT_SIZE; i++) inArray[i] = 2;
    //   inArray[input.getPinID(in.key)] = in.value;
    //   ArInit<MAX_OUTPUT_SIZE> outArray;
    //   for (int i=0; i<MAX_OUTPUT_SIZE; i++) outArray[i] = 2;
    //   outArray[output.getPinID(out.key)] = out.value;
    //   return mapImpl(inArray.ar, outArray.ar);
    // }


Machine controller;

void resetWrapper(){
  // for some reason non static member function cannot be used as interrupt routines
  controller.checkAndEnableNormalOutput();
}

void setup() {

  //interrupt routine for restarting all outputs after sensorevent has been fixed
  const int interruptPin = CONTROLLINO_IN0;
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), resetWrapper, RISING);


  // Setup the whole machine. All pin configurations go here!
  // The in/out-putPinLayout describes the normal (i.e. expected) input and output state
  // Remember to set MAX_INPUT_SIZE and MAX_OUTPUT_SIZE at the beginning of this
  // script correctly, i.e. length of input and output layout respectively.
  Pin inputPinLayout[] = {
    {F("M:B01"), CONTROLLINO_A0, 0},
    {F("M:B02"), CONTROLLINO_A1, 0},
    {F("M:B03"), CONTROLLINO_A2, 0},
    {F("M:B04"), CONTROLLINO_A3, 0},
    {F("M:B05"), CONTROLLINO_A4, 0},
    {F("M:B06"), CONTROLLINO_A5, 0},
    {F("M:B07"), CONTROLLINO_A6, 0},
    {F("M:B08"), CONTROLLINO_A7, 0},
    {F("M:B09"), CONTROLLINO_A8, 0},
    {F("M:B10"), CONTROLLINO_A9, 0},
    {F("M:B11"), CONTROLLINO_A10, 0},
    {F("M:B12"), CONTROLLINO_A11, 0},
    {F("M:B13"), CONTROLLINO_A12, 0},
    {F("M:B14"), CONTROLLINO_A13, 0},
    {F("M:B15"), CONTROLLINO_A14, 0},
    {F("M:B16"), CONTROLLINO_A15, 0},
    {F("S:B17"), CONTROLLINO_A0, 0},
    {F("S:B18"), CONTROLLINO_A1, 0},
    {F("S:B19"), CONTROLLINO_A2, 0},
    {F("S:B20"), CONTROLLINO_A3, 0},
    {F("S:B21"), CONTROLLINO_A4, 0},
    {F("S:B22"), CONTROLLINO_A5, 0},
    {F("S:B23"), CONTROLLINO_A6, 0},
    {F("S:B24"), CONTROLLINO_A7, 0},
    {F("S:B25"), CONTROLLINO_A8, 0},
    {F("S:B26"), CONTROLLINO_A9, 0},
    {F("S:B27"), CONTROLLINO_A10, 0},
    {F("S:B28"), CONTROLLINO_A11, 0},
    {F("S:B29"), CONTROLLINO_A12, 0},
    {F("S:B30"), CONTROLLINO_A13, 0},
    {F("S:B31"), CONTROLLINO_A14, 0},
    {F("S:B32"), CONTROLLINO_A15, 0}
  };

  // Pin outputPinLayout[] = {
  //   {"M:HS20_source1", CONTROLLINO_D0, 1},
  //   {"M:roots1_source1", CONTROLLINO_D1, 1},
  //   {"M:roots2_source1", CONTROLLINO_D2, 1},
  //   {"M:valve_source1", CONTROLLINO_D3, 1},
  //   {"M:backingpump_source1", CONTROLLINO_D4, 1},
  //   {"M:V0184_chop1", CONTROLLINO_D5, 1},
  //   {"M:valve_chop1", CONTROLLINO_D6, 1},
  //   {"M:backingpump_chop1", CONTROLLINO_D7, 1},
  //   {"M:HS20_source2", CONTROLLINO_D8, 1},
  //   {"M:roots1_source2", CONTROLLINO_D9, 1},
  //   {"M:roots2_source2", CONTROLLINO_D10, 1},
  //   {"M:valve_source2", CONTROLLINO_D11, 1},
  //   {"M:backingpump_source2", CONTROLLINO_D12, 1},
  //   {"M:V0183_chop2", CONTROLLINO_D13, 1},
  //   {"M:valve_chop2", CONTROLLINO_D14, 1},
  //   {"M:backingpump_chop2", CONTROLLINO_D15, 1},
  //   {"M:turbopump1_scatt", CONTROLLINO_D16, 1},
  //   {"M:turbopump2_scatt", CONTROLLINO_D17, 1},
  //   {"M:Ed100_scatt", CONTROLLINO_D18, 1},
  //   {"M:Ed63_scatt", CONTROLLINO_D19, 1},
  //   {"M:valve_scatt", CONTROLLINO_R0, 1},
  //   {"M:backinpump_scatt", CONTROLLINO_R1, 1},
  //   {"M:turbopump_pitot", CONTROLLINO_R2, 1},
  //   {"M:2xEd1001_tof", CONTROLLINO_R3, 1},
  //   {"M:valve_tof", CONTROLLINO_R4, 1},
  //   {"M:backingpump_tof", CONTROLLINO_R5, 1},
  //   {"M:2xEd1002_tof", CONTROLLINO_R6, 1},
  //   {"M:turbopump_det", CONTROLLINO_R7, 1},
  //   {"M:Ed63_det", CONTROLLINO_R8, 1},
  //   {"M:valve_det", CONTROLLINO_R9, 1},
  //   {"M:backingpump_det", CONTROLLINO_R10, 1},
  //   {"S:waterflow_1", CONTROLLINO_D2, 1},
  //   {"S:waterflow_2", CONTROLLINO_D5, 1},
  //   {"S:foz", CONTROLLINO_D8, 1},
  //   {"S:baz", CONTROLLINO_D9, 1}
  // };
  Pin outputPinLayout[] = {
    {F("M:G10"), CONTROLLINO_D0, 1},
    {F("M:G11"), CONTROLLINO_D1, 1},
    {F("M:G12"), CONTROLLINO_D2, 1},
    {F("M:G10"), CONTROLLINO_D3, 1},
    {F("M:G10"), CONTROLLINO_D4, 1},
    {F("M:G10"), CONTROLLINO_D5, 1},
    {F("M:G10"), CONTROLLINO_D6, 1},
    {F("M:G10"), CONTROLLINO_D7, 1},
    {F("M:G10"), CONTROLLINO_D8, 1},
    {F("M:G10"), CONTROLLINO_D9, 1},
    {F("M:G10"), CONTROLLINO_D10, 1},
    {F("M:G10"), CONTROLLINO_D11, 1},
    {F("M:G10"), CONTROLLINO_D12, 1},
    {F("M:G10"), CONTROLLINO_D13, 1},
    {F("M:G10"), CONTROLLINO_D14, 1},
    {F("M:G10"), CONTROLLINO_D15, 1},
    {F("M:G10"), CONTROLLINO_D16, 1},
    {F("M:G10"), CONTROLLINO_D17, 1},
    {F("M:G10"), CONTROLLINO_D18, 1},
    {F("M:G10"), CONTROLLINO_D19, 1},
    {F("M:G10"), CONTROLLINO_R0, 1},
    {F("M:G10"), CONTROLLINO_R1, 1},
    {F("M:G10"), CONTROLLINO_R2, 1},
    {F("M:G10"), CONTROLLINO_R3, 1},
    {F("M:G10"), CONTROLLINO_R4, 1},
    {F("M:G10"), CONTROLLINO_R5, 1},
    {F("M:G10"), CONTROLLINO_R6, 1},
    {F("M:G10"), CONTROLLINO_R7, 1},
    {F("M:G10"), CONTROLLINO_R8, 1},
    {F("M:G10"), CONTROLLINO_R9, 1},
    {F("M:G10"), CONTROLLINO_R10, 1},
    {F("S:G10"), CONTROLLINO_D2, 1},
    {F("S:G10"), CONTROLLINO_D5, 1},
    {F("S:G10"), CONTROLLINO_D8, 1},
    {F("S:G10"), CONTROLLINO_D9, 1}
  };

byte mapping[MAX_INPUT_SIZE][MAX_OUTPUT_SIZE] = {
//Input\Output :D00,D01,D02,D03,D04,D05,D06,D07,D08,D09,D10,D11,D12,D13,D14,D15,D16,D17,D18,D19,R00,R01,R02,R03,R04,R05,R06,R07,R08,R09,R10,S01,S02,S08,S09
                {0 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2}, // CONTROLLINO_A0

                {0 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2}, // CONTROLLINO_A1
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2}, // CONTROLLINO_A2
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 0 , 0 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2}, // CONTROLLINO_A3
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2 , 0 , 2 , 2}, // CONTROLLINO_A4
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2}, // CONTROLLINO_A5
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 2 , 0 , 2 , 2}, // CONTROLLINO_A6
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2}, // CONTROLLINO_A7
                
                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A8

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A9

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A10

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A11

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A12

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A13

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A14

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2},  // CONTROLLINO_A15

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

                {2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 0 , 2 , 0 , 0 , 2 , 2},  // S:CONTROLLINO_A?

};


  // start Serial and check if its connected correctly
  Serial.begin(9600);
  int startTime = millis();
  while(!Serial) {
    if (millis() - startTime >= 10000) {
      break;
    }
  }

  // start the Ethernet connection
  Ethernet.begin(mac, ip);
  // give the Ethernet a second to initialize
  delay(1000);

  // setup rs485 query and connection
  query.u8id = 1; // receiving (slave) adress is always one since we only have on device
  query.u16CoilsNo = 1; // always only read one register at a time
  ControllinoModbusMaster.begin(19200);     // baud-rate at 19200
  ControllinoModbusMaster.setTimeOut(5000); // if there is no answer in 5000 ms, roll over


  // enable connection with server (rpi) over port 4000
  Serial.println(F("Start connecting with ethernet..."));
  controller.ethernetConnected = client.connect(server, 4000);
  if (controller.ethernetConnected) {
    //TODO: change this to log message
    Serial.println(F("Connected."));
  } else {
    Serial.println(F("Connection failed."));
    controller.timerReconnect = millis();
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
