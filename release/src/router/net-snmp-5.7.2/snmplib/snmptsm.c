/*
 * snmptsmsm.c -- Implements RFC #5591
 *
 * This code implements a security model that assumes the local user
 * that executed the agent is the user who's attributes are passed up
 * by the transport underneath.  The RFC describing this security
 * model is RFC5591.
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/library/snmptsm.h>

#ifdef NETSNMP_TRANSPORT_SSH_DOMAIN
#include <net-snmp/library/snmpSSHDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_DTLSUDP_DOMAIN
#include <net-snmp/library/snmpDTLSUDPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_TLSTCP_DOMAIN
#include <net-snmp/library/snmpTLSTCPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_DTLSSCTP_DOMAIN
#include <net-snmp/library/snmpDTLSSCTPDomain.h>
#endif

netsnmp_feature_require(snmpv3_probe_contextEngineID_rfc5343)
netsnmp_feature_require(row_create)

#include <unistd.h>

static int      tsm_session_init(netsnmp_session *);
static void     tsm_free_state_ref(void *);
static int      tsm_clone_pdu(netsnmp_pdu *, netsnmp_pdu *);
static int      tsm_free_pdu(netsnmp_pdu *pdu);

u_int next_sess_id = 1;

/** Initialize the TSM security module */
void
init_tsm(void)
{
    struct snmp_secmod_def *def;
    int ret;

    def = SNMP_MALLOC_STRUCT(snmp_secmod_def);

    if (!def) {
        snmp_log(LOG_ERR,
                 "Unable to malloc snmp_secmod struct, not registering TSM\n");
        return;
    }

    def->encode_reverse = tsm_rgenerate_out_msg;
    def->decode = tsm_process_in_msg;
    def->session_open = tsm_session_init;
    def->pdu_free_state_ref = tsm_free_state_ref;
    def->pdu_clone = tsm_clone_pdu;
    def->pdu_free = tsm_free_pdu;
    def->probe_engineid = snmpv3_probe_contextEngineID_rfc5343;

    DEBUGMSGTL(("tsm","registering ourselves\n"));
    ret = register_sec_mod(SNMP_SEC_MODEL_TSM, "tsm", def);
    DEBUGMSGTL(("tsm"," returned %d\n", ret));

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "tsmUseTransportPrefix",
			       NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_TSM_USE_PREFIX);
}

/** shutdown the TSM security module */
void
shutdown_tsm(void)
{
}

/*
 * Initialize specific session information (right now, just set up things to
 * not do an engineID probe)
 */

static int
tsm_session_init(netsnmp_session * sess)
{
    DEBUGMSGTL(("tsm",
                "TSM: Reached our session initialization callback\n"));

    sess->flags |= SNMP_FLAGS_DONT_PROBE;

    /* XXX: likely needed for something: */
    /*
    tsmsession = sess->securityInfo =
    if (!tsmsession)
        return SNMPERR_GENERR;
    */

    return SNMPERR_SUCCESS;
}

/** Free our state information (this is only done on the agent side) */
static void
tsm_free_state_ref(void *ptr)
{
    netsnmp_tsmSecurityReference *tsmRef;

    if (NULL == ptr)
        return;

    tsmRef = (netsnmp_tsmSecurityReference *) ptr;
    /* the tmStateRef is always taken care of by the normal PDU, since this
       is just a reference to that one */
    /* DON'T DO: SNMP_FREE(tsmRef->tmStateRef); */
    /* SNMP_FREE(tsmRef);  ? */
}

static int
tsm_free_pdu(netsnmp_pdu *pdu)
{
    /* free the security reference */
    if (pdu->securityStateRef) {
        tsm_free_state_ref(pdu->securityStateRef);
        pdu->securityStateRef = NULL;
    }
    return 0;
}

/** This is called when a PDU is cloned (to increase reference counts) */
static int
tsm_clone_pdu(netsnmp_pdu *pdu, netsnmp_pdu *pdu2)
{
    netsnmp_tsmSecurityReference *oldref, *newref;

    oldref = pdu->securityStateRef;
    if (!oldref)
        return SNMPERR_SUCCESS;

    newref = SNMP_MALLOC_TYPEDEF(netsnmp_tsmSecurityReference);
    DEBUGMSGTL(("tsm", "cloned as pdu=%p, ref=%p (oldref=%p)\n",
            pdu2, newref, pdu2->securityStateRef));
    if (!newref)
        return SNMPERR_GENERR;
    
    memcpy(newref, oldref, sizeof(*oldref));

    pdu2->securityStateRef = newref;

    /* the tm state reference is just a link to the one in the pdu,
       which was already copied by snmp_clone_pdu before handing it to
       us. */

    memdup((u_char **) &newref->tmStateRef, oldref->tmStateRef,
           sizeof(*oldref->tmStateRef));
    return SNMPERR_SUCCESS;
}

/* asn.1 easing definitions */
#define TSMBUILD_OR_ERR(fun, args, msg, desc)       \
    DEBUGDUMPHEADER("send", desc); \
    rc = fun args;            \
    DEBUGINDENTLESS();        \
    if (rc == 0) { \
        DEBUGMSGTL(("tsm",msg)); \
        retval = SNMPERR_TOO_LONG; \
        goto outerr; \
    }

/****************************************************************************
 *
 * tsm_generate_out_msg
 *
 * Parameters:
 *	(See list below...)
 *
 * Returns:
 *	SNMPERR_SUCCESS                        On success.
 *	... and others
 *
 *
 * Generate an outgoing message.
 *
 ****************************************************************************/

int
tsm_rgenerate_out_msg(struct snmp_secmod_outgoing_params *parms)
{
    u_char         **wholeMsg = parms->wholeMsg;
    size_t	   *offset = parms->wholeMsgOffset;
    int rc;
    
    size_t         *wholeMsgLen = parms->wholeMsgLen;
    netsnmp_tsmSecurityReference *tsmSecRef;
    netsnmp_tmStateReference *tmStateRef;
    int             tmStateRefLocal = 0;
    
    DEBUGMSGTL(("tsm", "Starting TSM processing\n"));

    /* if we have this, then this message to be sent is in response to
       something that came in earlier and the tsmSecRef was created by
       the tsm_process_in_msg. */
    tsmSecRef = parms->secStateRef;
    
    if (tsmSecRef) {
        /* 4.2, step 1: If there is a securityStateReference (Response
           or Report message), then this Security Model uses the
           cached information rather than the information provided by
           the ASI. */

        /* 4.2, step 1: Extract the tmStateReference from the
           securityStateReference cache. */
        netsnmp_assert_or_return(NULL != tsmSecRef->tmStateRef, SNMPERR_GENERR);
        tmStateRef = tsmSecRef->tmStateRef;

        /* 4.2 step 1: Set the tmRequestedSecurityLevel to the value
           of the extracted tmTransportSecurityLevel. */
        tmStateRef->requestedSecurityLevel = tmStateRef->transportSecurityLevel;

        /* 4.2 step 1: Set the tmSameSecurity parameter in the
           tmStateReference cache to true. */
        tmStateRef->sameSecurity = NETSNMP_TM_USE_SAME_SECURITY;

        /* 4.2 step 1: The cachedSecurityData for this message can
           now be discarded. */
        SNMP_FREE(parms->secStateRef);
    } else {
        /* 4.2, step 2: If there is no securityStateReference (e.g., a
           Request-type or Notification message), then create a
           tmStateReference cache. */
        tmStateRef = SNMP_MALLOC_TYPEDEF(netsnmp_tmStateReference);
        netsnmp_assert_or_return(NULL != tmStateRef, SNMPERR_GENERR);
        tmStateRefLocal = 1;

        /* XXX: we don't actually use this really in our implementation */
        /* 4.2, step 2: Set tmTransportDomain to the value of
           transportDomain, tmTransportAddress to the value of
           transportAddress */

        /* 4.2, step 2: and tmRequestedSecurityLevel to the value of
           securityLevel. */
        tmStateRef->requestedSecurityLevel = parms->secLevel;

        /* 4.2, step 2: Set the transaction-specific tmSameSecurity
           parameter to false. */
        tmStateRef->sameSecurity = NETSNMP_TM_SAME_SECURITY_NOT_REQUIRED;

        if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                   NETSNMP_DS_LIB_TSM_USE_PREFIX)) {
            /* XXX: probably shouldn't be a hard-coded list of
               supported transports */
            /* 4.2, step 2: If the snmpTsmConfigurationUsePrefix
               object is set to true, then use the transportDomain to
               look up the corresponding prefix. */
            const char *prefix;
            if (strncmp("ssh:",parms->session->peername,4) == 0)
                prefix = "ssh:";
            else if (strncmp("dtls:",parms->session->peername,5) == 0)
                prefix = "dtls:";
            else if (strncmp("tls:",parms->session->peername,4) == 0)
                prefix = "tls:";
            else {
                /* 4.2, step 2: If the prefix lookup fails for any
                   reason, then the snmpTsmUnknownPrefixes counter is
                   incremented, an error indication is returned to the
                   calling module, and message processing stops. */
                snmp_increment_statistic(STAT_TSM_SNMPTSMUNKNOWNPREFIXES);
                SNMP_FREE(tmStateRef);
                return SNMPERR_GENERR;
            }

            /* 4.2, step 2: If the lookup succeeds, but there is no
               prefix in the securityName, or the prefix returned does
               not match the prefix in the securityName, or the length
               of the prefix is less than 1 or greater than 4 US-ASCII
               alpha-numeric characters, then the
               snmpTsmInvalidPrefixes counter is incremented, an error
               indication is returned to the calling module, and
               message processing stops. */
            if (strchr(parms->secName, ':') == 0 ||
                strlen(prefix)+1 >= parms->secNameLen ||
                strncmp(parms->secName, prefix, strlen(prefix)) != 0 ||
                parms->secName[strlen(prefix)] != ':') {
                /* Note: since we're assiging the prefixes above the
                   prefix lengths always meet the 1-4 criteria */
                snmp_increment_statistic(STAT_TSM_SNMPTSMINVALIDPREFIXES);
                SNMP_FREE(tmStateRef);
                return SNMPERR_GENERR;
            }

            /* 4.2, step 2: Strip the transport-specific prefix and
               trailing ':' character (US-ASCII 0x3a) from the
               securityName.  Set tmSecurityName to the value of
               securityName. */
            memcpy(tmStateRef->securityName,
                   parms->secName + strlen(prefix) + 1,
                   parms->secNameLen - strlen(prefix) - 1);
            tmStateRef->securityNameLen = parms->secNameLen - strlen(prefix) -1;
        } else {
            /* 4.2, step 2: If the snmpTsmConfigurationUsePrefix object is
               set to false, then set tmSecurityName to the value
               of securityName. */
            memcpy(tmStateRef->securityName, parms->secName,
                   parms->secNameLen);
            tmStateRef->securityNameLen = parms->secNameLen;
        }
    }

    /* truncate the security name with a '\0' for safety */
    tmStateRef->securityName[tmStateRef->securityNameLen] = '\0';

    /* 4.2, step 3: Set securityParameters to a zero-length OCTET
     *  STRING ('0400').
     */
    DEBUGDUMPHEADER("send", "tsm security parameters");
    rc = asn_realloc_rbuild_header(wholeMsg, wholeMsgLen, offset, 1,
                                     (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                             | ASN_OCTET_STR), 0);
    DEBUGINDENTLESS();
    if (rc == 0) {
        DEBUGMSGTL(("tsm", "building msgSecurityParameters failed.\n"));
        if (tmStateRefLocal)
            SNMP_FREE(tmStateRef);
        return SNMPERR_TOO_LONG;
    }
    
    /* 4.2, step 4: Combine the message parts into a wholeMsg and
       calculate wholeMsgLength.
     */
    while ((*wholeMsgLen - *offset) < parms->globalDataLen) {
        if (!asn_realloc(wholeMsg, wholeMsgLen)) {
            DEBUGMSGTL(("tsm", "building global data failed.\n"));
            if (tmStateRefLocal)
                SNMP_FREE(tmStateRef);
            return SNMPERR_TOO_LONG;
        }
    }

    *offset += parms->globalDataLen;
    memcpy(*wholeMsg + *wholeMsgLen - *offset,
           parms->globalData, parms->globalDataLen);

    /* 4.2, step 5: The wholeMsg, wholeMsgLength, securityParameters,
       and tmStateReference are returned to the calling Message
       Processing Model with the statusInformation set to success. */

    /* For the Net-SNMP implemantion that actually means we start
       encoding the full packet sequence from here before returning it */

    /*
     * Total packet sequence.  
     */
    rc = asn_realloc_rbuild_sequence(wholeMsg, wholeMsgLen, offset, 1,
                                     (u_char) (ASN_SEQUENCE |
                                               ASN_CONSTRUCTOR), *offset);
    if (rc == 0) {
        DEBUGMSGTL(("tsm", "building master packet sequence failed.\n"));
        if (tmStateRefLocal)
            SNMP_FREE(tmStateRef);
        return SNMPERR_TOO_LONG;
    }

    if (parms->pdu->transport_data &&
        parms->pdu->transport_data != tmStateRef) {
        snmp_log(LOG_ERR, "tsm: needed to free transport data\n");
        SNMP_FREE(parms->pdu->transport_data);
    }

    /* put the transport state reference into the PDU for the transport */
    if (SNMPERR_SUCCESS !=
        memdup((u_char **) &parms->pdu->transport_data,
               tmStateRef, sizeof(*tmStateRef))) {
        snmp_log(LOG_ERR, "tsm: malloc failure\n");
    }
    parms->pdu->transport_data_length = sizeof(*tmStateRef);

    if (tmStateRefLocal)
        SNMP_FREE(tmStateRef);
    DEBUGMSGTL(("tsm", "TSM processing completed.\n"));
    return SNMPERR_SUCCESS;
}

/****************************************************************************
 *
 * tsm_process_in_msg
 *
 * Parameters:
 *	(See list below...)
 *
 * Returns:
 *	TSM_ERR_NO_ERROR                        On success.
 *	TSM_ERR_GENERIC_ERROR
 *	TSM_ERR_UNSUPPORTED_SECURITY_LEVEL
 *
 *
 * Processes an incoming message.
 *
 ****************************************************************************/

int
tsm_process_in_msg(struct snmp_secmod_incoming_params *parms)
{
    u_char type_value;
    size_t remaining;
    u_char *data_ptr;
    netsnmp_tmStateReference *tmStateRef;
    netsnmp_tsmSecurityReference *tsmSecRef;
    u_char          ourEngineID[SNMP_MAX_ENG_SIZE];
    static size_t   ourEngineID_len = sizeof(ourEngineID);
    
    /* Section 5.2, step 1: Set the securityEngineID to the local
       snmpEngineID. */
    ourEngineID_len =
        snmpv3_get_engineID((u_char*) ourEngineID, ourEngineID_len);
    netsnmp_assert_or_return(ourEngineID_len != 0 &&
                             ourEngineID_len <= *parms->secEngineIDLen,
                             SNMPERR_GENERR);
    memcpy(parms->secEngineID, ourEngineID, *parms->secEngineIDLen);

    /* Section 5.2, step 2: If tmStateReference does not refer to a
       cache containing values for tmTransportDomain,
       tmTransportAddress, tmSecurityName, and
       tmTransportSecurityLevel, then the snmpTsmInvalidCaches counter
       is incremented, an error indication is returned to the calling
       module, and Security Model processing stops for this
       message. */
    if (!parms->pdu->transport_data ||
        sizeof(netsnmp_tmStateReference) !=
        parms->pdu->transport_data_length) {
        /* if we're not coming in over a proper transport; bail! */
        DEBUGMSGTL(("tsm","improper transport data\n"));
        return -1;
    }
    tmStateRef = (netsnmp_tmStateReference *) parms->pdu->transport_data;
    parms->pdu->transport_data = NULL;

    if (tmStateRef == NULL ||
        /* not needed: tmStateRef->transportDomain == NULL || */
        /* not needed: tmStateRef->transportAddress == NULL || */
        tmStateRef->securityName[0] == '\0'
        ) {
        snmp_increment_statistic(STAT_TSM_SNMPTSMINVALIDCACHES);
        return SNMPERR_GENERR;
    }

    /* Section 5.2, step 3: Copy the tmSecurityName to securityName. */
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_TSM_USE_PREFIX)) {
        /* Section 5.2, step 3:
          If the snmpTsmConfigurationUsePrefix object is set to true, then
          use the tmTransportDomain to look up the corresponding prefix.
        */
        const char *prefix = NULL;
        /*
          possibilities:
           |--------------------+-------|
           | snmpTLSTCPDomain   | tls:  |
           | snmpDTLSUDPDomain  | dtls: |
           | snmpSSHDomain      | ssh:  |
           |--------------------+-------|
        */
        
        if (tmStateRef->transportDomain == NULL) {
            /* XXX: snmpTsmInvalidCaches++ ??? */
            return SNMPERR_GENERR;
        }

        /* XXX: cache in session! */
#ifdef NETSNMP_TRANSPORT_SSH_DOMAIN
        if (netsnmp_oid_equals(netsnmp_snmpSSHDomain,
                               netsnmp_snmpSSHDomain_len,
                               tmStateRef->transportDomain,
                               tmStateRef->transportDomainLen) == 0) {
            prefix = "ssh";
        }
#endif /*  NETSNMP_TRANSPORT_SSH_DOMAIN */

#ifdef NETSNMP_TRANSPORT_DTLSUDP_DOMAIN
        if (netsnmp_oid_equals(netsnmpDTLSUDPDomain,
                               netsnmpDTLSUDPDomain_len,
                               tmStateRef->transportDomain,
                               tmStateRef->transportDomainLen) == 0) {
            
            prefix = "dtls";
        }
#endif /* NETSNMP_TRANSPORT_DTLSUDP_DOMAIN */

#ifdef NETSNMP_TRANSPORT_TLSTCP_DOMAIN
        if (netsnmp_oid_equals(netsnmpTLSTCPDomain,
                               netsnmpTLSTCPDomain_len,
                               tmStateRef->transportDomain,
                               tmStateRef->transportDomainLen) == 0) {
            
            prefix = "tls";
        }
#endif /* NETSNMP_TRANSPORT_TLSTCP_DOMAIN */

        /* Section 5.2, step 3:
          If the prefix lookup fails for any reason, then the
          snmpTsmUnknownPrefixes counter is incremented, an error
          indication is returned to the calling module, and message
          processing stops.
        */
        if (prefix == NULL) {
            snmp_increment_statistic(STAT_TSM_SNMPTSMUNKNOWNPREFIXES);
            return SNMPERR_GENERR;
        }

        /* Section 5.2, step 3:
          If the lookup succeeds but the prefix length is less than 1 or
          greater than 4 octets, then the snmpTsmInvalidPrefixes counter
          is incremented, an error indication is returned to the calling
          module, and message processing stops.
        */
#ifdef NOT_USING_HARDCODED_PREFIXES
        /* the above code actually ensures this will never happen as
           we don't support a dynamic prefix database where this might
           happen. */
        if (strlen(prefix) < 1 || strlen(prefix) > 4) {
            /* XXX: snmpTsmInvalidPrefixes++ */
            return SNMPERR_GENERR;
        }
#endif
        
        /* Section 5.2, step 3:
          Set the securityName to be the concatenation of the prefix, a
          ':' character (US-ASCII 0x3a), and the tmSecurityName.
        */
        snprintf(parms->secName, *parms->secNameLen,
                 "%s:%s", prefix, tmStateRef->securityName);
    } else {
        /* if the use prefix flag wasn't set, do a straight copy */
        strncpy(parms->secName, tmStateRef->securityName, *parms->secNameLen);
    }

    /* set the length of the security name */
    *parms->secNameLen = strlen(parms->secName);
    DEBUGMSGTL(("tsm", "user: %s/%d\n", parms->secName, (int)*parms->secNameLen));

    /* Section 5.2 Step 4:
       Compare the value of tmTransportSecurityLevel in the
       tmStateReference cache to the value of the securityLevel
       parameter passed in the processIncomingMsg ASI.  If securityLevel
       specifies privacy (Priv) and tmTransportSecurityLevel specifies
       no privacy (noPriv), or if securityLevel specifies authentication
       (auth) and tmTransportSecurityLevel specifies no authentication
       (noAuth) was provided by the Transport Model, then the
       snmpTsmInadequateSecurityLevels counter is incremented, an error
       indication (unsupportedSecurityLevel) together with the OID and
       value of the incremented counter is returned to the calling
       module, and Transport Security Model processing stops for this
       message.*/
    if (parms->secLevel > tmStateRef->transportSecurityLevel) {
        snmp_increment_statistic(STAT_TSM_SNMPTSMINADEQUATESECURITYLEVELS);
        DEBUGMSGTL(("tsm", "inadequate security level: %d\n", parms->secLevel));
        /* net-snmp returns error codes not OIDs, which are dealt with later */
        return SNMPERR_UNSUPPORTED_SEC_LEVEL;
    }

    /* Section 5.2 Step 5
       The tmStateReference is cached as cachedSecurityData so that a
       possible response to this message will use the same security
       parameters.  Then securityStateReference is set for subsequent
       references to this cached data.
    */
    if (NULL == *parms->secStateRef) {
        tsmSecRef = SNMP_MALLOC_TYPEDEF(netsnmp_tsmSecurityReference);
    } else {
        tsmSecRef = *parms->secStateRef;
    }

    netsnmp_assert_or_return(NULL != tsmSecRef, SNMPERR_GENERR);

    *parms->secStateRef = tsmSecRef;
    tsmSecRef->tmStateRef = tmStateRef;

    /* If this did not come through a tunneled connection, this
       security model is inappropriate (and would be a HUGE security
       hole to assume otherwise).  This is functionally a double check
       since the pdu wouldn't have transport data otherwise.  But this
       is safer though is functionally an extra step beyond the TSM
       RFC. */
    DEBUGMSGTL(("tsm","checking how we got here\n"));
    if (!(parms->pdu->flags & UCD_MSG_FLAG_TUNNELED)) {
        DEBUGMSGTL(("tsm","  pdu not tunneled\n"));
        if (!(parms->sess->flags & NETSNMP_TRANSPORT_FLAG_TUNNELED)) {
            DEBUGMSGTL(("tsm","  session not tunneled\n"));
            return SNMPERR_USM_AUTHENTICATIONFAILURE;
        }
        DEBUGMSGTL(("tsm","  but session is tunneled\n"));
    } else {
        DEBUGMSGTL(("tsm","  tunneled\n"));
    }

    /* Section 5.2, Step 6:
       The scopedPDU component is extracted from the wholeMsg. */
    /*
     * Eat the first octet header.
     */
    remaining = parms->wholeMsgLen - (parms->secParams - parms->wholeMsg);
    if ((data_ptr = asn_parse_sequence(parms->secParams, &remaining,
                                        &type_value,
                                        (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                         ASN_OCTET_STR),
                                        "tsm first octet")) == NULL) {
        /*
         * RETURN parse error 
         */
        return SNMPERR_ASN_PARSE_ERR;
    }
    
    *parms->scopedPdu = data_ptr;
    *parms->scopedPduLen = parms->wholeMsgLen - (data_ptr - parms->wholeMsg);

    /* Section 5.2, Step 7:
       The maxSizeResponseScopedPDU is calculated.  This is the maximum
       size allowed for a scopedPDU for a possible Response message.
     */
    *parms->maxSizeResponse = parms->maxMsgSize; /* XXX */

    /* Section 5.2, Step 8:
       The statusInformation is set to success and a return is made to
       the calling module passing back the OUT parameters as specified
       in the processIncomingMsg ASI.
    */
    return SNMPERR_SUCCESS;
}
