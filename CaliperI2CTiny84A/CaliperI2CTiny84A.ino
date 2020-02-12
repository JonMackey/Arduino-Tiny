/*
*	CaliperI2CTiny84A, Copyright Jonathan Mackey 2018
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
*	This code uses USI based TWI/I2C library for Arduino, USIWire
*	Copyright (c) 2017 Puuu.  All right reserved.
*
*
*	To get the ATtiny Arduino package, install attiny by David A. Mellis
*		https://raw.githubusercontent.com/damellis/attiny/ide-1.6.x-boards-manager/package_damellis_attiny_index.json
*	The Tiny I2C library I used is USIWire, install it from the Sketch->Include Library->Manage Libraries dialog
*
*	Serial commands: (when HAS_SERIAL is defined)
*	z - zeros the caliper
*	s - puts the caliper to sleep (turns if off)
*	w - wakes up the caliper (turns it on)
*	v - Serial send the current caliper value formatted in the current units
*	V - Enter serial send the caliper value whenever it changes mode.
*		Note that the data changed pin will not go high in this mode.
*  		Use v or r to stop sending.  The mode will be reset to use data changed pin.
*	r - Serial send the raw value as hex
*	R - Enter serial send the raw value whenever it changes mode.
*		Note that the data changed pin will not go high in this mode.
*  		Use v or r to stop sending.  The mode will be reset to use data changed pin.
*/
#include <Arduino.h>
#include <avr/interrupt.h>
#include <USIWire.h>

// Pins on the ATtiny84A are assigned clockwise starting at PA0
// PB3/Reset is an exception, it's pin 11
#define I2C_SLAVE_ADDR  0x20		// I2C slave base address
#define Address0Pin		0			// PA0
#define Address1Pin		1			// PA1
#define RxPin 			2			// PA2
#define TxPin 			3			// PA3
// SCI/SCK is PA4
// MISO is PA5
// SDA/MOSI is PA6
#define CaliperOnOffPin	7			// PA7
#define CaliperDataChangedPin	8	// PB2
#define CaliperClocKPin 9			// PB1/PCINT9
#define CaliperDataPin	10			// PB0

// SoftwareSerial0 is a modified version of SoftwareSerial for PortA pins.
// SoftwareSerial will take over ALL pin change interrupts.  The SoftwareSerial0
// modification only uses PCINT0_vect leaving PCINT1_vect for the
// CaliperDataChangedPin on PortB
#define HAS_SERIAL
#ifdef HAS_SERIAL
#include "CaliperUtils.h"
#include <SoftwareSerial0.h>
SoftwareSerial0 swSerial(RxPin, TxPin); // RX, TX
#define BAUD_RATE	19200
#endif

volatile	uint8_t		gBitsInPacket;
volatile	uint32_t	gPacketStart;
volatile	uint32_t	gValue;
volatile	uint32_t	gPacketValue;
static 		uint32_t	sCurrentValue;
static		bool		sDataChanged;
static		uint8_t		sMode;
enum SDataChangedMode
{
	eSetDataChangedPinOnChange,
	eSerialSendRawOnChange,
	eSerialSendFormattedOnChange
};

/*********************************** setup ************************************/
void setup(void)
{
#ifdef HAS_SERIAL
	swSerial.begin(BAUD_RATE);
	delay(10);
	swSerial.println(F("Starting..."));
#endif
		
	pinMode(Address0Pin, INPUT);  			// Address bit 0
	pinMode(Address1Pin, INPUT);  			// Address bit 1
	pinMode(CaliperClocKPin, INPUT);  		// Clock
	pinMode(CaliperDataPin, INPUT); 		// Data
 	pinMode(CaliperDataChangedPin, OUTPUT); // Data Changed
 	pinMode(CaliperOnOffPin, OUTPUT); // Controls the power to the caliper
	digitalWrite(CaliperDataChangedPin, LOW);
	digitalWrite(CaliperOnOffPin, HIGH);	// Turn on the caliper
	
 	gPacketStart = micros();
 	sCurrentValue = gPacketValue = 0xFFFFFFFF;
 	gValue = 0;
 	gBitsInPacket = 0;
 	sDataChanged = false;
 	uint8_t	slaveAddr = I2C_SLAVE_ADDR + (digitalRead(Address1Pin)<<1) + digitalRead(Address0Pin);
#ifdef HAS_SERIAL
	swSerial.print(F("Slave Address = 0x"));
	swSerial.println(slaveAddr, HEX);
#endif
	Wire.begin(slaveAddr);	// Join I2C
	Wire.onRequest(DataRequestEvent);			// Register event
	Wire.onReceive(DataReceiveEvent);			// Register event

	cli();
	ADCSRA &= ~_BV(ADEN);	// Turn off ADC to save power.
	GIMSK |= _BV(PCIE1);	// Enable port B for pin change interrupts
    PCMSK1 |= _BV(PCINT9);	// Enable pin PB1 pin change interrupt
	sei();
	
	sMode = eSetDataChangedPinOnChange;
}

/******************************** Clock ISR ***********************************/
/*
*	This ISR assumes only the clock pin on this port is being watched for
*	pin state changes.
*/
ISR(PCINT1_vect)
{
	if (!digitalRead(CaliperClocKPin))
	{
		uint8_t		data = digitalRead(CaliperDataPin);
		uint32_t	now = micros();
		uint32_t	duration = now - gPacketStart;
		/*
		*	If more than 50ms has passed THEN
		*	we're at the start of a new packet.
		*/
		if (duration > 50000)
		{
			gPacketStart = now;
			if (gBitsInPacket == 24)
			{
				gPacketValue = gValue;
			}
			gValue = 0;
			gBitsInPacket = 0;
		}
		gValue >>= 1;
		gBitsInPacket++;
		if (!data)
		{
			gValue |= 0x00800000;
		}
	}
}

/*********************************** Sleep ************************************/
void Sleep(void)
{
	digitalWrite(CaliperOnOffPin, LOW);
}

/************************************ Wake ************************************/
void Wake(void)
{
	digitalWrite(CaliperOnOffPin, HIGH);
}

/*********************************** Zero ************************************/
bool Zero(void)
{
	Sleep();
	delay(300);
	Wake();
}

/***************************** DataReceiveEvent *******************************/
/*
*	Handles commands from the master.
*/
void DataReceiveEvent(
	int	inBytesReceived)
{
	switch(Wire.read())
	{
		case '0':	// Turn off the caliper
			Sleep();
			break;
		case '1':	// Turn on the caliper
			Wake();
			break;
	}
}

/***************************** ClearDataChanged *******************************/
void ClearDataChanged(void)
{
	if (sDataChanged)
	{
		sDataChanged = false;
		digitalWrite(CaliperDataChangedPin, LOW);
	}
}

/***************************** DataRequestEvent *******************************/
void DataRequestEvent(void)
{
	// Respond with message of 3 bytes as expected by the master
	Wire.write((sCurrentValue >> 16)  & 0xFF);
	Wire.write((sCurrentValue >> 8)  & 0xFF);
	Wire.write(sCurrentValue & 0xFF);
	/*
	*	Clear the data changed pin if set.
	*/
	ClearDataChanged();
}

/*********************************** loop *************************************/
void loop(void)
{
	bool valueChanged = gPacketValue != sCurrentValue;
	if (valueChanged)
	{
		sCurrentValue = gPacketValue;
	}
	
	/*
	*	If the value changed THEN
	*	notify the master based on the mode
	*/
	if (valueChanged && !sDataChanged)
	{
	#ifdef HAS_SERIAL
		switch(sMode)
		{
			case eSetDataChangedPinOnChange:
				sDataChanged = true;
				digitalWrite(CaliperDataChangedPin, HIGH);
				break;
			case eSerialSendRawOnChange:
				SendRawValue();
				break;
			case eSerialSendFormattedOnChange:
				SendFormattedValue();
				break;
		}
	#else
		sDataChanged = true;
		digitalWrite(CaliperDataChangedPin, HIGH);
	#endif
	}
#ifdef HAS_SERIAL
	if (swSerial.available() > 0)
	{
		while(swSerial.available())
		{
			switch(swSerial.read())
			{
			case 'z':
				Zero();
				break;
			case 's':
				Sleep();
				break;
			case 'w':
				Wake();
				break;
			case 'v':
				SendFormattedValue();
				sMode = eSetDataChangedPinOnChange;
				break;
			case 'V':
				SendFormattedValue();
				sMode = eSerialSendFormattedOnChange;
				break;
			case 'r':
				SendRawValue();
				sMode = eSetDataChangedPinOnChange;
				break;
			case 'R':
				SendRawValue();
				sMode = eSerialSendRawOnChange;
				break;
			case 'd':
				swSerial.print("sDataChanged = ");
				swSerial.println(sDataChanged ? F("true") : F("false"));
				break;
			}
		}
	}
	#endif
}

#ifdef HAS_SERIAL
/********************************* SendRawValue *******************************/
void SendRawValue(void)
{
	ClearDataChanged();
	uint32_t	rawValue = sCurrentValue;
	swSerial.println(sCurrentValue, HEX);
}

/****************************** SendFormattedValue ****************************/
void SendFormattedValue(void)
{
	ClearDataChanged();
	uint32_t	rawValue = sCurrentValue;
	char buffer[50];
	char*	endBuffPtr = UInt32ToBinaryStr(rawValue, buffer, 0x800000);	// display the last 6 nibbles
	*endBuffPtr = 0;
	uint8_t	dPlaces;
	float	value = CaliperUtils::RawToValue(rawValue, &dPlaces);
	swSerial.print(buffer);
	swSerial.print(F(" = "));
	swSerial.print(value, dPlaces);
	swSerial.println(CaliperUtils::IsMetric(rawValue) ? F("mm") : F("in"));
}

/****************************** UInt32ToBinaryStr *****************************/
char* UInt32ToBinaryStr(
	int32_t		inValue,
	char*		inBuffer,
	uint32_t	inMask)
{
	uint32_t	nMask = inMask;
	char*	buffPtr = inBuffer;
	for (; nMask != 0; nMask >>= 1)
	{
		*(buffPtr++) = (inValue & nMask) ? '1':'0';
		if (nMask & 0xEEEEEEEF)
		{
			continue;
		}
		*(buffPtr++) = ' ';
	}
	*buffPtr = 0;
	return(buffPtr);
}
#endif
