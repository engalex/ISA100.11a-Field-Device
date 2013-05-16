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
/// Date:         February 2008
/// Description:  This file holds the provisioning data
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "provision.h"
#include "../isa100/dmap_dmo.h"
#include "../isa100/dlmo_utils.h"
#include "../isa100/dmap.h"
#include "../isa100/mlde.h"
#include "../isa100/mlsm.h"


  EUI64_ADDR c_oEUI64BE; // nwk order EUI64ADDR
  EUI64_ADDR c_oEUI64LE; // little endian order
  
#if( DEVICE_TYPE == DEV_TYPE_MC13225 ) && defined (BACKBONE_SUPPORT)
    uint8 g_ucProvisioned = PROV_STATE_INIT;
#endif
  
uint8 g_ucPAValue;

const DLL_MIB_DEV_CAPABILITIES c_stCapability = 
{
  DLL_MSG_QUEUE_SIZE_MAX, // m_unQueueCapacity
  DLL_CHANNEL_MAP,        // m_unChannelMap
  DLL_ACK_TURNAROUND,     // m_unAckTurnaround
  (uint16)(DIAG_STRUCT_STANDARD_SIZE * DLL_MAX_NEIGH_DIAG_NO),  // m_unNeighDiagCapacity
  DLL_CLOCK_ACCURACY,     // m_ucClockAccuracy;
  DLL_DL_ROLE,            // m_ucDLRoles;
  (BIT0 | BIT1)           // m_ucOptions;   //group codes and graph extensions are supported 
};

const DLL_MIB_ENERGY_DESIGN c_stEnergyDesign = 
{
#ifdef BACKBONE_SUPPORT
  
  0x7FFF, //  days capacity, 0x7FFF stands for permanent power supply
    3600, //  3600 seconds per hour Rx listen capacity
     600, //  600 seconds per hour Rx listen capacity
     120  //  120 Advertises/minute rate 
  
#else 
    #ifdef ROUTING_SUPPORT
      180,    // 180 days battery capacity
      360,    // 360 seconds per hour Rx listen capacity
      100,    // 100 DPDUs/minute transmit rate
       60     //  60 Advertises/minute rate          
    #else
      180,    // 180 days battery capacity
      360,    // 360 seconds per hour Rx listen capacity
      100,    // 100 DPDUs/minute transmit rate
       60     //  60 Advertises/minute rate     
  #endif // ROUTING_SUPPORT 
#endif // BACKBONE_SUPPORT
};

const DLL_SMIB_ENTRY_TIMESLOT c_aDefTemplates[DEFAULT_TS_TEMPLATE_NO] =
{
  {
    { DEFAULT_RX_TEMPL_UID, // m_unUID
      DLL_MASK_TSBOUND, // m_ucType - Rx + ACK relative to the end of incoming DPDU + respect slot boundaries
      {  
        //1us = 1.048576 * 2^-20
        1271, 3578, 977, 1187, 0, 0, 0, 0  //timings inside Rx timeslot(2^-20 sec)
      } // m_utTemplate
    }  
  },
  {
    { DEFAULT_TX_TEMPL_UID, // m_unUID
      DLL_TX_TIMESLOT | (1 << DLL_ROT_TSCCA ),  // m_ucType - Tx + ACK relative to the end of incoming DPDU +  
                                                //            CCA when energy above threshold + not keep listening
      {  
        //1us = 1.048576 * 2^-20
        2319, 2529, 977, 1187, 0, 0, 0, 0       //timings inside Tx timeslot(2^-20 sec)
      } // m_utTemplate
    }  
  },
  { //default receive template for scanning
    { DEFAULT_DISCOVERY_TEMPL_UID, // m_unUID
      0, // m_ucType - Rx + ACK relative to the end of incoming DPDU + not respect slot boundaries
      {  
        //1us = 1.048576 * 2^-20
        0, 0xffff, 977, 1187, 0, 0, 0, 0       //timings inside Rx timeslot(2^-20 sec)
      } // m_utTemplate
    }  
  },
};


const DLL_SMIB_ENTRY_CHANNEL c_aDefChannels[DEFAULT_CH_NO] =
{
  {
    { 0x01,
      16, //range 1 to 16 (not 0 to 15!!!)
      0x18, 0xD9, 0xC5, 0xE7, 0xA3, 0x40, 0x6B, 0xF2 //seq0 ... seq8
    }
  },
  {
    { 0x02,
      16, //range 1 to 16 (not 0 to 15!!!)
      0x2F, 0xB6, 0x04, 0x3A, 0x7E, 0x5C, 0x9D, 0x81 //seq0 ... seq8
    }
  },
  {
    { 0x03,
      3, //range 1 to 16 (not 0 to 15!!!)
      0x94, 0x0E, 0, 0, 0, 0, 0, 0 //seq0 ... seq8
    }
  },
  {
    { 0x04,
      3, //range 1 to 16 (not 0 to 15!!!)
      0x9E, 0x04, 0, 0, 0, 0, 0, 0 //seq0 ... seq8
    }
  },
  {
    { 0x05,
      16, //range 1 to 16 (not 0 to 15!!!)
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE //seq0 ... seq8
    }
  }

};

#define CODE_CAN 124
#define CODE_JPN 392
#define CODE_MEX 484
#define CODE_USA 840
    
void PROVISION_Init(void)
{        
    uint8 aProvBuffer[2+8+2+16];
    ReadPersistentData( aProvBuffer, MANUFACTURING_START_ADDR , sizeof(aProvBuffer) ); 

#if  (DEVICE_TYPE == DEV_TYPE_MC13225)
    const uint8 * pProv = aProvBuffer +2+8+2;
    
    g_ucPAValue = *(pProv++);
    
    if( g_ucPAValue > 0x1F )
    {
        g_ucPAValue = 0x0C;
    }
    
    uint8 ucProvCTune = *(pProv++);
      
    if( *(uint16*) aProvBuffer != 0xFFFF && (*(uint16*) aProvBuffer & 0xFEFE) ) // manufacturing version
    {
        CRM_XTAL_CNTL.fTune = *pProv; 
    }
    else
    {
        ucProvCTune = (0x18 + ucProvCTune) & 0x001F; // m_ucCristal
        CRM_XTAL_CNTL.fTune = 0x0F;
    }
    
    DelayLoop(10*8000); // 10 ms
    CRM_XTAL_CNTL.bulktune = (ucProvCTune >> 4) & 0x01; // set bulk tune first (4 uF Capacitor)
    
    DelayLoop(10*8000); // 10 ms
    CRM_XTAL_CNTL.cTune = ucProvCTune & 0x0F;
#endif

   // manufacturing data - BBR will receive manufacturing on different way
#if  !defined( BACKBONE_SUPPORT )     
    memcpy( c_oEUI64BE, aProvBuffer + 2, sizeof(c_oEUI64BE) );
    DLME_CopyReversedEUI64Addr( c_oEUI64LE, c_oEUI64BE );          
#endif              
}
  
////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Dorin Pavel   
/// @brief  Generic function for read non-volatile data 
/// @params p_pucDst - destination buffer
///         p_uAddr  - (sector) address 
///         p_unSize - number of bytes to read
/// @return none
/// @remarks
///      Access level: user level
//////////////////////////////////////////////////////////////////////////////////////////////////// 
void ReadPersistentData( uint8 *p_pucDst, PROV_ADDR_TYPE p_uAddr, uint16 p_unSize )
{ 
#if( DEVICE_TYPE == DEV_TYPE_MC13225 )
   // read persistent data from flash
    NVM_FlashRead(p_pucDst, p_uAddr, p_unSize);  
#else
    #warning  "HW platform not supported"
#endif  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Dorin Pavel   
/// @brief  Generic function for write non-volatile data  
/// @params p_pucSrc            - source buffer
///         p_uAddr - (sector) address
///         p_unSize            - number of bytes to write
/// @return none
/// @remarks
///      Access level: user level
//////////////////////////////////////////////////////////////////////////////////////////////////// 
void WritePersistentData( const uint8 *p_pucSrc, PROV_ADDR_TYPE p_uAddr, uint16 p_unSize )
{ 
#if( DEVICE_TYPE == DEV_TYPE_MC13225 )
   // write data into flash 
     NVM_FlashWrite( (void*)p_pucSrc, p_uAddr, p_unSize);
#else
    #warning  "HW platform not supported" 
#endif  
}
  
#if( DEVICE_TYPE == DEV_TYPE_MC13225 )
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @author NIVIS LLC, Dorin Pavel   
  /// @brief  Generic function for clear non-volatile data  
  /// @params p_ulSectorNmb - sector number 
  /// @return none
  /// @remarks
  ///      Access level: user level
  //////////////////////////////////////////////////////////////////////////////////////////////////// 
  void EraseSector( uint32 p_ulSectorNmb )
  {  
     NVM_FlashErase(p_ulSectorNmb);
  }
#endif  
