/********************************************************************************/
/*										*/
/*			    Shutdown		 				*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*	      $Id: shutdown.c 987 2017-04-17 18:27:09Z kgoldman $		*/
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tss2/tss.h>
#include <tss2/tssresponsecode.h>

static void printUsage(void);
TPM_RC shutdownCommand(TPM_SU shutdownType);

int verbose = FALSE;

int main(int argc, char *argv[])
{
    TPM_RC 			rc = 0;
    int				i;			/* argc iterator */
    TSS_CONTEXT			*tssContext = NULL;
    Shutdown_In 		in;
   
    setvbuf(stdout, 0, _IONBF, 0);      /* output may be going through pipe to log file */
    TSS_SetProperty(NULL, TPM_TRACE_LEVEL, "1");
    in.shutdownType = TPM_SU_CLEAR;			/* default */

    /* command line argument defaults */
    for (i=1 ; (i<argc) && (rc == 0) ; i++) {
	if (strcmp(argv[i],"-c") == 0) {
	    in.shutdownType = TPM_SU_CLEAR;
	}
	else if (strcmp(argv[i],"-s") == 0) {
	    in.shutdownType = TPM_SU_STATE;
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
			 TPM_CC_Shutdown,
			 TPM_RH_NULL, NULL, 0);
    }
    {
	TPM_RC rc1 = TSS_Delete(tssContext);
	if (rc == 0) {
	    rc = rc1;
	}
    }
    if (rc == 0) {
	if (verbose) printf("shutdown: success\n");
    }
    else {
	const char *msg;
	const char *submsg;
	const char *num;
	printf("shutdown: failed, rc %08x\n", rc);
	TSS_ResponseCode_toString(&msg, &submsg, &num, rc);
	printf("%s%s%s\n", msg, submsg, num);
	rc = EXIT_FAILURE;
    }
    return rc;
}

static void printUsage(void)
{
    printf("\n");
    printf("shutdown\n");
    printf("\n");
    printf("Runs TPM2_Shutdown\n");
    printf("\n");
    printf("\t-c shutdown clear (default)\n");
    printf("\t-s shutdown state\n");
    exit(1);	
}

