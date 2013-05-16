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
/// Date:         October 2008
/// Description:  This file holds the service feedback codes 
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_SFC_H_
#define _NIVIS_SFC_H_

enum {
  SFC_SUCCESS = 0,                      //success
  SFC_FAILURE = 1,                      //generic failure
  SFC_OTHER = 2,                        //reason other than listed in this enum
  SFC_INVALID_ARGUMENT = 3,             //invalid attribute to a service call
  SFC_INVALID_OBJ_ID = 4,               //invalid object ID
  SFC_INVALID_SERVICE = 5,              //unsupported or illegal service
  SFC_INVALID_ATTRIBUTE = 6,            //invalid attribute index
  SFC_INVALID_ELEMENT_INDEX = 7,        //invalid array or structure element index (or indices)
  SFC_READ_ONLY_ATTRIBUTE = 8,          //read-only attribute
  SFC_VALUE_OUT_OF_RANGE = 9,           //value is out of permitted range  
  SFC_INNAPROPRIATE_PROCESS_MODE = 10,  //process is in an inappropiate mode for the request
  SFC_INCOMPATIBLE_MODE = 11,            //value is not acceptable in current context
  SFC_INVALID_VALUE = 12,               //value not acceptable for other reason (too large/too smal/invalid eng. units code)
  SFC_INTERNAL_ERROR = 13,              //device internal problem
  SFC_INVALID_SIZE = 14,                //size not valid (may be too big/too small)
  SFC_INCOMPATIBLE_ATTRIBUTE = 15,      //attribute not supported in this version
  SFC_INVALID_METHOD = 16,              //invalid method identifier
  SFC_OBJECT_STATE_CONFLICT = 17,       //state of object in conflict with action requested
  SFC_INCONSISTENT_CONTENT = 18,        //the content of the service requested is inconsistent
  SFC_INVALID_PARAMETER = 19,           //value  conveyed is not legal for method invocation
  SFC_OBJECT_ACCESS_DENIED = 20,        //object is not permitting access
  SFC_TYPE_MISSMATCH = 21,              //data not as expected (too many/too few octets)
  SFC_DEVICE_HARDWARE_CONDITION = 22,   //e.g. memory defect problem
  SFC_DEVICE_SENSOR_CONDITION = 23,     //problem with sensor detected
  SFC_DEVICE_SOFTWARE_CONDITION = 24,   //device software specific condition prevented request from succeeding
                                        //e.g. lockout or environmental condition not in range
  SFC_FIELD_OPERATION_CONDITION = 25,   //field specific condition prevented request from succeeding
                                        //e.g. lockout or environmental condition not in range
  SFC_CONFIGURATION_MISMATCH = 26,      //a configuration conflict was detected
  SFC_INSUFFICIENT_DEVICE_RESOURCES = 27, //e.g. queue full, buffers/memory unavailable
  SFC_VALUE_LIMITED = 28,               //e.g. value limited by device
  SFC_DATA_WARNING = 29,                //e.g. value has been modified due to a device specific reason
  SFC_INALID_FUNCTION_REFERENCE = 30,   //function referenced for execution is invalid
  SFC_FUNCTION_PROCESS_ERROR = 31,      //function referenced could not be performed due to a device specific reason
  SFC_WARNING = 32,                     //successful; there is aditional info that may be of interest to the user
                                        //e.g. via accessing an attribute or by sending an alert
  SFC_WRITE_ONLY_ATTRIBUTE = 33,        //write-only attribute (e.g. a command attribute)
  SFC_OPERATION_ACCEPTED = 34,          //method operation accepted
  SFC_INVALID_BLOCK_SIZE = 35,          //upload or download block size not valid
  SFC_INVALID_DOWNLOAD_SIZE = 36,       //total size for upload not valid
  SFC_UNEXPECTED_METH_SEQUENCE = 37,    //required method sequencing has not been followed
  SFC_TIMING_VIOLATION = 38,            //object timing requirements have not been satisfied
  SFC_OPERATION_INCOMPLETE = 39,        //method operation, or method operation sequence not successful
  SFC_INVALID_DATA = 40,                //data received is not valid (e.g., checksum error, data content not as expected)
  SFC_DATA_SEQUENCE_ERROR = 41,         //data is ordered; data received is not in the order required 
				        //example: duplicate data was received
  SFC_OPERATION_ABORTED = 42,           //operation aborted by server
  SFC_INVALID_BLOCK_NUMBER = 43,        //invalid block number
  SFC_BLOCK_DATA_ERROR = 44,            //error in block of data, example, wrong size, invalid content
  SFC_BLOCK_NOT_DOWNLOAD = 45,          //the specified block of data has not been successfully downloaded
  
  SFC_VENDOR_DEFINED_ERROR_128  = 128,
  
  SFC_TIMEOUT                     = 129,
  SFC_DEVICE_NOT_FOUND            = 130,
  SFC_DUPLICATE                   = 131,
  SFC_ELEMENT_NOT_FOUND           = 132,
  SFC_INVALID_ADDRESS             = 133,
  SFC_NO_LINK                     = 134,
  SFC_RX_LINK                     = 135,
  SFC_TX_LINK                     = 136,
  SFC_NO_ROUTE                    = 137,
  SFC_NO_CONTRACT                 = 138,
  SFC_NO_KEY                      = 139,
  SFC_NACK                        = 140,
  SFC_NO_GRAPH                    = 141,
  SFC_TTL_EXPIRED                 = 142,
  SFC_RETRY_NO_EXPIRED_ON_GRAPH   = 143,
  SFC_RETRY_NO_EXPIRED_ON_SRC     = 144,  
  SFC_TOO_LATE                    = 145,
  SFC_ACK_RECEIVED                = 146,
  SFC_NOT_MY_ACK                  = 147,
  SFC_UNSUPPORTED_MMIC_SIZE       = 178,
  SFC_SECURITY_FAILED             = 179,
  SFC_NO_ACK_EXPECTED             = 180,  
  
  
  SFC_VENDOR_DEFINED_ERROR_254  = 254,
  SFC_EXTENSION_CODE = 255
};

#endif //_NIVIS_SFC_H_
