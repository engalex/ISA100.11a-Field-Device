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
* Name:         asm.c
* Author:       Marius Vilvoi
* Date:         October 2007
* Description:  This file is provided ...
*               This file holds definitions of the ...
* Changes:
* Revisions:
****************************************************************************************************/
#include "asm.h"
#include "itc.h"
#include "global.h"
#include "MC1322x.h"
#include "digitals.h"
#include "itc.h"
#include <string.h>
#include "spif_interface.h"
#include "system.h"


#define TO_ENCRYPT_MAX_LEN    (1024) // may not need so much

#define AES_ON_MODE_CTR()         ( ASM_CONTROL1_CRYPTMODE = 2 )    // ASM_CONTROL1_CTR = 1; ASM_CONTROL1_CBC = 0;
#define AES_ON_MODE_CBC()         ( ASM_CONTROL1_CRYPTMODE = 1 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 1;
#define AES_ON_MODE_CCM()         ( ASM_CONTROL1_CRYPTMODE = 3 )    // ASM_CONTROL1_CTR = 1; ASM_CONTROL1_CBC = 1;
#define AES_OFF()                 ( ASM_CONTROL1_CRYPTMODE = 0 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 0;

#define AES_IS_OFF()              ( ASM_CONTROL1_CRYPTMODE == 0 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 0;


#define DISABLE_ASM_MODULE()      ( ASM_CONTROL1_ON = 0 )
#define ENABLE_ASM_MODULE()       ( ASM_CONTROL1_ON = 1 )

#define ASM_SELFTEST_ON()         ( ASM_CONTROL1_SELFTEST = 1 )
#define ASM_SELFTEST_OFF()        ( ASM_CONTROL1_SELFTEST = 0 )

#define ASM_NORMALMODE_ON()       ( ASM_CONTROL1_MODE = 1 )
#define ASM_NORMALMODE_OFF()      ( ASM_CONTROL1_MODE = 0 )

#define ASM_CTR_ON()              ( ASM_CONTROL1_CRYPTMODE = 2 )    // ASM_CONTROL1_CTR = 1; ASM_CONTROL1_CBC = 0;
#define ASM_CTR_OFF()             ( ASM_CONTROL1_CRYPTMODE = 0 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 0;

#define ASM_CBC_ON()              ( ASM_CONTROL1_CRYPTMODE = 1 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 1;
#define ASM_CBC_OFF()             ( ASM_CONTROL1_CRYPTMODE = 0 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 0;

#define ASM_CCM_ON()              ( ASM_CONTROL1_CRYPTMODE = 3 )    // ASM_CONTROL1_CTR = 1; ASM_CONTROL1_CBC = 1;
#define ASM_CCM_OFF()             ( ASM_CONTROL1_CRYPTMODE = 0 )    // ASM_CONTROL1_CTR = 0; ASM_CONTROL1_CBC = 0;

// self clear bits
#define ASM_START_OPERATION()     ( ASM_CONTROL0_START = 1 )
#define ASM_CLEAR()               ( ASM_CONTROL0_CLEAR = 1 )
#define ASM_LOAD_MAC()            ( ASM_CONTROL0_LOAD_MAC = 1 )



 uint32 g_aulAESTempBufer[TO_ENCRYPT_MAX_LEN/sizeof(uint32)]; 

__arm uint32 rotate4Bytes(uint32 p_ulData )
{
    uint32 v = p_ulData;
    uint32 t = v ^ ((v << 16) | (v >> 16));
    t &= ~0xff0000;
    v = (v << 24) | (v >> 8);
    return v ^ (t >> 8);
}

__arm uint32 rotUnaligned4Bytes2(const uint8 * p_pData )
{
   return ( (uint32)p_pData[0] << 24 ) | ( (uint32)p_pData[1] << 16 ) | ( (uint32)p_pData[2] << 8 ) | p_pData[3];
}

__arm uint32 rotUnaligned4Bytes3(const uint8 * p_pData, int32 p_nDataLen )
{
    if( p_nDataLen >= 4 )
    {
        return ( (uint32)p_pData[0] << 24 ) | ( (uint32)p_pData[1] << 16 ) | ( (uint32)p_pData[2] << 8 ) | p_pData[3];
    }

    switch( p_nDataLen )
    {
    case 0: return 0;  
    case 1: return  ( (uint32)p_pData[0] << 24 );
    case 2: return  ( (uint32)p_pData[0] << 24 ) | ( (uint32)p_pData[1] << 16 );
    case 3: return  ( (uint32)p_pData[0] << 24 ) | ( (uint32)p_pData[1] << 16 ) | ( (uint32)p_pData[2] << 8 );
    }

    return  0; // not necesary but to make compiler happy
}

__arm uint32 getZeroFilled4Bytes(uint32  p_ulData, int32 p_nDataBytesLen )
{
    if( p_nDataBytesLen >= 4 )
        return p_ulData;

    switch( p_nDataBytesLen )
    {
    case 1: return (p_ulData & 0xFF000000);
    case 2: return (p_ulData & 0xFFFF0000);
    case 3: return (p_ulData & 0xFFFFFF00);
    }

    // <= 0
    return  0;
}

__arm void loadDataReg(const uint8 * p_pData, int32 p_nDataLen )
{
    volatile uint32 * pData = &ASM_DATA0;
    
    pData[0] = rotUnaligned4Bytes3( p_pData, p_nDataLen );
    pData[1] = rotUnaligned4Bytes3( p_pData+4, p_nDataLen-4 );
    pData[2] = rotUnaligned4Bytes3( p_pData+8, p_nDataLen-8 );
    pData[3] = rotUnaligned4Bytes3( p_pData+12, p_nDataLen-12 );
}

__arm void startAesAndAuthOnly
                        ( const uint8 * p_pucKey,
                          const uint8 * p_pucNonce,
                          const uint8 * p_pucToAuthOnly,
                          int32         p_lToAuthOnlyLen,
                          uint16        p_unToEncryptLen )
{
    uint32 ulTmp;
   volatile uint32 * pAESReg;

    AES_OFF();
    ASM_CLEAR();                        // clear all memory in ASM to obtain the corect MIC.

    // set keys
    pAESReg = &ASM_KEY0;
    pAESReg[0] = rotate4Bytes( *(uint32*)(p_pucKey) );
    pAESReg[1] = rotate4Bytes( *(uint32*)(p_pucKey+4) );
    pAESReg[2] = rotate4Bytes( *(uint32*)(p_pucKey+8) );
    pAESReg[3] = rotate4Bytes( *(uint32*)(p_pucKey+12) );


    // building B0 and A1 (in counter)
    ulTmp = ((uint32)p_pucNonce[0] << 16) | ((uint32)p_pucNonce[1] << 8) | p_pucNonce[2];
    ASM_DATA0 = (0x49L << 24) | ulTmp;
    ASM_CTR0  = (0x01L << 24) | ulTmp;

    ASM_DATA1 = ASM_CTR1 = rotUnaligned4Bytes2(p_pucNonce+3);
    ASM_DATA2 = ASM_CTR2 = rotUnaligned4Bytes2(p_pucNonce+7);

    ulTmp = ((uint32)p_pucNonce[11] << 24) | ((uint32)p_pucNonce[12] << 16);
    ASM_DATA3 = ulTmp | p_unToEncryptLen;
    ASM_CTR3  = ulTmp | 0x0001;

    // start with authetication only
    AES_ON_MODE_CBC();
    ASM_START_OPERATION();

    // building B1

    ulTmp  =  p_lToAuthOnlyLen << 16;
    if( p_lToAuthOnlyLen > 1 )
    {
        ulTmp |= ( (uint32)p_pucToAuthOnly[0] << 8 ) | p_pucToAuthOnly[1];
    }
    else if( p_lToAuthOnlyLen == 1 )
    {
        ulTmp |= ( (uint32)p_pucToAuthOnly[0] << 8 );
    }

     while ( ! (ASM_STATUS_DONE) )
        ;
    // done with B0

    pAESReg = &ASM_DATA0;
    pAESReg[0] = ulTmp;
    pAESReg[1] = rotUnaligned4Bytes3( p_pucToAuthOnly+2, p_lToAuthOnlyLen-2 );
    pAESReg[2] = rotUnaligned4Bytes3( p_pucToAuthOnly+6, p_lToAuthOnlyLen-6 );
    pAESReg[3] = rotUnaligned4Bytes3( p_pucToAuthOnly+10, p_lToAuthOnlyLen-10 );

    // proccesing B1
    ASM_START_OPERATION();

    p_pucToAuthOnly += 14;
    p_lToAuthOnlyLen -= 14;

    while ( p_lToAuthOnlyLen > 0 )
    {
        while ( ! (ASM_STATUS_DONE) )
          ;

        loadDataReg( p_pucToAuthOnly, p_lToAuthOnlyLen );

        ASM_START_OPERATION();

        p_lToAuthOnlyLen -= 16;
        p_pucToAuthOnly += 16;
    }

    while ( ! (ASM_STATUS_DONE) )
       ;
}

// This function allows to carry out a self test to verify the operation of the encryption engine.
void ASM_Init(void)
{
  volatile uint16 unTestTimeout = 3330;

  ENABLE_ASM_MODULE();
  if( ! ASM_STATUS_TESTPASS )
  {
      ASM_SELFTEST_ON();
      ASM_START_OPERATION();

      // wait timeout
      while(  !(ASM_STATUS_TESTPASS) && (unTestTimeout))
      {
        unTestTimeout--;
      }

      ASM_SELFTEST_OFF();

      ASM_NORMALMODE_ON();
  }
}



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
//                p_pucToEncrypt  - to autehnticate and encrypt buffer
//                p_unToEncryptLen  - to autehnticate and encrypt buffer len
//                p_pucEncryptedBuff  - encrypted buffer + 32 MIC
//                p_ucInterruptFlag - 1 if the call is from  interrupt, 0 from user space
// Return:        SUCCES or ERROR
// Obs:
//      Access level: Interrupt level for p_ucInterruptFlag = 1 and user level for p_ucInterruptFlag = 0
// NOTE :         Built for Auth
//          B0 = AuthFlags || Nonce N || l(m).         // l(m) = p_unToEncryptLen
//          Ai = EncrFlags || Nonce N || Counter i, for i = 0, 1, 2, …
///////////////////////////////////////////////////////////////////////////////////
__arm uint8 AES_Crypt
                        ( const uint8 * p_pucKey,
                          const uint8 * p_pucNonce,
                          const uint8 * p_pucToAuthOnly,
                          uint16        p_unToAuthOnlyLen,
                          uint8       * p_pucToEncrypt,
                          uint16        p_unToEncryptLen )
{
int32 lLen;
uint32 * pAESBuffer; 
uint8  * pToEncrypt; 
unsigned int  unIrqState = IntDisableIRQ();
volatile uint32 * pAESReg;
      
  do
  {      
#pragma diag_suppress=Pe170
      pAESBuffer = g_aulAESTempBufer - 4; 
#pragma diag_default=Pe170
      pToEncrypt = p_pucToEncrypt;
      
      startAesAndAuthOnly( p_pucKey, p_pucNonce, p_pucToAuthOnly, p_unToAuthOnlyLen, p_unToEncryptLen );
      AES_ON_MODE_CCM();          // continue with authetication and encryption of plain text

      // Encryption with Auth
      lLen = (uint32)p_unToEncryptLen;
      while ( lLen > 0 )
      {
          loadDataReg( pToEncrypt, lLen );

          ASM_START_OPERATION();

          if( unIrqState == IRQ_STATE_ENABLED ) // on user space with interrupts enabled
          {
            IntEnableIRQ();
            __asm("  Nop ");
            IntDisableIRQ();
            
            if( AES_IS_OFF() ) // IRQ happened 
            {
                break;
            }
          }
          
          pToEncrypt += 16;
          lLen -= 16;
          pAESBuffer += 4;

          while ( ! (ASM_STATUS_DONE) )
            ;

          pAESReg = &ASM_CTR_RESULT0;
          pAESBuffer[0] = rotate4Bytes( pAESReg[0] );
          pAESBuffer[1] = rotate4Bytes( pAESReg[1] );
          pAESBuffer[2] = rotate4Bytes( pAESReg[2] );
          pAESBuffer[3] = rotate4Bytes( pAESReg[3] );

          ASM_CTR3++;
      }
  } while( lLen > 0 ); // repeat if interrupt happend during ASM operation
  
  AES_ON_MODE_CTR(); // encrypt 32 bits MIC

  ASM_DATA0 = ASM_CBC_RESULT0;

  ((uint8*)(&ASM_CTR3))[0] = 0;

  ASM_START_OPERATION();

  memcpy( p_pucToEncrypt, g_aulAESTempBufer, p_unToEncryptLen );
  p_pucToEncrypt += p_unToEncryptLen;
  
  while ( ! (ASM_STATUS_DONE) )
    ;

  lLen = ASM_CTR_RESULT0;
  AES_OFF();

  IntRestoreIRQ( unIrqState );
  
  p_pucToEncrypt[0] =  (uint8) (((uint32)lLen) >> 24);
  p_pucToEncrypt[1] =  (uint8) (((uint32)lLen) >> 16);
  p_pucToEncrypt[2] =  (uint8) (((uint32)lLen) >> 8);
  p_pucToEncrypt[3] =  (uint8) ((uint32)lLen);

  return AES_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////////
// Name: AES_Decrypt
// Author: NIVIS LLC,
// Description: Perfrorms 128 AES decryption over p_pucToEncrypt buffer and
//                  32 bit authenthication over p_pucToAuthOnly | p_pucDecryptedBuff buffer
// Parameters:
//                p_pucKey  - encryption key (16 bytes long)
//                p_pucNonce  - nonce (13 bytes long)
//                p_pucToAuthOnly  - to authenticate buffer
//                p_unToAuthOnlyLen  - to authenticate buffer length
//                p_pucToDecrypt  - to autehnticate and decrypt buffer
//                p_unToDecryptLen  - to autehnticate and decrypt buffer len. !!! ??? ATTENTION :  must contain to decrypt len + 4 len of MIC 32
//                p_pucDecryptedBuff  - decrypted buffer ( fara 32 MIC )
// Return:        SUCCES or ERROR
// Obs:
//      Access level: Interrupt level for p_ucInterruptFlag = 1 and user level for p_ucInterruptFlag = 0
// Obs:
//      Access level: Interrupt level only
///////////////////////////////////////////////////////////////////////////////////
__arm uint8 AES_Decrypt
                        ( const uint8 * p_pucKey,
                          const uint8 * p_pucNonce,
                          const uint8 * p_pucToAuthOnly,
                          uint16        p_unToAuthOnlyLen,
                          uint8       * p_pucToDecrypt,
                          uint16        p_unToDecryptLen )
{
int32  lLen;
uint32 * pAESBuffer; 
uint8  * pToDecrypt; 
unsigned int  unIrqState; 
uint32   ulTmp0,ulTmp1,ulTmp2,ulTmp3;
 volatile uint32 * pAESReg;

  if ( p_unToDecryptLen < 4 )  return AES_ERROR;

  // recover the crypted MIC to decrypt it
  p_unToDecryptLen -=4;

  unIrqState = IntDisableIRQ();
      
  do
  {
#pragma diag_suppress=Pe170
      pAESBuffer = g_aulAESTempBufer - 4; 
#pragma diag_default=Pe170
      pToDecrypt = p_pucToDecrypt;
      startAesAndAuthOnly( p_pucKey, p_pucNonce, p_pucToAuthOnly, p_unToAuthOnlyLen, p_unToDecryptLen );

      lLen = (uint32)p_unToDecryptLen;
      while ( lLen > 0  )
      {
          loadDataReg( pToDecrypt, lLen );

          AES_ON_MODE_CTR();   // decryption only
          ASM_START_OPERATION();
          
          if( unIrqState == IRQ_STATE_ENABLED ) // on user space with interrupts enabled
          {
            IntEnableIRQ();
            __asm("  Nop ");
            IntDisableIRQ();
            
            if( AES_IS_OFF() ) // IRQ happened 
            {
                break;
            }
          }
          
          pToDecrypt += 16;
          pAESBuffer += 4;
          
          while ( ! (ASM_STATUS_DONE) )
            ;

          pAESReg = &ASM_CTR_RESULT0;
          if( lLen < 16 )
          {
              ulTmp0 = getZeroFilled4Bytes( pAESReg[0], lLen );
              ulTmp1 = getZeroFilled4Bytes( pAESReg[1], lLen-4 );
              ulTmp2 = getZeroFilled4Bytes( pAESReg[2], lLen-8 );
              ulTmp3 = getZeroFilled4Bytes( pAESReg[3], lLen-12 );
          }
          else
          {
              ulTmp0 = pAESReg[0];
              ulTmp1 = pAESReg[1];
              ulTmp2 = pAESReg[2];
              ulTmp3 = pAESReg[3];
          }

          pAESReg = &ASM_DATA0;
          pAESReg[0] = ulTmp0;
          pAESReg[1] = ulTmp1;
          pAESReg[2] = ulTmp2;
          pAESReg[3] = ulTmp3;
          
          AES_ON_MODE_CBC();   // auth only
          ASM_START_OPERATION();

          pAESBuffer[0] = rotate4Bytes( ulTmp0 );
          pAESBuffer[1] = rotate4Bytes( ulTmp1 );
          pAESBuffer[2] = rotate4Bytes( ulTmp2 );
          pAESBuffer[3] = rotate4Bytes( ulTmp3 );

          lLen -= 16;

//          while ( ! (ASM_STATUS_DONE) )
//            ;

          ASM_CTR3++;
      }
  } while( lLen > 0 ); // repeat if interrupt happend during ASM operation

  AES_ON_MODE_CTR();   // decryption only

  ASM_DATA0 = ASM_CBC_RESULT0; // the computed MIC 32

  ((uint8*)(&ASM_CTR3))[0] = 0;

  ASM_START_OPERATION();

  memcpy( p_pucToDecrypt, g_aulAESTempBufer, p_unToDecryptLen ); 

  while ( ! (ASM_STATUS_DONE) )
  ;

  lLen = ASM_CTR_RESULT0;
  AES_OFF();

  IntRestoreIRQ( unIrqState );

//  SET_GPIO_LO( TEST_LINE_3 );
  return (lLen == rotUnaligned4Bytes2( p_pucToDecrypt + p_unToDecryptLen ) ? AES_SUCCESS : AES_ERROR);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Implementation of the Matyas-Meyer-Oseas hash function with 16 byte block length 
/// @param  p_pucInputBuff - the input buffer with length in bytes not smaller than 16 bytes  
/// @param  p_unInputBuffLen - length in bytes of the input buffer
/// @return AES_ERROR if fail, AES_SUCCESS if success 
/// @remarks
///      Access level: User
///      Obs: result data replaces the first "MAX_HMAC_INPUT_SIZE" bytes from original source buffer
void Crypto_Hash_Function( const uint8 * p_pucKey,
                            uint8  p_ucXOR,
                            uint8* p_pucInputBuff,
                            uint16 p_unInputBuffLen
                           )
{  
  volatile uint32 * pAESReg;
  uint8* pucOutputBuff = p_pucInputBuff;
  uint8 aucInput[HASH_DATA_BLOCK_LEN];
  uint16 unInitialLen = p_unInputBuffLen + HASH_DATA_BLOCK_LEN;
  
  unsigned int ucNextBlock = 0;
  unsigned int ucLastBlock = 0;  
  
  //the key for the first algorithm iteration need to be full 0 
  memset(aucInput, 0x00, HASH_DATA_BLOCK_LEN);
  
  MONITOR_ENTER();
  
  AES_OFF();                                                                    // RESET AES State Machine
  ASM_CLEAR();                                                                  // clear all memory in ASM to obtain the corect MIC.
  AES_ON_MODE_CTR();                                                            // do only AES and XOR with all DATA_X zeros
  
  pAESReg = &ASM_DATA0;

  pAESReg[0] = 0; 
  pAESReg[1] = 0;
  pAESReg[2] = 0;
  pAESReg[3] = 0;
  
  while(!ucLastBlock)
  {
    // set curent iteration key
    pAESReg = &ASM_KEY0;
    pAESReg[0] = rotate4Bytes( *(uint32*)aucInput );
    pAESReg[1] = rotate4Bytes( *(uint32*)(aucInput+4) );
    pAESReg[2] = rotate4Bytes( *(uint32*)(aucInput+8) );
    pAESReg[3] = rotate4Bytes( *(uint32*)(aucInput+12) );

    if( p_pucKey ) // first block is the key XOR p_ucXOR
    {
        for(int i = 0; i<HASH_DATA_BLOCK_LEN; i++)
        {
            aucInput[i] = p_pucKey[i] ^ p_ucXOR;
        }
        p_pucKey = NULL;
    }
    else if( p_unInputBuffLen >= HASH_DATA_BLOCK_LEN )
    {
        memcpy(aucInput, p_pucInputBuff, HASH_DATA_BLOCK_LEN);  
        
        p_unInputBuffLen -= HASH_DATA_BLOCK_LEN;
        p_pucInputBuff += HASH_DATA_BLOCK_LEN;
    }  
    else
    {
        memset(aucInput, 0x00, HASH_DATA_BLOCK_LEN);
        aucInput[p_unInputBuffLen] = 0x80;   //the bit7 of the padding's first byte must be "1"
        
        if( !p_unInputBuffLen )
        {
            //last iteration
            ucLastBlock = 1;   
        }
        else
        {
            //+1 - concatenation of the input buffer with first byte 0x80
            //+2 - adding the last 2 bytes representing the buffer length info - particular case for 16 bytes data block len
            if( HASH_DATA_BLOCK_LEN - 3 >= p_unInputBuffLen)
            {
                memcpy(aucInput, p_pucInputBuff, p_unInputBuffLen);
                //last iteration
                ucLastBlock = 1;
            }
            else if( !ucNextBlock && (HASH_DATA_BLOCK_LEN - 2 <= p_unInputBuffLen) )
            {
                memcpy(aucInput, p_pucInputBuff, p_unInputBuffLen);
                ucNextBlock = 1;
            }
            else
            {
                aucInput[p_unInputBuffLen] = 0x00;  
                ucLastBlock = 1;      
            }
        }
        
        if( ucLastBlock )
        {
            //it's the last block
            aucInput[HASH_DATA_BLOCK_LEN - 2] = unInitialLen >> 5;  //the MSB of bits number inside the input buffer
            aucInput[HASH_DATA_BLOCK_LEN - 1] = unInitialLen << 3;  //the LSB of bits number inside the input buffer
        }

    }
    
    pAESReg = &ASM_CTR0;
    
    pAESReg[0] = rotate4Bytes(*(uint32*)(aucInput));
    pAESReg[1] = rotate4Bytes(*(uint32*)(aucInput+4));
    pAESReg[2] = rotate4Bytes(*(uint32*)(aucInput+8));
    pAESReg[3] = rotate4Bytes(*(uint32*)(aucInput+12));
        
    // COMPUTE AES
    ASM_START_OPERATION();  
    
    while ( ! (ASM_STATUS_DONE) )
      ;   
      
    pAESReg = &ASM_CTR_RESULT0;
    
    // xor-ing with Mi
    * ( uint32 * ) (aucInput)    ^= rotate4Bytes( pAESReg[0] );
    * ( uint32 * ) (aucInput+4)  ^= rotate4Bytes( pAESReg[1] );
    * ( uint32 * ) (aucInput+8)  ^= rotate4Bytes( pAESReg[2] );
    * ( uint32 * ) (aucInput+12) ^= rotate4Bytes( pAESReg[3] );    
  }
  
  AES_OFF();
  MONITOR_EXIT();
  
  //save the result data over the original buffer
  memcpy(pucOutputBuff, aucInput, HASH_DATA_BLOCK_LEN);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// @author NIVIS LLC, Mircea Vlasin
/// @brief  Implementation of Keyed-Hash Message Authentication Code algorithm
/// @param  p_pucKey - cryptographic key(16 bytes length)
/// @param  p_pucInputBuff - the input buffer with length in bytes not smaller than 16 bytes 
/// @param  p_unInputBuffLen - length in bytes of the input buffer
/// @return none 
/// @remarks
///      Access level: User
///      Obs: result data replaces the first "MAX_HMAC_INPUT_SIZE" bytes from original source buffer
void Keyed_Hash_MAC(const uint8* p_pucKey, 
                  uint8* p_pucInputBuff,
                  uint16 p_unInputBuffLen
                  )
{
  Crypto_Hash_Function(p_pucKey, 0x36, p_pucInputBuff, p_unInputBuffLen );   
  Crypto_Hash_Function(p_pucKey, 0x5C, p_pucInputBuff, HASH_DATA_BLOCK_LEN );  
}

