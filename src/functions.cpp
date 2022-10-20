#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <max6675.h>
#include "RBDdimmer.h"
#include <Preferences.h>
#include <functions.h>

// Replace with your network credentials
// const char *ssid = "arge";
// const char *password = "1a2b3c4d5e";
const char *ssid = "Gaggia-Access-Point";
const char *password = "123456789";

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

// javascript sent event timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 500;

#define relayPin 2
#define brewPin 1
#define dimmerzcPin 4
#define dimmerPin 3

#define thermoSO 7
#define thermoCS 8
#define thermoSCK 6

#define PUMP_RANGE 100
#define GET_KTYPE_READ_EVERY 250 // thermocouple data read interval not recommended to be changed to lower than 250 (ms)
// #define REFRESH_SCREEN_EVERY    150 // Screen refresh interval (ms)
#define DELTA_RANGE 0.25f // % to apply as delta

unsigned long thermoTimer = millis();
unsigned long pageRefreshTimer = millis();
unsigned long brewingTimer = millis();
unsigned long activeBrewingStart = 4294967295; // max value so timer only updates after start defined

volatile float kProbeReadValue; // temp val

volatile int HPWR = 550;
volatile int HPWR_OUT;
int MainCycleDivider = 5;
int BrewCycleDivider = 2;
int setPoint;   // Boiler set temperature
int offsetTemp; // offset temperature between boiler and brewbasket

int preinfusePressureBar; // Pump running ratio %
int brewPressureBar;      // Pump running ratio %
int preinfuseTime;
int preinfuseSoakTime;

bool brewActive;
bool brewTimerActive; // active if brewing
bool heaterStatus;

bool preinfusion;
bool preinfusionFinished;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO);
dimmerLamp pump(dimmerPin, dimmerzcPin, PUMP_RANGE, 1);
Preferences preferences;

//#############################################################################################
//###################################____GET PREFERENCES____###################################
//#############################################################################################

void getPreferences()
{
  // Open Preferences with my-app namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("settingsPref", true);

  // Remove all preferences under the opened namespace
  // preferences.clear();

  // Or remove the counter key only
  // preferences.remove("counter");

  // Get the counter value, if the key does not exist, return a default value of 0
  // Note: Key name is limited to 15 chars.
  setPoint = preferences.getInt("setPoint", 99);
  offsetTemp = preferences.getInt("offsetTemp", 7);
  brewPressureBar = preferences.getInt("brewPressure", 100);
  preinfusion = preferences.getBool("preinfusion", false);
  preinfusePressureBar = preferences.getInt("preinfPress", 30);
  preinfuseTime = preferences.getInt("preinfuseTime", 7);
  preinfuseSoakTime = preferences.getInt("preinfSoakTime", 3);

  preferences.end();
}

void resetPreferences()
{
  preferences.begin("settingsPref", false);
  preferences.clear();
  preferences.end();
}

//##############################################################################################################################
//#############################################___________SENSORS_READ________##################################################
//##############################################################################################################################

void sensorsRead()
{ // Reading the thermocouple temperature
  // static long thermoTimer;
  // Reading the temperature every 350ms between the loops
  if (millis() > thermoTimer)
  {
    kProbeReadValue = thermocouple.readCelsius(); // Making sure we're getting a value

    /*
    This *while* is here to prevent situations where the system failed to get a temp reading and temp reads as 0 or -7(cause of the offset)
    If we would use a non blocking function then the system would keep the SSR in HIGH mode which would most definitely cause boiler overheating
    */
    while (kProbeReadValue <= 0.0f || kProbeReadValue == NAN || kProbeReadValue > 165.0f)
    {
      /* In the event of the temp failing to read while the SSR is HIGH
      we force set it to LOW while trying to get a temp reading - IMPORTANT safety feature */
      setBoiler(LOW);
      if (millis() > thermoTimer)
      {
        kProbeReadValue = thermocouple.readCelsius(); // Making sure we're getting a value
        thermoTimer = millis() + GET_KTYPE_READ_EVERY;
      }
    }
    thermoTimer = millis() + GET_KTYPE_READ_EVERY;
  }
}

// Get Sensor Readings and return JSON object
String getSensorReadings()
{
  readings["temperature"] = String(int(kProbeReadValue) - offsetTemp);
  //readings["heaterstate"] = String(heaterStatus);
  if (brewTimerActive)
  {
    long timeIn = (millis() > activeBrewingStart) ? (int)((millis() - activeBrewingStart) / 1000) : 0;

    readings["time"] = String(timeIn);
    readings["time_label"] = String(timeIn);
    if (preinfusion && timeIn <= preinfuseTime + preinfuseSoakTime)
    {
      readings["time_label"] = "PRE";
    }
    else if (preinfusion)
    {
      readings["time_label"] = String(timeIn - (preinfuseTime + preinfuseSoakTime));
    }
  }
  else
  {
    readings["time"] = "0";
    readings["time_label"] = "0";
  }

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

//#############################################################################################
//#####################################____DO COFFEE____######################################
//#############################################################################################

// delta stuff
inline static float TEMP_DELTA(float d) { return (d * DELTA_RANGE); }

void justDoCoffee()
{
  // USART_CH1.println("DO_COFFEE ENTER");
  int HPWR_LOW = HPWR / MainCycleDivider;
  static double heaterWave;
  static bool heaterState;
  float BREW_TEMP_DELTA;
  // Calculating the boiler heating power range based on the below input values
  HPWR_OUT = mapRange(kProbeReadValue, setPoint - 10, setPoint, HPWR, HPWR_LOW, 0);
  HPWR_OUT = constrain(HPWR_OUT, HPWR_LOW, HPWR); // limits range of sensor values to HPWR_LOW and HPWR
  BREW_TEMP_DELTA = mapRange(kProbeReadValue, setPoint, setPoint + TEMP_DELTA(setPoint), TEMP_DELTA(setPoint), 0, 0);
  BREW_TEMP_DELTA = constrain(BREW_TEMP_DELTA, 0, TEMP_DELTA(setPoint));

  // USART_CH1.println("DO_COFFEE TEMP CTRL BEGIN");
  if (brewActive)
  {
    // Applying the HPWR_OUT variable as part of the relay switching logic
    if (kProbeReadValue > setPoint - 1.5f && kProbeReadValue < setPoint + 0.25f && !preinfusionFinished)
    {
      if (millis() - heaterWave > HPWR_OUT * BrewCycleDivider && !heaterState)
      {
        setBoiler(LOW);
        heaterState = true;
        heaterWave = millis();
      }
      else if (millis() - heaterWave > HPWR_LOW * MainCycleDivider && heaterState)
      {
        setBoiler(HIGH);
        heaterState = false;
        heaterWave = millis();
      }
    }
    else if (kProbeReadValue > setPoint - 1.5f && kProbeReadValue < setPoint && preinfusionFinished)
    {
      if (millis() - heaterWave > HPWR * BrewCycleDivider && !heaterState)
      {
        setBoiler(HIGH);
        heaterState = true;
        heaterWave = millis();
      }
      else if (millis() - heaterWave > HPWR && heaterState)
      {
        setBoiler(LOW);
        heaterState = false;
        heaterWave = millis();
      }
    }
    else if (kProbeReadValue <= setPoint - 1.5f)
    {
      setBoiler(HIGH);
    }
    else
    {
      setBoiler(LOW);
    }
  }
  else
  { // if brewState == 0
    // USART_CH1.println("DO_COFFEE BREW BTN NOT ACTIVE BLOCK");
    // brewTimer(0);
    if (kProbeReadValue < ((float)setPoint - 10.00f))
    {
      setBoiler(HIGH);
    }
    else if (kProbeReadValue >= ((float)setPoint - 10.f) && kProbeReadValue < ((float)setPoint - 5.f))
    {
      if (millis() - heaterWave > HPWR_OUT && !heaterState)
      {
        setBoiler(HIGH);
        heaterState = true;
        heaterWave = millis();
      }
      else if (millis() - heaterWave > HPWR_OUT / BrewCycleDivider && heaterState)
      {
        setBoiler(LOW);
        heaterState = false;
        heaterWave = millis();
      }
    }
    else if ((kProbeReadValue >= ((float)setPoint - 5.f)) && (kProbeReadValue <= ((float)setPoint - 0.25f)))
    {
      if (millis() - heaterWave > HPWR_OUT * BrewCycleDivider && !heaterState)
      {
        setBoiler(HIGH);
        heaterState = !heaterState;
        heaterWave = millis();
      }
      else if (millis() - heaterWave > HPWR_OUT / BrewCycleDivider && heaterState)
      {
        setBoiler(LOW);
        heaterState = !heaterState;
        heaterWave = millis();
      }
    }
    else
    {
      setBoiler(LOW);
    }
  }
}

//#############################################################################################
//###############################____PRESSURE_CONTROL____######################################
//#############################################################################################

void setPressureProfile()
{
  int newBarValue;

  if (brewActive)
  { // runs this only when brew button activated
    long timeIn = (millis() > activeBrewingStart) ? (int)((millis() - activeBrewingStart) / 1000) : 0.0f;
    if (preinfusion && timeIn <= preinfuseTime) // PI State
    {
      newBarValue = preinfusePressureBar;
      preinfusionFinished = false;
    }
    else if (preinfusion && timeIn <= preinfuseTime + preinfuseSoakTime) // Soak State
    {
      newBarValue = 0;
      preinfusionFinished = false;
    }

    else // PP State
    {
      newBarValue = brewPressureBar;
      preinfusionFinished = true;
    }
  }
  else
  {
    newBarValue = 0;
    preinfusionFinished = false;
  }
  if (newBarValue != pump.getPower())
  {
    setPressure(newBarValue);
  }
  // Serial.println(pump.counter);
}

void setPressure(int targetValue)
{
  // int pumpValue = targetValue;
  if (targetValue <= 0)
  {
    pump.begin(OFF);
    pump.setPower(0);
  }
  else
  {
    pump.begin(ON);
  }
  // Serial.println(pumpValue);

  pump.setPower(targetValue);
}

//#############################################################################################
//###############################_____HELPER_FUCTIONS____######################################
//#############################################################################################

// Send Events
void sendEvents()
{
  if ((millis() - lastTime) > timerDelay)
  {
    // Send Events to the client with the Sensor Readings
    events.send("ping", NULL, millis());
    events.send(getSensorReadings().c_str(), "new_readings", millis());
    lastTime = millis();
  }
}

// Function to get the state of the brew switch button
bool brewState()
{
  return digitalRead(brewPin) == LOW; // pin will be low when switch is ON.
}

void brewTimer(bool start)
{ // small function for easier brew timer start/stop
  if (!brewTimerActive && start)
  {
    activeBrewingStart = millis();
    brewTimerActive = true;
  }
  else if (!start)
  {
    brewTimerActive = false;
    activeBrewingStart = 4294967295;
  }
}

void brewDetect()
{
  if (brewState())
  {
    if (!brewActive)
    {
      brewTimer(true); //  timer start
    }
    brewActive = true;
  }
  else
  {
    brewTimer(false);
    brewActive = false;
    brewingTimer = millis();
  }
}

// Actuating the heater element
void setBoiler(int val)
{

  if (val == HIGH)
  {
    digitalWrite(relayPin, HIGH); // boilerPin -> HIGH
    heaterStatus = true;
  }
  else
  {
    digitalWrite(relayPin, LOW); // boilerPin -> LOW
    heaterStatus = false;
    ;
  }
}

float mapRange(float refNumber, float refStart, float refEnd, float targetStart, float targetEnd, int decimalPrecision)
{
  float deltaRef = refEnd - refStart;
  float deltaTarget = targetEnd - targetStart;

  float pct = fmax(0.0f, fmin(1.0f, abs((refNumber - refStart) / deltaRef)));
  float finalNumber = targetStart + pct * deltaTarget;

  int calcScale = (int)pow(10, decimalPrecision);
  return (float)round(finalNumber * calcScale) / calcScale;
}

//#############################################################################################
//###############################____INIT_AND_ADMIN_CTRL____###################################
//#############################################################################################

void pinInit()
{
  pinMode(relayPin, OUTPUT);
  pinMode(brewPin, INPUT_PULLUP);
}

void initSPIFFS()
{
  if (!SPIFFS.begin())
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Initialize WiFi
void initWiFi()
{
  delay(2000);
  //WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);
  //Serial.print("Connecting to WiFi ..");
  //   while (WiFi.status() != WL_CONNECTED)
  //   {
  //     Serial.print('.');
  //     delay(1000);
  //   }
  //   Serial.println(WiFi.localIP());
  // }

  Serial.print("[+] AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());
}
void initWebServer()
{
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html"); });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/settings.html", String(), false, processor); });

  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        preferences.begin("settingsPref", false);
        String value;
        if (request->hasParam("tempInput", true)) {
            value = request->getParam("tempInput", true)->value();
            preferences.putInt("setPoint", value.toInt());
        };
        if (request->hasParam("offsetInput", true)) {
            value = request->getParam("offsetInput", true)->value();
            preferences.putInt("offsetTemp", value.toInt());
        };
        if (request->hasParam("PPInput", true)) {
            value = request->getParam("PPInput", true)->value();
            preferences.putInt("brewPressure", value.toInt());
        };
        if (request->hasParam("PICheck", true)) {
            preferences.putBool("preinfusion", true);
            if (request->hasParam("PIInput", true)) {
              value = request->getParam("PIInput", true)->value();
              preferences.putInt("preinfPress", value.toInt());
            };
            if (request->hasParam("PItime", true)) {
              value = request->getParam("PItime", true)->value();
              preferences.putInt("preinfuseTime", value.toInt());
            };
            if (request->hasParam("Soaktime", true)) {
              value = request->getParam("Soaktime", true)->value();
              preferences.putInt("preinfSoakTime", value.toInt());
            };

        } else {
            preferences.putBool("preinfusion", false);
        };
        preferences.end();
        getPreferences();
        request->send(SPIFFS, "/post.html", "text/html"); });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        resetPreferences();
        getPreferences();
        request->send(SPIFFS, "/reset.html", "text/html"); });

  server.serveStatic("/", SPIFFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String(); });

  server.addHandler(&events);

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  // Start server
  server.begin();
}

//#############################################################################################
//###########################____SETTINGS PAGE PLACE HOLDERS____###############################
//#############################################################################################

String processor(const String &var)
{
  // Serial.println(var);
  if (var == "setTemp")
  {
    return String(setPoint);
  }
  else if (var == "setOffset")
  {
    return String(offsetTemp);
  }
  else if (var == "PP")
  {
    return String(brewPressureBar);
  }
  else if (var == "PreInfusion" && preinfusion)
  {
    return String("Checked");
  }
  else if (var == "PI")
  {
    return String(preinfusePressureBar);
  }

  else if (var == "PItime")
  {
    return String(preinfuseTime);
  }
  else if (var == "Soaktime")
  {
    return String(preinfuseSoakTime);
  }

  return String();
}
