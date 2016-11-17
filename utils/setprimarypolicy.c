/********************************************************************************/
/*										*/
/*			    SetPrimaryPolicy 					*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*	      $Id: setprimarypolicy.c 778 2016-10-19 15:21:05Z kgoldman $	*/
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

/* 

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <tss2/tss.h>
#include <tss2/tssutils.h>
#include <tss2/tssresponsecode.h>

static void printUsage(void);

int verbose = FALSE;

int main(int argc, char *argv[])
{
    TPM_RC			rc = 0;
    int				i;    /* argc iterator */
    TSS_CONTEXT			*tssContext = NULL;
    SetPrimaryPolicy_In 	in;
    char 			authHandleChar = 'p';
    const char			*authPassword = NULL; 
    const char			*policyFilename = NULL;
    TPMI_SH_AUTH_SESSION    	sessionHandle0 = TPM_RS_PW;
    unsigned int		sessionAttributes0 = 0;
    TPMI_SH_AUTH_SESSION    	sessionHandle1 = TPM_RH_NULL;
    unsigned int		sessionAttributes1 = 0;
    TPMI_SH_AUTH_SESSION    	sessionHandle2 = TPM_RH_NULL;
    unsigned int		sessionAttributes2 = 0;

    TSS_SetProperty(NULL, TPM_TRACE_LEVEL, "1");

    in.hashAlg = TPM_ALG_NULL;	/* default */

    for (i=1 ; (i<argc) && (rc == 0) ; i++) {
	if (strcmp(argv[i],"-hi") == 0) {
	    i++;
	    if (i < argc) {
		authHandleChar = argv[i][0];
	    }
	    else {
		printf("Missing parameter for -hi\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-pwda") == 0) {
	    i++;
	    if (i < argc) {
		authPassword = argv[i];
	    }
	    else {
		printf("-pwda option needs a value\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-pol") == 0) {
	    i++;
	    if (i < argc) {
		policyFilename = argv[i];
	    }
	    else {
		printf("-pol option needs a value\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-halg") == 0) {
	    i++;
	    if (i < argc) {
		if (strcmp(argv[i],"sha256") == 0) {
		    in.hashAlg = TPM_ALG_SHA256;
		}
		else if (strcmp(argv[i],"sha1") == 0) {
		    in.hashAlg = TPM_ALG_SHA1;
		}
		else {
		    printf("Bad parameter for -halg\n");
		    printUsage();
		}
	    }
	    else {
		printf("-halg option needs a value\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-se0") == 0) {
	    i++;
	    if (i < argc) {
		sscanf(argv[i],"%x", &sessionHandle0);
	    }
	    else {
		printf("Missing parameter for -se0\n");
		printUsage();
	    }
	    i++;
	    if (i < argc) {
		sscanf(argv[i],"%x", &sessionAttributes0);
		if (sessionAttributes0 > 0xff) {
		    printf("Out of range session attributes for -se0\n");
		    printUsage();
		}
	    }
	    else {
		printf("Missing parameter for -se0\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-se1") == 0) {
	    i++;
	    if (i < argc) {
		sscanf(argv[i],"%x", &sessionHandle1);
	    }
	    else {
		printf("Missing parameter for -se1\n");
		printUsage();
	    }
	    i++;
	    if (i < argc) {
		sscanf(argv[i],"%x", &sessionAttributes1);
		if (sessionAttributes1 > 0xff) {
		    printf("Out of range session attributes for -se1\n");
		    printUsage();
		}
	    }
	    else {
		printf("Missing parameter for -se1\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-se2") == 0) {
	    i++;
	    if (i < argc) {
		sscanf(argv[i],"%x", &sessionHandle2);
	    }
	    else {
		printf("Missing parameter for -se2\n");
		printUsage();
	    }
	    i++;
	    if (i < argc) {
		sscanf(argv[i],"%x", &sessionAttributes2);
		if (sessionAttributes2 > 0xff) {
		    printf("Out of range session attributes for -se2\n");
		    printUsage();
		}
	    }
	    else {
		printf("Missing parameter for -se2\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-h") == 0) {
	    printUsage();
	}
	else if (strcmp(argv[i],"-v") == 0) {
	    verbose = TRUE;
	    TSS_SetProperty(NULL, TPM_TRACE_LEVEL, "2");
	}
	else {
	    printf("\n%s is not a valid option\n", argv[i]);
	    printUsage();
	}
    }
    if (policyFilename != NULL) {
	if (in.hashAlg == TPM_ALG_NULL) {
	    printf("-pol requires -halg\n");
	    printUsage();
	}
    }
    else {
	if (in.hashAlg != TPM_ALG_NULL) {
	    printf("-halg requires -pol\n");
	    printUsage();
	}
    }
    /* Table 50 - TPMI_RH_HIERARCHY primaryHandle */
    if (rc == 0) {
	if (authHandleChar == 'l') {
	    in.authHandle = TPM_RH_LOCKOUT;
	}
	else if (authHandleChar == 'e') {
	    in.authHandle = TPM_RH_ENDORSEMENT;
	}
	else if (authHandleChar == 'o') {
	    in.authHandle = TPM_RH_OWNER;
	}
	else if (authHandleChar == 'p') {
	    in.authHandle = TPM_RH_PLATFORM;
	}
	else {
	    printf("Missing or illegal -hi\n");
	    printUsage();
	}
    }
    /* authorization policy */
    if (policyFilename != NULL) {
	rc = TSS_File_Read2B(&in.authPolicy.b,
			     sizeof(TPMU_HA),
			     policyFilename);
    }
    else {
	in.authPolicy.t.size = 0;	/* default empty policy */
    }
    /* Start a TSS context */
    if (rc == 0) {
	rc = TSS_Create(&tssContext);
    }
    /* call TSS to execute the command */
    if (rc == 0) {
	rc = TSS_Execute(tssContext,
			 NULL,
			 (COMMAND_PARAMETERS *)&in,
			 NULL,
			 TPM_CC_SetPrimaryPolicy,
			 sessionHandle0, authPassword, sessionAttributes0,
			 sessionHandle1, NULL, sessionAttributes1,
			 sessionHandle2, NULL, sessionAttributes2,
			 TPM_RH_NULL, NULL, 0);
    }
    {
	TPM_RC rc1 = TSS_Delete(tssContext);
	if (rc == 0) {
	    rc = rc1;
	}
    }
    if (rc == 0) {
	if (verbose) printf("setprimarypolicy: success\n");
    }
    else {
	const char *msg;
	const char *submsg;
	const char *num;
	printf("setprimarypolicy: failed, rc %08x\n", rc);
	TSS_ResponseCode_toString(&msg, &submsg, &num, rc);
	printf("%s%s%s\n", msg, submsg, num);
	rc = EXIT_FAILURE;
    }
    return rc;
}

static void printUsage(void)
{
    printf("\n");
    printf("setprimarypolicy\n");
    printf("\n");
    printf("Runs TPM2_SetPrimaryPolicy\n");
    printf("\n");
    printf("\t[-hi authhandle hierarchy (l, e, o, p)(default platform)]\n");
    printf("\t[-pwda authorization password (default empty)]\n");
    printf("\t[-pol policy file (default empty policy)]\n");
    printf("\t[-halg [sha1, sha256] (default null)]\n");
    printf("\n");
    printf("\t-se[0-2] session handle (default PWAP)\n");
    printf("\t\t01 continue\n");
    printf("\t\t20 command decrypt\n");
    printf("\t\t40 response encrypt\n");
    exit(1);	
}
