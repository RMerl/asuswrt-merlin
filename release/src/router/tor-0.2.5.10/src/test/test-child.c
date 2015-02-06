/* Copyright (c) 2011-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include <stdio.h>
#include "orconfig.h"
#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <string.h>

#ifdef _WIN32
#define SLEEP(sec) Sleep((sec)*1000)
#else
#define SLEEP(sec) sleep(sec)
#endif

/** Trivial test program which prints out its command line arguments so we can
 * check if tor_spawn_background() works */
int
main(int argc, char **argv)
{
  int i;
  int delay = 1;
  int fast = 0;

  if (argc > 1)  {
    if (!strcmp(argv[1], "--hang")) {
      delay = 60;
    } else if (!strcmp(argv[1], "--fast")) {
      fast = 1;
      delay = 0;
    }
  }

  fprintf(stdout, "OUT\n");
  fprintf(stderr, "ERR\n");
  for (i = 1; i < argc; i++)
    fprintf(stdout, "%s\n", argv[i]);
  if (!fast)
    fprintf(stdout, "SLEEPING\n");
  /* We need to flush stdout so that test_util_spawn_background_partial_read()
     succeed. Otherwise ReadFile() will get the entire output in one */
  // XXX: Can we make stdio flush on newline?
  fflush(stdout);
  if (!fast)
    SLEEP(1);
  fprintf(stdout, "DONE\n");
  fflush(stdout);
  if (fast)
    return 0;

  while (--delay) {
    SLEEP(1);
  }

  return 0;
}

