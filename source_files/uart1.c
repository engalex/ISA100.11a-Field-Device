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

/***************************************************************************************************
* Name:         uart1.c
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#include "uart1.h"
#include "typedef.h"
#include "global.h"
#include "digitals.h"
#include "itc.h"
#include "timers.h"
#include "asm.h"
#include <string.h>
#include "ISA100/uart_link.h"
#include "../LibInterface/ITC_Interface.h"

#if defined ( BACKBONE_SUPPORT )

  #define STX 0xF0
  #define ETX 0xF1
  #define CHX 0xF2

  unsigned char    g_auc_Uart1_Tx[UART1_PACKET_LENGTH+2+2];
  unsigned char    g_uc_Uart1_TxIdx = 0;
  unsigned char    g_uc_Uart1_TxLen = 0;

  unsigned int     g_unLastSTXTime;
  unsigned char    g_ucBufferReadyReq = 0;
  
  void UART1_Interrupt(void);

  void UART1_Init(void)
  {
//    itcAssignHandler(UART1_Interrupt,UART1_INT,NORMAL_INT);
//    ITC_ENABLE_INTERRUPT(UART1_INT);
  IntAssignHandler(gUart1Int_c, (IntHandlerFunc_t)UART1_Interrupt);
  ITC_SetPriority(gUart1Int_c, gItcNormalPriority_c);
  ITC_EnableInterrupt(gUart1Int_c);

    UART1_UCON    = 0x0000;                                                       // disable UART 1 for setting baud rate
    GPIO_FUNC_SEL0_P14 = 1;                                                       // set pin P20 (gpio 14) for function select tx
    GPIO_FUNC_SEL0_P15 = 1;                                                       // set pin P19 (gpio 15) for function select rx
    UART1_UBRCNT  = ( (UART_BAUD_115200 ) << 16 ) + 9999;                           // set desired baud rate
    UART1_URXCON  = 1;                                                            // set buffer level
    UART1_UTXCON  = 31;                                                           // 31 = ok // set buffer level
    UART1_TX_INT_DI();
    UART1_RXTX_EN();
    g_ucBufferReadyReq = 1+UART_MSG_TYPE_BUFFER_READY;
  }
  
  void UART1_Interrupt(void)
  {
    volatile unsigned long ulDEBStatus = UART1_USTAT;  //Dummy read to clear flags
    static  uint8 s_ucRxLastChar;

    // RX
    while ( UART1_URXCON )
    {
        unsigned char ucRxChar = UART1_UDATA;
        if( ucRxChar == STX )
        {
            g_stUartRx.m_ucPkLen = 0;
            g_stUartRx.m_unCrc = 0xFFFF;
            g_unLastSTXTime = (uint16)RTC_COUNT;
           s_ucRxLastChar = ucRxChar;
        }
        else if(s_ucRxLastChar != ETX ) // accept chars if STX only first
        {            
            unsigned int unRxIdx = g_stUartRx.m_unBuffLen; 
            if( ucRxChar == ETX )
            {
                uint8 * pStartRxPk = g_stUartRx.m_aBuff + 1 + unRxIdx;
                
                if( (g_stUartRx.m_ucPkLen >5) && (0 == g_stUartRx.m_unCrc) )
                {
                     g_stUartRx.m_ucPkLen -= 2; // remove the CRC
                     if( UART_LINK_DispatchMessage( g_stUartRx.m_ucPkLen, pStartRxPk ) )
                     {
                        // store the paket on queue
                        pStartRxPk[-1] = g_stUartRx.m_ucPkLen;
                        g_stUartRx.m_unBuffLen = unRxIdx + g_stUartRx.m_ucPkLen+1;
                        g_stUartRx.m_aBuff[g_stUartRx.m_unBuffLen] = 0;
                     }
                     g_ucBufferReadyReq = 1 + UART_MSG_TYPE_BUFFER_READY;
                }
                else if( pStartRxPk[0] == UART_MSG_TYPE_BUFFER_CHECK )
                {
                     g_ucBufferReadyReq = 1 + UART_MSG_TYPE_BUFFER_READY;
                }
                else
                {
                     g_ucBufferReadyReq = 1 + UART_MSG_TYPE_BUFFER_WRONG;
                }
                g_stUartRx.m_ucPkLen = 0;
            }
            else if( ucRxChar != CHX )
            {
                unRxIdx += g_stUartRx.m_ucPkLen;
            
                if( (sizeof( g_stUartRx.m_aBuff ) > 255) && (g_stUartRx.m_ucPkLen == 255 ) )
                {
                    ucRxChar = ETX; // discard the message
                }
                else if( unRxIdx < sizeof( g_stUartRx.m_aBuff ) - 2 )
                {
                    unsigned char ucBufCh = ( s_ucRxLastChar != CHX ? ucRxChar : ~ucRxChar );
                    g_stUartRx.m_aBuff[1+unRxIdx] = ucBufCh;
                    g_stUartRx.m_unCrc = COMPUTE_CRC( g_stUartRx.m_unCrc, ucBufCh );
                    g_stUartRx.m_ucPkLen++;
                }
                else 
                {
                    ucRxChar = ETX; // discard the message
                }
            }
            s_ucRxLastChar = ucRxChar;
        }
    }

    // TX
    while( UART1_UTXCON )
    {
        if ( g_uc_Uart1_TxIdx < g_uc_Uart1_TxLen )
        {
            unsigned char ucTxChar = g_auc_Uart1_Tx[g_uc_Uart1_TxIdx];
            if( g_uc_Uart1_TxIdx && ucTxChar >= STX && ucTxChar <= CHX  )
            {
                g_auc_Uart1_Tx[g_uc_Uart1_TxIdx] = ~ucTxChar;
                ucTxChar = CHX;
            }
            else
            {
                g_uc_Uart1_TxIdx++;
            }

            UART1_UDATA = ucTxChar;
        }
        else if ( g_uc_Uart1_TxLen && g_uc_Uart1_TxIdx == g_uc_Uart1_TxLen )
        {
             g_uc_Uart1_TxIdx ++;
             UART1_UDATA = ETX;
        }
        else 
        {
              g_uc_Uart1_TxIdx = 0;
              if( g_ucBufferReadyReq ) // add an buffer ready message
              {
                  g_auc_Uart1_Tx[0] = g_ucBufferReadyReq-1;
                  g_ucBufferReadyReq = 0;

                  g_uc_Uart1_TxLen = 1;
                  UART1_UDATA = STX;
              }
              else
              {
                  UART1_TX_INT_DI();
                  g_uc_Uart1_TxLen = 0;
                  break;
              }
        }
    }

  }  
  

  unsigned char UART1_SendMsg(const unsigned char * p_pucBuff, unsigned char p_ucLen)
  {
    unsigned char ucStatus = 0;
    uint16 unCRC;
    
    if( p_ucLen )
    {
        unCRC = ComputeCCITTCRC( p_pucBuff, p_ucLen );
    }

    UART1_RXTX_INT_DI();

    if ( !g_uc_Uart1_TxLen ) // not message on progress
    {
        if( g_ucBufferReadyReq ) // a buffer ready request
        {
            // simulate an end of one byte TX buffer
            g_uc_Uart1_TxLen = 1;
            g_uc_Uart1_TxIdx = 2;
        }
        else if( p_ucLen )
        {
            
            ucStatus = 1;
            
            g_auc_Uart1_Tx[0] = STX;
            memcpy( g_auc_Uart1_Tx+1, p_pucBuff, p_ucLen );
            g_auc_Uart1_Tx[ ++p_ucLen ] = (unCRC >> 8); // CRC -> HI byte
            g_auc_Uart1_Tx[ ++p_ucLen ] = unCRC & 0xFF; // CRC -> LO byte

            g_uc_Uart1_TxLen = p_ucLen+1; // STX 
        }
    }

    UART1_RXTX_INT_EN();
    return ucStatus;
  }
  
#endif // BACKBONE_SUPPORT

