/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file sandbox.h
 * \brief Header file for sandbox.c.
 **/

#ifndef SANDBOX_H_
#define SANDBOX_H_

#include "orconfig.h"
#include "torint.h"

#ifndef SYS_SECCOMP

/**
 * Used by SIGSYS signal handler to check if the signal was issued due to a
 * seccomp2 filter violation.
 */
#define SYS_SECCOMP 1

#endif

#if defined(HAVE_SECCOMP_H) && defined(__linux__)
#define USE_LIBSECCOMP
#endif

struct sandbox_cfg_elem;

/** Typedef to structure used to manage a sandbox configuration. */
typedef struct sandbox_cfg_elem sandbox_cfg_t;

/**
 * Linux definitions
 */
#ifdef USE_LIBSECCOMP

#ifndef __USE_GNU
#define __USE_GNU
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/ucontext.h>
#include <seccomp.h>
#include <netdb.h>

#define PARAM_PTR 0
#define PARAM_NUM 1

/**
 * Enum used to manage the type of the implementation for general purpose.
 */
typedef enum {
  /** Libseccomp implementation based on seccomp2*/
  LIBSECCOMP2 = 0
} SB_IMPL;

/**
 *  Configuration parameter structure associated with the LIBSECCOMP2
 *  implementation.
 */
typedef struct smp_param {
  /** syscall associated with parameter. */
  int syscall;

  /** parameter value. */
  char *value;
  /** parameter value, second argument. */
  char *value2;

  /**  parameter flag (0 = not protected, 1 = protected). */
  int prot;
} smp_param_t;

/**
 * Structure used to manage a sandbox configuration.
 *
 * It is implemented as a linked list of parameters. Currently only controls
 * parameters for open, openat, execve, stat64.
 */
struct sandbox_cfg_elem {
  /** Sandbox implementation which dictates the parameter type. */
  SB_IMPL implem;

  /** Configuration parameter. */
  smp_param_t *param;

  /** Next element of the configuration*/
  struct sandbox_cfg_elem *next;
};

/** Function pointer defining the prototype of a filter function.*/
typedef int (*sandbox_filter_func_t)(scmp_filter_ctx ctx,
    sandbox_cfg_t *filter);

/** Type that will be used in step 3 in order to manage multiple sandboxes.*/
typedef struct {
  /** function pointers associated with the filter */
  sandbox_filter_func_t *filter_func;

  /** filter function pointer parameters */
  sandbox_cfg_t *filter_dynamic;
} sandbox_t;

#endif // USE_LIBSECCOMP

#ifdef USE_LIBSECCOMP
/** Pre-calls getaddrinfo in order to pre-record result. */
int sandbox_add_addrinfo(const char *addr);

struct addrinfo;
/** Replacement for getaddrinfo(), using pre-recorded results. */
int sandbox_getaddrinfo(const char *name, const char *servname,
                        const struct addrinfo *hints,
                        struct addrinfo **res);
void sandbox_freeaddrinfo(struct addrinfo *addrinfo);
void sandbox_free_getaddrinfo_cache(void);
#else
#define sandbox_getaddrinfo(name, servname, hints, res)  \
  getaddrinfo((name),(servname), (hints),(res))
#define sandbox_add_addrinfo(name) \
  ((void)(name))
#define sandbox_freeaddrinfo(addrinfo) \
  freeaddrinfo((addrinfo))
#define sandbox_free_getaddrinfo_cache()
#endif

#ifdef USE_LIBSECCOMP
/** Returns a registered protected string used with the sandbox, given that
 * it matches the parameter.
 */
const char* sandbox_intern_string(const char *param);
#else
#define sandbox_intern_string(s) (s)
#endif

/** Creates an empty sandbox configuration file.*/
sandbox_cfg_t * sandbox_cfg_new(void);

/**
 * Function used to add a open allowed filename to a supplied configuration.
 * The (char*) specifies the path to the allowed file; we take ownership
 * of the pointer.
 */
int sandbox_cfg_allow_open_filename(sandbox_cfg_t **cfg, char *file);

int sandbox_cfg_allow_chmod_filename(sandbox_cfg_t **cfg, char *file);
int sandbox_cfg_allow_chown_filename(sandbox_cfg_t **cfg, char *file);

/* DOCDOC */
int sandbox_cfg_allow_rename(sandbox_cfg_t **cfg, char *file1, char *file2);

/**
 * Function used to add a openat allowed filename to a supplied configuration.
 * The (char*) specifies the path to the allowed file; we steal the pointer to
 * that file.
 */
int sandbox_cfg_allow_openat_filename(sandbox_cfg_t **cfg, char *file);

#if 0
/**
 * Function used to add a execve allowed filename to a supplied configuration.
 * The (char*) specifies the path to the allowed file; that pointer is stolen.
 */
int sandbox_cfg_allow_execve(sandbox_cfg_t **cfg, const char *com);
#endif

/**
 * Function used to add a stat/stat64 allowed filename to a configuration.
 * The (char*) specifies the path to the allowed file; that pointer is stolen.
 */
int sandbox_cfg_allow_stat_filename(sandbox_cfg_t **cfg, char *file);

/** Function used to initialise a sandbox configuration.*/
int sandbox_init(sandbox_cfg_t* cfg);

/** Return true iff the sandbox is turned on. */
int sandbox_is_active(void);

void sandbox_disable_getaddrinfo_cache(void);

#endif /* SANDBOX_H_ */

