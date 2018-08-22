#include "Controllino.h"
#include "ArduinoJson.h"


void serialize (int key, int value){
  // turn key and value into dictionary, serialize and send it
  if (!Serial){
    // here maybe there should be some blinking going on or something
    // like this just to signal that the connection is lost
    // or return an int from function (0 for all good 1 for error)
    return;
  }
  // initilize Json object the size of 200 is set and has to be
  // changed according to the size of the json document
  // probably just use enough so a complete status report can be sent
  StaticJsonBuffer<200> jsonBuffer;
  // root is now the object which holds the json data an where data can
  // be appended and sent to Serial
  JsonObject& root = jsonBuffer.createObject();
  root[String(key)] = value;
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


template <int maxInputSize>
class Inputs {

  private:
    Pin _inputData[maxInputSize];
 
  public:
    Inputs() {}
    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time
      for (int i = 0; i < maxInputSize; i++) {
        pinMode(pinLayout[i].pinNumber, INPUT);
        _inputData[i] = pinLayout[i];
        // TODO:rethink if it is really needed to read here
        _inputData[i].pinState = digitalRead(_inputData[i].pinNumber);
      }
    }
    
    const Pin * getAllInputs() {
      // read out all inputs, store them in _inputData and return constant reference to them
      // and because c++ is being a bitch one has to pass a pointer to _inputData instead
      // of a reference and the number of elements is lost in the translation
      // it feels like the whole class structure is wrong and that one shouldnt need to pass
      // an array around anyway.
      for (auto& pin : _inputData) {
        pin.pinState = digitalRead(pin.pinNumber);
      }
      return _inputData;
    }

    int getInputState(String sensor) {
      // return current value for sensor by string as int
      // TODO: maybe also return name and pin but only if needed
      for (auto & i : _inputData) {
        if (i.pinName == sensor) {
          i.pinState = digitalRead(i.pinNumber);
          return i.pinState;
        }
      }
      // LOG: no sensor with that name found
      return 0;
    }
};


template <int maxOutputSize>
class Outputs {

private:
    Pin _outputData[maxOutputSize];
  
public:
    Outputs() {}
    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time
      for (int i = 0; i < maxOutputSize; i++) {
        pinMode(pinLayout[i].pinNumber, OUTPUT);
        _outputData[i] = pinLayout[i];
        _outputData[i].pinState = 0; // make sure all outputs are off initially
        digitalWrite(_outputData[i].pinNumber, 0);
      }
    }
    
    void setOutput(String outputName, int stateValue) {
      // set state of outputName to stateValue
      for (auto& pin : _outputData) {
        if (pin.pinName == outputName) {
          digitalWrite(pin.pinNumber, stateValue);
          // LOG: wich pin has been set to what
          return;
        }
      }
      // LOG: no output with that name has been found
    }
};


template <int maxInputSize, int maxOutputSize>
class Machine {

  private:
    Inputs <maxInputSize> inHandler;
    Outputs <maxOutputSize> outHandler;
  
  public:
    Machine() : inHandler(), outHandler() {}

    void begin(Pin inputPinLayout[], Pin outputPinLayout[]) {
      // constructor method with different name to call at a later time
      inHandler.begin(inputPinLayout);
      outHandler.begin(outputPinLayout);
    }

    void setMachineState(Dict config[], int arraySize) {
      // set multiple outputs at once
      // maybe this should live in Outputs
      // also all those nested for loops cannot be good for performance

      //LOG setting machine state
      for (int i = 0; i<arraySize; i++) {
        outHandler.setOutput(config[i].key, config[i].value);
      }
    }

    int checkAndReact() {
      // important part: all health checks are defined here
      // returns: int as error status
      int error = 0;
      if (inHandler.getInputState("foo") == 1) {
        //run react method
        //log EVERYTHING
        error = 1;
      }

      if (inHandler.getInputState("bar") == 1) {
        //run some other shit
        //and ofcourse...log it
        error = 1;
      }
    return error;
    }

    void shutdownSource(){
      outHandler.setOutput("out1", 0);
      // LOG it
    }
};


// dont forget to set the template parameter correctly
Machine <2, 2> checker;


void setup() {

  // Setup the whole machine. All pin configurations go here!
  // For the PinState a default value of 0 is set because is is a standart type.
  // Remember to set the template parameter (the values inside < >)
  // at the Machine init correctly.
  Pin inputPinLayout[] = {
    {"foo", CONTROLLINO_A0},
    {"bar", CONTROLLINO_A1}
  };

  Pin outputPinLayout[] = {
    {"out1", CONTROLLINO_D0},
    {"out2", CONTROLLINO_D1}
  };

  Dict normalOperationConfig[] = {
    {"out1", 1},
    {"out2", 1}
  };

  checker.begin(inputPinLayout, outputPinLayout);
  int errorStatus = checker.checkAndReact();
  if (!errorStatus) {
    checker.setMachineState(normalOperationConfig, 2);
  }

}

void loop() {

checker.checkAndReact();


}
