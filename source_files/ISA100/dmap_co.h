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
/// Date:         December 2008
/// Description:  This file holds definitions of the concentrator object in DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DMAP_CO_H_
#define _NIVIS_DMAP_CO_H_

#include "../typedef.h"
#include "aslde.h"
#include "dmap_dmo.h"
#include "dmap_utils.h"

#define CONTRACT_DEFAULT_DEADLINE  1000 // 10 seconds; not specified in specification;

// constant that defines how often the CO_ConcentratorTask is executed (100 represents 1/100 = 10 msec)
#define CO_TASKS_FREQ        100    

/*
 Persistent data, application area: 

 Start Address:        uint8  ucIsData (0xA5 -> valid data, !=0xA5 -> no data)   
                       uint8  aucArray1[ sizeof(COMM_ASSOC_ENDP) ] 
                       uint8  m_ucPubItemsNo 
                       uint8  aucArray2[ m_ucPubItemsNo * sizeof(OBJ_ATTR_IDX_AND_SIZE) ]

*/ 

// persistent data
#define ENDPOINT_WRITTEN  0xA0
#define ATTR_WRITTEN      0x05   // 0xA5 = ENDPOINT_WRITTEN | ATTR_WRITTEN  


enum{
  ENDPOINT_NOT_CONFIGURED         = 0,
  AWAITING_CONTRACT_ESTABLISHMENT = 1,
  CONTRACT_ACTIVE_AS_REQUESTED    = 2,
  CONTRACT_ACTIVE_NEGOTIATED_DOWN = 3,
  AWAITING_CONTRACT_TERMINATION   = 4,
  CONTRACT_ESTABLISHMENT_FAILED   = 5,
  CONTRACT_INACTIVE               = 6,
  // following states are not specified in ISA
  CONTRACT_REQUEST_TERMINATION    = 7
}; //CONTRACT_STATUS;  

enum{
  CO_ATTR_ZERO = 0,
  CO_REVISION = 1,
  CO_COMMENDP = 2,
  CO_CONTRACTDATA = 3,
  CO_MAXITEMS = 4,
  CO_ITEMSNO = 5,
  CO_PUBDATA = 6,
  CO_ATTR_NO = 7
}; //CO_ATTRIBUTES;


typedef struct{
  uint16  m_unContractID;
  uint8   m_ucStatus;       //for range see CONTRACT_STATUS enum;
  uint8   m_ucActualPhase;
  int16   m_nActualPeriod;  // not in specification; 
}COMM_CONTRACT_DATA;


typedef struct{
  //additional here - eeprom status
  uint8                 m_ucIsData; 
  uint8                 m_ucPubItemsNo; 
  //search data
  uint16                m_unSrcObjectId; //object identifier of the CO instance if many on the same UAP
  uint16                m_unSrcTSAP;
  //attributes    
  COMM_ASSOC_ENDP       m_stEndpoint;  
  OBJ_ATTR_IDX_AND_SIZE m_aAttrDescriptor[MAX_PUBLISH_ITEMS];   
  COMM_CONTRACT_DATA    m_stContract;
  uint8                 m_ucRevision;  
  //additional - for publish task
  uint8                 m_ucContractPriority;
  uint16                m_unTimestamp;  
  uint32                m_ulNextPblSec;
  uint16                m_unNextPblFract;
  uint8                 m_ucFreshSeqNo;   // IMPORTANT: leave this member at the end of structure!!!
}CO_STRUCT;

typedef void (*C0_READ_FCT)(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize);
typedef uint8 (*CO_WRITE_FCT)(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);

typedef struct
{
  void *          m_pValue;
  uint8           m_ucSize;
  uint8           m_ucAccess;
  C0_READ_FCT     m_pReadFct;
  CO_WRITE_FCT    m_pWriteFct;
  
} CO_FCT_STRUCT; 

extern CO_STRUCT* g_pstCrtCO;

uint8 CO_Read(uint16 p_unAttrID, uint16* p_punSize, uint8* p_pBuf);
uint8 CO_Write(uint16 p_unAttrID, uint16 p_unSize, const uint8* p_pBuf);
void CO_readAssocEndp(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize);
void CO_Init();
void CO_ConcentratorTask();

void CO_NotifyContractDeletion(uint16 p_unContractID);
void CO_NotifyAddContract(DMO_CONTRACT_ATTRIBUTE * p_pContract);
CO_STRUCT* FindConcentratorByObjId(uint16 p_unSrcObjectID, uint16 p_unSrcTSAP);
CO_STRUCT* FindConcentratorByContract(uint16 p_unContractID);

CO_STRUCT* AddConcentratorObject(uint16 p_unSrcObjectID, uint16 p_unSrcTSAP);

#endif //_NIVIS_DMAP_CO_H_
