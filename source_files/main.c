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

#include <string.h>
#include <stdio.h>

#include "typedef.h"
#include "MC1322x.h"
#include "global.h"
#include "itc.h"
#include "timers.h"
#include "digitals.h"
#include "asm.h"
#include "maca.h"
#include "uart1.h"
#include "spif_interface.h"
#include "spi.h"
#include "crm.h"
#include "ISA100/phy.h"
#include "ISA100/NLDE.h"
#include "ISA100/DMAP.h"
#include "ISA100/dmap_armo.h"
#include "ISA100/DLDE.h"
#include "ISA100/uart_link.h"
#include "ISA100/aslsrvc.h"
#include "ISA100/slme.h"
#include "ISA100/mlsm.h"
#include "ISA100/uap.h"
#include "uart2.h"
#include "CommonAPI/DAQ_Comm.h"

///////////////////////////////////////////////////////////////////////////////////
// Name: MAIN Function
///////////////////////////////////////////////////////////////////////////////////
  void main(void)
  {
    IntDisableAll();
  
    WDT_INIT();  
             
    Digitals_Init();
   
    ItcInit();
    
    NVM_FlashInit();
    
    CRM_Init();  
        
    TMR_Init();
    ASM_Init();
    UART1_Init();
    UART2_Init();
    SPI_Init();
    
    PROVISION_Init();
        
    MACA_Init(); 
    
    DAQ_Init();
        
    DMAP_ResetStack(0);
    
    IntEnableAll();
    
    //--------------------------------------------------------------------
    // Main Loop
    //--------------------------------------------------------------------
    for (;;)
    {           
        NLDE_Task();        // as fast as possible, keep this task first on loop
  
        DMAP_Task();        //
        ASLDE_ASLTask();    //
 
        UART_LINK_Task();   //
        UART2_CommControl();
        
        DAQ_RxHandler();
        
        if( g_uc250msFlag ) // 250ms Tasks
        {
            g_uc250msFlag = 0;
            
            // Handle DAQ after all other stack related tasks.
            DAQ_TxHandler();            
            UAP_MainTask();     
            
            ARMO_Task();
            
            if( !g_stTAI.m_uc250msStep ) // first 250ms slot from each second -> 1sec Tasks
            {
                ASLDE_PerformOneSecondOperations();
                DMAP_DMO_CheckNewDevInfoTTL();
    
                DMAP_CheckSecondCounter();
                
                ARMO_OneSecondTask();
    
                SLME_KeyUpdateTask();
                
                DMO_PerformOneSecondTasks();  
                
                MACA_WachDog();
            }
        }

        FEED_WDT();
    }
  }
