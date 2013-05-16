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
/// Date:         January 2008
/// Description:  This file holds definitions of the device manager application process
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DMAP_H_
#define _NIVIS_DMAP_H_

#include "../typedef.h"
#include "aslsrvc.h"
#include "mlde.h"

/*==========================[ public datatypes ]=============================*/
#define ISA100_START_PORTS 0xF0B0

enum
{
  OBJ_TYPE_UDO   = 3,
  OBJ_TYPE_HRCO  = 4,
  OBJ_TYPE_DPO   = 120,
  OBJ_TYPE_ASLMO = 121,  
  OBJ_TYPE_TLMO  = 122,
  OBJ_TYPE_NLMO  = 123,
  OBJ_TYPE_DLMO  = 124,
  OBJ_TYPE_DSMO  = 125,
  OBJ_TYPE_ARMO  = 126,
  OBJ_TYPE_DMO   = 127  
}; // ISA100_OBJECT_TYPES;

enum
{
  DMAP_DMO_OBJ_ID   = 1,
  DMAP_ARMO_OBJ_ID  = 2,
  DMAP_DSMO_OBJ_ID  = 3,
  DMAP_DLMO_OBJ_ID  = 4, 
  DMAP_NLMO_OBJ_ID  = 5,
  DMAP_TLMO_OBJ_ID  = 6,
  DMAP_ASLMO_OBJ_ID = 7,  
  DMAP_UDO_OBJ_ID   = 8,
  DMAP_DPO_OBJ_ID   = 9,
  DMAP_HRCO_OBJ_ID  = 10,  
  
  DMAP_OBJ_NO       = 10    
}; // STANDARD_DMAP_OBJECT_IDS

enum
{
  SM_STSO_OBJ_ID  = 1,
  SM_DSO_OBJ_ID   = 2,
  SM_SCO_OBJ_ID   = 3,
  SM_DMSO_OBJ_ID  = 4, //device management service 
  SM_SMO_OBJ_ID   = 5, //system monitoring
  SM_PSMO_OBJ_ID  = 6,
  SM_UDO_OBJ_ID   = 7,  
  SM_ARO_OBJ_ID   = 8,
  SM_DPSO_OBJ_ID  = 9,
  SM_HRCO_OBJ_ID  = 10,  
    
  SM_OBJ_NO       = 10  
}; // SYSTEM_MANAGEMENT_OBJ_IDS

enum
{
  STSO_CTR_UTC_OFFSET = 1,
  STSO_NEXT_UTC_TIME,
  STSO_NEXT_UTC_OFFSET 
}; // SM_STSO_ATTRIBUTES

enum{
  PSMO_SECURITY_JOIN_REQ = 1,
  PSMO_SECURITY_JOIN_CONF = 2,
  PSMO_SEC_NEW_SESSION = 6,
};//PSMO_METHODS;

enum{
    DSMO_NEW_KEY = 1,
    DSMO_DELETE_KEY = 2,
    DSMO_KEY_POLICY_UPDATE = 3
};//DSMO_METHODS

enum{
  DSO_LOOKUP_SHORT_ADDR = 1,
  DSO_LOOKUP_IPV6_ADDR
};//DSO_METHODS;

enum{
  DMSO_JOIN_REQUEST = 1,
  DMSO_CONTRACT_REQUEST,
  DMSO_JOIN_CONFIRMATION
};//DMSO_METHODS;

enum{
  DMAP_COMMAND_NONE = 0,
  DMAP_COMMAND_STOP,
  DMAP_COMMAND_START,
  DMAP_COMMAND_SW_RESET,
  DMAP_COMMAND_HW_RESET,
  DMAP_COMMAND_STATE_NO    
}; // DMAP_COMMAND_STATES 

enum{
  TLMO_SCHED_WRITE = 1,
  TLMO_READ_ROW,
  TLMO_WRITE_ROW,
  TLMO_RESET_ROW,
  TLMO_DELETE_ROW
}; //TLMO_METHODS;

enum{
  NLMO_SET_ROW_RT           = 1,
  NLMO_GET_ROW_RT           = 2,
  NLMO_DEL_ROW_RT           = 3,
  NLMO_SET_ROW_CONTRACT_TBL = 4,
  NLMO_GET_ROW_CONTRACT_TBL = 5,
  NLMO_DEL_ROW_CONTRACT_TBL = 6,
  NLMO_SET_ROW_ATT          = 7,
  NLMO_GET_ROW_ATT          = 8,
  NLMO_DEL_ROW_ATT          = 9  
}; //NLMO_METHODS;

typedef struct
{
  uint16 unContractId;
  uint8 ucContractStatus;
}JOIN_REPLY_CONTRACT;

typedef struct
{
  uint16 m_unJoinTimeout; // seconds
} JOIN_RETRY_CONTROL;

typedef struct
{
  uint8 m_aucAddr[8];
  uint8 m_ucJoinReqID;  
  uint8 m_ucFwReqID;
  uint8 m_ucTTL;
  uint8 m_ucLastMethodID;
} NEW_DEVICE_JOIN_INFO;

/*==========================[ public defines ]===============================*/

#define INVALID_CONTRACTID   ((uint16)0x0000)
#define JOIN_CONTRACT_ID              0x0001

// internally used UIDs (dlmo UIDs are ExtDLUint type (0 to 32767)
// for internally UIDs, use range 0x8000 to oxFFFF

//UIDs hardcoded by the ISA100
#define JOIN_SF                 0x0001
#define JOIN_ROUTE_UID          0x0001
#define JOIN_GRAPH_UID          0x0001
#define JOIN_REQ_TX_LINK_UID    0x0001
#define JOIN_RESP_RX_LINK_UID   0x0002
#define JOIN_ADV_RX_LINK_UID    0x0003

/*==========================[ globals ]======================================*/

extern uint8 g_ucJoinStatus;
extern uint16 g_unSysMngContractID;
extern SHORT_ADDR g_unSysMngrShortAddr;

extern JOIN_RETRY_CONTROL  g_stJoinRetryCntrl;

extern uint16 g_unRadioSleepReqId;
extern uint16 g_unJoinCommandReqId;

#define JOIN_CMD_WAIT_REQ_ID        0xFFFF 
#define JOIN_CMD_APPLY_ASAP_REQ_ID  0xFEFF

#ifdef BACKBONE_SUPPORT
  extern uint8 g_ucSMLinkTimeout;
#endif

#ifdef DEBUG_RESET_REASON
  extern uint8  g_aucDebugResetReason[16];
#endif  
  
/*==========================[ public functions ]=============================*/

extern void DMAP_Init();

extern void DMAP_Task();


  

extern uint8 GetGenericAttribute(  uint8       p_ucTSAPID,
                                   uint16      p_unObjID,
                                   ATTR_IDTF * p_pIdtf,
                                   uint8 *     p_pBuf,
                                   uint16 *    p_punLen);

extern uint8 SetGenericAttribute(  uint8       p_ucTSAPID, 
                                   uint16      unObjID,
                                   ATTR_IDTF * p_pIdtf,
                                   const uint8 *     p_pBuf,
                                   uint16      p_unLen);

extern void DMAP_ExecuteGenericMethod( EXEC_REQ_SRVC * p_pExecReq,
                                       APDU_IDTF *     p_pIdtf,
                                       EXEC_RSP_SRVC * p_pExecRsp );

extern void DMAP_CheckTenthCounter(void);

extern void DMAP_CheckSecondCounter(void);
extern uint8 g_ucIsOOBAccess;

__monitor void DMAP_ResetStack(uint8 p_ucReason);
  
#ifndef BACKBONE_SUPPORT    
    extern uint16 g_unLastJoinCnt;
    
    void DMAP_CheckNeighborLink(void);
    void DMAP_DLMO_IndicateDAUX(  uint16  p_unRouterAddr );
#else
    #define DMAP_CheckNeighborLink()
    #define DMAP_DLMO_IndicateDAUX(...)
#endif    

#if defined( ROUTING_SUPPORT ) || defined( BACKBONE_SUPPORT )

    extern void DMAP_DMO_CheckNewDevInfoTTL(void);
#else
    
    #define DMAP_DMO_CheckNewDevInfoTTL()
    
#endif // ( ROUTING_SUPPORT || BACKBONE_SUPPORT )

#if (DEVICE_TYPE == DEV_TYPE_MC13225)
    
    #if defined(BACKBONE_SUPPORT)
    
    typedef enum
    {
        LOCAL_MNG_RESTARTED_SUBCMD = 1,
        LOCAL_MNG_PROVISIONING_SUBCMD,
        LOCAL_MNG_ROUTES_SUBCMD,
        LOCAL_MNG_KEY_SUBCMD,
        LOCAL_MNG_ALERT_SUBCMD,
        LOCAL_MNG_SET_ENGINEERING_MODE,        
        LOCAL_MNG_READ_ENGINEERING_MODE,        
        
        LOCAL_MNG_SUBCMD_NO      
    } LOCAL_MNG_SUBCMD;
    uint8 DMAP_ParseLocalMng( const uint8 * p_pMngCmd, uint8 p_ucMngCmdLen );
    
    #endif
#endif
    
#endif //_NIVIS_DMAP_H_
