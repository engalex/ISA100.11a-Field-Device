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
* Name:         digitals.h
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This header file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#ifndef _NIVIS_DIGITALS_H_
#define _NIVIS_DIGITALS_H_

#include "typedef.h"
#include "global.h"
#include "MC1322x.h"
#include "../LibInterface/gpio.h"
#include "../LibInterface/Platform.h"
#include "CommonAPI/Common_API.h"

//============================================================================================================
// Basic pin operations
//============================================================================================================

  #define GET_GPIO_AS_BIT(x) (1L <<((x) % 32))

  #define SELECT_GPIO_AS_OUTPUT( ucGPIO ) Gpio_SetPinDir( ucGPIO, gGpioDirOut_c )
  #define SELECT_GPIO_AS_INPUT( ucGPIO )  Gpio_SetPinDir( ucGPIO, gGpioDirIn_c )
  
  #define SET_GPIO_HI( ucGPIO )  Gpio_SetPinData( ucGPIO, gGpioPinStateHigh_c )
  #define SET_GPIO_LO( ucGPIO )  Gpio_SetPinData( ucGPIO, gGpioPinStateLow_c )
  #define TOGGLE_PIN( ucGPIO )   Gpio_TogglePin( ucGPIO )
  
  #define ENABLE_GPIO_PULLUP( ucGPIO )    Gpio_EnPinPullup(ucGPIO, 1)
  #define DISABLE_GPIO_PULLUP( ucGPIO )   Gpio_EnPinPullup(ucGPIO, 0)
  #define SET_GPIO_WITH_PULLUP( ucGPIO )    Gpio_SelectPinPullup(ucGPIO, gGpioPinPullup_c)
  #define SET_GPIO_WITH_PULLDOWN( ucGPIO )  Gpio_SelectPinPullup(ucGPIO, gGpioPinPulldown_c)
  
  #define ENABLE_GPIO_HYSTERESIS( ucGPIO )   Gpio_EnPinHyst(ucGPIO, 1)
  #define DISABLE_GPIO_HYSTERESIS( ucGPIO )  Gpio_EnPinHyst(ucGPIO, 0)
  
  #define SET_GPIO_READ_SOURCE_TO_PAD( ucGPIO ) Gpio_SetPinReadSource(ucGPIO, gGpioPinReadPad_c)
  #define SET_GPIO_READ_SOURCE_TO_REG( ucGPIO ) Gpio_SetPinReadSource(ucGPIO, gGpioPinReadReg_c)

  #define GET_LO_GPIO_VALUE( ucGPIO )           ((GPIO_REGS_P->DataLo & GET_GPIO_AS_BIT( ucGPIO )))
  #define GET_HI_GPIO_VALUE( ucGPIO )           ((GPIO_REGS_P->DataHi & GET_GPIO_AS_BIT( ucGPIO )))
  #define GET_GPIO_VALUE2( ucGPIO, state )      Gpio_GetPinData(ucGPIO, state )

  #define SET_LED_ON(LED_PIN)  SET_GPIO_HI( LED_PIN ) /* LEDs turn ON when pin goes High */
  #define SET_LED_OFF(LED_PIN) SET_GPIO_LO( LED_PIN ) /* LEDs turn OFF when pin goes Low */
  #define TOGGLE_LED(LED_PIN)  TOGGLE_PIN( LED_PIN )



//============================================================================================================
// Nivis Crow Radio Modem p/n: 51000143 Rev. E
//============================================================================================================

  // SSI related pins
  #define SSI_MOSI  gGpioPin0_c       /* pin #28 */
  #define SSI_MISO  gGpioPin1_c       /* pin #29 */
  #define RTC_CS    gGpioPin2_c       /* pin #30 */
  #define SSI_CLK   gGpioPin3_c       /* pin #27 */
  #define SSI_CS    gGpioPin10_c      /* pin #24 */
  
  #define SPI_CLK   gGpioPin7_c       /* pin #27 */
  #define SPI_MOSI  gGpioPin6_c       /* pin #28 */
  #define SPI_MISO  gGpioPin5_c       /* pin #29 */
  #define SPI_SS    gGpioPin4_c       /* pin #30 */

  #define EXT_MEM_PWR  gGpioPin11_c

  #ifdef WAKEUP_ON_EXT_PIN 
      #define APP_WAKEUP_LINE gGpioPin9_c
  #endif
  #define MT_RTS    gGpioPin20_c
  #define SP_CTS    gGpioPin21_c
  #define SP_TIME   gGpioPin26_c
    //Pin definitions for 5 wires Integration Kit
  #define UART2_TX  gGpioPin18_c
  #define UART2_RX  gGpioPin19_c


/////////////////////////////////
// see it on : sensor_node_schematic.pdf = pag 1 ( 1322X-SRB - Main Schematic )
// and : MC1322x_Reference_Manual.pdf = pag 11-4 ( 11.4.2 Pin Register Mappings )

  #define KYB0_LINE     gGpioPin22_c /* KB0 */
  #define KYB1_LINE     gGpioPin23_c /* KB1 */
  #define KYB2_LINE     gGpioPin24_c /* KB2 */
  #define KYB3_LINE     gGpioPin25_c /* KB3 */
  #define KYB4_LINE     gGpioPin26_c /* KB4 */
  #define KYB5_LINE     gGpioPin27_c /* KB5 */
  #define KYB6_LINE     gGpioPin28_c /* KB6 */
  #define KYB7_LINE     gGpioPin29_c /* KB7 */


  #define ADC0_LINE     gGpioPin30_c /* ADC0 */
  #define ADC1_LINE     gGpioPin31_c /* ADC1 */
  #define ADC2_LINE     gGpioPin32_c /* ADC2 */
  #define ADC3_LINE     gGpioPin33_c /* ADC3 */
  #define ADC4_LINE       gGpioPin34_c /* ADC4 */
  #define ADC5_LINE       gGpioPin35_c /* ADC5 */
  #define ADC6_LINE       gGpioPin36_c /* ADC6 */
  #define ADC7_RTCK_LINE  gGpioPin37_c /* ADC7_RTCK */
  #define ADC2_VREFH_LINE gGpioPin38_c /* ADC2_VREFH */
  #define ADC2_VREFL_LINE gGpioPin39_c /* ADC2_VREFL */
  #define ADC1_VREFH_LINE gGpioPin40_c /* ADC1_VREFH */
  #define ADC1_VREFL_LINE gGpioPin41_c /* ADC1_VREFL */

  #define PA_ENA_PIN1   gGpioPin42_c /* ANT_1 */
  #define PA_ENA_PIN2   gGpioPin43_c /* ANT_2 */
  #define TX_FCT_PIN    gGpioPin44_c 
  #define RX_FCT_PIN    gGpioPin45_c 

  #if !defined( SINGLE_ENDED_RF )
    #define RX_LNA_ON() 
    #define TX_PA_ON()  _GPIO_DATA_SET1.Reg   = GET_GPIO_AS_BIT( PA_ENA_PIN1 ) | GET_GPIO_AS_BIT( PA_ENA_PIN2 )
    #define TX_PA_OFF() _GPIO_DATA_RESET1.Reg = GET_GPIO_AS_BIT( PA_ENA_PIN1 ) | GET_GPIO_AS_BIT( PA_ENA_PIN2 )

  #else
    #define RX_LNA_ON() 
    #define TX_PA_ON()  
    #define TX_PA_OFF() 
  #endif


      // 32 KHz crystal circuit control. This pin must always be ON to ensure that 32 KHz clock is available
      // Note: the pin's functionality is LED_JOIN_STATUS on the PLATFORM_INTEGRATION_KIT
#if ( PLATFORM == PLATFORM_DEVELOPMENT ) 
      #define CLK_32KHZ_PIN               (KYB0_LINE)
#endif

  //----------------------------------------------------------------------------
  // LEDs
  //----------------------------------------------------------------------------
  
      #if ( PLATFORM == PLATFORM_DEVELOPMENT )  // We cannot use this GPIO as LED because it is cotrolling the 32KHz clock
        
        // LED D19 RED controlled by GPIO22 gGpioPin22_c / KBY_0 (LED_CLK_PIN)
        #define LED_JOIN_STATUS_ON()              
        #define LED_JOIN_STATUS_OFF()             
        #define LED_JOIN_STATUS_TOGGLE()  
       
      #elif ( PLATFORM == PLATFORM_VN210 )

        // LED D19 RED controlled by GPIO22 gGpioPin22_c / KBY_0
        #define LED_JOIN_STATUS              (KYB0_LINE)
        #define LED_JOIN_STATUS_ON()         SET_LED_ON(LED_JOIN_STATUS)
        #define LED_JOIN_STATUS_OFF()        SET_LED_OFF(LED_JOIN_STATUS)
        #define LED_JOIN_STATUS_TOGGLE()     TOGGLE_LED(LED_JOIN_STATUS)

        // LED D20 GREEN controlled by GPIO31 gGpioPin31_c / ADC_1
        #define LED_EXTERNAL_COMM            (ADC1_LINE)
        #define LED_EXTERNAL_COMM_ON()       SET_LED_ON(LED_EXTERNAL_COMM)
        #define LED_EXTERNAL_COMM_OFF()      SET_LED_OFF(LED_EXTERNAL_COMM)
        #define LED_EXTERNAL_COMM_TOGGLE()   TOGGLE_LED(LED_EXTERNAL_COMM)
   
      #else
        #warning "Unsupported platform"
      #endif


      
      // All LEDs (To Do: Change these functions to single step function for all LEDs -> single step set the GPIO register(s) for all the LEDs)
      #define SET_LEDS_AS_OUTPUT()  { SELECT_GPIO_AS_OUTPUT(LED_JOIN_STATUS); SELECT_GPIO_AS_OUTPUT(LED_EXTERNAL_COMM); }
      #define INIT_LEDS()           { SET_LEDS_AS_OUTPUT(); LEDS_OFF(); }
      #define LEDS_ON()             { LED_JOIN_STATUS_ON(); LED_EXTERNAL_COMM_ON(); }
      #define LEDS_OFF()            { LED_JOIN_STATUS_OFF(); LED_EXTERNAL_COMM_OFF(); }
      #define LEDS_TOGGLE()         { LED_JOIN_STATUS_TOGGLE(); LED_EXTERNAL_COMM_TOGGLE(); }

  //----------------------------------------------------------------------------
  // Switches, Push-Buttons and External Interrupt pins
  //----------------------------------------------------------------------------
  
      // SW3: Switch BOOT on GPIO23 gGpioPin23_c / KBI_1 input
      #define SWITCH_BOOT                         KYB1_LINE
      #define SWITCH_BOOT_STATUS()                ( GET_LO_GPIO_VALUE( SWITCH_BOOT ) )
  
      // SW5: Switch COMM Interface on GPIO30 / ADC0 input
      #define SWITCH_COMM_INTERFACE               ADC0_LINE
      #define SWITCH_COMM_INTERFACE_STATUS()      ( GET_LO_GPIO_VALUE( SWITCH_COMM_INTERFACE ) )
  
      // SW4: Push-Button WakeUp Status on GPIO28 gGpioPin28_c / KBI_6 input
      #define PUSHBUTTON_WAKEUP_STATUS            KYB6_LINE
      #define WAKEUP_STATUS_IS_PRESSED()          ( !GET_LO_GPIO_VALUE( PUSHBUTTON_WAKEUP_STATUS ) )

      // Wake-up circuit on GPIO26 gGpioPin26_c / KBI_4 --> this input line is Wake-up from UART1, UART2, SPI, I2C buses (interrupt)
      #define EXT_WAKEUP                          KYB4_LINE
      
      #define EXT_WAKEUP_IS_ON()                  ( GET_LO_GPIO_VALUE( EXT_WAKEUP ) )

//============================================================================================================
// Debug
//============================================================================================================

    #define DEBUG1_OFF() 
    #define DEBUG1_ON()  
    #define DEBUG2_OFF() 
    #define DEBUG2_ON()  
    #define DEBUG3_OFF() 
    #define DEBUG3_ON()  
    #define DEBUG4_OFF() 
    #define DEBUG4_ON()  


void Digitals_Init(void);

#endif /* _NIVIS_DIGITALS_H_ */

