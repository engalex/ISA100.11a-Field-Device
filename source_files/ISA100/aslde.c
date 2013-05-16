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
/// Date:         January 2008
/// Description:  This file implements the application layer queue
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "string.h"
#include "aslde.h"
#include "aslsrvc.h"
#include "tlde.h"
#include "dmap.h"
#include "dmap_dmo.h"
#include "dmap_co.h"
#include "dmap_armo.h"
#include "uap.h"
#include "../CommonAPI/Common_API.h"
#if defined(BACKBONE_SUPPORT)
  #include "uart_link.h"
#endif

#define SM_ADDR_HI_BYTE (((uint8*)&g_stDMO.m_unSysMngShortAddr)[1])
#define SM_ADDR_LO_BYTE (((uint8*)&g_stDMO.m_unSysMngShortAddr)[0])


#if defined( BACKBONE_SUPPORT ) && (DEVICE_TYPE == DEV_TYPE_MC13225)
  #define APP_HANDLE_SELECTOR 0x8000
#else
  #define APP_HANDLE_SELECTOR 0

#endif

/*  RX/TX common APDU buffer  */
#pragma data_alignment=4        //to work also with 32 bits processors
uint8  g_aucAPDUBuff[COMMON_APDU_BUF_SIZE];

uint8 * g_pAslMsgQEnd = g_aucAPDUBuff;

/* APDU confirmation array  */
static APDU_CONFIRM_IDTF g_astAPDUConf[MAX_APDU_CONF_ENTRY_NO];
static uint8  g_ucAPDUConfNo;

uint8 g_ucHandle = 0;

/* Tx  APDU buffer; used by TL also! */
uint8  g_oAPDU[MAX_APDU_SIZE + 4]; // add 4 bytes for MIC

/* ASLMO object definition and ASLMO alert counters.  */
ASLMO_STRUCT  g_stASLMO;
const uint16  c_unMaxASLMOCounterNo = MAX_ASLMO_COUNTER_N0;  // number of devices for which counts can be simultaneously maintained

/* ASLMO counters */
MALFORMED_APDU_COUNTER g_astASLMOCounters[MAX_ASLMO_COUNTER_N0];
uint16                 g_unCounterNo;    

const DMAP_FCT_STRUCT c_aASLMOFct[ASLMO_ATTR_NO] = {
   0,   0,                                              DMAP_EmptyReadFunc,       NULL,  
   ATTR_CONST(g_stASLMO.m_bMalformedAPDUAdvise),        DMAP_ReadUint8,           DMAP_WriteUint8,   
   ATTR_CONST(g_stASLMO.m_ulCountingPeriod),            DMAP_ReadUint32,          DMAP_WriteUint32,   
   ATTR_CONST(g_stASLMO.m_unMalformedAPDUThreshold),    DMAP_ReadUint16,          DMAP_WriteUint16,     
   ATTR_CONST(g_stASLMO.m_stAlertDescriptor),           DMAP_ReadAlertDescriptor, DMAP_WriteAlertDescriptor,   
   ATTR_CONST(c_unMaxASLMOCounterNo),                   DMAP_ReadUint16,          DMAP_WriteUint16
};


/***************************** local functions ********************************/

static uint8 ASLDE_searchHighestPriority(APDU_IDTF * p_pIdtf);

static uint8 * ASLDE_concatenateAPDUs(  uint16      p_unContractID,
                                        uint8 *     p_pOutBuf,
                                        uint8 *     p_pOutBufEnd,
                                        uint8       p_ucHandle  );

static uint8 * ASLDE_getEUI64DestAPDU ( APDU_IDTF * p_stAPDUIdtf,
                                        uint8 *     p_pOutBuf,
                                        uint8 *     p_pOutBufEnd,
                                        uint8       p_ucHandle );

static void ASLDE_flushAPDUBuffer(void);

static void ASLDE_checkTLConfirms(void);

static void ASLDE_cleanAPDUBuffer(void);

static uint8 ASLMO_reset(uint8 p_ucResetType);

void ASLDE_loadIDTF(APDU_IDTF * p_pIdtf, const ASL_QUEUE_HDR * p_pHdr );

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Initializes the common Rx/Tx APDU buffer and the confirmation buffer
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_Init(void)
{
  g_pAslMsgQEnd = g_aucAPDUBuff;
  g_ucAPDUConfNo = 0;
  
  // init aslmo object and counters
  memset(&g_stASLMO, 0, sizeof(g_stASLMO));
  memset(&g_astASLMOCounters, 0, sizeof(g_astASLMOCounters));
  //g_ulResetCounter = g_stASLMO.m_ulCountingPeriod; // default counting period is zero
  
  // all ASLMO attributes default values are zero, except the alert descriptor priority
  g_stASLMO.m_stAlertDescriptor.m_ucPriority = ALERT_PR_MEDIUM_M;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Performs application layer message management (app queue state machine)
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_ASLTask(void)
{
  ASLDE_checkTLConfirms();
  ASLDE_cleanAPDUBuffer();
  ASLDE_flushAPDUBuffer();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Mark an APDU to delete
/// @param  p_pAPDU - pointer to the APDU
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_MarkForDeleteAPDU(uint8 *  p_pAPDU)
{
  p_pAPDU -= sizeof(ASL_QUEUE_HDR); // point to the management header of the APDU

//  if ( (p_pAPDU < g_aucAPDUBuff) || ((p_pAPDU + ((ASL_QUEUE_HDR*)p_pAPDU)->m_unLen) > g_pAslMsgQEnd) )
//      return; // p_pAPDU is out of valida data range

  ((ASL_QUEUE_HDR*)p_pAPDU)->m_ucStatus = APDU_READY_TO_CLEAR;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches for an RX message that belongs to the requester's SAP ID
/// @param  p_ucTSAPID - SAP ID of the caller
/// @param  p_pIdtf    - output: highest priority APDU information
/// @return pointer to the received TSDU
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * ASLDE_GetMyAPDU(  uint8       p_ucTSAPID,
                          APDU_IDTF * p_pIdtf)

{
  uint8* pIdx;

  for (pIdx = g_aucAPDUBuff;
       pIdx < g_pAslMsgQEnd;
       pIdx += ((ASL_QUEUE_HDR*)pIdx)->m_unLen)
  {
      ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pIdx;

      // search for an RX TSDU
      if ( pHdr->m_ucDirection == RX_APDU
          && pHdr->m_ucDstSAP == p_ucTSAPID
          && pHdr->m_ucStatus == APDU_NOT_PROCESSED )
      {
          ASLDE_loadIDTF( p_pIdtf, pHdr );           
          
          // check for malformed APDU; object addressing of first APDU cannot be inferred
          uint8 ucHeader = *(pIdx + sizeof(ASL_QUEUE_HDR)) & OBJ_ADDRESSING_TYPE_MASK;
          if( p_pIdtf->m_ucAddrLen == 16 && (ucHeader == OID_INFERRED << 5) )
          {              
              ASLMO_AddMalformedAPDU(p_pIdtf->m_aucAddr);   
              ((ASL_QUEUE_HDR*)pIdx)->m_ucStatus = APDU_READY_TO_CLEAR;            
          }
          else
          {
              return (pIdx + sizeof(ASL_QUEUE_HDR));
          }
      }
  }

  return NULL; // nothing Rx TSDU found
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches for a TX message for a specified contract
/// @param  p_unContractID  - the contract ID
/// @return 
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_DiscardAPDU( uint16 p_unContractID )
{
  uint8 * pData = g_aucAPDUBuff;

  // check timeout of APDUs in the ASL queue
  while (pData < g_pAslMsgQEnd)
  {
      if( ((ASL_QUEUE_HDR*)pData)->m_ucDirection == TX_APDU
          && 2 == ((ASL_QUEUE_HDR*)pData)->m_ucPeerAddrLen
          && ((ASL_QUEUE_HDR*)pData)->m_stInfo.m_stTx.m_stDest.m_unContractId == p_unContractID )
      {               
          
          if( ((ASL_QUEUE_HDR*)pData)->m_ucStatus == APDU_SENT_WAIT_JUST_TL_CNF )
          {
              // if the APDU is already on the DLL queue, try to discard that message also
              DLDE_DiscardMsg( ((ASL_QUEUE_HDR*)pData)->m_stInfo.m_stTx.m_ucConcatHandle );
          }
          
          ((ASL_QUEUE_HDR*)pData)->m_ucStatus = APDU_READY_TO_CLEAR;  
          
          return; 
      }
      
      pData += ((ASL_QUEUE_HDR*)pData)->m_unLen;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches for a TX message that belongs to the requester's SAP ID
/// @param  p_ucReqID       - the request ID of the APDU that has to be found
/// @param  p_unDstObjID    - destination objID, to which the request was sent
/// @param  p_unSrcObjID    - destination objID, from which the request was sent
/// @param  p_pAPDUIdtf     - pointer to the identifier of response APDU
/// @param  p_pLen          - output parameter: pointer to length of APDU
/// @return pointer to the APDU or NULL if not found
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * ASLDE_SearchOriginalRequest( uint8        p_ucReqID,
                                     uint16       p_unDstObjID,
                                     uint16       p_unSrcObjID,
                                     APDU_IDTF *  p_pAPDUIdtf,
                                     uint16 *     p_pLen )
{
  uint8* pIdx;

  for (pIdx = g_aucAPDUBuff;
       pIdx < g_pAslMsgQEnd;
       pIdx += ((ASL_QUEUE_HDR*)pIdx)->m_unLen)
  {
      ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pIdx;

      if ( pHdr->m_ucDirection == TX_APDU
          && pHdr->m_stInfo.m_stTx.m_ucReqID == p_ucReqID
          && pHdr->m_ucSrcSAP == p_pAPDUIdtf->m_ucDstTSAPID
          && pHdr->m_ucDstSAP == p_pAPDUIdtf->m_ucSrcTSAPID )
      {
      #ifndef PRECISE_REQID_SEARCH

          *p_pLen = pHdr->m_unAPDULen;
          // mark for delete also
          pHdr->m_ucStatus = APDU_READY_TO_CLEAR;
          return (pIdx + sizeof(ASL_QUEUE_HDR));

      #else

          if ( p_pAPDUIdtf->m_ucAddrLen == 8 && pHdr->m_ucPeerAddrLen == 8)
          {
            if ( !memcmp( p_pAPDUIdtf->m_aucAddr,
                          pHdr->m_stInfo.m_stTx.m_stDest.m_aucEUI64,
                          8 ) )
            {
                *p_pLen = pHdr->m_unAPDULen;
                // mark for delete also
                pHdr->m_ucStatus = APDU_READY_TO_CLEAR;
                return (pIdx + sizeof(ASL_QUEUE_HDR));
            }
          }
          else if ( p_pAPDUIdtf->m_ucAddrLen == 16 && pHdr->m_ucPeerAddrLen == 2)
          // no destination address available; contractID available
          {
              DMO_CONTRACT_ATTRIBUTE * pContract = DMO_FindContract(pHdr->m_stInfo.m_stTx.m_stDest.m_unContractId);

              if (pContract && !memcmp( pContract->m_aDstAddr128, p_pAPDUIdtf->m_aucAddr, 16))
              {
                  *p_pLen = pHdr->m_unAPDULen;
                  // mark for delete also
                  pHdr->m_ucStatus = APDU_READY_TO_CLEAR;
                  return (pIdx + sizeof(ASL_QUEUE_HDR));
              }
          }

      #endif // #ifndef (PRECISE_REQID_SEARCH)
      }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Computes the actual size of the queue (how many elements are occupied)
/// @param  none
/// @return the queue size
/// @remarks
///      Access level: user level\n
///      Context: Every UAP periodically must call this function
////////////////////////////////////////////////////////////////////////////////////////////////////
uint16 ASLDE_GetAPDUFreeSpace(void)
{
  return (g_aucAPDUBuff + sizeof(g_aucAPDUBuff) - g_pAslMsgQEnd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Manages the timeout counters of the queue APDUs, and ASLMO malformed APDU time intervals
/// @param  none
/// @return none
/// @remarks
///      Access level: user level\n
///      Context: Must be called every second
////////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_PerformOneSecondOperations(void)
{
    uint8 * pData = g_aucAPDUBuff;
    
    // check timeout of APDUs in the ASL queue
    while (pData < g_pAslMsgQEnd)
    {
        ASL_QUEUE_HDR * pMsgHdr = (ASL_QUEUE_HDR*)pData;
        if ( pMsgHdr->m_unTimeout )
        {
            pMsgHdr->m_unTimeout--;
        }
        else if ( pMsgHdr->m_ucDirection == RX_APDU )
        {   // delete; this APDU was not handled by any app process
            pMsgHdr->m_ucStatus = APDU_READY_TO_CLEAR;
        }
        else if( pMsgHdr->m_stInfo.m_stTx.m_ucRetryNo) // it is a retry TX APDU
        {   
            pMsgHdr->m_stInfo.m_stTx.m_ucRetryNo--;
            pMsgHdr->m_unTimeout = pMsgHdr->m_stInfo.m_stTx.m_unDefTimeout;
            pMsgHdr->m_ucStatus  = APDU_READY_TO_SEND;            
        }
        else // it is a expired TX APDU
        {   // all retries have failed; delete the APDU;
            pMsgHdr->m_ucStatus = APDU_READY_TO_CLEAR;
            // notify other UAPs about this failure;
            UAP_DataConfirm( pMsgHdr->m_stInfo.m_stTx.m_unAppHandle, SFC_TIMEOUT);
        }
        
        pData += ((ASL_QUEUE_HDR*)pData)->m_unLen;
    }
    
    // check ASLMO malformed APDU counters time interval    
    for (uint16 unIdx = 0; unIdx < g_unCounterNo; unIdx++)
    {                            
        // check ASLMO reset counter
        if (g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter)
        {
            if( g_astASLMOCounters[unIdx].m_ulResetCounter )
            {
                g_astASLMOCounters[unIdx].m_ulResetCounter--;     
            }
            else 
            {
                // the time interval from the first detected malformed APDU expired
                // without the threshold being met; ASL counter shall be reset to zero;
                g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter = 0;
            }
        }  
    }  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Searches for the highest priority APDU
/// @param  p_pIdtf  - attribute identifier structure
/// @return service feedback code
/// @remarks
///      Access level: user level\n
///      Context: Called by ASLDE_Concatenate();
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 ASLDE_searchHighestPriority(APDU_IDTF * p_pIdtf)
{
  const uint8 * pData = g_aucAPDUBuff;

  p_pIdtf->m_ucPriorityAndFlags = 0;
  p_pIdtf->m_ucAddrLen  = 0;

  while (pData < g_pAslMsgQEnd)
  {
      // search for unprocessed TX messages
      ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pData;

      if (pHdr->m_ucDirection == TX_APDU && pHdr->m_ucStatus == APDU_READY_TO_SEND)
      {
          if ( !p_pIdtf->m_ucAddrLen || ((pHdr->m_ucPriorityAndFlags & 0x0F) > (p_pIdtf->m_ucPriorityAndFlags & 0x0F)))
          {
              ASLDE_loadIDTF( p_pIdtf, pHdr );
          }
      }
      pData += pHdr->m_unLen;
  }

  if ( p_pIdtf->m_ucAddrLen )
  {
      return SFC_SUCCESS;
  }
  else
  {
      return SFC_ELEMENT_NOT_FOUND;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Attaches the confirmation status of the TL of an TSDU, to the corresponding APDUs in the
///         common Rx/Tx APDU buffer
/// @param  none
/// @return none
/// @remarks HANDLEs in ASL layer are in the 0x00 - 0xFF range\n
///      Access level: interrupt level
///////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_checkTLConfirms( void )
{
  APDU_CONFIRM_IDTF * pConf = g_astAPDUConf;
  uint8 * pData;

  while (pConf < (g_astAPDUConf + g_ucAPDUConfNo))
  {
      pData = g_aucAPDUBuff;

      while (pData < g_pAslMsgQEnd)
      {
          ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pData;

          if( (pHdr->m_ucStatus == APDU_SENT_WAIT_JUST_TL_CNF || pHdr->m_ucStatus == APDU_SENT_WAIT_CNF_AND_RESPONSE) && (pHdr->m_stInfo.m_stTx.m_ucConcatHandle == pConf->m_ucHandle) )
          {
              if ( pConf->m_ucConfirmStatus == SFC_SUCCESS )
              {
                  if ( pHdr->m_ucStatus == APDU_SENT_WAIT_JUST_TL_CNF )
                  {   // succesfully sent; no response expected; delete the APDU
                      
                      uint8 unContractType = SRVC_APERIODIC_COMM;
                      
                      if( 2 == pHdr->m_ucPeerAddrLen )
                      {
                        //contract is used
                        DMO_CONTRACT_ATTRIBUTE * pContract = DMO_FindContract(pHdr->m_stInfo.m_stTx.m_stDest.m_unContractId);  
                        if( pContract )
                        {
                            unContractType = pContract->m_ucServiceType;
                        }
                      }
                      
                      //filter the generated publish messages which can match with WriteResponse ReqID
                      if( SRVC_APERIODIC_COMM == unContractType )
                      {    
                          //RadioSleep exception 
                          if( (uint16)pHdr->m_stInfo.m_stTx.m_ucReqID == g_unRadioSleepReqId )
                          {
                              g_ulRadioSleepCounter = g_ulRadioSleep;
                              g_unRadioSleepReqId = 0xFFFF;    //RadioSleep is active 
                          }
                          
                          //JoinCommand exception
                          if( (uint16)pHdr->m_stInfo.m_stTx.m_ucReqID == g_unJoinCommandReqId )
                          {
                              g_unJoinCommandReqId = JOIN_CMD_APPLY_ASAP_REQ_ID;     
                          }
                      }
                      
                      pHdr->m_ucStatus = APDU_READY_TO_CLEAR;
                  }
                  else if ( pHdr->m_ucStatus == APDU_SENT_WAIT_CNF_AND_RESPONSE )
                  {   // succesfully sent; wait for the response
                      pHdr->m_ucStatus = APDU_CONFIRMED_WAIT_RSP;
                  }
              }
              else 
              {   
#if defined(UART_MSG_TYPE_DEBUG)
                  uint8 ucDbgInfo[ 2+sizeof(ASL_QUEUE_HDR) ];
                  
                  ucDbgInfo[0] = 0;
                  ucDbgInfo[1] = pConf->m_ucConfirmStatus;
                  memcpy( ucDbgInfo+2, pHdr, sizeof(*pHdr) );
                  
                  UART_LINK_AddMessage( UART_MSG_TYPE_DEBUG, 0, 0, NULL, sizeof(ucDbgInfo), ucDbgInfo ); 
#endif
                  if( SFC_NO_CONTRACT == pConf->m_ucConfirmStatus && 2 == pHdr->m_ucPeerAddrLen)
                  {
                      //check if publishing contract                      
                      CO_STRUCT* pstCO = FindConcentratorByContract(pHdr->m_stInfo.m_stTx.m_stDest.m_unContractId);
                      if( pstCO )
                      {
                        pstCO->m_stContract.m_ucStatus = CONTRACT_REQUEST_TERMINATION;
                      }
                      //if no contract the message will be discarded - no retry needed 
                      pHdr->m_ucStatus = APDU_READY_TO_CLEAR;
                      
                  }
                  else
                  {
                      // let the APDU retry functionality handle this error within 2 seconds
                      pHdr->m_unTimeout = 2;
                  }
              }
              
              // notify UAPs about transmission's confirm status
              UAP_DataConfirm( pHdr->m_stInfo.m_stTx.m_unAppHandle, pConf->m_ucConfirmStatus );
          }
          pData += pHdr->m_unLen;
      }
      pConf++;
  }
  g_ucAPDUConfNo = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Deletes the TX APDUs marked as APDU_READY_TO_CLEAR
/// @param  none
/// @return none
/// @remarks HANDLEs in ASL layer are in the 0x00 - 0xFF range\n
///      Access level: interrupt level
///////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_cleanAPDUBuffer(void)
{
  uint8* pData = g_aucAPDUBuff;

  while (pData < g_pAslMsgQEnd)
  {
      ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pData;

      uint16 unLen = pHdr->m_unLen;

      if ( pHdr->m_ucStatus == APDU_READY_TO_CLEAR )
      {
          g_pAslMsgQEnd -= unLen;
          memmove( pData,
                   pData + unLen,
                   (uint16)(g_pAslMsgQEnd - pData));

      }
      else
      {
          pData += unLen;
      }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Handles a application layer queue element, constructs the APDU and commits it to the 
///         Transport Layer
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
///////////////////////////////////////////////////////////////////////////////////////////////////
void ASLDE_flushAPDUBuffer(void)
{
  uint8 * pEnd; 
  APDU_IDTF stAPDUIdtf;

  if ( SFC_SUCCESS != ASLDE_searchHighestPriority(&stAPDUIdtf) )
      return; // nothing found

  uint16  unContractID;  
  uint8 * pEUI64 = NULL;
  
  if ( stAPDUIdtf.m_ucAddrLen == 2 )  // contract based APDU
  {
      unContractID = *(uint16*)stAPDUIdtf.m_aucAddr;

      // concatenate all APDUs for on same contract
      pEnd =  ASLDE_concatenateAPDUs( unContractID,
                                          g_oAPDU,
                                          g_oAPDU + MAX_APDU_SIZE,
                                          g_ucHandle);

  }
  else // EUI64 based APDU
  {
      // extract (copy) the EUI64 based APDU from APDU queue (could be a Backbone Join Request
      // or a field device join response.

      pEUI64 = stAPDUIdtf.m_aucAddr;
      unContractID = 0;
      pEnd = ASLDE_getEUI64DestAPDU ( &stAPDUIdtf,
                                        g_oAPDU,
                                        g_oAPDU + MAX_APDU_SIZE,
                                        g_ucHandle);
      

  }
  //if ( pEnd != NULL ) // not necessary; it should always be an APDU if ASLDE_searchHighestPriority succesfull
  //{

      TLDE_DATA_Request( pEUI64,                          // destination EUI64 ADDR
                         unContractID,                    // contract ID
                         stAPDUIdtf.m_ucPriorityAndFlags, // priority
                         pEnd-g_oAPDU,                    // data length
                         g_oAPDU,                         // app data
                         (g_ucHandle++) | APP_HANDLE_SELECTOR,                // handle
                         stAPDUIdtf.m_ucSrcTSAPID,        // src port
                         stAPDUIdtf.m_ucDstTSAPID         // dst port
                       );
  //}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Copies (concatenates) more APDU's into a single TSDU
/// @param  p_unContractID - contract identifier
/// @param  p_pOutBuf - output buffer
/// @param  p_pOutBufEnd - pointer to the output buffer's end
/// @param  p_ucHandle - concatenation handler
/// @return output buffer
/// @remarks
///      Access level: user level
///////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * ASLDE_concatenateAPDUs( uint16      p_unContractID,
                                uint8 *     p_pOutBuf,
                                uint8 *     p_pOutBufEnd,
                                uint8       p_ucHandle  )
{
  uint8 * pData = g_aucAPDUBuff;

  while (pData < g_pAslMsgQEnd)
  {
      // search for APDUs matching this contract
      ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pData;

      if ( pHdr->m_ucDirection == TX_APDU
            && pHdr->m_ucPeerAddrLen == 2
            && pHdr->m_ucStatus == APDU_READY_TO_SEND )
      {
          if ( p_unContractID == pHdr->m_stInfo.m_stTx.m_stDest.m_unContractId )
          {
              if ( pHdr->m_unAPDULen <= (p_pOutBufEnd - p_pOutBuf) )
              {
                  uint8 ucAPDUByte = *(pData + sizeof(ASL_QUEUE_HDR)); // first byte of the APDU header
                  uint8 ucServType = ucAPDUByte & 0x1F;
                  pHdr->m_stInfo.m_stTx.m_ucConcatHandle = p_ucHandle;        
                  
                  /* **** Object Addressing byte (first byte of the APDU header) ********************/
                  /*             b7                  |       b6 b5    |    b4 b3 b2 b1 b0           */
                  /* srvc primitive: 0=req, 1 = resp |  obj addr mode |    ASL srvc type            */                  
                  
                  if( (ucAPDUByte & 0x80)                       // response service primitive type
                      || (ucServType == SRVC_TYPE_ALERT_REP)    // alert report service type
                      || (ucServType == SRVC_TYPE_ALERT_ACK)    // alert acknowledge service type
                      || (ucServType == SRVC_TYPE_PUBLISH) )    // publish service type
                  {
                      pHdr->m_ucStatus = APDU_SENT_WAIT_JUST_TL_CNF;  // just wait the below layer confirm and delete the APDU from the queue
                  }
                  else
                  {
                      pHdr->m_ucStatus = APDU_SENT_WAIT_CNF_AND_RESPONSE;
                  }

                  memcpy(p_pOutBuf,
                         pData+sizeof(ASL_QUEUE_HDR),
                         pHdr->m_unAPDULen);

                  p_pOutBuf += pHdr->m_unAPDULen;
              }
              else return p_pOutBuf; // there is no more space in the output buffer; concatenate next time
          }
      }
      pData += pHdr->m_unLen;
  }

  return p_pOutBuf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Copies (concatenates) more APDU's into a single TSDU
/// @param  p_stAPDUIdtf - APDU identifier structure
/// @param  p_pOutBuf - output buffer
/// @param  p_pOutBufEnd - pointer to the output buffer's end
/// @param  p_ucHandle - concatenation handler
/// @return output buffer
/// @remarks
///      Access level: user level
///////////////////////////////////////////////////////////////////////////////////////////////////
uint8 * ASLDE_getEUI64DestAPDU ( APDU_IDTF * p_stAPDUIdtf,
                                 uint8 *     p_pOutBuf,
                                 uint8 *     p_pOutBufEnd,
                                 uint8       p_ucHandle )
{

  uint8 * pData = g_aucAPDUBuff;

  while (pData < g_pAslMsgQEnd)
  {
      // search for APDUs matching this contract
      ASL_QUEUE_HDR * pHdr = (ASL_QUEUE_HDR*)pData;

      if ( pHdr->m_ucDirection == TX_APDU
            && pHdr->m_ucPeerAddrLen == 8
            && pHdr->m_ucStatus == APDU_READY_TO_SEND
            && pHdr->m_ucPriorityAndFlags == p_stAPDUIdtf->m_ucPriorityAndFlags )
      {
          if ( !memcmp( p_stAPDUIdtf->m_aucAddr,
                        pHdr->m_stInfo.m_stTx.m_stDest.m_aucEUI64,
                        8)
              )
          {
              if ( pHdr->m_unAPDULen <= (p_pOutBufEnd - p_pOutBuf) )
              {
                  pHdr->m_stInfo.m_stTx.m_ucConcatHandle = p_ucHandle;

                  if ( *(pData + sizeof(ASL_QUEUE_HDR)) & 0x80) // response service type; only client/server services are supported for EUI64 addressing
                  {
                      pHdr->m_ucStatus = APDU_SENT_WAIT_JUST_TL_CNF;
                  }
                  else
                  {
                      pHdr->m_ucStatus = APDU_SENT_WAIT_CNF_AND_RESPONSE;
                  }

                  memcpy( p_pOutBuf,
                          pData + sizeof(ASL_QUEUE_HDR),
                          pHdr->m_unAPDULen );

                  p_pOutBuf += pHdr->m_unAPDULen;

                  return p_pOutBuf;
              }
          }
      }
      pData += pHdr->m_unLen;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Used by Transport Layer to indicate to the Application Layer that a new APDU has been
///         received. The APDU is placed in the Application Layer queue.
/// @param  p_pSrcAddr - pointer to source address (always 16 bytes; if first octet is 0xFF,
///                      the last 8 bytes represent MAC address
/// @param  p_ucSrcTsap - source transport SAP
/// @param  p_ucPriorityAndFlags - priority and flags
/// @param  p_unTSDULen - message length
/// @param  p_pAppData - pointer to the received APDU
/// @param  p_ucDstTsap - destination transport SAP
/// @param  p_ucTransportTime  - !TODOX!
/// @return none
/// @remarks
///      Access level: user level
///////////////////////////////////////////////////////////////////////////////////////////////////
// @todo   time parameter ignored
// @todo   TBD (Ticus): How check req id is incremental even if restarts?
void TLDE_DATA_Indication ( const uint8 * p_pSrcAddr,       // 128 bit or MAC address
                            uint8         p_ucSrcTsap,
                            uint8         p_ucPriorityAndFlags,
                            uint16        p_unTSDULen,
                            const uint8 * p_pAppData,
                            uint8         p_ucDstTsap,
                            uint8         p_ucTransportTime)
{
  uint16 unTotalLen = DMAP_GetAlignedLength(p_unTSDULen + sizeof(ASL_QUEUE_HDR));

  if(  p_unTSDULen < 3 )// minimal data
      return;

#if ( !defined(NL_FRAG_SUPPORT) && !defined(BACKBONE_SUPPORT) )

  if (unTotalLen  > 0xFF)
      return; // length on queue header is only 1 byte if not backbone or fragmentation not active

#endif //  !defined(NL_FRAG_SUPPORT) && !defined(BACKBONE_SUPPORT) )

  // check if there is enough free space in the common Rx/Tx APDU buffer
  if (unTotalLen > ASLDE_GetAPDUFreeSpace())
  {
      Log( LOG_WARNING, LOG_M_TL, TLOG_Indicate, 1, "9" );
      return; 
  }

#ifdef WCI_SUPPORT
    uint8 ucAddrLen = 16;
    uint8 * pSrcAddr = (uint8 *)p_pSrcAddr;
    if ( *pSrcAddr == 0xFF )  // MAC addr ?   
    {
        pSrcAddr += 8;
        ucAddrLen = 8;
    }
    
    WCI_Log( LOG_M_TL, TLOG_Indicate, ucAddrLen, pSrcAddr,
                                      sizeof(p_ucSrcTsap), &p_ucSrcTsap,
                                      sizeof(p_ucPriorityAndFlags), &p_ucPriorityAndFlags,
                                      (uint8)p_unTSDULen, p_pAppData,
                                      sizeof(p_ucDstTsap), &p_ucDstTsap,
                                      sizeof(p_ucTransportTime), &p_ucTransportTime );    
#endif  // WCI_SUPPORT  

  // construct the APDU management header
  ASL_QUEUE_HDR * pNext = (ASL_QUEUE_HDR*)g_pAslMsgQEnd;

  pNext->m_ucPriorityAndFlags  = p_ucPriorityAndFlags;
  pNext->m_ucDirection = RX_APDU;
  pNext->m_ucStatus    = APDU_NOT_PROCESSED;  
  pNext->m_ucSrcSAP    = p_ucSrcTsap;
  pNext->m_ucDstSAP    = p_ucDstTsap;
  pNext->m_unTimeout   = APP_QUE_TTL_SEC;
  pNext->m_unAPDULen   = p_unTSDULen;
  pNext->m_unLen       = unTotalLen;

  if ( 0xFF == *p_pSrcAddr)
  {   // src address is 64bit MAC address
      pNext->m_ucPeerAddrLen = 8;
      p_pSrcAddr += 8;     // only last 8 bytes will be copied
  }
  else // src address is 128 bit address
  {
      pNext->m_ucPeerAddrLen = 16;
      
#ifdef BACKBONE_SUPPORT
      // reset the SM link timeout counter for any incomming message from System Manager
      if (!memcmp(p_pSrcAddr, g_stDMO.m_aucSysMng128BitAddr, 16))
      {
          g_ucSMLinkTimeout = SM_LINK_TIMEOUT;
      }
#endif
  }

  memcpy( pNext->m_stInfo.m_stRx.m_aucSrcAddr,
          p_pSrcAddr,
          pNext->m_ucPeerAddrLen);

  // copy the APDU
  memcpy( g_pAslMsgQEnd + sizeof(ASL_QUEUE_HDR),
          p_pAppData,
          p_unTSDULen);

  g_pAslMsgQEnd += unTotalLen;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Called by the transport layer to signal the confirmation status of a previously issued
///         APDU
/// @param  p_hHandle  - application layer handler
/// @param  p_ucStatus - confirmation status
/// @return none
/// @remarks HANDLEs in ASL layer are in the 0x00 - 0xFF range\n
///      Access level: interrupt level
///////////////////////////////////////////////////////////////////////////////////////////////////
void TLDE_DATA_Confirm( HANDLE  p_hHandle,
                        uint8   p_ucStatus)
{
    if ( g_ucAPDUConfNo < MAX_APDU_CONF_ENTRY_NO )
    {
        WCI_Log( LOG_M_TL, TLOG_Confirm, sizeof(p_hHandle), &p_hHandle, sizeof(p_ucStatus), &p_ucStatus );

        g_astAPDUConf[g_ucAPDUConfNo].m_ucHandle = p_hHandle;
        g_astAPDUConf[g_ucAPDUConfNo++].m_ucConfirmStatus = p_ucStatus;        
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Implements the ASLMO object reset method
/// @param  p_ucResetType - reset type
/// @return service feedback code
/// @remarks
///      Access level: user level\n
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 ASLMO_reset(uint8 p_ucResetType)
{
  uint8 ucSFC = SFC_FAILURE;
  
  switch (p_ucResetType)
  {
  case ASLMO_DYNAMIC_DATA_RESET: 
      memset(g_astASLMOCounters, 0, sizeof(g_astASLMOCounters));
      g_unCounterNo   = 0;
      ucSFC = SFC_SUCCESS;
      break;      
      
  case ASLMO_RESET_TO_FACTORY_DEF: break;           // tbd
      
  case ASLMO_RESET_TO_PROVISIONED_SETTINGS: break;  // tbd
      
  case ASLMO_WARM_RESET: break;                     // tbd
      
  default: ucSFC = SFC_INVALID_ARGUMENT; break;     
  }
  
  return ucSFC;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
/// @brief  Add a malformed APDU event
/// @param  p_pSrcAddr128 - Source IPv6 address of the APDU
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void ASLMO_AddMalformedAPDU(const uint8 * p_pSrcAddr128)
{
  if (!p_pSrcAddr128 || !g_stASLMO.m_bMalformedAPDUAdvise || !g_stASLMO.m_unMalformedAPDUThreshold || !g_stASLMO.m_ulCountingPeriod)
      return; // nothing to do; 
  
  uint16 unIdx;  
  // first check if there is already a counter assigned to this address
  for (unIdx = 0; unIdx < g_unCounterNo; unIdx++)
  {
      if( !memcmp(p_pSrcAddr128, g_astASLMOCounters[unIdx].m_auc128BitAddr, 16) )
      {
          if( !g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter )
          {
              //first received malformed APDU
              g_astASLMOCounters[unIdx].m_ulResetCounter = g_stASLMO.m_ulCountingPeriod;
          }
          g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter++;          
          break;
      }
  }
  
  // new address
  if( unIdx == g_unCounterNo )  
  {
     if( unIdx < MAX_ASLMO_COUNTER_N0 )
     {
        g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter = 1; // new entry;
        g_astASLMOCounters[unIdx].m_ulResetCounter = g_stASLMO.m_ulCountingPeriod;
        memcpy(g_astASLMOCounters[unIdx].m_auc128BitAddr, p_pSrcAddr128, 16);
        g_unCounterNo++;
     }
     else
         return;
  } 
  
  // check if alert threshold has been reached
  if( g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter >= g_stASLMO.m_unMalformedAPDUThreshold
      && !g_stASLMO.m_stAlertDescriptor.m_bAlertReportDisabled ) // alert enabled
  {
      // alert condition reached; generate the alert
        ALERT stAlert;
        uint8 aucAlertBuf[24];        
        
        stAlert.m_ucPriority = g_stASLMO.m_stAlertDescriptor.m_ucPriority & 0x7F;
        stAlert.m_unDetObjTLPort = 0xF0B0; // ASLMO is DMAP port
        stAlert.m_unDetObjID = DMAP_ASLMO_OBJ_ID; 
        stAlert.m_ucClass = ALERT_CLASS_EVENT; 
        stAlert.m_ucDirection = ALARM_DIR_RET_OR_NO_ALARM; 
        stAlert.m_ucCategory = ALERT_CAT_COMM_DIAG; 
        stAlert.m_ucType = ASLMO_MALFORMED_APDU;     
        
        // build alert data
        stAlert.m_unSize = sizeof(aucAlertBuf); 
       
        memcpy( aucAlertBuf, 
                g_astASLMOCounters[unIdx].m_auc128BitAddr,
                sizeof(g_astASLMOCounters[unIdx].m_auc128BitAddr) );

        DMAP_InsertUint16( aucAlertBuf+16, g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter );   
        
        DMAP_InsertUint32( aucAlertBuf+18, g_stASLMO.m_ulCountingPeriod - g_astASLMOCounters[unIdx].m_ulResetCounter );           
        
        //fractional TAI -> 2^-16 sec
        aucAlertBuf[22] = 0;
        aucAlertBuf[23] = 0;
        
        ARMO_AddAlertToQueue( &stAlert, aucAlertBuf );

        g_astASLMOCounters[unIdx].m_unMalformedAPDUCounter = 0;        
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief This function executes the ASLMO methods 
/// @param  p_unMethID - method identifier
/// @param  p_unReqSize - input parameters size
/// @param  p_pReqBuf - input parameters buffer
/// @param  p_pRspSize - output parameters size
/// @param  p_pRspBuf - output parameters buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 ASLMO_Execute(uint8   p_ucMethID, 
                    uint16  p_unReqSize, 
                    uint8*  p_pReqBuf,
                    uint16* p_pRspSize,
                    uint8*  p_pRspBuf)
{
  if (p_ucMethID != ASLMO_RESET_METHOD_ID)
      return SFC_INVALID_METHOD;    
  
  uint8 ucResetType = *p_pReqBuf;
  
  if ( (p_unReqSize != 1) || (!ucResetType) || (ucResetType > ASLMO_DYNAMIC_DATA_RESET) )
      return SFC_INVALID_ARGUMENT;
  
  return ASLMO_reset(ucResetType);
}
                            

void ASLDE_loadIDTF(APDU_IDTF * p_pIdtf, const ASL_QUEUE_HDR * p_pHdr )
{
    p_pIdtf->m_ucPriorityAndFlags  = p_pHdr->m_ucPriorityAndFlags;
    p_pIdtf->m_ucAddrLen   = p_pHdr->m_ucPeerAddrLen;
    p_pIdtf->m_ucSrcTSAPID = p_pHdr->m_ucSrcSAP;
    p_pIdtf->m_ucDstTSAPID = p_pHdr->m_ucDstSAP;
    p_pIdtf->m_unDataLen   = p_pHdr->m_unAPDULen;
    
    memcpy(p_pIdtf->m_aucAddr, &p_pHdr->m_stInfo, p_pIdtf->m_ucAddrLen);
}
