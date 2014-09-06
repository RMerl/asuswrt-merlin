#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* optind, optarg and optopt */
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/ds_agent.h>

#include "../agent/mibgroup/agentx/agentx_config.h"
#include "../agent/mibgroup/agentx/client.h"
#include "../agent/mibgroup/agentx/protocol.h"

netsnmp_feature_require(snmp_split_pdu)
netsnmp_feature_require(snmp_reset_var_types)


#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

extern const oid sysuptime_oid[];
extern const size_t sysuptime_oid_len;
extern const oid snmptrap_oid[];
extern const size_t snmptrap_oid_len;

static void
usage(const char* progname)
{
    fprintf(stderr,
            "USAGE: %s [OPTIONS] TRAP-PARAMETERS\n"
            "\n"
            "  Version:  %s\n"
            "  Web:      http://www.net-snmp.org/\n"
            "  Email:    net-snmp-coders@lists.sourceforge.net\n"
            "\n"
            "OPTIONS:\n", progname, netsnmp_get_version());

    fprintf(stderr,
            "  -h\t\t\tdisplay this help message\n"
            "  -V\t\t\tdisplay package version number\n"
            "  -m MIB[:...]\t\tload given list of MIBs (ALL loads "
            "everything)\n"
            "  -M DIR[:...]\t\tlook in given list of directories for MIBs\n"
            "  -D[TOKEN[,...]]\tturn on debugging output for the specified "
            "TOKENs\n"
            "\t\t\t   (ALL gives extremely verbose debugging output)\n"
            "  -d\t\t\tdump all traffic\n");
#ifndef NETSNMP_DISABLE_MIB_LOADING
    fprintf(stderr,
            "  -P MIBOPTS\t\tToggle various defaults controlling mib "
            "parsing:\n");
    snmp_mib_toggle_options_usage("\t\t\t  ", stderr);
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    fprintf(stderr,
            "  -L LOGOPTS\t\tToggle various defaults controlling logging:\n");
    snmp_log_options_usage("\t\t\t  ", stderr);

    fprintf(stderr,
            "  -c context\n"
            "  -U uptime\n"
            "  -x ADDRESS\t\tuse ADDRESS as AgentX address\n"
            "\n"
            "TRAP-PARAMETERS:\n"
            "  trapoid [OID TYPE VALUE] ...\n");
}

struct tState_s;
typedef const struct tState_s* tState;
struct tState_s {
    void (*entry)(tState self); /**<< State entry action */
    void (*exit)(tState self); /**<< State exit action */
    /** Handler for AgentX-Response-PDU's */
    void (*response)(tState self, netsnmp_pdu *res);
    /** State to change to if an AgentX timeout occurs or the timer runs out */
    tState timeout;
    void (*disconnect)(tState self); /**<< Handler for disconnect indications */
    /** Handler for Close-PDU indications */
    void (*close)(tState self, netsnmp_pdu *res);
    const char* name; /**<< Name of the current state */
    int is_open; /**<< If the connection is open in this state */
};

static tState state; /**<< Current state of the state machine */
static tState next_state; /**<< Next state of the state machine */

static const char  *context = NULL; /**<< Context that delivers the trap */
static size_t       contextLen; /**<< Length of eventual context */
static int          result = 1; /**<< Program return value */
static netsnmp_pdu *pdu = NULL; /**<< The trap pdu that is to be sent */
/** The reference number of the next packet */
static long         packetid = 0;
/** The session id of the session to the master */
static long         session;
static void        *sessp = NULL; /**<< The current communication session */

#define STATE_CALL(method)                                              \
    if(!state->method) {                                                \
        snmp_log(LOG_ERR, "No " #method " method in %s, terminating\n", \
                 state->name);                                          \
        abort();                                                        \
    } else                                                              \
        state->method

static void
change_state(tState new_state)
{
    if (next_state && next_state != new_state)
        DEBUGMSGTL(("process", "Ignore transition to %s\n", next_state->name));
    next_state = new_state;
}

static int
handle_agentx_response(int operation, netsnmp_session *sp, UNUSED int reqid,
                       netsnmp_pdu *act, UNUSED void *magic)
{
    switch(operation) {
    case NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE:
        if(act->command == AGENTX_MSG_CLEANUPSET) {
            /* Do nothing - no response and no action as nothing get
             * allocated in any handler here
             */
        } else if(act->command != AGENTX_MSG_RESPONSE) {
            /* Copy the head to a response */
            netsnmp_pdu* res = snmp_split_pdu(act, 0, 0);
            res->command = AGENTX_MSG_RESPONSE;
            if (act->sessid != session || !state->is_open)
                res->errstat = AGENTX_ERR_NOT_OPEN;
            if(res->errstat == AGENTX_ERR_NOERROR)
                switch(act->command) {
                case AGENTX_MSG_GET:
                    res->variables = snmp_clone_varbind(act->variables);
                    snmp_reset_var_types(res->variables, SNMP_NOSUCHOBJECT);
                    break;
                case AGENTX_MSG_GETNEXT:
                case AGENTX_MSG_GETBULK:
                    res->variables = snmp_clone_varbind(act->variables);
                    snmp_reset_var_types(res->variables, SNMP_ENDOFMIBVIEW);
                    break;
                case AGENTX_MSG_TESTSET:
                    res->errstat = SNMP_ERR_NOTWRITABLE;
                    res->errindex = 1;
                    break;
                case AGENTX_MSG_COMMITSET:
                    res->errstat = SNMP_ERR_COMMITFAILED;
                    res->errindex = 1;
                    break;
                case AGENTX_MSG_UNDOSET:
                    /* Success - could undo not setting any value :-) */
                    break;
                case AGENTX_MSG_CLOSE:
                    /* Always let the master succeed! */
                    break;
                default:
                    /* Unknown command */
                    res->errstat = AGENTX_ERR_PARSE_FAILED;
                    break;
                }
            if(snmp_send(sp, res) == 0)
                snmp_free_pdu(res);
            switch(act->command) {
            case AGENTX_MSG_CLOSE:
                /* Take action once the answer is sent! */
                STATE_CALL(close)(state, act);
                break;
            default:
                /* Do nothing */
                break;
            }
        } else
            /* RESPONSE act->time, act->errstat, act->errindex, varlist */
            STATE_CALL(response)(state, act);
        break;
    case NETSNMP_CALLBACK_OP_TIMED_OUT:
        change_state(state->timeout);
        break;
    case NETSNMP_CALLBACK_OP_DISCONNECT:
        STATE_CALL(disconnect)(state);
        break;
    }
    return 0;
}

extern const struct tState_s Connecting;
extern const struct tState_s Opening;
extern const struct tState_s Notifying;
extern const struct tState_s Closing;
extern const struct tState_s Disconnecting;
extern const struct tState_s Exit;

static void
StateDisconnect(UNUSED tState self)
{
    snmp_log(LOG_ERR, "Unexpected disconnect in state %s\n", self->name);
    change_state(&Disconnecting);
}

static void
StateClose(UNUSED tState self, netsnmp_pdu *act)
{
    snmp_log(LOG_ERR, "Unexpected close with reason code %ld in state %s\n",
             act->errstat, self->name);
    change_state(&Disconnecting);
}

static void
ConnectingEntry(UNUSED tState self)
{
    netsnmp_session init;
    netsnmp_transport* t;
    void* sess;

    if(sessp) {
        snmp_sess_close(sessp);
        sessp = NULL;
    }

    snmp_sess_init(&init);
    init.version = AGENTX_VERSION_1;
    init.retries = 0; /* Retries are handled by the state machine */
    init.timeout = SNMP_DEFAULT_TIMEOUT;
    init.flags |= SNMP_FLAGS_STREAM_SOCKET;
    init.callback = handle_agentx_response;
    init.authenticator = NULL;

    if(!(t = netsnmp_transport_open_client(
             "agentx", netsnmp_ds_get_string(
                 NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_X_SOCKET)))) {
        snmp_perror("Failed to connect to AgentX server");
        change_state(&Exit);
    } else if(!(sess = snmp_sess_add_ex(
                    &init, t, NULL, agentx_parse, NULL, NULL,
                    agentx_realloc_build, agentx_check_packet, NULL))) {
        snmp_perror("Failed to create session");
        change_state(&Exit);
    } else {
        sessp = sess;
        change_state(&Opening);
    }
}

const struct tState_s Connecting = {
    ConnectingEntry,
    NULL,
    NULL,
    NULL,
    StateDisconnect,
    NULL,
    "Connnecting",
    0
};

static netsnmp_pdu*
pdu_create_opt_context(int command, const char* context, size_t len)
{
    netsnmp_pdu* res = snmp_pdu_create(command);
    if (res)
        if (context) {
            if (snmp_clone_mem((void**)&res->contextName, context, len)) {
                snmp_free_pdu(res);
                res = NULL;
            } else
                res->contextNameLen = len;
        }
    return res;
}

static void
OpeningEntry(UNUSED tState self)
{
    netsnmp_pdu* act =
        pdu_create_opt_context(AGENTX_MSG_OPEN, context, contextLen);
    if(act) {
        act->sessid = 0;
        act->transid = 0;
        act->reqid = ++packetid;
        act->time = 0;
        snmp_pdu_add_variable(act, NULL, 0, ASN_OCTET_STR, NULL, 0);
        if(snmp_sess_send(sessp, act) == 0)
            snmp_free_pdu(act);
    }
}

static void
OpeningRes(UNUSED tState self, netsnmp_pdu *act)
{
    if(act->errstat == AGENTX_ERR_NOERROR) {
        session = act->sessid;
        change_state(&Notifying);
    } else {
        snmp_perror("Failed to open session");
        change_state(&Exit);
    }
}

const struct tState_s Opening = {
    OpeningEntry,
    NULL,
    OpeningRes,
    &Disconnecting,
    StateDisconnect,
    StateClose,
    "Opening",
    0
};

static void
NotifyingEntry(UNUSED tState self)
{
    netsnmp_pdu* act = snmp_clone_pdu(pdu);
    if(act) {
        act->sessid = session;
        act->transid = 0;
        act->reqid = ++packetid;
        if(snmp_sess_send(sessp, act) == 0)
            snmp_free_pdu(act);
    }
}

static void
NotifyingRes(UNUSED tState self, netsnmp_pdu *act)
{
    if(act->errstat == AGENTX_ERR_NOERROR)
        result = 0;
    else
        snmp_perror("Failed to send notification");
    /** \todo: Retry handling --- ClosingReconnect??? */
    change_state(&Closing);
}

const struct tState_s Notifying = {
    NotifyingEntry,
    NULL,
    NotifyingRes,
    NULL,            /** \todo: Retry handling? */
    StateDisconnect, /** \todo: Retry handling? */
    StateClose,
    "Notifying",
    1
};

static void
ClosingEntry(UNUSED tState self)
{
    /* CLOSE pdu->errstat */
    netsnmp_pdu* act =
        pdu_create_opt_context(AGENTX_MSG_CLOSE, context, contextLen);
    if(act) {
        act->sessid = session;
        act->transid = 0;
        act->reqid = ++packetid;
        act->errstat = AGENTX_CLOSE_SHUTDOWN;
        if(snmp_sess_send(sessp, act) == 0)
            snmp_free_pdu(act);
    }
}

static void
ClosingRes(UNUSED tState self, netsnmp_pdu *act)
{
    if(act->errstat != AGENTX_ERR_NOERROR) {
        snmp_log(LOG_ERR, "AgentX error status of %ld\n", act->errstat);
    }
    change_state(&Disconnecting);
}

static void
ClosingDisconnect(UNUSED tState self)
{
    change_state(&Disconnecting);
}

static void
ClosingClose(UNUSED tState self, UNUSED netsnmp_pdu *act)
{
    change_state(&Disconnecting);
}

const struct tState_s Closing = {
    ClosingEntry,
    NULL,
    ClosingRes,
    &Disconnecting,
    ClosingDisconnect,
    ClosingClose,
    "Closing",
    1
};

static void
DisconnectingEntry(UNUSED tState self)
{
    snmp_sess_close(sessp);
    sessp = NULL;
    change_state(&Exit);
}

const struct tState_s Disconnecting = {
    DisconnectingEntry,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Disconnecting",
    0
};

const struct tState_s Exit = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Exit",
    0
};

int
main(int argc, char *argv[])
{
    int             arg;
    char           *prognam;
    char           *cp = NULL;

    const char*     sysUpTime = NULL;

    prognam = strrchr(argv[0], '/');
    if (prognam)
        ++prognam;
    else
        prognam = argv[0];

    putenv(strdup("POSIXLY_CORRECT=1"));

    while ((arg = getopt(argc, argv, ":Vhm:M:D:dP:L:U:c:x:")) != -1) {
        switch (arg) {
        case 'h':
            usage(prognam);
            exit(0);
        case 'm':
            setenv("MIBS", optarg, 1);
            break;
        case 'M':
            setenv("MIBDIRS", optarg, 1);
            break;
        case 'c':
            context = optarg;
            contextLen = strlen(context);
            break;
        case 'D':
            debug_register_tokens(optarg);
            snmp_set_do_debugging(1);
            break;
        case 'd':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                                   NETSNMP_DS_LIB_DUMP_PACKET, 1);
            break;
        case 'U':
            sysUpTime = optarg;
            break;
        case 'V':
            fprintf(stderr, "NET-SNMP version: %s\n", netsnmp_get_version());
            exit(0);
            break;
#ifndef DISABLE_MIB_LOADING
        case 'P':
            cp = snmp_mib_toggle_options(optarg);
            if (cp != NULL) {
                fprintf(stderr, "Unknown parser option to -P: %c.\n", *cp);
                usage(prognam);
                exit(1);
            }
            break;
#endif /* DISABLE_MIB_LOADING */
        case 'L':
            if (snmp_log_options(optarg, argc, argv) < 0) {
                exit(1);
            }
            break;
        case 'x':
            if (optarg != NULL) {
                netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
                                      NETSNMP_DS_AGENT_X_SOCKET, optarg);
            } else
                usage(argv[0]);
            break;

        case ':':
            fprintf(stderr, "Option -%c requires an operand\n", optopt);
            usage(prognam);
            exit(1);
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: -%c\n", optopt);
            usage(prognam);
            exit(1);
            break;
        }
    }

    arg = optind;

    /* initialize tcpip, if necessary */
    SOCK_STARTUP;

    init_snmp("snmpapp");
    agentx_config_init();

    /* NOTIFY varlist */
    pdu = pdu_create_opt_context(AGENTX_MSG_NOTIFY, context, contextLen);

    if (sysUpTime)
        snmp_add_var(pdu, sysuptime_oid, sysuptime_oid_len, 't', sysUpTime);

    if (arg == argc) {
        fprintf(stderr, "Missing trap-oid parameter\n");
        usage(prognam);
        SOCK_CLEANUP;
        exit(1);
    }

    if (snmp_add_var(pdu, snmptrap_oid, snmptrap_oid_len, 'o', argv[arg])) {
        snmp_perror(argv[arg]);
        SOCK_CLEANUP;
        exit(1);
    }
    ++arg;

    while (arg < argc) {
        oid    name[MAX_OID_LEN];
        size_t name_length = MAX_OID_LEN;
        arg += 3;
        if (arg > argc) {
            fprintf(stderr, "%s: Missing type/value for variable\n",
                    argv[arg - 3]);
            SOCK_CLEANUP;
            exit(1);
        }
        if (!snmp_parse_oid(argv[arg - 3], name, &name_length)) {
            snmp_perror(argv[arg - 3]);
            SOCK_CLEANUP;
            exit(1);
        }
        if (snmp_add_var(pdu, name, name_length, argv[arg - 2][0],
                         argv[arg - 1]) != 0) {
            snmp_perror(argv[arg - 3]);
            SOCK_CLEANUP;
            exit(1);
        }
    }

    packetid = 0;

    state = &Connecting;
    next_state = NULL;
    if(state->entry) state->entry(state);

    /* main loop here... */
    for(;;) {
        int block = 1;
        int numfds = 0;
        int count;
        fd_set fdset;
        struct timeval timeout;

        while(next_state) {
            if(state->exit) state->exit(state);
            DEBUGMSGTL(("process", "State transition: %s -> %s\n",
                        state->name, next_state->name));
            state = next_state;
            next_state = NULL;
            if(state->entry) state->entry(state);
        }

        if(state == &Exit)
            break;

        FD_ZERO(&fdset);
        snmp_sess_select_info(sessp, &numfds, &fdset, &timeout, &block);
        count = select(numfds, &fdset, NULL, NULL, !block ? &timeout : NULL);
        if (count > 0)
            snmp_sess_read(sessp, &fdset);
        else if (count == 0)
            snmp_sess_timeout(sessp);
        else if (errno != EINTR) {
            snmp_log(LOG_ERR, "select error [%s]\n", strerror(errno));
            change_state(&Exit);
        }
    }

    /* at shutdown time */
    snmp_free_pdu(pdu);
    pdu = NULL;

    snmp_shutdown("snmpapp");

    SOCK_CLEANUP;
    exit(result);
}
