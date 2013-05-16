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
/// Author:       Nivis LLC, Ion Ticus
/// Date:         January 2008
/// Description:  Implements transport layer data entity from ISA100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "tlde.h"
#include "tlme.h"
#include "slme.h"
#include "nlde.h"
#include "nlme.h"
#include "aslsrvc.h"
#include "mlde.h"
#include "mlsm.h"
#include "dmap.h"
#include "provision.h"
#include "../asm.h"

#if (DEVICE_TYPE == DEV_TYPE_MC13225)
    #include "uart_link.h"
#endif

#if DUPLICATE_ARRAY_SIZE > 0
    uint8  g_ucDuplicateIdx; 
    uint32 g_aDuplicateHistArray[DUPLICATE_ARRAY_SIZE]; 
    uint8  TLDE_IsDuplicate( const uint8 * p_pMIC );
#else // no duplicate detection support
    #define TLDE_IsDuplicate(...) 0
#endif

#if ( _UAP_TYPE != 0 )
    extern uint8 g_ucUapId;
#endif
    
  typedef struct
  {
      uint8 m_aIpv6SrcAddr[16];    
      uint8 m_aIpv6DstAddr[16];    
      uint8 m_aPadding[3]; 
      uint8 m_ucNextHdr; // 17
      uint8 m_aSrcPort[2];    
      uint8 m_aDstPort[2];   
      uint8 m_aTLSecurityHdr[4];
  } IPV6_PSEUDO_HDR; // encrypted will avoid check sum since is part of MIC

  typedef union
  {
      uint32            m_ulAligned;
      IPV6_PSEUDO_HDR   m_st;
  } IPV6_ALIGNED_PSEUDO_HDR;
  
#pragma pack(1)
  typedef struct
  {
      uint8 m_ucUdpEncoding;    
      uint8 m_ucUdpPorts;    
      uint8 m_aUdpCkSum[2];        
      uint8 m_ucSecurityCtrl;    // must be SECURITY_CTRL_ENC_NONE
      uint8 m_ucTimeAndCounter;    
      uint8 m_ucCounter;    
  } TL_HDR_PLAIN; // plain request mandatory check sum
  
  typedef struct
  {
      uint8 m_ucUdpEncoding;    
      uint8 m_ucUdpPorts;    
      uint8 m_ucSecurityCtrl;   // must be SECURITY_ENC_MIC_32  
      uint8 m_ucTimeAndCounter;    
      uint8 m_ucCounter;    
  } TL_HDR_ENC_SINGLE_KEY; // encrypted will avoid check sum since is part of MIC

  typedef struct
  {
      uint8 m_ucUdpEncoding;    
      uint8 m_ucUdpPorts;    
      uint8 m_ucSecurityCtrl;   // must be SECURITY_CTRL_ENC_MIC_32  
      uint8 m_ucKeyIdx;    
      uint8 m_ucTimeAndCounter;    
      uint8 m_ucCounter;    
  } TL_HDR_ENC_MORE_KEYS; // encrypted will avoid check sum since is part of MIC
  
  typedef union
  {
      TL_HDR_PLAIN          m_stPlain; 
      TL_HDR_ENC_SINGLE_KEY m_stEncOneKey; 
      TL_HDR_ENC_MORE_KEYS  m_stEncMoreKeys; 
  } TL_HDR;
  
#pragma pack()

#define TLDE_TIME_MAX_DIFF 2
  
  
const uint8 c_aLocalLinkIpv6Prefix[8] = {0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8 c_aIpv6ConstPseudoHdr[8] = { 0, 0, 0, UDP_PROTOCOL_CODE, 0xF0, 0xB0, 0xF0, 0xB0 };
//uint8 g_ucMsgIncrement;


uint8 TLDE_decryptTLPayload( const SLME_KEY * p_pKey,
                             uint32   p_ulIssueTime,
                             const IPV6_PSEUDO_HDR * p_pAuth,
                             uint16                  p_unSecHeaderLen,
                             uint16   p_unTLDataLen,
                             void *   p_pTLData );

void TLDE_encryptTLPayload( uint16                  p_unAppDataLen, 
                            void *                  p_pAppData, 
                            const IPV6_PSEUDO_HDR * p_pAuth, 
                            uint16                  p_unSecHeaderLen,
                            const uint8 *           p_pKey, 
                            uint32                  p_ulCrtTai );

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  It is used by the Application Layer to send an packet to one device
/// @param  p_pEUI64DestAddr     - final EUI64 destination
/// @param  p_unContractID       - contract ID
/// @param  p_ucPriorityAndFlags - message priority + DE + ECN
/// @param  p_unAppDataLen  - APP data length
/// @param  p_pAppData      - APP data buffer
/// @param  p_appHandle     - app level handler
/// @param  p_uc4BitSrcPort - source port (16 bit port supported only)
/// @param  p_uc4BitDstPort - destination port (16 bit port supported only)
/// @return none
/// @remarks
///      Access level: user level\n
///      Context: After calling of that function a NLDE_DATA_Confirm has to be received.\n
///      Obs: !!! p_pAppData can be altered by TLDE_DATA_Request and must have size at least TL_MAX_PAYLOAD+16\n
///      On future p_uc4BitSrcPort must be maped via a TSAP id to a port (something like socket id) \n
//       but that is not clear specified on ISA100
////////////////////////////////////////////////////////////////////////////////////////////////////
void TLDE_DATA_Request(   const uint8* p_pEUI64DestAddr,
                          uint16       p_unContractID,
                          uint8        p_ucPriorityAndFlags,
                          uint16       p_unAppDataLen,
                          void *       p_pAppData,
                          HANDLE       p_appHandle,
                          uint8        p_uc4BitSrcPort,
                          uint8        p_uc4BitDstPort )
{
  TL_HDR                  stHdr;
  IPV6_ALIGNED_PSEUDO_HDR stPseudoHdr;
  
  uint16 unHdrLen;
  uint8 ucKeysNo;
  const SLME_KEY * pKey = NULL;
  
  if ( p_pEUI64DestAddr )
  {
      WCI_Log( LOG_M_TL, TLOG_DataReq, 8, p_pEUI64DestAddr,
                                       sizeof(p_unContractID), &p_unContractID,
                                       sizeof(p_ucPriorityAndFlags), &p_ucPriorityAndFlags,
                                       (uint8)p_unAppDataLen,  (uint8 *)p_pAppData,
                                       sizeof(p_appHandle), &p_appHandle,
                                       sizeof(p_uc4BitSrcPort), &p_uc4BitSrcPort,
                                       sizeof(p_uc4BitDstPort), &p_uc4BitDstPort );
  }
  else  // p_pEUI64DestAddr == NULL  
  {
      WCI_Log( LOG_M_TL, TLOG_DataReq, sizeof(p_unContractID), &p_unContractID,
                                       sizeof(p_ucPriorityAndFlags), &p_ucPriorityAndFlags,
                                       (uint8)p_unAppDataLen,  (uint8 *)p_pAppData,
                                       sizeof(p_appHandle), &p_appHandle,
                                       sizeof(p_uc4BitSrcPort), &p_uc4BitSrcPort,
                                       sizeof(p_uc4BitDstPort), &p_uc4BitDstPort );
  }

  memcpy( stPseudoHdr.m_st.m_aPadding, c_aIpv6ConstPseudoHdr, sizeof(c_aIpv6ConstPseudoHdr) ) ;
  stPseudoHdr.m_st.m_aSrcPort[1] |= p_uc4BitSrcPort;
  stPseudoHdr.m_st.m_aDstPort[1] |= p_uc4BitDstPort;

  stHdr.m_stPlain.m_ucUdpPorts = (p_uc4BitSrcPort << 4) | p_uc4BitDstPort; // UDP compresed hdr
  
  if( !p_pEUI64DestAddr ) // search for contract ID
  {
      NLME_CONTRACT_ATTRIBUTES * pContract = NLME_FindContract( p_unContractID );
      if( !pContract ) // contract not found
      {
          TLDE_DATA_Confirm( p_appHandle, SFC_NO_CONTRACT );
          return;
      }

      memcpy( stPseudoHdr.m_st.m_aIpv6DstAddr, pContract->m_aDestAddress, 16 );
      TLME_SetPortInfo( stPseudoHdr.m_st.m_aIpv6DstAddr, *(uint16*)stPseudoHdr.m_st.m_aSrcPort, TLME_PORT_TPDU_OUT );
      
      if( g_stDSMO.m_ucTLSecurityLevel != SECURITY_NONE )
      {
            pKey = SLME_FindTxKey( stPseudoHdr.m_st.m_aIpv6DstAddr, stHdr.m_stPlain.m_ucUdpPorts, &ucKeysNo );
            if( !pKey )
            {
                TLDE_DATA_Confirm( p_appHandle, SFC_NO_KEY );
                return;
            }
      }

      TLME_SetPortInfo( stPseudoHdr.m_st.m_aIpv6DstAddr, *(uint16*)stPseudoHdr.m_st.m_aSrcPort, TLME_PORT_TPDU_OUT_OK );

#ifndef BACKBONE_SUPPORT       
      if( !g_unShortAddr )
      {
          memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr,   c_aLocalLinkIpv6Prefix, 8 );
          memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr+8, c_oEUI64BE, 8 ); 
      }
      else
#endif //  #ifndef BACKBONE_SUPPORT          
      {
          memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr, g_aIPv6Address, 16 );
      }
  }
  else // join response, use local addresses
  {
      memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr,   c_aLocalLinkIpv6Prefix, 8 );
      memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr+8, c_oEUI64BE, 8 ); 
      memcpy( stPseudoHdr.m_st.m_aIpv6DstAddr,   c_aLocalLinkIpv6Prefix, 8 );
      memcpy( stPseudoHdr.m_st.m_aIpv6DstAddr+8, p_pEUI64DestAddr, 8 );
  }

  
  uint32 ulCrtTai = MLSM_GetCrtTaiSec();
  uint8  ucTimeAndCounter = (uint8)((ulCrtTai << 2) | g_stTAI.m_uc250msStep); // add 250ms time sense
  
  g_ucMsgIncrement++;    
          
  if( !pKey ) // not encryption
  {
      stHdr.m_stPlain.m_ucUdpEncoding = UDP_ENCODING_PLAIN;
      stPseudoHdr.m_st.m_aTLSecurityHdr[0] = stHdr.m_stPlain.m_ucSecurityCtrl = SECURITY_NONE;
      stPseudoHdr.m_st.m_aTLSecurityHdr[1] = stHdr.m_stPlain.m_ucTimeAndCounter =  ucTimeAndCounter;
      stPseudoHdr.m_st.m_aTLSecurityHdr[2] = stHdr.m_stPlain.m_ucCounter = g_ucMsgIncrement;          
      stPseudoHdr.m_st.m_aTLSecurityHdr[3] = (p_unAppDataLen ? *((uint8*)p_pAppData) : 0 );        
            
      uint32 ulUdpCkSum = p_unAppDataLen + 3 + 8; // security + mic + UDP header length
      
      // make happy check sum over UDP and add twice udp payload size
      *(uint16*)stPseudoHdr.m_st.m_aPadding = __swap_bytes(ulUdpCkSum);      
      ulUdpCkSum = IcmpInterimChksum( (const uint8*)&stPseudoHdr.m_st, sizeof(stPseudoHdr.m_st), ulUdpCkSum );
            
      if( p_unAppDataLen > 1 )
      {      
          ulUdpCkSum = IcmpInterimChksum( ((uint8*)p_pAppData)+1, p_unAppDataLen-1, ulUdpCkSum );
      }
            
      ulUdpCkSum = IcmpGetFinalCksum( ulUdpCkSum );
      
      stHdr.m_stPlain.m_aUdpCkSum[0] = ulUdpCkSum >> 8;
      stHdr.m_stPlain.m_aUdpCkSum[1] = ulUdpCkSum & 0xFF;
      
      unHdrLen = sizeof( stHdr.m_stPlain );
  }
  else // encryption
  {
      stHdr.m_stPlain.m_ucUdpEncoding = UDP_ENCODING_ENCRYPTED;
      
      if( ucKeysNo > 1 )
      {
          stHdr.m_stEncMoreKeys.m_ucSecurityCtrl = SECURITY_CTRL_ENC_MIC_32;
          stHdr.m_stEncMoreKeys.m_ucKeyIdx = pKey->m_ucKeyID;    
          stHdr.m_stEncMoreKeys.m_ucTimeAndCounter = ucTimeAndCounter; // add 250ms time sense
          stHdr.m_stEncMoreKeys.m_ucCounter = g_ucMsgIncrement;              
            
          unHdrLen = sizeof( stHdr.m_stEncMoreKeys );
      }
      else
      {
          stHdr.m_stEncOneKey.m_ucSecurityCtrl = SECURITY_ENC_MIC_32;
          stHdr.m_stEncOneKey.m_ucTimeAndCounter = ucTimeAndCounter; // add 250ms time sense
          stHdr.m_stEncOneKey.m_ucCounter = g_ucMsgIncrement;              
            
          unHdrLen = sizeof( stHdr.m_stEncOneKey );
      }
      
      memcpy( stPseudoHdr.m_st.m_aTLSecurityHdr, &stHdr.m_stEncMoreKeys.m_ucSecurityCtrl, unHdrLen-2 );
      
      TLDE_encryptTLPayload(p_unAppDataLen, p_pAppData, &stPseudoHdr.m_st, unHdrLen, pKey->m_aKey, ulCrtTai );

      p_unAppDataLen += 4; // add MIC_32 size
  }

  if( p_pEUI64DestAddr ) // MAC
  {
      stPseudoHdr.m_st.m_aIpv6DstAddr[0] = 0xFF; // force to a MAC address
  }
  
  
  NLDE_DATA_RequestMemOptimzed(
                                stPseudoHdr.m_st.m_aIpv6DstAddr,
                                g_unShortAddr,
                                p_unContractID,
                                p_ucPriorityAndFlags,
                                unHdrLen,
                                &stHdr,
                                p_unAppDataLen,
                                p_pAppData,
                                p_appHandle
#if defined(BACKBONE_SUPPORT) 
                                , 1 // to ETH by default
#endif                                  
                                );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  It is invoked by Network Layer to notify about result code of a data request
/// @param  p_hTLHandle - transport layer handler (used to find correspondent request)
/// @param  p_ucStatus  - request status
/// @return none
/// @remarks
///      Access level: Interrupt level\n
///      Context: After any NLDE_DATA_Request
////////////////////////////////////////////////////////////////////////////////////////////////////
void NLDE_DATA_Confirm (  HANDLE       p_hTLHandle,
                          uint8        p_ucStatus )
{
  WCI_Log( LOG_M_NL, NLOG_Confirm, sizeof(p_hTLHandle), &p_hTLHandle, sizeof(p_ucStatus), &p_ucStatus );

#if defined( BACKBONE_SUPPORT ) 
  if( (p_hTLHandle & 0x8000) == 0 ) // from ETH
  {
      uint8 aConfMsg[3];
      aConfMsg[0] = (p_hTLHandle >> 8);
      aConfMsg[1] = (uint8)p_hTLHandle;
      aConfMsg[2] = p_ucStatus;
      
      UART_LINK_AddNwkConf( aConfMsg );
      return;
  }
  p_hTLHandle &= 0x7FFF;
#endif // BACKBONE_SUPPORT
        
  TLDE_DATA_Confirm ( p_hTLHandle, p_ucStatus );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  It is invoked by Network Layer to notify transport layer about new packet
/// @param  p_pSrcAddr  - The IPv6 or EUI64 source address (if first byte is 0xFF -> last 8 bytes are EUI64)  
/// @param  p_unTLDataLen - network layer payload length of received message
/// @param  p_pTLData     - network layer payload data of received message
/// @param  m_ucPriorityAndFlags - message priority + DE + ECN
/// @return none
/// @remarks
///      Access level: User level\n
///      Obs: p_pTLData will be altered on this function
////////////////////////////////////////////////////////////////////////////////////////////////////
void NLDE_DATA_Indication (   const uint8 * p_pSrcAddr,
                              uint16        p_unTLDataLen,
                              void *        p_pTLData,
                              uint8         m_ucPriorityAndFlags )
                              
{
  uint8  ucTimeTraveling;
  const SLME_KEY * pKey = NULL;
  uint32 ulCrtTime;
  
  IPV6_ALIGNED_PSEUDO_HDR stPseudoHdr;

#ifdef WCI_SUPPORT  
    #warning "Assumed src addr len is 16"
    uint8 ucAddrLen = 16;
    uint8 * pSrcAddr = (uint8 *)p_pSrcAddr;
    if ( *pSrcAddr == 0xFF )  // MAC addr ?   
    {
        pSrcAddr += 8;
        ucAddrLen -= 8;
    }
    
    WCI_Log( LOG_M_NL, NLOG_Indicate, ucAddrLen, pSrcAddr,
                                      (uint8)p_unTLDataLen,  (uint8 *)p_pTLData,
                                      sizeof(m_ucPriorityAndFlags), &m_ucPriorityAndFlags );    
#endif // WCI_SUPPORT
    
  memcpy( stPseudoHdr.m_st.m_aPadding, c_aIpv6ConstPseudoHdr, sizeof(c_aIpv6ConstPseudoHdr) ) ;
  stPseudoHdr.m_st.m_aSrcPort[1] |= ((TL_HDR*)p_pTLData)->m_stPlain.m_ucUdpPorts >> 4;
  stPseudoHdr.m_st.m_aDstPort[1] |= ((TL_HDR*)p_pTLData)->m_stPlain.m_ucUdpPorts & 0x0F;;
    
  memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr, p_pSrcAddr, 16 );
  
  if(  p_pSrcAddr[0] == 0xFF ) // MAC address -> join request -> use local address
  {
      memcpy( stPseudoHdr.m_st.m_aIpv6SrcAddr,   c_aLocalLinkIpv6Prefix, 8 );
      memcpy( stPseudoHdr.m_st.m_aIpv6DstAddr,   c_aLocalLinkIpv6Prefix, 8 ); 
      memcpy( stPseudoHdr.m_st.m_aIpv6DstAddr+8, c_oEUI64BE, 8 ); 
  }
  else   
  {
      memcpy( stPseudoHdr.m_st.m_aIpv6DstAddr, g_aIPv6Address, 16 );
      TLME_SetPortInfo( p_pSrcAddr, *(uint16*)stPseudoHdr.m_st.m_aDstPort, TLME_PORT_TPDU_IN );
  }

  if(   (stPseudoHdr.m_st.m_aDstPort[1] & 0x0F) != UAP_DMAP_ID 
#if ( _UAP_TYPE != 0 )
     && (stPseudoHdr.m_st.m_aDstPort[1] & 0x0F) != g_ucUapId  
#endif
       ) // invalid application port
  {
      Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "1", 2, stPseudoHdr.m_st.m_aDstPort );
      TLME_Alert( TLME_ILLEGAL_USE_OF_PORT, stPseudoHdr.m_st.m_aDstPort, 2 );
      return;
  }
  
  ulCrtTime = MLSM_GetCrtTaiSec() + TLDE_TIME_MAX_DIFF;
  ucTimeTraveling = ( ulCrtTime & 0x3F);


  if( ((TL_HDR*)p_pTLData)->m_stPlain.m_ucUdpEncoding == UDP_ENCODING_PLAIN )
  {
      if( p_unTLDataLen <= sizeof(((TL_HDR*)p_pTLData)->m_stPlain) ) 
          return;

      
      if( p_pSrcAddr[0] != 0xFF  ) // valid IPv6 addr -> not MAC addr
      {
          if( g_stDSMO.m_ucTLSecurityLevel != SECURITY_NONE ) // BBR mandatory cut UDP checksum when receive encrypted message on compressed TL header 
          {
              Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "2", 1, &g_stDSMO.m_ucTLSecurityLevel );
              TLME_Alert( TLME_TPDU_OUT_OF_SEC_POL, (uint8*)p_pTLData, (p_unTLDataLen < 40 ? p_unTLDataLen : 40) );
              return;
          }
      }
      
      ucTimeTraveling -= ((TL_HDR*)p_pTLData)->m_stPlain.m_ucTimeAndCounter >> 2;      
      if( ucTimeTraveling & 0x80 )
      {
          ucTimeTraveling += 0x40;
      }
      
      
      // escape control + ports + check sum but add UDP header length      
      uint32 ulUdpCkSum = p_unTLDataLen - 4 + 8 ;
     *(uint16*)stPseudoHdr.m_st.m_aPadding = __swap_bytes(ulUdpCkSum); 
     
      ulUdpCkSum = IcmpInterimChksum( (const uint8*)&stPseudoHdr.m_st, sizeof(stPseudoHdr.m_st)-sizeof(stPseudoHdr.m_st.m_aTLSecurityHdr), ulUdpCkSum );
      
      // compute the checksum on NL payload but escape UDP hdr (control byte, ports, and checksum bytes)
      ulUdpCkSum = IcmpInterimChksum( &((TL_HDR*)p_pTLData)->m_stPlain.m_ucSecurityCtrl, p_unTLDataLen-4, ulUdpCkSum );
      
      ulUdpCkSum = IcmpGetFinalCksum( ulUdpCkSum );
      
      if(     ((TL_HDR*)p_pTLData)->m_stPlain.m_aUdpCkSum[0] != (ulUdpCkSum >> 8)
          ||  ((TL_HDR*)p_pTLData)->m_stPlain.m_aUdpCkSum[1] != (ulUdpCkSum & 0xFF) )
      {
          Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "3", 2, ((TL_HDR*)p_pTLData)->m_stPlain.m_aUdpCkSum );
          return; // invlaid udp check sum
      }
      
      p_pTLData = ((uint8*)p_pTLData) + sizeof( ((TL_HDR*)p_pTLData)->m_stPlain );
      p_unTLDataLen -= sizeof( ((TL_HDR*)p_pTLData)->m_stPlain );
  }
  else if( ((TL_HDR*)p_pTLData)->m_stPlain.m_ucUdpEncoding == UDP_ENCODING_ENCRYPTED )
  {      
      uint8  ucKeyPorts = ((TL_HDR*)p_pTLData)->m_stPlain.m_ucUdpPorts;
      ucKeyPorts = (ucKeyPorts >> 4) | (ucKeyPorts << 4);
      
      uint16 unHeaderLen;
      
      if( ((TL_HDR*)p_pTLData)->m_stEncOneKey.m_ucSecurityCtrl == SECURITY_ENC_MIC_32 )
      {
          unHeaderLen = sizeof(((TL_HDR*)p_pTLData)->m_stEncOneKey);                    
          ucTimeTraveling -= ((TL_HDR*)p_pTLData)->m_stEncOneKey.m_ucTimeAndCounter >> 2;
          
          uint8 ucTmp;
          pKey = SLME_FindTxKey( stPseudoHdr.m_st.m_aIpv6SrcAddr, ucKeyPorts, &ucTmp );
      }
      else if( ((TL_HDR*)p_pTLData)->m_stEncMoreKeys.m_ucSecurityCtrl == SECURITY_CTRL_ENC_MIC_32 )
      {
          unHeaderLen = sizeof(((TL_HDR*)p_pTLData)->m_stEncMoreKeys);
          
          ucTimeTraveling -= ((TL_HDR*)p_pTLData)->m_stEncMoreKeys.m_ucTimeAndCounter >> 2;      
          
          pKey = SLME_FindKey( stPseudoHdr.m_st.m_aIpv6SrcAddr, 
                              ucKeyPorts, 
                              ((TL_HDR*)p_pTLData)->m_stEncMoreKeys.m_ucKeyIdx,
                              SLM_KEY_USAGE_SESSION);
      }
      else
      {
          Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "4" );
          TLME_Alert( TLME_TPDU_OUT_OF_SEC_POL, (uint8*)p_pTLData, (p_unTLDataLen < 40 ? p_unTLDataLen : 40) );
          return;
      }
      
      if( !pKey ) // key not found
      {
          Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "5" );
          TLME_Alert( TLME_TPDU_OUT_OF_SEC_POL, (uint8*)p_pTLData, (p_unTLDataLen < 40 ? p_unTLDataLen : 40) );
          return;
      }

      if( p_unTLDataLen <= (unHeaderLen+4) ) //  +MIC
      {
          Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "6" );
          return;      
      }
      
      if( ucTimeTraveling & 0x80 )
      {
          ucTimeTraveling += 0x40;
      }
      
      memcpy( stPseudoHdr.m_st.m_aTLSecurityHdr, &((TL_HDR*)p_pTLData)->m_stEncMoreKeys.m_ucSecurityCtrl, unHeaderLen-2 );      
      if( AES_SUCCESS != TLDE_decryptTLPayload( pKey, ulCrtTime-ucTimeTraveling, &stPseudoHdr.m_st, unHeaderLen, p_unTLDataLen, p_pTLData ) )
      {
          Log( LOG_ERROR, LOG_M_TL, TLOG_Indicate, 1, "7" );
          SLME_TL_FailReport();
          return; // invalid MIC -> discard it
      }

      if( TLDE_IsDuplicate( (uint8*)p_pTLData + p_unTLDataLen - 4) )
      {
          Log( LOG_INFO, LOG_M_TL, TLOG_Indicate, 1, "8" );
          return;
      }

      p_pTLData = ((uint8*)p_pTLData) + unHeaderLen;
      p_unTLDataLen -= unHeaderLen+4;
  }
  else
      return;
 
  if( ucTimeTraveling > TLDE_TIME_MAX_DIFF )
  {
      ucTimeTraveling -= TLDE_TIME_MAX_DIFF;
  }
  else
  {
      ucTimeTraveling = 0;
  }

  if( p_pSrcAddr[0] != 0xFF  ) // IPV6 addr
  {
      TLME_SetPortInfo( p_pSrcAddr, *(uint16*)stPseudoHdr.m_st.m_aDstPort, TLME_PORT_TPDU_IN_OK );
  }
  
  TLDE_DATA_Indication( p_pSrcAddr,
                        stPseudoHdr.m_st.m_aSrcPort[1] & 0x0F,
                        m_ucPriorityAndFlags,
                        p_unTLDataLen,
                        p_pTLData,
                        stPseudoHdr.m_st.m_aDstPort[1] & 0x0F,
                        ucTimeTraveling );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  decrypt transport layer payload in same buffer
/// @param  p_pKey        - key entry
/// @param  p_ulIssueTime - time when message was issued on remote source
/// @param  p_pAuth       - auth data (IPv6 pseudo header + security header)
/// @param  p_unTLDataLen - transport layer data length
/// @param  p_pTLData     - transport layer data buffer
/// @return none
/// @remarks
///      Access level: User level\n
///      Obs: !! use 1.2k stack !! p_pTLData will be altered on this function
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 TLDE_decryptTLPayload( const SLME_KEY * p_pKey,
                             uint32   p_ulIssueTime,
                             const IPV6_PSEUDO_HDR * p_pAuth,
                             uint16                  p_unSecHeaderLen,
                             uint16   p_unTLDataLen,
                             void *   p_pTLData )
{
    uint8  aNonce[13];
    uint16 unAuthLen = sizeof(IPV6_PSEUDO_HDR) - 4 + p_unSecHeaderLen - 2; //
    
    memcpy(aNonce, p_pKey->m_aIssuerEUI64, 8);    
    aNonce[8] = (uint8)(p_ulIssueTime >> 14);
    aNonce[9] = (uint8)(p_ulIssueTime >> 6);
    aNonce[10] = ((uint8*)p_pAuth)[unAuthLen-2];
    aNonce[11] = ((uint8*)p_pAuth)[unAuthLen-1];
    aNonce[12] = 0xFF; 
    
    // skip UDP section from AUTH because that section may be altered by BBR
    return AES_Decrypt_User( p_pKey->m_aKey,
                                    aNonce,
                                    (uint8*)p_pAuth,
                                    unAuthLen,
                                    ((uint8*)p_pTLData) + p_unSecHeaderLen,
                                    p_unTLDataLen -  p_unSecHeaderLen );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  encrypt transport layer payload in same buffer
/// @param  p_unAppDataLen  - application layer buffer
/// @param  p_pAppData      - application layer data buffer
/// @param  p_pAuth         - auth data (IPv6 pseudo header + security header)
/// @param  p_pKey          - encryption key
/// @param  p_ulCrtTai      - current TAI
/// @return none
/// @remarks
///      Access level: User level
////////////////////////////////////////////////////////////////////////////////////////////////////
void TLDE_encryptTLPayload( uint16 p_unAppDataLen, void * p_pAppData, const IPV6_PSEUDO_HDR * p_pAuth, uint16 p_unSecHeaderLen, const uint8 * p_pKey, uint32 p_ulCrtTai )
{
    uint8  aNonce[13];
    uint16 unAuthLen = sizeof(IPV6_PSEUDO_HDR) - 4 + p_unSecHeaderLen - 2;

    memcpy(aNonce, c_oEUI64BE, 8);
    aNonce[8]  = (uint8)(p_ulCrtTai >> 14);
    aNonce[9]  = (uint8)(p_ulCrtTai >> 6);
    aNonce[10] = ((uint8*)p_pAuth)[unAuthLen-2];
    aNonce[11] = ((uint8*)p_pAuth)[unAuthLen-1];
    aNonce[12] = 0xFF; 

    // skip UDP section from AUTH because that section can be altered by BBR
    AES_Crypt_User( p_pKey,
                    aNonce,
                    (const uint8 *) p_pAuth,
                     unAuthLen,
                     p_pAppData,
                     p_unAppDataLen );
}

#if DUPLICATE_ARRAY_SIZE > 0

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Check if message is duplicate at trasnport layer (on security enable only) 
    /// @param  p_pMIC          - message MIC
    /// @return 1 if is duplicate, 0 if not
    /// @remarks
    ///      Access level: User level\n
    ///       Note: may be a low chance to detect false duplicate but can take that risk since the RAM is a big concern\n
    ///       As an alternative keep time generation + IPV6 (6+16 = 22 bytes instead of 4 bytes)
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8  TLDE_IsDuplicate( const uint8 * p_pMIC ) 
    {
        uint8  ucIdx = 0;
        uint32 ulMic;

        memcpy( &ulMic, p_pMIC, 4 );

        for( ; ucIdx < DUPLICATE_ARRAY_SIZE; ucIdx ++ )
        {
           if( ulMic == g_aDuplicateHistArray[ucIdx] )
               return 1;
        }

        if( (++g_ucDuplicateIdx) >= DUPLICATE_ARRAY_SIZE )
            g_ucDuplicateIdx = 0;

        g_aDuplicateHistArray[g_ucDuplicateIdx] = ulMic;
        return 0;
    }


#endif // no duplicate detection support
