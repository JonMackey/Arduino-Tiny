/*
*	AdjustClock.ino
*	Copyright (c) 2017 Jonathan Mackey
*
*	Setup: 
*		ATtiny85 Fuse settings:
*		LFuse: A2, HFuse: DF, EFuse: FF (8 MHz out on pin 3)
*
*	Hookup momentary switches between physical pin 7 and ground, and between
*	6 and ground to implement the up and down buttons.
*	Hookup a frequency counter or o-scope to physical pin 3
*	Hookup a 9600 baud USART Rx to physical pin 5
*
*	1/PB5 = Configured as the RESET pin (default)
*	2/PB3 = Configured as CLKI (not used)
*	3/PB4 = Configured as CLKO
*	4/GND
*	5/PB0 = Serial Tx
*	6/PB1 = up button.  Connect other side to GND
*	7/PB2 = down button.  Connect other side to GND
*	8/VCC  Do not set the clock over 8Mhz if VCC = 3.3V
*
*	This uses SendOnlySoftwareSerial, which is SoftwareSerial modified to
*	only transmit, courtesy of Nick Gammon.
*	Download from http://gammon.com.au/Arduino/SendOnlySoftwareSerial.zip
*
*	Usage:
*	Sends the clock to an output pin to be measured by a frequency counter or
*	o-scope.  The clock is adjusted by pressing up and down buttons.  The clock
*	calibration value (OSCCAL) and delta is sent to the USART.  The OSCCAL is
*	NOT set permenantly, this value can be placed in the EEPROM or the delta
*	can be added to OSCCAL in setup().
*/
#include <Arduino.h>
#include <avr/eeprom.h>

#define UpPin 2				// PB2/PINB2
#define DownPin	1			// PB1/PINB1
#define TxPin	0			// PB0
#define DEBOUNCE_DELAY	20	// ms

#include <SendOnlySoftwareSerial.h>

SendOnlySoftwareSerial swSerial(TxPin); // RX, TX
static uint8_t	sStartPinsState = 6;
static uint8_t	sLastPinsState = 6;
static uint32_t	sDebounceStartTime = 0;
static int32_t	sOSCCAL_Delta = 0;

void PrintOSCCAL(void);

/*********************************** setup ************************************/
void setup(void)
{
	pinMode(UpPin, INPUT_PULLUP);  		// Up Button
	pinMode(DownPin, INPUT_PULLUP); 	// Down Button
	swSerial.begin(9600);
	PrintOSCCAL();
	/*swSerial.print("E2END = ");
	swSerial.println(eeprom_read_byte((uint8_t*const)(E2END -  0)));
	swSerial.print("E2END-1 = ");
	swSerial.println(eeprom_read_byte((uint8_t*const)(E2END -  1)));
	swSerial.print("E2END-2 = ");
	swSerial.println(eeprom_read_byte((uint8_t*const)(E2END -  2)));*/
}

/******************************** PrintOSCCAL *********************************/
void PrintOSCCAL(void)
{
	swSerial.print("OSCCAL = ");
	swSerial.print(OSCCAL);
	swSerial.print(", Delta = ");
	swSerial.println(sOSCCAL_Delta);
}

/*********************************** loop *************************************/
void loop(void)
{
	uint32_t	currentTime = millis();
	uint32_t	debounceDuration;
	if (currentTime > sDebounceStartTime)
	{
		debounceDuration = currentTime - sDebounceStartTime;
	} else
	{
		// Handles the case where the micros wraps around back to zero.
		debounceDuration = (0xFFFFFFFFU - sDebounceStartTime) + currentTime;
	}
	/*
	*	If a debounce period has passed
	*/
	if (debounceDuration >= DEBOUNCE_DELAY)
	{
		uint8_t		pinsState = PINB & 0b110;
		/*
		*	If debounced
		*/
		if (sStartPinsState == pinsState)
		{
			if (sLastPinsState != pinsState)
			{
				sLastPinsState = pinsState;
				switch (pinsState)
				{
					case 0b010:	// down button pressed
						OSCCAL -= 1;
						sOSCCAL_Delta--;
						PrintOSCCAL();
						break;
					case 0b100:	// up button pressed
						OSCCAL += 1;
						sOSCCAL_Delta++;
						PrintOSCCAL();
						break;
				}
			}
		}
		sStartPinsState = pinsState;
		sDebounceStartTime = currentTime;
	}
}
