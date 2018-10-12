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

template <size_t N> void jsonAddArray(JsonObject & someRoot, String name, int (&ar)[N]) {
  // Add an int array to a json object.
  // Parameters: someRoot, json object
  //             name, name of the array
  //             ar, reference to in array to be stored          
  JsonArray& someArray = someRoot.createNestedArray(name);
  for (int i=0; i<N; i++) {
    someArray.add(ar[i]);
  }
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

  Dict(String key = "", int value = 0) : key(key), value(value) {}
};

template <size_t N>
struct ArInit {
  // helper struct for "easy" input of parameters to map method
  // easy here means needlesly complicated and fucking stupid
    int ar[N];
  
  // custom constructor cannot be used because implicit list initialization
  // would be overwritten and cannot be reimplemented because std cant be used

    int& operator[](int i) {
      return ar[i];
    }
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

    int getPinID(String pinName) {
      // Return the position (i.e. id) of pin with name pinName
      // in inputData
      // Parameters: String pinName: name of pin to be found
      // Return: int, # element in inputData
      for (int i=0; i<MAX_INPUT_SIZE; i++) {
        if (inputData[i].pinName == pinName) {
          return i;
      String errorMessage = "No Input with name: " + pinName + " found.";
      JsonObject & root = buildJson("error");
      root["error"] = errorMessage;
      sendJson(root);
      // no error catching is attempted
      // if the name is not found the program should fail horribly and obviously
      // NOTE: all mapings need to be tested beforehand in simulation
        }
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
          // TODO check if this clause is actually stept into
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

    int getPinID(String pinName) {
      // Return the position (i.e. id) of pin with name pinName
      // in in/ouputdata
      // Parameters: String pinName: name of pin to be found
      // Return: int, # element in input ot ouputData
      for (int i=0; i<MAX_OUTPUT_SIZE; i++) {
        if (outputData[i].pinName == pinName) {
          return i;
      String errorMessage = "No Output with name: " + pinName + " found.";
      JsonObject & root = buildJson("error");
      root["error"] = errorMessage;
      sendJson(root);
      // no error catching is attempted
      // if the name is not found the program should fail horribly and obviously
      // NOTE: all mapings need to be tested beforehand in simulation
        }
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
      jsonAddArray(root, "output", configArray);
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

};

class Machine {

  private:

    Inputs input;
    Outputs output;

  public:

    // flag to check wether loop() has run once
    bool firstLoopFinished = false;

    // flag to indicate wether a sensor is showing an error
    bool sensorError = false;

    Machine() {}

    void begin(Pin inputPinLayout[], Pin outputPinLayout[]) {
      // constructor method with different name to call after initialzation
      input.begin(inputPinLayout);
      output.begin(outputPinLayout);
    }

    template <size_t N>
    void setOutput(Dict (&out)[N]) {
      // stupid passalong of some bullshit. the overloading in outputs
      // is now completly useless because it cant be reached anymore
      // and set output should not be used from outside Machine if we were
      // serious about data encapsulation but who cares!!!
      output.setOutput(out);
    }

    bool mapImpl(int (&inputArray)[MAX_INPUT_SIZE], int (&outputArray)[MAX_OUTPUT_SIZE]) {
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
      // Returns: false: if output is not changed
      //          true: if output is changed

      if (output.isAllreadySet(outputArray) && firstLoopFinished) {
        // do not repeat test if output is allready set anyway
        // but do it anyway if we are in the first loop, i.e. before normal operation
        return false;
      }

      if (input.compare(inputArray)) {
        output.setOutput(outputArray);
        sensorError = true;
        JsonObject & root = buildJson("sensorEvent");
        jsonAddArray(root, "input", inputArray);
        jsonAddArray(root, "output", outputArray);
        sendJson(root);
        return true;
      }
      return false;
    }

    bool map(ArInit<MAX_INPUT_SIZE> in, ArInit<MAX_OUTPUT_SIZE> out) {
      // map for "easy" usage, meaning initialsation of in and out array inside map call
      return mapImpl(in.ar, out.ar);
    }

    bool map(Dict in, ArInit<MAX_OUTPUT_SIZE> out) {
      // Overoading of map. All overloaded mappings just refer
      // the logic to mapping-method with complete in and out arrays
//      tempInputArray[input.getPinID(in.key)] = in.value;
      ArInit<MAX_INPUT_SIZE> inArray;
      for (int i=0; i<MAX_INPUT_SIZE; i++) inArray[i] = 2;
      inArray[input.getPinID(in.key)] = in.value;
      return mapImpl(inArray.ar, out.ar);
    }

    bool map(ArInit<MAX_INPUT_SIZE> in, Dict out) {
      // Overoading of map.
      ArInit<MAX_OUTPUT_SIZE> outArray;
      for (int i=0; i<MAX_OUTPUT_SIZE; i++) outArray[i] = 2;
      outArray[output.getPinID(out.key)] = out.value;
      return mapImpl(in.ar, outArray.ar);
    }

    bool map(Dict in, Dict out) {
      // Overoading of map.
      ArInit<MAX_INPUT_SIZE> inArray;
      for (int i=0; i<MAX_INPUT_SIZE; i++) inArray[i] = 2;
      inArray[input.getPinID(in.key)] = in.value;
      ArInit<MAX_OUTPUT_SIZE> outArray;
      for (int i=0; i<MAX_OUTPUT_SIZE; i++) outArray[i] = 2;
      outArray[output.getPinID(out.key)] = out.value;
      return mapImpl(inArray.ar, outArray.ar);
    }

};


Machine controller;


void setup() {

  // Setup the whole machine. All pin configurations go here!
  // For the PinState a default value of 0 is set.
  // Remember to set MAX_INPUT_SIZE and MAX_OUTPUT_SIZE at the beginning of this
  // script correctly, i.e. length of input and output layout respectively.
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

  // initilize controller with input and output pins
  // read inputs and set accordingly, set outputs to 0
  controller.begin(inputPinLayout, outputPinLayout);

  //wait a bit. seems to be needed to set everything up correctly
  delay(1);


}

Dict normalOperationConfig[] = {
  // initial output state if no errors are detected
  // this needs be live in the global scope (so not in setup())
  // because it is needed inside of loop()
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

Dict in;
ArInit<MAX_OUTPUT_SIZE> out;

void loop() {


  // water flow check (shut down entire area but not water intake)
  controller.map(in = {"sourceFlow_1", 1}, out = {0,0,0,2,2,2,2,2,2,2,0,2});
  controller.map(in = {"sourceFlow_2", 1}, out = {0,0,0,2,2,2,2,2,2,2,0,2});
  controller.map(in = {"chamberFlow_1", 1}, out = {0,2,2,0,0,2,2,2,2,2,0,0});
  controller.map(in = {"chamberFlow_2", 1}, out = {0,2,2,0,0,2,2,2,2,2,0,0});
  controller.map(in = {"detectorFlow_1", 1}, out = {0,2,2,2,2,0,0,2,2,2,2,0});
  controller.map(in = {"detectorFlow_2", 1}, out = {0,2,2,2,2,0,0,2,2,2,2,0});

  // ground water check (shut down entire area and also water intake)
  controller.map(in = {"sourceGroundH2O", 1}, out = {0,0,0,2,2,2,2,0,2,2,0,2});
  controller.map(in = {"chamberGroundH2O", 1}, out = {0,0,0,2,2,2,2,0,2,2,0,0});
  controller.map(in = {"detectorGroundH2O", 1}, out = {0,0,0,2,2,2,2,0,2,2,2,0});


  if (!controller.firstLoopFinished && !controller.sensorError) {
    // if no inputs are showing errors and only if loop()
    // is run the first time: set output to normalOperationConfig
    controller.setOutput(normalOperationConfig);
    controller.firstLoopFinished = true;
  }

}
