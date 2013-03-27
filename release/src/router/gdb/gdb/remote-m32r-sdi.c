/* Remote debugging interface for M32R/SDI.

   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   Contributed by Renesas Technology Co.
   Written by Kei Sakamoto <sakamoto.kei@renesas.com>.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "inferior.h"
#include "target.h"
#include "regcache.h"
#include "gdb_string.h"
#include <ctype.h>
#include <signal.h>
#ifdef __MINGW32__
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>


#include "serial.h"

/* Descriptor for I/O to remote machine.  */

static struct serial *sdi_desc = NULL;

#define SDI_TIMEOUT 30


#define SDIPORT 3232

static char chip_name[64];

static int step_mode;
static unsigned long last_pc_addr = 0xffffffff;
static unsigned char last_pc_addr_data[2];

static int mmu_on = 0;

static int use_ib_breakpoints = 1;

#define MAX_BREAKPOINTS 1024
static int max_ib_breakpoints;
static unsigned long bp_address[MAX_BREAKPOINTS];
static unsigned char bp_data[MAX_BREAKPOINTS][4];

/* dbt -> nop */
static const unsigned char dbt_bp_entry[] = {
  0x10, 0xe0, 0x70, 0x00
};

#define MAX_ACCESS_BREAKS 4
static int max_access_breaks;
static unsigned long ab_address[MAX_ACCESS_BREAKS];
static unsigned int ab_type[MAX_ACCESS_BREAKS];
static unsigned int ab_size[MAX_ACCESS_BREAKS];
static CORE_ADDR hit_watchpoint_addr = 0;

static int interrupted = 0;

/* Forward data declarations */
extern struct target_ops m32r_ops;


/* Commands */
#define SDI_OPEN                 1
#define SDI_CLOSE                2
#define SDI_RELEASE              3
#define SDI_READ_CPU_REG         4
#define SDI_WRITE_CPU_REG        5
#define SDI_READ_MEMORY          6
#define SDI_WRITE_MEMORY         7
#define SDI_EXEC_CPU             8
#define SDI_STOP_CPU             9
#define SDI_WAIT_FOR_READY      10
#define SDI_GET_ATTR            11
#define SDI_SET_ATTR            12
#define SDI_STATUS              13

/* Attributes */
#define SDI_ATTR_NAME            1
#define SDI_ATTR_BRK             2
#define SDI_ATTR_ABRK            3
#define SDI_ATTR_CACHE           4
#define SDI_CACHE_TYPE_M32102    0
#define SDI_CACHE_TYPE_CHAOS     1
#define SDI_ATTR_MEM_ACCESS      5
#define SDI_MEM_ACCESS_DEBUG_DMA 0
#define SDI_MEM_ACCESS_MON_CODE  1

/* Registers */
#define SDI_REG_R0               0
#define SDI_REG_R1               1
#define SDI_REG_R2               2
#define SDI_REG_R3               3
#define SDI_REG_R4               4
#define SDI_REG_R5               5
#define SDI_REG_R6               6
#define SDI_REG_R7               7
#define SDI_REG_R8               8
#define SDI_REG_R9               9
#define SDI_REG_R10             10
#define SDI_REG_R11             11
#define SDI_REG_R12             12
#define SDI_REG_FP              13
#define SDI_REG_LR              14
#define SDI_REG_SP              15
#define SDI_REG_PSW             16
#define SDI_REG_CBR             17
#define SDI_REG_SPI             18
#define SDI_REG_SPU             19
#define SDI_REG_CR4             20
#define SDI_REG_EVB             21
#define SDI_REG_BPC             22
#define SDI_REG_CR7             23
#define SDI_REG_BBPSW           24
#define SDI_REG_CR9             25
#define SDI_REG_CR10            26
#define SDI_REG_CR11            27
#define SDI_REG_CR12            28
#define SDI_REG_WR              29
#define SDI_REG_BBPC            30
#define SDI_REG_PBP             31
#define SDI_REG_ACCH            32
#define SDI_REG_ACCL            33
#define SDI_REG_ACC1H           34
#define SDI_REG_ACC1L           35


/* Low level communication functions */

/* Check an ack packet from the target */
static int
get_ack (void)
{
  int c;

  if (!sdi_desc)
    return -1;

  c = serial_readchar (sdi_desc, SDI_TIMEOUT);

  if (c < 0)
    return -1;

  if (c != '+')			/* error */
    return -1;

  return 0;
}

/* Send data to the target and check an ack packet */
static int
send_data (void *buf, int len)
{
  int ret;

  if (!sdi_desc)
    return -1;

  if (serial_write (sdi_desc, buf, len) != 0)
    return -1;

  if (get_ack () == -1)
    return -1;

  return len;
}

/* Receive data from the target */
static int
recv_data (void *buf, int len)
{
  int total = 0;
  int c;

  if (!sdi_desc)
    return -1;

  while (total < len)
    {
      c = serial_readchar (sdi_desc, SDI_TIMEOUT);

      if (c < 0)
	return -1;

      ((unsigned char *) buf)[total++] = c;
    }

  return len;
}

/* Store unsigned long parameter on packet */
static void
store_long_parameter (void *buf, long val)
{
  val = htonl (val);
  memcpy (buf, &val, 4);
}

static int
send_cmd (unsigned char cmd)
{
  unsigned char buf[1];
  buf[0] = cmd;
  return send_data (buf, 1);
}

static int
send_one_arg_cmd (unsigned char cmd, unsigned char arg1)
{
  unsigned char buf[2];
  buf[0] = cmd;
  buf[1] = arg1;
  return send_data (buf, 2);
}

static int
send_two_arg_cmd (unsigned char cmd, unsigned char arg1, unsigned long arg2)
{
  unsigned char buf[6];
  buf[0] = cmd;
  buf[1] = arg1;
  store_long_parameter (buf + 2, arg2);
  return send_data (buf, 6);
}

static int
send_three_arg_cmd (unsigned char cmd, unsigned long arg1, unsigned long arg2,
		    unsigned long arg3)
{
  unsigned char buf[13];
  buf[0] = cmd;
  store_long_parameter (buf + 1, arg1);
  store_long_parameter (buf + 5, arg2);
  store_long_parameter (buf + 9, arg3);
  return send_data (buf, 13);
}

static unsigned char
recv_char_data (void)
{
  unsigned char val;
  recv_data (&val, 1);
  return val;
}

static unsigned long
recv_long_data (void)
{
  unsigned long val;
  recv_data (&val, 4);
  return ntohl (val);
}


/* Check if MMU is on */
static void
check_mmu_status (void)
{
  unsigned long val;

  /* Read PC address */
  if (send_one_arg_cmd (SDI_READ_CPU_REG, SDI_REG_BPC) == -1)
    return;
  val = recv_long_data ();
  if ((val & 0xc0000000) == 0x80000000)
    {
      mmu_on = 1;
      return;
    }

  /* Read EVB address */
  if (send_one_arg_cmd (SDI_READ_CPU_REG, SDI_REG_EVB) == -1)
    return;
  val = recv_long_data ();
  if ((val & 0xc0000000) == 0x80000000)
    {
      mmu_on = 1;
      return;
    }

  mmu_on = 0;
}


/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
static void
m32r_create_inferior (char *execfile, char *args, char **env, int from_tty)
{
  CORE_ADDR entry_pt;

  if (args && *args)
    error (_("Cannot pass arguments to remote STDEBUG process"));

  if (execfile == 0 || exec_bfd == 0)
    error (_("No executable file specified"));

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_create_inferior(%s,%s)\n", execfile,
			args);

  entry_pt = bfd_get_start_address (exec_bfd);

  /* The "process" (board) is already stopped awaiting our commands, and
     the program is already downloaded.  We just set its PC and go.  */

  clear_proceed_status ();

  /* Tell wait_for_inferior that we've started a new process.  */
  init_wait_for_inferior ();

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */
  target_terminal_init ();

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  write_pc (entry_pt);
}

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

static void
m32r_open (char *args, int from_tty)
{
  struct hostent *host_ent;
  struct sockaddr_in server_addr;
  char *port_str, hostname[256];
  int port;
  int i, n;
  int yes = 1;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_open(%d)\n", from_tty);

  target_preopen (from_tty);

  push_target (&m32r_ops);

  if (args == NULL)
    sprintf (hostname, "localhost:%d", SDIPORT);
  else
    {
      port_str = strchr (args, ':');
      if (port_str == NULL)
	sprintf (hostname, "%s:%d", args, SDIPORT);
      else
	strcpy (hostname, args);
    }

  sdi_desc = serial_open (hostname);
  if (!sdi_desc)
    error (_("Connection refused."));

  if (get_ack () == -1)
    error (_("Cannot connect to SDI target."));

  if (send_cmd (SDI_OPEN) == -1)
    error (_("Cannot connect to SDI target."));

  /* Get maximum number of ib breakpoints */
  send_one_arg_cmd (SDI_GET_ATTR, SDI_ATTR_BRK);
  max_ib_breakpoints = recv_char_data ();
  if (remote_debug)
    printf_filtered ("Max IB Breakpoints = %d\n", max_ib_breakpoints);

  /* Initialize breakpoints. */
  for (i = 0; i < MAX_BREAKPOINTS; i++)
    bp_address[i] = 0xffffffff;

  /* Get maximum number of access breaks. */
  send_one_arg_cmd (SDI_GET_ATTR, SDI_ATTR_ABRK);
  max_access_breaks = recv_char_data ();
  if (remote_debug)
    printf_filtered ("Max Access Breaks = %d\n", max_access_breaks);

  /* Initialize access breask. */
  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    ab_address[i] = 0x00000000;

  check_mmu_status ();

  /* Get the name of chip on target board. */
  send_one_arg_cmd (SDI_GET_ATTR, SDI_ATTR_NAME);
  recv_data (chip_name, 64);

  if (from_tty)
    printf_filtered ("Remote %s connected to %s\n", target_shortname,
		     chip_name);
}

/* Close out all files and local state before this target loses control. */

static void
m32r_close (int quitting)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_close(%d)\n", quitting);

  if (sdi_desc)
    {
      send_cmd (SDI_CLOSE);
      serial_close (sdi_desc);
      sdi_desc = NULL;
    }

  inferior_ptid = null_ptid;
  return;
}

/* Tell the remote machine to resume.  */

static void
m32r_resume (ptid_t ptid, int step, enum target_signal sig)
{
  unsigned long pc_addr, bp_addr, ab_addr;
  int ib_breakpoints;
  unsigned char buf[13];
  int i;

  if (remote_debug)
    {
      if (step)
	fprintf_unfiltered (gdb_stdlog, "\nm32r_resume(step)\n");
      else
	fprintf_unfiltered (gdb_stdlog, "\nm32r_resume(cont)\n");
    }

  check_mmu_status ();

  pc_addr = read_pc ();
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "pc <= 0x%lx\n", pc_addr);

  /* At pc address there is a parallel instruction with +2 offset,
     so we have to make it a serial instruction or avoid it. */
  if (pc_addr == last_pc_addr)
    {
      /* Avoid a parallel nop. */
      if (last_pc_addr_data[0] == 0xf0 && last_pc_addr_data[1] == 0x00)
	{
	  pc_addr += 2;
	  /* Now we can forget this instruction. */
	  last_pc_addr = 0xffffffff;
	}
      /* Clear a parallel bit. */
      else
	{
	  buf[0] = SDI_WRITE_MEMORY;
	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	    store_long_parameter (buf + 1, pc_addr);
	  else
	    store_long_parameter (buf + 1, pc_addr - 1);
	  store_long_parameter (buf + 5, 1);
	  buf[9] = last_pc_addr_data[0] & 0x7f;
	  send_data (buf, 10);
	}
    }

  /* Set PC. */
  send_two_arg_cmd (SDI_WRITE_CPU_REG, SDI_REG_BPC, pc_addr);

  /* step mode. */
  step_mode = step;
  if (step)
    {
      /* Set PBP. */
      send_two_arg_cmd (SDI_WRITE_CPU_REG, SDI_REG_PBP, pc_addr | 1);
    }
  else
    {
      /* Unset PBP. */
      send_two_arg_cmd (SDI_WRITE_CPU_REG, SDI_REG_PBP, 0x00000000);
    }

  if (use_ib_breakpoints)
    ib_breakpoints = max_ib_breakpoints;
  else
    ib_breakpoints = 0;

  /* Set ib breakpoints. */
  for (i = 0; i < ib_breakpoints; i++)
    {
      bp_addr = bp_address[i];

      if (bp_addr == 0xffffffff)
	continue;

      /* Set PBP. */
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8000 + 4 * i, 4,
			    0x00000006);
      else
	send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8000 + 4 * i, 4,
			    0x06000000);

      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8080 + 4 * i, 4, bp_addr);
    }

  /* Set dbt breakpoints. */
  for (i = ib_breakpoints; i < MAX_BREAKPOINTS; i++)
    {
      bp_addr = bp_address[i];

      if (bp_addr == 0xffffffff)
	continue;

      if (!mmu_on)
	bp_addr &= 0x7fffffff;

      /* Write DBT instruction. */
      buf[0] = SDI_WRITE_MEMORY;
      store_long_parameter (buf + 1, (bp_addr & 0xfffffffc));
      store_long_parameter (buf + 5, 4);
      if ((bp_addr & 2) == 0 && bp_addr != (pc_addr & 0xfffffffc))
	{
	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	    {
	      buf[9] = dbt_bp_entry[0];
	      buf[10] = dbt_bp_entry[1];
	      buf[11] = dbt_bp_entry[2];
	      buf[12] = dbt_bp_entry[3];
	    }
	  else
	    {
	      buf[9] = dbt_bp_entry[3];
	      buf[10] = dbt_bp_entry[2];
	      buf[11] = dbt_bp_entry[1];
	      buf[12] = dbt_bp_entry[0];
	    }
	}
      else
	{
	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	    {
	      if ((bp_addr & 2) == 0)
		{
		  buf[9] = dbt_bp_entry[0];
		  buf[10] = dbt_bp_entry[1];
		  buf[11] = bp_data[i][2] & 0x7f;
		  buf[12] = bp_data[i][3];
		}
	      else
		{
		  buf[9] = bp_data[i][0];
		  buf[10] = bp_data[i][1];
		  buf[11] = dbt_bp_entry[0];
		  buf[12] = dbt_bp_entry[1];
		}
	    }
	  else
	    {
	      if ((bp_addr & 2) == 0)
		{
		  buf[9] = bp_data[i][0];
		  buf[10] = bp_data[i][1] & 0x7f;
		  buf[11] = dbt_bp_entry[1];
		  buf[12] = dbt_bp_entry[0];
		}
	      else
		{
		  buf[9] = dbt_bp_entry[1];
		  buf[10] = dbt_bp_entry[0];
		  buf[11] = bp_data[i][2];
		  buf[12] = bp_data[i][3];
		}
	    }
	}
      send_data (buf, 13);
    }

  /* Set access breaks. */
  for (i = 0; i < max_access_breaks; i++)
    {
      ab_addr = ab_address[i];

      if (ab_addr == 0x00000000)
	continue;

      /* DBC register */
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	{
	  switch (ab_type[i])
	    {
	    case 0:		/* write watch */
	      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
				  0x00000086);
	      break;
	    case 1:		/* read watch */
	      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
				  0x00000046);
	      break;
	    case 2:		/* access watch */
	      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
				  0x00000006);
	      break;
	    }
	}
      else
	{
	  switch (ab_type[i])
	    {
	    case 0:		/* write watch */
	      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
				  0x86000000);
	      break;
	    case 1:		/* read watch */
	      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
				  0x46000000);
	      break;
	    case 2:		/* access watch */
	      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
				  0x06000000);
	      break;
	    }
	}

      /* DBAH register */
      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8180 + 4 * i, 4, ab_addr);

      /* DBAL register */
      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8200 + 4 * i, 4,
			  0xffffffff);

      /* DBD register */
      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8280 + 4 * i, 4,
			  0x00000000);

      /* DBDM register */
      send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8300 + 4 * i, 4,
			  0x00000000);
    }

  /* Resume program. */
  send_cmd (SDI_EXEC_CPU);

  /* Without this, some commands which require an active target (such as kill)
     won't work.  This variable serves (at least) double duty as both the pid
     of the target process (if it has such), and as a flag indicating that a
     target is active.  These functions should be split out into seperate
     variables, especially since GDB will someday have a notion of debugging
     several processes.  */
  inferior_ptid = pid_to_ptid (32);

  return;
}

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.  */

static void
gdb_cntrl_c (int signo)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "interrupt\n");
  interrupted = 1;
}

static ptid_t
m32r_wait (ptid_t ptid, struct target_waitstatus *status)
{
  static RETSIGTYPE (*prev_sigint) ();
  unsigned long bp_addr, pc_addr;
  int ib_breakpoints;
  long i;
  unsigned char buf[13];
  unsigned long val;
  int ret, c;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_wait()\n");

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.sig = 0;

  interrupted = 0;
  prev_sigint = signal (SIGINT, gdb_cntrl_c);

  /* Wait for ready */
  buf[0] = SDI_WAIT_FOR_READY;
  if (serial_write (sdi_desc, buf, 1) != 0)
    error (_("Remote connection closed"));

  while (1)
    {
      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
      if (c < 0)
	error (_("Remote connection closed"));

      if (c == '-')		/* error */
	{
	  status->kind = TARGET_WAITKIND_STOPPED;
	  status->value.sig = TARGET_SIGNAL_HUP;
	  return inferior_ptid;
	}
      else if (c == '+')	/* stopped */
	break;

      if (interrupted)
	ret = serial_write (sdi_desc, "!", 1);	/* packet to interrupt */
      else
	ret = serial_write (sdi_desc, ".", 1);	/* packet to wait */
      if (ret != 0)
	error (_("Remote connection closed"));
    }

  status->kind = TARGET_WAITKIND_STOPPED;
  if (interrupted)
    status->value.sig = TARGET_SIGNAL_INT;
  else
    status->value.sig = TARGET_SIGNAL_TRAP;

  interrupted = 0;
  signal (SIGINT, prev_sigint);

  check_mmu_status ();

  /* Recover parallel bit. */
  if (last_pc_addr != 0xffffffff)
    {
      buf[0] = SDI_WRITE_MEMORY;
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	store_long_parameter (buf + 1, last_pc_addr);
      else
	store_long_parameter (buf + 1, last_pc_addr - 1);
      store_long_parameter (buf + 5, 1);
      buf[9] = last_pc_addr_data[0];
      send_data (buf, 10);
      last_pc_addr = 0xffffffff;
    }

  if (use_ib_breakpoints)
    ib_breakpoints = max_ib_breakpoints;
  else
    ib_breakpoints = 0;

  /* Set back pc by 2 if m32r is stopped with dbt. */
  last_pc_addr = 0xffffffff;
  send_one_arg_cmd (SDI_READ_CPU_REG, SDI_REG_BPC);
  pc_addr = recv_long_data () - 2;
  for (i = ib_breakpoints; i < MAX_BREAKPOINTS; i++)
    {
      if (pc_addr == bp_address[i])
	{
	  send_two_arg_cmd (SDI_WRITE_CPU_REG, SDI_REG_BPC, pc_addr);

	  /* If there is a parallel instruction with +2 offset at pc
	     address, we have to take care of it later. */
	  if ((pc_addr & 0x2) != 0)
	    {
	      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
		{
		  if ((bp_data[i][2] & 0x80) != 0)
		    {
		      last_pc_addr = pc_addr;
		      last_pc_addr_data[0] = bp_data[i][2];
		      last_pc_addr_data[1] = bp_data[i][3];
		    }
		}
	      else
		{
		  if ((bp_data[i][1] & 0x80) != 0)
		    {
		      last_pc_addr = pc_addr;
		      last_pc_addr_data[0] = bp_data[i][1];
		      last_pc_addr_data[1] = bp_data[i][0];
		    }
		}
	    }
	  break;
	}
    }

  /* Remove ib breakpoints. */
  for (i = 0; i < ib_breakpoints; i++)
    {
      if (bp_address[i] != 0xffffffff)
	send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8000 + 4 * i, 4,
			    0x00000000);
    }
  /* Remove dbt breakpoints. */
  for (i = ib_breakpoints; i < MAX_BREAKPOINTS; i++)
    {
      bp_addr = bp_address[i];
      if (bp_addr != 0xffffffff)
	{
	  if (!mmu_on)
	    bp_addr &= 0x7fffffff;
	  buf[0] = SDI_WRITE_MEMORY;
	  store_long_parameter (buf + 1, bp_addr & 0xfffffffc);
	  store_long_parameter (buf + 5, 4);
	  buf[9] = bp_data[i][0];
	  buf[10] = bp_data[i][1];
	  buf[11] = bp_data[i][2];
	  buf[12] = bp_data[i][3];
	  send_data (buf, 13);
	}
    }

  /* Remove access breaks. */
  hit_watchpoint_addr = 0;
  for (i = 0; i < max_access_breaks; i++)
    {
      if (ab_address[i] != 0x00000000)
	{
	  buf[0] = SDI_READ_MEMORY;
	  store_long_parameter (buf + 1, 0xffff8100 + 4 * i);
	  store_long_parameter (buf + 5, 4);
	  serial_write (sdi_desc, buf, 9);
	  c = serial_readchar (sdi_desc, SDI_TIMEOUT);
	  if (c != '-' && recv_data (buf, 4) != -1)
	    {
	      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
		{
		  if ((buf[3] & 0x1) == 0x1)
		    hit_watchpoint_addr = ab_address[i];
		}
	      else
		{
		  if ((buf[0] & 0x1) == 0x1)
		    hit_watchpoint_addr = ab_address[i];
		}
	    }

	  send_three_arg_cmd (SDI_WRITE_MEMORY, 0xffff8100 + 4 * i, 4,
			      0x00000000);
	}
    }

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "pc => 0x%lx\n", pc_addr);

  return inferior_ptid;
}

/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.  */
static void
m32r_detach (char *args, int from_tty)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_detach(%d)\n", from_tty);

  m32r_resume (inferior_ptid, 0, 0);

  /* calls m32r_close to do the real work */
  pop_target ();
  if (from_tty)
    fprintf_unfiltered (gdb_stdlog, "Ending remote %s debugging\n",
			target_shortname);
}

/* Return the id of register number REGNO. */

static int
get_reg_id (int regno)
{
  switch (regno)
    {
    case 20:
      return SDI_REG_BBPC;
    case 21:
      return SDI_REG_BPC;
    case 22:
      return SDI_REG_ACCL;
    case 23:
      return SDI_REG_ACCH;
    case 24:
      return SDI_REG_EVB;
    }

  return regno;
}

/* Read the remote registers into the block REGS.  */

static void m32r_fetch_register (struct regcache *, int);

static void
m32r_fetch_registers (struct regcache *regcache)
{
  int regno;

  for (regno = 0; regno < gdbarch_num_regs (current_gdbarch); regno++)
    m32r_fetch_register (regcache, regno);
}

/* Fetch register REGNO, or all registers if REGNO is -1.
   Returns errno value.  */
static void
m32r_fetch_register (struct regcache *regcache, int regno)
{
  unsigned long val, val2, regid;

  if (regno == -1)
    m32r_fetch_registers (regcache);
  else
    {
      char buffer[MAX_REGISTER_SIZE];

      regid = get_reg_id (regno);
      send_one_arg_cmd (SDI_READ_CPU_REG, regid);
      val = recv_long_data ();

      if (regid == SDI_REG_PSW)
	{
	  send_one_arg_cmd (SDI_READ_CPU_REG, SDI_REG_BBPSW);
	  val2 = recv_long_data ();
	  val = ((0x00cf & val2) << 8) | ((0xcf00 & val) >> 8);
	}

      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "m32r_fetch_register(%d,0x%08lx)\n",
			    regno, val);

      /* We got the number the register holds, but gdb expects to see a
         value in the target byte ordering.  */
      store_unsigned_integer (buffer, 4, val);
      regcache_raw_supply (regcache, regno, buffer);
    }
  return;
}

/* Store the remote registers from the contents of the block REGS.  */

static void m32r_store_register (struct regcache *, int);

static void
m32r_store_registers (struct regcache *regcache)
{
  int regno;

  for (regno = 0; regno < gdbarch_num_regs (current_gdbarch); regno++)
    m32r_store_register (regcache, regno);

  registers_changed ();
}

/* Store register REGNO, or all if REGNO == 0.
   Return errno value.  */
static void
m32r_store_register (struct regcache *regcache, int regno)
{
  int regid;
  ULONGEST regval, tmp;

  if (regno == -1)
    m32r_store_registers (regcache);
  else
    {
      regcache_cooked_read_unsigned (regcache, regno, &regval);
      regid = get_reg_id (regno);

      if (regid == SDI_REG_PSW)
	{
	  unsigned long psw, bbpsw;

	  send_one_arg_cmd (SDI_READ_CPU_REG, SDI_REG_PSW);
	  psw = recv_long_data ();

	  send_one_arg_cmd (SDI_READ_CPU_REG, SDI_REG_BBPSW);
	  bbpsw = recv_long_data ();

	  tmp = (0x00cf & psw) | ((0x00cf & regval) << 8);
	  send_two_arg_cmd (SDI_WRITE_CPU_REG, SDI_REG_PSW, tmp);

	  tmp = (0x0030 & bbpsw) | ((0xcf00 & regval) >> 8);
	  send_two_arg_cmd (SDI_WRITE_CPU_REG, SDI_REG_BBPSW, tmp);
	}
      else
	{
	  send_two_arg_cmd (SDI_WRITE_CPU_REG, regid, regval);
	}

      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "m32r_store_register(%d,0x%08lu)\n",
			    regno, (unsigned long) regval);
    }
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
m32r_prepare_to_store (struct regcache *regcache)
{
  /* Do nothing, since we can store individual regs */
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_prepare_to_store()\n");
}

static void
m32r_files_info (struct target_ops *target)
{
  char *file = "nothing";

  if (exec_bfd)
    {
      file = bfd_get_filename (exec_bfd);
      printf_filtered ("\tAttached to %s running program %s\n",
		       chip_name, file);
    }
}

/* Read/Write memory.  */
static int
m32r_xfer_memory (CORE_ADDR memaddr, gdb_byte *myaddr, int len,
		  int write,
		  struct mem_attrib *attrib, struct target_ops *target)
{
  unsigned long taddr;
  unsigned char buf[0x2000];
  int ret, c;

  taddr = memaddr;

  if (!mmu_on)
    {
      if ((taddr & 0xa0000000) == 0x80000000)
	taddr &= 0x7fffffff;
    }

  if (remote_debug)
    {
      if (write)
	fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory(%08lx,%d,write)\n",
			    memaddr, len);
      else
	fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory(%08lx,%d,read)\n",
			    memaddr, len);
    }

  if (write)
    {
      buf[0] = SDI_WRITE_MEMORY;
      store_long_parameter (buf + 1, taddr);
      store_long_parameter (buf + 5, len);
      if (len < 0x1000)
	{
	  memcpy (buf + 9, myaddr, len);
	  ret = send_data (buf, len + 9) - 9;
	}
      else
	{
	  if (serial_write (sdi_desc, buf, 9) != 0)
	    {
	      if (remote_debug)
		fprintf_unfiltered (gdb_stdlog,
				    "m32r_xfer_memory() failed\n");
	      return 0;
	    }
	  ret = send_data (myaddr, len);
	}
    }
  else
    {
      buf[0] = SDI_READ_MEMORY;
      store_long_parameter (buf + 1, taddr);
      store_long_parameter (buf + 5, len);
      if (serial_write (sdi_desc, buf, 9) != 0)
	{
	  if (remote_debug)
	    fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory() failed\n");
	  return 0;
	}

      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
      if (c < 0 || c == '-')
	{
	  if (remote_debug)
	    fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory() failed\n");
	  return 0;
	}

      ret = recv_data (myaddr, len);
    }

  if (ret <= 0)
    {
      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory() fails\n");
      return 0;
    }

  return ret;
}

static void
m32r_kill (void)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_kill()\n");

  inferior_ptid = null_ptid;

  return;
}

/* Clean up when a program exits.

   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

static void
m32r_mourn_inferior (void)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_mourn_inferior()\n");

  remove_breakpoints ();
  generic_mourn_inferior ();
}

static int
m32r_insert_breakpoint (struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address;
  int ib_breakpoints;
  unsigned char buf[13];
  int i, c;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_insert_breakpoint(%08lx,...)\n",
			addr);

  if (use_ib_breakpoints)
    ib_breakpoints = max_ib_breakpoints;
  else
    ib_breakpoints = 0;

  for (i = 0; i < MAX_BREAKPOINTS; i++)
    {
      if (bp_address[i] == 0xffffffff)
	{
	  bp_address[i] = addr;
	  if (i >= ib_breakpoints)
	    {
	      buf[0] = SDI_READ_MEMORY;
	      if (mmu_on)
		store_long_parameter (buf + 1, addr & 0xfffffffc);
	      else
		store_long_parameter (buf + 1, addr & 0x7ffffffc);
	      store_long_parameter (buf + 5, 4);
	      serial_write (sdi_desc, buf, 9);
	      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
	      if (c != '-')
		recv_data (bp_data[i], 4);
	    }
	  return 0;
	}
    }

  error (_("Too many breakpoints"));
  return 1;
}

static int
m32r_remove_breakpoint (struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address;
  int i;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_remove_breakpoint(%08lx)\n",
			addr);

  for (i = 0; i < MAX_BREAKPOINTS; i++)
    {
      if (bp_address[i] == addr)
	{
	  bp_address[i] = 0xffffffff;
	  break;
	}
    }

  return 0;
}

static void
m32r_load (char *args, int from_tty)
{
  struct cleanup *old_chain;
  asection *section;
  bfd *pbfd;
  bfd_vma entry;
  char *filename;
  int quiet;
  int nostart;
  struct timeval start_time, end_time;
  unsigned long data_count;	/* Number of bytes transferred to memory */
  int ret;
  static RETSIGTYPE (*prev_sigint) ();

  /* for direct tcp connections, we can do a fast binary download */
  quiet = 0;
  nostart = 0;
  filename = NULL;

  while (*args != '\000')
    {
      char *arg;

      while (isspace (*args))
	args++;

      arg = args;

      while ((*args != '\000') && !isspace (*args))
	args++;

      if (*args != '\000')
	*args++ = '\000';

      if (*arg != '-')
	filename = arg;
      else if (strncmp (arg, "-quiet", strlen (arg)) == 0)
	quiet = 1;
      else if (strncmp (arg, "-nostart", strlen (arg)) == 0)
	nostart = 1;
      else
	error (_("Unknown option `%s'"), arg);
    }

  if (!filename)
    filename = get_exec_file (1);

  pbfd = bfd_openr (filename, gnutarget);
  if (pbfd == NULL)
    {
      perror_with_name (filename);
      return;
    }
  old_chain = make_cleanup_bfd_close (pbfd);

  if (!bfd_check_format (pbfd, bfd_object))
    error (_("\"%s\" is not an object file: %s"), filename,
	   bfd_errmsg (bfd_get_error ()));

  gettimeofday (&start_time, NULL);
  data_count = 0;

  interrupted = 0;
  prev_sigint = signal (SIGINT, gdb_cntrl_c);

  for (section = pbfd->sections; section; section = section->next)
    {
      if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)
	{
	  bfd_vma section_address;
	  bfd_size_type section_size;
	  file_ptr fptr;
	  int n;

	  section_address = bfd_section_lma (pbfd, section);
	  section_size = bfd_get_section_size (section);

	  if (!mmu_on)
	    {
	      if ((section_address & 0xa0000000) == 0x80000000)
		section_address &= 0x7fffffff;
	    }

	  if (!quiet)
	    printf_filtered ("[Loading section %s at 0x%lx (%d bytes)]\n",
			     bfd_get_section_name (pbfd, section),
			     section_address, (int) section_size);

	  fptr = 0;

	  data_count += section_size;

	  n = 0;
	  while (section_size > 0)
	    {
	      char unsigned buf[0x1000 + 9];
	      int count;

	      count = min (section_size, 0x1000);

	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, section_address);
	      store_long_parameter (buf + 5, count);

	      bfd_get_section_contents (pbfd, section, buf + 9, fptr, count);
	      if (send_data (buf, count + 9) <= 0)
		error (_("Error while downloading %s section."),
		       bfd_get_section_name (pbfd, section));

	      if (!quiet)
		{
		  printf_unfiltered (".");
		  if (n++ > 60)
		    {
		      printf_unfiltered ("\n");
		      n = 0;
		    }
		  gdb_flush (gdb_stdout);
		}

	      section_address += count;
	      fptr += count;
	      section_size -= count;

	      if (interrupted)
		break;
	    }

	  if (!quiet && !interrupted)
	    {
	      printf_unfiltered ("done.\n");
	      gdb_flush (gdb_stdout);
	    }
	}

      if (interrupted)
	{
	  printf_unfiltered ("Interrupted.\n");
	  break;
	}
    }

  interrupted = 0;
  signal (SIGINT, prev_sigint);

  gettimeofday (&end_time, NULL);

  /* Make the PC point at the start address */
  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_ptid = null_ptid;	/* No process now */

  /* This is necessary because many things were based on the PC at the time
     that we attached to the monitor, which is no longer valid now that we
     have loaded new code (and just changed the PC).  Another way to do this
     might be to call normal_stop, except that the stack may not be valid,
     and things would get horribly confused... */

  clear_symtab_users ();

  if (!nostart)
    {
      entry = bfd_get_start_address (pbfd);

      if (!quiet)
	printf_unfiltered ("[Starting %s at 0x%lx]\n", filename, entry);
    }

  print_transfer_performance (gdb_stdout, data_count, 0, &start_time,
			      &end_time);

  do_cleanups (old_chain);
}

static void
m32r_stop (void)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_stop()\n");

  send_cmd (SDI_STOP_CPU);

  return;
}


/* Tell whether this target can support a hardware breakpoint.  CNT
   is the number of hardware breakpoints already installed.  This
   implements the TARGET_CAN_USE_HARDWARE_WATCHPOINT macro.  */

int
m32r_can_use_hw_watchpoint (int type, int cnt, int othertype)
{
  return sdi_desc != NULL && cnt < max_access_breaks;
}

/* Set a data watchpoint.  ADDR and LEN should be obvious.  TYPE is 0
   for a write watchpoint, 1 for a read watchpoint, or 2 for a read/write
   watchpoint. */

int
m32r_insert_watchpoint (CORE_ADDR addr, int len, int type)
{
  int i;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_insert_watchpoint(%08lx,%d,%d)\n",
			addr, len, type);

  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    {
      if (ab_address[i] == 0x00000000)
	{
	  ab_address[i] = addr;
	  ab_size[i] = len;
	  ab_type[i] = type;
	  return 0;
	}
    }

  error (_("Too many watchpoints"));
  return 1;
}

int
m32r_remove_watchpoint (CORE_ADDR addr, int len, int type)
{
  int i;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_remove_watchpoint(%08lx,%d,%d)\n",
			addr, len, type);

  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    {
      if (ab_address[i] == addr)
	{
	  ab_address[i] = 0x00000000;
	  break;
	}
    }

  return 0;
}

int
m32r_stopped_data_address (struct target_ops *target, CORE_ADDR *addr_p)
{
  int rc = 0;
  if (hit_watchpoint_addr != 0x00000000)
    {
      *addr_p = hit_watchpoint_addr;
      rc = 1;
    }
  return rc;
}

int
m32r_stopped_by_watchpoint (void)
{
  CORE_ADDR addr;
  return m32r_stopped_data_address (&current_target, &addr);
}


static void
sdireset_command (char *args, int from_tty)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_sdireset()\n");

  send_cmd (SDI_OPEN);

  inferior_ptid = null_ptid;
}


static void
sdistatus_command (char *args, int from_tty)
{
  unsigned char buf[4096];
  int i, c;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_sdireset()\n");

  if (!sdi_desc)
    return;

  send_cmd (SDI_STATUS);
  for (i = 0; i < 4096; i++)
    {
      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
      if (c < 0)
	return;
      buf[i] = c;
      if (c == 0)
	break;
    }

  printf_filtered ("%s", buf);
}


static void
debug_chaos_command (char *args, int from_tty)
{
  unsigned char buf[3];

  buf[0] = SDI_SET_ATTR;
  buf[1] = SDI_ATTR_CACHE;
  buf[2] = SDI_CACHE_TYPE_CHAOS;
  send_data (buf, 3);
}


static void
use_debug_dma_command (char *args, int from_tty)
{
  unsigned char buf[3];

  buf[0] = SDI_SET_ATTR;
  buf[1] = SDI_ATTR_MEM_ACCESS;
  buf[2] = SDI_MEM_ACCESS_DEBUG_DMA;
  send_data (buf, 3);
}

static void
use_mon_code_command (char *args, int from_tty)
{
  unsigned char buf[3];

  buf[0] = SDI_SET_ATTR;
  buf[1] = SDI_ATTR_MEM_ACCESS;
  buf[2] = SDI_MEM_ACCESS_MON_CODE;
  send_data (buf, 3);
}


static void
use_ib_breakpoints_command (char *args, int from_tty)
{
  use_ib_breakpoints = 1;
}

static void
use_dbt_breakpoints_command (char *args, int from_tty)
{
  use_ib_breakpoints = 0;
}


/* Define the target subroutine names */

struct target_ops m32r_ops;

static void
init_m32r_ops (void)
{
  m32r_ops.to_shortname = "m32rsdi";
  m32r_ops.to_longname = "Remote M32R debugging over SDI interface";
  m32r_ops.to_doc = "Use an M32R board using SDI debugging protocol.";
  m32r_ops.to_open = m32r_open;
  m32r_ops.to_close = m32r_close;
  m32r_ops.to_detach = m32r_detach;
  m32r_ops.to_resume = m32r_resume;
  m32r_ops.to_wait = m32r_wait;
  m32r_ops.to_fetch_registers = m32r_fetch_register;
  m32r_ops.to_store_registers = m32r_store_register;
  m32r_ops.to_prepare_to_store = m32r_prepare_to_store;
  m32r_ops.deprecated_xfer_memory = m32r_xfer_memory;
  m32r_ops.to_files_info = m32r_files_info;
  m32r_ops.to_insert_breakpoint = m32r_insert_breakpoint;
  m32r_ops.to_remove_breakpoint = m32r_remove_breakpoint;
  m32r_ops.to_can_use_hw_breakpoint = m32r_can_use_hw_watchpoint;
  m32r_ops.to_insert_watchpoint = m32r_insert_watchpoint;
  m32r_ops.to_remove_watchpoint = m32r_remove_watchpoint;
  m32r_ops.to_stopped_by_watchpoint = m32r_stopped_by_watchpoint;
  m32r_ops.to_stopped_data_address = m32r_stopped_data_address;
  m32r_ops.to_kill = m32r_kill;
  m32r_ops.to_load = m32r_load;
  m32r_ops.to_create_inferior = m32r_create_inferior;
  m32r_ops.to_mourn_inferior = m32r_mourn_inferior;
  m32r_ops.to_stop = m32r_stop;
  m32r_ops.to_stratum = process_stratum;
  m32r_ops.to_has_all_memory = 1;
  m32r_ops.to_has_memory = 1;
  m32r_ops.to_has_stack = 1;
  m32r_ops.to_has_registers = 1;
  m32r_ops.to_has_execution = 1;
  m32r_ops.to_magic = OPS_MAGIC;
};


extern initialize_file_ftype _initialize_remote_m32r;

void
_initialize_remote_m32r (void)
{
  int i;

  init_m32r_ops ();

  /* Initialize breakpoints. */
  for (i = 0; i < MAX_BREAKPOINTS; i++)
    bp_address[i] = 0xffffffff;

  /* Initialize access breaks. */
  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    ab_address[i] = 0x00000000;

  add_target (&m32r_ops);

  add_com ("sdireset", class_obscure, sdireset_command,
	   _("Reset SDI connection."));

  add_com ("sdistatus", class_obscure, sdistatus_command,
	   _("Show status of SDI connection."));

  add_com ("debug_chaos", class_obscure, debug_chaos_command,
	   _("Debug M32R/Chaos."));

  add_com ("use_debug_dma", class_obscure, use_debug_dma_command,
	   _("Use debug DMA mem access."));
  add_com ("use_mon_code", class_obscure, use_mon_code_command,
	   _("Use mon code mem access."));

  add_com ("use_ib_break", class_obscure, use_ib_breakpoints_command,
	   _("Set breakpoints by IB break."));
  add_com ("use_dbt_break", class_obscure, use_dbt_breakpoints_command,
	   _("Set breakpoints by dbt."));
}
