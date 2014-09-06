/*
 * util_funcs.c
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#if HAVE_RAISE
#define alarm raise
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/snmp_logging.h>

#ifdef USING_UCD_SNMP_ERRORMIB_MODULE
#include "ucd-snmp/errormib.h"
#else
#define setPerrorstatus(x) snmp_log_perror(x)
#endif

char **argvrestartp, *argvrestartname, *argvrestart;

RETSIGTYPE
restart_doit(int a)
{
    char * name = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                        NETSNMP_DS_LIB_APPTYPE);
    snmp_shutdown(name);

    /*  This signal handler may run with SIGALARM blocked.
     *  Since the signal mask is preserved accross execv(), we must
     *  make sure that SIGALARM is unblocked prior of execv'ing.
     *  Otherwise SIGALARM will be ignored in the next incarnation
     *  of snmpd, because the signal is blocked. And thus, the
     *  restart doesn't work anymore.
     *
     *  A quote from the sigprocmask() man page:
     *  The use of sigprocmask() is unspecified in a multithreaded process; see
     *  pthread_sigmask(3).
     */
#if HAVE_SIGPROCMASK
    {
        sigset_t empty_set;

        sigemptyset(&empty_set);
        sigprocmask(SIG_SETMASK, &empty_set, NULL);
    }
#elif HAVE_SIGBLOCK
    sigsetmask(0);
#endif

    /*
     * do the exec
     */
#if HAVE_EXECV
    execv(argvrestartname, argvrestartp);
    setPerrorstatus(argvrestartname);
#endif
}

int
restart_hook(int action,
             u_char * var_val,
             u_char var_val_type,
             size_t var_val_len,
             u_char * statP, oid * name, size_t name_len)
{

    long            tmp = 0;

    if (var_val_type != ASN_INTEGER) {
        snmp_log(LOG_NOTICE, "Wrong type != int\n");
        return SNMP_ERR_WRONGTYPE;
    }
    tmp = *((long *) var_val);
    if (tmp == 1 && action == COMMIT) {
#ifdef SIGALRM
        signal(SIGALRM, restart_doit);
#endif
        alarm(NETSNMP_RESTARTSLEEP);
    }
    return SNMP_ERR_NOERROR;
}
