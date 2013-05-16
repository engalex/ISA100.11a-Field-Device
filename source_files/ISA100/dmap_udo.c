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
/// Author:       Nivis LLC, 
/// Date:         June 2008
/// Description:  This file implements the Upload/Download Oject in DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "dmap_udo.h"
#include "dmap.h"
#include "aslsrvc.h"
#include "mlsm.h"

#define UPG_TAI_CUTOVER_DELAY       5   //five second waiting until process the upgrading

#define UDO_DESCR_SIZE 10
#define UDO_ATTR_CONST(x) {(void*)&x, sizeof(x)}

//udo attributees - globals
const uint8 c_ucSupportedOperations = 1; // RO ; 1=download only
const uint8 c_aDescription[] = { 'D', 'E', 'S', 'C', 'R', 'I', 'P', 'T', 'O', 'R'};

uint8 g_ucUDOState; //RO
uint8 g_ucCommand; //WO - non cacheable ??
uint16 g_unMaxUploadSize; //RO
uint16 g_unDownloadPrepTime; //RO
uint16 g_unDownloadActivationTime; //RO
uint16 g_unUploadPrepTime; //RO
uint16 g_unUploadProcessingTime; //RO
uint16 g_unDownloadProcessingTime; //RO
TIME   g_ulUDOCutoverTime; //RW - in seconds
uint16 g_unLastBlockDwld; //RO
uint16 g_unLastBlockUpld; //RO
uint8  g_ucErrorCode;

uint32 g_ulRemainingBytes;
uint16 g_unBlockSize;

const uint16 c_unMaxBlockSize = MAX_APDU_SIZE - (MAX_EXEC_REQ_HDR_SIZE + 2); //RO, +2 DownloadData method Block Number field
const uint32 c_ulMaxDownloadSize = FLASH_PROG_END_ADDR - FLASH_PROG_START_ADDR; //RO

typedef struct
{
  void* m_pAddr;
  uint8 m_ucSize;
} UDO_ATTR_CONSTS;

const UDO_ATTR_CONSTS g_aAttrConst[UDO_ATTR_NO] =
{
  {0, 0}, //res for future use
  UDO_ATTR_CONST(c_ucSupportedOperations),
  {0, 0}, //descriprion - cannot provide const value for size
  UDO_ATTR_CONST(g_ucUDOState),
  UDO_ATTR_CONST(g_ucCommand),
  UDO_ATTR_CONST(c_unMaxBlockSize),
  UDO_ATTR_CONST(c_ulMaxDownloadSize),
  UDO_ATTR_CONST(g_unMaxUploadSize),
  UDO_ATTR_CONST(g_unDownloadPrepTime),
  UDO_ATTR_CONST(g_unDownloadActivationTime),
  UDO_ATTR_CONST(g_unUploadPrepTime),
  UDO_ATTR_CONST(g_unUploadProcessingTime),
  UDO_ATTR_CONST(g_unDownloadProcessingTime),
  UDO_ATTR_CONST(g_ulUDOCutoverTime),
  UDO_ATTR_CONST(g_unLastBlockDwld),
  UDO_ATTR_CONST(g_unLastBlockUpld),
  UDO_ATTR_CONST(g_ucErrorCode)
};
//~udo globals

static uint8 UDO_startDownload(uint16 p_unBlockSize,
                               uint32 p_ulDwldSize,
                               uint8 p_ucOpMode);

static uint8 UDO_downloadData(uint16 p_unCrtBlockNumber,
                              uint8 p_ucDataSize,
                              uint8* p_pData);

static uint8 UDO_endDownload(uint8 p_ucReason);

static uint8 UDO_apply(uint8 p_ucCommand);

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Initializes the upload download object
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void UDO_Init()
{
  g_ucUDOState = UDO_STATE_IDLE;
  g_ucErrorCode = UDO_NO_ERROR;

  g_unMaxUploadSize = 0;
  g_unDownloadPrepTime = 0;
  g_unDownloadActivationTime = 0;
  g_unUploadPrepTime = 0;
  g_unUploadProcessingTime = 0;
  g_unDownloadProcessingTime = 0;
  g_ulUDOCutoverTime = 0;
  g_unLastBlockDwld = 0;
  g_unLastBlockUpld = 0;
  
  g_ulRemainingBytes = 0;
  g_unBlockSize = 0;
}


  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Mircea Vlasin
  /// @brief  Executes an upload/download object's method
  /// @param  p_pExecReq - execute request data
  /// @param  p_pExecRsp - execute response data
  /// @return none
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void UDO_ProcessExecuteRequest( EXEC_REQ_SRVC * p_pExecReq, EXEC_RSP_SRVC * p_pExecRsp )
  {
    p_pExecRsp->m_unLen = 0;
    
    switch( p_pExecReq->m_ucMethID )
    {
        case UDO_START_DWLD:
            if(7 != p_pExecReq->m_unLen)
            {
                p_pExecRsp->m_ucSFC = SFC_INVALID_SIZE;
            }
            else
            {
              uint32 ulDwldSize = ((uint32)p_pExecReq->p_pReqData[2] << 24) |
                                  ((uint32)p_pExecReq->p_pReqData[3] << 16) |
                                  ((uint32)p_pExecReq->p_pReqData[4] << 8)  |
                                    p_pExecReq->p_pReqData[5];
  
              p_pExecRsp->m_ucSFC = UDO_startDownload(((uint16)p_pExecReq->p_pReqData[0] << 8) | p_pExecReq->p_pReqData[1],
                                                       ulDwldSize,
                                                       p_pExecReq->p_pReqData[6]);
            }
            break;
  
        case UDO_DWLD_DATA:
            if(p_pExecReq->m_unLen < 3)
            {
                p_pExecRsp->m_ucSFC = SFC_INVALID_SIZE;
            }
            else
            {
                p_pExecRsp->m_ucSFC = UDO_downloadData(((uint16)p_pExecReq->p_pReqData[0] << 8) | p_pExecReq->p_pReqData[1],
                                                        p_pExecReq->m_unLen - 2,
                                                        p_pExecReq->p_pReqData + 2);
  
                if( SFC_INVALID_BLOCK_NUMBER == p_pExecRsp->m_ucSFC )
                {
                    //"g_unLastBlockDwld" wasn't incremented
                    p_pExecRsp->m_unLen = 2;
                    p_pExecRsp->p_pRspData[0] = g_unLastBlockDwld >> 8;
                    p_pExecRsp->p_pRspData[1] = g_unLastBlockDwld;
                }
            }
            break;
  
        case UDO_END_DWLD:
            if( 1 != p_pExecReq->m_unLen  )
            {
                p_pExecRsp->m_ucSFC = SFC_INVALID_SIZE;
            }
            else
            {
                p_pExecRsp->m_ucSFC = UDO_endDownload(p_pExecReq->p_pReqData[0]);
            }
            break;
  
        case UDO_START_UPLD:
        case UDO_UPLD_DATA:
        case UDO_END_UPLD:
        default: p_pExecRsp->m_ucSFC = SFC_INVALID_METHOD;
    }  
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Mihaela Goloman
  /// @brief  Reads an UDO attribute's value
  /// @param  p_ucAttrId - attribute identifier
  /// @param  p_pSize - attribute's size
  /// @param  p_pBuf - buffer containing the attribute's value
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UDO_Read(uint16   p_unAttrId,
                 uint16 * p_pSize,
                 uint8*   p_pBuf)
  {
    *p_pSize = 0;
    if( (p_unAttrId >= UDO_ATTR_NO) || (UDO_RES == p_unAttrId) )
      return SFC_INVALID_ATTRIBUTE;
  
    if( UDO_COMM == p_unAttrId )
      return SFC_WRITE_ONLY_ATTRIBUTE;
  
    if( UDO_DESCR == p_unAttrId )
    {
      memcpy(p_pBuf, c_aDescription, sizeof(c_aDescription));
      *p_pSize = sizeof(c_aDescription);
      return SFC_SUCCESS;
    }
  
    uint8 ucSize = g_aAttrConst[p_unAttrId].m_ucSize;
    for(uint8 ucIdx=0; ucIdx<ucSize; ucIdx++)
    {
      *(p_pBuf+ucIdx) = *((uint8*)g_aAttrConst[p_unAttrId].m_pAddr + ucSize - ucIdx - 1);
    }
    *p_pSize = ucSize;
    return SFC_SUCCESS;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Mihaela Goloman
  /// @brief  Writes an UDO attribute's value
  /// @param  p_ucAttrId - attribute identifier
  /// @param  p_ucSize - attribute's size
  /// @param  p_pBuf - buffer containing the attribute's value
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UDO_Write(uint16   p_unAttrId,
                  uint16   p_unSize,
                  const uint8* p_pBuf)
  {
    if((p_unAttrId >= UDO_ATTR_NO) || (UDO_RES == p_unAttrId))
      return SFC_INVALID_ATTRIBUTE;
  
    if( UDO_COMM == p_unAttrId )
    {
       if( 1 == p_unSize )
       {
         return UDO_apply(*p_pBuf);
       }
       else return SFC_INVALID_SIZE;
    }
  
    if( UDO_CUTOVER_TIME == p_unAttrId )
    {
       if(4 == p_unSize)
       {
         g_ulUDOCutoverTime = ((uint32)p_pBuf[0] << 24) |
                               ((uint32)p_pBuf[1] << 16) |
                               ((uint32)p_pBuf[2] << 8)  |
                               ((uint32)p_pBuf[3]);
         return SFC_SUCCESS;
       }
       else return SFC_INVALID_SIZE;
    }
  
    return SFC_READ_ONLY_ATTRIBUTE;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Eduard Erdei
  /// @brief  Used by a client to reach an agreement with an
  ///         UploadDownload object to participate in a download for which
  ///         the client will be providing the data, one block at a time
  /// @param  p_unBlockSize (in) - size of a block of data to be be downloaded
  /// @param  p_ulDwldSize (in) - total size of data to be downloaded
  /// @param  p_ucOpMode (in) - desired mode of operation
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  #define UPG_FILE_HEADER_SIZE      20
  
  uint8 UDO_startDownload(uint16 p_unBlockSize, uint32 p_ulDwldSize, uint8 p_ucOpMode)
  {
    if( UDO_STATE_IDLE != g_ucUDOState )
      return SFC_UNEXPECTED_METH_SEQUENCE;
  
    if( p_unBlockSize > c_unMaxBlockSize || UPG_FILE_HEADER_SIZE > p_unBlockSize )
      return SFC_INVALID_BLOCK_SIZE;
  
    if( p_ulDwldSize > c_ulMaxDownloadSize )
      return SFC_INVALID_DOWNLOAD_SIZE;
  
    if( p_ucOpMode )
      return SFC_INVALID_PARAMETER; // see page 382 in ISA100 May release
  
    g_ulRemainingBytes = p_ulDwldSize;
    g_unBlockSize = p_unBlockSize;
    g_ucUDOState = UDO_STATE_DOWNLOADING;
  
    //TO DO - get and prepare destination for new code  

    return SFC_SUCCESS; // operationAccepted;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Eduard Erdei, Mircea Vlasin
  /// @brief  Used by a client to provide data to an
  ///         UploadDownload object for an agreed download operation
  /// @param  p_unCrtBlockNumber - current block number being downloaded(received from the client)
  /// @param  p_ucDataSize - size in bytes of the data block being downloaded
  /// @param  p_pucData - content of the data block being downloaded
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UDO_downloadData(uint16 p_unCrtBlockNumber, uint8 p_ucDataSize, uint8* p_pucData)
  {
    uint32 ulCrtEepAddr = UDP_FW_CODE_START_ADDR;
  
    if( UDO_STATE_DOWNLOADING != g_ucUDOState)
      return SFC_UNEXPECTED_METH_SEQUENCE;
  
    if( !p_unCrtBlockNumber )
      return SFC_INVALID_BLOCK_NUMBER;
    
    if( p_unCrtBlockNumber == g_unLastBlockDwld ) // duplicate message
      return SFC_SUCCESS;
  
    if( p_unCrtBlockNumber != (g_unLastBlockDwld + 1) )
      return SFC_INVALID_BLOCK_NUMBER;
  
    if( p_ucDataSize != g_unBlockSize ) // maybe it's the last block
    {
      if( (uint32)p_ucDataSize != g_ulRemainingBytes )
        return SFC_BLOCK_DATA_ERROR;
    }
  
    if( 1 == p_unCrtBlockNumber )
    {
      //  the first packet contains also the binary upgrade file header
      //  perform some minimal validations of the upgrade file
       
      p_pucData += UPG_FILE_HEADER_SIZE;
  
  #if   (DEVICE_TYPE == DEV_TYPE_MC13225) 
        //TO DO - save current FW block to appropriate persistent area
  #else
        #warning  "UDO_downloadData not supported"   
  #endif    
      
    }
    else // not the first block
    {
      ulCrtEepAddr += (p_unCrtBlockNumber - 1) * g_unBlockSize - UPG_FILE_HEADER_SIZE;
    
  #if   (DEVICE_TYPE == DEV_TYPE_MC13225) 
        //TO DO - save current FW block to appropriate persistent area
  #else
        #warning  "UDO_downloadData not supported"   
  #endif    
    }
  
    g_unLastBlockDwld++;
    g_ulRemainingBytes -= p_ucDataSize;
  
    return SFC_SUCCESS;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Eduard Erdei
  /// @brief  Used by a client to terminate a download operation
  ///         that either has completed successfully,
  ///         or which the client wishes to abort
  /// @param  p_ucReason (in) - client's reason for teminating the download
  ///                           operation (0 = dwld complete, 1 = dwld aborted)
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UDO_endDownload(uint8 p_ucReason)
  {
    if( g_ucUDOState != UDO_STATE_DOWNLOADING )
      return SFC_UNEXPECTED_METH_SEQUENCE;
  
    if( p_ucReason )
    {
        g_ucUDOState = UDO_STATE_DL_ERROR;
        g_ucErrorCode = UDO_CLIENT_ABORT;
        return SFC_SUCCESS;
    }
  
    if( g_ulRemainingBytes ) // if download is not completed
        return SFC_OPERATION_INCOMPLETE;
  
    g_ucUDOState = UDO_STATE_DL_COMPLETE;
    g_unBlockSize = 0;
  
    return SFC_SUCCESS;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Eduard Erdei
  /// @brief  Implements the "write" of the command attribute of
  ///         the UDO object
  /// @param  p_ucCommand - action command for the UDO object
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 UDO_apply(uint8 p_ucCommand)
  {
    if( UDO_STATE_APPLYING == g_ucUDOState || UDO_STATE_DOWNLOADING == g_ucUDOState || UDO_STATE_IDLE == g_ucUDOState )
      return SFC_OBJECT_STATE_CONFLICT;
  
    if( p_ucCommand > 1 )
      return SFC_VALUE_OUT_OF_RANGE;
  
    if( UDO_STATE_DL_ERROR == g_ucUDOState && p_ucCommand != 0 )
      return SFC_OBJECT_STATE_CONFLICT; // only reset is accepted in this stage
  
    g_ucCommand = p_ucCommand;
  
  
    if( 0 == p_ucCommand ) // reset
    {
      UDO_Init();
      return SFC_SUCCESS;
    }
    else
    {
      // TODO: at g_ulUDOCutoverTime start firmware update process
      if( !g_ulUDOCutoverTime )
        g_ulUDOCutoverTime = MLSM_GetCrtTaiSec() + UPG_TAI_CUTOVER_DELAY;   //to be sure that response is sent to the SM
  
      g_ucUDOState = UDO_STATE_APPLYING;
      return SFC_OPERATION_ACCEPTED;
    }
  }
