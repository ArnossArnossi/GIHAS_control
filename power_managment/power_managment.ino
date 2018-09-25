#include "Controllino.h"
#include "ArduinoJson.h"



const int MAX_INPUT_SIZE = 9;
const int MAX_OUTPUT_SIZE = 12;


JsonObject & buildJson (String type){
  // build JsonObject with type denoting type of json message
  // the returned object should never live in the global scope
  // or else it wont be destroyed by garbage collection => memory leak 

  // initilize Json object with certain site. Size has to be
  // changed according to the size of the json document
  // 1200 is enough for 32 key value <str, int> pairs for the output and a bit more
  StaticJsonBuffer<1200> jsonBuffer;
  // root is now the object which holds the json data an where data can
  // be appended and sent to Serial
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = type;
  return root;
}

void sendJson(JsonObject & root){
  if (!Serial) {
    return;
  }
  // if the JSONbuffer is to small the program hangs at printTo() and the endl;
  // charecter after that never gets send
  root.printTo(Serial);
  // only needed if we seperate incoming streams via readline()f
  Serial.println();
}


//TODO make name and number constant again somehow
struct Pin {
  String pinName;
  int pinNumber;
  int pinState;
};


struct Dict {
  String key;
  int value;
};


class Inputs {

  private:
    Pin inputData[MAX_INPUT_SIZE];
 
  public:
    Inputs() {}

    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time

      for (int i = 0; i < MAX_INPUT_SIZE; i++) {
        pinMode(pinLayout[i].pinNumber, INPUT);
        inputData[i] = pinLayout[i];
        // TODO:rethink if it is really needed to read here
        inputData[i].pinState = digitalRead(inputData[i].pinNumber);
      }
    }

    void update() {
      // Update the states of all pins in inputData to current value.

      for (auto & pin : inputData) {
        pin.pinState = digitalRead(pin.pinNumber);
      }
    }
    
    const Pin * getAllInputs() {
      // read out all inputs, store them in inputData and return constant reference to them
      // and because c++ is being a bitch one has to pass a pointer to inputData instead
      // of a reference and the number of elements is lost in the translation
      // it feels like the whole class structure is wrong and that one shouldnt need to pass
      // an array around anyway.

      update();
      return inputData;
    }

    int getInputState(String sensor) {
      // return current value for sensor by string as int
      // TODO: maybe also return name and pin but only if needed

      for (auto & i : inputData) {
        if (i.pinName == sensor) {
          i.pinState = digitalRead(i.pinNumber);
          return i.pinState;
        }
      }
      String errorMessage = "No Input with name: " + sensor + " found. Returning 0.";
      JsonObject & root = buildJson("error");
      root["error"] = errorMessage;
      sendJson(root);
      return 0;
    }

    bool compare(int (&toBeChecked)[MAX_INPUT_SIZE]) {
      // Compares a given inputState with the current state of all inputpins.
      // Parameters: toBeChecked: reference to int array[MAX_INPUT_SIZE], can only have elments 0, 1 or 2.
      //                          0: check if pin on same position has state 0
      //                          1: check if pin on same position has state 1
      //                          2: do not check pin at all
      // Returns: true, if all 0s and 1s have corresponding state in correct position
      //          false, in all other cases. Also if only 2s are given in toBeChecked
      
      bool sensorEvent = false;
      update(); //update all inputs

      for (int i=0; i<MAX_INPUT_SIZE; i++) {
        if (toBeChecked[i] == 2) {
          continue;
        }
        if (toBeChecked[i] != inputData[i].pinState) {
          return false;
        } else {
          sensorEvent = true;
        }
      }
      return sensorEvent;
    }

    bool compare(Dict toBeChecked) {
      // Overloaded function
      // Parameters: toBeChecked: Dict structure with key: name of pin
      //                                              value: expected state
      // Returns: true: if pin name with same state is found in inputData
      //          false: all other cases 
      update();

      for (auto & j : inputData) {
        if (toBeChecked.key == j.pinName && toBeChecked.value == j.pinState) {
          return true;
        }
      }
      return false;
    }

    template <size_t N>
    bool compare(Dict (&toBeChecked)[N]) {
      // Overloaded function
      // Parameters: toBeChecked: Dict array with key: name of pin
      //                                               value: expected state
      // Returns: true: if all pin names with same state are found in inputData
      //          false: all other cases, also if no names are found in inputData 

      
      bool sensorEvent = false;
      update();

      for (auto & i : toBeChecked) {
        for (auto & j : inputData) {

          if (i.key != j.pinName) continue;
          if (i.value != j.pinState) {
            return false;
          } else {
            sensorEvent = true;
          }
        }
      }
      return sensorEvent;
    }

};


class Outputs {

private:
    Pin outputData[MAX_OUTPUT_SIZE];

  
public:
    Outputs() {}
    
    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time
      for (int i = 0; i < MAX_OUTPUT_SIZE; i++) {
        pinMode(pinLayout[i].pinNumber, OUTPUT);
        outputData[i] = pinLayout[i];
        outputData[i].pinState = 0; // make sure all outputs are off initially
        digitalWrite(outputData[i].pinNumber, 0);
      }
    }
    
    void setOutput(String outputName, int stateValue) {
      // set state of the Pin with outputName to stateValue

      for (auto& pin : outputData) {
        if (pin.pinName == outputName) {
          digitalWrite(pin.pinNumber, stateValue);
          pin.pinState = stateValue;
          JsonObject & root = buildJson("setOut");
          root[pin.pinName] = stateValue;
          sendJson(root);
          return;
        }
      }
      String errorMessage = "No Output with name: " + outputName + " found. Nothing is set.";
      JsonObject & root = buildJson("error");
      root["error"] = errorMessage;
      sendJson(root);
    }

    template <size_t N>
    void setOutput(Dict (&config)[N]) {
      // set multiple outputs at once 
      // Parameters:  config: reference to Dict array with names and desired states of muliple pins

      for (int i = 0; i<N; i++) {
        setOutput(config[i].key, config[i].value);
      }
    }

    void setOutput(int (&configArray)[MAX_OUTPUT_SIZE]) {
      // Overloaded function to allow setting the entire output state with
      // one array. Must have same size AND ORDER as outputPinLayout.
      // Elements can be  0: set outputpin with same index to 0
      //                  1: set outputpin with same index to 1
      //                  2: dont do anything with that pin
      // Parameters: configArray: reference to int array with
      //                          MAX_OUTPUT_SIZE as size

      for (int i = 0; i < MAX_OUTPUT_SIZE; i++) {
        if (configArray[i] < 2) {
          digitalWrite(outputData[i].pinNumber, configArray[i]);
          outputData[i].pinState = configArray[i];          
        }
      }
      JsonObject & root = buildJson("setAllOutputs");
      root["output"] = configArray;
      sendJson(root);
    }

    bool isAllreadySet(int (&toBeChecked)[MAX_OUTPUT_SIZE]) {
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

    bool isAllreadySet(Dict toBeChecked) {

      int tempArray[MAX_OUTPUT_SIZE] = {};
      for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
        if (toBeChecked.key == outputData[i].pinName) {
          tempArray[i] = toBeChecked.value;
        } else {
          tempArray[i] = 2;
        }
      }
      return isAllreadySet(tempArray);
    }

    template <size_t N>
    bool isAllreadySet(Dict (&toBeChecked)[N]) {

      int tempArray[MAX_OUTPUT_SIZE] = {};
      for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
        for (auto & j : toBeChecked) {
          if (j.key == outputData[i]) {
            tempArray[i] = j.val;
          } else {
            tempArray[i] = 2;
          }
        }
      }
      return isAllreadySet(tempArray);

    }


   void shutdownSource(bool closeOffH2O) {
      // shutdown source. ugly repetion galore
      setOutput("heliumSource", 0);
      setOutput("sourcePump_1", 0);
      setOutput("sourcePump_2", 0);
      if (closeOffH2O) {
        setOutput("sourceH2O", 0);
      }
      setOutput("vacuumChamberValve", 0);

      JsonObject & root = buildJson("shutdownSource");
      root["warn"] = "Source shut down. Cooling water closing : " + String(closeOffH2O);
      for (const auto & i : outputData) {
        root[i.pinName] = i.pinState;
      }
      sendJson(root);
    }

    void shutdownChamber(bool closeOffH2O) {
     setOutput("heliumSource", 0);
     setOutput("chamberPump_1", 0);
     setOutput("chamberPump_2", 0);
     if (closeOffH2O) {
       setOutput("chamberH2O", 0);
     }
     setOutput("vacuumChamberValve", 0);
     setOutput("vacuumDetectorValve", 0);

    JsonObject & root = buildJson("shutdownChamber");
    root["warn"] = "Chamber shut down. Cooling water closing : " + String(closeOffH2O);
    for (const auto & i : outputData) {
      root[i.pinName] = i.pinState;
    }
    sendJson(root);
  }

    void shutdownDetector(bool closeOffH2O) {
      setOutput("heliumSource", 0);
      setOutput("detectorPump_1", 0);
      setOutput("detectorPump_2", 0);
      if (closeOffH2O) {
        setOutput("detectorH2O", 0);
      }
      setOutput("vacuumDetectorValve", 0);

      JsonObject & root = buildJson("shutdownDetector");
      root["warn"] = "Detector shut down. Cooling water closing : " + String(closeOffH2O);
      for (const auto & i : outputData) {
        root[i.pinName] = i.pinState;
      }
      sendJson(root);
    }

};


// class Machine {

//   private:

    
//     // controlparameters for responses to sensor signal
//     bool sourceFullShutdown = false, sourceSoftShutdown = false;
//     bool chamberFullShutdown = false, chamberSoftShutdown = false;
//     bool detectorFullShutdown = false, detectorSoftShutdown = false;

//     // list of water flow sensors to be checked
//     // defined here so they dont get initilized each time a check is done
//     String sourceFlowSensors[2] = {"sourceFlow_1", "sourceFlow_2"};
//     String chamberFlowSensors[2] = {"chamberFlow_1", "chamberFlow_2"};
//     String detectorFlowSensors[2] = {"detectorFlow_1", "detectorFlow_2"};
    

//     }

//     // temp sensors will probably be implemented in the same way as water flow sensors are
//     // this is alot of boilerplate code which seems unnecessary but I should finish this first
//     // and then try to make this prettier.

   

// };


// dont forget to set the template parameters to : 
// number of entries in inputPinLayout and outputPinLayout respectively
// initilized here so they are global variables
Inputs input;
Outputs output;

// flag to check wether loop() has run once
bool firstLoopFinished = false;

// flag to indicate wether a sensor is showing an error
bool sensorError = false;


bool mapInputToOutput(int (&inputArray)[MAX_INPUT_SIZE], int (&outputArray)[MAX_OUTPUT_SIZE]) {
  // Set all outputs to a given state if all inputs are in a given state.
  // Parameters: inputArray: int array of length MAX_INPUT_SIZE
  //             elements can be: 0: check if state of pin on same position
  //                                 in the inputPinLayout is 0
  //                              1: check if state of pin is 1
  //                              2: do not check state of pin
  //             outputArray: int array of length MAX_OUTPUT_SIZE
  //             elements can be: 0: set state to 0
  //                              1: set state to 1
  //                              2: do not set state

  if (output.isAllreadySet(outputArray)) {
    return false; // do not repeat test if output is allready set anyway
  }

  if (input.compare(inputArray)) {
    output.setOutput(outputArray);
    sensorError = true;
    return true;
  }
}

//TODO: overload mapInput...aber die muss eh wieder geändert werden also mal schauen


void setup() {

  // Setup the whole machine. All pin configurations go here!
  // For the PinState a default value of 0 is set.
  // Remember to set the template parameter (the values inside < >)
  // at the Inputs and Outputs init correctly.
  Pin inputPinLayout[] = {
    {"sourceFlow_1", CONTROLLINO_A0},
    {"sourceFlow_2", CONTROLLINO_A1},
    {"chamberFlow_1", CONTROLLINO_A2},
    {"chamberFlow_2", CONTROLLINO_A3},
    {"detectorFlow_1", CONTROLLINO_A4},
    {"detectorFlow_2", CONTROLLINO_A5},
    {"sourceGroundH2O", CONTROLLINO_A6},
    {"chamberGroundH2O", CONTROLLINO_A7},
    {"detectorGroundH2O", CONTROLLINO_A8},
  };

  Pin outputPinLayout[] = {
    {"heliumSource", CONTROLLINO_D0},
    {"sourcePump_1", CONTROLLINO_D1},
    {"sourcePump_2", CONTROLLINO_D2},
    {"chamberPump_1", CONTROLLINO_D3},
    {"chamberPump_2", CONTROLLINO_D4},
    {"detectorPump_1", CONTROLLINO_D5},
    {"detectorPump_2", CONTROLLINO_D6},
    {"sourceH2O", CONTROLLINO_D7},
    {"chamberH2O", CONTROLLINO_D8},
    {"detectorH2O", CONTROLLINO_D9},
    {"vacuumChamberValve", CONTROLLINO_D10},
    {"vacuumDetectorValve", CONTROLLINO_D11}
  
  };

  // start Serial and check if its connected correctly
  Serial.begin(9600);
  int startTime = millis();
  while(!Serial) {
    if (millis() - startTime >= 10000) {
      break;
    }
  }

  // initilize input and output pins
  // read inputs and set accordingly, set outputs to 0
  input.begin(inputPinLayout);
  output.begin(outputPinLayout);

  //wait a bit. seems to be needed to set everything up correctly
  delay(1);

}

Dict normalOperationConfig[] = {
  // initial output state if no errors are detected
  // this needs be live in the global scope (so not in setup())
  // because it is needed inside of loop()
  // but can be made much smaller in size (see overloading in Outputs)
  {"heliumSource", 1},
  {"sourcePump_1", 1},
  {"sourcePump_2", 1},
  {"chamberPump_1", 1},
  {"chamberPump_2", 1},
  {"detectorPump_1", 1},
  {"detectorPump_2", 1},
  {"sourceH2O", 1},
  {"chamberH2O", 1},
  {"detectorH2O", 1},
  {"vacuumChamberValve", 1},
  {"vacuumDetectorValve", 1}
};


void loop() {

// // important part: all health checks are defined here
//   // returns: int as error status
//   // one could also check all inputs at start and 
//   int error = 0;

//   // ground water test
  
//   if (inHandler.getInputState("sourceGroundH2O") == 1 && !sourceFullShutdown) {
//     JsonObject & root = buildJson("sensorEventGroundH2O");
//     root["warn"] = "Ground water on source detected!";
//     sendJson(root);
//     shutdownSource(true);
//     sourceFullShutdown = true;
//   }

//   if (inHandler.getInputState("chamberGroundH2O") == 1 && !chamberFullShutdown) {
//     JsonObject & root = buildJson("sensorEventGroundH2O");
//     root["warn"] = "Ground water on scattering chamber detected!";
//     sendJson(root);
//     shutdownChamber(true);
//     chamberFullShutdown = true;
//   }

//   if (inHandler.getInputState("detectorGroundH2O") == 1 && !detectorFullShutdown) {
//     JsonObject & root = buildJson("sensorEventGroundH2O");
//     root["warn"] = "Ground water on detector found!";
//     sendJson(root);
//     shutdownDetector(true);
//     detectorFullShutdown = true;
//   }

//   // water flow test
//   if (!sourceSoftShutdown && !sourceFullShutdown) {


//     for (auto & i : sourceFlowSensors) {
//       if (inHandler.getInputState(i) == 1) {
//         JsonObject & root = buildJson("sensorEventH2OFlow");
//         root["warn"] = "Waterflow below critical level!";
//         root[i] = 1;
//         sendJson(root);
//         shutdownSource(false);
//         sourceSoftShutdown = true;
//       }
//     }
//   }

//   if (!chamberSoftShutdown && !chamberFullShutdown) {
//     for (auto & i : chamberFlowSensors) {
//       if (inHandler.getInputState(i) == 1) {
//         JsonObject & root = buildJson("sensorEventH2OFlow");
//         root["warn"] = "Waterflow below critical level!";
//         root[i] = 1;
//         sendJson(root);
//         shutdownChamber(false);
//         chamberSoftShutdown = true;
//       }
//     }
//   }

//   if (!detectorSoftShutdown && !detectorFullShutdown) {
//     for (auto & i : detectorFlowSensors) {
//       if (inHandler.getInputState(i) == 1) {
//         JsonObject & root = buildJson("sensorEventH2OFlow");
//         root["warn"] = "Waterflow below critical level!";
//         root[i] = 1;
//         sendJson(root);
//         shutdownDetector(false);
//         detectorSoftShutdown = true;
//       }
//     }
//   }
  int foo[] = {2,2,2,2,2,2,2,2,2};
  int bar[] = {2,2,2,2,2,2,2,2,2,2,2,2};
  bool some = false;
  some = mapInputToOutput(foo, bar);

  if (!firstLoopFinished && !sensorError) {
    // if no inputs are showing errors and only if loop()
    // is run the first time: set output to normalOperationConfig
    output.setOutput(normalOperationConfig);
    firstLoopFinished = true;
  }

}
