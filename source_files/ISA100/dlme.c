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
/// Author:       Nivis LLC, Mihaela Goloman
/// Date:         December 2007
/// Description:  This file implements the data link layer - dlme module
/// Changes:      Created 
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
//#include "provision.h"
#include "dlme.h"
#include "mlde.h"
#include "mlsm.h"
#include "dlmo_utils.h"
#include "dmap.h"
#include "dmap_armo.h"
#include "sfc.h"
#include "tmr_util.h"

#define TABLES_CONSTANTS(x,y) { (uint8*)&x, sizeof(*x.m_aTable), sizeof(x.m_aTable)/sizeof(*x.m_aTable), sizeof(x.m_aTable), y }
#define ATTRIBUTES_CONSTANTS(x) { &x, sizeof(x) }

#if (DEVICE_TYPE == DEV_TYPE_MC13225)
    #define MODEM_WORMUP_TMR2       USEC_TO_TMR2(72)
    #define MODEM_CCA_TMR2          USEC_TO_TMR2(128+148) 
    #define MODEM_PREAMBLE_TMR2     USEC_TO_TMR2(5*32+24+32) // 5  = 4 pramble + sfd, 24 = 96-72 + pype delay
#endif

#ifdef DLME_QUEUE_PROTECTION_ENABLED
    uint8  g_ucDLMEQueueBusyFlag;
    uint8  g_ucDLMEQueueCmd;
#endif

    uint8  g_ucHashRefreshFlag;
    
    uint8  g_ucChDiagAlarmTimeout;
    uint8  g_ucDiscoveryAlertTimer;
    
DLL_SMIB_DATA_STRUCT g_stDll;
uint32 g_ulDllTaiSeconds;
uint32 g_ulRadioSleepCounter;

typedef struct
{
  uint8 *      m_pSMIBTable;
  uint16       m_unSMIBSize;
  uint8        m_ucSMIBMaxRecNo;
  uint16       m_unSMIBTableSize;
  uint16       m_unAttrID;
} DLL_TABLES_CONSTANTS;

typedef struct
{
  void * m_pVarAddress;
  uint8  m_ucVarSize; 
} DLL_ATTRIBUTES_CONSTANTS;

typedef struct
{
  uint8   m_ucCmdLength;
  uint8   m_ucMethId;
  uint8   m_ucAttrId;
  uint8   m_ucUnused;
  uint32  m_ulTAI;  
} DLME_CMD_HDR;

const DLL_TABLES_CONSTANTS g_aDllTableConst[SMIB_IDX_NO] =
{
   TABLES_CONSTANTS(g_stDll.m_stChannels, DL_CH),            
   TABLES_CONSTANTS(g_stDll.m_stTimeslots, DL_TS_TEMPL),      
   TABLES_CONSTANTS(g_stDll.m_stNeighbors, DL_NEIGH),         
   TABLES_CONSTANTS(g_stDll.m_stNeighDiag, DL_NEIGH_DIAG),   
   TABLES_CONSTANTS(g_stDll.m_stSuperframes, DL_SUPERFRAME),  
   TABLES_CONSTANTS(g_stDll.m_stGraphs, DL_GRAPH),            
   TABLES_CONSTANTS(g_stDll.m_stLinks, DL_LINK),              
   TABLES_CONSTANTS(g_stDll.m_stRoutes, DL_ROUTE)             
};

const DLL_ATTRIBUTES_CONSTANTS g_aDllAttribConst[DL_NON_INDEXED_ATTR_NO] =
{
   ATTRIBUTES_CONSTANTS(g_ucDllActScanHostFract), // 0
   ATTRIBUTES_CONSTANTS(g_stDllAdvJoinInfo),      // 1
   ATTRIBUTES_CONSTANTS(g_unDllAdvSuperframe),    // 2
   ATTRIBUTES_CONSTANTS(g_unDllSubnetId),         // 3
   ATTRIBUTES_CONSTANTS(g_stSolicTemplate),       // 4
   ATTRIBUTES_CONSTANTS(g_stAdvFilter),           // 5 
   ATTRIBUTES_CONSTANTS(g_stSolicFilter),         // 6
   {&g_ulDllTaiSeconds, 4},                       // 7 // TAI time
   ATTRIBUTES_CONSTANTS(g_stTaiAdjust),           // 8    
   ATTRIBUTES_CONSTANTS(g_ucMaxBackoffExp),       // 9
   ATTRIBUTES_CONSTANTS(g_ucMaxDSDUSize),         // 10
   ATTRIBUTES_CONSTANTS(g_unMaxLifetime),         // 11
   ATTRIBUTES_CONSTANTS(g_unNackBackoffDur)   ,   // 12  
   ATTRIBUTES_CONSTANTS(g_ucLinkPriorityXmit),    // 13
   ATTRIBUTES_CONSTANTS(g_ucLinkPriorityRcv),     // 14
   ATTRIBUTES_CONSTANTS(g_unIdleChannels),        // 15
   ATTRIBUTES_CONSTANTS(g_unClockExpire),         // 16
   ATTRIBUTES_CONSTANTS(g_unClockStale),          // 17
   ATTRIBUTES_CONSTANTS(g_ulRadioSilence),        // 18
   ATTRIBUTES_CONSTANTS(g_ulRadioSleep),          // 19
   ATTRIBUTES_CONSTANTS(g_cRadioTransmitPower),   // 20
   ATTRIBUTES_CONSTANTS(g_unCountryCode),         // 21
   ATTRIBUTES_CONSTANTS(g_stCandidates),          // 22
   ATTRIBUTES_CONSTANTS(g_ucDiscoveryAlert),      // 23
   ATTRIBUTES_CONSTANTS(g_stSmoothFactors),       // 24
   ATTRIBUTES_CONSTANTS(g_stQueuePriority),       // 25
   ATTRIBUTES_CONSTANTS(g_ucAlertPolicy),         // 26
   ATTRIBUTES_CONSTANTS(g_nEnergyLeft),           // 27
   ATTRIBUTES_CONSTANTS(g_stChDiag),              // 28
   (void*)&c_stEnergyDesign, sizeof(c_stEnergyDesign), //  29
   (void*)&c_stCapability, sizeof(c_stCapability),     //  30
   
     
};

// const table for mapping the attribute IDs to local representation in the DLL_ATTRIBUTES_CONSTANTS indexes
// the indexed octet string attributes indexes are have bit7 = 1 (0x80, 0x81, etc)
// 0xFF entries indicate an invalid index (for ex. device capabilities attribute 18 is a const struct) and these attributes are not stored in SMIB tables
// metadata attributes actually reffer to their own smibs (channel meta reffers to channel smib)
const uint8 c_ucAttrIdMap[DL_NO]  = 
// attribute ID    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,   
              { 0xFF, 0, 1, 2, 3, 4, 5, 6, 7, 8,  9, 10, 11, 12, 13, 14, 29, 27, 30, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25                
                , 0x80 | SMIB_IDX_DL_CH                             // DL_CH
                , SMIB_IDX_DL_CH                                    // DL_CH_META
                , 0x80 | SMIB_IDX_DL_TS_TEMPL                       // DL_TS_TEMPL
                , SMIB_IDX_DL_TS_TEMPL                              // DL_TS_TEMPL_META
                , 0x80 | SMIB_IDX_DL_NEIGH                          // DL_NEIGH  
                , 0x80 | SMIB_IDX_DL_NEIGH                          // DL_NEIGH_DIAG_RESET
                , SMIB_IDX_DL_NEIGH                                 // DL_NEIGH_META,       
                , 0x80 | SMIB_IDX_DL_SUPERFRAME                     // DL_SUPERFRAME   
                , 0x80 | SMIB_IDX_DL_SUPERFRAME                     // DL_SF_IDLE
                , SMIB_IDX_DL_SUPERFRAME                            // DL_SUPERFRAME_META 
                , 0x80 | SMIB_IDX_DL_GRAPH                          // DL_GRAPH,
                , SMIB_IDX_DL_GRAPH                                 // DL_GRAPH_META   
                , 0x80 | SMIB_IDX_DL_LINK                           // DL_LINK
                , SMIB_IDX_DL_LINK                                  // DL_LINK_META 
                , 0x80 | SMIB_IDX_DL_ROUTE                          // DL_ROUTE
                , SMIB_IDX_DL_ROUTE                                 // DL_ROUTE_META  
                , 0x80 | SMIB_IDX_DL_NEIGH_DIAG                     // DL_NEIGH_DIAG
                , SMIB_IDX_DL_NEIGH_DIAG                            // DL_DIAG_META
                , 28                                                // DL_CH_DIAG  
                , 26                                                // DL_ALERT_POLICY
              }; 

const DLL_MIB_SMOOTH_FACTORS c_stSmoothFactors = { 0, 10, 10, 0, 10, 10, 0, 1, 1 };

DLL_LOOP_DETECTED g_stLoopDetected;

// DLME message queue
#pragma data_alignment=4        //to work also with 32 bits processors
uint8 g_aDlmeMsgQueue[DLME_MSG_QUEUE_LENGTH]; 

static uint8* g_pDlmeMsgQueueEnd = g_aDlmeMsgQueue; 

static void DLME_addHashEntry( const DLL_HASH_STRUCT * p_pHashEntry );

static DLL_SMIB_NEIGHBOR_DIAG * DLME_findDiagEntry( uint16 p_unNeighUID );
static void DLME_updateNeigDiag(DLL_SMIB_NEIGHBOR* p_pstNeighbor);

static uint8 DLME_setSMIB( LOCAL_SMIB_IDX p_ucLocalAttrID, const void * p_pSMIBStruct );
static uint8 DLME_deleteSMIB( LOCAL_SMIB_IDX  p_ucLocalAttrID, uint16 p_unUID );

static uint8 DLME_executeVirtualAttribute( uint8             p_ucAttrID, 
                                          LOCAL_SMIB_IDX    p_ucLocalID, 
                                          uint8             p_ucCommand, 
                                          uint8 *           p_pValue);

void DLME_refreshTMR2Steps(DLL_SMIB_TIMESLOT * p_pTimeslot);

__monitor uint8 DLME_AddMsgToQueue( uint8   p_ucLength,
                          uint32  p_ulTAI,
                          uint8   p_ucMethId, 
                          uint8   p_ucAttrId,
                          uint8*  p_pucPayload );

void DLME_updateSfChSeq( DLL_SMIB_SUPERFRAME * p_pSf, DLL_SMIB_CHANNEL * p_pChSeq );


void DLME_Init()
{  
  PHY_Disable_Request( PHY_IDLE );
    
  memset(&g_stDll, 0, sizeof(g_stDll));
  
  // init default smooth factor values
  memcpy(&g_stSmoothFactors, &c_stSmoothFactors, sizeof(c_stSmoothFactors));
  // this is an exception, only for neighbor diagnostics (entry postion in table is always static)
  g_ucDllNeighDiagNo = g_aDllTableConst[SMIB_IDX_DL_NEIGH_DIAG].m_ucSMIBMaxRecNo;
  
  g_pDlmeMsgQueueEnd = g_aDlmeMsgQueue;  
      
  g_ucMaxBackoffExp     = DLL_MAX_BACKOFF_EXP_DEFAULT; //will be set by SM during join process
  g_ucMaxDSDUSize       = DLL_MAX_DSDU_SIZE_DEFAULT;
  g_unMaxLifetime       = DLL_MAX_LIFETIME_DEFAULT; 
  g_unNackBackoffDur    = DLL_NACK_BACKOFF_DUR_DEFAULT; 
  g_ucLinkPriorityXmit  = DLL_LINK_PRIORITY_XMIT_DEFAULT;
  g_ucLinkPriorityRcv   = DLL_LINK_PRIORITY_RCV_DEFAULT;
  g_unClockExpire       = DLL_CLOCK_EXPIRE_DEFAULT;
  g_unClockStale        = DLL_CLOCK_STALE_DEFAULT;
  g_ulRadioSilence      = DLL_RADIO_SILENCE_DEFAULT;
  g_cRadioTransmitPower = DLL_RADIO_TRANSMIT_POWER_DEFAULT;
  g_unCountryCode       = DLL_COUNTRY_CODE_DEFAULT;
  
  g_ucHashRefreshFlag     = 0; // used for managing future smibs
  g_ucChDiagAlarmTimeout  = 0; // usedd to avoid multiple chDiag alerts within a period of time
  
  DelayLoop( 1000 ); // dummy wait in order to be sure PHY is idle ?!
  PHY_Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Searches an indexed attribute's row
/// @param  p_ucLocalID - local attribute identifier
/// @param  p_unID - table identifier
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Obs: Caller must disable dll interrupts
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_FindSMIBIdx( LOCAL_SMIB_IDX  p_ucLocalID, uint16 p_unID )
{
  uint8* pTable = g_aDllTableConst[p_ucLocalID].m_pSMIBTable;
  unsigned int unSize = g_aDllTableConst[p_ucLocalID].m_unSMIBSize;
  unsigned int unRecNo = g_stDll.m_aSMIBTableRecNo[p_ucLocalID];
  
  unsigned int unIdx;

  for( unIdx=0; unIdx<unRecNo; unIdx++ )
  {
      if( *(uint16*)(pTable) == p_unID )
          return unIdx;

      pTable += unSize;
  }
  return 0xFF;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Searches an indexed attribute's table address
/// @param  p_ucLocalID - local attribute identifier
/// @param  p_unID - table identifier
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Obs: Caller must disable dll interrupts
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8* DLME_FindSMIB( LOCAL_SMIB_IDX  p_ucLocalID, uint16 p_unID )
{
  uint8* pTable = g_aDllTableConst[p_ucLocalID].m_pSMIBTable;
  unsigned int unSize = g_aDllTableConst[p_ucLocalID].m_unSMIBSize;
  unsigned int unRecNo = g_stDll.m_aSMIBTableRecNo[p_ucLocalID];
  
  for( ; unRecNo; unRecNo-- )
  {
      if( *(uint16*)(pTable) == p_unID )
          return pTable;

      pTable += unSize;
  }
  return NULL;  
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Updates the neighbor diag entry based on the neighbor changes
/// @param  p_pstNeighbor - pointer to the current changed neighbor
/// @return none
/// @remarks
///      Access level: user level\n
///      Obs: Caller must disable dll interrupts
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLME_updateNeigDiag(DLL_SMIB_NEIGHBOR* p_pstNeighbor)
{
    DLL_SMIB_NEIGHBOR_DIAG * pDiag = DLME_findDiagEntry( p_pstNeighbor->m_unUID );
            
    if( p_pstNeighbor->m_ucInfo & DLL_MASK_NEIDIAG ) // need diagnostic
    {
        if( !pDiag )
        {
            pDiag = DLME_findDiagEntry( 0 );
        }
        if( pDiag )
        {
            pDiag->m_unUID = p_pstNeighbor->m_unUID;
            pDiag->m_ucDiagFlags = p_pstNeighbor->m_ucInfo;     
        }
    }
    else  // not need diagnostic
    {
        if( pDiag )
        {
            memset( pDiag, 0, sizeof( *pDiag ) ); 
            pDiag =  NULL;
        }
    }
    p_pstNeighbor->m_pstDiag = pDiag;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Writes an indexed attribute row to the device
/// @param  p_ucLocalID - local attribute identifier
/// @param  p_pSMIBStruct - row data to write
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Obs: Caller must disable dll interrupts
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_setSMIB( LOCAL_SMIB_IDX p_ucLocalID, const void * p_pSMIBStruct )
{      
    uint8 * pSMIB = DLME_FindSMIB(p_ucLocalID, *(uint16*)( p_pSMIBStruct ) );
    uint8  ucRecNo; 
    
    if( !pSMIB ) // not founded, add it
    {
        ucRecNo = g_stDll.m_aSMIBTableRecNo[p_ucLocalID];
      
        if( ucRecNo >= g_aDllTableConst[p_ucLocalID].m_ucSMIBMaxRecNo )
        {
          return SFC_INSUFFICIENT_DEVICE_RESOURCES;
        }
        
        g_stDll.m_aSMIBTableRecNo[p_ucLocalID] = ucRecNo + 1;
        
        pSMIB = g_aDllTableConst[p_ucLocalID].m_pSMIBTable;      
        pSMIB += ucRecNo * g_aDllTableConst[p_ucLocalID].m_unSMIBSize;      
    }
  
    // overwrite it
    memcpy( pSMIB, p_pSMIBStruct, g_aDllTableConst[p_ucLocalID].m_unSMIBSize);    

    switch( p_ucLocalID ) // precomputed fields
    {
    case SMIB_IDX_DL_TS_TEMPL:
        DLME_refreshTMR2Steps(&((DLL_SMIB_ENTRY_TIMESLOT*)pSMIB)->m_stTimeslot); 
        break;
        
    case SMIB_IDX_DL_CH:    
        for( ucRecNo = 0; ucRecNo < g_ucDllSuperframesNo; ucRecNo++ )
        {
            if( g_aDllSuperframesTable[ucRecNo].m_stSuperframe.m_unChIndex == ((DLL_SMIB_CHANNEL*)pSMIB)->m_unUID )
            {
                DLME_updateSfChSeq( &g_aDllSuperframesTable[ucRecNo].m_stSuperframe, (DLL_SMIB_CHANNEL*)pSMIB );
            }
        }
        break;
        
    case SMIB_IDX_DL_SUPERFRAME:
      {
        DLL_SMIB_CHANNEL* pChSeq = (DLL_SMIB_CHANNEL*)DLME_FindSMIB( SMIB_IDX_DL_CH, ((DLL_SMIB_SUPERFRAME*)pSMIB)->m_unChIndex );
        if( pChSeq )
        {
            DLME_updateSfChSeq( (DLL_SMIB_SUPERFRAME*)pSMIB, pChSeq );
        }
      }
        break;
        
    case SMIB_IDX_DL_NEIGH:   
        DLME_updateNeigDiag( (DLL_SMIB_NEIGHBOR*)pSMIB );
        break;
    }
      
    g_ucHashRefreshFlag = 1; // need to recompute the HASH
    return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  Deletes an indexed attribute's row on the device
/// @param  p_ucLocalID - local attribute identifier
/// @param  p_unUID - table identifier
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Obs: Caller must disable dll interrupts
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_deleteSMIB(LOCAL_SMIB_IDX  p_ucLocalID, uint16 p_unUID)
{
  uint8* pTable = g_aDllTableConst[p_ucLocalID].m_pSMIBTable;
  unsigned int unSmibSize = g_aDllTableConst[p_ucLocalID].m_unSMIBSize;
  uint8* pTableEnd = pTable + unSmibSize * g_stDll.m_aSMIBTableRecNo[p_ucLocalID];
  
  for( ; pTable < pTableEnd; pTable += unSmibSize )
  {
      if( *(uint16*)(pTable) == p_unUID )
      {   
          if( p_ucLocalID == SMIB_IDX_DL_NEIGH )
          {
              if ( ((DLL_SMIB_ENTRY_NEIGHBOR*)pTable)->m_stNeighbor.m_pstDiag )
              {
                  memset( ((DLL_SMIB_ENTRY_NEIGHBOR*)pTable)->m_stNeighbor.m_pstDiag, 0, sizeof(DLL_SMIB_NEIGHBOR_DIAG));
              }
          }
          
          memcpy( pTable, pTable + unSmibSize, pTableEnd - pTable - unSmibSize );
          g_stDll.m_aSMIBTableRecNo[p_ucLocalID] --;
          
          g_ucHashRefreshFlag = 1; // need to recompute the HASH
          return SFC_SUCCESS;
      }
  }

  return SFC_INVALID_ELEMENT_INDEX;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Reads a non indexed attribute from the device
/// @param  p_ucAttrID - attribute identifier
/// @param  p_pValue - attribute value
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLME_GetMIBRequest(uint8 p_ucAttrID, void * p_pValue)
{
  if( p_ucAttrID >= DL_NO ) 
      return SFC_INVALID_PARAMETER;  
  
  uint8 ucLocalId = c_ucAttrIdMap[p_ucAttrID];

  if ( ucLocalId < DL_NON_INDEXED_ATTR_NO )
  {
      MONITOR_ENTER();
      
      memcpy(p_pValue,
             g_aDllAttribConst[ ucLocalId ].m_pVarAddress,
             g_aDllAttribConst[ ucLocalId ].m_ucVarSize);
      
      if( p_ucAttrID == DL_TAI_TIME ) // add fraction time
      {
          uint16 unTAIFract = ((uint16)g_stTAI.m_uc250msStep << 13) + TMR0_TO_FRACTION2( TMR_Get250msOffset() );
          ((uint8*)p_pValue)[4] = unTAIFract & 0xFF; // little endian
          ((uint8*)p_pValue)[5] = unTAIFract >> 8; // little endian          
      }
      
      if (p_ucAttrID == DL_CANDIDATES)
      {   // reset candidates on each read
          g_stCandidates.m_ucCandidateNo = 0; 
      }
      else if (p_ucAttrID == DL_CH_DIAG)
      {   // reset channel diagnostics counters
          memset(&g_stChDiag, 0, sizeof(g_stChDiag));         
      }
      
      
      MONITOR_EXIT();
      
      return SFC_SUCCESS;
  }   
  
  return SFC_INVALID_PARAMETER;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Reads an indexed attribute's metadata
/// @param  p_ucAttrID - attribute identifier
/// @param  p_punSize  - metadata size
/// @param  p_pBuf - buffer containing metadata
/// @return service feedback code
/// @remarks Check outside that p_unAttrID is in the correct range
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_GetMetaData(uint16   p_unAttrID,
                       uint16*  p_punSize,
                       uint8*   p_pBuf)
{
  LOCAL_SMIB_IDX ucLocalID = (LOCAL_SMIB_IDX)(c_ucAttrIdMap[p_unAttrID]&0x3f);  
  
  if (DL_NEIGH_DIAG_META == p_unAttrID) // this is an exception, only for neighbor diagnostics
  {
      unsigned int unDiagNo = 0;
      unsigned int unIdx;
      
      for( unIdx = 0; unIdx < DLL_MAX_NEIGH_DIAG_NO; unIdx++)
      {
          if ( g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_unUID )
          {
              unDiagNo++;
          }
      }  
      
      unDiagNo *= (uint16)DIAG_STRUCT_STANDARD_SIZE;
      
      // insert current octets used by diagniostics 
      *(p_pBuf++) = unDiagNo >> 8;  // high byte
      *(p_pBuf++) = unDiagNo;       // low byte
      
      // insert maximum entries in octets that attribute can hold      
      unDiagNo = g_aDllTableConst[ucLocalID].m_ucSMIBMaxRecNo * (uint16)DIAG_STRUCT_STANDARD_SIZE;
      *(p_pBuf++) = unDiagNo >> 8;  // high byte
      *(p_pBuf++) = unDiagNo;       // low byte 
  }
  else
  {
      // insert current entry number
      *(p_pBuf++) = g_stDll.m_aSMIBTableRecNo[ucLocalID] >> 8;  // high byte
      *(p_pBuf++) = g_stDll.m_aSMIBTableRecNo[ucLocalID];   
      // insert maximum entries that attribute can hold
      *(p_pBuf++) = g_aDllTableConst[ucLocalID].m_ucSMIBMaxRecNo >> 8;  // high byte
      *(p_pBuf++) = g_aDllTableConst[ucLocalID].m_ucSMIBMaxRecNo;  
  }   
  
  *p_punSize = 4;
  
  return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Writes a non indexed attribute on the device
/// @param  p_ucAttrID - attribute identifier
/// @param  p_pValue - attribute value
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLME_SetMIBRequest(uint8 p_ucAttrID, const void* p_pValue)
{
  if( p_ucAttrID < DL_NO ) 
  {
      uint8 ucLocalId = c_ucAttrIdMap[p_ucAttrID];
  
      if ( ucLocalId < DL_NON_INDEXED_ATTR_NO )
      {
          MONITOR_ENTER();
          memcpy( g_aDllAttribConst[ ucLocalId ].m_pVarAddress,
                  p_pValue,
                  g_aDllAttribConst[ ucLocalId ].m_ucVarSize );
          
          switch( p_ucAttrID )
          {
              case DL_LINK_PRIORITY_XMIT:
              case DL_LINK_PRIORITY_RCV:
                g_ucHashRefreshFlag = 1; // need to recompute the hash table
                break;
              
              case DL_CLOCK_EXPIRE:
                g_stTAI.m_unSrcClkTimeout = g_unClockExpire;
                g_stTAI.m_ucClkInterogationStatus = MLSM_CLK_INTEROGATION_OFF;
                break;   
              
              case DL_CLOCK_STALE:
                g_stTAI.m_unClkTimeout = g_unClockStale; // clock stale in 1 sec units
                break;
              
              case DL_DISCOVERY_ALERT:
                g_ucDiscoveryAlertTimer = g_ucDiscoveryAlert;
                break;
                
              case DL_RADIO_TRANSMIT_POWER:
                //TO DO - mechanism for updating the Transmit Power
                break;
              default:break;  
          }
          
          MONITOR_EXIT();
          return SFC_SUCCESS;
      }
  }  
  
  return SFC_INVALID_PARAMETER;
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Ion Ticus, Eduard Erdei
/// @brief  Reads an indexed attribute's row data
/// @param  p_ucAttrID - attribute identifier
/// @param  p_unUID - table identifier
/// @param  p_pValue - row data
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLME_GetSMIBRequest(uint8   p_ucAttrID, 
                          uint16  p_unUID, 
                          void *  p_pValue)
{
  uint8 * pSMIB = NULL;
    
  // check the SMIB flag
  if (c_ucAttrIdMap[p_ucAttrID] & 0x80)
  {      
      LOCAL_SMIB_IDX ucLocalID = (LOCAL_SMIB_IDX)(c_ucAttrIdMap[p_ucAttrID]&0x7f);
      
      MONITOR_ENTER();
      
      pSMIB = DLME_FindSMIB( ucLocalID, p_unUID );      
      if( pSMIB )
      {         
          memcpy(p_pValue, pSMIB, g_aDllTableConst[ucLocalID].m_unSMIBSize); 
          
          if (p_ucAttrID == DL_NEIGH_DIAG)
          {
              // reset diagnostic counters 
              memset( &((DLL_SMIB_ENTRY_NEIGDIAG*)pSMIB)->m_stNeighDiag.m_stDiag, 
                      0, 
                      sizeof(DLL_NEIGH_DIAG_COUNTERS) );        
          }
      }      
      
      MONITOR_EXIT();
      
      // this are exceptions, only for SMIBS that are not actually stored in the SMIBS tables;
      // ex: neighbor smib entry conversion to neighborDiagReset entry (neighborDiagReset is not stored separately in smibs tables)
      if ( pSMIB )
      {
          switch( p_ucAttrID )
          {
          case DL_SF_IDLE:
                    {
                        int32  lIdleTimer  = ((DLL_SMIB_ENTRY_SUPERFRAME*) p_pValue)->m_stSuperframe.m_lIdleTimer;
                        uint8  ucIdleFlag  = ((DLL_SMIB_ENTRY_SUPERFRAME*) p_pValue)->m_stSuperframe.m_ucInfo & DLL_MASK_SFIDLE;             
                                     
                        ((DLL_SMIB_ENTRY_IDLE_SF*) p_pValue)->m_stSfIdle.m_unUID = p_unUID;
                        ((DLL_SMIB_ENTRY_IDLE_SF*) p_pValue)->m_stSfIdle.m_lIdleTimer = lIdleTimer;
                        ((DLL_SMIB_ENTRY_IDLE_SF*) p_pValue)->m_stSfIdle.m_ucIdle = ucIdleFlag;
                        break;
                    }
          case DL_NEIGH_DIAG_RESET:
                    {
                        uint8 ucDiagLevel = ((DLL_SMIB_ENTRY_NEIGHBOR*) p_pValue)->m_stNeighbor.m_ucInfo & DLL_MASK_NEIDIAG;
              
                        ((DLL_SMIB_ENTRY_NEIGH_DIAG_RST*) p_pValue)->m_stNeighDiagRst.m_unUID = p_unUID;
                        ((DLL_SMIB_ENTRY_NEIGH_DIAG_RST*) p_pValue)->m_stNeighDiagRst.m_ucLevel = ucDiagLevel;
                        break;              
                    }                    
          }                 
      }
      
      return (pSMIB ? SFC_SUCCESS : SFC_INVALID_ELEMENT_INDEX);      
  }
  
  return SFC_INVALID_PARAMETER;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Ion Ticus, Eduard Erdei
/// @brief  Writes an indexed attribute's row data on the device
/// @param  p_ucAttrID - attribute identifier
/// @param  p_ulSchedTAI - scheduled write time
/// @param  p_pValue - row data
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////

uint8 DLME_SetSMIBRequest(uint8  p_ucAttrID, 
                             uint32 p_ulSchedTAI,
                             const DLMO_DOUBLE_IDX_SMIB_ENTRY * pDoubleIdxEntry)
{
  uint8 ucStatus = SFC_INVALID_PARAMETER;
  
  
  if( p_ucAttrID < DL_NO )
  {
      uint8 ucLocalId = c_ucAttrIdMap[p_ucAttrID];
      if( ucLocalId >= 0x80 && ucLocalId < (0x80|SMIB_IDX_NO) ) // valid SMIB table
      {
          ucLocalId &= 0x7F;
          
            
          ucStatus = DLME_AddMsgToQueue( g_aDllTableConst[ucLocalId].m_unSMIBSize + 2, // +2 for the firs index that is not store in SMIB
                                         p_ulSchedTAI, 
                                         1,                 // add method
                                         p_ucAttrID, 
                                         (uint8*)&pDoubleIdxEntry->m_unUID );  
          
          #ifdef BACKBONE_SUPPORT //backbone support
            if( ucLocalId == SMIB_IDX_DL_SUPERFRAME && !g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_SUPERFRAME] ) // it is first superframe    
            {
                MONITOR_ENTER();
                MLSM_SetSlotLength( pDoubleIdxEntry->m_stEntry.m_stSmib.m_stSuperframe.m_unTsDur );
                MONITOR_EXIT();
            }
          #endif    
      }
  }
  
  return ucStatus;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Ion Ticus, Eduard Erdei
/// @brief  deletes an indexed attribute row
/// @param  p_unAttrID    - attribute identifier
/// @param  p_unUID       - row index
/// @param  p_ulSchedTAI  - TAI time when delete must be performed
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_DeleteSMIBRequest(uint8 p_ucAttrID, 
                             uint16 p_unUID,
                             uint32 p_ulSchedTAI )
{
  uint8 ucStatus = SFC_INVALID_PARAMETER;

  if( p_ucAttrID < DL_NO )
  {
      uint8 ucLocalId = c_ucAttrIdMap[p_ucAttrID];
      if( ucLocalId >= 0x80 && ucLocalId < (0x80|SMIB_IDX_NO) ) // valid SMIB table
      {
          ucStatus = DLME_AddMsgToQueue(2, p_ulSchedTAI, 0, p_ucAttrID, (uint8*)&p_unUID );  // delete method
      }
  }

  return ucStatus;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_ResetSMIBRequest(uint8 p_ucTable, 
                            uint8 p_ucLevel,  
                            uint8 p_bNoRef)
{
//  DISABLE_DLL_TMRIRQ;

//  TBD  
  
//  ENABLE_DLL_TMRIRQ;

  return SFC_FAILURE;
}

void DLME_addHashEntry( const DLL_HASH_STRUCT * p_pHashEntry )
{
    uint16 unSuperframeUID = g_aDllLinksTable[p_pHashEntry->m_ucLinkIdx].m_stLink.m_unSfIdx; 
    uint8  ucPriority = p_pHashEntry->m_ucPriority;
    
    DLL_HASH_STRUCT * pCrtHashStruct = g_stDll.m_aHashTable;
    DLL_HASH_STRUCT * pEndHashStruct = g_stDll.m_aHashTable+g_stDll.m_ucHashTableSize;
    for( ;pCrtHashStruct < pEndHashStruct; pCrtHashStruct++ )
    {
        if( (ucPriority > pCrtHashStruct->m_ucPriority) 
           || ((ucPriority == pCrtHashStruct->m_ucPriority) && (unSuperframeUID > g_aDllLinksTable[pCrtHashStruct->m_ucLinkIdx].m_stLink.m_unSfIdx)) )
        {
            memmove( pCrtHashStruct+1, pCrtHashStruct, (uint8*) ( pEndHashStruct ) - (uint8*) pCrtHashStruct );
            break;
        }
    }
             
    memcpy( pCrtHashStruct, p_pHashEntry, sizeof(DLL_HASH_STRUCT) );    
    g_stDll.m_ucHashTableSize++;
}

void DLME_RebuildHashTable(void)
{
    DLL_HASH_STRUCT   stHashEntry;
    uint8 ucIdx;
    g_stDll.m_ucHashTableSize=0; 
    DLL_SMIB_SUPERFRAME * pSf = NULL;
    
    for( ucIdx=0 ;ucIdx < g_ucDllLinksNo; ucIdx ++ )
    {      
        DLL_SMIB_LINK * pLink = &g_aDllLinksTable[ucIdx].m_stLink;
        
        if( (pLink->m_mfLinkType & DLL_MASK_LNKIDLE) && (0 == pLink->m_ucActiveTimer) )
        {          
            Log( LOG_INFO, LOG_M_DLL, DLOG_Hash, 1, "1", sizeof(DLL_SMIB_LINK), pLink);
            continue;
        }
        
        if( (pSf==NULL) || (pSf->m_unUID != pLink->m_unSfIdx) ) // search optimization
        {
            stHashEntry.m_ucSuperframeIdx = DLME_FindSMIBIdx(SMIB_IDX_DL_SUPERFRAME, pLink->m_unSfIdx);      
            pSf = &g_aDllSuperframesTable[stHashEntry.m_ucSuperframeIdx].m_stSuperframe;          
            
            // don't care if stHashEntry.m_ucSuperframeIdx == 0xFF (will be read from unknwon memmory but it is ok for ucontrollers)
            if ( (pSf->m_ucInfo & DLL_MASK_SFIDLE) && ((0 == pSf->m_lIdleTimer) || (pSf->m_lIdleTimer < -1)) ) 
            {
                stHashEntry.m_ucSuperframeIdx = 0xFF;
            }
        }
        
        if ( stHashEntry.m_ucSuperframeIdx == 0xFF )
        {          
            Log( LOG_ERROR, LOG_M_DLL, DLOG_Hash, 1, "2", sizeof(DLL_SMIB_LINK), pLink);
            continue;
        }
                        
        stHashEntry.m_ucNeighborIdx = 0xFF; // as protection to be sure is populate on all cases
        
        stHashEntry.m_ucLinkType  = pLink->m_mfLinkType;                    
        if( pLink->m_mfLinkType & DLL_MASK_LNKTX ) // TX link
        {
            if( pLink->m_mfLinkType & DLL_MASK_LNKRX ) // are both TX and RX
            {
                stHashEntry.m_ucLinkType -= DLL_MASK_LNKRX; // set as TX only
            }              
            
            stHashEntry.m_ucPriority = g_ucLinkPriorityXmit;            
            
            if( !(pLink->m_mfLinkType & (DLL_MASK_LNKJOIN | DLL_MASK_LNKDISC)) )
            {
                if( pLink->m_mfTypeOptions & DLL_LNK_NEIGHBOR_MASK ) //neighbor by address
                {                    
                    stHashEntry.m_ucNeighborIdx = DLME_FindSMIBIdx(SMIB_IDX_DL_NEIGH, pLink->m_unNeighbor);
                    if( stHashEntry.m_ucNeighborIdx == 0xFF )
                    {          
                        Log( LOG_ERROR, LOG_M_DLL, DLOG_Hash, 1, "5", sizeof(DLL_SMIB_LINK), pLink);
                        continue;
                    }
                }
            }
        }
        else // it is RX only link
        {
            stHashEntry.m_ucPriority = g_ucLinkPriorityRcv;
        }
        
        stHashEntry.m_ucTemplateIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_TS_TEMPL, pLink->m_unTemplate1 );
        if ( stHashEntry.m_ucTemplateIdx == 0xFF )
        {          
            Log( LOG_ERROR, LOG_M_DLL, DLOG_Hash, 1, "6", sizeof(DLL_SMIB_LINK), pLink);
            continue;
        }
        
        if( pLink->m_mfTypeOptions & DLL_MASK_LNKPR )
        {
            stHashEntry.m_ucPriority = pLink->m_ucPriority;
        }
        
        stHashEntry.m_ucPriority <<= 4;
        stHashEntry.m_ucPriority |= (pSf->m_ucInfo & DLL_MASK_SFPRIORITY) >> DLL_ROT_SFPRIORITY;
        
        stHashEntry.m_ucLinkIdx = ucIdx;
        DLME_addHashEntry( &stHashEntry );
        
        if( (pLink->m_mfLinkType & (DLL_MASK_LNKTX |  DLL_MASK_LNKRX) ) == ((DLL_MASK_LNKTX |  DLL_MASK_LNKRX) ) ) // shared links
        {
            
            stHashEntry.m_ucTemplateIdx = DLME_FindSMIBIdx( SMIB_IDX_DL_TS_TEMPL, pLink->m_unTemplate2 );
            if ( stHashEntry.m_ucTemplateIdx == 0xFF )
            {          
                Log( LOG_ERROR, LOG_M_DLL, DLOG_Hash, 1, "7", sizeof(DLL_SMIB_LINK), pLink);
                continue;
            }
            
            stHashEntry.m_ucLinkType = pLink->m_mfLinkType - DLL_MASK_LNKTX; // set as RX only
            stHashEntry.m_ucPriority &= 0x0F;
            stHashEntry.m_ucPriority |= (g_ucLinkPriorityRcv << 4);
            DLME_addHashEntry( &stHashEntry );
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches for a free (unused) entry in the neighbor diagnostics buffers
/// @param  p_unNeighUID - neighbor identifier
/// @return pointer to the free entry or null if not available
/// @remarks
///      Access level: Interrupt Level
////////////////////////////////////////////////////////////////////////////////////////////////////
DLL_SMIB_NEIGHBOR_DIAG * DLME_findDiagEntry(uint16 p_unNeighUID)
{
  for (uint8 ucIdx = 0; ucIdx < DLL_MAX_NEIGH_DIAG_NO; ucIdx++)
  {
      if ( g_aDllNeighDiagTable[ucIdx].m_stNeighDiag.m_unUID == p_unNeighUID )
      {
          return &g_aDllNeighDiagTable[ucIdx].m_stNeighDiag;
      }
  }  
  return NULL; // no free entry has been found
}

uint16 subWormupFromTmr2Value( uint16 p_unFractionValue, uint16 p_unRemoveValue )
{
   p_unFractionValue =  FRACTION_TO_TMR2( p_unFractionValue );
   if( p_unFractionValue <= p_unRemoveValue )
      return 0;
   
   return (p_unFractionValue - p_unRemoveValue);
}

void DLME_refreshTMR2Steps(DLL_SMIB_TIMESLOT * p_pTimeslot)
{
    // on TX or RX tamplates  TMR2 values are with same offset on strcuture
    p_pTimeslot->m_utTemplate.m_stRx.m_unEnableReceiverTMR2 = subWormupFromTmr2Value(p_pTimeslot->m_utTemplate.m_stRx.m_unEarliestDPDU, MODEM_WORMUP_TMR2+MODEM_PREAMBLE_TMR2);    
    p_pTimeslot->m_utTemplate.m_stRx.m_unTimeoutTMR2 = subWormupFromTmr2Value(p_pTimeslot->m_utTemplate.m_stRx.m_unTimeoutDPDU, MODEM_WORMUP_TMR2+MODEM_PREAMBLE_TMR2);    
    p_pTimeslot->m_utTemplate.m_stRx.m_unAckEarliestTMR2 = subWormupFromTmr2Value(p_pTimeslot->m_utTemplate.m_stRx.m_unAckEarliest, MODEM_WORMUP_TMR2+MODEM_PREAMBLE_TMR2);    
    p_pTimeslot->m_utTemplate.m_stRx.m_unAckLatestTMR2 = subWormupFromTmr2Value(p_pTimeslot->m_utTemplate.m_stRx.m_unAckLatest, MODEM_WORMUP_TMR2+MODEM_PREAMBLE_TMR2);
      
    if( (p_pTimeslot->m_ucInfo & DLL_MASK_TSTYPE) == DLL_TX_TIMESLOT ) // TX timeslot
    {
        if(p_pTimeslot->m_ucInfo & DLL_MASK_TSCCA )
        {        
            p_pTimeslot->m_utTemplate.m_stTx.m_unXmitEarliestTMR2 -= MODEM_CCA_TMR2;
            p_pTimeslot->m_utTemplate.m_stTx.m_unXmitLatestTMR2   -= MODEM_CCA_TMR2;
        }        
    }
    else // RX slot (Just TX and RX timeslot type accepted)
    {
          p_pTimeslot->m_utTemplate.m_stRx.m_unAckEarliestTMR2 = 
            (p_pTimeslot->m_utTemplate.m_stRx.m_unAckEarliestTMR2 + p_pTimeslot->m_utTemplate.m_stRx.m_unAckLatestTMR2) >> 1;
    }
}

void DLME_CopyReversedEUI64Addr( uint8 * p_pDst, const uint8 * p_pSrc )
{
    const uint8 * p_pSrcEnd = p_pSrc+8;
    
    do
    {
        *(p_pDst++) = *(--p_pSrcEnd);
    } 
    while ( p_pSrc < p_pSrcEnd );
}



////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Dorin Pavel
/// @brief  Adds a command to the message queue
/// @param  p_ucLength - command size
/// @param  p_ulTAI - scheduled command time
/// @param  p_ucMethID - method identifier
/// @param  p_ucAttrId - attribute identifier
/// @param  p_pucPayload - command data
/// @return service feedback code (success or queue full)
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor uint8 DLME_AddMsgToQueue( uint8   p_ucLength,
                          uint32  p_ulTAI,
                          uint8   p_ucMethId, 
                          uint8   p_ucAttrId,
                          uint8*  p_pucPayload )
{  
  uint8  ucSFC = SFC_SUCCESS;  
  uint16 unCmdLength = DMAP_GetAlignedLength( p_ucLength + sizeof(DLME_CMD_HDR) ); 
  
  DLME_CMD_HDR stCmdHdr;
  
#ifdef DLME_QUEUE_PROTECTION_ENABLED
    g_ucDLMEQueueCmd = 1;
#endif
  
  stCmdHdr.m_ucCmdLength = unCmdLength;
  stCmdHdr.m_ucMethId    = p_ucMethId;
  stCmdHdr.m_ucAttrId    = p_ucAttrId;
  stCmdHdr.m_ucUnused    = 0;
  stCmdHdr.m_ulTAI       = p_ulTAI;        

  MONITOR_ENTER();

  // ordering by TAI
  uint8* pIdx; 
  for( pIdx = g_aDlmeMsgQueue; 
       pIdx < g_pDlmeMsgQueueEnd; 
       pIdx += ((DLME_CMD_HDR*)pIdx)->m_ucCmdLength )
  {    
      if( ((DLME_CMD_HDR*)pIdx)->m_ulTAI == p_ulTAI ) 
      {
          // already exists? 
          if(    ( *(uint32*)pIdx == *(uint32*)&stCmdHdr ) 
              && !memcmp( pIdx+sizeof(DLME_CMD_HDR), p_pucPayload, p_ucLength ) )
          {
              MONITOR_EXIT();                      
              return ucSFC;  
          }
      }
      else if( ((DLME_CMD_HDR*)pIdx)->m_ulTAI > p_ulTAI )
      {
        break;
      }
  }
  
  if( g_pDlmeMsgQueueEnd + unCmdLength <= g_aDlmeMsgQueue + DLME_MSG_QUEUE_LENGTH) // has space
  {
      memmove(pIdx + unCmdLength, pIdx, g_pDlmeMsgQueueEnd - pIdx);
      
      // store a new command
      Log( LOG_DEBUG, LOG_M_DLL, DLOG_SMIB_Mod, sizeof(DLME_CMD_HDR), pIdx, 4,  p_pucPayload );
      
       memcpy(pIdx, &stCmdHdr, sizeof(DLME_CMD_HDR));
       memcpy(pIdx+sizeof(DLME_CMD_HDR), p_pucPayload, p_ucLength);
      
       g_pDlmeMsgQueueEnd += unCmdLength;
  }  
  else
  {
      ucSFC = SFC_INSUFFICIENT_DEVICE_RESOURCES; 
  }  
  
  MONITOR_EXIT();  
    
  return ucSFC;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Dorin Pavel
/// @brief  Message queue parser
/// @return none
/// @remarks
///      Access level: interrupt level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLME_ParseMsgQueue( void )
{
  uint8* pIdx;

#ifdef DLME_QUEUE_PROTECTION_ENABLED
  g_ucDLMEQueueBusyFlag = 0;
#endif
  
  for(  pIdx = g_aDlmeMsgQueue; 
        pIdx < g_pDlmeMsgQueueEnd && ((DLME_CMD_HDR*)pIdx)->m_ulTAI <= g_ulDllTaiSeconds;  
        pIdx += ((DLME_CMD_HDR*)pIdx)->m_ucCmdLength )
  {      
    
      // exec cmd
      uint8 ucAttrID = ((DLME_CMD_HDR*)pIdx)->m_ucAttrId;
      LOCAL_SMIB_IDX ucLocalID = (LOCAL_SMIB_IDX)c_ucAttrIdMap[ucAttrID];
      uint8 * pValue = pIdx + sizeof(DLME_CMD_HDR);
      
      if ( (ucLocalID & 0x80) && ((ucLocalID & 0x7F) < SMIB_IDX_NO) ) // smib request
      {      
          ucLocalID &= 0x7F;
          
          // if is not an exception, continue as regular action
          if ( !  DLME_executeVirtualAttribute(ucAttrID, ucLocalID, ((DLME_CMD_HDR*)pIdx)->m_ucMethId, pValue) )
          {
              if( !((DLME_CMD_HDR*)pIdx)->m_ucMethId || *(uint16*)pValue != *(uint16*)(pValue+2) ) // detele request, or different SMIB add
              {
                  Log( LOG_DEBUG, LOG_M_DLL, DLOG_SMIB_Del, 1, &((DLME_CMD_HDR*)pIdx)->m_ucAttrId, 2,  pValue );
                  DLME_deleteSMIB( ucLocalID, *(uint16*)pValue );
              }
              
              if( ((DLME_CMD_HDR*)pIdx)->m_ucMethId ) // add request
              {                  
                  Log( LOG_DEBUG, LOG_M_DLL, DLOG_SMIB_Add, 1, &((DLME_CMD_HDR*)pIdx)->m_ucAttrId, 2,  pValue+2 );
                  DLME_setSMIB( ucLocalID, pValue+2 );
              }
          }          
      }
      else // MIB request
      {
          DLME_SetMIBRequest(((DLME_CMD_HDR*)pIdx)->m_ucAttrId, pValue );
      }  
  }

  // update the message queue (shift it) if necessary
  if( pIdx != g_aDlmeMsgQueue )
  {  
    memmove(g_aDlmeMsgQueue, pIdx, g_pDlmeMsgQueueEnd - pIdx);
    g_pDlmeMsgQueueEnd -= (pIdx - g_aDlmeMsgQueue);
  }  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Handles the write of several virtual indexed attributes such as NeighborDiagReset and SfIdle.
/// @param  p_ucAttrID - attribute identifier
/// @param  p_ucLocalID - local attribute identifier
/// @param  p_ucCommand - operation identifier (add/delete...)
/// @return flag - 1 if virtual attribute
/// @remarks
///      Access level: interrupt level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_executeVirtualAttribute(uint8             p_ucAttrID, 
                                   LOCAL_SMIB_IDX    p_ucLocalID, 
                                   uint8             p_ucCommand, 
                                   uint8 *           p_pValue)
{
    uint8 * pSmib;
    
    if( (p_ucAttrID == DL_SF_IDLE) || (p_ucAttrID == DL_NEIGH_DIAG_RESET) )
    {
        if( p_ucCommand )  // add command
        {        
            pSmib = DLME_FindSMIB( p_ucLocalID, *(uint16*)(p_pValue) );
            if( pSmib ) // founded
            {
                if( p_ucAttrID == DL_SF_IDLE )
                {
                    memcpy( &((DLL_SMIB_SUPERFRAME*)pSmib)->m_lIdleTimer, &((DLL_SMIB_SF_IDLE*)(p_pValue+2))->m_lIdleTimer, sizeof(((DLL_SMIB_SUPERFRAME*)pSmib)->m_lIdleTimer) );
                    ((DLL_SMIB_SUPERFRAME*)pSmib)->m_ucInfo &= ~DLL_MASK_SFIDLE; 
                    ((DLL_SMIB_SUPERFRAME*)pSmib)->m_ucInfo |=  ((DLL_SMIB_SF_IDLE*)(p_pValue+2))->m_ucIdle & DLL_MASK_SFIDLE;
                    
                    g_ucHashRefreshFlag = 1; // need to recompute the hash table  
                }
                else // p_ucAttrID == DL_NEIGH_DIAG_RESET
                {
                    ((DLL_SMIB_NEIGHBOR*)pSmib)->m_ucInfo &= ~DLL_MASK_NEIDIAG;
                    ((DLL_SMIB_NEIGHBOR*)pSmib)->m_ucInfo |= ((DLL_SMIB_NEIGH_DIAG_RST*)(p_pValue+2))->m_ucLevel & DLL_MASK_NEIDIAG;       
                
                    //update also the appropriate Neighbor Diag
                    DLME_updateNeigDiag( (DLL_SMIB_NEIGHBOR*)pSmib );
                }
            }
        }
        // else is a delete command; delete is not allowed on virtual attributes
        
        return 1; // virtual -> discard it        
    }

    return 0;    
}
     
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  
/// @param  - none
/// @return - none
/// @remarks  g_ucDiscoveryAlert enable and candidateNo must be checked outside!
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DLME_CheckNeighborDiscoveryAlarm(void)
{
  // check if discovery alert enabled 
  // if ( g_ucDiscoveryAlert && g_stCandidates.m_ucCandidateNo) // g_ucDiscoveryAlert is checked outside
  //{
      ALERT stAlert;
      uint8 aucAlertBuf[sizeof(DLL_MIB_CANDIDATES)];
      
      // obs: following invocation of DLMO_ReadCandidates will also clear the candidates list
      if (SFC_SUCCESS == DLMO_ReadCandidates(DL_CANDIDATES, &stAlert.m_unSize, aucAlertBuf))
      {
          stAlert.m_ucPriority = ALERT_PR_MEDIUM_M;
          stAlert.m_unDetObjTLPort = 0xF0B0; // DLMO is DMAP port
          stAlert.m_unDetObjID = DMAP_DLMO_OBJ_ID; 
          stAlert.m_ucClass = ALERT_CLASS_EVENT; 
          stAlert.m_ucDirection = ALARM_DIR_RET_OR_NO_ALARM; 
          stAlert.m_ucCategory = ALERT_CAT_COMM_DIAG; 
          stAlert.m_ucType = DL_NEIGH_DISCOVERY_ALERT;
          
          ARMO_AddAlertToQueue( &stAlert, aucAlertBuf ); 
      }
  //}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  
/// @param  - none
/// @return - none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_CheckChannelDiagAlarm(void)
{  
  uint8 ucGenerateAlarm = 0;
  
  if( g_ucChDiagAlarmTimeout )
  {
      g_ucChDiagAlarmTimeout--; 
  }
  
  // check if dl_connectivity alert is enabled and if a chDiag alarm has been sent previously 
  if( g_ucAlertPolicy.m_ucEnabled && !g_ucChDiagAlarmTimeout )
  {
      // get a copy of g_stChDiag; do not use DLME_GetMIBRequest() because counters do not need to be reset
      DLL_MIB_CHANNEL_DIAG stChDiag;  
      
      MONITOR_ENTER();      
      memcpy(&stChDiag, &g_stChDiag, sizeof(stChDiag));      
      MONITOR_EXIT(); 
            
      for (uint8 ucIdx = 0; ucIdx < 16; ucIdx++)
      {
          if (stChDiag.m_astCh[ucIdx].m_unUnicastTxCtr <= g_ucAlertPolicy.m_unChanMinUnicast)
              continue; // there is an insuficient number of unicast transactions on this channel
          
          uint8 ucRatio = (uint32)(stChDiag.m_astCh[ucIdx].m_unNoAckCtr * 100) / stChDiag.m_astCh[ucIdx].m_unUnicastTxCtr;
          
          if (ucRatio > g_ucAlertPolicy.m_unNoAckThresh)
          {
               ucGenerateAlarm = 1;
               break; // do not continue searching for alarm conditions
          }
          
          ucRatio = (uint32)(stChDiag.m_astCh[ucIdx].m_unCCABackoffCtr * 100) / stChDiag.m_astCh[ucIdx].m_unUnicastTxCtr;
          
          if (ucRatio > g_ucAlertPolicy.m_ucCCABackoffThresh)
          {
               ucGenerateAlarm = 1;
               break; // do not continue searching for alarm conditions
          }          
      }
      
      if (ucGenerateAlarm)
      {
          ALERT stAlert;
          uint8 aucAlertBuf[1+34]; // alert will contain the attributeID + an exact copy of dl-channel-diag

          aucAlertBuf[0] = DL_CH_DIAG;
          uint16 unSize = sizeof(aucAlertBuf) - 1;                    
          
          if (SFC_SUCCESS == DLMO_BinarizeChDiagnostics(&stChDiag, &unSize, aucAlertBuf+1))
          {
              stAlert.m_ucPriority = ALERT_PR_MEDIUM_M;
              stAlert.m_unDetObjTLPort = 0xF0B0; // DLMO is DMAP port
              stAlert.m_unDetObjID = DMAP_DLMO_OBJ_ID; 
              stAlert.m_ucClass = ALERT_CLASS_ALARM; 
              stAlert.m_ucDirection = ALARM_DIR_IN_ALARM; 
              stAlert.m_ucCategory = ALERT_CAT_COMM_DIAG; 
              stAlert.m_ucType = DL_CONNECTIVITY_ALERT;              
              stAlert.m_unSize = 1+unSize;
              
              if( SFC_SUCCESS == ARMO_AddAlertToQueue( &stAlert, aucAlertBuf ) )
              {              
                  g_ucChDiagAlarmTimeout = DLMO_NEIGH_DIAG_ALERT_TIMEOUT;
              }
          }
      }
  }
  
  return ucGenerateAlarm;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  
/// @param  - none
/// @return - none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_CheckNeighborDiagAlarm(void)
{    
  uint8 bAlertSent = 0;
  
  // check if dl_connectivity alert is enabled
  if (g_ucAlertPolicy.m_ucEnabled)
  {    
      ALERT stAlert;
      uint8 aucAlertBuf[1+24]; // alert will contain the attributeID + an exact copy of dl-neighbor-diag
      
      stAlert.m_ucPriority = ALERT_PR_MEDIUM_M;
      stAlert.m_unDetObjTLPort = 0xF0B0; // DLMO is DMAP port
      stAlert.m_unDetObjID = DMAP_DLMO_OBJ_ID; 
      
      if( g_stLoopDetected.m_unSrcAddr ) // loop detected
      {
          aucAlertBuf[0] = DL_ROUTE;
          aucAlertBuf[1] = 0; // for future extension
          aucAlertBuf[2] = 1; // loop detected (2 may be dead end path ...)
          aucAlertBuf[3] = g_stLoopDetected.m_ucHopLeft;
          memcpy( aucAlertBuf+4, &g_stLoopDetected.m_unSrcAddr, 4 );
          
          g_stLoopDetected.m_unSrcAddr = 0;
          
          stAlert.m_unSize = sizeof(aucAlertBuf);
          
          stAlert.m_ucClass = ALERT_CLASS_EVENT; 
          stAlert.m_ucDirection = ALARM_DIR_RET_OR_NO_ALARM; 
          stAlert.m_ucCategory = ALERT_CAT_COMM_DIAG; 
          stAlert.m_ucType = DL_CUSTOM_DIAG_ALERT;              
          
          ARMO_AddAlertToQueue( &stAlert, aucAlertBuf ); 
      }      
      
      for (uint8 ucIdx=0; ucIdx < g_ucDllNeighborsNo; ucIdx++)
      {
          MONITOR_ENTER();      
          if( g_aDllNeighborsTable[ucIdx].m_stNeighbor.m_ucAlertTimeout )
          {
              g_aDllNeighborsTable[ucIdx].m_stNeighbor.m_ucAlertTimeout --;
          }
          else if( g_aDllNeighborsTable[ucIdx].m_stNeighbor.m_pstDiag 
                   && !bAlertSent
                   && (g_aDllNeighborsTable[ucIdx].m_stNeighbor.m_pstDiag->m_stDiag.m_unUnicastTxCtr > g_ucAlertPolicy.m_unNeiMinUnicast) )
          {
              // get a copy of the diagnostic structure; do not use DLME_GetMIBRequest() because counters do not need to be reset
              DLMO_SMIB_ENTRY stEntry;    
              
              memcpy( &stEntry.m_stSmib.m_stNeighDiag, 
                      g_aDllNeighborsTable[ucIdx].m_stNeighbor.m_pstDiag, 
                      sizeof(stEntry.m_stSmib.m_stNeighDiag));      
              
              uint32 ulTotalFailNo = (stEntry.m_stSmib.m_stNeighDiag.m_stDiag.m_unTxFailedNo 
                                        + stEntry.m_stSmib.m_stNeighDiag.m_stDiag.m_unTxCCABackoffNo 
                                        + stEntry.m_stSmib.m_stNeighDiag.m_stDiag.m_unTxNackNo) * 100;
              uint8 ucRatio = ulTotalFailNo / stEntry.m_stSmib.m_stNeighDiag.m_stDiag.m_unUnicastTxCtr;
              
              if (ucRatio > g_ucAlertPolicy.m_ucNeiErrThresh)
              {
                  // alarm condition fulfilled; generate alarm (do not reset diag counters)                  

                  aucAlertBuf[0] = DL_NEIGH_DIAG;                                    
                  stAlert.m_unSize = 1 + DLMO_GetSMIBNeighDiagEntry((const DLMO_SMIB_ENTRY*)&stEntry.m_stSmib.m_stNeighDiag, aucAlertBuf+1);
                  
                  stAlert.m_ucClass         = ALERT_CLASS_ALARM; 
                  stAlert.m_ucDirection     = ALARM_DIR_IN_ALARM; 
                  stAlert.m_ucCategory      = ALERT_CAT_COMM_DIAG; 
                  stAlert.m_ucType          = DL_CONNECTIVITY_ALERT;              
                  
                  uint8 bRspCode = ARMO_AddAlertToQueue( &stAlert, aucAlertBuf );
                  
                  if( SFC_SUCCESS == bRspCode )
                  {
                      g_aDllNeighborsTable[ucIdx].m_stNeighbor.m_ucAlertTimeout = DLMO_NEIGH_DIAG_ALERT_TIMEOUT;
                  }                                   
                  
                  bAlertSent = 1; // too much work for this time; following neighbors will be checked next time
              }
          }
          MONITOR_EXIT(); 
      }      
  }
  
  return bAlertSent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Doinel Alban
/// @brief  Extracts radio channel from specified channel hopping sequence and index
/// @param  p_pChTable - pointer to m_aSequence[] into a DLL_SMIB_CHANNEL struct
/// @param  p_ucChIdx  - channel index
/// @return channel number
/// @remarks  
///      Access level: Interrupt level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 MLSM_GetChFromTable( const uint8 * p_pChTable, uint8 p_ucChIdx )
{
    uint8 ucCh = p_pChTable[p_ucChIdx / 2];
    if((p_ucChIdx & 0x01) == 0) // first channel value is on the lower nibble
    {
        return (ucCh & 0x0F);
    }
    return (ucCh >> 4);
}

void MLSM_setChToTable( uint8 * p_pChTable, uint8 p_ucChIdx, uint8 p_ucCh )
{
    if(p_ucChIdx & 0x01) 
    {
        p_pChTable[p_ucChIdx / 2] |= p_ucCh << 4;
    }
    else
    {
        p_pChTable[p_ucChIdx / 2] = p_ucCh;
    }
}

void DLME_updateSfChSeq( DLL_SMIB_SUPERFRAME * p_pSf, DLL_SMIB_CHANNEL * p_pChSeq )
{
    p_pSf->m_ucChSeqLen = 0;
    
    for( uint8 i = 0; i<p_pChSeq->m_ucLength; i++  )
    {
        uint8 ucCh = MLSM_GetChFromTable( p_pChSeq->m_aSeq, i );
        if( p_pSf->m_unChMap & (1<<ucCh) )
        {
            MLSM_setChToTable( p_pSf->m_aChSeq, p_pSf->m_ucChSeqLen++, ucCh );
        }
    }
    g_ucDllRestoreSfOffset = 1;  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Selects the strongest RSSI of all diagnostic neighbors
/// @param  - none
/// @return strongest RSSI
/// @remarks  
///      Access level: User level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLME_GetStrongestNeighbor( void )
{
  int8 cRSSI = -128;
  
  MONITOR_ENTER();
  
  for( uint16 unIdx = 0; unIdx < DLL_MAX_NEIGH_DIAG_NO; unIdx++ )
  {
      if( g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_unUID 
            && ((g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_ucDiagFlags & DLL_MASK_NEICLKSRC) == 0x80) // only preferred clock source
            && (g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_cRSSI > cRSSI) )
      {
          cRSSI = g_aDllNeighDiagTable[unIdx].m_stNeighDiag.m_cRSSI;       
      }
  }
  
  MONITOR_EXIT();
  
  return cRSSI + ISA100_RSSI_OFFSET;
}
