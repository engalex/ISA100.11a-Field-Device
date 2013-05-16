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
/// Date:         December 2007
/// Description:  Implements Network Layer Data Entity of ISA100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "nlme.h"
#include "nlde.h"
#include "dlde.h"
#include "mlde.h"
#include "mlsm.h"
#include "tlde.h"
#include "sfc.h"
#include "slme.h"
#include "../uart_link.h"

#define FRAG1_H_SIZE                      4
#define FRAG2_H_SIZE                      5
#define HC1_BASIC_H_SIZE                  1
#define HC1_NO_CONTR_HOPS_DEFAULT_H_SIZE  2
#define HC1_CONTR_HOPS_DEFAULT_H_SIZE     5

#define HC1_DISPATCH_BASIC          0x01 //b00000001
#define HC1_DISPATCH_NO_CONTRACT    0x7C //b01111100 //  011 + 111HH01110111 = 011111HH + 01110111
#define HC1_DISPATCH_WITH_CONTRACT  0x6C //b01101100 //  011 + 011HH01110111 = 011011HH + 01110111
#define HC1_DISPATCH_FRAG1          0xC0 //b11000000
#define HC1_DISPATCH_FRAG2          0xE0 //b11100000

#define HC1_DISPATCH_FRAG_MASK      0xF8 //b11111000
#define HC1_DISPATCH_CONTRACT_MASK  0xEC //b11101100
#define HC1_DISPATCH_HOPS_MASK      0x03 //b00000011

#define HC1_ENCODING_LIMIT          0x77 //b01110111  011 + 011HH01110111 = 011011HH + 01110111



typedef union
{
    struct
    {
        uint8 m_ucHC1Dispatch;
    } m_stBasic;
    struct
    {
        uint8 m_ucHC1Dispatch;
        uint8 m_ucHC1Encoding;
        uint8 m_aFlowLabel[3];
        uint8 m_ucHopLimit;
    } m_stContractEnabled;
    struct
    {
        uint8 m_ucHC1Dispatch;
        uint8 m_ucHC1Encoding;
        uint8 m_ucHopLimit;
    } m_stNoContractEnabled;
} NWK_HEADER_HC1;

typedef struct
{
    uint8           m_aDatagramSize[2];
    uint8           m_aDatagramTag[2];
    NWK_HEADER_HC1 m_stHC1;
} NWK_HEADER_FRAG1;

typedef struct
{
    uint8           m_aDatagramSize[2];
    uint8           m_aDatagramTag[2];
    uint8           m_ucDatagramOffset;
    NWK_HEADER_HC1 m_stHC1;
} NWK_HEADER_FRAG2;

typedef union
{
    NWK_HEADER_HC1    m_stHC1;
    NWK_HEADER_FRAG1  m_stFrag1;
    NWK_HEADER_FRAG2  m_stFrag2;
    
    uint8 m_ucHC1Dispatch;
    uint8 m_aRowData[DLL_MAX_DSDU_SIZE_DEFAULT];
} DLL_PACKET;

#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225) 
  #define MAX_RCV_SRC_ADDR_LEN 16
#else
  #define MAX_RCV_SRC_ADDR_LEN 8
#endif


typedef struct
{
#if defined(BACKBONE_SUPPORT)
  uint8  m_unContractID;
#endif    
  uint8 m_ucPkLen;
  uint8 m_aPkBuffer[1+DLL_MAX_DSDU_SIZE_DEFAULT]; // NL dispatch + NL payload
  uint8 m_ucPriorityAndFlags;
  uint8 m_ucSrcDstAddrLen;
  uint8 m_aSrcDstAddr[MAX_RCV_SRC_ADDR_LEN];
} NLDE_RCV_PACKET;

NLDE_RCV_PACKET g_stRcvPacket;

#if defined( NL_FRAG_SUPPORT )

  typedef struct
  {
      uint16 m_hTLHandler;
      uint16 m_unContractID;
      uint16 m_unPayloadLen;
      uint16 m_unPayloadOffset;
      uint16 m_unDatagramSize;
      uint8  m_aPayload[MAX_DATAGRAM_SIZE];
      uint8  m_aDstSrcAddr[8];
      uint8  m_ucDstSrcAddrLen;
      uint8  m_ucPriority;
  } NWK_SND_BIG_MSG;
  
  typedef struct
  {
      TIME   m_tExpirationTime;
      uint16 m_unContractID;
      uint16 m_unPayloadLen;
      uint16 m_unPayloadOffset;
      uint8  m_aPayload[1+MAX_DATAGRAM_SIZE]; // NL dispatch + NL payload
      uint8  m_ucPriorityAndFlags;
      uint8  m_ucEscapeHeaderLen;
      uint8  m_aSrcDstAddr[MAX_RCV_SRC_ADDR_LEN];
      uint8  m_ucSrcDstAddrLen;        
  } NWK_RCV_BIG_MSG;

  #define FRAG_MAX_SIZE   88
  #define FRAG_PAYLOAD_INIT_OFFSET FRAG_MAX_SIZE

  NWK_SND_BIG_MSG g_stSndMultiplePkMsg;
  NWK_RCV_BIG_MSG g_stRcvMultiplePkMsg;
  
  uint8 NLDE_getNextFragment( uint8 * p_pPkBuff );
#endif  



void NLDE_dispatchRxMsg( const uint8 * p_pSrcDstAddr,
                         uint8         p_ucSrcDstAddrLen,
                         uint8       * p_pTLPayload,
                         uint16        p_unTLPayloadLen,
                         uint8         p_ucPriorityFlags
#if defined(BACKBONE_SUPPORT)
                         , uint16        p_unContractId
#endif                         
                         );

uint8  NLDE_send6LoPANPacketToUdp(   const uint8 * p_pIPv6DstAddr, 
                                     const uint8 * p_pIPv6SrcAddr,
                                     uint8         p_ucPriorityAndFlags,
                                     uint16        p_unContractID,
                                     const uint8 * p_pTLPayload,
                                     uint16        p_unTLPayloadLen );


uint16 g_unNewDataGramNo;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  It is used by the Application Layer to send the packet to one device
///         in the network.
/// @param  p_pDestAddr  - EUI64/IPV6 final destination (first byte is 0xFF -> MAC address)
/// @param  p_unSrcShortAddr  - message source short address 
/// @param  p_unContractID    - contract ID
/// @param  p_ucPriorityAndFlags  - message priority, Discard Eligibility, ECN
/// @param  p_unNsduHeaderLen - NSDU header length
/// @param  p_pNsduHeader     - NSDU header data
/// @param  p_unAppDataLen    - APP data length
/// @param  p_pAppData        - APP data buffer
/// @param  p_nsduHandle      - transport level handler
/// @param  p_bOutgoingInterface - outgoing interface: 0 to DLL, 1 to ETH (BBR only)
/// @return none
/// @remarks
///      Access level: user level\n
///      Context: After the call of this function a DLDE_DATA_Confirm has to be received.
////////////////////////////////////////////////////////////////////////////////////////////////////
void NLDE_DATA_RequestMemOptimzed
                      (   const uint8* p_pDestAddr,
                          uint16       p_unSrcShortAddr,
                          uint16       p_unContractID,
                          uint8        p_ucPriorityAndFlags,
                          uint16       p_unNsduHeaderLen,
                          const void * p_pNsduHeader,
                          uint16       p_unAppDataLen,
                          const void * p_pAppData,
                          HANDLE       p_nsduHandle
#if defined(BACKBONE_SUPPORT) 
                          , uint8        p_bOutgoingInterface
#endif
                          )
{
    DLL_PACKET stDllPk;
    uint8 * pHC1;
        
    uint8  ucHopsNo = 64;
    uint8  ucContractEnabled = 0;
    uint8  ucHCDispatch;
    uint8  ucIdx;
    uint8  ucHeaderLen;
    uint16 unNsduDataLen;
    uint8  ucDstSrcAddrLen;
    
#ifdef WCI_SUPPORT
    uint8 ucAddrLen = 16;
    uint8 * pDstAddr = (uint8 *)p_pDestAddr;
    if ( *pDstAddr == 0xFF )  // MAC addr ?   
    {
        pDstAddr += 8;
        ucAddrLen -= 8;
    }
    
    WCI_Log( LOG_M_NL, NLOG_DataReq, ucAddrLen, pDstAddr,
                                     sizeof(p_unSrcShortAddr), &p_unSrcShortAddr,
                                     sizeof(p_unContractID), &p_unContractID,
                                     sizeof(p_ucPriorityAndFlags), &p_ucPriorityAndFlags,
                                     p_unNsduHeaderLen, p_pNsduHeader,
                                     (uint8)p_unAppDataLen, (uint8 *)p_pAppData,
                                     sizeof(p_nsduHandle), &p_nsduHandle );
    
#endif  // WCI_SUPPORT
    
#if !defined(BACKBONE_SUPPORT) 
    #define p_bOutgoingInterface 0 // send to DLL
#endif    
        
    uint8  aDstAddr[4];
    uint8  ucIsIPv6Address = (p_pDestAddr[0] != 0xFF);
    
    if( *(uint8*)p_pNsduHeader == UDP_ENCODING_ENCRYPTED )
    {
        ucHCDispatch = HC1_DISPATCH_BASIC;
        ucHeaderLen = HC1_BASIC_H_SIZE;
    }
    else if( ucIsIPv6Address ) // regular non encrypted
    {
        ucHCDispatch = HC1_DISPATCH_NO_CONTRACT | 2;  
        ucHeaderLen =  HC1_NO_CONTR_HOPS_DEFAULT_H_SIZE;
    }
    else // join
    {
        ucHCDispatch = HC1_DISPATCH_NO_CONTRACT | 1;  
        ucHeaderLen =  HC1_NO_CONTR_HOPS_DEFAULT_H_SIZE;
    }
        
    if( ucIsIPv6Address ) // IPV6 address
    {
        NLME_CONTRACT_ATTRIBUTES * pContract = NLME_FindContract(p_unContractID);
        if( pContract ) // contract found
        {
            ucContractEnabled = pContract->m_bIncludeContractFlag;
            
//Contract priority establishes a base priority for all messages sent using that contract. 
//  Four contract priorities are supported using 2 bits
//Message priority establishes priority within a contract. 
//  Two message priorities are supported using 1 bit, low = 0 and high = 1. 
//  Another 1 bit is reserved for future releases of this standard.             
            p_ucPriorityAndFlags |= ((uint8)pContract->m_bPriority) << 2;            
        }
          
#if !defined(BACKBONE_SUPPORT) // it is field device
        if( ucContractEnabled ) 
#else // defined(BACKBONE_SUPPORT)           
        if( p_bOutgoingInterface ) // it is for ETH 
#endif            
        {                        
            NLME_ROUTE_ATTRIBUTES * pRoute = NLME_FindDestinationRoute( (uint8*) p_pDestAddr );
            if( pRoute ) // route found
            {
                ucHopsNo = pRoute->m_ucNWK_HopLimit;
#if defined(BACKBONE_SUPPORT)             
                p_bOutgoingInterface = pRoute->m_bOutgoingInterface;
                if( !p_bOutgoingInterface ) // to DLL -> not any contract used
                {
                    ucContractEnabled = 0;
                }
#endif
            }
            if( ucContractEnabled ) // contract enabled ntw header
            {
                ucHeaderLen =  HC1_CONTR_HOPS_DEFAULT_H_SIZE;
                ucHCDispatch = HC1_DISPATCH_WITH_CONTRACT;
            }
            else if( ucHCDispatch != HC1_DISPATCH_BASIC || ucHopsNo != 64 )
            {
                ucHeaderLen =  HC1_NO_CONTR_HOPS_DEFAULT_H_SIZE;
                ucHCDispatch = HC1_DISPATCH_NO_CONTRACT;  
            }
            
            if( ucHCDispatch != HC1_DISPATCH_BASIC )
            {
                switch( ucHopsNo )
                {
                case 1  : ucHCDispatch |= 1; break; 
                case 64 : ucHCDispatch |= 2; break;  
                case 255: ucHCDispatch |= 3; break;  
                
                default:  ucHeaderLen ++; // add routes no
                }
            }
        }
                
        if( !p_bOutgoingInterface ) // it is for dll
        {
              NLME_ADDR_TRANS_ATTRIBUTES * pAttEntry = NLME_FindATT( p_pDestAddr );
              if( !pAttEntry )
              {
                  NLME_Alert( NLME_ALERT_DEST_UNREACHABLE, p_pDestAddr, 16 );
                  NLDE_DATA_Confirm( p_nsduHandle, SFC_INVALID_ADDRESS );
                  return;
              }
              
              aDstAddr[0] = pAttEntry->m_aShortAddress[0];
              aDstAddr[1] = pAttEntry->m_aShortAddress[1];
              aDstAddr[2] = (uint8)(p_unSrcShortAddr >> 8);
              aDstAddr[3] = (uint8)(p_unSrcShortAddr);
              
              p_pDestAddr = aDstAddr;
              
              ucDstSrcAddrLen = 4;
        }
    }
    else // p_pDestAddr[0] == 0xFF -> MAC address
    {        
#if defined(BACKBONE_SUPPORT)             
        p_bOutgoingInterface = 0; // DLL
#endif                
        p_pDestAddr = p_pDestAddr+8; // MAC = last 8 bytes
        ucDstSrcAddrLen = 8;        
    }    
    
    if( ucHCDispatch == HC1_DISPATCH_BASIC ) // basic format, discard first TL byte
    {
        p_unNsduHeaderLen--;
        p_pNsduHeader = (uint8*)p_pNsduHeader  + 1;
    }
    
    unNsduDataLen = p_unAppDataLen+p_unNsduHeaderLen;
    
    if( unNsduDataLen > MAX_DATAGRAM_SIZE )
    {
        NLDE_DATA_Confirm( p_nsduHandle, SFC_INVALID_SIZE );
        return;
    }

    if( p_nsduHandle == HANDLE_NL_BIG_MSG )
    {
        NLDE_DATA_Confirm( p_nsduHandle, SFC_INVALID_PARAMETER );
        return;
    }
    
    if( unNsduDataLen + ucHeaderLen > sizeof(DLL_PACKET) ) // fragmenation ... add fragmented header
    {
#if !defined( NL_FRAG_SUPPORT )        
        NLME_Alert( NLME_ALERT_NPDU_LEN_TOO_LONG, (uint8*)&unNsduDataLen, 2 );
        NLDE_DATA_Confirm( p_nsduHandle, SFC_INVALID_SIZE );
        return;        
#elif defined(BACKBONE_SUPPORT)
        if( p_bOutgoingInterface ) // to router, not to DL
        {
              NLME_Alert( NLME_ALERT_NPDU_LEN_TOO_LONG, &unNsduDataLen, 2 );
              NLDE_DATA_Confirm( p_nsduHandle, SFC_INVALID_SIZE ); // cannot send fragmented data to SM/GW 
              return;
        }            
#else // defined( NL_FRAG_SUPPORT ) && !defined(BACKBONE_SUPPORT)
        ucHeaderLen += FRAG1_H_SIZE; // add fragmented header

        if( g_stSndMultiplePkMsg.m_unPayloadOffset )
        {
            NLME_Alert( NLME_ALERT_OUT_OF_MEM, &unNsduDataLen, 2 );
            NLDE_DATA_Confirm( p_nsduHandle, SFC_INSUFFICIENT_DEVICE_RESOURCES );
            return;
        }


        pHC1 = (uint8*)&stDllPk.m_stFrag1.m_stHC1;
#endif  
    }
    else
    {
        pHC1 = (uint8*)&stDllPk.m_stHC1;
    }

    // HC1 header construction
    *(pHC1++) = ucHCDispatch;
        
    if( ucHCDispatch != HC1_DISPATCH_BASIC )
    {         
        *(pHC1++) = HC1_ENCODING_LIMIT;
        
        if( ucContractEnabled )
        {
            *(pHC1++) = p_ucPriorityAndFlags >> 4; // flags from priority
            *(pHC1++) = (uint8)(p_unContractID >> 8);
            *(pHC1++) = (uint8)(p_unContractID);
        }
        if( (ucHCDispatch & HC1_DISPATCH_HOPS_MASK) == 0 ) // hops carried inline
        {
            *(pHC1++) = ucHopsNo;
        }
    }

    if( ucHeaderLen + p_unNsduHeaderLen >= sizeof(DLL_PACKET) )
    {
        NLME_Alert( NLME_ALERT_NPDU_LEN_TOO_LONG, (uint8*)&unNsduDataLen, 2 );
        NLDE_DATA_Confirm( p_nsduHandle, SFC_INVALID_PARAMETER );
        return;
    }

    memcpy( (uint8*)&stDllPk + ucHeaderLen, p_pNsduHeader, p_unNsduHeaderLen );
    ucHeaderLen += p_unNsduHeaderLen;

    if(  p_unAppDataLen + ucHeaderLen <= sizeof(DLL_PACKET) ) // without fragmentation
    {
        memcpy( (uint8*)&stDllPk + ucHeaderLen, p_pAppData, p_unAppDataLen );
        ucIdx = ucHeaderLen + p_unAppDataLen;

        if( p_bOutgoingInterface ) // to router, not to DL
        {
            uint8 ucStatus = UART_LINK_AddMessage( UART_MSG_TYPE_NWK_T2B, 
                                                  p_ucPriorityAndFlags,
                                                  16,
                                                  p_pDestAddr,
                                                  ucIdx, 
                                                  &stDllPk );

            NLDE_DATA_Confirm( p_nsduHandle, ucStatus );
            return;
        }
    }
    else // with fragmentation -> send first packet
    {      
#if defined( NL_FRAG_SUPPORT )        
      
        ucIdx = FRAG_MAX_SIZE - (ucHeaderLen-FRAG1_H_SIZE); // decrease 1 because frag1 is 1 byte more that frag2
        memcpy( (uint8*)&stDllPk + ucHeaderLen, p_pAppData, ucIdx );
        p_pAppData = (uint8*)p_pAppData + ucIdx;
        
        g_stSndMultiplePkMsg.m_unPayloadOffset =  FRAG_PAYLOAD_INIT_OFFSET;
        g_stSndMultiplePkMsg.m_unPayloadLen = p_unAppDataLen + (ucHeaderLen-FRAG1_H_SIZE);

        // create fragmentation header
        //  5 bits   11 bits	     16 bits
        // 11000     Datagram_size   Datagram_tag

        g_unNewDataGramNo ++;

        g_stSndMultiplePkMsg.m_unDatagramSize = g_stSndMultiplePkMsg.m_unPayloadLen;
        
        stDllPk.m_stFrag1.m_aDatagramSize[0] = ((uint8)(g_stSndMultiplePkMsg.m_unDatagramSize >> 8)) | HC1_DISPATCH_FRAG1;
        stDllPk.m_stFrag1.m_aDatagramSize[1] = (uint8)(g_stSndMultiplePkMsg.m_unDatagramSize);
        stDllPk.m_stFrag1.m_aDatagramTag[0]  = (uint8)(g_unNewDataGramNo >> 8);
        stDllPk.m_stFrag1.m_aDatagramTag[1]  = (uint8)(g_unNewDataGramNo);

        g_stSndMultiplePkMsg.m_unPayloadLen -= FRAG_PAYLOAD_INIT_OFFSET;

        // save payload for next time sending
        memcpy( g_stSndMultiplePkMsg.m_aPayload, p_pAppData, g_stSndMultiplePkMsg.m_unPayloadLen );
                
        g_stSndMultiplePkMsg.m_ucPriority = (p_ucPriorityAndFlags & NLDE_PRIORITY_MASK);
        g_stSndMultiplePkMsg.m_unContractID = p_unContractID;
        g_stSndMultiplePkMsg.m_ucDstSrcAddrLen = ucDstSrcAddrLen;
        
        memcpy( g_stSndMultiplePkMsg.m_aDstSrcAddr, p_pDestAddr, ucDstSrcAddrLen ); 
        
        ucIdx = FRAG1_H_SIZE + FRAG_MAX_SIZE;

        g_stSndMultiplePkMsg.m_hTLHandler = p_nsduHandle;
        p_nsduHandle = HANDLE_NL_BIG_MSG; // special network handler
#endif // defined( NL_FRAG_SUPPORT )        
    }

#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225) 
    p_nsduHandle |= 0x8000; // from APP -> set handle bit on backbone support case
#endif // BACKBONE_SUPPORT

    DLDE_Data_Request( ucDstSrcAddrLen,
                         (uint8*)p_pDestAddr,
                         p_ucPriorityAndFlags,
                         p_unContractID,
                         ucIdx,
                         &stDllPk,
                         p_nsduHandle);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Searches for the next fragment of a fragmented packet
/// @param  p_pPkBuff  - pointer to the buffer where to copy the fragment
/// @return size of fragment
/// @remarks
///      Access level: Interrupt level\n
///      Context: From DLDE_DATA_Confirm with success on a fragmented message
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined( NL_FRAG_SUPPORT )        
  uint8 NLDE_getNextFragment( uint8 * p_pPkBuff )
  {
       uint16 unPayloadSize = g_stSndMultiplePkMsg.m_unPayloadLen;
  
      // not end of datagram
      if( unPayloadSize )
      {
          if( unPayloadSize > FRAG_MAX_SIZE )
          {
              unPayloadSize = FRAG_MAX_SIZE;
          }
  
          g_stSndMultiplePkMsg.m_unPayloadLen -= unPayloadSize;
  
          // create fragmentation header
          //  5 bits   11 bits	     16 bits       8bits
          // 11100     Datagram_size   Datagram_tag  Datagrame_offset        
          *(p_pPkBuff++) = ((uint8)(g_stSndMultiplePkMsg.m_unDatagramSize >> 8)) | HC1_DISPATCH_FRAG2;
          *(p_pPkBuff++) = (uint8)(g_stSndMultiplePkMsg.m_unDatagramSize);
          *(p_pPkBuff++) = (uint8)(g_unNewDataGramNo >> 8);
          *(p_pPkBuff++) = (uint8)(g_unNewDataGramNo);
   
          *(p_pPkBuff++) = (uint8)(g_stSndMultiplePkMsg.m_unPayloadOffset/8);
          memcpy( p_pPkBuff,
                    g_stSndMultiplePkMsg.m_aPayload+g_stSndMultiplePkMsg.m_unPayloadOffset - FRAG_PAYLOAD_INIT_OFFSET,
                    unPayloadSize );
  
          g_stSndMultiplePkMsg.m_unPayloadOffset +=  unPayloadSize;
          unPayloadSize +=  FRAG2_H_SIZE;
      }
  
      return (uint8)unPayloadSize;
  }
#endif  
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Dorin Pavel, Ion Ticus
/// @brief  It is invoked by the Data-Link Layer to communicate to the Network Layer 
///         the result of a previously issued transmit request.
/// @param  p_oHandle       - application level handler (same as on request)
/// @param  p_ucLocalStatus - confirmation status
/// @return none
/// @remarks
///      Access level: Interrupt or User level\n
///      Context: Must be preceded by a transmit request action
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_Data_Confirm (HANDLE p_hHandle, uint8 p_ucLocalStatus)
{
  
  Log( (p_ucLocalStatus ? LOG_WARNING : LOG_DEBUG), LOG_M_DLL, DLOG_Confirm, sizeof(p_hHandle), &p_hHandle, sizeof(p_ucLocalStatus), &p_ucLocalStatus );
    WCI_Log( LOG_M_DLL, DLOG_Confirm, sizeof(p_hHandle), &p_hHandle, sizeof(p_ucLocalStatus), &p_ucLocalStatus );
    
   // a short message confirmation
    if( p_hHandle != HANDLE_NL_BIG_MSG )
    {      
        NLDE_DATA_Confirm( p_hHandle, p_ucLocalStatus );
        return;
    }

#if defined( NL_FRAG_SUPPORT )        
    // a big message confirmation
    if( !g_stSndMultiplePkMsg.m_unPayloadOffset ) // but not big message on progress
          return;

    // a big message confirmation
    if( !g_stSndMultiplePkMsg.m_unPayloadLen || p_ucLocalStatus != SFC_SUCCESS ) // end of datagrame
    {
        NLDE_DATA_Confirm( g_stSndMultiplePkMsg.m_hTLHandler, p_ucLocalStatus );
        
        g_stSndMultiplePkMsg.m_unPayloadLen = 0; // release the big message buffer
        g_stSndMultiplePkMsg.m_unPayloadOffset = 0;
    }
    else   // datagrame on progess
    {
        uint8  aDLLPacket[DLL_MAX_DSDU_SIZE_DEFAULT];
        uint8  ucPayloadSize = NLDE_getNextFragment(aDLLPacket);

        DLDE_Data_Request( g_stSndMultiplePkMsg.m_ucDstSrcAddrLen,
                           g_stSndMultiplePkMsg.m_aDstSrcAddr,
                           g_stSndMultiplePkMsg.m_ucPriority,
                           g_stSndMultiplePkMsg.m_unContractID,
                           ucPayloadSize,
                           aDLLPacket,
                           HANDLE_NL_BIG_MSG);

    }
#endif    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Dorin Pavel, Ion Ticus
/// @brief  It is invoked by DLL Layer to notify the Network Layer of a
///         successfully received payload addressed to the device.
/// @param  p_ucSrcDstAddrLen - The source addressing length (4 or 8)
/// @param  p_pSrcDstAddr     - Network source address + destination address (if length is 4)
/// @param  p_ucPriority      - Priority of the payload
/// @param  p_ucPayloadLength - Payload length
/// @param  p_pPayload        - Payload. Number of octets as per p_ucPayloadLength
/// @return none
/// @remarks
///      Access level: Interrupt\n
///      Context: When a new packet data was received and processed by DLL
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_Data_Indicate (uint8         p_ucSrcDstAddrLen,
                         const uint8 * p_pSrcDstAddr,
                         uint8         p_ucPriority,
                         uint8         p_ucPayloadLength,
			 const void *  p_pPayload)
{

  uint8 ucPkHeaderLen;
  const DLL_PACKET * pDllPk = p_pPayload;
  
  Log( LOG_DEBUG, LOG_M_DLL, DLOG_Indicate, p_ucSrcDstAddrLen, p_pSrcDstAddr,
                                     sizeof(p_ucPriority), &p_ucPriority,
                                     p_ucPayloadLength, (uint8 *)p_pPayload );
  
  WCI_Log( LOG_M_DLL, DLOG_Indicate, p_ucSrcDstAddrLen, p_pSrcDstAddr,
                                     sizeof(p_ucPriority), &p_ucPriority,
                                     p_ucPayloadLength, (uint8 *)p_pPayload );

  
  if( p_ucSrcDstAddrLen == 4 )
  {
  // check if I'm not final destination
    if(  p_pSrcDstAddr[2] != (uint8)(g_unShortAddr >> 8)
     ||  p_pSrcDstAddr[3] != (uint8)(g_unShortAddr)  )
    {
#if defined(BACKBONE_SUPPORT) // old type of TR 
        // I'm not the final dest -> send to 6lowPAN <-> IPV6 router
        UART_LINK_AddMessage ( UART_MSG_TYPE_NWK_T2B, 
                              p_ucPriority,
                              p_ucSrcDstAddrLen, p_pSrcDstAddr,
                              p_ucPayloadLength, p_pPayload );
#endif        
        return;
    }
  }
  
  // HC1 -> sigle packet datagram
  if(    pDllPk->m_ucHC1Dispatch ==  HC1_DISPATCH_BASIC 
     || (pDllPk->m_ucHC1Dispatch & HC1_DISPATCH_CONTRACT_MASK) == (HC1_DISPATCH_NO_CONTRACT & HC1_DISPATCH_CONTRACT_MASK) ) 
  {
      if( g_stRcvPacket.m_ucPkLen ) // not space ...
      {
            NLME_Alert( NLME_ALERT_OUT_OF_MEM, p_pPayload, p_ucPayloadLength );
            return;
      }

#if defined(BACKBONE_SUPPORT)
      g_stRcvPacket.m_unContractID = 0;
#endif      
      g_stRcvPacket.m_aPkBuffer[0] = pDllPk->m_ucHC1Dispatch;
      if( pDllPk->m_ucHC1Dispatch == HC1_DISPATCH_BASIC )
      {
          ucPkHeaderLen = HC1_BASIC_H_SIZE;
      }
      else
      {
          if( (pDllPk->m_ucHC1Dispatch & (~HC1_DISPATCH_HOPS_MASK)) == HC1_DISPATCH_NO_CONTRACT )
          {
              ucPkHeaderLen = HC1_NO_CONTR_HOPS_DEFAULT_H_SIZE;
          }
          else
          {
#if defined(BACKBONE_SUPPORT)
              p_ucPriority |= pDllPk->m_stFrag1.m_stHC1.m_stContractEnabled.m_aFlowLabel[0] << 4;
              g_stRcvPacket.m_unContractID = ((uint16)(pDllPk->m_stHC1.m_stContractEnabled.m_aFlowLabel[1]) << 8) 
                                              | pDllPk->m_stHC1.m_stContractEnabled.m_aFlowLabel[2];
#endif      
              ucPkHeaderLen = HC1_CONTR_HOPS_DEFAULT_H_SIZE;
          }
          if( (pDllPk->m_ucHC1Dispatch & HC1_DISPATCH_HOPS_MASK) == 0 )
          {
              ucPkHeaderLen++;
          }
      }

      if( p_ucPayloadLength >= ucPkHeaderLen ) // has space and valid packet size
      {
          g_stRcvPacket.m_ucSrcDstAddrLen = p_ucSrcDstAddrLen;
          memcpy( g_stRcvPacket.m_aSrcDstAddr, p_pSrcDstAddr, p_ucSrcDstAddrLen );

          g_stRcvPacket.m_ucPriorityAndFlags = p_ucPriority;
          g_stRcvPacket.m_ucPkLen = p_ucPayloadLength-ucPkHeaderLen;
          memcpy( g_stRcvPacket.m_aPkBuffer+1, (const uint8*)p_pPayload+ucPkHeaderLen, g_stRcvPacket.m_ucPkLen );
      }
  }
#if defined( NL_FRAG_SUPPORT )        
  //  5 bits   11 bits	     16 bits
  //  11000     Datagram_size   Datagram_tag
  //  first fragment datagram
  else if( (pDllPk->m_ucHC1Dispatch & HC1_DISPATCH_FRAG_MASK) == HC1_DISPATCH_FRAG1 ) 
  {
      if( g_stRcvMultiplePkMsg.m_unPayloadLen ) // not space ...
      {
            NLME_Alert( NLME_ALERT_OUT_OF_MEM, p_pPayload, p_ucPayloadLength );
            return;
      }

      if( p_ucPayloadLength > FRAG1_H_SIZE+HC1_BASIC_H_SIZE )
      {
          uint16 unDatagramSize = (((uint16)pDllPk->m_stFrag2.m_aDatagramSize[0] & 0x07) << 8)
                                  + pDllPk->m_stFrag2.m_aDatagramSize[1];

          if( unDatagramSize <= MAX_DATAGRAM_SIZE )
          {            
#if defined(BACKBONE_SUPPORT)
              g_stRcvMultiplePkMsg.m_unContractID = 0;
#endif      
              g_stRcvMultiplePkMsg.m_aPayload[0] = pDllPk->m_stFrag1.m_stHC1.m_stBasic.m_ucHC1Dispatch;
              if( pDllPk->m_stFrag1.m_stHC1.m_stBasic.m_ucHC1Dispatch == HC1_DISPATCH_BASIC  )
              {         
                  ucPkHeaderLen = FRAG1_H_SIZE + HC1_BASIC_H_SIZE;
              }
              else  if((pDllPk->m_stFrag1.m_stHC1.m_stBasic.m_ucHC1Dispatch & HC1_DISPATCH_CONTRACT_MASK) == (HC1_DISPATCH_NO_CONTRACT & HC1_DISPATCH_CONTRACT_MASK) )
              {
                  if( (pDllPk->m_stFrag1.m_stHC1.m_stBasic.m_ucHC1Dispatch & (~HC1_DISPATCH_HOPS_MASK)) == HC1_DISPATCH_NO_CONTRACT )
                  {
                      ucPkHeaderLen = FRAG1_H_SIZE+HC1_NO_CONTR_HOPS_DEFAULT_H_SIZE;
#if defined(BACKBONE_SUPPORT)
                      p_ucPriority |= pDllPk->m_stFrag1.m_stHC1.m_stContractEnabled.m_aFlowLabel[0] << 4;
                      g_stRcvMultiplePkMsg.m_unContractID = 
                                                ((uint16)(pDllPk->m_stFrag1.m_stHC1.m_stContractEnabled.m_aFlowLabel[1]) << 8) 
                                              | pDllPk->m_stFrag1.m_stHC1.m_stContractEnabled.m_aFlowLabel[2];
#endif      
                  }
                  else
                  {
                      ucPkHeaderLen = FRAG1_H_SIZE+HC1_CONTR_HOPS_DEFAULT_H_SIZE;
                  }
                  if( (pDllPk->m_stFrag1.m_stHC1.m_stBasic.m_ucHC1Dispatch & HC1_DISPATCH_HOPS_MASK) == 0 )
                  {
                      ucPkHeaderLen++;
                  }
              }
              else // not UDP or not 16 bit address type
              {
                  NLME_Alert( NLME_ALERT_HEADER_ERROR, p_pPayload, p_ucPayloadLength );
                  return;
              }
              if( p_ucPayloadLength > ucPkHeaderLen )
              {
                  g_stRcvMultiplePkMsg.m_ucEscapeHeaderLen = ucPkHeaderLen - FRAG1_H_SIZE;
                  g_stRcvMultiplePkMsg.m_tExpirationTime = MAX_RCV_TIME_MULTIPLE_PK + MLSM_GetCrtTaiSec();
                  g_stRcvMultiplePkMsg.m_unPayloadLen  = unDatagramSize-g_stRcvMultiplePkMsg.m_ucEscapeHeaderLen;
                  g_stRcvMultiplePkMsg.m_unPayloadOffset = p_ucPayloadLength-ucPkHeaderLen;
                  g_stRcvMultiplePkMsg.m_ucPriorityAndFlags = p_ucPriority;
                  g_stRcvMultiplePkMsg.m_ucSrcDstAddrLen = p_ucSrcDstAddrLen;
                  memcpy( g_stRcvMultiplePkMsg.m_aSrcDstAddr, p_pSrcDstAddr, p_ucSrcDstAddrLen );

                  memcpy( g_stRcvMultiplePkMsg.m_aPayload+1, (const uint8*)p_pPayload+ucPkHeaderLen,  g_stRcvMultiplePkMsg.m_unPayloadOffset );
              }
          }
      }
  }
        //  5 bits   11 bits	     16 bits
        // 11100     Datagram_size   Datagram_tag
  else if( (pDllPk->m_ucHC1Dispatch & HC1_DISPATCH_FRAG_MASK) == HC1_DISPATCH_FRAG2 ) // next fragment datagram
  {
      if( !g_stRcvMultiplePkMsg.m_unPayloadLen ) // unexpected fragment because no start fragment
      {
            NLME_Alert( NLME_ALERT_FRAG_ERROR, p_pPayload, p_ucPayloadLength );
            return;
      }
      
      // unexpected fragment because other message
      if(  p_ucSrcDstAddrLen != g_stRcvMultiplePkMsg.m_ucSrcDstAddrLen )
      {
            NLME_Alert( NLME_ALERT_FRAG_ERROR, p_pPayload, p_ucPayloadLength );
            return;
      }

      if(  memcmp( p_pSrcDstAddr, g_stRcvMultiplePkMsg.m_aSrcDstAddr, p_ucSrcDstAddrLen ) )
      {
            NLME_Alert( NLME_ALERT_FRAG_ERROR, p_pPayload, p_ucPayloadLength );
            return;
      }
      
      if( p_ucPayloadLength <= FRAG2_H_SIZE ) // invalid length
      {
            NLME_Alert( NLME_ALERT_FRAG_ERROR, p_pPayload, p_ucPayloadLength );
            return;
      }

      p_ucPayloadLength -= FRAG2_H_SIZE;

      // unexpected fragment because other offset
      if(  (g_stRcvMultiplePkMsg.m_unPayloadOffset+g_stRcvMultiplePkMsg.m_ucEscapeHeaderLen)/8
                != pDllPk->m_stFrag2.m_ucDatagramOffset )
      {
            NLME_Alert( NLME_ALERT_FRAG_ERROR, p_pPayload, p_ucPayloadLength );
            return;
      }

      if(  g_stRcvMultiplePkMsg.m_unPayloadLen < g_stRcvMultiplePkMsg.m_unPayloadOffset + p_ucPayloadLength )
      {
            NLME_Alert( NLME_ALERT_FRAG_ERROR, p_pPayload, p_ucPayloadLength );
            return;
      }

      memcpy( g_stRcvMultiplePkMsg.m_aPayload + 1 + g_stRcvMultiplePkMsg.m_unPayloadOffset, &pDllPk->m_stFrag2.m_stHC1,  p_ucPayloadLength);
      g_stRcvMultiplePkMsg.m_unPayloadOffset += p_ucPayloadLength;
  }
#endif // defined( NL_FRAG_SUPPORT )        
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Initializes the contract and route tables. Resets the current packet and multi-packet 
///         structures
/// @param  none
/// @return none
/// @remarks
///      Access level: User Level
////////////////////////////////////////////////////////////////////////////////////////////////////
void NLDE_Init(void)
{
  g_stRcvPacket.m_ucPkLen = 0;
#if defined( NL_FRAG_SUPPORT )        
  g_stRcvMultiplePkMsg.m_unPayloadLen = 0;
  g_stSndMultiplePkMsg.m_unPayloadOffset = 0;
  g_stSndMultiplePkMsg.m_unPayloadLen = 0;
#endif  
  
  NLME_Init();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Performs network layer messages management (starting from this point 
///         message dispatch will be on User Level)
/// @param  none
/// @return none
/// @remarks
///      Access level: User Level
////////////////////////////////////////////////////////////////////////////////////////////////////
void NLDE_Task(void)
{
    if( g_stRcvPacket.m_ucPkLen ) // single packet received ...
    {
        NLDE_dispatchRxMsg( g_stRcvPacket.m_aSrcDstAddr,
                            g_stRcvPacket.m_ucSrcDstAddrLen,
                            g_stRcvPacket.m_aPkBuffer,
                            g_stRcvPacket.m_ucPkLen,
                            g_stRcvPacket.m_ucPriorityAndFlags
#if defined(BACKBONE_SUPPORT)
                            , g_stRcvPacket.m_unContractID
#endif                         
                              );

        g_stRcvPacket.m_ucPkLen = 0;
    }
#if defined( NL_FRAG_SUPPORT )        
    if( g_stRcvMultiplePkMsg.m_unPayloadLen ) // rcv big message on progress
    {
        if( g_stRcvMultiplePkMsg.m_unPayloadOffset >= g_stRcvMultiplePkMsg.m_unPayloadLen ) // completed message
        {
            NLDE_dispatchRxMsg( g_stRcvMultiplePkMsg.m_aSrcDstAddr,
                                g_stRcvMultiplePkMsg.m_ucSrcDstAddrLen,
                                g_stRcvMultiplePkMsg.m_aPayload,
                                g_stRcvMultiplePkMsg.m_unPayloadLen,
                                g_stRcvMultiplePkMsg.m_ucPriorityAndFlags
#if defined(BACKBONE_SUPPORT)
                                , g_stRcvMultiplePkMsg.m_unContractID
#endif                         
                              );
            
            g_stRcvMultiplePkMsg.m_unPayloadLen = 0;
        }
        else  if( g_stRcvMultiplePkMsg.m_tExpirationTime < MLSM_GetCrtTaiSec() ) // expiration happend, clear the buffer
        {
            g_stRcvMultiplePkMsg.m_unPayloadLen = 0;
        }
    }
#endif // #if ( NL_FRAG_SUPPORT )            
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Dispatch an RX message received from RF interface
/// @param  p_pSrcDstAddr    - Network source address + destination address (if length is 4)
/// @param  p_ucSrcDstAddrLen  - The source addressing length (4 or 8)
/// @param  p_pTLPayload      - ISA100 Transport layer payload 
/// @param  p_unTLPayloadLen  - ISA100 Transport layer payload length
/// @param  p_ucPriorityFlags - message priority + DE + ECN
/// @param  p_unContractId    - Contract ID (make sense if local loop on other subnet) (on BBR only) 
/// @return none
/// @remarks
///      Access level: User level\n
///      Context: From NL task
////////////////////////////////////////////////////////////////////////////////////////////////////
void NLDE_dispatchRxMsg( const uint8 * p_pSrcDstAddr,
                         uint8         p_ucSrcDstAddrLen,
                         uint8       * p_pTLPayload,
                         uint16        p_unTLPayloadLen,
                         uint8         p_ucPriorityFlags
#if defined(BACKBONE_SUPPORT)
                         , uint16        p_unContractId 
#endif                         
                           )
{
    uint8 aIpv6SrcAddr[16];

    if( *p_pTLPayload == HC1_DISPATCH_BASIC ) // add elided TL UDP hdr
    {
        *p_pTLPayload = UDP_ENCODING_ENCRYPTED;
        p_unTLPayloadLen ++;
    }
    else // not elided TL UDP hdr -> jump over NL dispatch byte
    {
        p_pTLPayload++;
    }
    
    if( p_ucSrcDstAddrLen == 8 ) // mac address
    {
        aIpv6SrcAddr[0] = 0xFF;
        memcpy( aIpv6SrcAddr+8, p_pSrcDstAddr, 8 );
    }
#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)
    else if( p_ucSrcDstAddrLen == 16 ) // ipv6 address
    {
        memcpy( aIpv6SrcAddr, p_pSrcDstAddr, 16 );
    }

#endif    
    else // ( p_ucSrcAddrLen == 2 ) // short address
    {
        NLME_ADDR_TRANS_ATTRIBUTES * pAtt = NLME_FindATTByShortAddr( p_pSrcDstAddr );
        if( !pAtt )
        {
            NLME_Alert( NLME_ALERT_NO_ROUTE, p_pSrcDstAddr, 2 );
            return;
        }
        
        memcpy( aIpv6SrcAddr, pAtt->m_aIPV6Addr, 16 );
    }
    NLDE_DATA_Indication( aIpv6SrcAddr,
                          p_unTLPayloadLen,
                          p_pTLPayload,
                          p_ucPriorityFlags );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Check if NL buffer is empty
/// @param  none
/// @return Return true if NL may receive a message (buffer is empty)  
/// @remarks
///      Access level: User level\n
///      Context: BBR only
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLDE_DATA_HasSpaceToAPP(void)
{
    return (g_stRcvPacket.m_ucPkLen == 0);
}
