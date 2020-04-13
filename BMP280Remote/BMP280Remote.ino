
/*
*	BMP280Remote.ino, Copyright Jonathan Mackey 2019
*
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
*	This code uses a modified version of Felix Rusu's RFM69 library.
*	Copyright Felix Rusu 2016, http://www.LowPowerLab.com/contact
*
*	The RFM69 library modifications allow it to be used on an ATtiny84 MCU.
*	I shedded some code (about 1K), adjusted the SPI speed, and made some
*	optimizations.
*
*	This code also uses a modified version of Jack Christensen's tinySPI Library.
*	Copyright 2018 Jack Christensen https://github.com/JChristensen/tinySPI
*
*	The tinySPI Library modification allows for 800KHz, 2MHz and 4MHz SPI speeds
*	when used with an 8MHz cpu clock.  The original code allowed for only a
*	single speed of approximately 666KHz with a 8MHz clock.
*/
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/interrupt.h>
#include "RFM69.h"    // https://github.com/LowPowerLab/RFM69
#include "tinySPI.h"
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "BMP280SPI.h"

/*
*	IMPORTANT RADIO SETTINGS
*/
//Frequency of the RFM69 module:
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ

/*
*	This remote goes to sleep for 4 seconds then wakes up and transmits the
*	current barometric pressure and temperature.
*
*	Format: <message><pressure><temperature><crc>
*/

/*
*	Setting fuses and uploading a new sketch:
*	The board has a standard ICSP header and a ON/OFF PROG switch.  When
*	programming, the switch should be in the OFF PROG position.  When in the OFF
*	PROG position the board is powered from the ICSP.  This position also powers
*	a pullup for NSS of the RFM68 module.  Without this pullup the RFM69's NSS
*	would be floating which will cause problems when trying to set fuses and
*	load sketches.  Any 3v3 ISP can be used.  Do not use a 5 volt ISP or you
*	will damage the RFM69.
*/
#define HAS_RADIO
#ifdef HAS_RADIO
RFM69 radio;
#endif

#define BMP280CSPin		0	// PA0
#define UnusedPA1		1	// PA1
#define UnusedPA2		2	// PA2 (labeled RXD)
#define TxPin			3	// PA3
#define RFM69SelectPin	7	// PA7
#define RFM69DIO0Pin	8	// PB2
#define UnusedPB1		9	// PB1
#define UnusedPB0		10	// PB0

BMP280SPI	bmp280(BMP280CSPin);

// The Arduino gcc configuration limits literal constants to 2 bytes so they
// have to be defined as regular hex values
const uint32_t	kBMP280 = 0x424D5032;	// 'BMP2';
const uint8_t	kTargetID = 0;	// 0 = broadcast, 1 = gateway

/********************************* setup **************************************/
void setup(void)
{    
	pinMode(TxPin, INPUT_PULLUP);
	// Any unused pins are set as inputs pulled high to have
	// less of an impact on battery life.
	pinMode(UnusedPA1, INPUT_PULLUP);
	pinMode(UnusedPA2, INPUT_PULLUP);
	pinMode(UnusedPB0, INPUT_PULLUP);
	pinMode(UnusedPB1, INPUT_PULLUP);

	/*
	*	If the reset source was a watchdog reset THEN
	*	increment the watchdog reset count EEPROM param.
	*	The watchdog reset count is 16 bits starting at address 2.
	*/
	if (MCUSR & _BV(WDRF))
	{
		MCUSR &= ~_BV(WDRF);
		uint16_t	wdrCount;
		EEPROM.get(2, wdrCount);
		wdrCount++;
		EEPROM.put(2, wdrCount);
	}
	
	SPI.begin();
	bmp280.begin();

	// Read the network and node IDs from EEPROM
	{
		uint8_t	networkID = EEPROM.read(0);
		uint8_t	nodeID = EEPROM.read(1);

		radio.initialize(FREQUENCY, nodeID, networkID);
	}
	radio.sleep();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	cli();
	ADCSRA &= ~_BV(ADEN);		// Turn off ADC to save power.
	sei();
	
	/*
	*	Enable the watchdog and leave it enabled.  If for any reason the remote
	*	doesn't go back to sleep by the next watchdog period the MCU will reset.
	*/
	wdt_enable(WDTO_4S);
}

/******************************** watchdog ************************************/
/*
*	This gets triggered to wake up from power down to send the current
*	temperature and pressure to the host.
*/
ISR(WATCHDOG_vect)
{
	// WDIE is NOT set here.  Resetting the MCU is desired if some action is
	// isn't returning before the next timeout.
	// WDTCSR |= _BV(WDIE);	// Set WDIE.  WDIE gets cleared when the interrupt
							// is called It needs to be set back to 1 otherwise
							// the next timeout will reset the MCU
}

/********************************** loop **************************************/
void loop(void)
{
	SendBMPPacket();
	GoToSleep();
}

/******************************* SendBMPPacket ********************************/
void SendBMPPacket(void)
{
	struct
	{
		uint32_t	message;
		int32_t		temp;
		uint32_t	pres;
	} packet;
	bmp280.DoForcedRead(packet.temp, packet.pres);

	packet.message = kBMP280;
    /*
    *	Do a send without first checking if the channel is clear.  The RFM69
    *	gets into a strange state where canSend() always returns false even
    *	though there isn't another transmitter for miles.  When this happens
    *	sending isn't possible until the transceiver's power is cycled.  Simply
    *	reinitializing by calling RFM69::initialize() doesn't help.
    */
    radio.sendFrame(kTargetID, &packet, sizeof(packet));
    //radio.send(kTargetID, &packet, sizeof(packet));
	radio.sleep();
}

/********************************* GoToSleep **********************************/
/*
*	Goes into a timed sleep
*/
void GoToSleep(void)
{
	WDTCSR |= _BV(WDIE);
	sleep_mode();	// Go to sleep
}

