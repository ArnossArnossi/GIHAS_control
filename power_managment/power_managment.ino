#include "Controllino.h"
#include "ArduinoJson.h"


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


template <int maxInputSize>
class Inputs {

  private:
    Pin inputData[maxInputSize];
 
  public:
    Inputs() {}

    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time
      for (int i = 0; i < maxInputSize; i++) {
        pinMode(pinLayout[i].pinNumber, INPUT);
        inputData[i] = pinLayout[i];
        // TODO:rethink if it is really needed to read here
        inputData[i].pinState = digitalRead(inputData[i].pinNumber);
      }
    }
    
    const Pin * getAllInputs() {
      // read out all inputs, store them in inputData and return constant reference to them
      // and because c++ is being a bitch one has to pass a pointer to inputData instead
      // of a reference and the number of elements is lost in the translation
      // it feels like the whole class structure is wrong and that one shouldnt need to pass
      // an array around anyway.
      for (auto& pin : inputData) {
        pin.pinState = digitalRead(pin.pinNumber);
      }
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
};


template <int maxOutputSize>
class Outputs {

private:
    Pin outputData[maxOutputSize];
  
public:
    Outputs() {}
    
    void begin(Pin pinLayout[]) {
      // constructor method with different name to call at a later time
      for (int i = 0; i < maxOutputSize; i++) {
        pinMode(pinLayout[i].pinNumber, OUTPUT);
        outputData[i] = pinLayout[i];
        outputData[i].pinState = 0; // make sure all outputs are off initially
        digitalWrite(outputData[i].pinNumber, 0);
      }
    }
    
    void setOutput(String outputName, int stateValue) {
      // set state of outputName to stateValue
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

    const Pin * getAllOutputs() {
      return outputData;
    }
};


template <int maxInputSize, int maxOutputSize>
class Machine {

  private:
    Inputs <maxInputSize> inHandler;
    Outputs <maxOutputSize> outHandler;
    
    // controlparameters for responses to sensor signal
    bool sourceFullShutdown = false, sourceSoftShutdown = false;
    bool chamberFullShutdown = false, chamberSoftShutdown = false;
    bool detectorFullShutdown = false, detectorSoftShutdown = false;

    // list of water flow sensors to be checked
    // defined here so they dont get initilized each time a check is done
    String sourceFlowSensors[2] = {"sourceFlow_1", "sourceFlow_2"};
    String chamberFlowSensors[2] = {"chamberFlow_1", "chamberFlow_2"};
    String detectorFlowSensors[2] = {"detectorFlow_1", "detectorFlow_2"};
    
  
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
      // maybe LOG this
      for (int i = 0; i<arraySize; i++) {
        outHandler.setOutput(config[i].key, config[i].value);
      }
    }

    int checkAndReact() {
      // important part: all health checks are defined here
      // returns: int as error status
      // one could also check all inputs at start and 
      int error = 0;

      // ground water test
      
      if (inHandler.getInputState("sourceGroundH2O") == 1 && !sourceFullShutdown) {
        JsonObject & root = buildJson("sensorEventGroundH2O");
        root["warn"] = "Ground water on source detected!";
        sendJson(root);
        shutdownSource(true);
        sourceFullShutdown = true;
      }

      if (inHandler.getInputState("chamberGroundH2O") == 1 && !chamberFullShutdown) {
        JsonObject & root = buildJson("sensorEventGroundH2O");
        root["warn"] = "Ground water on scattering chamber detected!";
        sendJson(root);
        shutdownChamber(true);
        chamberFullShutdown = true;
      }

      if (inHandler.getInputState("detectorGroundH2O") == 1 && !detectorFullShutdown) {
        JsonObject & root = buildJson("sensorEventGroundH2O");
        root["warn"] = "Ground water on detector found!";
        sendJson(root);
        shutdownDetector(true);
        detectorFullShutdown = true;
      }

      // water flow test
      if (!sourceSoftShutdown && !sourceFullShutdown) {


        for (auto & i : sourceFlowSensors) {
          if (inHandler.getInputState(i) == 1) {
            JsonObject & root = buildJson("sensorEventH2OFlow");
            root["warn"] = "Waterflow below critical level!";
            root[i] = 1;
            sendJson(root);
            shutdownSource(false);
            sourceSoftShutdown = true;
          }
        }
      }

      if (!chamberSoftShutdown && !chamberFullShutdown) {
        for (auto & i : chamberFlowSensors) {
          if (inHandler.getInputState(i) == 1) {
            JsonObject & root = buildJson("sensorEventH2OFlow");
            root["warn"] = "Waterflow below critical level!";
            root[i] = 1;
            sendJson(root);
            shutdownChamber(false);
            chamberSoftShutdown = true;
          }
        }
      }

      if (!detectorSoftShutdown && !detectorFullShutdown) {
        for (auto & i : detectorFlowSensors) {
          if (inHandler.getInputState(i) == 1) {
            JsonObject & root = buildJson("sensorEventH2OFlow");
            root["warn"] = "Waterflow below critical level!";
            root[i] = 1;
            sendJson(root);
            shutdownDetector(false);
            detectorSoftShutdown = true;
          }
        }
      }
    }
    // temp sensors will probably be implemented in the same way as water flow sensors are
    // this is alot of boilerplate code which seems unnecessary but I should finish this first
    // and then try to make this prettier.

    void shutdownSource(bool closeOffH2O) {
      // shutdown source. ugly repetion galore
      outHandler.setOutput("heliumSource", 0);
      outHandler.setOutput("sourcePump_1", 0);
      outHandler.setOutput("sourcePump_2", 0);
      if (closeOffH2O) {
        outHandler.setOutput("sourceH2O", 0);
      }
      outHandler.setOutput("vacuumChamberValve", 0);

      JsonObject & root = buildJson("shutdownSource");
      root["warn"] = "Source shut down. Cooling water closing : " + String(closeOffH2O);
      const Pin * outputs = outHandler.getAllOutputs();
      for (int i = 0; i < maxOutputSize; i++) {
        root[outputs[i].pinName] = outputs[i].pinState;
      }
      sendJson(root);
    }

    void shutdownChamber(bool closeOffH2O) {
     outHandler.setOutput("heliumSource", 0);
     outHandler.setOutput("chamberPump_1", 0);
     outHandler.setOutput("chamberPump_2", 0);
     if (closeOffH2O) {
       outHandler.setOutput("chamberH2O", 0);
     }
     outHandler.setOutput("vacuumChamberValve", 0);
     outHandler.setOutput("vacuumDetectorValve", 0);

    JsonObject & root = buildJson("shutdownChamber");
    root["warn"] = "Chamber shut down. Cooling water closing : " + String(closeOffH2O);
    const Pin * outputs = outHandler.getAllOutputs();
    for (int i = 0; i < maxOutputSize; i++) {
      root[outputs[i].pinName] = outputs[i].pinState;
    }
    sendJson(root);
  }

    void shutdownDetector(bool closeOffH2O) {
      outHandler.setOutput("heliumSource", 0);
      outHandler.setOutput("detectorPump_1", 0);
      outHandler.setOutput("detectorPump_2", 0);
      if (closeOffH2O) {
        outHandler.setOutput("detectorH2O", 0);
      }
      outHandler.setOutput("vacuumDetectorValve", 0);

      JsonObject & root = buildJson("shutdownDetector");
      root["warn"] = "Detector shut down. Cooling water closing : " + String(closeOffH2O);
      const Pin * outputs = outHandler.getAllOutputs();
      for (int i = 0; i < maxOutputSize; i++) {
        root[outputs[i].pinName] = outputs[i].pinState;
      }
      sendJson(root);
    }


};


// dont forget to set the template parameter correctly
Machine <9, 12> checker;


void setup() {

  // Setup the whole machine. All pin configurations go here!
  // For the PinState a default value of 0 is set because is is a standart type.
  // Remember to set the template parameter (the values inside < >)
  // at the Machine init correctly.
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

  Dict normalOperationConfig[] = {
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

  // start Serial and check if its connected correctly
  Serial.begin(9600);
  int startTime = millis();
  while(!Serial) {
    if (millis() - startTime >= 10000) {
      break;
    }
  }

  checker.begin(inputPinLayout, outputPinLayout);

  //wait a bit. seems to be needed to set everything up correctly
  delay(1);
  int errorStatus = checker.checkAndReact();
  if (!errorStatus) {
    // make sure to set the lenght of normalOperationConfig correctly
    checker.setMachineState(normalOperationConfig, 12);
  }

}

void loop() {

checker.checkAndReact();


}
