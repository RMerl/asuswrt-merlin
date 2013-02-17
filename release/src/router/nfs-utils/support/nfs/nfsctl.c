/*
 * support/nfs/nfsctl.c
 *
 * Central syscall to the nfsd kernel module.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <asm/unistd.h>
#include "nfslib.h"

/* compatibility hack... */
#ifndef __NR_nfsctl
#define __NR_nfsctl	__NR_nfsservctl
#endif

int
nfsctl (int cmd, struct nfsctl_arg * argp, union nfsctl_res * resp)
{
  return syscall (__NR_nfsctl, cmd, argp, resp);
}
