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
/// Description:  This file holds definitions of the upper sub-layer of data link layer
/// Changes:      Rev 1.0 - created
/// Revisions:    1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DLDE_H_
#define _NIVIS_DLDE_H_

/*===========================[ public includes ]=============================*/
#include "../typedef.h"
#include "provision.h"
#include "dlme.h"

/*==========================[ public datatypes ]=============================*/

typedef struct
{
  HANDLE  m_hHandle;
  uint16  m_unGraphID;
  uint8   m_ucRetryNo;
  uint8   m_ucPriority;
  uint8   m_ucPeerAddrLen;
  uint8   m_ucLifetime; // implements dlmo11a.MaxLifetime (stored in seconds here)
  
  // (part of the DPDU)
  uint8   m_ucDROUTLen; 
  uint8   m_ucPayloadLen;   
  
  union  
  {
      uint8   m_aEUI64[8]; 
      uint16  m_unShort;  
  } m_stPeerAddr;
    
  uint8   m_aDROUT[2*MAX_DROUT_ROUTES_NO + 3]; 
    
  uint8   m_ucDADDRCtrlByte;
  uint16  m_unDADDRSrcAddr;
  uint16  m_unDADDRDstAddr;
  
  uint8   m_ucDHDR;
  uint8   m_ucNeighborIdx; // use to keep precomputed IDX
  uint8   m_ucFinalIdx;    // use to keep precomputed IDX 
  uint8   m_ucGraphIdx;    // use to keep precomputed IDX 
  
} DLL_MSG_HRDS;

typedef struct
{
  // some management info (not part of the DPDU)
  DLL_MSG_HRDS m_stHdrs;
  uint8   m_aPayload[DLL_MAX_DSDU_SIZE_DEFAULT];
} DLL_DPDU;

typedef struct
{
    uint8   m_ucAddrType;
    uint8   m_ucSeqNo;
    uint16  m_unSunbetId;
    uint8   m_ucSecurityPos;
    
    uint8   m_bDestIsNL : 1;
    uint8   m_bDiscardFlag : 1;
    uint8   m_bIsAdvertise : 1;
} DLL_RX_MHR;

/*==========================[ public defines ]===============================*/    
#define GRAPH_ID_PREFIX       0xA000  // the "1010" prefix for the 12bit graph IDs
#define GRAPH_PREFIX_MASK     0xF000  // mask for the "1010" prefix of a graph ID 
#define GRAPH_MASK            0x0FFF    
#define DLL_NEXT_HOP_HANDLE   0xF100  // this handle is used for "in transit" DLL frames

typedef enum {
  UNICAST_DEST = 0,
  DUOCAST_DEST,
  BROADCAST_DEST
} DESTINATION_TYPE;

typedef enum {
  DLL_ACK = 0,
  DLL_ACK_ECN,
  DLL_NACK0,
  DLL_NACK1
} DLL_ACK_TYPE;

typedef enum {
  LINK_RECOVERY = 0,
  TIME_RECOVERY
} RECOVERY_TYPE;

/*==========================[ public variables ]==============================*/

/* DLL message queue */
extern DLL_DPDU g_aDllMsgQueue[DLL_MSG_QUEUE_SIZE_MAX];

/* chosen message within the queue */
extern DLL_DPDU * g_pDllMsg;

/* actual queue size */
extern DLL_DPDU * g_pDllMsgQueueEnd;

/* current HashTable element */
extern DLL_HASH_STRUCT * g_pCrtHashEntry;

/* number of discarded DLL messages */
extern uint16 g_unDiscardedDllMsgNo;

extern uint8 g_ucPingStatus;

extern DLL_DPDU * g_pDiscardMsg;

#define SET_DebugLastAck(...)

extern uint8 g_ucTxNeighborIdx;

/*==========================[ public functions ]==============================*/

extern void DLL_Init(void);

extern uint8 DLDE_ScheduleLink (void);

extern void DLDE_Data_Request(uint8        p_ucDstAddrLen,
                              uint8 *      p_pDstAddr,
                              uint8        p_ucPriority,
                              uint16       p_unContractID,
                              uint8        p_ucPayloadLength,
                              const void * p_pPayload,
                              HANDLE       p_hHandle);

extern void DLDE_HandleRxMessage( uint8 * p_pRxBuff );

extern void DLDE_AckInterpreter( uint16        p_unAckTypeErrCode,
                                 const uint8 * p_pRxBuff );

extern uint8 DLDE_CheckIfDestIsNwk( uint16 p_unFinalShortAddr );

extern void DLDE_CheckQueueMsgs();

extern void DLDE_CheckDLLQueueTTL(void);

extern DLL_DPDU * DLDE_SearchDiscardableDllMsg(uint8 p_ucPriority);

extern uint8 DLDE_GetMsgNoInQueue( uint16 p_unShortAddress );

extern uint8 DLDE_GetMsgNoInQueueForGraph( uint16 p_unGraphID );

extern unsigned int DLDE_CheckForwardQueueLimitation( uint8 p_ucPriority, uint8* p_pucReason );

extern void DLDE_DiscardMsg( uint16 p_unHandle );


#endif  // _NIVIS_DLDE_H_
