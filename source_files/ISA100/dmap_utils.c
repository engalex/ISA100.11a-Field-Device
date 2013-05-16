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
/// Description:  This file implements the dmap functions for setting/getting data to/from buffers
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "dmap_dmo.h"
#include "dmap_utils.h"
#include "aslsrvc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Empty read function to protect write only attributes
/// @param  p_pValue  
/// @param  p_pBuf
/// @param  p_ucSize
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_EmptyReadFunc(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
  // empty function
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes uint8 value
/// @param  p_pValue 
/// @param  p_pBuf 
/// @param  p_ucSize 
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadUint8(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
  *p_pBuf = *(uint8*)(p_pValue);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Debinarizes uint8 value
/// @param  p_pValue 
/// @param  p_pBuf 
/// @param  p_ucSize 
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_WriteUint8(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  *(uint8*)(p_pValue) = *p_pBuf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes uint16 value
/// @param  p_pValue 
/// @param  p_pBuf 
/// @param  p_ucSize 
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadUint16(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
  *(p_pBuf++) = (*(uint16*)(p_pValue)) >> 8;
  *(p_pBuf++) = (*(uint16*)(p_pValue));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Debinarizes uint16 value
/// @param  p_pValue 
/// @param  p_pBuf 
/// @param  p_ucSize 
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_WriteUint16(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  *(uint16*)(p_pValue) = (uint16)(*(p_pBuf++)) << 8;
  *(uint16*)(p_pValue) |= (uint16)(*(p_pBuf++));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes uint32 value
/// @param  p_pValue 
/// @param  p_pBuf 
/// @param  p_ucSize 
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadUint32(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
  *(p_pBuf++) = (*(uint32*)(p_pValue)) >> 24;
  *(p_pBuf++) = (*(uint32*)(p_pValue)) >> 16;
  *(p_pBuf++) = (*(uint32*)(p_pValue)) >> 8;
  *(p_pBuf++) = (*(uint32*)(p_pValue)); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Debinarizes uint32 value
/// @param  p_pValue 
/// @param  p_pBuf 
/// @param  p_ucSize 
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_WriteUint32(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  *(uint32*)(p_pValue) = ((uint32)*(p_pBuf++)) << 24;
  *(uint32*)(p_pValue) |= ((uint32)*(p_pBuf++)) << 16;
  *(uint32*)(p_pValue) |= ((uint32)*(p_pBuf++)) << 8;
  *(uint32*)(p_pValue) |= ((uint32)*(p_pBuf++));
}

uint8 DMAP_GetVisibleStringLength( const uint8 * p_pValue, uint8 p_ucSize)
{
  uint8 i = 0;
  for( ; i<p_ucSize && p_pValue[i]; i++ )
    ;
    
  return i;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes visible string
/// @param  p_pValue - (in) pointer to the visibile string
/// @param  p_pBuf -(out) buffer to be filled with binarized data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadVisibleString(const void * p_pValue, uint8 * p_pBuf, uint8* p_pSize)
{
  *p_pSize = DMAP_GetVisibleStringLength(p_pValue,*p_pSize);
  memcpy(p_pBuf, p_pValue, *p_pSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Debinarizes visible string
/// @param  p_pValue - (out) pointer to the visible string
/// @param  p_pBuf - (in) buffer containing data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_WriteVisibleString(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  memcpy(p_pValue, p_pBuf, p_ucSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes octet string
/// @param  p_pValue - (in) pointer to the visibile string
/// @param  p_pBuf -(out) buffer to be filled with binarized data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadOctetString(const void * p_pValue, uint8 * p_pBuf, uint8* p_pSize)
{
  memcpy(p_pBuf, p_pValue, *p_pSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes alert descriptor data
/// @param  p_pValue - (in) pointer to alert descriptor data
/// @param  p_pBuf - (out) buffer to be filled with binarized data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadAlertDescriptor(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{  
  *(p_pBuf++) = ((APP_ALERT_DESCRIPTOR*)p_pValue)->m_bAlertReportDisabled;
  *(p_pBuf++) = ((APP_ALERT_DESCRIPTOR*)p_pValue)->m_ucPriority;
  *p_ucSize = 2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Debinarizes alert descriptor data
/// @param  p_pValue - (out) pointer to alert descriptor data
/// @param  p_pBuf - (in) buffer containing alert descriptor data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_WriteAlertDescriptor(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  ((APP_ALERT_DESCRIPTOR*)p_pValue)->m_bAlertReportDisabled = *(p_pBuf++);
  ((APP_ALERT_DESCRIPTOR*)p_pValue)->m_ucPriority           = *(p_pBuf++);  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes compressed alert descriptor data
/// @param  p_pValue - (in) pointer to alert descriptor data
/// @param  p_pBuf - (out) buffer to be filled with binarized data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadCompressAlertDescriptor(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{  
  *(p_pBuf++) = !(*(uint8*)p_pValue & 0x80);
  *(p_pBuf++) = *(uint8*)p_pValue & 0x0F;
  *p_ucSize = 2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Debinarizes compressed alert descriptor data
/// @param  p_pValue - (out) pointer to alert descriptor data
/// @param  p_pBuf - (in) buffer containing alert descriptor data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_WriteCompressAlertDescriptor(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
   *(uint8*)p_pValue =  (p_pBuf[1] & 0x0F);
   if( !p_pBuf[0] ) // report not disabled -> is enabled
   {
      *(uint8*)p_pValue |= 0x80;
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Binarizes contract meta data
/// @param  p_pValue - (in) pointer to contract meta data
/// @param  p_pBuf - (out) buffer to be filled with binarized data
/// @param  p_ucSize - data size
/// @return none
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ReadContractMeta(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
    // insert current entries number
  *(p_pBuf++) = g_stDMO.m_ucContractNo >> 8;
  *(p_pBuf++) = g_stDMO.m_ucContractNo;
  
  // insert maximum entries that contract table can hold
  *(p_pBuf++) = (uint16)(MAX_CONTRACT_NO) >> 8;  // high byte
  *(p_pBuf++) = (uint16)(MAX_CONTRACT_NO);       // low byte 
  
  *p_ucSize = 4;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Inserts (binarizes) an uint32 into a specified buffer by p_pData
/// @param  p_pData     - (output) a pointer to the data buffer
/// @param  p_ulValue   - the 32 bit uint data to be binarized
/// @return pointer at end of inserted data
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * DMAP_InsertUint32( uint8 * p_pData, uint32  p_ulValue)
{
  *(p_pData++) = p_ulValue >> 24;
  *(p_pData++) = p_ulValue >> 16;
  *(p_pData++) = p_ulValue >> 8;
  *(p_pData++) = p_ulValue;
  
  return p_pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parses an uint32 data from stream
/// @param  p_pData     - pointer to the data to be parsed
/// @param  p_pValue    - (output)a pointer to an uint32 , where to put the result  
/// @return pointer to the next data to be parsed
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8 * DMAP_ExtractUint32( const uint8 * p_pData, uint32 * p_pulValue)
{
  *p_pulValue = ((uint32)*(p_pData++)) << 24;
  *p_pulValue |= ((uint32)*(p_pData++)) << 16;
  *p_pulValue |= ((uint32)*(p_pData++)) << 8;
  *p_pulValue |= *(p_pData++);
  
  return p_pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parses an uint16 data from stream
/// @param  p_pData     - pointer to the data to be parsed
/// @param  p_pValue    - (output)a pointer to an uint16 , where to put the result  
/// @return pointer to the next data to be parsed
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8 * DMAP_ExtractUint16( const uint8 * p_pData, uint16 * p_punValue)
{
  *p_punValue = ((uint16)*(p_pData++)) << 8;
  *p_punValue |= *(p_pData++);  
  return p_pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Inserts (binarizes) an uint16 into a specified buffer by p_pData
/// @param  p_pData     - (output) a pointer to the data buffer
/// @param  p_ulValue   - the 16 bit uint data to be binarized
/// @return pointer at end of inserted data
/// @remarks  
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * DMAP_InsertUint16( uint8 * p_pData, uint16  p_unValue)
{
  *(p_pData++) = p_unValue >> 8;
  *(p_pData++) = p_unValue;
  
  return p_pData;
}

uint8 DMAP_ReadAttr(uint16 p_unAttrID, uint16* p_punBufferSize, uint8* p_pucRspBuffer, const DMAP_FCT_STRUCT * p_pDmapFct, uint8 p_ucAttrNo )
{
    //*p_punBufferSize = 0;
    
    if( !p_unAttrID || p_unAttrID >= p_ucAttrNo ) // check if valid attribute id
    {
        return SFC_INVALID_ATTRIBUTE;
    }
    
    p_pDmapFct += p_unAttrID;
        
    uint8 ucSize = p_pDmapFct->m_ucSize; // check if size of buffer is large enough
    
    if( ucSize > *p_punBufferSize )
    {
        return SFC_INVALID_SIZE;
    }    
    
    //call specific read function
    p_pDmapFct->m_pReadFct( p_pDmapFct->m_pValue, p_pucRspBuffer, &ucSize );
    *p_punBufferSize = ucSize;
    
    return SFC_SUCCESS;
    
}

uint8 DMAP_WriteAttr(uint16 p_unAttrID, uint8 p_ucBufferSize, const uint8* p_pucBuffer, const DMAP_FCT_STRUCT * p_pDmapFct, uint8 p_ucAttrNo )
{        
    if( !p_unAttrID || p_unAttrID >= p_ucAttrNo ) //check if valid attribute id
    {
        return SFC_INVALID_ATTRIBUTE;
    }
    
    p_pDmapFct += p_unAttrID;
    
    if( !p_pDmapFct->m_pWriteFct )
        return SFC_READ_ONLY_ATTRIBUTE;
   
    if( p_pDmapFct->m_pWriteFct == DMAP_WriteVisibleString )
    {
        memset( p_pDmapFct->m_pValue, 0, p_pDmapFct->m_ucSize );
        if( p_ucBufferSize > p_pDmapFct->m_ucSize )
        {
          p_ucBufferSize = p_pDmapFct->m_ucSize; // truncate the size
        }
    }
    
    p_pDmapFct->m_pWriteFct(p_pDmapFct->m_pValue, p_pucBuffer, p_ucBufferSize);
    return SFC_SUCCESS;
}

uint16 DMAP_GetAlignedLength(uint16 p_unLen)
{
  uint16 unRemainder = p_unLen & (CPU_WORD_SIZE-1);
  if (unRemainder)
  {
      p_unLen += CPU_WORD_SIZE - unRemainder; // ensure word alignment
  }  
  
  return p_unLen;
}

