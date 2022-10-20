#ifndef RBDMCUESP32_H
#define RBDMCUESP32_H

#include "Arduino.h"
#include "RBDdimmer.h"
#include <stdio.h>
#include <esp32-hal-gpio.h>
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "rom/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_struct.h"



void ARDUINO_ISR_ATTR isr_ext();
void ARDUINO_ISR_ATTR onTimerISR();

#endif
