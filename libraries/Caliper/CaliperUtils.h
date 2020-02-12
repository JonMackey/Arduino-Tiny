/*
*	CaliperUtils.h, Copyright Jonathan Mackey 2018
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
#ifndef CaliperUtils_H
#define CaliperUtils_H

class CaliperUtils
{
public:
	static float			RawToValue(
								uint32_t				inRawValue,
								uint8_t*				outDecimalPlaces = NULL);
	static bool				IsMetric(
								uint32_t				inRawValue)
								{return((inRawValue & 0x800000) == 0);}
	static bool				ValueIsValid(
								uint32_t				inRawValue)
								{return(inRawValue != 0xFFFFFFFF);}
};

#endif