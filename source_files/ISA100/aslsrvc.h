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
/// Author:       Nivis LLC, Eduard Erdei
/// Date:         January 2008
/// Description:  This file holds definitions of the application sub-layer data entity
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_ASLSRVC_H_
#define _NIVIS_ASLSRVC_H_

#include "aslde.h"
#include "../typedef.h"

extern const uint8 * ASLSRVC_GetGenericObject(  const uint8 *        p_pInBuffer, 
                                                signed short         p_nInBufLen, 
                                                GENERIC_ASL_SRVC *   p_pGenericObj,
                                                const uint8 *       p_pSrcAddr128 );

uint8 ASLSRVC_AddGenericObject( void *        p_pObj, 
                                uint8         p_ucServiceType,
                                uint8         p_ucPriorityAndFlags,
                                uint8         p_ucSrcTSAPID,
                                uint8         p_ucDstTSAPID,
                                uint16        p_unContractID,
                                uint16        p_unBinSize);

uint8 ASLSRVC_AddGenericObjectToSM( void * p_pObj, uint8 p_ucServiceType );
uint8 ASLSRVC_AddJoinObject( void * p_pObj, uint8 p_ucServiceType, uint8 * p_pEUI64Adddr );

uint8 * ASLSRVC_SetGenericObj(uint8 p_ucServiceType, void * p_pObj, uint16 p_unOutBufLen, uint8 * p_pBuf);

extern uint8   g_ucReqID;

#endif // _NIVIS_ASLSRVC_H_
