/*
*	CaliperI2CTiny85.ino
*	Copyright (c) 2017 Jonathan Mackey
*
*	Turns a caliper into an I2C slave using an ATtiny85.
*
*	Whenever the data changes CaliperDataChangedPin goes high.  The I2C master
*	then reads the changed data.  Reading the data resets the CaliperDataChangedPin
*
*	The I2C master can send a '0' to turn the caliper off and a '1' to turn the
*	caliper on.  To zero the caliper, turn it off, delay 300ms, then turn it on.
*	Note that zeroing by turning it off/on will also set the units to mm.
*
*	>>>>>> IMPORTANT <<<<<<
*	In order to use all 6 io pins on a ATtiny85 you need to set the H fuse to 5F.
*	Doing this will disable low voltage/standard programming of the ATtiny85.
*	In order to go back to the standard setting HFuse DF, you need a high voltage
*	programmer.  They're very easy to make, see:
*	https://arduinodiy.wordpress.com/2015/05/16/high-voltage-programmingunbricking-for-attiny/
*
*	If the idea of disabling low voltage programming via the Arduino ISP makes
*	you nervious, you could decide not to use the on/off or data changed pins.
*
*	To get the Tiny85 Arduino package, install attiny by David A. Mellis
*		https://raw.githubusercontent.com/damellis/attiny/ide-1.6.x-boards-manager/package_damellis_attiny_index.json
*	The Tiny I2C library I used is USIWire, install if from the Sketch->Include Library->Manage Libraries dialog
*/
#include <Arduino.h>
#include <avr/interrupt.h>
#include <USIWire.h>

#define I2C_SLAVE_ADDR  0x26		// I2C slave address
#define CaliperClocKPin 1			// PB1/PCINT1
#define CaliperDataChangedPin	3	// PB3
#define CaliperDataPin	4			// PB4
#define CaliperOnOffPin	5			// PB5

volatile	uint8_t		gBitsInPacket;
volatile	uint32_t	gPacketStart;
volatile	uint32_t	gValue;
volatile	uint32_t	gPacketValue;
static 		uint32_t	sCurrentValue;
static		bool		sDataChanged;

/*********************************** setup ************************************/
void setup(void)
{
	GIMSK = 0b00100000;    // Turns on pin change interrupts
    PCMSK = 0b00000010;    // Turn on interrupt for pin PB1
    sei();
		
	pinMode(CaliperClocKPin, INPUT);  		// Clock
	pinMode(CaliperDataPin, INPUT); 		// Data
 	pinMode(CaliperDataChangedPin, OUTPUT); // Data Changed
 	pinMode(CaliperOnOffPin, OUTPUT); // Controls the power to the caliper
	digitalWrite(CaliperDataChangedPin, LOW);
	digitalWrite(CaliperOnOffPin, HIGH);	// Turn on the caliper
	
 	gPacketStart = micros();
 	sCurrentValue = gPacketValue = 0xBADBADBAD;
 	gValue = 0;
 	gBitsInPacket = 0;
 	sDataChanged = false;
	Wire.begin(I2C_SLAVE_ADDR);			// Join I2C
	Wire.onRequest(DataRequestEvent);	// Register event
	Wire.onReceive(DataReceiveEvent);	// Register event
}

/******************************** Clock ISR ***********************************/
/*
*	This ISR assumes only the clock pin on this port is being watched for
*	pin state changes.
*/
ISR(PCINT0_vect)
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
			digitalWrite(CaliperOnOffPin, LOW);
			break;
		case '1':	// Turn on the caliper
			digitalWrite(CaliperOnOffPin, HIGH);
			break;
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
	*	If the data changed pin is set THEN
	*	clear it.
	*/
	if (sDataChanged)
	{
		sDataChanged = false;
		digitalWrite(CaliperDataChangedPin, LOW);
	}
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
	*	notify the master
	*/
	if (valueChanged && !sDataChanged)
	{
		sDataChanged = true;
		digitalWrite(CaliperDataChangedPin, HIGH);
	}
}
