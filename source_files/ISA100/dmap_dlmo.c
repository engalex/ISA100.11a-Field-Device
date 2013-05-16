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
/// Date:         January 2008
/// Description:  This file implements the data link management object in DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "dmap_dlmo.h"
#include "dmap.h"
#include "aslsrvc.h"
#include "mlde.h"
#include "mlsm.h"
#include "dlmo_utils.h"
#include "dmap_utils.h"

typedef uint8 (*DLMO_GET_SMIB_FCT)(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
typedef uint8 (*DLMO_SET_SMIB_FCT)(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);

typedef uint8 (*DLMO_GET_MIB_FCT)(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);
typedef uint8 (*DLMO_SET_MIB_FCT)(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);

typedef struct
{
  uint8   m_ucAccess;
  uint8   m_ucLocalParserIdx;
} DLMO_ATTR_PARSER_STRUCT; 

static uint8 DLMO_readRow(uint16  p_unAttrID, 
                          uint16  p_unReqSize, 
                          uint8*  p_pReqBuf,
                          uint16* p_pRspSize,
                          uint8*  p_pRspBuf);

const DLMO_GET_SMIB_FCT c_aGetSMIBFct[SMIB_IDX_PARSER_NO] = {
                                    DLMO_GetSMIBChannelEntry,    
                                    DLMO_GetSMIBTimeslotEntry,   
                                    DLMO_GetSMIBNeighborEntry,   
                                    DLMO_GetSMIBNeighDiagResetEntry,                                    
                                    DLMO_GetSMIBSuperframeEntry,    
                                    DLMO_GetSMIBSfIdleEntry,        
                                    DLMO_GetSMIBGraphEntry,         
                                    DLMO_GetSMIBLinkEntry,          
                                    DLMO_GetSMIBRouteEntry,   
                                    DLMO_GetSMIBNeighDiagEntry
};

const DLMO_SET_SMIB_FCT c_aSetSMIBFct[SMIB_IDX_PARSER_NO] = {
                                    DLMO_SetSMIBChannelEntry,    
                                    DLMO_SetSMIBTimeslotEntry,   
                                    DLMO_SetSMIBNeighborEntry,   
                                    DLMO_SetSMIBNeighDiagResetEntry,                                    
                                    DLMO_SetSMIBSuperframeEntry,    
                                    DLMO_SetSMIBSfIdleEntry,        
                                    DLMO_SetSMIBGraphEntry,         
                                    DLMO_SetSMIBLinkEntry,          
                                    DLMO_SetSMIBRouteEntry,
                                    DLMO_SetSMIBNeighDiagEntry   // obs: neighbor diagnostics are read only; it implements an empty function
};


const DLMO_GET_MIB_FCT c_aGetMIBFct[] = {
                                  DLMO_ReadUInt8,               
                                  DLMO_ReadUInt16,   
                                  DLMO_ReadUInt32,   
                                  DLME_GetMetaData,
                                  DLMO_ReadSubnetFilter,
                                  DLMO_ReadTAI,    
                                  DLMO_ReadTAIAdjust,        
                                  DLMO_ReadCandidates,         
                                  DLMO_ReadSmoothFactors,          
                                  DLMO_ReadQueuePriority,   
                                  DLMO_ReadDeviceCapability,
                                  DLMO_ReadAdvJoinInfo,
                                  DLMO_ReadChDiagnostics,
                                  DLMO_ReadEnergyDesign,
                                  DLMO_ReadAlertPolicy
};

const DLMO_SET_MIB_FCT c_aSetMIBFct[] = {
                                  DLMO_WriteInt8,               
                                  DLMO_WriteUInt16,   
                                  DLMO_WriteUInt32,                                     
                                  DLMO_WriteProtection,       // no write allowed on metadata
                                  DLMO_WriteSubnetFilter,
                                  DLMO_WriteProtection,       // no write allowed on TAI
                                  DLMO_WriteTAIAdjust,        
                                  DLMO_WriteProtection,       // no write allowed on candidates  
                                  DLMO_WriteSmoothFactors,          
                                  DLMO_WriteQueuePriority,   
                                  DLMO_WriteProtection,       // no write allowed on capabilities  
                                  DLMO_WriteAdvJoinInfo ,
                                  DLMO_WriteProtection,       // no write allowed on diagnostics
                                  DLMO_WriteProtection,       // no write allowed on energy design
                                  DLMO_WriteAlertPolicy
};

/* const table used to identify the appropriate parser/binarizer functions for DLMO attributes     */
/* table contains info about acces rights (read/write) and indexes in the function pointer tables  */
/* indexes that have "0x80 | localIndex" reffer to indexed attributes (tables)                     */
const DLMO_ATTR_PARSER_STRUCT c_ucAttrParserInfo[DL_NO] = 
{
                   // idx in local fctptr tables        // ISA100 attribute ID
                   /*************************************************************/ 
  { 0             , 0xFF },                                // atribute ID 0 does not exist    
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_ACT_SCAN_HOST_FRACT = 1
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_ADV_JOIN_INFO },        // DL_ADV_JOIN_INFO       = 2
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_ADV_SUPERFRAME      = 3
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_SUBNET_ID           = 4  
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_SOLIC_TEMPL         = 5
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_SUBNETFILTER },         // DL_ADV_FILTER          = 6
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_SUBNETFILTER },         // DL_SOLIC_FILTER        = 7
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_TAI },                  // DL_TAI_TIME            = 8
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_TAI_ADJUST },           // DL_TAI_ADJUST          = 9
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_MAX_BACKOFF_EXP     = 10
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_MAX_DSDU_SIZE       = 11  
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },                // DL_MAX_LIFETIME        = 12
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_NACK_BACKOFF_DUR    = 13
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_LINK_PRIORITY_XMIT  = 14
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_LINK_PRIORITY_RCV   = 15
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_ENERGY_DESIGN },        // DL_ENERGY_DESIGN       = 16
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_ENERGY_LEFT         = 17
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_DEV_CAPABILITY },       // DL_DEV_CAPABILITY      = 18
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_IDLE_CHANNELS       = 19
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_CLOCK_EXPIRE        = 20
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_CLOCK_STALE         = 21
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT32 },               // DL_RADIO_SILENCE       = 22
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT32 },               // DL_RADIO_SLEEP         = 23
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_RADIO_TRANSMIT_POWER= 24
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT16 },               // DL_COUNTRY_CODE        = 25
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_CANDIDATES },           // DL_CANDIDATES          = 26
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_UINT8 },                // DL_DISCOVERY_ALERT     = 27
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_SMOOTHFACTORS },        // DL_SMOOTH_FACTORS      = 28
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_QUEUE_PRIORITY },       // DL_QUEUE_PRIOTITY      = 29
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_CH },        // DL_CH                  = 30
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_CH_META             = 31
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_TS_TEMPL },  // DL_TS_TEMPL            = 32
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_TS_TEMPL_META       = 33  
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_NEIGH },     // DL_NEIGH               = 34
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_NEIGH_DIAG_RST },// DL_NEIGH_DIAG_RESET= 35
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_NEIGH_META          = 36
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_SUPERFRAME },// DL_SUPERFRAME          = 37          
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_SFIDLE },    // DL_SF_IDLE             = 38
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_SUPERFRAME_META     = 39
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_GRAPH },     // DL_GRAPH               = 40
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_GRAPH_META          = 41
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_LINK },      // DL_LINK                = 42
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_LINK_META           = 43
  { RW_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_ROUTE },     // DL_ROUTE               = 44
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_ROUTE_META          = 45
  { RO_ATTR_ACCESS, 0x80 | SMIB_IDX_PARSER_DL_NEIGH_DIAG },// DL_NEIGH_DIAG          = 46
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_METADATA },             // DL_DIAG_META           = 47
  { RO_ATTR_ACCESS, MIB_IDX_PARSER_CH_DIAG },              // DL_CH_DIAG             = 48
  { RW_ATTR_ACCESS, MIB_IDX_PARSER_ALERT_POLICY }          // DL_ALERT_POLICY        = 49
};                    
                  
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei, Mircea Vlasin
/// @brief  Executes a data link management object's method
/// @param  p_unMethID - method identifier
/// @param  p_unReqSize - input parameters size
/// @param  p_pReqBuf - input parameters buffer
/// @param  p_pRspSize - output parameters size
/// @param  p_pRspBuf - output parameters buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_Execute(uint8   p_ucMethID,
                   uint16  p_unReqSize, 
                   uint8*  p_pReqBuf,
                   uint16* p_pRspSize,
                   uint8*  p_pRspBuf)
{
  uint8 ucSFC = SFC_SUCCESS;
  
  uint16 unAttrID = ((uint16)p_pReqBuf[0] << 8) | p_pReqBuf[1];
  p_pReqBuf += 2;
  p_unReqSize -= 2;
  
  if (!unAttrID || (unAttrID >= DL_NO))
      return SFC_INVALID_ATTRIBUTE;
  
  uint32 ulSchedTAI = 0;
  if (p_ucMethID == DLMO_WRITE_ROW)
  {
      ulSchedTAI = ((uint32)p_pReqBuf[0] << 24) |
                   ((uint32)p_pReqBuf[1] << 16) |
                   ((uint32)p_pReqBuf[2] << 8) |
                   p_pReqBuf[3];
      p_pReqBuf += 4;
      p_unReqSize -= 4;
  }
  
  switch(p_ucMethID)
  {      
      case DLMO_WRITE_ROW_NOW:
      case DLMO_WRITE_ROW:  ucSFC = DLMO_WriteRow(unAttrID, ulSchedTAI, p_unReqSize, p_pReqBuf ); break;
      case DLMO_READ_ROW:   ucSFC = DLMO_readRow(unAttrID, p_unReqSize, p_pReqBuf, p_pRspSize, p_pRspBuf); break;      
      default:              ucSFC = SFC_INVALID_METHOD;
  }
 
  return ucSFC;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Reads a data link management object attribute's value 
/// @param  p_stAttr - attribute identifier structure
/// @param  p_pSize - attribute's size
/// @param  p_pBuf - buffer containing the read data
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_Read(ATTR_IDTF* p_pAttrIdtf, uint16* p_punSize, uint8* p_pBuf)
{
  if ( !p_pAttrIdtf->m_unAttrID || (p_pAttrIdtf->m_unAttrID >= DL_NO))
      return SFC_INVALID_ATTRIBUTE;
  
  //if (p_pAttrIdtf->m_ucAttrFormat > ATTR_ONE_INDEX)
    //  return SFC_INVALID_ELEMENT_INDEX;     

  // all attributes in DLMO are read/write or read_only; do not check access rights  
  
  if (c_ucAttrParserInfo[p_pAttrIdtf->m_unAttrID].m_ucLocalParserIdx & 0x80)
  {      
      // requested attribute is an indexed attribute; perform a wraper to read_row method     
      uint8 aReqBuf[2] = { p_pAttrIdtf->m_unIndex1 >> 8, p_pAttrIdtf->m_unIndex1};        
      return DLMO_readRow(p_pAttrIdtf->m_unAttrID, 2 , aReqBuf, p_punSize, p_pBuf);
  }
  else
  {
      LOCAL_MIB_PARSER_IDX ucLocalParserIdx = (LOCAL_MIB_PARSER_IDX)(c_ucAttrParserInfo[p_pAttrIdtf->m_unAttrID].m_ucLocalParserIdx);
      
      return c_aGetMIBFct[ucLocalParserIdx](p_pAttrIdtf->m_unAttrID, p_punSize, p_pBuf); 
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Reads a single row of a data link management object's indexed attribute
/// @param  p_unAttrID - attribute identifier
/// @param  p_unReqSize - input parameters size
/// @param  p_pReqBuf - input parameters buffer
/// @param  p_pRspSize - output parameters size
/// @param  p_pRspBuf - output parameters buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_readRow(uint16   p_unAttrID, 
                   uint16   p_unReqSize, 
                   uint8*   p_pReqBuf,
                   uint16*  p_pRspSize,
                   uint8*   p_pRspBuf)
{
  uint8 ucRetSFC;
  
  // check if requested attribute is indexed (SMIB)
  if (!(c_ucAttrParserInfo[p_unAttrID].m_ucLocalParserIdx & 0x80))
      return SFC_INVALID_ATTRIBUTE;
  
  if( p_unReqSize != 2 )
      return SFC_INVALID_SIZE;
  
  //extract the uint16 index   
  uint16 unUID = (((uint16)p_pReqBuf[0]) << 8) | p_pReqBuf[1];
  
  // do not check access rights; all attributes are read_only or read_only
  DLMO_SMIB_ENTRY stEntry;
  
  ucRetSFC = DLME_GetSMIBRequest(p_unAttrID, unUID, &stEntry);
                                 
  if (SFC_SUCCESS != ucRetSFC)
      return ucRetSFC;  // invalid attributeID or invalid index
  
  // in this point, it is sure that attributeID reffers to a SMIB  
  LOCAL_SMIB_PARSER_IDX ucLocalParserIdx = (LOCAL_SMIB_PARSER_IDX)(c_ucAttrParserInfo[p_unAttrID].m_ucLocalParserIdx & 0x7F);
  
  *p_pRspSize = (uint16)c_aGetSMIBFct[ucLocalParserIdx](&stEntry, p_pRspBuf);
  
   return ucRetSFC;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei, Mircea Vlasin
/// @brief  Writes a single row of a data link management object's indexed attribute
/// @param  p_unAttrID    - attribute identifier
/// @param  p_ulSchedTAI  - scheduled write time
/// @param  p_unReqSize   - input parameters size
/// @param  p_pReqBuf     - input parameters buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_WriteRow(uint16  p_unAttrID, 
                    uint32  p_ulSchedTAI, 
                    uint16  p_unReqSize, 
                    uint8*  p_pReqBuf)
{  
  if (!p_unReqSize)
      return SFC_INVALID_SIZE; // at least one byte for index
  
  if ( c_ucAttrParserInfo[p_unAttrID].m_ucAccess == RO_ATTR_ACCESS )
      return SFC_READ_ONLY_ATTRIBUTE;
  
  if (!(c_ucAttrParserInfo[p_unAttrID].m_ucLocalParserIdx & 0x80))
      return SFC_INVALID_ATTRIBUTE;  
  
  uint8 ucRetSFC;
  DLMO_DOUBLE_IDX_SMIB_ENTRY stTwoIdxSMIB; 
  
  stTwoIdxSMIB.m_unUID = (((uint16)p_pReqBuf[0]) << 8) | p_pReqBuf[1];
  
  p_pReqBuf += 2;  
  p_unReqSize -= 2;
  
  if (p_unReqSize == 0)
  {
      // write_row method with zero len payload: delete the entry 
      return DLME_DeleteSMIBRequest(p_unAttrID, stTwoIdxSMIB.m_unUID, p_ulSchedTAI);  
  }
  
  uint16 unRspSize = p_unReqSize;  // unRspSize is in/out parameter in the following function
    
  // now start parsing the SMIB    
  LOCAL_SMIB_PARSER_IDX ucLocalParserIdx = (LOCAL_SMIB_PARSER_IDX)(c_ucAttrParserInfo[p_unAttrID].m_ucLocalParserIdx & 0x7F);
    
  ucRetSFC = c_aSetSMIBFct[ucLocalParserIdx](&unRspSize, p_pReqBuf, &stTwoIdxSMIB.m_stEntry);  
  
  if(SFC_SUCCESS != ucRetSFC)
  {
      return ucRetSFC;
  }

  return DLME_SetSMIBRequest(p_unAttrID, p_ulSchedTAI, &stTwoIdxSMIB);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman
/// @brief  Writes a data link management object attribute's value 
/// @param  p_ucAttrId - attribute identifier structure
/// @param  p_ucSize - attribute's size
/// @param  p_pBuf - buffer containing the value to be set
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DLMO_Write(ATTR_IDTF * p_pAttrIdtf, 
                 uint16    * p_pSize, 
                 const uint8 * p_pBuf)
{  
  uint8 ucRetSFC;
  
  if (!p_pAttrIdtf->m_unAttrID || (p_pAttrIdtf->m_unAttrID >= DL_NO))
      return SFC_INVALID_ATTRIBUTE;
  
  if (c_ucAttrParserInfo[p_pAttrIdtf->m_unAttrID].m_ucAccess == RO_ATTR_ACCESS)
      return SFC_READ_ONLY_ATTRIBUTE;
  
  if (c_ucAttrParserInfo[p_pAttrIdtf->m_unAttrID].m_ucLocalParserIdx & 0x80)
  {   
      // write on indexed attributes (SMIBS)    
      LOCAL_SMIB_PARSER_IDX ucLocalParserIdx = (LOCAL_SMIB_PARSER_IDX)(c_ucAttrParserInfo[p_pAttrIdtf->m_unAttrID].m_ucLocalParserIdx & 0x7F);     
      
      DLMO_DOUBLE_IDX_SMIB_ENTRY stTwoIdxSMIB;      
      stTwoIdxSMIB.m_unUID = p_pAttrIdtf->m_unIndex1;
      
      ucRetSFC = c_aSetSMIBFct[ucLocalParserIdx]( p_pSize, p_pBuf, &stTwoIdxSMIB.m_stEntry);  
  
      if(SFC_SUCCESS != ucRetSFC)
          return ucRetSFC;
      
      // by calling DLME_SetSMIBRequest, only in the foloowing TAI sec write will be completed
      return DLME_SetSMIBRequest(p_pAttrIdtf->m_unAttrID, 0, &stTwoIdxSMIB);      
  }
  else
  {
      // write on non indexed attributes
      LOCAL_MIB_PARSER_IDX ucLocalParserIdx = (LOCAL_MIB_PARSER_IDX)(c_ucAttrParserInfo[p_pAttrIdtf->m_unAttrID].m_ucLocalParserIdx);     
      return c_aSetMIBFct[ucLocalParserIdx](p_pAttrIdtf->m_unAttrID, p_pSize, p_pBuf);
  } 
}

