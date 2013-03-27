/* m32r exception, interrupt, and trap (EIT) support
   Copyright (C) 1998, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Renesas.

   This file is part of GDB, the GNU debugger.

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

#include "sim-main.h"
#include "syscall.h"
#include "targ-vals.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <linux/sysctl.h>
#include <linux/types.h>
#include <linux/unistd.h>

#define TRAP_ELF_SYSCALL 0
#define TRAP_LINUX_SYSCALL 2
#define TRAP_FLUSH_CACHE 12

/* The semantic code invokes this for invalid (unrecognized) instructions.  */

SEM_PC
sim_engine_invalid_insn (SIM_CPU *current_cpu, IADDR cia, SEM_PC vpc)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

#if 0
  if (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)
    {
      h_bsm_set (current_cpu, h_sm_get (current_cpu));
      h_bie_set (current_cpu, h_ie_get (current_cpu));
      h_bcond_set (current_cpu, h_cond_get (current_cpu));
      /* sm not changed */
      h_ie_set (current_cpu, 0);
      h_cond_set (current_cpu, 0);

      h_bpc_set (current_cpu, cia);

      sim_engine_restart (CPU_STATE (current_cpu), current_cpu, NULL,
			  EIT_RSVD_INSN_ADDR);
    }
  else
#endif
    sim_engine_halt (sd, current_cpu, NULL, cia, sim_stopped, SIM_SIGILL);
  return vpc;
}

/* Process an address exception.  */

void
m32r_core_signal (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia,
		  unsigned int map, int nr_bytes, address_word addr,
		  transfer_type transfer, sim_core_signals sig)
{
  if (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)
    {
      m32rbf_h_cr_set (current_cpu, H_CR_BBPC,
                       m32rbf_h_cr_get (current_cpu, H_CR_BPC));
      if (MACH_NUM (CPU_MACH (current_cpu)) == MACH_M32R)
        {
          m32rbf_h_bpsw_set (current_cpu, m32rbf_h_psw_get (current_cpu));
          /* sm not changed */
          m32rbf_h_psw_set (current_cpu, m32rbf_h_psw_get (current_cpu) & 0x80);
        }
      else if (MACH_NUM (CPU_MACH (current_cpu)) == MACH_M32RX)
        {
          m32rxf_h_bpsw_set (current_cpu, m32rxf_h_psw_get (current_cpu));
          /* sm not changed */
          m32rxf_h_psw_set (current_cpu, m32rxf_h_psw_get (current_cpu) & 0x80);
        }
      else
        {
          m32r2f_h_bpsw_set (current_cpu, m32r2f_h_psw_get (current_cpu));
          /* sm not changed */
          m32r2f_h_psw_set (current_cpu, m32r2f_h_psw_get (current_cpu) & 0x80);
        }
      m32rbf_h_cr_set (current_cpu, H_CR_BPC, cia);

      sim_engine_restart (CPU_STATE (current_cpu), current_cpu, NULL,
                          EIT_ADDR_EXCP_ADDR);
    }
  else
    sim_core_signal (sd, current_cpu, cia, map, nr_bytes, addr,
                     transfer, sig);
}

/* Read/write functions for system call interface.  */

static int
syscall_read_mem (host_callback *cb, struct cb_syscall *sc,
		  unsigned long taddr, char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  return sim_core_read_buffer (sd, cpu, read_map, buf, taddr, bytes);
}

static int
syscall_write_mem (host_callback *cb, struct cb_syscall *sc,
		   unsigned long taddr, const char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  return sim_core_write_buffer (sd, cpu, write_map, buf, taddr, bytes);
}

/* Translate target's address to host's address.  */

static void *
t2h_addr (host_callback *cb, struct cb_syscall *sc,
          unsigned long taddr)
{
  extern sim_core_trans_addr (SIM_DESC, sim_cpu *, unsigned, address_word);
  void *addr;
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  if (taddr == 0)
    return NULL;

  return sim_core_trans_addr (sd, cpu, read_map, taddr);
}

static unsigned int
conv_endian (unsigned int tvalue)
{
  unsigned int hvalue;
  unsigned int t1, t2, t3, t4;

  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      t1 = tvalue & 0xff000000;
      t2 = tvalue & 0x00ff0000;
      t3 = tvalue & 0x0000ff00;
      t4 = tvalue & 0x000000ff;

      hvalue =  t1 >> 24;
      hvalue += t2 >> 8;
      hvalue += t3 << 8;
      hvalue += t4 << 24;
    }
  else
    hvalue = tvalue;

  return hvalue;
}

static unsigned short
conv_endian16 (unsigned short tvalue)
{
  unsigned short hvalue;
  unsigned short t1, t2;

  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      t1 = tvalue & 0xff00;
      t2 = tvalue & 0x00ff;

      hvalue =  t1 >> 8;
      hvalue += t2 << 8;
    }
  else
    hvalue = tvalue;

  return hvalue;
}

static void
translate_endian(void *addr, size_t size)
{
  unsigned int *p = (unsigned int *) addr;
  int i;
  
  for (i = 0; i <= size - 4; i += 4,p++)
    *p = conv_endian(*p);
  
  if (i <= size - 2)
    *((unsigned short *) p) = conv_endian16(*((unsigned short *) p));
}

/* Trap support.
   The result is the pc address to continue at.
   Preprocessing like saving the various registers has already been done.  */

USI
m32r_trap (SIM_CPU *current_cpu, PCADDR pc, int num)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  host_callback *cb = STATE_CALLBACK (sd);

#ifdef SIM_HAVE_BREAKPOINTS
  /* Check for breakpoints "owned" by the simulator first, regardless
     of --environment.  */
  if (num == TRAP_BREAKPOINT)
    {
      /* First try sim-break.c.  If it's a breakpoint the simulator "owns"
	 it doesn't return.  Otherwise it returns and let's us try.  */
      sim_handle_breakpoint (sd, current_cpu, pc);
      /* Fall through.  */
    }
#endif

  switch (num)
    {
    case TRAP_ELF_SYSCALL :
      {
        CB_SYSCALL s;
 
        CB_SYSCALL_INIT (&s);
        s.func = m32rbf_h_gr_get (current_cpu, 0);
        s.arg1 = m32rbf_h_gr_get (current_cpu, 1);
        s.arg2 = m32rbf_h_gr_get (current_cpu, 2);
        s.arg3 = m32rbf_h_gr_get (current_cpu, 3);
 
        if (s.func == TARGET_SYS_exit)
          {
            sim_engine_halt (sd, current_cpu, NULL, pc, sim_exited, s.arg1);
          }
 
        s.p1 = (PTR) sd;
        s.p2 = (PTR) current_cpu;
        s.read_mem = syscall_read_mem;
        s.write_mem = syscall_write_mem;
        cb_syscall (cb, &s);
        m32rbf_h_gr_set (current_cpu, 2, s.errcode);
        m32rbf_h_gr_set (current_cpu, 0, s.result);
        m32rbf_h_gr_set (current_cpu, 1, s.result2);
        break;
      }

    case TRAP_LINUX_SYSCALL :
      {
	CB_SYSCALL s;
        unsigned int func, arg1, arg2, arg3, arg4, arg5, arg6, arg7;
        int result, result2, errcode;

        if (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)
          {
            /* The new pc is the trap vector entry.
               We assume there's a branch there to some handler.
	       Use cr5 as EVB (EIT Vector Base) register.  */
            USI new_pc = m32rbf_h_cr_get (current_cpu, 5) + 0x40 + num * 4;
            return new_pc;
          }

	func = m32rbf_h_gr_get (current_cpu, 7);
	arg1 = m32rbf_h_gr_get (current_cpu, 0);
	arg2 = m32rbf_h_gr_get (current_cpu, 1);
	arg3 = m32rbf_h_gr_get (current_cpu, 2);
	arg4 = m32rbf_h_gr_get (current_cpu, 3);
	arg5 = m32rbf_h_gr_get (current_cpu, 4);
	arg6 = m32rbf_h_gr_get (current_cpu, 5);
	arg7 = m32rbf_h_gr_get (current_cpu, 6);

        CB_SYSCALL_INIT (&s);
        s.func = func;
        s.arg1 = arg1;
        s.arg2 = arg2;
        s.arg3 = arg3;

        s.p1 = (PTR) sd;
        s.p2 = (PTR) current_cpu;
        s.read_mem = syscall_read_mem;
        s.write_mem = syscall_write_mem;

        result = 0;
        result2 = 0;
        errcode = 0;

        switch (func)
          {
          case __NR_exit:
	    sim_engine_halt (sd, current_cpu, NULL, pc, sim_exited, arg1);
            break;

          case __NR_read:
            result = read(arg1, t2h_addr(cb, &s, arg2), arg3);
            errcode = errno;
            break;

          case __NR_write:
            result = write(arg1, t2h_addr(cb, &s, arg2), arg3);
            errcode = errno;
            break;

          case __NR_open:
            result = open((char *) t2h_addr(cb, &s, arg1), arg2, arg3);
            errcode = errno;
            break;

          case __NR_close:
            result = close(arg1);
            errcode = errno;
            break;

          case __NR_creat:
            result = creat((char *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_link:
            result = link((char *) t2h_addr(cb, &s, arg1),
                          (char *) t2h_addr(cb, &s, arg2));
            errcode = errno;
            break;

          case __NR_unlink:
            result = unlink((char *) t2h_addr(cb, &s, arg1));
            errcode = errno;
            break;

          case __NR_chdir:
            result = chdir((char *) t2h_addr(cb, &s, arg1));
            errcode = errno;
            break;

          case __NR_time:
            {
              time_t t;

              if (arg1 == 0)
                {
                  result = (int) time(NULL);
                  errcode = errno;
                }
              else
                {
                  result = (int) time(&t);
                  errcode = errno;

                  if (result != 0)
                    break;

                  translate_endian((void *) &t, sizeof(t));
                  if ((s.write_mem) (cb, &s, arg1, (char *) &t, sizeof(t)) != sizeof(t))
                    {
                      result = -1;
                      errcode = EINVAL;
                    }
                }
            }
            break;

          case __NR_mknod:
            result = mknod((char *) t2h_addr(cb, &s, arg1),
                           (mode_t) arg2, (dev_t) arg3);
            errcode = errno;
            break;

          case __NR_chmod:
            result = chmod((char *) t2h_addr(cb, &s, arg1), (mode_t) arg2);
            errcode = errno;
            break;

          case __NR_lchown32:
          case __NR_lchown:
            result = lchown((char *) t2h_addr(cb, &s, arg1),
                            (uid_t) arg2, (gid_t) arg3);
            errcode = errno;
            break;

          case __NR_lseek:
            result = (int) lseek(arg1, (off_t) arg2, arg3);
            errcode = errno;
            break;

          case __NR_getpid:
            result = getpid();
            errcode = errno;
            break;

          case __NR_getuid32:
          case __NR_getuid:
            result = getuid();
            errcode = errno;
            break;

          case __NR_utime:
            {
              struct utimbuf buf;

              if (arg2 == 0)
                {
                  result = utime((char *) t2h_addr(cb, &s, arg1), NULL);
                  errcode = errno;
                }
              else
                {
                  buf = *((struct utimbuf *) t2h_addr(cb, &s, arg2));
                  translate_endian((void *) &buf, sizeof(buf));
                  result = utime((char *) t2h_addr(cb, &s, arg1), &buf);
                  errcode = errno;
                }
            }
            break;

          case __NR_access:
            result = access((char *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_ftime:
            {
              struct timeb t;

              result = ftime(&t);
              errcode = errno;

              if (result != 0)
                break;

              t.time = conv_endian(t.time);
              t.millitm = conv_endian16(t.millitm);
              t.timezone = conv_endian16(t.timezone);
              t.dstflag = conv_endian16(t.dstflag);
              if ((s.write_mem) (cb, &s, arg1, (char *) &t, sizeof(t))
                  != sizeof(t))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }

          case __NR_sync:
            sync();
            result = 0;
            break;

          case __NR_rename:
            result = rename((char *) t2h_addr(cb, &s, arg1),
                            (char *) t2h_addr(cb, &s, arg2));
            errcode = errno;
            break;

          case __NR_mkdir:
            result = mkdir((char *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_rmdir:
            result = rmdir((char *) t2h_addr(cb, &s, arg1));
            errcode = errno;
            break;

          case __NR_dup:
            result = dup(arg1);
            errcode = errno;
            break;

          case __NR_brk:
            result = brk((void *) arg1);
            errcode = errno;
            //result = arg1;
            break;

          case __NR_getgid32:
          case __NR_getgid:
            result = getgid();
            errcode = errno;
            break;

          case __NR_geteuid32:
          case __NR_geteuid:
            result = geteuid();
            errcode = errno;
            break;

          case __NR_getegid32:
          case __NR_getegid:
            result = getegid();
            errcode = errno;
            break;

          case __NR_ioctl:
            result = ioctl(arg1, arg2, arg3);
            errcode = errno;
            break;

          case __NR_fcntl:
            result = fcntl(arg1, arg2, arg3);
            errcode = errno;
            break;

          case __NR_dup2:
            result = dup2(arg1, arg2);
            errcode = errno;
            break;

          case __NR_getppid:
            result = getppid();
            errcode = errno;
            break;

          case __NR_getpgrp:
            result = getpgrp();
            errcode = errno;
            break;

          case __NR_getrlimit:
            {
              struct rlimit rlim;

              result = getrlimit(arg1, &rlim);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &rlim, sizeof(rlim));
              if ((s.write_mem) (cb, &s, arg2, (char *) &rlim, sizeof(rlim))
                  != sizeof(rlim))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_getrusage:
            {
              struct rusage usage;

              result = getrusage(arg1, &usage);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &usage, sizeof(usage));
              if ((s.write_mem) (cb, &s, arg2, (char *) &usage, sizeof(usage))
                  != sizeof(usage))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_gettimeofday:
            {
              struct timeval tv;
              struct timezone tz;
              
              result = gettimeofday(&tv, &tz);
              errcode = errno;
              
              if (result != 0)
                break;

              translate_endian((void *) &tv, sizeof(tv));
              if ((s.write_mem) (cb, &s, arg1, (char *) &tv, sizeof(tv))
                  != sizeof(tv))
                {
                  result = -1;
                  errcode = EINVAL;
                }

              translate_endian((void *) &tz, sizeof(tz));
              if ((s.write_mem) (cb, &s, arg2, (char *) &tz, sizeof(tz))
                  != sizeof(tz))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_getgroups32:
          case __NR_getgroups:
            {
              gid_t *list;

              if (arg1 > 0)
                list = (gid_t *) malloc(arg1 * sizeof(gid_t));

              result = getgroups(arg1, list);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) list, arg1 * sizeof(gid_t));
              if (arg1 > 0)
                if ((s.write_mem) (cb, &s, arg2, (char *) list, arg1 * sizeof(gid_t))
                    != arg1 * sizeof(gid_t))
                  {
                    result = -1;
                     errcode = EINVAL;
                  }
            }
            break;

          case __NR_select:
            {
              int n;
              fd_set readfds;
              fd_set *treadfdsp;
              fd_set *hreadfdsp;
              fd_set writefds;
              fd_set *twritefdsp;
              fd_set *hwritefdsp;
              fd_set exceptfds;
              fd_set *texceptfdsp;
              fd_set *hexceptfdsp;
              struct timeval *ttimeoutp;
              struct timeval timeout;
              
              n = arg1;

              treadfdsp = (fd_set *) arg2;
              if (treadfdsp != NULL)
                {
                  readfds = *((fd_set *) t2h_addr(cb, &s, (unsigned int) treadfdsp));
                  translate_endian((void *) &readfds, sizeof(readfds));
                  hreadfdsp = &readfds;
                }
              else
                hreadfdsp = NULL;
              
              twritefdsp  = (fd_set *) arg3;
              if (twritefdsp != NULL)
                {
                  writefds = *((fd_set *) t2h_addr(cb, &s, (unsigned int) twritefdsp));
                  translate_endian((void *) &writefds, sizeof(writefds));
                  hwritefdsp = &writefds;
                }
              else
                hwritefdsp = NULL;
              
              texceptfdsp = (fd_set *) arg4;
              if (texceptfdsp != NULL)
                {
                  exceptfds = *((fd_set *) t2h_addr(cb, &s, (unsigned int) texceptfdsp));
                  translate_endian((void *) &exceptfds, sizeof(exceptfds));
                  hexceptfdsp = &exceptfds;
                }
              else
                hexceptfdsp = NULL;
              
              ttimeoutp = (struct timeval *) arg5;
              timeout = *((struct timeval *) t2h_addr(cb, &s, (unsigned int) ttimeoutp));
              translate_endian((void *) &timeout, sizeof(timeout));

              result = select(n, hreadfdsp, hwritefdsp, hexceptfdsp, &timeout);
              errcode = errno;

              if (result != 0)
                break;

              if (treadfdsp != NULL)
                {
                  translate_endian((void *) &readfds, sizeof(readfds));
                  if ((s.write_mem) (cb, &s, (unsigned long) treadfdsp,
                       (char *) &readfds, sizeof(readfds)) != sizeof(readfds))
                    {
                      result = -1;
                      errcode = EINVAL;
                    }
                }

              if (twritefdsp != NULL)
                {
                  translate_endian((void *) &writefds, sizeof(writefds));
                  if ((s.write_mem) (cb, &s, (unsigned long) twritefdsp,
                       (char *) &writefds, sizeof(writefds)) != sizeof(writefds))
                    {
                      result = -1;
                      errcode = EINVAL;
                    }
                }

              if (texceptfdsp != NULL)
                {
                  translate_endian((void *) &exceptfds, sizeof(exceptfds));
                  if ((s.write_mem) (cb, &s, (unsigned long) texceptfdsp,
                       (char *) &exceptfds, sizeof(exceptfds)) != sizeof(exceptfds))
                    {
                      result = -1;
                      errcode = EINVAL;
                    }
                }

              translate_endian((void *) &timeout, sizeof(timeout));
              if ((s.write_mem) (cb, &s, (unsigned long) ttimeoutp,
                   (char *) &timeout, sizeof(timeout)) != sizeof(timeout))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_symlink:
            result = symlink((char *) t2h_addr(cb, &s, arg1),
                             (char *) t2h_addr(cb, &s, arg2));
            errcode = errno;
            break;

          case __NR_readlink:
            result = readlink((char *) t2h_addr(cb, &s, arg1),
                              (char *) t2h_addr(cb, &s, arg2),
                              arg3);
            errcode = errno;
            break;

          case __NR_readdir:
            result = (int) readdir((DIR *) t2h_addr(cb, &s, arg1));
            errcode = errno;
            break;

#if 0
          case __NR_mmap:
            {
              result = (int) mmap((void *) t2h_addr(cb, &s, arg1),
                                  arg2, arg3, arg4, arg5, arg6);
              errcode = errno;

              if (errno == 0)
                {
                  sim_core_attach (sd, NULL,
                                   0, access_read_write_exec, 0,
                                   result, arg2, 0, NULL, NULL);
                }
            }
            break;
#endif
          case __NR_mmap2:
            {
              void *addr;
              size_t len;
              int prot, flags, fildes;
              off_t off;
              
              addr   = (void *)  t2h_addr(cb, &s, arg1);
              len    = arg2;
              prot   = arg3;
              flags  = arg4;
              fildes = arg5;
              off    = arg6 << 12;

	      result = (int) mmap(addr, len, prot, flags, fildes, off);
              errcode = errno;
              if (result != -1)
                {
                  char c;
		  if (sim_core_read_buffer (sd, NULL, read_map, &c, result, 1) == 0)
                    sim_core_attach (sd, NULL,
                                     0, access_read_write_exec, 0,
                                     result, len, 0, NULL, NULL);
                }
            }
            break;

          case __NR_mmap:
            {
              void *addr;
              size_t len;
              int prot, flags, fildes;
              off_t off;
              
              addr   = *((void **)  t2h_addr(cb, &s, arg1));
              len    = *((size_t *) t2h_addr(cb, &s, arg1 + 4));
              prot   = *((int *)    t2h_addr(cb, &s, arg1 + 8));
              flags  = *((int *)    t2h_addr(cb, &s, arg1 + 12));
              fildes = *((int *)    t2h_addr(cb, &s, arg1 + 16));
              off    = *((off_t *)  t2h_addr(cb, &s, arg1 + 20));

              addr   = (void *) conv_endian((unsigned int) addr);
              len    = conv_endian(len);
              prot   = conv_endian(prot);
              flags  = conv_endian(flags);
              fildes = conv_endian(fildes);
              off    = conv_endian(off);

              //addr   = (void *) t2h_addr(cb, &s, (unsigned int) addr);
              result = (int) mmap(addr, len, prot, flags, fildes, off);
              errcode = errno;

              //if (errno == 0)
              if (result != -1)
                {
                  char c;
		  if (sim_core_read_buffer (sd, NULL, read_map, &c, result, 1) == 0)
                    sim_core_attach (sd, NULL,
                                     0, access_read_write_exec, 0,
                                     result, len, 0, NULL, NULL);
                }
            }
            break;

          case __NR_munmap:
            {
            result = munmap((void *)arg1, arg2);
            errcode = errno;
            if (result != -1)
              {
                sim_core_detach (sd, NULL, 0, arg2, result);
              }
            }
            break;

          case __NR_truncate:
            result = truncate((char *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_ftruncate:
            result = ftruncate(arg1, arg2);
            errcode = errno;
            break;

          case __NR_fchmod:
            result = fchmod(arg1, arg2);
            errcode = errno;
            break;

          case __NR_fchown32:
          case __NR_fchown:
            result = fchown(arg1, arg2, arg3);
            errcode = errno;
            break;

          case __NR_statfs:
            {
              struct statfs statbuf;

              result = statfs((char *) t2h_addr(cb, &s, arg1), &statbuf);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &statbuf, sizeof(statbuf));
              if ((s.write_mem) (cb, &s, arg2, (char *) &statbuf, sizeof(statbuf))
                  != sizeof(statbuf))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_fstatfs:
            {
              struct statfs statbuf;

              result = fstatfs(arg1, &statbuf);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &statbuf, sizeof(statbuf));
              if ((s.write_mem) (cb, &s, arg2, (char *) &statbuf, sizeof(statbuf))
                  != sizeof(statbuf))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_syslog:
            result = syslog(arg1, (char *) t2h_addr(cb, &s, arg2));
            errcode = errno;
            break;

          case __NR_setitimer:
            {
              struct itimerval value, ovalue;

              value = *((struct itimerval *) t2h_addr(cb, &s, arg2));
              translate_endian((void *) &value, sizeof(value));

              if (arg2 == 0)
                {
                  result = setitimer(arg1, &value, NULL);
                  errcode = errno;
                }
              else
                {
                  result = setitimer(arg1, &value, &ovalue);
                  errcode = errno;

                  if (result != 0)
                    break;

                  translate_endian((void *) &ovalue, sizeof(ovalue));
                  if ((s.write_mem) (cb, &s, arg3, (char *) &ovalue, sizeof(ovalue))
                      != sizeof(ovalue))
                    {
                      result = -1;
                      errcode = EINVAL;
                    }
                }
            }
            break;

          case __NR_getitimer:
            {
              struct itimerval value;

              result = getitimer(arg1, &value);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &value, sizeof(value));
              if ((s.write_mem) (cb, &s, arg2, (char *) &value, sizeof(value))
                  != sizeof(value))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_stat:
            {
              char *buf;
              int buflen;
              struct stat statbuf;

              result = stat((char *) t2h_addr(cb, &s, arg1), &statbuf);
              errcode = errno;
              if (result < 0)
                break;

              buflen = cb_host_to_target_stat (cb, NULL, NULL);
              buf = xmalloc (buflen);
              if (cb_host_to_target_stat (cb, &statbuf, buf) != buflen)
                {
                  /* The translation failed.  This is due to an internal
                     host program error, not the target's fault.  */
                  free (buf);
                  result = -1;
                  errcode = ENOSYS;
                  break;
                }
              if ((s.write_mem) (cb, &s, arg2, buf, buflen) != buflen)
                {
                  free (buf);
                  result = -1;
                  errcode = EINVAL;
                  break;
                }
              free (buf);
            }
            break;

          case __NR_lstat:
            {
              char *buf;
              int buflen;
              struct stat statbuf;

              result = lstat((char *) t2h_addr(cb, &s, arg1), &statbuf);
              errcode = errno;
              if (result < 0)
                break;

              buflen = cb_host_to_target_stat (cb, NULL, NULL);
              buf = xmalloc (buflen);
              if (cb_host_to_target_stat (cb, &statbuf, buf) != buflen)
                {
                  /* The translation failed.  This is due to an internal
                     host program error, not the target's fault.  */
                  free (buf);
                  result = -1;
                  errcode = ENOSYS;
                  break;
                }
              if ((s.write_mem) (cb, &s, arg2, buf, buflen) != buflen)
                {
                  free (buf);
                  result = -1;
                  errcode = EINVAL;
                  break;
                }
              free (buf);
            }
            break;

          case __NR_fstat:
            {
              char *buf;
              int buflen;
              struct stat statbuf;

              result = fstat(arg1, &statbuf);
              errcode = errno;
              if (result < 0)
                break;

              buflen = cb_host_to_target_stat (cb, NULL, NULL);
              buf = xmalloc (buflen);
              if (cb_host_to_target_stat (cb, &statbuf, buf) != buflen)
                {
                  /* The translation failed.  This is due to an internal
                     host program error, not the target's fault.  */
                  free (buf);
                  result = -1;
                  errcode = ENOSYS;
                  break;
                }
              if ((s.write_mem) (cb, &s, arg2, buf, buflen) != buflen)
                {
                  free (buf);
                  result = -1;
                  errcode = EINVAL;
                  break;
                }
              free (buf);
            }
            break;

          case __NR_sysinfo:
            {
              struct sysinfo info;

              result = sysinfo(&info);
              errcode = errno;

              if (result != 0)
                break;

              info.uptime    = conv_endian(info.uptime);
              info.loads[0]  = conv_endian(info.loads[0]);
              info.loads[1]  = conv_endian(info.loads[1]);
              info.loads[2]  = conv_endian(info.loads[2]);
              info.totalram  = conv_endian(info.totalram);
              info.freeram   = conv_endian(info.freeram);
              info.sharedram = conv_endian(info.sharedram);
              info.bufferram = conv_endian(info.bufferram);
              info.totalswap = conv_endian(info.totalswap);
              info.freeswap  = conv_endian(info.freeswap);
              info.procs     = conv_endian16(info.procs);
#if LINUX_VERSION_CODE >= 0x20400
              info.totalhigh = conv_endian(info.totalhigh);
              info.freehigh  = conv_endian(info.freehigh);
              info.mem_unit  = conv_endian(info.mem_unit);
#endif
              if ((s.write_mem) (cb, &s, arg1, (char *) &info, sizeof(info))
                  != sizeof(info))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

#if 0
          case __NR_ipc:
            {
              result = ipc(arg1, arg2, arg3, arg4,
                           (void *) t2h_addr(cb, &s, arg5), arg6);
              errcode = errno;
            }
            break;
#endif

          case __NR_fsync:
            result = fsync(arg1);
            errcode = errno;
            break;

          case __NR_uname:
            /* utsname contains only arrays of char, so it is not necessary
               to translate endian. */
            result = uname((struct utsname *) t2h_addr(cb, &s, arg1));
            errcode = errno;
            break;

          case __NR_adjtimex:
            {
              struct timex buf;

              result = adjtimex(&buf);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &buf, sizeof(buf));
              if ((s.write_mem) (cb, &s, arg1, (char *) &buf, sizeof(buf))
                  != sizeof(buf))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_mprotect:
            result = mprotect((void *) arg1, arg2, arg3);
            errcode = errno;
            break;

          case __NR_fchdir:
            result = fchdir(arg1);
            errcode = errno;
            break;

          case __NR_setfsuid32:
          case __NR_setfsuid:
            result = setfsuid(arg1);
            errcode = errno;
            break;

          case __NR_setfsgid32:
          case __NR_setfsgid:
            result = setfsgid(arg1);
            errcode = errno;
            break;

#if 0
          case __NR__llseek:
            {
              loff_t buf;

              result = _llseek(arg1, arg2, arg3, &buf, arg5);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &buf, sizeof(buf));
              if ((s.write_mem) (cb, &s, t2h_addr(cb, &s, arg4),
                                 (char *) &buf, sizeof(buf)) != sizeof(buf))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_getdents:
            {
              struct dirent dir;

              result = getdents(arg1, &dir, arg3);
              errcode = errno;

              if (result != 0)
                break;

              dir.d_ino = conv_endian(dir.d_ino);
              dir.d_off = conv_endian(dir.d_off);
              dir.d_reclen = conv_endian16(dir.d_reclen);
              if ((s.write_mem) (cb, &s, arg2, (char *) &dir, sizeof(dir))
                  != sizeof(dir))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;
#endif

          case __NR_flock:
            result = flock(arg1, arg2);
            errcode = errno;
            break;

          case __NR_msync:
            result = msync((void *) arg1, arg2, arg3);
            errcode = errno;
            break;

          case __NR_readv:
            {
              struct iovec vector;

              vector = *((struct iovec *) t2h_addr(cb, &s, arg2));
              translate_endian((void *) &vector, sizeof(vector));

              result = readv(arg1, &vector, arg3);
              errcode = errno;
            }
            break;

          case __NR_writev:
            {
              struct iovec vector;

              vector = *((struct iovec *) t2h_addr(cb, &s, arg2));
              translate_endian((void *) &vector, sizeof(vector));

              result = writev(arg1, &vector, arg3);
              errcode = errno;
            }
            break;

          case __NR_fdatasync:
            result = fdatasync(arg1);
            errcode = errno;
            break;

          case __NR_mlock:
            result = mlock((void *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_munlock:
            result = munlock((void *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_nanosleep:
            {
              struct timespec req, rem;

              req = *((struct timespec *) t2h_addr(cb, &s, arg2));
              translate_endian((void *) &req, sizeof(req));

              result = nanosleep(&req, &rem);
              errcode = errno;

              if (result != 0)
                break;

              translate_endian((void *) &rem, sizeof(rem));
              if ((s.write_mem) (cb, &s, arg2, (char *) &rem, sizeof(rem))
                  != sizeof(rem))
                {
                  result = -1;
                  errcode = EINVAL;
                }
            }
            break;

          case __NR_mremap: /* FIXME */
            result = (int) mremap((void *) t2h_addr(cb, &s, arg1), arg2, arg3, arg4); 
            errcode = errno;
            break;

          case __NR_getresuid32:
          case __NR_getresuid:
            {
              uid_t ruid, euid, suid;

              result = getresuid(&ruid, &euid, &suid);
              errcode = errno;

              if (result != 0)
                break;

              *((uid_t *) t2h_addr(cb, &s, arg1)) = conv_endian(ruid);
              *((uid_t *) t2h_addr(cb, &s, arg2)) = conv_endian(euid);
              *((uid_t *) t2h_addr(cb, &s, arg3)) = conv_endian(suid);
            }
            break;

          case __NR_poll:
            {
              struct pollfd ufds;

              ufds = *((struct pollfd *) t2h_addr(cb, &s, arg1));
              ufds.fd = conv_endian(ufds.fd);
              ufds.events = conv_endian16(ufds.events);
              ufds.revents = conv_endian16(ufds.revents);

              result = poll(&ufds, arg2, arg3);
              errcode = errno;
            }
            break;

          case __NR_getresgid32:
          case __NR_getresgid:
            {
              uid_t rgid, egid, sgid;

              result = getresgid(&rgid, &egid, &sgid);
              errcode = errno;

              if (result != 0)
                break;

              *((uid_t *) t2h_addr(cb, &s, arg1)) = conv_endian(rgid);
              *((uid_t *) t2h_addr(cb, &s, arg2)) = conv_endian(egid);
              *((uid_t *) t2h_addr(cb, &s, arg3)) = conv_endian(sgid);
            }
            break;

          case __NR_pread:
            result =  pread(arg1, (void *) t2h_addr(cb, &s, arg2), arg3, arg4); 
            errcode = errno;
            break;

          case __NR_pwrite:
            result =  pwrite(arg1, (void *) t2h_addr(cb, &s, arg2), arg3, arg4); 
            errcode = errno;
            break;

          case __NR_chown32:
          case __NR_chown:
            result = chown((char *) t2h_addr(cb, &s, arg1), arg2, arg3);
            errcode = errno;
            break;

          case __NR_getcwd:
            result = (int) getcwd((char *) t2h_addr(cb, &s, arg1), arg2);
            errcode = errno;
            break;

          case __NR_sendfile:
            {
              off_t offset;

              offset = *((off_t *) t2h_addr(cb, &s, arg3));
              offset = conv_endian(offset);

              result = sendfile(arg1, arg2, &offset, arg3);
              errcode = errno;

              if (result != 0)
                break;

              *((off_t *) t2h_addr(cb, &s, arg3)) = conv_endian(offset);
            }
            break;

          default:
            result = -1;
            errcode = ENOSYS;
            break;
          }
        
        if (result == -1)
	  m32rbf_h_gr_set (current_cpu, 0, -errcode);
        else
	  m32rbf_h_gr_set (current_cpu, 0, result);
	break;
      }

    case TRAP_BREAKPOINT:
      sim_engine_halt (sd, current_cpu, NULL, pc,
		       sim_stopped, SIM_SIGTRAP);
      break;

    case TRAP_FLUSH_CACHE:
      /* Do nothing.  */
      break;

    default :
      {
	/* Use cr5 as EVB (EIT Vector Base) register.  */
        USI new_pc = m32rbf_h_cr_get (current_cpu, 5) + 0x40 + num * 4;
	return new_pc;
      }
    }

  /* Fake an "rte" insn.  */
  /* FIXME: Should duplicate all of rte processing.  */
  return (pc & -4) + 4;
}
