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
/// Date:         February 2008
/// Description:  This file holds definitions of the dlmo_utils module
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DLMO_UTILS_H_
#define _NIVIS_DLMO_UTILS_H_

#include "../typedef.h"
#include "dlme.h"

//------------------------------------------------------------------------------
uint8 DLMO_ReadUInt8(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);
uint8 DLMO_ReadInt16(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);
uint8 DLMO_ReadUInt16(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);
uint8 DLMO_ReadUInt32(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

//------------------------------------------------------------------------------
uint8 DLMO_WriteInt8(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_WriteUInt8(uint16 p_unAttrID, uint16 * p_punSize, const uint8* p_pBuf);
uint8 DLMO_WriteUInt16(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_WriteUInt32(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);

//------------------------------------------------------------------------------
uint8 DLMO_WriteSubnetFilter(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_ReadSubnetFilter(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_WriteAdvJoinInfo(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_ReadAdvJoinInfo(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_ReadDeviceCapability(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_ReadTAI(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_WriteTAIAdjust(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_ReadTAIAdjust(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_ReadCandidates(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_WriteSmoothFactors(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_ReadSmoothFactors(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_WriteQueuePriority(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
uint8 DLMO_ReadQueuePriority(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_ReadAlertPolicy(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);
uint8 DLMO_WriteAlertPolicy(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);

uint8 DLMO_ReadEnergyDesign(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_ReadChDiagnostics(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);

uint8 DLMO_WriteProtection(uint16 p_unAttrID, uint16* p_punSize, const uint8* p_pBuf);
//------------------------------------------------------------------------------
uint8 DLMO_SetSMIBChannelEntry(uint16* p_pSize,  const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBTimeslotEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBNeighborEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBNeighDiagResetEntry( uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry );
uint8 DLMO_SetSMIBSuperframeEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBSfIdleEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBGraphEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBLinkEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBRouteEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);
uint8 DLMO_SetSMIBNeighDiagEntry(uint16* p_pSize, const uint8* p_pBuf, DLMO_SMIB_ENTRY* p_pEntry);

//------------------------------------------------------------------------------
uint8 DLMO_GetSMIBChannelEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBTimeslotEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBNeighborEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBNeighDiagResetEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBSuperframeEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBSfIdleEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBGraphEntry(const DLMO_SMIB_ENTRY* p_pEntry,  uint8* p_pRspBuf);
uint8 DLMO_GetSMIBLinkEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBRouteEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);
uint8 DLMO_GetSMIBNeighDiagEntry(const DLMO_SMIB_ENTRY* p_pEntry, uint8* p_pRspBuf);

//------------------------------------------------------------------------------
__monitor const uint8 * DLMO_ExtractAdvJoinInfo( const uint8 * p_pucBuf, DLL_MIB_ADV_JOIN_INFO * p_pstAdvJoinInfo);
__monitor uint8 * DLMO_InsertAdvJoinInfo( uint8 * p_pucBuf, DLL_MIB_ADV_JOIN_INFO * p_pstAdvJoinInfo);

uint8 * DLMO_InsertExtDLUint( uint8 * p_pData, uint16  p_unValue);
const uint8 * DLMO_ExtractExtDLUint( const uint8 * p_pData, uint16 * p_pValue);

uint8 DLMO_BinarizeChDiagnostics(DLL_MIB_CHANNEL_DIAG * p_pstChDiag, uint16  * p_punSize, uint8* p_pBuf);

#endif //_NIVIS_DLMO_UTILS_H_

