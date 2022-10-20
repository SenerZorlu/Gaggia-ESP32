#if defined(ARDUINO_ARCH_ESP32)

#include "RBDmcuESP32.h"

unsigned char dividerCounter = 1;
static dimmerLamp *dimmer;

dimmerLamp::dimmerLamp(uint8_t user_dimmer_pin, uint8_t zc_dimmer_pin, uint8_t _range, char _divider) : dimmer_pin(user_dimmer_pin),
																										zc_pin(zc_dimmer_pin),
																										range(_range)

{
	dimmer = this;
	divider = _divider > 0 ? _divider : 1;
	zeroCross = 0;
	skip = true;
	a = 0;

	pinMode(user_dimmer_pin, OUTPUT);
}

void dimmerLamp::timer_init(void)
{
	hw_timer_t *timer = NULL;
	// Use 1st timer of 4 (counted from zero).
	// Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more info).
	timer = timerBegin(0, 80, true);
	// Attach onTimer function to our timer.
	timerAttachInterrupt(timer, &onTimerISR, true);
	// Set alarm to call onTimer function every second (value in microseconds).
	// Repeat the alarm (third parameter)
	timerAlarmWrite(timer, 100, true);
	// Start an alarm
	timerAlarmEnable(timer);
}

void dimmerLamp::ext_int_init(void)
{
	pinMode(zc_pin, INPUT_PULLUP);

	attachInterrupt(zc_pin, isr_ext, RISING);
}

void dimmerLamp::begin(ON_OFF_typedef ON_OFF)
{
	dimState = ON_OFF;
	if (dimState == OFF)
	{
		a = 0;
	}
	timer_init();
	ext_int_init();
}

void dimmerLamp::setPower(int power)
{
	if (power >= range)
	{
		power = range;
	}
	dimPower = power;

	delay(1);
}

int dimmerLamp::getPower(void)
{
	if (dimState == ON)
		return dimPower;
	else
		return 0;
}

void ARDUINO_ISR_ATTR isr_ext()
{
	if (dividerCounter >= dimmer->divider)
	{
		dividerCounter = 1;
		if (dimmer->dimState == ON)
		{
			dimmer->zeroCross = 1;
		}
	}
	else
	{
		dividerCounter++;
	}
}

void ARDUINO_ISR_ATTR onTimerISR()
{

	if (dimmer->zeroCross == 1)
	{
		dimmer->a += dimmer->dimPower;

		if (dimmer->a >= dimmer->range)
		{
			dimmer->a -= dimmer->range;
			dimmer->skip = false;
		}
		else
		{
			dimmer->skip = true;
		}

		if (dimmer->a > dimmer->range)
		{
			dimmer->a = 0;
			dimmer->skip = false;
		}
	}
	if (dimmer->skip)
	{
		digitalWrite(dimmer->dimmer_pin, LOW);
		dimmer->zeroCross = 0;
		// dimmer -> counter++;
	}
	else
	{
		digitalWrite(dimmer->dimmer_pin, HIGH);
		// dimmer -> counter =0;
	}
}

#endif