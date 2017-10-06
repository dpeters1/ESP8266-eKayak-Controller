/*
  ESP8266 Wifi Power System Controller 

  To be used with the "Houston" Android app to control
  and monitor the brushless power system for my E-kayak and other projects.

  Created July 7th, 2017 in Ottawa
  By Dominic Peters
  Modified August 30th 2017
  By Dominic Peters

  https://hackaday.io/project/13342-e-kayak

*/

#include <ESP8266_HTTP_SDK.h>
#include <ArduinoJson.h>
#include <Simple-INA226.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RunningAverage.h"

#define potPin A0
#define escPin 3
#define rudderPin 16
#define tempPin 13

const char *ssid = "Kayak Controller";
const char* flashSsid = "Wu-Tang LAN";
const char* password = "allstream11";
bool flashMode = false;
const double VBUS_MULTI_FACTOR = 1.453;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

long lastReading = 0; // For sensor polling interval
long lastCommand = 0; // For command sending interval

/***** Peripherals *****/
SIMPLE_INA226 ina;
OneWire oneWire(tempPin);
DallasTemperature tempSensors(&oneWire);
Servo esc;
Servo rudder;

int throttlePosition = 0; // 0-100%
int lastThrottlePos = 0;
int steeringPosition = 90;   // Midpoint
int lastSteeringPos = 90;
int steeringTrim = 0;
int throttleIntervalCounter = 0;
int steeringIntervalCounter = 0;
double throttleStepSize;
double steeringStepSize;
/**********************/

const int bufferLen = 256;
StaticJsonBuffer<bufferLen> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
char * jsonBuff = new char[bufferLen]();

RunningAverage powerAverage(16);

void setup() {
  //Serial.begin(115200);
  delay(1000);
  //Serial.println("");

  pinMode(tempPin, INPUT);
  if(digitalRead(tempPin) == LOW) {flashMode = true;} // Jumper connected for flash mode

  if(flashMode) {
    //Serial.println("Entering Flash Mode...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(flashSsid, password);

    while(WiFi.waitForConnectResult() != WL_CONNECTED){
      WiFi.begin(flashSsid, password);
      //Serial.println("WiFi failed, retrying.");
      //Serial.print("MAC addr: ");
      //Serial.println(WiFi.macAddress());
    }
    httpUpdater.setup(&httpServer);
    httpServer.begin();
    //Serial.print("Update Server Ready at ");
    //Serial.println(WiFi.localIP());
  }

  else {  // Jumper disconnected and pin is connected to pull-up resistor
    //Serial.println("Booting Sketch...");
    WiFi.softAP(ssid);
    SdkWebServer_Init(9701);
    ina.init(75, 0.75); // max expected amps and resistor val in mOhms
    esc.attach(escPin);
    rudder.attach(rudderPin);
    tempSensors.begin();
    tempSensors.setWaitForConversion(false);
    pinMode(potPin, INPUT);
    powerAverage.clear();
  }
}

void loop() {

  if(flashMode) {httpServer.handleClient();}
  
  else {ReadSensors(100); setThrottle(20);}
}

void ReadSensors(int interval) {
  yield();
  long now = millis();
  long actualInt = now - lastReading;                
  if (actualInt > interval) {  // Read 1 sensor every "interval" milliseconds or longer
      lastReading = now;
  }
  else {
      return;
  }

  float power = ina.getPower()*VBUS_MULTI_FACTOR;

  powerAverage.addValue(power);
  tempSensors.requestTemperatures();
  
  root["Voltage"] = ina.getVoltage()*VBUS_MULTI_FACTOR;
  root["Current"] = ina.getCurrent();
  root["Power"] = power;
  root["powerAvg"] = powerAverage.getAverage();
  root["throttlePos"] = throttlePosition;
  root["escTemp"] = tempSensors.getTempCByIndex(0);
  root["battTemp"] = tempSensors.getTempCByIndex(1);

  /*
  powerAverage.addValue(power);
  root["Voltage"] = 44.4;
  root["Current"] = 32;
  root["Power"] = power;
  root["powerAvg"] = powerAverage.getAverage();
  root["throttlePos"] = throttlePosition;
  root["escTemp"] = 67;
  root["battTemp"] = 40;

 */
  //root.printTo(Serial); Serial.println("");

  root.printTo(jsonBuff, bufferLen);
  SdkWebServer_updateData(jsonBuff);
    
  ESP.wdtFeed(); 
  yield();
}

void setThrottle(int interval) {
  yield;
  long now = millis();
  long actualInt = now - lastCommand;                
  if (actualInt > interval) {  // Read 1 sensor every "interval" milliseconds or longer
      lastCommand = now;
  }
  else {
      return;
  }

  steeringTrim = SdkWebServer_getSteeringTrim();
  
  if(SdkWebServer_ManualCtrl()) {
    throttlePosition = map(analogRead(potPin), 0, 1023, 0, 100);
    steeringPosition = (90+steeringTrim);
    steeringPosition = abs(steeringPosition-180);

    esc.write(map(throttlePosition, 0, 100, 0, 180));
    rudder.write(steeringPosition);
  }
  else {
    throttlePosition = SdkWebServer_getThrottlePos();
    steeringPosition = (SdkWebServer_getSteeringPos()+steeringTrim);
    steeringPosition = abs(steeringPosition-180);

    esc.write(map(smoothOutput(0), 0, 100, 0, 180));
    rudder.write(smoothOutput(1));
  }
  /*
  Serial.print("Steering: ");
  Serial.println(steeringPosition);
  Serial.print("Throttle: ");
  Serial.println(throttlePosition);
  */
  ESP.wdtFeed(); 
  yield();
}

int smoothOutput(int channel) { // 0 and 1 are throttle and steering channels respectively

  int numSteps = 4;
  
  if(channel == 0){
    if(throttlePosition != lastThrottlePos){ // New throttle position received
                                             // You wil have at least 80ms or 4 loops until the next command
      throttleIntervalCounter = 0;
      throttleStepSize = (throttlePosition - lastThrottlePos)/numSteps;

      lastThrottlePos = throttlePosition;
    }
    if(throttleIntervalCounter >= numSteps) {return throttlePosition;}

    throttleIntervalCounter += 1;
    int intervalPos = throttlePosition - (throttleStepSize*numSteps) + (throttleStepSize*throttleIntervalCounter);

    return intervalPos;
  }

  if(channel==1){
    if(steeringPosition != lastSteeringPos){ // New steering position received
                                             // You wil have at least 80ms or 4 loops until the next command
      steeringIntervalCounter = 0;
      steeringStepSize = (steeringPosition - lastSteeringPos)/numSteps;

      lastSteeringPos = steeringPosition;
    }
    if(steeringIntervalCounter >= numSteps) {return steeringPosition;}

    steeringIntervalCounter += 1;
    int intervalPos = steeringPosition - (steeringStepSize*numSteps) + (steeringStepSize*steeringIntervalCounter);

    return intervalPos;
  }
}

