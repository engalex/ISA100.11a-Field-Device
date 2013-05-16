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
/// Author:       Nivis LLC, Adrian Simionescu, Ion Ticus
/// Date:         January 2008
/// Description:  Implements Network Layer Management Entity of ISA100 standard
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "provision.h"
#include "nlme.h"
#include "string.h"
#include "sfc.h"
#include "dmap.h"
#include "dmap_armo.h"

 NLME_ATRIBUTES g_stNlme;


 
#if defined( BACKBONE_SUPPORT )
 const uint8  c_ucBackboneCapable = 1;
#else
 const uint8  c_ucBackboneCapable = 0;
#endif

 const uint8  c_ucDL_Capable = 1;
 const uint16 c_unMaxNsduSize = ((MAX_DATAGRAM_SIZE & 0xFF) << 8)  | (MAX_DATAGRAM_SIZE >> 8);

 const NLME_ATTRIBUTES_PARAMS c_aAttributesParams[ NLME_ATRBT_ID_NO - 1 ] =
 {
     // NLME_ATRBT_ID_BBR_CAPABLE
     {(uint8*)&c_ucBackboneCapable, sizeof(c_ucBackboneCapable), 0,  1 },   

     // NLME_ATRBT_ID_DL_CAPABLE
     {(uint8*)&c_ucDL_Capable, sizeof(c_ucDL_Capable), 0, 1 },             

     // NLME_ATRBT_ID_SHORT_ADDR
     {(uint8*)&g_unShortAddr, sizeof(g_unShortAddr), 0, 1 },  

     // NLME_ATRBT_ID_IPV6_ADDR
     {(uint8*)g_aIPv6Address, sizeof(g_aIPv6Address), 0, 1 },  

     // NLME_ATRBT_ID_ROUTE_TABLE
     { NULL, 16+16+1+1, 16, 0 },

     // NLME_ATRBT_ID_DEFAULT_ROUTE_FLAG
     {(uint8*)&g_stNlme.m_ucEnableDefaultRoute, sizeof(g_stNlme.m_ucEnableDefaultRoute), 0, 0 },  
     
    //  NLME_ATRBT_ID_DEFAULT_ROUTE_ID,
     {(uint8*)&g_stNlme.m_aDefaultRouteEntry, sizeof(g_stNlme.m_aDefaultRouteEntry), 0, 0 },  

    //  NLME_ATRBT_ID_CONTRACT_TABLE,
     { NULL, 35, 2+16, 0 },

    //  NLME_ATRBT_ID_ATT_TABLE,
     { NULL, 18, 16, 0 },

    //  NLME_ATRBT_ID_MAX_NSDU_SIZE,
     {(uint8*)&c_unMaxNsduSize, sizeof(c_unMaxNsduSize), 0, 1 },             

    //  NLME_ATRBT_ID_FRAG_REASSEMBLY_TIMEOUT,
     {(uint8*)&g_stNlme.m_ucFragReassemblyTimeout, sizeof(g_stNlme.m_ucFragReassemblyTimeout), 0, 0 },  

    //  NLME_ATRBT_ID_FRAG_DATAGRAM_TAG,
     {(uint8*)&g_stNlme.m_unFragDatagramTag, sizeof(g_stNlme.m_unFragDatagramTag), 0, 1 },  

     //  NLME_ATRBT_ID_ROUTE_TBL_META,
     { NULL,    4, 0, 1 },  

     //  NLME_ATRBT_ID_CONTRACT_TBL_META,
     { NULL,  4, 0, 1 },  

     //  NLME_ATRBT_ID_ATT_TBL_META,
     { NULL, 4, 0, 1 },  

     //  NLME_ATRBT_ID_ALERT_DESC,
     { NULL, 2, 0, 0 }  
};

  
#if (MAX_ROUTES_NO > 0)
  uint8 NLME_setRoute( uint32 p_ulTaiCutover, const uint8 * p_pData );
  uint8 NLME_getRoute( const uint8 * p_pIdxData, uint8 * p_pData );
  uint8 NLME_delRoute( const uint8 * p_pIdxData );
  
  NLME_ROUTE_ATTRIBUTES * NLME_findRoute( const uint8*   p_ucDestAddress );   
#else
  #define NLME_setRoute(...) SFC_INSUFFICIENT_DEVICE_RESOURCES
  #define NLME_getRoute(...) SFC_INVALID_ELEMENT_INDEX
  #define NLME_delRoute(...) SFC_INVALID_ELEMENT_INDEX
#endif  

#if (MAX_ADDR_TRANSLATIONS_NO > 0)      
  uint8 NLME_setATT( uint32 p_ulTaiCutover, const uint8 * p_pData );
  uint8 NLME_getATT( const uint8 * p_pIdxData, uint8 * p_pData  );
  uint8 NLME_delATT( const uint8 * p_pIdxData );
#else
  #define NLME_setATT(...) SFC_INSUFFICIENT_DEVICE_RESOURCES
  #define NLME_getATT(...) SFC_INVALID_ELEMENT_INDEX
  #define NLME_delATT(...) SFC_INVALID_ELEMENT_INDEX
#endif
  
  uint8 NLME_setContract( uint32 p_ulTaiCutover, const uint8 * p_pData );  
  uint8 NLME_getContract( const uint8 * p_pIdxData, uint8 * p_pData  );  
  uint8 NLME_delContract( const uint8 * p_pIdxData );

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Reset NLME attributes
/// @param  none
/// @return none
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
 void NLME_Init( void )
 {
    memset( &g_stNlme, 0, sizeof(g_stNlme) );
             
    g_stNlme.m_ucFragReassemblyTimeout = 60;
 }

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Set a network layer SMIB row or MIB 
/// @param  p_ucAttributeID - attribute ID
/// @param  p_ulTaiCutover - TAI cutover (ignored)
/// @param  p_pData - value if MIB or index+index+value if SMIB
/// @param  p_ucDataLen - p_pData size
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_SetRow( uint8         p_ucAttributeID, 
                   uint32        p_ulTaiCutover, 
                   const uint8 * p_pData,
                   uint8         p_ucDataLen )
{
    if( (--p_ucAttributeID) >= (NLME_ATRBT_ID_NO-1) )
    {
         return SFC_INVALID_ATTRIBUTE;
    }

    if( c_aAttributesParams[p_ucAttributeID].m_ucIsReadOnly )
    {
        return SFC_READ_ONLY_ATTRIBUTE;
    }

    if( p_ucDataLen != (c_aAttributesParams[p_ucAttributeID].m_ucSize + c_aAttributesParams[p_ucAttributeID].m_ucIndexSize) )
    {
        return SFC_INVALID_SIZE;
    }

    switch( p_ucAttributeID + 1 )
    {
    case NLME_ATRBT_ID_ROUTE_TABLE:    return NLME_setRoute( p_ulTaiCutover, p_pData );
    case NLME_ATRBT_ID_CONTRACT_TABLE: return NLME_setContract( p_ulTaiCutover, p_pData );
    case NLME_ATRBT_ID_ATT_TABLE:      return NLME_setATT( p_ulTaiCutover, p_pData );
    case NLME_ATRBT_ID_ALERT_DESC:     
                                       DMAP_WriteCompressAlertDescriptor( &g_stNlme.m_ucAlertDesc, p_pData, p_ucDataLen );
                                       break;
    default:
                memcpy( c_aAttributesParams[p_ucAttributeID].m_pValue, p_pData, p_ucDataLen );
    }

    return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Get a network layer SMIB row or MIB
/// @param  p_ucAttributeID - attribute ID
/// @param  p_pIdxData - index to identify SMIB row (ignored if MIB)
/// @param  p_ucIdxDataLen - size of index
/// @param  p_pData - value if MIB or index+value if SMIB
/// @param  p_pDataLen - p_pData size
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_GetRow( uint8         p_ucAttributeID, 
                   const uint8 * p_pIdxData,
                   uint8         p_ucIdxDataLen,
                   uint8       * p_pData,
                   uint16      * p_pDataLen )
{
    if( (--p_ucAttributeID) >= (NLME_ATRBT_ID_NO-1) )
    {
         return SFC_INVALID_ATTRIBUTE;
    }

    *p_pDataLen = c_aAttributesParams[p_ucAttributeID].m_ucSize;

    if( p_ucIdxDataLen != c_aAttributesParams[p_ucAttributeID].m_ucIndexSize )
    {
        return SFC_INCONSISTENT_CONTENT;
    }

    switch( p_ucAttributeID + 1 )
    {
#if !defined(BACKBONE_SUPPORT) || (DEVICE_TYPE != DEV_TYPE_MC13225)   // is not BBR MC13225
    case NLME_ATRBT_ID_ROUTE_TABLE:    return NLME_getRoute( p_pIdxData, p_pData );
    case NLME_ATRBT_ID_ATT_TABLE:      return NLME_getATT( p_pIdxData, p_pData );
#endif
    
    case NLME_ATRBT_ID_CONTRACT_TABLE: return NLME_getContract( p_pIdxData, p_pData );
    
    case NLME_ATRBT_ID_ROUTE_TBL_META: 
                  p_pData[0] = 0;        
#if( MAX_ROUTES_NO > 0 )                  
                  p_pData[1] = g_stNlme.m_ucRoutesNo;
#else
                  p_pData[1] = 0;
#endif
                  p_pData[2] = 0;
                  p_pData[3] = MAX_ROUTES_NO;
                  break;
                  
    case NLME_ATRBT_ID_CONTRACT_TBL_META: 
                  p_pData[0] = 0;
                  p_pData[1] = g_stNlme.m_ucContractNo;
                  p_pData[2] = 0;
                  p_pData[3] = MAX_CONTRACT_NO;
                  break;
#if (MAX_ADDR_TRANSLATIONS_NO > 0 )                  
    case NLME_ATRBT_ID_ATT_TBL_META: 
                  p_pData[0] = 0;
                  p_pData[1] = g_stNlme.m_ucAddrTransNo;
                  p_pData[2] = 0;
                  p_pData[3] = MAX_ADDR_TRANSLATIONS_NO;
                  break;
#endif      
    case NLME_ATRBT_ID_ALERT_DESC:
      {
                  uint8 ucLen;
                  DMAP_ReadCompressAlertDescriptor( &g_stNlme.m_ucAlertDesc, p_pData, &ucLen ); 
                  *p_pDataLen = ucLen;
                  break;
      }
      
    case NLME_ATRBT_ID_SHORT_ADDR: DMAP_InsertUint16( p_pData, g_unShortAddr); break;  
    
    default:
                  memcpy( p_pData, c_aAttributesParams[p_ucAttributeID].m_pValue, c_aAttributesParams[p_ucAttributeID].m_ucSize);            
    }
    return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Get a network layer non-indexed attribute (it is a wraper to the NLME_GetRow function)
/// @param  p_ucAttributeID - attribute ID
/// @param  p_punLen - pointer; in/out parameter. in - length of available buffer; out - actual size
///                     of read data
/// @param p_pucBuffer  - pointer to a buffer where to put read data
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLMO_Read( uint16 p_unAttrID, uint16 * p_punLen, uint8 * p_pucBuffer)
{
#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)   // is BBR MC13225
  if( NLME_ATRBT_ID_ROUTE_TABLE == p_unAttrID
      || NLME_ATRBT_ID_ATT_TABLE == p_unAttrID
      || NLME_ATRBT_ID_DEFAULT_ROUTE_FLAG == p_unAttrID
      || NLME_ATRBT_ID_DEFAULT_ROUTE_ID == p_unAttrID )
  {
      return SFC_NACK; // parsed by AN 
  }        
#endif
  
  if( NLME_ATRBT_ID_ROUTE_TABLE == p_unAttrID  
      || NLME_ATRBT_ID_CONTRACT_TABLE == p_unAttrID
      || NLME_ATRBT_ID_ATT_TABLE == p_unAttrID )
  {
      return SFC_INVALID_ATTRIBUTE; // only mib requests are accepted
  }
 
  return NLME_GetRow( p_unAttrID, NULL, 0, p_pucBuffer, p_punLen);
}

#if defined(BACKBONE_SUPPORT) && (DEVICE_TYPE == DEV_TYPE_MC13225)   // is BBR MC13225
  uint8 NLMO_Write( uint8 p_ucAttributeID,  uint8 p_ucDataLen, const uint8 * p_pData )
  {
      if( NLME_ATRBT_ID_ROUTE_TABLE == p_ucAttributeID
          || NLME_ATRBT_ID_ATT_TABLE == p_ucAttributeID
          || NLME_ATRBT_ID_DEFAULT_ROUTE_FLAG == p_ucAttributeID
          || NLME_ATRBT_ID_DEFAULT_ROUTE_ID == p_ucAttributeID )
      {
          return SFC_NACK; // parsed by AN 
      }        
      
      return NLME_SetRow( p_ucAttributeID, 0, p_pData, p_ucDataLen );
  }
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Delete a network layer SMIB row
/// @param  p_ucAttributeID - attribute ID
/// @param  p_ulTaiCutover - ignored
/// @param  p_pIdxData - index to identify SMIB row 
/// @param  p_ucIdxDataLen - size of index
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_DeleteRow( uint8         p_ucAttributeID, 
                      uint32        p_ulTaiCutover, 
                      const uint8 * p_pIdxData,
                      uint8         p_ucIdxDataLen )
{
    if( (--p_ucAttributeID) >= (NLME_ATRBT_ID_NO-1) )
    {
         return SFC_INVALID_ATTRIBUTE;
    }

    if( c_aAttributesParams[p_ucAttributeID].m_ucIndexSize ) // SMIB
    {
        if( p_ucIdxDataLen != c_aAttributesParams[p_ucAttributeID].m_ucIndexSize )
        {
            return SFC_INCONSISTENT_CONTENT;
        }

        switch( p_ucAttributeID + 1 )
        {
        case NLME_ATRBT_ID_ROUTE_TABLE:    return NLME_delRoute( p_pIdxData );
        case NLME_ATRBT_ID_CONTRACT_TABLE: return NLME_delContract( p_pIdxData );
        case NLME_ATRBT_ID_ATT_TABLE:      return NLME_delATT( p_pIdxData );
        }
    }

    return SFC_INCOMPATIBLE_MODE;
}
 

#if (MAX_ROUTES_NO > 0) // NL route support (BBR only according with Robert)

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Set a network layer route
    /// @param  p_ulTaiCutover - ignored
    /// @param  p_pData - index of replacement + new route entry
    /// @return SFC code
    /// @remarks
    ///      Access level: user level\n
    ///      Context: BBR only since devices don't use that table
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 NLME_setRoute( uint32 p_ulTaiCutover, const uint8 * p_pData )
    {  
        NLME_ROUTE_ATTRIBUTES * pRoute = NLME_findRoute(p_pData);        
        if( !pRoute ) // not found, add it
        {
            if( g_stNlme.m_ucRoutesNo >= MAX_ROUTES_NO )
            {
                return SFC_INSUFFICIENT_DEVICE_RESOURCES;
            }
            pRoute = g_stNlme.m_aRoutesTable+g_stNlme.m_ucRoutesNo;
            g_stNlme.m_ucRoutesNo++;
        }
        
    // that because ISA100 decides to send idx twice
        p_pData += c_aAttributesParams[NLME_ATRBT_ID_ROUTE_TABLE - 1].m_ucIndexSize;
    
    // copy as it is
        memcpy( (uint8*)pRoute, p_pData, c_aAttributesParams[NLME_ATRBT_ID_ROUTE_TABLE - 1].m_ucSize ); 
        
        return SFC_SUCCESS;
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Get a network layer route
    /// @param  p_pIdxData - route index
    /// @param  p_pData - route entry
    /// @return SFC code
    /// @remarks
    ///      Access level: user level\n
    ///      Context: BBR only since devices don't use that table
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 NLME_getRoute( const uint8 * p_pIdxData, uint8 * p_pData )
    {
        NLME_ROUTE_ATTRIBUTES * pRoute = NLME_findRoute(p_pIdxData);
        if( !pRoute )
        {
            return SFC_INVALID_ELEMENT_INDEX;
        }
    
        // copy as it is
        memcpy( p_pData, (uint8*)pRoute, c_aAttributesParams[NLME_ATRBT_ID_ROUTE_TABLE - 1].m_ucSize ); 
        return SFC_SUCCESS;
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Delete a network layer route
    /// @param  p_pIdxData - route index
    /// @return SFC code
    /// @remarks
    ///      Access level: user level\n
    ///      Context: BBR only since devices don't use that table
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 NLME_delRoute( const uint8 * p_pIdxData )
    {
        NLME_ROUTE_ATTRIBUTES * pRoute = NLME_findRoute(p_pIdxData);
        if( !pRoute )
        {
            return SFC_INVALID_ELEMENT_INDEX;
        }
    
        g_stNlme.m_ucRoutesNo --;
        memmove( pRoute, pRoute+1, (uint8*)(g_stNlme.m_aRoutesTable+g_stNlme.m_ucRoutesNo)-(uint8*)pRoute );
    
        return SFC_SUCCESS;
    }
 
     ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Find a route on table
    /// @param  p_ucDestAddress - IPv6 dest address (index on route table) 
    /// @return pointer to route, null if not found
    /// @remarks
    ///      Access level: user level or interrupt level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    NLME_ROUTE_ATTRIBUTES * NLME_findRoute( const uint8*   p_ucDestAddress ) 
    {
      NLME_ROUTE_ATTRIBUTES * pRoute = g_stNlme.m_aRoutesTable;
      NLME_ROUTE_ATTRIBUTES * pEndRoute = g_stNlme.m_aRoutesTable+g_stNlme.m_ucRoutesNo;
          
      for( ; pRoute < pEndRoute; pRoute++ )
      {
         if ( ! memcmp( pRoute->m_aDestAddress, p_ucDestAddress, sizeof(pRoute->m_aDestAddress) ) )
                return pRoute;
      }
      return NULL;
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Find the nwk route, otherwise use default
    /// @param  p_ucDestAddress - IPv6 dest address (index on route table) 
    /// @return pointer to route, null if not found
    /// @remarks
    ///      Access level: user level or interrupt level\n
    ///      Obs: if returned index = g_ucRoutesNo means no route found
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    NLME_ROUTE_ATTRIBUTES * NLME_FindDestinationRoute( uint8*   p_ucDestAddress )
    {
      NLME_ROUTE_ATTRIBUTES * pRoute = NLME_findRoute( p_ucDestAddress );
    
      if( !pRoute && g_stNlme.m_ucEnableDefaultRoute )
      {
            return NLME_findRoute( g_stNlme.m_aDefaultRouteEntry );
      }
    
      return pRoute;
    }
    

#endif // (MAX_ROUTES_NO > 0)

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Set a network layer contract
/// @param  p_ulTaiCutover - ignored
/// @param  p_pData - index of replacement + new contract entry
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_setContract( uint32 p_ulTaiCutover, const uint8 * p_pData )
{
    NLME_CONTRACT_ATTRIBUTES * pContract = NULL;

    if( memcmp( p_pData+2+16+2, g_aIPv6Address, 16 ) ) // Accept my TX contract only
    {
        return SFC_INVALID_ELEMENT_INDEX;
    }
    
    if( !memcmp( p_pData+2, g_aIPv6Address, 16 ) ) // my contract
    {
        pContract = NLME_FindContract( (((uint16)(p_pData[0])) << 8) | p_pData[1]);    
    }
    if( !pContract ) // not found, add it
    {
        if( g_stNlme.m_ucContractNo >= MAX_CONTRACT_NO )
        {
            return SFC_INSUFFICIENT_DEVICE_RESOURCES;
        }
        pContract = g_stNlme.m_aContractTable+g_stNlme.m_ucContractNo;
        g_stNlme.m_ucContractNo++;
    }
            
    // that because ISA100 decides to send idx twice
    p_pData += c_aAttributesParams[NLME_ATRBT_ID_CONTRACT_TABLE - 1].m_ucIndexSize;    
    
    pContract->m_unContractID = (((uint16)(p_pData[0])) << 8) | p_pData[1];
    memcpy( pContract->m_aDestAddress, p_pData+2+16, c_aAttributesParams[NLME_ATRBT_ID_CONTRACT_TABLE - 1].m_ucSize-2-16 ); 
    
    return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Get a network layer contract
/// @param  p_pIdxData - contract index
/// @param  p_pData - contract entry
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_getContract( const uint8 * p_pIdxData, uint8 * p_pData )
{
    NLME_CONTRACT_ATTRIBUTES * pContract;

    pContract = NLME_FindContract( (((uint16)(p_pIdxData[0])) << 8) | p_pIdxData[1]);    
    if( !pContract )
    {
        return SFC_INVALID_ELEMENT_INDEX;
    }

    *(p_pData++) = pContract->m_unContractID >> 8;
    *(p_pData++) = pContract->m_unContractID & 0xFF;
    
    memcpy( p_pData, g_aIPv6Address, 16 ); // Only my contracts     
    memcpy( p_pData+16, pContract->m_aDestAddress, c_aAttributesParams[NLME_ATRBT_ID_CONTRACT_TABLE - 1].m_ucSize-2-16 ); 
    
    return SFC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Delete a network layer contract
/// @param  p_pIdxData - contract index
/// @return SFC code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_delContract( const uint8 * p_pIdxData )
{
    NLME_CONTRACT_ATTRIBUTES * pContract;

    pContract = NLME_FindContract( (((uint16)(p_pIdxData[0])) << 8) | p_pIdxData[1]);
    if( !pContract )
    {
        return SFC_INVALID_ELEMENT_INDEX;
    }

    g_stNlme.m_ucContractNo --;
    memmove( pContract, pContract+1, (uint8*)(g_stNlme.m_aContractTable+g_stNlme.m_ucContractNo)-(uint8*)pContract );

    return SFC_SUCCESS;
}

#if ( MAX_ADDR_TRANSLATIONS_NO > 0 )
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Set a network layer ATT entry
    /// @param  p_ulTaiCutover - ignored
    /// @param  p_pData - index of replacement + new ATT entry
    /// @return SFC code
    /// @remarks
    ///      Access level: user level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 NLME_setATT( uint32 p_ulTaiCutover, const uint8 * p_pData )
    {
        NLME_ADDR_TRANS_ATTRIBUTES * pAtt = NLME_FindATT( p_pData );
        if( !pAtt )
        {
            if( g_stNlme.m_ucAddrTransNo >= MAX_ADDR_TRANSLATIONS_NO )
            {
                return SFC_INSUFFICIENT_DEVICE_RESOURCES;
            }
            pAtt = g_stNlme.m_aAddrTransTable+g_stNlme.m_ucAddrTransNo;
            g_stNlme.m_ucAddrTransNo++;
        }
    
    // that because ISA100 decides to send idx twice
        p_pData += c_aAttributesParams[NLME_ATRBT_ID_ATT_TABLE - 1].m_ucIndexSize;
        
    // copy as it is
        memcpy( (uint8*)pAtt, p_pData, c_aAttributesParams[NLME_ATRBT_ID_ATT_TABLE - 1].m_ucSize ); 
        
        return SFC_SUCCESS;
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Get a network layer ATT entry
    /// @param  p_pIdxData - ATT index
    /// @param  p_pData - ATT entry
    /// @return SFC code
    /// @remarks
    ///      Access level: user level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 NLME_getATT( const uint8 * p_pIdxData, uint8 * p_pData )
    {
        NLME_ADDR_TRANS_ATTRIBUTES * pAtt = NLME_FindATT( p_pIdxData );
        if( !pAtt )
        {
            return SFC_INVALID_ELEMENT_INDEX;
        }
    
    // copy as it is
        memcpy( p_pData, (uint8*)pAtt, c_aAttributesParams[NLME_ATRBT_ID_ATT_TABLE - 1].m_ucSize ); 
    
        return SFC_SUCCESS;
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @author NIVIS LLC, Ion Ticus
    /// @brief  Delete a network layer ATT entry
    /// @param  p_pIdxData - ATT index
    /// @return SFC code
    /// @remarks
    ///      Access level: user level
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    uint8 NLME_delATT( const uint8 * p_pIdxData )
    {
        NLME_ADDR_TRANS_ATTRIBUTES * pAtt = NLME_FindATT( p_pIdxData );
        if( !pAtt )
        {
            return SFC_INVALID_ELEMENT_INDEX;
        }
    
    
        g_stNlme.m_ucAddrTransNo --;
        memmove( pAtt, pAtt+1, (uint8*)(g_stNlme.m_aAddrTransTable+g_stNlme.m_ucAddrTransNo)-(uint8*)pAtt );
    
        return SFC_SUCCESS;
    }
#endif    
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Adrian Simionescu
/// @brief  Find a contract on table
/// @param  p_unContractId - the contract id
/// @return contract index
/// @remarks
///      Access level: user level\n
///      Obs: if returned index = g_ucContractNo means no contract found
////////////////////////////////////////////////////////////////////////////////////////////////////
NLME_CONTRACT_ATTRIBUTES * NLME_FindContract (  uint16 p_unContractID )
{
  NLME_CONTRACT_ATTRIBUTES * pContract = g_stNlme.m_aContractTable;
  NLME_CONTRACT_ATTRIBUTES * pEndContract = g_stNlme.m_aContractTable+g_stNlme.m_ucContractNo;
  
  for( ; pContract < pEndContract; pContract++ )
  {
      if( pContract->m_unContractID == p_unContractID )
            return pContract;
  }
  return NULL;
}  

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Ion Ticus
/// @brief  Add a DMO contract on NL contract table 
//          (keep consistency until ISA100 will decide to unify those 2 tables - not on ISA spec)
/// @param  p_pDmoContract - the DMO contract
/// @return SFC code
/// @remarks
///      Access level: user level\n
///      Obs: For a success result SM need to load DMO contract after NL contract 
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLME_AddDmoContract( const DMO_CONTRACT_ATTRIBUTE * p_pDmoContract )
{
    NLME_CONTRACT_ATTRIBUTES * pContract = NLME_FindContract( p_pDmoContract->m_unContractID );
    if( !pContract ) // contract not found
    {
        if( g_stNlme.m_ucContractNo >= MAX_CONTRACT_NO )
        {
            return SFC_INSUFFICIENT_DEVICE_RESOURCES;
        }
     
        pContract = g_stNlme.m_aContractTable + g_stNlme.m_ucContractNo++;
        
        pContract->m_unContractID = p_pDmoContract->m_unContractID;
        memcpy( pContract->m_aDestAddress, p_pDmoContract->m_aDstAddr128, sizeof(pContract->m_aDestAddress) );
        pContract->m_bPriority = p_pDmoContract->m_ucPriority;
        pContract->m_bIncludeContractFlag = 0; // missing info
    }
    return SFC_SUCCESS;
}

#if ( MAX_ADDR_TRANSLATIONS_NO > 0 )
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Find ATT entry using 16 bit short address (unique on subnet)
  /// @return ATT entry if success, NULL if not found 
  /// @remarks
  ///      Access level: Interrupt level or user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  NLME_ADDR_TRANS_ATTRIBUTES * NLME_FindATTByShortAddr ( const uint8 * p_pShortAddr )
  {
     NLME_ADDR_TRANS_ATTRIBUTES * pAtt = g_stNlme.m_aAddrTransTable;
     NLME_ADDR_TRANS_ATTRIBUTES * pEndAtt = g_stNlme.m_aAddrTransTable+g_stNlme.m_ucAddrTransNo;
     for(; pAtt < pEndAtt; pAtt++)  // look in Address Translation Table
     {
        if( pAtt->m_aShortAddress[1] == p_pShortAddr[1] && pAtt->m_aShortAddress[0] == p_pShortAddr[0] )
          return pAtt;      
     }
  
     return NULL;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Find ATT entry using IPV6 address (ATT index)
  /// @return ATT entry if success, NULL if not found 
  /// @remarks
  ///      Access level: Interrupt level or user level
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  NLME_ADDR_TRANS_ATTRIBUTES * NLME_FindATT ( const uint8 * p_pIpv6Addr )
  {
     NLME_ADDR_TRANS_ATTRIBUTES * pAtt = g_stNlme.m_aAddrTransTable;
     NLME_ADDR_TRANS_ATTRIBUTES * pEndAtt = g_stNlme.m_aAddrTransTable+g_stNlme.m_ucAddrTransNo;
     for(; pAtt < pEndAtt; pAtt++)  // look in Address Translation Table
     {
        if( ! memcmp( pAtt->m_aIPV6Addr, p_pIpv6Addr, sizeof(pAtt->m_aIPV6Addr)))
          return pAtt;      
     }
  
     return NULL;
  }
#endif
#if defined (BACKBONE_SUPPORT) 
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Ion Ticus
  /// @brief  Check if message destination is in current subnet 
  /// @return 1 if message remains in subnet, 0 if not 
  /// @remarks
  ///      Access level: Interrupt level\n
  ///      Context: BBR only
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8 NLME_IsInSameSubnet( uint16 p_unDestAddr )
  {
        p_unDestAddr = __swap_bytes(p_unDestAddr);
        
        for ( uint8  ucIdx = 0; ucIdx < g_stNlme.m_ucToEthRoutesNo; ucIdx ++ )
        {
            if( g_stNlme.m_aToEthRoutes[ucIdx] == p_unDestAddr )
                return !g_stNlme.m_ucDefaultIsDLL;
        }
        return g_stNlme.m_ucDefaultIsDLL;
  }

#endif

void NLME_Alert( uint8 p_ucAlertValue, const uint8 * p_pNpdu, uint8 p_ucNpduLen )
{
    if( g_stNlme.m_ucAlertDesc & 0x80 )
    {
        ALERT stAlert;
        
        uint8 ucAlertData[40];
        
        if( p_ucNpduLen >= sizeof(ucAlertData) - 2 )
        {
            p_ucNpduLen = sizeof(ucAlertData) - 2;
        }
        
        stAlert.m_ucPriority = g_stNlme.m_ucAlertDesc & 0x7F;
        stAlert.m_unDetObjTLPort = 0xF0B0; // TLME is DMAP port
        stAlert.m_unDetObjID = DMAP_NLMO_OBJ_ID; 
        stAlert.m_ucClass = ALERT_CLASS_EVENT; 
        stAlert.m_ucDirection = ALARM_DIR_IN_ALARM; 
        stAlert.m_ucCategory = ALERT_CAT_COMM_DIAG; 
        stAlert.m_ucType = 0; // NL_Dropped_PDU                     
        stAlert.m_unSize = p_ucNpduLen+2; 
        
        ucAlertData[0] = p_ucNpduLen+2;
        ucAlertData[1] = p_ucAlertValue;
        memcpy( ucAlertData+2, p_pNpdu, p_ucNpduLen );
        
        ARMO_AddAlertToQueue( &stAlert, ucAlertData );
        
    }
}

//////////////////////////////////////NLMO Object///////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Eduard Erdei
/// @brief  Executes network layer management object methods
/// @param  p_unMethID - method identifier
/// @param  p_unReqSize - request buffer size
/// @param  p_pReqBuf - request buffer containing method parameters
/// @param  p_pRspSize - response buffer size
/// @param  p_pRspBuf - response buffer
/// @return service feedback code
/// @remarks
///      Access level: user level
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8 NLMO_Execute(uint8    p_ucMethID,
                   uint16   p_unReqSize,
                   const uint8 *  p_pReqBuf,
                   uint16 * p_pRspSize,
                   uint8 *  p_pRspBuf)
{
  uint8 ucSFC;
  // " Change: ASL works with uint16 length. DMAP works with uint8 length"
  
  if ( p_unReqSize < 2 )
      return SFC_INVALID_SIZE;
  
  uint16 unAttrID = ((uint16)p_pReqBuf[0] << 8) | p_pReqBuf[1];
  p_pReqBuf += 2;
  p_unReqSize -= 2;
  
  uint32 ulSchedTAI;
  if( (NLMO_GET_ROW_RT != p_ucMethID) 
      && (NLMO_GET_ROW_CONTRACT_TBL != p_ucMethID) 
      && (NLMO_GET_ROW_ATT != p_ucMethID) )
  {
      if ( p_unReqSize < 4 )
          return SFC_INVALID_SIZE;
      
      p_pReqBuf =  DMAP_ExtractUint32( p_pReqBuf, &ulSchedTAI );
      p_unReqSize -= 4;
  } 
    
#if (MAX_ADDR_TRANSLATIONS_NO == 0)   // no ATT table support
  if( (p_ucMethID == NLMO_GET_ROW_RT 
          || p_ucMethID == NLMO_GET_ROW_ATT 
          || p_ucMethID == NLMO_SET_ROW_RT 
          || p_ucMethID == NLMO_SET_ROW_ATT 
          || p_ucMethID == NLMO_DEL_ROW_RT 
          || p_ucMethID == NLMO_DEL_ROW_ATT )
     && (unAttrID == NLME_ATRBT_ID_ROUTE_TABLE || unAttrID == NLME_ATRBT_ID_ATT_TABLE) )
  {
      return SFC_NACK; // parsed by AN 
  }        
#endif

  
  switch(p_ucMethID)
  {        
  case NLMO_GET_ROW_RT:
  case NLMO_GET_ROW_ATT:    
  case NLMO_GET_ROW_CONTRACT_TBL:
        ucSFC = NLME_GetRow( unAttrID,
                             p_pReqBuf,
                             p_unReqSize,    
                             p_pRspBuf,  
                             p_pRspSize);                 
      break;
      
  case NLMO_SET_ROW_RT:
  case NLMO_SET_ROW_ATT:    
  case NLMO_SET_ROW_CONTRACT_TBL:
      ucSFC = NLME_SetRow( unAttrID,
                           ulSchedTAI,
                           p_pReqBuf,
                           p_unReqSize);     
      break;     
  
  case NLMO_DEL_ROW_RT:  
  case NLMO_DEL_ROW_ATT:      
  case NLMO_DEL_ROW_CONTRACT_TBL:  
      ucSFC = NLME_DeleteRow( unAttrID,
                              ulSchedTAI,
                              p_pReqBuf,
                              p_unReqSize);       
      break;

  default: 
      ucSFC = SFC_INVALID_METHOD;
      break;
  }
  
  return ucSFC;
}
