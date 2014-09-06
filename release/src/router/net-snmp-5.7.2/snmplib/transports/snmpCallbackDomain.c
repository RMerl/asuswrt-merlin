#include <net-snmp/net-snmp-config.h>

#include <net-snmp/library/snmpCallbackDomain.h>

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>

#ifdef WIN32
#include <net-snmp/library/winpipe.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>

#ifndef NETSNMP_STREAM_QUEUE_LEN
#define NETSNMP_STREAM_QUEUE_LEN  5
#endif

#ifdef NETSNMP_TRANSPORT_CALLBACK_DOMAIN

static netsnmp_transport_list *trlist = NULL;

static int      callback_count = 0;

typedef struct callback_hack_s {
    void           *orig_transport_data;
    netsnmp_pdu    *pdu;
} callback_hack;

typedef struct callback_queue_s {
    int             callback_num;
    netsnmp_callback_pass *item;
    struct callback_queue_s *next, *prev;
} callback_queue;

callback_queue *thequeue;

static netsnmp_transport *
find_transport_from_callback_num(int num)
{
    static netsnmp_transport_list *ptr;
    for (ptr = trlist; ptr; ptr = ptr->next)
        if (((netsnmp_callback_info *) ptr->transport->data)->
            callback_num == num)
            return ptr->transport;
    return NULL;
}

static void
callback_debug_pdu(const char *ourstring, netsnmp_pdu *pdu)
{
    netsnmp_variable_list *vb;
    int             i = 1;
    DEBUGMSGTL((ourstring,
                "PDU: command = %d, errstat = %ld, errindex = %ld\n",
                pdu->command, pdu->errstat, pdu->errindex));
    for (vb = pdu->variables; vb; vb = vb->next_variable) {
        DEBUGMSGTL((ourstring, "  var %d:", i++));
        DEBUGMSGVAR((ourstring, vb));
        DEBUGMSG((ourstring, "\n"));
    }
}

void
callback_push_queue(int num, netsnmp_callback_pass *item)
{
    callback_queue *newitem = SNMP_MALLOC_TYPEDEF(callback_queue);
    callback_queue *ptr;

    if (newitem == NULL)
        return;
    newitem->callback_num = num;
    newitem->item = item;
    if (thequeue) {
        for (ptr = thequeue; ptr && ptr->next; ptr = ptr->next) {
        }
        ptr->next = newitem;
        newitem->prev = ptr;
    } else {
        thequeue = newitem;
    }
    DEBUGIF("dump_send_callback_transport") {
        callback_debug_pdu("dump_send_callback_transport", item->pdu);
    }
}

netsnmp_callback_pass *
callback_pop_queue(int num)
{
    netsnmp_callback_pass *cp;
    callback_queue *ptr;

    for (ptr = thequeue; ptr; ptr = ptr->next) {
        if (ptr->callback_num == num) {
            if (ptr->prev) {
                ptr->prev->next = ptr->next;
            } else {
                thequeue = ptr->next;
            }
            if (ptr->next) {
                ptr->next->prev = ptr->prev;
            }
            cp = ptr->item;
            SNMP_FREE(ptr);
            DEBUGIF("dump_recv_callback_transport") {
                callback_debug_pdu("dump_recv_callback_transport",
                                   cp->pdu);
            }
            return cp;
        }
    }
    return NULL;
}

/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.  
 */

char *
netsnmp_callback_fmtaddr(netsnmp_transport *t, void *data, int len)
{
    char buf[SPRINT_MAX_LEN];
    netsnmp_callback_info *mystuff;

    if (!t)
        return strdup("callback: unknown");

    mystuff = (netsnmp_callback_info *) t->data;

    if (!mystuff)
        return strdup("callback: unknown");

    snprintf(buf, SPRINT_MAX_LEN, "callback: %d on fd %d",
             mystuff->callback_num, mystuff->pipefds[0]);
    return strdup(buf);
}



/*
 * You can write something into opaque that will subsequently get passed back 
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...  
 */

int
netsnmp_callback_recv(netsnmp_transport *t, void *buf, int size,
		      void **opaque, int *olength)
{
    int rc = -1;
    char newbuf[1];
    netsnmp_callback_info *mystuff = (netsnmp_callback_info *) t->data;

    DEBUGMSGTL(("transport_callback", "hook_recv enter\n"));

    while (rc < 0) {
#ifdef WIN32
	rc = recvfrom(mystuff->pipefds[0], newbuf, 1, 0, NULL, 0);
#else
	rc = read(mystuff->pipefds[0], newbuf, 1);
#endif
	if (rc < 0 && errno != EINTR) {
	    break;
	}
    }
    if (rc > 0)
        memset(buf, 0, rc);

    if (mystuff->linkedto) {
        /*
         * we're the client.  We don't need to do anything. 
         */
    } else {
        /*
         * malloc the space here, but it's filled in by
         * snmp_callback_created_pdu() below 
         */
        int            *returnnum = (int *) calloc(1, sizeof(int));
        *opaque = returnnum;
        *olength = sizeof(int);
    }
    DEBUGMSGTL(("transport_callback", "hook_recv exit\n"));
    return rc;
}



int
netsnmp_callback_send(netsnmp_transport *t, void *buf, int size,
		      void **opaque, int *olength)
{
    int from, rc = -1;
    netsnmp_callback_info *mystuff = (netsnmp_callback_info *) t->data;
    netsnmp_callback_pass *cp;

    /*
     * extract the pdu from the hacked buffer 
     */
    netsnmp_transport *other_side;
    callback_hack  *ch = (callback_hack *) * opaque;
    netsnmp_pdu    *pdu = ch->pdu;
    *opaque = ch->orig_transport_data;
    SNMP_FREE(ch);

    DEBUGMSGTL(("transport_callback", "hook_send enter\n"));

    cp = SNMP_MALLOC_TYPEDEF(netsnmp_callback_pass);
    if (!cp)
        return -1;

    cp->pdu = snmp_clone_pdu(pdu);
    if (cp->pdu->transport_data) {
        /*
         * not needed and not properly freed later 
         */
        SNMP_FREE(cp->pdu->transport_data);
    }

    if (cp->pdu->flags & UCD_MSG_FLAG_EXPECT_RESPONSE)
        cp->pdu->flags ^= UCD_MSG_FLAG_EXPECT_RESPONSE;

    /*
     * push the sent pdu onto the stack 
     */
    /*
     * AND send a bogus byte to the remote callback receiver's pipe 
     */
    if (mystuff->linkedto) {
        /*
         * we're the client, send it to the parent 
         */
        cp->return_transport_num = mystuff->callback_num;

        other_side = find_transport_from_callback_num(mystuff->linkedto);
        if (!other_side) {
            snmp_free_pdu(cp->pdu);
            SNMP_FREE(cp);
            return -1;
        }

	while (rc < 0) {
#ifdef WIN32
	    rc = sendto(((netsnmp_callback_info*) other_side->data)->pipefds[1], " ", 1, 0, NULL, 0);
#else
	    rc = write(((netsnmp_callback_info *)other_side->data)->pipefds[1],
		       " ", 1);
#endif
	    if (rc < 0 && errno != EINTR) {
		break;
	    }
	}
        callback_push_queue(mystuff->linkedto, cp);
        /*
         * we don't need the transport data any more 
         */
        SNMP_FREE(*opaque);
    } else {
        /*
         * we're the server, send it to the person that sent us the request 
         */
        from = **((int **) opaque);
        /*
         * we don't need the transport data any more 
         */
        SNMP_FREE(*opaque);
        other_side = find_transport_from_callback_num(from);
        if (!other_side) {
            snmp_free_pdu(cp->pdu);
            SNMP_FREE(cp);
            return -1;
        }
	while (rc < 0) {
#ifdef WIN32
	    rc = sendto(((netsnmp_callback_info*) other_side->data)->pipefds[1], " ", 1, 0, NULL, 0);
#else
	    rc = write(((netsnmp_callback_info *)other_side->data)->pipefds[1],
		       " ", 1);
#endif
	    if (rc < 0 && errno != EINTR) {
		break;
	    }
	}
        callback_push_queue(from, cp);
    }

    DEBUGMSGTL(("transport_callback", "hook_send exit\n"));
    return 0;
}



int
netsnmp_callback_close(netsnmp_transport *t)
{
    int             rc;
    netsnmp_callback_info *mystuff = (netsnmp_callback_info *) t->data;
    DEBUGMSGTL(("transport_callback", "hook_close enter\n"));

#ifdef WIN32
    rc  = closesocket(mystuff->pipefds[0]);
    rc |= closesocket(mystuff->pipefds[1]);
#else
    rc  = close(mystuff->pipefds[0]);
    rc |= close(mystuff->pipefds[1]);
#endif

    rc |= netsnmp_transport_remove_from_list(&trlist, t);

    DEBUGMSGTL(("transport_callback", "hook_close exit\n"));
    return rc;
}



int
netsnmp_callback_accept(netsnmp_transport *t)
{
    DEBUGMSGTL(("transport_callback", "hook_accept enter\n"));
    DEBUGMSGTL(("transport_callback", "hook_accept exit\n"));
    return -1;
}



/*
 * Open a Callback-domain transport for SNMP.  Local is TRUE if addr
 * is the local address to bind to (i.e. this is a server-type
 * session); otherwise addr is the remote address to send things to
 * (and we make up a temporary name for the local end of the
 * connection).  
 */

netsnmp_transport *
netsnmp_callback_transport(int to)
{

    netsnmp_transport *t = NULL;
    netsnmp_callback_info *mydata;
    int             rc;

    /*
     * transport 
     */
    t = SNMP_MALLOC_TYPEDEF(netsnmp_transport);
    if (!t)
        return NULL;

    /*
     * our stuff 
     */
    mydata = SNMP_MALLOC_TYPEDEF(netsnmp_callback_info);
    if (!mydata) {
        SNMP_FREE(t);
        return NULL;
    }
    mydata->linkedto = to;
    mydata->callback_num = ++callback_count;
    mydata->data = NULL;
    t->data = mydata;

#ifdef WIN32
    rc = create_winpipe_transport(mydata->pipefds);
#else
    rc = pipe(mydata->pipefds);
#endif
    t->sock = mydata->pipefds[0];

    if (rc) {
        SNMP_FREE(mydata);
        SNMP_FREE(t);
        return NULL;
    }

    t->f_recv    = netsnmp_callback_recv;
    t->f_send    = netsnmp_callback_send;
    t->f_close   = netsnmp_callback_close;
    t->f_accept  = netsnmp_callback_accept;
    t->f_fmtaddr = netsnmp_callback_fmtaddr;

    netsnmp_transport_add_to_list(&trlist, t);

    if (to)
        DEBUGMSGTL(("transport_callback", "initialized %d linked to %d\n",
                    mydata->callback_num, to));
    else
        DEBUGMSGTL(("transport_callback",
                    "initialized master listening on %d\n",
                    mydata->callback_num));
    return t;
}

int
netsnmp_callback_hook_parse(netsnmp_session * sp,
                            netsnmp_pdu *pdu,
                            u_char * packetptr, size_t len)
{
    if (SNMP_MSG_RESPONSE == pdu->command ||
        SNMP_MSG_REPORT == pdu->command)
        pdu->flags |= UCD_MSG_FLAG_RESPONSE_PDU;
    else
        pdu->flags &= (~UCD_MSG_FLAG_RESPONSE_PDU);

    return SNMP_ERR_NOERROR;
}

int
netsnmp_callback_hook_build(netsnmp_session * sp,
                            netsnmp_pdu *pdu, u_char * ptk, size_t * len)
{
    /*
     * very gross hack, as this is passed later to the transport_send
     * function 
     */
    callback_hack  *ch = SNMP_MALLOC_TYPEDEF(callback_hack);
    if (ch == NULL)
        return -1;
    DEBUGMSGTL(("transport_callback", "hook_build enter\n"));
    ch->pdu = pdu;
    ch->orig_transport_data = pdu->transport_data;
    pdu->transport_data = ch;
    switch (pdu->command) {
    case SNMP_MSG_GETBULK:
        if (pdu->max_repetitions < 0) {
            sp->s_snmp_errno = SNMPERR_BAD_REPETITIONS;
            return -1;
        }
        if (pdu->non_repeaters < 0) {
            sp->s_snmp_errno = SNMPERR_BAD_REPEATERS;
            return -1;
        }
        break;
    case SNMP_MSG_RESPONSE:
    case SNMP_MSG_TRAP:
    case SNMP_MSG_TRAP2:
        pdu->flags &= (~UCD_MSG_FLAG_EXPECT_RESPONSE);
        /*
         * Fallthrough
         */
    default:
        if (pdu->errstat == SNMP_DEFAULT_ERRSTAT)
            pdu->errstat = 0;
        if (pdu->errindex == SNMP_DEFAULT_ERRINDEX)
            pdu->errindex = 0;
        break;
    }

    /*
     * Copy missing values from session defaults
     */
    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
        if (pdu->community_len == 0) {
            if (sp->community_len == 0) {
                sp->s_snmp_errno = SNMPERR_BAD_COMMUNITY;
                return -1;
            }
            pdu->community = (u_char *) malloc(sp->community_len);
            if (pdu->community == NULL) {
                sp->s_snmp_errno = SNMPERR_MALLOC;
                return -1;
            }
            memmove(pdu->community,
                    sp->community, sp->community_len);
            pdu->community_len = sp->community_len;
        }
        break;
#endif
    case SNMP_VERSION_3:
        if (pdu->securityNameLen == 0) {
	  pdu->securityName = (char *)malloc(sp->securityNameLen);
            if (pdu->securityName == NULL) {
                sp->s_snmp_errno = SNMPERR_MALLOC;
                return -1;
            }
            memmove(pdu->securityName,
                     sp->securityName, sp->securityNameLen);
            pdu->securityNameLen = sp->securityNameLen;
        }
        if (pdu->securityModel == -1)
            pdu->securityModel = sp->securityModel;
        if (pdu->securityLevel == 0)
            pdu->securityLevel = sp->securityLevel;
        /* WHAT ELSE ?? */
    }
    ptk[0] = 0;
    *len = 1;
    DEBUGMSGTL(("transport_callback", "hook_build exit\n"));
    return 1;
}

int
netsnmp_callback_check_packet(u_char * pkt, size_t len)
{
    return 1;
}

netsnmp_pdu    *
netsnmp_callback_create_pdu(netsnmp_transport *transport,
                            void *opaque, size_t olength)
{
    netsnmp_pdu    *pdu;
    netsnmp_callback_pass *cp =
        callback_pop_queue(((netsnmp_callback_info *) transport->data)->
                           callback_num);
    if (!cp)
        return NULL;
    pdu = cp->pdu;
    pdu->transport_data = opaque;
    pdu->transport_data_length = olength;
    if (opaque)                 /* if created, we're the server */
        *((int *) opaque) = cp->return_transport_num;
    SNMP_FREE(cp);
    return pdu;
}

netsnmp_session *
netsnmp_callback_open(int attach_to,
                      int (*return_func) (int op,
                                          netsnmp_session * session,
                                          int reqid, netsnmp_pdu *pdu,
                                          void *magic),
                      int (*fpre_parse) (netsnmp_session *,
                                         struct netsnmp_transport_s *,
                                         void *, int),
                      int (*fpost_parse) (netsnmp_session *, netsnmp_pdu *,
                                          int))
{
    netsnmp_session callback_sess, *callback_ss;
    netsnmp_transport *callback_tr;

    callback_tr = netsnmp_callback_transport(attach_to);
    snmp_sess_init(&callback_sess);
    callback_sess.callback = return_func;
    if (attach_to) {
        /*
         * client 
         */
        /*
         * trysess.community = (u_char *) callback_ss; 
         */
    } else {
        callback_sess.isAuthoritative = SNMP_SESS_AUTHORITATIVE;
    }
    callback_sess.remote_port = 0;
    callback_sess.retries = 0;
    callback_sess.timeout = 30000000;
    callback_sess.version = SNMP_DEFAULT_VERSION;       /* (mostly) bogus */
    callback_ss = snmp_add_full(&callback_sess, callback_tr,
                                fpre_parse,
                                netsnmp_callback_hook_parse, fpost_parse,
                                netsnmp_callback_hook_build,
                                NULL,
                                netsnmp_callback_check_packet,
                                netsnmp_callback_create_pdu);
    if (callback_ss)
        callback_ss->local_port =
            ((netsnmp_callback_info *) callback_tr->data)->callback_num;
    return callback_ss;
}



void
netsnmp_clear_callback_list(void)
{

    netsnmp_transport_list *list = trlist, *next = NULL;
    netsnmp_transport *tr = NULL;

    DEBUGMSGTL(("callback_clear", "called netsnmp_callback_clear_list()\n"));
    while (list != NULL) {
	next = list->next;
	tr = list->transport;

	if (tr != NULL) {
	    tr->f_close(tr);
  	    netsnmp_transport_remove_from_list(&trlist, tr);
	    netsnmp_transport_free(tr);
	}
	list = next;
    }
    trlist = NULL;

}

#endif /* NETSNMP_TRANSPORT_CALLBACK_DOMAIN */
