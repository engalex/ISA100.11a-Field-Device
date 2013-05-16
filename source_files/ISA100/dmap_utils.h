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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file
/// @verbatim
/// Author:       Nivis LLC, Mihaela Goloman
/// Date:         November 2008
/// Description:  This file holds definitions of the dmap_utils module
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DMAP_UTILS_H_
#define _NIVIS_DMAP_UTILS_H_

#include "../typedef.h"
#include "config.h"

#define RO_ATTR_ACCESS   0x01  // read only attribute access
#define WO_ATTR_ACCESS   0x02  // write only attribute access
#define RW_ATTR_ACCESS   0x03  // read/write attribute access

#define ATTR_CONST(x) (void*)&x, sizeof(x)

typedef void (*DMAP_READ_FCT)(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize);
typedef void (*DMAP_WRITE_FCT)(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);

typedef struct
{
  void *          m_pValue;
  uint8           m_ucSize;
  DMAP_READ_FCT   m_pReadFct;
  DMAP_WRITE_FCT  m_pWriteFct;
  
} DMAP_FCT_STRUCT; 

void DMAP_ReadUint8(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadUint16(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadUint32(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadVisibleString(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadOctetString(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadAlertDescriptor(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadCompressAlertDescriptor(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_ReadContractMeta(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
void DMAP_EmptyReadFunc(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);

void DMAP_WriteUint8(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
void DMAP_WriteUint16(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
void DMAP_WriteUint32(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
void DMAP_WriteVisibleString(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
#define DMAP_WriteOctetString DMAP_WriteVisibleString

void DMAP_WriteAlertDescriptor(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
void DMAP_WriteCompressAlertDescriptor(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);

uint8 DMAP_GetVisibleStringLength( const uint8 * p_pValue, uint8 p_ucSize);


uint8 * DMAP_InsertUint32( uint8 * p_pData, uint32  p_ulValue);
uint8 * DMAP_InsertUint16( uint8 * p_pData, uint16  p_unValue);
const uint8 * DMAP_ExtractUint32( const uint8 * p_pData, uint32 * p_pulValue);
const uint8 * DMAP_ExtractUint16( const uint8 * p_pData, uint16 * p_punValue);

uint8 DMAP_ReadAttr(uint16 p_unAttrID, uint16* p_punBufferSize, uint8* p_pucRspBuffer, const DMAP_FCT_STRUCT * p_pDmapFct, uint8 p_ucAttrNo );
uint8 DMAP_WriteAttr(uint16 p_unAttrID, uint8 p_ucBufferSize, const uint8* p_pucBuffer, const DMAP_FCT_STRUCT * p_pDmapFct, uint8 p_ucAttrNo );
uint16 DMAP_GetAlignedLength(uint16 p_unLen);

#endif //_NIVIS_DMAP_UTILS_H_
