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

/***************************************************************************************************
* Name:         asm.h
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This header file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#ifndef _NIVIS_ASM_H_
#define _NIVIS_ASM_H_

#include "typedef.h"

//#define AES_CANNOT_ENCRYPT_ON_SAME_BUFFER

#define AES_SUCCESS             0
#define AES_ERROR               1

#define MAX_NONCE_LEN           13


__arm uint32 rotate4Bytes( uint32 p_ulData );

void ASM_Init(void);


///////////////////////////////////////////////////////////////////////////////////
// Name: AES_Crypt
// Author: NIVIS LLC,
// Description: Perfrorms 128 AES encryption over p_pucToEncrypt buffer and
//                  32 bit authenthication over p_pucToAuthOnly | p_pucToEncrypt buffer
// Parameters:
//                p_pucKey  - encryption key (16 bytes long)
//                p_pucNonce  - nonce (13 bytes long)
//                p_pucToAuthOnly  - to authenticate buffer
//                p_unToAuthOnlyLen  - to authenticate buffer length
//                p_pucToEncrypt  - to autehnticate and encrypt buffer and will contains the result (+MIC32 must be allocated)
//                p_unToEncryptLen  - to autehnticate and encrypt buffer len
// Return:        SUCCES or ERROR
// Obs:
//      Access level: Interrupt level for p_ucInterruptFlag = 1 and user level for p_ucInterruptFlag = 0
///////////////////////////////////////////////////////////////////////////////////
__arm uint8 AES_Crypt   ( const uint8 * p_pucKey,
                          const uint8 * p_pucNonce,
                          const uint8 * p_pucToAuthOnly,
                          uint16        p_unToAuthOnlyLen,
                          uint8       * p_pucToEncrypt,
                          uint16        p_unToEncryptLen );


///////////////////////////////////////////////////////////////////////////////////
// Name: AES_Decrypt
// Author: NIVIS LLC,
// Description: Perfrorms 128 AES decryption over p_pucToDecrypt buffer and
//                  32 bit authenthication over p_pucToAuthOnly | p_pucDecryptedBuff buffer
// Parameters:
//                p_pucKey  - encryption key (16 bytes long)
//                p_pucNonce  - nonce (13 bytes long)
//                p_pucToAuthOnly  - to authenticate buffer
//                p_unToAuthOnlyLen  - to authenticate buffer length
//                p_pucToDecrypt  - to autehnticate and decrypt buffer and will contains the result
//                p_unToDecryptLen  - to autehnticate and decrypt buffer len. !!! ??? ATTENTION :  must contain to decrypt len + 4 len of MIC 32
// Return:        SUCCES or ERROR
// Obs:
//      Access level: Interrupt level for p_ucInterruptFlag = 1 and user level for p_ucInterruptFlag = 0
// Obs:
//      Access level: Interrupt level only
///////////////////////////////////////////////////////////////////////////////////
__arm uint8 AES_Decrypt ( const uint8 * p_pucKey,
                          const uint8 * p_pucNonce,
                          const uint8 * p_pucToAuthOnly,
                          uint16        p_unToAuthOnlyLen,
                          uint8       * p_pucToDecrypt,
                          uint16        p_unToDecryptLen);


#define AES_Decrypt_User AES_Decrypt
#define AES_Crypt_User   AES_Crypt

#define HASH_DATA_BLOCK_LEN     (uint8)16           //lenght in bytes of the hash data block
#define MAX_HMAC_INPUT_SIZE     2*16+2*8+2+2*2+2*16 //according with the Security Join Response Specs

void Keyed_Hash_MAC(const uint8* p_pucKey,
                  uint8* p_pucInputBuff,
                  uint16 p_unInputBuffLen
                  );

#endif /* _NIVIS_ASM_H_ */

