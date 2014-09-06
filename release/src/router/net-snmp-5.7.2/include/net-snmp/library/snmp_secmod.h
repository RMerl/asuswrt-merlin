#ifndef SNMPSECMOD_H
#define SNMPSECMOD_H

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/library/snmp_transport.h>

/* Locally defined security models.
 * (Net-SNMP enterprise number = 8072)*256 + local_num
 */
#define NETSNMP_SEC_MODEL_KSM     2066432
#define NETSNMP_KSM_SECURITY_MODEL     NETSNMP_SEC_MODEL_KSM
#define NETSNMP_TSM_SECURITY_MODEL     SNMP_SEC_MODEL_TSM

struct snmp_secmod_def;

/*
 * parameter information passed to security model routines
 */
struct snmp_secmod_outgoing_params {
    int             msgProcModel;
    u_char         *globalData;
    size_t          globalDataLen;
    int             maxMsgSize;
    int             secModel;
    u_char         *secEngineID;
    size_t          secEngineIDLen;
    char           *secName;
    size_t          secNameLen;
    int             secLevel;
    u_char         *scopedPdu;
    size_t          scopedPduLen;
    void           *secStateRef;
    u_char         *secParams;
    size_t         *secParamsLen;
    u_char        **wholeMsg;
    size_t         *wholeMsgLen;
    size_t         *wholeMsgOffset;
    netsnmp_pdu    *pdu;        /* IN - the pdu getting encoded            */
    netsnmp_session *session;   /* IN - session sending the message        */
};

struct snmp_secmod_incoming_params {
    int             msgProcModel;       /* IN */
    size_t          maxMsgSize; /* IN     - Used to calc maxSizeResponse.  */

    u_char         *secParams;  /* IN     - BER encoded securityParameters. */
    int             secModel;   /* IN */
    int             secLevel;   /* IN     - AuthNoPriv; authPriv etc.      */

    u_char         *wholeMsg;   /* IN     - Original v3 message.           */
    size_t          wholeMsgLen;        /* IN     - Msg length.                    */

    u_char         *secEngineID;        /* OUT    - Pointer snmpEngineID.          */
    size_t         *secEngineIDLen;     /* IN/OUT - Len available; len returned.   */
    /*
     * NOTE: Memory provided by caller.      
     */

    char           *secName;    /* OUT    - Pointer to securityName.       */
    size_t         *secNameLen; /* IN/OUT - Len available; len returned.   */

    u_char        **scopedPdu;  /* OUT    - Pointer to plaintext scopedPdu. */
    size_t         *scopedPduLen;       /* IN/OUT - Len available; len returned.   */

    size_t         *maxSizeResponse;    /* OUT    - Max size of Response PDU.      */
    void          **secStateRef;        /* OUT    - Ref to security state.         */
    netsnmp_session *sess;      /* IN     - session which got the message  */
    netsnmp_pdu    *pdu;        /* IN     - the pdu getting parsed         */
    u_char          msg_flags;  /* IN     - v3 Message flags.              */
};


/*
 * function pointers:
 */

/*
 * free's a given security module's data; called at unregistration time 
 */
typedef int     (SecmodSessionCallback) (netsnmp_session *);
typedef int     (SecmodPduCallback) (netsnmp_pdu *);
typedef int     (Secmod2PduCallback) (netsnmp_pdu *, netsnmp_pdu *);
typedef int     (SecmodOutMsg) (struct snmp_secmod_outgoing_params *);
typedef int     (SecmodInMsg) (struct snmp_secmod_incoming_params *);
typedef void    (SecmodFreeState) (void *);
typedef void    (SecmodHandleReport) (void *sessp,
                                      netsnmp_transport *transport,
                                      netsnmp_session *,
                                      int result,
                                      netsnmp_pdu *origpdu);
typedef int     (SecmodDiscoveryMethod) (void *slp, netsnmp_session *session);
typedef int     (SecmodPostDiscovery) (void *slp, netsnmp_session *session);

typedef int     (SecmodSessionSetup) (netsnmp_session *in_session,
                                      netsnmp_session *out_session);
/*
 * definition of a security module
 */

/*
 * all of these callback functions except the encoding and decoding
 * routines are optional.  The rest of them are available if need.  
 */
struct snmp_secmod_def {
    /*
     * session maniplation functions 
     */
    SecmodSessionCallback *session_open;        /* called in snmp_sess_open()  */
    SecmodSessionCallback *session_close;       /* called in snmp_sess_close() */
    SecmodSessionSetup    *session_setup;

    /*
     * pdu manipulation routines 
     */
    SecmodPduCallback *pdu_free;        /* called in free_pdu() */
    Secmod2PduCallback *pdu_clone;      /* called in snmp_clone_pdu() */
    SecmodPduCallback *pdu_timeout;     /* called when request timesout */
    SecmodFreeState *pdu_free_state_ref;        /* frees pdu->securityStateRef */

    /*
     * de/encoding routines: mandatory 
     */
    SecmodOutMsg   *encode_reverse;     /* encode packet back to front */
    SecmodOutMsg   *encode_forward;     /* encode packet forward */
    SecmodInMsg    *decode;     /* decode & validate incoming */

   /*
    * error and report handling
    */
   SecmodHandleReport *handle_report;

   /*
    * default engineID discovery mechanism
    */
   SecmodDiscoveryMethod *probe_engineid;
   SecmodPostDiscovery   *post_probe_engineid;
};


/*
 * internal list
 */
struct snmp_secmod_list {
    int             securityModel;
    struct snmp_secmod_def *secDef;
    struct snmp_secmod_list *next;
};


/*
 * register a security service 
 */
int             register_sec_mod(int, const char *,
                                 struct snmp_secmod_def *);
/*
 * find a security service definition 
 */
NETSNMP_IMPORT
struct snmp_secmod_def *find_sec_mod(int);
/*
 * register a security service 
 */
int             unregister_sec_mod(int);        /* register a security service */
void            init_secmod(void);
NETSNMP_IMPORT
void            shutdown_secmod(void);

/*
 * clears the sec_mod list
 */
NETSNMP_IMPORT
void            clear_sec_mod(void);

#ifdef __cplusplus
}
#endif
#endif                          /* SNMPSECMOD_H */
