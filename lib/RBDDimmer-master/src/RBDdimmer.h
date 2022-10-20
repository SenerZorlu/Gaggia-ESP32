#ifndef RBDDIMMER_H
#define RBDDIMMER_H

#include <stdlib.h>
#include "esp32/RBDmcuESP32.h"

typedef enum
{
  OFF = false,
  ON = true
} ON_OFF_typedef;

class dimmerLamp
{
private:
  void timer_init(void);
  void ext_int_init(void);

public:
  uint8_t dimmer_pin;
  uint8_t zc_pin;
  uint8_t range;
  char divider;
  volatile int counter;
  volatile ON_OFF_typedef dimState;
  volatile int dimPower;
  volatile uint8_t zeroCross;
  volatile bool skip;
  volatile int a;

  dimmerLamp(uint8_t user_dimmer_pin, uint8_t zc_dimmer_pin, uint8_t _range, char _divider);

  void begin(ON_OFF_typedef ON_OFF);
  void setPower(int power);
  int getPower(void);
};

#endif
