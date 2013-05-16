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
/// Author:       Nivis LLC, Mihaela Goloman
/// Date:         November 2008
/// Description:  This file holds definitions of the alert report management object in DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DMAP_ARMO_H_
#define _NIVIS_DMAP_ARMO_H_

#include "../typedef.h"
#include "aslde.h"
#include "dmap_utils.h"
#include "dmap_dmo.h"

typedef enum{
  ALARM_DIR_RET_OR_NO_ALARM = 0,
  ALARM_DIR_IN_ALARM = 1
}ALARM_DIRECTION;

typedef enum{
  ALERT_CLASS_EVENT = 0,
  ALERT_CLASS_ALARM = 1
}ALERT_CLASS;

typedef enum{
  ALERT_CAT_DEV_DIAG = 0,   //device diagnostics alerts
  ALERT_CAT_COMM_DIAG = 1,  //communications diagnostics alerts
  ALERT_CAT_SECURITY = 2,   //security alerts
  ALERT_CAT_PROCESS = 3,    //process alert
  ALERT_CAT_NO = 4
}ALERT_CATEGORY;

typedef enum{
  ALERT_PR_JOURNAL_L = 0, //low treshold for journal priority
  ALERT_PR_JOURNAL_M = 1,
  ALERT_PR_JOURNAL_H = 2, //high treshold for journal priority
  ALERT_PR_LOW_L = 3,
  ALERT_PR_LOW_M = 4,
  ALERT_PR_LOW_H = 5,
  ALERT_PR_MEDIUM_L = 6,
  ALERT_PR_MEDIUM_M = 7,
  ALERT_PR_MEDIUM_H = 8,
  ALERT_PR_HIGH_L = 9,
  ALERT_PR_HIGH_M = 10,
  ALERT_PR_HIGH_H = 11,
  ALERT_PR_URGENT_L = 12,
  ALERT_PR_URGENT_LM = 13,
  ALERT_PR_URGENT_MH = 14,
  ALERT_PR_URGENT_H = 15  
}ALERT_PRIORITY;

enum{
  ARMO_ALERT_MASTER_DEV_DIAG = 1,
  ARMO_CONF_TIMEOUT_DEV_DIAG = 2,
  ARMO_ALERTS_DISABLE_DEV_DIAG = 3,
  
  ARMO_ALERT_MASTER_COMM_DIAG = 4,
  ARMO_CONF_TIMEOUT_COMM_DIAG = 5,
  ARMO_ALERTS_DISABLE_COMM_DIAG = 6,
 
  ARMO_ALERT_MASTER_SECURITY = 7,
  ARMO_CONF_TIMEOUT_SECURITY = 8,
  ARMO_ALERTS_DISABLE_SECURITY = 9,

  ARMO_ALERT_MASTER_PROCESS = 10,
  ARMO_CONF_TIMEOUT_PROCESS = 11,
  ARMO_ALERTS_DISABLE_PROCESS = 12,
  
  ARMO_COMM_DIAG_RECOV_ALERT_DESCR  = 13,
  ARMO_SECURITY_RECOV_ALERT_DESCR = 14,
  ARMO_DEV_DIAG_RECOV_ALERT_DESCR = 15,
  ARMO_PROCESS_RECOV_ALERT_DESCR = 16,
  
  ARMO_ATTR_NO = 17
}; //ARMO_ATTRIBUTES;

enum{
  ARMO_ALARM_RECOVERY = 1
}; //ARMO_METHODS;

typedef struct{
  uint8   m_aNetworkAddr[16]; //IPv6 address of remote enpoint
  uint16  m_unTLPort; //transport layer port at remote endpoint (0 means not configured endpoint)
  uint16  m_unObjID; //object identifier at remote endpoint
  
  // additional
  uint16 m_unContractID;
  uint8  m_ucContractStatus;
}ALERT_COMM_ENDPOINT;

typedef struct
{
  uint8 m_ucState;  // alarm recovery state
  uint8 m_ucLastID; // last reported alarm ID    
}ALERT_RECOVERY;    // internal use for description of the alarm recovery status

enum{
  RECOVERY_DISABLED   = 0,
  RECOVERY_ENABLED    = 1,
  RECOVERY_START_SENT = 2,
  RECOVERY_DONE       = 3
 //RECOVERY_STOP_SENT   
}; //ALERT_RECOVERY_STATUS


/*alert types*/
enum{
  ALERT_DD_ALARM_REC_START = 0,
  ALERT_DD_ALARM_REC_END = 1,
  ALERT_DD_FAILURE = 2,
  ALERT_DD_OFF_SPEC = 3,
  ALERT_DD_MAINTENANCE = 4,
  ALERT_DD_CK_FUNCTION = 5
};//ALERT_DEV_DIAG

enum{
  ALERT_CD_DL_CONNECTIVITY = 0,
  ALERT_CD_NEIGHDISC = 1
};//ALERT_COMM_DIAG

enum{
  ALARM_RECOVERY_START    = 0,
  ALARM_RECOVERY_END      = 1
};//ALARM_RECOVERY_ALERT_TYPES

enum{
  ALERT_PR_ALARM_REC_START,
  ALERT_PR_ALARM_REC_END,
  ALERT_PR_DISCRETE_ALARM,
  ALERT_PR_HIGH_ALARM,
  ALERT_PR_HIGH_HIGH_ALARM,
  ALERT_PR_LOW_ALARM,
  ALERT_PR_LOW_LOW_ALARM,
  ALERT_PR_DEVIATION_LOW,
  ALERT_PR_DEVIATION_HIGH,
  ALERT_PR_OBJ_DISABLED
};//ALERT_PROCESS

typedef struct{
  
  ALERT_COMM_ENDPOINT   m_stAlertMaster;    //alert recv addr
  uint8                 m_ucConfTimeout;    //timeout waiting for alert confirmation
  uint8                 m_ucAlertsDisable;  //disable all alerts for this category
  APP_ALERT_DESCRIPTOR  m_stRecoveryDescr; 
  
  //additional  
  ALERT_RECOVERY        m_stRecovery;
  uint8  m_ucAlertsNo;   
  uint8  m_ucAlertReportTimer;              // used to control the bandwidth of alert reporting
  
}ARMO_STRUCT;

enum{
  ARMO_NO_CONTR                     ,
  ARMO_AWAIT_CONTRACT_ESTABLISHMENT ,
  ARMO_CONTRACT_ACTIVE              ,
  ARMO_WAITING_CONTRACT_TERMINATION 
};

//extern uint8 g_ucDiscoveryFlag;

extern const DMAP_FCT_STRUCT c_aARMOFct[ARMO_ATTR_NO];
extern ARMO_STRUCT g_aARMO[ALERT_CAT_NO];
extern uint16  g_unAlarmTimeout;


#define ARMO_Read(p_unAttrID,p_punBufferSize,p_pucRspBuffer) \
            DMAP_ReadAttr(p_unAttrID,p_punBufferSize,p_pucRspBuffer,c_aARMOFct,ARMO_ATTR_NO)

#define ARMO_Write(p_unAttrID,p_ucBufferSize,p_pucBuffer)   \
            DMAP_WriteAttr(p_unAttrID,p_ucBufferSize,p_pucBuffer,c_aARMOFct,ARMO_ATTR_NO)

uint8 ARMO_Execute(uint8   p_ucMethID, 
                   uint16  p_unReqSize, 
                   uint8*  p_pReqBuf,
                   uint16* p_pRspSize,
                   uint8*  p_pRspBuf);

uint8 ARMO_AddAlertToQueue(const ALERT* p_pAlert, const uint8* p_pBuf);

void ARMO_ProcessAlertAck(uint8 p_ucID);
void ARMO_NotifyAddContract(DMO_CONTRACT_ATTRIBUTE * p_pContract);
void ARMO_Init();
void ARMO_Task();
void ARMO_OneSecondTask( void );

#endif //_NIVIS_DMAP_ARMO_H_
