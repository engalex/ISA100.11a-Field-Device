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
/// @file       tmr_util.h
/// @verbatim   
/// Author:       Nivis LLC, Ion Ticus
/// Date:         December 2008
/// Description:  Timer translations
/// Changes:      Created 
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_TMR_UTIL_H_
#define _NIVIS_TMR_UTIL_H_

  #include "../system.h"

  #define ABSOLUTE_DELAY 0
  #define RELATIVE_DELAY 1

  // fraction  = 1 / 2^^20 sec
  // fraction2 = 1 / 2^^15 sec
  // usec      = 1 / 10^^6 sec

  // 2 ^^ 15 / 10^^6 =  2147 / 2^^16
  #define USEC_TO_FRACTION2(x) ((((uint32)(x)) * 2147) >> 16)
  
  //  10 ^^ 6 / 2^^15  =  5 ^^ 6 / 2 ^^ 9
  #define FRACTION2_TO_USEC(x) ((((uint32)(x)) * (125*125)) >> 9)

  // 10 ^^ 6 / 2 ^^ 20 = 4 * 5 ^^ 6 / 2 ^^ 16 =  62500 / 2 ^^ 16
  #define FRACTION_TO_USEC(x) ((((uint32)(x)) * 62500) >> 16)

  // 2 ^^ 20 / 10 ^^ 6 = 2^^16 / (4 * 5^^6) =  2 ^^ 16 / 62500
  #define USEC_TO_FRACTION(x) ((((uint32)(x)) << 16) / 62500)

  #define FRACTION2_TO_FRACTION(x) (((uint32)(x)) << 5)
  #define FRACTION_TO_FRACTION2(x) (((uint32)(x)) >> 5)


  #if ( DEVICE_TYPE == DEV_TYPE_MC13225 )

      #include "../crm.h"
      #include "..\timers.h"

      //  1.333 ->  4 / 3
      #define TMR2_TO_USEC(x)        ((((uint32)(x)) << 2) / 3)
      #define USEC_TO_TMR2(x)        ((((uint32)(x)) * 3) >> 2)
        
      // TMR2_TO_USEC() / 4 = (4/3)/4 = 1 / 3  
      #define TMR2_TO_PHY_BITS(x)     ((uint32)(x) / 3)
      #define PHY_BITS_TO_TMR2(x)     ((uint32)(x) * 3)
        
      #define TMR0_TO_FRACTION(x) (((uint32)(x)) << 5)
      #define FRACTION_TO_TMR0(x) (((uint32)(x)) >> 5)
      
      // 4/3 * 2^^20 / 10 ^^ 6 = 2^^22 / (3 * 5^^6 * 2^^6) =  2^^16 / (3 * 5^^6)
      #define TMR2_TO_FRACTION(x) ((((uint32)(x)) << 16) / (15625 * 3))
      #define FRACTION_TO_TMR2(x) ((((uint32)(x)) * 15625 * 3) >> 16)
      
      #define FRACTION2_TO_TMR0(x) (x)
      #define TMR0_TO_FRACTION2(x) (x)
                  
      #define USEC_TO_TMR0(x) USEC_TO_FRACTION2(x)
      #define TMR0_TO_USEC(x) FRACTION2_TO_USEC(x)      

      __arm inline uint32 TMR_Get250msOffset(void) 
      {
          return (RTC_COUNT - g_ul250msStartTmr);
      }
      
      #define TMR_GetSlotOffset()      TMR2_CNTR

      // 2^^16 / 2 ^^ 5 / (3 * 5^^6) = 2 ^^ 11 / (3 * 5^^6)
      #define TMR2_TO_TMR0(x)      ((((uint32)(x)) << 11) / (15625 * 3)) // FRACTION_TO_TMR0( TMR2_TO_FRACTION(x) )
      #define TMR0_TO_TMR2(x)      ((((uint32)(x)) * 15625 * 3) >> 11)

      extern   uint16 g_unDllTMR2SlotLength;

      #define  g_unTMRStartSlotOffset ( TMR_Get250msOffset() - TMR2_TO_TMR0(TMR_GetSlotOffset()) ) // as TMR0

      #define TMR_CLK_250MS             8192         // (2^^15 / 2^^2 = 2^^13) no of Timer clocks to count 250ms

      inline void TMR_Set250msCorrection(int16 p_nCorrection)
      {
          g_unRtc250msDuration -= p_nCorrection;
      }
      
      __arm inline void TMR_SetCounter(uint16 p_unCntr)
      {
          if( p_unCntr < (CLK_250MS_32KHZ + 1) )
          {
              p_unCntr = CLK_250MS_32KHZ + 1 - p_unCntr;
              RTC_TIMEOUT = p_unCntr;
              g_ul250msEndTmr = RTC_COUNT + p_unCntr;              
          }
      }                                     
      
      #ifdef BACKBONE_SUPPORT
        #define MAX_TMR_250MS_CORRECTION 1 // 1 -> 30.5*4 = 122 ppm
        #define MAX_TMR_BBR_CORRECTION   2 // -> max correction is between 61 ppm ... 30.5 ppm
      #else
        #define MAX_TMR_250MS_CORRECTION 1 // 1 -> 30.5*4 = 122 ppm
      #endif
  
  #else
    #warning  "HW platform not supported"
  #endif

#endif
