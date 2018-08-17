#include "Controllino.h"
//#include "Utils.h" //TODO: include logging


struct Pin {
  const String pinName;
  const int pinNumber;
  int pinState;
};

template <int maxInputSize>
class Inputs {

  private:
    Pin _inputData[maxInputSize];
 
  public:
    Inputs(Pin pinLayout[]) {
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
    Inputs(Pin pinLayout[]) {
      for (int i = 0; i < maxOutputSize; i++) {
        pinMode(pinLayout[i].pinNumber, OUTPUT);
        _outputData[i] = pinLayout[i];
        _outputData[i].pinState = 0; // make sure all outputs are off initially
        digitalWrite(_outputData[i].pinNumber);
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



void setup() {

}

void loop() {

}
