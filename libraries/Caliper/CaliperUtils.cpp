/*
*	CaliperUtils.cpp, Copyright Jonathan Mackey 2018
*	Static utility functions used by the Caliper class and CaliperI2CTiny84A
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
#include "Arduino.h"
#include "CaliperUtils.h"

/******************************** RawToValue **********************************/
float CaliperUtils::RawToValue(
	uint32_t	inRawValue,
	uint8_t*	outDecimalPlaces)
{
	float	value;
	uint8_t	dPlaces = 2;
	if (ValueIsValid(inRawValue))
	{
		if (IsMetric(inRawValue))
		{
			/*
			*	For metric the value is 100/mm
			*/
			value = (inRawValue & 0xFFFFF);
			value /= 100;
		} else
		{
			/*
			*	For inches:
			*	- the value is 1000/inch
			*	- bit 0 is for 0.0005, so strip if off
			*/
			value = ((inRawValue & 0xFFFFF) >> 1);
			value /= 1000;
			/*
			*	When measuring in inches, bit 0 determines whether to add 0.0005
			*	to the final value.
			*
			*	If bit 0 THEN
			*	add 0.0005
			*/
			if (inRawValue & 1)
			{
				dPlaces = 4;
				value += 0.0005;
			} else
			{
				dPlaces = 3;
			}
		}
		/*
		*	Bit 20 determines if the value is negative.
		*/
		if (inRawValue & 0x100000)
		{
			value = -value;
		}
	} else
	{
		value = 0;
		dPlaces = 0;
	}
	if (outDecimalPlaces)
	{
		*outDecimalPlaces = dPlaces;
	}
	return(value);
}

