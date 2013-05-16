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
/// Description:  This file holds the definitions of the  DMO object of the DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DMAP_DMO_H_
#define _NIVIS_DMAP_DMO_H_

#include "../typedef.h"
#include "aslde.h"
#include "dmap_utils.h"

#define ISA100_SMAP_PORT 0xF0B1

#define DMO_CONTRACT_BUF_SIZE 41

enum
{
  PWR_STATUS_LINE = 0,    // line powered
  PWR_STATUS_1    = 1,    // battery powered, greater than 75% remaining capacity
  PWR_STATUS_2    = 2,    // battery powered, between 25% and 75% remaining capacity
  PWR_STATUS_3    = 3     // battery powered, less than 25% remaining capacity
}; // POWER_STATUS_VALUES

enum
{
  INCOMING_DATA_ECN = 0,
  OUTGOING_DATA_ECN
}; // CONGESTION_DIRECTION

enum
{
  CONTRACT_REQ_TYPE_NEW     = 0,
  CONTRACT_REQ_TYPE_MODIFY  = 1,
  CONTRACT_REQ_TYPE_RENEW   = 2  
}; // CONTRACT_REQUEST_TYPES;

enum
{
  DMO_PRIORITY_BEST_EFFORT    = 0,
  DMO_PRIORITY_REAL_TIME_SEQ  = 1,  
  DMO_PRIORITY_REAL_TIME_BUF  = 2,
  DMO_PRIORITY_NWK_CTRL       = 3  
}; // CONTRACT_PRIORITY

enum
{
  CONTRACT_RSP_OK                 = 0,  // success with immediate effect
  CONTRACT_RSP_OK_DELAY           = 1,  // success with delayed effect
  CONTRACT_RSP_OK_NEG_DOWN        = 2,  // success with immediate effect but negotiated down
  CONTRACT_RSP_OK_DELAY_NEG_DOWN  = 3,  // success with delayed effect but negotiated down
  CONTRACT_RSP_FAIL               = 4,  // failure with no further guidance
  CONTRACT_RSP_FAIL_BUT_RETRY     = 5,  // failure with retry guidance
  CONTRACT_RSP_FAIL_BUT_RETRY_NEG = 6   // failure with retry and negotiation guidance
  
    
}; // CONTRACT_RESPONSE_CODES

enum
{
  CONTRACT_NEGOTIABLE_REVOCABLE         = 0,
  CONTRACT_NEGOTIABLE_NON_REVOCABLE     = 1,
  CONTRACT_NON_NEGOTIABLE_REVOCABLE     = 2,
  CONTRACT_NON_NEGOTIABLE_NON_REVOCABLE = 3
}; // CONTRACT_NEGOTIABILITY_TYPES

enum
{
   CONTRACT_TERMINATION   = 0,
   CONTRACT_DEACTIVATION  = 1,
   CONTRACT_REACTIVATION  = 2
}; // SCO_CONTRACT_TERMINATION_REQUEST_TYPES

enum
{
  SCO_GET_CONTRACT = 1
}; // SCO_METHOD_IDS

enum
{
  DMO_EUI64                         = 1,
  DMO_16BIT_DL_ALIAS                = 2,
  DMO_128BIT_NWK_ADDR               = 3,
  DMO_DEVICE_ROLE_CAPABILITY        = 4,
  DMO_ASSIGNED_DEVICE_ROLE          = 5,
  DMO_VENDOR_ID                     = 6,
  DMO_MODEL_ID                      = 7,
  DMO_TAG_NAME                      = 8,
  DMO_SERIAL_NO                     = 9,
  DMO_PWR_SUPPLY_STATUS             = 10,
  DMO_PWR_CK_ALERT_DESCRIPTOR       = 11,
  DMO_DMAP_STATE                    = 12,
  DMO_JOIN_COMMAND                  = 13,
  DMO_STATIC_REV_LEVEL              = 14,
  DMO_RESTART_COUNT                 = 15,
  DMO_UPTIME                        = 16,
  DMO_DEV_MEM_TOTAL                 = 17,
  DMO_DEV_MEM_USED                  = 18,
  DMO_TAI_TIME                      = 19,
  DMO_COMM_SW_MAJOR_VERSION         = 20,
  DMO_COMM_SW_MINOR_VERSION         = 21,  
  DMO_SW_REVISION_INFO              = 22,
  DMO_SYS_MNG_128BIT_ADDR           = 23,
  DMO_SYS_MNG_EUI64                 = 24,
  DMO_SYS_MNG_16BIT_ALIAS           = 25,
  DMO_CONTRACT_TABLE                = 26,
  DMO_CONTRACT_REQ_TIMEOUT          = 27,
  DMO_MAX_CLNT_SRV_RETRIES          = 28,
  DMO_MAX_RETRY_TOUT_INTERVAL       = 29,
  DMO_OBJECTS_COUNT                 = 30,
  DMO_OBJECTS_LIST                  = 31,
  DMO_CONTRACT_METADATA             = 32,
  DMO_NON_VOLTATILE_MEM_CAPABLE     = 33,
  DMO_WARM_RESTART_ATTEMPT_TIMEOUT  = 34,

#ifdef BACKBONE_SUPPORT
  DMO_CRT_UTC_DRIFT                 = 35,
  DMO_NEXT_TIME_DRIFT               = 36,
  DMO_NEXT_UTC_DRIFT                = 37,
#endif  

  DMO_ATTR_NO,
  
  // custom attributes
  DMO_NIVIS_RESET_REASON            = 64,         
  DMO_NIVIS_MANUFACTURING_DATA      = 65,
  DMO_NIVIS_DLL_RETRY_INFO          = 66,
  DMO_NIVIS_PING_INTERVAL           = 67
}; // DMO_ATTRIBUTES

enum
{
  DMO_TERMINATE_CONTRACT  = 1,
  DMO_MODIFY_CONTRACT     = 2,  
  DMO_PROXY_SM_JOIN_REQ   = 3, 
  DMO_PROXY_SM_CONTR_REQ  = 4,
  DMO_PROXY_SEC_SYM_REQ   = 5  
};  // DMO_METHODS

enum
{
  DMO_POWER_STATUS_ALARM    = 0,
  DMO_DEVICE_RESTART_ALARM  = 1
}; // DMO_ALARM_TYPES

enum{
  SCO_REQ_CONTRACT        = 1,
  SCO_TERMINATE_CONTRACT  = 2
};  //SCO methods

enum
{
  DMO_JOIN_CMD_NONE                   = 0,
  DMO_JOIN_CMD_START                  = 1,
  DMO_JOIN_CMD_WARM_RESTART           = 2,
  DMO_JOIN_CMD_RESTART_AS_PROVISIONED = 3,
  DMO_JOIN_CMD_RESET_FACTORY_DEFAULT  = 4,
  DMO_JOIN_CMD_HARD_RESET  = 5,
  DMO_PROV_RESET_FACTORY_DEFAULT = 6    
};  //  DMO join command states

typedef union
{
  struct
  {
      int16  m_nPeriod;      
      uint16 m_unDeadline;           
      uint8  m_ucPhase;
  }m_stPeriodic;
  
  struct
  {      
      int16  m_nComittedBurst;  
      int16  m_nExcessBurst;    
      uint8  m_ucMaxSendWindow; 
  }m_stAperiodic;  
  
} DMO_CONTRACT_BANDWIDTH;

typedef struct
{
  uint32    m_ulAssignedExpTime;      // assigned contract expiration time
  uint32    m_ulActivationTAI; 
  uint16    m_unContractID;
  uint16    m_unSrcTLPort;     
  uint16    m_unAssignedMaxNSDUSize;
  uint16    m_unDstTLPort;
  uint8     m_ucContractStatus;
  uint8     m_ucServiceType;  
  uint8     m_ucReliability;          // assigned reliability and publish auto retransmit
  uint8     m_ucPriority;  
  IPV6_ADDR m_aDstAddr128; 
  
  DMO_CONTRACT_BANDWIDTH m_stBandwidth;
  
} DMO_CONTRACT_ATTRIBUTE;


typedef struct
{
  uint16  m_unObjID;
  uint8   m_ucObjType;
  uint8   m_ucObjSubType;
  uint8   m_ucVendorSubType;
  
} DATA_TYPE_OBJ_ID_AND_TYPE;


typedef struct{
  uint16      m_unRestartCount;     // do not move this attribute to other position in struct  
  uint8       m_aucTagName[16];     // do not move this attribute to other position in struct
  uint8       m_aucVendorID[16];    // do not move this attribute to other position in struct
  uint8       m_aucModelID[16];     // do not move this attribute to other position in struct
  uint8       m_aucSerialNo[16];    // do not move this attribute to other position in struct
   
#ifdef BACKBONE_SUPPORT
  uint32       m_ulNextDriftTAI;
  uint16       m_unCrtUTCDrift;
  uint16       m_unNextUTCDrift;
#endif
  
  
  uint32      m_ulUsedDevMem;         // do not move this attribute to other position in struct (used also for alignement)
  IPV6_ADDR   m_auc128BitAddr;        // do not move this attribute to other position in struct
  IPV6_ADDR   m_aucSysMng128BitAddr;  // do not move this attribute to other position in struct
  uint32      m_ulUptime;             // do not move this attribute to other position in struct
  
  SHORT_ADDR  m_unShortAddr;
  SHORT_ADDR  m_unSysMngShortAddr;
  uint8       m_unSysMngEUI64[8];  
  uint16      m_unAssignedDevRole;  
  uint8       m_ucPWRStatus; 
  uint8       m_ucDMAPState;    
  uint32      m_ulStaticRevLevel;       
  uint16      m_unContractReqTimeout;  
  uint16      m_unMaxRetryToutInterval;
  uint16      m_unWarmRestartAttemptTout;
  uint8       m_ucMaxClntSrvRetries; 
  uint8       m_ucJoinCommand;     
  
  uint16      m_unNeighPingInterval;  // custom attribute; used for custom PER detection algorithm by SM

  // DMO contracts table
  uint8                   m_ucContractNo;  
  DMO_CONTRACT_ATTRIBUTE  m_aContractTbl[MAX_CONTRACT_NO]; 
  
  // power status check alert descriptor
  APP_ALERT_DESCRIPTOR    m_stPwrAlertDescriptor;
  
} DMO_ATTRIBUTES;

#define g_ucDmapMaxRetry    g_stDMO.m_ucMaxClntSrvRetries
#define g_unDmapRetryTout   g_stDMO.m_unMaxRetryToutInterval


extern DMO_ATTRIBUTES g_stDMO;
extern const uint8 c_ucCommSWMajorVer;
extern const uint8 c_ucCommSWMinorVer;
extern const uint8 c_aucSWRevInfo[12];

extern uint16 g_unCustomAlarmCounter;    

extern const DMAP_FCT_STRUCT c_aDMOFct[DMO_ATTR_NO];

DMO_CONTRACT_ATTRIBUTE * DMO_FindContract(uint16 p_unContractID);

DMO_CONTRACT_ATTRIBUTE * DMO_GetContract( const uint8 * p_pDstAddr128,
                                          uint16        p_unDstSAP,
                                          uint16        p_unSrcSAP,                        
                                          uint8         p_ucSrvcType );

uint8 DMO_Execute( uint8          p_ucMethodID,
                   uint8          p_ucReqLen,
                   const uint8 *  p_pReqBuf,
                   uint16 *       p_punRspLen,
                   uint8 *        p_pRspBuf );

uint8 DMO_Read( ATTR_IDTF * p_pAttrIdtf,
                uint16 *    p_pBufLen,
                uint8 *     p_pRspBuf);

uint8 DMO_Write(uint16 p_unAttrID, uint8 p_ucBufferSize, const uint8* p_pucBuffer);

void DMO_Init( void );

uint8 DMO_RequestContract( uint8         p_ucReqType,
                           uint16        p_unContractId,
                           const uint8 * p_pDstAddr128,
                           uint16        p_unDstSAP,
                           uint16        p_unSrcSAP,
                           uint32        p_ulContractLife,
                           uint8         p_ucSrvcType,
                           uint8         p_ucPriority,
                           uint16        p_unMaxAPDUSize, 
                           uint8         p_ucRetransmit,              
                           uint8         p_ucUAPContractReqID,
                           const DMO_CONTRACT_BANDWIDTH * p_pBandwidth                              
                           );

uint8 DMO_ModifyOrRenewContract ( DMO_CONTRACT_ATTRIBUTE * p_pContract,  
                                  uint8                    p_ucRequestType );

uint8 DMO_RequestContractTermination( uint16 p_unContractID,
                                      uint8  p_ucOperation );

void DMO_ProcessContractResponse( EXEC_RSP_SRVC* p_pExecRsp );

uint8 DMO_ProcessFirstContract( EXEC_RSP_SRVC* p_pExecRsp );

void DMO_PerformOneSecondTasks( void );

void DMO_NotifyNewKeyAdded( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts );

void DMO_NotifyCongestion( uint8 p_ucCongestionDirection, APDU_IDTF * p_pstApduIdtf );

void DMO_NotifyBadEndpoint( const uint8 * p_pDstAddr128 );

const uint8 * DMO_ExtractBandwidthInfo( const uint8 *            p_pData, 
                                        DMO_CONTRACT_BANDWIDTH * p_pBandwidth,
                                        uint8                    p_ucServiceType );

uint8 * DMO_InsertBandwidthInfo( uint8 *                         p_pData, 
                                 const DMO_CONTRACT_BANDWIDTH *  p_pBandwidth, 
                                 uint8                           p_ucServiceType);

void DMO_PerformJoinCommandReset();

extern const DMAP_FCT_STRUCT c_aDMOFct[DMO_ATTR_NO];


#endif
