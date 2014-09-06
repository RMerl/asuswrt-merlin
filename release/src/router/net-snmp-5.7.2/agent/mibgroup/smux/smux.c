/*
 * Smux module authored by Rohit Dube.
 * Rewritten by Nick Amato <naamato@merit.net>.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <sys/types.h>
#include <ctype.h>

#if HAVE_IO_H                   /* win32 */
#include <io.h>
#endif
#include <stdio.h>
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
#if HAVE_ERR_H
#include <err.h>
#endif
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
#include <errno.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <sys/stat.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/tools.h>

#include "smux.h"
#include "mibdefs.h"
#include "snmpd.h"

netsnmp_feature_require(snprint_objid)

long            smux_long;
u_long          smux_ulong;
struct sockaddr_in smux_sa;
struct counter64 smux_counter64;
oid             smux_objid[MAX_OID_LEN];
u_char          smux_str[SMUXMAXSTRLEN];
int             smux_listen_sd = -1;

static struct timeval smux_rcv_timeout;
static long   smux_reqid;

void            init_smux(void);
static u_char  *smux_open_process(int, u_char *, size_t *, int *);
static u_char  *smux_rreq_process(int, u_char *, size_t *);
static u_char  *smux_close_process(int, u_char *, size_t *);
static u_char  *smux_trap_process(u_char *, size_t *);
static u_char  *smux_parse(u_char *, oid *, size_t *, size_t *, u_char *);
static u_char  *smux_parse_var(u_char *, size_t *, oid *, size_t *,
                               size_t *, u_char *);
static void     smux_send_close(int, int);
static void     smux_list_detach(smux_reg **, smux_reg *);
static void     smux_replace_active(smux_reg *, smux_reg *);
static void     smux_peer_cleanup(int);
static int      smux_auth_peer(oid *, size_t, char *, int);
static int      smux_build(u_char, long, oid *,
                           size_t *, u_char, u_char *, size_t, u_char *,
                           size_t *);
static int      smux_list_add(smux_reg **, smux_reg *);
static int      smux_pdu_process(int, u_char *, size_t);
static int      smux_send_rrsp(int, int);
static smux_reg *smux_find_match(smux_reg *, int, oid *, size_t, long);
static smux_reg *smux_find_replacement(oid *, size_t);
u_char         *var_smux(struct variable *, oid *, size_t *, int, size_t *,
                         WriteMethod ** write_method);
int             var_smux_write(int, u_char *, u_char, size_t, u_char *,
                               oid *, size_t);

static smux_reg *ActiveRegs;    /* Active registrations                 */
static smux_reg *PassiveRegs;   /* Currently unused registrations       */

static smux_peer_auth *Auths[SMUX_MAX_PEERS];   /* Configured peers */
static int      nauths, npeers = 0;

struct variable2 smux_variables[] = {
    /*
     * bogus entry, as in pass.c 
     */
    {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_smux, 0, {MIBINDEX}},
};



void
smux_parse_smux_socket(const char *token, char *cptr)
{
    DEBUGMSGTL(("smux", "port spec: %s\n", cptr));
    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_SMUX_SOCKET, cptr);
}

void
smux_parse_peer_auth(const char *token, char *cptr)
{
    smux_peer_auth *aptr;
    char           *password_cptr;
    int             rv;

    if ((aptr =
         (smux_peer_auth *) calloc(1, sizeof(smux_peer_auth))) == NULL) {
        snmp_log_perror("smux_parse_peer_auth: malloc");
        return;
    }
    if (nauths == SMUX_MAX_PEERS) {
	config_perror("Too many smuxpeers");
	free(aptr);
	return;
    }

    password_cptr = strchr(cptr, ' ');
    if (password_cptr)
        *(password_cptr++) = '\0';

    /*
     * oid 
     */
    aptr->sa_active_fd = -1;
    aptr->sa_oid_len = MAX_OID_LEN;
    rv = read_objid( cptr, aptr->sa_oid, &aptr->sa_oid_len );
    DEBUGMSGTL(("smux_conf", "parsing registration for: %s\n", cptr));
    if (!rv)
        config_perror("Error parsing smux oid");

    if (password_cptr != NULL) {    /* Do we have a password or not? */
	    DEBUGMSGTL(("smux_conf", "password is: %s\n",
	                SNMP_STRORNULL(password_cptr)));

        /*
         * password 
         */
        if (*password_cptr)
            strlcpy(aptr->sa_passwd, password_cptr, sizeof(aptr->sa_passwd));
    } else {
        /*
         * null passwords OK 
         */
        DEBUGMSGTL(("smux_conf", "null password\n"));
    }

    Auths[nauths++] = aptr;
    return;
}

void
smux_free_peer_auth(void)
{
    int             i;

    for (i = 0; i < nauths; i++) {
        free(Auths[i]);
        Auths[i] = NULL;
    }
    nauths = 0;
}

void
init_smux(void)
{
    snmpd_register_config_handler("smuxpeer", smux_parse_peer_auth,
                                  smux_free_peer_auth,
                                  "OID-IDENTITY PASSWORD");
    snmpd_register_config_handler("smuxsocket",
                                  smux_parse_smux_socket, NULL,
                                  "SMUX bind address");
}

void
real_init_smux(void)
{
    struct sockaddr_in lo_socket;
    char           *smux_socket;
    int             one = 1;

    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE) == SUB_AGENT) {
        smux_listen_sd = -1;
        return;
    }

    /*
     * Reqid 
     */
    smux_reqid = 0;
    smux_listen_sd = -1;

    /*
     * Receive timeout 
     */
    smux_rcv_timeout.tv_sec = 0;
    smux_rcv_timeout.tv_usec = 500000;

    /*
     * Get ready to listen on the SMUX port
     */
    memset(&lo_socket, (0), sizeof(lo_socket));
    lo_socket.sin_family = AF_INET;

    smux_socket = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
					NETSNMP_DS_SMUX_SOCKET);
#ifdef NETSNMP_ENABLE_LOCAL_SMUX
    if (!smux_socket)
        smux_socket = "127.0.0.1";   /* By default, listen on localhost only */
#endif
    netsnmp_sockaddr_in( &lo_socket, smux_socket, SMUXPORT );

    if ((smux_listen_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        snmp_log_perror("[init_smux] socket failed");
        return;
    }
#ifdef SO_REUSEADDR
    /*
     * At least on Linux, when the master agent terminates, any
     * TCP connections for SMUX peers are put in the TIME_WAIT
     * state for about 60 seconds. If the master agent is started
     * during this time, the bind for the listening socket will
     * fail because the SMUX port is in use.
     */
    if (setsockopt(smux_listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *) &one,
                   sizeof(one)) < 0) {
        snmp_log_perror("[init_smux] setsockopt(SO_REUSEADDR) failed");
    }
#endif                          /* SO_REUSEADDR */

    if (bind(smux_listen_sd, (struct sockaddr *) &lo_socket,
             sizeof(lo_socket)) < 0) {
        snmp_log_perror("[init_smux] bind failed");
        close(smux_listen_sd);
        smux_listen_sd = -1;
        return;
    }
#ifdef	SO_KEEPALIVE
    if (setsockopt(smux_listen_sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &one,
                   sizeof(one)) < 0) {
        snmp_log_perror("[init_smux] setsockopt(SO_KEEPALIVE) failed");
        close(smux_listen_sd);
        smux_listen_sd = -1;
        return;
    }
#endif                          /* SO_KEEPALIVE */

    if (listen(smux_listen_sd, SOMAXCONN) == -1) {
        snmp_log_perror("[init_smux] listen failed");
        close(smux_listen_sd);
        smux_listen_sd = -1;
        return;
    }

    DEBUGMSGTL(("smux_init",
                "[smux_init] done; smux listen sd is %d, smux port is %d\n",
                smux_listen_sd, ntohs(lo_socket.sin_port)));
}

u_char         *
var_smux(struct variable * vp,
         oid * name,
         size_t * length,
         int exact, size_t * var_len, WriteMethod ** write_method)
{
    u_char         *valptr, val_type;
    smux_reg       *rptr;

    *write_method = var_smux_write;
    /*
     * search the active registration list 
     */
    for (rptr = ActiveRegs; rptr; rptr = rptr->sr_next) {
        if (0 >= snmp_oidtree_compare(vp->name, vp->namelen, rptr->sr_name,
                                      rptr->sr_name_len))
            break;
    }
    if (rptr == NULL)
        return NULL;
    else if (exact && (*length < rptr->sr_name_len))
        return NULL;

    valptr = smux_snmp_process(exact, name, length,
                               var_len, &val_type, rptr->sr_fd);

    if (valptr == NULL)
        return NULL;

    if ((snmp_oidtree_compare(name, *length, rptr->sr_name,
                              rptr->sr_name_len)) != 0) {
        /*
         * the peer has returned a value outside
         * * of the registered tree
         */
        return NULL;
    } else {
        /*
         * set the type and return the value 
         */
        vp->type = val_type;
        return valptr;
    }
}

int
var_smux_write(int action,
               u_char * var_val,
               u_char var_val_type,
               size_t var_val_len,
               u_char * statP, oid * name, size_t name_len)
{
    smux_reg       *rptr;
    u_char          buf[SMUXMAXPKTSIZE], *ptr, sout[3], type;
    int             reterr;
    size_t          var_len, datalen, name_length, packet_len;
    size_t          len;
    ssize_t         tmp_len;
    long            reqid, errsts, erridx;
    u_char          *dataptr;

    DEBUGMSGTL(("smux", "[var_smux_write] entering var_smux_write\n"));

    len = SMUXMAXPKTSIZE;
    reterr = SNMP_ERR_NOERROR;
    var_len = var_val_len;
    name_length = name_len;

    /*
     * XXX find the descriptor again 
     */
    for (rptr = ActiveRegs; rptr; rptr = rptr->sr_next) {
        if (!snmp_oidtree_compare(name, name_len, rptr->sr_name,
                                  rptr->sr_name_len))
            break;
    }

    if (!rptr) {
        DEBUGMSGTL(("smux", "[var_smux_write] unknown registration\n"));
        return SNMP_ERR_GENERR;
    }

    switch (action) {
    case RESERVE1:
        DEBUGMSGTL(("smux", "[var_smux_write] entering RESERVE1\n"));

        /*
         * length might be long 
         */
        var_len += (*(var_val + 1) & ASN_LONG_LEN) ?
            var_len + ((*(var_val + 1) & 0x7F) + 2) : 2;

        switch (var_val_type) {
        case ASN_INTEGER:
        case ASN_OCTET_STR:
        case ASN_COUNTER:
        case ASN_GAUGE:
        case ASN_TIMETICKS:
        case ASN_UINTEGER:
        case ASN_COUNTER64:
        case ASN_IPADDRESS:
        case ASN_OPAQUE:
        case ASN_NSAP:
        case ASN_OBJECT_ID:
        case ASN_BIT_STR:
            datalen = var_val_len;
            dataptr = var_val;
            break;
        case SNMP_NOSUCHOBJECT:
        case SNMP_NOSUCHINSTANCE:
        case SNMP_ENDOFMIBVIEW:
        case ASN_NULL:
        default:
            DEBUGMSGTL(("smux",
                        "[var_smux_write] variable not supported\n"));
            return SNMP_ERR_GENERR;
            break;
        }

        if ((smux_build((u_char) SMUX_SET, smux_reqid,
                        name, &name_length, var_val_type, dataptr,
                        datalen, buf, &len)) < 0) {
            DEBUGMSGTL(("smux", "[var_smux_write] smux build failed\n"));
            return SNMP_ERR_GENERR;
        }

        if (sendto(rptr->sr_fd, (void *) buf, len, 0, NULL, 0) < 0) {
            DEBUGMSGTL(("smux", "[var_smux_write] send failed\n"));
            return SNMP_ERR_GENERR;
        }

        while (1) {
            /*
             * peek at what's received 
             */
            if ((len = recvfrom(rptr->sr_fd, (void *) buf,
                            SMUXMAXPKTSIZE, MSG_PEEK, NULL, NULL)) <= 0) {
                if ((len == -1) && ((errno == EINTR) || (errno == EAGAIN)))
                {
                   continue;
                }
                DEBUGMSGTL(("smux",
                            "[var_smux_write] peek failed or timed out\n"));
                /*
                 * do we need to do a peer cleanup in this case?? 
                 */
                smux_peer_cleanup(rptr->sr_fd);
                smux_snmp_select_list_del(rptr->sr_fd);
                return SNMP_ERR_GENERR;
            }

            DEBUGMSGTL(("smux", "[var_smux_write] Peeked at %" NETSNMP_PRIz
                        "d bytes\n", len));
            DEBUGDUMPSETUP("var_smux_write", buf, len);

            /*
             * determine if we received more than one packet 
             */
            packet_len = len;
            ptr = asn_parse_header(buf, &packet_len, &type);
            packet_len += (ptr - buf);
            if (len > (ssize_t)packet_len) {
                /*
                 * set length to receive only the first packet 
                 */
                len = packet_len;
            }

            /*
             * receive the first packet 
             */
            tmp_len = len;
            do
            {
               len = tmp_len;
               len = recvfrom(rptr->sr_fd, (void *) buf, len, 0, NULL, NULL);
            }
            while((len == -1) && ((errno == EINTR) || (errno == EAGAIN)));

            if (len <= 0) {
                DEBUGMSGTL(("smux",
                            "[var_smux_write] recv failed or timed out\n"));
                smux_peer_cleanup(rptr->sr_fd);
                smux_snmp_select_list_del(rptr->sr_fd);
                return SNMP_ERR_GENERR;
            }

            DEBUGMSGTL(("smux", "[var_smux_write] Received %" NETSNMP_PRIz
                        "d bytes\n", len));

            if (buf[0] == SMUX_TRAP) {
                DEBUGMSGTL(("smux", "[var_smux_write] Received trap\n"));
                snmp_log(LOG_INFO, "Got trap from peer on fd %d\n",
                         rptr->sr_fd);
                ptr = asn_parse_header(buf, &len, &type);
                smux_trap_process(ptr, &len);


                /*
                 * go and peek at received data again 
                 */
                /*
                 * we could receive the reply or another trap 
                 */
            } else {
                ptr = buf;
                ptr = asn_parse_header(ptr, &len, &type);
                if ((ptr == NULL) || type != SNMP_MSG_RESPONSE)
                    return SNMP_ERR_GENERR;

                ptr =
                    asn_parse_int(ptr, &len, &type, &reqid, sizeof(reqid));
                if ((ptr == NULL) || type != ASN_INTEGER)
                    return SNMP_ERR_GENERR;

                ptr =
                    asn_parse_int(ptr, &len, &type, &errsts,
                                  sizeof(errsts));
                if ((ptr == NULL) || type != ASN_INTEGER)
                    return SNMP_ERR_GENERR;

                if (errsts) {
                    DEBUGMSGTL(("smux",
                                "[var_smux_write] errsts returned\n"));
                    return (errsts);
                }

                ptr =
                    asn_parse_int(ptr, &len, &type, &erridx,
                                  sizeof(erridx));
                if ((ptr == NULL) || type != ASN_INTEGER)
                    return SNMP_ERR_GENERR;

                reterr = SNMP_ERR_NOERROR;
                break;
            }
        }                       /* while (1) */
        break;                  /* case Action == RESERVE1 */

    case RESERVE2:
        DEBUGMSGTL(("smux", "[var_smux_write] entering RESERVE2\n"));
        reterr = SNMP_ERR_NOERROR;
        break;                  /* case Action == RESERVE2 */

    case FREE:
    case COMMIT:
        ptr = sout;
        *(ptr++) = (u_char) SMUX_SOUT;
        *(ptr++) = (u_char) 1;
        if (action == FREE) {
            *ptr = (u_char) 1;  /* rollback */
            DEBUGMSGTL(("smux",
                        "[var_smux_write] entering FREE - sending RollBack \n"));
        } else {
            *ptr = (u_char) 0;  /* commit */
            DEBUGMSGTL(("smux",
                        "[var_smux_write] entering FREE - sending Commit \n"));
        }

        if ((sendto(rptr->sr_fd, (void *) sout, 3, 0, NULL, 0)) < 0) {
            DEBUGMSGTL(("smux",
                        "[var_smux_write] send rollback/commit failed\n"));
            return SNMP_ERR_GENERR;
        }

        reterr = SNMP_ERR_NOERROR;
        break;                  /* case Action == COMMIT */

    default:
        break;
    }
    return reterr;
}


int
smux_accept(int sd)
{
    u_char          data[SMUXMAXPKTSIZE], *ptr, type;
    struct sockaddr_in in_socket;
    struct timeval  tv;
    int             fail, fd;
    socklen_t       alen;
    int             length;
    size_t          len;

    alen = sizeof(struct sockaddr_in);
    /*
     * this may be too high 
     */
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    /*
     * connection request 
     */
    DEBUGMSGTL(("smux", "[smux_accept] Calling accept()\n"));
    errno = 0;
    if ((fd = accept(sd, (struct sockaddr *) &in_socket, &alen)) < 0) {
        snmp_log_perror("[smux_accept] accept failed");
        return -1;
    } else {
        snmp_log(LOG_INFO, "[smux_accept] accepted fd %d from %s:%d\n",
                 fd, inet_ntoa(in_socket.sin_addr),
                 ntohs(in_socket.sin_port));
        if (npeers + 1 == SMUXMAXPEERS) {
            snmp_log(LOG_ERR,
                     "[smux_accept] denied peer on fd %d, limit %d reached",
                     fd, SMUXMAXPEERS);
            close(sd);
            return -1;
        }

        /*
         * now block for an OpenPDU 
         */
        do
        {
           length = recvfrom(fd, (char *) data, SMUXMAXPKTSIZE, 0, NULL, NULL);
        }
        while((length == -1) && ((errno == EINTR) || (errno == EAGAIN)));

        if (length <= 0) {
            DEBUGMSGTL(("smux",
                        "[smux_accept] peer on fd %d died or timed out\n",
                        fd));
            close(fd);
            return -1;
        }
        /*
         * try to authorize him 
         */
        ptr = data;
        len = length;
        if ((ptr = asn_parse_header(ptr, &len, &type)) == NULL) {
            smux_send_close(fd, SMUXC_PACKETFORMAT);
            close(fd);
            DEBUGMSGTL(("smux", "[smux_accept] peer on %d sent bad open", fd));
            return -1;
        } else if (type != (u_char) SMUX_OPEN) {
            smux_send_close(fd, SMUXC_PROTOCOLERROR);
            close(fd);
            DEBUGMSGTL(("smux",
                        "[smux_accept] peer on %d did not send open: (%d)\n",
                        fd, type));
            return -1;
        }
        ptr = smux_open_process(fd, ptr, &len, &fail);
        if (fail) {
            smux_send_close(fd, SMUXC_AUTHENTICATIONFAILURE);
            close(fd);
            DEBUGMSGTL(("smux",
                        "[smux_accept] peer on %d failed authentication\n",
                        fd));
            return -1;
        }

        /*
         * he's OK 
         */
#ifdef SO_RCVTIMEO
        if (setsockopt
            (fd, SOL_SOCKET, SO_RCVTIMEO, (void *) &tv, sizeof(tv)) < 0) {
            DEBUGMSGTL(("smux",
                        "[smux_accept] setsockopt(SO_RCVTIMEO) failed fd %d\n",
                        fd));
            snmp_log_perror("smux_accept: setsockopt SO_RCVTIMEO");
        }
#endif
        npeers++;
        DEBUGMSGTL(("smux", "[smux_accept] fd %d\n", fd));

        /*
         * Process other PDUs already read, e.g. a registerRequest. 
         */
        len = length - (ptr - data);
        if (smux_pdu_process(fd, ptr, len) < 0) {
            /*
             * Easy come, easy go.  Clean-up is already done. 
             */
            return -1;
        }
    }
    return fd;
}

int
smux_process(int fd)
{
    int             length, tmp_length;
    u_char          data[SMUXMAXPKTSIZE];
    u_char          type, *ptr;
    size_t          packet_len;

    do
    {
       length = recvfrom(fd, (char *) data, SMUXMAXPKTSIZE, MSG_PEEK, NULL,
                         NULL);
    }
    while((length == -1) && ((errno == EINTR) || (errno == EAGAIN)));

    if (length <= 0)
    {
       snmp_log_perror("[smux_process] peek failed");
       smux_peer_cleanup(fd);
       return -1;
    }

    /*
     * determine if we received more than one packet 
     */
    packet_len = length;
    ptr = asn_parse_header(data, &packet_len, &type);
    packet_len += (ptr - data);
    if (length > packet_len) {
        /*
         * set length to receive only the first packet 
         */
        length = packet_len;
    }

    tmp_length = length;
    do
    {
       length = tmp_length;
       length = recvfrom(fd, (char *) data, length, 0, NULL, NULL);
    }
    while((length == -1) && ((errno == EINTR) || (errno == EAGAIN)));

    if (length <= 0) {
        /*
         * the peer went away, close this descriptor 
         * * and delete it from the list
         */
        DEBUGMSGTL(("smux",
                    "[smux_process] peer on fd %d died or timed out\n",
                    fd));
        smux_peer_cleanup(fd);
        return -1;
    }

    return smux_pdu_process(fd, data, length);
}

static int
smux_pdu_process(int fd, u_char * data, size_t length)
{
    int             error;
    size_t          len;
    u_char         *ptr, type;

    DEBUGMSGTL(("smux", "[smux_pdu_process] Processing %" NETSNMP_PRIz
                "d bytes\n", length));

    error = 0;
    ptr = data;
    while (error == 0 && ptr != NULL && ptr < data + length) {
        len = length - (ptr - data);
        ptr = asn_parse_header(ptr, &len, &type);
        DEBUGMSGTL(("smux", "[smux_pdu_process] type is %d\n",
                    (int) type));
        switch (type) {
        case SMUX_OPEN:
            smux_send_close(fd, SMUXC_PROTOCOLERROR);
            DEBUGMSGTL(("smux",
                        "[smux_pdu_process] peer on fd %d sent duplicate open?\n",
                        fd));
            smux_peer_cleanup(fd);
            error = -1;
            break;
        case SMUX_CLOSE:
            ptr = smux_close_process(fd, ptr, &len);
            smux_peer_cleanup(fd);
            error = -1;
            break;
        case SMUX_RREQ:
            ptr = smux_rreq_process(fd, ptr, &len);
            break;
        case SMUX_RRSP:
            error = -1;
            smux_send_close(fd, SMUXC_PROTOCOLERROR);
            smux_peer_cleanup(fd);
            DEBUGMSGTL(("smux",
                        "[smux_pdu_process] peer on fd %d sent RRSP!\n",
                        fd));
            break;
        case SMUX_SOUT:
            error = -1;
            smux_send_close(fd, SMUXC_PROTOCOLERROR);
            smux_peer_cleanup(fd);
            DEBUGMSGTL(("smux", "This shouldn't have happened!\n"));
            break;
        case SMUX_TRAP:
            snmp_log(LOG_INFO, "Got trap from peer on fd %d\n", fd);
            if (ptr)
            {
               DEBUGMSGTL(("smux", "[smux_pdu_process] call smux_trap_process.\n"));
               ptr = smux_trap_process(ptr, &len);
            }
            else
            {
               DEBUGMSGTL(("smux", "[smux_pdu_process] smux_trap_process not called: ptr=NULL.\n"));
               DEBUGMSGTL(("smux", "[smux_pdu_process] Error: \n%s\n", snmp_api_errstring(0)));
            }
            /*
             * watch out for close on top of this...should return correct end 
             */
            break;
        default:
            smux_send_close(fd, SMUXC_PACKETFORMAT);
            smux_peer_cleanup(fd);
            DEBUGMSGTL(("smux", "[smux_pdu_process] Wrong type %d\n",
                        (int) type));
            error = -1;
            break;
        }
    }
    return error;
}

static u_char  *
smux_open_process(int fd, u_char * ptr, size_t * len, int *fail)
{
    u_char          type;
    long            version;
    oid             oid_name[MAX_OID_LEN];
    char            passwd[SMUXMAXSTRLEN];
    char            descr[SMUXMAXSTRLEN];
    char            oid_print[SMUXMAXSTRLEN];
    int             i;
    size_t          oid_name_len, string_len;

    if (!(ptr = asn_parse_int(ptr, len, &type, &version, sizeof(version)))) {
        DEBUGMSGTL(("smux", "[smux_open_process] version parse failed\n"));
        *fail = TRUE;
        return ((ptr += *len));
    }
    DEBUGMSGTL(("smux",
                "[smux_open_process] version %ld, len %" NETSNMP_PRIz
                "u, type %d\n", version, *len, (int) type));

    oid_name_len = MAX_OID_LEN;
    if ((ptr = asn_parse_objid(ptr, len, &type, oid_name,
                               &oid_name_len)) == NULL) {
        DEBUGMSGTL(("smux", "[smux_open_process] oid parse failed\n"));
        *fail = TRUE;
        return ((ptr += *len));
    }
    snprint_objid(oid_print, sizeof(oid_print), oid_name, oid_name_len);

    if (snmp_get_do_debugging()) {
        DEBUGMSGTL(("smux", "[smux_open_process] smux peer: %s\n",
                    oid_print));
        DEBUGMSGTL(("smux", "[smux_open_process] len %" NETSNMP_PRIz
                    "u, type %d\n", *len, (int) type));
    }

    string_len = SMUXMAXSTRLEN;
    if ((ptr = asn_parse_string(ptr, len, &type, (u_char *) descr,
                                &string_len)) == NULL) {
        DEBUGMSGTL(("smux", "[smux_open_process] descr parse failed\n"));
        *fail = TRUE;
        return ((ptr += *len));
    }

    if (snmp_get_do_debugging()) {
        DEBUGMSGTL(("smux", "[smux_open_process] smux peer descr: "));
        for (i = 0; i < (int) string_len; i++)
            DEBUGMSG(("smux", "%c", descr[i]));
        DEBUGMSG(("smux", "\n"));
        DEBUGMSGTL(("smux", "[smux_open_process] len %" NETSNMP_PRIz
                    "u, type %d\n", *len, (int) type));
    }
    descr[string_len] = 0;

    string_len = SMUXMAXSTRLEN;
    if ((ptr = asn_parse_string(ptr, len, &type, (u_char *) passwd,
                                &string_len)) == NULL) {
        DEBUGMSGTL(("smux", "[smux_open_process] passwd parse failed\n"));
        *fail = TRUE;
        return ((ptr += *len));
    }

    if (snmp_get_do_debugging()) {
        DEBUGMSGTL(("smux", "[smux_open_process] smux peer passwd: "));
        for (i = 0; i < (int) string_len; i++)
            DEBUGMSG(("smux", "%c", passwd[i]));
        DEBUGMSG(("smux", "\n"));
        DEBUGMSGTL(("smux", "[smux_open_process] len %" NETSNMP_PRIz
                    "u, type %d\n", *len, (int) type));
    }
    passwd[string_len] = '\0';
    if (!smux_auth_peer(oid_name, oid_name_len, passwd, fd)) {
        snmp_log(LOG_WARNING,
                 "refused smux peer: oid %s, descr %s\n",
                 oid_print, descr);
        *fail = TRUE;
        return ptr;
    }
    snmp_log(LOG_INFO,
             "accepted smux peer: oid %s, descr %s\n",
             oid_print, descr);
    *fail = FALSE;
    return ptr;
}

static void
smux_send_close(int fd, int reason)
{
    u_char          outpacket[3], *ptr;

    ptr = outpacket;

    *(ptr++) = (u_char) SMUX_CLOSE;
    *(ptr++) = (u_char) 1;
    *ptr = (u_char) (reason & 0xFF);

    if (snmp_get_do_debugging())
        DEBUGMSGTL(("smux",
                    "[smux_close] sending close to fd %d, reason %d\n", fd,
                    reason));

    /*
     * send a response back 
     */
    if (sendto(fd, (char *) outpacket, 3, 0, NULL, 0) < 0) {
        snmp_log_perror("[smux_snmp_close] send failed");
    }
}


static int
smux_auth_peer(oid * name, size_t namelen, char *passwd, int fd)
{
    int             i;
    char            oid_print[SMUXMAXSTRLEN];

    if (snmp_get_do_debugging()) {
        snprint_objid(oid_print, sizeof(oid_print), name, namelen);
        DEBUGMSGTL(("smux:auth", "[smux_auth_peer] Authorizing: %s, %s\n",
                    oid_print, passwd));
    }

    for (i = 0; i < nauths; i++) {
        if (snmp_get_do_debugging()) {
            snprint_objid(oid_print, sizeof(oid_print),
                          Auths[i]->sa_oid, Auths[i]->sa_oid_len);
            DEBUGMSGTL(("smux:auth", "[smux_auth_peer] Checking OID: %s (%d)\n",
                    oid_print, i));
        }
        if (snmp_oid_compare(Auths[i]->sa_oid, Auths[i]->sa_oid_len,
                             name, namelen) == 0) {
            if (snmp_get_do_debugging()) {
                DEBUGMSGTL(("smux:auth", "[smux_auth_peer] Checking P/W: %s (%d)\n",
                        Auths[i]->sa_passwd, Auths[i]->sa_active_fd));
            }
            if (!(strcmp(Auths[i]->sa_passwd, passwd)) &&
                (Auths[i]->sa_active_fd == -1)) {
                /*
                 * matched, mark the auth 
                 */
                Auths[i]->sa_active_fd = fd;
                return 1;
            }
        }
    }
    /*
     * did not match oid and passwd 
     */
    return 0;
}


/*
 * XXX - Bells and Whistles:
 * Need to catch signal when snmpd goes down and send close pdu to gated 
 */
static u_char  *
smux_close_process(int fd, u_char * ptr, size_t * len)
{
    long            down = 0;
    int             length = *len;

    /*
     * This is the integer part of the close pdu 
     */
    while (length--) {
        down = (down << 8) | (long) *ptr;
        ptr++;
    }

    DEBUGMSGTL(("smux",
                "[smux_close_process] close from peer on fd %d reason %ld\n",
                fd, down));
    smux_peer_cleanup(fd);

    return NULL;
}

static u_char  *
smux_rreq_process(int sd, u_char * ptr, size_t * len)
{
    long            priority, rpriority;
    long            operation;
    oid             oid_name[MAX_OID_LEN];
    size_t          oid_name_len;
    int             i, result;
    u_char          type;
    smux_reg       *rptr, *nrptr;

    oid_name_len = MAX_OID_LEN;
    ptr = asn_parse_objid(ptr, len, &type, oid_name, &oid_name_len);

    DEBUGMSGTL(("smux", "[smux_rreq_process] smux subtree: "));
    DEBUGMSGOID(("smux", oid_name, oid_name_len));
    DEBUGMSG(("smux", "\n"));

    if ((ptr = asn_parse_int(ptr, len, &type, &priority,
                             sizeof(priority))) == NULL) {
        DEBUGMSGTL(("smux",
                    "[smux_rreq_process] priority parse failed\n"));
        smux_send_rrsp(sd, -1);
        return NULL;
    }
    DEBUGMSGTL(("smux", "[smux_rreq_process] priority %ld\n", priority));

    if ((ptr = asn_parse_int(ptr, len, &type, &operation,
                             sizeof(operation))) == NULL) {
        DEBUGMSGTL(("smux",
                    "[smux_rreq_process] operation parse failed\n"));
        smux_send_rrsp(sd, -1);
        return NULL;
    }
    DEBUGMSGTL(("smux", "[smux_rreq_process] operation %ld\n", operation));

    if (operation == SMUX_REGOP_DELETE) {
        /*
         * search the active list for this registration 
         */
        rptr =
            smux_find_match(ActiveRegs, sd, oid_name, oid_name_len,
                            priority);
        if (rptr) {
            rpriority = rptr->sr_priority;
            /*
             * unregister the mib 
             */
            unregister_mib(rptr->sr_name, rptr->sr_name_len);
            /*
             * find a replacement 
             */
            nrptr =
                smux_find_replacement(rptr->sr_name, rptr->sr_name_len);
            if (nrptr) {
                /*
                 * found one 
                 */
                smux_replace_active(rptr, nrptr);
            } else {
                /*
                 * no replacement found 
                 */
                smux_list_detach(&ActiveRegs, rptr);
                free(rptr);
            }
            smux_send_rrsp(sd, rpriority);
            return ptr;
        }
        /*
         * search the passive list for this registration 
         */
        rptr =
            smux_find_match(PassiveRegs, sd, oid_name, oid_name_len,
                            priority);
        if (rptr) {
            rpriority = rptr->sr_priority;
            smux_list_detach(&PassiveRegs, rptr);
            free(rptr);
            smux_send_rrsp(sd, rpriority);
            return ptr;
        }
        /*
         * This peer cannot unregister the tree, it does not
         * * belong to him.  Send him an error.
         */
        smux_send_rrsp(sd, -1);
        return ptr;

    } else if ((operation == SMUX_REGOP_REGISTER_RO) ||
               (operation == SMUX_REGOP_REGISTER_RW)) {
        if (priority < -1) {
            DEBUGMSGTL(("smux",
                        "[smux_rreq_process] peer fd %d invalid priority %ld",
                        sd, priority));
            smux_send_rrsp(sd, -1);
            return NULL;
        }
        if ((nrptr = malloc(sizeof(smux_reg))) == NULL) {
            snmp_log_perror("[smux_rreq_process] malloc");
            smux_send_rrsp(sd, -1);
            return NULL;
        }
        nrptr->sr_priority = priority;
        nrptr->sr_name_len = oid_name_len;
        nrptr->sr_fd = sd;
        for (i = 0; i < (int) oid_name_len; i++)
            nrptr->sr_name[i] = oid_name[i];

        /*
         * See if this tree matches or scopes any of the
         * * active trees.
         */
        for (rptr = ActiveRegs; rptr; rptr = rptr->sr_next) {
            result =
                snmp_oid_compare(oid_name, oid_name_len, rptr->sr_name,
                                 rptr->sr_name_len);
            if (result == 0) {
                if (oid_name_len == rptr->sr_name_len) {
                    if (nrptr->sr_priority == -1) {
                        nrptr->sr_priority = rptr->sr_priority;
                        do {
                            nrptr->sr_priority++;
                        } while (smux_list_add(&PassiveRegs, nrptr));
                        goto done;
                    } else if (nrptr->sr_priority < rptr->sr_priority) {
                        /*
                         * Better priority.  There are no better
                         * * priorities for this tree in the passive list,
                         * * so replace the current active tree.
                         */
                        smux_replace_active(rptr, nrptr);
                        goto done;
                    } else {
                        /*
                         * Equal or worse priority 
                         */
                        do {
                            nrptr->sr_priority++;
                        } while (smux_list_add(&PassiveRegs, nrptr) == -1);
                        goto done;
                    }
                } else if (oid_name_len < rptr->sr_name_len) {
                    /*
                     * This tree scopes a current active
                     * * tree.  Replace the current active tree.
                     */
                    smux_replace_active(rptr, nrptr);
                    goto done;
                } else {        /* oid_name_len > rptr->sr_name_len */
                    /*
                     * This tree is scoped by a current
                     * * active tree.  
                     */
                    do {
                        nrptr->sr_priority++;
                    } while (smux_list_add(&PassiveRegs, nrptr) == -1);
                    goto done;
                }
            }
        }
        /*
         * We didn't find it in the active list.  Add it at
         * * the requested priority.
         */
        if (nrptr->sr_priority == -1)
            nrptr->sr_priority = 0;
        smux_list_add(&ActiveRegs, nrptr);
        if (register_mib("smux", (struct variable *)
                             smux_variables, sizeof(struct variable2),
                             1, nrptr->sr_name, nrptr->sr_name_len)
                     != SNMPERR_SUCCESS) {
		DEBUGMSGTL(("smux", "[smux_rreq_process] Failed to register subtree\n"));
		smux_list_detach(&ActiveRegs, nrptr);
		free(nrptr);
		smux_send_rrsp(sd, -1);
		return NULL;
	}

      done:
        smux_send_rrsp(sd, nrptr->sr_priority);
        return ptr;
    } else {
        DEBUGMSGTL(("smux", "[smux_rreq_process] unknown operation\n"));
        smux_send_rrsp(sd, -1);
        return NULL;
    }
}

/*
 * Find the registration with a matching descriptor, OID and priority.  If
 * the priority is -1 then find a registration with a matching descriptor,
 * a matching OID, and the highest priority.
 */
static smux_reg *
smux_find_match(smux_reg * regs, int sd, oid * oid_name,
                size_t oid_name_len, long priority)
{
    smux_reg       *rptr, *bestrptr;

    bestrptr = NULL;
    for (rptr = regs; rptr; rptr = rptr->sr_next) {
        if (rptr->sr_fd != sd)
            continue;
        if (snmp_oid_compare
            (rptr->sr_name, rptr->sr_name_len, oid_name, oid_name_len))
            continue;
        if (rptr->sr_priority == priority)
            return rptr;
        if (priority != -1)
            continue;
        if (bestrptr) {
            if (bestrptr->sr_priority > rptr->sr_priority)
                bestrptr = rptr;
        } else {
            bestrptr = rptr;
        }
    }
    return bestrptr;
}

static void
smux_replace_active(smux_reg * actptr, smux_reg * pasptr)
{
    smux_list_detach(&ActiveRegs, actptr);
    unregister_mib(actptr->sr_name, actptr->sr_name_len);

    smux_list_detach(&PassiveRegs, pasptr);
    (void) smux_list_add(&ActiveRegs, pasptr);

    register_mib("smux", (struct variable *) smux_variables,
                 sizeof(struct variable2), 1, pasptr->sr_name,
                 pasptr->sr_name_len);
    free(actptr);
}

static void
smux_list_detach(smux_reg ** head, smux_reg * m_remove)
{
    smux_reg       *rptr, *rptr2;

    if (*head == NULL) {
        DEBUGMSGTL(("smux", "[smux_list_detach] Ouch!"));
        return;
    }
    if (*head == m_remove) {
        *head = (*head)->sr_next;
        return;
    }
    for (rptr = *head, rptr2 = rptr->sr_next; rptr2;
         rptr2 = rptr2->sr_next, rptr = rptr->sr_next) {
        if (rptr2 == m_remove) {
            rptr->sr_next = rptr2->sr_next;
            return;
        }
    }
}

/*
 * Attempt to add a registration (in order) to a list.  If the
 * add fails (because of an existing registration with equal
 * priority) return -1.
 */
static int
smux_list_add(smux_reg ** head, smux_reg * add)
{
    smux_reg       *rptr, *prev;
    int             result;

    if (*head == NULL) {
        *head = add;
        (*head)->sr_next = NULL;
        return 0;
    }
    prev = NULL;
    for (rptr = *head; rptr; rptr = rptr->sr_next) {
        result = snmp_oid_compare(add->sr_name, add->sr_name_len,
                                  rptr->sr_name, rptr->sr_name_len);
        if (result == 0) {
            /*
             * Same tree...
             */
            if (add->sr_priority == rptr->sr_priority) {
                /*
                 * ... same pri : nope 
                 */
                return -1;
            } else if (add->sr_priority < rptr->sr_priority) {
                /*
                 * ... lower pri : insert and return
                 */
                add->sr_next = rptr;
                if ( prev ) { prev->sr_next = add; }
                else        {         *head = add; }
                return 0;
#ifdef XXX
            } else {
                /*
                 * ... higher pri : put after 
                 */
                add->sr_next  = rptr->sr_next;
                rptr->sr_next = add;
#endif
            }
        } else if (result < 0) {
            /*
             * Earlier tree : insert and return
             */
            add->sr_next = rptr;
            if ( prev ) { prev->sr_next = add; }
            else        {         *head = add; }
            return 0;
#ifdef XXX
        } else  {
            /*
             * Later tree : put after
             */
            add->sr_next = rptr->sr_next;
            rptr->sr_next = add;
            return 0;
#endif
        }
        prev = rptr;
    }
    /*
     * Otherwise, this entry must come last
     */
    if ( prev ) { prev->sr_next = add; }
    else        {         *head = add; }
    add->sr_next = NULL;
    return 0;
}

/*
 * Find a replacement for this registration.  In order
 * of preference:
 *
 *      - Least difference in subtree length
 *      - Best (lowest) priority
 *
 * For example, if we need to replace .1.3.6.1.69, 
 * we would pick .1.3.6.1.69.1 instead of .1.3.6.69.1.1
 *
 */
static smux_reg *
smux_find_replacement(oid * name, size_t name_len)
{
    smux_reg       *rptr, *bestptr;
    int             bestlen, difflen;

    bestlen = SMUX_MAX_PRIORITY;
    bestptr = NULL;

    for (rptr = PassiveRegs; rptr; rptr = rptr->sr_next) {
        if (!snmp_oidtree_compare(rptr->sr_name, rptr->sr_name_len,
                                  name, name_len)) {
            if ((difflen = rptr->sr_name_len - name_len)
                < bestlen || !bestptr) {
                bestlen = difflen;
                bestptr = rptr;
            } else if ((difflen == bestlen) &&
                       (rptr->sr_priority < bestptr->sr_priority))
                bestptr = rptr;
        }
    }
    return bestptr;
}

u_char         *
smux_snmp_process(int exact,
                  oid * objid,
                  size_t * len,
                  size_t * return_len, u_char * return_type, int sd)
{
    u_char          packet[SMUXMAXPKTSIZE], *ptr, result[SMUXMAXPKTSIZE];
    ssize_t         length = SMUXMAXPKTSIZE;
    int             tmp_length;
    u_char          type;
    size_t          packet_len;

    /*
     * Send the query to the peer
     */
    smux_reqid++;

    if (exact)
        type = SMUX_GET;
    else
        type = SMUX_GETNEXT;

    if (smux_build(type, smux_reqid, objid, len, 0, NULL,
                   *len, packet, (size_t *) &length) < 0) {
        snmp_log(LOG_ERR, "[smux_snmp_process]: smux_build failed\n");
        return NULL;
    }
    DEBUGMSGTL(("smux", "[smux_snmp_process] oid from build: "));
    DEBUGMSGOID(("smux", objid, *len));
    DEBUGMSG(("smux", "\n"));

    if (sendto(sd, (char *) packet, length, 0, NULL, 0) < 0) {
        snmp_log_perror("[smux_snmp_process] send failed");
    }

    DEBUGMSGTL(("smux",
                "[smux_snmp_process] Sent %d request to peer; %" NETSNMP_PRIz "d bytes\n",
                (int) type, length));

    while (1) {
        /*
         * peek at what's received 
         */
        length = recvfrom(sd, (char *) result, SMUXMAXPKTSIZE, MSG_PEEK, NULL,
                          NULL);
        if (length <= 0) {
            if ((length == -1) && ((errno == EINTR) || (errno == EAGAIN)))
            {
               continue;
            }
            else
            {
               snmp_log_perror("[smux_snmp_process] peek failed");
               smux_peer_cleanup(sd);
               smux_snmp_select_list_del(sd);
               return NULL;
            }
        }

        DEBUGMSGTL(("smux", "[smux_snmp_process] Peeked at %" NETSNMP_PRIz "d bytes\n",
                    length));
        DEBUGDUMPSETUP("smux_snmp_process", result, length);

        /*
         * determine if we received more than one packet 
         */
        packet_len = length;
        ptr = asn_parse_header(result, &packet_len, &type);
        packet_len += (ptr - result);
        if (length > packet_len) {
            /*
             * set length to receive only the first packet 
             */
            length = packet_len;
        }

        /*
         * receive the first packet 
         */
        tmp_length = length;
        do
        {
           length = tmp_length;
           length = recvfrom(sd, (char *) result, length, 0, NULL, NULL);
        }
        while((length == -1) && ((errno == EINTR) || (errno == EAGAIN)));

        if (length <= 0) {
           snmp_log_perror("[smux_snmp_process] recv failed");
           smux_peer_cleanup(sd);
           smux_snmp_select_list_del(sd);
           return NULL;
        }

        DEBUGMSGTL(("smux", "[smux_snmp_process] Received %" NETSNMP_PRIz "d bytes\n",
                    length));

        if (result[0] == SMUX_TRAP) {
            DEBUGMSGTL(("smux", "[smux_snmp_process] Received trap\n"));
            snmp_log(LOG_INFO, "Got trap from peer on fd %d\n", sd);
            ptr = asn_parse_header(result, (size_t *) &length, &type);
            smux_trap_process(ptr, (size_t *) &length);

            /*
             * go and peek at received data again 
             */
            /*
             * we could receive the reply or another trap 
             */
        } else {
            /*
             * Interpret reply 
             */
            ptr = smux_parse(result, objid, len, return_len, return_type);
            /*
             * ptr will point to query result or NULL if error 
             */
            break;
        }
    }                           /* while (1) */

    return ptr;
}

static u_char  *
smux_parse(u_char * rsp,
           oid * objid,
           size_t * oidlen, size_t * return_len, u_char * return_type)
{
    size_t          length = SMUXMAXPKTSIZE;
    u_char         *ptr, type;
    long            reqid, errstat, errindex;

    ptr = rsp;

    /*
     * Return pointer to the snmp/smux return value.
     * return_len should contain the number of bytes in the value
     * returned above.
     * objid is the next object, with len for GETNEXT.
     * objid and len are not changed for GET
     */
    ptr = asn_parse_header(ptr, &length, &type);
    if (ptr == NULL || type != SNMP_MSG_RESPONSE)
        return NULL;

    if ((ptr = asn_parse_int(ptr, &length, &type, &reqid,
                             sizeof(reqid))) == NULL) {
        DEBUGMSGTL(("smux", "[smux_parse] parse of reqid failed\n"));
        return NULL;
    }
    if ((ptr = asn_parse_int(ptr, &length, &type, &errstat,
                             sizeof(errstat))) == NULL) {
        DEBUGMSGTL(("smux",
                    "[smux_parse] parse of error status failed\n"));
        return NULL;
    }
    if ((ptr = asn_parse_int(ptr, &length, &type, &errindex,
                             sizeof(errindex))) == NULL) {
        DEBUGMSGTL(("smux", "[smux_parse] parse of error index failed\n"));
        return NULL;
    }

    /*
     * XXX How to send something intelligent back in case of an error 
     */
    DEBUGMSGTL(("smux",
                "[smux_parse] Message type %d, reqid %ld, errstat %ld, \n\terrindex %ld\n",
                (int) type, reqid, errstat, errindex));
    if (ptr == NULL || errstat != SNMP_ERR_NOERROR)
        return NULL;

    /*
     * stuff to return 
     */
    return (smux_parse_var
            (ptr, &length, objid, oidlen, return_len, return_type));
}


static u_char  *
smux_parse_var(u_char * varbind,
               size_t * varbindlength,
               oid * objid,
               size_t * oidlen, size_t * varlength, u_char * vartype)
{
    oid             var_name[MAX_OID_LEN];
    size_t          var_name_len;
    size_t          var_val_len;
    u_char         *var_val;
    size_t          str_len, objid_len;
    size_t          len;
    u_char         *ptr;
    u_char          type;

    ptr = varbind;
    len = *varbindlength;

    DEBUGMSGTL(("smux", "[smux_parse_var] before any processing: "));
    DEBUGMSGOID(("smux", objid, *oidlen));
    DEBUGMSG(("smux", "\n"));

    ptr = asn_parse_header(ptr, &len, &type);
    if (ptr == NULL || type != (ASN_SEQUENCE | ASN_CONSTRUCTOR)) {
        snmp_log(LOG_NOTICE, "[smux_parse_var] Panic: type %d\n",
                 (int) type);
        return NULL;
    }

    /*
     * get hold of the objid and the asn1 coded value 
     */
    var_name_len = MAX_OID_LEN;
    ptr = snmp_parse_var_op(ptr, var_name, &var_name_len, vartype,
                            &var_val_len, &var_val, &len);

    *oidlen = var_name_len;
    memcpy(objid, var_name, var_name_len * sizeof(oid));

    DEBUGMSGTL(("smux", "[smux_parse_var] returning oid : "));
    DEBUGMSGOID(("smux", objid, *oidlen));
    DEBUGMSG(("smux", "\n"));
    /*
     * XXX 
     */
    len = SMUXMAXPKTSIZE;
    DEBUGMSGTL(("smux",
                "[smux_parse_var] Asn coded len of var %" NETSNMP_PRIz
                "u, type %d\n", var_val_len, (int) *vartype));

    switch ((short) *vartype) {
    case ASN_INTEGER:
        *varlength = sizeof(long);
        asn_parse_int(var_val, &len, vartype,
                      (long *) &smux_long, *varlength);
        return (u_char *) & smux_long;
        break;
    case ASN_COUNTER:
    case ASN_GAUGE:
    case ASN_TIMETICKS:
    case ASN_UINTEGER:
        *varlength = sizeof(u_long);
        asn_parse_unsigned_int(var_val, &len, vartype,
                               (u_long *) & smux_ulong, *varlength);
        return (u_char *) & smux_ulong;
        break;
    case ASN_COUNTER64:
        *varlength = sizeof(smux_counter64);
        asn_parse_unsigned_int64(var_val, &len, vartype,
                                 (struct counter64 *) &smux_counter64,
                                 *varlength);
        return (u_char *) & smux_counter64;
        break;
    case ASN_IPADDRESS:
        *varlength = 4;
        /*
         * consume the tag and length, but just copy here
         * because we know it is an ip address
         */
        if ((var_val = asn_parse_header(var_val, &len, &type)) == NULL)
            return NULL;
        memcpy((u_char *) & (smux_sa.sin_addr.s_addr), var_val,
               *varlength);
        return (u_char *) & (smux_sa.sin_addr.s_addr);
        break;
    case ASN_OCTET_STR:
        /*
         * XXX 
         */
        if (len == 0)
            return NULL;
        str_len = SMUXMAXSTRLEN;
        asn_parse_string(var_val, &len, vartype, smux_str, &str_len);
        *varlength = str_len;
        return smux_str;
        break;
    case ASN_OPAQUE:
    case ASN_NSAP:
    case ASN_OBJECT_ID:
        objid_len = MAX_OID_LEN;
        asn_parse_objid(var_val, &len, vartype, smux_objid, &objid_len);
        *varlength = objid_len * sizeof(oid);
        return (u_char *) smux_objid;
        break;
    case SNMP_NOSUCHOBJECT:
    case SNMP_NOSUCHINSTANCE:
    case SNMP_ENDOFMIBVIEW:
    case ASN_NULL:
        return NULL;
        break;
    case ASN_BIT_STR:
        /*
         * XXX 
         */
        if (len == 0)
            return NULL;
        str_len = SMUXMAXSTRLEN;
        asn_parse_bitstring(var_val, &len, vartype, smux_str, &str_len);
        *varlength = str_len;
        return (u_char *) smux_str;
        break;
    default:
        snmp_log(LOG_ERR, "bad type returned (%x)\n", *vartype);
        return NULL;
        break;
    }
}

/*
 * XXX This is a bad hack - do not want to muck with ucd code 
 */
static int
smux_build(u_char type,
           long reqid,
           oid * objid,
           size_t * oidlen,
           u_char val_type,
           u_char * val, size_t val_len, u_char * packet, size_t * length)
{
    u_char         *ptr, *save1, *save2;
    size_t          len;
    long            errstat = 0;
    long            errindex = 0;

    /*
     * leave space for Seq and length 
     */
    save1 = packet;
    ptr = packet + 4;
    len = *length - 4;

    /*
     * build reqid 
     */
    ptr = asn_build_int(ptr, &len,
                                 (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                           ASN_INTEGER), &reqid,
                                 sizeof(reqid));
    if (ptr == NULL) {
        return -1;
    }

    /*
     * build err stat 
     */
    ptr = asn_build_int(ptr, &len,
                        (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                  ASN_INTEGER), &errstat, sizeof(errstat));
    if (ptr == NULL) {
        return -1;
    }

    /*
     * build err index 
     */
    ptr = asn_build_int(ptr, &len,
                        (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                  ASN_INTEGER), &errindex,
                        sizeof(errindex));
    if (ptr == NULL) {
        return -1;
    }

    save2 = ptr;
    ptr += 4;
    len -= 4;

    if (type != SMUX_SET) {
        val_type = ASN_NULL;
        val_len = 0;
    }

    /*
     * build var list : snmp_build_var_op not liked by gated XXX 
     */
    ptr = snmp_build_var_op(ptr, objid, oidlen, val_type, val_len,
                            val, &len);
    if (ptr == NULL) {
        return -1;
    }

    len = ptr - save1;
    asn_build_sequence(save1, &len, type, (ptr - save1 - 4));

    len = ptr - save2;
    asn_build_sequence(save2, &len,
                       (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                       (ptr - save2 - 4));

    *length = ptr - packet;

    return 0;
}

static void
smux_peer_cleanup(int sd)
{
    smux_reg       *nrptr, *rptr, *rptr2;
    int             i;

    /*
     * close the descriptor 
     */
    close(sd);

    /*
     * delete all of the passive registrations that this peer owns 
     */
    for (rptr = PassiveRegs; rptr; rptr = nrptr) {
        nrptr = rptr->sr_next;
        if (rptr->sr_fd == sd) {
            smux_list_detach(&PassiveRegs, rptr);
            free(rptr);
        }
        rptr = nrptr;
    }
    /*
     * find replacements for all of the active registrations found 
     */
    for (rptr = ActiveRegs; rptr; rptr = rptr2) {
        rptr2 = rptr->sr_next;
        if (rptr->sr_fd == sd) {
            smux_list_detach(&ActiveRegs, rptr);
            unregister_mib(rptr->sr_name, rptr->sr_name_len);
            if ((nrptr = smux_find_replacement(rptr->sr_name,
                                               rptr->sr_name_len)) !=
                NULL) {
                smux_list_detach(&PassiveRegs, nrptr);
                smux_list_add(&ActiveRegs, nrptr);
                register_mib("smux", (struct variable *)
                             smux_variables, sizeof(struct variable2),
                             1, nrptr->sr_name, nrptr->sr_name_len);
            }
            free(rptr);
        }
    }

    /*
     * decrement the peer count 
     */
    npeers--;

    /*
     * make his auth available again 
     */
    for (i = 0; i < nauths; i++) {
        if (Auths[i]->sa_active_fd == sd) {
            char            oid_name[128];
            Auths[i]->sa_active_fd = -1;
            snprint_objid(oid_name, sizeof(oid_name), Auths[i]->sa_oid,
                          Auths[i]->sa_oid_len);
            snmp_log(LOG_INFO, "peer disconnected: %s\n", oid_name);
        }
    }
}

int
smux_send_rrsp(int sd, int pri)
{
    u_char          outdata[2 + sizeof(int)];
    u_char         *ptr = outdata;
    int             intsize = sizeof(int);
    u_int           mask = ((u_int) 0xFF) << (8 * (sizeof(int) - 1));
    /*
     * e.g. mask is 0xFF000000 on a 32-bit machine 
     */
    int             sent;

    /*
     * This is kind of like calling asn_build_int(), but the
     * encoding will always be the size of an integer on this
     * machine, never shorter.
     */
    *ptr++ = (u_char) SMUX_RRSP;
    *ptr++ = (u_char) intsize;

    /*
     * Copy each byte, most significant first. 
     */
    while (intsize--) {
        *ptr++ = (u_char) ((pri & mask) >> (8 * (sizeof(int) - 1)));
        pri <<= 8;
    }

    sent = sendto(sd, (char *) outdata, sizeof outdata, 0, NULL, 0);
    if (sent < 0) {
        DEBUGMSGTL(("smux", "[smux_send_rrsp] send failed\n"));
    }
    return (sent);
}

static u_char  *
smux_trap_process(u_char * rsp, size_t * len)
{
    oid             sa_enterpriseoid[MAX_OID_LEN], var_name[MAX_OID_LEN];
    size_t          datalen, var_name_len, var_val_len, maxlen;
    size_t          sa_enterpriseoid_len;
    u_char          vartype, *ptr, *var_val;

    long            trap, specific;
    u_long          timestamp;

    netsnmp_variable_list *snmptrap_head, *snmptrap_ptr, *snmptrap_tmp;
    snmptrap_head = NULL;
    snmptrap_ptr = NULL;

    ptr = rsp;

    /*
     * parse the sub-agent enterprise oid 
     */
    sa_enterpriseoid_len = MAX_OID_LEN;
    if ((ptr = asn_parse_objid(ptr, len,
                               &vartype, (oid *) & sa_enterpriseoid,
                               &sa_enterpriseoid_len)) == NULL) {
        DEBUGMSGTL(("smux",
                    "[smux_trap_process] asn_parse_objid failed\n"));
        return NULL;
    }

    /*
     * parse the agent-addr ipAddress 
     */
    datalen = SMUXMAXSTRLEN;
    if (((ptr = asn_parse_string(ptr, len,
                                 &vartype, smux_str,
                                 &datalen)) == NULL) ||
        (vartype != (u_char) ASN_IPADDRESS)) {
        DEBUGMSGTL(("smux",
                    "[smux_trap_process] asn_parse_string failed\n"));
        return NULL;
    }

    /*
     * parse the generic trap int 
     */
    datalen = sizeof(long);
    if ((ptr = asn_parse_int(ptr, len, &vartype, &trap, datalen)) == NULL) {
        DEBUGMSGTL(("smux",
                    "[smux_trap_process] asn_parse_int generic failed\n"));
        return NULL;
    }

    /*
     * parse the specific trap int 
     */
    datalen = sizeof(long);
    if ((ptr = asn_parse_int(ptr, len,
                             &vartype, &specific, datalen)) == NULL) {
        DEBUGMSGTL(("smux",
                    "[smux_trap_process] asn_parse_int specific failed\n"));
        return NULL;
    }

    /*
     * parse the timeticks timestamp 
     */
    datalen = sizeof(u_long);
    if (((ptr = asn_parse_unsigned_int(ptr, len,
                                       &vartype, (u_long *) & timestamp,
                                       datalen)) == NULL) ||
        (vartype != (u_char) ASN_TIMETICKS)) {
        DEBUGMSGTL(("smux",
                    "[smux_trap_process] asn_parse_unsigned_int (timestamp) failed\n"));
        return NULL;
    }

    /*
     * parse out the overall sequence 
     */
    ptr = asn_parse_header(ptr, len, &vartype);
    if (ptr == NULL || vartype != (ASN_SEQUENCE | ASN_CONSTRUCTOR)) {
        return NULL;
    }

    /*
     * parse the variable bindings 
     */
    while (ptr && *len) {

        /*
         * get the objid and the asn1 coded value 
         */
        var_name_len = MAX_OID_LEN;
        ptr = snmp_parse_var_op(ptr, var_name, &var_name_len, &vartype,
                                &var_val_len, (u_char **) & var_val, len);

        if (ptr == NULL) {
            return NULL;
        }

        maxlen = SMUXMAXPKTSIZE;
        switch ((short) vartype) {
        case ASN_INTEGER:
            var_val_len = sizeof(long);
            asn_parse_int(var_val, &maxlen, &vartype,
                          (long *) &smux_long, var_val_len);
            var_val = (u_char *) & smux_long;
            break;
        case ASN_COUNTER:
        case ASN_GAUGE:
        case ASN_TIMETICKS:
        case ASN_UINTEGER:
            var_val_len = sizeof(u_long);
            asn_parse_unsigned_int(var_val, &maxlen, &vartype,
                                   (u_long *) & smux_ulong, var_val_len);
            var_val = (u_char *) & smux_ulong;
            break;
        case ASN_COUNTER64:
            var_val_len = sizeof(smux_counter64);
            asn_parse_unsigned_int64(var_val, &maxlen, &vartype,
                                     (struct counter64 *) &smux_counter64,
                                     var_val_len);
            var_val = (u_char *) & smux_counter64;
            break;
        case ASN_IPADDRESS:
            var_val_len = 4;
            /*
             * consume the tag and length, but just copy here
             * because we know it is an ip address
             */
            if ((var_val =
                 asn_parse_header(var_val, &maxlen, &vartype)) == NULL)
                return NULL;
            memcpy((u_char *) & (smux_sa.sin_addr.s_addr), var_val,
                   var_val_len);
            var_val = (u_char *) & (smux_sa.sin_addr.s_addr);
            break;
        case ASN_OPAQUE:
        case ASN_OCTET_STR:
            /*
             * XXX 
             */
            if (len == NULL)
                return NULL;
            var_val_len = SMUXMAXSTRLEN;
            asn_parse_string(var_val, &maxlen, &vartype,
                             smux_str, &var_val_len);
            var_val = smux_str;
            break;
        case ASN_OBJECT_ID:
            var_val_len = MAX_OID_LEN;
            asn_parse_objid(var_val, &maxlen, &vartype,
                            smux_objid, &var_val_len);
            var_val_len *= sizeof(oid);
            var_val = (u_char *) smux_objid;
            break;
        case SNMP_NOSUCHOBJECT:
        case SNMP_NOSUCHINSTANCE:
        case SNMP_ENDOFMIBVIEW:
        case ASN_NULL:
            var_val = NULL;
            break;
        case ASN_BIT_STR:
            /*
             * XXX 
             */
            if (len == NULL)
                return NULL;
            var_val_len = SMUXMAXSTRLEN;
            asn_parse_bitstring(var_val, &maxlen, &vartype,
                                smux_str, &var_val_len);
            var_val = (u_char *) smux_str;
            break;
        case ASN_NSAP:
        default:
            snmp_log(LOG_ERR, "bad type returned (%x)\n", vartype);
            var_val = NULL;
            break;
        }

        snmptrap_tmp =
            (netsnmp_variable_list *)
            malloc(sizeof(netsnmp_variable_list));
        if (snmptrap_tmp == NULL)
            return NULL;
        memset(snmptrap_tmp, 0, sizeof(netsnmp_variable_list));
        if (snmptrap_head == NULL) {
            snmptrap_head = snmptrap_tmp;
            snmptrap_ptr = snmptrap_head;
        } else {
            snmptrap_ptr->next_variable = snmptrap_tmp;
            snmptrap_ptr = snmptrap_ptr->next_variable;
        }

        snmptrap_ptr->type = vartype;
        snmptrap_ptr->next_variable = NULL;
        snmp_set_var_objid(snmptrap_ptr, var_name, var_name_len);
        snmp_set_var_value(snmptrap_ptr, (char *) var_val, var_val_len);

    }

    /*
     * send the traps 
     */
    send_enterprise_trap_vars(trap, specific, (oid *) & sa_enterpriseoid,
                              sa_enterpriseoid_len, snmptrap_head);

    /*
     * free trap variables 
     */
    snmp_free_varbind(snmptrap_head);

    return ptr;

}

#define NUM_SOCKETS	32
static int      sdlist[NUM_SOCKETS], sdlen = 0;

int smux_snmp_select_list_add(int sd)
{
   if (sdlen < NUM_SOCKETS)
   {
      sdlist[sdlen++] = sd;
      return(1);
   }
   return(0);
}

int smux_snmp_select_list_del(int sd)
{
   int i, found=0;

   for (i = 0; i < (sdlen); i++) {
      if (sdlist[i] == sd)
      {
         sdlist[i] = 0;
         found = 1;
      }
      if ((found) &&(i < (sdlen - 1)))
         sdlist[i] = sdlist[i + 1];
   }
   if (found)
   {
      sdlen--;
      return(1);
   }
   return(0);
}

int smux_snmp_select_list_get_length(void)
{
   return(sdlen);
}

int smux_snmp_select_list_get_SD_from_List(int pos)
{
   if (pos < NUM_SOCKETS)
   {
      return(sdlist[pos]);
   }
   return(0);
}
