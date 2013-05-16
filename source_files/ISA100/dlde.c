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
/// Author:       NIVIS LLC, Dorin Pavel, Eduard Erdei
/// Date:         December 2007
/// Description:  This file implements the upper sub-layer of data link layer
/// Changes:      Rev 1.0 - created
/// Revisions:    1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "nlme.h"   // just for the visibility of the short address
#include "nlde.h"
#include "tlde.h"   // just for the visibility of the TLDE_DISCARD_ELIGIBILITY_MASK
#include "dlme.h"
#include "mlde.h"
#include "mlsm.h"
#include "dlmo_utils.h"
#include "dmap.h"
#include "dmap_dmo.h"
#include "tmr_util.h"
#include "dlde.h"
#include "../asm.h"

/* DLL message queue */
DLL_DPDU g_aDllMsgQueue[DLL_MSG_QUEUE_SIZE_MAX];

/* chosen message within the queue */
DLL_DPDU * g_pDllMsg;

/* actual queue size */
DLL_DPDU * g_pDllMsgQueueEnd;

/* current HashTable element  */
DLL_HASH_STRUCT *g_pCrtHashEntry;

uint8 g_ucTxNeighborIdx;
uint8 g_ucPingStatus;

DLL_DPDU * g_pDiscardMsg;

/***************************** local functions ********************************/
static void DLDE_Init (void);
static void DLDE_remMsgFromQueue ( DLL_DPDU * p_pDllMsg, uint8 p_ucConfirmReason );
static DLL_DPDU * DLDE_reqMsgFromQueue (DLL_SMIB_LINK * p_pLink);

static uint8 DLDE_tryToExtendGraph(uint16 p_unGraphID, uint16 unFinalDestAddr);

static void   DLDE_routeUsingLongAddr(uint8 * p_pDstAddr,  DLL_MSG_HRDS*   p_pMsgHrds);
static __monitor uint8  DLDE_copyRouteInfoToMsg(DLL_SMIB_ROUTE * p_pRoute,  DLL_MSG_HRDS*   p_pMsgHrds);
#ifdef BACKBONE_SUPPORT
  static __monitor uint8  DLDE_findRoute(uint8 p_routeType, uint16 p_unUID, SHORT_ADDR p_unNwkSrcAddr);
  #define DLDE_searchRoute(p_routeType, p_unUID, p_unNwkSrcAddr) DLDE_findRoute(p_routeType, p_unUID, p_unNwkSrcAddr)
#else
  static __monitor uint8  DLDE_findRoute(uint8 p_routeType, uint16 p_unUID);
  #define DLDE_searchRoute(p_routeType, p_unUID, p_unNwkSrcAddr) DLDE_findRoute(p_routeType, p_unUID)
#endif

static uint8  DLDE_retryMsg(void);
static __monitor uint8 DLDE_addMsgWithLinkCk (const DLL_MSG_HRDS* p_pMsgHrds, const uint8 * p_pPayload );

static uint8 DLDE_selectGraphNeighbor(DLL_SMIB_ENTRY_GRAPH * p_pstGraph, uint8 p_ucRetryNo, uint8 p_ucPrevNeighborIdx);
static void DLDE_setMsgGraphNeighbor(DLL_MSG_HRDS * p_pDllMsgHdr,  DLL_SMIB_ENTRY_GRAPH * p_pstGraph);
static DLL_SMIB_NEIGHBOR *  DLDE_syncMsgNeighborIdx( DLL_MSG_HRDS * p_pDllMsgHdr );


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Dorin Pavel
/// @brief  Initializes the DLL layer
/// @param  none
/// @return none
/// @remarks
///      Access level: User
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLL_Init(void)
{
  MLDE_Init();
  DLDE_Init();
  MLSM_Init();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Dorin Pavel
/// @brief  Initializes the DLL upper sublayer
/// @param  none
/// @return none
/// @remarks
///      Access level: User
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_Init(void)
{
  // DLL queue initialisation
  g_pDllMsgQueueEnd = g_aDllMsgQueue;  
  g_pDllMsg = NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Eduard Erdei
/// @brief  Removes a message from the queue, specified by an application handle
/// @param  p_unHandle - the nandle of the message that has to be removed
/// @return none
/// @remarks
///      Access level: Interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_DiscardMsg( uint16 p_unHandle )
{   
  DLL_DPDU * pMsg = g_aDllMsgQueue;
  
  MONITOR_ENTER();  // start exclusive area
  
  DLL_DPDU * pEnd = g_pDllMsgQueueEnd;
  
  while( pMsg < pEnd )
  {
      if( pMsg->m_stHdrs.m_hHandle == p_unHandle )
      {
          pMsg->m_stHdrs.m_ucLifetime = (uint8)g_ulDllTaiSeconds + 1;
          pMsg->m_stHdrs.m_hHandle    = DLL_NEXT_HOP_HANDLE;  // avoid confirmation of the msg to the upper layers
          pMsg->m_stHdrs.m_ucRetryNo  = 0xF0; // force delete
          break; 
      }
      pMsg++;
  }
  
  MONITOR_EXIT();    
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Dorin Pavel
/// @brief  Removes a message from the queue and updates the queue size
/// @param  p_ucConfirmReason reason of confirmation 
/// @return none
/// @remarks
///      Access level: Interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_remMsgFromQueue( DLL_DPDU * p_pDllMsg, uint8 p_ucConfirmReason )
{
  if ( (g_aDllMsgQueue <= p_pDllMsg) && (p_pDllMsg < g_pDllMsgQueueEnd) )
  {
      HANDLE unCurrentHandle = p_pDllMsg->m_stHdrs.m_hHandle;
          
      g_pDllMsgQueueEnd--;
      memmove( p_pDllMsg, p_pDllMsg+1, (uint8*)g_pDllMsgQueueEnd-(uint8*)p_pDllMsg );

      if (DLL_NEXT_HOP_HANDLE != unCurrentHandle) // handle belongs to upper layers
      {
          DLDE_Data_Confirm(unCurrentHandle, p_ucConfirmReason );
      }    
      
      g_pDllMsg = NULL; // pointer may be corrupted
  }  
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Eduard Erdei 
/// @brief  Searches a discard eligible message in the queue, and deletes it if the message priority
///         is equal or lower than p_ucPriority
/// @param  p_ucPriority - priority threshold 
/// @return  disrad eligible message
/// @remarks
///      Access level: Interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////
DLL_DPDU * DLDE_SearchDiscardableDllMsg(uint8 p_ucPriority)
{
  DLL_DPDU * pMsg;
  
  for( pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++ )
  {
      if( (pMsg->m_stHdrs.m_ucDADDRCtrlByte & DLL_DISCARD_ELIGIBLE_MASK)
          && pMsg->m_stHdrs.m_ucPriority <= p_ucPriority )
      {
          return pMsg;
      }
  }
  
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Dorin Pavel, Mircea Vlasin
/// @brief  Looks for a certain message within the queue
/// @param  none
/// @return message or NULL if message not found
/// @remarks
///      Access level: Interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////
DLL_DPDU * DLDE_reqMsgFromQueue (DLL_SMIB_LINK * p_pLink)
{
    DLL_DPDU * pMsg;    
    g_ucTxNeighborIdx = 0xFF;
    
    if( p_pLink->m_mfLinkType & DLL_MASK_LNKJOIN ) // link join -> search for join responses first
    {
        for(pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++ )
        {
            if( 8 == pMsg->m_stHdrs.m_ucPeerAddrLen )
            {
                return pMsg;
            }
        }
    }
    
    // search within the message queue for the passed destination
    uint8 ucTypeOptions = p_pLink->m_mfTypeOptions;
    DLL_SMIB_NEIGHBOR * pNeighbor;
    
    if(  ucTypeOptions & DLL_MASK_LNKGR ) // graph link
    {
        uint16 unGraphId = p_pLink->m_unGraphId;
        for(pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++ )
        {                
            if( unGraphId == pMsg->m_stHdrs.m_unGraphID )
            {            
                pNeighbor = DLDE_syncMsgNeighborIdx( &pMsg->m_stHdrs );
                
                if( pNeighbor)
                {
                    if( p_pLink->m_mfLinkType & DLL_MASK_LNKIDLE)
                    {  
                        // if idle link, check if it was activated for this neighbour
                        if( !pNeighbor->m_ucBacklogLinkTimer || pNeighbor->m_unLinkBacklogIdx != p_pLink->m_unUID )
                        {
                            continue;
                        }                      
                    }
                    
                    g_ucTxNeighborIdx = pMsg->m_stHdrs.m_ucNeighborIdx;
                    return pMsg;
                }
                
                if(    (pMsg->m_stHdrs.m_ucGraphIdx >= g_ucDllGraphsNo)       // check graph IDX consistency
                    || (g_aDllGraphsTable[pMsg->m_stHdrs.m_ucGraphIdx].m_stGraph.m_unUID != unGraphId) )
                {
                    pMsg->m_stHdrs.m_ucGraphIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_GRAPH, unGraphId);
                }

                if( pMsg->m_stHdrs.m_ucGraphIdx != 0xFF )
                {
                    DLDE_setMsgGraphNeighbor(&pMsg->m_stHdrs, g_aDllGraphsTable + pMsg->m_stHdrs.m_ucGraphIdx );
                    if( pMsg->m_stHdrs.m_ucNeighborIdx < 0xFF && !g_aDllNeighborsTable[pMsg->m_stHdrs.m_ucNeighborIdx].m_stNeighbor.m_unNACKBackoffTmr )
                    {
                        if( p_pLink->m_mfLinkType & DLL_MASK_LNKIDLE)
                        {  
                            // if idle link, check if it was activated for this neighbour
                            if( !g_aDllNeighborsTable[pMsg->m_stHdrs.m_ucNeighborIdx].m_stNeighbor.m_ucBacklogLinkTimer 
                                || g_aDllNeighborsTable[pMsg->m_stHdrs.m_ucNeighborIdx].m_stNeighbor.m_unLinkBacklogIdx != p_pLink->m_unUID )
                            {
                                continue;
                            }                      
                        }  
                        
                        g_ucTxNeighborIdx = pMsg->m_stHdrs.m_ucNeighborIdx;
                        return pMsg;
                    }
                }
            }
        }
    }
    
    uint16 unNeighbor = p_pLink->m_unNeighbor;
    if( ucTypeOptions & DLL_LNK_NEIGHBOR_MASK ) // DLL_NEIGHBOR
    {
        for(pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++)
        {
            if( 2 == pMsg->m_stHdrs.m_ucPeerAddrLen )
            {
                if( pMsg->m_stHdrs.m_stPeerAddr.m_unShort == unNeighbor ) // is next hop address
                {
                    pNeighbor = DLDE_syncMsgNeighborIdx( &pMsg->m_stHdrs );                
                    if( pNeighbor )
                    {
                        g_ucTxNeighborIdx = pMsg->m_stHdrs.m_ucNeighborIdx;
                        return pMsg;
                    }
                }
                
                if(       pMsg->m_stHdrs.m_unDADDRDstAddr == unNeighbor 
                      &&  pMsg->m_stHdrs.m_unDADDRDstAddr != pMsg->m_stHdrs.m_stPeerAddr.m_unShort ) // is final address
                {                      
                    if( (pMsg->m_stHdrs.m_ucFinalIdx >= g_ucDllNeighborsNo) // check final neighbor IDX consistency
                       ||(pMsg->m_stHdrs.m_stPeerAddr.m_unShort != g_aDllNeighborsTable[pMsg->m_stHdrs.m_ucFinalIdx].m_stNeighbor.m_unUID) )
                    {
                        pMsg->m_stHdrs.m_ucFinalIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, pMsg->m_stHdrs.m_unDADDRDstAddr );
                    }
                    
                    if( pMsg->m_stHdrs.m_ucFinalIdx < 0xFF && !g_aDllNeighborsTable[pMsg->m_stHdrs.m_ucFinalIdx].m_stNeighbor.m_unNACKBackoffTmr )
                    {
                        g_ucTxNeighborIdx = pMsg->m_stHdrs.m_ucFinalIdx;
                        return pMsg;
                    }
                }                
            }
        }
        g_ucTxNeighborIdx = g_pCrtHashEntry->m_ucNeighborIdx;
    }
    else if( ucTypeOptions & DLL_LNK_GROUP_MASK ) // DLL_GROUP
    {
        uint8 ucGroupCode = (uint8)unNeighbor;
        for(pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++ )
        {           
            if( 2 == pMsg->m_stHdrs.m_ucPeerAddrLen )
            {
                pNeighbor = DLDE_syncMsgNeighborIdx( &pMsg->m_stHdrs );
                
                if( pNeighbor && ucGroupCode == pNeighbor->m_ucGroupCode )
                {
                    if( p_pLink->m_mfLinkType & DLL_MASK_LNKIDLE )
                    {
                        // it's an idle link; check if it was designated for this neighbour
                        if( !pNeighbor->m_ucBacklogLinkTimer || pNeighbor->m_unLinkBacklogIdx != p_pLink->m_unUID )
                        {
                            continue;
                        }
                    }
                    
                    g_ucTxNeighborIdx = pMsg->m_stHdrs.m_ucNeighborIdx;
                    return pMsg;
                }
            }
        }
    }
    
  return NULL;    
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author LLC Nivis, Dorin Pavel, Mircea Vlasin
/// @brief  Link scheduler, looks for an active link within a certain timeslot
/// @param  none
/// @return Link type
/// @remarks
///      Access level: Interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_ScheduleLink (void)
{  
  DLL_HASH_STRUCT * pCrtHashStruct = g_stDll.m_aHashTable;
  
  for( ;pCrtHashStruct < g_stDll.m_aHashTable+g_stDll.m_ucHashTableSize; pCrtHashStruct ++ )
  {
    DLL_SMIB_LINK * pLink =  &g_aDllLinksTable[pCrtHashStruct->m_ucLinkIdx].m_stLink;
    uint16 unSlotNo = g_aDllSuperframesTable[pCrtHashStruct->m_ucSuperframeIdx].m_stSuperframe.m_unCrtOffset;
    uint8  ucLinkType = (pLink->m_mfTypeOptions & DLL_MASK_LNKSC);

    if ( 0 == ucLinkType ) // 0 - offset only
    {
      if( pLink->m_aSchedule.m_aOffset.m_unOffset != unSlotNo )
      {
        continue; // not now 
      }
    }
    else if ( (1 << DLL_ROT_LNKSC) == ucLinkType ) // 1 - offset & interval
    {
      if(  unSlotNo < pLink->m_aSchedule.m_aRange.m_unFirst )
      {
        continue; // not now 
      }
      if( (unSlotNo - pLink->m_aSchedule.m_aOffsetInterval.m_unOffset) % pLink->m_aSchedule.m_aOffsetInterval.m_unInterval )
      {
          continue; // not now 
      }
    }
    else if ( (2 << DLL_ROT_LNKSC) == ucLinkType ) // 2 - range
    {
      if(  unSlotNo < pLink->m_aSchedule.m_aRange.m_unFirst )
      {
        continue; // not now 
      }
      if( unSlotNo > pLink->m_aSchedule.m_aRange.m_unLast )
      {
        continue; // not now 
      }
    }
    else // 3 - bitmap
    {
      if (  (32 <= unSlotNo)
          ||( (32 > unSlotNo) && (0 == ((pLink->m_aSchedule.m_ulBitmap >> unSlotNo) & 1)) )  )
      {
        continue; // not now 
      }
    }

    // active link found
    g_pCrtHashEntry  = pCrtHashStruct;

    // get link type
    ucLinkType = pCrtHashStruct->m_ucLinkType;

    if ( ucLinkType & DLL_MASK_LNKTX ) // Tx link
    {   
        g_pDllMsg = DLDE_reqMsgFromQueue(pLink);        
      
        if( g_pDllMsg ) // message found
        {                    
            //check the shared link
            if( ucLinkType & DLL_MASK_LNKEXP )
            {
                if( SFC_SUCCESS == MLSM_UpdateShareLinkExpBackoff(SKIP_LINK) )
                {
                    continue; //check the next links
                }
            }
            
            return SFC_TX_LINK;
        }
        // validation to avoid Advertisment/Synchro message sending by one routing device that is not fully joined
        else if( DEVICE_JOINED == g_ucDiscoveryState )// not message found on a joined device
        {
#ifndef BACKBONE_SUPPORT                  
            if(g_ucTxNeighborIdx != 0xFF)
            {
                DLL_SMIB_NEIGHBOR * pNeighbor  = &g_aDllNeighborsTable[g_ucTxNeighborIdx].m_stNeighbor;
                if(  (pNeighbor->m_ucInfo & DLL_MASK_NEICLKSRC)               
                && ( (g_stTAI.m_ucClkInterogationStatus == MLSM_CLK_INTEROGATION_ON)  // clockExpired interval or custom ping for PER calculation
                     || (   pNeighbor->m_pstDiag 
                         && (pNeighbor->m_pstDiag->m_unFromLastCommunication >= g_stDMO.m_unNeighPingInterval) )  
                    )
                  )
                {
                    // no message but send a blank message to the preffered/secondary source clock for ACK time synchronization and for custom PER estimation
                    return SFC_TX_LINK;
                }
            }
#endif            
            if( (ucLinkType & DLL_MASK_LNKDISC) && ((ucLinkType & DLL_MASK_LNKDISC) != DLL_MASK_LNKDISC) )
            {   
                g_ucTxNeighborIdx = 0xFF;
                return SFC_TX_LINK; // discovery link to send advertisement
            }
        }
    }
    else if (ucLinkType & DLL_MASK_LNKRX) // a Rx link
    {       
        return SFC_RX_LINK;
    }
    else // delegated link
    { 
    }
  }

  return SFC_NO_LINK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Accepts payload from the network layer (or transceiver if BACKBONE_SUPPORT),
///         selects the route through the DLL subnet, fills the available DLL subheaders,
///         and places a message on the device's DLL message queue
/// @param  p_ucDstSrcAddrLen - the destination(and source) address length (8 or 4)
/// @param  p_pDstSrcAddr - Network destination address + source address (if length is 4)
/// @param  p_ucPriorityAndFlags - message priority, Discard Eligibility, ECN
/// @param  p_unContractID - contract ID of the payload
/// @param  p_ucPayloadLength - payload length
/// @param  p_pPayload - payload buffer
/// @param  p_hHandle - application level handler
/// @return none
/// @remarks
///      Access level: User or Interrupt if BACKBONE_SUPPORT
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_Data_Request(uint8        p_ucDstSrcAddrLen,
                       uint8 *      p_pDstSrcAddr,
                       uint8        p_ucPriorityAndFlags,
                       uint16       p_unContractID,
                       uint8        p_ucPayloadLength,
                       const void * p_pPayload,
                       HANDLE       p_hHandle)
{
  DLL_MSG_HRDS  stMsgHrds;
  memset(&stMsgHrds, 0, sizeof(stMsgHrds));

  Log( LOG_DEBUG, LOG_M_DLL, DLOG_DataReq, 2, &p_hHandle, p_ucDstSrcAddrLen, p_pDstSrcAddr );
  
  WCI_Log( LOG_M_DLL, DLOG_DataReq, p_ucDstSrcAddrLen, p_pDstSrcAddr,
                                    sizeof(p_ucPriorityAndFlags), &p_ucPriorityAndFlags,
                                    sizeof(p_unContractID), &p_unContractID,
                                    (uint8)p_ucPayloadLength, (uint8 *)p_pPayload,
                                    sizeof(p_hHandle), &p_hHandle );
  
  if (p_ucPayloadLength > g_ucMaxDSDUSize )
  {
     DLDE_Data_Confirm(p_hHandle, SFC_INVALID_SIZE);
     return;
  }
  
  stMsgHrds.m_hHandle = p_hHandle;
  
  // init the DPDU time to live, as the curent TAI in seconds + g_unMaxLifetime in seconds
  // if graph routing will be used, it will be overwritten later
  stMsgHrds.m_ucLifetime = (g_unMaxLifetime >> 2);
  
  // fill the priority field in the DROUT header
  stMsgHrds.m_ucPriority = p_ucPriorityAndFlags & 0x0F; //b3b2 - Contract Priority
                                                        //b1b0 - Message Priority
  
  // fill the Frame Control field in MHR. (16 bit addressing.
  // addressing mode will be overwritten later if required)
  
  stMsgHrds.m_ucDADDRCtrlByte = (p_ucPriorityAndFlags & TLDE_ECN_MASK) >> 1;
  if( p_ucPriorityAndFlags & TLDE_DISCARD_ELIGIBILITY_MASK )
  {
      stMsgHrds.m_ucDADDRCtrlByte |= DLL_DISCARD_ELIGIBLE_MASK;
  }  
      
#ifdef BACKBONE_SUPPORT
  //the message entered in subnet through the BBR  
  stMsgHrds.m_ucDADDRCtrlByte |= DLL_LAST_HOP_MASK;
#endif  
  
  // Hierarchical route selection (see page 10 in ISA_SP100_DLL_Draft-Standard rev.2)
  stMsgHrds.m_aDROUT[0] = stMsgHrds.m_ucPriority << 3;
  
  if ( 8 == p_ucDstSrcAddrLen ) 
  {
      //Assume this is a single-hop scenario, and use this address in the IEEE MAC
      stMsgHrds.m_ucPeerAddrLen = 8;
      
      stMsgHrds.m_unDADDRSrcAddr = 0; 
      stMsgHrds.m_unDADDRDstAddr = 0;          
      
      DLDE_routeUsingLongAddr(p_pDstSrcAddr, &stMsgHrds);
  }
  else
  {// it is sure that the next hop addr will be 2bytes
    uint8 ucRouteTableIdx = 0xFF;
    uint8 ucRouteSFC = SFC_INVALID_ADDRESS;    

    stMsgHrds.m_ucPeerAddrLen = 2; //Network destination address + source address length       
    
    stMsgHrds.m_unDADDRSrcAddr = (((uint16)p_pDstSrcAddr[2]) << 8 ) | p_pDstSrcAddr[3];
    stMsgHrds.m_unDADDRDstAddr = (((uint16)p_pDstSrcAddr[0]) << 8 ) | p_pDstSrcAddr[1];
    
    if( stMsgHrds.m_unDADDRSrcAddr == g_unShortAddr )
    {
        stMsgHrds.m_unDADDRSrcAddr = 0; 
    }
    
    MONITOR_ENTER(); // start exclusive area
     
    if (p_unContractID) // 2. The ContractID is associated with a particular route
    {
    #ifdef BACKBONE_SUPPORT
        ucRouteTableIdx = DLDE_searchRoute(DLL_RTSEL_BBR_CONTRACT, p_unContractID, stMsgHrds.m_unDADDRSrcAddr );
    #else
        ucRouteTableIdx = DLDE_searchRoute(DLL_RTSEL_DEV_CONTRACT, p_unContractID, stMsgHrds.m_unDADDRSrcAddr );
    #endif
    }
    
    if( ucRouteTableIdx >= g_ucDllRoutesNo )
    {
        ucRouteTableIdx = DLDE_searchRoute( DLL_RTSEL_DSTADDR, stMsgHrds.m_unDADDRDstAddr, stMsgHrds.m_unDADDRSrcAddr );
    }

    if (ucRouteTableIdx >= g_ucDllRoutesNo)
    {
        ucRouteTableIdx = DLDE_searchRoute( DLL_RTSEL_DEFAULT, 0, stMsgHrds.m_unDADDRSrcAddr );
    }

    if (ucRouteTableIdx < g_ucDllRoutesNo)
    {
       ucRouteSFC = DLDE_copyRouteInfoToMsg(&g_aDllRoutesTable[ucRouteTableIdx].m_stRoute, &stMsgHrds);
    }
    
    MONITOR_EXIT(); // end exclusive area

    
    // verify if routing succesfull
    if ( ucRouteSFC !=  SFC_SUCCESS )
    {
      DLDE_Data_Confirm(p_hHandle, ucRouteSFC);
      return;
    }
  }
  
  stMsgHrds.m_ucLifetime += MLSM_GetCrtTaiSec();
  
  // insert very first originator address
  stMsgHrds.m_ucPayloadLen = p_ucPayloadLength;
  
  // check if payload len is zero and elide DROUT and MehsHeader from DLL header
  if ( p_ucPayloadLength == 0 )
  {      
      stMsgHrds.m_ucDROUTLen = 0;
      stMsgHrds.m_unDADDRSrcAddr = 0;
      stMsgHrds.m_unDADDRDstAddr = 0;
  }
  
  MONITOR_ENTER();  // start exclusive area
  
  uint8 stResult = DLDE_addMsgWithLinkCk( &stMsgHrds, p_pPayload );
  
  MONITOR_EXIT();
  
  if( stResult != SFC_SUCCESS )
  {
      DLDE_Data_Confirm(p_hHandle, stResult);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Computes the number of messages in the queue that have to be transmittes to a specified neighbor.
/// @param  p_unShortAddress - address of the specified neighbor 
/// @return none
/// @remarks
///      Access level: Interrupt level (Called from MLDE_OutgoingFrame()) 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_GetMsgNoInQueue( uint16 p_unShortAddress )
{
  uint8 ucMsgNo = 0;  
  DLL_DPDU * pMsg;
  
  for( pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++ )
  {
      if( (2 == pMsg->m_stHdrs.m_ucPeerAddrLen) 
            && (pMsg->m_stHdrs.m_stPeerAddr.m_unShort == p_unShortAddress) )
      {
          ucMsgNo++;
      }                
  }  
  
  return ucMsgNo;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Computes the number of messages in the queue that have to be transmittes on a specified graph
/// @param  p_unShortAddress - address of the specified neighbor 
/// @return none
/// @remarks
///      Access level: Interrupt level (called by MLDE_IncomingFrame())
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_GetMsgNoInQueueForGraph( uint16 p_unGraphID )
{
  uint8 ucMsgNo = 0;  
  DLL_DPDU * pMsg;
  
  for( pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd; pMsg++ )
  {
      if( p_unGraphID == pMsg->m_stHdrs.m_unGraphID ) 
      {
          ucMsgNo++;
      }                
  }  
  
  return ucMsgNo;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Verifies if forwarding buffer size rules for a certain priority message has been exceeded.
/// @param  p_ucPriority - priority of the message that has to be added to the dll queue
/// @param  p_pucReason - the limitation reason that must be updated inside = 1-QueuePriority MIB limitation;= 0-other 
/// @return no of remaining messages 
/// @remarks
///      Access level: Interrupt (called by MLDE_IncomingFrame())
//       Obs: if remaining messages 2 means are at least 2 availabe spaces on queue
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int DLDE_CheckForwardQueueLimitation( uint8 p_ucPriority, uint8* p_pucReason )
{
  unsigned int unMsgNo = (uint8*)(g_aDllMsgQueue + DLL_MSG_QUEUE_SIZE_MAX) - (uint8*)g_pDllMsgQueueEnd;
  
  *p_pucReason = 0;  
  
  if( unMsgNo )
  {              
      if( unMsgNo > sizeof(g_aDllMsgQueue[0]) )
      {
          unMsgNo = 2;
      }
      else 
      {
          unMsgNo = 1;
      }
  
      unsigned int unQueuePriorityIdx;      
      for( unQueuePriorityIdx=0;  unQueuePriorityIdx < g_stQueuePriority.m_ucPriorityNo; unQueuePriorityIdx++ )
      {
          if( p_ucPriority <= g_stQueuePriority.m_aucPriority[unQueuePriorityIdx] ) // limmit founded
          {
              DLL_DPDU * pMsg;
              unsigned int unPriorityMsgNo = g_stQueuePriority.m_aucQMax[unQueuePriorityIdx];
              
              for( pMsg = g_aDllMsgQueue; pMsg < g_pDllMsgQueueEnd && unPriorityMsgNo; pMsg++ )
              {
                  if( pMsg->m_stHdrs.m_ucPriority <= g_stQueuePriority.m_aucPriority[unQueuePriorityIdx] &&
                     DLL_NEXT_HOP_HANDLE == pMsg->m_stHdrs.m_hHandle )
                  {
                      unPriorityMsgNo--;
                  }                
              } 
              
              if( unPriorityMsgNo < unMsgNo )
              {
                  unMsgNo = unPriorityMsgNo;
                  *p_pucReason = 1;
              }
              break;
          }
      }      
  }
  
  return unMsgNo;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin 
/// @brief  Adds a DLL message into the dll message queue. It also checks if there is an available link
///         for trasnmitting the message.
/// @param  p_unShortAddress - address of the specified neighbor 
/// @return none
/// @remarks
///      Access level: User mode or Interrupt mode
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLDE_addMsgWithLinkCk (const DLL_MSG_HRDS* p_pMsgHrds, const uint8 * p_pPayload )
{
      
    if( g_pDllMsgQueueEnd < g_aDllMsgQueue + DLL_MSG_QUEUE_SIZE_MAX )
    {
        DLL_DPDU * pMsg = g_aDllMsgQueue;
       
        //the old messages with the same priority will be prior sent 
        while( pMsg < g_pDllMsgQueueEnd && p_pMsgHrds->m_ucPriority <= pMsg->m_stHdrs.m_ucPriority  )
        {
            pMsg++;
        }

        if( pMsg <= g_pDllMsg ) //added before g_pDllMsg, increment g_pDllMsg
        {
            g_pDllMsg++;    
        }        
        
        memmove( pMsg + 1, pMsg, (uint8*)g_pDllMsgQueueEnd - (uint8*)pMsg );
        memcpy( &pMsg->m_stHdrs, p_pMsgHrds, sizeof(pMsg->m_stHdrs) );      
        memcpy( pMsg->m_aPayload, p_pPayload, p_pMsgHrds->m_ucPayloadLen );
        
        pMsg->m_stHdrs.m_ucRetryNo = 0;
                
        g_pDllMsgQueueEnd++;       
          
        return SFC_SUCCESS;
    }
    
    return SFC_INSUFFICIENT_DEVICE_RESOURCES;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Specifies if a neighbor is a clock source for the current device
/// @param  p_unNeighborAddr - the neighbor 16 bit address
/// @return TRUE if the neighbor is a clock source for the current device or FALSE otherwise
/// @remarks
///      Access level: User or Interrupt\n
///      Context: called by DLDE_HandleRxMessage() -> interrupt level\n
///                         DLDE_Data_Request() -> user level (or interrupt level if BACKBONE_SUPPORT)
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLDE_isClockSource(uint16 p_unNeighborAddr)
{
  // todo: return should specify if secondary or preffered  clock source
  DLL_SMIB_ENTRY_NEIGHBOR * pNeighbor = (DLL_SMIB_ENTRY_NEIGHBOR *)
              DLME_FindSMIB( SMIB_IDX_DL_NEIGH, p_unNeighborAddr );

  return ( pNeighbor && (pNeighbor->m_stNeighbor.m_ucInfo & DLL_MASK_NEICLKSRC) );
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Part of the RouteEngine. Routes the message using the long destination
///         address. Fills the DROUT, DADDR sub-headers of the message.
///         It is the 1st routing type in the hierarchical routing.
/// @param  p_pDstAddr - the long destination address
/// @param  p_pMsgHdr - a pointer to the message headers that has to be routed
/// @return none
/// @remarks
///      Access level: User or Interrupt if BACKBONE_SUPPORT\n
///      Context: called by "DLDE_Data_Request" that is already declared as "__monitor" 
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_routeUsingLongAddr(uint8 * p_pDstAddr, DLL_MSG_HRDS * p_pMsgHdr)
{
  DLME_CopyReversedEUI64Addr( p_pMsgHdr->m_stPeerAddr.m_aEUI64, p_pDstAddr );  
  
  // compressed form of DROUT - destination one hop away
  p_pMsgHdr->m_ucDROUTLen = 2;
  p_pMsgHdr->m_aDROUT[0] |= 0x01 | DLL_DROUT_COMPRESS_MASK; // bits 0-3 represent number of hops = 1;
  p_pMsgHdr->m_aDROUT[1]  = 0;
  
  p_pMsgHdr->m_ucNeighborIdx = 0xFF;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Part of the DLDE-Rx module. Checks a received message, and decides if
///         it has to be forwarded to a neighbor (puts it in the message queue)
///         or if it is the final destination it sends the payload  to the NWK layer
/// @param  none
/// @return none
/// @remarks
///      Access level: Interrupt\n
///      Context: g_stQueueMsg must be populated with RX message
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_HandleRxMessage( uint8 * p_pRxBuff )
{
    //g_stDllRxHdrExt.m_ucSecurityPos is the offset of DLL payload 
    uint8 * pPayload = p_pRxBuff+g_stDllRxHdrExt.m_ucSecurityPos; 
   
    // check if this device is the final destination of the message
    if( g_stDllRxHdrExt.m_bDestIsNL )
    {
        uint8 aSrcDestAddr[8]; 
        uint8 ucSrcDestAddrLen = 8;
        uint8 ucAddrType = g_stDllRxHdrExt.m_ucAddrType & (DLL_MHR_FC_SRC_ADDR_MASK | DLL_MHR_FC_DEST_ADDR_MASK);
    
        switch( ucAddrType )
        {
        case ((SHORT_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) | (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET)):
            aSrcDestAddr[0] = g_stDllMsgHrds.m_unDADDRSrcAddr >> 8;
            aSrcDestAddr[1] = g_stDllMsgHrds.m_unDADDRSrcAddr;
            aSrcDestAddr[2] = g_stDllMsgHrds.m_unDADDRDstAddr >> 8;
            aSrcDestAddr[3] = g_stDllMsgHrds.m_unDADDRDstAddr;            
            ucSrcDestAddrLen = 4;
            break;
            
        case ((SHORT_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) | (EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET)):
            // the source address is in MAC header (8 bytes)
            DLME_CopyReversedEUI64Addr( aSrcDestAddr, g_stDllMsgHrds.m_stPeerAddr.m_aEUI64);
            break;
            
        case ((EXTENDED_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) | (SHORT_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET)):
            if( g_stDllMsgHrds.m_ucNeighborIdx == 0xFF )
                return;
            
            DLME_CopyReversedEUI64Addr( aSrcDestAddr, g_aDllNeighborsTable[g_stDllMsgHrds.m_ucNeighborIdx].m_stNeighbor.m_aEUI64 );                            
            break;
            
        default:
            return;
        }
        
        //the LH(Last Hop)regarding NWK layer not sent to upper layers 
        uint8 ucPriorityAndFlags = g_stDllMsgHrds.m_ucPriority | ((g_stDllMsgHrds.m_ucDADDRCtrlByte & DLL_ECN_MASK) << 1);
        if( g_stDllMsgHrds.m_ucDADDRCtrlByte & DLL_DISCARD_ELIGIBLE_MASK )
        {
            ucPriorityAndFlags |= TLDE_DISCARD_ELIGIBILITY_MASK;
        }    
        
        DLDE_Data_Indicate(ucSrcDestAddrLen,                                        
                           aSrcDestAddr,                                                
                           ucPriorityAndFlags,                // priority and flags
                           g_stDllMsgHrds.m_ucPayloadLen,     // length of payload
                           pPayload );                 // payload
    }
   else // g_stDllRxHdrExt.m_bDestIsNL == 0 -> it is a hop scenario
   { 
        uint8  ucNeighbor = 0xFF;        
        uint8 * pDROUT = g_stDllMsgHrds.m_aDROUT;       
        
        g_stDllMsgHrds.m_ucGraphIdx = 0xFF;
        
        /* check ECN bits in DADDR    */
        if( g_ucDllECNStatus && (g_stDllMsgHrds.m_ucDADDRCtrlByte & DLL_ECN_MASK))
        {
            g_stDllMsgHrds.m_ucDADDRCtrlByte |= DLL_ECN_MASK;
        }          
    
        /* --------------- DROUT first octet structure  ---------------  */
        /* | bit7     | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 | */
        /* | Compress |  Priority                 |  DLL Forward Limit | */
        
        uint8 ucHopsNo = *pDROUT & 0x07;

        if( ucHopsNo >= 0x07 ) // next byte is the forward limit
        {
            ucHopsNo = *(++pDROUT);
            if( ucHopsNo <= 0x07 )
            {
                g_stDllMsgHrds.m_ucDROUTLen--;  //eliminate the extended byte for Forward Limits
                memmove(pDROUT, pDROUT + 1, g_stDllMsgHrds.m_ucDROUTLen - 1);
                pDROUT--;
            }
        }
                       
        if( !ucHopsNo )
        {                    
            if( g_stDPO.m_unCustomAlgorithms & HOPNO_DISCARD_DETECTION_FLAG )
            {                
                // loop
                g_stLoopDetected.m_unSrcAddr = g_stDllMsgHrds.m_unDADDRSrcAddr;
                g_stLoopDetected.m_unDstAddr = g_stDllMsgHrds.m_unDADDRDstAddr;
                g_stLoopDetected.m_ucHopLeft = ucHopsNo;
            }
            
            return; // the hops remaining is decremented to zero. the packet is not forwarded any further
        }
        
        *(pDROUT++) -= 1;
        
        if( g_stDPO.m_unCustomAlgorithms & MYSELF_LOOP_DETECTION_FLAG )
        {
           if( g_unShortAddr && (g_unShortAddr == g_stDllMsgHrds.m_unDADDRSrcAddr) ) // I'm the originator of message
           {
              g_stLoopDetected.m_unSrcAddr = g_stDllMsgHrds.m_unDADDRSrcAddr;
              g_stLoopDetected.m_unDstAddr = g_stDllMsgHrds.m_unDADDRDstAddr;
              g_stLoopDetected.m_ucHopLeft = ucHopsNo;
           }
        }
        
        // init the DPDU time to live, as the curent TAI in seconds + g_unMaxLifetime in seconds
        // if graph routing will be used, it will be overwritten later
        g_stDllMsgHrds.m_ucLifetime = g_ulDllTaiSeconds + (g_unMaxLifetime >> 2);        
        
        if ( g_stDllMsgHrds.m_aDROUT[0] & DLL_DROUT_COMPRESS_MASK ) 
        {
            // compressed DROUT header
            g_stDllMsgHrds.m_ucGraphIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_GRAPH, (uint16)(*pDROUT));
        }
        else
        {
            // non-compressed DROUT header 
            uint8 ucEntryNo = *(pDROUT++);
            if ( !ucEntryNo || ucEntryNo > (uint8)MAX_DROUT_ROUTES_NO ) // maximum entries ( DLDE_CheckIfDestIsNwk checks that nr of entries is non-zero)
            {
                return;
            }
            
            //uint8 ucRoutOffset = ucGraphOff + 1; //+1 to jump over the routing table entries no
            uint16 unFirstEntry = pDROUT[0] | ((uint16)pDROUT[1] << 8);           
            
            if ((unFirstEntry & GRAPH_PREFIX_MASK) == (uint16)(GRAPH_ID_PREFIX)) // first entry is graph
            {
                // graph routing message; try routing on first entry in routing table            
                g_stDllMsgHrds.m_ucGraphIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_GRAPH, unFirstEntry & GRAPH_MASK); 
              
                if ( ucEntryNo > 1) // if more entries are; try also on second entry if available, even if first entry routing was succesfull
                {                
                    uint16 unSecondEntry = pDROUT[2] | ((uint16)pDROUT[3] << 8);   
                    
                    if ((unSecondEntry & GRAPH_PREFIX_MASK) == (uint16)GRAPH_ID_PREFIX ) // second entry is a graph
                    {
                        uint8 ucSecondaryGraphIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_GRAPH, unSecondEntry & GRAPH_MASK);      
                        
                        if ( ucSecondaryGraphIdx != 0xFF )
                        {
                            // remove first entry in routing table                            
                            *(pDROUT-1) = --ucEntryNo;
                            memmove(pDROUT, pDROUT+2, ucEntryNo*2 );
                            g_stDllMsgHrds.m_ucDROUTLen -= 2;
                            g_stDllMsgHrds.m_ucGraphIdx = ucSecondaryGraphIdx;
                        }                    
                    }                
                    else if ( g_stDllMsgHrds.m_ucGraphIdx == 0xFF ) // second entry is an unicast address
                    {
                        // graph routing failed both on first and second entries; 
                        // last chance: second entry is a unicast address that is found in neighbor tables also   
                        ucNeighbor = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, unSecondEntry );
                        
                        // remove first and second entry in routing table
                        ucEntryNo -= 2;
                        *(pDROUT-1) = ucEntryNo;
                        memmove(pDROUT, pDROUT+4, ucEntryNo*2);    
                        g_stDllMsgHrds.m_ucDROUTLen -= 4;
                    }
                }           
            }
            else 
            {
                // first entry in route table is an unicast address                
                ucNeighbor = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, unFirstEntry );  
                
                // remove first entry in routing table
                *(pDROUT-1) = --ucEntryNo;
                memmove(pDROUT, pDROUT+2, ucEntryNo*2);
                g_stDllMsgHrds.m_ucDROUTLen -= 2;
            }        
        }
        
        if ( g_stDllMsgHrds.m_ucGraphIdx != 0xFF )
        {
            DLL_SMIB_ENTRY_GRAPH * pGraph = g_aDllGraphsTable + g_stDllMsgHrds.m_ucGraphIdx;
            
            // init the dll header graphID (for easy access from schedule link)
            g_stDllMsgHrds.m_unGraphID = pGraph->m_stGraph.m_unUID;    
            
            if( pGraph->m_stGraph.m_unMaxLifetime )
            {
                // overwrite the DPDU time to live, as the curent TAI in seconds + pGraph->m_unMaxLifetime in seconds          
                g_stDllMsgHrds.m_ucLifetime = g_ulDllTaiSeconds + (pGraph->m_stGraph.m_unMaxLifetime >> 2);
            }            
            
            ucNeighbor = DLDE_selectGraphNeighbor(pGraph, 0, 0xFF);
        }
        else
        {            
            if( ucNeighbor == 0xFF )
            {
                ucNeighbor = DLDE_tryToExtendGraph(g_stDllMsgHrds.m_unGraphID, g_stDllMsgHrds.m_unDADDRDstAddr);
            }
            g_stDllMsgHrds.m_unGraphID = 0xFFFF; // source routing ...
        }
        
        if ( ucNeighbor != 0xFF )
        {
            g_stDllMsgHrds.m_ucNeighborIdx = ucNeighbor;
            g_stDllMsgHrds.m_ucFinalIdx = 0xFF; 
            
            g_stDllMsgHrds.m_ucPeerAddrLen = 2;                        
            g_stDllMsgHrds.m_stPeerAddr.m_unShort = g_aDllNeighborsTable[ucNeighbor].m_stNeighbor.m_unUID;         // little endian  
            
            // attach a DLL level handler to the message
            g_stDllMsgHrds.m_hHandle = DLL_NEXT_HOP_HANDLE;
                
            // add the message to the queue
            if ( g_stDllRxHdrExt.m_bDiscardFlag ) // the dll msg queue is full and we have a discard eligible candidate
            {
                DLDE_remMsgFromQueue( g_pDiscardMsg, SFC_FAILURE );
            }
            uint8 ucSfc = DLDE_addMsgWithLinkCk(&g_stDllMsgHrds, pPayload );
        }
    }
    
    // store current slot execution status for neighbor statistics: exclude DSDUs without payload
    if (g_stDllMsgHrds.m_ucPayloadLen)
    {
        g_ucSlotExecutionStatus = SLOT_STS_RX_SUCCESS;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Tries to extend an non-existing graph to a neighbor
/// @param  p_unGraphID     - graph ID that has to be extended
/// @param  unFinalDestAddr - the final network destination addres
/// @return pointer to a neighbor entry in neighbor table
/// @remarks
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_tryToExtendGraph(uint16 p_unGraphID, uint16 unFinalDestAddr)
{
    uint8 ucNeighbor = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, unFinalDestAddr );
    
    if (ucNeighbor != 0xFF)
    {
        uint8   unExtGraphNo = (g_aDllNeighborsTable[ucNeighbor].m_stNeighbor.m_ucInfo & DLL_MASK_NEINOFGR) >> DLL_ROT_NEINOFGR;
        uint16 * aExtGraphs = g_aDllNeighborsTable[ucNeighbor].m_stNeighbor.m_aExtendedGraphs;
        
        for (uint8 ucIdx = 0; ucIdx < unExtGraphNo; ucIdx++)
        {
            if (p_unGraphID == (aExtGraphs[ucIdx] & GRAPH_MASK) )
            {
                return ucNeighbor;
            }
        }
    }    

    return ucNeighbor; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Searches for a route table index
/// @param  p_ucRouteType - the routing type(contractId, destAddr or default)
/// @param  p_unUID - the contractID or destAddr of the route
/// @param  p_unNwkSrcAddr - NWK source address in LSB- is validated just for BBR
/// @return Route table index
/// @remarks
///      Access level: User or Interrupt if BACKBONE_SUPPORT\n
///      Context: called by "DLDE_Data_Request" that is already declared as "__monitor"
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef BACKBONE_SUPPORT
  __monitor uint8 DLDE_findRoute(uint8 p_ucRouteType, uint16 p_unUID, SHORT_ADDR p_unNwkSrcAddr)
#else
  __monitor uint8 DLDE_findRoute(uint8 p_ucRouteType, uint16 p_unUID )
#endif
{
  uint8 ucRouteTableIdx = g_ucDllRoutesNo;
  uint16 unLowestRouteID = 0xFFFF; //if many routes with same Alternative and Selector teh route with the lowest index will be selected
                                   //the route index is an ExtDLUint(15 bits value)            
  
  for( uint8 ucIdx = 0; ucIdx < g_ucDllRoutesNo; ucIdx++ )
  {
      if( p_ucRouteType == (g_aDllRoutesTable[ucIdx].m_stRoute.m_ucInfo & DLL_RTSEL_MASK) )
      {
          if( DLL_RTSEL_DEFAULT != p_ucRouteType ) 
          {
              if( g_aDllRoutesTable[ucIdx].m_stRoute.m_unSelector == p_unUID )
              {
#ifdef BACKBONE_SUPPORT
                  //for Contract ID route needed to validate also the NWK source address
                  if( p_ucRouteType == DLL_RTSEL_BBR_CONTRACT && p_unNwkSrcAddr != g_aDllRoutesTable[ucIdx].m_stRoute.m_unNwkSrcAddr )
                  {
                      continue;
                  }
#endif    
              }
              else
                  continue;            
          }
          
          if( g_aDllRoutesTable[ucIdx].m_stRoute.m_unUID < unLowestRouteID )
          {
              unLowestRouteID = g_aDllRoutesTable[ucIdx].m_stRoute.m_unUID; 
              ucRouteTableIdx = ucIdx;
          }
          
      }
  }
  
  return ucRouteTableIdx;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin 
/// @brief  Part of the RouteEngine. Used to copy route info into a message, using
///         the ROUTE-SMIB and GRAPH-SMIB tables
/// @param  p_pRoute - route entry
/// @param  p_pMsgHrds - a pointer to the message headers that has to be routed
/// @return service feedback code
/// @remarks
///      Access level: User or Interrupt level if BACKBONE_SUPPORT\n
///      Context: called by "DLDE_Data_Request" 
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLDE_copyRouteInfoToMsg(DLL_SMIB_ROUTE * p_pRoute,  DLL_MSG_HRDS*   p_pMsgHrds)
{
    unsigned int unRoutesNo = (p_pRoute->m_ucInfo & DLL_MASK_RTNO) >> DLL_ROT_RTNO;
    
    // check route table entry number boundaries
    if ( (0 == unRoutesNo) || (unRoutesNo > MAX_DROUT_ROUTES_NO) )
        return SFC_INVALID_DATA;
    
    if( p_pRoute->m_ucFwdLimit < 0x07 )
    {
        p_pMsgHrds->m_aDROUT[0] |= p_pRoute->m_ucFwdLimit;
        p_pMsgHrds->m_ucDROUTLen = 1;
    }
    else
    {
        p_pMsgHrds->m_aDROUT[0] |= 0x07;
        p_pMsgHrds->m_aDROUT[1] = p_pRoute->m_ucFwdLimit;
        p_pMsgHrds->m_ucDROUTLen = 2;
    }
    
    p_pMsgHrds->m_ucFinalIdx = 0xFF; 
    
    // check first entry in route table
    if ( (p_pRoute->m_aRoutes[0] & GRAPH_PREFIX_MASK) == (uint16)GRAPH_ID_PREFIX )
    {
        // first entry is a graph    
        unsigned int unGraphId = p_pRoute->m_aRoutes[0] & GRAPH_MASK;
        p_pMsgHrds->m_unGraphID = unGraphId;
        p_pMsgHrds->m_ucGraphIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_GRAPH, unGraphId );
        
        if (p_pMsgHrds->m_ucGraphIdx == 0xFF)
            return SFC_NO_ROUTE;        
        
        DLL_SMIB_ENTRY_GRAPH * pGraph = g_aDllGraphsTable +  p_pMsgHrds->m_ucGraphIdx;
                
        if( pGraph->m_stGraph.m_unMaxLifetime )
        {            
            p_pMsgHrds->m_ucLifetime = (pGraph->m_stGraph.m_unMaxLifetime >> 2); // overwrite the DPDU time to live
        }
        
        // copy to MHR the destination address (the first neighbor of the graph)        
        DLDE_setMsgGraphNeighbor( p_pMsgHrds, pGraph );
        
        if( (unRoutesNo == 1) && (unGraphId  <= 0x00FF) ) 
        {          
            // compress DROUT header; graph can be represented as 10100000gggggggg
            p_pMsgHrds->m_aDROUT[0] |= DLL_DROUT_COMPRESS_MASK;
            
            if( 1 == p_pRoute->m_ucFwdLimit )
            {
                unGraphId = 0;  // destination is one hop away; routing info is already present in MAC headers
            }
            p_pMsgHrds->m_aDROUT[p_pMsgHrds->m_ucDROUTLen++] = (uint8)unGraphId;  // only low byte of graphID is transmitted
        }
        else
        {
            // non-compressed DROUT header
            p_pMsgHrds->m_aDROUT[p_pMsgHrds->m_ucDROUTLen++] = unRoutesNo;
            
            memcpy( p_pMsgHrds->m_aDROUT + p_pMsgHrds->m_ucDROUTLen,    
                   p_pRoute->m_aRoutes,
                   unRoutesNo * 2 );
            
            p_pMsgHrds->m_ucDROUTLen += unRoutesNo * 2;          
        }
    }
    else
    {
        // fill the next hop address in MHR
        p_pMsgHrds->m_stPeerAddr.m_unShort = p_pRoute->m_aRoutes[0];        
        p_pMsgHrds->m_ucNeighborIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, p_pMsgHrds->m_stPeerAddr.m_unShort );        
          
        // signal to schedule link that the messageis not graph-routed
        p_pMsgHrds->m_unGraphID = 0xFFFF;  // signal source routing to scheduleLink...
        
        if ( p_pRoute->m_ucFwdLimit == 1 )
        {
            // destination is one hop away; use compressed DROUT header
            p_pMsgHrds->m_aDROUT[0] |= DLL_DROUT_COMPRESS_MASK;
            p_pMsgHrds->m_aDROUT[p_pMsgHrds->m_ucDROUTLen++] = 0;
        }     
        else
        {
            // destination is more than one hop away; use non-copressed DROUT header
            unRoutesNo --;    //eliminate the first hop
            p_pMsgHrds->m_aDROUT[p_pMsgHrds->m_ucDROUTLen++] = unRoutesNo;
            
            memcpy( p_pMsgHrds->m_aDROUT + p_pMsgHrds->m_ucDROUTLen,    
                    p_pRoute->m_aRoutes + 1,
                    unRoutesNo * 2 );
            
            p_pMsgHrds->m_ucDROUTLen += unRoutesNo * 2; 
        }      
    }
      
    return SFC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Verifies if an incomming Rx message has to be transmitted to the
///         network layer
/// @param  p_unFinalShortAddr - final destination 
/// @return 2 if message will be discarded after acknowledgement 
///         1 if destination is the network layer, 
///         0 if the mesasge has to be transmitted to a neighbor (hop scenario)
/// @remarks
///      Access level: Interrupt\n
///      Context: g_stDllMsgHrds must be populated with current message headers
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_CheckIfDestIsNwk(uint16 p_unFinalShortAddr)
{  
    if (g_stDllRxHdrExt.m_ucAddrType & 0x44)
    {
        return 1; // src or dst address mode is extended type (8 bytes)
    }
    
    if (!(g_stDllRxHdrExt.m_ucAddrType & DLL_MHR_FC_DEST_ADDR_MASK))
    {
        return 1; // broadcast message, because destination addres mode is 0 (no dst address present)
    }
        
    if( 0 == g_stDllMsgHrds.m_ucDROUTLen )
    {        
        return 2; // //the message will be discarded after acknowledgement   
    }
    
    if( g_stDllMsgHrds.m_aDROUT[0] & DLL_DROUT_COMPRESS_MASK ) 
    {
        //have one compressed DROUT header        
        if( !g_stDllMsgHrds.m_aDROUT[g_stDllMsgHrds.m_ucDROUTLen-1] ) 
        {
            return 1; // last hop for this message
        }
    }
    
    if( p_unFinalShortAddr == g_unShortAddr ) 
        return 1;
    
    return (!NLME_IsInSameSubnet( p_unFinalShortAddr));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Eduard Erdei, Mircea Vlasin
/// @brief  Interprets an incomming Rx acknowledge PDU or handles ACK errors
/// @param  p_unAckTypeErrCode - specifies error code and Ack type if error code is success on MSB
/// @param  p_pRxBuff - 1 byte length of message + ACK message
///                       or broadcast->to indicate that the message can be removed from the queue
/// @return none
/// @remarks
///      Access level: Interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_AckInterpreter(uint16         p_unAckTypeErrCode,
                          const uint8 * p_pRxBuff )
{
  uint8 ucClockSyncReq = 1;  
  
  if( DLL_NEXT_HOP_HANDLE + 1 != g_stDllMsgHrds.m_hHandle ) 
  {
      if( !g_pDllMsg )
        return;
      ucClockSyncReq = 0;   //no Clock Synchronize Request  
  }
  
  // intenal generated ACK/NACK
  if( p_unAckTypeErrCode == ((UNICAST_DEST << 8) | SFC_SUCCESS ))// received message
  {  
      do
      {
          uint8 ucDHRFrameCtrl;
          uint8 ucIdx;          
          uint8 ucUnknwnPeerEUI64;
          unsigned int unFrameLength = p_pRxBuff[0];
          
          p_pRxBuff ++;

         uint8 aucAuthData[MAX_ACK_LEN + 4];   //including virtual MIC field 

          union
          {
            uint32 m_ulAlignForced;
            uint8 m_aucNonce[MAX_NONCE_LEN];            
          } stAligned;
    
          p_unAckTypeErrCode = SFC_TIMEOUT; // if malformed ACK/NACK -> timeout
                        
          if( (p_pRxBuff[1] & DLL_MHR_FC_DEST_ADDR_MASK) != (NO_ADDRESS_MODE << DLL_MHR_FC_DEST_ADDR_OFFSET) )
          {
              SET_DebugLastAck( SFC_TYPE_MISSMATCH );
              break;  //no destination address supported by ACK  
          }
              
          // Subnet ID + EUI64 address or no address + Subnet ID
          ucIdx = ( p_pRxBuff[1] == ((EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET) | BIT4) ? 3+2+8 : 3 );
    
          if( ucIdx >= unFrameLength )
          {
              SET_DebugLastAck( SFC_INVALID_SIZE );
              break;
          }

          if( p_pRxBuff[2] != g_ucMsgSendCnt ) // not my expected ACK
         {
              SET_DebugLastAck( SFC_NOT_MY_ACK );
              break;
          }
          
          // authentication checking
          ucDHRFrameCtrl = p_pRxBuff[ucIdx++];
              
          if( (ucDHRFrameCtrl & (BIT2 | BIT1) ) != BIT1 ) // accept MMIC-32 only
          {
              SET_DebugLastAck( SFC_UNSUPPORTED_MMIC_SIZE );
              break;
          }
    
          if( unFrameLength + 4 > sizeof(aucAuthData) )
          {
              SET_DebugLastAck( SFC_INVALID_DATA );
              break;
          }

          //need to be copied also the authentication MIC
          memcpy(aucAuthData, p_pRxBuff, ucIdx);     //included also the DHR
          memcpy(aucAuthData + ucIdx, g_oAckPdu, 4 ); // here is saved the TX MIX
          memcpy(aucAuthData + ucIdx + 4, p_pRxBuff + ucIdx, unFrameLength - ucIdx);

          DLL_SMIB_NEIGHBOR * pNeighbor;
          
          if( g_ucTxNeighborIdx == 0xFF )
          {
              pNeighbor = NULL;
          }
          else
          {
              pNeighbor = &g_aDllNeighborsTable[g_ucTxNeighborIdx].m_stNeighbor;
          }
          
          //compute the nonce                
          ucUnknwnPeerEUI64 = ( !pNeighbor|| !memcmp(pNeighbor->m_aEUI64, c_aucInvalidEUI64Addr, 8)) ;
          if( ucUnknwnPeerEUI64 )
          {
              // not known neighbor MAC            
              if( g_stDllMsgHrds.m_ucPeerAddrLen == 8 ) // send to EUI64
              {
                  DLME_CopyReversedEUI64Addr( stAligned.m_aucNonce, g_stDllMsgHrds.m_stPeerAddr.m_aEUI64 );
              }
              else
              {  
                  if( p_pRxBuff[1] != ((EXTENDED_ADDRESS_MODE << DLL_MHR_FC_SRC_ADDR_OFFSET) | BIT4) ) // not 8 bytes source address present ... nothing to do
                  {
                    SET_DebugLastAck( SFC_DATA_SEQUENCE_ERROR );
                    break;
                  }
        
                  DLME_CopyReversedEUI64Addr( stAligned.m_aucNonce,  p_pRxBuff + 3 + 2 ); //+2 - Subnet ID
              }            
          }
          else // known neigbor
          {
                DLME_CopyReversedEUI64Addr( stAligned.m_aucNonce,  pNeighbor->m_aEUI64 ); 
          }
    
          int nSlotDiff = 0;
          uint16 nTimeCorrection = 0;
          uint16 unSlowHopTsOffset = 0xFFFF;              
          
          // check for clock & timeslot offset correction info
          if (ucDHRFrameCtrl & BIT7)
          {
              nTimeCorrection = p_pRxBuff[ucIdx++];
              nTimeCorrection |= ((uint16)(p_pRxBuff[ucIdx++])) << 8;            
          }
          
          if (ucDHRFrameCtrl & BIT6)
          {                              
              ucIdx = DLMO_ExtractExtDLUint( p_pRxBuff + ucIdx, &unSlowHopTsOffset ) - p_pRxBuff; 
              if( g_unCrtUsedSlowHoppingOffset != 0xFFFF ) // valid slot offset
              {
                  nSlotDiff = MLDE_GetSlowOffsetDiff( unSlowHopTsOffset - g_unCrtUsedSlowHoppingOffset );
              }
          }
          
          *(uint32*)(stAligned.m_aucNonce + 8) = MLSM_GetSlotStartMsBE(nSlotDiff);
          
          stAligned.m_aucNonce[12] = p_pRxBuff[2];   //Sequence Number
    
          if(AES_SUCCESS != AES_Decrypt( g_pDllKey,
                                        stAligned.m_aucNonce, 
                                        aucAuthData, 
                                        unFrameLength, 
                                        aucAuthData + unFrameLength, 4 ) )
          {
              SLME_DL_FailReport();
              SET_DebugLastAck( SFC_SECURITY_FAILED );
              break;                     
          }
          
          MACA_ResetWachDog();
          
          if( pNeighbor ) 
          {
              // mark the communication history: succesful transmission              
              pNeighbor->m_ucCommHistory |= 0x01;  
              
              // reset the last communication counter 
              if( pNeighbor->m_pstDiag )
              {
                  pNeighbor->m_pstDiag->m_unFromLastCommunication = 0;
              }
              
              if (ucUnknwnPeerEUI64) // save the correspondent address
              {
                  DLME_CopyReversedEUI64Addr( pNeighbor->m_aEUI64, stAligned.m_aucNonce ); 
              }
              
              if( 0xFF != g_ucIdleLinkIdx )
              {
                  // an idle link has to be activated          
                  // linkBacklog is in 1/8 second units; we use internally 1/4 units     
                  g_aDllLinksTable[g_ucIdleLinkIdx].m_stLink.m_ucActiveTimer = (pNeighbor->m_ucLinkBacklogDur >> 1) + 1;   
                  
                  // start a timer on neighbour also (in order to track the idle link for this neighbour)
                  pNeighbor->m_ucBacklogLinkTimer = g_aDllLinksTable[g_ucIdleLinkIdx].m_stLink.m_ucActiveTimer;
                  
                  // need to recompute the hash table  
                  g_ucHashRefreshFlag = 1;                 
              }
          }
    
          // check for clock & timeslot offset correction info
          #ifndef BACKBONE_SUPPORT       
          if (ucDHRFrameCtrl & BIT7)
          {
              MLSM_ClockCorrection(nTimeCorrection, unSlowHopTsOffset);
          }
          #endif
    
          // check for the presence of a DAUX header
          if (ucDHRFrameCtrl & BIT3)
          {
              // ack DAUX may contain activate idle superframe or LQI/RSSI info
              MLDE_InterpretDauxHdr(p_pRxBuff + ucIdx);
          }
                
          //check the shared link
          if( g_pCrtHashEntry->m_ucLinkType & DLL_MASK_LNKEXP )
          {
            MLSM_UpdateShareLinkExpBackoff(TX_WITH_ACK);
          }
          
          ucDHRFrameCtrl &= (BIT1 | BIT0) << DHR_ACK_TYPE_OFFSET;
    
          p_unAckTypeErrCode = SFC_NACK;
          
          SET_DebugLastAck( ucDHRFrameCtrl >> DHR_ACK_TYPE_OFFSET );
          
          switch( ucDHRFrameCtrl )
          {
              case (DLL_ACK << DHR_ACK_TYPE_OFFSET) :
                      p_unAckTypeErrCode = SFC_SUCCESS;                      
                      // save neighbor diagnostic info
                      g_ucSlotExecutionStatus = SLOT_STS_TX_SUCCESS;
                      if( pNeighbor )
                      {
                          pNeighbor->m_ucECNIgnoreTmr = 0;
                      }
                      
                      if( ucClockSyncReq )
                          return;
                      break;
                
              case (DLL_ACK_ECN << DHR_ACK_TYPE_OFFSET):
                      p_unAckTypeErrCode = SFC_SUCCESS;                                   
                      g_ucSlotExecutionStatus = SLOT_STS_TX_SUCCESS;
                      if( pNeighbor )
                      {
                          pNeighbor->m_ucECNIgnoreTmr = ECN_IGNORE_TIME;
                      }
                      if( ucClockSyncReq )
                          return;
                      break;
                  
              case (DLL_NACK0 << DHR_ACK_TYPE_OFFSET):    
#ifndef BACKBONE_SUPPORT                    
                    // NACK0 on DEVICE_SECURITY_JOIN_REQ_SENT state means Router is out of resources -> try another router
                    if( DEVICE_SECURITY_JOIN_REQ_SENT == g_ucJoinStatus ) 
                    {
                        g_stJoinRetryCntrl.m_unJoinTimeout = 0; //force rejoin process
                    }          
#endif              
              // no break here
              case (DLL_NACK1 << DHR_ACK_TYPE_OFFSET):                      
                      g_ucSlotExecutionStatus = SLOT_STS_NACK;
                      if( pNeighbor )
                      {
                          pNeighbor->m_unNACKBackoffTmr = g_unNackBackoffDur;
                      }
                      if( ucClockSyncReq || SFC_SUCCESS == DLDE_retryMsg())
                          return;
                      break;
          }
     }
      while (0);
      
     if( SFC_TIMEOUT == p_unAckTypeErrCode && (g_pCrtHashEntry->m_ucLinkType & DLL_MASK_LNKEXP) ) //check the shared link
     {  
        MLSM_UpdateShareLinkExpBackoff(TX_WITH_NO_ACK);
     }
  }
  else // not success or BCAST 
  {
      //check the shared link
      //if Broadast Message "g_ucTxNeighborIdx" == 0xFF
      if( g_pCrtHashEntry->m_ucLinkType & DLL_MASK_LNKEXP )
      {
          MLSM_UpdateShareLinkExpBackoff(TX_WITH_NO_ACK);
      } 
  }

  if( ucClockSyncReq )
  {
      return;           //the Active Clock Synchronization Request is not saved on the DLL Queue
  }
  
  uint8 ucSFC;
  if( p_unAckTypeErrCode != SFC_TIMEOUT )
  {
      DLDE_remMsgFromQueue( g_pDllMsg, p_unAckTypeErrCode );
      return;
  }
  
  ucSFC = DLDE_retryMsg();
  
  if (SFC_SUCCESS != ucSFC)
  {
      DLDE_remMsgFromQueue( g_pDllMsg, ucSFC );
  }
}



///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Verifyies the number of retransmissions, searches for alternative links
/// @param  none
/// @return service feedback code
///         SUCCESS if recovery was performed\n
///         TIMEOUT if there is no alternative link or the maximum nr. of retransmissions has been reached
/// @remarks
///      Access level: Interrupt\n
///      Context: g_pDllMsg must be valid
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_retryMsg( void )
{
  if( g_pDllMsg )
  {
      g_pDllMsg->m_stHdrs.m_ucRetryNo++;
      if( !(g_pDllMsg->m_stHdrs.m_ucRetryNo & 1) ) // from 2 to 2 retries
      {        
        if( !g_pDllMsg->m_stHdrs.m_ucPayloadLen ) // join ping message ... discard it
        {
            return SFC_TTL_EXPIRED;
        }
        
        DLL_DPDU * pBetterMsg = NULL;
        uint8 ucMinRetryNo = g_pDllMsg->m_stHdrs.m_ucRetryNo; 
        for(  DLL_DPDU * pMsg = g_pDllMsgQueueEnd-1; pMsg > g_pDllMsg; pMsg-- ) // found min retry no
        {   
            if(   g_pDllMsg->m_stHdrs.m_ucPriority == pMsg->m_stHdrs.m_ucPriority 
               && ucMinRetryNo > pMsg->m_stHdrs.m_ucRetryNo )
            {
                pBetterMsg = pMsg;
                ucMinRetryNo = pMsg->m_stHdrs.m_ucRetryNo;
                if( !ucMinRetryNo )
                    break;
            }
        }          
        if( pBetterMsg )
        {
            // exchange the messages
            uint8 aTmpBuff[sizeof( DLL_DPDU )];
            
            memcpy( aTmpBuff, g_pDllMsg, sizeof( DLL_DPDU ) );
            memcpy( g_pDllMsg, pBetterMsg, sizeof( DLL_DPDU ) );
            memcpy( pBetterMsg, aTmpBuff, sizeof( DLL_DPDU ) );
            g_pDllMsg = pBetterMsg;                        
         }          
      }
      
      Log( LOG_WARNING, LOG_M_DLL, DLOG_Retry, 1, &g_pDllMsg->m_stHdrs.m_ucRetryNo, 2, &g_pDllMsg->m_stHdrs.m_unGraphID,  g_pDllMsg->m_stHdrs.m_ucPeerAddrLen, &g_pDllMsg->m_stHdrs.m_stPeerAddr );
      // if the message uses source routing, alternative link recovery is not possible.
      // alternative neighbor can be found only for graph routed messages!
      if( g_pDllMsg->m_stHdrs.m_unGraphID != 0xFFFF )
      {
          DLL_SMIB_ENTRY_GRAPH * pGraph =  (DLL_SMIB_ENTRY_GRAPH *)
                    DLME_FindSMIB( SMIB_IDX_DL_GRAPH, g_pDllMsg->m_stHdrs.m_unGraphID );
          
          if (pGraph) // graph founded
          {  
              DLDE_setMsgGraphNeighbor(&g_pDllMsg->m_stHdrs, pGraph );
              if( g_pDllMsg->m_stHdrs.m_ucNeighborIdx < 0xFF )
              {
                  return SFC_SUCCESS;            
              }
          }
          
          Log( LOG_ERROR, LOG_M_DLL, DLOG_Retry, 1, "1" );  
          return SFC_NO_GRAPH; // the graph ID does not exist                   
      }
  }
  
  return SFC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Selects a neighbor from the graph entry. It also considers if that neighbor recently
///         reported congestion, or if neighbor has recently responded with aa NACK.
/// @param  p_pstGraph  - pointer to a graph entry in graph table
/// @param  p_ucRetryNo - number of retries of the message
/// @return index of the selected neighbor
/// @remarks
///      Access level: Interrupt\n
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLDE_selectGraphNeighbor(DLL_SMIB_ENTRY_GRAPH * p_pstGraph, uint8 p_ucRetryNo, uint8 p_ucPrevNeighborIdx )
{
  uint8  ucIdxInGraph = p_ucRetryNo % p_pstGraph->m_stGraph.m_ucNeighCount;  
  uint8 ucCounter = p_pstGraph->m_stGraph.m_ucNeighCount;  
  
  if(     (p_ucPrevNeighborIdx >= g_ucDllNeighborsNo) 
      ||  (p_pstGraph->m_stGraph.m_aNeighbors[ucIdxInGraph] != g_aDllNeighborsTable[p_ucPrevNeighborIdx].m_stNeighbor.m_unUID ) )
  {
      p_ucPrevNeighborIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, p_pstGraph->m_stGraph.m_aNeighbors[ucIdxInGraph]);
  }
  
  for( ; ucCounter; ucCounter-- )
  {  
      if( p_ucPrevNeighborIdx < 0xFF )
      {
          // check if neighbor reported congestion or NACK recently
          if( !g_aDllNeighborsTable[p_ucPrevNeighborIdx].m_stNeighbor.m_ucECNIgnoreTmr
                && !g_aDllNeighborsTable[p_ucPrevNeighborIdx].m_stNeighbor.m_unNACKBackoffTmr )
          {
              return p_ucPrevNeighborIdx;
          }            
      }
      
      if( (++ucIdxInGraph) >= p_pstGraph->m_stGraph.m_ucNeighCount )
      {
          ucIdxInGraph = 0;
      }
      
      p_ucPrevNeighborIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, p_pstGraph->m_stGraph.m_aNeighbors[ucIdxInGraph]);
  }
  
  return p_ucPrevNeighborIdx; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Verifyies if the maxLifetime of a DPDU has been reached, and removes it from the queue
/// @param  none
/// @return none
/// @remarks
///      Access level: Interrupt\n
///      Context: Called once at the start of each TAI second
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLDE_CheckDLLQueueTTL(void)
{  
  DLL_DPDU * pMsg = g_pDllMsgQueueEnd;
  uint8 ucCrtTai = (uint8)g_ulDllTaiSeconds;
  
  while( g_aDllMsgQueue < pMsg )
  {         
      if( (--pMsg)->m_stHdrs.m_ucLifetime == ucCrtTai )
      {
          Log( LOG_ERROR, LOG_M_DLL, DLOG_Confirm, 1, "1", pMsg->m_stHdrs.m_ucPeerAddrLen, &g_pDllMsg->m_stHdrs.m_stPeerAddr );          
        
          DLDE_remMsgFromQueue( pMsg, SFC_TTL_EXPIRED );
      }
  }  
}

static void DLDE_setMsgGraphNeighbor(DLL_MSG_HRDS * p_pDllMsgHdr,  DLL_SMIB_ENTRY_GRAPH * p_pstGraph)
{
    p_pDllMsgHdr->m_ucNeighborIdx = DLDE_selectGraphNeighbor(p_pstGraph, p_pDllMsgHdr->m_ucRetryNo,p_pDllMsgHdr->m_ucNeighborIdx);
    if( p_pDllMsgHdr->m_ucNeighborIdx < 0xFF )
    {
        p_pDllMsgHdr->m_stPeerAddr.m_unShort = g_aDllNeighborsTable[p_pDllMsgHdr->m_ucNeighborIdx].m_stNeighbor.m_unUID;
    }
}


static DLL_SMIB_NEIGHBOR * DLDE_syncMsgNeighborIdx( DLL_MSG_HRDS * p_pDllMsgHdr )
{
    DLL_SMIB_NEIGHBOR * pNeighbor = &g_aDllNeighborsTable[p_pDllMsgHdr->m_ucNeighborIdx].m_stNeighbor;
    
    if( (p_pDllMsgHdr->m_ucNeighborIdx >= g_ucDllNeighborsNo) 
       ||(p_pDllMsgHdr->m_stPeerAddr.m_unShort != pNeighbor->m_unUID) )
    {
        // neighbor table changed, resync
        // m_aMHR+DLL_MHR_DEST_ADDR_OFFSET is 2 bytes aligned, CPU is little endian
        p_pDllMsgHdr->m_ucNeighborIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_NEIGH, p_pDllMsgHdr->m_stPeerAddr.m_unShort );
        if( p_pDllMsgHdr->m_ucNeighborIdx == 0xFF )
            return NULL;
        
        pNeighbor = &g_aDllNeighborsTable[p_pDllMsgHdr->m_ucNeighborIdx].m_stNeighbor;
    }        
    return (!pNeighbor->m_unNACKBackoffTmr ? pNeighbor : NULL );
}
