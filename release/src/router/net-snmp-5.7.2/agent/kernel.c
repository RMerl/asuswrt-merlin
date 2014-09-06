/***********************************************************************
   Net-SNMP - Simple Network Management Protocol agent library.
 ***********************************************************************/
/** @file kernel.c
 *     Net-SNMP Kernel Data Access Library.
 *     Provides access to kernel virtual memory for systems that
 *     support it.
 * @author   See README file for a list of contributors
 */
/* Copyrights:
 *     Copyright holders are listed in README file.
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted. License terms are specified
 *     in COPYING file distributed with the Net-SNMP package.
 */
/***********************************************************************/

#include <net-snmp/net-snmp-config.h>

#ifdef NETSNMP_CAN_USE_NLIST

#include <sys/types.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <errno.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_KVM_H
#include <kvm.h>
#endif

#include <net-snmp/net-snmp-includes.h>

#include "kernel.h"
#include <net-snmp/agent/ds_agent.h>

#ifndef NULL
#define NULL 0
#endif


#if HAVE_KVM_H
kvm_t *kd = NULL;

/**
 * Initialize the support for accessing kernel virtual memory.
 *
 * @return TRUE upon success; FALSE upon failure.
 */
int
init_kmem(const char *file)
{
    int res = TRUE;

#if HAVE_KVM_OPENFILES
    char            err[4096];

    kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, err);
    if (!kd && !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                       NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
        snmp_log(LOG_CRIT, "init_kmem: kvm_openfiles failed: %s\n", err);
        res = FALSE;
    }
#else
    kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);
    if (!kd && !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
				       NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
        snmp_log(LOG_CRIT, "init_kmem: kvm_open failed: %s\n",
                 strerror(errno));
        res = FALSE;
    }
#endif                          /* HAVE_KVM_OPENFILES */
    return res;
}

/** Reads kernel memory.
 *  Seeks to the specified location in kmem, then
 *  does a read of given amount ob bytes into target buffer.
 *
 * @param off The location to seek.
 *
 * @param target The target buffer to read into.
 *
 * @param siz Number of bytes to read.
 *
 * @return gives 1 on success and 0 on failure.
 */
int
klookup(unsigned long off, void *target, size_t siz)
{
    int             result;
    if (kd == NULL)
        return 0;
    result = kvm_read(kd, off, target, siz);
    if (result != siz) {
#if HAVE_KVM_OPENFILES
        snmp_log(LOG_ERR, "kvm_read(*, %lx, %p, %zx) = %d: %s\n", off,
                 target, siz, result, kvm_geterr(kd));
#else
        snmp_log(LOG_ERR, "kvm_read(*, %lx, %p, %d) = %d: ", off, target,
                 siz, result);
        snmp_log_perror("klookup");
#endif
        return 0;
    }
    return 1;
}

/** Closes the kernel memory support.
 */
void
free_kmem(void)
{
    if (kd != NULL)
    {
      kvm_close(kd);
      kd = NULL;
    }
}

#else                           /* HAVE_KVM_H */

static off_t    klseek(off_t);
static int      klread(char *, int);
int             swap = -1, mem = -1, kmem = -1;

/**
 * Initialize the support for accessing kernel virtual memory.
 *
 * @return TRUE upon success; FALSE upon failure.
 */
int
init_kmem(const char *file)
{
    kmem = open(file, O_RDONLY);
    if (kmem < 0 && !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					    NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
        snmp_log_perror(file);
    }
    if (kmem >= 0)
        fcntl(kmem, F_SETFD, 1/*FD_CLOEXEC*/);
    mem = open("/dev/mem", O_RDONLY);
    if (mem < 0 && !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					   NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
        snmp_log_perror("/dev/mem");
    }
    if (mem >= 0)
        fcntl(mem, F_SETFD, 1/*FD_CLOEXEC*/);
#ifdef DMEM_LOC
    swap = open(DMEM_LOC, O_RDONLY);
    if (swap < 0 && !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					    NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
        snmp_log_perror(DMEM_LOC);
    }
    if (swap >= 0)
        fcntl(swap, F_SETFD, 1/*FD_CLOEXEC*/);
#endif
    return kmem >= 0 && mem >= 0 && swap >= 0;
}

/** @private
 *  Seek into the kernel for a value.
 */
static off_t
klseek(off_t base)
{
    return (lseek(kmem, (off_t) base, SEEK_SET));
}

/** @private
 *  Read from the kernel.
 */
static int
klread(char *buf, int buflen)
{
    return (read(kmem, buf, buflen));
}

/** Reads kernel memory.
 *  Seeks to the specified location in kmem, then
 *  does a read of given amount ob bytes into target buffer.
 *
 * @param off The location to seek.
 *
 * @param target The target buffer to read into.
 *
 * @param siz Number of bytes to read.
 *
 * @return gives 1 on success and 0 on failure.
 */
int
klookup(unsigned long off, void *target, size_t siz)
{
    long            retsiz;

    if (kmem < 0)
        return 0;

    if ((retsiz = klseek((off_t) off)) != off) {
        snmp_log(LOG_ERR, "klookup(%lx, %p, %d): ", off, target, siz);
        snmp_log_perror("klseek");
        return (0);
    }
    if ((retsiz = klread(target, siz)) != siz) {
        if (snmp_get_do_debugging()) {
            /*
             * these happen too often on too many architectures to print them
             * unless we're in debugging mode. People get very full log files. 
             */
            snmp_log(LOG_ERR, "klookup(%lx, %p, %d): ", off, target, siz);
            snmp_log_perror("klread");
        }
        return (0);
    }
    DEBUGMSGTL(("verbose:kernel:klookup", "klookup(%lx, %p, %d) succeeded", off, target, siz));
    return (1);
}

/** Closes the kernel memory support.
 */
void
free_kmem(void)
{
    if (swap >= 0) {
        close(swap);
        swap = -1;
    }
    if (mem >= 0) {
        close(mem);
        mem = -1;
    }
    if (kmem >= 0) {
        close(kmem);
        kmem = -1;
    }
}

#endif                          /* HAVE_KVM_H */

#else
int unused;	/* Suppress "empty translation unit" warning */
#endif                          /* NETSNMP_CAN_USE_NLIST */
