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
 * Name:         Common_API.c
 * Author:       NIVIS LLC, John Chapman and Kedar Kapoor
 * Date:         July 2008
 * Description:  This file has the Common Radio routines for the RF Modem side of the common API.
 *               This also is the API for the single processor solution.  This routine will call
 *               into the serial port to grab data.
 * Changes:
 * Revisions:
 ***************************************************************************************************/

#include "Common_API.h"
#include "../global.h"
#include "DAQ_Comm.h"
#include "../ISA100/aslde.h"
#include "../ISA100/mlsm.h"
#include "../ISA100/dmap.h"
#include "../ISA100/dmap_armo.h"
#include "../ISA100/dmap_dmo.h"
#include "../ISA100/dlme.h"
#include "../ISA100/aslsrvc.h"
#include "../ISA100/nlme.h"

#include <string.h>

#if ( _UAP_TYPE != 0 )

uint8 API_accessDMAP( uint8 * p_pRcvData, uint8 p_ucMsgLen, uint8 * p_pRspData, uint8 * p_pucRspLen  );
void API_onRcvUAPAlarm( uint8 * p_pRcvData, uint8 p_ucMsgLen, uint8 * p_pucAlertID );
  
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
  uint8 API_onRcvStackSpecific( ApiMessageFrame * p_pMsgFrame );
#endif

uint8 API_onRcvApiCommands( ApiMessageFrame * p_pMsgFrame );

typedef void (*API_FCT_OnRcv)(ApiMessageFrame * p_pMsgFrame);

typedef struct 
{
    uint8   m_aDstAddr128[16];
    uint16  m_unDstTLPort;
    uint16  m_unSrcTLPort;
    uint32  m_ulContractLife;
    uint16  m_unMaxAPDUSize;
    uint16  m_unContractID;
    uint8   m_unReqType;
    uint8   m_ucSrvcType;
    uint8   m_ucPriority;
    uint8   m_ucReliability;                              
    DMO_CONTRACT_BANDWIDTH  m_stBandwidth;
} UAP_CONTR_REQ;


/****************
 * SendMessage
 ****************/
enum SEND_ERROR_CODE SendMessage(enum MSG_CLASS p_MsgClass,
                                 uint8 p_MsgType,
                                 uint8 p_ucIsRsp,
                                 uint8 p_ucMSG_ID, 
                                 uint8 p_ucBuffSize,
                           const uint8 *p_pMsgBuff)
{    
    static uint8 s_ucMsgId;
    ApiMessageFrame MessageFrame; 

    if( p_ucBuffSize > (MAX_API_BUFFER_SIZE - sizeof(ApiMessageFrame)) )
    {
        return ER_UNSUPPORTED_DATA_SIZE;
    }
    
    if( !p_ucIsRsp )
    {
        p_ucMSG_ID = (s_ucMsgId++) | 0x80;
    }
    
    MessageFrame.MessageHeader.MessageClass = p_MsgClass;
    MessageFrame.MessageHeader.m_bIsRsp = p_ucIsRsp;    
    MessageFrame.MessageHeader.Reserved = 0;    
    MessageFrame.MessageType = p_MsgType;
    MessageFrame.MessageID = p_ucMSG_ID;        
    MessageFrame.MessageSize = p_ucBuffSize;

    if (SUCCESS != API_PHY_Write(&MessageFrame, p_pMsgBuff )){
        return ER_QUEUE_FULL;
    }
    
    return NO_ERR;
}

#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
uint8 API_onRcvStackSpecific( ApiMessageFrame * p_pMsgFrame )
{
    uint8   ucMessageSize;
    uint8 * pRcvData = (uint8 *)(p_pMsgFrame + 1); // skip the header
    
    if( !p_pMsgFrame->MessageHeader.m_bIsRsp )
    {
        switch (p_pMsgFrame->MessageType)
        {
            // Generic Read/Write attributes
            case ISA100_GET_TAI_TIME:
                DMAP_InsertUint32( pRcvData, MLSM_GetCrtTaiSec() );
                SendMessage(STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 4, pRcvData );
                break;
            
            case ISA100_JOIN_STATUS:
                SendMessage(STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 1, &g_ucJoinStatus );
                break;
            
            case ISA100_UAP_IDS:
                SendMessage(STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 1, &g_ucUapId );
                break;
            
            case ISA100_ROUTING_DEVICE:
                if( g_stDPO.m_unDeviceRole & 0x02 )
                    pRcvData[0] = 1; //Routing support
                else
                    pRcvData[0] = 0;
                SendMessage(STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 1, pRcvData );
                break;
            
            case ISA100_GET_ADDR:
                //Be careful!!! - need MAX_API_BUFFER_SIZE >= 4 + 26 
                pRcvData[0] = g_unShortAddr >> 8;
                pRcvData[1] = g_unShortAddr;
                memcpy(pRcvData + 2, c_oEUI64BE, sizeof(c_oEUI64BE));
                memcpy(pRcvData + 10, g_aIPv6Address, sizeof(g_aIPv6Address));
                SendMessage(STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 26, pRcvData );
                break;
            
            case ISA100_CONTRACT_REQUEST:
                {
                uint8 ucResponse;
                UAP_CONTR_REQ stContract;
                const uint8 * pData = pRcvData + 2;
                               
                if( CONTRACT_REQ_TYPE_NEW != pRcvData[1] )
                {
                    pData = DMAP_ExtractUint16( pData, &stContract.m_unContractID );
                }
                
                stContract.m_ucSrvcType = *(pData++);
                pData = DMAP_ExtractUint16( pData, &stContract.m_unSrcTLPort );
                memcpy( stContract.m_aDstAddr128, pData, 16 );
                pData += 16;
                pData = DMAP_ExtractUint16( pData, &stContract.m_unDstTLPort );
                
                pData ++; // skip negociation 
                
                pData = DMAP_ExtractUint32(pData, &stContract.m_ulContractLife );
                stContract.m_ucPriority = *(pData++);
                
                pData = DMAP_ExtractUint16( pData, &stContract.m_unMaxAPDUSize );  
                
                stContract.m_ucReliability = *(pData++);
                
                pData = DMO_ExtractBandwidthInfo( pData, &stContract.m_stBandwidth, stContract.m_ucSrvcType );
                
                if( (pData - pRcvData) == p_pMsgFrame->MessageSize ) 
                {
                    ucResponse = DMO_RequestContract(   pRcvData[1],
                                                        stContract.m_unContractID,
                                                        stContract.m_aDstAddr128, 
                                                        stContract.m_unDstTLPort, 
                                                        stContract.m_unSrcTLPort,
                                                        stContract.m_ulContractLife,
                                                        stContract.m_ucSrvcType,
                                                        stContract.m_ucPriority,
                                                        stContract.m_unMaxAPDUSize,
                                                        stContract.m_ucReliability,
                                                        pRcvData[0],  // UPA contract request ID
                                                        &stContract.m_stBandwidth );
                }
                else
                {
                    ucResponse = SFC_INVALID_SIZE;
                }
                
                if (SFC_SUCCESS == ucResponse) 
                {
                    SendMessage(API_ACK, DATA_OK, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                }
                else if (SFC_DUPLICATE == ucResponse) // contract already exists, build a fake contract response 
                {
                    uint8 aContrRsp[32];
                    uint8 * pucTemp = aContrRsp;
                    DMO_CONTRACT_ATTRIBUTE *pContract = DMO_GetContract( stContract.m_aDstAddr128, 
                                                                        stContract.m_unDstTLPort, 
                                                                        stContract.m_unSrcTLPort, 
                                                                        stContract.m_ucSrvcType);
                    *(pucTemp++) = pRcvData[0]; // req ID
                    *(pucTemp++) = 0;           // success with immediate effect
                    
                    pucTemp = DMAP_InsertUint16(pucTemp, pContract->m_unContractID);   
                    *(pucTemp++) = pContract->m_ucServiceType; 
                    pucTemp = DMAP_InsertUint32(pucTemp, pContract->m_ulAssignedExpTime);
                    *(pucTemp++) = pContract->m_ucPriority;   
                    pucTemp = DMAP_InsertUint16(pucTemp, pContract->m_unAssignedMaxNSDUSize);   
                    *(pucTemp++) = pContract->m_ucReliability;
                    
                    pucTemp = DMO_InsertBandwidthInfo(  pucTemp, 
                                                      &pContract->m_stBandwidth, 
                                                      pContract->m_ucServiceType );     
                    
                    SendMessage(STACK_SPECIFIC, ISA100_NOTIFY_ADD_CONTRACT, API_RSP, p_pMsgFrame->MessageID, pucTemp - aContrRsp, aContrRsp );
                }
                else 
                {
                    SendMessage(API_NACK, NACK_STACK_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                }
                }
                break;
            
            case ISA100_TL_APDU_REQUEST:      
                ucMessageSize = p_pMsgFrame->MessageSize;
                {
                    uint16 unContractId =  (((uint16)(pRcvData[4])) << 8) | pRcvData[5];
                    if (  ( ucMessageSize > 6 )
                        && (unContractId != g_unSysMngContractID)
                        && ( SFC_SUCCESS == ASLSRVC_AddGenericObject(
                                                                    pRcvData + 6,
                                                                    pRcvData[2],   // ServiceType
                                                                    pRcvData[3],   // priority
                                                                    pRcvData[0],   // SrcTSAPID
                                                                    pRcvData[1],   // DstTSAPID
                                                                     unContractId,// contract ID
                                                                     ucMessageSize-6 ) ) )
                    {
                        SendMessage(API_ACK, DATA_OK, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                    }
                    else 
                    {
                        SendMessage(API_NACK, NACK_STACK_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                    }
                }
                break;
            
            case ISA100_CONTRACT_TERMINATE:
                if( SFC_SUCCESS == DMO_RequestContractTermination( (uint16)(pRcvData[0] << 8) | pRcvData[1],
                                                                    pRcvData[2]) )
                {
                    SendMessage(API_ACK, DATA_OK, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                }
                else
                {
                    SendMessage(API_NACK, NACK_STACK_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                }
                break;
            
            case ISA100_DMAP_ACCESS:
              {
                uint8 ucCrtJoinCommand = g_stDMO.m_ucJoinCommand;
                uint8 aucRsp[MAX_API_BUFFER_SIZE - sizeof(ApiMessageFrame)];                
                ucMessageSize = sizeof(aucRsp);
                
                if( SFC_SUCCESS == API_accessDMAP( pRcvData, p_pMsgFrame->MessageSize, aucRsp, &ucMessageSize ) )
                {
                    SendMessage( STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, ucMessageSize, aucRsp);
                    
                    //check if the reset was triggered by the DMAP_ACCESS command  
                    if( ucCrtJoinCommand != g_stDMO.m_ucJoinCommand )
                    {
                        //reset must be apply instantly - when the DAQ will receive the API DMAP_ACCESS response the reset is already applied by Radio Modem   
                        DMO_PerformJoinCommandReset();
                    }
                }
                else
                {            
                    SendMessage( API_NACK, NACK_STACK_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                }
              }
            break;
            
            case ISA100_ADD_UAP_ALARM:
                API_onRcvUAPAlarm( pRcvData, p_pMsgFrame->MessageSize, pRcvData );
                SendMessage(STACK_SPECIFIC, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 2, pRcvData);
                break;
                
             case ISA100_NOTIFY_MALFORMED_APDU:
                //pRcvData -> Source IPv6 address of the APDU in MSB format    
                ASLMO_AddMalformedAPDU(pRcvData);   
                SendMessage(API_ACK, DATA_OK, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
                break;
            
            case ISA100_SECURITY_LEVEL:
                pRcvData[0] = g_stDSMO.m_ucDLSecurityLevel;
                pRcvData[1] = g_stDSMO.m_ucTLSecurityLevel;
                SendMessage(STACK_SPECIFIC, ISA100_SECURITY_LEVEL, API_RSP, p_pMsgFrame->MessageID, 2, pRcvData);
                break;
                
            case ISA100_LINK_QUALITY:
                pRcvData[0] = DLME_GetStrongestNeighbor();
                SendMessage(STACK_SPECIFIC, ISA100_LINK_QUALITY, API_RSP, p_pMsgFrame->MessageID, 1, pRcvData);
                break;
                
            default:
                return 0;
        }  
    }
    else
    {
        switch (p_pMsgFrame->MessageType)
        {      
            // Custom/Specialized attributes
            case ISA100_GET_INITIAL_INFO: // power type, power level, uap ID, routing type
                g_ucUapId = pRcvData[2];    
                
    #if defined (ROUTING_SUPPORT)
                if( pRcvData[3] <= DEVICE_ROLE )
                {
                    g_ucDAQDeviceRole = (pRcvData[3] ? 0x03: 0x01);
                    g_stDPO.m_unDeviceRole = g_ucDAQDeviceRole;    
                }
    #endif
                break;
                
            case ISA100_POWER_LEVEL:
                if( PWR_STATUS_3 >= pRcvData[0] )
                {
                    g_stDMO.m_ucPWRStatus = pRcvData[0];
                }
                break;
            
            default:
                return 0;
        }  
    }
    
    return 1;
}
#endif

uint8 API_onRcvApiCommands( ApiMessageFrame * p_pMsgFrame )
{
    uint8 * pRcvData = (uint8*)(p_pMsgFrame+1);
    if( !p_pMsgFrame->MessageHeader.m_bIsRsp ) // is request
    {
        switch (p_pMsgFrame->MessageType)
        {
        case API_HW_PLATFORM:
            pRcvData[0] = API_PLATFORM >> 8;
            pRcvData[1] = API_PLATFORM;
            SendMessage(API_COMMANDS, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 2, pRcvData );
            break;        
          
        case API_FW_VERSION:
            pRcvData[0] = API_VERSION >> 8;
            pRcvData[1] = API_VERSION;
            SendMessage(API_COMMANDS, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 2, pRcvData );
            break;        
          
        case API_MAX_BUFFER_SIZE: 
            pRcvData[0] = 0;
            pRcvData[1] = MAX_API_BUFFER_SIZE;
            SendMessage(API_COMMANDS, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 2, pRcvData );
            break;        
    
    #if (SPI1_MODE != NONE)
        case API_MAX_SPI_SPEED:
            pRcvData[0] = MAX_SPI_SPEED;
            SendMessage(API_COMMANDS, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 1, pRcvData );
            break;        
          
        case API_UPDATE_SPI_SPEED:
            if (API_PHY_SetSpeed((enum SPI_SPEED)pRcvData[0]) == SFC_SUCCESS){
                SendMessage(API_ACK, API_CHANGE_ACCEPTED, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
            }
            else {
                SendMessage(API_NACK, NACK_API_COMMAND_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
            }
            break;
    
        #ifndef WAKEUP_ON_EXT_PIN
        case API_HEARTBEAT_FREQ:
            if (DAQ_UpdateHeartBeatFreq((enum HEARTBEAT_FREQ)pRcvData[0]) == SFC_SUCCESS){
                SendMessage(API_ACK, API_CHANGE_ACCEPTED, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
            }
            else {
                SendMessage(API_NACK, NACK_API_COMMAND_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
            }
            break;
        #endif
    #endif
    
    #if ( UART2_MODE != NONE )
        case API_UPDATE_UART_SPEED:
            if( UART2_UpdateSpeed( pRcvData[0] ) == SFC_SUCCESS )
            {
                SendMessage(API_ACK, API_CHANGE_ACCEPTED, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
            }
            else {
                SendMessage(API_NACK, NACK_API_COMMAND_ERROR, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
            }
            break;
            
        case API_MAX_UART_SPEED:
            pRcvData[0] = MAX_UART_SPEED;
            SendMessage(API_COMMANDS, p_pMsgFrame->MessageType, API_RSP, p_pMsgFrame->MessageID, 1, pRcvData );
            break;        
    #endif
    
        default:
            return 0;
        }
    }
    else // is response
    {
        return 0;
    }
    
    return 1;
}

/****************
 * GetMessage
 ****************/
void API_OnRcvMsg( ApiMessageFrame * p_pMsgFrame )
{
    uint8 ucParseFlag = 0;
    switch( p_pMsgFrame->MessageHeader.MessageClass )
    {
#if ( _UAP_TYPE == UAP_TYPE_SIMPLE_API )
    case DATA_PASSTHROUGH: 
        break;
#endif    
    
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
    case STACK_SPECIFIC:   ucParseFlag = API_onRcvStackSpecific( p_pMsgFrame ); break;
#endif
    
    case API_COMMANDS:     ucParseFlag = API_onRcvApiCommands( p_pMsgFrame ); break;
    }
    
    if( !ucParseFlag && !p_pMsgFrame->MessageHeader.m_bIsRsp )
    {
        SendMessage(API_NACK, NACK_UNSUPPORTED_FEATURE, API_RSP, p_pMsgFrame->MessageID, 0, NULL);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  This function allows full acces to the DMAP through RADIO API
/// @param  p_pRcvData - pointer to a buffer that containss concatenated APDUs
/// @param  p_ucMsgLen - lenght of the p_pRcvData buffer
/// @param  p_pRspData - pointer to a buffer where to put response APDUs (concatenated)
/// @param  p_pucRspLen- in/out parameter; pointer to an uint8 data tha for input
///                      represents the size of the p_pRspData; for ouput represents the length of the
///                      response APDUs (concatenated)
/// @return SFC_SUCCESS if at least one succesfull response, SFC_FAILURE otherwise
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 API_accessDMAP( uint8 * p_pRcvData, uint8 p_ucMsgLen, uint8 * p_pRspData, uint8 * p_pucRspLen  )
{
  GENERIC_ASL_SRVC  stGenSrvcReq, stGenSrvcRsp;
  uint16 unRspBufSize = *p_pucRspLen;  
  uint8 aucSrvcBuf[DAQ_BUFFER_SIZE];
  uint8 * pNext = p_pRspData;  
  
  *p_pucRspLen = 0;
  
  const uint8 * pAPDU = p_pRcvData;
  
 g_ucIsOOBAccess = 1;
    
  while ( pAPDU = ASLSRVC_GetGenericObject( pAPDU,
                                            p_ucMsgLen - (pAPDU - p_pRcvData),
                                            &stGenSrvcReq,
                                            NULL) )
  {   
      uint8 ucFlag = SFC_SUCCESS;
      switch ( stGenSrvcReq.m_ucType )
      {
      case SRVC_READ_REQ  : 
              stGenSrvcRsp.m_ucType = SRVC_READ_RSP;      
              stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_pRspData = aucSrvcBuf; 
              stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_unLen  = sizeof( aucSrvcBuf);
              stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_unDstOID = stGenSrvcReq.m_stSRVC.m_stReadReq.m_unSrcOID;
              stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_unSrcOID = stGenSrvcReq.m_stSRVC.m_stReadReq.m_unDstOID;
              stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_ucReqID  = stGenSrvcReq.m_stSRVC.m_stReadReq.m_ucReqID;
              
              stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_ucSFC = GetGenericAttribute( UAP_DMAP_ID,
                                                                               stGenSrvcReq.m_stSRVC.m_stReadReq.m_unDstOID,
                                                                               &stGenSrvcReq.m_stSRVC.m_stReadReq.m_stAttrIdtf,
                                                                               stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_pRspData, 
                                                                               &stGenSrvcRsp.m_stSRVC.m_stReadRsp.m_unLen );   
              break;
      case SRVC_WRITE_REQ : 
              stGenSrvcRsp.m_ucType = SRVC_WRITE_RSP; 
              stGenSrvcRsp.m_stSRVC.m_stWriteRsp.m_unDstOID = stGenSrvcReq.m_stSRVC.m_stWriteReq.m_unSrcOID;
              stGenSrvcRsp.m_stSRVC.m_stWriteRsp.m_unSrcOID = stGenSrvcReq.m_stSRVC.m_stWriteReq.m_unDstOID;
              stGenSrvcRsp.m_stSRVC.m_stWriteRsp.m_ucReqID  = stGenSrvcReq.m_stSRVC.m_stWriteReq.m_ucReqID;
              
              if( g_stDPO.m_ucAllowOOBProvisioning ) // allows direct access removal for OOB if somebod
              {                 
                  stGenSrvcRsp.m_stSRVC.m_stWriteRsp.m_ucSFC = SetGenericAttribute( UAP_DMAP_ID,
                                                                                    stGenSrvcReq.m_stSRVC.m_stWriteReq.m_unDstOID,
                                                                                    &stGenSrvcReq.m_stSRVC.m_stWriteReq.m_stAttrIdtf,
                                                                                    stGenSrvcReq.m_stSRVC.m_stWriteReq.p_pReqData,
                                                                                    stGenSrvcReq.m_stSRVC.m_stWriteReq.m_unLen );
              }
              else
              {
                  stGenSrvcRsp.m_stSRVC.m_stWriteRsp.m_ucSFC = SFC_OBJECT_ACCESS_DENIED;
              }
              break;
              
      case SRVC_EXEC_REQ  : 
              stGenSrvcRsp.m_ucType = SRVC_EXEC_RSP;  
              stGenSrvcRsp.m_stSRVC.m_stExecRsp.p_pRspData  = aucSrvcBuf;                  
              stGenSrvcRsp.m_stSRVC.m_stExecRsp.m_unDstOID = stGenSrvcReq.m_stSRVC.m_stExecReq.m_unSrcOID;
              stGenSrvcRsp.m_stSRVC.m_stExecRsp.m_unSrcOID = stGenSrvcReq.m_stSRVC.m_stExecReq.m_unDstOID;
              stGenSrvcRsp.m_stSRVC.m_stExecRsp.m_ucReqID  = stGenSrvcReq.m_stSRVC.m_stExecReq.m_ucReqID;

              if( g_stDPO.m_ucAllowOOBProvisioning ) // allows direct access removal for OOB if somebod
              {                   
                  DMAP_ExecuteGenericMethod(  &stGenSrvcReq.m_stSRVC.m_stExecReq, 
                                              NULL, 
                                              &stGenSrvcRsp.m_stSRVC.m_stExecRsp ); 
              }
              else
              {
                  stGenSrvcRsp.m_stSRVC.m_stExecRsp.m_unLen = 0;
                  stGenSrvcRsp.m_stSRVC.m_stExecRsp.m_ucSFC = SFC_OBJECT_ACCESS_DENIED;
              }
              break;
              
      default:
             ucFlag = SFC_FAILURE;
      }

      if (ucFlag == SFC_SUCCESS)
      {
          pNext = ASLSRVC_SetGenericObj(  stGenSrvcRsp.m_ucType,
                                          &stGenSrvcRsp.m_stSRVC.m_stReadRsp,
                                          unRspBufSize - (pNext - p_pRspData),
                                          pNext );   
          if( !pNext ) break; // buffer is full           
      }
  }

  uint8 ucNeedFlashDPOInfo = g_ucIsOOBAccess;
  
  g_ucIsOOBAccess = 0;
  
  if( ucNeedFlashDPOInfo == 7 )
  {
      DPO_FlashDPOInfo();      
  }
  else if( ucNeedFlashDPOInfo == 3 )
  {
      DPO_SavePersistentData();
  }
  
  *p_pucRspLen = pNext - p_pRspData;             

  if (*p_pucRspLen)
      return SFC_SUCCESS;
  
  return SFC_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Parses a recieved alarm through ISA API interface, and adds the alarm to the ARMO queue
/// @param  p_pRcvData - pointer to a buffer that containss concatenated APDUs
/// @param  p_ucMsgLen - lenght of the p_pRcvData buffer
/// @param  p_pucAlertID - pointer to a buffer where to put API response
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void API_onRcvUAPAlarm( uint8 * p_pRcvData, uint8 p_ucMsgLen, uint8 * p_pucAlertID )
{
  /* 1 byte UAP, 2 bytes Object ID, 1 byte class, 1 byte direction, 1 byte category, 1 byte type, 1 byte priority  */  
  
  if( p_ucMsgLen < 8 ) 
  {
      *p_pucAlertID = 1;
      *(p_pucAlertID + 1) = 0;
      return;
  }

  ALERT stAlert;
  
  stAlert.m_unSize = p_ucMsgLen - 8;
  stAlert.m_unDetObjTLPort = 0xF0B0 + *(p_pRcvData++); 
  stAlert.m_unDetObjID = *(p_pRcvData++) << 8;
  stAlert.m_unDetObjID |= *(p_pRcvData++);
  stAlert.m_ucClass     = *(p_pRcvData++);
  stAlert.m_ucDirection = *(p_pRcvData++);  
  stAlert.m_ucCategory  = *(p_pRcvData++) & 0x03;  
  stAlert.m_ucType      = *(p_pRcvData++);  
  stAlert.m_ucPriority  = *(p_pRcvData++);           
         
  
  *p_pucAlertID = 0;
  *(p_pucAlertID + 1) = (stAlert.m_ucCategory << 6) | (g_aARMO[stAlert.m_ucCategory].m_ucAlertsNo & 0x3F);
  if( SFC_SUCCESS != ARMO_AddAlertToQueue( &stAlert, p_pRcvData ) )
  {
    *p_pucAlertID = 1;
  }
}

#endif // _UAP_TYPE
