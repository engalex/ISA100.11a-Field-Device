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
* Name:         crm.c
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#include "typedef.h"
#include "maca.h"
#include "itc.h"
#include "crm.h"
#include "../LibInterface/ITC_Interface.h"

#include "ISA100/uap.h"
#include "ISA100/dmap.h"

uint32 g_ul250msStartTmr; /*this variable is used to memorise the moment of the start of a 250ms interval */
uint32 g_ul250msEndTmr; /*this variable is used to memorise the moment of the end of a 250ms interval */
uint32 g_aSaveGPIO[4];

uint16 g_unRtc250msDuration = CLK_250MS_32KHZ;

#if ( SPI1_MODE != NONE ) && defined( WAKEUP_ON_EXT_PIN ) // wake up for SPI interface
  uint32 g_ulLastCrmStatus;
#endif    


__arm void RtcClock_Interrupt(void)
{    
    ITC.IntDisNum = gCrmInt_c; // disable CRM interrupt until timer interrupt
#if ( SPI1_MODE != NONE ) && defined( WAKEUP_ON_EXT_PIN ) // wake up for SPI interface
    g_ulLastCrmStatus |= _CRM_STATUS.Reg; // save wake up status before clear the register
#endif    
    _CRM_STATUS.Reg = _CRM_STATUS.Reg;
    
    g_ul250msStartTmr = RTC_COUNT; 
    RTC_TIMEOUT = g_unRtc250msDuration;
    
    ITC_TestSet(gTmrInt_c);
        
    g_ul250msEndTmr = g_ul250msStartTmr + g_unRtc250msDuration + 2; // duration is 2 tics more that g_unRtc250msDuration
    g_unRtc250msDuration = CLK_250MS_32KHZ;
}

void CRM_Init (void)
{
      //XtalAdjust
//    CRM_XTAL_CNTL.CTune = 0x18;
//    CRM_XTAL_CNTL.FTune = 0x0F;

    // hibernate init    
    CRM_WU_CNTL_EXT_WU_POL &= ~4 ;

    CRM_WU_CNTL_EXT_WU_EN = 1;   //wake up from Key_4 ; //wake up on level high is default at reset
    CRM_WU_CNTL_EXT_WU_IEN = 0;  //without generating an interrupt
    
    CRM_WU_CNTL_TIMER_WU_EN = 1; //wake up from counter
    CRM_SLEEP_CNTL_RAM_RET = 3;  //3=96k
    CRM_SLEEP_CNTL_MCU_RET = 1;  // e un must pt ticus
    CRM_SLEEP_CNTL_DIG_PAD_EN = 1;
    
    //start the 32KHz osc
    _RINGOSC_CNTL.Bits.ROSC_EN=0;
    _XTAL32_CNTL.Bits.XTAL32_EN=1; //start the osc
    
    DelayLoop( 1000 );
    
    //source RTC_counter from 32KHz
    CRM_SYS_XTAL32_EXISTS = 1;  
    //reduce pwr consumption
    XTAL32_CNTL = 1;
    
    
    // set RTC interrupt
    IntAssignHandler(gCrmInt_c, (IntHandlerFunc_t)RtcClock_Interrupt);
    ITC_SetPriority(gCrmInt_c, gItcFastPriority_c); //fiq
    ITC_EnableInterrupt(gCrmInt_c);
    g_ul250msStartTmr = RTC_COUNT; 
    RTC_TIMEOUT = CLK_250MS_32KHZ;
    CRM_WU_CNTL_RTC_WU_EN = 1;
    CRM_WU_CNTL_RTC_WU_IEN = 1;   
}

#if ( SPI1_MODE != NONE ) && defined( WAKEUP_ON_EXT_PIN ) // wake up for SPI interface
  uint8 CRM_ResetWakeUpFlag(void)
  {
      uint8 ucResult = (g_ulLastCrmStatus | _CRM_STATUS.Reg) & 0x10;
      
      _CRM_STATUS.Reg = ucResult; // clear the EXT status flag
      g_ulLastCrmStatus = 0;      // clear the last saved EXT status 
      
      return ucResult;
  }
#endif    
