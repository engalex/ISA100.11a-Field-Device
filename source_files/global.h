/*
* Copyright (C) 2013 Nivis LLC.
* Email:   opensource@nivis.com
* Website: http://www.nivis.com
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, version 3 of the License.
* 
* Redistribution and use in source and binary forms must retain this
* copyright notice.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

/***************************************************************************************************
* Name:         global.h
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This header file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#ifndef _NIVIS_GLOBAL_H_
#define _NIVIS_GLOBAL_H_

#include "typedef.h"

void initZeroFunction(void);

extern const uint16 g_aCCITTCrcTable[256];
#define COMPUTE_CRC(nAcumulator,chBuff) ((nAcumulator) << 8) ^ g_aCCITTCrcTable[((nAcumulator) >> 8) ^ (chBuff) ]

uint16 ComputeCCITTCRC(const unsigned char *p_pchBuf, unsigned char p_chLen);

uint32 IcmpInterimChksum(const uint8 *p_pData, uint16 p_unDataLen, uint32 p_ulPrevCheckSum );
uint16 IcmpGetFinalCksum( uint32 p_ulPrevCheckSum );

void DelayLoop(uint32 p_ulCount);

#endif /* _NIVIS_GLOBAL_H_ */

