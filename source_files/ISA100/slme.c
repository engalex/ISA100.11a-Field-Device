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
/// @file       SLME.c
/// @verbatim   
/// Author:       Nivis LLC, Ion Ticus
/// Date:         December 2008
/// Description:  Security Layer Managemenmt Entities
/// Changes:      Created 
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "slme.h"
#include "sfc.h"
#include "string.h"
#include "mlsm.h"
#include "dmap.h"
#include "dmap_armo.h"
#include "../asm.h"

#if  (DEVICE_TYPE == DEV_TYPE_MC13225)  
  #include "uart_link.h"
#endif

uint8           g_ucSLMEKeysNo;
SLME_KEY        g_aKeysTable[MAX_SLME_KEYS_NO];
DSMO_ATTRIBUTES g_stDSMO;

uint8     g_ucMsgIncrement;
uint16    g_unRenewTimeout = 0;  

const DMAP_FCT_STRUCT c_aDSMOFct[DSMO_ATTR_NO] =
{
  { 0, 0                                                , DMAP_EmptyReadFunc     , NULL },     // just for protection; attributeID will match index in this table     
  { ATTR_CONST(g_stDSMO.m_ucProtocolVer)                , DMAP_ReadUint8         , NULL },   //DSMO_PROTOCOL_VER           = 1,
  { ATTR_CONST(g_stDSMO.m_ucDLSecurityLevel)            , DMAP_ReadUint8         , DMAP_WriteUint8 },   //DSMO_DL_SEC_LEVEL           = 2,
  { ATTR_CONST(g_stDSMO.m_ucTLSecurityLevel)            , DMAP_ReadUint8         , DMAP_WriteUint8 },   //DSMO_TL_SEC_LEVEL           = 3,
  { ATTR_CONST(g_stDSMO.m_unJoinTimeout)                , DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_JOIN_TIMEOUT           = 4,
  { ATTR_CONST(g_stDSMO.m_stDLAlert.m_unAttrFailLimit)  , DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_DL_MIC_FAIL_LIMIT      = 5,
  { ATTR_CONST(g_stDSMO.m_stDLAlert.m_unAttrFailPeriod) , DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_DL_MIC_FAIL_PERIOD     = 6,
  { ATTR_CONST(g_stDSMO.m_stTLAlert.m_unAttrFailLimit)  , DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_TL_MIC_FAIL_LIMIT      = 7,
  { ATTR_CONST(g_stDSMO.m_stTLAlert.m_unAttrFailPeriod) , DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_TL_MIC_FAIL_PERIOD     = 8,
  { ATTR_CONST(g_stDSMO.m_stKeyAlert.m_unAttrFailLimit) , DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_KEY_FAIL_LIMIT         = 9,
  { ATTR_CONST(g_stDSMO.m_stKeyAlert.m_unAttrFailPeriod), DMAP_ReadUint16        , DMAP_WriteUint16 },  //DSMO_KEY_FAIL_PERIOD        = 10,
  { ATTR_CONST(g_stDSMO.m_stDLAlert.m_stAlertDesc)      , DMAP_ReadAlertDescriptor, DMAP_WriteAlertDescriptor },  //DSMO_DL_SEC_FAIL_RATE_ALERT = 11,
  { ATTR_CONST(g_stDSMO.m_stTLAlert.m_stAlertDesc)      , DMAP_ReadAlertDescriptor, DMAP_WriteAlertDescriptor },  //DSMO_TL_SEC_FAIL_RATE_ALERT = 12,
  { ATTR_CONST(g_stDSMO.m_stKeyAlert.m_stAlertDesc)     , DMAP_ReadAlertDescriptor, DMAP_WriteAlertDescriptor },  //DSMO_KEY_UPD_FAIL_RATE_ALERT= 13,
  { ATTR_CONST(g_stDSMO.m_unPDUMaxAge)                  , DMAP_ReadUint16        , DMAP_WriteUint16 }  //DSMO_PDU_MAX_AGE            = 14
};  

//global key K_global shall be 0x00490053004100200031003000300000, 
// which is the representation of the null terminated unicode string ISA 100

#pragma data_alignment=4
extern const uint8 c_aulWellKnownISAKey[16] = { 0x00,0x49,0x00,0x53,0x00,0x41,0x00,0x20,0x00,0x31,0x00,0x30,0x00,0x30,0x00,0x00};

void SLME_KeyUpdateTask( void );
uint8 newKeyRequest(const SLME_KEY * p_pKey);
uint8 searchForActiveKey(const SLME_KEY * p_pKey);
uint32 SLME_getTAI(uint32 p_ulKeyTime );


const DSMO_ATTRIBUTES c_stInitDSMO = 
{
    1, //  uint8  m_ucProtocolVer;
    SECURITY_MIC_32, // uint8  m_ucDLSecurityLevel;
    SECURITY_NONE, // uint8  m_ucTLSecurityLevel;
    60, // uint16 m_unJoinTimeout;
    { 0, 0, { 0, 6 }, 5, 60 }, //m_stDLAlert:m_ulFailPeriod,m_unFailCount,m_stAlertDesc,m_unAttrFailLimit,m_unAttrFailPeriod
    { 0, 0, { 0, 6 }, 5, 5 }, //m_stTLAlert:m_ulFailPeriod,m_unFailCount,m_stAlertDesc,m_unAttrFailLimit,m_unAttrFailPeriod
    { 0, 0, { 0, 6 }, 1, 1 }, //m_stKeyAlert:m_ulFailPeriod,m_unFailCount,m_stAlertDesc,m_unAttrFailLimit,m_unAttrFailPeriod

    510 // uint16 m_unPDUMaxAge;      
};

void  SLME_ckAlert( SLME_ALERT_ST * p_pAlert, uint8 p_ucAlertType );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Mircea Vlasin
/// @brief  Provide necessary initializations for security sublayer
/// @param  None
/// @return None
/// @remarks
///      Access level: User\n
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
void SLME_Init(void)
{    
    PHY_AES_ResetKey();
    
    g_ucSLMEKeysNo = 0;
    
    memcpy( &g_stDSMO, &c_stInitDSMO, sizeof(g_stDSMO) );
}
               

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Adrian Simionescu, Ion Ticus, Mircea Vlasin
/// @brief  Add a security key inside the "g_aKeysTable" 
/// @param  p_pucPeerIPv6Address - remote IPv6 address  
/// @param  p_ucUdpPorts - compressed form of IPv6 ports b7b6b5b4 = Src Port, b3b2b1b0- Dest Port
/// @param  p_ucKeyID - security key identifier
/// @param  p_pucKey - key material
/// @param  p_pucIssuerEUI64 - EUI64 address of the key issuer, used in TL decryption
/// @param  p_ulValidNotBefore - period in seconds after the Key becomes valid - offset related current TAI
/// @param  p_ulSoftLifetime - period in seconds after an update key is needed - offset related current TAI 
/// @param  p_ulHardLifetime - period in seconds after the key becomes invalid - offset related current TAI
/// @param  p_ucUsage - key usage(DLL = 0, Session= 1, Master = 2)  
/// @param  p_ucPolicy - key policy
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Context: Used by DMAP when a NewKey request received from the Security Manager
////////////////////////////////////////////////////////////////////////////////////////////////////
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
                   uint8        p_ucPolicy )
{   
  SLME_KEY stTmp;  
  memset( &stTmp, 0, sizeof(stTmp) );  
  
  //Key type is always "Symetric key - unencrypted"
  //Key granularity is always "Seconds"
    
  stTmp.m_ucUdpPorts     = p_ucUdpPorts;
  stTmp.m_ucKeyID        = p_ucKeyID;
  stTmp.m_ulValidNotBefore = p_ulValidNotBefore;
  if( !p_ulValidNotBefore )
  {
      p_ulValidNotBefore = MLSM_GetCrtTaiSec();      
  }
  
  stTmp.m_ulSoftLifetime = (p_ulSoftLifetime ? p_ulValidNotBefore+p_ulSoftLifetime : 0xFFFFFFFF);
  stTmp.m_ulHardLifetime = (p_ulHardLifetime ? p_ulValidNotBefore+p_ulHardLifetime : 0xFFFFFFFF);
  stTmp.m_ucUsage        = p_ucUsage;
  stTmp.m_ucPolicy       = p_ucPolicy;

  if( p_pucPeerIPv6Address )
  {
      memcpy( stTmp.m_aPeerIPv6Address, p_pucPeerIPv6Address, 16 );  
  }
  if( p_pucIssuerEUI64 )
  {
      memcpy( stTmp.m_aIssuerEUI64, p_pucIssuerEUI64, 8 );      
  }
  memcpy( stTmp.m_aKey, p_pucKey, 16 );  

  SLME_DeleteKey( p_pucPeerIPv6Address, p_ucUdpPorts, p_ucKeyID, p_ucUsage );
      
  SLME_KEY * pKey = g_aKeysTable;
  SLME_KEY * pKeyEnd = g_aKeysTable + g_ucSLMEKeysNo;
  for( ; pKey < pKeyEnd; pKey++ )
  {    
      if( pKey->m_ucUsage < p_ucUsage ) continue;
      if( pKey->m_ucUsage > p_ucUsage ) break;
      
//      signed int nCmpResult = memcmp( &stTmp, pKey, 17 ); //address + ports
      
//      if( nCmpResult < 0 ) break;
//      if( nCmpResult > 0 ) continue;
      
      if( stTmp.m_ulHardLifetime > pKey->m_ulHardLifetime )
          break;
  
      if( stTmp.m_ulHardLifetime < pKey->m_ulHardLifetime )
          continue;
  
      if( stTmp.m_ulSoftLifetime > pKey->m_ulSoftLifetime )
          break;
  
      if( stTmp.m_ulSoftLifetime < pKey->m_ulSoftLifetime )
          continue;

      if( p_ucKeyID > pKey->m_ucKeyID )
          break;      
  }

  if( g_ucSLMEKeysNo == MAX_SLME_KEYS_NO )  // if table is full
  {
    return SFC_INSUFFICIENT_DEVICE_RESOURCES; 
  }

  memmove( pKey + 1, pKey, (uint8*)pKeyEnd - (uint8*)pKey );
  memcpy(  pKey, &stTmp, sizeof( SLME_KEY ) );
  
  g_ucSLMEKeysNo++;

  if( p_ucUsage == SLM_KEY_USAGE_DLL ) // DLL key update
  {
      PHY_AES_ResetKey();
  }

#if  (DEVICE_TYPE == DEV_TYPE_MC13225)  
  if( p_pucPeerIPv6Address && p_pucIssuerEUI64  )
  {
     UART_LINK_LocalKeyManagement( 1, &stTmp ); // add
  }  
#endif
  
  g_unRenewTimeout = 0;
  
  return SFC_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Adrian Simionescu, Ion Ticus, Mircea Vlasin
/// @brief  Delete a security key from the "g_aKeysTable" 
/// @param  p_pucPeerIPv6Address - remote IPv6 address  
/// @param  p_ucUdpPorts - compressed form of IPv6 ports b7b6b5b4 = Src Port, b3b2b1b0- Dest Port
/// @param  p_ucKeyID - security key identifier
/// @param  p_ucUsage - key usage(DLL = 0, Session= 1, Master = 2)  
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Context: Used by DMAP when a DeleteKey request received from the Security Manager\n
///               Used to delete the old key when a NewKey request received for an existent key
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 SLME_DeleteKey( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts, uint8  p_ucKeyID, uint8 p_ucUsage )
{
  SLME_KEY* pKey;
  uint8     ucUsage;
    
  if( p_ucUsage == SLM_KEY_USAGE_SESSION )
  {
      pKey = (SLME_KEY*) SLME_FindKey(  p_pucPeerIPv6Address, p_ucUdpPorts, p_ucKeyID, p_ucUsage ); 
  }
  else
  {
      pKey = (SLME_KEY*) SLME_GetNonSessionKey( p_ucKeyID, p_ucUsage ); 
  }
  
  if ( pKey == NULL )  // if KeyID is not found 
  {
    return SFC_INVALID_ELEMENT_INDEX;  
  }  
  
#if  (DEVICE_TYPE == DEV_TYPE_MC13225)  
  UART_LINK_LocalKeyManagement( 0, pKey ); // delete
#endif

  g_ucSLMEKeysNo --;
  
  ucUsage = pKey->m_ucUsage;
  
  memcpy(  pKey
         , pKey + 1
         , (g_aKeysTable + g_ucSLMEKeysNo - pKey) * sizeof(SLME_KEY) );
  
//  memset( g_aKeysTable + g_ucSLMEKeysNo, 0, sizeof(SLME_KEY) ); panaroic security
  
  if( ucUsage == SLM_KEY_USAGE_DLL ) // DLL key update
  {
      PHY_AES_ResetKey();
  }

  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Find a key on keys table
/// @param  p_pucPeerIPv6Address - peer IPv6 address
/// @param  m_ucUdpPorts - compressed form of IPv6 ports b7b6b5b4 = Src Port, b3b2b1b0- Dest Port
/// @param  p_ucKeyID - security key identifier
/// @param  p_ucKeyUsage - key usage(DLL = 0, Session= 1, Master = 2)
/// @return pointer to key structure, NULL if not found
/// @remarks
///      Access level:  user level\n
///      Context: Used by TL to choose the key used for decryption 
////////////////////////////////////////////////////////////////////////////////////////////////////
const SLME_KEY * SLME_FindKey( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts, uint8  p_ucKeyID, uint8 p_ucKeyUsage )
{
  const SLME_KEY * pKey = g_aKeysTable;
  uint8            ucIdx = g_ucSLMEKeysNo;
  
  for( ; ucIdx--; pKey++ )
  {
      if( pKey->m_ucUsage == p_ucKeyUsage && pKey->m_ucUdpPorts == p_ucUdpPorts && pKey->m_ucKeyID == p_ucKeyID && !memcmp(p_pucPeerIPv6Address,pKey->m_aPeerIPv6Address,16) )  
      {
          return pKey;  
      }   
  }
  return NULL;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Find a key on keys table
/// @param  p_pucPeerIPv6Address - peer IPv6 address
/// @param  m_ucUdpPorts - compressed form of IPv6 ports b7b6b5b4 = Src Port, b3b2b1b0- Dest Port
/// @param  p_pucKeyCount - no of keys to that session
/// @return pointer to key structure, NULL if not found
/// @remarks
///      Access level:  user level\n
///      Context: Used by TL to choose the key used for encryption 
////////////////////////////////////////////////////////////////////////////////////////////////////
const SLME_KEY * SLME_FindTxKey( const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts, uint8 * p_pucKeyCount )
{
  const SLME_KEY * pKey = g_aKeysTable;
  uint8            ucIdx = g_ucSLMEKeysNo;
  
  uint32 ulCrtTai = MLSM_GetCrtTaiSec();
  const SLME_KEY * pFoundKey = NULL;
  uint8    ucKeyCount = 0;

  for( ; ucIdx--; pKey++ )
  {
      if(  pKey->m_ucUdpPorts == p_ucUdpPorts && !memcmp(p_pucPeerIPv6Address,pKey->m_aPeerIPv6Address,16) ) 
      {
          if( !pFoundKey )
          {
              if( pKey->m_ulValidNotBefore <= ulCrtTai ) // <= instead < to be safe, even is not exact ISA100 spec
              {
                  pFoundKey = pKey;
              }
          }
          
          if( (++ucKeyCount) > 1 && pFoundKey )
          {
              break;
          }
      }   
  }
  
  *p_pucKeyCount = ucKeyCount;
  return pFoundKey;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Find a non session key on keys table
/// @param  p_unKeyID - security key identifier, 0xFFFF to get the first active key with desired "Usage"  
/// @param  p_ucKeyUsage - key usage(DLL = 0, Session= 1, Master = 2)
/// @return pointer to key structure, NULL if not found
/// @remarks
///      Access level:  user level and interrupt level\n
///      Context: Used by DLL to choose the key used for decryption based on the received message's KeyId\n
///               Used by the DMAP when a NewKey request received
////////////////////////////////////////////////////////////////////////////////////////////////////
const SLME_KEY * SLME_GetNonSessionKey( uint16  p_unKeyID, uint8 p_ucKeyUsage )
{
    const SLME_KEY * pKey = g_aKeysTable;
    uint8            ucIdx = g_ucSLMEKeysNo;
    
    for( ; ucIdx--; pKey++ )
    {
        if( pKey->m_ucUsage == p_ucKeyUsage )  
        {
            if( 0xFFFF == p_unKeyID )
            {
                if( pKey->m_ulValidNotBefore <= g_ulDllTaiSeconds ) // <= instead < to be safe, even is not exact ISA100 spec
                    return pKey;
            }
            else 
            {
                if( pKey->m_ucKeyID == (uint8)p_unKeyID )
                    return pKey;  
            }
        }   
    }
    return NULL;  
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Remove the invalid keys from key table 
/// @param  None
/// @return None
/// @remarks
///      Access level: user level\n
///      Context: executed by 1 second task
////////////////////////////////////////////////////////////////////////////////////////////////////
void SLME_KeyUpdateTask( void )
{
  SLME_ckAlert( &g_stDSMO.m_stDLAlert, 0 );
  SLME_ckAlert( &g_stDSMO.m_stTLAlert, 1 );
  SLME_ckAlert( &g_stDSMO.m_stKeyAlert, 2 );
  
  uint8  ucAllowsRenew = (!g_unRenewTimeout);
  if( ucAllowsRenew )
  {
      g_unRenewTimeout = 600; // try a renew at every 10 minutes
  }
  else
  {
      g_unRenewTimeout--;
  }
  
  uint32 ulCrtTai = MLSM_GetCrtTaiSec();
  
  SLME_KEY * pKey = g_aKeysTable;
  SLME_KEY * pKeyEnd = g_aKeysTable + g_ucSLMEKeysNo;  
  
  for( ; pKey < pKeyEnd; pKey++ )
  {    
      if( pKey->m_ulHardLifetime <= ulCrtTai ) // hard life time expired ... delete the key
      {
          uint8 ucUsage = pKey->m_ucUsage;
          
          g_ucSLMEKeysNo --;
          pKeyEnd--;

          memmove( pKey, pKey+1, (uint8*)pKeyEnd - (uint8*)pKey );
          
          if( ucUsage == SLM_KEY_USAGE_DLL ) // DLL key update
          {
              PHY_AES_ResetKey();
          }
      }
      else if( ucAllowsRenew )
      {           
          if( pKey->m_ulSoftLifetime <= ulCrtTai )
          {
              if( !searchForActiveKey( pKey ) )
              {
                  if( SLME_GenerateNewSessionKeyRequest( pKey->m_aPeerIPv6Address, pKey->m_ucUdpPorts ) == SFC_SUCCESS )
                  {
                      ucAllowsRenew = 0; // don't allows 2 keys renew same time
                  }
                  else
                  {
                      g_unRenewTimeout = 0; // cannot add the key request, try next second
                  }
              }
          }
      }
  }  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Find a DLL key on keys table
/// @param  None
/// @return pointer to key structure, NULL if not found
/// @remarks
///      Access level:  interrupt level\n
///      Context: Used by DLL to choose the key used for encryption
////////////////////////////////////////////////////////////////////////////////////////////////////
const SLME_KEY * SLME_GetDLLTxKey(void)
{
    SLME_KEY * pKey = g_aKeysTable;
    uint8     ucIdx = 0;
    
    do  // first valid key, any key ...
    {
        if( pKey->m_ulValidNotBefore <= g_ulDllTaiSeconds ) // <= instead < to be safe, even is not exact ISA100 spec
        {
            return pKey;
        }
        
        pKey++ ;
    } while( (++ucIdx) < g_ucSLMEKeysNo );
    
    return g_aKeysTable;
}


#define DSMO_MAX_NEW_KEY_SIZE       62 + MIC_SIZE

//KeyUsage(1 byte) + MasterKeyId(1 byte) + Key Id(1 byte) + KeyUdpPorts(4 byte) + IPv6PeerAddr(16bytes) + NonceSubstring(4 bytes) + MIC(4 bytes)
#define DSMO_MAX_DEL_KEY_SIZE       31

//KeyUsage(1 byte) + MasterKeyId(1 byte) + Key Id(1 byte) + KeyUdpPorts(4 byte) + IPv6PeerAddr(16bytes) + SoftLife(1 byte) + NonceSubstring(4 bytes) + MIC(4 bytes)
#define DSMO_KEY_POLIC_UPD_SIZE     32

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Performs execution of a DSMO method
/// @param  p_pstExecReq  - pointer to the Execure Request structure
/// @param  p_pstExecRsp  - pointer to the Execure Response structure
/// @return None
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DSMO_Execute( EXEC_REQ_SRVC * p_pstExecReq, EXEC_RSP_SRVC * p_pstExecRsp )                                      
{  
    union
    {
      uint32 m_ulAligned;
      uint8  m_aucNonce[13]; 
    } stAlignedNonce;
    
    stAlignedNonce.m_aucNonce[12] = 0xFF;
    
    p_pstExecRsp->m_unLen = 0; // response of the execute request will contain only SFC and no data
    
    switch( p_pstExecReq->m_ucMethID )
    {
        case DSMO_NEW_KEY:
        {

            //check the key usage
            uint8 ucKeyUsage = (p_pstExecReq->p_pReqData[0] & POLICY_KEY_USAGE_MASK) >> POLICY_KEY_USAGE_OFF;
            uint8 ucReqSize = DSMO_MAX_NEW_KEY_SIZE;
            if( SLM_KEY_USAGE_DLL == ucKeyUsage || SLM_KEY_USAGE_MASTER == ucKeyUsage)
            {
                ucReqSize -= 28;  //RemoteIPv6, RemoteEUI64, SourcePort + RemotePort   
            }    
            
            //check the granularity
            uint32 ulValidNotBeforeSec;
            uint32 ulHardLifeTimeSec;
            const uint8* pucPolicy = p_pstExecReq->p_pReqData + 1;
            
            switch( p_pstExecReq->p_pReqData[0] & POLICY_KEY_GRAN_MASK )
            {
                case 0x01:      //minute
                    ucReqSize -= 1; 
                    pucPolicy = DMAP_ExtractUint32( pucPolicy, &ulValidNotBeforeSec);
                    ulValidNotBeforeSec *= 60;
                    
                    ulHardLifeTimeSec = ((uint32)*(pucPolicy++) << 16);
                    ulHardLifeTimeSec |= ((uint32)*(pucPolicy++) << 8);
                    ulHardLifeTimeSec |= *(pucPolicy++);
                    ulHardLifeTimeSec *= 60;
                    break;
                case 0x02:      //hour
                    ucReqSize -= 3; 
                    ulValidNotBeforeSec = ((uint32)*(pucPolicy++) << 16);
                    ulValidNotBeforeSec |= ((uint32)*(pucPolicy++) << 8);
                    ulValidNotBeforeSec |= *(pucPolicy++);
                    ulValidNotBeforeSec *= 3600; 
                    
                    ulHardLifeTimeSec = ((uint32)*(pucPolicy++) << 8);
                    ulHardLifeTimeSec |= *(pucPolicy++);
                    ulHardLifeTimeSec *= 3600;
                    break;      
                case 0x03:      //day
                    ucReqSize -= 4; 
                    ulValidNotBeforeSec = ((uint32)*(pucPolicy++) << 8);
                    ulValidNotBeforeSec |= *(pucPolicy++);
                    ulValidNotBeforeSec *= 86400;
                    
                    ulHardLifeTimeSec = ((uint32)*(pucPolicy++) << 8);
                    ulHardLifeTimeSec |= *(pucPolicy++);
                    ulHardLifeTimeSec *= 86400;
                    break;      
                default:
                    pucPolicy = DMAP_ExtractUint32( pucPolicy, &ulValidNotBeforeSec);
                    pucPolicy = DMAP_ExtractUint32( pucPolicy, &ulHardLifeTimeSec);  
                    break;
            }
            
            
            if( p_pstExecReq->m_unLen < ucReqSize )
            {
                p_pstExecRsp->m_ucSFC = SFC_INVALID_SIZE;
                return;
            }
            
            uint8* pucStaticData = p_pstExecReq->p_pReqData + ucReqSize - 28;
            
            //find the Master Key
            const SLME_KEY* pstKey = SLME_GetNonSessionKey( pucStaticData[2], SLM_KEY_USAGE_MASTER );
            if( pstKey )
            {
                //authenticate the message
                memcpy(stAlignedNonce.m_aucNonce, c_oSecManagerEUI64BE, 8);
                memcpy(stAlignedNonce.m_aucNonce + 8, pucStaticData + 3, 4);
                
                if( SECURITY_CTRL_ENC_MIC_32 == 
                    (pucStaticData[1] & (SEC_LEVEL_MASK | SEC_KEY_ID_MODE_MASK)) )
                {
                    //only if Auth+Enc+MIC32
                    if( AES_SUCCESS == AES_Decrypt_User( pstKey->m_aKey,
                                                         stAlignedNonce.m_aucNonce,      
                                                         p_pstExecReq->p_pReqData, 
                                                         ucReqSize - 20,    //Enc Key + MIC32                   
                                                         p_pstExecReq->p_pReqData + ucReqSize - 20,
                                                         16 + MIC_SIZE))
                    {
                        //add the Key 
                        uint8 ucStatus;
                        uint8 ucPorts = 0;
                        const uint8 * pEui64 = NULL;
                        const uint8 * pIpv6 = NULL;
                        
                        if( SLM_KEY_USAGE_MASTER == ucKeyUsage )
                        {
                            pEui64 = c_oSecManagerEUI64BE;
                        }
                        else if( SLM_KEY_USAGE_SESSION == ucKeyUsage )
                        {
                            pIpv6 = pucPolicy + 1 + 10;
                            pEui64 = pucPolicy + 1 + 2;
                            ucPorts = (pucPolicy[2] << 4) | (pucPolicy[28] & 0x0F);
                        }
                        
                        ucStatus = SLME_SetKey( pIpv6, 
                                               ucPorts, // p_ucUdpPorts
                                               pucStaticData[7],    //KeyId
                                               pucStaticData + 8,   //KeyData
                                               pEui64,
                                               ulValidNotBeforeSec,
                                               ulHardLifeTimeSec >> 1,
                                               ulHardLifeTimeSec,
                                               ucKeyUsage,
                                               pucPolicy[0]);
                        
                        if( SFC_SUCCESS == ucStatus )
                        {                            
                            p_pstExecRsp->p_pRspData[0] = SFC_SUCCESS;   
                            
                            if( SLM_KEY_USAGE_SESSION == ucKeyUsage )
                            {
                                DMO_NotifyNewKeyAdded(pIpv6, ucPorts);   //to unblock the contract request sending  
                            }
                        }
                        else
                        {
                            p_pstExecRsp->p_pRspData[0] = SFC_FAILURE;
                        }
                                                
                        
                        //search the first active Master Key - cover also the case when a new active MasterKey was added 
                        pstKey = SLME_GetNonSessionKey( 0xFFFF, SLM_KEY_USAGE_MASTER );
                        
                        if( pstKey )
                        {    
                            p_pstExecRsp->m_unLen = 11;
                            
                            p_pstExecRsp->p_pRspData[1] = (SECURITY_MIC_32 | (KEY_ID_MODE << KEY_ID_MODE_OFFSET));
                            
                            p_pstExecRsp->p_pRspData[2] = pstKey->m_ucKeyID;
                        
                            uint32 ulCrtTai = (MLSM_GetCrtTaiSec() << 10) | g_ucMsgIncrement;
                        
                            memcpy(stAlignedNonce.m_aucNonce, c_oEUI64BE, 8);
                            DMAP_InsertUint32( stAlignedNonce.m_aucNonce + 8, ulCrtTai );
                    
                            memcpy( p_pstExecRsp->p_pRspData + 3, stAlignedNonce.m_aucNonce + 8, 4);
                        
                            AES_Crypt_User(pstKey->m_aKey, stAlignedNonce.m_aucNonce, p_pstExecRsp->p_pRspData, 7, p_pstExecRsp->p_pRspData + 7, 0);
                        
                            p_pstExecRsp->m_ucSFC = SFC_SUCCESS;
                            return;
                        }
                    }
                }
            }
            
            SLME_KeyFailReport();
            p_pstExecRsp->m_ucSFC = SFC_FAILURE;        
            break;
        }
        case DSMO_DELETE_KEY:   
        {
            uint8 ucReqSize = DSMO_MAX_DEL_KEY_SIZE;
                           
            if( SLM_KEY_USAGE_SESSION != p_pstExecReq->p_pReqData[0] )
            {
                ucReqSize -= 2+16+2;
            }
                
            //KeyUsage(1 byte) + MasterKeyId(1 byte) + Key Id(1 byte) + KeyUdpPorts(4 byte) + IPv6PeerAddr(16bytes) + NonceSubstring(4 bytes) + MIC(4 bytes)
            if( p_pstExecReq->m_unLen < ucReqSize )
            {
                p_pstExecRsp->m_ucSFC = SFC_INVALID_SIZE;
                return;
            }
        
            const SLME_KEY* pstKey = SLME_GetNonSessionKey( p_pstExecReq->p_pReqData[1], SLM_KEY_USAGE_MASTER );
            if( pstKey )
            {
                memcpy(stAlignedNonce.m_aucNonce, c_oSecManagerEUI64BE, 8);
                memcpy(stAlignedNonce.m_aucNonce + 8, p_pstExecReq->p_pReqData + ucReqSize - 8, 4);

                if( AES_SUCCESS == AES_Decrypt_User( pstKey->m_aKey,
                                                    stAlignedNonce.m_aucNonce,      
                                                    p_pstExecReq->p_pReqData, 
                                                    ucReqSize - MIC_SIZE,                      
                                                    p_pstExecReq->p_pReqData + ucReqSize - MIC_SIZE,
                                                    MIC_SIZE) )
                {
                    if( SLM_KEY_USAGE_SESSION == p_pstExecReq->p_pReqData[0] )
                    {
                          p_pstExecRsp->m_ucSFC = SLME_DeleteKey(p_pstExecReq->p_pReqData + 5, 
                                                                ((p_pstExecReq->p_pReqData[4] & 0x0F) << 4) | (p_pstExecReq->p_pReqData[22] & 0x0F),   // p_ucUdpPorts,
                                                                 p_pstExecReq->p_pReqData[2],
                                                                 p_pstExecReq->p_pReqData[0]  );
                    }
                    else
                    {
                          p_pstExecRsp->m_ucSFC = SLME_DeleteKey(NULL, // peer ipv6
                                                                0,   // p_ucUdpPorts,
                                                                 p_pstExecReq->p_pReqData[2], // key ID
                                                                 p_pstExecReq->p_pReqData[0]  ); // ussage
                    }
                    return;
                }
            }
            
            SLME_KeyFailReport();
            p_pstExecRsp->m_ucSFC = SFC_FAILURE;
            break;
        }
        case DSMO_KEY_POLICY_UPDATE:  
        {
            uint8 ucReqSize = DSMO_KEY_POLIC_UPD_SIZE;
            
            if( SLM_KEY_USAGE_SESSION != p_pstExecReq->p_pReqData[0] )
            {
                ucReqSize -= 20;
            }
            
            if( p_pstExecReq->m_unLen < ucReqSize )
            {
                p_pstExecRsp->m_ucSFC = SFC_INVALID_SIZE;
                return;
            }
            
            const SLME_KEY* pstKey = SLME_GetNonSessionKey( p_pstExecReq->p_pReqData[1], SLM_KEY_USAGE_MASTER );
            if( pstKey )
            {
                memcpy(stAlignedNonce.m_aucNonce, c_oSecManagerEUI64BE, 8);
                memcpy(stAlignedNonce.m_aucNonce + 8, p_pstExecReq->p_pReqData + ucReqSize - 8, 4);
                
                
                if( AES_SUCCESS == AES_Decrypt_User( pstKey->m_aKey,
                                                    stAlignedNonce.m_aucNonce,      
                                                    p_pstExecReq->p_pReqData, 
                                                    ucReqSize - MIC_SIZE,                      
                                                    p_pstExecReq->p_pReqData + ucReqSize - MIC_SIZE,
                                                    MIC_SIZE) )
                {
                    //choose the key to be updated
                    if( SLM_KEY_USAGE_SESSION != p_pstExecReq->p_pReqData[0] )
                    {
                        pstKey = SLME_GetNonSessionKey( p_pstExecReq->p_pReqData[2], p_pstExecReq->p_pReqData[0] );  
                    }
                    else
                    {
                        pstKey = SLME_FindKey( p_pstExecReq->p_pReqData + 6,
                                              ((p_pstExecReq->p_pReqData[4] & 0x0F) << 4) | (p_pstExecReq->p_pReqData[22] & 0x0F),   // UdpPorts,
                                              p_pstExecReq->p_pReqData[2],      //KeyId
                                              p_pstExecReq->p_pReqData[0]);     //Key Usage  
                    }   
                    
                    if( pstKey )
                    {
                        //update the Key Soft Lifetime
                        //TO DO - update the key
                        p_pstExecRsp->m_ucSFC = SFC_SUCCESS;
                    }
                }
            }
            
            SLME_KeyFailReport();
            p_pstExecRsp->m_ucSFC = SFC_FAILURE;
            break;
        }
        
        default:              
        p_pstExecRsp->m_ucSFC = SFC_INVALID_METHOD;
        break;
    }  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Updates the session keys added during join process
/// @param  p_pucIssuerEUI64 - EUI64 address of the key issuer
/// @param  p_pucPeerIPv6Address - remote IPv6 address
/// @return None
/// @remarks
///      Access level: user level\n
///      Context: The SM's EUI64 and IPv6 are unknown when the joining device received the SecurityJoinResponse\n
///               message(not complete informations for the session keys with the SM)
////////////////////////////////////////////////////////////////////////////////////////////////////
void SLME_UpdateJoinSessionsKeys( const uint8 * p_pucIssuerEUI64, const uint8* p_pucPeerIPv6Address )
{
  SLME_KEY * pKey = g_aKeysTable;
  SLME_KEY * pKeyEnd = g_aKeysTable + g_ucSLMEKeysNo;
  
  for( ; pKey < pKeyEnd; pKey++ )
  {    
      if( pKey->m_ucUsage == SLM_KEY_USAGE_SESSION ) 
      {
          memcpy( pKey->m_aIssuerEUI64, p_pucIssuerEUI64, sizeof(pKey->m_aIssuerEUI64) );
          memcpy( pKey->m_aPeerIPv6Address, p_pucPeerIPv6Address, sizeof(pKey->m_aPeerIPv6Address) );
        #if  (DEVICE_TYPE == DEV_TYPE_MC13225)  
          UART_LINK_LocalKeyManagement( 1, pKey ); // add
        #endif
      }
  }  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Requests a new session key
/// @param  p_pucPeerIPv6Address - IPv6 address key that must be replaced
/// @param  p_ucUdpPorts - ports key that must be replaced
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 SLME_GenerateNewSessionKeyRequest(const uint8* p_pucPeerIPv6Address, uint8  p_ucUdpPorts)
{
  union
    {
      uint32 m_ulAligned;
      uint8  m_aucNonce[13]; 
    } stAlignedNonce;
    
  EXEC_REQ_SRVC stExecReq;
  uint8         aucReqParams[MAX_APDU_SIZE]; 
  uint8* pucBuff = aucReqParams;    
  uint16 unPort;
  
  stExecReq.m_unSrcOID = DMAP_DSMO_OBJ_ID;
  stExecReq.m_unDstOID = SM_PSMO_OBJ_ID;
  stExecReq.m_ucMethID = PSMO_SEC_NEW_SESSION;  
  stExecReq.p_pReqData = aucReqParams;
  
  memcpy(pucBuff, g_stDMO.m_auc128BitAddr, sizeof(g_stDMO.m_auc128BitAddr));
  pucBuff += 16;
  
  unPort = ISA100_START_PORTS | (p_ucUdpPorts >> 4);
  *(pucBuff++) = unPort >> 8;          //SrcPort
  *(pucBuff++) = unPort;
  
  memcpy(pucBuff, p_pucPeerIPv6Address, 16 );
  pucBuff += 16;
  
  unPort = ISA100_START_PORTS | (p_ucUdpPorts & 0x0F);
  *(pucBuff++) = unPort >> 8;          //DstPort
  *(pucBuff++) = unPort;
  
  *(pucBuff++) = 0x01;   //Algorithm_Identifier
  *(pucBuff++) = 0x01;   //Protocol_Version
  *(pucBuff++) = (SECURITY_MIC_32 | (KEY_ID_MODE << KEY_ID_MODE_OFFSET));   //Security_Control

  //find the master key used to authenticate the message
  //the Master Key ID is hardcoded = 0
  const SLME_KEY* pstMasterKey = SLME_GetNonSessionKey( 0, SLM_KEY_USAGE_MASTER );
  
  if( !pstMasterKey )
      return SFC_FAILURE;
  
  *(pucBuff++) = pstMasterKey->m_ucKeyID;    //Key_Identifier
  
  uint32 ulCrtTai = (MLSM_GetCrtTaiSec() << 10) | g_ucMsgIncrement;
                        
  memcpy(stAlignedNonce.m_aucNonce, c_oEUI64BE, 8);
  DMAP_InsertUint32( stAlignedNonce.m_aucNonce + 8, ulCrtTai );
  stAlignedNonce.m_aucNonce[12] = 0xFF;
  
  memcpy( pucBuff, stAlignedNonce.m_aucNonce + 8, 4);            //Time_Stamp
  pucBuff += 4;
                        
  stExecReq.m_unLen = pucBuff - aucReqParams;
  
  AES_Crypt_User(pstMasterKey->m_aKey, 
                 stAlignedNonce.m_aucNonce, 
                 stExecReq.p_pReqData, 
                 stExecReq.m_unLen,
                 stExecReq.p_pReqData + stExecReq.m_unLen,
                 0);    //just authentication
  
  stExecReq.m_unLen += MIC_SIZE;    
  
  return ASLSRVC_AddGenericObjectToSM(&stExecReq, SRVC_EXEC_REQ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  check if exists an active key correspondednt with current key 
/// @param  p_pKey - checked key
/// @return 0 if no key, <>0 if key exists
/// @remarks
///      Access level: user level\n
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 searchForActiveKey(const SLME_KEY * p_pKey)
{
  SLME_KEY * pKey = g_aKeysTable;
  SLME_KEY * pKeyEnd = g_aKeysTable + g_ucSLMEKeysNo;
  
  uint32 ulCrtTai = MLSM_GetCrtTaiSec();
  
  for( ; pKey < pKeyEnd; pKey++ )
  {    
      if(     (ulCrtTai <= pKey->m_ulSoftLifetime)
          &&  (pKey->m_ucUsage == p_pKey->m_ucUsage)
          &&  (pKey->m_ucUdpPorts == p_pKey->m_ucUdpPorts)
          &&  !memcmp( pKey->m_aPeerIPv6Address,  p_pKey->m_aPeerIPv6Address, sizeof(pKey->m_aPeerIPv6Address) ) )
      {
          return 1;
      }
  }  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Add Alert
/// @param  p_pAlert - Alert descriptor
/// @param  p_ucAlertType - SLME alert type
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void  SLME_ckAlert( SLME_ALERT_ST * p_pAlert, uint8 p_ucAlertType )
{  
  if( p_pAlert->m_unFailCount >= p_pAlert->m_unAttrFailLimit )
  {
      if( !p_pAlert->m_stAlertDesc.m_bAlertReportDisabled ) // alert is enabled
      {
          ALERT stAlert;
          uint8 aData[2] = { p_pAlert->m_unFailCount >> 8, p_pAlert->m_unFailCount & 0xFF};
          
          stAlert.m_ucPriority = p_pAlert->m_stAlertDesc.m_ucPriority;
          stAlert.m_unDetObjTLPort = 0xF0B0; // SLME is DMAP port
          stAlert.m_unDetObjID = DMAP_DSMO_OBJ_ID; 
          stAlert.m_ucClass = ALERT_CLASS_EVENT; 
          stAlert.m_ucDirection = ALARM_DIR_IN_ALARM; 
          stAlert.m_ucCategory = ALERT_CAT_SECURITY; 
          stAlert.m_ucType = p_ucAlertType; 
          stAlert.m_unSize = 2; 
          
          ARMO_AddAlertToQueue( &stAlert, aData );
      }
      p_pAlert->m_unFailCount = 0;
  }
   
  if( p_pAlert->m_ulFailPeriod  )
  {
      --p_pAlert->m_ulFailPeriod ;
  }
  else
  {
      p_pAlert->m_ulFailPeriod  = p_pAlert->m_unAttrFailPeriod;
      if( p_ucAlertType == 2 )
      {
          p_pAlert->m_ulFailPeriod  *= 3600L;
      }
      p_pAlert->m_unFailCount = 0;
  } 
}

#define NEW_SESSION_KEY_RESP_SIZE   11    
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Validates a new session key response
/// @param  p_pstExecRsp  - pointer to the ASL Execure Response structure
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DSMO_ValidateNewSessionKeyResponse(EXEC_RSP_SRVC * p_pstExecRsp)
{
    if( p_pstExecRsp->m_unLen < NEW_SESSION_KEY_RESP_SIZE )
        return SFC_TYPE_MISSMATCH;
    
    if( !p_pstExecRsp->p_pRspData[0] )
        return SFC_FAILURE;
    
    if( (SECURITY_MIC_32 | (KEY_ID_MODE << KEY_ID_MODE_OFFSET)) != p_pstExecRsp->p_pRspData[1] )
        return SFC_INVALID_VALUE; //the security level not accepted or the session was not granted
    
    const SLME_KEY* pstMasterKey = SLME_GetNonSessionKey( p_pstExecRsp->p_pRspData[2], SLM_KEY_USAGE_MASTER );
  
    if( !pstMasterKey )
        return SFC_INVALID_VALUE;
    
    union
    {
      uint32 m_ulAligned;
      uint8  m_aucNonce[13]; 
    } stAlignedNonce;
    
    //authenticate the message
    memcpy(stAlignedNonce.m_aucNonce, c_oSecManagerEUI64BE, 8);
    memcpy(stAlignedNonce.m_aucNonce + 8, p_pstExecRsp->p_pRspData + 3, 4);
    stAlignedNonce.m_aucNonce[12] = 0xFF;
    
    if( AES_SUCCESS == AES_Decrypt_User( pstMasterKey->m_aKey,
                                         stAlignedNonce.m_aucNonce,      
                                         p_pstExecRsp->p_pRspData, 
                                         NEW_SESSION_KEY_RESP_SIZE - MIC_SIZE,                      
                                         p_pstExecRsp->p_pRspData + NEW_SESSION_KEY_RESP_SIZE - MIC_SIZE,
                                         MIC_SIZE) )
    {
        return SFC_SUCCESS;    
    }
    
    return SFC_INCONSISTENT_CONTENT;
}

void DSMO_SetJoinTimeout( uint16 p_unJoinTimeout )
{
    if( p_unJoinTimeout >= 2 * 4 * 64 ) // max 4 APP retries on both directions 
    {
        p_unJoinTimeout = 2 * 4 * 64;        
    }
    g_stJoinRetryCntrl.m_unJoinTimeout = p_unJoinTimeout;
}
