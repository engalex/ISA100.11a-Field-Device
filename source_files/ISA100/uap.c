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
/// Author:       Nivis LLC, Eduard Erdei
/// Date:         March 2009
/// Description:  This file implements UAP functionalities
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "uap.h"
#include "aslde.h"
#include "aslsrvc.h"
#include "dmap.h"
#include "dmap_co.h"
#include "../digitals.h"
#include "../CommonAPI/RadioApi.h"
#include "../COmmonAPI/Common_API.h"
#include "../CommonAPI/DAQ_Comm.h"



// Rares: Some constants defining nr of 250ms loops. Note: due to unsigned char [0..255], max 255*250ms = 63.75 sec is possible
#define EVERY_250MSEC 1
#define EVERY_500MSEC 2*EVERY_250MSEC
#define EVERY_1SEC    2*EVERY_500MSEC
#define EVERY_5SEC    5*EVERY_1SEC
#define EVERY_10SEC   2*EVERY_5SEC
#define EVERY_15SEC   3*EVERY_5SEC
#define EVERY_30SEC   2*EVERY_15SEC
#define EVERY_60SEC   2*EVERY_30SEC



#if ( _UAP_TYPE != 0 )

#if ( PLATFORM == PLATFORM_VN210 )
    
    // The period of time the LEDs indication is active (User presses the Wakeup&Status push-button to start the LEDs activity)
    #define LEDS_SIGNALING_PERIOD        10*EVERY_1SEC // This is the time period on which the LEDs signaling is active
    #define LEDS_SIGNALING_ALWAYS_ON     255          // Same as above but only for debug -> LEDs signaling is always active
    #define LEDS_SIGNALING_OFF           0            // All LEDs should be turned off.
    
    // The states signaled by the JOIN_STATUS led through solid on (0) or blinking at different rates (for example slow every second or fast every 1/2 second)
    //#define LED_JOIN_STATUS_JOINED_PERIOD   1                 // Solid ON
    #define LED_JOIN_STATUS_DISCOVERY_PERIOD  2*EVERY_1SEC      // Blink
    #define LED_JOIN_STATUS_JOINING_PERIOD    EVERY_500MSEC     // Blink
        
    uint8 g_ucRequestForLedsON;       // Activate/Deactivate LEDs Signaling
    
#endif // PLATFORM == PLATFORM_VN210     

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  notifies an internal or external UAP about a new or modified contract
/// @param  p_pContractRsp - contract response buffer
/// @param  p_ucContractRspLen - contract response len
/// @return -
/// @remarks
///      Access level: user level
///      Obs:
////////////////////////////////////////////////////////////////////////////////////////////////////
void UAP_NotifyContractResponse( const uint8 * p_pContractRsp, uint8 p_ucContractRspLen )
{
    // called every time a new contract is added to DMAP DMO contract table

    // Send Contract Data onto the application processor.
    SendMessage(STACK_SPECIFIC, ISA100_NOTIFY_ADD_CONTRACT, API_REQ, 0, p_ucContractRspLen, p_pContractRsp );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  notifies an internal or external UAP about a contract deletion from DMAP DMO contract table
/// @param  p_unContractID - UID of the deleted contract
/// @return -
/// @remarks
///      Access level: user level
///      Obs:
////////////////////////////////////////////////////////////////////////////////////////////////////
void UAP_NotifyContractDeletion(uint16 p_unContractID)
{
    // called every time a contract is deleted in DMAP DMO contract table
    // check here if this UAP is the owner of this contract and perform required operations

    p_unContractID = __swap_bytes( p_unContractID );
    // Send notification of contract deletion to the application processor
    SendMessage(STACK_SPECIFIC, ISA100_NOTIFY_DEL_CONTRACT, API_REQ, 0, 2, (uint8 *)&p_unContractID);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  notifies an internal or external UAP about the receipt of an alert acknowledge
/// @param  p_unContractID - UID of the deleted contract
/// @return -
/// @remarks
///      Access level: user level
///      Obs:
////////////////////////////////////////////////////////////////////////////////////////////////////
void UAP_NotifyAlertAcknowledge(uint8 p_ucAlertID)
{
    // called every time an alert acknowledge is received

    // Send notification of an alert acknowledge to the application processor
    SendMessage(STACK_SPECIFIC, ISA100_ON_UAP_ALARM_ACK, API_REQ, 0, 1, &p_ucAlertID);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief
/// @param  -
/// @return -
/// @remarks
///      Access level: user level
///      Obs:
////////////////////////////////////////////////////////////////////////////////////////////////////
void UAP_OnJoinReset(void)
{

    g_ucExternalCommStatus = EXTERNAL_COMM_NOTOK;

#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
    static const uint8 aConst[1] = {0};
    //called every time the ISA100 stack is reset
    SendMessage(STACK_SPECIFIC, ISA100_NOTIFY_JOIN, API_REQ, 0, 1, aConst);
#endif
}

void UAP_OnJoin(void)
{
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
    static const uint8 aConst[1] = {1};
    //called every time the ISA100 stack is reset
    SendMessage(STACK_SPECIFIC, ISA100_NOTIFY_JOIN, API_REQ, 0, 1, aConst );
#endif
}

#if ( PLATFORM == PLATFORM_VN210 )  

  void UAP_ledsSignaling( void )
  {
     if (g_ucRequestForLedsON)
     {
         // 
         // LED_JOIN_STATUS
         //
         if ( g_ucJoinStatus == DEVICE_DISCOVERY )  /* Field Device in discovery state */
         {
            if ( g_ucRequestForLedsON % LED_JOIN_STATUS_DISCOVERY_PERIOD == 0 ) // Blink slower -> Field device in discovery mode
            {
               LED_JOIN_STATUS_TOGGLE();
            }
         }
         else if ( g_ucJoinStatus == DEVICE_JOINED ) /* Field Device Joined */
         {
           LED_JOIN_STATUS_ON(); 
         }
         else //if () /* Field Device in Joining Mode -> all the other states that are not discovery or joined*/
         {
            if ( g_ucRequestForLedsON % LED_JOIN_STATUS_JOINING_PERIOD == 0 )   // Blink faster -> Field device in joining mode
            {
               LED_JOIN_STATUS_TOGGLE();
            }
         }                
         
         // 
         // LED_EXTERNAL_COMM
         //
         if ( g_ucExternalCommStatus == EXTERNAL_COMM_OK ) /* Field Device established communication with external device */
         {
             LED_EXTERNAL_COMM_ON();
         }
         else if ( g_ucExternalCommStatus == EXTERNAL_COMM_NOTOK ) /* Field Device in progress of communicating with external device ?! */
         {
             if ( g_ucRequestForLedsON % LED_EXTERNAL_COMM_COMMUNICATING == 0 )
             {
                 LED_EXTERNAL_COMM_TOGGLE();
             }
         } 
         else // if ( g_ucExternalCommStatus == EXTERNAL_COMM_DISABLED )  // Communicate with external device is disabled ?!
         {
             LED_EXTERNAL_COMM_OFF();
         }
     }
     else
     {
        // Turn OFF all the LEDs
        LEDS_OFF();
     }
  }

  void UAP_ckPushButtonAndLED( void )
  {
     static uint8 s_ucPushButtonTimeout = 0;
      
     if( WAKEUP_STATUS_IS_PRESSED() )
     {
        if( s_ucPushButtonTimeout < (10*EVERY_1SEC) )
        {
            s_ucPushButtonTimeout++;
        }
        else // button pressed for more that 10sec
        {
            g_stDMO.m_ucJoinCommand = DMO_JOIN_CMD_RESET_FACTORY_DEFAULT;
            g_unJoinCommandReqId = JOIN_CMD_APPLY_ASAP_REQ_ID;  //force the reset next second             
            s_ucPushButtonTimeout = 0;
        }
        
        // LEDs status here as specified in ERD document -> enable leds signaling for the specified period of time
        if (s_ucPushButtonTimeout>EVERY_1SEC)
        {  //if too short then probably is just noise
          g_ucRequestForLedsON = LEDS_SIGNALING_PERIOD;
        }
     }
     else // button released
     {
        s_ucPushButtonTimeout = 0;
        
        // LEDs status here as specified in ERD document
        if (g_ucRequestForLedsON > 0 && g_ucRequestForLedsON != LEDS_SIGNALING_ALWAYS_ON) // The LEDs signaling is active all the time if LEDS_SIGNALING_ALWAYS_ON is used. Debug only.
          g_ucRequestForLedsON--;
     }
   }

#endif // #if ( PLATFORM == PLATFORM_VN210 )

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC,
/// @brief  UAP periodic task
/// @param  -
/// @return -
/// @remarks
///      Access level: user level
///      Obs:
////////////////////////////////////////////////////////////////////////////////////////////////////
void UAP_MainTask()
{
#if ( PLATFORM == PLATFORM_VN210 )  
    UAP_ledsSignaling();
    UAP_ckPushButtonAndLED(); 
#endif
}

#endif // #if ( _UAP_TYPE != 0 )
