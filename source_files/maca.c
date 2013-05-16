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
* Name:         maca.c
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#include "ISA100/mlde.h"
#include "ISA100/mlsm.h"
#include "ISA100/dmap.h"
#include "maca.h"
#include "crm.h"
#include "../LibInterface/Platform.h"


const uint8 gaRFSynVCODivI_c[16] ={
          0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,0x2f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30
  };

const uint32 gaRFSynVCODivF_c[16] = { 
      0x00355555, 0x006aaaaa, 0x00a00000, 0x00d55555, 0x010aaaaa, 0x01400000, 0x01755555, 0x01aaaaaa,\
      0x01e00000, 0x00155555, 0x004aaaaa, 0x00800000, 0x00b55555, 0x00eaaaaa, 0x01200000, 0x01555555
  };


typedef union
{
  struct
  {
    uint32 bAcplCode      :4;
    uint32                :7;
    uint32 b802           :1;
    uint32 bOvr           :1;
    uint32 bBusy          :1;
    uint32 bCrc           :1;
    uint32 bTo            :1;
    uint32                :16;
  } Bits;
  uint32 Reg;
} MACA_STATUS_CODE_T;

uint8  g_ucLastLQI;

#define RESET_POWER_OUTAGE 0
#define RESET_UNKWNOWN     1
#define RESET_DIRTY_MEM    2
#define RESET_NO_ACK       3

#ifdef MACA_WACHDOG_ENABLED
  uint16 g_unStackSignature = 0x3A72;
  uint16 g_unWachDogRTC;
#endif  

#define IS_STRT_FLAG(x)        ( MASK_IRQ_STRT & (x) )
#define IS_LVL_FLAG(x)         ( MASK_IRQ_LVL & (x) )
#define IS_DI_FLAG(x)          ( MASK_IRQ_DI & (x) )
#define IS_CRC_FLAG(x)         ( MASK_IRQ_CRC & (x) )
#define IS_ACPL_FLAG(x)        ( MASK_IRQ_ACPL & (x) )

///////////////////////////////////////////////////////////////////////////////////
// Name:
// Author:        Marius Vilvoi
// Description:
//                this function will be used for
//
//
// Parameters:
//
// Return:        none
// Obs:
//      Access level:
//      Context:
///////////////////////////////////////////////////////////////////////////////////
#define PLATFORM_CLOCK         (24000000)
#define gDigitalClock_PN_c     24
#define gDigitalClock_RN_c     ((uint32)21)
#define gDigitalClock_RAFC_c_   0

void MACA_startClk(void)
{
  MACA_RESET = MACA_RESET_CLK_0N;    // set clock on and clear the reset flag      
  DelayLoop( 100 );
  
  MACA_CONTROL = CTRL_SEQ_NOP | CTRL_PRM;  
  while ( (MACA_STATUS & STATUS_CODE_MASK) == STATUS_CODE_NOT_COMPLETED )
    ;    
  
  MACA_CLRIRQ = 0xFFFF; // Clear all interrupts.     
}

void MACA_Init()
{      
//  MACA_startClk();
  
  //Freescale lib call
//  RadioInit(PLATFORM_CLOCK, gDigitalClock_PN_c, (gDigitalClock_RN_c<<25) + gDigitalClock_RAFC_c_ );
    
//  PHY_Reset();     
}

void MACA_Reset()
{
// reset the modem
    MACA_RESET = MACA_RESET_ON;                                               // set clock off and reset the modem        
    DelayLoop( 100 );
   
//wake up the modem
    MACA_startClk();
    
    RadioInit(PLATFORM_CLOCK, gDigitalClock_PN_c, (gDigitalClock_RN_c<<25) + gDigitalClock_RAFC_c_ );

// Freescale forum patch start // SMAC_InitFlybackSettings
    #define RF_BASE 0x80009a00 
    
    uint32_t val8, or; 
    val8 = *(volatile uint32_t *)(RF_BASE+8); 
    or = val8 | 0x0000f7df; 
    *(volatile uint32_t *)(RF_BASE+8) = or; 
    *(volatile uint32_t *)(RF_BASE+12) = 0x00ffffff; 
    *(volatile uint32_t *)(RF_BASE+16) = (((uint32_t)0x00ffffff)>>12); 
    *(volatile uint32_t *)(RF_BASE) = 16;       
// Freescale forum patch end
      
    
    MACA_TMRDIS       = 0x0000003F;                                           // Disable all action triggers
    MACA_CLKDIV       = 0x0000005F;                                           // DIVIDER_24_MHZ_TO_250_KHZ      
    MACA_WARMUP       = 0x00180012;      
    MACA_EOFDELAY     = 0x00000004;  // 16 us for complete action delay
    MACA_CCADELAY     = 0x00200020;  // <- Freescale recomandation //0x001a0022;
//    MACA_CCADELAY     = 0x001A0022; 
    MACA_TXCCADELAY   = 0x00000025;  

    MACA_PREAMBLE     = 0x00000000;
    MACA_FRAMESYNC0   = 0x000000A7;
    MACA_FRAMESYNC1   = 0;
    MACA_FLTREJ       = 0;
    
#ifdef TRIGGER_RX_LEVEL      
    MACA_SETRXLVL     = MACA_RX_LEVEL_TRIGGER + 1;                            //  de corelat cu IRQs de LVL poate nu se va folosi deloc
#else
    MACA_SETRXLVL     = 0x00FF; 
#endif
    
    MACA_MASKIRQ      = 0;

#ifdef SINGLE_ENDED_RF
    SetComplementaryPAState( 0 );
#else    
    SetComplementaryPAState( 1 ); // dual port
#endif        
    
    SetPowerLevelLockMode( 0 ); 
    
    SetEdCcaThreshold( 60 );
    
    CRM_REGS_P->VregCntl = 0x00000FF8; //Enables the radio voltage source  
    while( !CRM_STATUS.vReg1P5VRdy ) // Wait for regulator to become active
      ;  
        
    g_ucXVRStatus = PHY_DISABLED;
    
    MACA_CLRIRQ = 0xFFFF; // Clear all interrupts.     
    
    SetPower( g_ucPAValue ); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author Eduard Erdei
/// @brief  Set RF modem CCA Threshold
/// @param  p_ucThreshold - CCA threshold
/// @return none
/// @remarks
///      Access level: User level
////////////////////////////////////////////////////////////////////////////////////////////////////
void MACA_SetCCAThreshold(uint8 p_ucThreshold)
{
  MONITOR_ENTER(); 
  
  PHY_Disable_Request();  
  
  SetEdCcaThreshold( p_ucThreshold );
  
  MONITOR_EXIT(); 
}

///////////////////////////////////////////////////////////////////////////////////
// Name:
// Author:        Marius Vilvoi
// Description:
//                this function will be used for
//
//
// Parameters:
//
// Return:        none
// Obs:
//      Access level:
//      Context:
///////////////////////////////////////////////////////////////////////////////////
void MACA_Interrupt(void)
{
MACA_STATUS_CODE_T ulMacaStatus;
uint32 ulGetIrqs = MACA_IRQ;

  ulMacaStatus.Reg = MACA_STATUS;
  MACA_CLRIRQ = ulGetIrqs;

  if( g_ucXVRStatus == PHY_RX_START_DO ) // was on correct status
  {      
      if ( IS_CRC_FLAG(ulGetIrqs) )
      {
        PHY_ErrorIndicate(RX_CRC_ERROR, NULL);        
      }
#ifdef TRIGGER_RX_LEVEL      
      else if ( IS_LVL_FLAG(ulGetIrqs)  )
      {        
        MACA_MASKIRQ &= ~MASK_IRQ_LVL ;
        MACA_CLRIRQ = ulGetIrqs;        
        // TO DO at RX LEVEL IRQ
        // ...
      }      
#endif      
      else if ( IS_DI_FLAG(ulGetIrqs) ) // valid RX packet
      {        
        DEBUG3_OFF();
                            
        PHY_Disable_Request();
        
        g_aucPhyRx[0] -= 2; // remove the CRC from length
        if( g_aucPhyRx[0] <= PHY_BUFFERS_LENGTH )
        {     
          g_ucLastLQI = PhyPlmeGetLQI(); 
          if( !PHY_HeaderIndicate( (uint8*)g_aucPhyRx ) )
          {
              PHY_ErrorIndicate(OTH_ERROR, NULL);        
          }
          else 
          {
              PHY_DataIndicate( (uint8*)g_aucPhyRx ); 
          }
        }
        else
        {
          PHY_ErrorIndicate(RX_BUFFER_OVERFLOW, NULL);
        }
      }
      else if( IS_ACPL_FLAG(ulGetIrqs) ) // action complete
      {
          PHY_ErrorIndicate(PACKET_INCOMPLETE, NULL);          
      }
  }
  else if ( g_ucXVRStatus == PHY_TX_START_DO )
  {
      DEBUG4_OFF();
      if( IS_ACPL_FLAG(ulGetIrqs) ) // action complete
      {
        PHY_Disable_Request();

        if ( ulMacaStatus.Bits.bAcplCode )
        {
          PHY_DataConfirm(PHY_DATA_OTH_ERROR );
        }
        else if( ulMacaStatus.Bits.bBusy )
        {
          PHY_DataConfirm( TRANCEIVER_BUSY  );
        }
        else
        {
          PHY_DataConfirm(PHY_DATA_SUCCES );
        }
      }
      else // was start only case
      {
          DEBUG4_ON();
      }
  }
}


#ifdef MACA_WACHDOG_ENABLED
  void MACA_WachDog(void)
  {  
      if( g_unStackSignature != 0x3A72 )
      {
#ifdef DEBUG_RESET_REASON
            while( 1 ) // wachdog reset
              ;
#endif
      }
      if( !g_ulRadioSleepCounter )
      {
          if( (++g_unWachDogRTC) > 120 ) // nothig received for more that 2 minutes -> something wrong
          {
              if( 0 == (g_unWachDogRTC & 0x3F) ) // reset every minute the modem
              {
#ifdef DEBUG_RESET_REASON
                  g_aucDebugResetReason[13]++;
#endif
                  MONITOR_ENTER();
                  PHY_Disable_Request();
                  MACA_Reset();
                  
                  if(g_unWachDogRTC > 900 ) // nothig received 15 minutes ... something very wrong ... .. reset it
                  {                 
                      while( 1 ) // wachdog reset
                          ;
                  }
                  MONITOR_EXIT();
              }
          }
      }
  }

  void MACA_ResetWachDog(void)
  {
      g_unWachDogRTC = 0;    
  }
#endif    
