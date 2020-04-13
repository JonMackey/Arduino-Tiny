
/*
*	Dust Collector Full board code, Copyright Jonathan Mackey 2018
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
*
*
*	This very simple sketch triggers an alarm once the voltage across the motor
*	control MOSFET exceeds sTriggerThreshold.  The design assumes the use of a
*	motor control MOSFET with a fairly high on resistance by today's standards.
*	In this case a BSS138 is used.
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
#include <avr/interrupt.h>
#include <RFM69.h>    // https://github.com/LowPowerLab/RFM69
#include <tinySPI.h>
#include <EEPROM.h>

/*
*	IMPORTANT RADIO SETTINGS
*/
#define NODEID            56	// Unique for each node on same network
#define NETWORKID         100	// The same on all nodes that talk to each other
#define GATEWAYID         1
//Frequency of the RFM69 module:
#define FREQUENCY         RF69_433MHZ
//#define FREQUENCY         RF69_868MHZ
//#define FREQUENCY         RF69_915MHZ

/*
*	Uploading a new sketch:
*	The DCF board has a standard ICSP header and a PROG/RUN DPDT switch.  Pin 1
*	of the ICSP header is as marked on the board.  When programming, the switch
*	should be in the PROG position (closest to the ICSP header.)  Any 3v3 ISP
*	can be used.  Do not use a 5 volt ISP, you may damage the RFM69.  The ICSP
*	cable I use is only 5 wire, with 3v3 not connected (not supplied to the
*	board by the ISP.)  If you use a 6 wire cable then obviously disconnect the
*	12V power.
*/

/*
*	If the current threshold setting is not adequate (too much/too little):
*	USE_SERIAL is used to determine the amount of torque needed to trigger an
*	alarm. Connect a 3v3 serial board to the serial connection on the DCF board.
*	From the edge of the board: GND TX RX.  TX connects to RX of
*	the serial board, RX to TX, and GND.
*	From the Arduino IDE or any other serial terminal, connect to the serial
*	port of the DCF module.  Turn on the module and squeeze the motor shaft till
*	till the desired trigger torque is reached.  Use the value displayed to set
*	the value of sTriggerThreshold below via serial commands.
*	If the motor stops too early then increase the threshold by sending a '+'
*	character followed by an 'S' character to restart the motor.  See the loop()
*	function for a list of the commands you can send.
*	The initial value of sTriggerThreshold is read from EEPROM.  For new boards
*	the kDefaultTriggerThreshold is used till a new triggerThreshold value is
*	saved to EEPROM. 
*/
#define USE_SERIAL
#ifdef USE_SERIAL
#include <SoftwareSerial.h>
#include "MSPeriod.h"

#define SERIAL_BAUD   19200
#define RxPin 2			// PA2
#define TxPin 3			// PA3
SoftwareSerial swSerial(RxPin, TxPin); // RX, TX

#endif

#define HAS_RFM69
#ifdef HAS_RFM69
RFM69 radio;
#endif

#define MotorControlPin	9	// PB1 Turns the motor MOSFET off/on
#define FlasherPin		1	// PA1 Turns the flasher MOSFET off/on
#define MotorVoltagePin 0	// PA0 1/2 voltage across MOSFET. 

static bool	sMotorStopped = false;

#define SAMPLE_SIZE	8
static uint16_t	sSampleCount;
static uint16_t	sRingBuf[SAMPLE_SIZE];
static uint8_t	sRingBufIndex;
static uint16_t	sSampleAccumulator;

static const uint32_t kThresholdLowerLimit = 15;
static const uint32_t kDefaultTriggerThreshold = 30;
static const uint32_t kThresholdUpperLimit = 100;
static uint32_t	sTriggerThreshold;

void StartMotor(void);
#ifdef USE_SERIAL
void ReportThreshold(void);
MSPeriod	averageDelay(250);	// Limits sending serial average values to once every 1/4 second.
#endif

/*********************************** setup ************************************/
void setup(void)
{    
#ifdef USE_SERIAL
	swSerial.begin(SERIAL_BAUD);
	delay(10);
#endif  
	pinMode(FlasherPin, OUTPUT);
	pinMode(MotorControlPin, OUTPUT);
	
	EEPROM.get(0, sTriggerThreshold);
	/*
	*	Sanity check the threshold value.
	*/
	if (sTriggerThreshold > kThresholdUpperLimit ||
		sTriggerThreshold < kThresholdLowerLimit)
	{
		sTriggerThreshold = kDefaultTriggerThreshold;
	}
	StartMotor();

#ifdef HAS_RFM69
	radio.initialize(FREQUENCY,NODEID,NETWORKID);

	radio.sendWithRetry(GATEWAYID, "START", 6);

	radio.sleep();
#endif
}

/********************************* StartMotor *********************************/
void StartMotor(void)
{
	sMotorStopped = false;
	digitalWrite(FlasherPin, LOW);
	digitalWrite(MotorControlPin, HIGH);
#ifdef USE_SERIAL
	ReportThreshold();
	averageDelay.Start();
#endif
	sSampleCount = 0;
	sSampleAccumulator = 0;
	sRingBufIndex = 0;
	delay(2000);	// Give the motor a chance to start.
}

#ifdef USE_SERIAL
/****************************** ReportThreshold *******************************/
void ReportThreshold(void)
{
	swSerial.print('R');
	swSerial.println(sTriggerThreshold);
}
#endif
/************************************ loop ************************************/
void loop(void)
{
#ifdef USE_SERIAL
	if (swSerial.available())
	{
		switch (swSerial.read())
		{
			case 'S':	// Start motor if stopped.
				if (sMotorStopped)
				{
					StartMotor();
				}
				break;
			case '+':	// Increase the trigger threshold by 1
				if (sTriggerThreshold < kThresholdUpperLimit)
				{
					sTriggerThreshold++;
				}
				ReportThreshold();
				break;
			case '-':	// Decrease the trigger threshold by 1
				if (sTriggerThreshold > kThresholdLowerLimit)
				{
					sTriggerThreshold--;
				}
				ReportThreshold();
				break;
			case 'D':	// Restore the default trigger threshold
				sTriggerThreshold = kDefaultTriggerThreshold;
				break;
			case 's':	// Save trigger threshold to EEPROM
				EEPROM.put(0, sTriggerThreshold);
				break;
			case 'R':	// Report trigger threshold
				ReportThreshold();
				break;
		}
	}
#endif
	if (!sMotorStopped)
	{
		uint16_t	reading = analogRead(MotorVoltagePin);
		sSampleAccumulator += reading;					// Add the newest reading
		uint16_t	oldestReading = sRingBuf[sRingBufIndex];
		sRingBuf[sRingBufIndex] = reading;				// Save the newest reading
		sRingBufIndex++;
		if (sRingBufIndex >= SAMPLE_SIZE)
		{
			sRingBufIndex = 0;
		}
		if (sSampleCount >= SAMPLE_SIZE)
		{
			sSampleAccumulator -= oldestReading;	// Remove the oldest reading
		} else
		{
			sSampleCount++;
		}
		uint16_t average = sSampleAccumulator/sSampleCount;
		if (average > sTriggerThreshold)
		{
			sMotorStopped = true;
			digitalWrite(MotorControlPin, LOW);
			digitalWrite(FlasherPin, HIGH);
		#ifdef HAS_RFM69
			radio.sendWithRetry(GATEWAYID, "DCF", 4);
			radio.sleep();
		#endif
		#ifdef USE_SERIAL
			// To force a print
			averageDelay.Start(-(int32_t)averageDelay.Get());
		#endif
		}
		#ifdef USE_SERIAL
		if (averageDelay.Passed())
		{
			swSerial.print('A');
			swSerial.println(average);
			averageDelay.Start();
		}
		#endif
	}
}
