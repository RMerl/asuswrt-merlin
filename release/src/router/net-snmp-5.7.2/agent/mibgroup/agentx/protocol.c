/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#include <errno.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net-snmp/net-snmp-includes.h>

#include "agentx/protocol.h"

const char     *
agentx_cmd(u_char code)
{
    switch (code) {
    case AGENTX_MSG_OPEN:
        return "Open";
    case AGENTX_MSG_CLOSE:
        return "Close";
    case AGENTX_MSG_REGISTER:
        return "Register";
    case AGENTX_MSG_UNREGISTER:
        return "Unregister";
    case AGENTX_MSG_GET:
        return "Get";
    case AGENTX_MSG_GETNEXT:
        return "Get Next";
    case AGENTX_MSG_GETBULK:
        return "Get Bulk";
    case AGENTX_MSG_TESTSET:
        return "Test Set";
    case AGENTX_MSG_COMMITSET:
        return "Commit Set";
    case AGENTX_MSG_UNDOSET:
        return "Undo Set";
    case AGENTX_MSG_CLEANUPSET:
        return "Cleanup Set";
    case AGENTX_MSG_NOTIFY:
        return "Notify";
    case AGENTX_MSG_PING:
        return "Ping";
    case AGENTX_MSG_INDEX_ALLOCATE:
        return "Index Allocate";
    case AGENTX_MSG_INDEX_DEALLOCATE:
        return "Index Deallocate";
    case AGENTX_MSG_ADD_AGENT_CAPS:
        return "Add Agent Caps";
    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        return "Remove Agent Caps";
    case AGENTX_MSG_RESPONSE:
        return "Response";
    default:
        return "Unknown";
    }
}

int
agentx_realloc_build_int(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         unsigned int value, int network_order)
{
    unsigned int    ivalue = value;
    size_t          ilen = *out_len;
#ifdef WORDS_BIGENDIAN
    unsigned int    i = 0;
#endif

    while ((*out_len + 4) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
    }

    if (network_order) {
#ifndef WORDS_BIGENDIAN
        value = ntohl(value);
#endif
        memmove((*buf + *out_len), &value, 4);
        *out_len += 4;
    } else {
#ifndef WORDS_BIGENDIAN
        memmove((*buf + *out_len), &value, 4);
        *out_len += 4;
#else
        for (i = 0; i < 4; i++) {
            *(*buf + *out_len) = (u_char) value & 0xff;
            (*out_len)++;
            value >>= 8;
        }
#endif
    }
    DEBUGDUMPSETUP("send", (*buf + ilen), 4);
    DEBUGMSG(("dumpv_send", "  Integer:\t%u (0x%.2X)\n", ivalue,
              ivalue));
    return 1;
}

void
agentx_build_int(u_char * bufp, u_int value, int network_byte_order)
{
    u_char         *orig_bufp = bufp;
    u_int           orig_val = value;

    if (network_byte_order) {
#ifndef WORDS_BIGENDIAN
        value = ntohl(value);
#endif
        memmove(bufp, &value, 4);
    } else {
#ifndef WORDS_BIGENDIAN
        memmove(bufp, &value, 4);
#else
        *bufp = (u_char) value & 0xff;
        value >>= 8;
        bufp++;
        *bufp = (u_char) value & 0xff;
        value >>= 8;
        bufp++;
        *bufp = (u_char) value & 0xff;
        value >>= 8;
        bufp++;
        *bufp = (u_char) value & 0xff;
#endif
    }
    DEBUGDUMPSETUP("send", orig_bufp, 4);
    DEBUGMSG(("dumpv_send", "  Integer:\t%u (0x%.2X)\n", orig_val,
              orig_val));
}

int
agentx_realloc_build_short(u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           unsigned short value, int network_order)
{
    unsigned short  ivalue = value;
    size_t          ilen = *out_len;
#ifdef WORDS_BIGENDIAN
    unsigned short  i = 0;
#endif

    while ((*out_len + 2) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
    }

    if (network_order) {
#ifndef WORDS_BIGENDIAN
        value = ntohs(value);
#endif
        memmove((*buf + *out_len), &value, 2);
        *out_len += 2;
    } else {
#ifndef WORDS_BIGENDIAN
        memmove((*buf + *out_len), &value, 2);
        *out_len += 2;
#else
        for (i = 0; i < 2; i++) {
            *(*buf + *out_len) = (u_char) value & 0xff;
            (*out_len)++;
            value >>= 8;
        }
#endif
    }
    DEBUGDUMPSETUP("send", (*buf + ilen), 2);
    DEBUGMSG(("dumpv_send", "  Short:\t%hu (0x%.2hX)\n", ivalue, ivalue));
    return 1;
}

int
agentx_realloc_build_oid(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         int inclusive, oid * name, size_t name_len,
                         int network_order)
{
    size_t          ilen = *out_len, i = 0;
    int             prefix = 0;

    DEBUGPRINTINDENT("dumpv_send");
    DEBUGMSG(("dumpv_send", "OID: "));
    DEBUGMSGOID(("dumpv_send", name, name_len));
    DEBUGMSG(("dumpv_send", "\n"));

    /*
     * Updated clarification from the AgentX mailing list.
     * The "null Object Identifier" mentioned in RFC 2471,
     * section 5.1 is a special placeholder value, and
     * should only be used when explicitly mentioned in
     * this RFC.  In particular, it does *not* mean {0, 0}
     */
    if (name_len == 0)
        inclusive = 0;

    /*
     * 'Compact' internet OIDs 
     */
    if (name_len >= 5 && (name[0] == 1 && name[1] == 3 &&
                          name[2] == 6 && name[3] == 1 &&
                          name[4] > 0 && name[4] < 256)) {
        prefix = name[4];
        name += 5;
        name_len -= 5;
    }

    while ((*out_len + 4 + (4 * name_len)) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
    }

    *(*buf + *out_len) = (u_char) name_len;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) prefix;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) inclusive;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) 0x00;
    (*out_len)++;

    DEBUGDUMPHEADER("send", "OID Header");
    DEBUGDUMPSETUP("send", (*buf + ilen), 4);
    DEBUGMSG(("dumpv_send", "  # subids:\t%d (0x%.2X)\n", (int)name_len,
              (unsigned int)name_len));
    DEBUGPRINTINDENT("dumpv_send");
    DEBUGMSG(("dumpv_send", "  prefix:\t%d (0x%.2X)\n", prefix, prefix));
    DEBUGPRINTINDENT("dumpv_send");
    DEBUGMSG(("dumpv_send", "  inclusive:\t%d (0x%.2X)\n", inclusive,
              inclusive));
    DEBUGINDENTLESS();
    DEBUGDUMPHEADER("send", "OID Segments");

    for (i = 0; i < name_len; i++) {
        if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                      name[i], network_order)) {
            DEBUGINDENTLESS();
            return 0;
        }
    }
    DEBUGINDENTLESS();

    return 1;
}

int
agentx_realloc_build_string(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            u_char * string, size_t string_len,
                            int network_order)
{
    size_t          ilen = *out_len, i = 0;

    while ((*out_len + 4 + (4 * ((string_len + 3) / 4))) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
    }

    DEBUGDUMPHEADER("send", "Build String");
    DEBUGDUMPHEADER("send", "length");
    if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                  string_len, network_order)) {
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }

    if (string_len == 0) {
        DEBUGMSG(("dumpv_send", "  String: <empty>\n"));
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 1;
    }

    memmove((*buf + *out_len), string, string_len);
    *out_len += string_len;

    /*
     * Pad to a multiple of 4 bytes if necessary (per RFC 2741).  
     */

    if (string_len % 4 != 0) {
        for (i = 0; i < 4 - (string_len % 4); i++) {
            *(*buf + *out_len) = 0;
            (*out_len)++;
        }
    }

    DEBUGDUMPSETUP("send", (*buf + ilen + 4), ((string_len + 3) / 4) * 4);
    DEBUGMSG(("dumpv_send", "  String:\t%s\n", string));
    DEBUGINDENTLESS();
    DEBUGINDENTLESS();
    return 1;
}

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
int
agentx_realloc_build_double(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            double double_val, int network_order)
{
    union {
        double          doubleVal;
        int             intVal[2];
        char            c[sizeof(double)];
    } du;
    int             tmp;
    u_char          opaque_buffer[3 + sizeof(double)];

    opaque_buffer[0] = ASN_OPAQUE_TAG1;
    opaque_buffer[1] = ASN_OPAQUE_DOUBLE;
    opaque_buffer[2] = sizeof(double);

    du.doubleVal = double_val;
    tmp = htonl(du.intVal[0]);
    du.intVal[0] = htonl(du.intVal[1]);
    du.intVal[1] = tmp;
    memcpy(&opaque_buffer[3], &du.c[0], sizeof(double));

    return agentx_realloc_build_string(buf, buf_len, out_len,
                                       allow_realloc, opaque_buffer,
                                       3 + sizeof(double), network_order);
}

int
agentx_realloc_build_float(u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           float float_val, int network_order)
{
    union {
        float           floatVal;
        int             intVal;
        char            c[sizeof(float)];
    } fu;
    u_char          opaque_buffer[3 + sizeof(float)];

    opaque_buffer[0] = ASN_OPAQUE_TAG1;
    opaque_buffer[1] = ASN_OPAQUE_FLOAT;
    opaque_buffer[2] = sizeof(float);

    fu.floatVal = float_val;
    fu.intVal = htonl(fu.intVal);
    memcpy(&opaque_buffer[3], &fu.c[0], sizeof(float));

    return agentx_realloc_build_string(buf, buf_len, out_len,
                                       allow_realloc, opaque_buffer,
                                       3 + sizeof(float), network_order);
}
#endif

int
agentx_realloc_build_varbind(u_char ** buf, size_t * buf_len,
                             size_t * out_len, int allow_realloc,
                             netsnmp_variable_list * vp, int network_order)
{
    DEBUGDUMPHEADER("send", "VarBind");
    DEBUGDUMPHEADER("send", "type");
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    if ((vp->type == ASN_OPAQUE_FLOAT) || (vp->type == ASN_OPAQUE_DOUBLE)
        || (vp->type == ASN_OPAQUE_I64) || (vp->type == ASN_OPAQUE_U64)
        || (vp->type == ASN_OPAQUE_COUNTER64)) {
        if (!agentx_realloc_build_short
            (buf, buf_len, out_len, allow_realloc,
             (unsigned short) ASN_OPAQUE, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
    } else
#endif
    if (vp->type == ASN_PRIV_INCL_RANGE || vp->type == ASN_PRIV_EXCL_RANGE) {
        if (!agentx_realloc_build_short
            (buf, buf_len, out_len, allow_realloc,
             (unsigned short) ASN_OBJECT_ID, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
    } else {
        if (!agentx_realloc_build_short
            (buf, buf_len, out_len, allow_realloc,
             (unsigned short) vp->type, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
    }

    while ((*out_len + 2) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
    }

    *(*buf + *out_len) = 0;
    (*out_len)++;
    *(*buf + *out_len) = 0;
    (*out_len)++;
    DEBUGINDENTLESS();

    DEBUGDUMPHEADER("send", "name");
    if (!agentx_realloc_build_oid(buf, buf_len, out_len, allow_realloc, 0,
                                  vp->name, vp->name_length,
                                  network_order)) {
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }
    DEBUGINDENTLESS();

    DEBUGDUMPHEADER("send", "value");
    switch (vp->type) {

    case ASN_INTEGER:
    case ASN_COUNTER:
    case ASN_GAUGE:
    case ASN_TIMETICKS:
    case ASN_UINTEGER:
        if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                      *(vp->val.integer), network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        break;

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case ASN_OPAQUE_FLOAT:
        DEBUGDUMPHEADER("send", "Build Opaque Float");
        DEBUGPRINTINDENT("dumpv_send");
        DEBUGMSG(("dumpv_send", "  Float:\t%f\n", *(vp->val.floatVal)));
        if (!agentx_realloc_build_float
            (buf, buf_len, out_len, allow_realloc, *(vp->val.floatVal),
             network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
        break;

    case ASN_OPAQUE_DOUBLE:
        DEBUGDUMPHEADER("send", "Build Opaque Double");
        DEBUGPRINTINDENT("dumpv_send");
        DEBUGMSG(("dumpv_send", "  Double:\t%f\n", *(vp->val.doubleVal)));
        if (!agentx_realloc_build_double
            (buf, buf_len, out_len, allow_realloc, *(vp->val.doubleVal),
             network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
        break;

    case ASN_OPAQUE_I64:
    case ASN_OPAQUE_U64:
    case ASN_OPAQUE_COUNTER64:
        /*
         * XXX - TODO - encode as raw OPAQUE for now (so fall through
         * here).  
         */
#endif

    case ASN_OCTET_STR:
    case ASN_IPADDRESS:
    case ASN_OPAQUE:
        if (!agentx_realloc_build_string
            (buf, buf_len, out_len, allow_realloc, vp->val.string,
             vp->val_len, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        break;

    case ASN_OBJECT_ID:
    case ASN_PRIV_EXCL_RANGE:
    case ASN_PRIV_INCL_RANGE:
        if (!agentx_realloc_build_oid
            (buf, buf_len, out_len, allow_realloc, 1, vp->val.objid,
             vp->val_len / sizeof(oid), network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        break;

    case ASN_COUNTER64:
        if (network_order) {
            DEBUGDUMPHEADER("send", "Build Counter64 (high, low)");
            if (!agentx_realloc_build_int
                (buf, buf_len, out_len, allow_realloc,
                 vp->val.counter64->high, network_order)
                || !agentx_realloc_build_int(buf, buf_len, out_len,
                                             allow_realloc,
                                             vp->val.counter64->low,
                                             network_order)) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return 0;
            }
            DEBUGINDENTLESS();
        } else {
            DEBUGDUMPHEADER("send", "Build Counter64 (low, high)");
            if (!agentx_realloc_build_int
                (buf, buf_len, out_len, allow_realloc,
                 vp->val.counter64->low, network_order)
                || !agentx_realloc_build_int(buf, buf_len, out_len,
                                             allow_realloc,
                                             vp->val.counter64->high,
                                             network_order)) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return 0;
            }
            DEBUGINDENTLESS();
        }
        break;

    case ASN_NULL:
    case SNMP_NOSUCHOBJECT:
    case SNMP_NOSUCHINSTANCE:
    case SNMP_ENDOFMIBVIEW:
        break;

    default:
        DEBUGMSGTL(("agentx_build_varbind", "unknown type %d (0x%02x)\n",
                    vp->type, vp->type));
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }
    DEBUGINDENTLESS();
    DEBUGINDENTLESS();
    return 1;
}

int
agentx_realloc_build_header(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            netsnmp_pdu *pdu)
{
    size_t          ilen = *out_len;
    const int       network_order =
        pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER;

    while ((*out_len + 4) >= *buf_len) {
        if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
            return 0;
        }
    }

    /*
     * First 4 bytes are version, pdu type, flags, and a 0 reserved byte.  
     */

    *(*buf + *out_len) = 1;
    (*out_len)++;
    *(*buf + *out_len) = pdu->command;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) (pdu->flags & AGENTX_MSG_FLAGS_MASK);
    (*out_len)++;
    *(*buf + *out_len) = 0;
    (*out_len)++;

    DEBUGDUMPHEADER("send", "AgentX Header");
    DEBUGDUMPSETUP("send", (*buf + ilen), 4);
    DEBUGMSG(("dumpv_send", "  Version:\t%d\n", (int) *(*buf + ilen)));
    DEBUGPRINTINDENT("dumpv_send");
    DEBUGMSG(("dumpv_send", "  Command:\t%d (%s)\n", pdu->command,
              agentx_cmd((u_char)pdu->command)));
    DEBUGPRINTINDENT("dumpv_send");
    DEBUGMSG(("dumpv_send", "  Flags:\t%02x\n", (int) *(*buf + ilen + 2)));

    DEBUGDUMPHEADER("send", "Session ID");
    if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                  pdu->sessid, network_order)) {
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }
    DEBUGINDENTLESS();

    DEBUGDUMPHEADER("send", "Transaction ID");
    if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                  pdu->transid, network_order)) {
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }
    DEBUGINDENTLESS();

    DEBUGDUMPHEADER("send", "Request ID");
    if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                  pdu->reqid, network_order)) {
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }
    DEBUGINDENTLESS();

    DEBUGDUMPHEADER("send", "Dummy Length :-(");
    if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                  0, network_order)) {
        DEBUGINDENTLESS();
        DEBUGINDENTLESS();
        return 0;
    }
    DEBUGINDENTLESS();

    if (pdu->flags & AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT) {
        DEBUGDUMPHEADER("send", "Community");
        if (!agentx_realloc_build_string
            (buf, buf_len, out_len, allow_realloc, pdu->community,
             pdu->community_len, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
    }

    DEBUGINDENTLESS();
    return 1;
}

static int
_agentx_realloc_build(u_char ** buf, size_t * buf_len, size_t * out_len,
                      int allow_realloc,
                      netsnmp_session * session, netsnmp_pdu *pdu)
{
    size_t          ilen = *out_len;
    netsnmp_variable_list *vp;
    int             inc, i = 0;
    const int       network_order =
        pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * Various PDU types don't include context information (RFC 2741, p. 20). 
     */
    switch (pdu->command) {
    case AGENTX_MSG_OPEN:
    case AGENTX_MSG_CLOSE:
    case AGENTX_MSG_RESPONSE:
    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_CLEANUPSET:
        pdu->flags &= ~(AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT);
    }

	/* We've received a PDU that has specified a context.  NetSNMP however, uses
	 * the pdu->community field to specify context when using the AgentX
	 * protocol.  Therefore we need to copy the context name and length into the
	 * pdu->community and pdu->community_len fields, respectively. */
	if (pdu->contextName != NULL && pdu->community == NULL)
	{	
		pdu->community     = (u_char *) strdup(pdu->contextName);
		pdu->community_len = pdu->contextNameLen;
		pdu->flags |= AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT;
	}

    /*
     * Build the header (and context if appropriate).  
     */
    if (!agentx_realloc_build_header
        (buf, buf_len, out_len, allow_realloc, pdu)) {
        return 0;
    }

    /*
     * Everything causes a response, except for agentx-Response-PDU and
     * agentx-CleanupSet-PDU.  
     */

    pdu->flags |= UCD_MSG_FLAG_EXPECT_RESPONSE;

    DEBUGDUMPHEADER("send", "AgentX Payload");
    switch (pdu->command) {

    case AGENTX_MSG_OPEN:
        /*
         * Timeout  
         */
        while ((*out_len + 4) >= *buf_len) {
            if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                DEBUGINDENTLESS();
                return 0;
            }
        }
        *(*buf + *out_len) = (u_char) pdu->time;
        (*out_len)++;
        for (i = 0; i < 3; i++) {
            *(*buf + *out_len) = 0;
            (*out_len)++;
        }
        DEBUGDUMPHEADER("send", "Open Timeout");
        DEBUGDUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUGMSG(("dumpv_send", "  Timeout:\t%d\n",
                  (int) *(*buf + *out_len - 4)));
        DEBUGINDENTLESS();

        DEBUGDUMPHEADER("send", "Open ID");
        if (!agentx_realloc_build_oid
            (buf, buf_len, out_len, allow_realloc, 0, pdu->variables->name,
             pdu->variables->name_length, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
        DEBUGDUMPHEADER("send", "Open Description");
        if (!agentx_realloc_build_string
            (buf, buf_len, out_len, allow_realloc,
             pdu->variables->val.string, pdu->variables->val_len,
             network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
        break;

    case AGENTX_MSG_CLOSE:
        /*
         * Reason  
         */
        while ((*out_len + 4) >= *buf_len) {
            if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                DEBUGINDENTLESS();
                return 0;
            }
        }
        *(*buf + *out_len) = (u_char) pdu->errstat;
        (*out_len)++;
        for (i = 0; i < 3; i++) {
            *(*buf + *out_len) = 0;
            (*out_len)++;
        }
        DEBUGDUMPHEADER("send", "Close Reason");
        DEBUGDUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUGMSG(("dumpv_send", "  Reason:\t%d\n",
                  (int) *(*buf + *out_len - 4)));
        DEBUGINDENTLESS();
        break;

    case AGENTX_MSG_REGISTER:
    case AGENTX_MSG_UNREGISTER:
        while ((*out_len + 4) >= *buf_len) {
            if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                DEBUGINDENTLESS();
                return 0;
            }
        }
        if (pdu->command == AGENTX_MSG_REGISTER) {
            *(*buf + *out_len) = (u_char) pdu->time;
        } else {
            *(*buf + *out_len) = 0;
        }
        (*out_len)++;
        *(*buf + *out_len) = (u_char) pdu->priority;
        (*out_len)++;
        *(*buf + *out_len) = (u_char) pdu->range_subid;
        (*out_len)++;
        *(*buf + *out_len) = (u_char) 0;
        (*out_len)++;

        DEBUGDUMPHEADER("send", "(Un)Register Header");
        DEBUGDUMPSETUP("send", (*buf + *out_len - 4), 4);
        if (pdu->command == AGENTX_MSG_REGISTER) {
            DEBUGMSG(("dumpv_send", "  Timeout:\t%d\n",
                      (int) *(*buf + *out_len - 4)));
            DEBUGPRINTINDENT("dumpv_send");
        }
        DEBUGMSG(("dumpv_send", "  Priority:\t%d\n",
                  (int) *(*buf + *out_len - 3)));
        DEBUGPRINTINDENT("dumpv_send");
        DEBUGMSG(("dumpv_send", "  Range SubID:\t%d\n",
                  (int) *(*buf + *out_len - 2)));
        DEBUGINDENTLESS();

        vp = pdu->variables;
        DEBUGDUMPHEADER("send", "(Un)Register Prefix");
        if (!agentx_realloc_build_oid
            (buf, buf_len, out_len, allow_realloc, 0, vp->name,
             vp->name_length, network_order)) {

            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();

        if (pdu->range_subid) {
            DEBUGDUMPHEADER("send", "(Un)Register Range");
            if (!agentx_realloc_build_int
                (buf, buf_len, out_len, allow_realloc,
                 vp->val.objid[pdu->range_subid - 1], network_order)) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return 0;
            }
            DEBUGINDENTLESS();
        }
        break;

    case AGENTX_MSG_GETBULK:
        DEBUGDUMPHEADER("send", "GetBulk Non-Repeaters");
        if (!agentx_realloc_build_short
            (buf, buf_len, out_len, allow_realloc, 
            (u_short)pdu->non_repeaters,
             network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();

        DEBUGDUMPHEADER("send", "GetBulk Max-Repetitions");
        if (!agentx_realloc_build_short
            (buf, buf_len, out_len, allow_realloc, 
            (u_short)pdu->max_repetitions,
             network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();

        /*
         * Fallthrough  
         */

    case AGENTX_MSG_GET:
    case AGENTX_MSG_GETNEXT:
        DEBUGDUMPHEADER("send", "Get* Variable List");
        for (vp = pdu->variables; vp != NULL; vp = vp->next_variable) {
            inc = (vp->type == ASN_PRIV_INCL_RANGE);
            if (!agentx_realloc_build_oid
                (buf, buf_len, out_len, allow_realloc, inc, vp->name,
                 vp->name_length, network_order)) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return 0;
            }
            if (!agentx_realloc_build_oid
                (buf, buf_len, out_len, allow_realloc, 0, vp->val.objid,
                 vp->val_len / sizeof(oid), network_order)) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return 0;
            }
        }
        DEBUGINDENTLESS();
        break;

    case AGENTX_MSG_RESPONSE:
        pdu->flags &= ~(UCD_MSG_FLAG_EXPECT_RESPONSE);
        if (!agentx_realloc_build_int(buf, buf_len, out_len, allow_realloc,
                                      pdu->time, network_order)) {
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGDUMPHEADER("send", "Response");
        DEBUGDUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUGMSG(("dumpv_send", "  sysUpTime:\t%lu\n", pdu->time));
        DEBUGINDENTLESS();

        if (!agentx_realloc_build_short
            (buf, buf_len, out_len, allow_realloc, 
            (u_short)pdu->errstat,
             network_order)
            || !agentx_realloc_build_short(buf, buf_len, out_len,
                                           allow_realloc, 
                                           (u_short)pdu->errindex,
                                           network_order)) {
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGDUMPHEADER("send", "Response errors");
        DEBUGDUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUGMSG(("dumpv_send", "  errstat:\t%ld\n", pdu->errstat));
        DEBUGPRINTINDENT("dumpv_send");
        DEBUGMSG(("dumpv_send", "  errindex:\t%ld\n", pdu->errindex));
        DEBUGINDENTLESS();

        /*
         * Fallthrough  
         */

    case AGENTX_MSG_INDEX_ALLOCATE:
    case AGENTX_MSG_INDEX_DEALLOCATE:
    case AGENTX_MSG_NOTIFY:
    case AGENTX_MSG_TESTSET:
        DEBUGDUMPHEADER("send", "Get* Variable List");
        for (vp = pdu->variables; vp != NULL; vp = vp->next_variable) {
            if (!agentx_realloc_build_varbind
                (buf, buf_len, out_len, allow_realloc, vp,
                 network_order)) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return 0;
            }
        }
        DEBUGINDENTLESS();
        break;

    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_PING:
        /*
         * "Empty" packet.  
         */
        break;

    case AGENTX_MSG_CLEANUPSET:
        pdu->flags &= ~(UCD_MSG_FLAG_EXPECT_RESPONSE);
        break;

    case AGENTX_MSG_ADD_AGENT_CAPS:
        DEBUGDUMPHEADER("send", "AgentCaps OID");
        if (!agentx_realloc_build_oid
            (buf, buf_len, out_len, allow_realloc, 0, pdu->variables->name,
             pdu->variables->name_length, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();

        DEBUGDUMPHEADER("send", "AgentCaps Description");
        if (!agentx_realloc_build_string
            (buf, buf_len, out_len, allow_realloc,
             pdu->variables->val.string, pdu->variables->val_len,
             network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
        break;

    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        DEBUGDUMPHEADER("send", "AgentCaps OID");
        if (!agentx_realloc_build_oid
            (buf, buf_len, out_len, allow_realloc, 0, pdu->variables->name,
             pdu->variables->name_length, network_order)) {
            DEBUGINDENTLESS();
            DEBUGINDENTLESS();
            return 0;
        }
        DEBUGINDENTLESS();
        break;

    default:
        session->s_snmp_errno = SNMPERR_UNKNOWN_PDU;
        return 0;
    }
    DEBUGINDENTLESS();

    /*
     * Fix the payload length (ignoring the 20-byte header).  
     */

    agentx_build_int((*buf + 16), (*out_len - ilen) - 20, network_order);

    DEBUGMSGTL(("agentx_build", "packet built okay\n"));
    return 1;
}

int
agentx_realloc_build(netsnmp_session * session, netsnmp_pdu *pdu,
                     u_char ** buf, size_t * buf_len, size_t * out_len)
{
    if (session == NULL || buf_len == NULL ||
        out_len == NULL || pdu == NULL || buf == NULL) {
        return -1;
    }
    if (!_agentx_realloc_build(buf, buf_len, out_len, 1, session, pdu)) {
        if (session->s_snmp_errno == 0) {
            session->s_snmp_errno = SNMPERR_BAD_ASN1_BUILD;
        }
        return -1;
    }

    return 0;
}

        /***********************
	*
	*  Utility functions for parsing an AgentX packet
	*
	***********************/

int
agentx_parse_int(u_char * data, u_int network_byte_order)
{
    u_int           value = 0;

    /*
     *  Note - this doesn't handle 'PDP_ENDIAN' systems
     *      If anyone needs this added, contact the coders list
     */
    DEBUGDUMPSETUP("recv", data, 4);
    if (network_byte_order) {
        memmove(&value, data, 4);
#ifndef WORDS_BIGENDIAN
        value = ntohl(value);
#endif
    } else {
#ifndef WORDS_BIGENDIAN
        memmove(&value, data, 4);
#else
        /*
         * The equivalent of the 'ntohl()' macro,
         * except this macro is null on big-endian systems 
         */
        value += data[3];
        value <<= 8;
        value += data[2];
        value <<= 8;
        value += data[1];
        value <<= 8;
        value += data[0];
#endif
    }
    DEBUGMSG(("dumpv_recv", "  Integer:\t%u (0x%.2X)\n", value, value));

    return value;
}


int
agentx_parse_short(u_char * data, u_int network_byte_order)
{
    u_short         value = 0;

    if (network_byte_order) {
        memmove(&value, data, 2);
#ifndef WORDS_BIGENDIAN
        value = ntohs(value);
#endif
    } else {
#ifndef WORDS_BIGENDIAN
        memmove(&value, data, 2);
#else
        /*
         * The equivalent of the 'ntohs()' macro,
         * except this macro is null on big-endian systems 
         */
        value += data[1];
        value <<= 8;
        value += data[0];
#endif
    }

    DEBUGDUMPSETUP("recv", data, 2);
    DEBUGMSG(("dumpv_recv", "  Short:\t%hu (0x%.2X)\n", value, value));
    return value;
}


u_char         *
agentx_parse_oid(u_char * data, size_t * length, int *inc,
                 oid * oid_buf, size_t * oid_len, u_int network_byte_order)
{
    u_int           n_subid;
    u_int           prefix;
    u_int           tmp_oid_len;
    int             i;
    int             int_offset;
    u_int          *int_ptr = (u_int *)oid_buf;
    u_char         *buf_ptr = data;

    if (*length < 4) {
        DEBUGMSGTL(("agentx", "Incomplete Object ID\n"));
        return NULL;
    }

    DEBUGDUMPHEADER("recv", "OID Header");
    DEBUGDUMPSETUP("recv", data, 4);
    DEBUGMSG(("dumpv_recv", "  # subids:\t%d (0x%.2X)\n", data[0],
              data[0]));
    DEBUGPRINTINDENT("dumpv_recv");
    DEBUGMSG(("dumpv_recv", "  prefix: \t%d (0x%.2X)\n", data[1],
              data[1]));
    DEBUGPRINTINDENT("dumpv_recv");
    DEBUGMSG(("dumpv_recv", "  inclusive:\t%d (0x%.2X)\n", data[2],
              data[2]));

    DEBUGINDENTLESS();
    DEBUGDUMPHEADER("recv", "OID Segments");

    n_subid = data[0];
    prefix = data[1];
    if (inc)
        *inc = data[2];
    int_offset = sizeof(oid)/4;

    buf_ptr += 4;
    *length -= 4;

    DEBUGMSG(("djp", "  parse_oid\n"));
    DEBUGMSG(("djp", "  sizeof(oid) = %d\n", (int)sizeof(oid)));
    if (n_subid == 0 && prefix == 0) {
        /*
         * Null OID 
         */
        memset(int_ptr, 0, 2 * sizeof(oid));
        *oid_len = 2;
        DEBUGPRINTINDENT("dumpv_recv");
        DEBUGMSG(("dumpv_recv", "OID: NULL (0.0)\n"));
        DEBUGINDENTLESS();
        return buf_ptr;
    }

    /*
     * Check that the expanded OID will fit in the buffer provided
     */
    tmp_oid_len = (prefix ? n_subid + 5 : n_subid);
    if (*oid_len < tmp_oid_len) {
        DEBUGMSGTL(("agentx", "Oversized Object ID (buf=%" NETSNMP_PRIz "d"
		    " pdu=%d)\n", *oid_len, tmp_oid_len));
        DEBUGINDENTLESS();
        return NULL;
    }

#ifdef WORDS_BIGENDIAN
# define endianoff 1
#else
# define endianoff 0
#endif
    if (*length < 4 * n_subid) {
        DEBUGMSGTL(("agentx", "Incomplete Object ID\n"));
        DEBUGINDENTLESS();
        return NULL;
    }

    if (prefix) {	 
        if (int_offset == 2) {  	/* align OID values in 64 bit agent */  
	    memset(int_ptr, 0, 10*sizeof(int_ptr[0])); 
	    int_ptr[0+endianoff] = 1;
	    int_ptr[2+endianoff] = 3;
	    int_ptr[4+endianoff] = 6;
	    int_ptr[6+endianoff] = 1;
	    int_ptr[8+endianoff] = prefix;
        } else { /* assume int_offset == 1 */
	    int_ptr[0] = 1;
	    int_ptr[1] = 3;
	    int_ptr[2] = 6;
	    int_ptr[3] = 1;
	    int_ptr[4] = prefix;
        }
        int_ptr = int_ptr + (int_offset * 5);
    }

    for (i = 0; i < (int) (int_offset * n_subid); i = i + int_offset) {
	int x;

	x = agentx_parse_int(buf_ptr, network_byte_order);
	if (int_offset == 2) {
            int_ptr[i+0] = 0;
	    int_ptr[i+1] = 0;
	    int_ptr[i+endianoff]=x;
        } else {
	    int_ptr[i] = x;
        }
        buf_ptr += 4;
        *length -= 4;
    }

    *oid_len = tmp_oid_len;

    DEBUGINDENTLESS();
    DEBUGPRINTINDENT("dumpv_recv");
    DEBUGMSG(("dumpv_recv", "OID: "));
    DEBUGMSGOID(("dumpv_recv", oid_buf, *oid_len));
    DEBUGMSG(("dumpv_recv", "\n"));

    return buf_ptr;
}



u_char         *
agentx_parse_string(u_char * data, size_t * length,
                    u_char * string, size_t * str_len,
                    u_int network_byte_order)
{
    u_int           len;

    if (*length < 4) {
        DEBUGMSGTL(("agentx", "Incomplete string (too short: %d)\n",
                    (int)*length));
        return NULL;
    }

    len = agentx_parse_int(data, network_byte_order);
    if (*length < len + 4) {
        DEBUGMSGTL(("agentx", "Incomplete string (still too short: %d)\n",
                    (int)*length));
        return NULL;
    }
    if (len > *str_len) {
        DEBUGMSGTL(("agentx", "String too long (too long)\n"));
        return NULL;
    }
    memmove(string, data + 4, len);
    string[len] = '\0';
    *str_len = len;

    len += 3;                   /* Extend the string length to include the padding */
    len >>= 2;
    len <<= 2;

    *length -= (len + 4);
    DEBUGDUMPSETUP("recv", data, (len + 4));
    DEBUGIF("dumpv_recv") {
        u_char         *buf = NULL;
        size_t          buf_len = 0, out_len = 0;

        if (sprint_realloc_asciistring(&buf, &buf_len, &out_len, 1,
                                       string, len)) {
            DEBUGMSG(("dumpv_recv", "String: %s\n", buf));
        } else {
            DEBUGMSG(("dumpv_recv", "String: %s [TRUNCATED]\n", buf));
        }
        if (buf != NULL) {
            free(buf);
        }
    }
    return data + (len + 4);
}

u_char         *
agentx_parse_opaque(u_char * data, size_t * length, int *type,
                    u_char * opaque_buf, size_t * opaque_len,
                    u_int network_byte_order)
{
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    union {
        float           floatVal;
        double          doubleVal;
        int             intVal[2];
        char            c[sizeof(double)];
    } fu;
    int             tmp;
    u_char         *buf;
#endif
    u_char         *const cp =
        agentx_parse_string(data, length,
                            opaque_buf, opaque_len, network_byte_order);

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    if (cp == NULL)
        return NULL;

    buf = opaque_buf;

    if ((*opaque_len <= 3) || (buf[0] != ASN_OPAQUE_TAG1))
        return cp;              /* Unrecognised opaque type */

    switch (buf[1]) {
    case ASN_OPAQUE_FLOAT:
        if ((*opaque_len != (3 + sizeof(float))) ||
            (buf[2] != sizeof(float)))
            return cp;          /* Encoding isn't right for FLOAT */

        memcpy(&fu.c[0], &buf[3], sizeof(float));
        fu.intVal[0] = ntohl(fu.intVal[0]);
        *opaque_len = sizeof(float);
        memcpy(opaque_buf, &fu.c[0], sizeof(float));
        *type = ASN_OPAQUE_FLOAT;
        DEBUGMSG(("dumpv_recv", "Float: %f\n", fu.floatVal));
        return cp;

    case ASN_OPAQUE_DOUBLE:
        if ((*opaque_len != (3 + sizeof(double))) ||
            (buf[2] != sizeof(double)))
            return cp;          /* Encoding isn't right for DOUBLE */

        memcpy(&fu.c[0], &buf[3], sizeof(double));
        tmp = ntohl(fu.intVal[1]);
        fu.intVal[1] = ntohl(fu.intVal[0]);
        fu.intVal[0] = tmp;
        *opaque_len = sizeof(double);
        memcpy(opaque_buf, &fu.c[0], sizeof(double));
        *type = ASN_OPAQUE_DOUBLE;
        DEBUGMSG(("dumpv_recv", "Double: %f\n", fu.doubleVal));
        return cp;

    case ASN_OPAQUE_I64:
    case ASN_OPAQUE_U64:
    case ASN_OPAQUE_COUNTER64:
    default:
        return cp;              /* Unrecognised opaque sub-type */
    }
#else
    return cp;
#endif
}


u_char         *
agentx_parse_varbind(u_char * data, size_t * length, int *type,
                     oid * oid_buf, size_t * oid_len,
                     u_char * data_buf, size_t * data_len,
                     u_int network_byte_order)
{
    u_char         *bufp = data;
    u_int           int_val;
    struct counter64 tmp64;

    DEBUGDUMPHEADER("recv", "VarBind:");
    DEBUGDUMPHEADER("recv", "Type");
    *type = agentx_parse_short(bufp, network_byte_order);
    DEBUGINDENTLESS();
    bufp += 4;
    *length -= 4;

    bufp = agentx_parse_oid(bufp, length, NULL, oid_buf, oid_len,
                            network_byte_order);
    if (bufp == NULL) {
        DEBUGINDENTLESS();
        return NULL;
    }

    switch (*type) {
    case ASN_INTEGER:
    case ASN_COUNTER:
    case ASN_GAUGE:
    case ASN_TIMETICKS:
    case ASN_UINTEGER:
        int_val = agentx_parse_int(bufp, network_byte_order);
        memmove(data_buf, &int_val, 4);
        *data_len = 4;
        bufp += 4;
        *length -= 4;
        break;

    case ASN_OCTET_STR:
    case ASN_IPADDRESS:
        bufp = agentx_parse_string(bufp, length, data_buf, data_len,
                                   network_byte_order);
        break;

    case ASN_OPAQUE:
        bufp = agentx_parse_opaque(bufp, length, type, data_buf, data_len,
                                   network_byte_order);
        break;

    case ASN_PRIV_INCL_RANGE:
    case ASN_PRIV_EXCL_RANGE:
    case ASN_OBJECT_ID:
        bufp =
            agentx_parse_oid(bufp, length, NULL, (oid *) data_buf,
                             data_len, network_byte_order);
        *data_len *= sizeof(oid);
        /*
         * 'agentx_parse_oid()' returns the number of sub_ids 
         */
        break;

    case ASN_COUNTER64:
        memset(&tmp64, 0, sizeof(tmp64));
	if (network_byte_order) {
	    tmp64.high = agentx_parse_int(bufp,   network_byte_order);
	    tmp64.low  = agentx_parse_int(bufp+4, network_byte_order);
	} else {
	    tmp64.high = agentx_parse_int(bufp+4, network_byte_order);
	    tmp64.low  = agentx_parse_int(bufp,   network_byte_order);
	}

        memcpy(data_buf, &tmp64, sizeof(tmp64));
	*data_len = sizeof(tmp64);
	bufp    += 8;
	*length -= 8;
        break;

    case ASN_NULL:
    case SNMP_NOSUCHOBJECT:
    case SNMP_NOSUCHINSTANCE:
    case SNMP_ENDOFMIBVIEW:
        /*
         * No data associated with these types. 
         */
        *data_len = 0;
        break;

    default:
        DEBUGMSG(("recv", "Can not parse type %x", *type));
        DEBUGINDENTLESS();
        return NULL;
    }
    DEBUGINDENTLESS();
    return bufp;
}

/*
 *  AgentX header:
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |    h.version  |   h.type      |   h.flags     |  <reserved>   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       h.sessionID                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     h.transactionID                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       h.packetID                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     h.payload_length                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    Total length = 20 bytes
 *
 *  If we don't seem to have the full packet, return NULL
 *    and let the driving code go back for the rest.
 *  Don't report this as an error, as it's quite "normal"
 *    with a connection-oriented service.
 *
 *  Note that once the header has been successfully processed
 *    (and hence we should have the full packet), any subsequent
 *    "running out of room" is indeed an error.
 */
u_char         *
agentx_parse_header(netsnmp_pdu *pdu, u_char * data, size_t * length)
{
    register u_char *bufp = data;
    size_t          payload;

    if (*length < 20) {         /* Incomplete header */
        return NULL;
    }

    DEBUGDUMPHEADER("recv", "AgentX Header");
    DEBUGDUMPHEADER("recv", "Version");
    DEBUGDUMPSETUP("recv", bufp, 1);
    pdu->version = AGENTX_VERSION_BASE | *bufp;
    DEBUGMSG(("dumpv_recv", "  Version:\t%d\n", *bufp));
    DEBUGINDENTLESS();
    bufp++;

    DEBUGDUMPHEADER("recv", "Command");
    DEBUGDUMPSETUP("recv", bufp, 1);
    pdu->command = *bufp;
    DEBUGMSG(("dumpv_recv", "  Command:\t%d (%s)\n", *bufp,
              agentx_cmd(*bufp)));
    DEBUGINDENTLESS();
    bufp++;

    DEBUGDUMPHEADER("recv", "Flags");
    DEBUGDUMPSETUP("recv", bufp, 1);
    pdu->flags |= *bufp;
    DEBUGMSG(("dumpv_recv", "  Flags:\t0x%x\n", *bufp));
    DEBUGINDENTLESS();
    bufp++;

    DEBUGDUMPHEADER("recv", "Reserved Byte");
    DEBUGDUMPSETUP("recv", bufp, 1);
    DEBUGMSG(("dumpv_recv", "  Reserved:\t0x%x\n", *bufp));
    DEBUGINDENTLESS();
    bufp++;

    DEBUGDUMPHEADER("recv", "Session ID");
    pdu->sessid = agentx_parse_int(bufp,
                                   pdu->
                                   flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUGINDENTLESS();
    bufp += 4;

    DEBUGDUMPHEADER("recv", "Transaction ID");
    pdu->transid = agentx_parse_int(bufp,
                                    pdu->
                                    flags &
                                    AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUGINDENTLESS();
    bufp += 4;

    DEBUGDUMPHEADER("recv", "Packet ID");
    pdu->reqid = agentx_parse_int(bufp,
                                  pdu->
                                  flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUGINDENTLESS();
    bufp += 4;

    DEBUGDUMPHEADER("recv", "Payload Length");
    payload = agentx_parse_int(bufp,
                               pdu->
                               flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUGINDENTLESS();
    bufp += 4;

    DEBUGINDENTLESS();
    *length -= 20;
    if (*length != payload) {   /* Short payload */
        return NULL;
    }
    return bufp;
}


int
agentx_parse(netsnmp_session * session, netsnmp_pdu *pdu, u_char * data,
             size_t len)
{
    register u_char *bufp = data;
    u_char          buffer[SNMP_MAX_MSG_SIZE];
    oid             oid_buffer[MAX_OID_LEN], end_oid_buf[MAX_OID_LEN];
    size_t          buf_len = sizeof(buffer);
    size_t          oid_buf_len = MAX_OID_LEN;
    size_t          end_oid_buf_len = MAX_OID_LEN;

    int             range_bound;        /* OID-range upper bound */
    int             inc;        /* Inclusive SearchRange flag */
    int             type;       /* VarBind data type */
    size_t         *length = &len;

    if (pdu == NULL)
        return (0);
 
    if (!IS_AGENTX_VERSION(session->version))
        return SNMPERR_BAD_VERSION;

#ifndef SNMPERR_INCOMPLETE_PACKET
    /*
     *  Ideally, "short" packets on stream connections should
     *    be handled specially, and the driving code set up to
     *    keep reading until the full packet is received.
     *
     *  For now, lets assume that all packets are read in one go.
     *    I've probably inflicted enough damage on the UCD library
     *    for one week!
     *
     *  I'll come back to this once Wes is speaking to me again.
     */
#define SNMPERR_INCOMPLETE_PACKET SNMPERR_ASN_PARSE_ERR
#endif


    /*
     *  Handle (common) header ....
     */
    bufp = agentx_parse_header(pdu, bufp, length);
    if (bufp == NULL)
        return SNMPERR_INCOMPLETE_PACKET;       /* i.e. wait for the rest */

    /*
     * Control PDU handling 
     */
    pdu->flags |= UCD_MSG_FLAG_ALWAYS_IN_VIEW;
    pdu->flags |= UCD_MSG_FLAG_FORCE_PDU_COPY;
    pdu->flags &= (~UCD_MSG_FLAG_RESPONSE_PDU);

    /*
     *  ... and (not-un-common) context
     */
    if (pdu->flags & AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT) {
        DEBUGDUMPHEADER("recv", "Context");
        bufp = agentx_parse_string(bufp, length, buffer, &buf_len,
                                   pdu->flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUGINDENTLESS();
        if (bufp == NULL)
            return SNMPERR_ASN_PARSE_ERR;

        pdu->community_len = buf_len;
        snmp_clone_mem((void **) &pdu->community,
                       (void *) buffer, (unsigned) buf_len);
		
		/* The NetSNMP API stuffs the context into the PDU's community string
		 * field, when using the AgentX Protocol.  The rest of the code however,
		 * expects to find the context in the PDU's context field.  Therefore we
		 * need to copy the context into the PDU's context fields.  */
		if (pdu->community_len > 0 && pdu->contextName == NULL)
		{
			pdu->contextName    = strdup((char *) pdu->community);
			pdu->contextNameLen = pdu->community_len;
		}

        buf_len = sizeof(buffer);
    }

    DEBUGDUMPHEADER("recv", "PDU");
    switch (pdu->command) {
    case AGENTX_MSG_OPEN:
        pdu->time = *bufp;      /* Timeout */
        bufp += 4;
        *length -= 4;

        /*
         * Store subagent OID & description in a VarBind 
         */
        DEBUGDUMPHEADER("recv", "Subagent OID");
        bufp = agentx_parse_oid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->
                                flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUGINDENTLESS();
        if (bufp == NULL) {
            DEBUGINDENTLESS();
            return SNMPERR_ASN_PARSE_ERR;
        }
        DEBUGDUMPHEADER("recv", "Subagent Description");
        bufp = agentx_parse_string(bufp, length, buffer, &buf_len,
                                   pdu->
                                   flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUGINDENTLESS();
        if (bufp == NULL) {
            DEBUGINDENTLESS();
            return SNMPERR_ASN_PARSE_ERR;
        }
        snmp_pdu_add_variable(pdu, oid_buffer, oid_buf_len,
                              ASN_OCTET_STR, buffer, buf_len);

        oid_buf_len = MAX_OID_LEN;
        buf_len = sizeof(buffer);
        break;

    case AGENTX_MSG_CLOSE:
        pdu->errstat = *bufp;   /* Reason */
        bufp += 4;
        *length -= 4;

        break;

    case AGENTX_MSG_UNREGISTER:
    case AGENTX_MSG_REGISTER:
        DEBUGDUMPHEADER("recv", "Registration Header");
        if (pdu->command == AGENTX_MSG_REGISTER) {
            pdu->time = *bufp;  /* Timeout (Register only) */
            DEBUGDUMPSETUP("recv", bufp, 1);
            DEBUGMSG(("dumpv_recv", "  Timeout:     \t%d\n", *bufp));
        }
        bufp++;
        pdu->priority = *bufp;
        DEBUGDUMPSETUP("recv", bufp, 1);
        DEBUGMSG(("dumpv_recv", "  Priority:    \t%d\n", *bufp));
        bufp++;
        pdu->range_subid = *bufp;
        DEBUGDUMPSETUP("recv", bufp, 1);
        DEBUGMSG(("dumpv_recv", "  Range Sub-Id:\t%d\n", *bufp));
        bufp++;
        bufp++;
        *length -= 4;
        DEBUGINDENTLESS();

        DEBUGDUMPHEADER("recv", "Registration OID");
        bufp = agentx_parse_oid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUGINDENTLESS();
        if (bufp == NULL) {
            DEBUGINDENTLESS();
            return SNMPERR_ASN_PARSE_ERR;
        }

        if (pdu->range_subid) {
            range_bound = agentx_parse_int(bufp, pdu->flags &
                                           AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            bufp += 4;
            *length -= 4;

            /*
             * Construct the end-OID.  
             */
            end_oid_buf_len = oid_buf_len * sizeof(oid);
            memcpy(end_oid_buf, oid_buffer, end_oid_buf_len);
            end_oid_buf[pdu->range_subid - 1] = range_bound;

            snmp_pdu_add_variable(pdu, oid_buffer, oid_buf_len,
                                  ASN_PRIV_INCL_RANGE,
                                  (u_char *) end_oid_buf, end_oid_buf_len);
        } else {
            snmp_add_null_var(pdu, oid_buffer, oid_buf_len);
        }

        oid_buf_len = MAX_OID_LEN;
        break;

    case AGENTX_MSG_GETBULK:
        DEBUGDUMPHEADER("recv", "Non-repeaters");
        pdu->non_repeaters = agentx_parse_short(bufp, pdu->flags &
                                                AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUGINDENTLESS();
        DEBUGDUMPHEADER("recv", "Max-repeaters");
        pdu->max_repetitions = agentx_parse_short(bufp + 2, pdu->flags &
                                                  AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUGINDENTLESS();
        bufp += 4;
        *length -= 4;
        /*
         * Fallthrough - SearchRange handling is the same 
         */

    case AGENTX_MSG_GETNEXT:
    case AGENTX_MSG_GET:

        /*
         * *  SearchRange List
         * *  Keep going while we have data left
         */
        DEBUGDUMPHEADER("recv", "Search Range");
        while (*length > 0) {
            bufp = agentx_parse_oid(bufp, length, &inc,
                                    oid_buffer, &oid_buf_len,
                                    pdu->flags &
                                    AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            if (bufp == NULL) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return SNMPERR_ASN_PARSE_ERR;
            }
            bufp = agentx_parse_oid(bufp, length, NULL,
                                    end_oid_buf, &end_oid_buf_len,
                                    pdu->flags &
                                    AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            if (bufp == NULL) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return SNMPERR_ASN_PARSE_ERR;
            }
            end_oid_buf_len *= sizeof(oid);
            /*
             * 'agentx_parse_oid()' returns the number of sub_ids 
             */

            if (inc) {
                snmp_pdu_add_variable(pdu, oid_buffer, oid_buf_len,
                                      ASN_PRIV_INCL_RANGE,
                                      (u_char *) end_oid_buf,
                                      end_oid_buf_len);
            } else {
                snmp_pdu_add_variable(pdu, oid_buffer, oid_buf_len,
                                      ASN_PRIV_EXCL_RANGE,
                                      (u_char *) end_oid_buf,
                                      end_oid_buf_len);
            }
            oid_buf_len = MAX_OID_LEN;
            end_oid_buf_len = MAX_OID_LEN;
        }

        DEBUGINDENTLESS();
        break;


    case AGENTX_MSG_RESPONSE:

        pdu->flags |= UCD_MSG_FLAG_RESPONSE_PDU;

        /*
         * sysUpTime 
         */
        pdu->time = agentx_parse_int(bufp, pdu->flags &
                                     AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        bufp += 4;
        *length -= 4;

        pdu->errstat = agentx_parse_short(bufp, pdu->flags &
                                          AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        pdu->errindex =
            agentx_parse_short(bufp + 2,
                               pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        bufp += 4;
        *length -= 4;
        /*
         * Fallthrough - VarBind handling is the same 
         */

    case AGENTX_MSG_INDEX_ALLOCATE:
    case AGENTX_MSG_INDEX_DEALLOCATE:
    case AGENTX_MSG_NOTIFY:
    case AGENTX_MSG_TESTSET:

        /*
         * *  VarBind List
         * *  Keep going while we have data left
         */

        DEBUGDUMPHEADER("recv", "VarBindList");
        while (*length > 0) {
            bufp = agentx_parse_varbind(bufp, length, &type,
                                        oid_buffer, &oid_buf_len,
                                        buffer, &buf_len,
                                        pdu->flags &
                                        AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            if (bufp == NULL) {
                DEBUGINDENTLESS();
                DEBUGINDENTLESS();
                return SNMPERR_ASN_PARSE_ERR;
            }
            snmp_pdu_add_variable(pdu, oid_buffer, oid_buf_len,
                                  (u_char) type, buffer, buf_len);

            oid_buf_len = MAX_OID_LEN;
            buf_len = sizeof(buffer);
        }
        DEBUGINDENTLESS();
        break;

    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_CLEANUPSET:
    case AGENTX_MSG_PING:

        /*
         * "Empty" packet 
         */
        break;


    case AGENTX_MSG_ADD_AGENT_CAPS:
        /*
         * Store AgentCap OID & description in a VarBind 
         */
        bufp = agentx_parse_oid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        if (bufp == NULL)
            return SNMPERR_ASN_PARSE_ERR;
        bufp = agentx_parse_string(bufp, length, buffer, &buf_len,
                                   pdu->flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        if (bufp == NULL)
            return SNMPERR_ASN_PARSE_ERR;
        snmp_pdu_add_variable(pdu, oid_buffer, oid_buf_len,
                              ASN_OCTET_STR, buffer, buf_len);

        oid_buf_len = MAX_OID_LEN;
        buf_len = sizeof(buffer);
        break;

    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        /*
         * Store AgentCap OID & description in a VarBind 
         */
        bufp = agentx_parse_oid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        if (bufp == NULL)
            return SNMPERR_ASN_PARSE_ERR;
        snmp_add_null_var(pdu, oid_buffer, oid_buf_len);

        oid_buf_len = MAX_OID_LEN;
        break;

    default:
        DEBUGINDENTLESS();
        DEBUGMSGTL(("agentx", "Unrecognised PDU type: %d\n",
                    pdu->command));
        return SNMPERR_UNKNOWN_PDU;
    }
    DEBUGINDENTLESS();
    return SNMP_ERR_NOERROR;
}




#ifdef TESTING

testit(netsnmp_pdu *pdu1)
{
    char            packet1[BUFSIZ];
    char            packet2[BUFSIZ];
    int             len1, len2;
    netsnmp_pdu     pdu2;
    netsnmp_session sess;

    memset(&pdu2, 0, sizeof(netsnmp_pdu));
    memset(packet1, 0, BUFSIZ);
    memset(packet2, 0, BUFSIZ);

    /*
     * Encode this into a "packet" 
     */
    len1 = BUFSIZ;
    if (agentx_build(&sess, pdu1, packet1, &len1) < 0) {
        DEBUGMSGTL(("agentx", "First build failed\n"));
        exit(1);
    }

    DEBUGMSGTL(("agentx", "First build succeeded:\n"));
    xdump(packet1, len1, "Ax1> ");

    /*
     * Unpack this into a PDU 
     */
    len2 = len1;
    if (agentx_parse(&pdu2, packet1, &len2, (u_char **) NULL) < 0) {
        DEBUGMSGTL(("agentx", "First parse failed\n"));
        exit(1);
    }
    DEBUGMSGTL(("agentx", "First parse succeeded:\n"));
    if (len2 != 0)
        DEBUGMSGTL(("agentx",
                    "Warning - parsed packet has %d bytes left\n", len2));

    /*
     * Encode this into another "packet" 
     */
    len2 = BUFSIZ;
    if (agentx_build(&sess, &pdu2, packet2, &len2) < 0) {
        DEBUGMSGTL(("agentx", "Second build failed\n"));
        exit(1);
    }

    DEBUGMSGTL(("agentx", "Second build succeeded:\n"));
    xdump(packet2, len2, "Ax2> ");

    /*
     * Compare the results 
     */
    if (len1 != len2) {
        DEBUGMSGTL(("agentx",
                    "Error: first build (%d) is different to second (%d)\n",
                    len1, len2));
        exit(1);
    }
    if (memcmp(packet1, packet2, len1) != 0) {
        DEBUGMSGTL(("agentx",
                    "Error: first build data is different to second\n"));
        exit(1);
    }

    DEBUGMSGTL(("agentx", "OK\n"));
}



main()
{
    netsnmp_pdu     pdu1;
    oid             oid_buf[] = { 1, 3, 6, 1, 2, 1, 10 };
    oid             oid_buf2[] = { 1, 3, 6, 1, 2, 1, 20 };
    oid             null_oid[] = { 0, 0 };
    char           *string = "Example string";
    char           *context = "LUCS";


    /*
     * Create an example AgentX pdu structure 
     */

    memset(&pdu1, 0, sizeof(netsnmp_pdu));
    pdu1.command = AGENTX_MSG_TESTSET;
    pdu1.flags = 0;
    pdu1.sessid = 16;
    pdu1.transid = 24;
    pdu1.reqid = 132;

    pdu1.time = 10;
    pdu1.non_repeaters = 3;
    pdu1.max_repetitions = 32;
    pdu1.priority = 5;
    pdu1.range_subid = 0;

    snmp_pdu_add_variable(&pdu1, oid_buf, sizeof(oid_buf) / sizeof(oid),
                          ASN_OBJECT_ID, (char *) oid_buf2,
                          sizeof(oid_buf2));
    snmp_pdu_add_variable(&pdu1, oid_buf, sizeof(oid_buf) / sizeof(oid),
                          ASN_INTEGER, (char *) &pdu1.reqid,
                          sizeof(pdu1.reqid));
    snmp_pdu_add_variable(&pdu1, oid_buf, sizeof(oid_buf) / sizeof(oid),
                          ASN_OCTET_STR, (char *) string, strlen(string));

    printf("Test with non-network order.....\n");
    testit(&pdu1);

    printf("\nTest with network order.....\n");
    pdu1.flags |= AGENTX_FLAGS_NETWORK_BYTE_ORDER;
    testit(&pdu1);

    pdu1.community = context;
    pdu1.community_len = strlen(context);
    pdu1.flags |= AGENTX_FLAGS_NON_DEFAULT_CONTEXT;
    printf("Test with non-default context.....\n");
    testit(&pdu1);


}
#endif

/*
 * returns the proper length of an incoming agentx packet. 
 */
/*
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   h.version   |    h.type     |    h.flags    |  <reserved>   |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                          h.sessionID                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        h.transactionID                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                          h.packetID                           |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        h.payload_length                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    20 bytes in header
 */

int
agentx_check_packet(u_char * packet, size_t packet_len)
{

    if (packet_len < 20)
        return 0;               /* minimum header length == 20 */

    return agentx_parse_int(packet + 16,
                            *(packet +
                              2) & AGENTX_FLAGS_NETWORK_BYTE_ORDER) + 20;
}
