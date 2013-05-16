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
/// Description:  This file implements the device manager application process
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include "provision.h"
#include "tlme.h"
#include "tlde.h"
#include "slme.h"
#include "nlme.h"
#include "tlde.h"
#include "nlde.h"
#include "dlde.h"
#include "dmap.h"
#include "dmap_dmo.h"
#include "dmap_dlmo.h"
#include "dmap_co.h"
#include "dmap_dpo.h"
#include "dmap_armo.h"
#include "dmap_udo.h"
#include "aslsrvc.h" 
#include "mlsm.h"
#include "uap.h"
#include "uart_link.h"
#include "dlmo_utils.h"
#include "tmr_util.h"
#include "../asm.h"
#include "../system.h"

#ifndef BACKBONE_SUPPORT 
    #include "../CommonAPI/DAQ_Comm.h"
#endif

#if (DEVICE_TYPE == DEV_TYPE_MC13225)
    #include "uart_link.h"
#endif

#ifdef DEBUG_RESET_REASON
  uint8  g_aucDebugResetReason[16]; // debug purpose
#endif  

/*==============================[ DMAP ]=====================================*/
#ifndef BACKBONE_SUPPORT

    typedef struct
    {
      uint16 m_unRouterAddr;
      uint16 m_unBannedPeriod;  //in seconds  
    } BANNED_ROUTER;
    
    uint8 g_ucBannedRoutNo;
    BANNED_ROUTER g_astBannedRouters[MAX_BANNED_ROUTER_NO];
    
    static SHORT_ADDR s_unDevShortAddr;
    uint16 g_unLastJoinCnt;

    static void AddBannedAdvRouter(SHORT_ADDR p_unDAUXRouterAddr, uint16 p_unBannedPeriod);
    static void UpdateBannedRouterStatus();
#else
    uint8 g_ucSMLinkTimeout = SM_LINK_TIMEOUT;    
#endif  //#ifndef BACKBONE_SUPPORT

#if (MAX_SIMULTANEOUS_JOIN_NO==1)    
  #warning  "for SM support, no more simultaneous joining sessions accepted" 
#endif  
    
const uint8 c_aTLMEMethParamLen[TLME_METHOD_NO-1] = {1, 18, 16, 18, 18}; // input arguments lenghts for tlmo methods  
    
uint16 g_unSysMngContractID; 

SHORT_ADDR  g_unDAUXRouterAddr;
uint8 g_ucIsOOBAccess;

uint16 g_unRadioSleepReqId;
uint16 g_unJoinCommandReqId;

uint32              g_ulDAUXTimeSync;
uint8               g_ucJoinStatus;
JOIN_RETRY_CONTROL  g_stJoinRetryCntrl;

__no_init static uint32 g_aulNewNodeChallenge[4] ;
static uint8 g_aucSecMngChallenge[16];

#if defined(ROUTING_SUPPORT) || defined(BACKBONE_SUPPORT)
  NEW_DEVICE_JOIN_INFO g_stDevInfo[MAX_SIMULTANEOUS_JOIN_NO];
  uint8 g_ucDevInfoEntryNo;
#endif // #if defined(ROUTING_SUPPORT || defined(BACKBONE_SUPPORT)
/*==============================[ DMO ]======================================*/
static void DMAP_processReadRequest( READ_REQ_SRVC * p_pReadReq,
                                     APDU_IDTF *     p_pIdtf);    

static void DMAP_processWriteRequest( WRITE_REQ_SRVC * p_pWriteReq,
                                      APDU_IDTF *      p_pIdtf);                                 

static void DMAP_processExecuteRequest( EXEC_REQ_SRVC * p_pExecReq,
                                        APDU_IDTF *     p_pIdtf);

static void DMAP_processReadResponse(READ_RSP_SRVC * p_pReadRsp, APDU_IDTF * p_pAPDUIdtf);

static void DMAP_processWriteResponse(WRITE_RSP_SRVC * p_pWriteRsp);

static void DMAP_processExecuteResponse( EXEC_RSP_SRVC * p_pExecRsp,
                                         APDU_IDTF *     p_pAPDUIdtf );
static void DMAP_joinStateMachine(void);
  
static uint16 DMO_generateSMJoinReqBuffer(uint8* p_pBuf);
static uint16 DMO_generateSecJoinReqBuffer(uint8* p_pucBuf);
static void DMO_checkSecJoinResponse(EXEC_RSP_SRVC* p_pExecRsp);
static void DMO_checkSMJoinResponse(EXEC_RSP_SRVC* p_pExecRsp);

static void DMAP_DMO_applyJoinStatus( uint8 p_ucSFC );

#if defined(ROUTING_SUPPORT) || defined(BACKBONE_SUPPORT)

  static uint8 DMAP_DMO_forwardJoinReq( EXEC_REQ_SRVC * p_pExecReq,
                                        APDU_IDTF *     p_pIdtf);
  
  static void DMAP_DMO_forwardJoinResp ( EXEC_RSP_SRVC * p_pExecRsp );
    
  static NEW_DEVICE_JOIN_INFO* DMAP_DMO_checkNewDevJoinInfo(uint8  p_pucMethodId, 
                                                            APDU_IDTF*  p_pstIdtf);
  
  static NEW_DEVICE_JOIN_INFO* DMAP_DMO_getNewDevJoinInfo( uint8  p_ucFwReqID );
  
#else  
  #define DMAP_DMO_forwardJoinReq(...)  SFC_INVALID_SERVICE
  #define DMAP_DMO_forwardJoinResp(...)  
#endif //#if defined(ROUTING_SUPPORT || defined(BACKBONE_SUPPORT)


/*==============================[ DLMO ]=====================================*/
static void DMAP_startJoin(void);

#ifndef BACKBONE_SUPPORT 
  static __monitor void DMAP_DLMO_prepareSMIBS(void);
#endif // not BACKBONE_SUPPORT

static uint8 NLMO_prepareJoinSMIBS(void);
static uint8 NLMO_updateJoinSMIBS(void);
  
/*==============================[ TLMO ]=====================================*/
static uint8 TLMO_execute( uint8    p_ucMethID,
                           uint16   p_unReqSize,
                           uint8*   p_pReqBuf,
                           uint16 * p_pRspSize,
                           uint8*   p_pRspBuf);

/*===========================[ DMAP implementation ]=========================*/

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Initializes the DLL with default SMIBS
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void setDllDefaultSMIBS(void)
{
  DLMO_DOUBLE_IDX_SMIB_ENTRY stTwoIdxSMIB;
  
  
  uint8 ucTmp = g_ucDllNeighDiagNo;
  
  memset(g_stDll.m_aSMIBTableRecNo, 0, sizeof(g_stDll.m_aSMIBTableRecNo));
  
  // this is an exception, only for neighbor diagnostics (entry postion in table is always static)
  g_ucDllNeighDiagNo = ucTmp;
      
  for ( const DLL_SMIB_ENTRY_CHANNEL * pEntry = c_aDefChannels; pEntry < (c_aDefChannels + DEFAULT_CH_NO); pEntry++)
  {
      stTwoIdxSMIB.m_unUID = *(uint16*)pEntry;
#pragma diag_suppress=Pa039
      memcpy(&stTwoIdxSMIB.m_stEntry, pEntry, sizeof(DLL_SMIB_ENTRY_CHANNEL));  
#pragma diag_default=Pa039
      DLME_SetSMIBRequest( DL_CH, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue 
  }
  
  for ( const DLL_SMIB_ENTRY_TIMESLOT * pEntry = c_aDefTemplates; pEntry < c_aDefTemplates + DEFAULT_TS_TEMPLATE_NO; pEntry++)
  {
      stTwoIdxSMIB.m_unUID = *(uint16*)pEntry;
#pragma diag_suppress=Pa039
      memcpy(&stTwoIdxSMIB.m_stEntry, pEntry, sizeof(DLL_SMIB_ENTRY_TIMESLOT)); 
#pragma diag_default=Pa039
      DLME_SetSMIBRequest( DL_TS_TEMPL, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue 
  }    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Mircea Vlasin
/// @brief  Initializes the device manager application process
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_Init()
{    
  UDO_Init();
  
#ifndef BACKBONE_SUPPORT
  if( g_ucJoinStatus != DEVICE_DISCOVERY )
  {
      if( (DEVICE_JOINED != g_ucJoinStatus) || g_unLastJoinCnt ) // not success joined or joined for a short period of time
      {
          //the last joining Router will be banned
          AddBannedAdvRouter(g_unDAUXRouterAddr, g_stDPO.m_unBannedTimeout);
      }
  }
  g_unLastJoinCnt = 0;
  g_stJoinRetryCntrl.m_unJoinTimeout = 16;
#endif
  
  setDllDefaultSMIBS();
  
  g_unSysMngContractID = INVALID_CONTRACTID;
  g_ucJoinStatus = DEVICE_DISCOVERY;
  
  g_unRadioSleepReqId = 0xFFFF; //Radio Sleep is active
  g_unJoinCommandReqId = JOIN_CMD_WAIT_REQ_ID;
  
  DPO_Init();
    
#if defined(ROUTING_SUPPORT) || defined(BACKBONE_SUPPORT)  
  g_ucDevInfoEntryNo = 0;
#endif 
  
  DMO_Init();
  CO_Init();
  ARMO_Init();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei, Mircea Vlasin
/// @brief  Main DMAP task 
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_Task()
{  
  if ( g_ucJoinStatus != DEVICE_JOINED )
  {
      DMAP_joinStateMachine();      
  }
  
  // check if there is an incoming APDU that has to be processed by DMAP
  APDU_IDTF stAPDUIdtf;
  const uint8 * pAPDUStart = ASLDE_GetMyAPDU( UAP_DMAP_ID, &stAPDUIdtf );
  
  if ( pAPDUStart )
  {
      GENERIC_ASL_SRVC  stGenSrvc;
      const uint8 *     pNext = pAPDUStart;
      
      if( (stAPDUIdtf.m_ucPriorityAndFlags & TLDE_ECN_MASK) == TLDE_ECN_MASK)
      {
          DMO_NotifyCongestion( INCOMING_DATA_ECN, &stAPDUIdtf); 
      }
        
      
#ifdef DLME_QUEUE_PROTECTION_ENABLED
      g_ucDLMEQueueCmd = 0;
#endif  
      while ( pNext = ASLSRVC_GetGenericObject( pNext,
                                                stAPDUIdtf.m_unDataLen - (pNext - pAPDUStart),
                                                &stGenSrvc,
                                                stAPDUIdtf.m_aucAddr)
              )
      {
          if( stGenSrvc.m_ucType == SRVC_EXEC_RSP ) // allows execute responses only from any state
          {
              DMAP_processExecuteResponse( &stGenSrvc.m_stSRVC.m_stExecRsp, &stAPDUIdtf);
          }
          else if( stAPDUIdtf.m_ucAddrLen == 16 || g_stFilterTargetID == 1 ) // was validated by TL security layer
          {
              if( g_ucJoinStatus >= DEVICE_SEND_SEC_CONFIRM_REQ || g_stFilterTargetID == 1 ) // security is place
              {
                  switch ( stGenSrvc.m_ucType )
                  {
                  case SRVC_READ_REQ  : DMAP_processReadRequest(  &stGenSrvc.m_stSRVC.m_stReadReq, &stAPDUIdtf );   break;
                  case SRVC_WRITE_REQ : DMAP_processWriteRequest( &stGenSrvc.m_stSRVC.m_stWriteReq, &stAPDUIdtf );  break;
                  case SRVC_EXEC_REQ  : DMAP_processExecuteRequest( &stGenSrvc.m_stSRVC.m_stExecReq, &stAPDUIdtf ); break;
                  case SRVC_READ_RSP  : DMAP_processReadResponse( &stGenSrvc.m_stSRVC.m_stReadRsp, &stAPDUIdtf) ;   break;          
                  case SRVC_WRITE_RSP : DMAP_processWriteResponse( &stGenSrvc.m_stSRVC.m_stWriteRsp );              break;          
                  case SRVC_ALERT_ACK : ARMO_ProcessAlertAck(stGenSrvc.m_stSRVC.m_stAlertAck.m_ucAlertID);          break;
                  }
              } 
          }
          else // was not validated by TL security layer, allows proxy methods only 
          {
              if( stGenSrvc.m_ucType == SRVC_EXEC_REQ )
              {
                  if( stGenSrvc.m_stSRVC.m_stExecReq.m_unDstOID == DMAP_DMO_OBJ_ID 
                      && stGenSrvc.m_stSRVC.m_stExecReq.m_ucMethID >= DMO_PROXY_SM_JOIN_REQ
                      && stGenSrvc.m_stSRVC.m_stExecReq.m_ucMethID <= DMO_PROXY_SEC_SYM_REQ 
                     )
                  {
                      DMAP_processExecuteRequest( &stGenSrvc.m_stSRVC.m_stExecReq, &stAPDUIdtf );
                  }
              } 
          }
      }
#ifdef DLME_QUEUE_PROTECTION_ENABLED // protect the DLME internal queue 
      if( g_ucDLMEQueueCmd )
      {
          if( !g_ucDLMEQueueBusyFlag )
          {
              g_ucDLMEQueueBusyFlag = 1;
              ASLDE_MarkForDeleteAPDU( (uint8*)pAPDUStart ); // todo: check if always applicable       
          }
      }
      else
#endif  
      {
          ASLDE_MarkForDeleteAPDU( (uint8*)pAPDUStart ); // todo: check if always applicable       
      }
  }
  
  CO_ConcentratorTask();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Generic function for writing an attribute
/// @param  p_ucTSAPID - SAP ID of the process
/// @param  p_unObjID - object identifier
/// @param  p_pIdtf - attribute identifier structure
/// @param  p_pBuf - buffer containing attribute data
/// @param  p_unLen - buffer size
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 SetGenericAttribute( uint8       p_ucTSAPID,
                           uint16      p_unObjID,
                           ATTR_IDTF * p_pIdtf,
                           const uint8 *     p_pBuf,
                           uint16      p_unLen)
{    
    uint8 ucResult = SFC_INVALID_OBJ_ID;
    
    if( UAP_DMAP_ID == p_ucTSAPID )
    {    
        switch( p_unObjID )
        {
        case DMAP_NLMO_OBJ_ID:  return NLMO_Write( p_pIdtf->m_unAttrID, p_unLen, p_pBuf );
        case DMAP_UDO_OBJ_ID:   return UDO_Write( p_pIdtf->m_unAttrID & 0x003F, p_unLen, p_pBuf );
        case DMAP_DLMO_OBJ_ID:  return DLMO_Write( p_pIdtf, &p_unLen, p_pBuf );
        case DMAP_DSMO_OBJ_ID:  return DSMO_Write( p_pIdtf->m_unAttrID, p_unLen, p_pBuf );
        case DMAP_ASLMO_OBJ_ID: return ASLMO_Write(p_pIdtf->m_unAttrID, p_unLen, p_pBuf);               

        case DMAP_DMO_OBJ_ID:   ucResult = DMO_Write( p_pIdtf->m_unAttrID, p_unLen, p_pBuf );                                                                                                 
                                if( ucResult == SFC_SUCCESS )
                                {
                                    DPO_SavePersistentData();
                                }
                                break;
                                
        case DMAP_ARMO_OBJ_ID:  ucResult = ARMO_Write( p_pIdtf->m_unAttrID, p_unLen, p_pBuf );
                                if( ucResult == SFC_SUCCESS )
                                {
                                    DPO_SavePersistentData();
                                }
                                break;
        
        case DMAP_DPO_OBJ_ID:   
              if( DPO_CkAccess() )
              {
                  ucResult = DPO_Write( p_pIdtf->m_unAttrID, p_unLen, p_pBuf ); 

                  if( ucResult == SFC_SUCCESS )
                  {
                      DPO_SavePersistentData();
                  }
              }
              else
              {
                  ucResult = SFC_OBJECT_ACCESS_DENIED;
              }
              break;
              
        case DMAP_HRCO_OBJ_ID:
            g_pstCrtCO = AddConcentratorObject(p_unObjID, UAP_DMAP_ID);
            if( g_pstCrtCO )
            {
                //the appropriate CO element already exist or was just added - update/set the object attributes  
                //g_pstCrtCO will be used inside CO_Write!!!!!
                return CO_Write(p_pIdtf->m_unAttrID, p_unLen, p_pBuf);
            }
            break;
        
        
        case DMAP_TLMO_OBJ_ID:               
            if (p_unLen < 0xFF)
            {
                return TLME_SetRow( p_pIdtf->m_unAttrID,
                                     0,                   // no TAI cutover
                                     p_pBuf,
                                     (uint8)p_unLen); 
            }
            break;
           
//            case DMAP_NLMO_OBJ_ID:     break;
        }
    }

    return ucResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin 
/// @brief  Generic function for reading an attribute
/// @param  p_ucTSAPID - SAP ID of the process
/// @param  p_unObjID - object identifier
/// @param  p_pIdtf - attribute identifier structure
/// @param  p_pBuf - output buffer containing attribute data
/// @param  p_punLen - buffer size
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 GetGenericAttribute( uint8       p_ucTSAPID,
                           uint16      p_unObjID,
                           ATTR_IDTF * p_pIdtf,
                           uint8 *     p_pBuf,
                           uint16 *    p_punLen)
{    
    if( UAP_DMAP_ID == p_ucTSAPID )
    {
        switch( p_unObjID )
        {
        case DMAP_NLMO_OBJ_ID:  return NLMO_Read( p_pIdtf->m_unAttrID, p_punLen, p_pBuf );  
        case DMAP_UDO_OBJ_ID:   return UDO_Read( p_pIdtf->m_unAttrID, p_punLen, p_pBuf ); 
        case DMAP_DLMO_OBJ_ID:  return DLMO_Read( p_pIdtf, p_punLen, p_pBuf );                      
        case DMAP_DMO_OBJ_ID:   return DMO_Read( p_pIdtf, p_punLen, p_pBuf );
        case DMAP_DSMO_OBJ_ID:  return DSMO_Read( p_pIdtf->m_unAttrID, p_punLen, p_pBuf );
        case DMAP_ARMO_OBJ_ID:  return ARMO_Read( p_pIdtf->m_unAttrID, p_punLen, p_pBuf );
        case DMAP_DPO_OBJ_ID:   return DPO_Read( p_pIdtf->m_unAttrID, p_punLen, p_pBuf );                
        case DMAP_ASLMO_OBJ_ID: return ASLMO_Read( p_pIdtf->m_unAttrID, p_punLen, p_pBuf );  
        
        case DMAP_HRCO_OBJ_ID:
            g_pstCrtCO = FindConcentratorByObjId(p_unObjID, UAP_DMAP_ID);
            if( g_pstCrtCO )
            {
                return CO_Read(p_pIdtf->m_unAttrID, p_punLen, p_pBuf );
            }
            break;        
            
        
        case DMAP_TLMO_OBJ_ID  : 
              *p_punLen = 0;
              return  TLME_GetRow( p_pIdtf->m_unAttrID, 
                                   NULL,        // no indexed attributes in TLMO
                                   0,           // no indexed attributes in TLMO
                                   p_pBuf,
                                   (uint8*)p_punLen );
             
        }
    }  
             
    *p_punLen = 0;
    return SFC_INVALID_OBJ_ID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin 
/// @brief  Generic function for method execution
/// @param  p_pExecReq - pointer to Execute Request data structure
/// @param  p_pIdtf - pointer to attribute identifier structure
/// @param  p_pExecRsp - pointer to Execute Response data structure
/// @return None
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_ExecuteGenericMethod(  EXEC_REQ_SRVC * p_pExecReq,
                                 APDU_IDTF *     p_pIdtf,
                                 EXEC_RSP_SRVC * p_pExecRsp )
{
    p_pExecRsp->m_unLen = 0;
        
    switch( p_pExecReq->m_unDstOID )
    {
        case DMAP_UDO_OBJ_ID:
            UDO_ProcessExecuteRequest( p_pExecReq, p_pExecRsp );
            break;
        
        case DMAP_DLMO_OBJ_ID:
            p_pExecRsp->m_ucSFC = DLMO_Execute( p_pExecReq->m_ucMethID,
                                                p_pExecReq->m_unLen,
                                                p_pExecReq->p_pReqData,
                                                &p_pExecRsp->m_unLen,
                                                p_pExecRsp->p_pRspData);      
            break;
        
        case DMAP_TLMO_OBJ_ID:
            p_pExecRsp->m_ucSFC = TLMO_execute( p_pExecReq->m_ucMethID,
                                                p_pExecReq->m_unLen,
                                                p_pExecReq->p_pReqData,
                                                &p_pExecRsp->m_unLen,
                                                p_pExecRsp->p_pRspData);
            break; 
        
        case DMAP_NLMO_OBJ_ID:
            p_pExecRsp->m_ucSFC = NLMO_Execute( p_pExecReq->m_ucMethID,
                                                p_pExecReq->m_unLen,
                                                p_pExecReq->p_pReqData,
                                                &p_pExecRsp->m_unLen,
                                                p_pExecRsp->p_pRspData);
            break; 
        
        case DMAP_DMO_OBJ_ID:
            switch( p_pExecReq->m_ucMethID )
            {
                case DMO_PROXY_SM_JOIN_REQ: 
                case DMO_PROXY_SM_CONTR_REQ:
                case DMO_PROXY_SEC_SYM_REQ:
                  
                    if ( p_pIdtf ) // just a protection if this function is called incorrectly
                    {
                        p_pExecRsp->m_ucSFC = DMAP_DMO_forwardJoinReq( p_pExecReq, p_pIdtf );
                        if( SFC_SUCCESS != p_pExecRsp->m_ucSFC )
                        {
                            ASLSRVC_AddJoinObject( p_pExecRsp, SRVC_EXEC_RSP, p_pIdtf->m_aucAddr ); 
                        }
                        return; // don't send a response yet 
                    }
                    // not break here
                default:
                    p_pExecRsp->m_ucSFC = DMO_Execute( p_pExecReq->m_ucMethID,
                                                       p_pExecReq->m_unLen,
                                                       p_pExecReq->p_pReqData,
                                                       &p_pExecRsp->m_unLen,
                                                       p_pExecRsp->p_pRspData);
            }
            break;
            
        case DMAP_DSMO_OBJ_ID:
            DSMO_Execute( p_pExecReq, p_pExecRsp );
            break;
            
        case DMAP_DPO_OBJ_ID:     
            p_pExecRsp->m_ucSFC = DPO_Execute(  p_pExecReq->m_ucMethID,
                                                p_pExecReq->m_unLen, 
                                                p_pExecReq->p_pReqData,
                                                &p_pExecRsp->m_unLen,
                                                p_pExecRsp->p_pRspData );
            break;            
            
        case DMAP_ARMO_OBJ_ID:    
            p_pExecRsp->m_ucSFC = ARMO_Execute( p_pExecReq->m_ucMethID,
                                                p_pExecReq->m_unLen, 
                                                p_pExecReq->p_pReqData,
                                                &p_pExecRsp->m_unLen,
                                                p_pExecRsp->p_pRspData);
            break;
            
        case DMAP_ASLMO_OBJ_ID:
            p_pExecRsp->m_ucSFC = ASLMO_Execute( p_pExecReq->m_ucMethID,
                                                 p_pExecReq->m_unLen, 
                                                 p_pExecReq->p_pReqData,
                                                 &p_pExecRsp->m_unLen,
                                                 p_pExecRsp->p_pRspData);
            break;
            
        default: 
            p_pExecRsp->m_ucSFC = SFC_INVALID_OBJ_ID;
            break;  
    }    
}
                                 
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Implements the join state machine
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_joinStateMachine(void)
{
    switch ( g_ucJoinStatus )
    {      
    case DEVICE_DISCOVERY:    
    #ifdef BACKBONE_SUPPORT
        if( (g_ucProvisioned == PROV_STATE_PROVISIONED) && (MLSM_GetCrtTaiSec() > 1576800000) )  // make sure the time was synchronized (aprox. at least 1stJanuary2008)         
        {    
            NLMO_prepareJoinSMIBS();
            DMAP_startJoin();
        }
    #endif
        break;
        
#ifndef BACKBONE_SUPPORT // regular device (not BBR) 
        
    case DEVICE_RECEIVED_ADVERTISE:       
        DMAP_DLMO_prepareSMIBS();  
        g_ucJoinStatus = DEVICE_FIND_ROUTER_EUI64;
        break;            
        
    case DEVICE_FIND_ROUTER_EUI64:
        if (MLSM_GetCrtTaiSec() > g_ulDAUXTimeSync ) 
        {
            uint8 aucDstAddr[4] = {g_unDAUXRouterAddr >> 8, g_unDAUXRouterAddr, 0, 0}; // do not have src nickname yet
            DLDE_Data_Request( 4,
                               aucDstAddr,
                               0x0F,  // manually force high priority
                               JOIN_CONTRACT_ID,
                               0,
                               NULL,
                               DLL_NEXT_HOP_HANDLE );
            g_ucJoinStatus = DEVICE_WAIT_ROUTER_EUI64;
            
            // compute the maximum TAI sec, until router EUI64 addres should be discoverd 
            // store it ing_ulDAUXTimeSync; consider the AdvertiseSFLength and DLL retry also
            // wait for 2 full superfarmes + random() % 64
            DSMO_SetJoinTimeout(((g_stDAUXSuperframe.m_unSfPeriod / 100) + 1 ) * 2 + (g_unRandValue & 0x3F));             
        }
        break;
    
    case DEVICE_WAIT_ROUTER_EUI64:          
        if( SFC_SUCCESS == NLMO_prepareJoinSMIBS() )     
        {
            DMAP_startJoin();
            break;
        }
//      not break here
        
#endif // BACKBONE_SUPPORT
    case DEVICE_SECURITY_JOIN_REQ_SENT:
    case DEVICE_SM_JOIN_REQ_SENT:
    case DEVICE_SM_CONTR_REQ_SENT: 
        if (!g_stJoinRetryCntrl.m_unJoinTimeout) 
        {
            DMAP_ResetStack(1);
        }
        break; 
        
    case DEVICE_SEND_SM_JOIN_REQ:
    {
        //retry until the sufficient space for request
        //send the SM Join Request
        EXEC_REQ_SRVC stExecReq;
        uint8 aucReqParams[MAX_APDU_SIZE];
        
        stExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
        stExecReq.m_unDstOID = DMAP_DMO_OBJ_ID;
        stExecReq.m_ucMethID = DMO_PROXY_SM_JOIN_REQ;  
        stExecReq.p_pReqData = aucReqParams;
        stExecReq.m_unLen    = DMO_generateSMJoinReqBuffer( stExecReq.p_pReqData );    
        
        if( SFC_SUCCESS == ASLSRVC_AddJoinObject(  &stExecReq, SRVC_EXEC_REQ, NULL ) )
        {
            //for each Request from join process need to update the timeout 
            DLL_MIB_ADV_JOIN_INFO stJoinInfo;  
            DLME_GetMIBRequest(DL_ADV_JOIN_INFO, &stJoinInfo);
            DSMO_SetJoinTimeout( 1 << (stJoinInfo.m_mfDllJoinBackTimeout & 0x0F));
            g_ucJoinStatus = DEVICE_SM_JOIN_REQ_SENT;
        }
        break;
    }    
    
    case DEVICE_SEND_SM_CONTR_REQ:
    {
        //retry until the sufficient space for request
        //send the SM Join Contract Request
        EXEC_REQ_SRVC stExecReq;
            
        stExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
        stExecReq.m_unDstOID = DMAP_DMO_OBJ_ID;
        stExecReq.m_ucMethID = DMO_PROXY_SM_CONTR_REQ;  
        stExecReq.p_pReqData = c_oEUI64BE;  //just the EUI64 of the device needed
        stExecReq.m_unLen    = 8;    
    
        if( SFC_SUCCESS == ASLSRVC_AddJoinObject(  &stExecReq, SRVC_EXEC_REQ, NULL ) )
        {
            //for each Request from join process need to update the timeout 
            DLL_MIB_ADV_JOIN_INFO stJoinInfo;  
            DLME_GetMIBRequest(DL_ADV_JOIN_INFO, &stJoinInfo);
            DSMO_SetJoinTimeout( 1 << (stJoinInfo.m_mfDllJoinBackTimeout & 0x0F) );
            g_ucJoinStatus = DEVICE_SM_CONTR_REQ_SENT;
        }
        break;
    }    
        
            
    case DEVICE_SEND_SEC_CONFIRM_REQ:
    {
        //retry until the sufficient space for request
        //send the SM Join Security Confirm Request
        uint8 aucTemp[48];
        
        //prepare the request buffer
        memcpy(aucTemp, g_aulNewNodeChallenge, 16);
        memcpy(aucTemp + 16, g_aucSecMngChallenge, 16);
        memcpy(aucTemp + 32, c_oEUI64BE, 8);
        memcpy(aucTemp + 40, c_oSecManagerEUI64BE, 8);
            
        Keyed_Hash_MAC(g_aJoinAppKey, aucTemp, sizeof(aucTemp));
        
        EXEC_REQ_SRVC stExecReq;
        stExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
        stExecReq.m_unDstOID = SM_PSMO_OBJ_ID;
        stExecReq.m_ucMethID = PSMO_SECURITY_JOIN_CONF;  
        stExecReq.p_pReqData = aucTemp;
        stExecReq.m_unLen    = 16;    
    
        if( SFC_SUCCESS == ASLSRVC_AddGenericObjectToSM(  &stExecReq, SRVC_EXEC_REQ ) )
        {
            //for each Request from join process need to update the timeout 
            DLL_MIB_ADV_JOIN_INFO stJoinInfo;  
            DLME_GetMIBRequest(DL_ADV_JOIN_INFO, &stJoinInfo);
            DSMO_SetJoinTimeout( 1 << (stJoinInfo.m_mfDllJoinBackTimeout & 0x0F) );
            g_ucJoinStatus = DEVICE_SEC_CONFIRM_REQ_SENT;
        }
        break;
    }    
    
    case DEVICE_SEC_CONFIRM_REQ_SENT:
        if (!g_stJoinRetryCntrl.m_unJoinTimeout)
        {
            DMAP_ResetStack(2);
        }
        break;
    } 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Processes a read request in the application queue and passes it to the target object 
/// @param  p_pReadReq - read request structure
/// @param  p_pIdtf - apdu structure
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processReadRequest( READ_REQ_SRVC * p_pReadReq,
                              APDU_IDTF *     p_pIdtf)

{
  READ_RSP_SRVC stReadRsp;
  uint8         aucRspBuff[MAX_APDU_SIZE]; 
  
  stReadRsp.m_unDstOID = p_pReadReq->m_unSrcOID;
  stReadRsp.m_unSrcOID = p_pReadReq->m_unDstOID;
  stReadRsp.m_ucReqID  = p_pReadReq->m_ucReqID;
  
  stReadRsp.m_pRspData = aucRspBuff;
  stReadRsp.m_unLen = sizeof(aucRspBuff); // inform following functions about maximum available buffer size;
  
  // check the ECN of the request
  if ((p_pIdtf->m_ucPriorityAndFlags & TLDE_ECN_MASK) == TLDE_ECN_MASK)
  {
      stReadRsp.m_ucFECCE = 1;
  }
    
  stReadRsp.m_ucSFC = GetGenericAttribute( UAP_DMAP_ID,
                                           p_pReadReq->m_unDstOID,    
                                           &p_pReadReq->m_stAttrIdtf, 
                                           stReadRsp.m_pRspData, 
                                           &stReadRsp.m_unLen );

  #if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)   // is BBR MC13225
  if( stReadRsp.m_ucSFC == SFC_NACK ) return;                          // parsed by AN
  #endif
  
  ASLSRVC_AddGenericObjectToSM( &stReadRsp, SRVC_READ_RSP );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei, Mircea Vlasin
/// @brief  Processes a write request in the application queue and passes it to the target object 
/// @param  p_pWriteReq - write request structure
/// @param  p_pIdtf - apdu structure
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processWriteRequest( WRITE_REQ_SRVC * p_pWriteReq,
                               APDU_IDTF *      p_pIdtf)
{
  WRITE_RSP_SRVC stWriteRsp;  
  
  stWriteRsp.m_unDstOID = p_pWriteReq->m_unSrcOID;
  stWriteRsp.m_unSrcOID = p_pWriteReq->m_unDstOID;
  stWriteRsp.m_ucReqID  = p_pWriteReq->m_ucReqID;
  
  // check the ECN of the request
  if ((p_pIdtf->m_ucPriorityAndFlags & TLDE_ECN_MASK) == TLDE_ECN_MASK)
  {
      stWriteRsp.m_ucFECCE = 1;
  }
  
  stWriteRsp.m_ucSFC = SetGenericAttribute( UAP_DMAP_ID,
                                            p_pWriteReq->m_unDstOID,
                                            &p_pWriteReq->m_stAttrIdtf,
                                            p_pWriteReq->p_pReqData,
                                            p_pWriteReq->m_unLen );                 

  //exception for RadioSleep - DL_RADIO_SLEEP = 23
  //RadioSleep will be activated later after transmission confirm
  if( DL_RADIO_SLEEP == p_pWriteReq->m_stAttrIdtf.m_unAttrID && 
      DMAP_DLMO_OBJ_ID == p_pWriteReq->m_unDstOID )
  {
      g_unRadioSleepReqId = p_pWriteReq->m_ucReqID;
      g_ulRadioSleepCounter = 0;
  }
  
  //exception for JoinCommand
  if( DMO_JOIN_COMMAND == p_pWriteReq->m_stAttrIdtf.m_unAttrID && 
      DMAP_DMO_OBJ_ID == p_pWriteReq->m_unDstOID &&
      JOIN_CMD_WAIT_REQ_ID == g_unJoinCommandReqId )
  {
      g_unJoinCommandReqId = p_pWriteReq->m_ucReqID;
  }
  
  #if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)   // is BBR MC13225
  if( stWriteRsp.m_ucSFC == SFC_NACK ) return;                          // parsed by AN
  #endif
  
  ASLSRVC_AddGenericObjectToSM( &stWriteRsp, SRVC_WRITE_RSP );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Processes an execute request in the application queue and passes it to the target object 
/// @param  p_pExecReq - execute request structure
/// @param  p_pIdtf - apdu structure
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processExecuteRequest( EXEC_REQ_SRVC * p_pExecReq,
                                 APDU_IDTF *     p_pIdtf )
{
    EXEC_RSP_SRVC stExecRsp;
    uint8         aucRsp[MAX_APDU_SIZE]; 
    
    stExecRsp.p_pRspData = aucRsp;
    stExecRsp.m_unDstOID = p_pExecReq->m_unSrcOID;
    stExecRsp.m_unSrcOID = p_pExecReq->m_unDstOID;
    stExecRsp.m_ucReqID  = p_pExecReq->m_ucReqID;
    
    // check the ECN of the request
    if ((p_pIdtf->m_ucPriorityAndFlags & TLDE_ECN_MASK) == TLDE_ECN_MASK)
    {
        stExecRsp.m_ucFECCE = 1;
    }
    
    DMAP_ExecuteGenericMethod( p_pExecReq, p_pIdtf, &stExecRsp );                                 

    #if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)   // is BBR MC13225
    if( stExecRsp.m_ucSFC == SFC_NACK ) return;                          // parsed by AN
    #endif

    ASLSRVC_AddGenericObjectToSM( &stExecRsp, SRVC_EXEC_RSP );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Processes a read response service
/// @param  p_pReadRsp - pointer to the read response structure
/// @param  p_pAPDUIdtf - pointer to the appropriate request APDU
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processReadResponse(READ_RSP_SRVC * p_pReadRsp, APDU_IDTF * p_pAPDUIdtf)
{
    uint16            unTxAPDULen;
    GENERIC_ASL_SRVC  stGenSrvc;
    
    const uint8 * pTxAPDU = ASLDE_SearchOriginalRequest( p_pReadRsp->m_ucReqID,                                                         
                                                         p_pReadRsp->m_unSrcOID,
                                                         p_pReadRsp->m_unDstOID,
                                                         p_pAPDUIdtf,                                                         
                                                         &unTxAPDULen);      
    if( NULL == pTxAPDU )
        return;
    
    if( NULL == ASLSRVC_GetGenericObject( pTxAPDU, unTxAPDULen, &stGenSrvc , NULL) )
        return;
    
    if( SRVC_READ_REQ != stGenSrvc.m_ucType )
        return;
    
    if( stGenSrvc.m_stSRVC.m_stReadReq.m_unDstOID != p_pReadRsp->m_unSrcOID )
        return; 
    
    if (p_pReadRsp->m_ucFECCE)
    {
        DMO_NotifyCongestion( OUTGOING_DATA_ECN, p_pAPDUIdtf ); 
    }
    
    if( SFC_SUCCESS == p_pReadRsp->m_ucSFC && SM_STSO_OBJ_ID == p_pReadRsp->m_unSrcOID )
    {
    
        switch( stGenSrvc.m_stSRVC.m_stReadReq.m_stAttrIdtf.m_unAttrID )
        {
#ifdef BACKBONE_SUPPORT
            case STSO_CTR_UTC_OFFSET:
                if( sizeof(g_stDPO.m_nCurrentUTCAdjustment) == p_pReadRsp->m_unLen )   
                {
                    g_stDPO.m_nCurrentUTCAdjustment = ((uint16)p_pReadRsp->m_pRspData[0] << 8) | p_pReadRsp->m_pRspData[1];
                }    
                break;
            case STSO_NEXT_UTC_TIME:
                if( sizeof(g_stDMO.m_ulNextDriftTAI) == p_pReadRsp->m_unLen )
                {
                    g_stDMO.m_ulNextDriftTAI = ((uint32)*p_pReadRsp->m_pRspData << 24) |
                                               ((uint32)*(p_pReadRsp->m_pRspData+1) << 16) | 
                                               ((uint32)*(p_pReadRsp->m_pRspData+2) << 8) | 
                                               *(p_pReadRsp->m_pRspData+3);
                }
                break;
            case STSO_NEXT_UTC_OFFSET:
                if( sizeof(g_stDMO.m_unNextUTCDrift) == p_pReadRsp->m_unLen )
                {
                    g_stDMO.m_unNextUTCDrift = ((uint16)p_pReadRsp->m_pRspData[0] << 8) | p_pReadRsp->m_pRspData[1];
                }
                break;
#endif  //#ifdef BACKBONE_SUPPORT
            default:;
        }
    }
    else
    {
        //update for other desired attributes
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Processes a write response service
/// @param  p_pWriteRsp - pointer to the write response structure
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processWriteResponse(WRITE_RSP_SRVC * p_pWriteRsp)
{
  // no write request is issued yet by DMAP  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, 
/// @brief  Processes an device management object response
/// @param  p_unSrcOID - source object identifier
/// @param  p_ucMethID - method identifier
/// @param  p_pExecRsp - pointer to the execute response structure
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processDMOResponse(uint16 p_unSrcOID, uint8 p_ucMethID, EXEC_RSP_SRVC* p_pExecRsp)
{
  switch(p_unSrcOID)
  {
    case DMAP_DMO_OBJ_ID:
      {
          if( DMO_PROXY_SEC_SYM_REQ == p_ucMethID && DEVICE_SECURITY_JOIN_REQ_SENT == g_ucJoinStatus )
          {
            //backbone or device Security Join Response
            DMO_checkSecJoinResponse(p_pExecRsp);
            break;
          }  
          if( DMO_PROXY_SM_JOIN_REQ == p_ucMethID && DEVICE_SM_JOIN_REQ_SENT == g_ucJoinStatus )
          {
            //backbone or device SM Join Response
            DMO_checkSMJoinResponse(p_pExecRsp);
            break;
          }
          if( DMO_PROXY_SM_CONTR_REQ == p_ucMethID && DEVICE_SM_CONTR_REQ_SENT == g_ucJoinStatus )
          {
            //backbone or device SM Contract Response
            if( SFC_SUCCESS == DMO_ProcessFirstContract(p_pExecRsp) )
            {
                //the contract response was successfully processed
                if( SFC_SUCCESS != NLMO_updateJoinSMIBS() )
                {
                    DMAP_ResetStack(3);
                }
                else
                {
#ifndef BACKBONE_SUPPORT  
                    //the next requests will be sent using short address
                    g_stDMO.m_unShortAddr = s_unDevShortAddr;                    
#endif  
                    
                    //contract and session key available - update the TL security level
                    uint8 ucTmp;
                    if( SLME_FindTxKey(g_stDMO.m_aucSysMng128BitAddr, (uint8)UAP_SMAP_ID & 0x0F, &ucTmp ) )  // have key to SM's UAP
                    {
                        g_stDSMO.m_ucTLSecurityLevel = SECURITY_ENC_MIC_32;
                    }
                    
                    g_ucJoinStatus = DEVICE_SEND_SEC_CONFIRM_REQ;
                }
            }
            break;
          }
      }
      break;
    
    case SM_PSMO_OBJ_ID:
      {
          if( PSMO_SECURITY_JOIN_REQ == p_ucMethID )
          {
            //proxy router Security Join Response
            DMAP_DMO_forwardJoinResp(p_pExecRsp);
            break;
          }
          if( PSMO_SECURITY_JOIN_CONF == p_ucMethID && DEVICE_SEC_CONFIRM_REQ_SENT == g_ucJoinStatus )
          {
            //backbone or device Security Join Confirm Response
            DMAP_DMO_applyJoinStatus(p_pExecRsp->m_ucSFC);
            break;
          }
      }
      break;
      
    case SM_DMSO_OBJ_ID:
      {
          if((DMSO_JOIN_REQUEST == p_ucMethID) ||
             (DMSO_CONTRACT_REQUEST == p_ucMethID))
          {
            //proxy router SM Join Response or SM Contract Request
            DMAP_DMO_forwardJoinResp(p_pExecRsp);            
            break;
          }
      }
      break;
      
    case SM_SCO_OBJ_ID:
      {
        if(SCO_REQ_CONTRACT == p_ucMethID)
        {
          //process contract response
          DMO_ProcessContractResponse(p_pExecRsp);
          break;
        }
      }
      break;
      
    default:
      break;
      
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei, Mircea Vlasin
/// @brief  Processes an execute response in the application queue and passes it to the target object 
/// @param  p_pExecRsp - pointer to the execute response structure
/// @param  p_pAPDUIdtf - pointer to the appropriate request APDU
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_processExecuteResponse( EXEC_RSP_SRVC * p_pExecRsp,
                                  APDU_IDTF *     p_pAPDUIdtf )
{  
    uint16            unTxAPDULen;
    GENERIC_ASL_SRVC  stGenSrvc;
    
    const uint8 * pTxAPDU = ASLDE_SearchOriginalRequest( p_pExecRsp->m_ucReqID,                                                         
                                                         p_pExecRsp->m_unSrcOID,
                                                         p_pExecRsp->m_unDstOID,
                                                         p_pAPDUIdtf,                                                         
                                                         &unTxAPDULen);      
    if( NULL == pTxAPDU )
        return;
    
    if( NULL == ASLSRVC_GetGenericObject( pTxAPDU, unTxAPDULen, &stGenSrvc, NULL ) )
        return;
    
    if( SRVC_EXEC_REQ != stGenSrvc.m_ucType )
        return;
    
    if( stGenSrvc.m_stSRVC.m_stExecReq.m_unDstOID != p_pExecRsp->m_unSrcOID )
        return; 
    
    if (p_pExecRsp->m_ucFECCE)
    {
        DMO_NotifyCongestion( OUTGOING_DATA_ECN, p_pAPDUIdtf ); 
    }
    
    switch( p_pExecRsp->m_unDstOID )
    {
        case DMAP_DLMO_OBJ_ID  :
        case DMAP_TLMO_OBJ_ID  :
        case DMAP_NLMO_OBJ_ID  :
        case DMAP_ASLMO_OBJ_ID : break;
        
        case DMAP_DSMO_OBJ_ID  :
            if( PSMO_SEC_NEW_SESSION == stGenSrvc.m_stSRVC.m_stExecReq.m_ucMethID )
            {
                if( SFC_SUCCESS != p_pExecRsp->m_ucSFC )
                {
                    // no response APDU must be validated
                    // new seession key establishment failed; this endpoint has to be banned
                    DMO_NotifyBadEndpoint( stGenSrvc.m_stSRVC.m_stExecReq.p_pReqData + 18 );
                }
                else
                {
                    switch( DSMO_ValidateNewSessionKeyResponse(p_pExecRsp) )
                    {
                        case SFC_FAILURE:
                            // new seession key establishment failed; this endpoint has to be banned
                            DMO_NotifyBadEndpoint( stGenSrvc.m_stSRVC.m_stExecReq.p_pReqData + 18 );
                            // probably Key Update Fail alert should be triggered
                            // SLME_KeyFailReport();
                            break;
                        case SFC_SUCCESS:
                        default:
                            break;
                    }
                }
            }
            break;
        
        case DMAP_DMO_OBJ_ID   : 
          {
            DMAP_processDMOResponse(p_pExecRsp->m_unSrcOID, 
                                    stGenSrvc.m_stSRVC.m_stExecReq.m_ucMethID,   
                                    p_pExecRsp);
          }
          break;        
        default:
          break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Updates join status on a succesfull join-confirm-response
/// @param  p_ucSFC - service feedback code
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_DMO_applyJoinStatus( uint8 p_ucSFC )
{
    if ( p_ucSFC != SFC_SUCCESS )
    {      
        DMAP_ResetStack(4);   
        return;  
    }  
    
    //activate the RxAdv idle link
    MONITOR_ENTER();
    DLL_SMIB_ENTRY_LINK * pstLink = (DLL_SMIB_ENTRY_LINK*) DLME_FindSMIB( SMIB_IDX_DL_LINK, JOIN_ADV_RX_LINK_UID );
    
    if( pstLink && (pstLink->m_stLink.m_mfLinkType & DLL_MASK_LNKIDLE) )
    {
        pstLink->m_stLink.m_mfLinkType &= ~DLL_MASK_LNKIDLE; //activate the RxAdv link
        g_ucHashRefreshFlag = 1; // need to recompute the hash table
    }
    MONITOR_EXIT();
    
    g_ucJoinStatus = DEVICE_JOINED;
    MLDE_SetDiscoveryState(DEVICE_JOINED);
    UAP_OnJoin();
    
#ifndef BACKBONE_SUPPORT  
    g_unLastJoinCnt = g_stDPO.m_unBannedTimeout;

    //clear the banned router table
    g_ucBannedRoutNo = 0;    
    
    // generate discovery alert 60 seconds after join
    g_ucDiscoveryAlert    = DLL_DISCOVERY_ALERT_DEFAULT;
#endif
}

#ifndef BACKBONE_SUPPORT 
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin 
    /// @brief  Informs DMAP that dll has received advertise 
    /// @param  p_unRouterAddr - router address
    /// @param  p_pstTimeSyncInfo - pointer to time synchronization structure
    /// @return none
    /// @remarks
    ///      Access level: interrupt level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void DMAP_DLMO_IndicateDAUX(  uint16 p_unRouterAddr  )
    {     
      if( !DAQ_IsReady() && (g_stFilterTargetID != 1) ) // cannot join until receive DAQ confirmation
          return;
      
      uint8 ucIdx;
      for( ucIdx = 0; ucIdx < g_ucBannedRoutNo; ucIdx++ )
      {              
          if( g_astBannedRouters[ucIdx].m_unRouterAddr == p_unRouterAddr )
              return; //wait another advertisement
      }
            
      // store the router address into the candidates list
      if( g_stJoinRetryCntrl.m_unJoinTimeout ) // check best neighbor for a period of time only  
      {
          if (SFC_SUCCESS == MLSM_AddNewCandidate(p_unRouterAddr))  // may return also duplicate or list full
              return; // first advertise
      }
         
      // second advertise or advertise list full
      g_unDAUXRouterAddr = p_unRouterAddr;   
      g_ulDAUXTimeSync = g_ulDllTaiSeconds;
      g_stCandidates.m_ucCandidateNo = 0;       
      
      // Stop the discovery state of the MAC sub-layer
      MLDE_SetDiscoveryState(DEVICE_RECEIVED_ADVERTISE);
        
      g_ucJoinStatus = DEVICE_RECEIVED_ADVERTISE;
    }
  
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
    /// @brief  Adds the necessary SMIBS, based on the data received inside advertisement 
    /// @param  none
    /// @return none
    /// @remarks
    ///      Access level: user level\n
    ///      Context: used to configure the minimal communication support for the join process  
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    __monitor void DMAP_DLMO_prepareSMIBS(void)
    {
      DLMO_DOUBLE_IDX_SMIB_ENTRY stTwoIdxSMIB;
        
      MONITOR_ENTER();
                  
      //the provisioned DL_Config will be managed by the SM - will not be locally removed by the device
      //setDllDefaultSMIBS();
      
      // the Advertisement Superframe's UID not included inside Advertisement DAUX  
      g_stDAUXSuperframe.m_unUID = JOIN_SF;
      
      //the advertisement superframe priority will be 0 according with the standard 
      //set the highest priority for the advertisement superframe
      //g_stDAUXSuperframe.m_ucInfo |= 0x3C;  //15 - maximum priority

#pragma diag_suppress=Pa039
      
      // add the superframe to DLL SMIBs
      stTwoIdxSMIB.m_unUID = g_stDAUXSuperframe.m_unUID;
      memcpy(&stTwoIdxSMIB.m_stEntry,  &g_stDAUXSuperframe, sizeof(stTwoIdxSMIB.m_stEntry.m_stSmib.m_stSuperframe));
      DLME_SetSMIBRequest(DL_SUPERFRAME, g_ulDAUXTimeSync, &stTwoIdxSMIB);     
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue
      
      memset( &stTwoIdxSMIB.m_stEntry.m_stSmib.m_stGraph, 0, sizeof(stTwoIdxSMIB.m_stEntry.m_stSmib.m_stGraph) );
      // create an outbound graph for transmiting the Join Requests messages 
      stTwoIdxSMIB.m_unUID = JOIN_GRAPH_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stGraph.m_unUID = JOIN_GRAPH_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stGraph.m_ucNeighCount = 1;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stGraph.m_ucQueue = 1 << DLL_ROT_GRNEICNT; //PreferredBranch = 0; Queue = 0 
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stGraph.m_aNeighbors[0] = g_unDAUXRouterAddr;
      DLME_SetSMIBRequest(DL_GRAPH, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue    
  
      memset( &stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute, 0, sizeof(stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute) );
      // create a default route per graph, to ensure that communication is possible with the advertising router;
      stTwoIdxSMIB.m_unUID = JOIN_ROUTE_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute.m_unUID      = JOIN_ROUTE_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute.m_ucInfo     = DLL_RTSEL_DEFAULT;        
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute.m_ucInfo     |= 1 << DLL_ROT_RTNO;  // one entry in route table
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute.m_ucFwdLimit = 16;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stRoute.m_aRoutes[0] = GRAPH_ID_PREFIX | JOIN_GRAPH_UID;      
      DLME_SetSMIBRequest(DL_ROUTE, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue
      
      
      memset( &stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink, 0, sizeof(stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink) );
      // create a tx link towards the advertising router
      stTwoIdxSMIB.m_unUID                                     = JOIN_REQ_TX_LINK_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unUID         = JOIN_REQ_TX_LINK_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unSfIdx       = g_stDAUXSuperframe.m_unUID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfLinkType    = DLL_MASK_LNKTX | DLL_MASK_LNKEXP | DLL_MASK_LNKADAPT;  
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfTypeOptions = 0x01 << DLL_ROT_LNKNEI;                // designates an address
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unNeighbor    = g_unDAUXRouterAddr;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unTemplate1   = DEFAULT_TX_TEMPL_UID;                           
      // add the join info
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfTypeOptions |= ((g_stDllAdvJoinInfo.m_mfDllJoinLinksType & DLL_MASK_JOIN_TX_SCHEDULE) >> DLL_ROT_JOIN_TX_SCHD) << DLL_ROT_LNKSC;
      memcpy(&stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_aSchedule, 
             &g_stDllAdvJoinInfo.m_stDllJoinTx, 
             sizeof(DLL_SCHEDULE));
      
      DLME_SetSMIBRequest(DL_LINK, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue
      
      // create an rx link from the advertising router
      // TAI cutover remains the same
      stTwoIdxSMIB.m_unUID                                   = JOIN_RESP_RX_LINK_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unUID       = JOIN_RESP_RX_LINK_UID;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfLinkType  = DLL_MASK_LNKRX | DLL_MASK_LNKADAPT;      // rx link + adaptive allowed
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unTemplate1 = DEFAULT_RX_TEMPL_UID;  
      // add the join info
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfTypeOptions = 
          ((g_stDllAdvJoinInfo.m_mfDllJoinLinksType & DLL_MASK_JOIN_RX_SCHEDULE) >> DLL_ROT_JOIN_RX_SCHD) << DLL_ROT_LNKSC;
      
      memcpy(&stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_aSchedule, 
             &g_stDllAdvJoinInfo.m_stDllJoinRx, 
             sizeof(DLL_SCHEDULE));
      
      DLME_SetSMIBRequest(DL_LINK, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue
    
      if( g_stDllAdvJoinInfo.m_mfDllJoinLinksType & DLL_MASK_ADV_RX_FLAG ) 
      {
          // create an rx link from the advertising router, link for receiving advertise
          // TAI cutover remains the same
          stTwoIdxSMIB.m_unUID                                   = JOIN_ADV_RX_LINK_UID;
          stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unUID       = JOIN_ADV_RX_LINK_UID;
          stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfLinkType  = DLL_MASK_LNKRX | DLL_MASK_LNKADAPT | DLL_MASK_LNKIDLE;     // rx link + adaptive allowed
          stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_unTemplate1 = DEFAULT_DISCOVERY_TEMPL_UID;
          // add the join info
          stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_mfTypeOptions = 
              ((g_stDllAdvJoinInfo.m_mfDllJoinLinksType & DLL_MASK_ADV_RX_SCHEDULE) >> DLL_ROT_ADV_RX_SCHD) << DLL_ROT_LNKSC;
          
          memcpy(&stTwoIdxSMIB.m_stEntry.m_stSmib.m_stLink.m_aSchedule, 
                 &g_stDllAdvJoinInfo.m_stDllJoinAdvRx, 
                 sizeof(DLL_SCHEDULE));
          
          DLME_SetSMIBRequest(DL_LINK, 0, &stTwoIdxSMIB);
          DLME_ParseMsgQueue(); // avoid getting full of DLME action queue
      }
    
      memset( &stTwoIdxSMIB.m_stEntry.m_stSmib.m_stNeighbor, 0, sizeof(stTwoIdxSMIB.m_stEntry.m_stSmib.m_stNeighbor) );
      // add the router in the neighbor table
      stTwoIdxSMIB.m_unUID                                  = g_unDAUXRouterAddr;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stNeighbor.m_unUID  = g_unDAUXRouterAddr;;
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stNeighbor.m_ucInfo = 0x80 | DLL_MASK_NEIDIAG_LINK;   // set as preffered clock source
      stTwoIdxSMIB.m_stEntry.m_stSmib.m_stNeighbor.m_ucCommHistory = 0xFF;
      DLME_SetSMIBRequest(DL_NEIGH, 0, &stTwoIdxSMIB );
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue
      
#pragma diag_default=Pa039
      
      MONITOR_EXIT();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Eduard Erdei
    /// @brief  Informs DMAP that dll has received advertise 
    /// @param  p_unRouterAddr - router address
    /// @param  p_pstTimeSyncInfo - pointer to time synchronization structure
    /// @return none
    /// @remarks
    ///      Access level: user level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void DMAP_CheckNeighborLink(void)
    {
        if (g_stTAI.m_ucClkInterogationStatus == MLSM_CLK_INTEROGATION_TIMEOUT)
        {
            DMAP_ResetStack(5);
        }            
    }

#else   //#ifndef BACKBONE_SUPPORT
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Checks the link with the System Manager, based on a SM's response
/// @param  none
/// @return none
/// @remarks
///      Access level: user level\n
///      Context: if no SM response during SM_LINK_RETRY_TIMEOUT seconds reset the stack
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_DMO_CheckSMLink(void)
{
    if( DEVICE_JOINED == g_ucJoinStatus )
    {
        if( g_ucSMLinkTimeout < SM_LINK_RETRY_TIMEOUT ) // during of retry timeout, ask each seconds
        {
            if( g_ucSMLinkTimeout ) // still timeout
            {
                //generate a ReadRequest to the SM for keepalive reason
                READ_REQ_SRVC stReadReq;
                stReadReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
                stReadReq.m_unDstOID = SM_STSO_OBJ_ID;

                stReadReq.m_stAttrIdtf.m_ucAttrFormat = ATTR_NO_INDEX;
                stReadReq.m_stAttrIdtf.m_unAttrID = STSO_CTR_UTC_OFFSET;
                
                if( SFC_SUCCESS == ASLSRVC_AddGenericObjectToSM( &stReadReq, SRVC_READ_REQ ))
                {
                    g_ucSMLinkTimeout--;
                }
            }
            else // timeout expired
            {
                DMAP_ResetStack(6);
                g_ucSMLinkTimeout = SM_LINK_TIMEOUT;
            }
        }
        else // more that SM_LINK_RETRY_TIMEOUT
        {
            g_ucSMLinkTimeout--;
        }
    }  
}
#endif // #ifndef BACKBONE_SUPPORT

#define CONTRACT_BUF_LEN    53
#define ATT_BUF_LEN         34
#define ROUTE_BUF_LEN       50
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Prepares indexed attributes needed for the join process
/// @param  none
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLMO_prepareJoinSMIBS(void)
{

#ifndef BACKBONE_SUPPORT
    DLMO_SMIB_ENTRY stEntry;
    // search neighbor table EUI64 address of router
    if( SFC_SUCCESS == DLME_GetSMIBRequest( DL_NEIGH, g_unDAUXRouterAddr, &stEntry) )
    {
        if (memcmp(stEntry.m_stSmib.m_stNeighbor.m_aEUI64,
                   c_aucInvalidEUI64Addr, 
                   sizeof(c_aucInvalidEUI64Addr)))
        {
            
            // add a contract to the advertising router
            uint8 aucTmpBuf[CONTRACT_BUF_LEN];
            aucTmpBuf[0] = (uint8)(JOIN_CONTRACT_ID) >> 8;
            aucTmpBuf[1] = (uint8)(JOIN_CONTRACT_ID);      
            memcpy(aucTmpBuf + 2, c_aLocalLinkIpv6Prefix, 8);
            memcpy(aucTmpBuf + 10, c_oEUI64BE, 8);
            //double the index
            memcpy(aucTmpBuf + 18, aucTmpBuf, 18);
            
            memcpy(aucTmpBuf + 36, c_aLocalLinkIpv6Prefix, 8);  
            DLME_CopyReversedEUI64Addr(aucTmpBuf + 44, stEntry.m_stSmib.m_stNeighbor.m_aEUI64);
            
            aucTmpBuf[52] = DMO_PRIORITY_NWK_CTRL; // b1b0 = priority; b2= include contract flag;
            
            NLME_SetRow( NLME_ATRBT_ID_CONTRACT_TABLE, 
                        0, 
                        aucTmpBuf,
                        CONTRACT_BUF_LEN );   
            
            
            // add the address translation for the advertising router
            memcpy(aucTmpBuf, c_aLocalLinkIpv6Prefix, 8);
            DLME_CopyReversedEUI64Addr(aucTmpBuf + 8, stEntry.m_stSmib.m_stNeighbor.m_aEUI64);
            memcpy(aucTmpBuf + 16, aucTmpBuf, 16);
            aucTmpBuf[32] = g_unDAUXRouterAddr >> 8;
            aucTmpBuf[33] = g_unDAUXRouterAddr;
            
            NLME_SetRow( NLME_ATRBT_ID_ATT_TABLE, 
                        0, 
                        aucTmpBuf,
                        ATT_BUF_LEN );
            
            return SFC_SUCCESS;
        }
    }
    return SFC_FAILURE;
 #else
    //add a contract to the SM
    uint8 aucTmpBuf[53];
    aucTmpBuf[0] = (uint8)(JOIN_CONTRACT_ID) >> 8;
    aucTmpBuf[1] = (uint8)(JOIN_CONTRACT_ID);      
    memcpy(aucTmpBuf + 2, g_stDMO.m_auc128BitAddr, 16);
    //double the index
    memcpy(aucTmpBuf + 18, aucTmpBuf, 18);
    
    memcpy(aucTmpBuf + 36, g_stDMO.m_aucSysMng128BitAddr, 16);  
    aucTmpBuf[52] = DMO_PRIORITY_NWK_CTRL; // b1b0 = priority; b2= include contract flag;
    
    NLME_SetRow( NLME_ATRBT_ID_CONTRACT_TABLE, 
                0, 
                aucTmpBuf,
                CONTRACT_BUF_LEN );   
    
    //add a route to the SM
    memcpy(aucTmpBuf, g_stDMO.m_aucSysMng128BitAddr, 16);
    //double the index
    memcpy(aucTmpBuf + 16, g_stDMO.m_aucSysMng128BitAddr, 16);
    memcpy(aucTmpBuf + 32, g_stDMO.m_aucSysMng128BitAddr, 16);
    aucTmpBuf[48] = 1;    // m_ucNWK_HopLimit = 1
    aucTmpBuf[49] = 1;    // m_bOutgoingInterface = 1
    
    NLME_SetRow( NLME_ATRBT_ID_ROUTE_TABLE, 
                0, 
                aucTmpBuf,
                ROUTE_BUF_LEN); 
    
    return SFC_SUCCESS;
 #endif   
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Updates indexed attributes for the join process
/// @param  none
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLMO_updateJoinSMIBS(void)
{
uint8 aucTmpBuf[CONTRACT_BUF_LEN];    //the maximum size for the used buffer

#ifndef BACKBONE_SUPPORT
    //delete the ATT for advertiser router
    DLMO_SMIB_ENTRY stEntry;
    memcpy(aucTmpBuf, c_aLocalLinkIpv6Prefix, 8);
    
    // search neighbor table EUI64 address of router
    if( SFC_SUCCESS == DLME_GetSMIBRequest(DL_NEIGH, g_unDAUXRouterAddr, &stEntry) )
    {
        DLME_CopyReversedEUI64Addr(aucTmpBuf + 8, stEntry.m_stSmib.m_stNeighbor.m_aEUI64);
        NLME_DeleteRow(NLME_ATRBT_ID_ATT_TABLE, 0, aucTmpBuf, 16 );
    }
#endif

    //for the BBR no needed to change the existing route to SM      
    //add the real ATT table with SM
    memcpy(aucTmpBuf, g_stDMO.m_aucSysMng128BitAddr, 16);
    memcpy(aucTmpBuf + 16, aucTmpBuf, 16);
    aucTmpBuf[32] = g_stDMO.m_unSysMngShortAddr >> 8;
    aucTmpBuf[33] = (uint8)g_stDMO.m_unSysMngShortAddr;
            
    return NLME_SetRow( NLME_ATRBT_ID_ATT_TABLE, 
                        0, 
                        aucTmpBuf,
                        ATT_BUF_LEN );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Reinitializes all device layers and sets the device on discovery state
/// @param  p_ucReason - reset reason
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
__monitor void DMAP_ResetStack(uint8 p_ucReason)
{
    // add reset reason to debug reset reason table
#ifdef DEBUG_RESET_REASON
    g_aucDebugResetReason[0] = p_ucReason;
    if( g_ucDiscoveryState == DEVICE_JOINED )
    {
        g_aucDebugResetReason[2] = g_aucDebugResetReason[1];
        g_aucDebugResetReason[1] = p_ucReason;
    }
    g_aucDebugResetReason[3] = g_stDMO.m_unRestartCount;          
#endif      
    
    MONITOR_ENTER();  
    
    g_unAlarmTimeout = 0;
    
    ASLDE_Init();
    DLME_Init(); // reset the modem inside
    TLME_Init();
    NLDE_Init();
    SLME_Init();  
    DLL_Init();
    
    DMAP_Init();  
  
    MLDE_SetDiscoveryState(DEVICE_DISCOVERY);
    
    UAP_OnJoinReset();
    
  #ifndef g_ucProvisioned  
    g_ucProvisioned = PROV_STATE_INIT;
  #endif  
        
    MONITOR_EXIT();  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Checks the timeouts based on second accuracy 
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_CheckSecondCounter(void)
{
#ifndef BACKBONE_SUPPORT
  UpdateBannedRouterStatus();
  
  DMAP_CheckNeighborLink();
  
  if( g_unLastJoinCnt )
      g_unLastJoinCnt--;
#endif
  
  if( g_ulRadioSleepCounter )
      g_ulRadioSleepCounter--;
  
  if( g_stJoinRetryCntrl.m_unJoinTimeout )  
      g_stJoinRetryCntrl.m_unJoinTimeout--;

  if( UDO_STATE_APPLYING == g_ucUDOState )
  {
    if( g_ulUDOCutoverTime <= MLSM_GetCrtTaiSec() )
    {
#if   (DEVICE_TYPE == DEV_TYPE_MC13225) 
    //TO DO - give control to FW swap mechanism;
#else 
    #warning "unknown device:activate the new FW discarded"
#endif    
	}

#ifdef BACKBONE_SUPPORT
    else
    {
      //be sure that SM Link timeout will not cancel the firmware upgrade process
      if(!g_ucSMLinkTimeout)
        g_ucSMLinkTimeout = SM_LINK_TIMEOUT;
    }
  }
  DMAP_DMO_CheckSMLink();
#else
  }
#endif  //#ifdef BACKBONE_SUPPORT
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Generates a Security join request buffer
/// @param  p_pucBuf - the request buffer
/// @return data size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint16 DMO_generateSecJoinReqBuffer(uint8* p_pucBuf)
{
    g_aulNewNodeChallenge[0] =  g_unRandValue;
    g_aulNewNodeChallenge[1] =  g_unRandValue;

    g_aulNewNodeChallenge[2] =  MLSM_GetCrtTaiSec();
    
    memcpy(p_pucBuf, c_oEUI64BE, 8);
  
    memcpy(p_pucBuf + 8, g_aulNewNodeChallenge, 16);
    
    //add the Key Info field
    //need to be specified the content coding of the Key_Info
    p_pucBuf[24] = 0x00;
    
    //add the algorihm identifier field
    p_pucBuf[25] = 0x01;     //only the AES_CCM symmetric key algorithm must used 
                             //b7..b4 - public key algorithm and options
                             //b3..b0 - symmetric key algorithm and mode
    
    AES_Crypt_User(g_aJoinAppKey, (uint8*)g_aulNewNodeChallenge, p_pucBuf, 26, p_pucBuf + 26, 0);
    
    return (26 + 4);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Validates join security response
/// @param  p_pExecRsp - pointer to the response service structure 
/// @return None
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
#define SEC_JOIN_RESP_SIZE      71
void DMO_checkSecJoinResponse(EXEC_RSP_SRVC* p_pExecRsp)
{
    if( SFC_SUCCESS != p_pExecRsp->m_ucSFC )
    {
        g_stJoinRetryCntrl.m_unJoinTimeout = 0;     //force rejoin process
        return;
    }
   
    if( p_pExecRsp->m_unLen < SEC_JOIN_RESP_SIZE )
        return;
    
    uint8 aucHashEntry[MAX_HMAC_INPUT_SIZE];
    
    memcpy(aucHashEntry, p_pExecRsp->p_pRspData, 16);       //Security Manager Challenge
    memcpy(aucHashEntry + 16, g_aulNewNodeChallenge, 16);   //New Device Challenge
    memcpy(aucHashEntry + 32, c_oEUI64BE, 8);
    memcpy(aucHashEntry + 40, c_oSecManagerEUI64BE, 8);
    
    memcpy(aucHashEntry + 48, p_pExecRsp->p_pRspData + 32, SEC_JOIN_RESP_SIZE - 65); //32(Offset) + 1(DLL Key Id) + 32(Keys Content)
    memcpy(aucHashEntry + 48 + SEC_JOIN_RESP_SIZE - 65, p_pExecRsp->p_pRspData + SEC_JOIN_RESP_SIZE - 32, 32);  //copy the Keys Content

    Keyed_Hash_MAC(g_aJoinAppKey, aucHashEntry, MAX_HMAC_INPUT_SIZE);
    
    //validate Hash_B field
    if( !memcmp(p_pExecRsp->p_pRspData + 16, aucHashEntry, 16) )
    {
        //retain the System Manager Challenge
        memcpy(g_aucSecMngChallenge, p_pExecRsp->p_pRspData, 16);
        
        //compute the Master Key
        memcpy(aucHashEntry, g_aulNewNodeChallenge, 16);        //New Device Challenge
        memcpy(aucHashEntry + 16, p_pExecRsp->p_pRspData, 16);  //Security Manager Challenge
        //the Device and SecMngr EUI64s not overwrited inside "aucHashEntry"
    
        Keyed_Hash_MAC(g_aJoinAppKey, aucHashEntry, 48);    //the first 16 bytes from "aucHashEntry" represent the Master Key
    
        //decrypt the DLL Key using the Master Key
        //MIC is not transmitted inside response - Encryption with MIC_SIZE = 4
        uint8 ucEncKeyOffset = SEC_JOIN_RESP_SIZE - 32;
        
        //for pure decryption no result validation needed 
        AES_Decrypt_User( aucHashEntry,              //Master_Key - first 16 bytes
                          (uint8*) g_aulNewNodeChallenge,     //first 13 bytes 
                          NULL, 0,                   // No authentication
                          p_pExecRsp->p_pRspData + ucEncKeyOffset,
                          32 + MIC_SIZE);

        //add the DLL
        uint8* pucKeyPolicy = p_pExecRsp->p_pRspData + ucEncKeyOffset - 7;  //2*3 bytes(Master Key, DLL and Session key policies) + 1 byte(DLL KeyID) 
        uint32 ulLifeTime;
        
        //add the Master Key
        ulLifeTime = (((uint16)(pucKeyPolicy[0])) << 8) | pucKeyPolicy[1];
        ulLifeTime *= 1800;
        
        SLME_SetKey(    NULL, // p_pucPeerIPv6Address, 
                        0,  // p_ucUdpPorts,
                        0,  // p_ucKeyID  - hardcoded
                        aucHashEntry,  // p_pucKey, 
                        c_oSecManagerEUI64BE,  // p_pucIssuerEUI64, 
                        0, // p_ulValidNotBefore
                        ulLifeTime, // p_ulSoftLifetime,
                        ulLifeTime*2, // p_ulHardLifetime,
                        SLM_KEY_USAGE_MASTER, // p_ucUsage, 
                        0 // p_ucPolicy -> need correct policy
                        );
        
        //add the DLL Key
        ulLifeTime = (((uint16)(pucKeyPolicy[2])) << 8) | pucKeyPolicy[3];
        ulLifeTime *= 1800;
        SLME_SetKey(    NULL, // p_pucPeerIPv6Address, 
                        0,  // p_ucUdpPorts,
                        pucKeyPolicy[6],  // p_ucKeyID,
                        pucKeyPolicy + 7,  // p_pucKey, 
                        NULL,  // p_pucIssuerEUI64, 
                        0, // p_ulValidNotBefore
                        ulLifeTime, // p_ulSoftLifetime,
                        ulLifeTime*2, // p_ulHardLifetime,
                        SLM_KEY_USAGE_DLL, // p_ucUsage, 
                        0 // p_ucPolicy -> need correct policy
                        );
        
        //add the SM Session key
        const uint8 * pKey =  pucKeyPolicy + 7 + 16;
        memset( aucHashEntry, 0, 16 ); 
        ulLifeTime = ((uint16)(pucKeyPolicy[4])) << 8 | pucKeyPolicy[5];
        if( ulLifeTime || memcmp( pKey, aucHashEntry, 16 ) ) // TL encrypted if ulLifeTime || key <> 0
        {
            ulLifeTime *= 1800;                
            
            SLME_SetKey(    g_stDMO.m_aucSysMng128BitAddr, // p_pucPeerIPv6Address, 
                            (uint8)UAP_SMAP_ID & 0x0F,  // p_ucUdpPorts,   //dest port - SM's SM_UAP 
                            0,  // p_ucKeyID,
                            pKey,  // p_pucKey, 
                            NULL,  // p_pucIssuerEUI64, 
                            0, // p_ulValidNotBefore
                            ulLifeTime, // p_ulSoftLifetime,
                            ulLifeTime*2, // p_ulHardLifetime,
                            SLM_KEY_USAGE_SESSION, // p_ucUsage, 
                            0 // p_ucPolicy -> need correct policy
                            );
        }
            
        g_ucJoinStatus = DEVICE_SEND_SM_JOIN_REQ;
    }    
    #ifdef DEBUG_RESET_REASON
    else
    {
        g_aucDebugResetReason[10]++;
    }
    #endif      
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Generates a System Manager join request buffer
/// @param  p_pBuf - the request buffer
/// @return data size
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint16 DMO_generateSMJoinReqBuffer(uint8* p_pBuf)
{
  uint8* pucTemp = p_pBuf;
  
  memcpy(p_pBuf, c_oEUI64BE, 8);
  p_pBuf += 8;

  *(p_pBuf++) = g_unDllSubnetId >> 8;
  *(p_pBuf++) = g_unDllSubnetId;
  
  //Device Role Capability
  *(p_pBuf++) = g_stDPO.m_unDeviceRole >> 8;
  *(p_pBuf++) = g_stDPO.m_unDeviceRole;  
  
  uint8 ucStrLen = DMAP_GetVisibleStringLength( g_stDMO.m_aucTagName, sizeof(g_stDMO.m_aucTagName) );
  *(p_pBuf++) = ucStrLen;
  memcpy(p_pBuf, g_stDMO.m_aucTagName, ucStrLen);  
  p_pBuf += ucStrLen;
  
  *(p_pBuf++) = c_ucCommSWMajorVer;
  *(p_pBuf++) = c_ucCommSWMinorVer;
  
  ucStrLen = DMAP_GetVisibleStringLength( c_aucSWRevInfo, sizeof(c_aucSWRevInfo) );
  *(p_pBuf++) = ucStrLen;
  memcpy(p_pBuf, c_aucSWRevInfo, ucStrLen );  
  
  if( !g_stDPO.m_ucJoinedAfterProvisioning )
  {
      // custom implementation: signnal to the SM that persistent data is erased  
      *(p_pBuf+2) = '+';
  }  
  
  p_pBuf += ucStrLen;
  
  // when Device Capability inside the SM Join Request
  uint16 unSize;
  DLMO_ReadDeviceCapability(DL_DEV_CAPABILITY, &unSize, p_pBuf); 
  p_pBuf += unSize;    
  
  return p_pBuf - pucTemp;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Validates join SM response
/// @param  p_pExecRsp - pointer to the response service structure 
/// @return None
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMO_checkSMJoinResponse(EXEC_RSP_SRVC* p_pExecRsp)
{
    //TO DO - wait specs to clarify if authentication with MIC32 needed    
    
    if( SFC_SUCCESS != p_pExecRsp->m_ucSFC )
    {
        g_stJoinRetryCntrl.m_unJoinTimeout = 0;     //force rejoin process
        return;
    }
    
    if( p_pExecRsp->m_unLen < 46 )
        return;
    
    memcpy(g_stDMO.m_auc128BitAddr, p_pExecRsp->p_pRspData, 16);
    p_pExecRsp->p_pRspData += 16;
    
#ifndef BACKBONE_SUPPORT
    //the device short address will be updated later 
    s_unDevShortAddr = (uint16)p_pExecRsp->p_pRspData[0] << 8 | p_pExecRsp->p_pRspData[1];
#else
    g_stDMO.m_unShortAddr = (uint16)p_pExecRsp->p_pRspData[0] << 8 | p_pExecRsp->p_pRspData[1];
#endif    
    g_stDMO.m_unAssignedDevRole = (uint16)p_pExecRsp->p_pRspData[2] << 8 | p_pExecRsp->p_pRspData[3];
    p_pExecRsp->p_pRspData += 4;
    
    memcpy(g_stDMO.m_aucSysMng128BitAddr, p_pExecRsp->p_pRspData, 16);
    p_pExecRsp->p_pRspData += 16;
    
    g_stDMO.m_unSysMngShortAddr = (uint16)p_pExecRsp->p_pRspData[0] << 8 | p_pExecRsp->p_pRspData[1];
    p_pExecRsp->p_pRspData += 2;
    
    memcpy(g_stDMO.m_unSysMngEUI64, p_pExecRsp->p_pRspData, 8);

    //update the SM Session Key's issuer
    SLME_UpdateJoinSessionsKeys( g_stDMO.m_unSysMngEUI64, g_stDMO.m_aucSysMng128BitAddr );
    
    g_ucJoinStatus = DEVICE_SEND_SM_CONTR_REQ;
    
    // m_unAssignedDevRole is persistent; save persistent data;
    DPO_SavePersistentData();
}


/*===========================[ DMO implementation ]==========================*/
#if defined(ROUTING_SUPPORT) || defined(BACKBONE_SUPPORT)
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei, Mircea Vlasin
/// @brief  Advertising router's DMO generates a join request to the DSMO
/// @param  p_pExecReq - execute request structure
/// @param  p_pdtf - APDU structure
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 DMAP_DMO_forwardJoinReq( EXEC_REQ_SRVC * p_pExecReq,
                               APDU_IDTF *     p_pIdtf)
{
    if ( (ASLDE_GetAPDUFreeSpace() > MIN_JOIN_FREE_SPACE) 
        && (DEVICE_JOINED == g_ucJoinStatus) ) 
    {  
        EXEC_REQ_SRVC stFwExecReq; 
                
        switch( p_pExecReq->m_ucMethID )
        {
            case DMO_PROXY_SM_JOIN_REQ:
                stFwExecReq.m_unDstOID = SM_DMSO_OBJ_ID;
                stFwExecReq.m_ucMethID = DMSO_JOIN_REQUEST;
                break;
            case DMO_PROXY_SM_CONTR_REQ:
                stFwExecReq.m_unDstOID = SM_DMSO_OBJ_ID;
                stFwExecReq.m_ucMethID = DMSO_CONTRACT_REQUEST;
                break;
            case DMO_PROXY_SEC_SYM_REQ:
                stFwExecReq.m_unDstOID = SM_PSMO_OBJ_ID;
                stFwExecReq.m_ucMethID = PSMO_SECURITY_JOIN_REQ;
                break;
        }
        
        stFwExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
        stFwExecReq.p_pReqData = p_pExecReq->p_pReqData;
        stFwExecReq.m_unLen    = p_pExecReq->m_unLen;      
        
        //check if the current request is a retry        
        NEW_DEVICE_JOIN_INFO* pstJoinInfo = DMAP_DMO_checkNewDevJoinInfo(p_pExecReq->m_ucMethID, p_pIdtf);
        if( pstJoinInfo )
        {        
            uint8 ucCrtReqID = g_ucReqID;
            if (SFC_SUCCESS == ASLSRVC_AddGenericObjectToSM( &stFwExecReq, SRVC_EXEC_REQ ) )
            {                
                // set join info
                pstJoinInfo->m_ucLastMethodID = p_pExecReq->m_ucMethID;
                pstJoinInfo->m_ucJoinReqID = p_pExecReq->m_ucReqID;
                pstJoinInfo->m_ucFwReqID   = ucCrtReqID;
                pstJoinInfo->m_ucTTL       = 2*APP_QUE_TTL_SEC;
                
                memcpy(pstJoinInfo->m_aucAddr, p_pIdtf->m_aucAddr, 8);
                if( pstJoinInfo >= g_stDevInfo +  g_ucDevInfoEntryNo )// is added device 
                {                
                    g_ucDevInfoEntryNo++;
                }
              
                return SFC_SUCCESS;
            }
        }
    }
    
    return SFC_INSUFFICIENT_DEVICE_RESOURCES;      
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Forwards a join response to a new joining device
/// @param  p_pExecRsp - pointer to the execute response
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_DMO_forwardJoinResp ( EXEC_RSP_SRVC * p_pExecRsp )
{
  NEW_DEVICE_JOIN_INFO* pJoinInfo = DMAP_DMO_getNewDevJoinInfo( p_pExecRsp->m_ucReqID);
  if( pJoinInfo )
  {
      // to save stack space, alter the SM response instead of declaring a new execute response
      p_pExecRsp->m_ucReqID  = pJoinInfo->m_ucJoinReqID;
      p_pExecRsp->m_unSrcOID = DMAP_DMO_OBJ_ID;
      p_pExecRsp->m_unDstOID = DMAP_DMO_OBJ_ID;  
      
      ASLSRVC_AddJoinObject( p_pExecRsp, SRVC_EXEC_RSP, pJoinInfo->m_aucAddr );
      
      if( SFC_SUCCESS != p_pExecRsp->m_ucSFC || pJoinInfo->m_ucLastMethodID == DMO_PROXY_SM_CONTR_REQ ) // a response for last request via proxy
      {
          //for NIVIS joining device the condition "SFC_SUCCESS != p_pExecRsp->m_ucSFC" is partialy redundant because the proxy router will be banned 
          pJoinInfo->m_ucTTL = 0; // mark for removing
      }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Check if the join request is a retry message
/// @param  p_pucMethodId - ISA100 proxy join method ID
/// @param  p_pIdtf - APDU structure
/// @return pointer to device join structure, NULL if fail
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
NEW_DEVICE_JOIN_INFO* DMAP_DMO_checkNewDevJoinInfo(uint8       p_pucMethodId, 
                                                   APDU_IDTF*  p_pstIdtf)
{
    //check if the message is a retry
    NEW_DEVICE_JOIN_INFO* pstJoinInfo;
    const NEW_DEVICE_JOIN_INFO* pstEndTable = g_stDevInfo + g_ucDevInfoEntryNo;
    
    for(pstJoinInfo = g_stDevInfo; pstJoinInfo < pstEndTable; pstJoinInfo++)
    {
        if( !memcmp(pstJoinInfo->m_aucAddr, p_pstIdtf->m_aucAddr, sizeof(pstJoinInfo->m_aucAddr)) ) 
        {
          // mandatory order: DMO_PROXY_SEC_SYM_REQ -> DMO_PROXY_SM_JOIN_REQ -> DMO_PROXY_SM_CONTR_REQ
            if( p_pucMethodId == DMO_PROXY_SM_JOIN_REQ ) 
            {
                if( pstJoinInfo->m_ucLastMethodID == DMO_PROXY_SEC_SYM_REQ )
                    return pstJoinInfo;
            }
            else if ( p_pucMethodId == DMO_PROXY_SM_CONTR_REQ )
            {
                if( pstJoinInfo->m_ucLastMethodID == DMO_PROXY_SM_JOIN_REQ )
                    return pstJoinInfo;
            }
                         
            return NULL; // invalid order (duplicate?) 
        }
    }
        
    // not found, new request
    if( p_pucMethodId != DMO_PROXY_SEC_SYM_REQ ) // invalid first request 
      return NULL;
    
    if( pstJoinInfo >= g_stDevInfo + MAX_SIMULTANEOUS_JOIN_NO ) // out of table
        return NULL;
    
    return pstJoinInfo;          
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Retrieves new device's join information
/// @param  p_ucFwReqID - forward request identifier
/// @param  p_pEUI64 - EUI64 address
/// @param  p_pPriority - priority
/// @param  p_pReqID - request identifier
/// @return join info correspondent to request ID
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
static NEW_DEVICE_JOIN_INFO* DMAP_DMO_getNewDevJoinInfo( uint8   p_ucFwReqID )
{  
  NEW_DEVICE_JOIN_INFO* pstJoinInfo;
  const NEW_DEVICE_JOIN_INFO* pstEndTable = g_stDevInfo + g_ucDevInfoEntryNo;
  
  for(pstJoinInfo = g_stDevInfo; pstJoinInfo < pstEndTable; pstJoinInfo++)
  {
      if ( p_ucFwReqID == pstJoinInfo->m_ucFwReqID )
      {
          return pstJoinInfo;                    
      }
  }
  
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Checks new device's time to live and removes it if time expired
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_DMO_CheckNewDevInfoTTL(void)
{  
  for (uint8 ucIdx = 0; ucIdx < g_ucDevInfoEntryNo; )
  {
      if ( g_stDevInfo[ucIdx].m_ucTTL )
      {
          g_stDevInfo[ucIdx].m_ucTTL--;
          ucIdx++;
      }
      else        
      {          
          g_ucDevInfoEntryNo--;
          
          memmove(g_stDevInfo+ucIdx,
                  g_stDevInfo+ucIdx+1,
                  (g_ucDevInfoEntryNo-ucIdx) * sizeof(*g_stDevInfo)) ;
          
      }
  }
}
 
#endif // ROUTING_SUPPORT || BACKBONE_SUPPORT


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mihaela Goloman, Eduard Erdei
/// @brief  Initiates a join request for a non routing device
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DMAP_startJoin(void)
{
  DLL_MIB_ADV_JOIN_INFO stJoinInfo;  
  
  if( SFC_SUCCESS != DLME_GetMIBRequest(DL_ADV_JOIN_INFO, &stJoinInfo) )
      return;
  
  EXEC_REQ_SRVC stExecReq;
  uint8         aucReqParams[MAX_APDU_SIZE];
    
  stExecReq.m_unSrcOID = DMAP_DMO_OBJ_ID;
  stExecReq.m_unDstOID = DMAP_DMO_OBJ_ID;
  stExecReq.m_ucMethID = DMO_PROXY_SEC_SYM_REQ;  
  stExecReq.p_pReqData = aucReqParams;
  stExecReq.m_unLen    = DMO_generateSecJoinReqBuffer( stExecReq.p_pReqData );    
   
  ASLSRVC_AddJoinObject(  &stExecReq, SRVC_EXEC_REQ, NULL );

  DSMO_SetJoinTimeout( 1 << (stJoinInfo.m_mfDllJoinBackTimeout & 0x0F));  
  g_ucJoinStatus = DEVICE_SECURITY_JOIN_REQ_SENT;  
}

//////////////////////////////////////TLMO Object///////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Executes a transport layer management object method
/// @param  p_unMethID  - method identifier
/// @param  p_unReqSize - request buffer size
/// @param  p_pReqBuf   - request buffer containing method parameters
/// @param  p_pRspSize  - pointer to uint16 where to pu response size
/// @param  p_pRspBuf  - pointer to response buffer (otput data)
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 TLMO_execute(uint8    p_ucMethID,
                   uint16   p_unReqSize,
                   uint8 *  p_pReqBuf,
                   uint16 * p_pRspSize,
                   uint8 *  p_pRspBuf)
{
  uint8 ucSFC = SFC_SUCCESS;   
  uint8 * pStart = p_pRspBuf;
  
  if (!p_ucMethID || p_ucMethID >= TLME_METHOD_NO)
      return SFC_INVALID_METHOD;
  
  if (p_unReqSize != c_aTLMEMethParamLen[p_ucMethID-1])
      return SFC_INVALID_SIZE;     
  
  *(p_pRspBuf++) = 0; // SUCCESS.... TL editor is not aware that execute service already has a SFC
  
  switch(p_ucMethID)
  {
  case TLME_RESET_METHID: 
      TLME_Reset(*p_pReqBuf); 
      break;
    
  case TLME_HALT_METHID: 
    {
        uint16 unPortNr = (((uint16)*(p_pReqBuf+16)) << 8) | *(p_pReqBuf+17);
        TLME_Halt( p_pReqBuf, unPortNr ); 
        break;
    }
      
  case TLME_PORT_RANGE_INFO_METHID:
    {
        TLME_PORT_RANGE_INFO stPortRangeInfo;  
        TLME_GetPortRangeInfo( p_pReqBuf, &stPortRangeInfo );
        
        p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, stPortRangeInfo.m_unNbActivePorts);
        p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, stPortRangeInfo.m_unFirstActivePort);
        p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, stPortRangeInfo.m_unLastActivePort);        
        break;          
    }
    
  case TLME_GET_PORT_INFO_METHID:
  case TLME_GET_NEXT_PORT_INFO_METHID:  
    {
        TLME_PORT_INFO stPortInfo;  
        uint16 unPorNo = (((uint16)*(p_pReqBuf+16)) << 8) | *(p_pReqBuf+17);
        
        uint8 ucStatus;
        
        if (p_ucMethID == TLME_GET_PORT_INFO_METHID)
        {
            ucStatus = TLME_GetPortInfo( p_pRspBuf, unPorNo, &stPortInfo);
        }
        else // if (p_ucMethID == TLME_GET_NEXT_PORT_INFO_METHID)
        {
            ucStatus = TLME_GetNextPortInfo( p_pRspBuf, unPorNo, &stPortInfo);
        }
        
        if (SFC_SUCCESS == ucStatus)
        {
            p_pRspBuf = DMAP_InsertUint16( p_pRspBuf, unPorNo );
            p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, stPortInfo.m_unUID );

#ifdef BACKBONE_SUPPORT            
            *(p_pRspBuf++) = 0; // not compressed  
#else
            *(p_pRspBuf++) = 1; // compressed any time at device level 
#endif            

            p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, stPortInfo.m_aTPDUCount[TLME_PORT_TPDU_IN_OK] );
            p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, stPortInfo.m_aTPDUCount[TLME_PORT_TPDU_IN] );
            p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, stPortInfo.m_aTPDUCount[TLME_PORT_TPDU_OUT_OK] );
            p_pRspBuf = DMAP_InsertUint32( p_pRspBuf, stPortInfo.m_aTPDUCount[TLME_PORT_TPDU_OUT] );
        }
        else
        {
            *pStart = 1; // FAIL
        }
        break;     
    }  
  }
  
  *p_pRspSize = p_pRspBuf - pStart;
  
  return ucSFC;
  
}

#ifndef BACKBONE_SUPPORT
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Ban an advertise router for a time period
/// @param  p_unDAUXRouterAddr - short address of router
/// @param  p_unBannedPeriod - ban time in seconds
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddBannedAdvRouter(SHORT_ADDR p_unDAUXRouterAddr, uint16 p_unBannedPeriod)
{
  // add at begin (not from interrupt)
  memmove( g_astBannedRouters+1, g_astBannedRouters, sizeof(g_astBannedRouters) - sizeof(g_astBannedRouters[0]) );
  g_astBannedRouters[0].m_unRouterAddr = p_unDAUXRouterAddr;
  g_astBannedRouters[0].m_unBannedPeriod = p_unBannedPeriod * (((g_stDMO.m_unRestartCount >> 2) & 0x0003) + 1) + (g_unRandValue & 0x1F); 
  
  if( g_ucBannedRoutNo < MAX_BANNED_ROUTER_NO )
  {
    g_ucBannedRoutNo++;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Updates time for banned routers
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
static void UpdateBannedRouterStatus()
{
  uint8 ucIdx = 0;
  
  for( ucIdx = 0; ucIdx < g_ucBannedRoutNo; ucIdx++ )
  {
    if( g_astBannedRouters[ ucIdx ].m_unBannedPeriod )
    {
        g_astBannedRouters[ ucIdx ].m_unBannedPeriod --;
    }
  }
  
  if( ucIdx && !g_astBannedRouters[ ucIdx-1 ].m_unBannedPeriod ) // last banned entry expired
  {
      g_ucBannedRoutNo--;      
  }
}
#endif  //#ifndef BACKBONE_SUPPORT

#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Parses the management commands received over the serial interface by the backbone
    /// @return none
    /// @remarks
    ///      Access level: user level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 DMAP_ParseLocalMng( const uint8 * p_pMngCmd, uint8 p_ucMngCmdLen )
    {
        switch( *p_pMngCmd )
        {
        case LOCAL_MNG_RESTARTED_SUBCMD:
                   
                if (p_ucMngCmdLen == 2)
                {
                    if( g_ucDiscoveryState != DEVICE_DISCOVERY ) // protect for invalid reset
                    {
                        DMAP_ResetStack(11);
                    }
                    return 1;
                }
                break;
                
        case LOCAL_MNG_PROVISIONING_SUBCMD:
                if( p_ucMngCmdLen >= 3
                            + sizeof(c_oEUI64BE)
                            + sizeof(g_stFilterBitMask)
                            + sizeof(g_stFilterTargetID)
                            + sizeof(g_aJoinAppKey)
                            + sizeof(c_oSecManagerEUI64BE)
                            + sizeof(g_stDMO.m_aucSysMng128BitAddr)
                            + sizeof(g_stDMO.m_auc128BitAddr)
                            + sizeof(g_stDMO.m_aucTagName) )
                {                    
                    p_pMngCmd += 1+2; // cmd + format
                    memcpy( c_oEUI64BE, p_pMngCmd, sizeof(c_oEUI64BE) ); 
                    p_pMngCmd += sizeof(c_oEUI64BE);
                    
                    g_stFilterBitMask  = ( ((uint16)p_pMngCmd[0]) << 8 ) | p_pMngCmd[1]; 
                    p_pMngCmd += 2;
                    
                    g_stFilterTargetID = ( ((uint16)p_pMngCmd[0]) << 8 ) | p_pMngCmd[1]; 
                    p_pMngCmd += 2;
                    
                    memcpy( g_aJoinAppKey, p_pMngCmd, sizeof(g_aJoinAppKey) ); 
                    p_pMngCmd += sizeof(g_aJoinAppKey);
                    
                    memcpy( c_oSecManagerEUI64BE, p_pMngCmd, sizeof(c_oSecManagerEUI64BE) ); 
                    p_pMngCmd += sizeof(c_oSecManagerEUI64BE);
                    
                    memcpy( g_stDMO.m_aucSysMng128BitAddr, p_pMngCmd, sizeof(g_stDMO.m_aucSysMng128BitAddr) ); 
                    p_pMngCmd += sizeof(g_stDMO.m_aucSysMng128BitAddr);
                    
                    memcpy( g_stDMO.m_auc128BitAddr, p_pMngCmd, sizeof(g_stDMO.m_auc128BitAddr) );      
                    p_pMngCmd += sizeof(g_stDMO.m_auc128BitAddr);
                    
                    DLME_CopyReversedEUI64Addr( c_oEUI64LE, c_oEUI64BE );
                    
                    DMAP_ResetStack(10);
                    g_ucProvisioned = PROV_STATE_PROVISIONED;                    
                    
                    // load after reset stack since m_aucTagName is updated on DMAP_ResetStack()
                    if( 0xFF != *p_pMngCmd )
                    {
                        memcpy( g_stDMO.m_aucTagName, p_pMngCmd, sizeof(g_stDMO.m_aucTagName) );      
                    }     
                    
                    return 1;
                }
                break;    
        
        case LOCAL_MNG_ROUTES_SUBCMD:
                p_ucMngCmdLen -= 2;
                if ( p_ucMngCmdLen < sizeof(g_stNlme.m_aToEthRoutes) )
                {
                    g_stNlme.m_ucDefaultIsDLL = !p_pMngCmd[1]; // 0 means BBR, 1 means ETH 
                    g_stNlme.m_ucToEthRoutesNo = p_ucMngCmdLen/2;
                    memcpy(g_stNlme.m_aToEthRoutes, p_pMngCmd+2, p_ucMngCmdLen);
                    return 1;
                }               
                break;
        case LOCAL_MNG_ALERT_SUBCMD:
          {
              ALERT stAlert;
              typedef struct
              {
                uint8 m_uaObjPort[2];
                uint8 m_uaObjId[2];
                uint8 m_uaTime[6];
                uint8 m_ucAlertId;
                uint8 m_ucClass;
                uint8 m_ucDirection;
                uint8 m_ucCategory;
                uint8 m_ucType;
                uint8 m_ucPriority;
              } BBR_ALERT;

              BBR_ALERT * pBBRAlert = (BBR_ALERT*)(p_pMngCmd+1);
              stAlert.m_unDetObjTLPort = ((uint16)(pBBRAlert->m_uaObjPort[0])<< 8) | pBBRAlert->m_uaObjPort[1];
              stAlert.m_unDetObjID = ((uint16)(pBBRAlert->m_uaObjId[0])<< 8) | pBBRAlert->m_uaObjId[1]; 
              stAlert.m_ucClass = pBBRAlert->m_ucClass; 
              stAlert.m_ucDirection = pBBRAlert->m_ucDirection; 
              stAlert.m_ucCategory = pBBRAlert->m_ucCategory & 0x03; 
              stAlert.m_ucType = pBBRAlert->m_ucType;                      
              stAlert.m_ucPriority = pBBRAlert->m_ucPriority;                      
              stAlert.m_unSize = p_ucMngCmdLen - 1 - sizeof(BBR_ALERT); 
              
              if( stAlert.m_unSize < 128 )
              {
                  ARMO_AddAlertToQueue( &stAlert, (uint8*)(pBBRAlert+1) );
              }
              
              break;
          }
          
        }
        
        return 0;
    }
#endif //#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)
