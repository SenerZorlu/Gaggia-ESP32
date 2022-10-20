#include <Arduino.h>
#include <functions.h>

void setup()
{
  // put your setup code here, to run once:

  Serial.begin(115200);
  getPreferences();
  pinInit();
  initWiFi();
  initSPIFFS();
  initWebServer();

  // Turn off boiler in case init is unsucessful
  setBoiler(LOW); // relayPin LOW
  // Pump
  setPressure(0);
}

void loop()
{
  // put your main code here, to run repeatedly:

  sensorsRead();
  brewDetect();
  justDoCoffee();
  setPressureProfile();
  sendEvents();
}