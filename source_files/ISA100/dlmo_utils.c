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
/// Date:         February 2008
/// Description:  This file implements the dlmo functions for setting/getting data to/from buffers
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "dlmo_utils.h"
#include "dmap_utils.h"
#include "mlde.h"
#include "sfc.h"

static int8 DLMO_computeChDiagPercent(uint16 p_unReference, uint16 p_unActualCtr);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes an uint8 DLMO attribute 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadUInt8(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8 ucValue = 0;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(p_unAttrID, &ucValue))
    return SFC_INCOMPATIBLE_ATTRIBUTE;
  
  *p_punSize = 1;
  p_pBuf[0] = ucValue;
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes an uint16 DLMO attribute 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadUInt16(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint16 unValue = 0;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(p_unAttrID, &unValue))
    return SFC_INCOMPATIBLE_ATTRIBUTE;
  
  *p_punSize = 2;
  p_pBuf[0] = unValue >> 8;
  p_pBuf[1] = unValue;
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes an uint32 DLMO attribute 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadUInt32(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint32 ulValue = 0;
  
  if ( SFC_SUCCESS != DLME_GetMIBRequest(p_unAttrID, &ulValue) )
      return SFC_INCOMPATIBLE_ATTRIBUTE;
  
  *p_punSize = 4;
  p_pBuf[0] = ulValue >> 24;
  p_pBuf[1] = ulValue >> 16;
  p_pBuf[2] = ulValue >> 8;
  p_pBuf[3] = ulValue;  
  
  return SFC_SUCCESS;
}

//-----------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Extracts an int8 DLMO attribute from buffer 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteInt8(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{  
  // at least one byte is needed to read the data
  if( !(*p_punSize) )
      return SFC_INVALID_SIZE;  

  *p_punSize = 1;
  
  //check ranges
//  if((DL_RADIO_TRANSMIT_POWER == p_unAttrID) && 
//     ((cValue < DLL_RADIO_TRANSMIT_POWER_LOW) ||
//      (cValue > DLL_RADIO_TRANSMIT_POWER_HIGH)))
//    return SFC_VALUE_OUT_OF_RANGE;

  //ranges ok, actually set value in DL
  return DLME_SetMIBRequest(p_unAttrID, p_pBuf);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Extracts an uint8 DLMO attribute from buffer 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteUInt8(uint16 p_unAttrID, uint16 * p_punSize, const uint8* p_pBuf)
{
  uint8 ucValue = *p_pBuf;
  
  // at least one byte is needed to read data
  if( !(*p_punSize) )
    return SFC_INVALID_SIZE;  
  
  *p_punSize = 1;
  
  //check ranges
  if((DL_MAX_BACKOFF_EXP == p_unAttrID) && 
     ((ucValue < DLL_MAX_BACKOFF_EXP_LOW) || 
      (ucValue > DLL_MAX_BACKOFF_EXP_HIGH)))
    return SFC_VALUE_OUT_OF_RANGE;

  if((DL_MAX_DSDU_SIZE == p_unAttrID) && 
     ((ucValue < DLL_MAX_DSDU_SIZE_LOW) ||
      (ucValue > DLL_MAX_DSDU_SIZE_HIGH)))
    return SFC_VALUE_OUT_OF_RANGE;

  if((DL_LINK_PRIORITY_XMIT == p_unAttrID) && 
     (ucValue > DLL_LINK_PRIORITY_XMIT_HIGH))
    return SFC_VALUE_OUT_OF_RANGE;

  if((DL_LINK_PRIORITY_RCV == p_unAttrID) && 
     (ucValue > DLL_LINK_PRIORITY_RCV_HIGH))
    return SFC_VALUE_OUT_OF_RANGE;  

  //ranges ok, actually set value in DL
  return DLME_SetMIBRequest(p_unAttrID, &ucValue);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Extracts an uint16 DLMO attribute from buffer 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteUInt16(uint16 p_unAttrID, uint16 * p_punSize, const uint8* p_pBuf)
{   
  // at least two bytea are needed to read data
  if( *p_punSize < 2 )
      return SFC_INVALID_SIZE;  
 
  uint16 unValue = ((uint16)p_pBuf[0] << 8) | p_pBuf[1];
  
  *p_punSize = 2;
  
  //check ranges
  if( (DL_MAX_LIFETIME == p_unAttrID) && 
      ((unValue < DLL_MAX_LIFETIME_LOW) || (unValue > DLL_MAX_LIFETIME_HIGH)) )
    return SFC_VALUE_OUT_OF_RANGE;
  
  if( (DL_NACK_BACKOFF_DUR == p_unAttrID) && 
     ((unValue < DLL_NACK_BACKOFF_DUR_LOW) || (unValue > DLL_NACK_BACKOFF_DUR_HIGH)) )
    return SFC_VALUE_OUT_OF_RANGE;
  
  if( (DL_CLOCK_STALE == p_unAttrID) && 
      ((unValue < DLL_CLOCK_STALE_LOW) || (unValue > DLL_CLOCK_STALE_HIGH)) )
    return SFC_VALUE_OUT_OF_RANGE;
  
  if( (DL_COUNTRY_CODE == p_unAttrID) && (unValue > DLL_COUNTRY_CODE_HIGH) )
    return SFC_VALUE_OUT_OF_RANGE;  
  
  //dll access now  
  return DLME_SetMIBRequest(p_unAttrID, &unValue);  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Extracts an uint32 DLMO attribute from buffer 
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteUInt32(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{
  // at least two bytes are needed to read data
  if( *p_punSize < 4 )
      return SFC_INVALID_SIZE;  
  
  *p_punSize = 4;
  
  uint32 lValue = ((uint32)p_pBuf[0] << 24) |
                  ((uint32)p_pBuf[1] << 16) |
                  ((uint32)p_pBuf[2] << 8) |
                   p_pBuf[3];
  
  return DLME_SetMIBRequest(p_unAttrID, &lValue);  
}

//-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Debinarizes a filter DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteSubnetFilter(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{
  DLL_MIB_SUBNET_FILTER stSubnetFilter;  
  const uint8* pStart = p_pBuf;

  //lsb!
  stSubnetFilter.m_unBitMask = (uint16)*(p_pBuf++);
  stSubnetFilter.m_unBitMask |= (uint16)*(p_pBuf++)  << 8;
  
  //lsb!
  stSubnetFilter.m_unTargetID = (uint16)*(p_pBuf++);
  stSubnetFilter.m_unTargetID |= (uint16)*(p_pBuf++) << 8;

  if(*p_punSize < (p_pBuf - pStart))
    return SFC_INVALID_SIZE;
  
  *p_punSize = p_pBuf-pStart;
  
  if( DL_ADV_FILTER == p_unAttrID )
  {
    g_stFilterBitMask = stSubnetFilter.m_unBitMask;
    g_stFilterTargetID = stSubnetFilter.m_unTargetID;
  }
  
  return DLME_SetMIBRequest(p_unAttrID, &stSubnetFilter);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a filter DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadSubnetFilter(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  if( DL_ADV_FILTER == p_unAttrID )
  {
    //lsb!
    *(p_pBuf++) = g_stFilterBitMask;
    *(p_pBuf++) = g_stFilterBitMask >> 8;

    //lsb!
    *(p_pBuf++) = g_stFilterTargetID;
    *(p_pBuf++) = g_stFilterTargetID >> 8;
  }
  else
  {
    if( DL_SOLIC_FILTER == p_unAttrID )
    {
        //lsb!
        *(p_pBuf++) = g_stSolicFilter.m_unBitMask;
        *(p_pBuf++) = g_stSolicFilter.m_unBitMask >> 8;

        //lsb!
        *(p_pBuf++) = g_stSolicFilter.m_unTargetID;
        *(p_pBuf++) = g_stSolicFilter.m_unTargetID >> 8;
    }
    else
        return SFC_INCOMPATIBLE_ATTRIBUTE;  
  }
  
  *p_punSize = 4;
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a TAI DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadTAI(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8 aTAI[6];
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_TAI_TIME, aTAI))
      return SFC_INCOMPATIBLE_ATTRIBUTE;
  
  *p_punSize = sizeof(aTAI);
  p_pBuf[0] = aTAI[3];
  p_pBuf[1] = aTAI[2];
  p_pBuf[2] = aTAI[1];
  p_pBuf[3] = aTAI[0];
  p_pBuf[4] = aTAI[5];
  p_pBuf[5] = aTAI[4];
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Debinarizes a TAIAdjust DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteTAIAdjust(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{    
  DLL_MIB_TAI_ADJUST stTAIAdjust;    
  const uint8* pStart = p_pBuf;

  p_pBuf = DMAP_ExtractUint32( p_pBuf, (uint32*)&stTAIAdjust.m_lTaiCorrection );  
  p_pBuf = DMAP_ExtractUint32( p_pBuf, &stTAIAdjust.m_ulTaiTimeToApply );
  
  if(*p_punSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  *p_punSize = p_pBuf-pStart;
  
  return DLME_SetMIBRequest(DL_TAI_ADJUST, &stTAIAdjust);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a TAIAdjust DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadTAIAdjust(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;
  
  DLL_MIB_TAI_ADJUST stTAIAdjust;
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_TAI_ADJUST, &stTAIAdjust))
      return SFC_INCOMPATIBLE_ATTRIBUTE;    
  
  p_pBuf = DMAP_InsertUint32(p_pBuf, stTAIAdjust.m_lTaiCorrection );  
  p_pBuf = DMAP_InsertUint32(p_pBuf, stTAIAdjust.m_ulTaiTimeToApply );
  
  *p_punSize = p_pBuf - pStart;
  return SFC_SUCCESS;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a Candidates DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadCandidates(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;  
  DLL_MIB_CANDIDATES stCandidates;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_CANDIDATES, &stCandidates))
    return SFC_INCOMPATIBLE_ATTRIBUTE; 
  
  *(p_pBuf++) = stCandidates.m_ucCandidateNo;
  for(uint8 ucIdx=0; ucIdx<stCandidates.m_ucCandidateNo; ucIdx++)
  {
    p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stCandidates.m_unCandidateAddr[ucIdx]);
    *(p_pBuf++) = stCandidates.m_cRSQI[ucIdx]; 
  }  
  
  *p_punSize = p_pBuf - pStart;
  return SFC_SUCCESS;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Debinarizes a SmoothFactors DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteSmoothFactors(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{ 
  DLL_MIB_SMOOTH_FACTORS stSmoothFactors;
  
#ifdef UT_ZERO_FILL  // unit testing support
  memset(&stSmoothFactors, 0, sizeof(DLL_MIB_SMOOTH_FACTORS));
#endif // UT_ZERO_FILL   
  
  const uint8* pStart = p_pBuf;

  p_pBuf = DMAP_ExtractUint16(p_pBuf, (uint16*)&stSmoothFactors.m_nRSSIThresh );  
  stSmoothFactors.m_ucRSSIAlphaLow  = *(p_pBuf++);
  stSmoothFactors.m_ucRSSIAlphaHigh = *(p_pBuf++);
  
  p_pBuf = DMAP_ExtractUint16(p_pBuf, (uint16*)&stSmoothFactors.m_nRSQIThresh );
  stSmoothFactors.m_ucRSQIAlphaLow  = *(p_pBuf++);
  stSmoothFactors.m_ucRSQIAlphaHigh = *(p_pBuf++);
  
  p_pBuf = DMAP_ExtractUint16(p_pBuf, (uint16*)&stSmoothFactors.m_nClkBiasThresh );
  stSmoothFactors.m_ucClkBiasAlphaLow  = *(p_pBuf++);
  stSmoothFactors.m_ucClkBiasAlphaHigh = *(p_pBuf++);

  if (*p_punSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  *p_punSize = p_pBuf - pStart;
  
  return DLME_SetMIBRequest(DL_SMOOTH_FACTORS, &stSmoothFactors);
}
                              
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a SmoothFactors DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadSmoothFactors(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;
  
  DLL_MIB_SMOOTH_FACTORS stSmoothFactors;
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_SMOOTH_FACTORS, &stSmoothFactors))
    return SFC_INCOMPATIBLE_ATTRIBUTE; 
  
  p_pBuf = DMAP_InsertUint16(p_pBuf, stSmoothFactors.m_nRSSIThresh );  
  *(p_pBuf++) = stSmoothFactors.m_ucRSSIAlphaLow;
  *(p_pBuf++) = stSmoothFactors.m_ucRSSIAlphaHigh;
  
  p_pBuf = DMAP_InsertUint16(p_pBuf, stSmoothFactors.m_nRSQIThresh );  
  *(p_pBuf++) = stSmoothFactors.m_ucRSQIAlphaLow;
  *(p_pBuf++) = stSmoothFactors.m_ucRSQIAlphaHigh;
  
  p_pBuf = DMAP_InsertUint16(p_pBuf, stSmoothFactors.m_nClkBiasThresh );  
  *(p_pBuf++) = stSmoothFactors.m_ucClkBiasAlphaLow;
  *(p_pBuf++) = stSmoothFactors.m_ucClkBiasAlphaHigh;
  
  *p_punSize = p_pBuf - pStart;
  return SFC_SUCCESS;    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Debinarizes a QueuePriority DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteQueuePriority(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{
  DLL_MIB_QUEUE_PRIORITY stQueuePriority; 
#ifdef UT_ZERO_FILL  // unit testing support
  memset(&stQueuePriority, 0, sizeof(DLL_MIB_QUEUE_PRIORITY));
#endif // UT_ZERO_FILL     
  
  const uint8* pStart = p_pBuf;

  stQueuePriority.m_ucPriorityNo = *(p_pBuf++);
  
  if (DLL_MAX_PRIORITY_NO < stQueuePriority.m_ucPriorityNo)
      return SFC_INVALID_VALUE;
      
  for(uint8 ucIdx=0; ucIdx<stQueuePriority.m_ucPriorityNo; ucIdx++)
  {
      stQueuePriority.m_aucPriority[ucIdx] = *(p_pBuf++);
      stQueuePriority.m_aucQMax[ucIdx] = *(p_pBuf++);
  }

  if (*p_punSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  *p_punSize = p_pBuf - pStart;
  
  return DLME_SetMIBRequest(DL_QUEUE_PRIOTITY, &stQueuePriority);  
}

uint8 DLMO_WriteProtection(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{
    return SFC_READ_ONLY_ATTRIBUTE; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a QueuePriority DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadQueuePriority(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;  
  DLL_MIB_QUEUE_PRIORITY stQueuePriority;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_QUEUE_PRIOTITY, &stQueuePriority))
      return SFC_INCOMPATIBLE_ATTRIBUTE; 
  
  *(p_pBuf++) = stQueuePriority.m_ucPriorityNo;
  
  for(uint8 ucIdx=0; ucIdx<stQueuePriority.m_ucPriorityNo; ucIdx++)
  {
    *(p_pBuf++) = stQueuePriority.m_aucPriority[ucIdx];
    *(p_pBuf++) = stQueuePriority.m_aucQMax[ucIdx];
  }

  *p_punSize = p_pBuf - pStart;
  return SFC_SUCCESS;      
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Binarizes a DeviceCapability DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadDeviceCapability(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;  
  
  int8   cRadioTxPower;
  uint16 unEnergyLeft;  
  DLL_MIB_DEV_CAPABILITIES stDevCap;
  DLL_MIB_ENERGY_DESIGN    stEnergyDesign;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_DEV_CAPABILITY, &stDevCap))
      return SFC_INCOMPATIBLE_ATTRIBUTE;  
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_ENERGY_DESIGN, &stEnergyDesign))
      return SFC_INCOMPATIBLE_ATTRIBUTE;  
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_ENERGY_LEFT, &unEnergyLeft))
      return SFC_INCOMPATIBLE_ATTRIBUTE; 
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_RADIO_TRANSMIT_POWER, &cRadioTxPower))
      return SFC_INCOMPATIBLE_ATTRIBUTE; 

  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stDevCap.m_unQueueCapacity);
  
  *(p_pBuf++) = stDevCap.m_ucClockAccuracy;  
  p_pBuf = DMAP_InsertUint16( p_pBuf, stDevCap.m_unChannelMap);    
  *(p_pBuf++) = stDevCap.m_ucDLRoles;
  
  p_pBuf = DMAP_InsertUint16( p_pBuf, stEnergyDesign.m_nEnergyLife);
  
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stEnergyDesign.m_unListenRate);
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stEnergyDesign.m_unTransmitRate);
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stEnergyDesign.m_unAdvRate);  
  
  p_pBuf = DMAP_InsertUint16( p_pBuf, unEnergyLeft );    

  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stDevCap.m_unAckTurnaround);
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stDevCap.m_unNeighDiagCapacity);
  
  *(p_pBuf++) = cRadioTxPower;
  *(p_pBuf++) = stDevCap.m_ucOptions;

  *p_punSize = p_pBuf - pStart;
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes an AlertPolicy DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadAlertPolicy(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;  
  DLL_MIB_ALERT_POLICY stAlertPolicy;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_ALERT_POLICY, &stAlertPolicy))
      return SFC_INCOMPATIBLE_ATTRIBUTE;  
  
  *(p_pBuf++) = stAlertPolicy.m_ucEnabled;  
  
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stAlertPolicy.m_unNeiMinUnicast);
  
  *(p_pBuf++) = stAlertPolicy.m_ucNeiErrThresh;
  
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stAlertPolicy.m_unChanMinUnicast);
  
  *(p_pBuf++) = stAlertPolicy.m_unNoAckThresh;
  *(p_pBuf++) = stAlertPolicy.m_ucCCABackoffThresh;

  *p_punSize = p_pBuf - pStart;
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parser for an AlertPolicy DLMO attribute stream
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - input buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteAlertPolicy(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{
  DLL_MIB_ALERT_POLICY stAlertPolicy;  

#ifdef UT_ZERO_FILL  // unit testing support
  memset(&stAlertPolicy, 0, sizeof(stAlertPolicy));
#endif // UT_ZERO_FILL  
  
  const uint8* pStart = p_pBuf;
  
  stAlertPolicy.m_ucEnabled = *(p_pBuf++);
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &stAlertPolicy.m_unNeiMinUnicast);
  
  stAlertPolicy.m_ucNeiErrThresh = *(p_pBuf++);
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &stAlertPolicy.m_unChanMinUnicast);
  
  stAlertPolicy.m_unNoAckThresh = *(p_pBuf++);
  stAlertPolicy.m_ucCCABackoffThresh = *(p_pBuf++);
  
  if (*p_punSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  *p_punSize = p_pBuf - pStart;
  
  return DLME_SetMIBRequest(DL_ALERT_POLICY, &stAlertPolicy);  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes an EnergyDesign DLMO attribute structure
/// @param  p_unAttrID - attribute identifier
/// @param  p_punSize - buffer size
/// @param  p_pBuf - output buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadEnergyDesign(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;    
  DLL_MIB_ENERGY_DESIGN stEnergyDesign;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_ENERGY_DESIGN, &stEnergyDesign))
      return SFC_INCOMPATIBLE_ATTRIBUTE;  
  
  p_pBuf = DMAP_InsertUint16( p_pBuf, (uint16)stEnergyDesign.m_nEnergyLife);  
  
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stEnergyDesign.m_unListenRate);
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stEnergyDesign.m_unTransmitRate);
  p_pBuf = DLMO_InsertExtDLUint(p_pBuf, stEnergyDesign.m_unAdvRate);  

  *p_punSize = p_pBuf - pStart;
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Reads and Binarizes a ChannelDiagnostic DLMO attribute structure
/// @param   p_unAttrID - attribute identifier
/// @param   p_punSize - buffer size
/// @param   p_pBuf - output buffer
/// @return  service feedback code
/// @remarks -  no overflow validations are made inside this function.
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadChDiagnostics(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{     
  DLL_MIB_CHANNEL_DIAG stChDiag;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_CH_DIAG, &stChDiag))
      return SFC_INCOMPATIBLE_ATTRIBUTE; 
  
  return DLMO_BinarizeChDiagnostics(&stChDiag, p_punSize, p_pBuf);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes a ChannelDiagnostic DLMO attribute structure
/// @param   p_pstChDiag - pointer to a ChannelDiagnostic DLMO attribute structure
/// @param   p_punSize - buffer size
/// @param   p_pBuf - output buffer
/// @return  service feedback code
/// @remarks -  no overflow validations are made inside this function.
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_BinarizeChDiagnostics(DLL_MIB_CHANNEL_DIAG * p_pstChDiag, uint16  * p_punSize, uint8* p_pBuf)
{
  uint8* pStart = p_pBuf;   
  uint16 unTxCount = 0;
  p_pBuf+=2;  // number of attempted transmissions for all channels will be available later
  
  if (*p_punSize < 34)
  {
       *p_punSize = 0;
       return SFC_INVALID_SIZE;
  }      
  
  for (uint8 ucIdx=0; ucIdx < 16; ucIdx++)
  {
      unTxCount += p_pstChDiag->m_astCh[ucIdx].m_unUnicastTxCtr;
      
      // compute per-channel diagnostics
      *(p_pBuf++) = DLMO_computeChDiagPercent( p_pstChDiag->m_astCh[ucIdx].m_unUnicastTxCtr,
                                               p_pstChDiag->m_astCh[ucIdx].m_unNoAckCtr );
      
      *(p_pBuf++) = DLMO_computeChDiagPercent( p_pstChDiag->m_astCh[ucIdx].m_unUnicastTxCtr,
                                               p_pstChDiag->m_astCh[ucIdx].m_unCCABackoffCtr );    
  }   
  
  // now fill the total tx counter on all channels  
  pStart[0] = unTxCount >> 8;
  pStart[1] = unTxCount;
 
  *p_punSize = p_pBuf - pStart;    
  return SFC_SUCCESS; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes a ChannelDiagnostic DLMO attribute structure
/// @param  p_unReference - total number of attempted transmissions
/// @param  p_unActualCtr - actual count of failures
/// @return signed percentage   
/// @remarks  - A value of 0 indicates that no unicast transmission has been attempted on the channel 
/// @remarks  - Positive values in the range of 1 to 101 indicates the percentage failure rate plus one
/// @remarks  - Negative values in the range of -1 to -101 indicates the percentage failure rate as a
///             negative number minus one. Failure rates are reported as negative numbers if they are
///             based on 5 or fewer attempted transmissions\n
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
int8 DLMO_computeChDiagPercent(uint16 p_unReference, uint16 p_unActualCtr)
{
  int8 cPercentage = 0;
  
  if (!p_unReference)
      return cPercentage; // no unicast transmissions on channel
  
  cPercentage = (((uint32)(p_unActualCtr) * 100) / p_unReference) + 1;
  
  if ( p_unReference <= 5)
      cPercentage = -cPercentage;
  
  return cPercentage;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Extracts an AdvertiseJoinInfo data from a stream
/// @param  p_pucBuf          - pointer to the input buffer
/// @param  p_pstAdvJoinInfo  - pointer to the data structure that needs to be populated
/// @return pointer to the next data to be parsed   
/// @remarks
///      Access level: user level and interrupt level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor const uint8 * DLMO_ExtractAdvJoinInfo( const uint8 * p_pucBuf, 
                                                DLL_MIB_ADV_JOIN_INFO * p_pstAdvJoinInfo)
{
  p_pstAdvJoinInfo->m_mfDllJoinBackTimeout = *(p_pucBuf++);
  p_pstAdvJoinInfo->m_mfDllJoinLinksType = *(p_pucBuf++);

  //Join Request Tx link
  if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_JOIN_TX_SCHEDULE )
  {
      //offset and interval or range
      p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinTx.m_aOffsetInterval.m_unOffset);
      p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinTx.m_aOffsetInterval.m_unInterval);
  }
  else
  {
      //offset only
      p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinTx.m_aOffset.m_unOffset);    
  }
  
  //Join Response Rx link
  if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_JOIN_RX_SCHEDULE )
  {
      //offset and interval or range
      p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinRx.m_aOffsetInterval.m_unOffset);
      p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinRx.m_aOffsetInterval.m_unInterval);
  }
  else
  {
      //offset only
      p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinRx.m_aOffset.m_unOffset);    
  } 
  
  //Join Adv Rx link
  if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_ADV_RX_FLAG )
  {
      if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_ADV_RX_SCHEDULE )
      {
          //offset and interval or range
          p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinAdvRx.m_aOffsetInterval.m_unOffset);
          p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinAdvRx.m_aOffsetInterval.m_unInterval);
      }
      else
      {
          //offset only
          p_pucBuf = DLMO_ExtractExtDLUint(p_pucBuf, &p_pstAdvJoinInfo->m_stDllJoinAdvRx.m_aOffset.m_unOffset);        
      }
  }
  
  return p_pucBuf;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Extracts an AdvertiseJoinInfo data from a stream
/// @param  p_pucBuf          - pointer to the destination buffer
/// @param  p_pstAdvJoinInfo  - pointer to the data structure that needs to be inserted
/// @return pointer to the next available byte from the destination buffer    
/// @remarks
///      Access level: user level and interrupt level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 * DLMO_InsertAdvJoinInfo( uint8 * p_pucBuf, 
                                          DLL_MIB_ADV_JOIN_INFO * p_pstAdvJoinInfo)
{
  *(p_pucBuf++) = p_pstAdvJoinInfo->m_mfDllJoinBackTimeout;
  *(p_pucBuf++) = p_pstAdvJoinInfo->m_mfDllJoinLinksType;

  //Join Request Tx link
  if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_JOIN_TX_SCHEDULE )
  {
      //offset and interval or range
      p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinTx.m_aOffsetInterval.m_unOffset);
      p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinTx.m_aOffsetInterval.m_unInterval);
  }
  else
  {
      //offset only
      p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinTx.m_aOffset.m_unOffset);    
  }
  
  //Join Response Rx link
  if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_JOIN_RX_SCHEDULE )
  {
      //offset and interval or range
      p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinRx.m_aOffsetInterval.m_unOffset);
      p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinRx.m_aOffsetInterval.m_unInterval);
  }
  else
  {
      //offset only
      p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinRx.m_aOffset.m_unOffset);    
  } 
  
  //Join Adv Rx link
  if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_ADV_RX_FLAG )
  {
      if( p_pstAdvJoinInfo->m_mfDllJoinLinksType & DLL_MASK_ADV_RX_SCHEDULE )
      {
          //offset and interval or range
          p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinAdvRx.m_aOffsetInterval.m_unOffset);
          p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinAdvRx.m_aOffsetInterval.m_unInterval);
      }
      else
      {
          //offset only
          p_pucBuf = DLMO_InsertExtDLUint(p_pucBuf, p_pstAdvJoinInfo->m_stDllJoinAdvRx.m_aOffset.m_unOffset);        
      }
  }
  
  return p_pucBuf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Writes an AdvertiseJoinInfo stream
/// @param  p_unSize          - input buffer size
/// @param  p_pucBuf          - pointer to the input buffer
/// @param  p_punStreamSize   - output param: the number of parsed octets
/// @return SFC_SUCCESS or other error feedback codes
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteAdvJoinInfo(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf)
{   
  const uint8* pucStart = p_pBuf;
  DLL_MIB_ADV_JOIN_INFO stAdv; 
  
  p_pBuf = DLMO_ExtractAdvJoinInfo(p_pBuf, &stAdv);
  
  if(*p_punSize < (p_pBuf - pucStart))
      return SFC_INVALID_SIZE;
  
  //perform consistency checks
  if((stAdv.m_mfDllJoinLinksType & DLL_MASK_JOIN_TX_SCHEDULE) == DLL_MASK_JOIN_TX_SCHEDULE)
      return SFC_INVALID_VALUE; //range 0-2
 
  if((stAdv.m_mfDllJoinLinksType & DLL_MASK_JOIN_RX_SCHEDULE) == DLL_MASK_JOIN_RX_SCHEDULE)
      return SFC_INVALID_VALUE; //range 0-2

  if((stAdv.m_mfDllJoinLinksType & DLL_MASK_ADV_RX_SCHEDULE) == DLL_MASK_ADV_RX_SCHEDULE)
      return SFC_INVALID_VALUE; //range 0-2    
  
  //now really set adv join info in dll
  if(SFC_SUCCESS != DLME_SetMIBRequest(DL_ADV_JOIN_INFO, &stAdv))
      return SFC_INCOMPATIBLE_ATTRIBUTE;
  
  *p_punSize = (uint16)(p_pBuf - pucStart);
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Reads an AdvertiseJoinInfo stream
/// @param  p_unSize          - output buffer size
/// @param  p_pucBuf          - pointer to the output buffer
/// @return SFC_SUCCESS or other error feedback codes
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_ReadAdvJoinInfo(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf)
{
  DLL_MIB_ADV_JOIN_INFO stAdvJoinInfo;
  uint8* pucStart = p_pBuf;
  
  if(SFC_SUCCESS != DLME_GetMIBRequest(DL_ADV_JOIN_INFO, &stAdvJoinInfo))
    return SFC_INCOMPATIBLE_ATTRIBUTE;
  
  p_pBuf = DLMO_InsertAdvJoinInfo(p_pBuf, &stAdvJoinInfo);

  *p_punSize = p_pBuf - pucStart;
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief Debinarizes channel entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - channel entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBChannelEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{
  const uint8 *pStart = p_pBuf;
  uint16 unStartSize = *p_pSize;
  
  DLL_SMIB_CHANNEL* pCh = &p_pEntry->m_stSmib.m_stChannel;

#ifdef UT_ZERO_FILL  // unit testing support
  memset(pCh, 0, sizeof(DLL_SMIB_CHANNEL));
#endif // UT_ZERO_FILL 

  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pCh->m_unUID);
  
  pCh->m_ucLength = *(p_pBuf++);
  
  uint8 ucSize = (pCh->m_ucLength + 1) / 2;  
    
  if (ucSize > sizeof(pCh->m_aSeq))
      return SFC_INVALID_VALUE;
  
  memcpy( pCh->m_aSeq, p_pBuf, ucSize );
  
  p_pBuf += ucSize;
  
  *p_pSize = (p_pBuf - pStart);
  
  if(unStartSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes channel entry data
/// @param  p_pEntry - channel entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBChannelEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;
  const DLL_SMIB_CHANNEL* pCh = &p_pEntry->m_stSmib.m_stChannel;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pCh->m_unUID);
  
  *(p_pRspBuf++) = pCh->m_ucLength;
  
  uint8 ucSize = (pCh->m_ucLength + 1) / 2;
  memcpy( p_pRspBuf, pCh->m_aSeq, ucSize );
  
  p_pRspBuf += ucSize;
  
  return (p_pRspBuf - pStart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief Debinarizes timeslot entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - timeslot entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBTimeslotEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{  
  const uint8 *pStart = p_pBuf;
  uint16 unStartSize = *p_pSize;  
  DLL_SMIB_TIMESLOT* pTs = &p_pEntry->m_stSmib.m_stTimeslot;

#ifdef UT_ZERO_FILL  // unit testing support
  memset(pTs, 0, sizeof(DLL_SMIB_TIMESLOT));
#endif // UT_ZERO_FILL   

  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pTs->m_unUID);
  
  pTs->m_ucInfo = *(p_pBuf++);  //template type and options 
  
  if(1 < ((pTs->m_ucInfo & DLL_MASK_TSTYPE) >> DLL_ROT_TSTYPE))
    return SFC_INCONSISTENT_CONTENT;  
  
  //Rx or Tx template
  //the Rx template members have the same offsets as the Tx template  
  p_pBuf = DMAP_ExtractUint16( p_pBuf, &pTs->m_utTemplate.m_stRx.m_unEarliestDPDU );
  p_pBuf = DMAP_ExtractUint16( p_pBuf, &pTs->m_utTemplate.m_stRx.m_unTimeoutDPDU );
  p_pBuf = DMAP_ExtractUint16( p_pBuf, &pTs->m_utTemplate.m_stRx.m_unAckEarliest );
  p_pBuf = DMAP_ExtractUint16( p_pBuf, &pTs->m_utTemplate.m_stRx.m_unAckLatest );    

  *p_pSize = (p_pBuf - pStart);
  if(unStartSize < (p_pBuf - pStart))
    return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes timeslot entry data
/// @param  p_pEntry - timeslot entry data
/// @param  p_pRspBuf - output buffer
/// @return buffer size 
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBTimeslotEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;
  const DLL_SMIB_TIMESLOT* pTs = &p_pEntry->m_stSmib.m_stTimeslot;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pTs->m_unUID);
  
  *(p_pRspBuf++) = pTs->m_ucInfo;

  //the Rx template members have the same offsets as the Tx template
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pTs->m_utTemplate.m_stRx.m_unEarliestDPDU );
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pTs->m_utTemplate.m_stRx.m_unTimeoutDPDU );
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pTs->m_utTemplate.m_stRx.m_unAckEarliest );
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pTs->m_utTemplate.m_stRx.m_unAckLatest );
  
  return (p_pRspBuf - pStart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief Debinarizes neighbor entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - neighbor entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBNeighborEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{
  const uint8 *pStart = p_pBuf;
  uint16 unStartSize = *p_pSize;
  DLL_SMIB_NEIGHBOR* pNei = &p_pEntry->m_stSmib.m_stNeighbor;

  // memset is needed for the internaly used backoff counter members  
  memset(pNei, 0, sizeof(DLL_SMIB_NEIGHBOR));
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pNei->m_unUID);
  
  DLME_CopyReversedEUI64Addr(pNei->m_aEUI64, p_pBuf);
  p_pBuf += 8;
  
  pNei->m_ucGroupCode = *(p_pBuf++);
  pNei->m_ucInfo = *(p_pBuf++);
  uint8 ucNofGr = (pNei->m_ucInfo & DLL_MASK_NEINOFGR) >> DLL_ROT_NEINOFGR;
  
  for(uint8 ucIdx = 0; ucIdx < ucNofGr; ucIdx++)
  {
      p_pBuf = DMAP_ExtractUint16( p_pBuf, pNei->m_aExtendedGraphs+ucIdx );
  }
  
  if(pNei->m_ucInfo & DLL_MASK_NEILINKBACKLOG)
  {
      p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pNei->m_unLinkBacklogIdx);
      pNei->m_ucLinkBacklogDur = *(p_pBuf++);
  }  
  
  pNei->m_ucCommHistory = 0xFF;
 
  *p_pSize = (p_pBuf - pStart);
  
  if (unStartSize < *p_pSize)
     return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief Debinarizes neighbor diagnostic reset data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - neighbor diagnostic reset entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBNeighDiagResetEntry( uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry )
{
  const uint8 * pStart = p_pBuf;  
  uint16        unStartSize = *p_pSize;
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &p_pEntry->m_stSmib.m_stNeighDiagReset.m_unUID);
  
  p_pEntry->m_stSmib.m_stNeighDiagReset.m_ucLevel = *(p_pBuf++) & DLL_MASK_NEIDIAG; // ensure that reserved bits are not harmful 
  
  if (unStartSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes neighbor diagnostic reset entry data
/// @param  p_pEntry - neighbor diagnostic reset entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBNeighDiagResetEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf; 
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiagReset.m_unUID);
  
  *(p_pRspBuf++) = p_pEntry->m_stSmib.m_stNeighDiagReset.m_ucLevel;
                                     
  return (uint8)(p_pRspBuf - pStart); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes neighbor diagnostic entry data 
/// @param  p_pEntry - neighbor diagnostic entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBNeighDiagEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf; 
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_unUID);
  
  // insert summary octet string
  if (p_pEntry->m_stSmib.m_stNeighDiag.m_ucDiagFlags & DLL_MASK_NEIDIAG_LINK)
  {
      *(p_pRspBuf++) = p_pEntry->m_stSmib.m_stNeighDiag.m_cRSSI + ISA100_RSSI_OFFSET;
      *(p_pRspBuf++) = p_pEntry->m_stSmib.m_stNeighDiag.m_ucRSQI;                           
      
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unRxDPDUNo);
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unTxSuccesNo);
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unTxFailedNo);
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unTxCCABackoffNo);
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unTxNackNo);
            
      *(p_pRspBuf++) = (*(uint16*)(&p_pEntry->m_stSmib.m_stNeighDiag.m_nClockSigma)) >> 8;
      *(p_pRspBuf++) = *(uint16*)(&p_pEntry->m_stSmib.m_stNeighDiag.m_nClockSigma);
  }
  
  // insert clock diagnostics
  if (p_pEntry->m_stSmib.m_stNeighDiag.m_ucDiagFlags & DLL_MASK_NEIDIAG_CLOCK)
  {
      *(p_pRspBuf++) = (*(uint16*)(&p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_nClkBias)) >> 8;
      *(p_pRspBuf++) = (*(uint16*)(&p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_nClkBias));
      
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unClkCount);
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unClkTimeoutNo);
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stNeighDiag.m_stDiag.m_unClkOutliersNo);      
  }
  
  return (uint8)(p_pRspBuf - pStart); 
}

uint8 DLMO_SetSMIBNeighDiagEntry( uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry )
{  
  return SFC_READ_ONLY_ATTRIBUTE;  // operation on read-only attribute; do nothing;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes neighbor entry data
/// @param  p_pEntry - neighbor entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBNeighborEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;  
  const DLL_SMIB_NEIGHBOR* pNei = &p_pEntry->m_stSmib.m_stNeighbor;
  
  uint8 ucExtGrCnt = (pNei->m_ucInfo & DLL_MASK_NEINOFGR) >> DLL_ROT_NEINOFGR;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pNei->m_unUID);
  
  DLME_CopyReversedEUI64Addr(p_pRspBuf, pNei->m_aEUI64);  
  p_pRspBuf += 8;
  
  *(p_pRspBuf++) = pNei->m_ucGroupCode;
  *(p_pRspBuf++) = pNei->m_ucInfo;
  
  for(uint8 ucIdx=0; ucIdx < ucExtGrCnt; ucIdx++)
  {
    p_pRspBuf = DMAP_InsertUint16(p_pRspBuf, pNei->m_aExtendedGraphs[ucIdx] );
  }       
  
  if(pNei->m_ucInfo & DLL_MASK_NEILINKBACKLOG)
  {
    p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pNei->m_unLinkBacklogIdx);
    *(p_pRspBuf++) = pNei->m_ucLinkBacklogDur;
  }  
 
  return (p_pRspBuf - pStart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief Debinarizes superframe entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - superframe entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBSuperframeEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{  
  DLL_SMIB_SUPERFRAME * pSf = &p_pEntry->m_stSmib.m_stSuperframe;
  const uint8 * pData = p_pBuf;
  
#ifdef UT_ZERO_FILL  // unit testing support
  memset(pSf, 0, sizeof(DLL_SMIB_SUPERFRAME));
#endif // UT_ZERO_FILL    
  
  pData = DLMO_ExtractExtDLUint(pData, &pSf->m_unUID);
  pData = DMAP_ExtractUint16(pData, &pSf->m_unTsDur);  
  pData = DLMO_ExtractExtDLUint(pData, &pSf->m_unChIndex);
  
  pSf->m_ucChBirth  = *(pData++);
  pSf->m_ucInfo   = *(pData++);
          
  pData = DMAP_ExtractUint16( pData, &pSf->m_unSfPeriod );
  pData = DMAP_ExtractUint16( pData, &pSf->m_unSfBirth );
  pData = DLMO_ExtractExtDLUint(pData, &pSf->m_unChRate); // this should always be nonzero!
  
  pSf->m_unChMap = 0x7FFF; // default value
  if (pSf->m_ucInfo & DLL_MASK_CH_MAP_OV)
  {
      pData = DMAP_ExtractUint16( pData, &pSf->m_unChMap );
  }
  
  uint8 ucSfType = (pSf->m_ucInfo & DLL_MASK_SFTYPE) >> DLL_ROT_SFTYPE;
  if (pSf->m_ucInfo & DLL_MASK_SFIDLE)
  {
      pData = DMAP_ExtractUint32( pData, (uint32*)&pSf->m_lIdleTimer );
  }
  
  if( ucSfType >= SF_TYPE_RND_SLOW_HOP_DUR) 
  {
      pSf->m_ucRndSlots = *(pData++);
  }
    
  if (*p_pSize < (uint16)(pData - p_pBuf))
      return SFC_INVALID_SIZE;
  
  if (!pSf->m_unTsDur || !pSf->m_unSfPeriod || (pSf->m_unSfBirth >= pSf->m_unSfPeriod))
      return SFC_INVALID_VALUE;
  
  *p_pSize = (uint16)(pData - p_pBuf);    
  
  return SFC_SUCCESS; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes superframe entry data
/// @param  p_pEntry - superframe entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBSuperframeEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;  
  const DLL_SMIB_SUPERFRAME *pSf = &p_pEntry->m_stSmib.m_stSuperframe;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pSf->m_unUID);
  
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pSf->m_unTsDur );
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pSf->m_unChIndex);
  
  *(p_pRspBuf++) = pSf->m_ucChBirth;
  *(p_pRspBuf++) = pSf->m_ucInfo;
  
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pSf->m_unSfPeriod );
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pSf->m_unSfBirth );
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pSf->m_unChRate);   

  if (pSf->m_ucInfo & DLL_MASK_CH_MAP_OV)
  {
      p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pSf->m_unChMap );
  }
  
  if (pSf->m_ucInfo & DLL_MASK_SFIDLE)
  {
      p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, *((uint32*)&pSf->m_lIdleTimer));
  }   
  
  uint8 ucSfType = (pSf->m_ucInfo & DLL_MASK_SFTYPE) >> DLL_ROT_SFTYPE;  
  
  if (ucSfType >= SF_TYPE_RND_SLOW_HOP_DUR)
  {
    *(p_pRspBuf++) = pSf->m_ucRndSlots;
  }
   
  return (p_pRspBuf-pStart);
} 

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief Debinarizes superframe idle entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - superframe idle entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBSfIdleEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{
  const uint8 * pStart = p_pBuf;  
  uint16        unStartSize = *p_pSize;
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &p_pEntry->m_stSmib.m_stSfIdle.m_unUID); 
  
  p_pEntry->m_stSmib.m_stSfIdle.m_ucIdle = *(p_pBuf++) & DLL_MASK_SFIDLE; // ensure reserved bits are not harmful
  
  if( p_pEntry->m_stSmib.m_stSfIdle.m_ucIdle)
  {
      p_pBuf = DMAP_ExtractUint32(p_pBuf, (uint32*)&p_pEntry->m_stSmib.m_stSfIdle.m_lIdleTimer); 
  }
  
  if (unStartSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
   
  return SFC_SUCCESS; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes superframe idle entry data
/// @param  p_pEntry - superframe idle entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBSfIdleEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8 * p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf; 
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, p_pEntry->m_stSmib.m_stSfIdle.m_unUID);
  
  *(p_pRspBuf++) = p_pEntry->m_stSmib.m_stSfIdle.m_ucIdle;
  
  if(p_pEntry->m_stSmib.m_stSfIdle.m_ucIdle)
    p_pRspBuf = DMAP_InsertUint32(p_pRspBuf, *((uint32*)&p_pEntry->m_stSmib.m_stSfIdle.m_lIdleTimer));
                                     
  return (uint8)(p_pRspBuf - pStart); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief Debinarizes graph entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - graph entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBGraphEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{
  const uint8 *pStart = p_pBuf;
  uint16 unStartSize = *p_pSize;  
  DLL_SMIB_GRAPH* pGr = &p_pEntry->m_stSmib.m_stGraph;

#ifdef UT_ZERO_FILL  // unit testing support
  memset(pGr, 0, sizeof(DLL_SMIB_GRAPH));
#endif // UT_ZERO_FILL 
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pGr->m_unUID);
 
  pGr->m_ucQueue = *(p_pBuf++);
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pGr->m_unMaxLifetime);
  
  
  pGr->m_ucNeighCount = (pGr->m_ucQueue & DLL_MASK_GRNEICNT) >> DLL_ROT_GRNEICNT;
  
  for(uint8 ucIdx = 0; ucIdx < pGr->m_ucNeighCount; ucIdx++)
  {
      p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pGr->m_aNeighbors[ucIdx]);
  }
    
  *p_pSize = (p_pBuf - pStart);
  
  if(unStartSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes graph entry data
/// @param  p_pEntry - graph entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBGraphEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;  
  const DLL_SMIB_GRAPH* pGr = &p_pEntry->m_stSmib.m_stGraph;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pGr->m_unUID);

  *(p_pRspBuf++) =  pGr->m_ucQueue;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pGr->m_unMaxLifetime);
  
  for(uint8 ucIdx = 0; ucIdx < pGr->m_ucNeighCount; ucIdx++)
  {
    p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pGr->m_aNeighbors[ucIdx]);
  } 

  return (p_pRspBuf - pStart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief Debinarizes link entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - link entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBLinkEntry(uint16* p_pSize, const uint8* p_pBuf,DLMO_SMIB_ENTRY* p_pEntry)
{
  const uint8 * pStart = p_pBuf;
  uint16 unStartSize = *p_pSize;
  DLL_SMIB_LINK* pLink = &p_pEntry->m_stSmib.m_stLink;

#ifdef UT_ZERO_FILL  // unit testing support
  memset(pLink, 0, sizeof(DLL_SMIB_LINK));
#endif // UT_ZERO_FILL 
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_unUID);
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_unSfIdx);
  pLink->m_mfLinkType = *(p_pBuf++); 
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_unTemplate1);
  
  if ((pLink->m_mfLinkType & DLL_MASK_LNKTX) && (pLink->m_mfLinkType & DLL_MASK_LNKRX))
  {
      p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_unTemplate2);
  }
  
  pLink->m_mfTypeOptions = *(p_pBuf++);
  
  uint8 ucOptions = (pLink->m_mfTypeOptions & DLL_MASK_LNKNEI) >> DLL_ROT_LNKNEI;
  
  if (ucOptions)
  {
      if (ucOptions == 0x03)
          return SFC_INVALID_VALUE;
      
      p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_unNeighbor);
  }
  
  ucOptions = (pLink->m_mfTypeOptions & DLL_MASK_LNKGR) >> DLL_ROT_LNKGR;
  
  if (ucOptions)
  {
      if (ucOptions == 0x03)
          return SFC_INVALID_VALUE;
      
      p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_unGraphId);
  }
  
  // supplementary check if link is valid
  if ( (pLink->m_mfLinkType & DLL_MASK_LNKTX)
        && 0 == (pLink->m_mfLinkType & (DLL_MASK_LNKDISC | DLL_MASK_LNKJOIN))
        && 0 == (pLink->m_mfTypeOptions & (DLL_MASK_LNKGR | DLL_MASK_LNKNEI)) )
  {
      return  SFC_TYPE_MISSMATCH;
  }
  
  uint8 ucSchedType = (pLink->m_mfTypeOptions & DLL_MASK_LNKSC) >> DLL_ROT_LNKSC;
  
  if ( ucSchedType < 3 )  
  {
      p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_aSchedule.m_aOffset.m_unOffset);
      
      if ( ucSchedType )
      {
          p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pLink->m_aSchedule.m_aOffsetInterval.m_unInterval);

          if ( (1 == ucSchedType) && (0 == pLink->m_aSchedule.m_aOffsetInterval.m_unInterval) )
              return SFC_INVALID_VALUE;
      
          if ( (2 == ucSchedType) && (pLink->m_aSchedule.m_aRange.m_unFirst > pLink->m_aSchedule.m_aRange.m_unLast) )
              return SFC_INVALID_VALUE;
      }
      else // offest only 
      {
          pLink->m_aSchedule.m_aRange.m_unLast = pLink->m_aSchedule.m_aRange.m_unFirst;
      }
  }
  else 
  {
      p_pBuf = DMAP_ExtractUint32( p_pBuf, &pLink->m_aSchedule.m_ulBitmap );
  }
 
  if (pLink->m_mfTypeOptions & DLL_MASK_LNKCH )
      pLink->m_ucChOffset = *(p_pBuf++);
  
  if (pLink->m_mfTypeOptions & DLL_MASK_LNKPR )
      pLink->m_ucPriority = *(p_pBuf++);  
  
  // not part of ISA100 link definition, but need to be initialized to zero (internal link backlog mechanism)
  pLink->m_ucActiveTimer = 0;

  *p_pSize = (uint16)(p_pBuf - pStart);
  
  if ( unStartSize < *p_pSize )
      return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes link entry data
/// @param  p_pEntry - link entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBLinkEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;  
  const DLL_SMIB_LINK* pLink = &p_pEntry->m_stSmib.m_stLink;
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_unUID);

  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_unSfIdx);
  *(p_pRspBuf++) = pLink->m_mfLinkType;
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_unTemplate1);
  
  if( (pLink->m_mfLinkType & DLL_MASK_LNKTX) && (pLink->m_mfLinkType & DLL_MASK_LNKRX) )
  {
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_unTemplate2);
  }
  
  *(p_pRspBuf++) = pLink->m_mfTypeOptions;
  
  if ( pLink->m_mfTypeOptions & DLL_MASK_LNKNEI )
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_unNeighbor);
 
  if ( pLink->m_mfTypeOptions & DLL_MASK_LNKGR )
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_unGraphId);
  
  uint8 ucSheduleType = (pLink->m_mfTypeOptions & DLL_MASK_LNKSC) >> DLL_ROT_LNKSC;
  
  if( ucSheduleType < 3)  
  {
      p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_aSchedule.m_aOffset.m_unOffset);
      
      if (ucSheduleType)
      {
          p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pLink->m_aSchedule.m_aOffsetInterval.m_unInterval);
      }
  }
  else
  {
      p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, pLink->m_aSchedule.m_ulBitmap );
  }
  
  if (pLink->m_mfTypeOptions & DLL_MASK_LNKCH)
      *(p_pRspBuf++) = pLink->m_ucChOffset;
  
  if (pLink->m_mfTypeOptions & DLL_MASK_LNKPR)
      *(p_pRspBuf++) = pLink->m_ucPriority;
    
  return (p_pRspBuf - pStart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Debinarizes route entry data
/// @param  p_pSize - input buffer size
/// @param  p_pBuf - input buffer 
/// @param  p_pEntry - route entry
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_SetSMIBRouteEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry)
{
  const uint8 *pStart = p_pBuf;
  uint16 unStartSize = *p_pSize;
  DLL_SMIB_ROUTE* pRoute = &p_pEntry->m_stSmib.m_stRoute;
  
#ifdef UT_ZERO_FILL  // unit testing support
  memset(pRoute, 0, sizeof(DLL_SMIB_ROUTE));
#endif // UT_ZERO_FILL 
  
  p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pRoute->m_unUID);
  
  pRoute->m_ucInfo = *(p_pBuf++);
  
  uint8 ucRouteSize = (pRoute->m_ucInfo & DLL_MASK_RTNO)  >> DLL_ROT_RTNO;
  
  if (ucRouteSize > (uint8)MAX_DROUT_ROUTES_NO)
      return SFC_INVALID_SIZE;
  
  pRoute->m_ucFwdLimit = *(p_pBuf++);
  
  for(uint8 ucIdx = 0; ucIdx < ucRouteSize; ucIdx++)
  {
      p_pBuf = DMAP_ExtractUint16( p_pBuf, pRoute->m_aRoutes+ucIdx );                         
  }
  
  uint8 ucRtAlternative = (pRoute->m_ucInfo & DLL_RTSEL_MASK);
  
  if( DLL_RTSEL_DEFAULT != ucRtAlternative )
  {
    p_pBuf = DMAP_ExtractUint16( p_pBuf, &pRoute->m_unSelector );                         

    if( DLL_RTSEL_BBR_CONTRACT == ucRtAlternative )
    {
    #ifdef BACKBONE_SUPPORT
        p_pBuf = DLMO_ExtractExtDLUint(p_pBuf, &pRoute->m_unNwkSrcAddr);
    #else
        return SFC_VALUE_OUT_OF_RANGE;
    #endif
    }
  }
 
  *p_pSize = (p_pBuf - pStart);
  
  if(unStartSize < (p_pBuf - pStart))
      return SFC_INVALID_SIZE;
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Binarizes route entry data
/// @param  p_pEntry - route entry
/// @param  p_pRspBuf - output buffer
/// @return buffer size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_GetSMIBRouteEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf)
{
  uint8 *pStart = p_pRspBuf;    
  const DLL_SMIB_ROUTE* pRt = &p_pEntry->m_stSmib.m_stRoute;
  
  uint8 ucRtCnt = (pRt->m_ucInfo & DLL_MASK_RTNO) >> DLL_ROT_RTNO;
  uint8 ucRtAlternative = (pRt->m_ucInfo & DLL_RTSEL_MASK);
  
  p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pRt->m_unUID);
  
  *(p_pRspBuf++) = pRt->m_ucInfo;
  *(p_pRspBuf++) = pRt->m_ucFwdLimit;
  
  for(uint8 ucIdx = 0; ucIdx < ucRtCnt; ucIdx++)
  {
      p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pRt->m_aRoutes[ucIdx] );
  }        
  
  if( DLL_RTSEL_DEFAULT != ucRtAlternative )
  {
      p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, pRt->m_unSelector );

  #ifdef BACKBONE_SUPPORT  
    if( DLL_RTSEL_BBR_CONTRACT == ucRtAlternative )
    {
        p_pRspBuf = DLMO_InsertExtDLUint(p_pRspBuf, pRt->m_unNwkSrcAddr);
    }
  #endif
  }
  
  return (p_pRspBuf - pStart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Inserts (binarizes) an extDLUint into a specified buffer by p_pData
/// @param  p_pData     - (output) a pointer to the data buffer
/// @param  p_unValue   - the 15 bit uint data to be binarized
/// @return p_pData     - pointer at end of inserted data
/// @remarks  don't check inside if value fits into 15 bits. this check must be done by caller\n
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * DLMO_InsertExtDLUint( uint8 * p_pData, uint16  p_unValue)
{

  if (p_unValue < 0x80) 
  {
      *(p_pData++) = p_unValue << 1;
  }
  else
  {
      *(p_pData++) = (p_unValue << 1) | 0x01;
      *(p_pData++) = p_unValue >> 7;      
  }

  return (p_pData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parses an extDLUint into an uint16
/// @param  p_pData     - pointer to the data to be parsed
/// @param  p_pValue    - (output)a pointer to an uint16 , where to put the result  
/// @return pointer to the next data to be parsed
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8 * DLMO_ExtractExtDLUint( const uint8 * p_pData, uint16 * p_pValue)
{
  if ( !(*p_pData & 0x01) )
  {
      *p_pValue = (uint16)(*(p_pData++)) >> 1;
      return p_pData;
  }  
  *p_pValue = (p_pData[0] >> 1) | ((uint16)( p_pData[1] ) << 7);
  
  return (p_pData+2);
  
}

