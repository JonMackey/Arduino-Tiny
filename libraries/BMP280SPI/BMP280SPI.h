/*
*	BMP280SPI.h, Copyright Jonathan Mackey 2019
*	Interface class for the Bosch BMP280 Digital Pressure Sensor.
*
*	This class is not intended to be a general interface.  It is specifically
*	for tinySPI and the FORCED mode of the BMP280 operation used by the data logger.
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
#ifndef BMP280SPI_h
#define BMP280SPI_h

#include "bmp280_defs.h"

class BMP280SPI
{
public:
							BMP280SPI(
								uint8_t					inCSPin);
	int8_t					begin(void);
	void					DoForcedRead(
								int32_t&				outTemp,
								uint32_t&				outPres);
protected:
	uint8_t				mCSPin;
	struct bmp280_calib_param	mCParams;

	inline void				BeginTransaction(void)
							{
								SPI.setDataMode(SPI_MODE0);
								digitalWrite(mCSPin, LOW);
							}

	inline void				EndTransaction(void)
							{
								digitalWrite(mCSPin, HIGH);
							}
	uint8_t					ReadReg8(
								uint8_t					inRegAddr);
	void					WriteReg8(
								uint8_t					inRegAddr,
								uint8_t					inRegData);
	void					ReadReg8(
								uint8_t					inRegAddr,
								uint8_t					inDataLength,
								uint8_t*				outRegData);
	void					ReadUncompData(
								int32_t&				outUncompPres,
								int32_t&				outUncompTemp);
	int32_t					ReadUncompData(void);
	int32_t					UncompToCompTemp32(
								int32_t					inUncompTemp);
	uint32_t				UncompToCompPres32(
								int32_t					inUncompPres);
	//void					DumpCParams(void);
};
#endif // BMP280SPI_h
