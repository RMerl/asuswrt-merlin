/*
 * Copyright Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/*
 * pkcs.c
 */

#include <net-snmp/net-snmp-config.h>
#ifdef NETSNMP_USE_PKCS11
#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/callback.h>
#include <security/cryptoki.h>

typedef struct netsnmp_pkcs_slot_session_s {
    CK_SLOT_ID        sid;
    CK_SESSION_HANDLE hdl;
} netsnmp_pkcs_slot_session; 

typedef struct netsnmp_pkcs_slot_info_s {
    int count;
    netsnmp_pkcs_slot_session *pSession; 
} netsnmp_pkcs_slot_info;

static CK_RV get_session_handle(CK_MECHANISM_TYPE, CK_FLAGS,\
                                CK_SESSION_HANDLE_PTR);
static CK_RV get_slot_session_handle(netsnmp_pkcs_slot_session *,\
                                     CK_SESSION_HANDLE_PTR);
static char *pkcserr_string(CK_RV);
static int free_slots(int, int, void *, void *);

static netsnmp_pkcs_slot_info *pSlot = NULL;

/*
 * initialize the Cryptoki library.
 */
int
pkcs_init(void)
{
    CK_RV          rv;
    CK_ULONG       slotcount;
    CK_SLOT_ID_PTR pSlotList = NULL;
    netsnmp_pkcs_slot_session    *tmp;
    int            i, rval = SNMPERR_SUCCESS;
    /* Initialize pkcs */
    if ((rv = C_Initialize(NULL)) != CKR_OK) {
        DEBUGMSGTL(("pkcs_init", "C_Initialize failed: %s",
                pkcserr_string(rv)));
        return SNMPERR_SC_NOT_CONFIGURED;
    }

    /* Get slot count */
    rv = C_GetSlotList(1, NULL_PTR, &slotcount);
    if (rv != CKR_OK || slotcount == 0) {
        DEBUGMSGTL(("pkcs_init", "C_GetSlotList failed: %s", 
                pkcserr_string(rv)));
        QUITFUN(SNMPERR_GENERR, pkcs_init_quit);
    }

    /* Found at least one slot, allocate memory for slot list */
    pSlotList = malloc(slotcount * sizeof (CK_SLOT_ID));
    pSlot = malloc(sizeof (netsnmp_pkcs_slot_info));
    pSlot->pSession = malloc(slotcount * sizeof (netsnmp_pkcs_slot_session));

    if (pSlotList == NULL_PTR ||
        pSlot == NULL_PTR ||
        pSlot->pSession == NULL_PTR) {
        DEBUGMSGTL(("pkcs_init","malloc failed.")); 
        QUITFUN(SNMPERR_GENERR, pkcs_init_quit);
    }

    /* Get the list of slots */
    if ((rv = C_GetSlotList(1, pSlotList, &slotcount)) != CKR_OK) {
        DEBUGMSGTL(("pkcs_init", "C_GetSlotList failed: %s", 
                pkcserr_string(rv)));
        QUITFUN(SNMPERR_GENERR, pkcs_init_quit);
    }

    /* initialize Slots structure */
    pSlot->count = slotcount;
    for (i = 0, tmp = pSlot->pSession; i < slotcount; i++, tmp++) {
        tmp->sid = pSlotList[i]; 
        tmp->hdl = NULL;
    }

    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_SHUTDOWN,
                        free_slots, NULL);

  pkcs_init_quit:
    SNMP_FREE(pSlotList);
    return rval;
}

/*
 * close all the opened sessions when finished with Cryptoki library.
 */
static int
free_slots(int majorID, int minorID, void *serverarg, void *clientarg)
{
    int            slotcount, i;

    if (pSlot != NULL) {
        slotcount = pSlot->count;
        for (i = 0; i < slotcount; i++) {
            if (pSlot->pSession->hdl != NULL) {
                free(pSlot->pSession->hdl);
            }
        }
        free(pSlot);
    }

    (void) C_Finalize(NULL);
    return 0;
}

/*
 * generate random data
 */
int
pkcs_random(u_char * buf, size_t buflen)
{
    CK_SESSION_HANDLE hSession;

    if (pSlot != NULL &&
        get_slot_session_handle(pSlot->pSession, &hSession) == CKR_OK &&
        C_GenerateRandom(hSession, buf, buflen) == CKR_OK) {
        return SNMPERR_SUCCESS;
    }

    return SNMPERR_GENERR;
}

/*
 * retrieve the session handle from the first slot that supports the specified
 * mechanism.
 */
static CK_RV
get_session_handle(CK_MECHANISM_TYPE mech_type, CK_FLAGS flag,
                CK_SESSION_HANDLE_PTR sess)
{
    CK_RV             rv = CKR_OK;
    CK_MECHANISM_INFO info;
    netsnmp_pkcs_slot_session       *p = NULL;
    int               i, slotcount = 0;
            
    if (pSlot) {
        slotcount = pSlot->count;
        p = pSlot->pSession;
    }

    /* Find a slot with matching mechanism */
    for (i = 0; i < slotcount; i++, p++) {
        rv = C_GetMechanismInfo(p->sid, mech_type, &info);

        if (rv != CKR_OK) {
            continue; /* to the next slot */
        } else {
            if (info.flags & flag) {
                rv = get_slot_session_handle(p, sess);
                break; /* found */
            }
        }
    }

    /* Show error if no matching mechanism found */
    if (i == slotcount) {
        DEBUGMSGTL(("pkcs_init","No cryptographic provider for %s",
                mech_type)); 
        return CKR_SESSION_HANDLE_INVALID;
    }

    return rv;
}

/*
 * retrieve the session handle from the specified slot.
 */
static CK_RV
get_slot_session_handle(netsnmp_pkcs_slot_session *p,
                        CK_SESSION_HANDLE_PTR sess)
{
    CK_RV rv = CKR_OK;
    if (p == NULL) {
        *sess = NULL;
        return CKR_SESSION_HANDLE_INVALID;
    }

    if (p->hdl == NULL) {
        /* Open a session */
        rv = C_OpenSession(p->sid, CKF_SERIAL_SESSION,
                NULL_PTR, NULL, &p->hdl);

        if (rv != CKR_OK) {
            DEBUGMSGTL(("get_slot_session_handle","can not open PKCS #11 session: %s",
                        pkcserr_string(rv)));
        }
    }
    *sess = p->hdl;

    return rv;
}

/*
 * perform a signature operation to generate MAC.
 */
int
pkcs_sign(CK_MECHANISM_TYPE mech_type, u_char * key, u_int keylen,
          u_char * msg, u_int msglen, u_char * mac, size_t * maclen)
{
    /*
     * Key template 
     */
    CK_OBJECT_CLASS class = CKO_SECRET_KEY;
    CK_KEY_TYPE keytype = CKK_GENERIC_SECRET;
    CK_BBOOL truevalue = TRUE;
    CK_BBOOL falsevalue= FALSE;
    CK_ATTRIBUTE template[] = {
        {CKA_CLASS, &class, sizeof (class)},
        {CKA_KEY_TYPE, &keytype, sizeof (keytype)},
        {CKA_SIGN, &truevalue, sizeof (truevalue)},
        {CKA_TOKEN, &falsevalue, sizeof (falsevalue)},
        {CKA_VALUE, key, keylen}
    };
    CK_SESSION_HANDLE hSession;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE hkey = (CK_OBJECT_HANDLE) 0;
    int                rval = SNMPERR_SUCCESS;
    if (get_session_handle(mech_type, CKF_SIGN, &hSession) != CKR_OK ||
        hSession == NULL) {
        QUITFUN(SNMPERR_GENERR, pkcs_sign_quit);
    }

    /* create a key object */
    if (C_CreateObject(hSession, template,
        (sizeof (template) / sizeof (CK_ATTRIBUTE)), &hkey) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_sign_quit);
    }

    mech.mechanism = mech_type;
    mech.pParameter = NULL_PTR;
    mech.ulParameterLen = 0;

    /* initialize a signature operation */
    if (C_SignInit(hSession, &mech, hkey) != CKR_OK ) {
        QUITFUN(SNMPERR_GENERR, pkcs_sign_quit);
    }
    /* continue a multiple-part signature operation */
    if (C_SignUpdate(hSession, msg, msglen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_sign_quit);
    }
    /* finish a multiple-part signature operation */
    if (C_SignFinal(hSession, mac, maclen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_sign_quit);
    }

  pkcs_sign_quit:

    if (key != (CK_OBJECT_HANDLE) 0) {
        (void) C_DestroyObject(hSession, hkey);
    }
    return rval;
}

/*
 * perform a message-digesting operation.
 */
int
pkcs_digest(CK_MECHANISM_TYPE mech_type, u_char * msg, u_int msglen,
            u_char * digest, size_t * digestlen)
{
    int               rval = SNMPERR_SUCCESS;
    CK_SESSION_HANDLE hSession;
    CK_MECHANISM      mech;
    if (get_session_handle(mech_type, CKF_DIGEST, &hSession) != CKR_OK ||
        hSession == NULL) {
        QUITFUN(SNMPERR_GENERR, pkcs_digest_quit);
    }

    mech.mechanism = mech_type;
    mech.pParameter = NULL_PTR;
    mech.ulParameterLen = 0;

    /* initialize a message-digesting operation */
    if (C_DigestInit(hSession, &mech)!= CKR_OK ) {
        QUITFUN(SNMPERR_GENERR, pkcs_digest_quit);
    }
    /* continue a multiple-part message-digesting operation */
    if (C_DigestUpdate(hSession, msg, msglen) != CKR_OK ) {
        QUITFUN(SNMPERR_GENERR, pkcs_digest_quit);
    }
    /* finish a multiple-part message-digesting operation */
    if (C_DigestFinal(hSession, digest, digestlen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_digest_quit);
    }

  pkcs_digest_quit:
    return rval;
}

/*
 * encrypt plaintext into ciphertext using key and iv.   
 */
int
pkcs_encrpyt(CK_MECHANISM_TYPE mech_type, u_char * key, u_int keylen,
             u_char * iv, u_int ivlen,
             u_char * plaintext, u_int ptlen,
             u_char * ciphertext, size_t * ctlen)
{
    int                rval = SNMPERR_SUCCESS;
    int                pad_size, offset;
    /*
     * Key template 
     */
    CK_OBJECT_CLASS class = CKO_SECRET_KEY;
    CK_KEY_TYPE keytype = CKK_DES;
    /* CK_KEY_TYPE AESkeytype = CKK_AES; */
    CK_BBOOL truevalue = TRUE;
    CK_BBOOL falsevalue = FALSE;

    CK_ATTRIBUTE template[] = {
        {CKA_CLASS, &class, sizeof (class)},
        {CKA_KEY_TYPE, &keytype, sizeof (keytype)},
        {CKA_ENCRYPT, &truevalue, sizeof (truevalue)},
        {CKA_TOKEN, &falsevalue, sizeof (falsevalue)},
        {CKA_VALUE, key, keylen} 
    };

    CK_SESSION_HANDLE hSession;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE hkey = (CK_OBJECT_HANDLE) 0;

    if (get_session_handle(mech_type, CKF_ENCRYPT,
                           &hSession) != CKR_OK ||
        hSession == NULL) {
        QUITFUN(SNMPERR_GENERR, pkcs_encrypt_quit);
    }

    if (C_CreateObject(hSession, template,
        (sizeof (template) / sizeof (CK_ATTRIBUTE)),
                                &hkey) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_encrypt_quit);
    }

    mech.mechanism = mech_type;
    mech.pParameter = iv;
    mech.ulParameterLen = ivlen;

    /* initialize an encryption operation */
    if (C_EncryptInit(hSession, &mech, hkey) != CKR_OK ) {
        QUITFUN(SNMPERR_GENERR, pkcs_encrypt_quit);
    }

    /* for DES */
    pad_size = BYTESIZE(SNMP_TRANS_PRIVLEN_1DES);

    if (ptlen + pad_size - ptlen % pad_size > *ctlen) {
        QUITFUN(SNMPERR_GENERR, pkcs_encrypt_quit);    
    }

    for (offset = 0; offset < ptlen; offset += pad_size) {
        /* continue a multiple-part encryption operation */
        if (C_EncryptUpdate(hSession, plaintext + offset, pad_size,
                            ciphertext + offset, ctlen) != CKR_OK) {
            QUITFUN(SNMPERR_GENERR, pkcs_encrypt_quit);
        }
    }

    /* finish a multiple-part encryption operation */
    if (C_EncryptFinal(hSession, ciphertext + offset, ctlen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_encrypt_quit);
    }
    *ctlen = offset;

  pkcs_encrypt_quit:
    if (key != (CK_OBJECT_HANDLE) 0) {
        (void) C_DestroyObject(hSession, hkey);
    }
    return rval;
}

/* 
 * decrypt ciphertext into plaintext using key and iv.
 */
int
pkcs_decrpyt(CK_MECHANISM_TYPE mech_type, u_char * key, u_int keylen,
             u_char * iv, u_int ivlen,
             u_char * ciphertext, u_int ctlen,
             u_char * plaintext, size_t * ptlen)
{
    int            rval = SNMPERR_SUCCESS;
    /*
     * Key template 
     */
    CK_OBJECT_CLASS class = CKO_SECRET_KEY;
    CK_KEY_TYPE keytype = CKK_DES;
    /* CK_KEY_TYPE AESkeytype = CKK_AES; */
    CK_BBOOL truevalue = TRUE;
    CK_BBOOL falsevalue= FALSE;
    CK_ATTRIBUTE template[] = {
        {CKA_CLASS, &class, sizeof (class)},
        {CKA_KEY_TYPE, &keytype, sizeof (keytype)},
        {CKA_DECRYPT, &truevalue, sizeof (truevalue)},
        {CKA_TOKEN, &falsevalue, sizeof (falsevalue)},
        {CKA_VALUE, key, keylen}
    };
    CK_SESSION_HANDLE hSession;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE hkey = (CK_OBJECT_HANDLE) 0;

    if (get_session_handle(mech_type, CKF_DECRYPT, &hSession) != CKR_OK ||
        hSession == NULL) {
        QUITFUN(SNMPERR_GENERR, pkcs_decrypt_quit);
    }

    if (C_CreateObject(hSession, template,
        (sizeof (template) / sizeof (CK_ATTRIBUTE)), &hkey) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_decrypt_quit);
    }

    mech.mechanism = mech_type;
    mech.pParameter = iv;
    mech.ulParameterLen = ivlen;

    /* initialize a decryption operation */
    if (C_DecryptInit(hSession, &mech, hkey) != CKR_OK ) {
        QUITFUN(SNMPERR_GENERR, pkcs_decrypt_quit);
    }
    /* continue a multiple-part decryption operation */
    if (C_DecryptUpdate(hSession, ciphertext, ctlen, plaintext,
                                        ptlen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_decrypt_quit);
    }
    /* finish a multiple-part decryption operation */
    if (C_DecryptFinal(hSession, plaintext, ptlen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_decrypt_quit);
    }

  pkcs_decrypt_quit:
    if (key != (CK_OBJECT_HANDLE) 0) {
        (void) C_DestroyObject(hSession, hkey);
    }
    return rval;
}

/*
 * Convert a passphrase into a master user key, Ku, according to the
 * algorithm given in RFC 2274 concerning the SNMPv3 User Security Model (USM)
 */
int
pkcs_generate_Ku(CK_MECHANISM_TYPE mech_type, u_char * passphrase, u_int pplen,
                 u_char * Ku, size_t * kulen)
{
    int                rval = SNMPERR_SUCCESS, nbytes = USM_LENGTH_EXPANDED_PASSPHRASE;
    CK_SESSION_HANDLE hSession;
    CK_MECHANISM mech;
    u_int        i, pindex = 0;
    u_char        buf[USM_LENGTH_KU_HASHBLOCK], *bufp;

    if (get_session_handle(mech_type, CKF_DIGEST, &hSession) != CKR_OK ||
        hSession == NULL) {
        QUITFUN(SNMPERR_GENERR, pkcs_generate_Ku_quit);
    }

    mech.mechanism = mech_type;
    mech.pParameter = NULL_PTR;
    mech.ulParameterLen = 0;

    /* initialize a message-digesting operation */
    if (C_DigestInit(hSession, &mech)!= CKR_OK ) {
        QUITFUN(SNMPERR_GENERR, pkcs_generate_Ku_quit);
    }

    while (nbytes > 0) {
        bufp = buf;
        for (i = 0; i < USM_LENGTH_KU_HASHBLOCK; i++) {
           /*
	    * fill a buffer with the supplied passphrase.  When the end
            * of the passphrase is reachedcycle back to the beginning.
            */
            *bufp++ = passphrase[pindex++ % pplen];
        }
        /* continue a multiple-part message-digesting operation */
        if (C_DigestUpdate(hSession, buf, USM_LENGTH_KU_HASHBLOCK) != CKR_OK ) {
            QUITFUN(SNMPERR_GENERR, pkcs_generate_Ku_quit);
        }
        nbytes -= USM_LENGTH_KU_HASHBLOCK;
    }
    /* finish a multiple-part message-digesting operation */
    if (C_DigestFinal(hSession, Ku, kulen) != CKR_OK) {
        QUITFUN(SNMPERR_GENERR, pkcs_generate_Ku_quit);
    }

  pkcs_generate_Ku_quit:
    return rval;
}
   
/*
 * pkcserr_stringor: returns a string representation of the given 
 * return code.
 */
static char *
pkcserr_string(CK_RV rv)
{
    static char errstr[128];
    switch (rv) {
    case CKR_OK:
        return ("CKR_OK");
        break;
    case CKR_CANCEL:
        return ("CKR_CANCEL");
        break;
    case CKR_HOST_MEMORY:
        return ("CKR_HOST_MEMORY");
        break;
    case CKR_SLOT_ID_INVALID:
        return ("CKR_SLOT_ID_INVALID");
        break;
    case CKR_GENERAL_ERROR:
        return ("CKR_GENERAL_ERROR");
        break;
    case CKR_FUNCTION_FAILED:
        return ("CKR_FUNCTION_FAILED");
        break;
    case CKR_ARGUMENTS_BAD:
        return ("CKR_ARGUMENTS_BAD");
        break;
    case CKR_NO_EVENT:
        return ("CKR_NO_EVENT");
        break;
    case CKR_NEED_TO_CREATE_THREADS:
        return ("CKR_NEED_TO_CREATE_THREADS");
        break;
    case CKR_CANT_LOCK:
        return ("CKR_CANT_LOCK");
        break;
    case CKR_ATTRIBUTE_READ_ONLY:
        return ("CKR_ATTRIBUTE_READ_ONLY");
        break;
    case CKR_ATTRIBUTE_SENSITIVE:
        return ("CKR_ATTRIBUTE_SENSITIVE");
        break;
    case CKR_ATTRIBUTE_TYPE_INVALID:
        return ("CKR_ATTRIBUTE_TYPE_INVALID");
        break;
    case CKR_ATTRIBUTE_VALUE_INVALID:
        return ("CKR_ATTRIBUTE_VALUE_INVALID");
        break;
    case CKR_DATA_INVALID:
        return ("CKR_DATA_INVALID");
        break;
    case CKR_DATA_LEN_RANGE:
        return ("CKR_DATA_LEN_RANGE");
        break;
    case CKR_DEVICE_ERROR:
        return ("CKR_DEVICE_ERROR");
        break;
    case CKR_DEVICE_MEMORY:
        return ("CKR_DEVICE_MEMORY");
        break;
    case CKR_DEVICE_REMOVED:
        return ("CKR_DEVICE_REMOVED");
        break;
    case CKR_ENCRYPTED_DATA_INVALID:
        return ("CKR_ENCRYPTED_DATA_INVALID");
        break;
    case CKR_ENCRYPTED_DATA_LEN_RANGE:
        return ("CKR_ENCRYPTED_DATA_LEN_RANGE");
        break;
    case CKR_FUNCTION_CANCELED:
        return ("CKR_FUNCTION_CANCELED");
        break;
    case CKR_FUNCTION_NOT_PARALLEL:
        return ("CKR_FUNCTION_NOT_PARALLEL");
        break;
    case CKR_FUNCTION_NOT_SUPPORTED:
        return ("CKR_FUNCTION_NOT_SUPPORTED");
        break;
    case CKR_KEY_HANDLE_INVALID:
        return ("CKR_KEY_HANDLE_INVALID");
        break;
    case CKR_KEY_SIZE_RANGE:
        return ("CKR_KEY_SIZE_RANGE");
        break;
    case CKR_KEY_TYPE_INCONSISTENT:
        return ("CKR_KEY_TYPE_INCONSISTENT");
        break;
    case CKR_KEY_NOT_NEEDED:
        return ("CKR_KEY_NOT_NEEDED");
        break;
    case CKR_KEY_CHANGED:
        return ("CKR_KEY_CHANGED");
        break;
    case CKR_KEY_NEEDED:
        return ("CKR_KEY_NEEDED");
        break;
    case CKR_KEY_INDIGESTIBLE:
        return ("CKR_KEY_INDIGESTIBLE");
        break;
    case CKR_KEY_FUNCTION_NOT_PERMITTED:
        return ("CKR_KEY_FUNCTION_NOT_PERMITTED");
        break;
    case CKR_KEY_NOT_WRAPPABLE:
        return ("CKR_KEY_NOT_WRAPPABLE");
        break;
    case CKR_KEY_UNEXTRACTABLE:
        return ("CKR_KEY_UNEXTRACTABLE");
        break;
    case CKR_MECHANISM_INVALID:
        return ("CKR_MECHANISM_INVALID");
        break;
    case CKR_MECHANISM_PARAM_INVALID:
        return ("CKR_MECHANISM_PARAM_INVALID");
        break;
    case CKR_OBJECT_HANDLE_INVALID:
        return ("CKR_OBJECT_HANDLE_INVALID");
        break;
    case CKR_OPERATION_ACTIVE:
        return ("CKR_OPERATION_ACTIVE");
        break;
    case CKR_OPERATION_NOT_INITIALIZED:
        return ("CKR_OPERATION_NOT_INITIALIZED");
        break;
    case CKR_PIN_INCORRECT:
        return ("CKR_PIN_INCORRECT");
        break;
    case CKR_PIN_INVALID:
        return ("CKR_PIN_INVALID");
        break;
    case CKR_PIN_LEN_RANGE:
        return ("CKR_PIN_LEN_RANGE");
        break;
    case CKR_PIN_EXPIRED:
        return ("CKR_PIN_EXPIRED");
        break;
    case CKR_PIN_LOCKED:
        return ("CKR_PIN_LOCKED");
        break;
    case CKR_SESSION_CLOSED:
        return ("CKR_SESSION_CLOSED");
        break;
    case CKR_SESSION_COUNT:
        return ("CKR_SESSION_COUNT");
        break;
    case CKR_SESSION_HANDLE_INVALID:
        return ("CKR_SESSION_HANDLE_INVALID");
        break;
    case CKR_SESSION_PARALLEL_NOT_SUPPORTED:
        return ("CKR_SESSION_PARALLEL_NOT_SUPPORTED");
        break;
    case CKR_SESSION_READ_ONLY:
        return ("CKR_SESSION_READ_ONLY");
        break;
    case CKR_SESSION_EXISTS:
        return ("CKR_SESSION_EXISTS");
        break;
    case CKR_SESSION_READ_ONLY_EXISTS:
        return ("CKR_SESSION_READ_ONLY_EXISTS");
        break;
    case CKR_SESSION_READ_WRITE_SO_EXISTS:
        return ("CKR_SESSION_READ_WRITE_SO_EXISTS");
        break;
    case CKR_SIGNATURE_INVALID:
        return ("CKR_SIGNATURE_INVALID");
        break;
    case CKR_SIGNATURE_LEN_RANGE:
        return ("CKR_SIGNATURE_LEN_RANGE");
        break;
    case CKR_TEMPLATE_INCOMPLETE:
        return ("CKR_TEMPLATE_INCOMPLETE");
        break;
    case CKR_TEMPLATE_INCONSISTENT:
        return ("CKR_TEMPLATE_INCONSISTENT");
        break;
    case CKR_TOKEN_NOT_PRESENT:
        return ("CKR_TOKEN_NOT_PRESENT");
        break;
    case CKR_TOKEN_NOT_RECOGNIZED:
        return ("CKR_TOKEN_NOT_RECOGNIZED");
        break;
    case CKR_TOKEN_WRITE_PROTECTED:
        return ("CKR_TOKEN_WRITE_PROTECTED");
        break;
    case CKR_UNWRAPPING_KEY_HANDLE_INVALID:
        return ("CKR_UNWRAPPING_KEY_HANDLE_INVALID");
        break;
    case CKR_UNWRAPPING_KEY_SIZE_RANGE:
        return ("CKR_UNWRAPPING_KEY_SIZE_RANGE");
        break;
    case CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT:
        return ("CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT");
        break;
    case CKR_USER_ALREADY_LOGGED_IN:
        return ("CKR_USER_ALREADY_LOGGED_IN");
        break;
    case CKR_USER_NOT_LOGGED_IN:
        return ("CKR_USER_NOT_LOGGED_IN");
        break;
    case CKR_USER_PIN_NOT_INITIALIZED:
        return ("CKR_USER_PIN_NOT_INITIALIZED");
        break;
    case CKR_USER_TYPE_INVALID:
        return ("CKR_USER_TYPE_INVALID");
        break;
    case CKR_USER_ANOTHER_ALREADY_LOGGED_IN:
        return ("CKR_USER_ANOTHER_ALREADY_LOGGED_IN");
        break;
    case CKR_USER_TOO_MANY_TYPES:
        return ("CKR_USER_TOO_MANY_TYPES");
        break;
    case CKR_WRAPPED_KEY_INVALID:
        return ("CKR_WRAPPED_KEY_INVALID");
        break;
    case CKR_WRAPPED_KEY_LEN_RANGE:
        return ("CKR_WRAPPED_KEY_LEN_RANGE");
        break;
    case CKR_WRAPPING_KEY_HANDLE_INVALID:
        return ("CKR_WRAPPING_KEY_HANDLE_INVALID");
        break;
    case CKR_WRAPPING_KEY_SIZE_RANGE:
        return ("CKR_WRAPPING_KEY_SIZE_RANGE");
        break;
    case CKR_WRAPPING_KEY_TYPE_INCONSISTENT:
        return ("CKR_WRAPPING_KEY_TYPE_INCONSISTENT");
        break;
    case CKR_RANDOM_SEED_NOT_SUPPORTED:
        return ("CKR_RANDOM_SEED_NOT_SUPPORTED");
        break;
    case CKR_RANDOM_NO_RNG:
        return ("CKR_RANDOM_NO_RNG");
        break;
    case CKR_DOMAIN_PARAMS_INVALID:
        return ("CKR_DOMAIN_PARAMS_INVALID");
        break;
    case CKR_BUFFER_TOO_SMALL:
        return ("CKR_BUFFER_TOO_SMALL");
        break;
    case CKR_SAVED_STATE_INVALID:
        return ("CKR_SAVED_STATE_INVALID");
        break;
    case CKR_INFORMATION_SENSITIVE:
        return ("CKR_INFORMATION_SENSITIVE");
        break;
    case CKR_STATE_UNSAVEABLE:
        return ("CKR_STATE_UNSAVEABLE");
        break;
    case CKR_CRYPTOKI_NOT_INITIALIZED:
        return ("CKR_CRYPTOKI_NOT_INITIALIZED");
        break;
    case CKR_CRYPTOKI_ALREADY_INITIALIZED:
        return ("CKR_CRYPTOKI_ALREADY_INITIALIZED");
        break;
    case CKR_MUTEX_BAD:
        return ("CKR_MUTEX_BAD");
        break;
    case CKR_MUTEX_NOT_LOCKED:
        return ("CKR_MUTEX_NOT_LOCKED");
        break;
    case CKR_VENDOR_DEFINED:
        return ("CKR_VENDOR_DEFINED");
        break;
    default:
        /* rv not found */
        snprintf(errstr, sizeof (errstr),
            "Unknown return code: 0x%x", rv);
        return (errstr);
        break;
    }
}
#else
int pkcs_unused;	/* Suppress "empty translation unit" warning */
#endif
