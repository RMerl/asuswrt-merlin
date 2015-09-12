/*
 * $Id: heavy.c,v 1.3 2005/04/25 16:42:24 paul Exp $
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* This programme shows the effects of 'heavy' long-running functions
 * on the cooperative threading model.
 *
 * Run it with a config file containing 'password whatever', telnet to it
 * (it defaults to port 4000) and enter the 'clear foo string' command.
 * then type whatever and observe that the vty interface is unresponsive
 * for quite a period of time, due to the clear_something command
 * taking a very long time to complete.
 */
#include <zebra.h>

#include "thread.h"
#include "vty.h"
#include "command.h"
#include "memory.h"
#include <math.h>

enum
{
  ITERS_FIRST = 0,
  ITERS_ERR = 100,
  ITERS_LATER = 400,
  ITERS_PRINT = 10,
  ITERS_MAX = 1000,
};

static void
slow_func (struct vty *vty, const char *str, const int i)
{
  double x = 1;
  int j;
  
  for (j = 0; j < 300; j++)
    x += sin(x)*j;
  
  if ((i % ITERS_LATER) == 0)
    printf ("%s: %d, temporary error, save this somehow and do it later..\n", 
            __func__, i);
  
  if ((i % ITERS_ERR) == 0)
    printf ("%s: hard error\n", __func__);
  
  if ((i % ITERS_PRINT) == 0)
    printf ("%s did %d, x = %g%s", str, i, x, VTY_NEWLINE);  
}

static void
clear_something (struct vty *vty, const char *str)
{
  int i;
  
  /* this could be like iterating through 150k of route_table 
   * or worse, iterating through a list of peers, to bgp_stop them with
   * each having 150k route tables to process...
   */
  for (i = ITERS_FIRST; i < ITERS_MAX; i++)
    slow_func (vty, str, i);
}

DEFUN (clear_foo,
       clear_foo_cmd,
       "clear foo .LINE",
       "clear command\n"
       "arbitrary string\n")
{
  char *str;
  if (!argc)
    {
      vty_out (vty, "%% string argument required%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  str = argv_concat (argv, argc, 0);
  
  clear_something (vty, str);
  XFREE (MTYPE_TMP, str);
  return CMD_SUCCESS;
}

static void
slow_vty_init()
{
  install_element (VIEW_NODE, &clear_foo_cmd);
}

void
test_init()
{
  slow_vty_init();
}
