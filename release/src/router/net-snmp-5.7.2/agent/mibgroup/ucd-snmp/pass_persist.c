#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_IO_H
#include <io.h>
#endif
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
#include <ctype.h>
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef WIN32
#include <limits.h>
#endif

#include <signal.h>
#include <errno.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "struct.h"
#include "pass_persist.h"
#include "pass_common.h"
#include "extensible.h"
#include "util_funcs.h"

netsnmp_feature_require(get_exten_instance)
netsnmp_feature_require(parse_miboid)

struct extensible *persistpassthrus = NULL;
int             numpersistpassthrus = 0;
struct persist_pipe_type {
    FILE           *fIn, *fOut;
    int             fdIn, fdOut;
    netsnmp_pid_t   pid;
}              *persist_pipes = (struct persist_pipe_type *) NULL;
static unsigned pipe_check_alarm_id;
static int      init_persist_pipes(void);
static void     close_persist_pipe(int iindex);
static int      open_persist_pipe(int iindex, char *command);
static void     check_persist_pipes(unsigned clientreg, void *clientarg);
static void     destruct_persist_pipes(void);
static int      write_persist_pipe(int iindex, const char *data);

/*
 * the relocatable extensible commands variables 
 */
struct variable2 extensible_persist_passthru_variables[] = {
    /*
     * bogus entry.  Only some of it is actually used. 
     */
    {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_extensible_pass_persist, 0, {MIBINDEX}},
};

void
init_pass_persist(void)
{
    snmpd_register_config_handler("pass_persist",
                                  pass_persist_parse_config,
                                  pass_persist_free_config,
                                  "miboid program");
    pipe_check_alarm_id = snmp_alarm_register(10, SA_REPEAT, check_persist_pipes, NULL);
}

void
shutdown_pass_persist(void)
{
    if (pipe_check_alarm_id) {
        snmp_alarm_unregister(pipe_check_alarm_id);
        pipe_check_alarm_id = 0;
    }

    /* Close any open pipes. */
    destruct_persist_pipes();
}

void
pass_persist_parse_config(const char *token, char *cptr)
{
    struct extensible **ppass = &persistpassthrus, **etmp, *ptmp;
    char           *tcptr, *endopt;
    int             i;
    long int        priority;

    /*
     * options
     */
    priority = DEFAULT_MIB_PRIORITY;
    while (*cptr == '-') {
      cptr++;
      switch (*cptr) {
      case 'p':
	/* change priority level */
	cptr++;
	cptr = skip_white(cptr);
	if (! isdigit((unsigned char)(*cptr))) {
	  config_perror("priority must be an integer");
	  return;
	}
	priority = strtol((const char*) cptr, &endopt, 0);
	if ((priority == LONG_MIN) || (priority == LONG_MAX)) {
	  config_perror("priority under/overflow");
	  return;
	}
	cptr = endopt;
	cptr = skip_white(cptr);
	break;
      default:
	config_perror("unknown option for pass directive");
	return;
      }
    }

    /*
     * MIB
     */
    if (*cptr == '.')
        cptr++;
    if (!isdigit((unsigned char)(*cptr))) {
        config_perror("second token is not a OID");
        return;
    }
    numpersistpassthrus++;

    while (*ppass != NULL)
        ppass = &((*ppass)->next);
    (*ppass) = (struct extensible *) malloc(sizeof(struct extensible));
    if (*ppass == NULL)
        return;
    (*ppass)->type = PASSTHRU_PERSIST;
    (*ppass)->mibpriority = priority;

    (*ppass)->miblen = parse_miboid(cptr, (*ppass)->miboid);
    while (isdigit((unsigned char)(*cptr)) || *cptr == '.')
        cptr++;
    /*
     * path
     */
    cptr = skip_white(cptr);
    if (cptr == NULL) {
        config_perror("No command specified on pass_persist line");
        (*ppass)->command[0] = 0;
    } else {
        for (tcptr = cptr; *tcptr != 0 && *tcptr != '#' && *tcptr != ';';
             tcptr++);
        sprintf((*ppass)->command, "%.*s", (int) (tcptr - cptr), cptr);
    }
    strlcpy((*ppass)->name, (*ppass)->command, sizeof((*ppass)->name));
    (*ppass)->next = NULL;

    register_mib_priority("pass_persist",
                 (struct variable *) extensible_persist_passthru_variables,
                 sizeof(struct variable2), 1, (*ppass)->miboid,
                 (*ppass)->miblen, (*ppass)->mibpriority);

    /*
     * argggg -- pasthrus must be sorted 
     */
    if (numpersistpassthrus > 1) {
        etmp = (struct extensible **)
            malloc(((sizeof(struct extensible *)) * numpersistpassthrus));
        if (etmp == NULL)
            return;
        for (i = 0, ptmp = (struct extensible *) persistpassthrus;
             i < numpersistpassthrus && ptmp != NULL; i++, ptmp = ptmp->next)
            etmp[i] = ptmp;
        qsort(etmp, numpersistpassthrus, sizeof(struct extensible *),
              pass_persist_compare);
        persistpassthrus = (struct extensible *) etmp[0];
        ptmp = (struct extensible *) etmp[0];

        for (i = 0; i < numpersistpassthrus - 1; i++) {
            ptmp->next = etmp[i + 1];
            ptmp = ptmp->next;
        }
        ptmp->next = NULL;
        free(etmp);
    }
}

void
pass_persist_free_config(void)
{
    struct extensible *etmp, *etmp2;

    for (etmp = persistpassthrus; etmp != NULL;) {
        etmp2 = etmp;
        etmp = etmp->next;
        unregister_mib_priority(etmp2->miboid, etmp2->miblen, etmp2->mibpriority);
        free(etmp2);
    }
    persistpassthrus = NULL;
    numpersistpassthrus = 0;
}

u_char         *
var_extensible_pass_persist(struct variable *vp,
                            oid * name,
                            size_t * length,
                            int exact,
                            size_t * var_len, WriteMethod ** write_method)
{
    oid             newname[MAX_OID_LEN];
    int             i, rtest, newlen;
    char            buf[SNMP_MAXBUF];
    static char     buf2[SNMP_MAXBUF];
    struct extensible *persistpassthru;
    FILE           *file;

    /*
     * Make sure that our basic pipe structure is malloced 
     */
    init_persist_pipes();

    for (i = 1; i <= numpersistpassthrus; i++) {
        persistpassthru = get_exten_instance(persistpassthrus, i);
        rtest = snmp_oidtree_compare(name, *length,
                                     persistpassthru->miboid,
                                     persistpassthru->miblen);
        if ((exact && rtest == 0) || (!exact && rtest <= 0)) {
            /*
             * setup args 
             */
            if (persistpassthru->miblen >= *length || rtest < 0)
                sprint_mib_oid(buf, persistpassthru->miboid,
                               persistpassthru->miblen);
            else
                sprint_mib_oid(buf, name, *length);

            /*
             * Open our pipe if necessary 
             */
            if (!open_persist_pipe(i, persistpassthru->name)) {
                return (NULL);
            }

            if (exact)
                snprintf(persistpassthru->command,
                  sizeof(persistpassthru->command), "get\n%s\n", buf);
            else
                snprintf(persistpassthru->command,
                  sizeof(persistpassthru->command), "getnext\n%s\n", buf);
            persistpassthru->command[ sizeof(persistpassthru->command)-1 ] = 0;

            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "persistpass-sending:\n%s",
                        persistpassthru->command));
            if (!write_persist_pipe(i, persistpassthru->command)) {
                *var_len = 0;
                /*
                 * close_persist_pipes is called in write_persist_pipe 
                 */
                return (NULL);
            }

            /*
             * valid call.  Exec and get output 
             */
		
            if ((file = persist_pipes[i].fIn)) {
                if (fgets(buf, sizeof(buf), file) == NULL) {
                    *var_len = 0;
                    close_persist_pipe(i);
                    return (NULL);
                }
                /*
                 * persistent scripts return "NONE\n" on invalid items 
                 */
                if (!strncmp(buf, "NONE", 4)) {
                    if (exact) {
                        *var_len = 0;
                        return (NULL);
                    }
                    continue;
                }
                newlen = parse_miboid(buf, newname);

                /*
                 * its good, so copy onto name/length 
                 */
                memcpy((char *) name, (char *) newname,
                       (int) newlen * sizeof(oid));
                *length = newlen;

                /*
                 * set up return pointer for setable stuff 
                 */
                *write_method = setPassPersist;

                if (newlen == 0 || fgets(buf, sizeof(buf), file) == NULL
                    || fgets(buf2, sizeof(buf2), file) == NULL) {
                    *var_len = 0;
                    close_persist_pipe(i);
                    return (NULL);
                }
                return netsnmp_internal_pass_parse(buf, buf2, var_len, vp);
            }
            *var_len = 0;
            return (NULL);
        }
    }
    if (var_len)
        *var_len = 0;
    *write_method = NULL;
    return (NULL);
}

int
setPassPersist(int action,
               u_char * var_val,
               u_char var_val_type,
               size_t var_val_len,
               u_char * statP, oid * name, size_t name_len)
{
    int             i, rtest;
    struct extensible *persistpassthru;

    char            buf[SNMP_MAXBUF], buf2[SNMP_MAXBUF];

    /*
     * Make sure that our basic pipe structure is malloced 
     */
    init_persist_pipes();

    for (i = 1; i <= numpersistpassthrus; i++) {
        persistpassthru = get_exten_instance(persistpassthrus, i);
        rtest = snmp_oidtree_compare(name, name_len,
                                     persistpassthru->miboid,
                                     persistpassthru->miblen);
        if (rtest <= 0) {
            if (action != ACTION)
                return SNMP_ERR_NOERROR;
            /*
             * setup args 
             */
            if (persistpassthru->miblen >= name_len || rtest < 0)
                sprint_mib_oid(buf, persistpassthru->miboid,
                               persistpassthru->miblen);
            else
                sprint_mib_oid(buf, name, name_len);
            snprintf(persistpassthru->command,
                     sizeof(persistpassthru->command), "set\n%s\n", buf);
            persistpassthru->command[ sizeof(persistpassthru->command)-1 ] = 0;
            netsnmp_internal_pass_set_format(buf, var_val, var_val_type, var_val_len);
            strlcat(persistpassthru->command, buf,
                    sizeof(persistpassthru->command));
            persistpassthru->command[ sizeof(persistpassthru->command)-2 ] = '\n';
            persistpassthru->command[ sizeof(persistpassthru->command)-1 ] = 0;

            if (!open_persist_pipe(i, persistpassthru->name)) {
                return SNMP_ERR_NOTWRITABLE;
            }

            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "persistpass-writing:  %s\n",
                        persistpassthru->command));
            if (!write_persist_pipe(i, persistpassthru->command)) {
                close_persist_pipe(i);
                return SNMP_ERR_NOTWRITABLE;
            }

            if (fgets(buf, sizeof(buf), persist_pipes[i].fIn) == NULL) {
                close_persist_pipe(i);
                return SNMP_ERR_NOTWRITABLE;
            }

            return netsnmp_internal_pass_str_to_errno(buf);
        }
    }
    if (snmp_get_do_debugging()) {
        sprint_mib_oid(buf2, name, name_len);
        DEBUGMSGTL(("ucd-snmp/pass_persist", "persistpass-notfound:  %s\n",
                    buf2));
    }
    return SNMP_ERR_NOSUCHNAME;
}

int
pass_persist_compare(const void *a, const void *b)
{
    const struct extensible *const *ap, *const *bp;
    ap = (const struct extensible * const *) a;
    bp = (const struct extensible * const *) b;
    return snmp_oid_compare((*ap)->miboid, (*ap)->miblen, (*bp)->miboid,
                            (*bp)->miblen);
}

/*
 * Initialize our persistent pipes
 *   - Returns 1 on success, 0 on failure.
 *   - Initializes all FILE pointers to NULL to indicate "closed"
 */
static int
init_persist_pipes(void)
{
    int             i;

    /*
     * if we are already taken care of, just return 
     */
    if (persist_pipes) {
        return persist_pipes ? 1 : 0;
    }

    /*
     * Otherwise malloc and initialize 
     */
    persist_pipes = (struct persist_pipe_type *)
        malloc(sizeof(struct persist_pipe_type) *
               (numpersistpassthrus + 1));
    if (persist_pipes) {
        for (i = 0; i <= numpersistpassthrus; i++) {
            persist_pipes[i].fIn = persist_pipes[i].fOut = (FILE *) 0;
            persist_pipes[i].fdIn = persist_pipes[i].fdOut = -1;
            persist_pipes[i].pid = NETSNMP_NO_SUCH_PROCESS;
        }
    }
    return persist_pipes ? 1 : 0;
}

/**
 * Return true if and only if the process associated with the persistent
 * pipe has stopped.
 *
 * @param[in] idx Persistent pipe index.
 */
static int process_stopped(int idx)
{
    if (persist_pipes[idx].pid != NETSNMP_NO_SUCH_PROCESS) {
#if HAVE_SYS_WAIT_H
        return waitpid(persist_pipes[idx].pid, NULL, WNOHANG) > 0;
#endif
#if defined(WIN32) && !defined (mingw32) && !defined(HAVE_SIGNAL)
        return WaitForSingleObject(persist_pipes[idx].pid, 0) == WAIT_OBJECT_0;
#endif
    }
    return 0;
}

/**
 * Iterate over all persistent pipes and close those pipes of which the
 * associated process has stopped.
 */
static void check_persist_pipes(unsigned clientreg, void *clientarg)
{
    int             i;

    if (!persist_pipes)
        return;

    for (i = 0; i <= numpersistpassthrus; i++) {
        if (process_stopped(i)) {
            snmp_log(LOG_INFO, "pass_persist[%d]: child process stopped - closing pipe\n", i);
            close_persist_pipe(i);
        }
    }
}

/*
 * Destruct our persistent pipes
 *
 */
static void
destruct_persist_pipes(void)
{
    int             i;

    /*
     * Return if there are no pipes 
     */
    if (!persist_pipes) {
        return;
    }

    for (i = 0; i <= numpersistpassthrus; i++) {
        close_persist_pipe(i);
    }

    free(persist_pipes);
    persist_pipes = (struct persist_pipe_type *) 0;
}

/*
 * returns 0 on failure, 1 on success 
 */
static int
open_persist_pipe(int iindex, char *command)
{
    static int      recurse = 0;        /* used to allow one level of recursion */

    DEBUGMSGTL(("ucd-snmp/pass_persist", "open_persist_pipe(%d,'%s') recurse=%d\n",
                iindex, command, recurse));
    /*
     * Open if it's not already open 
     */
    if (persist_pipes[iindex].pid == NETSNMP_NO_SUCH_PROCESS) {
        int             fdIn, fdOut;
        netsnmp_pid_t   pid;

        /*
         * Did we fail? 
         */
        if ((0 == get_exec_pipes(command, &fdIn, &fdOut, &pid)) ||
            (pid == NETSNMP_NO_SUCH_PROCESS)) {
            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "open_persist_pipe: pid == -1\n"));
            recurse = 0;
            return 0;
        }

        /*
         * If not, fill out our structure 
         */
        persist_pipes[iindex].pid = pid;
        persist_pipes[iindex].fdIn = fdIn;
        persist_pipes[iindex].fdOut = fdOut;
        persist_pipes[iindex].fIn = fdopen(fdIn, "r");
        persist_pipes[iindex].fOut = fdopen(fdOut, "w");

        /*
         * Setup our -non-buffered-io- 
         */
        setbuf(persist_pipes[iindex].fOut, (char *) 0);
        DEBUGMSGTL(("ucd-snmp/pass_persist", "open_persist_pipe: opened the pipes\n"));
    }

    /*
     * Send test packet always so we can self-catch 
     */
    {
        char            buf[SNMP_MAXBUF];
        /*
         * Should catch SIGPIPE around this call! 
         */
        if (!write_persist_pipe(iindex, "PING\n")) {
            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "open_persist_pipe: Error writing PING\n"));
            close_persist_pipe(iindex);

            /*
             * Recurse one time if we get a SIGPIPE 
             */
            if (!recurse) {
                DEBUGMSGTL(("ucd-snmp/pass_persist", "open_persist_pipe: recursing to reopen\n"));
                recurse = 1;
                return open_persist_pipe(iindex, command);
            }
            recurse = 0;
            return 0;
        }
        if (fgets(buf, sizeof(buf), persist_pipes[iindex].fIn) == NULL) {
            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "open_persist_pipe: Error reading for PONG\n"));
            close_persist_pipe(iindex);
            recurse = 0;
            return 0;
        }
        if (strncmp(buf, "PONG", 4)) {
            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "open_persist_pipe: Got %s instead of PONG!\n", buf));
            close_persist_pipe(iindex);
            recurse = 0;
            return 0;
        }
    }

    recurse = 0;
    return 1;
}

static int
write_persist_pipe(int iindex, const char *data)
{
#if HAVE_SIGNAL
    struct sigaction sa, osa;
    int             wret = 0, werrno = 0;

    /*
     * Don't write to a non-existant process 
     */
    if (persist_pipes[iindex].pid == NETSNMP_NO_SUCH_PROCESS) {
        DEBUGMSGTL(("ucd-snmp/pass_persist",
                    "write_persist_pipe: not writing %s, process is non-existent",
                    data));
        return 0;
    }

    /*
     * Setup our signal action to ignore SIGPIPEs 
     */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, &osa)) {
        DEBUGMSGTL(("ucd-snmp/pass_persist",
                    "write_persist_pipe: sigaction failed: %d", errno));
    }

    /*
     * Do the write 
     */
    wret = write(persist_pipes[iindex].fdOut, data, strlen(data));
    werrno = errno;

    /*
     * Reset the signal handler 
     */
    sigaction(SIGPIPE, &osa, (struct sigaction *) 0);

    if (wret < 0) {
        if (werrno != EPIPE) {
            DEBUGMSGTL(("ucd-snmp/pass_persist",
                        "write_persist_pipe: write returned unknown error %d (%s)\n",
                        werrno, strerror(werrno)));
        }
        close_persist_pipe(iindex);
        return 0;
    }
#endif                          /* HAVE_SIGNAL */
#if defined(WIN32) && !defined (mingw32) && !defined (HAVE_SIGNAL)
/* We have no signal here (maybe we can make a Thread?) so write may block, 
 * but probably never will.
 */
    int wret = 0, werrno = 0;

    /*
     * Do the write 
     */
    wret = write(persist_pipes[iindex].fdOut, data,strlen(data));
    werrno = errno;
    
    if (wret < 0) {
      if (werrno != EINTR) {
        DEBUGMSGTL(("ucd-snmp/pass_persist", "write_persist_pipe: write returned unknown error %d\n",errno));
      }
      close_persist_pipe(iindex);
      return 0;
    }
#endif                          /* WIN32 */
    return 1;
}

static void
close_persist_pipe(int iindex)
{
/*	Alexander Prömel, alexander@proemel.de 08/24/2006
	The hard coded pathnames, are temporary.
	I'll fix it soon.
	If you changed them here, you have to do it in ../util_funcs.c too.
*/
#ifdef __uClinux__
	char fifo_in_path[256];
	char fifo_out_path[256];

	snprintf(fifo_in_path, 256, "/flash/cp_%d", persist_pipes[iindex].pid);
	snprintf(fifo_out_path, 256, "/flash/pc_%d", persist_pipes[iindex].pid);
#endif

    /*
     * Check and nix every item 
     */
    if (persist_pipes[iindex].fOut) {
        fclose(persist_pipes[iindex].fOut);
        persist_pipes[iindex].fOut = (FILE *) 0;
    }
    if (persist_pipes[iindex].fdOut != -1) {
#ifndef WIN32
        /*
         * The sequence open()/fdopen()/fclose()/close() triggers an access
         * violation with the MSVC runtime. Hence skip the close() call when
         * using the MSVC runtime.
         */
        close(persist_pipes[iindex].fdOut);
#endif
        persist_pipes[iindex].fdOut = -1;
    }
    if (persist_pipes[iindex].fIn) {
        fclose(persist_pipes[iindex].fIn);
        persist_pipes[iindex].fIn = (FILE *) 0;
    }
    if (persist_pipes[iindex].fdIn != -1) {
#ifndef WIN32
        /*
         * The sequence open()/fdopen()/fclose()/close() triggers an access
         * violation with the MSVC runtime. Hence skip the close() call when
         * using the MSVC runtime.
         */
        close(persist_pipes[iindex].fdIn);
#endif
        persist_pipes[iindex].fdIn = -1;
    }

#ifdef __uClinux__
	/*remove the pipes*/
	unlink(fifo_in_path);
	unlink(fifo_out_path);
#endif

    if (persist_pipes[iindex].pid != NETSNMP_NO_SUCH_PROCESS) {
#if HAVE_SYS_WAIT_H
        waitpid(persist_pipes[iindex].pid, NULL, 0);
#endif
#if defined(WIN32) && !defined (mingw32) && !defined (HAVE_SIGNAL)
        if (!CloseHandle(persist_pipes[iindex].pid)) {
            DEBUGMSGTL(("ucd-snmp/pass_persist","close_persist_pipe pid: close error\n"));
        }
#endif
        persist_pipes[iindex].pid = NETSNMP_NO_SUCH_PROCESS;
    }

}
