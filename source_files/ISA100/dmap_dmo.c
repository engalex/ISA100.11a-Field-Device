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
/// Author:       Nivis LLC, Eduard Erdei 
/// Date:         December 2008
/// Description:  This file implements the DMO object of the DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "dmap_dmo.h"
#include "dmap_co.h"
#include "dmap_armo.h"
#include "dmap.h"
#include "aslsrvc.h"
#include "mlsm.h"
#include "string.h"
#include "config.h"
#include "sfc.h"
#include "nlme.h"
#include "tlde.h"
#include "uap.h"
#include "dlmo_utils.h"
#include "../CommonAPI/DAQ_Comm.h"

// DMAP DMO Object Constant atributes

const uint8 c_aucInvalidIPV6Addr[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const uint8  c_ucNonVolatileMemoryCapable = 0;

const uint32 c_ulDevMemTotal       = 0x18000;  // units in octets

const uint8 c_ucCommSWMajorVer  = 0;          // assigned by ISA100
const uint8 c_ucCommSWMinorVer  = 0;          // assigned by ISA100

#define STRING_VERSION '0','1','.','0','0','.','0','0'

#ifdef BACKBONE_SUPPORT
    const uint8 c_aucSWRevInfo[12]    = {'B','B','_','_', STRING_VERSION };
#else
    const uint8 c_aucSWRevInfo[12]    = {'V','N','_','_', STRING_VERSION };
#endif


const uint8 c_ucDMAPObjCount = (uint8)DMAP_OBJ_NO;

// we will not internally use this data, it will be read by SM; store it as a stream;
const uint8 c_aucObjList[DMAP_OBJ_NO*5] = 
{
  (uint16)(DMAP_DMO_OBJ_ID) >> 8,   (uint8)(DMAP_DMO_OBJ_ID),   (uint8)(OBJ_TYPE_DMO),   0, 0, 
  (uint16)(DMAP_ARMO_OBJ_ID) >> 8,  (uint8)(DMAP_ARMO_OBJ_ID),  (uint8)(OBJ_TYPE_ARMO),  0, 0,
  (uint16)(DMAP_DSMO_OBJ_ID) >> 8,  (uint8)(DMAP_DSMO_OBJ_ID),  (uint8)(OBJ_TYPE_DSMO),  0, 0,
  (uint16)(DMAP_DLMO_OBJ_ID) >> 8,  (uint8)(DMAP_DLMO_OBJ_ID),  (uint8)(OBJ_TYPE_DLMO),  0, 0,
  (uint16)(DMAP_NLMO_OBJ_ID) >> 8,  (uint8)(DMAP_NLMO_OBJ_ID),  (uint8)(OBJ_TYPE_NLMO),  0, 0,
  (uint16)(DMAP_TLMO_OBJ_ID) >> 8,  (uint8)(DMAP_TLMO_OBJ_ID),  (uint8)(OBJ_TYPE_TLMO),  0, 0,
  (uint16)(DMAP_ASLMO_OBJ_ID) >> 8, (uint8)(DMAP_ASLMO_OBJ_ID), (uint8)(OBJ_TYPE_ASLMO), 0, 0,  
  (uint16)(DMAP_UDO_OBJ_ID) >> 8,   (uint8)(DMAP_UDO_OBJ_ID),   (uint8)(OBJ_TYPE_UDO),   0, 0,  
  (uint16)(DMAP_DPO_OBJ_ID) >> 8,   (uint8)(DMAP_DPO_OBJ_ID),   (uint8)(OBJ_TYPE_DPO),   0, 0,
  (uint16)(DMAP_HRCO_OBJ_ID) >> 8,  (uint8)(DMAP_HRCO_OBJ_ID),  (uint8)(OBJ_TYPE_HRCO),  0, 0
};

DMO_ATTRIBUTES g_stDMO;

static uint8  g_ucContractReqType;
static uint8  g_ucContractRspPending;
static uint8  g_ucContractReqID = 1;      
static uint16 g_unTimeout;

static uint8  g_aucBannedEndpoint[16];
static uint16 g_unBannedEndpointTimer;

uint16 g_unCustomAlarmCounter;            // seconds

#ifndef BACKBONE_SUPPORT
    #define POWER_STATUS_UPDATE_PERIOD       3600 //seconds
    static uint16 g_unPowerUpdatePeriod;
#endif

#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
static uint8  g_ucUAPContractReqID;   // used to store the contract request ID of an UAP
static uint8  g_ucContractSrcSAP;
static uint8  g_ucKeySrcSAP;
#endif

static void DMO_readTAITime(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);
static void DMO_readContract(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize);

static uint8 DMO_parseContract( const uint8 *              p_pData,                          
                                DMO_CONTRACT_ATTRIBUTE *   p_pContract ); 

static uint8 DMO_deleteContract( uint16 p_unContractID );

static uint8 DMO_setContract( uint16        p_unDeleteIndex,
                              uint32        p_ulTaiCutover, 
                              const uint8 * p_pData, 
                              uint8         p_ucDataLen );

uint8 * DMO_InsertBandwidthInfo( uint8 *                         p_pData, 
                                 const DMO_CONTRACT_BANDWIDTH *  p_pBandwidth, 
                                 uint8                           p_ucServiceType);

static void DMO_checkPowerStatusAlert(void);

static void DMO_setPingStatus( void );

void DMO_generateRestartAlarm(void);
  
static uint8 DMO_sendContractRequest(DMO_CONTRACT_ATTRIBUTE * p_pContract,
                                     uint8                    p_ucRequestType);
  

const DMAP_FCT_STRUCT c_aDMOFct[DMO_ATTR_NO] =
{
  { 0, 0                                              , DMAP_EmptyReadFunc     , NULL },     // just for protection; attributeID will match index in this table     
  { c_oEUI64BE,     sizeof(c_oEUI64BE)                , DMAP_ReadOctetString   , NULL },     // DMO_EUI64                   = 1,
  { ATTR_CONST(g_stDMO.m_unShortAddr)                 , DMAP_ReadUint16        , DMAP_WriteUint16 },        // DMO_16BIT_DL_ALIAS          = 2,
  { ATTR_CONST(g_stDMO.m_auc128BitAddr)               , DMAP_ReadOctetString   , DMAP_WriteOctetString }, // DMO_128BIT_NWK_ADDR         = 3,   
  { ATTR_CONST(g_stDPO.m_unDeviceRole)                , DMAP_ReadUint16        , NULL },     // DMO_DEVICE_ROLE_CAPABILITY  = 4,
  { ATTR_CONST(g_stDMO.m_unAssignedDevRole)           , DMAP_ReadUint16        , DMAP_WriteUint16 },        // DMO_ASSIGNED_DEVICE_ROLE    = 5,  
  { ATTR_CONST(g_stDMO.m_aucVendorID)                 , DMAP_ReadVisibleString , DMAP_WriteVisibleString },     // DMO_VENDOR_ID               = 6,  
  { ATTR_CONST(g_stDMO.m_aucModelID)                  , DMAP_ReadVisibleString , DMAP_WriteVisibleString },     // DMO_MODEL_ID                = 7,  
  { ATTR_CONST(g_stDMO.m_aucTagName)                  , DMAP_ReadVisibleString , DMAP_WriteVisibleString }, // DMO_TAG_NAME                = 8,  
  { ATTR_CONST(g_stDMO.m_aucSerialNo)                 , DMAP_ReadVisibleString , DMAP_WriteVisibleString },     // DMO_SERIAL_NO               = 9,  
  { ATTR_CONST(g_stDMO.m_ucPWRStatus)                 , DMAP_ReadUint8         , DMAP_WriteUint8 },         // DMO_PWR_SUPPLY_STATUS       = 10,  
  { ATTR_CONST(g_stDMO.m_stPwrAlertDescriptor)        , DMAP_ReadAlertDescriptor, DMAP_WriteAlertDescriptor }, // DMO_PWR_CK_ALERT_DESCRIPTOR = 11,
  { ATTR_CONST(g_stDMO.m_ucDMAPState)                 , DMAP_ReadUint8         , NULL },     // DMO_DMAP_STATE              = 12,
  { ATTR_CONST(g_stDMO.m_ucJoinCommand)               , DMAP_ReadUint8         , DMAP_WriteUint8 },         // DMAP_JOIN_COMMAND           = 13,  
  { ATTR_CONST(g_stDMO.m_ulStaticRevLevel)            , DMAP_ReadUint32        , NULL },     // DMO_STATIC_REV_LEVEL        = 14,
  { ATTR_CONST(g_stDMO.m_unRestartCount)              , DMAP_ReadUint16        , NULL },     // DMAP_RESTART_COUNT          = 15,
  { ATTR_CONST(g_stDMO.m_ulUptime)                    , DMAP_ReadUint32        , NULL },     // DMAP_UPTIME                 = 16,
  { (void*)&c_ulDevMemTotal, sizeof(c_ulDevMemTotal)  , DMAP_ReadUint32        , NULL },     // DMAP_DEV_MEM_TOTAL          = 17,
  { ATTR_CONST(g_stDMO.m_ulUsedDevMem)                , DMAP_ReadUint32        , NULL },     // DMAP_DEV_MEM_USED           = 18,
  { 0, 6                                              , DMO_readTAITime        , NULL },     // DMAP_TAI_TIME               = 19,
  {(void*)&c_ucCommSWMajorVer , sizeof(c_ucCommSWMajorVer), DMAP_ReadUint8     , NULL },     // DMAP_COMM_SW_MAJOR_VERSION  = 20,
  {(void*)&c_ucCommSWMinorVer , sizeof(c_ucCommSWMinorVer), DMAP_ReadUint8     , NULL },     // DMAP_COMM_SW_MINOR_VERSION  = 21,  
  {(void*)c_aucSWRevInfo , sizeof(c_aucSWRevInfo)     , DMAP_ReadVisibleString , NULL },     // DMAP_SW_REVISION_INFO       = 22,
  { ATTR_CONST(g_stDMO.m_aucSysMng128BitAddr)         , DMAP_ReadOctetString   , DMAP_WriteOctetString }, // DMAP_SYS_MNG_128BIT_ADDR    = 23,
  { ATTR_CONST(g_stDMO.m_unSysMngEUI64)               , DMAP_ReadOctetString   , DMAP_WriteOctetString }, // DMAP_SYS_MNG_EUI64          = 24, 
  { ATTR_CONST(g_stDMO.m_unSysMngShortAddr)           , DMAP_ReadUint16        , DMAP_WriteUint16 },        // DMAP_SYS_MNG_16BIT_ALIAS    = 25,
  { 0, DMO_CONTRACT_BUF_SIZE                          , DMO_readContract       , NULL },       // DMAP_CONTRACT_TABLE       = 26,
  { ATTR_CONST(g_stDMO.m_unContractReqTimeout)        , DMAP_ReadUint16        , DMAP_WriteUint16 },        // DMAP_CONTRACT_REQ_TIMEOUT   = 27,
  { ATTR_CONST(g_stDMO.m_ucMaxClntSrvRetries)         , DMAP_ReadUint8         , DMAP_WriteUint8 },         // DMAP_MAX_CLNT_SRV_RETRIES   = 28,
  { ATTR_CONST(g_stDMO.m_unMaxRetryToutInterval)      , DMAP_ReadUint16        , DMAP_WriteUint16 },        // DMAP_MAX_RETRY_TOUT_INTERVAL= 29
  {(void*)&c_ucDMAPObjCount, sizeof(c_ucDMAPObjCount) , DMAP_ReadUint8         , NULL},     // DMAP_OBJECTS_COUNT          = 30,
  {(void*)&c_aucObjList, sizeof(c_aucObjList)         , DMAP_ReadOctetString   , NULL },    // DMAP_OBJECTS_LIST           = 31,
  { 0, 4                                              , DMAP_ReadContractMeta  , NULL },    // DMAP_CONTRACT_METADATA      = 32,
  { ATTR_CONST(c_ucNonVolatileMemoryCapable)          , DMAP_ReadUint8         , NULL},     // DMAP_DMO_NON_VOLTATILE_MEM_CAPABLE       = 33,
  { ATTR_CONST(g_stDMO.m_unWarmRestartAttemptTout)    , DMAP_ReadUint16        , DMAP_WriteUint16} // DMO_WARM_RESTART_ATTEMPT_TIMEOUT = 34,
  
#ifdef BACKBONE_SUPPORT
  ,
  { ATTR_CONST(g_stDMO.m_unCrtUTCDrift)                 , DMAP_ReadUint16         , NULL },  // DMO_CRT_UTC_DRIFT           = 35,
  { ATTR_CONST(g_stDMO.m_ulNextDriftTAI)                , DMAP_ReadUint32         , NULL },  // DMO_NEXT_TIME_DRIFT         = 36,
  { ATTR_CONST(g_stDMO.m_unNextUTCDrift)                , DMAP_ReadUint32         , NULL }   // DMO_NEXT_UTC_DRIFT          = 37,
#endif 
};


void DMO_readTAITime(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
  uint16 unSize;
  
  DLMO_ReadTAI( 0, &unSize, p_pBuf);  
  *p_ucSize = 6; 
}

void DMO_readContract(const void * p_pValue, uint8 * p_pBuf, uint8* p_ucSize)
{
    // tbd
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parses an bandwidth data stream to a dmo bandwidth structure (part of a contract entry)
/// @param  p_pData - buffer containing bandwidth data
/// @param  p_pBandwidth - bandwith structure to be filled
/// @param  p_ucServiceType - type of service (periodic or aperiodic)
/// @return pointer to the next data to be parsed
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8 * DMO_ExtractBandwidthInfo( const uint8 *            p_pData, 
                                        DMO_CONTRACT_BANDWIDTH * p_pBandwidth,
                                        uint8                    p_ucServiceType )
{
  // following line does the same work for both periodic/aperiodic contracts (period or commited burst)
  p_pData = DMAP_ExtractUint16( p_pData, (uint16*)&p_pBandwidth->m_stPeriodic.m_nPeriod); 
  
  if ( p_ucServiceType == SRVC_PERIODIC_COMM )
  {      
      p_pBandwidth->m_stPeriodic.m_ucPhase = *(p_pData++);
      
      p_pData = DMAP_ExtractUint16( p_pData, &p_pBandwidth->m_stPeriodic.m_unDeadline);
  }
  else // non periodic communication service
  {      
      p_pData = DMAP_ExtractUint16( p_pData, (uint16*)&p_pBandwidth->m_stAperiodic.m_nExcessBurst);      
      p_pBandwidth->m_stAperiodic.m_ucMaxSendWindow = *(p_pData++);     
  }  
  return p_pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes a bandwidth data structure to a stream(part of a contract entry)
/// @param  p_pData - buffer where to put data
/// @param  p_pBandwidth - pointer to the bandwith structure to be binarized
/// @param  p_ucServiceType - type of service (periodic or aperiodic)
/// @return pointer to the last written byte (actually to the next free available byte in stream)
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * DMO_InsertBandwidthInfo(  uint8 *                         p_pData, 
                                  const DMO_CONTRACT_BANDWIDTH *  p_pBandwidth, 
                                  uint8                           p_ucServiceType)
{
  // following line does the same work for both periodic/aperiodic contracts (period or commited burst)
  p_pData = DMAP_InsertUint16( p_pData, *(uint16*)&p_pBandwidth->m_stPeriodic.m_nPeriod); 
  
  if ( p_ucServiceType == SRVC_PERIODIC_COMM )
  {      
      *(p_pData++) = p_pBandwidth->m_stPeriodic.m_ucPhase;
      
      p_pData = DMAP_InsertUint16( p_pData, p_pBandwidth->m_stPeriodic.m_unDeadline);
  }
  else // non periodic communication service
  {      
      p_pData = DMAP_InsertUint16( p_pData, *(uint16*)&p_pBandwidth->m_stAperiodic.m_nExcessBurst);      
       *(p_pData++) = p_pBandwidth->m_stAperiodic.m_ucMaxSendWindow;
  }    
  return p_pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parses an contract data stream to a dmo contractdata structure
/// @param  p_pData     - pointer to the data to be parsed
/// @param  p_pContract - pointer to a dmo contract data structure, where to put data
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Obs:   Check outside if buffer length is correct
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_parseContract( const uint8 *              p_pData,                          
                         DMO_CONTRACT_ATTRIBUTE *   p_pContract )
{  
  p_pData = DMAP_ExtractUint16(p_pData, &p_pContract->m_unContractID); 
  
  p_pContract->m_ucContractStatus = *(p_pData++);  
  p_pContract->m_ucServiceType    = *(p_pData++); 
  
  p_pData = DMAP_ExtractUint32(p_pData, &p_pContract->m_ulActivationTAI); 
  p_pData = DMAP_ExtractUint16(p_pData, &p_pContract->m_unSrcTLPort); 
  
  memcpy(p_pContract->m_aDstAddr128, p_pData, sizeof(p_pContract->m_aDstAddr128));
  p_pData += sizeof(p_pContract->m_aDstAddr128);
  
  p_pData = DMAP_ExtractUint16(p_pData, &p_pContract->m_unDstTLPort);     
  p_pData = DMAP_ExtractUint32(p_pData, &p_pContract->m_ulAssignedExpTime);
  
  p_pContract->m_ucPriority = *(p_pData++); 
  
  p_pData = DMAP_ExtractUint16(p_pData, &p_pContract->m_unAssignedMaxNSDUSize);     
  
  p_pContract->m_ucReliability = *(p_pData++);   
  
  p_pData = DMO_ExtractBandwidthInfo(  p_pData, 
                                       &p_pContract->m_stBandwidth, 
                                       p_pContract->m_ucServiceType );   
  return SFC_SUCCESS;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Binarizes a dmo contract data structure to a stream
/// @param  p_pContract  - pointer to the contract to be binarized
/// @param  p_pRspBuf    - pointer to buffer where to put data
/// @param  p_ucBufLen   - input: available length of output buffer; output: no of bytes put into bufer  
/// @return pointer to the next data to be parsed
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_binarizeContract( const DMO_CONTRACT_ATTRIBUTE * p_pContract,  
                            uint8  * p_pRspBuf,
                            uint16 * p_unBufLen)
{
  uint8 * pucStart = p_pRspBuf;
  
  if ( *p_unBufLen < DMO_CONTRACT_BUF_SIZE )
      return SFC_DEVICE_SOFTWARE_CONDITION;
  
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, p_pContract->m_unContractID);  
  
  *(p_pRspBuf++) = p_pContract->m_ucContractStatus;    
  *(p_pRspBuf++) = p_pContract->m_ucServiceType;  
  
  p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, p_pContract->m_ulActivationTAI);  
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, p_pContract->m_unSrcTLPort);    

  memcpy(p_pRspBuf, p_pContract->m_aDstAddr128, sizeof(p_pContract->m_aDstAddr128));
  p_pRspBuf +=  sizeof(p_pContract->m_aDstAddr128);
  
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, p_pContract->m_unDstTLPort);   
  p_pRspBuf = DMAP_InsertUint32(p_pRspBuf, p_pContract->m_ulAssignedExpTime);
  
  *(p_pRspBuf++) = p_pContract->m_ucPriority;  
  
  p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, p_pContract->m_unAssignedMaxNSDUSize);     
  *(p_pRspBuf++) = p_pContract->m_ucReliability;
  
  p_pRspBuf = DMO_InsertBandwidthInfo(  p_pRspBuf,
                                        &p_pContract->m_stBandwidth,
                                        p_pContract->m_ucServiceType);  
  
  *p_unBufLen = p_pRspBuf - pucStart;
  
  return SFC_SUCCESS;   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Adds a contract to the dmo contract table
/// @param  p_unDeleteIndex - index of the contract to be deleted
/// @param  p_ulTaiCutover  - TAI time when adding should be performed (not implemented)
/// @param  p_pData         - pointer to binarized contract data
/// @param  p_ucDataLen     - lenght of binarized contract data
/// @return - servie feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_setContract(  uint16        p_unDeleteIndex,
                        uint32        p_ulTaiCutover, 
                        const uint8 * p_pData, 
                        uint8         p_ucDataLen )
{
  DMO_CONTRACT_ATTRIBUTE * pContract;
  
  if ( p_ucDataLen < DMO_CONTRACT_BUF_SIZE )
      return SFC_INVALID_SIZE;
 
  uint16 unIndex = ((uint16)p_pData[0] << 8) | p_pData[1];
  
  if ( p_unDeleteIndex != unIndex )
  {
      // index of write_row argument (or write srvc apdu header index) and index 
      // of contract data are different. perform delete operation on first index
      DMO_deleteContract( p_unDeleteIndex );
  } 
  
  // now find the second index contract  
  pContract = DMO_FindContract( unIndex );
    
  if( !pContract ) // not found, add it
  {
      if( g_stDMO.m_ucContractNo >= MAX_CONTRACT_NO )
      {
          return SFC_INSUFFICIENT_DEVICE_RESOURCES;
      }
      
      if( g_ucContractRspPending && (g_stDMO.m_ucContractNo < (MAX_CONTRACT_NO-1)) )
      {
          // take care of the 128bit address and SAPS of the awaiting contract response
           memcpy(g_stDMO.m_aContractTbl + g_stDMO.m_ucContractNo + 1,
                  g_stDMO.m_aContractTbl + g_stDMO.m_ucContractNo,
                  sizeof(g_stDMO.m_aContractTbl[0]) );
      }
      
      pContract = g_stDMO.m_aContractTbl + (g_stDMO.m_ucContractNo++);
  } 
  
  DMO_parseContract( p_pData, pContract );     
  
  // inform other DMAP concentrator about this new contract
  CO_NotifyAddContract(pContract);
  // inform other UAPs about this new contract  
//  UAP_NotifyAddContract(pContract);

// consistency check -> add the contract on NLME if missing
  NLME_AddDmoContract( pContract );
  
  return SFC_SUCCESS; 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Deletes a contract from the dmo contract table
/// @param  p_unContractID  - contractID to be deleted (doesnt matter if pContract is valid)
/// @return servie feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_deleteContract( uint16 p_unContractID )
{
  DMO_CONTRACT_ATTRIBUTE * pContract = DMO_FindContract(p_unContractID);  
  if( !pContract )
  {
      return SFC_INVALID_ELEMENT_INDEX;
  } 

  // notify other UAPs and objects about contract deletion
  if( pContract->m_unSrcTLPort != ISA100_DMAP_PORT ) // is not a DMAP originator contract
  {
      UAP_NotifyContractDeletion(p_unContractID);
  }
  
  g_stDMO.m_ucContractNo --;
  
  DMO_CONTRACT_ATTRIBUTE * pEndTable = g_stDMO.m_aContractTbl+g_stDMO.m_ucContractNo;
  if( g_ucContractRspPending )
  {
      pEndTable++;
  }
  
  // now delete the contract (take care also for the eventually saved data by considering g_ucContractRspPending)
  memmove( pContract, 
          pContract+1, 
          (uint8*)pEndTable-(uint8*)pContract );
    
  // notify DMPA concentrator object about contract deletion
  CO_NotifyContractDeletion(p_unContractID);
  
// consistency check -> remove the contract from NLME  
  p_unContractID = __swap_bytes( p_unContractID );
  NLME_DeleteRow( NLME_ATRBT_ID_CONTRACT_TABLE, 0, (uint8*)&p_unContractID, 2+16 );
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches a contract in the dmo contract table
/// @param  p_unContractID  - contract to be searched
/// @return pointer to contract entry in table or NULL if not found
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
DMO_CONTRACT_ATTRIBUTE * DMO_FindContract(uint16 p_unContractID)
{
  DMO_CONTRACT_ATTRIBUTE * pContract = g_stDMO.m_aContractTbl;
  DMO_CONTRACT_ATTRIBUTE * pEndContract = g_stDMO.m_aContractTbl+g_stDMO.m_ucContractNo;
  
  for( ; pContract < pEndContract; pContract++ )
  {
      if( pContract->m_unContractID == p_unContractID )
          return pContract;
  }
  return NULL;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches a contract in the dmo contract table
/// @param  p_pDstAddr128  - pointer to the 128bit address of destination
/// @param  p_unDstTLPort     - destination TL port
/// @param  p_unSrcTLPort     - source TL port
/// @param  p_ucSrvcType   - service type (0 - periodic; 1 - aperiodic)
/// @return pointer to the contract structure or NULL if not found
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
DMO_CONTRACT_ATTRIBUTE * DMO_GetContract( const uint8 * p_pDstAddr128,
                                          uint16        p_unDstTLPort,
                                          uint16        p_unSrcTLPort,                        
                                          uint8         p_ucSrvcType )

{
  DMO_CONTRACT_ATTRIBUTE * pContract = g_stDMO.m_aContractTbl;
  DMO_CONTRACT_ATTRIBUTE * pEndContract = g_stDMO.m_aContractTbl+g_stDMO.m_ucContractNo;
  
  for( ; pContract < pEndContract; pContract++ )
  {
      if( pContract->m_ucServiceType == p_ucSrvcType 
            && pContract->m_unDstTLPort == p_unDstTLPort
            && pContract->m_unSrcTLPort == p_unSrcTLPort            
            && !memcmp(p_pDstAddr128,
                       pContract->m_aDstAddr128,
                       sizeof(pContract->m_aDstAddr128)) )
      {
          return pContract; 
      }       
  }
  
  //the contract was not found;       
  return NULL;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Reads a DMO attribute
/// @param  p_pAttrIdtf  - pointer to attribute identifier structure
/// @param  p_punBufLen   - in/out param; indicates available space in bufer / size of buffer read params
/// @param  p_pRspBuf     - pointer to response buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_Read( ATTR_IDTF * p_pAttrIdtf,
                uint16 *    p_punBufLen,
                uint8 *     p_pRspBuf)
{   
  
  if ( DMO_CONTRACT_TABLE == p_pAttrIdtf->m_unAttrID )
  {
      DMO_CONTRACT_ATTRIBUTE * p_pContract = DMO_FindContract(p_pAttrIdtf->m_unIndex1); 
      
      if (p_pContract) // contract found
      {
          return DMO_binarizeContract( p_pContract, p_pRspBuf, p_punBufLen );                          
      }
      else
      {
          *p_punBufLen = 0;
          return SFC_INVALID_ELEMENT_INDEX; 
      }
  }
#ifdef DEBUG_RESET_REASON
  else if( DMO_NIVIS_RESET_REASON == p_pAttrIdtf->m_unAttrID )
  {
      memcpy( p_pRspBuf, g_aucDebugResetReason, sizeof(g_aucDebugResetReason) );  
      *p_punBufLen = sizeof(g_aucDebugResetReason);
      return SFC_SUCCESS;      
  }
#endif  
  else if( DMO_NIVIS_MANUFACTURING_DATA == p_pAttrIdtf->m_unAttrID )
  {
      ReadPersistentData( p_pRspBuf, MANUFACTURING_START_ADDR + 10 , 18); 
      *p_punBufLen = 18;
      return SFC_SUCCESS;
  }    
  else if( DMO_NIVIS_PING_INTERVAL == p_pAttrIdtf->m_unAttrID )
  {
      DMAP_InsertUint16( p_pRspBuf, g_stDMO.m_unNeighPingInterval );
      *p_punBufLen = 2;
      return SFC_SUCCESS; 
  }
  return DMAP_ReadAttr( p_pAttrIdtf->m_unAttrID, p_punBufLen, p_pRspBuf, c_aDMOFct, DMO_ATTR_NO );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, ion Ticus
/// @brief  Write a DMO attribute
/// @param  p_unAttrID  - attribute ID
/// @param  p_ucBufferSize   - attribute len
/// @param  p_pucBuffer   - attribute value
/// @return service feedback code
/// @remarks
///      Access level: user level
uint8 DMO_Write(uint16 p_unAttrID, uint8 p_ucBufferSize, const uint8* p_pucBuffer )
{
    if( !g_ucIsOOBAccess ) // is over the air
    {  
        if( p_unAttrID ==  DMO_VENDOR_ID || p_unAttrID == DMO_MODEL_ID || p_unAttrID == DMO_SERIAL_NO )
        {
            return SFC_READ_ONLY_ATTRIBUTE;
        }
    }
    
    if( p_unAttrID == DMO_NIVIS_PING_INTERVAL )
    {
        DMAP_ExtractUint16( p_pucBuffer, &g_stDMO.m_unNeighPingInterval );
        return SFC_SUCCESS;
    }
    
    return DMAP_WriteAttr(p_unAttrID,p_ucBufferSize,p_pucBuffer,c_aDMOFct,DMO_ATTR_NO);    
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Performs execution of a DMO method
/// @param  p_ucMethodID  - method identifier
/// @param  p_unReqLen - input parameters length
/// @param  p_unReqBuf - input parameters buffer
/// @param  p_punRspLen - output parameters length
/// @param  p_pRspBuf - output parameters buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_Execute( uint8          p_ucMethodID,
                   uint8          p_ucReqLen,
                   const uint8 *  p_pReqBuf,
                   uint16 *       p_punRspLen,
                   uint8 *        p_pRspBuf )                                      
{   
  // if generic set/delete method extract attribute ID and TAI
  if( p_ucMethodID == DMO_MODIFY_CONTRACT ||  p_ucMethodID == DMO_TERMINATE_CONTRACT ) 
  {
      if ( p_ucReqLen < 8 ) // 2 bytes (unAttrID) + 4 bytes (TAI cutover) + 2 bytes contract ID 
            return SFC_INVALID_SIZE; 
            
      uint16 unAttrID = ((uint16)p_pReqBuf[0] << 8) | p_pReqBuf[1];
      
      if ( unAttrID != DMO_CONTRACT_TABLE )
            return SFC_INCOMPATIBLE_ATTRIBUTE;  // only contract table attribute is an indexed attribute
                          
      uint16 unContractID = ((uint16)p_pReqBuf[6] << 8) | p_pReqBuf[7]; // contract ID
      
      if( p_ucMethodID == DMO_MODIFY_CONTRACT )
      {            
          return DMO_setContract( unContractID,
                                    0,          // TAI cutover -> ignored
                                    p_pReqBuf  + 8, 
                                    p_ucReqLen - 8 ); 
      }
      return DMO_deleteContract( unContractID );
  }
  
  
  return SFC_INVALID_METHOD;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Issues a contract modification or renewal to the SCO of the system managee
/// @param  p_pContract - pointer to a DMO_CONTRACT_ATTRIBUTE structure that contains renew/modification info
/// @param  p_ucRequestType - specifies the request type: renew or modification;
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_ModifyOrRenewContract ( DMO_CONTRACT_ATTRIBUTE * p_pContract,  
                                  uint8                    p_ucRequestType )
                                  
{
  if ( p_ucRequestType != CONTRACT_REQ_TYPE_MODIFY && p_ucRequestType != CONTRACT_REQ_TYPE_RENEW)
      return SFC_INVALID_ARGUMENT;
    
  // check if there is already a previously issued contract request
  if (g_ucContractRspPending || (g_ucJoinStatus != DEVICE_JOINED))                       
      return SFC_INSUFFICIENT_DEVICE_RESOURCES;  
    
  return DMO_sendContractRequest(p_pContract, p_ucRequestType);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Invocation of the SCO Contract Request method
/// @param  p_ucReqType - contract request type
/// @param  p_unContractId - contract ID, usefull just if p_ucReqType != CONTRACT_REQ_TYPE_NEW  
/// @param  p_pDstAddr128 -  destination long address
/// @param  p_unDstTLPort - destination TL Port
/// @param  p_unSrcTLPort - source TL Port
/// @param  p_ulContractLife 
/// @param  p_ucSrvcType - type of service
/// @param  p_ucPriority - contract priority
/// @param  p_unMaxAPDUSize - maximum payload size
/// @param  p_ucRealibility 
/// @param  p_pBandwidth - bandwidth information structure
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_CONTRACT_REQ_LEN  42

uint8 DMO_RequestContract( uint8         p_ucReqType,
                           uint16        p_unContractId,
                           const uint8 * p_pDstAddr128,
                           uint16        p_unDstTLPort,
                           uint16        p_unSrcTLPort,
                           uint32        p_ulContractLife,
                           uint8         p_ucSrvcType,
                           uint8         p_ucPriority,
                           uint16        p_unMaxAPDUSize, 
                           uint8         p_ucReliability,             
                           uint8         p_ucUAPContractReqID,
                           const DMO_CONTRACT_BANDWIDTH * p_pBandwidth                              
                           )
{   
  // check if there is already a previously issued contract request
  if (g_ucContractRspPending || (g_ucJoinStatus != DEVICE_JOINED))                       
      return SFC_INSUFFICIENT_DEVICE_RESOURCES;

  if( !memcmp( p_pDstAddr128, c_aucInvalidIPV6Addr, sizeof(c_aucInvalidIPV6Addr) ) )
      return SFC_INVALID_DATA;
  
  DMO_CONTRACT_ATTRIBUTE * pContract;
  DMO_CONTRACT_ATTRIBUTE stCrtContract;
  
  if( CONTRACT_REQ_TYPE_NEW == p_ucReqType )
  { 
      // just for safety, check if there is already a contract with these parameters
      pContract = DMO_GetContract( p_pDstAddr128,
                                   p_unDstTLPort,
                                   p_unSrcTLPort,                        
                                   p_ucSrvcType );
  
      if( pContract )
          return SFC_DUPLICATE; // that is not ISA100 but is a safe apro


      // ensure that there is at least one free entry in the contract table  
      if ( g_stDMO.m_ucContractNo >= MAX_CONTRACT_NO )
          return SFC_INSUFFICIENT_DEVICE_RESOURCES; 
      
      // check if this endpoint is banned (there is a previous bad history of contract
      // establishment with this endpoint)
      if( p_ucSrvcType == SRVC_PERIODIC_COMM && g_unBannedEndpointTimer && !memcmp( p_pDstAddr128, g_aucBannedEndpoint, 16 ) )
      {
          return SFC_FAILURE; 
      }
      

      // in order to save memory, save requested contract parameters in the next free contract table entry
      pContract = g_stDMO.m_aContractTbl+g_stDMO.m_ucContractNo;  
      
      if( !(pContract->m_ucServiceType == p_ucSrvcType 
            && pContract->m_unDstTLPort == p_unDstTLPort
            && pContract->m_unSrcTLPort == p_unSrcTLPort            
            && !memcmp(p_pDstAddr128,
                       pContract->m_aDstAddr128,
                       sizeof(pContract->m_aDstAddr128))) ) // not same as previous
      {
          g_ucContractReqID++; // delayed contract response protection
      }
  }
  else
  {
      //contract modification or renewal 
      pContract = DMO_FindContract(p_unContractId);
      
      if( NULL == pContract )
          return SFC_NO_CONTRACT;
      
      //normally the DestinationIPv6, TLSourcePort, TLDestport should not be changed - will be validated by SM
      pContract = &stCrtContract;
      pContract->m_unContractID = p_unContractId;
  }
  
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
  // save the UAP contract request ID
  g_ucUAPContractReqID = p_ucUAPContractReqID;
#endif 
  
  memcpy( pContract->m_aDstAddr128,
         p_pDstAddr128,
         sizeof(pContract->m_aDstAddr128) );
  
  memcpy( &pContract->m_stBandwidth, 
         p_pBandwidth,
         sizeof(DMO_CONTRACT_BANDWIDTH) );
  
  pContract->m_unSrcTLPort   = p_unSrcTLPort;
  pContract->m_unDstTLPort   = p_unDstTLPort;     
  pContract->m_ucServiceType = p_ucSrvcType; 
  pContract->m_ulAssignedExpTime = p_ulContractLife;
  pContract->m_ucPriority = p_ucPriority;
  pContract->m_unAssignedMaxNSDUSize = p_unMaxAPDUSize;
  pContract->m_ucReliability = p_ucReliability;
  
  
  if( CONTRACT_REQ_TYPE_NEW == p_ucReqType )
  {
      if( g_stDSMO.m_ucTLSecurityLevel != SECURITY_NONE )
      {
          // check if there is a valid key for this contract; if key not available, request 
          // a new key before the contract request is sent
          uint8 ucUdpPorts = (uint8)((p_unSrcTLPort << 4) | (p_unDstTLPort & 0x000F));
          uint8 ucTmp;
          
          if( NULL == SLME_FindTxKey( p_pDstAddr128, ucUdpPorts, &ucTmp ) )
          {            
              if (SFC_SUCCESS == SLME_GenerateNewSessionKeyRequest(p_pDstAddr128,ucUdpPorts))
              {
                  g_ucContractRspPending = 2;
                  g_unTimeout   = g_stDMO.m_unContractReqTimeout;
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )   
                  g_ucKeySrcSAP = p_unSrcTLPort - (uint16)ISA100_START_PORTS;
#endif    
                  return SFC_SUCCESS; // now wait for the key; 
              }
              else return SFC_INSUFFICIENT_DEVICE_RESOURCES;
          }
      }
  }
  
  // the key is already available or no TL security ; send the contract request 
  return DMO_sendContractRequest(pContract, p_ucReqType);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Transforms a DMO_CONTRACT_ATTRIBUTE structure intor a SCO contract request
/// @param  p_pContract - pointer to a DMO_CONTRACT_ATTRIBUTE structure
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_sendContractRequest(DMO_CONTRACT_ATTRIBUTE * p_pContract,
                              uint8                    p_ucRequestType)
{  
  uint8 aContractReqBuf[MAX_CONTRACT_REQ_LEN];
  uint8 * pReqBuf = aContractReqBuf;    

  *(pReqBuf++) = g_ucContractReqID;  
  *(pReqBuf++) = p_ucRequestType;
  
  if (p_ucRequestType != CONTRACT_REQ_TYPE_NEW) // contrat renewal or modification
  {
      pReqBuf = DMAP_InsertUint16( pReqBuf, p_pContract->m_unContractID );
  }
  
  *(pReqBuf++) = p_pContract->m_ucServiceType; 
  
  pReqBuf = DMAP_InsertUint16( pReqBuf, p_pContract->m_unSrcTLPort);    
  
  // destination 128 bit address and destination port
  memcpy( pReqBuf, p_pContract->m_aDstAddr128, 16 );
  pReqBuf += 16;
  
  pReqBuf = DMAP_InsertUint16( pReqBuf, p_pContract->m_unDstTLPort);   
  
  *(pReqBuf++) = (uint8)CONTRACT_NEGOTIABLE_REVOCABLE; 
  
  pReqBuf = DMAP_InsertUint32(pReqBuf, p_pContract->m_ulAssignedExpTime);
  
  *(pReqBuf++) = p_pContract->m_ucPriority;  
  
  pReqBuf = DMAP_InsertUint16( pReqBuf, p_pContract->m_unAssignedMaxNSDUSize);  

  *(pReqBuf++) = p_pContract->m_ucReliability; // reliability and publish auto retransmit
  
  pReqBuf =  DMO_InsertBandwidthInfo( pReqBuf, 
                                      &p_pContract->m_stBandwidth, 
                                      p_pContract->m_ucServiceType);    
  EXEC_REQ_SRVC stExecReq;
  stExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
  stExecReq.m_unDstOID = SM_SCO_OBJ_ID;
  stExecReq.m_ucMethID = SCO_REQ_CONTRACT;  
  stExecReq.p_pReqData = aContractReqBuf;
  stExecReq.m_unLen    = pReqBuf - aContractReqBuf;
  
  if (SFC_SUCCESS == ASLSRVC_AddGenericObjectToSM( &stExecReq, SRVC_EXEC_REQ) )
  {
  #if ( _UAP_TYPE == UAP_TYPE_ISA100_API )    
      g_ucContractSrcSAP = p_pContract->m_unSrcTLPort - (uint16)ISA100_START_PORTS;
  #endif    
      g_ucContractReqType = p_ucRequestType;
      g_ucContractRspPending = 1;
      g_unTimeout = g_stDMO.m_unContractReqTimeout;
      return SFC_SUCCESS;  
  }

  return SFC_INSUFFICIENT_DEVICE_RESOURCES;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  
/// @param  
/// @return 
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_NotifyNewKeyAdded( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts )
{
  if (g_ucContractRspPending == 2) // wait for session key to send the contract
  {  
      // if g_ucContractRspPending is set, we have some saved data in the contract table that
      // contains info about a new contract request that was not sent to the System Manager
      DMO_CONTRACT_ATTRIBUTE * pContract = g_stDMO.m_aContractTbl+g_stDMO.m_ucContractNo; 
      
      uint16 unSrcTLPort = 0xF0B0 | (p_ucUdpPorts >> 4);
      uint16 unDstTLPort = 0xF0B0 | (p_ucUdpPorts & 0x0F);
      
      if(     pContract->m_unSrcTLPort == unSrcTLPort 
           && pContract->m_unDstTLPort == unDstTLPort 
           && !memcmp( pContract->m_aDstAddr128, p_pucPeerIPv6Address, 16 )
             )
      {
          // we have a new valid key for our pending contract request; generate the contract request 
          g_ucContractRspPending = 0;
          DMO_sendContractRequest(pContract, CONTRACT_REQ_TYPE_NEW);
      }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Invocation of the SCO Contract Termiantion method on system manager
/// @param  p_unContractID - identifier of the contract that has to be terminated, deactivated or reactivated
/// @param  p_pucOperation - 0=contract_termination, 1=deactivation, 2=reactivation
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMO_RequestContractTermination( uint16 p_unContractID,
                                      uint8  p_ucOperation )
{
  if (g_ucJoinStatus != DEVICE_JOINED)
      return SFC_NO_CONTRACT;
  
  // just for safety, check if there is already a contract with these parameters
  DMO_CONTRACT_ATTRIBUTE * pContract = DMO_FindContract(p_unContractID);
  if (!pContract)
      return SFC_NO_CONTRACT;   
  
  // now start building the request buffer
  uint8 aReqBuf[3];

  aReqBuf[0] = p_unContractID >> 8;
  aReqBuf[1] = p_unContractID;
  aReqBuf[2] = p_ucOperation;
 
  EXEC_REQ_SRVC stExecReq;
  stExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
  stExecReq.m_unDstOID = SM_SCO_OBJ_ID;
  stExecReq.m_ucMethID = SCO_TERMINATE_CONTRACT;  
  stExecReq.p_pReqData = aReqBuf;
  stExecReq.m_unLen    = sizeof(aReqBuf);
  
  return ASLSRVC_AddGenericObjectToSM( &stExecReq, SRVC_EXEC_REQ );

}
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Process the response to a SCO request contract invocation.
/// @param  p_pExecRsp - pointer to the response service structure
/// @return none
/// @remarks  The function does not check inside if the response lenght is appropriate. It is assumed that
///           the System Manager will supply a correct contract response
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_ProcessContractResponse(EXEC_RSP_SRVC* p_pExecRsp)
{  
  const uint8* pucTemp = p_pExecRsp->p_pRspData;
  uint16 unContractID;
  DMO_CONTRACT_ATTRIBUTE * pContract = NULL;     
  
  if( !g_ucContractRspPending )
      return; // not contract on pending
  
  if ( *(pucTemp++) != g_ucContractReqID )
      return; // contract request identifiers does not match; it's not the expected response

  // inform other UAPs about this new contract  
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
  if( g_ucUapId && (g_ucContractSrcSAP == g_ucUapId) ) // it is application processor contract 
  {
      if( SFC_SUCCESS != p_pExecRsp->m_ucSFC )
      {
          uint8 aucTemp[2] = { g_ucUAPContractReqID, (uint8)CONTRACT_RSP_FAIL };           
          UAP_NotifyContractResponse( aucTemp, sizeof(aucTemp) );
      }
      else 
      {
          // overwrite the DMAP_DMO contract req ID with UAP req ID
          uint8 ucBackup = p_pExecRsp->p_pRspData[0]; 
          p_pExecRsp->p_pRspData[0] = g_ucUAPContractReqID; // overwrite the DMAP_DMO contract req ID with UAP req ID
          UAP_NotifyContractResponse( p_pExecRsp->p_pRspData, p_pExecRsp->m_unLen );          
          // restore the original contract request ID
          p_pExecRsp->p_pRspData[0] = ucBackup;
      }
  }
#endif
  
  pucTemp++;    //jump over the Response_Code
  pucTemp = DMAP_ExtractUint16(pucTemp, &unContractID);

  pContract = DMO_FindContract(unContractID);

  //the next contract request must be sent with different Contract Request Id
//  g_ucContractReqID++;
//  g_ucContractRspPending = 0;  
  
  // for now only Response_Code = 0 is implemented
  if( SFC_SUCCESS != p_pExecRsp->m_ucSFC || p_pExecRsp->p_pRspData[1] != CONTRACT_RSP_OK )
  {      
      // this endpoint will be banned for a period of time
      if( CONTRACT_REQ_TYPE_NEW == g_ucContractReqType )
        DMO_NotifyBadEndpoint( ((DMO_CONTRACT_ATTRIBUTE*)(g_stDMO.m_aContractTbl+g_stDMO.m_ucContractNo))->m_aDstAddr128 );    
      else
      {
        if( pContract )
        {
            //the DestIPv6 should not be changed based on the modifcation/renewal contract request
            DMO_NotifyBadEndpoint(pContract->m_aDstAddr128);
        }
      }
      return;     
  }

  //the next contract request must be sent with different Contract Request Id
  g_ucContractReqID++;
  g_ucContractRspPending = 0;  
  
  if( !pContract )
  {
      if ( CONTRACT_REQ_TYPE_NEW != g_ucContractReqType )
          return; 

      if (g_stDMO.m_ucContractNo >= MAX_CONTRACT_NO)
          return; // contract table is full
      
      pContract = g_stDMO.m_aContractTbl + (g_stDMO.m_ucContractNo++);
      
      pContract->m_unContractID = unContractID;
  }

  pContract->m_ucContractStatus = CONTRACT_RSP_OK;
  pContract->m_ucServiceType    = *(pucTemp++); 
  
  pContract->m_ulActivationTAI = 0; // not sent for response code = 0;
  pucTemp = DMAP_ExtractUint32(pucTemp, &pContract->m_ulAssignedExpTime);

  pContract->m_ucPriority = *(pucTemp++); 
  
  pucTemp = DMAP_ExtractUint16(pucTemp, &pContract->m_unAssignedMaxNSDUSize); 
  
  pContract->m_ucReliability = *(pucTemp++);
  
  pucTemp = DMO_ExtractBandwidthInfo(  pucTemp, 
                                       &pContract->m_stBandwidth, 
                                       pContract->m_ucServiceType );     
  
  // inform other DMAP concentrator about this new contract
  CO_NotifyAddContract(pContract);  
  
  // consistency check -> add the contract on NLME if missing
  NLME_AddDmoContract( pContract );  

  ARMO_NotifyAddContract(pContract);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Process the contract received in join response
/// @param  p_pExecRsp - pointer to the response service structure
/// @return none
/// @remarks
///      Access level: user level\n
///      Context: Called when a join response has been received
////////////////////////////////////////////////////////////////////////////////////////////////////

#define FIRST_CONTRACT_BUF_LEN  28

uint8 DMO_ProcessFirstContract( EXEC_RSP_SRVC* p_pExecRsp )          
{
  
  if( SFC_SUCCESS != p_pExecRsp->m_ucSFC )
  {
      g_stJoinRetryCntrl.m_unJoinTimeout = 0;     //force rejoin process
      return SFC_FAILURE;
  }
  
  // check the size of the buffer
  if( p_pExecRsp->m_unLen < FIRST_CONTRACT_BUF_LEN )
      return SFC_FAILURE;  
  
  // this is the first real contract; delete all previous fake contrats
  g_stDMO.m_ucContractNo  = 0;    
  g_stNlme.m_ucContractNo = 0;
  
  // add the first contract to the DMO contract table
  DMO_CONTRACT_ATTRIBUTE * pContract = g_stDMO.m_aContractTbl + (g_stDMO.m_ucContractNo++);
  
  const uint8* pucTemp = p_pExecRsp->p_pRspData;
  pucTemp = DMAP_ExtractUint16(pucTemp, &pContract->m_unContractID);
  
  
  pContract->m_ucContractStatus = CONTRACT_RSP_OK;
  pContract->m_ucServiceType    = SRVC_APERIODIC_COMM;
  pContract->m_ulActivationTAI  = 0;
  pContract->m_unSrcTLPort      = ISA100_DMAP_PORT; 
  
  memcpy(pContract->m_aDstAddr128,
         g_stDMO.m_aucSysMng128BitAddr,
         sizeof(pContract->m_aDstAddr128));
  
  pContract->m_unDstTLPort        = ISA100_SMAP_PORT; 
  pContract->m_ulAssignedExpTime  = (uint32)0xFFFFFFFF;  
  
  pucTemp = DMAP_ExtractUint16(pucTemp, &pContract->m_unAssignedMaxNSDUSize);
  
  pucTemp = DMO_ExtractBandwidthInfo( pucTemp, 
                                      &pContract->m_stBandwidth, 
                                      pContract->m_ucServiceType );   
  

  pContract->m_ucPriority = DMO_PRIORITY_NWK_CTRL;
  
  // add the first contract to NLMO contract table; 
  // access is through write_row; build wrie_row buffer, with doubled indexes
  g_unSysMngContractID = pContract->m_unContractID;
  
  uint8 aucTmpBuf[53];
  aucTmpBuf[0] = pContract->m_unContractID >> 8;
  aucTmpBuf[1] = pContract->m_unContractID;
  memcpy(aucTmpBuf+2, g_stDMO.m_auc128BitAddr, 16);

  //double the index
  memcpy(aucTmpBuf+18, aucTmpBuf, 18); 
  memcpy(aucTmpBuf+36, g_stDMO.m_aucSysMng128BitAddr, 16);
  aucTmpBuf[52] = ((*pucTemp) << 2) | DMO_PRIORITY_NWK_CTRL ; // *pucTemp contains include contract flag
  
  return NLME_SetRow( NLME_ATRBT_ID_CONTRACT_TABLE, 
                                0, 
                                aucTmpBuf,
                                sizeof(aucTmpBuf) );
  
  //the Join Contract Response contains the NLMO Route Entry fileds but these are not used  
  //NL_Next_Hop(IPv6 address), NL_Next_Hop(1 byte), NL_Hops_Limit(1 byte)   
}
             

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Initializes the DMO object
/// @param  - none
/// @return - none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_Init( void )
{
#ifndef BACKBONE_SUPPORT
  memset( &g_stDMO.m_ulUsedDevMem, 0, sizeof(g_stDMO) -  (uint32)&((DMO_ATTRIBUTES*)0)->m_ulUsedDevMem); // protect restart count attribute
  // at each resets, set current IPv6 address as local link address
  *(uint16*)g_stDMO.m_auc128BitAddr = 0x80FE; // local link, starts with FE8000...MACid
  memcpy( g_stDMO.m_auc128BitAddr+8, c_oEUI64BE, 8 );  
  
  g_unPowerUpdatePeriod = 0;    //force updating of the Power Status after stack reset
#else // BACKBONE_SUPPORT

  memset( &g_stDMO.m_ulUptime, 0, sizeof(g_stDMO) - (uint32)&((DMO_ATTRIBUTES*)0)->m_ulUptime ); // set to 0 rest of struct  
  
#endif  
  
  g_stDMO.m_ulUsedDevMem = 0x7100;
  
  g_ucContractRspPending = 0;
  
  g_stDMO.m_ucPWRStatus = PWR_STATUS_LINE;  //  assume that currently the device is line powered

  // other initial default values
  g_stDMO.m_unContractReqTimeout     = (uint16)(DMO_DEF_MAX_CLNT_SRV_RETRY * DMO_DEF_MAX_RETRY_TOUT_INT) ;//(uint16)DMO_DEF_CONTRACT_TOUT;
  g_stDMO.m_ucMaxClntSrvRetries      = (uint8)DMO_DEF_MAX_CLNT_SRV_RETRY;
  g_stDMO.m_unMaxRetryToutInterval   = (uint16)DMO_DEF_MAX_RETRY_TOUT_INT;
  g_stDMO.m_ucJoinCommand            = DMO_JOIN_CMD_NONE;
  g_stDMO.m_unWarmRestartAttemptTout = (uint16)DMO_DEF_WARM_RESTART_TOUT; 
  
#ifdef NON_VOLATILE_MEMORY_SUPPORT  
  // generate the warm restart alarm (only if NonVolatileMemoryCapability = 1)
  if (c_ucNonVolatileMemoryCapable)
  {
      DMO_generateRestartAlarm();
  }
#endif  
  
  g_stDMO.m_unRestartCount++;

  g_unCustomAlarmCounter = CUSTOM_ALERT_PERIOD;  

  g_ucDiscoveryAlertTimer = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Performs operations that need one second timing
/// @param  - none
/// @return - none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_PerformOneSecondTasks( void )
{
  if( g_unBannedEndpointTimer )
  {
      g_unBannedEndpointTimer--;
  }
    
  g_stDMO.m_ulUptime++;
    
  if (g_ucContractRspPending  )
  {      
      if(  g_unTimeout )
      {
          g_unTimeout--;
      }
      else
      {
          #if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
        
          if( (g_ucContractRspPending == 1 && g_ucContractSrcSAP == g_ucUapId)
                || (g_ucContractRspPending == 2 && g_ucKeySrcSAP == g_ucUapId) )
          {
              uint8 aucTemp[2] = { g_ucUAPContractReqID, (uint8)CONTRACT_RSP_FAIL };           
              UAP_NotifyContractResponse( aucTemp, sizeof(aucTemp) );
          }      
          
          #endif // #if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
        
          g_ucContractRspPending = 0;  
      }
  }
  
  if (g_ucJoinStatus == DEVICE_JOINED)
  {
      uint8 ucConnectivityAlarm;                
      
      ucConnectivityAlarm = DLME_CheckChannelDiagAlarm();
      ucConnectivityAlarm |= DLME_CheckNeighborDiagAlarm();
      
      if( ucConnectivityAlarm && (g_ucDiscoveryAlert == 1) && g_stCandidates.m_ucCandidateNo )
      {
          // If there is a dlmo11a.DL_Connectivity alert and dlmo11a.DiscoveryAlert is set to 1, 
          // the DL shall also send a dlmo11a.NeighborDiscovery alert at the same time
          DLME_CheckNeighborDiscoveryAlarm(); 
      }
            
      DMO_checkPowerStatusAlert();        
      DMO_setPingStatus();
      
      // neighbor discovery alert reporting mechanism described at page 307, 334 abd 350 in standard
      if( g_ucDiscoveryAlert > 1)
      {
          if( g_ucDiscoveryAlertTimer == 0 )
          {
              g_ucDiscoveryAlertTimer = g_ucDiscoveryAlert;     
          }
          else if( --g_ucDiscoveryAlertTimer == 0 )
          {
              DLME_CheckNeighborDiscoveryAlarm();
              g_ucDiscoveryAlert = 1;
          }        
      }      
      
      // send custom alert if no alerts have been generated in the last CUSTOM_ALERT_PERIOD seconds
      if( g_unCustomAlarmCounter )
      {
          g_unCustomAlarmCounter--;
      }
      else
      {
          DLME_CheckNeighborDiscoveryAlarm();          
      }          
  }

#ifndef BACKBONE_SUPPORT
  if( g_unPowerUpdatePeriod )
  {
      g_unPowerUpdatePeriod--;
  }
  else
  {
    #if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
      if( NO_ERR == SendMessage(STACK_SPECIFIC, ISA100_POWER_LEVEL, API_REQ, 0, 0, NULL) )
      {
          g_unPowerUpdatePeriod = POWER_STATUS_UPDATE_PERIOD;
      }
    #endif
  }
#endif  //#ifndef BACKBONE_SUPPORT  

  
  if( g_stDMO.m_ucJoinCommand != DMO_JOIN_CMD_NONE ) // a join command is on progress (emulated exec command via write ) 
  {
      //if device not joined accept reset without response sent confirmation - for NIVIS custom provisioning device
      if( g_ucJoinStatus == DEVICE_JOINED && JOIN_CMD_APPLY_ASAP_REQ_ID != g_unJoinCommandReqId )
          return;
      
      //just in case when JoinCommand not generate a StackReset
      g_unJoinCommandReqId = JOIN_CMD_WAIT_REQ_ID;
          
      DMO_PerformJoinCommandReset();
  }  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Decides if custom pings have to be transmitted to clock sources when no coomuniucation 
///         happend in the last pre-set time per
/// @param  - none
/// @return - none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_setPingStatus( void )
{
  MONITOR_ENTER();      
  
  g_ucPingStatus = 0; 
  if( g_stDMO.m_unNeighPingInterval )
  {
      for( unsigned int unIdx = 0; unIdx < DLL_MAX_NEIGH_DIAG_NO; unIdx++ )
      {
          if( g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_unUID 
                && (g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_ucDiagFlags & DLL_MASK_NEICLKSRC) )
          {
              if( g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_unFromLastCommunication > g_stDMO.m_unNeighPingInterval )
              {
                  g_ucPingStatus = 1;
              }
              else
              {
                  g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_unFromLastCommunication++;
              }
          }
      }
  }
  
  MONITOR_EXIT();          
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Tracks changes of the powerstatus attribute, and generates the power status alarm
/// @param  - none
/// @return - none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_checkPowerStatusAlert(void)
{  
  static uint8 ucLastPwrStatus = 0xFF;
  
  if ( ucLastPwrStatus != 0xFF && ucLastPwrStatus != g_stDMO.m_ucPWRStatus && !g_stDMO.m_stPwrAlertDescriptor.m_bAlertReportDisabled)  
  {
      // generate the power status alarm;
      ALERT stAlert;
      uint8 aucAlertBuf[2]; 
      aucAlertBuf[0] = g_stDMO.m_ucPWRStatus;
      stAlert.m_unSize = 1;
      
      stAlert.m_ucPriority      = g_stDMO.m_stPwrAlertDescriptor.m_ucPriority;
      stAlert.m_unDetObjTLPort  = ISA100_DMAP_PORT; // DMO is DMAP port
      stAlert.m_unDetObjID      = DMAP_DMO_OBJ_ID; 
      stAlert.m_ucClass         = ALERT_CLASS_EVENT; 
      stAlert.m_ucDirection     = ALARM_DIR_RET_OR_NO_ALARM; 
      stAlert.m_ucCategory      = ALERT_CAT_COMM_DIAG; 
      stAlert.m_ucType          = DMO_POWER_STATUS_ALARM;              
  
      ARMO_AddAlertToQueue( &stAlert, aucAlertBuf ); 
  }
  
  ucLastPwrStatus = g_stDMO.m_ucPWRStatus;
}

#ifdef NON_VOLATILE_MEMORY_SUPPORT
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Eduard Erdei
    /// @brief  Generates a warm restart alarm
    /// @param  - none
    /// @return - none
    /// @remarks
    ///      Access level: user level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void DMO_generateRestartAlarm(void)
    { 
      ALERT stAlert;
      uint8 aucAlertBuf[2]; // just for safety; no payload for restart alarms
      stAlert.m_unSize = 0;
      
      stAlert.m_ucPriority = 1; // maximum priority
      stAlert.m_unDetObjTLPort  = ISA100_DMAP_PORT; // DMO is DMAP port
      stAlert.m_unDetObjID      = DMAP_DMO_OBJ_ID; 
      stAlert.m_ucClass         = ALERT_CLASS_EVENT; 
      stAlert.m_ucDirection     = ALARM_DIR_RET_OR_NO_ALARM; 
      stAlert.m_ucCategory      = ALERT_CAT_COMM_DIAG; 
      stAlert.m_ucType          = DMO_DEVICE_RESTART_ALARM;              
      
      ARMO_AddAlertToQueue( &stAlert, aucAlertBuf ); 
    }
#endif

void DMO_NotifyCongestion( uint8 p_ucCongestionDirection, APDU_IDTF * p_pstApduIdtf )
{
  // TBD 
}

void DMO_NotifyBadEndpoint( const uint8 * p_pDstAddr128 )
{
    memcpy( g_aucBannedEndpoint, p_pDstAddr128, 16 );
    g_unBannedEndpointTimer = DMO_ENDPOINT_BANNING_PERIOD;
    
    g_unAlarmTimeout = 0; // communication with SM is ok; clean alert timeout
}

void DMO_PerformJoinCommandReset()
{
    switch( g_stDMO.m_ucJoinCommand )
    {                    
        case DMO_JOIN_CMD_RESTART_AS_PROVISIONED:        
#ifndef BACKBONE_SUPPORT
        g_stDPO.m_ucStructSignature = DPO_SIGNATURE;
        DPO_Write( DPO_ALLOW_OVER_THE_AIR_PROVISIONING, 1, &g_stDPO.m_ucAllowOverTheAirProvisioning ); // force rewrite 
#endif            
        DMAP_ResetStack(7);
        break;
        
        case DMO_JOIN_CMD_RESET_FACTORY_DEFAULT:
        {
            DPO_ResetToDefault();
            
            //reset to defaults the application data
            //at loading persistent data, the both CRCs(First + Second) will fail and will start with defaults values     
#if (SECTOR_FLAG)
            EraseSector( FIRST_APPLICATION_SECTOR_NO );
#else
            uint16 unFormatVersion = 0xFFFF;
            WritePersistentData( (uint8*)&unFormatVersion, FIRST_APPLICATION_START_ADDR, sizeof(unFormatVersion) );
#endif                 
            
#if (SECTOR_FLAG)
            EraseSector( SECOND_APPLICATION_SECTOR_NO );
#else    
            WritePersistentData( (uint8*)&unFormatVersion, SECOND_APPLICATION_START_ADDR, sizeof(unFormatVersion) );
#endif
            DMAP_ResetStack(7);
            break;
        }
        
        case DMO_JOIN_CMD_WARM_RESTART:
        DMAP_ResetStack(7);
        break;
        
        case DMO_JOIN_CMD_START:
        break;
        
        case DMO_PROV_RESET_FACTORY_DEFAULT:
        DPO_ResetToDefault();
        DMAP_ResetStack(7);
        break;
        
        case DMO_JOIN_CMD_HARD_RESET:
        while(1)
            ;
    }
}