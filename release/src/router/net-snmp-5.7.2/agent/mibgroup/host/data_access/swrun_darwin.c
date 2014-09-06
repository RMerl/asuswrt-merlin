/*
 * swrun_darwin.c:
 *     hrSWRunTable data access:
 *     Darwin
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swrun.h>

#include <stdlib.h>
#include <unistd.h>

#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>	/* for sysctl() and struct kinfo_proc */

#define __APPLE_API_EVOLVING 1
#include <sys/acl.h> /* or else CoreFoundation.h barfs */
#undef __APPLE_API_EVOLVING 

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOCFBundle.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

/** sigh... can't find Processes.h */
#ifndef kProcessDictionaryIncludeAllInformationMask 
#define kProcessDictionaryIncludeAllInformationMask (long)0xFFFFFFFF
#endif
#ifndef procNotFound
#define procNotFound -600
#endif

/* ---------------------------------------------------------------------
 */
static int _kern_argmax;
static int _set_command_name(netsnmp_swrun_entry *entry);

/** avoid kernel bug in 10.2. 8192 oughta be enough anyways, right? */
#define MAX_KERN_ARGMAX 8192

/* ---------------------------------------------------------------------
 */
void
netsnmp_arch_swrun_init(void)
{
    int    mib[2] = { CTL_KERN, KERN_ARGMAX };
    size_t size, mib_size = sizeof(mib)/sizeof(mib[0]);
    
    DEBUGMSGTL(("swrun:load:arch","init\n"));

    size = sizeof(_kern_argmax);
    if (sysctl(mib, mib_size, &_kern_argmax, &size, NULL, 0) == -1) {
        snmp_log(LOG_ERR, "Error in ARGMAX sysctl(): %s", strerror(errno));
        _kern_argmax = MAX_KERN_ARGMAX;
    }
    else if (_kern_argmax > MAX_KERN_ARGMAX) {
        DEBUGMSGTL(("swrun:load:arch",
                    "artificially limiting ARGMAX to %d (from %d)\n",
                    MAX_KERN_ARGMAX, _kern_argmax));
        _kern_argmax = MAX_KERN_ARGMAX;
    }


}

/* ---------------------------------------------------------------------
 */
#define SWRUNINDENT "           "
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    int	                 mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t               buf_size, mib_size = sizeof(mib)/sizeof(mib[0]);
    struct kinfo_proc   *processes = NULL;
    struct proc_taskallinfo taskinfo;
    netsnmp_swrun_entry *entry;
    int                  rc, num_entries, i;

    DEBUGMSGTL(("swrun:load:arch"," load\n"));

    /*
     * get size to allocate. This introduces a bit of a race condition,
     * as the size could change between this call and the next...
     */
    rc = sysctl(mib, mib_size, NULL, &buf_size, NULL, 0);
    if (rc < 0) {
        snmp_log(LOG_ERR, "KERN_PROC_ALL size sysctl failed: %d\n", rc);
        return -1;
    }

    processes = (struct kinfo_proc*) malloc(buf_size);
    if (NULL == processes) {
        snmp_log(LOG_ERR, "malloc failed\n");
        return -1;
    }

    rc = sysctl(mib, mib_size, processes, &buf_size, NULL, 0);
    if (rc < 0) {
        snmp_log(LOG_ERR, "KERN_PROC_ALL sysctl failed: %d\n", rc);
        free(processes);
        return -1;
    }
    
    num_entries = buf_size / sizeof(struct kinfo_proc);
    
    for (i = 0; i < num_entries; i++) {
        /*
         * skip empty names.
         * p_stat = (SIDL|SRUN|SSLEEP|SSTOP|SZOMB)
         */
        if ((NULL == processes[i].kp_proc.p_comm) ||
            (0 == processes[i].kp_proc.p_pid)) {
            DEBUGMSGTL(("swrun:load:arch",
                        " skipping p_comm '%s', pid %5d, p_pstat %d\n",
                        processes[i].kp_proc.p_comm ? 
                        processes[i].kp_proc.p_comm : "NULL",
                        processes[i].kp_proc.p_pid,
                        processes[i].kp_proc.p_stat));
            continue;
        }

        DEBUGMSGTL(("swrun:load:arch"," %s pid %5d\n",
                    processes[i].kp_proc.p_comm,
                    processes[i].kp_proc.p_pid));

        entry = netsnmp_swrun_entry_create(processes[i].kp_proc.p_pid);
        if (NULL == entry)
            continue; /* error already logged by function */
        rc = CONTAINER_INSERT(container, entry);

        /*
         * p_comm is a partial name, but it is all we have at this point.
         */
        entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
                                          sizeof(entry->hrSWRunName)-1,
                                          "%s", processes[i].kp_proc.p_comm);

        /** sysctl for name, path, params */
        rc = _set_command_name(entry);

        /*
         * map p_stat to RunStatus. Odd that there is no 'running' status.
         */
        switch(processes[i].kp_proc.p_stat) {
            case SRUN:
                entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNABLE;
                break;
            case SSLEEP:
            case SSTOP:
                entry->hrSWRunStatus = HRSWRUNSTATUS_NOTRUNNABLE;
                break;
            case SIDL:
            case SZOMB:
            default:
                entry->hrSWRunStatus = HRSWRUNSTATUS_INVALID;
                break;
        } 

        /*
         * check for system processes
         */
        if (P_SYSTEM & processes[i].kp_proc.p_flag) {
            entry->hrSWRunType = 2; /* operatingSystem */
            DEBUGMSGTL(("swrun:load:arch", SWRUNINDENT "SYSTEM\n"));
        }

        /*
         * get mem size, run time
         */
        rc = proc_pidinfo( processes[i].kp_proc.p_pid, PROC_PIDTASKALLINFO, 0,
                           &taskinfo, sizeof(taskinfo));
        if (sizeof(taskinfo) != rc) {
            DEBUGMSGTL(("swrun:load:arch", " proc_pidinfo returned %d\n", rc));
        }
        else {
            uint64_t task_mem = taskinfo.ptinfo.pti_resident_size / 1024;
            union {
                u_quad_t     uq; /* u_int64_t */
                UnsignedWide uw; /* struct u_int32_t hi/lo */
            } at, ns;
            at.uq = taskinfo.ptinfo.pti_total_user +
                    taskinfo.ptinfo.pti_total_system;
            ns.uw = AbsoluteToNanoseconds( at.uw );
            ns.uq = ns.uq / 10000000LL; /* nano to deci */
            if (task_mem > INT32_MAX) {
                DEBUGMSGTL(("swrun:load:arch", SWRUNINDENT "mem overflow\n"));
                task_mem = INT32_MAX;
            }
            if (ns.uq > INT32_MAX) {
                DEBUGMSGTL(("swrun:load:arch", SWRUNINDENT "time overflow\n"));
                ns.uq = INT32_MAX;
            }
            entry->hrSWRunPerfMem = task_mem;
            entry->hrSWRunPerfCPU = ns.uq;
        }
    }
    free(processes);

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}

/* ---------------------------------------------------------------------
 * The following code was snagged from Darwin code, and the original
 * file had the following licences:
 */

/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifdef JAGUAR /* xxx configure test? */
static int
_set_command_name_jaguar(netsnmp_swrun_entry *entry)
{
    int	        mib[3] = {CTL_KERN, KERN_PROCARGS, 0};
    size_t      procargssize, mib_size = sizeof(mib)/sizeof(mib[0]);
    char       *arg_end, *exec_path;
    int        *ip;
    int         len;
    char       *command_beg, *command, *command_end;
    char        arg_buf[MAX_KERN_ARGMAX]; /* max to avoid kernel bug */

    DEBUGMSGTL(("swrun:load:arch:_cn"," pid %d\n", entry->hrSWRunIndex));

    mib[2] = entry->hrSWRunIndex;

    memset(arg_buf, 0x0, sizeof(arg_buf));
    procargssize = _kern_argmax;
    if (sysctl(mib, mib_size, arg_buf, &procargssize, NULL, 0) == -1) {
        snmp_log(LOG_ERR, "Error in PROCARGS sysctl() for %s: %s\n",
                 entry->hrSWRunName, strerror(errno));
        entry->hrSWRunPath_len = 0;
        return -1;
    }

    /* Set ip just above the end of arg_buf. */
    arg_end = &arg_buf[procargssize];
    ip = (int *)arg_end;
    
    /*
     * Skip the last 2 words, since the last is a 0 word, and
     * the second to last may be as well, if there are no
     * arguments.
     */
    ip -= 3;
    
    /* Iterate down the arguments until a 0 word is found. */
    for (; *ip != 0; ip--) {
        if (ip == (int *)arg_buf) {
            DEBUGMSGTL(("swrun:load:arch:_cn"," unexpected toparg\n"));
            return -1;
        }
    }
    
    /* The saved exec_path is just above the 0 word. */
    ip++;
    exec_path = (char *)ip;
    DEBUGMSGTL(("swrun:load:arch:_cn"," exec_path %s\n", exec_path));
    len = strlen(exec_path);
    strlcpy(entry->hrSWRunPath, exec_path, sizeof(entry->hrSWRunPath));
    if (len > sizeof(entry->hrSWRunPath)-1) {
        DEBUGMSGTL(("swrun:load:arch:_cn"," truncating long run path\n"));
        entry->hrSWRunPath[sizeof(entry->hrSWRunPath)-2] = '$';
        entry->hrSWRunPath_len = sizeof(entry->hrSWRunPath)-1;
        DEBUGMSGTL(("swrun:load:arch:_cn"," exec_path %s\n",
                    entry->hrSWRunPath));
    }
    else
        entry->hrSWRunPath_len = len;
    
    /*
     * Get the beginning of the first argument.  It is word-aligned,
     * so skip padding '\0' bytes.
     */
    command_beg = exec_path + strlen(exec_path);
    DEBUGMSGTL(("swrun:load:arch:_cn"," command_beg '%s'\n", command_beg));
    for (; *command_beg == '\0'; command_beg++) {
        if (command_beg >= arg_end)
            return -1;
    }
    DEBUGMSGTL(("swrun:load:arch:_cn"," command_beg '%s'\n", command_beg));
    
    /* Get the basename of command. */
    command = command_end = command_beg + strlen(command_beg) + 1;
    for (command--; command >= command_beg; command--) {
        if (*command == '/')
            break;
    }
    command++;
    DEBUGMSGTL(("swrun:load:arch:_cn"," command '%s'\n", command));
    
    /* Allocate space for the command and copy. */
    DEBUGMSGTL(("swrun:load:arch:_cn",
                SWRUNINDENT "kernel name %s\n", command));
    if (strncmp(command, entry->hrSWRunName, sizeof(entry->hrSWRunName)-1)) {
        strlcpy(entry->hrSWRunName, command, sizeof(entry->hrSWRunName));
        entry->hrSWRunName_len = strlen(entry->hrSWRunName);
        DEBUGMSGTL(("swrun:load:arch:_cn", "**"
                    SWRUNINDENT "updated name to %s\n", entry->hrSWRunName));
        return 0;
    }

    /** no error, no change */
    return 1;
}
#else
static int
_set_command_name(netsnmp_swrun_entry *entry)
{
    int	        mib[3] = {CTL_KERN, 0, 0};
    size_t      procargssize, mib_size = sizeof(mib)/sizeof(mib[0]);
    char       *cp;
    int         len, nargs;
    char       *command_beg, *command, *command_end, *exec_path, *argN;
    char        arg_buf[MAX_KERN_ARGMAX]; /* max to avoid kernel bug */

    /*
     * arguments
     */
    mib[1] = KERN_PROCARGS2;
    mib[2] = entry->hrSWRunIndex;

    memset(arg_buf, 0x0, sizeof(arg_buf));
    procargssize = _kern_argmax;
    if (sysctl(mib, mib_size, arg_buf, &procargssize, NULL, 0) == -1) {
        snmp_log(LOG_ERR, "Error in PROCARGS2 sysctl() for %s: %s\n",
                 entry->hrSWRunName, strerror(errno));
        entry->hrSWRunPath_len = 0;
        entry->hrSWRunParameters_len = 0;
        return -1;
    }
    else {
        memcpy(&nargs,arg_buf, sizeof(nargs));
    }

    exec_path = arg_buf + sizeof(nargs);
    len = strlen(exec_path);
    strlcpy(entry->hrSWRunPath, exec_path, sizeof(entry->hrSWRunPath));
    if (len > sizeof(entry->hrSWRunPath)-1) {
        DEBUGMSGTL(("swrun:load:arch:_cn"," truncating long run path\n"));
        entry->hrSWRunPath[sizeof(entry->hrSWRunPath)-2] = '$';
        entry->hrSWRunPath_len = sizeof(entry->hrSWRunPath)-1;
    }
    else
        entry->hrSWRunPath_len = len;

    /** Skip the saved exec_path. */
#if 0
    cp = exec_path + len;
#else
    for (cp = exec_path; cp < &arg_buf[procargssize]; cp++) {
        if (*cp == '\0') 
            break; /* End of exec_path reached. */
    }
    if (cp != exec_path + len) {
        DEBUGMSGTL(("swrun:load:arch:_cn"," OFF BY %d\n",
                    (exec_path + len) - cp));
        netsnmp_assert( cp == exec_path + len );
    }
#endif
    if (cp == &arg_buf[procargssize]) {
        DEBUGMSGTL(("swrun:load:arch:_cn"," unexpected end of buffer\n"));
        return -1;
    }

    /** Skip trailing '\0' characters. */
    for (; cp < &arg_buf[procargssize]; cp++) {
        if (*cp != '\0')
            break; /* Beginning of first argument reached. */
    }
    if (cp == &arg_buf[procargssize]) {
        DEBUGMSGTL(("swrun:load:arch:_cn"," unexpected end of buffer\n"));
        return -1;
    }
    command_beg = cp;

    /*
     * Make sure that the command is '\0'-terminated.  This protects
     * against malicious programs; under normal operation this never
     * ends up being a problem..
     */
    for (; cp < &arg_buf[procargssize]; cp++) {
        if (*cp == '\0')
            break; /* End of first argument reached. */
    }
    if (cp == &arg_buf[procargssize]) {
        DEBUGMSGTL(("swrun:load:arch:_cn"," unexpected end of buffer\n"));
        return -1;
    }
    command_end = command = cp;
    --nargs;

    /*
     * save arguments
     */
    while( nargs && cp < &arg_buf[procargssize] ) {
        /** Skip trailing '\0' characters from prev arg. */
        for (; (cp < &arg_buf[procargssize]) && (*cp == 0); cp++) 
            ; /* noop */
        if (cp == &arg_buf[procargssize])
            continue; /* effectively a break */
    
        /** save argN start */
        argN = cp;
        --nargs;
        if (0 == nargs)
            continue; /* effectively a break */

        /** Skip to end of arg */
        for (; (cp < &arg_buf[procargssize]) && (*cp != 0); cp++) 
            ;  /* noop */
        if (cp == &arg_buf[procargssize])
            continue; /* effectively a break */

        /*
         * check for overrun into env
         */
        if ((*argN != '-') && strchr(argN,'='))  {
            DEBUGMSGTL(("swrun:load:arch:_cn", " *** OVERRUN INTO ENV %d\n",nargs));
            continue;
        }

        /*
         * save arg
         */
        if(entry->hrSWRunParameters_len < sizeof(entry->hrSWRunParameters)-1) {
            strlcat(&entry->hrSWRunParameters[entry->hrSWRunParameters_len],
                    argN, sizeof(entry->hrSWRunParameters));
            entry->hrSWRunParameters_len = strlen(entry->hrSWRunParameters);
            if ((entry->hrSWRunParameters_len+2 < sizeof(entry->hrSWRunParameters)-1) && (0 != nargs)) {
                /* add space between params */
                entry->hrSWRunParameters[entry->hrSWRunParameters_len++] = ' ';
                entry->hrSWRunParameters[entry->hrSWRunParameters_len] = 0;
            } else {
                DEBUGMSGTL(("swrun:load:arch:_cn"," truncating long arg list\n"));
                entry->hrSWRunParameters[entry->hrSWRunParameters_len++] = '$';
                entry->hrSWRunParameters[entry->hrSWRunParameters_len] = '0';
            }
        }
    }
    if (' ' == entry->hrSWRunParameters[entry->hrSWRunParameters_len])
        entry->hrSWRunParameters[entry->hrSWRunParameters_len--] = 0;

    
    /* Get the basename of command. */
    for (command--; command >= command_beg; command--) {
        if (*command == '/')
            break;
    }
    command++;
    
    /* Allocate space for the command and copy. */
    if (strncmp(command, entry->hrSWRunName, sizeof(entry->hrSWRunName)-1)) {
        strlcpy(entry->hrSWRunName, command, sizeof(entry->hrSWRunName));
        entry->hrSWRunName_len = strlen(entry->hrSWRunName);
        DEBUGMSGTL(("swrun:load:arch:_cn",
                    " **updated name to %s\n", entry->hrSWRunName));
    }

    return 0;
}
#endif
