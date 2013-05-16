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
/// Author:       Nivis LLC, Mircea Vlasin
/// Date:         December 2007
/// Description:  MAC Extension Layer Data Entity
/// Changes:      Rev 1.0 - created
/// Revisions:    1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "dmap.h"
#include "dmap_dmo.h"
#include "dlde.h"
#include "mlde.h"
#include "mlsm.h"
#include "nlme.h"
#include "../asm.h"
#include "tmr_util.h"
#include "dlmo_utils.h"

#ifndef BACKBONE_SUPPORT
  DLL_SMIB_SUPERFRAME g_stDAUXSuperframe;
#endif

static DLL_ADV_TIME_SYNC g_stDllAdvTimeSync;

uint8 g_ucMsgSendCnt;       //counter of the send messages during last second

const uint8 c_aucInvalidEUI64Addr[8] = {0, 0, 0, 0, 0, 0, 0, 0};

#pragma data_alignment=2
uint8 g_aucDllRxTxBuf[MAX_PHY_PAYLOAD_LEN+3];

/* Tx ACK PDU item */
uint8 g_oAckPdu[MAX_ACK_LEN];    
uint8 g_ucAckLen;
uint8 g_ucMsgForMe;

uint16 g_unCrtUsedSlowHoppingOffset;

DLL_MSG_HRDS g_stDllMsgHrds;
DLL_RX_MHR   g_stDllRxHdrExt;

// flag to signal that in the cuurent slot, the queue is almost full (congestion)
uint8 g_ucDllECNStatus;

const uint8 * g_pDllKey;
uint32        g_ulRcvTimestamp;
union
{
    uint32 m_ulAligned;
    uint8  m_aucNonce[13]; 
} g_stAlignedNonce;

#define aucNonce g_stAlignedNonce.m_aucNonce

uint16 MLDE_getStartSlotTime( const unsigned char * p_pSlowOffset, 
                              uint32 * p_pulPeerStartSlotTime, 
                              uint32 * p_pulCrtStartSlotTime, 
                              uint16 * p_punCrtSlowSlotOffset );
    
static void MLDE_preloadAck( uint16 p_unRxSFD, uint16 p_unCrtSlowHopOffset );
static void MLDE_composeAck(uint8  p_ucAckType, const uint8 * p_pucRxMIC);
unsigned int MLDE_generateActivateLinkDauxHdr( uint8 * p_pDauxHdr );
static unsigned int MLDE_generateAdvDauxHdr ( uint8  *p_pDauxHdr );
static unsigned int MLDE_generateLqiRssiDauxHdr ( uint8  *p_pDauxHdr );

uint8 g_ucDiscoveryState = DEVICE_DISCOVERY;


///////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin, Eduard Erdei
/// @brief  Generates a LQI RSSI type DAUX headerf
/// @param  p_pDauxHdr - pointer to the content of header
/// @return Length in bytes of the generated header
/// @remarks
///      Access level: Interrupt\n
///      Context: Only ACK frame type may contain this type od DAUX
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int MLDE_generateLqiRssiDauxHdr( uint8 * p_pDauxHdr )
{
  p_pDauxHdr[0] = DLL_LQI_RSSI_REPORT_DAUX << DLL_DAUX_TYPE_OFFSET; // DAUX header type

  p_pDauxHdr[1] = GET_LAST_RSSI() + (int16)ISA100_RSSI_OFFSET; // RSSI reports shall be biased by +64 dBm
  p_pDauxHdr[2] = g_ucLastLQI;                                 // LQI
  
  return 3;
}

#ifdef IDLE_LINK_FULL_SUPPORT
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Generates an Link Activation TypeDAUX sub-header
/// @param  p_pDauxHdr - pointer to the content of header
/// @return Length in bytes of the generated header
/// @remarks
///      Access level: Interrupt\n
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int MLDE_generateActivateLinkDauxHdr( uint8 * p_pDauxHdr )
{
  uint8 * pucDauxData = p_pDauxHdr;
  
  // find the specified link; caller of this function must check that m_unLinkBacklogIdx is valid
  DLL_SMIB_NEIGHBOR *   pNeighbor = &g_aDllNeighborsTable[g_ucTxNeighborIdx].m_stNeighbor;
  
  g_ucIdleLinkIdx = DLME_FindSMIBIdx(SMIB_IDX_DL_LINK, pNeighbor->m_unLinkBacklogIdx);
  
  if( 0xFF == g_ucIdleLinkIdx )
      return 0; // configuration missmatch
  
  if( g_aDllLinksTable[g_ucIdleLinkIdx].m_stLink.m_ucActiveTimer )
  {
      g_ucIdleLinkIdx = 0xFF;
      return 0; // link is already active
  }
  
  *(pucDauxData++) = DLL_ACTIVATE_IDLE_LINK_DAUX << DLL_DAUX_TYPE_OFFSET;
  
  pucDauxData = DLMO_InsertExtDLUint( pucDauxData, pNeighbor->m_unLinkBacklogIdx);
  
  *(pucDauxData++) = pNeighbor->m_ucLinkBacklogDur;
  
  // do not activate the link now; link will be activated only after ACK is received)
  
  return pucDauxData - p_pDauxHdr;  
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  It is invoked by DLL Layer when is necessary to added an advertisment
///         auxiliary sub-header
/// @param  p_pDauxHdr - pointer to the content of header
/// @return Length in bytes of the generated header
/// @remarks
///      Access level: Interrupt\n
///      Context: Type of current link determine if is necessary a DAUX to be sended
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int MLDE_generateAdvDauxHdr ( uint8  *p_pDauxHdr )
{
    uint8* pTemp;
    uint32 ulDauxCkSum = 0;
    DLL_SMIB_ENTRY_SUPERFRAME *pSuperframe = NULL;
    
    uint8 ucAdvOpt = 0x00;

    //find the index of the advertisment superframe inside the superframes table
    pSuperframe = (DLL_SMIB_ENTRY_SUPERFRAME*) DLME_FindSMIB(SMIB_IDX_DL_SUPERFRAME, g_unDllAdvSuperframe);
    if( !pSuperframe )
    {
        LogShort( LOG_ERROR, LOG_M_DLL, DLOG_genAdvDauxHdr, 1, g_unDllAdvSuperframe );
        return 0;
    }

    //jump over the advertisment options byte
    pTemp = p_pDauxHdr+1;

    //add the advertisment time synchronization info
    g_stDllAdvTimeSync.m_ulDllTaiSecond = g_ulDllTaiSeconds;

    //approximate the SFD sending time related to the timeslot template

    g_stDllAdvTimeSync.m_unDllTaiFraction = TMR0_TO_FRACTION2(g_unTMRStartSlotOffset) 
                                            + ((uint16)g_stTAI.m_uc250msStep << 13)
                                            + (g_unTxSFDFraction >> 5);
    
    memcpy( pTemp, &g_stDllAdvTimeSync, sizeof(DLL_ADV_TIME_SYNC));
    pTemp += sizeof(DLL_ADV_TIME_SYNC);

    //add the advertisment superframe information related 
    *(pTemp++) = (uint8)pSuperframe->m_stSuperframe.m_unTsDur;
    *(pTemp++) = (uint8)(pSuperframe->m_stSuperframe.m_unTsDur >> 8);
    
    if( pSuperframe->m_stSuperframe.m_unChIndex > 0x0F )
        return 0;   //wrong configuration
    
    *(pTemp++) = pSuperframe->m_stSuperframe.m_unChIndex << 1; // extended DL Uint
    *(pTemp++) = pSuperframe->m_stSuperframe.m_ucChBirth;
    
    //add the superframe fields
    pTemp = DLMO_InsertExtDLUint(pTemp, pSuperframe->m_stSuperframe.m_unSfPeriod);
   
    *(pTemp++) = pSuperframe->m_stSuperframe.m_unSfBirth << 1; //extended DL Uint
    
    //add the slow hopping info - 0 or 2 bytes
    if( pSuperframe->m_stSuperframe.m_unChRate > 1 )
    {
        ucAdvOpt |= DLL_ADV_OPT_SF_SLOW_HOP;   //set the superframe slow hopping flag
        pTemp = DLMO_InsertExtDLUint( pTemp, pSuperframe->m_stSuperframe.m_unChRate );
    }
    
    //add the channel map info - 0 or 2 bytes
    if( pSuperframe->m_stSuperframe.m_ucInfo & DLL_MASK_CH_MAP_OV )
    {
        ucAdvOpt |= DLL_ADV_OPT_SKIP_CH;
        *(pTemp++) = (uint8)pSuperframe->m_stSuperframe.m_unChMap;
        *(pTemp++) = (uint8)(pSuperframe->m_stSuperframe.m_unChMap >> 8);
    }

    //add the advertisment join information from the global "g_stDllAdvJoinInfo" structure
    pTemp = DLMO_InsertAdvJoinInfo(pTemp, &g_stDllAdvJoinInfo);
    
    //add the advertisment options byte at the beginning of the header
    p_pDauxHdr[0] = ucAdvOpt;

    //add the advertisment integrity check
    ulDauxCkSum = IcmpInterimChksum(p_pDauxHdr, pTemp-p_pDauxHdr, ulDauxCkSum);
    ulDauxCkSum = IcmpGetFinalCksum(ulDauxCkSum);
    
    *(pTemp++) = ulDauxCkSum >> 8;           
    *(pTemp++) = ulDauxCkSum; 

    return (pTemp-p_pDauxHdr);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Decode the DAUX header from a received message
/// @param  p_pucDauxHdr - pointer to the content of header containing also the
///                        integrity information(IEEE 16-bit ITU-T CRC)
/// @return 0 - if fails, DAUX header length if success
/// @remarks
///      Access level: Interrupt\n
///      Context: invoked by "MLDE_IncomingFrame" and "DLDE_AckInterpreter" routines
///               everytimes when the DLL receive from PHY a message containing an
///               DAUX header 
////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8 * MLDE_InterpretDauxHdr( const uint8* p_pucDauxHdr)
{
  uint8 ucAdvOptions;
  const uint8* pucTemp;


  //check the type of the DAUX header(the high significants 3 bits of the first byte)
  ucAdvOptions = p_pucDauxHdr[0];

  //jump over the first byte representing the advertisment options
  pucTemp = p_pucDauxHdr+1;

  switch( ucAdvOptions & DLL_DAUX_TYPE_MASK )
  {
  case DLL_ADVERTISMENT_DAUX << DLL_DAUX_TYPE_OFFSET:
      {
          //decode the time synchronization information
          memcpy( (void*) &g_stDllAdvTimeSync, (const void*) pucTemp, sizeof( DLL_ADV_TIME_SYNC ) );
          pucTemp += sizeof( DLL_ADV_TIME_SYNC );
    
          //the device TAI will be updated inside "DMAP_DLMO_IndicateDAUX" routine
          
    #ifndef BACKBONE_SUPPORT
          //decode the superframe informations
          g_stDAUXSuperframe.m_unTsDur = pucTemp[0] | (uint16)(pucTemp[1] << 8);
          
          g_stDAUXSuperframe.m_unChIndex = (pucTemp[2] >> 1) & 0x0F; //extended DL Uint
          pucTemp += 3;
          
          g_stDAUXSuperframe.m_ucChBirth = *(pucTemp++);
          
          pucTemp = DLMO_ExtractExtDLUint(pucTemp, &g_stDAUXSuperframe.m_unSfPeriod);
    
          g_stDAUXSuperframe.m_unSfBirth = (*(pucTemp++)) >> 1; //extended DL Uint
          
          //decode slow hopping info - 0, 1 or 2 bytes
          if( ucAdvOptions & DLL_ADV_OPT_SF_SLOW_HOP )
          {
              pucTemp = DLMO_ExtractExtDLUint(pucTemp, &g_stDAUXSuperframe.m_unChRate);
          }
          else
          { 
              // defaults for slotted hopping
              g_stDAUXSuperframe.m_unChRate = 1;
          }
          
          //decode channel map info - 0 or 2 bytes
          g_stDAUXSuperframe.m_unChMap = 0x7FFF;
          g_stDAUXSuperframe.m_ucInfo = 0;
          if( ucAdvOptions & DLL_ADV_OPT_SKIP_CH )
          {
              g_stDAUXSuperframe.m_ucInfo |= DLL_MASK_CH_MAP_OV;
              g_stDAUXSuperframe.m_unChMap = *(pucTemp++);
              g_stDAUXSuperframe.m_unChMap |= ((uint16)*(pucTemp++)) << 8;
          }     
                      
          // only in discovery mode the advJoinInfo should be used.
          // for a routing device the "g_stDllAdvJoinInfo" MIB will be overwrite by SM on join Response
          if (g_ucJoinStatus == DEVICE_DISCOVERY)
          {
              memset(&g_stDllAdvJoinInfo, 0, sizeof(g_stDllAdvJoinInfo));
              
              //set the Subnet ID according with the advertising router's Subnet ID  
              g_unDllSubnetId = g_stDllRxHdrExt.m_unSunbetId;
              
              //decode the advertisment join informations needed to generate the join links
              pucTemp = DLMO_ExtractAdvJoinInfo( pucTemp, &g_stDllAdvJoinInfo);             
          }
          else 
          {    
              // just to count the size of the Advertisement Join Info
              DLL_MIB_ADV_JOIN_INFO stAdv;              
              pucTemp = DLMO_ExtractAdvJoinInfo( pucTemp, &stAdv);
          }
          
    #else   //#ifndef BACKBONE_SUPPORT
          uint16 unVal;
    
          pucTemp += 4;     //TsDur(2 bytes) + ChIdx(1 byte) + ChBirth(1 byte)
          pucTemp = DLMO_ExtractExtDLUint(pucTemp, &unVal); //skip the Superframe Period bytes (1 or 2)
          pucTemp ++;       //skip Superframe Birth byte   
          
          //decode slow hopping info - 0 or 1 bytes
          if( ucAdvOptions & DLL_ADV_OPT_SF_SLOW_HOP )
          {
              pucTemp ++;
          }
          
          //decode channel map info - 0 or 2 bytes
          if( ucAdvOptions & DLL_ADV_OPT_SKIP_CH )
              pucTemp += 2;
          
          // just to count the size of the Advertisement Join Info
          DLL_MIB_ADV_JOIN_INFO stAdv;              
          pucTemp = DLMO_ExtractAdvJoinInfo( pucTemp, &stAdv);
          
    #endif  //#ifndef BACKBONE_SUPPORT          
          
          //check the Advertise DAUX integrity
          uint16 unChecksum = *(pucTemp++) << 8;
          unChecksum |= *(pucTemp++);    
           
          if( unChecksum )
          {
              uint32 ulDauxCkSum = 0;
              ulDauxCkSum = IcmpInterimChksum(p_pucDauxHdr, pucTemp-p_pucDauxHdr-2, ulDauxCkSum);
              ulDauxCkSum = IcmpGetFinalCksum(ulDauxCkSum);
              if( unChecksum != (uint16)ulDauxCkSum )              
              {
                  return NULL;
              }
          }  
    
          break;
      }
      
  case DLL_ACTIVATE_IDLE_LINK_DAUX << DLL_DAUX_TYPE_OFFSET:
  {
      uint16 unLinkId;

      // read the ID of the link that has to be activated
      pucTemp = DLMO_ExtractExtDLUint(pucTemp, &unLinkId);      
      // find the specified link; caller of this function must check that m_unLinkBacklogIdx is valid
      DLL_SMIB_ENTRY_LINK * pLink = (DLL_SMIB_ENTRY_LINK*) DLME_FindSMIB( SMIB_IDX_DL_LINK, unLinkId ); 
      
      if( pLink ) // activate the link
      {          
          pLink->m_stLink.m_ucActiveTimer = *pucTemp; //in 1/4 s units - according with DLL Changes            
          
          g_stDll.m_ucIdleLinksFlag = 1;
          g_ucHashRefreshFlag = 1;    // need to recompute the hash table   
          
          // Honeywell request: initialize m_ucBacklogLinkTimer value with link m_ucActiveTimer
          if( g_stDllMsgHrds.m_ucNeighborIdx < sizeof(g_aDllNeighborsTable)/sizeof(g_aDllNeighborsTable[0]) )
          {
              g_aDllNeighborsTable[g_stDllMsgHrds.m_ucNeighborIdx].m_stNeighbor.m_ucBacklogLinkTimer = pLink->m_stLink.m_ucActiveTimer;
          }     
      }
      
      pucTemp++;
      break;
  }
  
  case DLL_LQI_RSSI_REPORT_DAUX << DLL_DAUX_TYPE_OFFSET:
  {    
      int8  cRSSI   = (int8)*(pucTemp++) - (int8)ISA100_RSSI_OFFSET;  // RSSI reports are biased by +64 dBm
      uint8 ucRSQI  = *(pucTemp++);    
      break;
  }
 
  case DLL_SOLICITATION_DAUX << DLL_DAUX_TYPE_OFFSET:
  {
      uint16 unDauxTargetInterval, unDAUXSubnetID;
      
      unDauxTargetInterval = *(pucTemp++);
      unDauxTargetInterval |= ((uint16)*(pucTemp++)) << 8;   
      
      if( ucAdvOptions & 0x01 )
      {
          unDAUXSubnetID = *(pucTemp++);
          unDAUXSubnetID |= ((uint16)*(pucTemp++)) << 8;   
      }
      break;  
  }
  
  default:
      // not requested -> ignore
      return NULL;
  }
  
  return pucTemp;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Generate inside "g_aucDllRxTxBuf" one completed message to be
///         passed to the PHY layer for transmiting;\n
///         Message can be generated using the queue message with the
///         index "g_ucDllMsgQueueIndex", if exist, or using the data associated
///         with an DAUX header for advertisment generation;\n
/// @param  None
/// @return TX frame len
/// @remarks
///      Access level: Interrupt\n
///      Context: call by DLL Header Composer module
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 MLDE_OutgoingFrame()
{
    unsigned int ucHdrLen = 0;    
    
    uint8 ucAddrType;
    uint8 ucDHDR = 0;
    
    const SLME_KEY * pKey = SLME_GetDLLTxKey();
        
    //fill the first byte of the MHR's Frame Control field according with IEEE 802.15.4 MAC
    g_aucDllRxTxBuf[0] = 0x41;  // Reserved => Bit7 = 0 
                                // PAN ID Compression => Bit6 = 1(only one PAN ID is used) 
                                // Ack Request => Bit5 = 0(False)
                                // Frame Pending => Bit4 = 0(False)                                               
                                // Security Enabled => Bit3 = 0(False)
                                // Frame Type => Bit0Bit1Bit2 = 100(Data)
    DLL_SMIB_NEIGHBOR * pNeighbor;
    
    if( g_ucTxNeighborIdx < sizeof(g_aDllNeighborsTable) / sizeof(g_aDllNeighborsTable[0] ) )
    {
        pNeighbor = &g_aDllNeighborsTable[g_ucTxNeighborIdx].m_stNeighbor;        
    }
    else
    {
        pNeighbor = NULL;
    }
    
    if( !g_pDllMsg )
    {      
        //no message available in queue - message will contain just an advertisment/solicitation/ClockCorrection Request
        //must generate headers for a DLL queue message element
        
        memset((void*)&g_stDllMsgHrds, 0x00, sizeof(g_stDllMsgHrds));
                        
#ifndef BACKBONE_SUPPORT      
        if( pNeighbor )
        {
            //generate the clock correction request 
            g_stDllMsgHrds.m_hHandle = DLL_NEXT_HOP_HANDLE + 1;
            
            ucAddrType = 0x10 // frame version
                            | (SHORT_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) 
                            | (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET); 
            
            //ACK needed for time correction
            ucDHDR = DLL_MASK_NEED_ACK;
            
            //update the destination address field inside MHR header
            g_stDllMsgHrds.m_stPeerAddr.m_unShort = pNeighbor->m_unUID;    
            g_stDllMsgHrds.m_ucPeerAddrLen = 2;
        }
        else
#endif          
        {
            //generate the Advertisement message
            
            //change the first byte of the MHR's Frame Control field according with IEEE 802.15.4 MAC
            g_aucDllRxTxBuf[0] = 0x01;  // Reserved => Bit7 = 0 
                                        // PAN ID Compression => Bit6 = 0(only one address is present, the source address) 
                                        // Ack Request => Bit5 = 0(False)
                                        // Frame Pending => Bit4 = 0(False)                                               
                                        // Security Enabled => Bit3 = 0(False)
                                        // Frame Type => Bit0Bit1Bit2 = 100(Data)
    
            // fill the Frame Control field in MHR. (16 bit addressing. addressing mode will be overwritten later if required)
            ucAddrType = 0x10 // frame version
                            | (NO_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) 
                            | (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET); 
            
            //no ACK needed for broadcast message
            g_stDllMsgHrds.m_ucPeerAddrLen = 0; //no destination address present for broadcast messages                         
        }
    }
    else // have message
    {
        memcpy( &g_stDllMsgHrds, &g_pDllMsg->m_stHdrs, sizeof(DLL_MSG_HRDS) ); // headers only
                
        //ACK needed for non broadcast messages
        ucDHDR = DLL_MASK_NEED_ACK;
        
        if( g_stDllMsgHrds.m_ucPeerAddrLen == 2 ) // 
        {
            g_stDllMsgHrds.m_stPeerAddr.m_unShort = pNeighbor->m_unUID;
            ucAddrType = 0x10 // frame version
                            | (SHORT_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) 
                            | (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET); 
        }
        else
        {
            ucAddrType = 0x10 // frame version
                            | (EXTENDED_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) 
                            | (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET); 
        }
    }    
            
    if( (SHORT_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) == (ucAddrType & DLL_MHR_FC_DEST_ADDR_MASK) ) // not bcast message
    {
        //search if device know the EUI-64 of the destination device
        if( !pNeighbor || !memcmp( pNeighbor->m_aEUI64, c_aucInvalidEUI64Addr, 8) )
        {
            ucDHDR |= DLL_MASK_NEED_EUI64; //no neighbor found - need an EUI-64 of the destination device in ACK message
        }
    }
    //determine the value of the sequence number
    if( (++g_ucMsgSendCnt) == 0xFF )
    {
        //the value of 0xFF is reserved for TL Nonce 
        g_ucMsgSendCnt = 0;
    }
        
    //build the MHR(IEEE 802.15.4 MAC) header
    g_aucDllRxTxBuf[1] = ucAddrType;              // address type
    g_aucDllRxTxBuf[2] = g_ucMsgSendCnt;          // update the "Sequence number" field of the MHR
    g_aucDllRxTxBuf[3] = g_unDllSubnetId;         // fill the destination PAN ID. Matches the ISA100.11a MIB DllMib_SubnetID
    g_aucDllRxTxBuf[4] = g_unDllSubnetId >> 8;    // little endian
    
    uint8 * pTmp = g_aucDllRxTxBuf +  5;
    memcpy( pTmp, &g_stDllMsgHrds.m_stPeerAddr, g_stDllMsgHrds.m_ucPeerAddrLen );
    pTmp += g_stDllMsgHrds.m_ucPeerAddrLen;
    
    //add source address     
    if (g_unShortAddr)
    {
        *(pTmp++) = g_unShortAddr;
        *(pTmp++) = g_unShortAddr >> 8; // littke endian
    }
    else
    {
        memcpy( pTmp, &c_oEUI64LE, 8);
        pTmp += 8;
        
        //update the source addressing mode bits from "Frame Control" field
        g_aucDllRxTxBuf[1] |= (EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET); //Extended addressing mode
    }        
    
    if( pNeighbor )
    {        
        if(pNeighbor->m_ucInfo & DLL_MASK_NEIDIAG_LINK) //check if link quality needed in ACK/NACK
        {
            ucDHDR |= DLL_MASK_NEED_LQI;
        }
        if( pNeighbor->m_ucInfo & DLL_MASK_NEICLKSRC ) // check if clock needed in ACK/NACK
        {
            ucDHDR |= DLL_MASK_CLOCK_RECIPIENT;
        }
        
        // commHistory contains info about the last 8 attempted transmissions; for ex. if b0=0, the last Tx attempt failed
        // if b0=1, the last Tx attempt was succesfull (valid ACK/NACK received)
        pNeighbor->m_ucCommHistory <<= 1;     
    }
            
    //build DHDR header
    uint8 * pDHDR = pTmp++;
        
    //build DMXHR header
    *(pTmp++) = g_stDSMO.m_ucDLSecurityLevel + (KEY_ID_MODE << KEY_ID_MODE_OFFSET);   //1 octet length for Key Index field    
    *(pTmp++) = pKey->m_ucKeyID;      
    *(pTmp++) = g_ucDllCrtCh;  // virtual field
    
    //check if active superframe type is slow hopping
    if(g_aDllSuperframesTable[g_pCrtHashEntry->m_ucSuperframeIdx].m_stSuperframe.m_unChRate > 1)
    {
        ucDHDR |= DLL_MASK_SLOW_OFFSET;
        //save the slow hopping offset
        g_unCrtUsedSlowHoppingOffset = g_aDllSuperframesTable[g_pCrtHashEntry->m_ucSuperframeIdx].m_stSuperframe.m_unCrtSlowHopOffset;
        pTmp = DLMO_InsertExtDLUint( pTmp, g_unCrtUsedSlowHoppingOffset );
    }
    else
    {
        g_unCrtUsedSlowHoppingOffset = 0xFFFF;
    }
        
    //build DAUX header
    //check if the link is configured to send a DAUX header(advertisment, solicitation...) and is suficient space in frame
    unsigned int unDAUXLen = 0;
    unsigned int unAdvOpt = (g_pCrtHashEntry->m_ucLinkType & DLL_MASK_LNKDISC);
    if( (DLL_LNKDISC_ADV << DLL_ROT_LNKDISC) == unAdvOpt || ( DLL_LNKDISC_BURST << DLL_ROT_LNKDISC ) == unAdvOpt)
    {        
        //generate an DAUX header and compute the length
        unDAUXLen = MLDE_generateAdvDauxHdr ( pTmp );
    }
#ifdef IDLE_LINK_FULL_SUPPORT // need to decide if realy need idle links    
    else if( pNeighbor 
                && (pNeighbor->m_ucInfo & DLL_MASK_NEILINKBACKLOG) 
                && (DLDE_GetMsgNoInQueue(pNeighbor->m_unUID) > LINK_ACTIVATION_THRESHOLD) 
             )             
    {
        unDAUXLen = MLDE_generateActivateLinkDauxHdr ( pTmp );          
    }
#endif
    
    // check if is suficient space in frame DAUX 
    if( unDAUXLen )
    {
        ucDHDR |= DLL_MASK_NEED_DAUX; // has to be signalled in DHDR
        pTmp += unDAUXLen; //add the DAUX header inside message for outgoing
    }

    *pDHDR = g_stDllMsgHrds.m_ucDHDR = ucDHDR;
    
    if( g_stDllMsgHrds.m_ucPayloadLen )
    {
        uint8* pucNoDROUT = pTmp;   //in the case when no DROUT and DADDR needed
            
        //build DROUT header
        memcpy( pTmp, g_stDllMsgHrds.m_aDROUT, g_stDllMsgHrds.m_ucDROUTLen);
        pTmp += g_stDllMsgHrds.m_ucDROUTLen;
        
        //build DADDR header        
        *(pTmp++) = g_stDllMsgHrds.m_ucDADDRCtrlByte;
        
        pTmp = DLMO_InsertExtDLUint( pTmp, g_stDllMsgHrds.m_unDADDRSrcAddr );
        
        if( g_stDllMsgHrds.m_ucPeerAddrLen == 2 && g_stDllMsgHrds.m_unDADDRDstAddr == g_stDllMsgHrds.m_stPeerAddr.m_unShort)
        {
            pTmp = DLMO_InsertExtDLUint( pTmp, 0 );  // MHR dst and DADDR dst are the same; use zero value in DADDR
                                                     // obs: do not alter m_unDADDRDstAddr to zero becasue it is possible for 
                                                     // the retry mechanism to route this packet through another path
        }
        else
        {        
            pTmp = DLMO_InsertExtDLUint( pTmp, g_stDllMsgHrds.m_unDADDRDstAddr );
        }
        
        //copy the payload
        if( (pTmp + g_stDllMsgHrds.m_ucPayloadLen) <= g_aucDllRxTxBuf + (MAX_PHY_PAYLOAD_LEN-MIC_SIZE) ) // ck space (PHY-MIC)
        {
            memcpy( pTmp, g_pDllMsg->m_aPayload, g_stDllMsgHrds.m_ucPayloadLen);
            pTmp += g_stDllMsgHrds.m_ucPayloadLen;
        }
        else
        {
            pTmp = pucNoDROUT;                 // the DROUT and DADDR will not be sent       
            g_stDllMsgHrds.m_ucPayloadLen = 0; // don't add the payload if don't have space
            g_pDllMsg = NULL;                  // force the DPDU to not be discarded based on the received ACK 
        }
    }

    ucHdrLen = pTmp - g_aucDllRxTxBuf;
        
    //prepare the nonce
    memcpy(aucNonce, c_oEUI64BE, 8);
    *(uint32*)(aucNonce + 8) = MLSM_GetSlotStartMsBE(0);
    
    aucNonce[12] = g_ucMsgSendCnt;
        
    // g_aucDllRxTxBuf[1] - 0x10--10-- short short
    // g_aucDllRxTxBuf[1] - 0x11--10-- long short
    // g_aucDllRxTxBuf[1] - 0x10--11-- short long
    // g_aucDllRxTxBuf[1] - 0x11--11-- long long
    
    // 0 if DLL key, <>0 if provisioned DLL join key (use 8 byte address)
    if( g_aucDllRxTxBuf[1] & 0x44 )
    {
        PHY_AES_SetKeyInterrupt( g_aJoinDllKey );
    }
    else
    {
        PHY_AES_SetKeyInterrupt( pKey->m_aKey );
    }
    
    uint8 ucAuthLen;
    uint8 ucEncLen;
    
    if( g_stDSMO.m_ucDLSecurityLevel == SECURITY_MIC_32 ) // AUTH only
    {
        ucAuthLen = ucHdrLen;
        ucEncLen  = 0;
    }
    else // encryption
    {
        ucAuthLen = ucHdrLen - g_stDllMsgHrds.m_ucPayloadLen;
        ucEncLen  = g_stDllMsgHrds.m_ucPayloadLen;
    }
    
    AES_Crypt( g_pDllKey, g_stAlignedNonce.m_aucNonce, g_aucDllRxTxBuf, ucAuthLen, g_aucDllRxTxBuf+ucAuthLen, ucEncLen );
    memcpy(g_oAckPdu, pTmp, 4 ); //save the outgoing message's MIC
    
    // remove virtual field
    memmove( pDHDR+3, pDHDR + 4, pTmp + 4 - 4 - pDHDR );        
    return (ucHdrLen-1+MIC_SIZE); 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Analyze the message "g_aucDllRxTxBuf", received from PHY layer containing
///         (MHR and DHR headers, DPDU payload and MMIC) and decide the response type;\n
///         In the same time prepare also a message for queue, "g_stQueueMsg";
/// @param  p_pRxBuff   = 1 byte length + received packet
/// @return response type(ACK = 0, NACK0 = 1, NACK1 = 2, NACK2 = 3)
/// @remarks
///      Access level: Interrupt\n
///      Context: Called by the PHY layer everytimes when a new valid message is received
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 MLDE_IncomingFrame( uint8 * p_pRxBuff )
{
    uint8   *    pCrt = p_pRxBuff + 1;
    uint8   *    pEndFrame;
    
    pEndFrame = pCrt + p_pRxBuff[0] - MIC_SIZE;
    
    // add virtual field
    memmove( p_pRxBuff, pCrt, g_stDllRxHdrExt.m_ucSecurityPos ); 
    p_pRxBuff[ g_stDllRxHdrExt.m_ucSecurityPos ] = g_ucLastRFChannel;        
    
    // MHR and DHDR are loaded on MLDE_IncomingHeader()
    pCrt += g_stDllRxHdrExt.m_ucSecurityPos; 
    
    //DMXHR
    if( g_stDllMsgHrds.m_ucDHDR & DLL_MASK_SLOW_OFFSET ) 
    {
        uint16 unSlowHopOffset;
        pCrt = (uint8*)DLMO_ExtractExtDLUint( pCrt, &unSlowHopOffset ); //DMXHR header contain the slow hopping slot offset
    }
    
    if( pCrt > pEndFrame ) // invalid frame
        return DLL_DISCARD_MSG;
    
    g_stDllRxHdrExt.m_bIsAdvertise = 0;
    //DAUX
    //check if an DAUX header is present and verify it's integrity
    if( g_stDllMsgHrds.m_ucDHDR & DLL_MASK_NEED_DAUX ) //DAUX header exist
    {
        g_stDllRxHdrExt.m_bIsAdvertise = ((pCrt[0] & DLL_DAUX_TYPE_MASK) == (DLL_ADVERTISMENT_DAUX << DLL_DAUX_TYPE_OFFSET));
        pCrt = (uint8*)MLDE_InterpretDauxHdr( pCrt );
        
        if( !pCrt || pCrt > pEndFrame ) // invalid frame
            return DLL_DISCARD_MSG;


        if( g_stDllRxHdrExt.m_bIsAdvertise ) // advertise DAUX
        {
            if(g_ucDiscoveryState == DEVICE_DISCOVERY)
            {
    #ifndef BACKBONE_SUPPORT
                MLSM_SetCrtTaiTime( &g_stDllAdvTimeSync );
    #endif
                g_stDllMsgHrds.m_ucDROUTLen = 0;
                g_stDllMsgHrds.m_unDADDRSrcAddr = 0;
                g_stDllMsgHrds.m_unDADDRDstAddr = 0;
                return DLL_NO_ACK_SEND_MSG_DAUX;  //including broadcast messages
            }
        }
    }
    
    //check if DROUT and MESH header present in message
    if( pEndFrame > pCrt ) // remaining more bytes
    {
        //DROUT
        if( 0x07 == (pCrt[0] & 0x07) )             //more than 6 remaining hops
        {
            g_stDllMsgHrds.m_ucDROUTLen = 2;
        }
        else
        {
            g_stDllMsgHrds.m_ucDROUTLen = 1; 
        }
        
        if( !(pCrt[0] & DLL_DROUT_COMPRESS_MASK) )
        {
             g_stDllMsgHrds.m_ucDROUTLen += pCrt[g_stDllMsgHrds.m_ucDROUTLen] << 1;  
        }
                
        g_stDllMsgHrds.m_ucDROUTLen++;  //count also the third byte of DROUT - GraphId/EntriesNo depending on compress flag  
        
        if( g_stDllMsgHrds.m_ucDROUTLen > sizeof(g_stDllMsgHrds.m_aDROUT) ) //check oversize and negative result also
            return DLL_DISCARD_MSG;
        
        memcpy( g_stDllMsgHrds.m_aDROUT, pCrt, g_stDllMsgHrds.m_ucDROUTLen );
        pCrt += g_stDllMsgHrds.m_ucDROUTLen;
        
        //DADDR HEADER
        g_stDllMsgHrds.m_ucDADDRCtrlByte = *(pCrt++);
        pCrt = (uint8*)DLMO_ExtractExtDLUint(pCrt, &g_stDllMsgHrds.m_unDADDRSrcAddr);    //parse the DADDR source address
        pCrt = (uint8*)DLMO_ExtractExtDLUint(pCrt, &g_stDllMsgHrds.m_unDADDRDstAddr);    //parse the DADDR destination address 
        
        // restore DADDR src and dst address if necessary        
        if( !g_stDllMsgHrds.m_unDADDRSrcAddr && g_stDllMsgHrds.m_ucPeerAddrLen == 2)
        {
            g_stDllMsgHrds.m_unDADDRSrcAddr = g_stDllMsgHrds.m_stPeerAddr.m_unShort; 
        }
        if( !g_stDllMsgHrds.m_unDADDRDstAddr )
        {
            g_stDllMsgHrds.m_unDADDRDstAddr = g_unShortAddr; 
        }
        

#ifdef BACKBONE_SUPPORT
        if( g_stDllMsgHrds.m_ucDADDRCtrlByte & DLL_LAST_HOP_MASK )
        {
            //the message entered in subnet through the BBR and therefore shall not exit through the BBR
            //avoid circular routes
            return DLL_DISCARD_MSG;
        }
#endif          
    }
    else
    {
        g_stDllMsgHrds.m_unDADDRSrcAddr = 0;
        g_stDllMsgHrds.m_unDADDRDstAddr = 0;
        g_stDllMsgHrds.m_ucDROUTLen = 0;
    }
    
    if( g_stDllMsgHrds.m_ucNeighborIdx == 0xFF && g_stDllMsgHrds.m_ucPeerAddrLen == 2 ) // unknown neighbor
    {  
        if ( g_stDllRxHdrExt.m_bIsAdvertise ) // device is not on discovery here because DAUX parsing discard that case
        {
            MLSM_AddNewCandidate( g_stDllMsgHrds.m_stPeerAddr.m_unShort );
        }                
        Log( LOG_WARNING, LOG_M_DLL, DLOG_Indicate, 1, "4", g_stDllMsgHrds.m_ucPeerAddrLen, &g_stDllMsgHrds.m_stPeerAddr );
        return DLL_DISCARD_MSG_NOT_FOR_ME; // Actually msg is for me, but from uknown neighbor
    }
    
    g_stDllMsgHrds.m_ucPayloadLen = pEndFrame - pCrt;
    
    if( g_stDllMsgHrds.m_ucPayloadLen > DLL_MAX_DSDU_SIZE_DEFAULT ) //check oversize and negative result also
        return DLL_DISCARD_MSG;
        
   // decrypt settings was prepopulated on MLDE_IncomingHeader()
    const uint8* pucStart = pCrt;
    if (! (p_pRxBuff[g_stDllRxHdrExt.m_ucSecurityPos - 2 ] & SECURITY_ENC) ) // auth only packet
    {
        pCrt = pEndFrame;
    }
    g_stDllRxHdrExt.m_ucSecurityPos = pucStart - p_pRxBuff; // DLL payload pos 
    
    uint8 ucAESResult = AES_Decrypt( g_pDllKey, g_stAlignedNonce.m_aucNonce,
                                 p_pRxBuff, pCrt - p_pRxBuff,
                                 pCrt, pEndFrame + MIC_SIZE - pCrt );
   if( AES_SUCCESS != ucAESResult )
   {
      if( g_ucDiscoveryState == DEVICE_JOINED )
      {
          SLME_DL_FailReport();
      }
      return DLL_DISCARD_MSG;
    }

  #ifndef BACKBONE_SUPPORT
    if( g_stDllRxHdrExt.m_bIsAdvertise )
    {
        MLSM_SetCrtTaiTime( &g_stDllAdvTimeSync );
    }
  #endif       
    if( DLL_MSG_BROADCAST == g_ucMsgForMe )
    {
        return DLL_NO_ACK_SEND_MSG_BROADCAST;   //send message to the network layer - no ACK is necessary
    }
      
    g_stDllMsgHrds.m_ucPriority = (g_stDllMsgHrds.m_aDROUT[0] & DLL_DROUT_PRIORITY_MASK) >> DLL_DROUT_PRIORITY_ROT;
    
    //decide the DLL acknowledge type      
    g_stDllRxHdrExt.m_bDestIsNL = DLDE_CheckIfDestIsNwk(g_stDllMsgHrds.m_unDADDRDstAddr);
    if( !g_stDllRxHdrExt.m_bDestIsNL ) // msg is not for my NL; it must be routed to a neighbor
    {                    
        g_stDllRxHdrExt.m_bDiscardFlag = 0;
        // check if enough place in queue; check for discardable messages if necessary
	uint8 ucReason;
        uint8 ucQueueSpace = DLDE_CheckForwardQueueLimitation( g_stDllMsgHrds.m_ucPriority, &ucReason );
        if ( !ucQueueSpace ) // not any space available
        {
            g_stDllRxHdrExt.m_bDiscardFlag = 1;
            // check if that message cand discard another eligible message from queue
            // messages may be discarded from DLL queue only in favor of an incoming DPDU with DE=0,
            if( !(g_stDllMsgHrds.m_ucDADDRCtrlByte & DLL_DISCARD_ELIGIBLE_MASK) )
            {
                g_pDiscardMsg = DLDE_SearchDiscardableDllMsg(g_stDllMsgHrds.m_ucPriority);
                
                //also check to not exceed the allocated msgs by the dlmo11a.QueuePriority 
                if( !g_pDiscardMsg || 
                   ( ucReason && (g_pDiscardMsg->m_stHdrs.m_ucPriority > g_stDllMsgHrds.m_ucPriority || DLL_NEXT_HOP_HANDLE != g_pDiscardMsg->m_stHdrs.m_hHandle)) 
                   )
                {              
                    MLDE_composeAck( DLL_NACK0, pEndFrame );            
                    return DLL_NAK_DISCARD_MSG;
                }
            }       
        }
        else if ( ucQueueSpace == 1 )// a single space available
        {
            //   there is place on queue, but it is very close to congestion; signal this to sender
            MLDE_composeAck( DLL_ACK_ECN, pEndFrame  );
            g_ucDllECNStatus = 1;
            return DLL_ACK_SEND_MSG;
        }
    }   
    
    if( !g_stDllMsgHrds.m_ucDROUTLen )
    {
      #ifndef BACKBONE_SUPPORT      
        // is just a DLL request(clock synchro or EUI64 request)
        // if a clock repeater’s clock has expired and it is polled for a time update, it should respond with a NACK1
        if( (g_stDllMsgHrds.m_ucDHDR & DLL_MASK_CLOCK_RECIPIENT) && (g_stTAI.m_ucClkInterogationStatus != MLSM_CLK_INTEROGATION_OFF) )
        {
            MLDE_composeAck( DLL_NACK1, pEndFrame  );
        }
        else
        {
            MLDE_composeAck( DLL_ACK, pEndFrame  );
        }        
      #else
          MLDE_composeAck( DLL_ACK, pEndFrame  );
      #endif // #ifndef BACKBONE_SUPPORT
        
        Log( LOG_DEBUG, LOG_M_DLL, DLOG_Indicate, 1, "2" );        
        
        return DLL_NAK_DISCARD_MSG; 
    }
    
    //prepare ACK - used the global "g_oAckPdu" buffer and save the length inside "g_ucAckLen"
    MLDE_composeAck( DLL_ACK, pEndFrame  );
    
    //message will be saved on the queue or will be transmitted to the NWK Layer
    return DLL_ACK_SEND_MSG;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Mircea Vlasin
/// @brief  Analyze the message p_pRxBuff, received from PHY layer containing
///         (MHR + DHDR + DMXHR);\n
///         The length of p_pRxBuff must be at least PHY_MIN_HDR_SIZE;\n
///         In the same time prepare also a queue message's headers, "g_stDllMsgHrds";
/// @param  p_pRxBuff composed as 1 byte length + packet
/// @return 0 if error, 1 if need to continue to receive the packet
/// @remarks
///      Access level: Interrupt\n
///      Context: Called by the PHY layer everytimes when a new valid message is received
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 MLDE_IncomingHeader( uint8 * p_pRxBuff )
{
    unsigned int  unCrtHdrLen;
    
    if( p_pRxBuff[0] < 12 ) // PHY_MIN_HDR_SIZE
    {
        return 0;
    }    
    
    p_pRxBuff += 2; // go to MHR (skip length and frame type)   
          
    // MHR
    memcpy( &g_stDllRxHdrExt, p_pRxBuff, 4 );
    //filter subnet ID
    if( g_stDllRxHdrExt.m_unSunbetId != g_unDllSubnetId  )
    {
        if (   (g_ucJoinStatus != DEVICE_DISCOVERY)
            || ((g_stDllRxHdrExt.m_unSunbetId ^ g_stFilterTargetID) & g_stFilterBitMask) ) 
        {
            return 0;
        }
    }
    
    g_ucMsgForMe = DLL_MSG_FOR_ME;
        
    //destination address
    uint8 ucDestAddrMode = (g_stDllRxHdrExt.m_ucAddrType & DLL_MHR_FC_DEST_ADDR_MASK);
    
    if( (NO_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) == ucDestAddrMode )
    {
        g_ucMsgForMe = DLL_MSG_BROADCAST;
        unCrtHdrLen = DLL_MHR_DEST_ADDR_OFFSET;
    }
    else if( (SHORT_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) == ucDestAddrMode )
    {
        //check if my address is the link destination address
        if( g_unShortAddr != *(uint16*)(p_pRxBuff+DLL_MHR_DEST_ADDR_OFFSET) ) // not for me -> discard it
        {
            return 0;
        }
        
        unCrtHdrLen = DLL_MHR_DEST_ADDR_OFFSET + 2;  //short address using 2 bytes
    }
    else    //long destination address
    {
        //check if my address is the link destination address
        if( memcmp(p_pRxBuff + DLL_MHR_DEST_ADDR_OFFSET, c_oEUI64LE, 8) ) // not for me -> discard it
        {
            return 0;
        }
        
        unCrtHdrLen = DLL_MHR_DEST_ADDR_OFFSET + 8;  //long address using 8 bytes
    }
    
    //TODO - for Active Scanning, solicitation message not contains source address
    //update the global queue element
    
    //short source address
    if( (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET) == (p_pRxBuff[DLL_MHR_ADDR_TYPE_OFFSET] & DLL_MHR_FC_SRC_ADDR_MASK) )
    {
        g_stDllMsgHrds.m_stPeerAddr.m_aEUI64[0] = p_pRxBuff[unCrtHdrLen]; 
        g_stDllMsgHrds.m_stPeerAddr.m_aEUI64[1] = p_pRxBuff[unCrtHdrLen+1]; 
        
        g_stDllMsgHrds.m_ucNeighborIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, g_stDllMsgHrds.m_stPeerAddr.m_unShort );
        g_stDllMsgHrds.m_ucPeerAddrLen = 2;
        unCrtHdrLen += 2;
    }    
    // long source address 
    else if( (EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET) == (p_pRxBuff[DLL_MHR_ADDR_TYPE_OFFSET] & DLL_MHR_FC_SRC_ADDR_MASK) )
    {
        if( 0x01 == p_pRxBuff[-1] ) //no PAN ID compression suported just with short source address - Advertisements
        {
            return 0;   
        }
            
        if(  unCrtHdrLen != (DLL_MHR_DEST_ADDR_OFFSET+2) ) // accept short destination address only
        {
            return 0; 
        }
        
        g_stDllMsgHrds.m_ucNeighborIdx = 0xFF;
        memcpy( g_stDllMsgHrds.m_stPeerAddr.m_aEUI64, p_pRxBuff + unCrtHdrLen, 8 );
        g_stDllMsgHrds.m_ucPeerAddrLen = 8;
        unCrtHdrLen += 8;
    }
    else  // unsupported source address length
    {
        return 0;
    }
        
    // DHDR  
    g_stDllRxHdrExt.m_ucSecurityPos = 1 + unCrtHdrLen + 1 + 2; // virtual field position
    
    p_pRxBuff += unCrtHdrLen;
    g_stDllMsgHrds.m_ucDHDR = *(p_pRxBuff++); 
    
    // DMXHR      
     //Security Control byte 
    //Key ID field need to be 1 byte length (required by ISA)
    // current implementation accepy MIC on 32 only -> - xxx01001 or xxx01101
    
    if( (p_pRxBuff[0] & 0x1B) != (DEFAULT_ISA100_SEC_LAYER & 0x1B) )    
    {
         return 0;   
    }        
   
    uint16 unSlowOffset = 0xFFFF;
    
    uint16 unRxSFD = MLDE_getStartSlotTime( p_pRxBuff+2, 
                                           (uint32*)(aucNonce + 8), 
                                           &g_ulRcvTimestamp,
                                           &unSlowOffset );
   
    //read the "Sequence Number" field from MHR header
    //update the last byte of the nonce represented by "Configuration Byte"    
    aucNonce[12] = g_stDllRxHdrExt.m_ucSeqNo;
    
    if( DLL_MSG_FOR_ME == g_ucMsgForMe )
    {
        MLDE_preloadAck( unRxSFD, unSlowOffset );
    }
    
    if( g_stDllMsgHrds.m_ucNeighborIdx != 0xFF ) // known 2 bytes address
    {
        DLME_CopyReversedEUI64Addr( aucNonce, g_aDllNeighborsTable[g_stDllMsgHrds.m_ucNeighborIdx].m_stNeighbor.m_aEUI64 );
    }
    else // 8 bytes source address
    {
        DLME_CopyReversedEUI64Addr( aucNonce, g_stDllMsgHrds.m_stPeerAddr.m_aEUI64 );
    }
    
    // g_stDllMsgHrds.m_aMHR[DLL_MHR_ADDR_TYPE_OFFSET] - 0x10--10-- short short
    // g_stDllMsgHrds.m_aMHR[DLL_MHR_ADDR_TYPE_OFFSET] - 0x11--10-- long short
    // g_stDllMsgHrds.m_aMHR[DLL_MHR_ADDR_TYPE_OFFSET] - 0x10--11-- short long
    // g_stDllMsgHrds.m_aMHR[DLL_MHR_ADDR_TYPE_OFFSET] - 0x11--11-- long long
    
    // 0 if DLL key, <>0 if provisioned DLL join key (use 8 byte address)
    if( ( g_stDllRxHdrExt.m_ucAddrType & 0x44) || (g_ucDiscoveryState == DEVICE_DISCOVERY) )
    {
        PHY_AES_SetKeyInterrupt( g_aJoinDllKey );
    }
    else
    {
        const SLME_KEY * pKey = SLME_GetNonSessionKey( p_pRxBuff[1], SLM_KEY_USAGE_DLL );
        if( !pKey )
            return 0;
      
        PHY_AES_SetKeyInterrupt( pKey->m_aKey );
    }
        
    return 1;
}


uint16 MLDE_getStartSlotTime( const unsigned char * p_pSlowOffset, 
                              uint32 * p_pulPeerStartSlotTime, 
                              uint32 * p_pulCrtStartSlotTime, 
                              uint16 * p_punCrtSlowSlotOffset )
{
    uint16 unRxSFD;
    

    unRxSFD = TMR_GetSlotOffset() - PHY_GetLastRXTmr2Age() ;
        
    // if device is on slow hopping, 
    // may accept packet from different slot but only if slow offset is on packet
    DLL_SMIB_SUPERFRAME * pSuperframe = &g_aDllSuperframesTable[g_pCrtHashEntry->m_ucSuperframeIdx].m_stSuperframe;
    if( (pSuperframe->m_unChRate > 1) && (g_stDllMsgHrds.m_ucDHDR & DLL_MASK_SLOW_OFFSET) ) 
    {        
        uint16 unCrtSlowHopOffset = pSuperframe->m_unCrtSlowHopOffset;
        int    nCrtDiff = 0;
                
        if( unRxSFD >= 0x8000 ) // previous slot
        {
            unRxSFD += g_unDllTMR2SlotLength;
            if( !unCrtSlowHopOffset ) // was first slot on slow hopping
            {
                unCrtSlowHopOffset = pSuperframe->m_unChRate;
            }
            unCrtSlowHopOffset--;
            nCrtDiff = -1;
        }

        *p_pulCrtStartSlotTime = *p_pulPeerStartSlotTime = MLSM_GetSlotStartMsBE(nCrtDiff);
                        
        uint16 unPeerSlowHopOffset;
        DLMO_ExtractExtDLUint(p_pSlowOffset, &unPeerSlowHopOffset);
        
        if( unPeerSlowHopOffset != unCrtSlowHopOffset )
        {
            *p_punCrtSlowSlotOffset = unCrtSlowHopOffset;
            *p_pulPeerStartSlotTime = MLSM_GetSlotStartMsBE(MLDE_GetSlowOffsetDiff(unPeerSlowHopOffset-unCrtSlowHopOffset+nCrtDiff));
        }
    }
    else // not slow hopping case
    {
        *p_pulCrtStartSlotTime = *p_pulPeerStartSlotTime = MLSM_GetSlotStartMsBE(0);        
    }
        
    // A clock repeater should not send clock corrections to its neighbors through acknowledgements if 
    // it has not itself received a clock correction for a period that exceeds the ClockExpire attribute
    if(   ! (g_stDllMsgHrds.m_ucDHDR & DLL_MASK_CLOCK_RECIPIENT)
          || (unRxSFD >= 0x8000)
#ifndef BACKBONE_SUPPORT 
          || (g_stTAI.m_ucClkInterogationStatus != MLSM_CLK_INTEROGATION_OFF) 
#endif // #ifndef BACKBONE_SUPPORT     
            )
          {
              return 0xFFFF;
          }

    return TMR2_TO_FRACTION( unRxSFD ) ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Ion Ticus, Mircea Vlasin
/// @brief  Generates an acknowledge PDU (fills the g_oAckPdu[] buffer) and put
///         the effective length in "g_ucAckLen"(including MMIC)
/// @param  p_pSlowOffset - RX slow offset encoded buffer
/// @return None
/// @remarks
///      Access level: Interrupt\n
///      Context: Called by "MLDE_IncomingHeader"
////////////////////////////////////////////////////////////////////////////////////////////////////
void MLDE_preloadAck( uint16 p_unRxSFD, uint16 p_unCrtSlowHopOffset )
{    
    uint8 * pDHR;
    uint8 ucAckDhrFrmCtrl = BIT1; // no clock correction, no slow, ack type, no DAUX, MMIC32;
    uint8 ucDHROffset = 3;
    
    //fill the first byte of the MHR's Frame Control field according with IEEE 802.15.4 MAC
    g_oAckPdu[0] = 0x01;  // Reserved => Bit7 = 0 
                          // PAN ID Compression => Bit6 = 0(only one or no address is present) 
                          // Ack Request => Bit5 = 0(False)
                          // Frame Pending => Bit4 = 0(False)                                               
                          // Security Enabled => Bit3 = 0(False)
                          // Frame Type => Bit0Bit1Bit2 = 100(Data)
    
    g_oAckPdu[1] = 0x10;  // No Source Address => Bit6Bit7 = 00
                          // Frame Version => Bit4Bit5 = 10
                          // No Destination Address => Bit2Bit3 = 00
                          // Reserved => Bit0Bit1 = 00
    
    g_oAckPdu[2] = g_stDllRxHdrExt.m_ucSeqNo;
    
    pDHR = g_oAckPdu + 3; 
    if ( g_stDllMsgHrds.m_ucDHDR & DLL_MASK_NEED_EUI64 )
    {
        g_oAckPdu[1] |= EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET;    //Source 64 bit address present
        
        //add also the Subnet ID
        *(pDHR++) = (uint8)g_unDllSubnetId;
        *(pDHR++) = (uint8)(g_unDllSubnetId >> 8);
      
        memcpy(pDHR, c_oEUI64LE, 8);
        pDHR += 8;
        ucDHROffset += 2 + 8;
    }
    
    pDHR += 5;  //+1 for DHD header; +4 for virtual MIC field
                //the MIC will be added when entire message received
    
    if( p_unRxSFD != 0xFFFF ) // valid RxSFD 
    {
          ucAckDhrFrmCtrl |= BIT7; // include clock correction                          
          *(pDHR++) = p_unRxSFD;
          *(pDHR++) = p_unRxSFD>>8;
    }
    if( p_unCrtSlowHopOffset != 0xFFFF ) // valid value are between 0 and 32767 because extended uint encoding     
    {
        //current slow hopping offset different like the clock source's slow hopping timeslot offset
        ucAckDhrFrmCtrl |= BIT6; // include slow hopping timeslot offset
        pDHR = DLMO_InsertExtDLUint( pDHR, p_unCrtSlowHopOffset );
    }
    
    if( g_stDllMsgHrds.m_ucDHDR & DLL_MASK_NEED_LQI )
    {
        ucAckDhrFrmCtrl |= BIT3; // include DAUX in ACK        
        pDHR += MLDE_generateLqiRssiDauxHdr( pDHR );
    }  
    else if( 1 )     //need to check if the BacklogLink need to be activated 
    {
        if( g_aDllNeighborsTable[g_stDllMsgHrds.m_ucNeighborIdx].m_stNeighbor.m_ucInfo & DLL_MASK_NEILINKBACKLOG )
        {
            ucAckDhrFrmCtrl |= BIT3; // include DAUX
            
            *(pDHR++) = DLL_ACTIVATE_IDLE_LINK_DAUX << DLL_DAUX_TYPE_OFFSET; // DAUX header type
            
            pDHR = DLMO_InsertExtDLUint(pDHR, g_aDllNeighborsTable[g_stDllMsgHrds.m_ucNeighborIdx].m_stNeighbor.m_unLinkBacklogIdx); //idle link UID 
            *(pDHR++) = g_aDllNeighborsTable[g_stDllMsgHrds.m_ucNeighborIdx].m_stNeighbor.m_ucLinkBacklogDur;   //activating duration
        }
    }
    
    g_oAckPdu[ucDHROffset] = ucAckDhrFrmCtrl;
    
    g_ucAckLen = pDHR - g_oAckPdu;    //include also the virtual MIC
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Ion Ticus, Mircea Vlasin
/// @brief  Generates an acknowledge PDU (fills the g_oAckPdu[] buffer) and put
///         the effective length in "g_ucAckLen"(including MMIC)
/// @param  p_ucAckType - the type of acknowledge (ACK = 0, ACK_ECN = 1, NACK0 = 2, NACK1 = 3)
/// @param  p_pucRxMIC - the MIC of the incoming message
/// @return None
/// @remarks
///      Access level: Interrupt\n
///      Context: Called by "MLDE_IncomingFrame" when ACK/NACK needed as response
////////////////////////////////////////////////////////////////////////////////////////////////////
void MLDE_composeAck(uint8  p_ucAckType, const uint8 * p_pucRxMIC)
{
  uint16 unDHROffset = 3;  
  
  //g_oAckPdu was prepopulated on MLDE_preloadAck() 
  if( (g_oAckPdu[1] & DLL_MHR_FC_SRC_ADDR_MASK) == (EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET) )   
  {
      //Source EUI64 address and Subnet ID present
      unDHROffset += 2 + 8;
  }
  
  if( p_ucAckType != DLL_ACK )
  {
      g_oAckPdu[unDHROffset] |= (p_ucAckType << DHR_ACK_TYPE_OFFSET); // no clock correction, no slow, ack type, no DAUX, MMIC32;
  }

  unDHROffset++;
  memcpy( g_oAckPdu + unDHROffset, p_pucRxMIC, 4 ); // set virtual MIC

  memcpy( g_stAlignedNonce.m_aucNonce, c_oEUI64BE, 8 ); 
  *(uint32*)(g_stAlignedNonce.m_aucNonce+8) = g_ulRcvTimestamp; // set the timestamp of receiving packet
  
  AES_Crypt( g_pDllKey, g_stAlignedNonce.m_aucNonce,
                  g_oAckPdu, g_ucAckLen, // include echo MIC
                  g_oAckPdu + g_ucAckLen, 0 ); 
      
      
  if( g_ucAckLen > unDHROffset )
  {
      memmove( g_oAckPdu + unDHROffset, g_oAckPdu + unDHROffset + 4, g_ucAckLen - unDHROffset ); // remove virtual field
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Provide necessary initializations
/// @param  None
/// @return None
/// @remarks
///      Access level: User\n
///      Context: Called at DLL layer initialization
////////////////////////////////////////////////////////////////////////////////////////////////////
void MLDE_Init()
{
  g_ucMsgSendCnt = 0xFF;
 
  g_stCandidates.m_ucCandidateNo = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Set the DLL layer discovery status  
/// @param  p_ucState - discovery state
/// @return None
/// @remarks
///      Access level: User
////////////////////////////////////////////////////////////////////////////////////////////////////
void MLDE_SetDiscoveryState( uint8 p_ucState)
{
  g_ucDiscoveryState = p_ucState;
}

int MLDE_GetSlowOffsetDiff( int p_nDiff )
{
    uint16 unChRate = g_aDllSuperframesTable[g_pCrtHashEntry->m_ucSuperframeIdx].m_stSuperframe.m_unChRate;
    if( p_nDiff )
    {
        if( p_nDiff > 0 )
        {
            if( p_nDiff > (unChRate / 2) )
            {
                p_nDiff -= unChRate;
            }
        }
        else // p_nDiff < 0 
        {
            if( -p_nDiff > unChRate / 2 )
            {
                p_nDiff += unChRate;
            }
        }
    }
    
    return p_nDiff;
}
