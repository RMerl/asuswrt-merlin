/*
 * Copyright (C) 2003-2007 Red Hat, Inc.
 *
 * This file is part of util-linux.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Written by Elliot Lee <sopwith@redhat.com>
 * New personality options & code added by Jindrich Novy <jnovy@redhat.com>
 * ADD_NO_RANDOMIZE flag added by Arjan van de Ven <arjanv@redhat.com>
 * Help and MIPS support from Mike Frysinger (vapier@gentoo.org)
 * Better error handling from Dmitry V. Levin (ldv@altlinux.org)
 *
 * based on ideas from the ppc32 util by Guy Streeter (2002-01), based on the
 * sparc32 util by Jakub Jelinek (1998, 1999)
 */

#include <syscall.h>
#include <linux/personality.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <sys/utsname.h>
#include "nls.h"
#include "c.h"

#define set_pers(pers) ((long)syscall(SYS_personality, pers))

/* Options without equivalent short options */
enum {
	OPT_4GB		= CHAR_MAX + 1,
	OPT_UNAME26
};

#define turn_on(_flag, _opts) \
	do { \
		(_opts) |= _flag; \
		if (verbose) \
			printf(_("Switching on %s.\n"), #_flag); \
	} while(0)


#if !HAVE_DECL_UNAME26
# define UNAME26                 0x0020000
#endif
#if !HAVE_DECL_ADDR_NO_RANDOMIZE
# define ADDR_NO_RANDOMIZE       0x0040000
#endif
#if !HAVE_DECL_FDPIC_FUNCPTRS
# define FDPIC_FUNCPTRS          0x0080000
#endif
#if !HAVE_DECL_MMAP_PAGE_ZERO
# define MMAP_PAGE_ZERO          0x0100000
#endif
#if !HAVE_DECL_ADDR_COMPAT_LAYOUT
# define ADDR_COMPAT_LAYOUT      0x0200000
#endif
#if !HAVE_DECL_READ_IMPLIES_EXEC
# define READ_IMPLIES_EXEC       0x0400000
#endif
#if !HAVE_DECL_ADDR_LIMIT_32BIT
# define ADDR_LIMIT_32BIT        0x0800000
#endif
#if !HAVE_DECL_SHORT_INODE
# define SHORT_INODE             0x1000000
#endif
#if !HAVE_DECL_WHOLE_SECONDS
# define WHOLE_SECONDS           0x2000000
#endif
#if !HAVE_DECL_STICKY_TIMEOUTS
# define STICKY_TIMEOUTS         0x4000000
#endif
#if !HAVE_DECL_ADDR_LIMIT_3GB
# define ADDR_LIMIT_3GB          0x8000000
#endif

/* Options --3gb and --4gb are for compatibitity with an old Debian setarch
   implementation. */
static const struct option longopts[] =
{
    { "help",               0, 0, 'h' },
    { "verbose",            0, 0, 'v' },
    { "addr-no-randomize",  0, 0, 'R' },
    { "fdpic-funcptrs",     0, 0, 'F' },
    { "mmap-page-zero",     0, 0, 'Z' },
    { "addr-compat-layout", 0, 0, 'L' },
    { "read-implies-exec",  0, 0, 'X' },
    { "32bit",              0, 0, 'B' },
    { "short-inode",        0, 0, 'I' },
    { "whole-seconds",      0, 0, 'S' },
    { "sticky-timeouts",    0, 0, 'T' },
    { "3gb",                0, 0, '3' },
    { "4gb",                0, 0, OPT_4GB },
    { "uname-2.6",          0, 0, OPT_UNAME26 },
    { NULL,                 0, 0, 0 }
};

static void __attribute__((__noreturn__))
show_help(void)
{
  const char *p = program_invocation_short_name;

  if (!*p)
    p = "setarch";

  printf(_("Usage: %s%s [options] [program [program arguments]]\n\nOptions:\n"),
         p, !strcmp(p, "setarch") ? " <arch>" : "");

   printf(_(
   " -h, --help               displays this help text\n"
   " -v, --verbose            says what options are being switched on\n"
   " -R, --addr-no-randomize  disables randomization of the virtual address space\n"
   " -F, --fdpic-funcptrs     makes function pointers point to descriptors\n"
   " -Z, --mmap-page-zero     turns on MMAP_PAGE_ZERO\n"
   " -L, --addr-compat-layout changes the way virtual memory is allocated\n"
   " -X, --read-implies-exec  turns on READ_IMPLIES_EXEC\n"
   " -B, --32bit              turns on ADDR_LIMIT_32BIT\n"
   " -I, --short-inode        turns on SHORT_INODE\n"
   " -S, --whole-seconds      turns on WHOLE_SECONDS\n"
   " -T, --sticky-timeouts    turns on STICKY_TIMEOUTS\n"
   " -3, --3gb                limits the used address space to a maximum of 3 GB\n"
   "     --4gb                ignored (for backward compatibility only)\n"));

   printf(_(
   "     --uname-2.6          turns on UNAME26\n"));

  printf(_("\nFor more information see setarch(8).\n"));
  exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__))
show_usage(const char *s)
{
  const char *p = program_invocation_short_name;

  if (!*p)
    p = "setarch";

  fprintf(stderr, _("%s: %s\nTry `%s --help' for more information.\n"), p, s, p);
  exit(EXIT_FAILURE);
}


int set_arch(const char *pers, unsigned long options)
{
  struct utsname un;
  int i;
  unsigned long pers_value;

  struct {
    int perval;
    const char *target_arch, *result_arch;
  } transitions[] = {
    {PER_LINUX32, "linux32", NULL},
    {PER_LINUX, "linux64", NULL},
#if defined(__powerpc__) || defined(__powerpc64__)
    {PER_LINUX32, "ppc32", "ppc"},
    {PER_LINUX32, "ppc", "ppc"},
    {PER_LINUX, "ppc64", "ppc64"},
    {PER_LINUX, "ppc64pseries", "ppc64"},
    {PER_LINUX, "ppc64iseries", "ppc64"},
#endif
#if defined(__x86_64__) || defined(__i386__) || defined(__ia64__)
    {PER_LINUX32, "i386", "i386"},
    {PER_LINUX32, "i486", "i386"},
    {PER_LINUX32, "i586", "i386"},
    {PER_LINUX32, "i686", "i386"},
    {PER_LINUX32, "athlon", "i386"},
#endif
#if defined(__x86_64__) || defined(__i386__)
    {PER_LINUX, "x86_64", "x86_64"},
#endif
#if defined(__ia64__) || defined(__i386__)
    {PER_LINUX, "ia64", "ia64"},
#endif
#if defined(__hppa__)
    {PER_LINUX32, "parisc32", "parisc"},
    {PER_LINUX32, "parisc", "parisc"},
    {PER_LINUX, "parisc64", "parisc64"},
#endif
#if defined(__s390x__) || defined(__s390__)
    {PER_LINUX32, "s390", "s390"},
    {PER_LINUX, "s390x", "s390x"},
#endif
#if defined(__sparc64__) || defined(__sparc__)
    {PER_LINUX32, "sparc", "sparc"},
    {PER_LINUX32, "sparc32bash", "sparc"},
    {PER_LINUX32, "sparc32", "sparc"},
    {PER_LINUX, "sparc64", "sparc64"},
#endif
#if defined(__mips64__) || defined(__mips__)
    {PER_LINUX32, "mips32", "mips"},
    {PER_LINUX32, "mips", "mips"},
    {PER_LINUX, "mips64", "mips64"},
#endif
#if defined(__alpha__)
    {PER_LINUX, "alpha", "alpha"},
    {PER_LINUX, "alphaev5", "alpha"},
    {PER_LINUX, "alphaev56", "alpha"},
    {PER_LINUX, "alphaev6", "alpha"},
    {PER_LINUX, "alphaev67", "alpha"},
#endif
    {-1, NULL, NULL}
  };

  for(i = 0; transitions[i].perval >= 0; i++)
      if(!strcmp(pers, transitions[i].target_arch))
	break;

  if(transitions[i].perval < 0)
    errx(EXIT_FAILURE, _("%s: Unrecognized architecture"), pers);

  pers_value = transitions[i].perval | options;
  if (set_pers(pers_value) == -EINVAL)
    return 1;

  uname(&un);
  if(transitions[i].result_arch &&
	strcmp(un.machine, transitions[i].result_arch))
  {
    if(strcmp(transitions[i].result_arch, "i386")
       || (strcmp(un.machine, "i486")
	   && strcmp(un.machine, "i586")
	   && strcmp(un.machine, "i686")
	   && strcmp(un.machine, "athlon")))
      errx(EXIT_FAILURE, _("%s: Unrecognized architecture"), pers);
  }

  return 0;
}

int main(int argc, char *argv[])
{
  const char *p;
  unsigned long options = 0;
  int verbose = 0;
  int c;

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  if (argc < 1)
    show_usage(_("Not enough arguments"));

  p = program_invocation_short_name;
  if (!strcmp(p, "setarch")) {
    argv++;
    argc--;
    if (argc < 1)
      show_usage(_("Not enough arguments"));
    p = argv[0];
    argv[0] = argv[-1];      /* for getopt_long() to get the program name */
    if (!strcmp(p, "-h") || !strcmp(p, "--help"))
      show_help();
  }
  #if defined(__sparc64__) || defined(__sparc__)
   if (!strcmp(p, "sparc32bash")) {
       if (set_arch(p, 0L))
           err(EXIT_FAILURE, _("Failed to set personality to %s"), p);
       execl("/bin/bash", NULL);
       err(EXIT_FAILURE, "/bin/bash");
   }
  #endif

  while ((c = getopt_long(argc, argv, "+hv3BFILRSTXZ", longopts, NULL)) != -1) {
    switch (c) {
    case 'h':
      show_help();
      break;
    case 'v':
      verbose = 1;
      break;
    case 'R':
	turn_on(ADDR_NO_RANDOMIZE, options);
	break;
    case 'F':
	turn_on(FDPIC_FUNCPTRS, options);
	break;
    case 'Z':
	turn_on(MMAP_PAGE_ZERO, options);
	break;
    case 'L':
	turn_on(ADDR_COMPAT_LAYOUT, options);
	break;
    case 'X':
	turn_on(READ_IMPLIES_EXEC, options);
	break;
    case 'B':
	turn_on(ADDR_LIMIT_32BIT, options);
	break;
    case 'I':
	turn_on(SHORT_INODE, options);
	break;
    case 'S':
	turn_on(WHOLE_SECONDS, options);
	break;
    case 'T':
	turn_on(STICKY_TIMEOUTS, options);
	break;
    case '3':
	turn_on(ADDR_LIMIT_3GB, options);
	break;
    case OPT_4GB:          /* just ignore this one */
      break;
    case OPT_UNAME26:
	turn_on(UNAME26, options);
	break;
    }
  }

  argc -= optind;
  argv += optind;

  if (set_arch(p, options))
    err(EXIT_FAILURE, _("Failed to set personality to %s"), p);

  if (!argc) {
    execl("/bin/sh", "-sh", NULL);
    err(EXIT_FAILURE, "/bin/sh");
  }

  execvp(argv[0], argv);
  err(EXIT_FAILURE, "%s", argv[0]);
  return EXIT_FAILURE;
}
