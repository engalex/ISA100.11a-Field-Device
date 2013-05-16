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
* Name:         digitals.c
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#include "digitals.h"
#include "../LibInterface/Platform.h"

//===================================================================================================
#if ( PLATFORM == PLATFORM_DEVELOPMENT )

  #warning "---------> Platform set as: PLATFORM_DEVELOPMENT"

  #define gDirHiValue_c               GET_GPIO_AS_BIT( PA_ENA_PIN1 ) | GET_GPIO_AS_BIT( PA_ENA_PIN2 )    //MBAR_GPIO + 0x04
  #define gDataHiValue_c              0x00000000     //MBAR_GPIO + 0x0C

  #define gDirLoValue_c               GET_GPIO_AS_BIT( CLK_32KHZ_PIN )  \
                                      | GET_GPIO_AS_BIT( EXT_MEM_PWR )  \
                                      | GET_GPIO_AS_BIT( SSI_CS )       \
                                      | GET_GPIO_AS_BIT( SSI_CLK )      \
                                      | GET_GPIO_AS_BIT( SSI_MOSI )     \
                                      | GET_GPIO_AS_BIT( RTC_CS )      
  
  #define gDataLoValue_c              GET_GPIO_AS_BIT( CLK_32KHZ_PIN ) | GET_GPIO_AS_BIT( EXT_MEM_PWR )  //MBAR_GPIO + 0x08

  #define gPuEnLoValue_c              0xffffffff     //MBAR_GPIO + 0x10
  #define gPuEnHiValue_c              0xffffffff     //MBAR_GPIO + 0x14
  #define gFuncSel0Value_c            0x00000000     //MBAR_GPIO + 0x18
  #define gFuncSel1Value_c            0x00000000     //MBAR_GPIO + 0x1C
  #define gFuncSel2Value_c            0x05000000    // 0x00000000 //MBAR_GPIO + 0x20 // TXon RXon 
  #define gFuncSel3Value_c            0x00000000     //MBAR_GPIO + 0x24
  #define gInputDataSelLoValue_c      0x00000000     //MBAR_GPIO + 0x28
  #define gInputDataSelHiValue_c      0x00000000     //MBAR_GPIO + 0x2C
  #define gPuSelLoValue_c             0x10003000     //MBAR_GPIO + 0x30
  #define gPuSelHiValue_c             0x8001c000     //MBAR_GPIO + 0x34
  #define gHystEnLoValue_c            0x00000000     //MBAR_GPIO + 0x38
  #define gHystEnHiValue_c            0x00000000     //MBAR_GPIO + 0x3C
  #define gPuKeepLoValue_c            0xc0000000     //MBAR_GPIO + 0x40
  #define gPuKeepHiValue_c            0x000000df     //MBAR_GPIO + 0x44

//===================================================================================================
#elif ( PLATFORM == PLATFORM_VN210 )  

  // GPIO Pad Direction Registers (GPIO_PAD_DIR0 = gDirLoValue_c and GPIO_PAD_DIR1 = gDirHiValue_c)
  // Input  = 0
  // Output = 1
      // GPIO Direction for pins   0-31 (Lo) : MBAR_GPIO + 0x00
    #ifdef WAKEUP_ON_EXT_PIN 
          #define gDirLoValue_c   0x00000000                              \
                                  | GET_GPIO_AS_BIT( LED_JOIN_STATUS )    \
                                  | GET_GPIO_AS_BIT( LED_EXTERNAL_COMM )  \
                                  | GET_GPIO_AS_BIT( EXT_MEM_PWR )        \
                                  | GET_GPIO_AS_BIT( SSI_CS )             \
                                  | GET_GPIO_AS_BIT( SSI_CLK )            \
                                  | GET_GPIO_AS_BIT( SSI_MOSI )           \
                                  | GET_GPIO_AS_BIT( RTC_CS )             \
                                  | GET_GPIO_AS_BIT( APP_WAKEUP_LINE )    \
                                  | GET_GPIO_AS_BIT( UART2_TX )           \
                                  | GET_GPIO_AS_BIT( MT_RTS )             

  // GPIO Data Registers (GPIO_DATA0 = gDataLoValue_c and GPIO_DATA1 = gDataHiValue_c)
  // Low = 0
  // High = 1
      // GPIO Data value for pins   0-31 (Lo) : MBAR_GPIO + 0x08
      #define gDataLoValue_c      0x00000000                              \
                                  | GET_GPIO_AS_BIT( EXT_MEM_PWR )   /* reverse logic -> power off*/ \
                                  | GET_GPIO_AS_BIT( APP_WAKEUP_LINE )    \
                                  | GET_GPIO_AS_BIT( UART2_TX )           \
                                  | GET_GPIO_AS_BIT( MT_RTS )            
    #else //WAKEUP_ON_EXT_PIN
          #define gDirLoValue_c   0x00000000                              \
                                  | GET_GPIO_AS_BIT( LED_JOIN_STATUS )    \
                                  | GET_GPIO_AS_BIT( LED_EXTERNAL_COMM )  \
                                  | GET_GPIO_AS_BIT( SPI_SS )             \
                                  | GET_GPIO_AS_BIT( EXT_MEM_PWR )        \
                                  | GET_GPIO_AS_BIT( SSI_CS )             \
                                  | GET_GPIO_AS_BIT( SSI_CLK )            \
                                  | GET_GPIO_AS_BIT( SSI_MOSI )           \
                                  | GET_GPIO_AS_BIT( RTC_CS )             \
                                  | GET_GPIO_AS_BIT( UART2_TX )           \
                                  | GET_GPIO_AS_BIT( MT_RTS )             

  // GPIO Data Registers (GPIO_DATA0 = gDataLoValue_c and GPIO_DATA1 = gDataHiValue_c)
  // Low = 0
  // High = 1
      // GPIO Data value for pins   0-31 (Lo) : MBAR_GPIO + 0x08
      #define gDataLoValue_c      0x00000000                                \
                                  | GET_GPIO_AS_BIT( SPI_SS )               \
                                  | GET_GPIO_AS_BIT( EXT_MEM_PWR )   /* reverse logic -> power off*/ \
                                  | GET_GPIO_AS_BIT( UART2_TX )             \
                                  | GET_GPIO_AS_BIT( MT_RTS )            
    #endif //WAKEUP_ON_EXT_PIN

      // GPIO Direction for pins  32-63 (Hi) : MBAR_GPIO + 0x04
      #define gDirHiValue_c       0x00000000                              \
                                  | GET_GPIO_AS_BIT( PA_ENA_PIN2 )        \
                                  | GET_GPIO_AS_BIT( PA_ENA_PIN1 )        


      // GPIO Data value for pins  32-63 (Hi) : MBAR_GPIO + 0x0C
      #define gDataHiValue_c    0x00000000


  // GPIO Pull-up Enable Registers (GPIO_PAD_PU_EN0 = gPuEnLoValue_c and GPIO_PAD_PU_EN1 = gPuEnHiValue_c)
  // 0 = Pull-ups/pull-downs disabled for inputs, 
  // 1 = (Default) Pull-ups/pull-downs enabled for inputs
      // GPIO Pull-up Enable Register for pins   0-31 (Lo) : MBAR_GPIO + 0x10
      #define gPuEnLoValue_c      0xffffffff

      // GPIO Pull-up Enable Register for pins  32-63 (Hi) : MBAR_GPIO + 0x14
      #define gPuEnHiValue_c      0xffffffff     

  // GPIO Function Select Registers (GPIO_FUNC_SEL3 = gFuncSel3Value_c, GPIO_FUNC_SEL2 = gFuncSel2Value_c, GPIO_FUNC_SEL1 = gFuncSel1Value_c, and GPIO_FUNC_SEL0 = gFuncSel0Value_c)
  // 0 = Functional Mode 0 (default)
  // 1 = Alternate Mode  1 (Function 1)
  // 2 = Alternate Mode  2 (Function 2)
  // 3 = Alternate Mode  3 (Function 3)
      #define gFuncSel0Value_c    0x00000000     //MBAR_GPIO + 0x18
      #define gFuncSel1Value_c    0x00000000     //MBAR_GPIO + 0x1C

      #define gFuncSel2Value_c    0x05000000    // 0x00000000 //MBAR_GPIO + 0x20 // TXon RXon 

      #define gFuncSel3Value_c    0x00000000     //MBAR_GPIO + 0x24

  // GPIO Data Select Registers (GPIO_DATA_SEL1 = gInputDataSelHiValue_c and GPIO_DATA_SEL0 = gInputDataSelLoValue_c)
  // 0 = (Default) Data is read from the pad.
  // 1 = Data is read from the GPIO Data Register
      #define gInputDataSelLoValue_c      0x00000000     //MBAR_GPIO + 0x28
      #define gInputDataSelHiValue_c      0x00000000     //MBAR_GPIO + 0x2C

  // GPIO Pad Pull-up Select (GPIO_PAD_PU_SEL1 = gPuSelHiValue_c and GPIO_PAD_PU_SEL0 = gPuSelLoValue_c)
  // 0 = Weak pull-down (default for all except GPIOs 12-13 (I2C), 63 (EVTI_B), 46 (TMS), 47 (TCK) and 48 (TDI))
  // 1 = Weak pull-up (default for GPIOs 12-13 (I2C), 63 (EVTI_B), 46 (TMS), 47 (TCK) and 48 (TDI))
      //MBAR_GPIO + 0x30
  #if ( SPI1_MODE != NONE )
      #define gPuSelLoValue_c   0x00000000                                         \
                              | GET_GPIO_AS_BIT( SP_CTS )                          \
                              | GET_GPIO_AS_BIT( MT_RTS )                       \
                              | GET_GPIO_AS_BIT( PUSHBUTTON_WAKEUP_STATUS )        \
                              | GET_GPIO_AS_BIT( SPI_SS )   
  #else
      #define gPuSelLoValue_c   0x00000000                                         \
                              | GET_GPIO_AS_BIT( SP_CTS )                          \
                              | GET_GPIO_AS_BIT( MT_RTS )                       \
                              | GET_GPIO_AS_BIT( PUSHBUTTON_WAKEUP_STATUS )        \
                              | GET_GPIO_AS_BIT( UART2_TX )                        
  #endif

      //MBAR_GPIO + 0x34
      #define gPuSelHiValue_c   0x8001c000

  // GPIO Pad Hysteresis Enable Registers (GPIO_PAD_HYST_EN1 = gHystEnHiValue_c and GPIO_PAD_HYST_EN0 = gHystEnLoValue_c)
  // 0 = (Default) Hysteresis disabled
  // 1 = Hysteresis enabled
      //MBAR_GPIO + 0x38
      #define gHystEnLoValue_c  0x00000000

      //MBAR_GPIO + 0x3C
      #define gHystEnHiValue_c  0x00000000

  // GPIO Pad Keeper Enable Registers (GPIO_PAD_KEEP1 = gPuKeepHiValue_c and GPIO_PAD_KEEP0 = gPuKeepLoValue_c)
  // 0 = (Default) Pull-up keep disabled for all but GPIOs 30-36, 38-39
  // 1 = Pull-up keep enabled
      //MBAR_GPIO + 0x40
      #define gPuKeepLoValue_c  0xc0000000
      //#define gPuKeepLoValue_c  0x00000000
      //#define gPuKeepLoValue_c  0x00000000                                \
      //                          | GET_GPIO_AS_BIT( ADC0_LINE )            \
      //                          | GET_GPIO_AS_BIT( ADC1_LINE )

      //MBAR_GPIO + 0x44
      //#define gPuKeepHiValue_c  0x000000df
      #define gPuKeepHiValue_c  0x00000000                                \
                                | GET_GPIO_AS_BIT( ADC2_LINE )            \
                                | GET_GPIO_AS_BIT( ADC3_LINE )            \
                                | GET_GPIO_AS_BIT( ADC4_LINE )            \
                                | GET_GPIO_AS_BIT( ADC5_LINE )            \
                                | GET_GPIO_AS_BIT( ADC6_LINE )            \
                                | GET_GPIO_AS_BIT( ADC2_VREFH_LINE )      \
                                | GET_GPIO_AS_BIT( ADC2_VREFL_LINE )

#else
    #warning "Unsupported platform"          
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  Digitals_Init
/// @param  -
/// @return -
/// @remarks
///      Access level:
///      Obs:
////////////////////////////////////////////////////////////////////////////////////////////////////
void Digitals_Init(void)
{
  GPIO_REGS_P->DataLo = gDataLoValue_c;                  //MBAR_GPIO + 0x08
  GPIO_REGS_P->DataHi = gDataHiValue_c;                  //MBAR_GPIO + 0x0C

  GPIO_REGS_P->DirLo =  gDirLoValue_c;                   //MBAR_GPIO + 0x00
  GPIO_REGS_P->DirHi =  gDirHiValue_c;                   //MBAR_GPIO + 0x04

  GPIO_REGS_P->PuEnLo = gPuEnLoValue_c;                  //MBAR_GPIO + 0x10
  GPIO_REGS_P->PuEnHi = gPuEnHiValue_c;                  //MBAR_GPIO + 0x14
  GPIO_REGS_P->FuncSel0 = gFuncSel0Value_c;              //MBAR_GPIO + 0x18
  GPIO_REGS_P->FuncSel1 = gFuncSel1Value_c;              //MBAR_GPIO + 0x1C
  GPIO_REGS_P->FuncSel2 = gFuncSel2Value_c;              //MBAR_GPIO + 0x20
  GPIO_REGS_P->FuncSel3 = gFuncSel3Value_c;              //MBAR_GPIO + 0x24
  GPIO_REGS_P->InputDataSelLo = gInputDataSelLoValue_c;  //MBAR_GPIO + 0x28
  GPIO_REGS_P->InputDataSelHi = gInputDataSelHiValue_c;  //MBAR_GPIO + 0x2C
  GPIO_REGS_P->PuSelLo = gPuSelLoValue_c;                //MBAR_GPIO + 0x30
  GPIO_REGS_P->PuSelHi = gPuSelHiValue_c;                //MBAR_GPIO + 0x34
  GPIO_REGS_P->HystEnLo = gHystEnLoValue_c;              //MBAR_GPIO + 0x38
  GPIO_REGS_P->HystEnHi = gHystEnHiValue_c;              //MBAR_GPIO + 0x3C
  GPIO_REGS_P->PuKeepLo = gPuKeepLoValue_c;              //MBAR_GPIO + 0x40
  GPIO_REGS_P->PuKeepHi = gPuKeepHiValue_c;              //MBAR_GPIO + 0x44   
}
