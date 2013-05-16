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
* Name:         timers.c
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#include "timers.h"
#include "crm.h"
#include "ISA100/mlde.h"
#include "ISA100/mlsm.h"
#include "../LibInterface/EmbeddedTypes.h"
#include "../LibInterface/ITC_Interface.h"
#include "../LibInterface/Timer.h"

volatile unsigned char g_uc250msFlag = 0;

void TMR_Init(void)
{
  TmrInit();  

  TmrDisable( gTmr0_c );
  TmrDisable( gTmr1_c );
  TmrDisable( gTmr3_c );  
  TmrEnable(  gTmr2_c );
  
// TMR2 ==> 62,50 ms; prescaler = 32
  TMR2_CTRL = 0x1A20;  
  TMR2_SCTRL = 0x0000; 
  TMR2_LOAD = 0x0000;  
  TMR2_COMP1 =  46875; 
  TMR2_CSCTRL =0x0040;  
}


void TMR_Interrupt(void)
{
  if (ITC.IntFrc & (1 << gTmrInt_c)) // every 250 ms
  {
      ITC_TestReset(gTmrInt_c); //forces deassertion for one interrupt flag in IntFrc register
      
      MLSM_On250ms();
      MLSM_OnTimeSlotStart( 1 );
      
      g_uc250msFlag = 1;
      
      while( CRM_STATUS_RTC_WU_EVT ) // wait for RTC clock 2 cycles
      {
          CRM_STATUS_RTC_WU_EVT = 1;  // clear RTC flag (make sense for debug purpose only - step by step debugging)
      }

      ITC.IntEnNum = gCrmInt_c;
  }

  if(TMR2_CSCTRL_TCF1)
  {
      TMR2_CSCTRL_TCF1 = 0;
      if( g_stSlot.m_uc250msSlotNo < g_stSlot.m_ucsMax250mSlotNo - 1 )
      {
          g_stSlot.m_uc250msSlotNo ++;
          
         #if (DEBUG_MODE >= 5)
            DEBUG1_ON();// test - enter slot (10msec) interrupt
         #endif
              
          MLSM_OnTimeSlotStart( 0 );
          
         #if (DEBUG_MODE >= 5)
            DEBUG1_OFF();// test - enter slot (10msec) interrupt
         #endif
      }
  }
}
