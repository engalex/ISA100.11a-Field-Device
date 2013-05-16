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
* Name:         mlsm.h
* Author:       Nivis LLC, Doinel Alban
* Date:         March 2008
* Description:  MAC Extension Layer - TAI and State Machine
* Changes:
* Revisions:
****************************************************************************************************/
#ifndef _NIVIS_MLSM_H_
#define _NIVIS_MLSM_H_

#include "mlde.h"

typedef struct
{
    uint8  m_uc250msStep;
    uint8  m_ucSecondsFromLastSync; //m_ucSecondsFromLastSync still needed for filtering DAUX syncs 
    int16  m_nCorrection;
    
    // for use with ISA100 clock correction mechanism    
    uint16 m_unSrcClkTimeout;     
    uint16 m_unClkTimeout;
    uint8  m_ucClkInterogationStatus;
} TAI_STRUCT;

typedef struct
{
    uint8  m_uc250msSlotNo;
    uint8  m_ucsMax250mSlotNo;
} SLOT_STRUCT;

enum
{
  MLSM_START_SLOT = 0,        // -new timeslot start
  MLSM_TX_FRAME,              // -packet transmission
  MLSM_RX_ACK,                // -ACK reception (after frame transmission)
  MLSM_RX_FRAME,              // -packet reception
  MLSM_TX_ACK,                // -ACK transmission (after frame reception)
  MLSM_TX_NACK                // -NACK transmission (after frame reception)
}; // MAC Extension State Machine - STATUS


enum
{
  // IMPORTANT!!! do not change order of enums !!!
  SLOT_STS_NONE           = 0,  // there was nothing to execute in the slot
  SLOT_STS_TX_SUCCESS     = 1,  // succesfull unicast transmission
  SLOT_STS_NACK           = 2,  // a tx message has been NACK-ed
  SLOT_STS_TX_ACK_TIMEOUT = 3,  // no ack/nack received for a unicast tx message
  SLOT_STS_TXCCA_BACKOFF  = 4,  // tx aborted due CCA  
  SLOT_STS_RX_SUCCESS     = 5   // a valid message has been received
};  // SLOT_EXECUTION_STATUS;

enum
{
  MLSM_CLK_INTEROGATION_OFF     = 0,
  MLSM_CLK_INTEROGATION_ON      = 1,
  MLSM_CLK_INTEROGATION_TIMEOUT = 2
}; // ACTIVE_CLOCK_INTEROGATION_STATUS
  

//global variables used by DLL state machine
extern uint16 g_unDllTimeslotLength;
extern uint8  g_ucDllCrtCh;              // current used radio channel
extern uint8  g_ucDllRestoreSfOffset;    // =1 when superframe offsets must be recalculated from TAI cutover
extern TAI_STRUCT g_stTAI;
extern SLOT_STRUCT g_stSlot;

extern uint8  g_ucSlotExecutionStatus;  // variable used to store current slot execution status
extern uint8  g_ucDllTdmaStat;
extern uint16 g_unTxSFDFraction;          // save the time of transmitted packet's SFD (from slot start) in fraction counts

extern uint8 g_ucIdleLinkIdx; 

#if  (DEVICE_TYPE == DEV_TYPE_MC13225)
    #define g_unRandValue MACA_RANDOM
#else
    extern uint16 g_unRandValue;
#endif

__monitor uint32 MLSM_GetCrtTaiSec(void);

__monitor void MLSM_GetCrtTaiTime(uint32 *p_pulTaiSec, uint16 *p_punTaiFract);
__arm uint32 MLSM_GetSlotStartMsBE(int p_nSlotDiff);

void MLSM_SetCrtTaiTime(DLL_ADV_TIME_SYNC *p_pstTimeSync);

void MLSM_Init();

void MLSM_ClockCorrection(uint16 p_unFraction, uint16 p_unTimeslotOffset);

void MLSM_SetSlotLength( uint16 p_unTimeslotLen  );

void MLSM_OnTimeSlotStart( unsigned int p_unFirstSlot );

void MLSM_On250ms( void );

void MLSM_SetAbsoluteTAI( uint32 p_ulSec, uint16 p_unSecFract2 );

uint8 MLSM_CanReceiveMsg(void);

#define TX_WITH_ACK                   0
#define TX_WITH_NO_ACK                1
#define LINK_REMOVING                 2
#define SKIP_LINK                     3

uint8 MLSM_UpdateShareLinkExpBackoff(uint8 p_ucReason);

uint8 MLSM_AddNewCandidate(uint16 p_unShortAddr);

/*************************** PHY layer functions ******************************/

void PHY_DataConfirm (uint8 p_ucStatus);

void PHY_DataIndicate( uint8 * p_pRxBuff );

uint8 PHY_HeaderIndicate( uint8 * p_pRxBuff );

void PHY_ErrorIndicate(uint8 p_ucStatus, uint8 *p_pucData);

#endif // _NIVIS_MLSM_H_

