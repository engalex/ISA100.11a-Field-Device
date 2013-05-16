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
/// Author:       Nivis LLC, Marius Vilvoi
/// Date:         November 2007
/// Description:  Implements Physical Layer of ISA100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "phy.h"
#include "tmr_util.h"
#include "dmap.h"
#include "mlsm.h"
#include "../../LibInterface/ITC_Interface.h"

#define ONE_BYTE_LEN        (8)                                                 //  1 byte = 32 uSec = 2 symbols => 2 Symbols / 0.25 Symbols clock step = 8
#define WARM_UP_DELAY       (50)                                                // 50 * 4 = 200 uS

volatile uint8 g_ucXVRStatus = PHY_DISABLED;                                    // state machine
volatile uint8 g_aucPhyRx[PHY_BUFFERS_LENGTH + 1 + 2];                          // plus 1 FRAME LEN + CRC

uint8 g_ucLastRFChannel;

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Marius Vilvoi
/// @brief  Set MACA_CONTROL register
/// @param  p_ulMacaCtrlValue - new value to be set
/// @return none
/// @remarks 
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
__arm void PHY_setMacaCtrl( uint32 p_ulMacaCtrlValue )
{
    volatile uint8 i = 24;
    MACA_CONTROL = p_ulMacaCtrlValue;
    
    while( --i )
      ;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Marius Vilvoi
/// @brief  This function will be used for disable transceiver at all (switch to PHY_DISABLE)
/// @param    
/// @return none
/// @remarks  
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
void PHY_DisableTR ( void )
{
  if( g_ucXVRStatus != PHY_DISABLED )
  {          
  
      TX_PA_OFF();
      
      MACA_MASKIRQ = 0;
      
      MACA_TMRDIS = (CLK_START | CLK_CPL | CLK_SOFT | CLK_START_ABORT | CLK_CPL_ABORT | CLK_SOFT_ABORT); // disable all timers

// ----------- Freescale patch --------------
      {
          uint32_t clk, TsmRxSteps, LastWarmupStep, LastWarmupData, LastWarmdownStep, LastWarmdownData;
          volatile uint32_t i;
          
//          ITC_DisableInterrupt(gMacaInt_c);  
        // Manual TSM modem shutdown 
        
          // read TSM_RX_STEPS 
          TsmRxSteps = (*((volatile uint32_t *)(0x80009204)));
         
          // isolate the RX_WU_STEPS 
          // shift left to align with 32-bit addressing 
          LastWarmupStep = (TsmRxSteps & 0x1f) << 2;
          // Read "current" TSM step and save this value for later 
          LastWarmupData = (*((volatile uint32_t *)(0x80009300 + LastWarmupStep)));
        
          // isolate the RX_WD_STEPS 
          // right-shift bits down to bit 0 position 
          // left-shift to align with 32-bit addressing 
          LastWarmdownStep = ((TsmRxSteps & 0x1f00) >> 8) << 2;
          // write "last warmdown data" to current TSM step to shutdown rx 
          LastWarmdownData = (*((volatile uint32_t *)(0x80009300 + LastWarmdownStep)));
          (*((volatile uint32_t *)(0x80009300 + LastWarmupStep))) = LastWarmdownData;
        
          // Abort 
//          MACA_CONTROL = CTRL_SEQ_ABORT; // Freescale original implementation
          MACA_CONTROL = CTRL_SEQ_ABORT | CTRL_PRM | CTRL_ASAP;
          
          // Wait ~8us 
          for (clk = MACA_CLK, i = 0; MACA_CLK - clk < 3 && i < 300; i++)
            ;
         
          // NOP 
//          MACA_CONTROL = CTRL_SEQ_NOP; // Freescale original implementation
          MACA_CONTROL = CTRL_SEQ_NOP | CTRL_PRM;
          
          
          // Wait ~8us 
          for (clk = MACA_CLK, i = 0; MACA_CLK - clk < 3 && i < 300; i++)
            ;
                   
          // restore original "last warmup step" data to TSM (VERY IMPORTANT!!!) 
          (*((volatile uint32_t *)(0x80009300 + LastWarmupStep))) = LastWarmupData;
        
          // Clear all MACA interrupts - we should have gotten the ABORT IRQ 
          MACA_CLRIRQ =  0xFFFF;
          
//          ITC_EnableInterrupt(gMacaInt_c);
      }
      
// ----------- Freescale patch end --------------
      
      g_ucXVRStatus = PHY_DISABLED;
      
      DEBUG3_OFF();
      DEBUG4_OFF();
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Marius Vilvoi
/// @brief  this function will be used for enable transceiver in receive mode on desired frequency channel (p_ucChannel)
/// @param  p_ucChannel      - Channel
///         p_ucRelativeFlag - if absolute delay or not
///         p_unDelay        - TMR2 steps
///         p_unAbortTimeout - Timeout for RX in 4us steps. (from start RX to end RX). 0 = NO TIMEOUT (not TMR2 steps!)
/// @return none
/// @remarks
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
void PHY_RX_Request ( uint8 p_ucChannel, uint8 p_ucRelativeFlag, uint16 p_unDelay, uint16 p_unAbortTimeout )
{  
    uint32 ulMacaCtrl;
    uint32 ulMacaIrq =  MASK_IRQ_CRC | MASK_IRQ_DI;
        
#ifdef DEBUG_RF_LINK    
   ulMacaIrq |= MASK_IRQ_STRT;
#endif    
#ifdef TRIGGER_RX_LEVEL    
   ulMacaIrq |= MASK_IRQ_LVL; 
#endif    
    
    MACA_TMRDIS = (CLK_START | CLK_CPL | CLK_SOFT | CLK_START_ABORT | CLK_CPL_ABORT | CLK_SOFT_ABORT); // disable all timers
    MACA_CLK = 0;
    
    ulMacaCtrl = (CTRL_PRM | CTRL_SEQ_RX);
    
    PHY_Disable_Request();
    
    RX_LNA_ON();
    
    g_ucLastRFChannel = p_ucChannel;
    SET_CHANNEL(p_ucChannel);
    
    MACA_TXLEN = 0x007F0000;    // RXLen
    MACA_DMATX = (uint32) g_aucPhyRx;
    MACA_DMARX = (uint32) g_aucPhyRx;
    g_ucXVRStatus = PHY_RX_START_DO;

    if( p_unDelay )
    {
        if( p_ucRelativeFlag == ABSOLUTE_DELAY )
        {
            p_unDelay -= TMR_GetSlotOffset();
            if( p_unDelay & 0x8000 )
            {
#ifdef DEBUG_RESET_REASON
            if( 0 == g_stSlot.m_uc250msSlotNo )
                g_aucDebugResetReason[8]++;
            else
                g_aucDebugResetReason[9]++;
#endif
                p_unDelay = 0;
            }
        }
        // TMR2_TO_BITS = TMR2_TO_USEC(x) / 4 = x / 3 
        p_unDelay /= 3;
    }
   
    MACA_CLKOFFSET = 0;
    if( p_unDelay > (MACA_CLK + WARM_UP_DELAY) ) // minimum delay
    {
        MACA_STARTCLK = p_unDelay;
        if ( p_unAbortTimeout )
        {
          MACA_SFTCLK = p_unDelay + p_unAbortTimeout;
          MACA_TMREN = CLK_START | CLK_SOFT;   // enable Soft Timeout for RX Clock for timer triggered action          
          ulMacaIrq |= MASK_IRQ_ACPL;
        }
        else
        {
            MACA_TMREN = CLK_START;    // enable Start Clock for timer triggered action
        }
    }
    else // go to on RX now
    {
        if ( p_unAbortTimeout > MACA_CLK + WARM_UP_DELAY )
        {
          MACA_SFTCLK = p_unAbortTimeout;
          MACA_TMREN = CLK_SOFT;    // enable Soft Timeout for RX Clock for timer triggered action    
          ulMacaIrq |= MASK_IRQ_ACPL;
        }
        else
        {
          MACA_TMREN = 0x00;          // disable Start Clock and Soft Timeout Clock for timer triggered action
        }
        ulMacaCtrl |= CTRL_ASAP;
    }
    
    MACA_MASKIRQ = ulMacaIrq; 
    MACA_CONTROL = ulMacaCtrl;

    while(p_ucChannel--)
    {
        DEBUG3_ON();
        DEBUG3_OFF();
    }
    
    DEBUG3_ON();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Marius Vilvoi
/// @brief  this function will be used for enable transceiver in transmit mode on desired frequency channel (p_ucChannel)
/// @param  p_ucChannel  - Channel
///         p_unBitDelay -  Delay on 4us steps
///         p_ucCCAFlag  - 0 => without CCA; else with CCA
///         p_pucData    - pointer to Data buffer
///         p_ucLen      - max 125 bytes
/// @return none
/// @remarks
///      Access level: Interrupt level
///      Context: 
///             Note 1:   For any TX sequence the RF transimted bytes (symbols) are :
///                            Preamble                        = 4 Bytes = 8 symbols = 128 uS
///                            Start of Frame Delimiter (SFD)  = 1 bytes = 2 symbols = 32 uS
///                            PHR (frame len)                 = 1 bytes = 2 symbols = 32 uS
///                            PHY message ... ()              = max 125 bytes
///                            FCS (CRC)                       = 2 bytes = 4 symbols = 64 uS
///             Note 2:   MACA_TXLEN register specifies how many bytes of data is to be transmitted
///                       during a Tx auto-sequence (including checksum, but excluding PHY header).
////////////////////////////////////////////////////////////////////////////////////////////////////
void PHY_TX_Request ( uint8 p_ucChannel, uint8 p_ucRelativeFlag,  uint16 p_unDelay, uint8 p_ucCCAFlag, uint8 * p_pucData, uint8 p_ucLen )
{
  uint32 ulMacaCtrl;
    
  MACA_TMRDIS = (CLK_START | CLK_CPL | CLK_SOFT | CLK_START_ABORT | CLK_CPL_ABORT | CLK_SOFT_ABORT); // disable all timers
  MACA_CLK = 0;
  
  ulMacaCtrl = ( p_ucCCAFlag ?  (CTRL_PRM | CTRL_MODE_NO_SLOTTED | CTRL_SEQ_TX) : (CTRL_PRM | CTRL_MODE_NO_CCA | CTRL_SEQ_TX) );
  
  PHY_Disable_Request();
  
  TX_PA_ON();
  
  g_ucLastRFChannel = p_ucChannel;
  SET_CHANNEL(p_ucChannel);
    
  MACA_TXLEN = (uint32) (p_ucLen + 2);                                        // p_ucLen Data buffer plus + 2 crc => see Obs. Note.
  MACA_DMATX = (uint32) p_pucData;
  MACA_DMARX = (uint32) g_aucPhyRx;                                           // here need for temp ack buffer !
  g_ucXVRStatus = PHY_TX_START_DO;

  if( p_unDelay )
  {
      if( p_ucRelativeFlag == ABSOLUTE_DELAY )
      {
          p_unDelay -= TMR_GetSlotOffset();
          if( p_unDelay & 0x8000 )
          {
              p_unDelay = 0;
          }
      }
      
      p_unDelay = TMR2_TO_PHY_BITS(p_unDelay);
  }
  

  if( p_unDelay >= (MACA_CLK + WARM_UP_DELAY) ) // minimum delay
  {
      MACA_CLKOFFSET = 0;
      MACA_STARTCLK = p_unDelay;
      MACA_TMREN = CLK_START;   // enable Start Clock for timer triggered action        
  }
  else // go to on RX now
  {
      MACA_TMREN = 0x00; // disable Start Clock for timer triggered action
      ulMacaCtrl |= CTRL_ASAP;
  }
  
#ifdef DEBUG_RF_LINK
  MACA_MASKIRQ = (MASK_IRQ_STRT | MASK_IRQ_ACPL);
#else
  MACA_MASKIRQ = (MASK_IRQ_ACPL);
#endif
  
  MACA_CONTROL = ulMacaCtrl;

  DEBUG4_ON();  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Marius Vilvoi
/// @brief  time elapsed from start of first RX data (after length byte).
/// @param  
/// @return tmr2 tics from start of first RX data (after length byte).
/// @remarks
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
__arm uint32 PHY_GetLastRXTmr2Age(void)
{
// values are volatile but we are OK with any order
  uint32 ulResult = MACA_CLK;
  ulResult -= MACA_TIMESTAMP; // = MACA_CLK - MACA_TIMESTAMP
  
  return (PHY_BITS_TO_TMR2(ulResult) + USEC_TO_TMR2(50)) ; // 50 us because modem internal delay
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Marius Vilvoi
/// @brief  uSec from end of SFD tx.
/// @param  
/// @return uSec from end of SFD tx.
/// @remarks
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
__arm uint32 PHY_GetLastTXuSecAge(void)
{
// values are volatile but we are OK with any order
#pragma diag_suppress=Pa082
  // TX time for data + crc
  return (uint32)(( MACA_CLK - MACA_CPLTIM + (uint16)(MACA_TXLEN << 3) + 4-1) * 4); // 4 is MACA_EOFDELAY, -1 because empirical
#pragma diag_default=Pa082
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC
/// @brief  to check if tranceiver is on RX on that channel
/// @param  p_ucChannel
/// @return 0 or 1
/// @remarks
///      Access level: Interrupt level
///      Context: 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 PHY_IsRXOnChannel(uint8 p_ucChannel )
{
    return (g_ucLastRFChannel == p_ucChannel && g_ucXVRStatus == PHY_RX_START_DO);
}

