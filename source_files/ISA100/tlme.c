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
/// Author:       Nivis LLC, Ion Ticus, Adrian Simionescu
/// Date:         June 2009
/// Description:  Implements transport layer management entity from ISA100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "tlme.h"
#include "sfc.h"
#include "dmap.h"
#include "dmap_armo.h"

#define MAX_TSDU_SIZE    88   // Valid value set: 13 – 65535  

#define    TLME_ATTR_RO 0     // read only 
#define    TLME_ATTR_RW 1     // read/write

typedef struct
{
    uint8 * m_pValue;
    uint8   m_ucSize;
    uint8   m_ucFlags;
} TLME_ATTRIBUTES_PARAMS;

// TLMO Attributes
const TLME_ATTRIBUTES_PARAMS c_aTLME_AttributesParams[ TLME_ATRBT_ID_NO ] =
 {      
     {NULL,  0,  TLME_ATTR_RO }, //no used      
     {(uint8*)&g_stTlme.m_unMaxTSDUsize,  sizeof(g_stTlme.m_unMaxTSDUsize),  TLME_ATTR_RO }, // TLME_ATRBT_MAX_TSDU_SIZE      
     {(uint8*)&g_stTlme.m_unMaxNbOfPorts, sizeof(g_stTlme.m_unMaxNbOfPorts), TLME_ATTR_RW }, // TLME_ATRBT_MAX_NB_OF_PORTS      
     
     {(uint8*)(g_stTlme.m_aTPDUCount + TLME_PORT_TPDU_IN),    sizeof(TPDU_COUNTER), TLME_ATTR_RO }, // TLME_ATRBT_PORT_TPDU_IN          
     {NULL,                                                   sizeof(TPDU_COUNTER), TLME_ATTR_RO }, // TLME_ATRBT_PORT_TPDU_IN_REJECTED          
     {(uint8*)(g_stTlme.m_aTPDUCount + TLME_PORT_TPDU_IN_OK), sizeof(TPDU_COUNTER), TLME_ATTR_RO }, // TLME_PORT_TPDU_IN_SUCCESS
     {(uint8*)(g_stTlme.m_aTPDUCount + TLME_PORT_TPDU_OUT),   sizeof(TPDU_COUNTER), TLME_ATTR_RO }, // TLME_PORT_TPDU_OUT
     {NULL,                                                   sizeof(TPDU_COUNTER), TLME_ATTR_RO }, // TLME_PORT_TPDU_OUT_REJECTED                    
     {(uint8*)(g_stTlme.m_aTPDUCount + TLME_PORT_TPDU_OUT_OK),sizeof(TPDU_COUNTER), TLME_ATTR_RO }, // TLME_PORT_TPDU_OUT_SUCCESS
          
     {NULL, 2, TLME_ATTR_RW },  // TLME_ATRBT_ILLEGAL_USE_OF_PORT
     {NULL, 2, TLME_ATTR_RW },  // TLME_ATRBT_TPDU_ON_UNREG_PORT
     {NULL, 2, TLME_ATTR_RW }   // TLME_ATRBT_TPDU_OUT_OF_SEC_POL
};

TLME_STRUCT g_stTlme;

TLME_PORT_INFO* TLME_findPortInfo( const uint8* p_pDeviceAddr, uint16 p_unPortNb );

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Initializes the TLMO attributes
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void TLME_Init(void)
{
    memset( &g_stTlme, 0, sizeof(g_stTlme) );
    g_stTlme.m_unMaxTSDUsize = MAX_TSDU_SIZE;
    g_stTlme.m_unMaxNbOfPorts = MAX_NB_OF_PORTS;
    g_stTlme.m_aAlertDesc[TLME_ILLEGAL_USE_OF_PORT] = ALERT_PR_MEDIUM_H;
    g_stTlme.m_aAlertDesc[TLME_TPDU_ON_UNREG_PORT]  = ALERT_PR_LOW_M;
    g_stTlme.m_aAlertDesc[TLME_TPDU_OUT_OF_SEC_POL] = ALERT_PR_JOURNAL_H;    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Reset the transport to initial states
/// @param  
/// @return none
/// @remarks
///      Access level: user level 
////////////////////////////////////////////////////////////////////////////////////////////////////

void TLME_Reset( const uint8 p_ucForced )      
{
    TLME_Init();
} 

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Halts a port. Like a reset, but scoped to one UDP port only
/// @param  p_pDeviceAddr - The device 128-bit address
/// @param  p_unPortNr - The port to halt
/// @return none
/// @remarks
///      Access level:  user level
////////////////////////////////////////////////////////////////////////////////////////////////////

void TLME_Halt( const uint8* p_pDeviceAddr, uint16 p_unPortNr )  
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Reports the UDP ports range information
/// @param  p_pDeviceAddr - The device 128-bit address 
/// @param  p_pPortRange - The requested UDP ports range information
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void  TLME_GetPortRangeInfo( const uint8* p_pDeviceAddr, TLME_PORT_RANGE_INFO * p_pPortRange )   
{  
  memset( p_pPortRange, 0, sizeof(*p_pPortRange) );
  
  TLME_PORT_INFO * pPortInfo = g_stTlme.m_aPortInfo;
  TLME_PORT_INFO * pPortInfoEnd = g_stTlme.m_aPortInfo+g_stTlme.m_ucNbOfPorts;
  
  for( ; pPortInfo < pPortInfoEnd; pPortInfo++ )
  {
     if ( !memcmp( p_pDeviceAddr, pPortInfo->m_aIpv6Addr, sizeof( pPortInfo->m_aIpv6Addr) ) )
     {
        if( !p_pPortRange->m_unNbActivePorts )
        {
            p_pPortRange->m_unFirstActivePort = pPortInfo->m_unPortNb;
            p_pPortRange->m_unLastActivePort = pPortInfo->m_unPortNb;
        }
        else if( p_pPortRange->m_unFirstActivePort > pPortInfo->m_unPortNb )
        {
            p_pPortRange->m_unFirstActivePort = pPortInfo->m_unPortNb;
        }
        else if( p_pPortRange->m_unLastActivePort < pPortInfo->m_unPortNb )
        {
            p_pPortRange->m_unLastActivePort = pPortInfo->m_unPortNb;
        }
          
        p_pPortRange->m_unNbActivePorts ++;
     }
  }    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Reports the UDP port information for a given UDP port or the first active UDP port 
/// @param  p_pDeviceAddr - The device 128-bit address 
/// @param  p_unPortNb - The port whose info is requested
/// @param  p_pPortInfo - The requested UDP port information 
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 TLME_GetPortInfo( const uint8* p_pDeviceAddr, uint16 p_unPortNb, TLME_PORT_INFO * p_pPortInfo ) 
{
  TLME_PORT_INFO * pPortInfo = TLME_findPortInfo( p_pDeviceAddr, p_unPortNb);
  
  if( pPortInfo )
  {
      memcpy( p_pPortInfo, pPortInfo, sizeof(*p_pPortInfo) );
      p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_IN]  -= p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_IN_OK];
      p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_OUT] -= p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_OUT_OK];
      return SFC_SUCCESS;
  }
  
  return SFC_INVALID_ELEMENT_INDEX;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Reports the UDP port information for a given UDP port or the first active UDP port 
/// @param  p_pDeviceAddr - The device 128-bit address 
/// @param  p_unPortNb - The previous port from which info is requested
/// @param  p_pPortInfo - The requested UDP port information 
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 TLME_GetNextPortInfo( const uint8* p_pDeviceAddr, uint16 p_unPortNb, TLME_PORT_INFO * p_pPortInfo )
{
    TLME_PORT_INFO * pPortInfo = TLME_findPortInfo( p_pDeviceAddr, p_unPortNb );
          
    if( pPortInfo )
    {
        TLME_PORT_INFO * pPortInfoEnd = g_stTlme.m_aPortInfo+g_stTlme.m_ucNbOfPorts;
        for( ; pPortInfo < pPortInfoEnd; pPortInfo++ )
        {
           if ( !memcmp( p_pDeviceAddr, pPortInfo->m_aIpv6Addr, sizeof( pPortInfo->m_aIpv6Addr) ) )
           {
                memcpy( p_pPortInfo, pPortInfo, sizeof(*p_pPortInfo) );
                p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_IN]  -= p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_IN_OK];
                p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_OUT] -= p_pPortInfo->m_aTPDUCount[TLME_PORT_TPDU_OUT_OK];
                return SFC_SUCCESS;
           }
        }
    }
    
    return SFC_INVALID_ELEMENT_INDEX;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Find the UDP port information for a given UDP port or the first active UDP port 
/// @param  p_pDeviceAddr - The device 128-bit address 
/// @param  p_unPortNb - The port whose info is requested
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
TLME_PORT_INFO* TLME_findPortInfo( const uint8* p_pDeviceAddr, uint16 p_unPortNb ) 
{
  TLME_PORT_INFO * pPortInfo = g_stTlme.m_aPortInfo;
  TLME_PORT_INFO * pPortInfoEnd = g_stTlme.m_aPortInfo+g_stTlme.m_ucNbOfPorts;
  
  for( ; pPortInfo < pPortInfoEnd; pPortInfo++ )
  {
     if ( !memcmp( p_pDeviceAddr, pPortInfo->m_aIpv6Addr, sizeof( pPortInfo->m_aIpv6Addr)) && (p_unPortNb== 0 || p_unPortNb == pPortInfo->m_unPortNb) )
       return pPortInfo;
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Set the UDP port information for a given UDP port
/// @param  p_pDeviceAddr - The device 128-bit address
/// @param  p_unPortNb - The port whose info will be set
/// @param  p_ucAction - Port action
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void TLME_SetPortInfo( const uint8* p_pDeviceAddr, uint16 p_unPortNb, PORT_ACTION p_ucAction )
{
  TLME_PORT_INFO * pPortInfo = TLME_findPortInfo( p_pDeviceAddr, p_unPortNb);
  
  if( !pPortInfo && g_stTlme.m_ucNbOfPorts < MAX_NB_OF_PORTS)
  {
      pPortInfo = g_stTlme.m_aPortInfo + g_stTlme.m_ucNbOfPorts;
    
      memcpy( pPortInfo->m_aIpv6Addr, p_pDeviceAddr, sizeof(pPortInfo->m_aIpv6Addr));
      pPortInfo->m_unPortNb = p_unPortNb;  
      pPortInfo->m_unUID =  p_unPortNb & 0x0F;
      g_stTlme.m_ucNbOfPorts++;
  }

  g_stTlme.m_aTPDUCount[p_ucAction] ++;
  
  if( pPortInfo )
  {
      pPortInfo->m_aTPDUCount[p_ucAction] ++;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Add Alert
/// @param  p_unAlertType - Alert type descriptor
/// @param  p_pData - Alert data
/// @param  p_unDataSize - Alert data size
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
void  TLME_Alert( TLME_ALERT_TYPE p_unAlertType, const uint8 * p_pData, uint16 p_unDataSize )
{
    if( g_stTlme.m_aAlertDesc[p_unAlertType] & 0x80  ) // alert is enabled
    {
        ALERT stAlert;
        
        stAlert.m_ucPriority = g_stTlme.m_aAlertDesc[p_unAlertType] & 0x7F;
        stAlert.m_unDetObjTLPort = 0xF0B0; // TLME is DMAP port
        stAlert.m_unDetObjID = DMAP_TLMO_OBJ_ID; 
        stAlert.m_ucClass = ALERT_CLASS_EVENT; 
        stAlert.m_ucDirection = ALARM_DIR_IN_ALARM; 
        stAlert.m_ucCategory = ALERT_CAT_COMM_DIAG; 
        stAlert.m_ucType = p_unAlertType; 
        stAlert.m_unSize = p_unDataSize; 
        
        ARMO_AddAlertToQueue( &stAlert, p_pData );
    }        
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu
/// @brief  Set a transport layer SMIB row or MIB 
/// @param  p_ucAttributeID - attribute ID
/// @param  p_ulTaiCutover - TAI cutover (ignored)
/// @param  p_pData - value if MIB or index+index+value if SMIB
/// @param  p_ucDataLen - p_pData size
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 TLME_SetRow( uint8         p_ucAttributeID, 
                   uint32        p_ulTaiCutover, 
                   const uint8 * p_pData,
                   uint8         p_ucDataLen )
{
    if( p_ucAttributeID >= TLME_ATRBT_ID_NO )
    {
         return SFC_INVALID_ATTRIBUTE;
    }

    if( c_aTLME_AttributesParams[p_ucAttributeID].m_ucFlags == TLME_ATTR_RO )
    {
        return SFC_READ_ONLY_ATTRIBUTE;
    }

    if( p_ucDataLen != c_aTLME_AttributesParams[p_ucAttributeID].m_ucSize )
    {
        return SFC_INVALID_SIZE;
    }

    if( c_aTLME_AttributesParams[p_ucAttributeID].m_pValue && p_ucAttributeID == TLME_ATRBT_MAX_NB_OF_PORTS )
    {
        DMAP_ExtractUint16(p_pData, (uint16*)c_aTLME_AttributesParams[p_ucAttributeID].m_pValue);
    }
    else if( p_ucAttributeID >= TLME_ATRBT_ILLEGAL_USE_OF_PORT && p_ucAttributeID <= TLME_ATRBT_TPDU_OUT_OF_SEC_POL )
    {
        // spec attribute   
        DMAP_WriteCompressAlertDescriptor( g_stTlme.m_aAlertDesc +  p_ucAttributeID - TLME_ATRBT_ILLEGAL_USE_OF_PORT, p_pData, 2 );
    }
    else
    {
        return SFC_INCOMPATIBLE_MODE;
    }
  
    return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus, Adrian Simionescu, Eduard Erdei
/// @brief  Get a transport layer SMIB row or MIB
/// @param  p_ucAttributeID - attribute ID
/// @param  p_pIdxData - index to identify SMIB row (ignored if MIB)
/// @param  p_ucIdxDataLen - size of index
/// @param  p_pData - value if MIB or index+value if SMIB
/// @param  p_pDataLen - p_pData size
/// @return SFC code 
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 TLME_GetRow( uint8         p_ucAttributeID, 
                   const uint8 * p_pIdxData,
                   uint8         p_ucIdxDataLen,
                   uint8       * p_pData,
                   uint8       * p_pDataLen )
{
    if( p_ucAttributeID >= TLME_ATRBT_ID_NO )
    {
         return SFC_INVALID_ATTRIBUTE;
    }
    
    *p_pDataLen = c_aTLME_AttributesParams[p_ucAttributeID].m_ucSize;
    
    if( c_aTLME_AttributesParams[p_ucAttributeID].m_pValue )
    {                
        if( p_ucAttributeID < TLME_ATRBT_PORT_TPDU_IN )
        {
            DMAP_InsertUint16( p_pData, *(uint16*)(c_aTLME_AttributesParams[p_ucAttributeID].m_pValue));
        }
        else
        {
            DMAP_InsertUint32( p_pData, *(uint32*)(c_aTLME_AttributesParams[p_ucAttributeID].m_pValue));
        }
    }
    else if( p_ucAttributeID >= TLME_ATRBT_ILLEGAL_USE_OF_PORT &&  p_ucAttributeID <= TLME_ATRBT_TPDU_OUT_OF_SEC_POL )
    {
        DMAP_ReadCompressAlertDescriptor( g_stTlme.m_aAlertDesc +  p_ucAttributeID - TLME_ATRBT_ILLEGAL_USE_OF_PORT, p_pData, p_pDataLen );
    }
    else if( p_ucAttributeID == TLME_ATRBT_PORT_TPDU_IN_REJECTED ) 
    {
        TPDU_COUNTER ulTemp = g_stTlme.m_aTPDUCount[TLME_PORT_TPDU_IN] - g_stTlme.m_aTPDUCount[TLME_PORT_TPDU_IN_OK];        
        DMAP_InsertUint32( p_pData, ulTemp);
    }
    else if( p_ucAttributeID == TLME_ATRBT_PORT_TPDU_OUT_REJECTED ) 
    {
        TPDU_COUNTER ulTemp = g_stTlme.m_aTPDUCount[TLME_PORT_TPDU_OUT] - g_stTlme.m_aTPDUCount[TLME_PORT_TPDU_OUT_OK];
        DMAP_InsertUint32( p_pData, ulTemp);       
    }
    else
    {
        return SFC_INCOMPATIBLE_MODE;
    }
    
    return SFC_SUCCESS;
}


