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
/// Description:  This file holds thye Physical Layer definitions of ISA 100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_PHY_H_
#define _NIVIS_PHY_H_

#include "../typedef.h"
#include "../global.h"
#include "../MC1322x.h"
#include "../digitals.h"
#include "../itc.h"
#include "../maca.h"


#define PHY_BUFFERS_LENGTH          ( 125 )
#define MIC_SIZE 4
#define PHY_MIN_HDR_SIZE            (15+1+2) // MHR + DHDR + first 2 bytes from DMXHR

extern void SetChannel(uint8 channel,  uint8 RFSynVCODivI, uint32 RFSynVCODivF); 

extern volatile uint8 g_ucXVRStatus;
extern volatile uint8 g_aucPhyRx[PHY_BUFFERS_LENGTH + 1 + 2];


// ----------- PHY CHANNELS ------------
#define PHY_CHAN_11               (unsigned char)  0
#define PHY_CHAN_12               (unsigned char)  1
#define PHY_CHAN_13               (unsigned char)  2
#define PHY_CHAN_14               (unsigned char)  3
#define PHY_CHAN_15               (unsigned char)  4
#define PHY_CHAN_16               (unsigned char)  5
#define PHY_CHAN_17               (unsigned char)  6
#define PHY_CHAN_18               (unsigned char)  7
#define PHY_CHAN_19               (unsigned char)  8
#define PHY_CHAN_20               (unsigned char)  9
#define PHY_CHAN_21               (unsigned char) 10
#define PHY_CHAN_22               (unsigned char) 11
#define PHY_CHAN_23               (unsigned char) 12
#define PHY_CHAN_24               (unsigned char) 13
#define PHY_CHAN_25               (unsigned char) 14
#define PHY_CHAN_26               (unsigned char) 15


// ------- PHY ENABLE REQUEST PARAMETERS -----------
#define PHY_DISABLE               (unsigned char) 0
#define PHY_ENABLE_TX             (unsigned char) 1
#define PHY_ENABLE_RX             (unsigned char) 2
// ------- ENABLE CONFIRM CODES
#define PHY_DISABLED_SUCCES       (unsigned char) 0
#define PHY_TX_ENABLED_SUCCES     (unsigned char) 1
#define PHY_TX_ENABLED_ERROR      (unsigned char) 2
#define PHY_RX_ENABLED_SUCCES     (unsigned char) 3
#define PHY_RX_ENABLED_ERROR      (unsigned char) 4


// ------- CCA REQUEST VOID   -----------
// ------- CCA CONFIRM CODES
#define PHY_CCA_CHAN_IDLE         (unsigned char) 0
#define PHY_CCA_CHAN_BUSY         (unsigned char) 1
#define PHY_CCA_TRANCEIVER_OFF    (unsigned char) 2
#define PHY_CCA_OTH_ERROR         (unsigned char) 3


// ------- DATA REQUEST VOID   -----------
// ------- DATA CONFIRM TX CODES
#define PHY_DATA_SUCCES           (unsigned char) 0
#define PHY_DATA_TRANCEIVER_OFF   (unsigned char) 1
#define PHY_DATA_TRANCEIVER_BUSY  (unsigned char) 2
#define PHY_DATA_RECEIVER_ON      (unsigned char) 3
#define PHY_DATA_OTH_ERROR        (unsigned char) 4


// ------- LOCAL MANAGEMENT REQUEST SERVICES PARAMETERS
#define PHY_MNG_RESET                 (unsigned char) 0
#define PHY_MNG_READ_TX_PWR_LVL       (unsigned char) 1
#define PHY_MNG_WRITE_TX_PWR_LVL      (unsigned char) 2
#define PHY_MNG_WRITE_RX_OVERFLOW_LVL (unsigned char) 3
#define PHY_MNG_IDLE                  (unsigned char) 4
// ------- LOCAL MANAGEMENT CONFIRM SERVICES CODES
#define PHY_MNG_SUCCES                 (unsigned char) 0
#define PHY_MNG_ERROR                  (unsigned char) 1


// ------- ERROR CODES (RX INDICATE)
#define RX_BUFFER_OVERFLOW        (unsigned char) 1
#define TX_BUFFER_UNDERFLOW       (unsigned char) 2
#define PACKET_INCOMPLETE         (unsigned char) 3
#define TRANCEIVER_BUSY           (unsigned char) 4
#define OTH_ERROR                 (unsigned char) 5
#define RX_GENERAL_ERROR          (unsigned char) 6
#define RX_CRC_ERROR              (unsigned char) 7


// ------- STATE MACHINE CODE
#define   PHY_DISABLED            (unsigned char)  0 // maca clock off
#define   PHY_IDLE                (unsigned char)  1 // maca clock on but do nothing
#define   PHY_CCA_START_DO        (unsigned char)  2 // start action
#define   PHY_TX_START_DO         (unsigned char)  3 // start action
#define   PHY_RX_START_DO         (unsigned char)  4 // start action


#define PHY_Disable_Request( ... ) PHY_DisableTR()
void PHY_DisableTR ( void );
void PHY_RX_Request ( uint8 p_ucChannel, uint8 p_ucRelativeFlag,  uint16 p_unBitDelay, uint16 p_unRXTimeout );
void PHY_TX_Request ( uint8 p_ucChannel, uint8 p_ucRelativeFlag,  uint16 p_unBitDelay, uint8 p_ucCCAFlag, uint8 * p_pucData, uint8 p_ucLen );
void PHY_MNG_Request ( uint8 p_ucService, uint8 * p_pucData);

extern void PHY_EnableConfirm  ( unsigned char p_ucStatus,  unsigned char p_ucChannel);
extern void PHY_EnableIndicate ( void );
extern void PHY_CCAConfirm     ( unsigned char p_ucStatus);
extern void PHY_DataConfirm    ( unsigned char p_ucStatus );

extern void PHY_MNG_Confirm    ( unsigned char p_ucService, unsigned char p_ucStatus,  unsigned char * p_pucData);
extern void PHY_MNG_Indicate   ( unsigned char p_ucService, unsigned char p_ucStatus,  unsigned char * p_pucData);
extern void PHY_ErrorIndicate  ( unsigned char p_ucStatus,  unsigned char * p_pucData);

__arm uint32 PHY_GetLastRXTmr2Age(void);
__arm uint32 PHY_GetLastTXuSecAge(void);

#define PHY_Reset() MACA_Reset()

void PHY_CkWachDog(void);



////////////////////////////////////////////////////////////////////////////////
// ------- Security Module
////////////////////////////////////////////////////////////////////////////////


#define PHY_AES_ResetKey()
extern const uint8 * g_pDllKey;
extern uint8 g_ucLastRFChannel;

#define PHY_AES_SetKeyInterrupt(x) g_pDllKey=x
#define PHY_WakeUpRequest() // wake up next time
#define PHY_SetToIdle()     // wake up modem if necessary


#define PHY_ReleaseRXBuff()

uint8 PHY_IsRXOnChannel(uint8 p_ucChannel );

#endif // _NIVIS_PHY_H_ 

