/*
*	BoilerMonitor.ino, Copyright Jonathan Mackey 2022
*
*	GNU license:
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
* 
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*	Please maintain this license information along with authorship and copyright
*	notices in any redistribution of this code.
*
*/
#include <Arduino.h>
#include "MSPeriod.h"

// ATtiny13A pins
const uint8_t kMOSIPin 		= 0;		// PB0
const uint8_t kBoilerPin	= 1;		// PB1
const uint8_t kSCKPin		= 2;		// PB2
const uint8_t kGreenLEDPin	= 3;		// PB3
const uint8_t kRedLEDPin	= 4;		// PB4
const uint8_t kResetPin		= 5;		// PB5

const uint8_t kBoilerPinMask = _BV(PINB1);

static bool		sBoilerStateChanged = true;
static uint8_t	sFlashState = 0;
#define FLASH_PERIOD	500
MSPeriod		flashPeriod;

/*********************************** setup ************************************/
void setup(void)
{
	pinMode(kGreenLEDPin, OUTPUT);		// PB4
	pinMode(kRedLEDPin, OUTPUT);		// PB3
	pinMode(kBoilerPin, INPUT_PULLUP);	// PB1
	
	// Pull-up all unused pins to save power
	pinMode(kMOSIPin, INPUT_PULLUP);	// PB0
	pinMode(kSCKPin, INPUT_PULLUP);		// PB2
	
	cli();
	ADCSRA &= ~_BV(ADEN);		// Turn off ADC to save power.
	PRR |= _BV(PRADC);
	/*
	*	Setup pin change interrupt for the boiler pin.
	*/
	PCMSK = _BV(PCINT1);
	GIMSK = _BV(PCIE);
	sei();
}

/************************************ loop ************************************/
void loop(void)
{
	if (sBoilerStateChanged)
	{
		sBoilerStateChanged = false;
		if (((~PINB) & kBoilerPinMask) != 0)
		{
			flashPeriod.Set(FLASH_PERIOD);
			flashPeriod.Start();
			digitalWrite(kGreenLEDPin, LOW);
			digitalWrite(kRedLEDPin, HIGH);
			sFlashState = 1;
		} else
		{
			flashPeriod.Set(0);
			digitalWrite(kGreenLEDPin, HIGH);
			digitalWrite(kRedLEDPin, LOW);
		}
	} else if (flashPeriod.Passed())
	{
		flashPeriod.Start();
		sFlashState++;
		digitalWrite(kRedLEDPin, sFlashState & 1);
	}
	
}

/*************************** Pin Changed Interrupt ****************************/
ISR(PCINT0_vect)
{
	sBoilerStateChanged = true;
}
