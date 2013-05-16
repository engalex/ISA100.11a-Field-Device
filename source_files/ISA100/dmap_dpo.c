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
/// Date:         February 2009
/// Description:  This file implements the device provisioning object in DMAP
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include "dmap_dpo.h"
#include "uap.h"
#include "dmap.h"
#include "dlmo_utils.h"
#include "dmap_dmo.h"
#include "dmap_dlmo.h"
#include "dmap_co.h"
#include "mlsm.h"
#include "dlme.h"
#include "dmap_armo.h"

    // provision info   
#ifndef FILTER_BITMASK        
    #define FILTER_BITMASK 0xFFFF //specific network only      
#endif

#ifndef FILTER_TARGETID        
    #define FILTER_TARGETID 0x0001 // default provisioning network      
#endif

#define DPO_DEFAULT_PROV_SIGNATURE 0xFF

#define ISA100_GW_UAP       1

#ifndef BACKBONE_SUPPORT  

  const DPO_STRUCT c_stDefaultDPO = 
  {
      DPO_DEFAULT_PROV_SIGNATURE, //   m_ucStructSignature; 
      DEFAULT_JOIN_ONLY, //   m_ucJoinMethodCapability; 
      TRUE, //   m_ucAllowProvisioning;
  #ifndef BACKBONE_SUPPORT
      TRUE, //   m_ucAllowOverTheAirProvisioning;
      TRUE, //   m_ucAllowOOBProvisioning;
  #else
      FALSE, //   m_ucAllowOverTheAirProvisioning;
      TRUE, //   m_ucAllowOOBProvisioning;
  #endif
      TRUE, //   m_ucAllowResetToFactoryDefaults;
      TRUE, //   m_ucAllowDefaultJoin;
      0, //  m_ucTargetJoinMethod // 0 = symmetric key, 1 = public key
      FILTER_BITMASK, //  m_unTargetNwkBitMask; // aligned to 4
      FILTER_TARGETID, //  m_unTargetNwkID; 
      { 0x00,0x49,0x00,0x53,0x00,0x41,0x00,0x20,0x00,0x31,0x00,0x30,0x00,0x30,0x00,0x00}, //   m_aJoinKey[16];        // aligned to 4
      {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, //   m_aTargetSecurityMngrEUI[8];
      0xFFFF, // uint16  m_unTargetFreqList; //frequencies for advertisments which are to be used
      34, //   m_nCurrentUTCAdjustment // from 2009 ...
      DEVICE_ROLE,
      0x0000, // m_unCustomAlgorithms
      180,    // m_unBannedTimeout
      15,     // m_ucAlarmBandwidth
      180,    // m_ucAlarmResetTimeout
      0,      // m_ucJoinedAfterProvisioning  
  };
#endif

#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
    uint8 g_ucDAQDeviceRole;
#endif

DPO_STRUCT  g_stDPO;

#ifndef BACKBONE_SUPPORT
  uint16 g_stFilterTargetID;
  uint16 g_stFilterBitMask;
#endif  

void DPO_ReadDLConfigInfo(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize);
void DPO_WriteDLConfigInfo(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);

void DPO_readAssocEndp(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize);
void DPO_writeAssocEndp(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);
void DPO_readAttrDesc(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize);
void DPO_writeAttrDesc(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);
void DPO_WriteDeviceRole(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);
void DPO_WriteUAPCORevision(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize);

const uint16  c_unDefaultNetworkID      = 1; //id for the default network
#define   c_aDefaultSYMJoinKey c_aulWellKnownISAKey
#define   c_aOpenSYMJoinKey    c_aulWellKnownISAKey

const uint16  c_unDefaultFrequencyList  = 0xFFFF; //list of frequencies used by the adv. routers of the default network

// has to start with write dll config, has to end with write ntw id

#ifdef BACKBONE_SUPPORT
  #define DPO_writeUint8          NULL
  #define DPO_writeUint16         NULL
  #define DPO_writeVisibleString  NULL
#else
  void DPO_writeUint8(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
  void DPO_writeUint16(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
  void DPO_writeVisibleString(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize);
#endif

const DMAP_FCT_STRUCT c_aDPOFct[DPO_ATTR_NO] = {
   0,   0,                                              DMAP_EmptyReadFunc,     NULL,  
   ATTR_CONST(c_unDefaultNetworkID),                    DMAP_ReadUint16,        NULL,
   ATTR_CONST(c_aDefaultSYMJoinKey),                    DMAP_ReadOctetString,   NULL,
   ATTR_CONST(c_aOpenSYMJoinKey),                       DMAP_ReadOctetString,   NULL,
   ATTR_CONST(c_unDefaultFrequencyList),                DMAP_ReadUint16,        NULL,
   ATTR_CONST(g_stDPO.m_ucJoinMethodCapability),        DMAP_ReadUint8,         DPO_writeUint8,
   ATTR_CONST(g_stDPO.m_ucAllowProvisioning),           DMAP_ReadUint8,         DPO_writeUint8,
   ATTR_CONST(g_stDPO.m_ucAllowOverTheAirProvisioning), DMAP_ReadUint8,         DPO_writeUint8,
   ATTR_CONST(g_stDPO.m_ucAllowOOBProvisioning),        DMAP_ReadUint8,         DPO_writeUint8,
   ATTR_CONST(g_stDPO.m_ucAllowResetToFactoryDefaults), DMAP_ReadUint8,         NULL,
   ATTR_CONST(g_stDPO.m_ucAllowDefaultJoin),            DMAP_ReadUint8,         DPO_writeUint8,
   ATTR_CONST(g_stDPO.m_unTargetNwkID),                 DMAP_ReadUint16,        DPO_writeUint16,
   ATTR_CONST(g_stDPO.m_unTargetNwkBitMask),            DMAP_ReadUint16,        DPO_writeUint16,
   ATTR_CONST(g_stDPO.m_ucTargetJoinMethod),            DMAP_ReadUint8,         DPO_writeUint8,
   ATTR_CONST(g_stDPO.m_aTargetSecurityMngrEUI),        DMAP_ReadOctetString,   DMAP_WriteOctetString,
   ATTR_CONST(g_stDMO.m_aucSysMng128BitAddr),           DMAP_ReadOctetString,   DMAP_WriteOctetString,   
   ATTR_CONST(g_stDPO.m_unTargetFreqList),              DMAP_ReadUint16,        DPO_writeUint16,
   NULL, MAX_APDU_SIZE,                                 DPO_ReadDLConfigInfo,   DPO_WriteDLConfigInfo,
   NULL, 0,                                             DMAP_EmptyReadFunc,     NULL,
   NULL, 0,                                             DMAP_EmptyReadFunc,     NULL,
   NULL, 0,                                             DMAP_EmptyReadFunc,     NULL
   
#ifdef BACKBONE_SUPPORT  
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
#else 
   ,ATTR_CONST(g_stDPO.m_nCurrentUTCAdjustment),         DMAP_ReadUint16,       DMAP_WriteUint16

   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,ATTR_CONST(g_stDPO.m_unDeviceRole),                  DMAP_ReadUint16,       DPO_WriteDeviceRole
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
#endif     

   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL
   ,NULL, 0,                                             DMAP_EmptyReadFunc,    NULL     

   ,ATTR_CONST(g_stDPO.m_unCustomAlgorithms),            DMAP_ReadUint16,       DPO_writeUint16
   ,ATTR_CONST(g_stDPO.m_unBannedTimeout),               DMAP_ReadUint16,       DPO_writeUint16
   ,ATTR_CONST(g_stDPO.m_ucAlarmBandwidth),              DMAP_ReadUint8,        DPO_writeUint8
   ,ATTR_CONST(g_stDPO.m_ucAlarmResetTimeout),           DMAP_ReadUint8,        DPO_writeUint8
};

  //from the joining device point of view
  #define ROUTER_JREQ_ADV_RX_BASE   1
  #define ROUTER_JREQ_TX_OFF        3
  #define ROUTER_JRESP_RX_OFF       5

  const DLL_SMIB_ENTRY_SUPERFRAME  c_aJoinSuperframe = 
  {
    {
      JOIN_SF,   // uid
      12000,  // 10480*0,95367us=9.9945ms //m_unTimeslotsLength -must be a little shorter than 10ms!
      3,      // m_unChIndex (channels 4, 9, 14)  
      50,     // m_unSfPeriod
      0x00,   // m_unSfBirth        
      0x7fff,   // m_unChMap        
      0,      // m_ucChBirth  
      0x00,   // m_ucInfo
      0,      // m_lIdleTimer                     
      0,      // m_ucChRate
      0,      // m_ucRndSlots  
      0,      // m_ucChLen
      0, 0, 0 // m_unCrtOffset, m_ucCrtChOffset, m_ucCrtSlowHopOffset
    }
  };  
  
  const DLL_SMIB_ENTRY_LINK c_stNwkDiscoveryLink =
  {
      // rx link along all superframe (range_
      {JOIN_ADV_RX_LINK_UID,
       JOIN_SF, // m_ucSuperframeUID
       DLL_MASK_LNKRX,                // m_mfLinkType Rx link
       0,                             // m_ucPriority
       DEFAULT_DISCOVERY_TEMPL_UID,   // m_ucTemplate1
       0,                             // m_ucTemplate2
       (uint8)( DLL_LNK_GROUP_MASK | (2 << DLL_ROT_LNKSC)),    // m_mfTypeOptions // options: group of neighbors, schedule typr = range 
       0, // m_ucChOffset
       0, // m_unNeighbor
       0, // m_mfGraphId
       {
         0, 49 // range: first, last
       } // m_aSchedule
      }
  };
  

#ifdef BACKBONE_SUPPORT

  const DLL_MIB_ADV_JOIN_INFO c_stDefJoinInfo =
  {
    0x24,     // Join timeout is 2^4 = 16 sec, Join backof is 2^2 = 4 times
    DLL_MASK_ADV_RX_FLAG,   //all join links type Offset only 
    ROUTER_JREQ_TX_OFF,
    0,
    ROUTER_JRESP_RX_OFF,
    0,
    ROUTER_JREQ_ADV_RX_BASE,
    0
  };

#endif

  
  typedef struct {
    uint16                m_unVersion;
    uint16                m_unAssignedDevRole;
    
    uint8                 m_aTagName[16];
    uint8                 m_aucVendorID[16];
    uint8                 m_aucModelID[16];
    uint8                 m_aucSerialNo[16];
    
    struct
    {
      struct
      {
            uint8   m_aNetworkAddr[16]; //IPv6 address of remote enpoint
            uint16  m_unTLPort; //transport layer port at remote endpoint
            uint16  m_unObjID; //object identifier at remote endpoint
      } m_stEndPoint;
      uint8    m_ucConfTimeout;    //timeout waiting for alert confirmation
      uint8    m_ucAlertsDisable;  //disable all alerts for this category
      uint8    m_ucRecoveryAlertsDisable; 
      uint8    m_ucRecoveryAlertsPriority; 
    }   m_aAlerts[6];
    
    uint8  m_ucJoinedAfterProvisioning;  // FALSE when reprovisioned; TRUE when joined and ARMO is configured;
    
    uint16  m_unCRC;    // Obs: Always leave this member in the last position in struct    
  } DPO_PERSISTENT;
  
  void DPO_SavePersistentData(void);
  void DPO_ExtractPersistentData(void);
  
 
#ifndef BACKBONE_SUPPORT  
    void DPO_setToDefault(void);
#endif    


#ifdef BACKBONE_SUPPORT  
  void DPO_Init(void)
  {
      // Necesary for initialization of JoinBackoff and JoinTimeout
      DLME_SetMIBRequest(DL_ADV_JOIN_INFO, &c_stDefJoinInfo);
      
      DPO_ExtractPersistentData();
            
      g_stDPO.m_unDeviceRole = DEVICE_ROLE;
      g_stDPO.m_ucAlarmResetTimeout = 180;
      g_stDPO.m_ucAlarmBandwidth = 30;
      
      g_unDllSubnetId = g_stFilterTargetID;      
  }
  
  void DPO_ReadDLConfigInfo(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize)
  {
  }

  void DPO_WriteDLConfigInfo(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize)
  {    
  }    
  
  void DPO_ResetToDefault( void )
  {
  }
  
  uint8 DPO_WriteSYMJoinKey(uint16  p_unReqSize, 
                            uint8*  p_pReqBuf,
                            uint16* p_pRspSize,
                            uint8*  p_pRspBuf)
  {
    uint8 ucSFC = SFC_OBJECT_STATE_CONFLICT;
    *p_pRspSize = 0;
    return ucSFC;
  }
  

  __monitor uint8 DPO_Execute(uint8   p_ucMethID,
                     uint16  p_unReqSize, 
                     uint8*  p_pReqBuf,
                     uint16* p_pRspSize,
                     uint8*  p_pRspBuf)
  {
    uint8 ucSFC = SFC_OBJECT_STATE_CONFLICT;
    *p_pRspSize = 0;
    return ucSFC;
  }
      
#else // ! BACKBONE_SUPPORT

void DPO_writeUint8(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  *(uint8*)(p_pValue) = *p_pBuf;
  DPO_FlashDPOInfo();
}

void DPO_writeUint16(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  *(uint16*)(p_pValue) = (uint16)(*(p_pBuf++)) << 8;
  *(uint16*)(p_pValue) |= (uint16)(*(p_pBuf++));
  DPO_FlashDPOInfo();
}

void DPO_writeVisibleString(void * p_pValue, const uint8 * p_pBuf, uint8 p_ucSize)
{
  memcpy(p_pValue, p_pBuf, p_ucSize);
  DPO_FlashDPOInfo();
}

  
void DPO_FlashDPOInfo(void)
{
    if( !g_ucIsOOBAccess )
    {
        DPO_SavePersistentData();
    
        if( g_stDPO.m_ucStructSignature == DPO_SIGNATURE )
        {
      #if (SECTOR_FLAG)
          uint8 aDlConfigInfo[MAX_APDU_SIZE+1];
          
          ReadPersistentData( aDlConfigInfo, PROVISION_START_ADDR + 256, sizeof(aDlConfigInfo) );
          
          EraseSector( PROVISION_SECTOR_NO );
          
          WritePersistentData( aDlConfigInfo, PROVISION_START_ADDR + 256, sizeof(aDlConfigInfo) );
    #endif
          
          WritePersistentData( (uint8*)&g_stDPO, PROVISION_START_ADDR, sizeof(g_stDPO) ); 
        }
    }
    else // OOB flash protection
    {
        g_ucIsOOBAccess = 7;
    }
}

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC
  /// @brief  Initializes the device provisioning object
  /// @param  none
  /// @return none
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////      
  void DPO_Init(void)
  {
    uint8 aDlConfigInfo[MAX_APDU_SIZE+1];
    
    ReadPersistentData( (uint8*)&g_stDPO, PROVISION_START_ADDR, sizeof(g_stDPO) );
    ReadPersistentData( aDlConfigInfo, PROVISION_START_ADDR + 256, sizeof(aDlConfigInfo) );
    
#ifdef RESET_TO_FACTORY
    //need to be called after the g_stDPO structure is populated - the allow provisioning flag will be checked 
    DPO_ResetToDefault(); 
    ReadPersistentData( (uint8*)&g_stDPO, PROVISION_START_ADDR, sizeof(g_stDPO) );
    
    
    //reset to defaults the application data
    uint16 unFormatVersion = 0xFFFF;
    //at loading persistent data, the both CRCs(First + Second) will fail and will start with defaults values     
    #if (SECTOR_FLAG)
    EraseSector( FIRST_APPLICATION_SECTOR_NO );
    #else
    WritePersistentData( (uint8*)&unFormatVersion, FIRST_APPLICATION_START_ADDR, sizeof(unFormatVersion) );
    #endif
    
    
    #if (SECTOR_FLAG)
    EraseSector( SECOND_APPLICATION_SECTOR_NO );
    #else    
    WritePersistentData( (uint8*)&unFormatVersion, SECOND_APPLICATION_START_ADDR, sizeof(unFormatVersion) );
    #endif
#endif
    
    uint8 ucDlConfigInfoSize = aDlConfigInfo[0];
    uint8 ucAttrNo = aDlConfigInfo[1];
    
    
    if( (DPO_SIGNATURE != g_stDPO.m_ucStructSignature) || (ucDlConfigInfoSize > MAX_APDU_SIZE) ) // unknown signature
    {  
        DPO_setToDefault();
    }
    else
    {
        ATTR_IDTF stAttrIdtf;
        
        uint8 * pDLLAttr = aDlConfigInfo+2;
        uint8 * pEndDLLAttr = aDlConfigInfo+1+ucDlConfigInfoSize;
        while( (ucAttrNo--) && (pDLLAttr < pEndDLLAttr) )
        {
          
          stAttrIdtf.m_unAttrID = *(pDLLAttr++);
          stAttrIdtf.m_unIndex1 = 0xFFFF;
          
          uint16 unAttrSize = pEndDLLAttr-pDLLAttr;
          if( DLMO_Write( &stAttrIdtf, &unAttrSize, pDLLAttr ) != SFC_SUCCESS )
          {            
              DPO_setToDefault();   // corrupted flash
              break;
          }
          DLME_ParseMsgQueue(); // avoid getting full of DLME action queue 
          
          pDLLAttr += unAttrSize;            
        }
    }
    
#if ( _UAP_TYPE == UAP_TYPE_ISA100_API )
    if( g_ucDAQDeviceRole )
    {
        //restore the DeviceRole configuration requested by DAQ
        //the ISA100_GET_INITIAL_INFO is sent by the modem just after a hardware reset
        g_stDPO.m_unDeviceRole = g_ucDAQDeviceRole;
    }
    else
#endif
    {
        //hardware reset or unsuported DeviceRole configuration form the DAQ
        if( !g_stDPO.m_unDeviceRole || (g_stDPO.m_unDeviceRole > DEVICE_ROLE) )
            g_stDPO.m_unDeviceRole = DEVICE_ROLE;
    }
    
    MLSM_SetSlotLength(g_aDllSuperframesTable[0].m_stSuperframe.m_unTsDur);  
    
    if( g_stDPO.m_unBannedTimeout == 0xFFFF )
    {
        g_stDPO.m_unBannedTimeout = 180;
        g_stDPO.m_ucAlarmResetTimeout = 180;
        g_stDPO.m_ucAlarmBandwidth = 30;
    }
    
    g_stFilterBitMask  = g_stDPO.m_unTargetNwkBitMask;
    g_stFilterTargetID = g_stDPO.m_unTargetNwkID;
    
    DPO_ExtractPersistentData();
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC
  /// @brief  Reads data link layer related configuration information
  /// @param  p_pValue - input buffer
  /// @param  p_pBuf - buffer to receive data
  /// @param  p_ucSize - data size
  /// @return none
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////  
  void DPO_ReadDLConfigInfo(const void* p_pValue, uint8* p_pBuf, uint8* p_ucSize)
  {
      ReadPersistentData( p_pBuf, PROVISION_START_ADDR + 256, 1 );
      if( p_pBuf[0] < MAX_APDU_SIZE )
      {
          *p_ucSize = p_pBuf[0];
          ReadPersistentData( p_pBuf, PROVISION_START_ADDR + 256 + 1, p_pBuf[0] );
      }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC
  /// @brief  Writes data link layer related configuration information
  /// @param  p_pValue - data buffer 
  /// @param  p_pBuf - input data
  /// @param  p_ucSize - data size
  /// @return none
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////    
  void DPO_WriteDLConfigInfo(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize)
  {    
      uint8 aDlConfigInfo[MAX_APDU_SIZE+1];
            
      aDlConfigInfo[0] = p_ucSize;
      memcpy( aDlConfigInfo+1, p_pBuf, p_ucSize );
      
  #if (SECTOR_FLAG)
      EraseSector( PROVISION_SECTOR_NO );
      WritePersistentData( (uint8*)&g_stDPO, PROVISION_START_ADDR, sizeof(g_stDPO) );
  #endif
      
      WritePersistentData( aDlConfigInfo, PROVISION_START_ADDR + 256, 1+p_ucSize );      
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC
  /// @brief  Resets provisioning 
  /// @param  none
  /// @return none
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////  
  void DPO_ResetToDefault(void)
  {   
    //reset the default settings for the provisioning
    EraseSector(PROVISION_SECTOR_NO);
#if (SECTOR_FLAG == 0) // eeprom
    static const uint8 c_aFF = 0xFF;  
    WritePersistentData( &c_aFF, PROVISION_START_ADDR, 1 );  
#endif    
    
    // clear the joinedAndConfigured flag from  application persistent data
    g_stDPO.m_ucJoinedAfterProvisioning = 0;
    DPO_SavePersistentData();    
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC
  /// @brief  Write a symmetric AES join key to the device
  /// @param  p_unReqSize - key data size
  /// @param  p_pReqBuf - buffer containing key data
  /// @param  p_unRspSize - response size
  /// @param  p_pRspBuf - buffer containing response 
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////    
  uint8 DPO_WriteSYMJoinKey(uint16  p_unReqSize, 
                            uint8*  p_pReqBuf,
                            uint16* p_pRspSize,
                            uint8*  p_pRspBuf)
  {
   //Table 414 – Write symmetric join key method
  //This method is used to write a symmetric AES join key to a device. This method is
  //evoked by the DPSO to provision a DBP with the Target AES join key. Depending
  //on the provisioning method used the join key may be encrypted by the session key
  //or the device’s public key. This method shall be executed only when
  //Allow_Provisioning is enabled  
    
    //arguments uint128 key value, uint8 encripted by)
    if(17 != p_unReqSize) 
      return SFC_INVALID_SIZE;
    
    uint8 aKeyValue[16]; 
    memcpy(aKeyValue, p_pReqBuf, 16);
      
    memcpy( g_stDPO.m_aJoinKey, aKeyValue, 16 );
      
    *p_pRspSize = 1;
    *p_pRspBuf = 0; //success
    
    return SFC_SUCCESS;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Mihaela Goloman
  /// @brief  Executes a DPO method
  /// @param  p_unMethID - method identifier
  /// @param  p_unReqSize - input parameters size
  /// @param  p_pReqBuf - input parameters buffer
  /// @param  p_pRspSize - output parameters size
  /// @param  p_pRspBuf - output parameters buffer
  /// @return service feedback code
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  __monitor uint8 DPO_Execute(uint8   p_ucMethID,
                     uint16  p_unReqSize, 
                     uint8*  p_pReqBuf,
                     uint16* p_pRspSize,
                     uint8*  p_pRspBuf )
  {
    uint8 ucSFC = SFC_INVALID_METHOD;
    
    if( !DPO_CkAccess() )
    {
        return SFC_OBJECT_ACCESS_DENIED;
    }
    
    MONITOR_ENTER();
    switch(p_ucMethID)
    {
        case DPO_RESET_TO_DEFAULT: 
            *p_pRspSize = 1;                             
            *p_pRspBuf  = 0; //success
            ucSFC = SFC_SUCCESS;
            g_stDMO.m_ucJoinCommand = DMO_PROV_RESET_FACTORY_DEFAULT;
            g_unJoinCommandReqId = JOIN_CMD_APPLY_ASAP_REQ_ID;  //force the reset next second 
            break;

        case DPO_WRITE_SYMMETRIC_JOIN_KEY:   
            ucSFC = DPO_WriteSYMJoinKey(p_unReqSize, p_pReqBuf, p_pRspSize, p_pRspBuf); 
            break;
    }  
    MONITOR_EXIT();
    return ucSFC;
  }
  
  
  ///////////////////////////////////////////////////////////////////////////////////////
  void DPO_WriteDeviceRole(void* p_pValue, const uint8* p_pBuf, uint8 p_ucSize)
  {
      if( 2 != p_ucSize )
          return;
      
      DMAP_ExtractUint16(p_pBuf, (uint16*)&g_stDPO.m_unDeviceRole); 
      DPO_FlashDPOInfo();
  }
  
  
#pragma diag_suppress=Pa039                     
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Mihaela Goloman
  /// @brief  Sets DPO to default
  /// @param  none
  /// @return none
  /// @remarks
  ///      Access level: user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////    
  void DPO_setToDefault(void)
  {
      uint16 unVersionNo = 0;
      memcpy(&g_stDPO, &c_stDefaultDPO, sizeof(g_stDPO));
      ReadPersistentData( (uint8*)&unVersionNo, MANUFACTURING_START_ADDR, 2 );
      if( unVersionNo )
      {
          ReadPersistentData( g_stDPO.m_aJoinKey,  
                             (unVersionNo & 0xFEFE ? MANUFACTURING_START_ADDR+12+16 : MANUFACTURING_START_ADDR+12+2) , 
                             sizeof(g_stDPO.m_aJoinKey) + sizeof(g_stDPO.m_aTargetSecurityMngrEUI) );          
      }
      
      DLMO_DOUBLE_IDX_SMIB_ENTRY stTwoIdxSMIB;
      stTwoIdxSMIB.m_unUID = c_aJoinSuperframe.m_stSuperframe.m_unUID;
      memcpy(&stTwoIdxSMIB.m_stEntry, &c_aJoinSuperframe.m_stSuperframe, sizeof(c_aJoinSuperframe.m_stSuperframe)); 
      DLME_SetSMIBRequest( DL_SUPERFRAME, 0, &stTwoIdxSMIB );
  
      stTwoIdxSMIB.m_unUID = c_stNwkDiscoveryLink.m_stLink.m_unUID;
      memcpy(&stTwoIdxSMIB.m_stEntry, &c_stNwkDiscoveryLink.m_stLink, sizeof(c_stNwkDiscoveryLink.m_stLink)); 
      DLME_SetSMIBRequest( DL_LINK, 0, &stTwoIdxSMIB );          
          
      DLME_ParseMsgQueue(); // avoid getting full of DLME action queue 
  }   

#pragma diag_default=Pa039
#endif  // ! BACKBONE_SUPPORT
  
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin, Eduard Erdei
/// @brief  Save persistently the critical data 
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DPO_SavePersistentData(void)
{
    if( !g_ucIsOOBAccess )
    {
        DPO_PERSISTENT stCrt;
        DPO_PERSISTENT stOld;
        
        memset( &stCrt, 0, sizeof(stCrt) );
        
        stCrt.m_unVersion = 1;
        stCrt.m_unAssignedDevRole = g_stDMO.m_unAssignedDevRole;
        memcpy( stCrt.m_aTagName, g_stDMO.m_aucTagName, sizeof(stCrt.m_aTagName) );
        memcpy( stCrt.m_aucVendorID, g_stDMO.m_aucVendorID, sizeof(stCrt.m_aucVendorID) );
        memcpy( stCrt.m_aucModelID, g_stDMO.m_aucModelID, sizeof(stCrt.m_aucModelID) );
        memcpy( stCrt.m_aucSerialNo, g_stDMO.m_aucSerialNo, sizeof(stCrt.m_aucSerialNo) );
        
        for(unsigned  int i=0; i<4; i++ )
        {
            memcpy( &stCrt.m_aAlerts[i].m_stEndPoint, &g_aARMO[i].m_stAlertMaster, sizeof(stCrt.m_aAlerts[i].m_stEndPoint) );
            stCrt.m_aAlerts[i].m_ucConfTimeout = g_aARMO[i].m_ucConfTimeout;
            stCrt.m_aAlerts[i].m_ucAlertsDisable = g_aARMO[i].m_ucAlertsDisable;
            stCrt.m_aAlerts[i].m_ucRecoveryAlertsDisable = g_aARMO[i].m_stRecoveryDescr.m_bAlertReportDisabled;
            stCrt.m_aAlerts[i].m_ucRecoveryAlertsPriority = g_aARMO[i].m_stRecoveryDescr.m_ucPriority;
        }    
    
        stCrt.m_ucJoinedAfterProvisioning = g_stDPO.m_ucJoinedAfterProvisioning;
        
        stCrt.m_unCRC = ComputeCCITTCRC( (const uint8*)&stCrt, (uint32)&((DPO_PERSISTENT*)0)->m_unCRC );
        
        ReadPersistentData( (uint8*)&stOld, FIRST_APPLICATION_START_ADDR, sizeof(stOld) );    
        
        if( memcmp(&stCrt, &stOld, sizeof(stOld)) ) // changed or corrupted, save it 
        {                       
        #if (SECTOR_FLAG)
            EraseSector( FIRST_APPLICATION_SECTOR_NO );
        #endif
              
            WritePersistentData( (uint8*)&stCrt, FIRST_APPLICATION_START_ADDR, sizeof(stCrt) ); 
            
        #if (SECTOR_FLAG)
            EraseSector( SECOND_APPLICATION_SECTOR_NO );
        #endif
            
            WritePersistentData( (uint8*)&stCrt, SECOND_APPLICATION_START_ADDR, sizeof(stCrt) ); 
            
        }
    }
    else
    {
        g_ucIsOOBAccess |= 2;
    }
}

void asciiChar( uint8 p_ucHex, uint8 * p_pStr )
{
    uint8 ucHex = p_ucHex >> 4;
    
    p_pStr[0] = (ucHex < 10 ?  '0' + ucHex : 'A' - 10 + ucHex);
    
    ucHex = p_ucHex & 0x0F;
    p_pStr[1] = (ucHex < 10 ?  '0' + ucHex : 'A' - 10 + ucHex);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin, Eduard Erdei
/// @brief  Read persistent data 
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void DPO_ExtractPersistentData(void)
{
    DPO_PERSISTENT stCrt;
    
    memset(&g_aARMO, 0, sizeof(g_aARMO));

    ReadPersistentData( (uint8*)&stCrt, FIRST_APPLICATION_START_ADDR, sizeof(stCrt) );
    
    uint16 unCRC = ComputeCCITTCRC( (const uint8*)&stCrt, (uint32)&((DPO_PERSISTENT*)0)->m_unCRC );
    
    if( unCRC != stCrt.m_unCRC )
    {
        // first sector is corrupted; try second sector 
        ReadPersistentData( (uint8*)&stCrt, SECOND_APPLICATION_START_ADDR, sizeof(stCrt) );
        
        unCRC = ComputeCCITTCRC( (const uint8*)&stCrt, (uint32)&((DPO_PERSISTENT*)0)->m_unCRC );
        
        if( unCRC != stCrt.m_unCRC )
        {
            // second sector is corrupted too; 
            stCrt.m_unVersion = 0xFF;  // ignore persistent data; use default values
        }
    }
    
    if( stCrt.m_unVersion == 1 )
    {
        memcpy( g_stDMO.m_aucTagName, stCrt.m_aTagName, sizeof(g_stDMO.m_aucTagName) );
        memcpy( g_stDMO.m_aucVendorID, stCrt.m_aucVendorID, sizeof(g_stDMO.m_aucVendorID) );
        memcpy( g_stDMO.m_aucModelID, stCrt.m_aucModelID, sizeof(g_stDMO.m_aucModelID) );
        memcpy( g_stDMO.m_aucSerialNo, stCrt.m_aucSerialNo, sizeof(g_stDMO.m_aucSerialNo) );
        
        g_stDMO.m_unAssignedDevRole = stCrt.m_unAssignedDevRole;
        g_stDPO.m_ucJoinedAfterProvisioning = stCrt.m_ucJoinedAfterProvisioning; 
        
        for(unsigned  int i=0; i<4; i++ )
        {
            memcpy( &g_aARMO[i].m_stAlertMaster, &stCrt.m_aAlerts[i].m_stEndPoint, sizeof(g_aARMO[i].m_stAlertMaster) );
            g_aARMO[i].m_ucConfTimeout = stCrt.m_aAlerts[i].m_ucConfTimeout;
            g_aARMO[i].m_ucAlertsDisable = stCrt.m_aAlerts[i].m_ucAlertsDisable;
            g_aARMO[i].m_stRecoveryDescr.m_bAlertReportDisabled = stCrt.m_aAlerts[i].m_ucRecoveryAlertsDisable;
            g_aARMO[i].m_stRecoveryDescr.m_ucPriority = stCrt.m_aAlerts[i].m_ucRecoveryAlertsPriority;
        }
    }
    else
    {
      //an update event will save the hardcoded values as persistent
      for(unsigned  int i=0; i<8; i++ )
      {
          asciiChar(c_oEUI64BE[i], g_stDMO.m_aucTagName+i*2);          
      }

      g_stDMO.m_aucTagName[0] = 'T'; // not standard but want to diferentiate between TAG ans EUI64     
      
      memcpy( g_stDMO.m_aucVendorID, "NIVIS           ", sizeof(g_stDMO.m_aucVendorID) );
      memcpy( g_stDMO.m_aucModelID, "FREESCALE_VN210 ", sizeof(g_stDMO.m_aucModelID) );
      memcpy( g_stDMO.m_aucSerialNo, g_stDMO.m_aucTagName, sizeof(g_stDMO.m_aucSerialNo) ); 
      g_stDMO.m_aucSerialNo[0] = 'S';  
      
      g_stDPO.m_ucJoinedAfterProvisioning = 0;
    }
}
  

uint8 DPO_CkAccess(void)
{
    if( !g_stDPO.m_ucAllowProvisioning )
        return 0;
                    
    if( !g_ucIsOOBAccess ) // is over the air
        return g_stDPO.m_ucAllowOverTheAirProvisioning;

    return g_stDPO.m_ucAllowOOBProvisioning;
 }
