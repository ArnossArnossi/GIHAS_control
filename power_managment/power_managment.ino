#include <map>
#include <utility> //needed for pair


#include "Controllino.h"
//#include "Utils.h" //TODO: include logging

using namespace std;

// 3d arraylike structure for storing name, pin and value of sensor
// these nested pairs seem ugly. Can this be done better?
// does not work because stl is not implemented in arduino i.e. awr
typedef map <String, pair <const int, int> > triple_map;

class Inputs {

  private:
    triple_map _inputData;
 
  public:
    Inputs(map <String, const int> pinLayout) {
      for (const auto& i : pinLayout){
        pinMode(i.second, INPUT) // acctually input is allready set as default
        pinState = digitalRead(i.second; //rethink if it is really needed to read here
        _inputData[i.first] = make_pair(i.second, pinState);
      }
    }
    
    //machst man das so mit dem method header?
    const &triple_map getAllInputs(){
      // read out all inputs, store them in _inputData and return constant reference to them
      for (auto &pin : _inputData){
        pin.second.second = digitalRead(pin.second.first);
      }
      //is this a constant reference?
      return const &_inputData
    }

    int getAllInputs(String sensor){
      // TODO: maybe overthink whether you want to get a list of inputs an not all of them
      // return current value for sensor by string as int
      // TODO: maybe also return name and pin but only if needed
      _inputData[sensor].second = digitalRead(_inputData[sensor].first)
      //this should return by value and therfore a normal int
      return _inputData[sensor].second
    }
}

class Outputs {

  private:
    // the state of the current Outputs
    // first element: name of sensor, second: pin id as int,
    // third: current value of output as int
    triple_map _outputData
  
  public:
    Outputs(map <String, const int> pinLayout) {
      for (const auto &i : pinLayout){
        pinMode(i.second, OUTPUT)
        digitalWrite(i.second; 0)
        _outputData[i.first] = make_pair(i.second, 0);
      }
    }

    void setOutput(pair <String, int> outputValue) {
      // parameters: outputvalue: pair of sensor name as string and value to set as int
      digitalWrite(_outputData[outputValue.first].first, outputValue.second)
    }

    void setMultipleOutputs(map <String, int> outputValues){
      // set mulitple outputs at once
      //takes a map of string defining the outputs and the desired states 
      for (const auto &i : outputValues){
        setOutput(i)
      }
    }


    




}


void setup() {

}

void loop() {

}
