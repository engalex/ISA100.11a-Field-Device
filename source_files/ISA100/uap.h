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
/// Date:         May 2008
/// Description:  This file holds common definitions of different demo application processes
/// Changes:      Rev. 1.0 - created
/// Revisions:    Rev. 1.0 - by author
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _NIVIS_UAP_H_
#define _NIVIS_UAP_H_

#include "dmap_dmo.h"

enum{
    UAP_MO_OBJ_TYPE         = 1,
    UAP_UDO_OBY_TYPE        = 3,
    UAP_CO_OBJ_TYPE         = 4,
    UAP_DISP_OBJ_TYPE       = 5,
    ANALOG_INPUT_OBJ_TYPE   = 99,
    ANALOG_OUTPUT_OBJ_TYPE  = 98,
    BINARY_INPUT_OBJ_TYPE   = 97,
    BINARY_OUTPUT_OBJ_TYPE  = 96,
    UAP_DATA_OBJ_TYPE       = 129,
    UAP_PO_OBJ_TYPE         = 130
};

enum
{
  UAP_MO_OBJ_ID = 1,    //Application Process Management Object - specified by standard with object identifier = 1 for all UAPs 
  UAP_UDO_OBJ_ID = 2,   //specified by the standard(Table 515 - Standard object instances)
  UAP_CO_OBJ_ID = 4,    //not specified by standard - Concentrator Object - needed for publishing data 
  UAP_DISP_OBJ_ID = 5,  //not specified by standard - Dispersion Object - needed for local loop
  UAP_PO_OBJ_ID   = 6,  //not specified by standard - Provisioning object - needed for provisionin the publish attributes and endpoints 
  UAP_DATA_OBJ_ID = 129,//not specified by standard
  UAP_OBJ_NO = 4                
}; // UAP_OBJECT_IDS

#define UAP_DataConfirm(...)

#if ( _UAP_TYPE == 0 )

      #define UAP_NotifyAddContract(...)
      #define UAP_NotifyContractDeletion(...)
      #define UAP_OnJoin()
      #define UAP_OnJoinReset()
      #define UAP_MainTask()
      #define UAP_NeedPowerOn() 0

#else

      #define UAP_NeedPowerOn() DAQ_NeedPowerOn()
    
      void UAP_NotifyContractResponse( const uint8 * p_pContractRsp, uint8 p_ucContractRspLen );
      void UAP_NotifyContractDeletion(uint16 p_unContractID);
      void UAP_NotifyAlertAcknowledge(uint8 p_ucAlertID);
      void UAP_OnJoin(void);
      void UAP_OnJoinReset(void);
      void UAP_MainTask(void);

#endif // _UAP_TYPE == 0

#endif // _NIVIS_UAP_H_
