/********************************************************************************/
/*										*/
/*			     TSS Authorization 					*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*            $Id: tssauth.c 791 2016-10-26 21:03:31Z kgoldman $		*/
/*										*/
/* (c) Copyright IBM Corporation 2015.						*/
/*										*/
/* All rights reserved.								*/
/* 										*/
/* Redistribution and use in source and binary forms, with or without		*/
/* modification, are permitted provided that the following conditions are	*/
/* met:										*/
/* 										*/
/* Redistributions of source code must retain the above copyright notice,	*/
/* this list of conditions and the following disclaimer.			*/
/* 										*/
/* Redistributions in binary form must reproduce the above copyright		*/
/* notice, this list of conditions and the following disclaimer in the		*/
/* documentation and/or other materials provided with the distribution.		*/
/* 										*/
/* Neither the names of the IBM Corporation nor the names of its		*/
/* contributors may be used to endorse or promote products derived from		*/
/* this software without specific prior written permission.			*/
/* 										*/
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS		*/
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT		*/
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR	*/
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT		*/
/* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,	*/
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT		*/
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,	*/
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY	*/
/* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT		*/
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE	*/
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.		*/
/********************************************************************************/

/* This layer handles command and response packet authorization parameters. */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef TPM_POSIX
#include <netinet/in.h>
#endif
#ifdef TPM_WINDOWS
#include <winsock2.h>
#endif

#include <tss2/tsserror.h>
#include <tss2/tssmarshal.h>
#include <tss2/Unmarshal_fp.h>

#include <tss2/tsstransmit.h>
#include <tss2/tssproperties.h>
#include <tss2/tssresponsecode.h>

#include "tssauth.h"

extern int tssVerbose;
extern int tssVverbose;

typedef TPM_RC (*MarshalFunction_t)(COMMAND_PARAMETERS *source,
				    UINT16 *written, BYTE **buffer, INT32 *size);
typedef TPM_RC (*UnmarshalFunction_t)(RESPONSE_PARAMETERS *target,
				      TPM_ST tag, BYTE **buffer, INT32 *size);
typedef TPM_RC (*UnmarshalInFunction_t)(COMMAND_PARAMETERS *target,
					BYTE **buffer, INT32 *size, TPM_HANDLE handles[]);

typedef struct MARSHAL_TABLE {
    TPM_CC 			commandCode;
    const char 			*commandText;
    MarshalFunction_t 		marshalFunction;	/* marshal input command */
    UnmarshalFunction_t 	unmarshalFunction;	/* unmarshal output response */
    UnmarshalInFunction_t	unmarshalInFunction;	/* unmarshal input command for parameter
							   checking */
} MARSHAL_TABLE;

static const MARSHAL_TABLE marshalTable [] = {
				 
    {TPM_CC_Startup, "TPM2_Startup",
     (MarshalFunction_t)TSS_Startup_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)Startup_In_Unmarshal},

    {TPM_CC_Shutdown, "TPM2_Shutdown",
     (MarshalFunction_t)TSS_Shutdown_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)Shutdown_In_Unmarshal},

    {TPM_CC_SelfTest, "TPM2_SelfTest",
     (MarshalFunction_t)TSS_SelfTest_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)SelfTest_In_Unmarshal},

    {TPM_CC_IncrementalSelfTest, "TPM2_IncrementalSelfTest",
     (MarshalFunction_t)TSS_IncrementalSelfTest_In_Marshal,
     (UnmarshalFunction_t)TSS_IncrementalSelfTest_Out_Unmarshal,
     (UnmarshalInFunction_t)IncrementalSelfTest_In_Unmarshal},

    {TPM_CC_GetTestResult, "TPM2_GetTestResult",
     NULL,
     (UnmarshalFunction_t)TSS_GetTestResult_Out_Unmarshal,
     NULL},

    {TPM_CC_StartAuthSession, "TPM2_StartAuthSession",
     (MarshalFunction_t)TSS_StartAuthSession_In_Marshal,
     (UnmarshalFunction_t)TSS_StartAuthSession_Out_Unmarshal,
     (UnmarshalInFunction_t)StartAuthSession_In_Unmarshal},
    
    {TPM_CC_PolicyRestart, "TPM2_PolicyRestart",
     (MarshalFunction_t)TSS_PolicyRestart_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyRestart_In_Unmarshal},

    {TPM_CC_Create, "TPM2_Create",
     (MarshalFunction_t)TSS_Create_In_Marshal,
     (UnmarshalFunction_t)TSS_Create_Out_Unmarshal,
     (UnmarshalInFunction_t)Create_In_Unmarshal},

    {TPM_CC_Load, "TPM2_Load",
     (MarshalFunction_t)TSS_Load_In_Marshal,
     (UnmarshalFunction_t)TSS_Load_Out_Unmarshal,
     (UnmarshalInFunction_t)Load_In_Unmarshal},

    {TPM_CC_LoadExternal, "TPM2_LoadExternal",
     (MarshalFunction_t)TSS_LoadExternal_In_Marshal,
     (UnmarshalFunction_t)TSS_LoadExternal_Out_Unmarshal,
     (UnmarshalInFunction_t)LoadExternal_In_Unmarshal},

    {TPM_CC_ReadPublic, "TPM2_ReadPublic",
     (MarshalFunction_t)TSS_ReadPublic_In_Marshal,
     (UnmarshalFunction_t)TSS_ReadPublic_Out_Unmarshal,
     (UnmarshalInFunction_t)ReadPublic_In_Unmarshal},

    {TPM_CC_ActivateCredential, "TPM2_ActivateCredential",
     (MarshalFunction_t)TSS_ActivateCredential_In_Marshal,
     (UnmarshalFunction_t)TSS_ActivateCredential_Out_Unmarshal,
     (UnmarshalInFunction_t)ActivateCredential_In_Unmarshal},

    {TPM_CC_MakeCredential, "TPM2_MakeCredential",
     (MarshalFunction_t)TSS_MakeCredential_In_Marshal,
     (UnmarshalFunction_t)TSS_MakeCredential_Out_Unmarshal,
     (UnmarshalInFunction_t)MakeCredential_In_Unmarshal},

    {TPM_CC_Unseal, "TPM2_Unseal",
     (MarshalFunction_t)TSS_Unseal_In_Marshal,
     (UnmarshalFunction_t)TSS_Unseal_Out_Unmarshal,
     (UnmarshalInFunction_t)Unseal_In_Unmarshal},

    {TPM_CC_ObjectChangeAuth, "TPM2_ObjectChangeAuth",
     (MarshalFunction_t)TSS_ObjectChangeAuth_In_Marshal,
     (UnmarshalFunction_t)TSS_ObjectChangeAuth_Out_Unmarshal,
     (UnmarshalInFunction_t)ObjectChangeAuth_In_Unmarshal},

    {TPM_CC_CreateLoaded, "TPM2_CreateLoaded",
     (MarshalFunction_t)TSS_CreateLoaded_In_Marshal,
     (UnmarshalFunction_t)TSS_CreateLoaded_Out_Unmarshal,
     (UnmarshalInFunction_t)CreateLoaded_In_Unmarshal},

    {TPM_CC_Duplicate, "TPM2_Duplicate",
     (MarshalFunction_t)TSS_Duplicate_In_Marshal,
     (UnmarshalFunction_t)TSS_Duplicate_Out_Unmarshal,
     (UnmarshalInFunction_t)Duplicate_In_Unmarshal},

    {TPM_CC_Rewrap, "TPM2_Rewrap",
     (MarshalFunction_t)TSS_Rewrap_In_Marshal,
     (UnmarshalFunction_t)TSS_Rewrap_Out_Unmarshal,
     (UnmarshalInFunction_t)Rewrap_In_Unmarshal},

    {TPM_CC_Import, "TPM2_Import",
     (MarshalFunction_t)TSS_Import_In_Marshal,
     (UnmarshalFunction_t)TSS_Import_Out_Unmarshal,
     (UnmarshalInFunction_t)Import_In_Unmarshal},

    {TPM_CC_RSA_Encrypt, "TPM2_RSA_Encrypt",
     (MarshalFunction_t)TSS_RSA_Encrypt_In_Marshal,
     (UnmarshalFunction_t)TSS_RSA_Encrypt_Out_Unmarshal,
     (UnmarshalInFunction_t)RSA_Encrypt_In_Unmarshal},

    {TPM_CC_RSA_Decrypt, "TPM2_RSA_Decrypt",
     (MarshalFunction_t)TSS_RSA_Decrypt_In_Marshal,
     (UnmarshalFunction_t)TSS_RSA_Decrypt_Out_Unmarshal,
     (UnmarshalInFunction_t)RSA_Decrypt_In_Unmarshal},

    {TPM_CC_ECDH_KeyGen, "TPM2_ECDH_KeyGen",
     (MarshalFunction_t)TSS_ECDH_KeyGen_In_Marshal,
     (UnmarshalFunction_t)TSS_ECDH_KeyGen_Out_Unmarshal,
     (UnmarshalInFunction_t)ECDH_KeyGen_In_Unmarshal},

    {TPM_CC_ECDH_ZGen, "TPM2_ECDH_ZGen",
     (MarshalFunction_t)TSS_ECDH_ZGen_In_Marshal,
     (UnmarshalFunction_t)TSS_ECDH_ZGen_Out_Unmarshal,
     (UnmarshalInFunction_t)ECDH_ZGen_In_Unmarshal},

    {TPM_CC_ECC_Parameters, "TPM2_ECC_Parameters",
     (MarshalFunction_t)TSS_ECC_Parameters_In_Marshal,
     (UnmarshalFunction_t)TSS_ECC_Parameters_Out_Unmarshal,
     (UnmarshalInFunction_t)ECC_Parameters_In_Unmarshal},

    {TPM_CC_ZGen_2Phase, "TPM2_ZGen_2Phase",
     (MarshalFunction_t)TSS_ZGen_2Phase_In_Marshal,
     (UnmarshalFunction_t)TSS_ZGen_2Phase_Out_Unmarshal,
     (UnmarshalInFunction_t)ZGen_2Phase_In_Unmarshal},

    {TPM_CC_EncryptDecrypt, "TPM2_EncryptDecrypt",
     (MarshalFunction_t)TSS_EncryptDecrypt_In_Marshal,
     (UnmarshalFunction_t)TSS_EncryptDecrypt_Out_Unmarshal,
     (UnmarshalInFunction_t)EncryptDecrypt_In_Unmarshal},

    {TPM_CC_EncryptDecrypt2, "TPM2_EncryptDecrypt2",
     (MarshalFunction_t)TSS_EncryptDecrypt2_In_Marshal,
     (UnmarshalFunction_t)TSS_EncryptDecrypt2_Out_Unmarshal,
     (UnmarshalInFunction_t)EncryptDecrypt2_In_Unmarshal},

    {TPM_CC_Hash, "TPM2_Hash",
     (MarshalFunction_t)TSS_Hash_In_Marshal,
     (UnmarshalFunction_t)TSS_Hash_Out_Unmarshal,
     (UnmarshalInFunction_t)Hash_In_Unmarshal},

    {TPM_CC_HMAC, "TPM2_HMAC",
     (MarshalFunction_t)TSS_HMAC_In_Marshal,
     (UnmarshalFunction_t)TSS_HMAC_Out_Unmarshal,
     (UnmarshalInFunction_t)HMAC_In_Unmarshal},

    {TPM_CC_GetRandom, "TPM2_GetRandom",
     (MarshalFunction_t)TSS_GetRandom_In_Marshal,
     (UnmarshalFunction_t)TSS_GetRandom_Out_Unmarshal,
     (UnmarshalInFunction_t)GetRandom_In_Unmarshal},

    {TPM_CC_StirRandom, "TPM2_StirRandom",
     (MarshalFunction_t)TSS_StirRandom_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)StirRandom_In_Unmarshal},

    {TPM_CC_HMAC_Start, "TPM2_HMAC_Start",
     (MarshalFunction_t)TSS_HMAC_Start_In_Marshal,
     (UnmarshalFunction_t)TSS_HMAC_Start_Out_Unmarshal,
     (UnmarshalInFunction_t)HMAC_Start_In_Unmarshal},

    {TPM_CC_HashSequenceStart, "TPM2_HashSequenceStart",
     (MarshalFunction_t)TSS_HashSequenceStart_In_Marshal,
     (UnmarshalFunction_t)TSS_HashSequenceStart_Out_Unmarshal,
     (UnmarshalInFunction_t)HashSequenceStart_In_Unmarshal},

    {TPM_CC_SequenceUpdate, "TPM2_SequenceUpdate",
     (MarshalFunction_t)TSS_SequenceUpdate_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)SequenceUpdate_In_Unmarshal},

    {TPM_CC_SequenceComplete, "TPM2_SequenceComplete",
     (MarshalFunction_t)TSS_SequenceComplete_In_Marshal,
     (UnmarshalFunction_t)TSS_SequenceComplete_Out_Unmarshal,
     (UnmarshalInFunction_t)SequenceComplete_In_Unmarshal},

    {TPM_CC_EventSequenceComplete, "TPM2_EventSequenceComplete",
     (MarshalFunction_t)TSS_EventSequenceComplete_In_Marshal,
     (UnmarshalFunction_t)TSS_EventSequenceComplete_Out_Unmarshal,
     (UnmarshalInFunction_t)EventSequenceComplete_In_Unmarshal},

    {TPM_CC_Certify, "TPM2_Certify",
     (MarshalFunction_t)TSS_Certify_In_Marshal,
     (UnmarshalFunction_t)TSS_Certify_Out_Unmarshal,
     (UnmarshalInFunction_t)Certify_In_Unmarshal},

    {TPM_CC_CertifyCreation, "TPM2_CertifyCreation",
     (MarshalFunction_t)TSS_CertifyCreation_In_Marshal,
     (UnmarshalFunction_t)TSS_CertifyCreation_Out_Unmarshal,
     (UnmarshalInFunction_t)CertifyCreation_In_Unmarshal},

    {TPM_CC_Quote, "TPM2_Quote",
     (MarshalFunction_t)TSS_Quote_In_Marshal,
     (UnmarshalFunction_t)TSS_Quote_Out_Unmarshal,
     (UnmarshalInFunction_t)Quote_In_Unmarshal},

    {TPM_CC_GetSessionAuditDigest, "TPM2_GetSessionAuditDigest",
     (MarshalFunction_t)TSS_GetSessionAuditDigest_In_Marshal,
     (UnmarshalFunction_t)TSS_GetSessionAuditDigest_Out_Unmarshal,
     (UnmarshalInFunction_t)GetSessionAuditDigest_In_Unmarshal},

    {TPM_CC_GetCommandAuditDigest, "TPM2_GetCommandAuditDigest",
     (MarshalFunction_t)TSS_GetCommandAuditDigest_In_Marshal,
     (UnmarshalFunction_t)TSS_GetCommandAuditDigest_Out_Unmarshal,
     (UnmarshalInFunction_t)GetCommandAuditDigest_In_Unmarshal},

    {TPM_CC_GetTime, "TPM2_GetTime",
     (MarshalFunction_t)TSS_GetTime_In_Marshal,
     (UnmarshalFunction_t)TSS_GetTime_Out_Unmarshal,
     (UnmarshalInFunction_t)GetTime_In_Unmarshal},

    {TPM_CC_Commit, "TPM2_Commit",
     (MarshalFunction_t)TSS_Commit_In_Marshal,
     (UnmarshalFunction_t)TSS_Commit_Out_Unmarshal,
     (UnmarshalInFunction_t)Commit_In_Unmarshal},

    {TPM_CC_EC_Ephemeral, "TPM2_EC_Ephemeral",
     (MarshalFunction_t)TSS_EC_Ephemeral_In_Marshal,
     (UnmarshalFunction_t)TSS_EC_Ephemeral_Out_Unmarshal,
     (UnmarshalInFunction_t)EC_Ephemeral_In_Unmarshal},

    {TPM_CC_VerifySignature, "TPM2_VerifySignature",
     (MarshalFunction_t)TSS_VerifySignature_In_Marshal,
     (UnmarshalFunction_t)TSS_VerifySignature_Out_Unmarshal,
     (UnmarshalInFunction_t)VerifySignature_In_Unmarshal},

    {TPM_CC_Sign, "TPM2_Sign",
     (MarshalFunction_t)TSS_Sign_In_Marshal,
     (UnmarshalFunction_t)TSS_Sign_Out_Unmarshal,
     (UnmarshalInFunction_t)Sign_In_Unmarshal},

    {TPM_CC_SetCommandCodeAuditStatus, "TPM2_SetCommandCodeAuditStatus",
     (MarshalFunction_t)TSS_SetCommandCodeAuditStatus_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)SetCommandCodeAuditStatus_In_Unmarshal},

    {TPM_CC_PCR_Extend, "TPM2_PCR_Extend",
     (MarshalFunction_t)TSS_PCR_Extend_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PCR_Extend_In_Unmarshal},

    {TPM_CC_PCR_Event, "TPM2_PCR_Event",
     (MarshalFunction_t)TSS_PCR_Event_In_Marshal,
     (UnmarshalFunction_t)TSS_PCR_Event_Out_Unmarshal,
     (UnmarshalInFunction_t)PCR_Event_In_Unmarshal},

    {TPM_CC_PCR_Read, "TPM2_PCR_Read",
     (MarshalFunction_t)TSS_PCR_Read_In_Marshal,
     (UnmarshalFunction_t)TSS_PCR_Read_Out_Unmarshal,
     (UnmarshalInFunction_t)PCR_Read_In_Unmarshal},

    {TPM_CC_PCR_Allocate, "TPM2_PCR_Allocate",
     (MarshalFunction_t)TSS_PCR_Allocate_In_Marshal,
     (UnmarshalFunction_t)TSS_PCR_Allocate_Out_Unmarshal,
     (UnmarshalInFunction_t)PCR_Allocate_In_Unmarshal},

    {TPM_CC_PCR_SetAuthPolicy, "TPM2_PCR_SetAuthPolicy",
     (MarshalFunction_t)TSS_PCR_SetAuthPolicy_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PCR_SetAuthPolicy_In_Unmarshal},

    {TPM_CC_PCR_SetAuthValue, "TPM2_PCR_SetAuthValue",
     (MarshalFunction_t)TSS_PCR_SetAuthValue_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PCR_SetAuthValue_In_Unmarshal},

    {TPM_CC_PCR_Reset, "TPM2_PCR_Reset",
     (MarshalFunction_t)TSS_PCR_Reset_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PCR_Reset_In_Unmarshal},

    {TPM_CC_PolicySigned, "TPM2_PolicySigned",
     (MarshalFunction_t)TSS_PolicySigned_In_Marshal,
     (UnmarshalFunction_t)TSS_PolicySigned_Out_Unmarshal,
     (UnmarshalInFunction_t)PolicySigned_In_Unmarshal},

    {TPM_CC_PolicySecret, "TPM2_PolicySecret",
     (MarshalFunction_t)TSS_PolicySecret_In_Marshal,
     (UnmarshalFunction_t)TSS_PolicySecret_Out_Unmarshal,
     (UnmarshalInFunction_t)PolicySecret_In_Unmarshal},

    {TPM_CC_PolicyTicket, "TPM2_PolicyTicket",
     (MarshalFunction_t)TSS_PolicyTicket_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyTicket_In_Unmarshal},

    {TPM_CC_PolicyOR, "TPM2_PolicyOR",
     (MarshalFunction_t)TSS_PolicyOR_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyOR_In_Unmarshal},

    {TPM_CC_PolicyPCR, "TPM2_PolicyPCR",
     (MarshalFunction_t)TSS_PolicyPCR_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyPCR_In_Unmarshal},

    {TPM_CC_PolicyLocality, "TPM2_PolicyLocality",
     (MarshalFunction_t)TSS_PolicyLocality_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyLocality_In_Unmarshal},

    {TPM_CC_PolicyNV, "TPM2_PolicyNV",
     (MarshalFunction_t)TSS_PolicyNV_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyNV_In_Unmarshal},

    {TPM_CC_PolicyAuthorizeNV, "TPM2_PolicyAuthorizeNV",
     (MarshalFunction_t)TSS_PolicyAuthorizeNV_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyAuthorizeNV_In_Unmarshal},

    {TPM_CC_PolicyCounterTimer, "TPM2_PolicyCounterTimer",
     (MarshalFunction_t)TSS_PolicyCounterTimer_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyCounterTimer_In_Unmarshal},

    {TPM_CC_PolicyCommandCode, "TPM2_PolicyCommandCode",
     (MarshalFunction_t)TSS_PolicyCommandCode_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyCommandCode_In_Unmarshal},

    {TPM_CC_PolicyPhysicalPresence, "TPM2_PolicyPhysicalPresence",
     (MarshalFunction_t)TSS_PolicyPhysicalPresence_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyPhysicalPresence_In_Unmarshal},

    {TPM_CC_PolicyCpHash, "TPM2_PolicyCpHash",
     (MarshalFunction_t)TSS_PolicyCpHash_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyCpHash_In_Unmarshal},

    {TPM_CC_PolicyNameHash, "TPM2_PolicyNameHash",
     (MarshalFunction_t)TSS_PolicyNameHash_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyNameHash_In_Unmarshal},

    {TPM_CC_PolicyDuplicationSelect, "TPM2_PolicyDuplicationSelect",
     (MarshalFunction_t)TSS_PolicyDuplicationSelect_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyDuplicationSelect_In_Unmarshal},

    {TPM_CC_PolicyAuthorize, "TPM2_PolicyAuthorize",
     (MarshalFunction_t)TSS_PolicyAuthorize_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyAuthorize_In_Unmarshal},

    {TPM_CC_PolicyAuthValue, "TPM2_PolicyAuthValue",
     (MarshalFunction_t)TSS_PolicyAuthValue_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyAuthValue_In_Unmarshal},

    {TPM_CC_PolicyPassword, "TPM2_PolicyPassword",
     (MarshalFunction_t)TSS_PolicyPassword_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyPassword_In_Unmarshal},

    {TPM_CC_PolicyGetDigest, "TPM2_PolicyGetDigest",
     (MarshalFunction_t)TSS_PolicyGetDigest_In_Marshal,
     (UnmarshalFunction_t)TSS_PolicyGetDigest_Out_Unmarshal,
     (UnmarshalInFunction_t)PolicyGetDigest_In_Unmarshal},

    {TPM_CC_PolicyNvWritten, "TPM2_PolicyNvWritten",
     (MarshalFunction_t)TSS_PolicyNvWritten_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyNvWritten_In_Unmarshal},

    {TPM_CC_PolicyTemplate, "TPM2_PolicyTemplate",
     (MarshalFunction_t)TSS_PolicyTemplate_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PolicyTemplate_In_Unmarshal},

    {TPM_CC_CreatePrimary, "TPM2_CreatePrimary",
     (MarshalFunction_t)TSS_CreatePrimary_In_Marshal,
     (UnmarshalFunction_t)TSS_CreatePrimary_Out_Unmarshal,
     (UnmarshalInFunction_t)CreatePrimary_In_Unmarshal},

    {TPM_CC_HierarchyControl, "TPM2_HierarchyControl",
     (MarshalFunction_t)TSS_HierarchyControl_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)HierarchyControl_In_Unmarshal},

    {TPM_CC_SetPrimaryPolicy, "TPM2_SetPrimaryPolicy",
     (MarshalFunction_t)TSS_SetPrimaryPolicy_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)SetPrimaryPolicy_In_Unmarshal},

    {TPM_CC_ChangePPS, "TPM2_ChangePPS",
     (MarshalFunction_t)TSS_ChangePPS_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)ChangePPS_In_Unmarshal},

    {TPM_CC_ChangeEPS, "TPM2_ChangeEPS",
     (MarshalFunction_t)TSS_ChangeEPS_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)ChangeEPS_In_Unmarshal},

    {TPM_CC_Clear, "TPM2_Clear",
     (MarshalFunction_t)TSS_Clear_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)Clear_In_Unmarshal},

    {TPM_CC_ClearControl, "TPM2_ClearControl",
     (MarshalFunction_t)TSS_ClearControl_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)ClearControl_In_Unmarshal},

    {TPM_CC_HierarchyChangeAuth, "TPM2_HierarchyChangeAuth",
     (MarshalFunction_t)TSS_HierarchyChangeAuth_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)HierarchyChangeAuth_In_Unmarshal},

    {TPM_CC_DictionaryAttackLockReset, "TPM2_DictionaryAttackLockReset",
     (MarshalFunction_t)TSS_DictionaryAttackLockReset_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)DictionaryAttackLockReset_In_Unmarshal},

    {TPM_CC_DictionaryAttackParameters, "TPM2_DictionaryAttackParameters",
     (MarshalFunction_t)TSS_DictionaryAttackParameters_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)DictionaryAttackParameters_In_Unmarshal},

    {TPM_CC_PP_Commands, "TPM2_PP_Commands",
     (MarshalFunction_t)TSS_PP_Commands_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)PP_Commands_In_Unmarshal},

    {TPM_CC_SetAlgorithmSet, "TPM2_SetAlgorithmSet",
     (MarshalFunction_t)TSS_SetAlgorithmSet_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)SetAlgorithmSet_In_Unmarshal},

    {TPM_CC_ContextSave, "TPM2_ContextSave",
     (MarshalFunction_t)TSS_ContextSave_In_Marshal,
     (UnmarshalFunction_t)TSS_ContextSave_Out_Unmarshal,
     (UnmarshalInFunction_t)ContextSave_In_Unmarshal},

    {TPM_CC_ContextLoad, "TPM2_ContextLoad",
     (MarshalFunction_t)TSS_ContextLoad_In_Marshal,
     (UnmarshalFunction_t)TSS_ContextLoad_Out_Unmarshal,
     (UnmarshalInFunction_t)ContextLoad_In_Unmarshal},

    {TPM_CC_FlushContext, "TPM2_FlushContext",
     (MarshalFunction_t)TSS_FlushContext_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)FlushContext_In_Unmarshal},

    {TPM_CC_EvictControl, "TPM2_EvictControl",
     (MarshalFunction_t)TSS_EvictControl_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)EvictControl_In_Unmarshal},

    {TPM_CC_ReadClock, "TPM2_ReadClock",
     NULL,
     (UnmarshalFunction_t)TSS_ReadClock_Out_Unmarshal,
     NULL},

    {TPM_CC_ClockSet, "TPM2_ClockSet",
     (MarshalFunction_t)TSS_ClockSet_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)ClockSet_In_Unmarshal},

    {TPM_CC_ClockRateAdjust, "TPM2_ClockRateAdjust",
     (MarshalFunction_t)TSS_ClockRateAdjust_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)ClockRateAdjust_In_Unmarshal},
    
    {TPM_CC_GetCapability, "TPM2_GetCapability",
     (MarshalFunction_t)TSS_GetCapability_In_Marshal,
     (UnmarshalFunction_t)TSS_GetCapability_Out_Unmarshal,
     (UnmarshalInFunction_t)GetCapability_In_Unmarshal},
    
    {TPM_CC_TestParms, "TPM2_TestParms",
     (MarshalFunction_t)TSS_TestParms_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)TestParms_In_Unmarshal},

    {TPM_CC_NV_DefineSpace, "TPM2_NV_DefineSpace",
     (MarshalFunction_t)TSS_NV_DefineSpace_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_DefineSpace_In_Unmarshal},

    {TPM_CC_NV_UndefineSpace, "TPM2_NV_UndefineSpace",
     (MarshalFunction_t)TSS_NV_UndefineSpace_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_UndefineSpace_In_Unmarshal},

    {TPM_CC_NV_UndefineSpaceSpecial, "TPM2_NV_UndefineSpaceSpecial",
     (MarshalFunction_t)TSS_NV_UndefineSpaceSpecial_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_UndefineSpaceSpecial_In_Unmarshal},

    {TPM_CC_NV_ReadPublic, "TPM2_NV_ReadPublic",
     (MarshalFunction_t)TSS_NV_ReadPublic_In_Marshal,
     (UnmarshalFunction_t)TSS_NV_ReadPublic_Out_Unmarshal,
     (UnmarshalInFunction_t)NV_ReadPublic_In_Unmarshal},

    {TPM_CC_NV_Write, "TPM2_NV_Write",
     (MarshalFunction_t)TSS_NV_Write_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_Write_In_Unmarshal},

    {TPM_CC_NV_Increment, "TPM2_NV_Increment",
     (MarshalFunction_t)TSS_NV_Increment_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_Increment_In_Unmarshal},

    {TPM_CC_NV_Extend, "TPM2_NV_Extend",
     (MarshalFunction_t)TSS_NV_Extend_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_Extend_In_Unmarshal},

    {TPM_CC_NV_SetBits, "TPM2_NV_SetBits",
     (MarshalFunction_t)TSS_NV_SetBits_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_SetBits_In_Unmarshal},

    {TPM_CC_NV_WriteLock, "TPM2_NV_WriteLock",
     (MarshalFunction_t)TSS_NV_WriteLock_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_WriteLock_In_Unmarshal},

    {TPM_CC_NV_GlobalWriteLock, "TPM2_NV_GlobalWriteLock",
     (MarshalFunction_t)TSS_NV_GlobalWriteLock_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_GlobalWriteLock_In_Unmarshal},

    {TPM_CC_NV_Read, "TPM2_NV_Read",
     (MarshalFunction_t)TSS_NV_Read_In_Marshal,
     (UnmarshalFunction_t)TSS_NV_Read_Out_Unmarshal,
     (UnmarshalInFunction_t)NV_Read_In_Unmarshal},

    {TPM_CC_NV_ReadLock, "TPM2_NV_ReadLock",
     (MarshalFunction_t)TSS_NV_ReadLock_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_ReadLock_In_Unmarshal},

    {TPM_CC_NV_ChangeAuth, "TPM2_NV_ChangeAuth",
     (MarshalFunction_t)TSS_NV_ChangeAuth_In_Marshal,
     NULL,
     (UnmarshalInFunction_t)NV_ChangeAuth_In_Unmarshal},

    {TPM_CC_NV_Certify, "TPM2_NV_Certify",
     (MarshalFunction_t)TSS_NV_Certify_In_Marshal,
     (UnmarshalFunction_t)TSS_NV_Certify_Out_Unmarshal,
     (UnmarshalInFunction_t)NV_Certify_In_Unmarshal}

};

/* The context for the entire command processor.  Update TSS_InitAuthContext() when changing
   this structure */

struct TSS_AUTH_CONTEXT {
    uint8_t 		commandBuffer [MAX_COMMAND_SIZE];
    uint8_t 		responseBuffer [MAX_RESPONSE_SIZE];
    const char 		*commandText;
    COMMAND_INDEX    	tpmCommandIndex;	/* index into attributes table */
    TPM_CC 		commandCode;
    TPM_RC 		responseCode;
    uint32_t 		commandHandleCount;
    uint32_t 		responseHandleCount;
    uint16_t		authCount;		/* authorizations in command */
    uint16_t 		commandSize;
    uint32_t 		cpBufferSize;
    uint8_t 		*cpBuffer;
    uint32_t 		responseSize;
    MarshalFunction_t 	marshalFunction;
    UnmarshalFunction_t unmarshalFunction;
    UnmarshalInFunction_t unmarshalInFunction;
} ;


static TPM_RC TSS_MarshalTable_Process(TSS_AUTH_CONTEXT *tssAuthContext,
				       TPM_CC commandCode)
{
    TPM_RC rc = 0;
    size_t index;
    int found = FALSE;

    /* get the command index in the dispatch table */
    for (index = 0 ; index < (sizeof(marshalTable) / sizeof(MARSHAL_TABLE)) ; (index)++) {
	if (marshalTable[index].commandCode == commandCode) {
	    found = TRUE;
	    break;
	}
    }
    if (found) {
	tssAuthContext->commandCode = commandCode;
	tssAuthContext->commandText = marshalTable[index].commandText;
	tssAuthContext->marshalFunction = marshalTable[index].marshalFunction;
	tssAuthContext->unmarshalFunction = marshalTable[index].unmarshalFunction;
	tssAuthContext->unmarshalInFunction = marshalTable[index].unmarshalInFunction;
    }
    else {
	printf("TSS_MarshalTable_Process: commandCode %08x not found\n", commandCode);
	rc = TSS_RC_COMMAND_UNIMPLEMENTED;
    }
    return rc;
}

TPM_RC TSS_AuthCreate(TSS_AUTH_CONTEXT **tssAuthContext)
{
    TPM_RC rc = 0;
    if (rc == 0) {
	*tssAuthContext = malloc(sizeof(TSS_AUTH_CONTEXT));
	if (*tssAuthContext == NULL) {
	    if (tssVerbose) printf("TSS_AuthCreate: malloc %u failed\n",
				   (unsigned int)sizeof(TSS_AUTH_CONTEXT));
	    rc = TSS_RC_OUT_OF_MEMORY;
	}
    }
    if (rc == 0) {
	TSS_InitAuthContext(*tssAuthContext);
    }
    return rc;
}

void TSS_InitAuthContext(TSS_AUTH_CONTEXT *tssAuthContext)
{
    memset(tssAuthContext->commandBuffer, 0, MAX_COMMAND_SIZE);
    memset(tssAuthContext->responseBuffer, 0, MAX_RESPONSE_SIZE);
    tssAuthContext->commandText = NULL;
    tssAuthContext->commandCode = 0;
    tssAuthContext->responseCode = 0;
    tssAuthContext->commandHandleCount = 0;
    tssAuthContext->responseHandleCount = 0;
    tssAuthContext->authCount = 0;
    tssAuthContext->commandSize = 0;
    tssAuthContext->cpBufferSize = 0;
    tssAuthContext->cpBuffer = NULL;
    tssAuthContext->responseSize = 0;
    tssAuthContext->marshalFunction = NULL;
    tssAuthContext->unmarshalFunction = NULL;
}

TPM_RC TSS_AuthDelete(TSS_AUTH_CONTEXT *tssAuthContext)
{
    if (tssAuthContext != NULL) {
	TSS_InitAuthContext(tssAuthContext);
	free(tssAuthContext);
    }
    return 0;
}

/* TSS_Marshal() marshals the in parameters into the TSS context.

   It also sets other member of the context in preparation for the rest of the sequence.  
*/

TPM_RC TSS_Marshal(TSS_AUTH_CONTEXT *tssAuthContext,
		   COMMAND_PARAMETERS *in,
		   TPM_CC commandCode)
{
    TPM_RC 		rc = 0;
    TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;	/* default until sessions are added */
    uint8_t 		*buffer;			/* for marshaling */
    uint8_t 		*bufferu;			/* for test unmarshaling */
    INT32 		size;
    
    TSS_InitAuthContext(tssAuthContext);
    /* index from command code to table and save items for this command */
    if (rc == 0) {
	rc = TSS_MarshalTable_Process(tssAuthContext, commandCode);
    }
    /* get the number of command and response handles from the TPM table */
    if (rc == 0) {
	tssAuthContext->tpmCommandIndex = CommandCodeToCommandIndex(commandCode);
	if (tssAuthContext->tpmCommandIndex == UNIMPLEMENTED_COMMAND_INDEX) {
	    printf("TSS_Marshal: commandCode %08x not found\n", commandCode);
	    rc = TSS_RC_COMMAND_UNIMPLEMENTED;
	}
    }
    if (rc == 0) {
#if 0
	tssAuthContext->commandHandleCount = s_ccAttr[tssAuthContext->tpmCommandIndex].cHandles;
	tssAuthContext->responseHandleCount = s_ccAttr[tssAuthContext->tpmCommandIndex].rHandle;
#endif
	tssAuthContext->commandHandleCount =
	    getCommandHandleCount(tssAuthContext->tpmCommandIndex);
	tssAuthContext->responseHandleCount =
	    getresponseHandleCount(tssAuthContext->tpmCommandIndex);
    }
    if (rc == 0) {
	/* make a copy of the command buffer and size since the marshal functions move them */
	buffer = tssAuthContext->commandBuffer;
	size = MAX_COMMAND_SIZE;
	/* marshal header, preliminary tag and command size */
	rc = TSS_TPMI_ST_COMMAND_TAG_Marshal(&tag, &tssAuthContext->commandSize, &buffer, &size);
    }
    if (rc == 0) {
	uint32_t commandSize = tssAuthContext->commandSize;
	rc = TSS_UINT32_Marshal(&commandSize, &tssAuthContext->commandSize, &buffer, &size);
    }
    if (rc == 0) {
	rc = TSS_TPM_CC_Marshal(&commandCode, &tssAuthContext->commandSize, &buffer, &size);
    }    
    if (rc == 0) {
	/* save pointer to marshaled data for test unmarshal */
	bufferu = buffer +
		  tssAuthContext->commandHandleCount * sizeof(TPM_HANDLE);
	/* if there is a marshal function */
	if (tssAuthContext->marshalFunction != NULL) {
	    /* if there is a structure to marshal */
	    if (in != NULL) {
		rc = tssAuthContext->marshalFunction(in, &tssAuthContext->commandSize,
						     &buffer, &size);
	    }
	    /* caller error, no structure supplied to marshal */
	    else {
		if (tssVerbose)
		    printf("TSS_Marshal: Command %08x requires command parameter structure\n",
			   commandCode);
		rc = TSS_RC_IN_PARAMETER;	
	    }
	}
	/* if there is no marshal function */
	else {
	    /* caller error, supplied structure but there is no marshal function */
	    if (in != NULL) {
		if (tssVerbose)
		    printf("TSS_Marshal: Command %08x does not take command parameter structure\n",
			   commandCode);
		rc = TSS_RC_IN_PARAMETER;	
	    }
	    /* no marshal function and no command parameter structure is OK */
	}
    }
    /* unmarshal to validate the input parameters */
    if ((rc == 0) && (tssAuthContext->unmarshalInFunction != NULL)) {
	COMMAND_PARAMETERS target;
	TPM_HANDLE 	handles[MAX_HANDLE_NUM];
	size = MAX_COMMAND_SIZE;
	rc = tssAuthContext->unmarshalInFunction(&target, &bufferu, &size, handles);
	if ((rc != 0) && tssVerbose) {
	    printf("TSS_Marshal: Invalid command parameter\n");
	}
    }
    /* back fill the correct commandSize */
    if (rc == 0) {
	uint16_t written;		/* dummy */
	uint32_t commandSize = tssAuthContext->commandSize;
	buffer = tssAuthContext->commandBuffer + sizeof(TPMI_ST_COMMAND_TAG);
	TSS_UINT32_Marshal(&commandSize, &written, &buffer, NULL);
    }
    /* record the interim cpBuffer and cpBufferSize before adding authorizations */
    if (rc == 0) {
	uint32_t notCpBufferSize;
	
	/* cpBuffer does not include the header and handles */
	notCpBufferSize = sizeof(TPMI_ST_COMMAND_TAG) + sizeof (uint32_t) + sizeof(TPM_CC) +
			  (sizeof(TPM_HANDLE) * tssAuthContext->commandHandleCount);

	tssAuthContext->cpBuffer = tssAuthContext->commandBuffer + notCpBufferSize;
	tssAuthContext->cpBufferSize = tssAuthContext->commandSize - notCpBufferSize;
    }
    return rc;
}

/* TSS_Unmarshal() unmarshals the response parameter.

   It returns an error if either there is no unmarshal function and out is not NULL or if there is
   an unmarshal function and out is not NULL.

   If there is no unmarshal function and out is NULL, the function is a noop.
*/

TPM_RC TSS_Unmarshal(TSS_AUTH_CONTEXT *tssAuthContext,
		     RESPONSE_PARAMETERS *out)
{
    TPM_RC 	rc = 0;
    TPM_ST 	tag;
    uint8_t 	*buffer;    
    INT32 	size;

    /* if there is an unmarshal function */
    if (tssAuthContext->unmarshalFunction != NULL) {
	/* if there is a structure to unmarshal */
	if (out != NULL) {
	    if (rc == 0) {
		/* get the response tag, determines whether there is a response parameterSize to
		   unmarshal */
		buffer = tssAuthContext->responseBuffer;
		size = tssAuthContext->responseSize;
		rc = TPM_ST_Unmarshal(&tag, &buffer, &size);
	    }
	    if (rc == 0) {
		/* move the buffer and size past the header */
		buffer = tssAuthContext->responseBuffer +
			 sizeof(TPM_ST) + sizeof(uint32_t) + sizeof(TPM_RC);
		size = tssAuthContext->responseSize -
		       (sizeof(TPM_ST) + sizeof(uint32_t) + sizeof(TPM_RC));
		rc = tssAuthContext->unmarshalFunction(out, tag, &buffer, &size);
	    }
	}
	/* caller error, no structure supplied to unmarshal */
	else {
	    if (tssVerbose)
		printf("TSS_Unmarshal: Command %08x requires response parameter structure\n",
		       tssAuthContext->commandCode);
	    rc = TSS_RC_OUT_PARAMETER;
	}
    }
    /* if there is no unmarshal function */
    else {
	/* caller error, structure supplied but no unmarshal function */
	if (out != NULL) {
	    if (tssVerbose)
		printf("TSS_Unmarshal: Command %08x does not take response parameter structure\n",
		       tssAuthContext->commandCode);
	    rc = TSS_RC_OUT_PARAMETER;
	}
	/* no unmarshal function and no response parameter structure is OK */
    }
    return rc;
}

/* TSS_SetCmdAuths() adds a list of TPMS_AUTH_COMMAND structures to the command buffer.

   The arguments are a NULL terminated list of TPMS_AUTH_COMMAND * structures.
 */

TPM_RC TSS_SetCmdAuths(TSS_AUTH_CONTEXT *tssAuthContext, ...)
{
    TPM_RC 		rc = 0;
    va_list		ap;
    uint16_t 		authorizationSize;	/* does not include 4 bytes of size */   
    TPMS_AUTH_COMMAND 	*authCommand = NULL;
    int 		done;
    uint32_t 		cpBufferSize;
    uint8_t 		*cpBuffer;
    uint8_t 		*buffer;

    /* calculate size of authorization area */
    done = FALSE;
    authorizationSize = 0;
    va_start(ap, tssAuthContext);
    while ((rc == 0) && !done){
	authCommand = va_arg(ap, TPMS_AUTH_COMMAND *);
	if (authCommand != NULL) {
	    rc = TSS_TPMS_AUTH_COMMAND_Marshal(authCommand, &authorizationSize, NULL, NULL);
	}
	else {
	    done = TRUE;
	}
    }
    va_end(ap);
    /* command called with authorizations */
    if (authorizationSize != 0) {
	/* back fill the tag TPM_ST_SESSIONS */
	if (rc == 0) {
	    uint16_t written = 0;		/* dummy */
	    TPMI_ST_COMMAND_TAG tag = TPM_ST_SESSIONS;
	    buffer = tssAuthContext->commandBuffer;
	    TSS_TPMI_ST_COMMAND_TAG_Marshal(&tag, &written, &buffer, NULL);
	}
	/* get cpBuffer, command parameters */
	if (rc == 0) {
	    rc = TSS_GetCpBuffer(tssAuthContext, &cpBufferSize, &cpBuffer);
	}
	/* new authorization area range check, will cpBuffer move overflow */
	if (cpBuffer +
	    cpBufferSize +
	    sizeof (uint32_t) +		/* authorizationSize */
	    authorizationSize		/* authorization area */
	    > tssAuthContext->commandBuffer + MAX_COMMAND_SIZE) {
	
	    if (tssVerbose)
		printf("TSS_SetCmdAuths: Command authorizations overflow command buffer\n");
	    rc = TSS_RC_INSUFFICIENT_BUFFER;
	}	
	/* move the cpBuffer to make space for the authorization area and its size */
	if (rc == 0) {
	    memmove(cpBuffer + sizeof (uint32_t) + authorizationSize,	/* to here */
		    cpBuffer,						/* from here */
		    cpBufferSize);
	}
	/* marshal the authorizationSize area, where cpBuffer was before move */
	if (rc == 0) {
	    uint32_t authorizationSize32 = authorizationSize;
	    uint16_t written;		/* dummy */
	    TSS_UINT32_Marshal(&authorizationSize32, &written, &cpBuffer, NULL);
	}
	/* marshal the command authorization areas */
	done = FALSE;
	authorizationSize = 0;
	va_start(ap, tssAuthContext);
	while ((rc == 0) && !done){
	    authCommand = va_arg(ap, TPMS_AUTH_COMMAND *);
	    if (authCommand != NULL) {
		rc = TSS_TPMS_AUTH_COMMAND_Marshal(authCommand, &authorizationSize, &cpBuffer, NULL);
		tssAuthContext->authCount++; /* count the number of authorizations for the response */
	    }
	    else {
		done = TRUE;
	    }
	}
	va_end(ap);
	if (rc == 0) {
	    uint16_t written;		/* dummy */
	    uint32_t commandSize;
	    /* mark cpBuffer new location, size doesn't change */
	    tssAuthContext->cpBuffer += sizeof (uint32_t) + authorizationSize;
	    /* record command stream used size */
	    tssAuthContext->commandSize += sizeof (uint32_t) + authorizationSize;
	    /* back fill the correct commandSize */
	    buffer = tssAuthContext->commandBuffer + sizeof(TPMI_ST_COMMAND_TAG);
	    commandSize = tssAuthContext->commandSize;
	    TSS_UINT32_Marshal(&commandSize, &written, &buffer, NULL);
	}
    }
    return rc;
}

/* TSS_GetRspAuths() unmarshals a response buffer into a NULL terminated list of TPMS_AUTH_RESPONSE
   structures.  This should not be called if the TPM returned a non-success response code.

   Returns an error if the number of response auths requested is not equal to the number of command
   auths, including zero.

   If the response tag is not TPM_ST_SESSIONS, the function is a noop (except for error checking).
 */

TPM_RC TSS_GetRspAuths(TSS_AUTH_CONTEXT *tssAuthContext, ...)
{
    TPM_RC 	rc = 0;
    va_list	ap;
    TPMS_AUTH_RESPONSE 	*authResponse = NULL;
    INT32 	size;
    uint8_t 	*buffer;
    TPM_ST 	tag;
    int 	done;
    uint16_t	authCount = 0;		/* authorizations in response */
    uint32_t 	parameterSize;
    
    /* unmarshal the response tag */
    if (rc == 0) {
	size = tssAuthContext->responseSize;
  	buffer = tssAuthContext->responseBuffer;
	rc = TPM_ST_Unmarshal(&tag, &buffer, &size);
    }
    /* check that the tag indicates that there are sessions */
    if (tag == TPM_ST_SESSIONS) {
	/* offset the buffer past the header and handles, and get the response parameterSize */
	if (rc == 0) {
	    uint32_t offsetSize = sizeof(TPM_ST) +  + sizeof (uint32_t) + sizeof(TPM_RC) +
				  (sizeof(TPM_HANDLE) * tssAuthContext->responseHandleCount);
	    buffer = tssAuthContext->responseBuffer + offsetSize;
	    size = tssAuthContext->responseSize - offsetSize;
	    rc = UINT32_Unmarshal(&parameterSize, &buffer, &size);
	}
	if (rc == 0) {
	    /* index past the response parameters to the authorization area */
	    buffer += parameterSize;
	    size -= parameterSize;
	}
	/* unmarshal the response authorization area */
	done = FALSE;
	va_start(ap, tssAuthContext);
	while ((rc == 0) && !done){
	    authResponse = va_arg(ap, TPMS_AUTH_RESPONSE *);
	    if (authResponse != NULL) {
		rc = TPMS_AUTH_RESPONSE_Unmarshal(authResponse, &buffer, &size);
		authCount++;
	    }
	    else {
		done = TRUE;
	    }
	}
	va_end(ap);
	/* check for extra bytes at the end of the response */
	if (rc == 0) {
	    if (size != 0) {
		if (tssVerbose)
		    printf("TSS_GetRspAuths: Extra bytes at the end of response authorizations\n");
		rc = TSS_RC_MALFORMED_RESPONSE;
	    }
	}
    }
    /* check that the same number was requested as were sent in the command.  Check for zero if not
       TPM_ST_SESSIONS */
    if (rc == 0) {
	if (tssAuthContext->authCount != authCount) {
	    if (tssVerbose)
		printf("TSS_GetRspAuths: "
		       "Command authorizations requested not equal number in command\n");
	    rc = TSS_RC_MALFORMED_RESPONSE;
	}
    }
    return rc;
}

TPM_CC TSS_GetCommandCode(TSS_AUTH_CONTEXT *tssAuthContext)
{
    TPM_CC commandCode = tssAuthContext->commandCode;
    return commandCode;
}

TPM_RC TSS_GetCpBuffer(TSS_AUTH_CONTEXT *tssAuthContext,
		       uint32_t *cpBufferSize,
		       uint8_t **cpBuffer)
{
    *cpBufferSize = tssAuthContext->cpBufferSize;
    *cpBuffer = tssAuthContext->cpBuffer;
    return 0;
}

/* TSS_GetCommandDecryptParam() returns the size and pointer to the first marshaled TPM2B */

TPM_RC TSS_GetCommandDecryptParam(TSS_AUTH_CONTEXT *tssAuthContext,
				  uint32_t *decryptParamSize,
				  uint8_t **decryptParamBuffer)
{
    TPM_RC 	rc = 0;
    /* the first parameter is the TPM2B */
    uint32_t cpBufferSize;
    uint8_t *cpBuffer;

    if (rc == 0) {
	rc = TSS_GetCpBuffer(tssAuthContext, &cpBufferSize, &cpBuffer);
    }
    /* FIXME range checks */
    /* extract contents of the first TPM2B */
    if (rc == 0) {
	*decryptParamSize = ntohs(*(uint16_t *)cpBuffer);
	*decryptParamBuffer = cpBuffer + sizeof(uint16_t);
    }
    return rc;
}

TPM_RC TSS_SetCommandDecryptParam(TSS_AUTH_CONTEXT *tssAuthContext,
				  uint32_t encryptParamSize,
				  uint8_t *encryptParamBuffer)
{
    TPM_RC 	rc = 0;
    /* the first parameter is the TPM2B */
    uint32_t decryptParamSize;
    uint8_t *decryptParamBuffer;

    if (rc == 0) {
	rc = TSS_GetCommandDecryptParam(tssAuthContext,
					&decryptParamSize,
					&decryptParamBuffer);
    }
    /* the encrypt data overwrites the already marshaled data */
    if (rc == 0) {
	if (decryptParamSize != encryptParamSize) {
	    if (tssVerbose)
		printf("TSS_SetCommandDecryptParam: Different encrypt and decrypt size\n");
	    rc = TSS_RC_BAD_ENCRYPT_SIZE;
	}
    }
    /* skip the 2B size, copy the data */
    if (rc == 0) {
	memcpy(decryptParamBuffer, encryptParamBuffer, encryptParamSize);
    }
    return rc;
}

/* TSS_GetCommandHandleCount() returns the number of handles in the command area */

TPM_RC TSS_GetCommandHandleCount(TSS_AUTH_CONTEXT *tssAuthContext,
				 uint32_t *commandHandleCount)
{
    *commandHandleCount = tssAuthContext->commandHandleCount;
    return 0;
}

/* TSS_GetAuthRole() returns AUTH_NONE if the handle in the handle area cannot be an authorization
   handle. */

AUTH_ROLE TSS_GetAuthRole(TSS_AUTH_CONTEXT *tssAuthContext,
			  uint32_t handleIndex)
{
    AUTH_ROLE authRole;
    authRole = getCommandAuthRole(tssAuthContext->tpmCommandIndex, handleIndex);
    return authRole;
}

/* TSS_GetCommandHandle() gets the command handle at the index.  Index is a zero based count, not a
   byte count.

   Returns 0 if the index exceeds the number of handles.
*/

TPM_RC TSS_GetCommandHandle(TSS_AUTH_CONTEXT *tssAuthContext,
			    TPM_HANDLE *commandHandle,
			    uint32_t index)
{
    TPM_RC 	rc = 0;
    uint8_t 	*buffer;
    INT32 	size;
   
    
    if (rc == 0) {
	if (index >= tssAuthContext->commandHandleCount) {
	    if (tssVerbose) printf("TSS_GetCommandHandle: index %u too large for command\n", index);
	    rc = TSS_RC_BAD_HANDLE_NUMBER;
	}
    }
    if (rc == 0) {
	/* index into the command handle */
	buffer = tssAuthContext->commandBuffer +
		 sizeof(TPMI_ST_COMMAND_TAG) + sizeof (uint32_t) + sizeof(TPM_CC) +
		 (sizeof(TPM_HANDLE) * index);
	size = sizeof(TPM_HANDLE);
	rc = TPM_HANDLE_Unmarshal(commandHandle, &buffer, &size);
    }
    return rc;
}
    
/* TSS_GetRpBuffer() returns a pointer to the response parameter area.

   FIXME missing range checks all over

   FIXME move to execute so it only has to be done once.
*/

TPM_RC TSS_GetRpBuffer(TSS_AUTH_CONTEXT *tssAuthContext,
		       uint32_t *rpBufferSize,
		       uint8_t **rpBuffer)
{
    TPM_RC 	rc = 0;
    TPM_ST 	tag;			/* response tag */
    uint32_t 	offsetSize;		/* to beginning of parameter area */
    INT32 	size;			/* tmp for unmarshal */
    uint8_t 	*buffer;		/* tmp for unmarshal */
    uint32_t 	parameterSize;		/* response parameter (if sessions) */
     
    /* unmarshal the response tag */
    if (rc == 0) {
	size = tssAuthContext->responseSize;
  	buffer = tssAuthContext->responseBuffer;
	rc = TPM_ST_Unmarshal(&tag, &buffer, &size);
    }
    if (rc == 0) {
	/* offset to parameterSize or parameters */
	offsetSize = sizeof(TPM_ST) +  + sizeof (uint32_t) + sizeof(TPM_RC) +
		     (sizeof(TPM_HANDLE) * tssAuthContext->responseHandleCount);
	
	/* no sessions -> no parameterSize */
	if (tag == TPM_ST_NO_SESSIONS) {
	    *rpBufferSize = tssAuthContext->responseSize - offsetSize;
	    *rpBuffer = tssAuthContext->responseBuffer + offsetSize;
	}
	/* sessions -> parameterSize */
	else {
	    if (rc == 0) {
		size = tssAuthContext->responseSize - offsetSize;
		buffer = tssAuthContext->responseBuffer + offsetSize;
		rc = UINT32_Unmarshal(&parameterSize, &buffer, &size);
	    }
	    /* FIXME need consistency check */
	    if (rc == 0) {
		offsetSize += sizeof(uint32_t);
		*rpBufferSize = parameterSize;
		*rpBuffer = tssAuthContext->responseBuffer + offsetSize;
	    }
	}
    }
    return rc;
}

/* TSS_GetResponseEncryptParam() returns the first TPM2B in the response area.

   The caller should ensure that the first response parameter is a TPM2B.
*/

TPM_RC TSS_GetResponseEncryptParam(TSS_AUTH_CONTEXT *tssAuthContext,
				   uint32_t *encryptParamSize,
				   uint8_t **encryptParamBuffer)
{
    TPM_RC 	rc = 0;
    /* the first parameter is the TPM2B */
    uint32_t rpBufferSize;
    uint8_t *rpBuffer;

    if (rc == 0) {
	rc = TSS_GetRpBuffer(tssAuthContext, &rpBufferSize, &rpBuffer);
    }
    /* FIXME range checks */
    /* extract contents of the first TPM2B */
    if (rc == 0) {
	*encryptParamSize = ntohs(*(uint16_t *)rpBuffer);
	*encryptParamBuffer = rpBuffer + sizeof(uint16_t);
    }
    return rc;
}

/* TSS_GetResponseEncryptParam() copies the decryptParamBuffer into the first TPM2B in the response
   area.

   The caller should ensure that the first response parameter is a TPM2B.
*/

TPM_RC TSS_SetResponseDecryptParam(TSS_AUTH_CONTEXT *tssAuthContext,
				   uint32_t decryptParamSize,
				   uint8_t *decryptParamBuffer)
{
    TPM_RC 	rc = 0;
    /* the first parameter is the TPM2B */
    uint32_t encryptParamSize;
    uint8_t *encryptParamBuffer;

    if (rc == 0) {
	rc = TSS_GetResponseEncryptParam(tssAuthContext,
					 &encryptParamSize,
					 &encryptParamBuffer);
    }
    /* the decrypt data overwrites the already marshaled data */
    if (rc == 0) {
	if (decryptParamSize != encryptParamSize) {
	    if (tssVerbose)
		printf("TSS_SetCommandDecryptParam: Different encrypt and decrypt size\n");
	    rc = TSS_RC_BAD_ENCRYPT_SIZE;
	}
    }
    /* skip the 2B size, copy the data */
    if (rc == 0) {
	memcpy(encryptParamBuffer, decryptParamBuffer, decryptParamSize);
    }
    return rc;
}

TPM_RC TSS_AuthExecute(TSS_CONTEXT *tssContext)
{
    TPM_RC rc = 0;
    if (tssVverbose) printf("TSS_AuthExecute: Executing %s\n", tssContext->tssAuthContext->commandText);
    /* transmit the command and receive the response.  Normally returns the TPM response code. */
    if (rc == 0) {
	rc = TSS_Transmit(tssContext,
			  tssContext->tssAuthContext->responseBuffer, &tssContext->tssAuthContext->responseSize,
			  tssContext->tssAuthContext->commandBuffer, tssContext->tssAuthContext->commandSize,
			  tssContext->tssAuthContext->commandText);
    }
    return rc;
}
