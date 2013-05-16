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
/// Description:  This file holds definitions of the app layer queue and client/server services
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_ASLDE_H_
#define _NIVIS_ASLDE_H_

/*===========================[ public includes ]=============================*/
#include "../typedef.h"
#include "../global.h"
#include "sfc.h"
#include "provision.h"
#include "config.h"

/*==========================[ public datatypes ]=============================*/

enum{
   SRVC_PERIODIC_COMM = 0,
   SRVC_APERIODIC_COMM = 1
}; // SRV_COMM_TYPE


enum{
  SRVC_REQUEST  = 0,
  SRVC_RESPONSE = 1
}; // SRVC_PRIMITIVE_TYPE

enum
{
  SRVC_TYPE_PUBLISH       = 0,
  SRVC_TYPE_ALERT_REP     = 1,
  SRVC_TYPE_ALERT_ACK     = 2,
  SRVC_TYPE_READ          = 3,
  SRVC_TYPE_WRITE         = 4,
  SRVC_TYPE_EXECUTE       = 5,
  SRVC_TYPE_TUNNEL        = 6
}; // ASL_SERVICE_TYPE

enum
{
  SRVC_PUBLISH    = ((SRVC_REQUEST << 7) | SRVC_TYPE_PUBLISH ),
  SRVC_READ_REQ   = ((SRVC_REQUEST << 7)  | SRVC_TYPE_READ ),
  SRVC_READ_RSP   = ((SRVC_RESPONSE << 7) | SRVC_TYPE_READ ),
  SRVC_WRITE_REQ  = ((SRVC_REQUEST << 7)  | SRVC_TYPE_WRITE ),
  SRVC_WRITE_RSP  = ((SRVC_RESPONSE << 7) | SRVC_TYPE_WRITE ),
  SRVC_EXEC_REQ   = ((SRVC_REQUEST << 7)  | SRVC_TYPE_EXECUTE ),
  SRVC_EXEC_RSP   = ((SRVC_RESPONSE << 7) | SRVC_TYPE_EXECUTE ),
  SRVC_ALERT_REP  = ((SRVC_REQUEST << 7)  | SRVC_TYPE_ALERT_REP),
  SRVC_ALERT_ACK  = ((SRVC_REQUEST << 7)  | SRVC_TYPE_ALERT_ACK)
};

enum
{
  OID_COMPACT   = 0,
  OID_MID_SIZE  = 1,
  OID_FULL_SIZE = 2,
  OID_INFERRED  = 3
}; // OBJ_ID_ADDR_MODE

#define OBJ_ADDRESSING_TYPE_MASK  0x60

enum
{
  ATTR_NO_INDEX   = 0,
  ATTR_ONE_INDEX  = 1,
  ATTR_TWO_INDEX  = 2
}; // ATTRIBUTE_FORMAT

enum
{
  ATTR_TYPE_SIX_BIT_NO_INDEX    = 0,
  ATTR_TYPE_SIX_BIT_ONE_DIM     = 1,
  ATTR_TYPE_SIX_BIT_TWO_DIM     = 2,
  ATTR_TYPE_TWELVE_BIT_EXTENDED = 3
}; // ATTRIBUTE_OPTION_TYPE

enum
{
  TWELVE_BIT_NO_INDEX = 0,
  TWELVE_BIT_ONE_DIM  = 1,
  TWELVE_BIT_TWO_DIM  = 2,
  TWELVE_BIT_RESERVED
}; // TWELVE_BIT_INDEX_OPTION

typedef struct
{
  uint8   m_ucAttrFormat;
  uint16  m_unAttrID;
  uint16  m_unIndex1;
  uint16  m_unIndex2;
} ATTR_IDTF;

typedef struct
{
  uint16    m_unSrcOID;
  uint16    m_unDstOID;
  uint8     m_ucReqID;
  ATTR_IDTF m_stAttrIdtf;
} READ_REQ_SRVC;

typedef struct
{
  uint16  m_unSrcOID;
  uint16  m_unDstOID;
  uint8   m_ucReqID;
  uint8   m_ucFECCE; // forward explicit congestion control echo
  uint8   m_ucSFC;
  uint16  m_unLen;
  uint8 * m_pRspData;
} READ_RSP_SRVC;

typedef struct
{
  uint16    m_unSrcOID;
  uint16    m_unDstOID;
  uint8     m_ucReqID;
  ATTR_IDTF m_stAttrIdtf;
  uint16    m_unLen;
  uint8 *   p_pReqData;
} WRITE_REQ_SRVC;

typedef struct
{
  uint16  m_unSrcOID;
  uint16  m_unDstOID;
  uint8   m_ucReqID;
  uint8   m_ucFECCE; // forward explicit congestion control echo
  uint8   m_ucSFC;
} WRITE_RSP_SRVC;

typedef struct
{
  uint16  m_unSrcOID;
  uint16  m_unDstOID;
  uint8   m_ucReqID;
  uint8   m_ucMethID;
  uint16  m_unLen;
  uint8 * p_pReqData;
} EXEC_REQ_SRVC;

typedef struct
{
  uint16  m_unSrcOID;
  uint16  m_unDstOID;
  uint8   m_ucReqID;
  uint8   m_ucFECCE; // forward explicit congestion control echo
  uint8   m_ucSFC;
  uint16  m_unLen;
  uint8 * p_pRspData;
} EXEC_RSP_SRVC;

typedef struct
{
  uint16  m_unPubOID;
  uint16  m_unSubOID;

  uint8   m_ucContentVersion;
  uint8   m_ucFreqSeqNo;
  uint16  m_unSize;
  uint8   m_ucNativePublish;
  uint8*  m_pData;
} PUBLISH_SRVC;

/* alert information  */
typedef struct{
  uint32  m_ulNextSendTAI;
  uint16  m_unDetObjTLPort;
  uint16  m_unDetObjID;
  //uint16  m_unDetObjType;  
  struct{
    TIME m_ulSeconds;
    uint16 m_unFract;
  }m_stDetectionTime;
  uint8   m_ucID;
  uint8   m_ucClass;
  uint8   m_ucDirection;
  uint8   m_ucCategory;
  uint8   m_ucType;
  uint8   m_ucPriority;
  uint16  m_unSize;
}ALERT;

typedef struct{
  uint16  m_unSrcOID;
  uint16  m_unDstOID;
  ALERT   m_stAlertInfo;
  uint8*  m_pAlertValue;
}ALERT_REP_SRVC;  //alert report service

//actually alert class(x), direction(y), category(z) and priority(w) are packed like this
//xyzzwwww;
#define MASK_ALERT_CLASS  0x80
#define ROT_ALERT_CLASS   7
#define MASK_ALARM_DIRECTION  0x40
#define ROT_ALARM_DIRECTION   6
#define MASK_ALERT_CATEGORY   0x30
#define ROT_ALERT_CATEGORY    4
#define MASK_ALERT_PRIORITY   0x0F
#define ROT_ALERT_PRIORITY    0

typedef struct{
  uint16  m_unSrcOID;
  uint16  m_unDstOID;
  uint8   m_ucAlertID;
}ALERT_ACK_SRVC;

typedef struct
{
  uint8 m_ucType;
  union
  {
    READ_REQ_SRVC   m_stReadReq;
    READ_RSP_SRVC   m_stReadRsp;
    WRITE_REQ_SRVC  m_stWriteReq;
    WRITE_RSP_SRVC  m_stWriteRsp;
    EXEC_REQ_SRVC   m_stExecReq;
    EXEC_RSP_SRVC   m_stExecRsp;
    ALERT_REP_SRVC  m_stAlertRep;
    ALERT_ACK_SRVC  m_stAlertAck;
    PUBLISH_SRVC    m_stPublish;
  }  m_stSRVC;
} GENERIC_ASL_SRVC;

typedef struct
{
  uint8 m_bAlertReportDisabled;
  uint8 m_ucPriority;
} APP_ALERT_DESCRIPTOR;

#define MAX_EXEC_REQ_HDR_SIZE   (1+4+1+1+2)     //ServiceType + SrcDestObj + ReqId + MethodId + DataLen

//////////////////////////////////////////////////////////////////////////////////////

// defines for local process IDs
#define UAP_DMAP_ID           0
#define UAP_SMAP_ID           1
#define UAP_APP1_ID           2
#define UAP_BBR_ID            9

// apdu queue entry header structure
typedef struct
{
  uint8   m_ucDirection;
  uint8   m_ucStatus;
  
#if ( defined(NL_FRAG_SUPPORT) || defined(BACKBONE_SUPPORT) )
  uint16  m_unLen;
  uint16  m_unAPDULen;
#else
  uint8   m_unLen;
  uint8   m_unAPDULen;
#endif // ( defined(NL_FRAG_SUPPORT) || defined(BACKBONE_SUPPORT) )

  uint16  m_unTimeout;          // in seconds
  
  uint8   m_ucPriorityAndFlags;
  uint8   m_ucSrcSAP;
  uint8   m_ucDstSAP;
  uint8   m_ucPeerAddrLen;

  union
  {
      struct
      {
        uint8 m_aucSrcAddr[16];
      }m_stRx;

      struct
      {
        union 
        {
            uint8   m_aucEUI64[8];
            uint16  m_unContractId;
        } m_stDest;
        
        uint16  m_unDefTimeout; // default timeout in seconds
        uint16  m_unAppHandle;
        
        uint8   m_ucConcatHandle;
        uint8   m_ucReqID;
        uint8   m_ucRetryNo;
      }m_stTx;
  } m_stInfo;

} ASL_QUEUE_HDR;

#define RX_APDU         0
#define TX_APDU         1

/* APDU states (these are signaled in the previously described management buffer  */
enum
{
  APDU_NOT_PROCESSED,
  APDU_READY_TO_SEND,
  APDU_SENT_WAIT_JUST_TL_CNF,
  APDU_SENT_WAIT_CNF_AND_RESPONSE,
  APDU_CONFIRMED_WAIT_RSP,
  APDU_READY_TO_CLEAR
}; // APDU_NEXT_ACTION

typedef struct
{
  uint8   m_aucAddr[16];
  uint8   m_ucAddrLen;
  uint8   m_ucSrcTSAPID;
  uint8   m_ucDstTSAPID;
  uint8   m_ucPriorityAndFlags;
  uint16  m_unDataLen;
} APDU_IDTF;

typedef struct
{
  uint8 m_ucHandle;
  uint8 m_ucConfirmStatus;
} APDU_CONFIRM_IDTF;

/* ASLMO object related   */
enum
{
  ASLMO_MALF_APDU_ADV_ID          = 1,
  ASLMO_TIME_INTERVAL_ID          = 2,
  ASLMO_MALF_APDU_THRESH_ID       = 3,
  ASLMO_MALF_APDU_ALERT_DESCR_ID  = 4,  
  ASLMO_MAX_COUNTER_NO            = 5,
  ASLMO_ATTR_NO                   = 6    
}; // ASLMO_ATTR_IDS

enum
{
  ASLMO_NOT_USED_RESET_TYPE           = 0,
  ASLMO_RESET_TO_FACTORY_DEF          = 1,
  ASLMO_RESET_TO_PROVISIONED_SETTINGS = 2,
  ASLMO_WARM_RESET                    = 3,
  ASLMO_DYNAMIC_DATA_RESET            = 4
  
}; // ASLMO_RESET_TYPES

enum
{
  ASLMO_NULL_METHOD_ID  = 0,
  ASLMO_RESET_METHOD_ID = 1,
  ASLMO_METHOD_NO       = 1
}; // ASLMO_METHOD_IDS

enum
{
  ASLMO_MALFORMED_APDU = 0,
  ASLMO_COMM_ALERT     = 1
}; // ASLMO_ALERT_TYPES 
  

typedef struct
{  
  uint32                m_ulCountingPeriod;           // attrID = 2
  APP_ALERT_DESCRIPTOR  m_stAlertDescriptor;          // attrID = 4
  uint16                m_unMalformedAPDUThreshold;   // attrID = 3
  uint8                 m_bMalformedAPDUAdvise;       // attrID = 1 
} ASLMO_STRUCT;

typedef struct
{
  IPV6_ADDR m_auc128BitAddr;
  uint32    m_ulResetCounter;      
  uint16    m_unMalformedAPDUCounter;  
} MALFORMED_APDU_COUNTER;

/*==========================[ public functions ]==============================*/

extern void ASLDE_Init(void);

extern void ASLDE_MarkForDeleteAPDU(uint8 * p_pAPDU);

extern uint8 * ASLDE_GetMyAPDU( uint8       p_ucTSAPID,
                                APDU_IDTF * p_pIdtf);

extern uint16 ASLDE_GetAPDUFreeSpace(void);

extern void ASLDE_DiscardAPDU( uint16 p_unContractID );

void TLDE_DATA_Indication ( const uint8 * p_pSrcAddr,       // 128 bit or MAC address
                            uint8         p_ucSrcTsap,
                            uint8         p_ucPriorityAndFlags,
                            uint16        p_unTSDULen,
                            const uint8 * p_pAppData,
                            uint8         p_ucDstTsap,
                            uint8         p_ucTransportTime);

extern  void TLDE_DATA_Confirm( HANDLE  p_hHandle,
                                uint8   p_ucStatus);

extern void ASLDE_ASLTask(void);

extern void ASLDE_PerformOneSecondOperations(void);

extern uint8 * ASLDE_SearchOriginalRequest( uint8        p_ucReqID,
                                            uint16       p_unDstObjID,
                                            uint16       p_unSrcObjID,
                                            APDU_IDTF *  p_pAPDUIdtf,
                                            uint16 *     p_pLen);

extern void ASLMO_AddMalformedAPDU(const uint8 * p_pSrcAddr128);  

extern uint8 ASLMO_Execute( uint8   p_ucMethID, 
                            uint16  p_unReqSize, 
                            uint8*  p_pReqBuf,
                            uint16* p_pRspSize,
                            uint8*  p_pRspBuf);

extern uint8  g_aucAPDUBuff[COMMON_APDU_BUF_SIZE];

extern uint8 * g_pAslMsgQEnd;

extern uint8 g_ucHandle;

extern const DMAP_FCT_STRUCT c_aASLMOFct[ASLMO_ATTR_NO];

#define ASLMO_Read(p_unAttrID,p_punBufferSize,p_pucRspBuffer) \
            DMAP_ReadAttr(p_unAttrID,p_punBufferSize,p_pucRspBuffer,c_aASLMOFct,ASLMO_ATTR_NO)

#define ASLMO_Write(p_unAttrID,p_ucBufferSize,p_pucBuffer)   \
            DMAP_WriteAttr(p_unAttrID,p_ucBufferSize,p_pucBuffer,c_aASLMOFct,ASLMO_ATTR_NO)

#endif // _NIVIS_ASLDE_H_

