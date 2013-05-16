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
#ifndef _NIVIS_PROVISION_H_
#define _NIVIS_PROVISION_H_

#include "config.h"
#include "dlme.h"
#include "../typedef.h"

#define JOIN_BACKOFF_BASE     5   // 500 msec

#define VENDOR_ID_SIZE  5
#define MODEL_ID_SIZE   9
#define TAG_NAME_SIZE   4
#define PWR_SUPPLY_SIZE 3
#define MEM_INFO_SIZE   6

#define DEFAULT_TS_TEMPLATE_NO  3
#define DEFAULT_CH_NO           5


#if( DEVICE_TYPE == DEV_TYPE_MC13225 )
  #include "../spif_interface.h"    

  typedef uint32 PROV_ADDR_TYPE;

  #define SECTOR_FLAG  1
  #define SECTOR_SIZE (4*1024)

  // persistent data sectors (sector 31 is reserved for factory, 0 for bootloader)
  #define MANUFACTURING_SECTOR_IDX        30 
  #define PROVISION_SECTOR_IDX            29
  #define FIRST_APPLICATION_SECTOR_IDX    28 
  #define SECOND_APPLICATION_SECTOR_IDX   27 

  #define MANUFACTURING_SECTOR_NO       (1UL << MANUFACTURING_SECTOR_IDX)  
  #define PROVISION_SECTOR_NO           (1UL << PROVISION_SECTOR_IDX)  
  #define FIRST_APPLICATION_SECTOR_NO   (1UL << FIRST_APPLICATION_SECTOR_IDX)  
  #define SECOND_APPLICATION_SECTOR_NO  (1UL << SECOND_APPLICATION_SECTOR_IDX)  

  #define MANUFACTURING_START_ADDR      (SECTOR_SIZE * MANUFACTURING_SECTOR_IDX)
  #define PROVISION_START_ADDR          (SECTOR_SIZE * PROVISION_SECTOR_IDX)
  #define FIRST_APPLICATION_START_ADDR  (SECTOR_SIZE * FIRST_APPLICATION_SECTOR_IDX)
  #define SECOND_APPLICATION_START_ADDR (SECTOR_SIZE * SECOND_APPLICATION_SECTOR_IDX)

#else
    #warning  "HW platform not supported" 
  
#endif

#include "dmap_dpo.h"

extern EUI64_ADDR   c_oEUI64BE; // nwk order EUI64ADDR
extern EUI64_ADDR   c_oEUI64LE; // little endian order 

#define   c_oSecManagerEUI64BE g_stDPO.m_aTargetSecurityMngrEUI
  
extern const DLL_SMIB_ENTRY_TIMESLOT    c_aDefTemplates[];
extern const DLL_SMIB_ENTRY_CHANNEL     c_aDefChannels[];
extern const DLL_MIB_DEV_CAPABILITIES   c_stCapability;
extern const DLL_MIB_ENERGY_DESIGN      c_stEnergyDesign; 
  
 
#ifdef BACKBONE_SUPPORT
  #define DLL_DL_ROLE   0x04
#elif ROUTING_SUPPORT
  #define DLL_DL_ROLE   0x02
#else
  #define DLL_DL_ROLE   0x01
#endif  

  void PROVISION_Init(void);
  
uint8 PROVISION_AddCmdToProvQueue( uint16  p_unObjId,
                                   uint8   p_ucAttrId,
                                   uint8   p_ucSize,
                                   uint8*  p_pucPayload );
void PROVISION_ValidateProvQueue( void );
void PROVISION_ExecuteProvCmdQueue( void );   

extern uint8 g_ucPAValue;

typedef enum
{
  PROV_STATE_INIT,
  PROV_STATE_ON_PROGRESS,
  PROV_STATE_PROVISIONED
} PROV_STATES;

#if( DEVICE_TYPE == DEV_TYPE_MC13225 ) && defined (BACKBONE_SUPPORT)
    extern uint8 g_ucProvisioned;
#else
    #define g_ucProvisioned PROV_STATE_PROVISIONED
#endif
        
void ReadPersistentData( uint8 *p_pucDst, PROV_ADDR_TYPE p_uAddr, uint16 p_unSize );
void WritePersistentData( const uint8 *p_pucSrc, PROV_ADDR_TYPE p_uAddr, uint16 p_unSize );

#if( DEVICE_TYPE == DEV_TYPE_MC13225 )
  void EraseSector( uint32 p_ulSectorNmb );
#else // EEPROM -> not need to erase the sector first
  #define EraseSector( ...)
#endif  
  
#endif // _NIVIS_PROVISION_H_
