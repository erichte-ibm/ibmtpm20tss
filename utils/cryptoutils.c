/********************************************************************************/
/*										*/
/*			OpenSSL Crypto Utilities				*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*	      $Id: cryptoutils.c 992 2017-04-19 15:22:19Z kgoldman $		*/
/*										*/
/* (c) Copyright IBM Corporation 2017.						*/
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

/* These functions are worthwhile sample code that probably (judgment call) do not belong in the TSS
   library.

   They show how to convert public or private EC or RSA among PEM format <-> EVP format <-> EC_KEY
   or RSA format <-> binary arrays <-> TPM format TPM2B_PRIVATE, TPM2B_SENSITIVE, TPM2B_PUBLIC
   usable for loadexternal or import.

   There are functions to convert public keys from TPM -> RSA <-> PEM, and to verify a TPM signature
   using a PEM format public key.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include <openssl/rsa.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#ifndef TPM_TSS_NOFILE
#include <tss2/tssfile.h>
#endif
#include <tss2/tssutils.h>
#include <tss2/tssmarshal.h>
#include <tss2/tsscrypto.h>
#include <tss2/Implementation.h>

#include "objecttemplates.h"
#include "cryptoutils.h"

extern int verbose;

#ifndef TPM_TSS_NOFILE

/* convertPemToEvpPrivKey() converts a PEM file to an openssl EVP_PKEY key pair */

TPM_RC convertPemToEvpPrivKey(EVP_PKEY **evpPkey,		/* freed by caller */
			      const char *pemKeyFilename,
			      const char *password)
{
    TPM_RC 	rc = 0;
    FILE 	*pemKeyFile = NULL;

    if (rc == 0) {
	rc = TSS_File_Open(&pemKeyFile, pemKeyFilename, "rb"); 	/* closed @2 */
    }
    if (rc == 0) {
	*evpPkey = PEM_read_PrivateKey(pemKeyFile, NULL, NULL, (void *)password);
	if (*evpPkey == NULL) {
	    printf("convertPemToEvpPrivKey: Error reading key file %s\n", pemKeyFilename);
	    rc = EXIT_FAILURE;
	}
    }
    if (pemKeyFile != NULL) {
	fclose(pemKeyFile);			/* @2 */
    }
    return rc;
}

#endif

#ifndef TPM_TSS_NOFILE

/* convertPemToEvpPubKey() converts a PEM file to an openssl EVP_PKEY public key */

TPM_RC convertPemToEvpPubKey(EVP_PKEY **evpPkey,		/* freed by caller */
			     const char *pemKeyFilename)
{
    TPM_RC 	rc = 0;
    FILE 	*pemKeyFile = NULL;

    if (rc == 0) {
	rc = TSS_File_Open(&pemKeyFile, pemKeyFilename, "rb"); 	/* closed @2 */
    }
    if (rc == 0) {
	*evpPkey = PEM_read_PUBKEY(pemKeyFile, NULL, NULL, NULL);
	if (*evpPkey == NULL) {
	    printf("convertPemToEvpPubKey: Error reading key file %s\n", pemKeyFilename);
	    rc = EXIT_FAILURE;
	}
    }
    if (pemKeyFile != NULL) {
	fclose(pemKeyFile);			/* @2 */
    }
    return rc;
}

#endif

/* convertEvpPkeyToEckey retrieves the EC_KEY key token from the EVP_PKEY */

TPM_RC convertEvpPkeyToEckey(EC_KEY **ecKey,		/* freed by caller */
			     EVP_PKEY *evpPkey)
{
    TPM_RC 	rc = 0;
    
    if (rc == 0) {
	*ecKey = EVP_PKEY_get1_EC_KEY(evpPkey);
	if (*ecKey == NULL) {
	    printf("convertPrivToEckey: Error extracting EC key from EVP_PKEY\n");
	    rc = EXIT_FAILURE;
	}
    }
    return rc;
}

/* convertEvpPkeyToRsakey() retrieves the RSA key token from the EVP_PKEY */

TPM_RC convertEvpPkeyToRsakey(RSA **rsaKey,		/* freed by caller */
			      EVP_PKEY *evpPkey)
{
    TPM_RC 	rc = 0;
    
    if (rc == 0) {
	*rsaKey = EVP_PKEY_get1_RSA(evpPkey);
	if (*rsaKey == NULL) {
	    printf("convertEvpPkeyToRsakey: EVP_PKEY_get1_RSA failed\n");  
	    rc = EXIT_FAILURE;
	}
    }
    return rc;
}

TPM_RC convertEcKeyToPrivateKeyBin(int 		*privateKeyBytes,
				   uint8_t 	**privateKeyBin,	/* freed by caller */
				   const EC_KEY *ecKey)
{
    TPM_RC 		rc = 0;
    const BIGNUM 	*privateKeyBn;

    /* get the ECC private key as a BIGNUM */
    if (rc == 0) {
	privateKeyBn = EC_KEY_get0_private_key(ecKey);
    }
    /* allocate a buffer for the private key array */
    if (rc == 0) {
	*privateKeyBytes = BN_num_bytes(privateKeyBn);
	rc = TSS_Malloc(privateKeyBin, *privateKeyBytes);
    }
    /* convert the private key bignum to binary */
    if (rc == 0) {
	BN_bn2bin(privateKeyBn, *privateKeyBin);
	if (verbose) TSS_PrintAll("convertEcKeyToPrivateKeyBin:", *privateKeyBin, *privateKeyBytes);
    }
    return rc;
}

/* convertRsaKeyToPrivateKeyBin() converts an openssl RSA key token private prime p to a binary
   array */

TPM_RC convertRsaKeyToPrivateKeyBin(int 	*privateKeyBytes,
				    uint8_t 	**privateKeyBin,	/* freed by caller */
				    const RSA	 *rsaKey)
{
    TPM_RC 		rc = 0;
    const BIGNUM 	*p;
    const BIGNUM 	*q;

    /* get the private primes */
    if (rc == 0) {
	rc = getRsaKeyParts(NULL, NULL, NULL, &p, &q, rsaKey);
    }
    /* allocate a buffer for the private key array */
    if (rc == 0) {
	*privateKeyBytes = BN_num_bytes(p);
	rc = TSS_Malloc(privateKeyBin, *privateKeyBytes);
    }
    /* convert the private key bignum to binary */
    if (rc == 0) {
	BN_bn2bin(p, *privateKeyBin);
    }    
    return rc;
}

TPM_RC convertEcKeyToPublicKeyBin(int 		*modulusBytes,
				  uint8_t 	**modulusBin,	/* freed by caller */
				  const EC_KEY 	*ecKey)
{
    TPM_RC 		rc = 0;
    const EC_POINT 	*ecPoint;
    const EC_GROUP 	*ecGroup;

    if (rc == 0) {
	ecPoint = EC_KEY_get0_public_key(ecKey);
	if (ecPoint == NULL) {
	    printf("convertEcKeyToPublicKeyBin: Error extracting EC point from EC public key\n");
	    rc = EXIT_FAILURE;
	}
    }
    if (rc == 0) {   
	ecGroup = EC_KEY_get0_group(ecKey);
	if (ecGroup  == NULL) {
	    printf("convertEcKeyToPublicKeyBin: Error extracting EC group from EC public key\n");
	    rc = EXIT_FAILURE;
	}
    }
    /* get the public modulus */
    if (rc == 0) {   
	*modulusBytes = EC_POINT_point2oct(ecGroup, ecPoint,
					   POINT_CONVERSION_UNCOMPRESSED,
					   NULL, 0, NULL);
    }
    if (rc == 0) {   
	rc = TSS_Malloc(modulusBin, *modulusBytes);
    }
    if (rc == 0) {
	EC_POINT_point2oct(ecGroup, ecPoint,
			   POINT_CONVERSION_UNCOMPRESSED,
			   *modulusBin, *modulusBytes, NULL);
	if (verbose) TSS_PrintAll("convertEcKeyToPublicKeyBin:", *modulusBin, *modulusBytes);
    }
    return rc;
}

/* convertRsaKeyToPublicKeyBin() converts from an openssl RSA key token to a public modulus */

TPM_RC convertRsaKeyToPublicKeyBin(int 		*modulusBytes,
				   uint8_t 	**modulusBin,	/* freed by caller */
				   const RSA 	*rsaKey)
{
    TPM_RC 		rc = 0;
    const BIGNUM 	*n;
    const BIGNUM 	*e;
    const BIGNUM 	*d;

    /* get the public modulus from the RSA key token */
    if (rc == 0) {
	rc = getRsaKeyParts(&n, &e, &d, NULL, NULL, rsaKey);
    }
    if (rc == 0) {
	*modulusBytes = BN_num_bytes(n);
    }
    if (rc == 0) {   
	rc = TSS_Malloc(modulusBin, *modulusBytes);
    }
    if (rc == 0) {
	BN_bn2bin(n, *modulusBin);
    }
    return rc;
}

TPM_RC convertEcPrivateKeyBinToPrivate(TPM2B_PRIVATE 	*objectPrivate,
				       int 		privateKeyBytes,
				       uint8_t 		*privateKeyBin,
				       const char 	*password)
{
    TPM_RC 		rc = 0;
    TPMT_SENSITIVE	tSensitive;
    TPM2B_SENSITIVE	bSensitive;

    /* In some cases, the sensitive data is not encrypted and the integrity value is not present.
       When an integrity value is not needed, it is not present and it is not represented by an
       Empty Buffer.

       In this case, the TPM2B_PRIVATE will just be a marshaled TPM2B_SENSITIVE, which is a
       marshaled TPMT_SENSITIVE */	

    /* construct TPMT_SENSITIVE	*/
    if (rc == 0) {
	/* This shall be the same as the type parameter of the associated public area. */
	tSensitive.sensitiveType = TPM_ALG_ECC;
	tSensitive.seedValue.b.size = 0;
	/* key password converted to TPM2B */
	rc = TSS_TPM2B_StringCopy(&tSensitive.authValue.b, password, sizeof(TPMU_HA));
    }
    if (rc == 0) {
	if (privateKeyBytes > 32) {	/* hard code NISTP256 */
	    printf("convertEcPrivateKeyBinToPrivate: Error, private key size %u not 32\n",
		   privateKeyBytes);
	    rc = EXIT_FAILURE;
	    
	}
    }
    if (rc == 0) {
	tSensitive.sensitive.ecc.t.size = privateKeyBytes;
	memcpy(tSensitive.sensitive.ecc.t.buffer, privateKeyBin, privateKeyBytes);
    }
    /* FIXME common code for EC and RSA */
    /* marshal the TPMT_SENSITIVE into a TPM2B_SENSITIVE */	
    if (rc == 0) {
	int32_t size = sizeof(bSensitive.t.sensitiveArea);	/* max size */
	uint8_t *buffer = bSensitive.b.buffer;			/* pointer that can move */
	bSensitive.t.size = 0;					/* required before marshaling */
	rc = TSS_TPMT_SENSITIVE_Marshal(&tSensitive,
					&bSensitive.b.size,	/* marshaled size */
					&buffer,		/* marshal here */
					&size);			/* max size */
    }
    /* marshal the TPM2B_SENSITIVE (as a TPM2B_PRIVATE, see above) into a TPM2B_PRIVATE */
    if (rc == 0) {
	int32_t size = sizeof(objectPrivate->t.buffer);	/* max size */
	uint8_t *buffer = objectPrivate->t.buffer;		/* pointer that can move */
	objectPrivate->t.size = 0;				/* required before marshaling */
	rc = TSS_TPM2B_PRIVATE_Marshal((TPM2B_PRIVATE *)&bSensitive,
				       &objectPrivate->t.size,	/* marshaled size */
				       &buffer,		/* marshal here */
				       &size);		/* max size */
    }
    return rc;
}

/* convertRsaPrivateKeyBinToPrivate() converts an RSA prime 'privateKeyBin' to either a
   TPM2B_PRIVATE or a TPM2B_SENSITIVE

*/

TPM_RC convertRsaPrivateKeyBinToPrivate(TPM2B_PRIVATE 	*objectPrivate,
					TPM2B_SENSITIVE *objectSensitive,
					int 		privateKeyBytes,
					uint8_t 	*privateKeyBin,
					const char 	*password)
{
    TPM_RC 		rc = 0;
    TPMT_SENSITIVE	tSensitive;
    TPM2B_SENSITIVE	bSensitive;

    if (rc == 0) {
	if (((objectPrivate == NULL) && (objectSensitive == NULL)) ||
	    ((objectPrivate != NULL) && (objectSensitive != NULL))) {
	    printf("convertRsaPrivateKeyBinToPrivate: Only one result supported\n");
	    rc = EXIT_FAILURE;
	}
    }

    /* In some cases, the sensitive data is not encrypted and the integrity value is not present.
       When an integrity value is not needed, it is not present and it is not represented by an
       Empty Buffer.

       In this case, the TPM2B_PRIVATE will just be a marshaled TPM2B_SENSITIVE, which is a
       marshaled TPMT_SENSITIVE */	

    /* construct TPMT_SENSITIVE	*/
    if (rc == 0) {
	/* This shall be the same as the type parameter of the associated public area. */
	tSensitive.sensitiveType = TPM_ALG_RSA;
	tSensitive.seedValue.b.size = 0;
	/* key password converted to TPM2B */
	rc = TSS_TPM2B_StringCopy(&tSensitive.authValue.b, password, sizeof(TPMU_HA));
    }
    if (rc == 0) {
	if ((size_t)privateKeyBytes > sizeof(tSensitive.sensitive.rsa.t.buffer)) {
	    printf("convertRsaPrivateKeyBinToPrivate: "
		   "Error, private key modulus %d greater than %lu\n",
		   privateKeyBytes, (unsigned long)sizeof(tSensitive.sensitive.rsa.t.buffer));
	    rc = EXIT_FAILURE;
	}
    }
    if (rc == 0) {
	tSensitive.sensitive.rsa.t.size = privateKeyBytes;
	memcpy(tSensitive.sensitive.rsa.t.buffer, privateKeyBin, privateKeyBytes);
    }
    /* FIXME common code for EC and RSA */
    /* marshal the TPMT_SENSITIVE into a TPM2B_SENSITIVE */	
    if (rc == 0) {
	if (objectPrivate != NULL) {
	    int32_t size = sizeof(bSensitive.t.sensitiveArea);	/* max size */
	    uint8_t *buffer = bSensitive.b.buffer;		/* pointer that can move */
	    bSensitive.t.size = 0;				/* required before marshaling */
	    rc = TSS_TPMT_SENSITIVE_Marshal(&tSensitive,
					    &bSensitive.b.size,	/* marshaled size */
					    &buffer,		/* marshal here */
					    &size);		/* max size */
	}
	else {	/* return TPM2B_SENSITIVE */
	    objectSensitive->t.sensitiveArea = tSensitive;
	}	
    }
    /* marshal the TPM2B_SENSITIVE (as a TPM2B_PRIVATE, see above) into a TPM2B_PRIVATE */
    if (rc == 0) {
	if (objectPrivate != NULL) {
	    int32_t size = sizeof(objectPrivate->t.buffer);	/* max size */
	    uint8_t *buffer = objectPrivate->t.buffer;		/* pointer that can move */
	    objectPrivate->t.size = 0;				/* required before marshaling */
	    rc = TSS_TPM2B_PRIVATE_Marshal((TPM2B_PRIVATE *)&bSensitive,
					   &objectPrivate->t.size,	/* marshaled size */
					   &buffer,		/* marshal here */
					   &size);		/* max size */
	}
    }
    return rc;
}

TPM_RC convertEcPublicKeyBinToPublic(TPM2B_PUBLIC 	*objectPublic,
				     int		keyType,
				     TPMI_ALG_HASH 	nalg,
				     TPMI_ALG_HASH	halg,
				     int 		modulusBytes,
				     uint8_t 		*modulusBin)
{
    TPM_RC 		rc = 0;

    if (rc == 0) {
	if (modulusBytes != 65) {	/* 1 for compression + 32 + 32 */
	    printf("convertEcPublicKeyBinToPublic: public modulus expected 65 bytes, actual %u\n",
		   modulusBytes);
	    rc = EXIT_FAILURE;
	}
    }
    if (rc == 0) {
	/* Table 184 - Definition of TPMT_PUBLIC Structure */
	objectPublic->publicArea.type = TPM_ALG_ECC;
	objectPublic->publicArea.nameAlg = nalg;
	objectPublic->publicArea.objectAttributes.val = TPMA_OBJECT_NODA;
	objectPublic->publicArea.objectAttributes.val |= TPMA_OBJECT_USERWITHAUTH;
	if (keyType == TYPE_SI) {
	    objectPublic->publicArea.objectAttributes.val |= TPMA_OBJECT_SIGN;
	}
	else {
	    objectPublic->publicArea.objectAttributes.val |= TPMA_OBJECT_DECRYPT;
	}
	objectPublic->publicArea.authPolicy.t.size = 0;
	/* Table 182 - Definition of TPMU_PUBLIC_PARMS Union <IN/OUT, S> */
	objectPublic->publicArea.parameters.eccDetail.symmetric.algorithm = TPM_ALG_NULL;
	if (keyType == TYPE_SI) {
	    objectPublic->publicArea.parameters.eccDetail.scheme.scheme = TPM_ALG_ECDSA;
	}
	else {
	    objectPublic->publicArea.parameters.eccDetail.scheme.scheme = TPM_ALG_NULL;
	}
	/* or always use ECDSA (sample code) */
	objectPublic->publicArea.parameters.eccDetail.scheme.scheme = TPM_ALG_ECDSA;
	/* Table 152 - Definition of TPMU_ASYM_SCHEME Union */
	objectPublic->publicArea.parameters.eccDetail.scheme.details.ecdsa.hashAlg = halg;
	objectPublic->publicArea.parameters.eccDetail.curveID = TPM_ECC_NIST_P256;	
	objectPublic->publicArea.parameters.eccDetail.kdf.scheme = TPM_ALG_NULL;
	objectPublic->publicArea.parameters.eccDetail.kdf.details.mgf1.hashAlg = halg;
    }
    if (rc == 0) {
	objectPublic->publicArea.unique.ecc.x.t.size = 32;	
	memcpy(objectPublic->publicArea.unique.ecc.x.t.buffer, modulusBin +1, 32);	

	objectPublic->publicArea.unique.ecc.y.t.size = 32;	
	memcpy(objectPublic->publicArea.unique.ecc.y.t.buffer, modulusBin +33, 32);	
    }
    return rc;
}

/* convertRsaPublicKeyBinToPublic() converts a public modulus to a TPM2B_PUBLIC structure. */

TPM_RC convertRsaPublicKeyBinToPublic(TPM2B_PUBLIC 	*objectPublic,
				      int		keyType,
				      TPMI_ALG_HASH 	nalg,
				      TPMI_ALG_HASH	halg,
				      int 		modulusBytes,
				      uint8_t 		*modulusBin)
{
    TPM_RC 		rc = 0;

    if (rc == 0) {
	if ((size_t)modulusBytes > sizeof(objectPublic->publicArea.unique.rsa.t.buffer)) {
	    printf("convertRsaPublicKeyBinToPublic: Error, "
		   "public key modulus %d greater than %lu\n", modulusBytes,
		   (unsigned long)sizeof(objectPublic->publicArea.unique.rsa.t.buffer));
	    rc = EXIT_FAILURE;
	}
    }
    if (rc == 0) {
	/* Table 184 - Definition of TPMT_PUBLIC Structure */
	objectPublic->publicArea.type = TPM_ALG_RSA;
	objectPublic->publicArea.nameAlg = nalg;
	objectPublic->publicArea.objectAttributes.val = TPMA_OBJECT_NODA;
	objectPublic->publicArea.objectAttributes.val |= TPMA_OBJECT_USERWITHAUTH;
	if (keyType == TYPE_SI) {
	    objectPublic->publicArea.objectAttributes.val |= TPMA_OBJECT_SIGN;
	}
	else {
	    objectPublic->publicArea.objectAttributes.val |= TPMA_OBJECT_DECRYPT;
	}
	objectPublic->publicArea.authPolicy.t.size = 0;
	/* Table 182 - Definition of TPMU_PUBLIC_PARMS Union <IN/OUT, S> */
	objectPublic->publicArea.parameters.rsaDetail.symmetric.algorithm = TPM_ALG_NULL;
	if (keyType == TYPE_SI) {
	    objectPublic->publicArea.parameters.rsaDetail.scheme.scheme = TPM_ALG_RSASSA;
	}
	else {
	    objectPublic->publicArea.parameters.rsaDetail.scheme.scheme = TPM_ALG_NULL;
	}
	objectPublic->publicArea.parameters.rsaDetail.scheme.details.rsassa.hashAlg = halg;
	objectPublic->publicArea.parameters.rsaDetail.keyBits = modulusBytes * 8;	
	objectPublic->publicArea.parameters.rsaDetail.exponent = 0;

	objectPublic->publicArea.unique.rsa.t.size = modulusBytes;
	memcpy(objectPublic->publicArea.unique.rsa.t.buffer, modulusBin, modulusBytes);
    }
    return rc;
}

TPM_RC convertEcKeyToPrivate(TPM2B_PRIVATE 	*objectPrivate,
			     EC_KEY 		*ecKey,
			     const char 	*password)
{
    TPM_RC 	rc = 0;
    int 	privateKeyBytes;
    uint8_t 	*privateKeyBin = NULL;
    
    if (rc == 0) {
	rc = convertEcKeyToPrivateKeyBin(&privateKeyBytes,
					 &privateKeyBin,	/* freed @1 */
					 ecKey);
    }
    if (rc == 0) {
	rc = convertEcPrivateKeyBinToPrivate(objectPrivate,
					     privateKeyBytes,
					     privateKeyBin,
					     password);
    }
    free(privateKeyBin);		/* @1 */
    return rc;
}

/* convertRsaKeyToPrivate() converts an openssl RSA key token to either a TPM2B_PRIVATE or
   TPM2B_SENSITIVE  */

TPM_RC convertRsaKeyToPrivate(TPM2B_PRIVATE 	*objectPrivate,
			      TPM2B_SENSITIVE 	*objectSensitive,
			      RSA 		*rsaKey,
			      const char 	*password)
{
    TPM_RC 	rc = 0;
    int 	privateKeyBytes;
    uint8_t 	*privateKeyBin = NULL;

    /* convert an openssl RSA key token private prime p to a binary array */
    if (rc == 0) {
	rc = convertRsaKeyToPrivateKeyBin(&privateKeyBytes,
					  &privateKeyBin,	/* freed @1 */
					  rsaKey);
    }
    /* convert an RSA prime 'privateKeyBin' to either a TPM2B_PRIVATE or a TPM2B_SENSITIVE */
    if (rc == 0) {
	rc = convertRsaPrivateKeyBinToPrivate(objectPrivate,
					      objectSensitive,
					      privateKeyBytes,
					      privateKeyBin,
					      password);
    }
    free(privateKeyBin);		/* @1 */
    return rc;
}

TPM_RC convertEcKeyToPublic(TPM2B_PUBLIC 	*objectPublic,
			    int			keyType,
			    TPMI_ALG_HASH 	nalg,
			    TPMI_ALG_HASH	halg,
			    EC_KEY 		*ecKey)
{
    TPM_RC 		rc = 0;
    int 		modulusBytes;
    uint8_t 		*modulusBin = NULL;
    
    if (rc == 0) {
	rc = convertEcKeyToPublicKeyBin(&modulusBytes,
					&modulusBin,		/* freed @1 */
					ecKey);
    }
    if (rc == 0) {
	rc = convertEcPublicKeyBinToPublic(objectPublic,
					   keyType,
					   nalg,
					   halg,
					   modulusBytes,
					   modulusBin);
    }
    free(modulusBin);		/* @1 */
    return rc;
}

/* convertRsaKeyToPublic() converts from an openssl RSA key token to a TPM2B_PUBLIC */

TPM_RC convertRsaKeyToPublic(TPM2B_PUBLIC 	*objectPublic,
			     int		keyType,
			     TPMI_ALG_HASH 	nalg,
			     TPMI_ALG_HASH	halg,
			     RSA 		*rsaKey)
{
    TPM_RC 		rc = 0;
    int 		modulusBytes;
    uint8_t 		*modulusBin = NULL;
    
    /* openssl RSA key token to a public modulus */
    if (rc == 0) {
	rc = convertRsaKeyToPublicKeyBin(&modulusBytes,
					 &modulusBin,		/* freed @1 */
					 rsaKey);
    }
    /* public modulus to TPM2B_PUBLIC */
    if (rc == 0) {
	rc = convertRsaPublicKeyBinToPublic(objectPublic,
					    keyType,
					    nalg,
					    halg,
					    modulusBytes,
					    modulusBin);
    }
    free(modulusBin);		/* @1 */
    return rc;
}

#ifndef TPM_TSS_NOFILE

TPM_RC convertEcPemToKeyPair(TPM2B_PUBLIC 	*objectPublic,
			     TPM2B_PRIVATE 	*objectPrivate,
			     int		keyType,
			     TPMI_ALG_HASH 	nalg,
			     TPMI_ALG_HASH	halg,
			     const char 	*pemKeyFilename,
			     const char 	*password)
{
    TPM_RC 	rc = 0;
    EVP_PKEY 	*evpPkey = NULL;
    EC_KEY 	*ecKey = NULL;

    /* convert a PEM file to an openssl EVP_PKEY */
    if (rc == 0) {
	rc = convertPemToEvpPrivKey(&evpPkey,		/* freed @1 */
				    pemKeyFilename,
				    password);
    }
    if (rc == 0) {
	rc = convertEvpPkeyToEckey(&ecKey,		/* freed @2 */
				   evpPkey);
    }
    if (rc == 0) {
	rc = convertEcKeyToPrivate(objectPrivate,
				   ecKey,
				   password);
    }
    if (rc == 0) {
	rc = convertEcKeyToPublic(objectPublic,
				  keyType,
				  nalg,
				  halg,
				  ecKey);
    }
    EC_KEY_free(ecKey);   		/* @2 */
    if (evpPkey != NULL) {
	EVP_PKEY_free(evpPkey);		/* @1 */
    }
    return rc;
}

#endif

#ifndef TPM_TSS_NOFILE

TPM_RC convertRsaPemToKeyPair(TPM2B_PUBLIC 	*objectPublic,
			      TPM2B_PRIVATE 	*objectPrivate,
			      int		keyType,
			      TPMI_ALG_HASH 	nalg,
			      TPMI_ALG_HASH	halg,
			      const char 	*pemKeyFilename,
			      const char 	*password)
{
    TPM_RC 	rc = 0;
    EVP_PKEY 	*evpPkey = NULL;
    RSA		*rsaKey = NULL;
    
    if (rc == 0) {
	rc = convertPemToEvpPrivKey(&evpPkey,		/* freed @1 */
				    pemKeyFilename,
				    password);
    }
    if (rc == 0) {
	rc = convertEvpPkeyToRsakey(&rsaKey,		/* freed @2 */
				    evpPkey);
    }
    if (rc == 0) {
	rc = convertRsaKeyToPrivate(objectPrivate,
				    NULL,
				    rsaKey,
				    password);
    }
    if (rc == 0) {
	rc = convertRsaKeyToPublic(objectPublic,
				   keyType,
				   nalg,
				   halg,
				   rsaKey);
    }
    if (rsaKey != NULL) {
	RSA_free(rsaKey);		/* @2 */
    }
    if (evpPkey != NULL) {
	EVP_PKEY_free(evpPkey);		/* @1 */
    }
    return rc;
}

#endif

/* getRsaKeyParts() gets the RSA key parts from an OpenSSL RSA key token.

   If n is not NULL, returns n, e, and d.  If p is not NULL, returns p and q.
*/

TPM_RC getRsaKeyParts(const BIGNUM **n,
		     const BIGNUM **e,
		     const BIGNUM **d,
		     const BIGNUM **p,
		     const BIGNUM **q,
		     const RSA *rsaKey)
{
    TPM_RC  	rc = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000
    if (n != NULL) {
	*n = rsaKey->n;
	*e = rsaKey->e;
	*d = rsaKey->d;
    }
    if (p != NULL) {
	*p = rsaKey->p;
	*q = rsaKey->q;
    }
#else
    if (n != NULL) {
	RSA_get0_key(rsaKey, n, e, d);
    }
    if (p != NULL) {
	RSA_get0_factors(rsaKey, p, q);
    }
#endif
    return rc;
}

/* returns the type (EVP_PKEY_RSA or EVP_PKEY_EC) of the EVP_PKEY.

 */

int getRsaPubkeyAlgorithm(EVP_PKEY *pkey)
{
    int 			pkeyType;	/* RSA or EC */
#if OPENSSL_VERSION_NUMBER < 0x10100000
    pkeyType = pkey->type;
#else
    pkeyType = EVP_PKEY_base_id(pkey);
#endif
    return pkeyType;
}

/* convertPublicToPEM() saves a PEM format public key from a TPM2B_PUBLIC
   
*/

TPM_RC convertPublicToPEM(const TPM2B_PUBLIC *public,
			  const char *pemFilename)
{
    TPM_RC 	rc = 0;
    EVP_PKEY 	*evpPubkey = NULL;          	/* OpenSSL public key, EVP format */

    /* convert TPM2B_PUBLIC to EVP_PKEY */
    if (rc == 0) {
	switch (public->publicArea.type) {
	  case TPM_ALG_RSA:
	    rc = convertRsaPublicToEvpPubKey(&evpPubkey,	/* freed @1 */
					     public);
	    break;
	  case TPM_ALG_ECC:
	    rc = convertEcPublicToEvpPubKey(&evpPubkey,		/* freed @1 */
					    public);
	    break;
	  default:
	    rc = TSS_RC_NOT_IMPLEMENTED;
	    break;
	}
    }
    /* write the openssl structure in PEM format */
    if (rc == 0) {
	rc = convertEvpPubkeyToPem(evpPubkey,
				   pemFilename);

    }
    if (evpPubkey != NULL) {
	EVP_PKEY_free(evpPubkey);		/* @1 */
    }
    return rc;
}

/* convertRsaPublicToEvpPubKey() converts an RSA TPM2B_PUBLIC to a EVP_PKEY.

*/

TPM_RC convertRsaPublicToEvpPubKey(EVP_PKEY **evpPubkey,	/* freed by caller */
				   const TPM2B_PUBLIC *public)
{
    TPM_RC 	rc = 0;
    int		irc;
    RSA		*rsaPubKey = NULL;
    
    if (rc == 0) {
	*evpPubkey = EVP_PKEY_new();
	if (*evpPubkey == NULL) {
	    printf("convertRsaPublicToEvpPubKey: EVP_PKEY failed\n");
	    rc = TSS_RC_OUT_OF_MEMORY;
	}
    }
    /* TPM to RSA token */
    if (rc == 0) {
	/* public exponent */
	unsigned char earr[3] = {0x01, 0x00, 0x01};
	rc = TSS_RSAGeneratePublicToken
	     (&rsaPubKey,				/* freed as part of EVP_PKEY  */
	      public->publicArea.unique.rsa.t.buffer,  	/* public modulus */
	      public->publicArea.unique.rsa.t.size,
	      earr,      				/* public exponent */
	      sizeof(earr));
    }
    /* RSA token to EVP */
    if (rc == 0) {
	irc  = EVP_PKEY_assign_RSA(*evpPubkey, rsaPubKey);
	if (irc == 0) {
	    RSA_free(rsaPubKey);	/* because not assigned tp EVP_PKEY */
	    printf("convertRsaPublicToEvpPubKey: EVP_PKEY_assign_RSA failed\n");
	    rc = TSS_RC_RSA_KEY_CONVERT;
	}
    }
    return rc;
}

/* convertEcPublicToEvpPubKey() converts an EC TPM2B_PUBLIC to a EVP_PKEY.
 */

TPM_RC convertEcPublicToEvpPubKey(EVP_PKEY **evpPubkey,		/* freed by caller */
				  const TPM2B_PUBLIC *public)
{
    TPM_RC 	rc = 0;
    int		irc;
    EC_GROUP 	*ecGroup;
    EC_KEY 	*ecKey = NULL;
    BIGNUM 	*x = NULL;		/* freed @2 */
    BIGNUM 	*y = NULL;		/* freed @3 */
    
    if (rc == 0) {
	*evpPubkey = EVP_PKEY_new();
	if (*evpPubkey == NULL) {
	    printf("convertEcPublicToEvpPubKey: EVP_PKEY failed\n");
	    rc = TSS_RC_OUT_OF_MEMORY;
	}
    }
    if (rc == 0) {
	ecKey = EC_KEY_new();		/* freed @1 */
	if (ecKey == NULL) {
	    printf("convertEcPublicToEvpPubKey: Error creating EC_KEY\n");
	    rc = TSS_RC_OUT_OF_MEMORY;
	}
    }
    if (rc == 0) {
	ecGroup = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
	if (ecGroup == NULL) {
	    printf("convertEcPublicToEvpPubKey: Error in EC_GROUP_new_by_curve_name\n");
	    rc = TSS_RC_OUT_OF_MEMORY;
	}
    }
    if (rc == 0) {
	/* returns void */
	EC_GROUP_set_asn1_flag(ecGroup, OPENSSL_EC_NAMED_CURVE);
    }
    /* assign curve to EC_KEY */
    if (rc == 0) {
	irc = EC_KEY_set_group(ecKey, ecGroup);
	if (irc != 1) {
	    printf("convertEcPublicToEvpPubKey: Error in EC_KEY_set_group\n");
	    rc = TSS_RC_EC_KEY_CONVERT;
	}
    }
    if (rc == 0) {
	rc = convertBin2Bn(&x,				/* freed @2 */
			   public->publicArea.unique.ecc.x.t.buffer,
			   public->publicArea.unique.ecc.x.t.size);	
    }
    if (rc == 0) {
	rc = convertBin2Bn(&y,				/* freed @3 */
			   public->publicArea.unique.ecc.y.t.buffer,
			   public->publicArea.unique.ecc.y.t.size);
    }
    if (rc == 0) {
	irc = EC_KEY_set_public_key_affine_coordinates(ecKey, x, y);
	if (irc != 1) {
	    printf("convertEcPublicToEvpPubKey: "
		   "Error converting public key from X Y to EC_KEY format\n");
	    rc = TSS_RC_EC_KEY_CONVERT;
	}
    }
    if (rc == 0) {
	irc = EVP_PKEY_set1_EC_KEY(*evpPubkey, ecKey);
	if (irc != 1) {
	    printf("convertEcPublicToEvpPubKey: "
		   "Error converting public key from EC to EVP format\n");
	    rc = TSS_RC_EC_KEY_CONVERT;
	}
    }
    if (ecKey != NULL) {
	EC_KEY_free(ecKey);	/* @1 */
    }
    if (x != NULL) {
	BN_free(x);		/* @2 */
    }
    if (y != NULL) {
	BN_free(y);		/* @3 */
    }
    return rc;
}

TPM_RC convertEvpPubkeyToPem(EVP_PKEY *evpPubkey,
			     const char *pemFilename)
{
    TPM_RC 	rc = 0;
    int		irc;
    FILE 	*pemFile = NULL; 
    
    if (rc == 0) {
	pemFile = fopen(pemFilename, "wb");
	if (pemFile == NULL) {
	    printf("convertEvpPubkeyToPem: Unable to open PEM file %s for write\n", pemFilename);
	    rc = TSS_RC_FILE_OPEN;
	}
    }
    if (rc == 0) {
	irc = PEM_write_PUBKEY(pemFile, evpPubkey);
	if (irc == 0) {
	    printf("convertEvpPubkeyToPem: Unable to write PEM file %s\n", pemFilename);
	    rc = TSS_RC_FILE_WRITE;
	}
    }
    if (pemFile != NULL) {
	fclose(pemFile);			/* @2 */
    }
    return rc;
}

#ifndef TPM_TSS_NOFILE

/* verifySignatureFromPem() verifies the signature 'tSignature' against the digest 'message' using
   the public key in the PEM format file 'pemFilename'.

*/

TPM_RC verifySignatureFromPem(unsigned char *message,
			      unsigned int messageSize,
			      TPMT_SIGNATURE *tSignature,
			      TPMI_ALG_HASH halg,
			      const char *pemFilename)
{
    TPM_RC 		rc = 0;
    EVP_PKEY 		*evpPkey = NULL;        /* OpenSSL public key, EVP format */
    
    /* read the public key from PEM format */
    if (rc == 0) {
	rc = convertPemToEvpPubKey(&evpPkey,		/* freed @1*/
				   pemFilename);
    }
    /* RSA or EC */
    if (rc == 0) {
	switch(tSignature->sigAlg) {
	  case TPM_ALG_RSASSA:
	    rc = verifyRSASignatureFromPem(message,
					   messageSize,
					   tSignature,
					   halg,
					   evpPkey);
	    break;
	  case TPM_ALG_ECDSA:
	    rc = verifyEcSignatureFromPem(message,
					  messageSize,
					  tSignature,
					  evpPkey);
	    break;
	  default:
	    printf("verifyRSASignatureFromPem: Unknown hash algorithm %04x\n", halg);
	    rc = TSS_RC_BAD_SIGNATURE_ALGORITHM;
	}
    }
    if (evpPkey != NULL) {
	EVP_PKEY_free(evpPkey);		/* @1 */
    }
    return rc;
}

#endif

/* verifyRSASignatureFromPem() verifies the signature 'tSignature' against the digest 'message'
   using the RSA public key in the PEM format file 'pemFilename'.

*/

TPM_RC verifyRSASignatureFromPem(unsigned char *message,
				 unsigned int messageSize,
				 TPMT_SIGNATURE *tSignature,
				 TPMI_ALG_HASH halg,
				 EVP_PKEY *evpPkey)
{
    TPM_RC 		rc = 0;
    int			irc;
    int 		nid;
    RSA 		*rsaPubKey = NULL;	/* OpenSSL public key, RSA format */
    
    /* map from hash algorithm to openssl nid */
    if (rc == 0) {
	switch (halg) {
	  case TPM_ALG_SHA1:
	    nid = NID_sha1;
	    break;
	  case TPM_ALG_SHA256:
	    nid = NID_sha256;
	    break;
	  case TPM_ALG_SHA384:
	    nid = NID_sha384;
	    break;
	  default:
	    printf("verifyRSASignatureFromPem: Unknown hash algorithm %04x\n", halg);
	    rc = TSS_RC_BAD_HASH_ALGORITHM;
	}
    }
    /* construct the RSA key token */
    if (rc == 0) {
	rsaPubKey = EVP_PKEY_get1_RSA(evpPkey);
	if (rsaPubKey == NULL) {
	    printf("verifyRSASignatureFromPem: EVP_PKEY_get1_RSA failed\n");
	    rc = TSS_RC_RSA_KEY_CONVERT;
	}
    }
    /* verify the signature */
    if (rc == 0) {
	irc = RSA_verify(nid,
			 message, messageSize,
			 tSignature->signature.rsassa.sig.t.buffer,
			 tSignature->signature.rsassa.sig.t.size,
			 rsaPubKey);
	if (irc != 1) {
	    printf("verifyRSASignatureFromPem: Bad signature\n");
	    rc = TSS_RC_RSA_SIGNATURE;
	}
    }
    if (rsaPubKey != NULL) {
	RSA_free(rsaPubKey);          	/* @3 */
    }
    return rc;
}

/* verifyRSASignatureFromPem() verifies the signature 'tSignature' against the digest 'message'
   using the EC public key in the PEM format file 'pemFilename'.

*/

TPM_RC verifyEcSignatureFromPem(unsigned char *message,
				unsigned int messageSize,
				TPMT_SIGNATURE *tSignature,
				EVP_PKEY *evpPkey)
{
    TPM_RC 		rc = 0;
    int			irc;
    EC_KEY 		*ecKey = NULL;
    BIGNUM 		*r = NULL;
    BIGNUM 		*s = NULL;
    ECDSA_SIG 		*ecdsaSig = NULL;

    /* construct the EC key token */
    if (rc == 0) {
	ecKey = EVP_PKEY_get1_EC_KEY(evpPkey);	/* freed @1 */
	if (ecKey == NULL) {
	    printf("verifyEcSignatureFromPem: EVP_PKEY_get1_EC_KEY failed\n");  
	    rc = TSS_RC_EC_KEY_CONVERT;
	}
    }
    /* construct the ECDSA_SIG signature token */
    if (rc == 0) {
	rc = convertBin2Bn(&r,			/* freed @2 */
			   tSignature->signature.ecdsa.signatureR.t.buffer,
			   tSignature->signature.ecdsa.signatureR.t.size);
    }	
    if (rc == 0) {
	rc = convertBin2Bn(&s,			/* freed @2 */
			   tSignature->signature.ecdsa.signatureS.t.buffer,
			   tSignature->signature.ecdsa.signatureS.t.size);
    }	
    if (rc == 0) {
	ecdsaSig = ECDSA_SIG_new(); 		/* freed @2 */
	if (ecdsaSig == NULL) {
	    printf("verifyEcSignatureFromPem: Error creating ECDSA_SIG_new\n");
	    rc = TSS_RC_OUT_OF_MEMORY;
	}
    }
    if (rc == 0) {
	ecdsaSig->r = r;
	ecdsaSig->s = s;
    }
    /* verify the signature */
    if (rc == 0) {
	irc = ECDSA_do_verify(message, messageSize, 
			      ecdsaSig, ecKey);
	if (irc != 1) {		/* quote signature did not verify */
	    printf("verifyRSASignatureFromPem: Bad signature\n");
	    rc = TSS_RC_RSA_SIGNATURE;
	}
    }
    if (ecKey != NULL) {
	EC_KEY_free(ecKey);		/* @1 */
    }
    /* if the ECDSA_SIG was allocated correctly, r and s are implicitly freed */
    if (ecdsaSig != NULL) {
	ECDSA_SIG_free(ecdsaSig);	/* @2 */
    }
    /* if not, explicitly free */
    else {
	if (r != NULL) BN_free(r);	/* @2 */
	if (s != NULL) BN_free(s);	/* @2 */
    }
    return rc;
}

/* convertBin2Bn() wraps the openSSL function in an error handler

   Converts a char array to bignum
*/

uint32_t convertBin2Bn(BIGNUM **bn,			/* freed by caller */
		       const unsigned char *bin,
		       unsigned int bytes)
{
    uint32_t rc = 0;

    /* BIGNUM *BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret);
    
       BN_bin2bn() converts the positive integer in big-endian form of length len at s into a BIGNUM
       and places it in ret. If ret is NULL, a new BIGNUM is created.

       BN_bin2bn() returns the BIGNUM, NULL on error.
    */
    if (rc == 0) {
        *bn = BN_bin2bn(bin, bytes, *bn);
        if (*bn == NULL) {
            printf("convertBin2Bn: Error in BN_bin2bn\n");
            rc = TSS_RC_BIGNUM;
        }
    }
    return rc;
}

