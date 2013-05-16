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
/// Description:  This file holds definitions of the data link management object in DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DMAP_DLMO_H_
#define _NIVIS_DMAP_DLMO_H_

#include "../typedef.h"
#include "dlme.h"
#include "aslde.h"

enum
{  
  DLMO_READ_ROW       = 2,
  DLMO_WRITE_ROW      = 3,
  DLMO_WRITE_ROW_NOW  = 4
}; //DLMO_METHODS;

typedef enum {
    SMIB_IDX_PARSER_DL_CH             = 0,
    SMIB_IDX_PARSER_DL_TS_TEMPL       = 1,
    SMIB_IDX_PARSER_DL_NEIGH          = 2,
    SMIB_IDX_PARSER_DL_NEIGH_DIAG_RST = 3,
    SMIB_IDX_PARSER_DL_SUPERFRAME     = 4,
    SMIB_IDX_PARSER_DL_SFIDLE         = 5,
    SMIB_IDX_PARSER_DL_GRAPH          = 6,
    SMIB_IDX_PARSER_DL_LINK           = 7,
    SMIB_IDX_PARSER_DL_ROUTE          = 8,
    SMIB_IDX_PARSER_DL_NEIGH_DIAG     = 9,
    
    SMIB_IDX_PARSER_NO
}LOCAL_SMIB_PARSER_IDX; // indexes to function pointer tables

typedef enum {
    MIB_IDX_PARSER_UINT8          = 0,
    MIB_IDX_PARSER_UINT16         = 1,
    MIB_IDX_PARSER_UINT32         = 2,
    MIB_IDX_PARSER_METADATA       = 3,
    MIB_IDX_PARSER_SUBNETFILTER   = 4,
    MIB_IDX_PARSER_TAI            = 5,
    MIB_IDX_PARSER_TAI_ADJUST     = 6,
    MIB_IDX_PARSER_CANDIDATES     = 7,
    MIB_IDX_PARSER_SMOOTHFACTORS  = 8 ,
    MIB_IDX_PARSER_QUEUE_PRIORITY = 9,
    MIB_IDX_PARSER_DEV_CAPABILITY = 10,
    MIB_IDX_PARSER_ADV_JOIN_INFO  = 11,
    MIB_IDX_PARSER_CH_DIAG        = 12,
    MIB_IDX_PARSER_ENERGY_DESIGN  = 13, 
    MIB_IDX_PARSER_ALERT_POLICY   = 14,
    
    MIB_IDX_PARSER_NO 
}LOCAL_MIB_PARSER_IDX; // indexes to function pointer tables
 

uint8 DLMO_Execute(uint8   p_ucMethID,
                   uint16  p_unReqSize, 
                   uint8*  p_pReqBuf,
                   uint16* p_pRspSize,
                   uint8*  p_pRspBuf);

uint8 DLMO_Read(ATTR_IDTF* p_pAttrIdtf, 
                uint16*    p_punSize, 
                uint8*     p_pBuf);

uint8 DLMO_Write(ATTR_IDTF*  p_pAttrIdtf, 
                 uint16 *    p_pSize, 
                 const uint8* p_pBuf);

uint8 DLMO_WriteRow(uint16  p_unAttrID, 
                    uint32  p_ulSchedTAI, 
                    uint16  p_unReqSize, 
                    uint8*  p_pReqBuf);

#endif //_NIVIS_DMAP_DLMO_H_
