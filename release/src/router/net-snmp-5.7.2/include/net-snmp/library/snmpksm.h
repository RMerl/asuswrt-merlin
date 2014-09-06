/*
 * Header file for Kerberos Security Model support
 */

#ifndef SNMPKSM_H
#define SNMPKSM_H

#ifdef __cplusplus
extern          "C" {
#endif

    int             ksm_rgenerate_out_msg(struct
                                          snmp_secmod_outgoing_params *);
    int             ksm_process_in_msg(struct snmp_secmod_incoming_params
                                       *);
    void            init_ksm(void);

    void            shutdown_ksm(void);

    /*
     * This is the "key usage" that is used by the new crypto API.  It's used
     * generally only if you are using derived keys.  The specifical says that
     * 1024-2047 are to be used by applications, and that even usage numbers are
     * to be used for encryption and odd numbers are to be used for checksums.
     */

#define KSM_KEY_USAGE_ENCRYPTION	1030
#define KSM_KEY_USAGE_CHECKSUM		1031

#define KSM_SEC_MODEL_NUMBER            SNMP_SEC_MODEL_KSM

#ifdef __cplusplus
}
#endif
#endif                          /* SNMPKSM_H */
