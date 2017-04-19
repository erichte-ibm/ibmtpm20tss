/********************************************************************************/
/*										*/
/*			    PCR_Allocate	 				*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*	      $Id: pcrallocate.c 987 2017-04-17 18:27:09Z kgoldman $		*/
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
    PCR_Allocate_In 		in;
    PCR_Allocate_Out 		out;
    int 			sha256 = FALSE;
    int 			sha1 = FALSE;
    const char			*parentPassword = NULL; 
    TPMI_SH_AUTH_SESSION    	sessionHandle0 = TPM_RS_PW;
    unsigned int		sessionAttributes0 = 0;
    TPMI_SH_AUTH_SESSION    	sessionHandle1 = TPM_RH_NULL;
    unsigned int		sessionAttributes1 = 0;
    TPMI_SH_AUTH_SESSION    	sessionHandle2 = TPM_RH_NULL;
    unsigned int		sessionAttributes2 = 0;
   
    setvbuf(stdout, 0, _IONBF, 0);      /* output may be going through pipe to log file */
    TSS_SetProperty(NULL, TPM_TRACE_LEVEL, "1");

    /* command line argument defaults */
    for (i=1 ; (i<argc) && (rc == 0) ; i++) {
	if (strcmp(argv[i],"-pwdp") == 0) {
	    i++;
	    if (i < argc) {
		parentPassword = argv[i];
	    }
	    else {
		printf("-pwda option needs a value\n");
		printUsage();
	    }
	}
	else if (strcmp(argv[i],"-sha1") == 0) {
	    sha1 = TRUE;
	}
	else if (strcmp(argv[i],"-sha256") == 0) {
	    sha256 = TRUE;
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
    /* at least one bank must be selected */
    if (rc == 0) {
	if (!sha1 && !sha256) {
	    printf("Either -sha1 or -sha256 must be specified\n");
	    printUsage();
	}
    }
    if (rc == 0) {
	in.authHandle = TPM_RH_PLATFORM;
	in.pcrAllocation.count = 2;
	uint8_t sha1mask = 0x00;	/* default don't select the bank */
	uint8_t sha256mask = 0x00;
	if (sha1) {
	    sha1mask = 0xff;	/* select all SHA-1 PCRs */
	}
	if (sha256) {
	    sha256mask = 0xff;	/* select all SHA-256 PCRs */
	}
	/* SHA-1 bank */
	in.pcrAllocation.pcrSelections[0].hash = TPM_ALG_SHA1;
	in.pcrAllocation.pcrSelections[0].sizeofSelect = 3;
	in.pcrAllocation.pcrSelections[0].pcrSelect[0] = sha1mask;
	in.pcrAllocation.pcrSelections[0].pcrSelect[1] = sha1mask;
	in.pcrAllocation.pcrSelections[0].pcrSelect[2] = sha1mask;
	/* SHA-256 bank */
	in.pcrAllocation.pcrSelections[1].hash = TPM_ALG_SHA256;
	in.pcrAllocation.pcrSelections[1].sizeofSelect = 3;
	in.pcrAllocation.pcrSelections[1].pcrSelect[0] = sha256mask;
	in.pcrAllocation.pcrSelections[1].pcrSelect[1] = sha256mask;
	in.pcrAllocation.pcrSelections[1].pcrSelect[2] = sha256mask;
    }
    /* Start a TSS context */
    if (rc == 0) {
	rc = TSS_Create(&tssContext);
    }
    /* call TSS to execute the command */
    if (rc == 0) {
	rc = TSS_Execute(tssContext,
			 (RESPONSE_PARAMETERS *)&out,
			 (COMMAND_PARAMETERS *)&in,
			 NULL,
			 TPM_CC_PCR_Allocate,
			 sessionHandle0, parentPassword, sessionAttributes0,
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
	if (verbose) printf("pcrallocate: success\n");
    }
    else {
	const char *msg;
	const char *submsg;
	const char *num;
	printf("pcrallocate: failed, rc %08x\n", rc);
	TSS_ResponseCode_toString(&msg, &submsg, &num, rc);
	printf("%s%s%s\n", msg, submsg, num);
	rc = EXIT_FAILURE;
    }
    return rc;
}

static void printUsage(void)
{
    printf("\n");
    printf("pcrallocate\n");
    printf("\n");
    printf("Runs TPM2_PCR_Allocate\n");
    printf("\n");
    printf("\nAllocates SHA-1 and/or SHA-256 banks for a full set of PCR 0-23.  Not all\n"
	   "TPMs support two banks\n");
    printf("\n");
    printf("\t[-pwdp platform hierarchy password (default empty)]\n");
    printf("\t-sha1 allocate a SHA-1 bank\n");
    printf("\t-sha256 allocate a SHA-256 bank\n");
    exit(1);	
}
