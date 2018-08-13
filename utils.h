#include "Controllino.h"
#include "ArduinoJson.h"


int inputState = 0;
const int inputPin = CONTROLLINO_A0;

void serialize (int pin, int data){
  if (!Serial){
    // here maybe there should be some blinking going on or something
    // like this just to signal that the connection is lost
    return;
  }
  // initilize Json object the size of 200 is set and has to be
  // changed according to the size of the json document
  // probably just use enough so a complete status report can be sent
  StaticJsonBuffer<200> jsonBuffer;
  // root is now the object which holds the json data an where data can
  // be appended and sent to Serial
  JsonObject& root = jsonBuffer.createObject();
  root[String(pin)] = data;
  root.printTo(Serial);
  // only needed if we seperate incoming streams via readline()f
  Serial.println();
}