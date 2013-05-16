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
/// Description:  This file holds definitions of the uart link layer
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _NIVIS_UART_LINK_H_
#define _NIVIS_UART_LINK_H_

    #include "slme.h"

    #define UART_MSG_TYPE_BUFFER_READY 0x00
    #define UART_MSG_TYPE_BUFFER_FULL  0x01
    #define UART_MSG_TYPE_BUFFER_CHECK 0x02
    #define UART_MSG_TYPE_BUFFER_WRONG 0x03
    #define UART_MSG_TYPE_ACK          0x10
//  #define UART_MSG_TYPE_NWK          0x80 
//  #define UART_MSG_TYPE_APP          0x81
    #define UART_MSG_TYPE_NWK_CONF     0x82
    #define UART_MSG_TYPE_NWK_T2B      0x83
    #define UART_MSG_TYPE_NWK_B2T      0x84
    #define UART_MSG_TYPE_LOCAL_MNG    0x85

#ifdef BACKBONE_SUPPORT
    #define UART_MSG_TYPE_DEBUG        0xFF

    typedef struct 
    {
        uint16 m_unCrc;
        uint16 m_unBuffLen;   
        uint8  m_ucPkLen;
        uint8  m_aBuff[255]; // aligned to 8 at offset 7 inside buffer    
    } UART_RX_QUEUE;      

    extern UART_RX_QUEUE g_stUartRx;
  
    void UART_LINK_Task ( void );

    __monitor uint8 UART_LINK_AddMessage ( uint8 p_unMessageType, 
                                           uint8 p_ucTrafficClass,
                                           uint8 p_ucSrcDstAddrLen, const uint8 *p_pSrcDst, 
                                           uint8 p_ucPayloadLen, const void * p_pPayload );
    
    __monitor void UART_LINK_AddNwkConf ( const void * p_pPayload );
    
    void UART_LINK_LocalKeyManagement ( uint8 p_ucAddKey, const SLME_KEY * p_pKey );
    

    uint8 UART_LINK_DispatchMessage ( uint8 p_ucPayloadLen, uint8 * p_pPayload );    
#else // BACKBONE_SUPPORT
    
  #define UART_LINK_Task()
  #define UART_LINK_AddMessage(...) 0
  #define UART_LINK_DispatchMessage(...) 0
  #define UART_LINK_LocalKeyManagement(...)

#endif // BACKBONE_SUPPORT   
#endif // _NIVIS_UART_LINK_H_

