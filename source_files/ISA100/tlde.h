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
/// Description:  This file holds definitions of transport layer data entity from ISA100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_TLDE_H_
#define _NIVIS_TLDE_H_

#include "tlme.h"
#include "nlde.h"

#define ISA100_START_PORTS 0xF0B0
#define ISA100_DMAP_PORT  (ISA100_START_PORTS)

#define UDP_ENCODING_PLAIN     0xF3 //b11110011 // check sum present, 4 bit ports
#define UDP_ENCODING_ENCRYPTED 0xF7 //b11110111 // check sum elided, 4 bit ports

#define UDP_PROTOCOL_CODE      17

extern const uint8 c_aLocalLinkIpv6Prefix[8];


#define TLDE_DISCARD_ELIGIBILITY_MASK BIT4
#define TLDE_ECN_MASK                 (BIT5 | BIT6) 
#define TLDE_PRIORITY_MASK            (BIT0 | BIT1)

void TLDE_DATA_Request(   const uint8* p_pEUI64DestAddr,
                          uint16       p_unContractID,
                          uint8        p_ucPriorityAndFlags,
                          uint16       p_unAppDataLen,
                          void *       p_pAppData,
                          HANDLE       p_appHandle,
                          uint8        p_uc4BitSrcPort,
                          uint8        p_uc4BitDstPort );

// -------- Callback functions from Network level to Transport Level -----------------------------


void NLDE_DATA_Confirm (  HANDLE       p_hTLHandle,
                          uint8        p_ucStatus );

void NLDE_DATA_Indication (   const uint8 * p_pSrcAddr,
                              uint16        p_unTLDataLen,
                              void *        p_pTLData,
                              uint8         p_ucECN );




// -------- Callback functions from Transport Level to Application Layer -----------------------------

#endif // _NIVIS_TLDE_H_ */

