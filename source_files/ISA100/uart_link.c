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
/// Description:  Implements uart link layer
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "../typedef.h"
#include "../uart1.h"
#include "../itc.h"

#include "uart_link.h"
#include "dlde.h"
#include "nlde.h"
#include "nlme.h"
#include "mlde.h"
#include "mlsm.h"
#include "aslsrvc.h"
#include "dmap_dmo.h"
#include "dmap.h"
#include "dmap_utils.h"
#include "../timers.h"
#include "tmr_util.h"

#if defined(BACKBONE_SUPPORT)

  #define UART_TASK_FREQ USEC_TO_FRACTION2( 10000 ) // each 10ms

//  #define TX_ACK_WAIT_TIME  25
  #define TX_ACK_WAIT_TIME  50

  #define TX_MAX_TO_BBR_MSG_NO  25 // 8

  #define TO_BBR_MSG_MAX_SIZE  (UART1_PACKET_LENGTH-3)

  #define MY_ADDR_HI_BYTE (((uint8*)&g_stDMO.m_unShortAddr)[1])
  #define MY_ADDR_LO_BYTE (((uint8*)&g_stDMO.m_unShortAddr)[0])
  #define SM_ADDR_HI_BYTE (((uint8*)&g_stDMO.m_unSysMngShortAddr)[1])
  #define SM_ADDR_LO_BYTE (((uint8*)&g_stDMO.m_unSysMngShortAddr)[0])

  UART_RX_QUEUE g_stUartRx;

  volatile uint8 g_aTxMessagesLen[ TX_MAX_TO_BBR_MSG_NO ];
  uint8 g_aTxMessagesTimer[ TX_MAX_TO_BBR_MSG_NO ];
  uint8 g_aToBBRPayloads[ TX_MAX_TO_BBR_MSG_NO ][TO_BBR_MSG_MAX_SIZE];

  uint16 g_unNextIdlePk = 0;
  

  uint8 UART_LINK_sendMsgToBackbone ( const uint8 * p_pMsg,  uint8 p_ucMsgLen );
  uint8 UART_LINK_parseRXMessage ( uint8 p_ucPayloadLen );

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Reads messages from UART and pending messages to UART
  /// @param  none
  /// @return none
  /// @remarks
  ///      Access level: user level
  ///      Context: 
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void UART_LINK_Task ( void )
  {
    // parse RX message
    static uint32 ulNextRTCCounter = 0;
    
    uint8 ucMsgIdx = g_stUartRx.m_aBuff[0];    
    if( ucMsgIdx )
    {
        if( UART_LINK_parseRXMessage(ucMsgIdx) )
        {        
            MONITOR_ENTER();
            g_stUartRx.m_unBuffLen -= 1+ucMsgIdx;
            memcpy( g_stUartRx.m_aBuff, g_stUartRx.m_aBuff+1+ucMsgIdx, g_stUartRx.m_unBuffLen+1+g_stUartRx.m_ucPkLen );
            MONITOR_EXIT();
        }
    }

    
    // request provisioning data if necessary
    if ( g_ucProvisioned != PROV_STATE_PROVISIONED )     // g_ucProvisioned PROV_STATE_INIT -> PROV_STATE_ON_PROGRESS -> PROV_STATE_PROVISIONED
    {
        if( g_uc250msFlag && !g_stTAI.m_uc250msStep ) // each second
        {      
            uint8 c_aProvReqPk[] = {UART_MSG_TYPE_LOCAL_MNG, 0xFF, 0xFE, g_ucProvisioned == PROV_STATE_INIT ? LOCAL_MNG_RESTARTED_SUBCMD : LOCAL_MNG_PROVISIONING_SUBCMD, 0};
#ifdef DEBUG_RESET_REASON
            c_aProvReqPk[4] = ((uint8)(g_stDMO.m_unRestartCount << 4)) | g_aucDebugResetReason[0];
#else
            c_aProvReqPk[4] = ((uint8)(g_stDMO.m_unRestartCount << 4));
#endif
            
            UART_LINK_sendMsgToBackbone( c_aProvReqPk, sizeof(c_aProvReqPk) );
        }
        
        return;
    }

    if( ulNextRTCCounter - RTC_COUNT <= UART_TASK_FREQ )
    {
        return;
    }
    
    ulNextRTCCounter = RTC_COUNT + UART_TASK_FREQ; 
    
    // send pending TX messages
    for( ucMsgIdx = 0; ucMsgIdx < TX_MAX_TO_BBR_MSG_NO; ucMsgIdx++ )
    {
        uint8 ucMsgLen = g_aTxMessagesLen[ucMsgIdx]; 
        if( ucMsgLen )
        {
            if(  (ucMsgIdx == 0) || (g_aTxMessagesTimer[ucMsgIdx] >= TX_ACK_WAIT_TIME) )
            {
                UART1_RX_INT_DI(); // stop receiving ACK to have consistent data
                if( UART_LINK_sendMsgToBackbone( g_aToBBRPayloads[ucMsgIdx], ucMsgLen ) )
                {                        
                    g_aTxMessagesTimer[ucMsgIdx] = 0;
                    if( ucMsgIdx == 0 ) // confirmation message 
                    {
                          MONITOR_ENTER();
                          
                          ucMsgLen = g_aTxMessagesLen[0] - ucMsgLen;
                          
                          if( !ucMsgLen || ucMsgLen > (sizeof(g_aToBBRPayloads[0])-3) )
                          {
                            g_aTxMessagesLen[0] = 0;
                          }
                          else // an interrupt added new info
                          {
                            memmove( g_aToBBRPayloads[0] + 3, g_aToBBRPayloads[0] + g_aTxMessagesLen[0] - ucMsgLen, ucMsgLen ); 
                            g_aTxMessagesLen[0] = 3+ucMsgLen;
                          }
                          
                          MONITOR_EXIT();
                    }
                      
                }
                    
                
                UART1_RX_INT_EN();
                return;
            }
            else
            {
                g_aTxMessagesTimer[ucMsgIdx] ++;
            }
        }
    }
    
    if( g_ucBufferReadyReq )
    {
        UART1_SendMsg( NULL, 0 ); // null message will force buffer ready message
    }

    if( g_unNextIdlePk )
    {
      g_unNextIdlePk --;
    }
    else
    {
        static const uint8 c_aIdlePk[] = {UART_MSG_TYPE_NWK_T2B,0xFF,0xFF};
        UART_LINK_sendMsgToBackbone( c_aIdlePk, sizeof(c_aIdlePk) );
    }

  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Add a message to be sent to UART
  /// @param  p_unMessageType - message type:  UART_MSG_TYPE_APP, UART_MSG_TYPE_NWK_CONF
  /// @param  p_ucPayloadLen  - message payload length
  /// @param  p_pPayload      - message payload
  /// @return SUCCESS,  OUT_OF_MEMORY, or INVALID_PARAMETER
  /// @remarks
  ///      Access level: user level and interrupt level
  ///      Context: 
  ////////////////////////////////////////////////////////////////////////////////////////////////////
 __monitor uint8 UART_LINK_AddMessage ( uint8 p_unMessageType, 
                                         uint8 p_ucTrafficClass,
                                        uint8 p_ucSrcDstAddrLen, const uint8 *p_pSrcDst, 
                                        uint8 p_ucPayloadLen, const void * p_pPayload )
  {
     unsigned int nIdx;
     uint8 ucPkLen = p_ucPayloadLen;
          
// 1 (msg type), 2 (handler), 1 (traffic class), 2 (src addr), 2 (dst addr), (16)(dst ipv6 addr), msg
      ucPkLen  += 4 + p_ucSrcDstAddrLen;
      if( p_ucSrcDstAddrLen == 16 )
      {
          ucPkLen += 4;
      }
      
      if( ucPkLen > sizeof(g_aToBBRPayloads[0]) )
          return SFC_INVALID_PARAMETER;
      
      
      // begin exclusive section
      MONITOR_ENTER();
      
      for( nIdx = 1; nIdx < TX_MAX_TO_BBR_MSG_NO; nIdx ++ )
      {
          if( g_aTxMessagesLen[nIdx] == 0 ) // buffer empty
          {
              g_aTxMessagesLen[ nIdx ] = ucPkLen;
              g_aTxMessagesTimer[nIdx] = TX_ACK_WAIT_TIME; 
              break;
          }
      }
      
      // end exclusive section        
      MONITOR_EXIT();

      if( nIdx < TX_MAX_TO_BBR_MSG_NO ) // buffer present
      {
          uint8 * pTxMsg = g_aToBBRPayloads[ nIdx ];                          
          pTxMsg[0] = p_unMessageType;
          pTxMsg[1] = nIdx;
//          pTxMsg[2] ++;
extern uint8  s_ucPrevCorrection;
          pTxMsg[2] = ((pTxMsg[2]+1) & 0x03) | (s_ucPrevCorrection << 2 ); // m_nCorrection debug
          pTxMsg[3] = p_ucTrafficClass;
          
          pTxMsg += 4;
          
          if( p_ucSrcDstAddrLen == 16 ) // itself message
          {
              memset( pTxMsg, 0, 4 );
              pTxMsg += 4;
          }
          
          memcpy( pTxMsg, p_pSrcDst, p_ucSrcDstAddrLen );
          memcpy( pTxMsg + p_ucSrcDstAddrLen, p_pPayload, p_ucPayloadLen );

          return SFC_SUCCESS;                
      }
      
#ifdef DEBUG_RESET_REASON
         g_aucDebugResetReason[12]++;
#endif      
      return SFC_INSUFFICIENT_DEVICE_RESOURCES;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Add a hetwork confirm message to be send to UART
  /// @param  p_pPayload - message payload
  /// @return none
  /// @remarks
  ///      Access level: user level and interrupt level
  ///      Context: 
  ////////////////////////////////////////////////////////////////////////////////////////////////////
 __monitor void UART_LINK_AddNwkConf ( const void * p_pPayload )
  {
     MONITOR_ENTER();
      
      uint8 nIdx = g_aTxMessagesLen[0];
      if( !nIdx )
      {
         nIdx = 3;
      }
                    
      if( nIdx < sizeof(g_aToBBRPayloads[0])-3 )
      {               
           g_aToBBRPayloads[0][0] = UART_MSG_TYPE_NWK_CONF;
           g_aToBBRPayloads[0][1] = 0;
           g_aToBBRPayloads[0][2] ++;
                          
           memcpy( (uint8*)(g_aToBBRPayloads[0]) + nIdx, p_pPayload, 3 );
           g_aTxMessagesLen[0] = nIdx+3;
      }
      
      MONITOR_EXIT();
  }
  
  
  uint8 UART_LINK_parseRXMessage ( uint8 p_ucPayloadLen )
  {    
      uint8 ucStatus = TRUE;
      switch( g_stUartRx.m_aBuff[1] )
      {
//  0     1      2                      4                           6               7        
// len | 0x84 | Handle[0] | Handle[1] | Contract[0] | Contract[1] | Traffic Class | SrcAddr[0] | SrcAddr[1] | DstAddr[0] | DstAddr[1] | ...
      case UART_MSG_TYPE_NWK_B2T:
           
              p_ucPayloadLen -= 10; // contract + traffic + short src + short dst
              
              // src and dest  = 0 -> I'm the destination 
              if( 0 == *(uint32*)(g_stUartRx.m_aBuff+7) )
              {
                  p_ucPayloadLen -= 16; // long src addr
                  
                  if( p_ucPayloadLen <= g_ucMaxDSDUSize)
                  {                        
                        // to APP 
                        MONITOR_ENTER();
                        
                        ucStatus = NLDE_DATA_HasSpaceToAPP();
                        
                        if( ucStatus )
                        {
                            DLDE_Data_Indicate( 16,
                                              g_stUartRx.m_aBuff + 11, 
                                              g_stUartRx.m_aBuff[6], //p_ucPriority
                                              p_ucPayloadLen,
                                              g_stUartRx.m_aBuff +  11 + 16 );
    
                            // conf = handle[0] | handle[1] | success
                            g_stUartRx.m_aBuff[4] = SFC_SUCCESS;
                            UART_LINK_AddNwkConf( g_stUartRx.m_aBuff+2 );
                        }
                        
                        MONITOR_EXIT();
                  }
              }
              else // not my address -> forward to RF network
              {
                  uint32 ulAddress = *(uint32*)(g_stUartRx.m_aBuff+7);
                  ulAddress = (ulAddress >> 16) | (ulAddress << 16); 
                                    
                  DLDE_Data_Request( 4,
                                       (uint8*)&ulAddress, // dest address
                                       g_stUartRx.m_aBuff[6], //p_ucPriority,
                                       ((uint16)(g_stUartRx.m_aBuff[4]) << 8) | g_stUartRx.m_aBuff[5], //p_unContractID,
                                       p_ucPayloadLen,
                                       g_stUartRx.m_aBuff+11,
                                       ((uint16)(g_stUartRx.m_aBuff[2]) << 8) | g_stUartRx.m_aBuff[3] );
              }
                
              break;
              
      case UART_MSG_TYPE_LOCAL_MNG:
              DMAP_ParseLocalMng( g_stUartRx.m_aBuff+4, p_ucPayloadLen-3 );       
              
              g_stUartRx.m_aBuff[4] = SFC_SUCCESS;
              UART_LINK_AddNwkConf( g_stUartRx.m_aBuff+2 );
              break;
      }   
       return ucStatus;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Dispatch a complete message received on UART link
  /// @param  p_ucPayloadLen  - message length
  /// @param  p_pPayload      - message payload
  /// @return 1 if need to save on queue, 0 if not
  /// @remarks
  ///      Access level: interrupt level
  ///      Context: 
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UART_LINK_DispatchMessage ( uint8 p_ucPayloadLen, uint8 * p_pPayload )
  {      
      if( p_pPayload[0] ==  UART_MSG_TYPE_ACK )
      {
          if(p_ucPayloadLen >= 10)
          {
              uint8  ucMsgIdx = p_pPayload[1];

              if( ucMsgIdx && ucMsgIdx < TX_MAX_TO_BBR_MSG_NO )
              {
                  if( g_aToBBRPayloads[ucMsgIdx][2] == p_pPayload[2] ) // same counter
                  {
                      g_aTxMessagesLen[ucMsgIdx] = 0;
                  }
              }

              uint32 ulSec;
              uint32 ulSecFraction;
              uint16 unRXDelay;

              // Little endian
              ulSec =   (((uint32)(p_pPayload[3])))
                      | (((uint32)(p_pPayload[4])) << 8)
                      | (((uint32)(p_pPayload[5])) << 16)
                      | (((uint32)(p_pPayload[6])) << 24);

              ulSecFraction =
                        (((uint32)(p_pPayload[7]))) 
                      | (((uint32)(p_pPayload[8])) << 8)
                      | (((uint32)(p_pPayload[9])) << 16);
              
              // consider ulSecFraction < 1 000 000 as true
              ulSecFraction = USEC_TO_FRACTION2( ulSecFraction ); // transform to fraction 2              
              unRXDelay = (RTC_COUNT - g_unLastSTXTime); 

              ulSecFraction += unRXDelay;
              if( ulSecFraction & 0x8000 )
              {
                  ulSecFraction &= 0x7FFF;
                  ulSec ++;
              }

              MLSM_SetAbsoluteTAI( ulSec, ulSecFraction );
          }
          return 0; // don't save ACK
      }
      return 1;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Send a message to UART and prepare to wait for ACK
  /// @param  p_pMsg      - message data
  /// @param  p_ucMsgLen  - message length
  /// @return none
  /// @remarks
  ///      Access level: user level
  ///      Context: 
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UART_LINK_sendMsgToBackbone ( const uint8 * p_pMsg,  uint8 p_ucMsgLen )
  {
    uint8 ucStatus = UART1_SendMsg( p_pMsg, p_ucMsgLen );
    if( ucStatus )
    {
        g_unNextIdlePk = 500; // -> 5 sec for resync
    }
    return ucStatus;
  }

#pragma pack(1)
      typedef struct {
          uint8  m_ucKeyOp;
          uint8  m_ucTLEncryption;
          uint8  m_aPeerIPv6Address[16];
          uint8  m_aUdpSrcPort[2];
          uint8  m_aUdpDstPort[2];
          uint8  m_ucKeyID;
          uint8  m_ucType;
          uint8  m_aKey[16];
          uint8  m_aIssuerEUI64[8];
          uint8  m_aValidNotBefore[4];
          uint8  m_aSoftLifeTime[4];
          uint8  m_aHarsLifeTime[4];
          uint8  m_ucPolicy;
      } LOCAL_KEY_MNG_CMD;
#pragma pack()
  
  void UART_LINK_LocalKeyManagement ( uint8 p_ucAddKey, const SLME_KEY * p_pKey )
  {
      
      uint8 ucCmdSize;
      LOCAL_KEY_MNG_CMD stCmd;

      stCmd.m_ucKeyOp = g_stDSMO.m_ucTLSecurityLevel;      
      memcpy( stCmd.m_aPeerIPv6Address, p_pKey->m_aPeerIPv6Address, sizeof(stCmd.m_aPeerIPv6Address) );
      
      stCmd.m_aUdpSrcPort[0] = 0xF0;
      stCmd.m_aUdpSrcPort[1] = 0xB0 | (p_pKey->m_ucUdpPorts >> 4);

      stCmd.m_aUdpDstPort[0] = 0xF0;
      stCmd.m_aUdpDstPort[1] = 0xB0 | (p_pKey->m_ucUdpPorts & 0x0F);
      
      stCmd.m_ucKeyID = p_pKey->m_ucKeyID;
      stCmd.m_ucType = p_pKey->m_ucUsage;
      
      if( p_ucAddKey == 1)
      {
          stCmd.m_ucKeyOp = 1;
          memcpy( stCmd.m_aKey, p_pKey->m_aKey, sizeof(stCmd.m_aKey) );
          memcpy( stCmd.m_aIssuerEUI64, p_pKey->m_aIssuerEUI64, sizeof(stCmd.m_aIssuerEUI64) );
          
          DMAP_InsertUint32( stCmd.m_aValidNotBefore, p_pKey->m_ulValidNotBefore );
          DMAP_InsertUint32( stCmd.m_aSoftLifeTime, p_pKey->m_ulSoftLifetime );
          DMAP_InsertUint32( stCmd.m_aHarsLifeTime, p_pKey->m_ulHardLifetime );
          
          stCmd.m_ucPolicy = p_pKey->m_ucPolicy;
          
          ucCmdSize = sizeof( stCmd );      
      }
      else
      {
          stCmd.m_ucKeyOp = 2;
          ucCmdSize = 1+1+16+2+2+2;      
      }
      
      UART_LINK_AddMessage( UART_MSG_TYPE_LOCAL_MNG, LOCAL_MNG_KEY_SUBCMD, 0, NULL, ucCmdSize, (uint8*)&stCmd );   
  }
  
#endif // BACKBONE_SUPPORT

