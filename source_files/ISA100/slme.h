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
/// @file       SLME.h
/// @verbatim   
/// Author:       Nivis LLC, Ion Ticus
/// Date:         December 2008
/// Description:  Security Layer Managemenmt Entities
/// Changes:      Created 
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_SLME_H_
#define _NIVIS_SLME_H_

#include "config.h"
#include "aslde.h"
#include "dmap_dmo.h"
#include "dmap_utils.h"

#define KEY_ID_MODE_OFFSET   3
#define KEY_ID_MODE          0x01


typedef enum
{
  SECURITY_NONE = 0,
  SECURITY_MIC_32,
  SECURITY_MIC_64,
  SECURITY_MIC_128,
  SECURITY_ENC,
  SECURITY_ENC_MIC_32,
  SECURITY_ENC_MIC_64,
  SECURITY_ENC_MIC_128
  
} DLL_SECURITY_LEVEL;

#define SECURITY_CTRL_ENC_NONE      SECURITY_NONE
#define SECURITY_CTRL_ENC_MIC_32    (SECURITY_ENC_MIC_32 | (KEY_ID_MODE << KEY_ID_MODE_OFFSET))

#define SEC_LEVEL_MASK              0x07
#define SEC_KEY_ID_MODE_MASK        0x18

#define POLICY_KEY_USAGE_MASK       0x1C
#define POLICY_KEY_GRAN_MASK        0x03

#define POLICY_KEY_USAGE_OFF        0x02

typedef enum
{
  SLM_KEY_USAGE_DLL = 0, 
  SLM_KEY_USAGE_SESSION = 1,
  SLM_KEY_USAGE_MASTER = 2,
  SLM_KEY_USAGE_JOIN = 3,
  SLM_KEY_USAGE_PUBLIC = 4,
  SLM_KEY_USAGE_ROOT = 5,
  SLM_KEY_USAGE_RESERVED = 6,
  SLM_KEY_USAGE_GLOBAL = 7  
}SLM_KEY_TYPE;

enum
{
  DSMO_PROTOCOL_VER           = 1,
  DSMO_DL_SEC_LEVEL           = 2,
  DSMO_TL_SEC_LEVEL           = 3,
  DSMO_JOIN_TIMEOUT           = 4,
  DSMO_DL_MIC_FAIL_LIMIT      = 5,
  DSMO_DL_MIC_FAIL_PERIOD     = 6,
  DSMO_TL_MIC_FAIL_LIMIT      = 7,
  DSMO_TL_MIC_FAIL_PERIOD     = 8,
  DSMO_KEY_FAIL_LIMIT         = 9,
  DSMO_KEY_FAIL_PERIOD        = 10,
  DSMO_DL_SEC_FAIL_RATE_ALERT = 11,
  DSMO_TL_SEC_FAIL_RATE_ALERT = 12,
  DSMO_KEY_UPD_FAIL_RATE_ALERT= 13,
  DSMO_PDU_MAX_AGE            = 14,
  DSMO_ATTR_NO                  
}; // DSMO_ATTRIBUTES

typedef struct
{    
    uint32 m_ulFailPeriod;
    uint16 m_unFailCount;
    APP_ALERT_DESCRIPTOR m_stAlertDesc;
    uint16 m_unAttrFailLimit;
    uint16 m_unAttrFailPeriod;  
} SLME_ALERT_ST;

typedef struct{

  uint8         m_ucProtocolVer;
  uint8         m_ucDLSecurityLevel;
  uint8         m_ucTLSecurityLevel;
  uint16        m_unJoinTimeout;
  SLME_ALERT_ST m_stDLAlert; 
  SLME_ALERT_ST m_stTLAlert; 
  SLME_ALERT_ST m_stKeyAlert; 
  uint16        m_unPDUMaxAge;  
} DSMO_ATTRIBUTES;

typedef struct
{
  uint8  m_aPeerIPv6Address[16]; // keep in that order on struct
  uint8  m_ucUdpPorts;          // keep in that order on struct
  uint8  m_ucKeyID;             // keep in that order on struct
  uint8  m_ucUsage;
  uint8  m_ucPolicy;            
  uint32 m_ulValidNotBefore;
  uint32 m_ulSoftLifetime;
  uint32 m_ulHardLifetime;      // alligned to 4
  uint8  m_aKey[16];
  uint8  m_aIssuerEUI64[8];
  uint16 m_unCounter;
  uint16 m_unMICFailures;  
}SLME_KEY;

extern SLME_KEY g_aKeysTable[MAX_SLME_KEYS_NO];
extern uint8  g_ucMsgIncrement;

extern const uint8 c_aulWellKnownISAKey[16];


//7.5.4.3 Protection of join process messages
//As the new device does not have the necessary DL subnet key and a TL level session key
//with the advertising router, all join process messages between the new device and the
//advertising router shall use the global non-secret key at the DL level to construct a 32-bit
//MMIC. At the TL level, the UDP checksum shall be used for these messages

#define g_aJoinAppKey   g_stDPO.m_aJoinKey
#define g_aJoinDllKey   c_aulWellKnownISAKey

void SLME_Init(void);

////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Adrian Simionescu
/// @brief  set a Key in g_aKeysTable
////////////////////////////////////////////////////////////////////////////////
uint8 SLME_SetKey( 
                 const uint8* p_pucPeerIPv6Address, 
                 uint8        p_ucUdpPorts,
                 uint8        p_ucKeyID,
                 const uint8* p_pucKey, 
                 const uint8* p_pucIssuerEUI64, 
                 uint32       p_ulValidNotBefore,
                 uint32       p_ulSoftLifetime,
                 uint32       p_ulHardLifetime,
                 uint8        p_ucUsage, 
                 uint8        p_ucPolicy );

uint8 SLME_DeleteKey( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts, uint8  p_ucKeyID, uint8 p_ucKeyUssage );

const SLME_KEY * SLME_FindKey( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts, uint8  p_ucKeyID, uint8 p_ucKeyUsage);
const SLME_KEY * SLME_FindTxKey( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts, uint8 * p_pucKeyCount );
const SLME_KEY * SLME_GetNonSessionKey( uint16  p_unKeyID, uint8 p_ucKeyUsage );
const SLME_KEY * SLME_GetDLLTxKey(void);
void SLME_KeyUpdateTask( void );

void SLME_UpdateJoinSessionsKeys( const uint8 * p_pucEUI64Issuer, const uint8* p_pucIPv6Addr );
uint8 SLME_GenerateNewSessionKeyRequest(const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts);

extern const DMAP_FCT_STRUCT c_aDSMOFct[DSMO_ATTR_NO];

#define DSMO_Read(p_unAttrID,p_punBufferSize,p_pucRspBuffer) \
            DMAP_ReadAttr(p_unAttrID,p_punBufferSize,p_pucRspBuffer,c_aDSMOFct,DSMO_ATTR_NO)

#define DSMO_Write(p_unAttrID,p_ucBufferSize,p_pucBuffer)   \
            DMAP_WriteAttr(p_unAttrID,p_ucBufferSize,p_pucBuffer,c_aDSMOFct,DSMO_ATTR_NO)

void DSMO_Execute( EXEC_REQ_SRVC * p_pstExecReq, EXEC_RSP_SRVC * p_pstExecRsp );
uint8 DSMO_ValidateNewSessionKeyResponse(EXEC_RSP_SRVC * p_pExecRsp);

extern DSMO_ATTRIBUTES g_stDSMO;

#define SLME_DL_FailReport() g_stDSMO.m_stDLAlert.m_unFailCount++
#define SLME_TL_FailReport() g_stDSMO.m_stTLAlert.m_unFailCount++
#define SLME_KeyFailReport() g_stDSMO.m_stKeyAlert.m_unFailCount++

void DSMO_SetJoinTimeout( uint16 p_unJoinTimeout );
#endif
