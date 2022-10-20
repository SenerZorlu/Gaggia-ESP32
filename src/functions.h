#ifndef GAGGIA_FUNCTIONS_H
#define GAGGIA_FUNCTIONS_H

void getPreferences();
void resetPreferences();
void sensorsRead();
String getSensorReadings();
void justDoCoffee();
void setPressureProfile();
void setPressure(int targetValue);
void sendEvents();
bool brewState();
void brewTimer(bool start);
void brewDetect();
void setBoiler(int val);
float mapRange(float refNumber, float refStart, float refEnd, float targetStart, float targetEnd, int decimalPrecision);
void pinInit();
void initSPIFFS();
void initWiFi();
void initWebServer();
String processor(const String &var);

#endif