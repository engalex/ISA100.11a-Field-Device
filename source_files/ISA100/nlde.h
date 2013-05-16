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
/// Description:  This file holds definitions of the Network Layer Data Entity
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_NLDE_H_
#define _NIVIS_NLDE_H_

#include "../typedef.h"
#include "config.h"

#define HC_UDP_FULL_COMPRESS        0xF0 // b11110000 - full compress 
#define HC_UDP_PARTIAL_COMPRESS     0xE0 // b11100000 - checksum is present elided

#define NLDE_PRIORITY_MASK          0x0F

#define HANDLE_NL_BIG_MSG 0xF000
#define HANDLE_NL_BBR     0xF001

typedef struct
{
    uint8   m_ucVersionAndTrafficClass;
    uint8   m_aFlowLabel[3];
    uint8   m_aPayloadSize[2];
    uint8   m_ucNextHeader;
    uint8   m_ucHopLimit;
    uint8   m_aIpv6SrcAddr[16];
    uint8   m_aIpv6DstAddr[16];
    uint8   m_aSrcPort[2];
    uint8   m_aDstPort[2];
    uint8   m_aUdpLength[2];
    uint8   m_aUdpCheckSum[2];    
} NWK_IPV6_UDP_HDR;


void NLDE_Task(void);

//  void NLDE_DATA_Request(   const uint8* p_pEUI64DestAddr,
//                            uint16       p_unContractID,
//                            uint8        p_ucPriority,
//                            uint16       p_unNsduDataLen,
//                            const void * p_pNsduData, 
//                            HANDLE       p_nsduHandle );

#define NLDE_DATA_Request(p_pDestAddr,p_unContractID,p_ucPriority,p_unNsduDataLen,p_pNsduData,p_nsduHandle ) \
          NLDE_DATA_RequestMemOptimzed(p_pDestAddr,p_unContractID,p_ucPriority,0,(void*)0,p_unNsduDataLen,p_pNsduData,p_nsduHandle)


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
                            );

void DLDE_Data_Confirm (HANDLE p_hHandle, uint8 p_ucLocalStatus);    

void DLDE_Data_Indicate (uint8         p_ucSrcDstAddrLen,
                         const uint8 * p_pSrcDstAddr,
                         uint8         p_ucPriority, 
                         uint8         p_ucPayloadLength,
			 const void *  p_pPayload);

void NLDE_Init(void);

uint8 NLDE_DATA_HasSpaceToAPP(void);

 // -------- Callback functions from DLL to Network level ----------------------------------------

#endif // _NIVIS_NLDE_H_ 

