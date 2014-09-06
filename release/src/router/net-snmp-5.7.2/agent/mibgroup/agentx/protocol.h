#ifndef AGENTX_PROTOCOL_H
#define AGENTX_PROTOCOL_H

config_belongs_in(agent_module)

#ifdef __cplusplus
extern          "C" {
#endif
    /*
     *  Definitions for Agent Extensibility Protocol (RFC 2257)
     *
     */

#define AGENTX_PORT	705
#ifndef NETSNMP_AGENTX_SOCKET
#define NETSNMP_AGENTX_SOCKET	"/var/agentx/master"
#endif

    /*
     * AgentX versions 
     */
    /*
     * Use values distinct from those used to represent SNMP versions 
     */

#define AGENTX_VERSION_BASE	192     /* Binary: 11xxxxxx */
#define AGENTX_VERSION_1	(AGENTX_VERSION_BASE | 0x1)

#define IS_AGENTX_VERSION(v)	(((v)&AGENTX_VERSION_BASE) == AGENTX_VERSION_BASE)


    /*
     * PDU types in AgentX 
     */
#define AGENTX_MSG_OPEN       ((u_char)1)
#define AGENTX_MSG_CLOSE      ((u_char)2)
#define AGENTX_MSG_REGISTER   ((u_char)3)
#define AGENTX_MSG_UNREGISTER ((u_char)4)
#define AGENTX_MSG_GET        ((u_char)5)
#define AGENTX_MSG_GETNEXT    ((u_char)6)
#define AGENTX_MSG_GETBULK    ((u_char)7)
#define AGENTX_MSG_TESTSET    ((u_char)8)
#define AGENTX_MSG_COMMITSET  ((u_char)9)
#define AGENTX_MSG_UNDOSET    ((u_char)10)
#define AGENTX_MSG_CLEANUPSET ((u_char)11)
#define AGENTX_MSG_NOTIFY     ((u_char)12)
#define AGENTX_MSG_PING       ((u_char)13)
#define AGENTX_MSG_INDEX_ALLOCATE    ((u_char)14)
#define AGENTX_MSG_INDEX_DEALLOCATE  ((u_char)15)
#define AGENTX_MSG_ADD_AGENT_CAPS    ((u_char)16)
#define AGENTX_MSG_REMOVE_AGENT_CAPS ((u_char)17)
#define AGENTX_MSG_RESPONSE    ((u_char)18)


    /*
     * Error codes from RFC 2257 
     */
#define AGENTX_ERR_OPEN_FAILED          (256)
#define AGENTX_ERR_NOT_OPEN             (257)
#define AGENTX_ERR_INDEX_WRONG_TYPE     (258)
#define AGENTX_ERR_INDEX_ALREADY_ALLOCATED (259)
#define AGENTX_ERR_INDEX_NONE_AVAILABLE (260)
#define AGENTX_ERR_INDEX_NOT_ALLOCATED  (261)
#define AGENTX_ERR_UNSUPPORTED_CONTEXT  (262)
#define AGENTX_ERR_DUPLICATE_REGISTRATION (263)
#define AGENTX_ERR_UNKNOWN_REGISTRATION (264)
#define AGENTX_ERR_UNKNOWN_AGENTCAPS    (265)

    /*
     * added in 1999 revision 
     */
#define AGENTX_ERR_NOERROR		SNMP_ERR_NOERROR
#define AGENTX_ERR_PARSE_FAILED         (266)
#define AGENTX_ERR_REQUEST_DENIED       (267)
#define AGENTX_ERR_PROCESSING_ERROR     (268)

    /*
     * Message processing models 
     */
#define AGENTX_MP_MODEL_AGENTXv1        (257)


    /*
     * PDU Flags - see also 'UCD_MSG_FLAG_xxx' in snmp.h 
     */
#define AGENTX_MSG_FLAG_INSTANCE_REGISTER     0x01
#define AGENTX_MSG_FLAG_NEW_INSTANCE          0x02
#define AGENTX_MSG_FLAG_ANY_INSTANCE          0x04
#define AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT   0x08
#define AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER    0x10

#define AGENTX_MSG_FLAGS_MASK                 0xff

    /*
     * Session Flags - see also 'UCD_FLAGS_xxx' in snmp.h 
     */
#define AGENTX_FLAGS_NETWORK_BYTE_ORDER       AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER



    int             agentx_realloc_build(netsnmp_session * session,
                                         netsnmp_pdu *pdu, u_char ** buf,
                                         size_t * buf_len,
                                         size_t * out_len);
    int             agentx_parse(netsnmp_session *, netsnmp_pdu *,
                                 u_char *, size_t);
    int             agentx_check_packet(u_char *, size_t);

#ifdef __cplusplus
}
#endif
#endif                          /* AGENTX_PROTOCOL_H */
