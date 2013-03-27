/* Target-vector operations for controlling win32 child processes, for GDB.

   Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007 Free Software Foundation, Inc.

   Contributed by Cygnus Solutions, A Red Hat Company.

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

/* Originally by Steve Chamberlain, sac@cygnus.com */

/* We assume we're being built with and will be used for cygwin.  */

#include "defs.h"
#include "frame.h"		/* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "exceptions.h"
#include "gdbcore.h"
#include "command.h"
#include "completer.h"
#include "regcache.h"
#include "top.h"
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <windows.h>
#include <imagehlp.h>
#include <sys/cygwin.h>
#include <signal.h>

#include "buildsym.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_obstack.h"
#include "gdb_string.h"
#include "gdbthread.h"
#include "gdbcmd.h"
#include <sys/param.h>
#include <unistd.h>
#include "exec.h"
#include "solist.h"
#include "solib.h"
#include "xml-support.h"

#include "i386-tdep.h"
#include "i387-tdep.h"

#include "i386-cygwin-tdep.h"

static struct target_ops win32_ops;

/* The starting and ending address of the cygwin1.dll text segment. */
static bfd_vma cygwin_load_start;
static bfd_vma cygwin_load_end;

static int have_saved_context;	/* True if we've saved context from a cygwin signal. */
static CONTEXT saved_context;	/* Containes the saved context from a cygwin signal. */

/* If we're not using the old Cygwin header file set, define the
   following which never should have been in the generic Win32 API
   headers in the first place since they were our own invention... */
#ifndef _GNU_H_WINDOWS_H
enum
  {
    FLAG_TRACE_BIT = 0x100,
    CONTEXT_DEBUGGER = (CONTEXT_FULL | CONTEXT_FLOATING_POINT)
  };
#endif
#include <sys/procfs.h>
#include <psapi.h>

#define CONTEXT_DEBUGGER_DR CONTEXT_DEBUGGER | CONTEXT_DEBUG_REGISTERS \
	| CONTEXT_EXTENDED_REGISTERS

static unsigned dr[8];
static int debug_registers_changed;
static int debug_registers_used;

/* The string sent by cygwin when it processes a signal.
   FIXME: This should be in a cygwin include file. */
#ifndef _CYGWIN_SIGNAL_STRING
#define _CYGWIN_SIGNAL_STRING "cYgSiGw00f"
#endif

#define CHECK(x)	check (x, __FILE__,__LINE__)
#define DEBUG_EXEC(x)	if (debug_exec)		printf_unfiltered x
#define DEBUG_EVENTS(x)	if (debug_events)	printf_unfiltered x
#define DEBUG_MEM(x)	if (debug_memory)	printf_unfiltered x
#define DEBUG_EXCEPT(x)	if (debug_exceptions)	printf_unfiltered x

static void win32_stop (void);
static int win32_win32_thread_alive (ptid_t);
static void win32_kill_inferior (void);

static enum target_signal last_sig = TARGET_SIGNAL_0;
/* Set if a signal was received from the debugged process */

/* Thread information structure used to track information that is
   not available in gdb's thread structure. */
typedef struct thread_info_struct
  {
    struct thread_info_struct *next;
    DWORD id;
    HANDLE h;
    char *name;
    int suspend_count;
    int reload_context;
    CONTEXT context;
    STACKFRAME sf;
  }
thread_info;

static thread_info thread_head;

/* The process and thread handles for the above context. */

static DEBUG_EVENT current_event;	/* The current debug event from
					   WaitForDebugEvent */
static HANDLE current_process_handle;	/* Currently executing process */
static thread_info *current_thread;	/* Info on currently selected thread */
static DWORD main_thread_id;		/* Thread ID of the main thread */

/* Counts of things. */
static int exception_count = 0;
static int event_count = 0;
static int saw_create;

/* User options. */
static int new_console = 0;
static int cygwin_exceptions = 0;
static int new_group = 1;
static int debug_exec = 0;		/* show execution */
static int debug_events = 0;		/* show events from kernel */
static int debug_memory = 0;		/* show target memory accesses */
static int debug_exceptions = 0;	/* show target exceptions */
static int useshell = 0;		/* use shell for subprocesses */

/* This vector maps GDB's idea of a register's number into an address
   in the win32 exception context vector.

   It also contains the bit mask needed to load the register in question.

   One day we could read a reg, we could inspect the context we
   already have loaded, if it doesn't have the bit set that we need,
   we read that set of registers in using GetThreadContext.  If the
   context already contains what we need, we just unpack it. Then to
   write a register, first we have to ensure that the context contains
   the other regs of the group, and then we copy the info in and set
   out bit. */

#define context_offset(x) ((int)&(((CONTEXT *)NULL)->x))
static const int mappings[] =
{
  context_offset (Eax),
  context_offset (Ecx),
  context_offset (Edx),
  context_offset (Ebx),
  context_offset (Esp),
  context_offset (Ebp),
  context_offset (Esi),
  context_offset (Edi),
  context_offset (Eip),
  context_offset (EFlags),
  context_offset (SegCs),
  context_offset (SegSs),
  context_offset (SegDs),
  context_offset (SegEs),
  context_offset (SegFs),
  context_offset (SegGs),
  context_offset (FloatSave.RegisterArea[0 * 10]),
  context_offset (FloatSave.RegisterArea[1 * 10]),
  context_offset (FloatSave.RegisterArea[2 * 10]),
  context_offset (FloatSave.RegisterArea[3 * 10]),
  context_offset (FloatSave.RegisterArea[4 * 10]),
  context_offset (FloatSave.RegisterArea[5 * 10]),
  context_offset (FloatSave.RegisterArea[6 * 10]),
  context_offset (FloatSave.RegisterArea[7 * 10]),
  context_offset (FloatSave.ControlWord),
  context_offset (FloatSave.StatusWord),
  context_offset (FloatSave.TagWord),
  context_offset (FloatSave.ErrorSelector),
  context_offset (FloatSave.ErrorOffset),
  context_offset (FloatSave.DataSelector),
  context_offset (FloatSave.DataOffset),
  context_offset (FloatSave.ErrorSelector)
  /* XMM0-7 */ ,
  context_offset (ExtendedRegisters[10*16]),
  context_offset (ExtendedRegisters[11*16]),
  context_offset (ExtendedRegisters[12*16]),
  context_offset (ExtendedRegisters[13*16]),
  context_offset (ExtendedRegisters[14*16]),
  context_offset (ExtendedRegisters[15*16]),
  context_offset (ExtendedRegisters[16*16]),
  context_offset (ExtendedRegisters[17*16]),
  /* MXCSR */
  context_offset (ExtendedRegisters[24])
};

#undef context_offset

/* This vector maps the target's idea of an exception (extracted
   from the DEBUG_EVENT structure) to GDB's idea. */

struct xlate_exception
  {
    int them;
    enum target_signal us;
  };

static const struct xlate_exception
  xlate[] =
{
  {EXCEPTION_ACCESS_VIOLATION, TARGET_SIGNAL_SEGV},
  {STATUS_STACK_OVERFLOW, TARGET_SIGNAL_SEGV},
  {EXCEPTION_BREAKPOINT, TARGET_SIGNAL_TRAP},
  {DBG_CONTROL_C, TARGET_SIGNAL_INT},
  {EXCEPTION_SINGLE_STEP, TARGET_SIGNAL_TRAP},
  {STATUS_FLOAT_DIVIDE_BY_ZERO, TARGET_SIGNAL_FPE},
  {-1, -1}};

static void
check (BOOL ok, const char *file, int line)
{
  if (!ok)
    printf_filtered ("error return %s:%d was %lu\n", file, line,
		     GetLastError ());
}

/* Find a thread record given a thread id.
   If get_context then also retrieve the context for this
   thread. */
static thread_info *
thread_rec (DWORD id, int get_context)
{
  thread_info *th;

  for (th = &thread_head; (th = th->next) != NULL;)
    if (th->id == id)
      {
	if (!th->suspend_count && get_context)
	  {
	    if (get_context > 0 && id != current_event.dwThreadId)
	      th->suspend_count = SuspendThread (th->h) + 1;
	    else if (get_context < 0)
	      th->suspend_count = -1;
	    th->reload_context = 1;
	  }
	return th;
      }

  return NULL;
}

/* Add a thread to the thread list */
static thread_info *
win32_add_thread (DWORD id, HANDLE h)
{
  thread_info *th;

  if ((th = thread_rec (id, FALSE)))
    return th;

  th = XZALLOC (thread_info);
  th->id = id;
  th->h = h;
  th->next = thread_head.next;
  thread_head.next = th;
  add_thread (pid_to_ptid (id));
  /* Set the debug registers for the new thread in they are used.  */
  if (debug_registers_used)
    {
      /* Only change the value of the debug registers.  */
      th->context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      CHECK (GetThreadContext (th->h, &th->context));
      th->context.Dr0 = dr[0];
      th->context.Dr1 = dr[1];
      th->context.Dr2 = dr[2];
      th->context.Dr3 = dr[3];
      /* th->context.Dr6 = dr[6];
      FIXME: should we set dr6 also ?? */
      th->context.Dr7 = dr[7];
      CHECK (SetThreadContext (th->h, &th->context));
      th->context.ContextFlags = 0;
    }
  return th;
}

/* Clear out any old thread list and reintialize it to a
   pristine state. */
static void
win32_init_thread_list (void)
{
  thread_info *th = &thread_head;

  DEBUG_EVENTS (("gdb: win32_init_thread_list\n"));
  init_thread_list ();
  while (th->next != NULL)
    {
      thread_info *here = th->next;
      th->next = here->next;
      (void) CloseHandle (here->h);
      xfree (here);
    }
  thread_head.next = NULL;
}

/* Delete a thread from the list of threads */
static void
win32_delete_thread (DWORD id)
{
  thread_info *th;

  if (info_verbose)
    printf_unfiltered ("[Deleting %s]\n", target_pid_to_str (pid_to_ptid (id)));
  delete_thread (pid_to_ptid (id));

  for (th = &thread_head;
       th->next != NULL && th->next->id != id;
       th = th->next)
    continue;

  if (th->next != NULL)
    {
      thread_info *here = th->next;
      th->next = here->next;
      CloseHandle (here->h);
      xfree (here);
    }
}

static void
do_win32_fetch_inferior_registers (struct regcache *regcache, int r)
{
  char *context_offset = ((char *) &current_thread->context) + mappings[r];
  long l;

  if (!current_thread)
    return;	/* Windows sometimes uses a non-existent thread id in its
		   events */

  if (current_thread->reload_context)
    {
#ifdef __COPY_CONTEXT_SIZE
      if (have_saved_context)
	{
	  /* Lie about where the program actually is stopped since cygwin has informed us that
	     we should consider the signal to have occurred at another location which is stored
	     in "saved_context. */
	  memcpy (&current_thread->context, &saved_context, __COPY_CONTEXT_SIZE);
	  have_saved_context = 0;
	}
      else
#endif
	{
	  thread_info *th = current_thread;
	  th->context.ContextFlags = CONTEXT_DEBUGGER_DR;
	  GetThreadContext (th->h, &th->context);
	  /* Copy dr values from that thread.  */
	  dr[0] = th->context.Dr0;
	  dr[1] = th->context.Dr1;
	  dr[2] = th->context.Dr2;
	  dr[3] = th->context.Dr3;
	  dr[6] = th->context.Dr6;
	  dr[7] = th->context.Dr7;
	}
      current_thread->reload_context = 0;
    }

#define I387_ST0_REGNUM I386_ST0_REGNUM

  if (r == I387_FISEG_REGNUM)
    {
      l = *((long *) context_offset) & 0xffff;
      regcache_raw_supply (regcache, r, (char *) &l);
    }
  else if (r == I387_FOP_REGNUM)
    {
      l = (*((long *) context_offset) >> 16) & ((1 << 11) - 1);
      regcache_raw_supply (regcache, r, (char *) &l);
    }
  else if (r >= 0)
    regcache_raw_supply (regcache, r, context_offset);
  else
    {
      for (r = 0; r < gdbarch_num_regs (current_gdbarch); r++)
	do_win32_fetch_inferior_registers (regcache, r);
    }

#undef I387_ST0_REGNUM
}

static void
win32_fetch_inferior_registers (struct regcache *regcache, int r)
{
  current_thread = thread_rec (PIDGET (inferior_ptid), TRUE);
  /* Check if current_thread exists.  Windows sometimes uses a non-existent
     thread id in its events */
  if (current_thread)
    do_win32_fetch_inferior_registers (regcache, r);
}

static void
do_win32_store_inferior_registers (const struct regcache *regcache, int r)
{
  if (!current_thread)
    /* Windows sometimes uses a non-existent thread id in its events */;
  else if (r >= 0)
    regcache_raw_collect (regcache, r,
			  ((char *) &current_thread->context) + mappings[r]);
  else
    {
      for (r = 0; r < gdbarch_num_regs (current_gdbarch); r++)
	do_win32_store_inferior_registers (regcache, r);
    }
}

/* Store a new register value into the current thread context */
static void
win32_store_inferior_registers (struct regcache *regcache, int r)
{
  current_thread = thread_rec (PIDGET (inferior_ptid), TRUE);
  /* Check if current_thread exists.  Windows sometimes uses a non-existent
     thread id in its events */
  if (current_thread)
    do_win32_store_inferior_registers (regcache, r);
}

static int psapi_loaded = 0;
static HMODULE psapi_module_handle = NULL;
static BOOL WINAPI (*psapi_EnumProcessModules) (HANDLE, HMODULE *, DWORD, LPDWORD) = NULL;
static BOOL WINAPI (*psapi_GetModuleInformation) (HANDLE, HMODULE, LPMODULEINFO, DWORD) = NULL;
static DWORD WINAPI (*psapi_GetModuleFileNameExA) (HANDLE, HMODULE, LPSTR, DWORD) = NULL;

static int
psapi_get_dll_name (DWORD BaseAddress, char *dll_name_ret)
{
  DWORD len;
  MODULEINFO mi;
  int i;
  HMODULE dh_buf[1];
  HMODULE *DllHandle = dh_buf;
  DWORD cbNeeded;
  BOOL ok;

  if (!psapi_loaded ||
      psapi_EnumProcessModules == NULL ||
      psapi_GetModuleInformation == NULL ||
      psapi_GetModuleFileNameExA == NULL)
    {
      if (psapi_loaded)
	goto failed;
      psapi_loaded = 1;
      psapi_module_handle = LoadLibrary ("psapi.dll");
      if (!psapi_module_handle)
	{
	  /* printf_unfiltered ("error loading psapi.dll: %u", GetLastError ()); */
	  goto failed;
	}
      psapi_EnumProcessModules = GetProcAddress (psapi_module_handle, "EnumProcessModules");
      psapi_GetModuleInformation = GetProcAddress (psapi_module_handle, "GetModuleInformation");
      psapi_GetModuleFileNameExA = (void *) GetProcAddress (psapi_module_handle,
						    "GetModuleFileNameExA");
      if (psapi_EnumProcessModules == NULL ||
	  psapi_GetModuleInformation == NULL ||
	  psapi_GetModuleFileNameExA == NULL)
	goto failed;
    }

  cbNeeded = 0;
  ok = (*psapi_EnumProcessModules) (current_process_handle,
				    DllHandle,
				    sizeof (HMODULE),
				    &cbNeeded);

  if (!ok || !cbNeeded)
    goto failed;

  DllHandle = (HMODULE *) alloca (cbNeeded);
  if (!DllHandle)
    goto failed;

  ok = (*psapi_EnumProcessModules) (current_process_handle,
				    DllHandle,
				    cbNeeded,
				    &cbNeeded);
  if (!ok)
    goto failed;

  for (i = 0; i < (int) (cbNeeded / sizeof (HMODULE)); i++)
    {
      if (!(*psapi_GetModuleInformation) (current_process_handle,
					  DllHandle[i],
					  &mi,
					  sizeof (mi)))
	error (_("Can't get module info"));

      len = (*psapi_GetModuleFileNameExA) (current_process_handle,
					   DllHandle[i],
					   dll_name_ret,
					   MAX_PATH);
      if (len == 0)
	error (_("Error getting dll name: %u."), (unsigned) GetLastError ());

      if ((DWORD) (mi.lpBaseOfDll) == BaseAddress)
	return 1;
    }

failed:
  dll_name_ret[0] = '\0';
  return 0;
}

/* Encapsulate the information required in a call to
   symbol_file_add_args */
struct safe_symbol_file_add_args
{
  char *name;
  int from_tty;
  struct section_addr_info *addrs;
  int mainline;
  int flags;
  struct ui_file *err, *out;
  struct objfile *ret;
};

/* Maintain a linked list of "so" information. */
struct lm_info
{
  DWORD load_addr;
};

static struct so_list solib_start, *solib_end;

/* Call symbol_file_add with stderr redirected.  We don't care if there
   are errors. */
static int
safe_symbol_file_add_stub (void *argv)
{
#define p ((struct safe_symbol_file_add_args *) argv)
  struct so_list *so = &solib_start;

  p->ret = symbol_file_add (p->name, p->from_tty, p->addrs, p->mainline, p->flags);
  return !!p->ret;
#undef p
}

/* Restore gdb's stderr after calling symbol_file_add */
static void
safe_symbol_file_add_cleanup (void *p)
{
#define sp ((struct safe_symbol_file_add_args *)p)
  gdb_flush (gdb_stderr);
  gdb_flush (gdb_stdout);
  ui_file_delete (gdb_stderr);
  ui_file_delete (gdb_stdout);
  gdb_stderr = sp->err;
  gdb_stdout = sp->out;
#undef sp
}

/* symbol_file_add wrapper that prevents errors from being displayed. */
static struct objfile *
safe_symbol_file_add (char *name, int from_tty,
		      struct section_addr_info *addrs,
		      int mainline, int flags)
{
  struct safe_symbol_file_add_args p;
  struct cleanup *cleanup;

  cleanup = make_cleanup (safe_symbol_file_add_cleanup, &p);

  p.err = gdb_stderr;
  p.out = gdb_stdout;
  gdb_flush (gdb_stderr);
  gdb_flush (gdb_stdout);
  gdb_stderr = ui_file_new ();
  gdb_stdout = ui_file_new ();
  p.name = name;
  p.from_tty = from_tty;
  p.addrs = addrs;
  p.mainline = mainline;
  p.flags = flags;
  catch_errors (safe_symbol_file_add_stub, &p, "", RETURN_MASK_ERROR);

  do_cleanups (cleanup);
  return p.ret;
}

static struct so_list *
win32_make_so (const char *name, DWORD load_addr)
{
  struct so_list *so;
  char buf[MAX_PATH + 1];
  char cwd[MAX_PATH + 1];
  char *p;
  WIN32_FIND_DATA w32_fd;
  HANDLE h = FindFirstFile(name, &w32_fd);
  MEMORY_BASIC_INFORMATION m;

  if (h == INVALID_HANDLE_VALUE)
    strcpy (buf, name);
  else
    {
      FindClose (h);
      strcpy (buf, name);
      if (GetCurrentDirectory (MAX_PATH + 1, cwd))
	{
	  p = strrchr (buf, '\\');
	  if (p)
	    p[1] = '\0';
	  SetCurrentDirectory (buf);
	  GetFullPathName (w32_fd.cFileName, MAX_PATH, buf, &p);
	  SetCurrentDirectory (cwd);
	}
    }

  if (strcasecmp (buf, "ntdll.dll") == 0)
    {
      GetSystemDirectory (buf, sizeof (buf));
      strcat (buf, "\\ntdll.dll");
    }
  so = XZALLOC (struct so_list);
  so->lm_info = (struct lm_info *) xmalloc (sizeof (struct lm_info));
  so->lm_info->load_addr = load_addr;
  cygwin_conv_to_posix_path (buf, so->so_name);
  strcpy (so->so_original_name, name);

  /* Record cygwin1.dll .text start/end.  */
  p = strchr (so->so_name, '\0') - (sizeof ("/cygwin1.dll") - 1);
  if (p >= so->so_name && strcasecmp (p, "/cygwin1.dll") == 0)
    {
      bfd *abfd;
      asection *text = NULL;
      CORE_ADDR text_vma;

      abfd = bfd_openr (name, "pei-i386");

      if (!abfd)
	return so;

      if (bfd_check_format (abfd, bfd_object))
	text = bfd_get_section_by_name (abfd, ".text");

      if (!text)
	{
	  bfd_close (abfd);
	  return so;
	}

      /* The symbols in a dll are offset by 0x1000, which is the the
	 offset from 0 of the first byte in an image - because of the
	 file header and the section alignment. */
      cygwin_load_start = load_addr + 0x1000;
      cygwin_load_end = cygwin_load_start + bfd_section_size (abfd, text);

      bfd_close (abfd);
    }

  return so;
}

static char *
get_image_name (HANDLE h, void *address, int unicode)
{
  static char buf[(2 * MAX_PATH) + 1];
  DWORD size = unicode ? sizeof (WCHAR) : sizeof (char);
  char *address_ptr;
  int len = 0;
  char b[2];
  DWORD done;

  /* Attempt to read the name of the dll that was detected.
     This is documented to work only when actively debugging
     a program.  It will not work for attached processes. */
  if (address == NULL)
    return NULL;

  /* See if we could read the address of a string, and that the
     address isn't null. */
  if (!ReadProcessMemory (h, address,  &address_ptr, sizeof (address_ptr), &done)
      || done != sizeof (address_ptr) || !address_ptr)
    return NULL;

  /* Find the length of the string */
  while (ReadProcessMemory (h, address_ptr + len++ * size, &b, size, &done)
	 && (b[0] != 0 || b[size - 1] != 0) && done == size)
    continue;

  if (!unicode)
    ReadProcessMemory (h, address_ptr, buf, len, &done);
  else
    {
      WCHAR *unicode_address = (WCHAR *) alloca (len * sizeof (WCHAR));
      ReadProcessMemory (h, address_ptr, unicode_address, len * sizeof (WCHAR),
			 &done);

      WideCharToMultiByte (CP_ACP, 0, unicode_address, len, buf, len, 0, 0);
    }

  return buf;
}

/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */
static int
handle_load_dll (void *dummy)
{
  LOAD_DLL_DEBUG_INFO *event = &current_event.u.LoadDll;
  char dll_buf[MAX_PATH + 1];
  char *dll_name = NULL;

  dll_buf[0] = dll_buf[sizeof (dll_buf) - 1] = '\0';

  if (!psapi_get_dll_name ((DWORD) (event->lpBaseOfDll), dll_buf))
    dll_buf[0] = dll_buf[sizeof (dll_buf) - 1] = '\0';

  dll_name = dll_buf;

  if (*dll_name == '\0')
    dll_name = get_image_name (current_process_handle,
			       event->lpImageName, event->fUnicode);
  if (!dll_name)
    return 1;

  solib_end->next = win32_make_so (dll_name, (DWORD) event->lpBaseOfDll);
  solib_end = solib_end->next;

  return 1;
}

static void
win32_free_so (struct so_list *so)
{
  if (so->lm_info)
    xfree (so->lm_info);
  xfree (so);
}

static int
handle_unload_dll (void *dummy)
{
  DWORD lpBaseOfDll = (DWORD) current_event.u.UnloadDll.lpBaseOfDll;
  struct so_list *so;

  for (so = &solib_start; so->next != NULL; so = so->next)
    if (so->next->lm_info->load_addr == lpBaseOfDll)
      {
	struct so_list *sodel = so->next;
	so->next = sodel->next;
	if (!so->next)
	  solib_end = so;
	win32_free_so (sodel);
	solib_add (NULL, 0, NULL, auto_solib_add);
	return 1;
      }

  error (_("Error: dll starting at 0x%lx not found."), (DWORD) lpBaseOfDll);

  return 0;
}

/* Clear list of loaded DLLs. */
static void
win32_clear_solib (void)
{
  solib_start.next = NULL;
  solib_end = &solib_start;
}

/* Load DLL symbol info. */
void
dll_symbol_command (char *args, int from_tty)
{
  int n;
  dont_repeat ();

  if (args == NULL)
    error (_("dll-symbols requires a file name"));

  n = strlen (args);
  if (n > 4 && strcasecmp (args + n - 4, ".dll") != 0)
    {
      char *newargs = (char *) alloca (n + 4 + 1);
      strcpy (newargs, args);
      strcat (newargs, ".dll");
      args = newargs;
    }

  safe_symbol_file_add (args, from_tty, NULL, 0, OBJF_SHARED | OBJF_USERLOADED);
}

/* Handle DEBUG_STRING output from child process.
   Cygwin prepends its messages with a "cygwin:".  Interpret this as
   a Cygwin signal.  Otherwise just print the string as a warning. */
static int
handle_output_debug_string (struct target_waitstatus *ourstatus)
{
  char *s = NULL;
  int retval = 0;

  if (!target_read_string
    ((CORE_ADDR) current_event.u.DebugString.lpDebugStringData, &s, 1024, 0)
      || !s || !*s)
    /* nothing to do */;
  else if (strncmp (s, _CYGWIN_SIGNAL_STRING, sizeof (_CYGWIN_SIGNAL_STRING) - 1) != 0)
    {
      if (strncmp (s, "cYg", 3) != 0)
	warning (("%s"), s);
    }
#ifdef __COPY_CONTEXT_SIZE
  else
    {
      /* Got a cygwin signal marker.  A cygwin signal is followed by the signal number
	 itself and then optionally followed by the thread id and address to saved context
	 within the DLL.  If these are supplied, then the given thread is assumed to have
	 issued the signal and the context from the thread is assumed to be stored at the
	 given address in the inferior.  Tell gdb to treat this like a real signal.  */
      char *p;
      int sig = strtol (s + sizeof (_CYGWIN_SIGNAL_STRING) - 1, &p, 0);
      int gotasig = target_signal_from_host (sig);
      ourstatus->value.sig = gotasig;
      if (gotasig)
	{
	  LPCVOID x;
	  DWORD n;
	  ourstatus->kind = TARGET_WAITKIND_STOPPED;
	  retval = strtoul (p, &p, 0);
	  if (!retval)
	    retval = main_thread_id;
	  else if ((x = (LPCVOID) strtoul (p, &p, 0))
		   && ReadProcessMemory (current_process_handle, x,
					 &saved_context, __COPY_CONTEXT_SIZE, &n)
		   && n == __COPY_CONTEXT_SIZE)
	    have_saved_context = 1;
	  current_event.dwThreadId = retval;
	}
    }
#endif

  if (s)
    xfree (s);
  return retval;
}

static int
display_selector (HANDLE thread, DWORD sel)
{
  LDT_ENTRY info;
  if (GetThreadSelectorEntry (thread, sel, &info))
    {
      int base, limit;
      printf_filtered ("0x%03lx: ", sel);
      if (!info.HighWord.Bits.Pres)
	{
	  puts_filtered ("Segment not present\n");
	  return 0;
	}
      base = (info.HighWord.Bits.BaseHi << 24) +
	     (info.HighWord.Bits.BaseMid << 16)
	     + info.BaseLow;
      limit = (info.HighWord.Bits.LimitHi << 16) + info.LimitLow;
      if (info.HighWord.Bits.Granularity)
	limit = (limit << 12) | 0xfff;
      printf_filtered ("base=0x%08x limit=0x%08x", base, limit);
      if (info.HighWord.Bits.Default_Big)
	puts_filtered(" 32-bit ");
      else
	puts_filtered(" 16-bit ");
      switch ((info.HighWord.Bits.Type & 0xf) >> 1)
	{
	case 0:
	  puts_filtered ("Data (Read-Only, Exp-up");
	  break;
	case 1:
	  puts_filtered ("Data (Read/Write, Exp-up");
	  break;
	case 2:
	  puts_filtered ("Unused segment (");
	  break;
	case 3:
	  puts_filtered ("Data (Read/Write, Exp-down");
	  break;
	case 4:
	  puts_filtered ("Code (Exec-Only, N.Conf");
	  break;
	case 5:
	  puts_filtered ("Code (Exec/Read, N.Conf");
	  break;
	case 6:
	  puts_filtered ("Code (Exec-Only, Conf");
	  break;
	case 7:
	  puts_filtered ("Code (Exec/Read, Conf");
	  break;
	default:
	  printf_filtered ("Unknown type 0x%x",info.HighWord.Bits.Type);
	}
      if ((info.HighWord.Bits.Type & 0x1) == 0)
	puts_filtered(", N.Acc");
      puts_filtered (")\n");
      if ((info.HighWord.Bits.Type & 0x10) == 0)
	puts_filtered("System selector ");
      printf_filtered ("Priviledge level = %d. ", info.HighWord.Bits.Dpl);
      if (info.HighWord.Bits.Granularity)
	puts_filtered ("Page granular.\n");
      else
	puts_filtered ("Byte granular.\n");
      return 1;
    }
  else
    {
      printf_filtered ("Invalid selector 0x%lx.\n",sel);
      return 0;
    }
}

static void
display_selectors (char * args, int from_tty)
{
  if (!current_thread)
    {
      puts_filtered ("Impossible to display selectors now.\n");
      return;
    }
  if (!args)
    {

      puts_filtered ("Selector $cs\n");
      display_selector (current_thread->h,
	current_thread->context.SegCs);
      puts_filtered ("Selector $ds\n");
      display_selector (current_thread->h,
	current_thread->context.SegDs);
      puts_filtered ("Selector $es\n");
      display_selector (current_thread->h,
	current_thread->context.SegEs);
      puts_filtered ("Selector $ss\n");
      display_selector (current_thread->h,
	current_thread->context.SegSs);
      puts_filtered ("Selector $fs\n");
      display_selector (current_thread->h,
	current_thread->context.SegFs);
      puts_filtered ("Selector $gs\n");
      display_selector (current_thread->h,
	current_thread->context.SegGs);
    }
  else
    {
      int sel;
      sel = parse_and_eval_long (args);
      printf_filtered ("Selector \"%s\"\n",args);
      display_selector (current_thread->h, sel);
    }
}

static struct cmd_list_element *info_w32_cmdlist = NULL;

static void
info_w32_command (char *args, int from_tty)
{
  help_list (info_w32_cmdlist, "info w32 ", class_info, gdb_stdout);
}


#define DEBUG_EXCEPTION_SIMPLE(x)       if (debug_exceptions) \
  printf_unfiltered ("gdb: Target exception %s at 0x%08lx\n", x, \
  (DWORD) current_event.u.Exception.ExceptionRecord.ExceptionAddress)

static int
handle_exception (struct target_waitstatus *ourstatus)
{
  thread_info *th;
  DWORD code = current_event.u.Exception.ExceptionRecord.ExceptionCode;

  ourstatus->kind = TARGET_WAITKIND_STOPPED;

  /* Record the context of the current thread */
  th = thread_rec (current_event.dwThreadId, -1);

  switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_ACCESS_VIOLATION");
      ourstatus->value.sig = TARGET_SIGNAL_SEGV;
      {
	/* See if the access violation happened within the cygwin DLL itself.  Cygwin uses
	   a kind of exception handling to deal with passed-in invalid addresses. gdb
	   should not treat these as real SEGVs since they will be silently handled by
	   cygwin.  A real SEGV will (theoretically) be caught by cygwin later in the process
	   and will be sent as a cygwin-specific-signal.  So, ignore SEGVs if they show up
	   within the text segment of the DLL itself. */
	char *fn;
	bfd_vma addr = (bfd_vma) current_event.u.Exception.ExceptionRecord.ExceptionAddress;
	if ((!cygwin_exceptions && (addr >= cygwin_load_start && addr < cygwin_load_end))
	    || (find_pc_partial_function (addr, &fn, NULL, NULL)
		&& strncmp (fn, "KERNEL32!IsBad", strlen ("KERNEL32!IsBad")) == 0))
	  return 0;
      }
      break;
    case STATUS_STACK_OVERFLOW:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_STACK_OVERFLOW");
      ourstatus->value.sig = TARGET_SIGNAL_SEGV;
      break;
    case STATUS_FLOAT_DENORMAL_OPERAND:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_DENORMAL_OPERAND");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_FLOAT_INEXACT_RESULT:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_INEXACT_RESULT");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_FLOAT_INVALID_OPERATION:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_INVALID_OPERATION");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_FLOAT_OVERFLOW:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_OVERFLOW");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_FLOAT_STACK_CHECK:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_STACK_CHECK");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_FLOAT_UNDERFLOW:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_UNDERFLOW");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_FLOAT_DIVIDE_BY_ZERO:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_FLOAT_DIVIDE_BY_ZERO");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_INTEGER_DIVIDE_BY_ZERO");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case STATUS_INTEGER_OVERFLOW:
      DEBUG_EXCEPTION_SIMPLE ("STATUS_INTEGER_OVERFLOW");
      ourstatus->value.sig = TARGET_SIGNAL_FPE;
      break;
    case EXCEPTION_BREAKPOINT:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_BREAKPOINT");
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
    case DBG_CONTROL_C:
      DEBUG_EXCEPTION_SIMPLE ("DBG_CONTROL_C");
      ourstatus->value.sig = TARGET_SIGNAL_INT;
      break;
    case DBG_CONTROL_BREAK:
      DEBUG_EXCEPTION_SIMPLE ("DBG_CONTROL_BREAK");
      ourstatus->value.sig = TARGET_SIGNAL_INT;
      break;
    case EXCEPTION_SINGLE_STEP:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_SINGLE_STEP");
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_ILLEGAL_INSTRUCTION");
      ourstatus->value.sig = TARGET_SIGNAL_ILL;
      break;
    case EXCEPTION_PRIV_INSTRUCTION:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_PRIV_INSTRUCTION");
      ourstatus->value.sig = TARGET_SIGNAL_ILL;
      break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      DEBUG_EXCEPTION_SIMPLE ("EXCEPTION_NONCONTINUABLE_EXCEPTION");
      ourstatus->value.sig = TARGET_SIGNAL_ILL;
      break;
    default:
      /* Treat unhandled first chance exceptions specially. */
      if (current_event.u.Exception.dwFirstChance)
	return -1;
      printf_unfiltered ("gdb: unknown target exception 0x%08lx at 0x%08lx\n",
		    current_event.u.Exception.ExceptionRecord.ExceptionCode,
	(DWORD) current_event.u.Exception.ExceptionRecord.ExceptionAddress);
      ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
  exception_count++;
  last_sig = ourstatus->value.sig;
  return 1;
}

/* Resume all artificially suspended threads if we are continuing
   execution */
static BOOL
win32_continue (DWORD continue_status, int id)
{
  int i;
  thread_info *th;
  BOOL res;

  DEBUG_EVENTS (("ContinueDebugEvent (cpid=%ld, ctid=%ld, %s);\n",
		  current_event.dwProcessId, current_event.dwThreadId,
		  continue_status == DBG_CONTINUE ?
		  "DBG_CONTINUE" : "DBG_EXCEPTION_NOT_HANDLED"));
  res = ContinueDebugEvent (current_event.dwProcessId,
			    current_event.dwThreadId,
			    continue_status);
  if (res)
    for (th = &thread_head; (th = th->next) != NULL;)
      if (((id == -1) || (id == (int) th->id)) && th->suspend_count)
	{

	  for (i = 0; i < th->suspend_count; i++)
	    (void) ResumeThread (th->h);
	  th->suspend_count = 0;
	  if (debug_registers_changed)
	    {
	      /* Only change the value of the debug registers */
	      th->context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	      th->context.Dr0 = dr[0];
	      th->context.Dr1 = dr[1];
	      th->context.Dr2 = dr[2];
	      th->context.Dr3 = dr[3];
	      /* th->context.Dr6 = dr[6];
		 FIXME: should we set dr6 also ?? */
	      th->context.Dr7 = dr[7];
	      CHECK (SetThreadContext (th->h, &th->context));
	      th->context.ContextFlags = 0;
	    }
	}

  debug_registers_changed = 0;
  return res;
}

/* Called in pathological case where Windows fails to send a
   CREATE_PROCESS_DEBUG_EVENT after an attach.  */
static DWORD
fake_create_process (void)
{
  current_process_handle = OpenProcess (PROCESS_ALL_ACCESS, FALSE,
					current_event.dwProcessId);
  main_thread_id = current_event.dwThreadId;
  current_thread = win32_add_thread (main_thread_id,
				     current_event.u.CreateThread.hThread);
  return main_thread_id;
}

static void
win32_resume (ptid_t ptid, int step, enum target_signal sig)
{
  thread_info *th;
  DWORD continue_status = DBG_CONTINUE;

  int pid = PIDGET (ptid);

  if (sig != TARGET_SIGNAL_0)
    {
      if (current_event.dwDebugEventCode != EXCEPTION_DEBUG_EVENT)
	{
	  DEBUG_EXCEPT(("Cannot continue with signal %d here.\n",sig));
	}
      else if (sig == last_sig)
	continue_status = DBG_EXCEPTION_NOT_HANDLED;
      else
#if 0
/* This code does not seem to work, because
  the kernel does probably not consider changes in the ExceptionRecord
  structure when passing the exception to the inferior.
  Note that this seems possible in the exception handler itself.  */
	{
	  int i;
	  for (i = 0; xlate[i].them != -1; i++)
	    if (xlate[i].us == sig)
	      {
		current_event.u.Exception.ExceptionRecord.ExceptionCode =
		  xlate[i].them;
		continue_status = DBG_EXCEPTION_NOT_HANDLED;
		break;
	      }
	  if (continue_status == DBG_CONTINUE)
	    {
	      DEBUG_EXCEPT(("Cannot continue with signal %d.\n",sig));
	    }
	}
#endif
	DEBUG_EXCEPT(("Can only continue with recieved signal %d.\n",
	  last_sig));
    }

  last_sig = TARGET_SIGNAL_0;

  DEBUG_EXEC (("gdb: win32_resume (pid=%d, step=%d, sig=%d);\n",
	       pid, step, sig));

  /* Get context for currently selected thread */
  th = thread_rec (current_event.dwThreadId, FALSE);
  if (th)
    {
      if (step)
	{
	  /* Single step by setting t bit */
	  win32_fetch_inferior_registers (get_current_regcache (),
					  gdbarch_ps_regnum (current_gdbarch));
	  th->context.EFlags |= FLAG_TRACE_BIT;
	}

      if (th->context.ContextFlags)
	{
	  if (debug_registers_changed)
	    {
	      th->context.Dr0 = dr[0];
	      th->context.Dr1 = dr[1];
	      th->context.Dr2 = dr[2];
	      th->context.Dr3 = dr[3];
	      /* th->context.Dr6 = dr[6];
	       FIXME: should we set dr6 also ?? */
	      th->context.Dr7 = dr[7];
	    }
	  CHECK (SetThreadContext (th->h, &th->context));
	  th->context.ContextFlags = 0;
	}
    }

  /* Allow continuing with the same signal that interrupted us.
     Otherwise complain. */

  win32_continue (continue_status, pid);
}

/* Get the next event from the child.  Return 1 if the event requires
   handling by WFI (or whatever).
 */
static int
get_win32_debug_event (int pid, struct target_waitstatus *ourstatus)
{
  BOOL debug_event;
  DWORD continue_status, event_code;
  thread_info *th;
  static thread_info dummy_thread_info;
  int retval = 0;
  ptid_t ptid = {-1};

  last_sig = TARGET_SIGNAL_0;

  if (!(debug_event = WaitForDebugEvent (&current_event, 1000)))
    goto out;

  event_count++;
  continue_status = DBG_CONTINUE;

  event_code = current_event.dwDebugEventCode;
  ourstatus->kind = TARGET_WAITKIND_SPURIOUS;
  th = NULL;
  have_saved_context = 0;

  switch (event_code)
    {
    case CREATE_THREAD_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%x code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "CREATE_THREAD_DEBUG_EVENT"));
      if (saw_create != 1)
	{
	  if (!saw_create && attach_flag)
	    {
	      /* Kludge around a Windows bug where first event is a create
		 thread event.  Caused when attached process does not have
		 a main thread. */
	      retval = ourstatus->value.related_pid = fake_create_process ();
	      saw_create++;
	    }
	  break;
	}
      /* Record the existence of this thread */
      th = win32_add_thread (current_event.dwThreadId,
			     current_event.u.CreateThread.hThread);
      if (info_verbose)
	printf_unfiltered ("[New %s]\n",
			   target_pid_to_str (
			     pid_to_ptid (current_event.dwThreadId)));
      retval = current_event.dwThreadId;
      break;

    case EXIT_THREAD_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "EXIT_THREAD_DEBUG_EVENT"));
      if (current_event.dwThreadId != main_thread_id)
	{
	  win32_delete_thread (current_event.dwThreadId);
	  th = &dummy_thread_info;
	}
      break;

    case CREATE_PROCESS_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "CREATE_PROCESS_DEBUG_EVENT"));
      CloseHandle (current_event.u.CreateProcessInfo.hFile);
      if (++saw_create != 1)
	{
	  CloseHandle (current_event.u.CreateProcessInfo.hProcess);
	  break;
	}

      current_process_handle = current_event.u.CreateProcessInfo.hProcess;
      if (main_thread_id)
	win32_delete_thread (main_thread_id);
      main_thread_id = current_event.dwThreadId;
      /* Add the main thread */
      th = win32_add_thread (main_thread_id,
			     current_event.u.CreateProcessInfo.hThread);
      retval = ourstatus->value.related_pid = current_event.dwThreadId;
      break;

    case EXIT_PROCESS_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "EXIT_PROCESS_DEBUG_EVENT"));
      if (saw_create != 1)
	break;
      ourstatus->kind = TARGET_WAITKIND_EXITED;
      ourstatus->value.integer = current_event.u.ExitProcess.dwExitCode;
      CloseHandle (current_process_handle);
      retval = main_thread_id;
      break;

    case LOAD_DLL_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "LOAD_DLL_DEBUG_EVENT"));
      CloseHandle (current_event.u.LoadDll.hFile);
      if (saw_create != 1)
	break;
      catch_errors (handle_load_dll, NULL, (char *) "", RETURN_MASK_ALL);
      ourstatus->kind = TARGET_WAITKIND_LOADED;
      ourstatus->value.integer = 0;
      retval = main_thread_id;
      break;

    case UNLOAD_DLL_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "UNLOAD_DLL_DEBUG_EVENT"));
      if (saw_create != 1)
	break;
      catch_errors (handle_unload_dll, NULL, (char *) "", RETURN_MASK_ALL);
      ourstatus->kind = TARGET_WAITKIND_LOADED;
      ourstatus->value.integer = 0;
      retval = main_thread_id;
      break;

    case EXCEPTION_DEBUG_EVENT:
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "EXCEPTION_DEBUG_EVENT"));
      if (saw_create != 1)
	break;
      switch (handle_exception (ourstatus))
	{
	case 0:
	  continue_status = DBG_EXCEPTION_NOT_HANDLED;
	  break;
	case 1:
	  retval = current_event.dwThreadId;
	  break;
	case -1:
	  last_sig = 1;
	  continue_status = -1;
	  break;
	}
      break;

    case OUTPUT_DEBUG_STRING_EVENT:	/* message from the kernel */
      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
		     (unsigned) current_event.dwProcessId,
		     (unsigned) current_event.dwThreadId,
		     "OUTPUT_DEBUG_STRING_EVENT"));
      if (saw_create != 1)
	break;
      retval = handle_output_debug_string (ourstatus);
      break;

    default:
      if (saw_create != 1)
	break;
      printf_unfiltered ("gdb: kernel event for pid=%ld tid=%ld\n",
			 (DWORD) current_event.dwProcessId,
			 (DWORD) current_event.dwThreadId);
      printf_unfiltered ("                 unknown event code %ld\n",
			 current_event.dwDebugEventCode);
      break;
    }

  if (!retval || saw_create != 1)
    {
      if (continue_status == -1)
	win32_resume (ptid, 0, 1);
      else
	CHECK (win32_continue (continue_status, -1));
    }
  else
    {
      inferior_ptid = pid_to_ptid (retval);
      current_thread = th ?: thread_rec (current_event.dwThreadId, TRUE);
    }

out:
  return retval;
}

/* Wait for interesting events to occur in the target process. */
static ptid_t
win32_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  int pid = PIDGET (ptid);

  target_terminal_ours ();

  /* We loop when we get a non-standard exception rather than return
     with a SPURIOUS because resume can try and step or modify things,
     which needs a current_thread->h.  But some of these exceptions mark
     the birth or death of threads, which mean that the current thread
     isn't necessarily what you think it is. */

  while (1)
    {
      int retval = get_win32_debug_event (pid, ourstatus);
      if (retval)
	return pid_to_ptid (retval);
      else
	{
	  int detach = 0;

	  if (deprecated_ui_loop_hook != NULL)
	    detach = deprecated_ui_loop_hook (0);

	  if (detach)
	    win32_kill_inferior ();
	}
    }
}

static void
do_initial_win32_stuff (DWORD pid)
{
  extern int stop_after_trap;
  int i;

  last_sig = TARGET_SIGNAL_0;
  event_count = 0;
  exception_count = 0;
  debug_registers_changed = 0;
  debug_registers_used = 0;
  for (i = 0; i < sizeof (dr) / sizeof (dr[0]); i++)
    dr[i] = 0;
  cygwin_load_start = cygwin_load_end = 0;
  current_event.dwProcessId = pid;
  memset (&current_event, 0, sizeof (current_event));
  push_target (&win32_ops);
  disable_breakpoints_in_shlibs ();
  win32_clear_solib ();
  clear_proceed_status ();
  init_wait_for_inferior ();

  terminal_init_inferior_with_pgrp (pid);
  target_terminal_inferior ();

  while (1)
    {
      stop_after_trap = 1;
      wait_for_inferior ();
      if (stop_signal != TARGET_SIGNAL_TRAP)
	resume (0, stop_signal);
      else
	break;
    }
  stop_after_trap = 0;
  return;
}

/* Since Windows XP, detaching from a process is supported by Windows.
   The following code tries loading the appropriate functions dynamically.
   If loading these functions succeeds use them to actually detach from
   the inferior process, otherwise behave as usual, pretending that
   detach has worked. */
static BOOL WINAPI (*DebugSetProcessKillOnExit)(BOOL);
static BOOL WINAPI (*DebugActiveProcessStop)(DWORD);

static int
has_detach_ability (void)
{
  static HMODULE kernel32 = NULL;

  if (!kernel32)
    kernel32 = LoadLibrary ("kernel32.dll");
  if (kernel32)
    {
      if (!DebugSetProcessKillOnExit)
	DebugSetProcessKillOnExit = GetProcAddress (kernel32,
						 "DebugSetProcessKillOnExit");
      if (!DebugActiveProcessStop)
	DebugActiveProcessStop = GetProcAddress (kernel32,
						 "DebugActiveProcessStop");
      if (DebugSetProcessKillOnExit && DebugActiveProcessStop)
	return 1;
    }
  return 0;
}

/* Try to set or remove a user privilege to the current process.  Return -1
   if that fails, the previous setting of that privilege otherwise.

   This code is copied from the Cygwin source code and rearranged to allow
   dynamically loading of the needed symbols from advapi32 which is only
   available on NT/2K/XP. */
static int
set_process_privilege (const char *privilege, BOOL enable)
{
  static HMODULE advapi32 = NULL;
  static BOOL WINAPI (*OpenProcessToken)(HANDLE, DWORD, PHANDLE);
  static BOOL WINAPI (*LookupPrivilegeValue)(LPCSTR, LPCSTR, PLUID);
  static BOOL WINAPI (*AdjustTokenPrivileges)(HANDLE, BOOL, PTOKEN_PRIVILEGES,
					      DWORD, PTOKEN_PRIVILEGES, PDWORD);

  HANDLE token_hdl = NULL;
  LUID restore_priv;
  TOKEN_PRIVILEGES new_priv, orig_priv;
  int ret = -1;
  DWORD size;

  if (GetVersion () >= 0x80000000)  /* No security availbale on 9x/Me */
    return 0;

  if (!advapi32)
    {
      if (!(advapi32 = LoadLibrary ("advapi32.dll")))
	goto out;
      if (!OpenProcessToken)
	OpenProcessToken = GetProcAddress (advapi32, "OpenProcessToken");
      if (!LookupPrivilegeValue)
	LookupPrivilegeValue = GetProcAddress (advapi32,
					       "LookupPrivilegeValueA");
      if (!AdjustTokenPrivileges)
	AdjustTokenPrivileges = GetProcAddress (advapi32,
						"AdjustTokenPrivileges");
      if (!OpenProcessToken || !LookupPrivilegeValue || !AdjustTokenPrivileges)
	{
	  advapi32 = NULL;
	  goto out;
	}
    }

  if (!OpenProcessToken (GetCurrentProcess (),
			 TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
			 &token_hdl))
    goto out;

  if (!LookupPrivilegeValue (NULL, privilege, &restore_priv))
    goto out;

  new_priv.PrivilegeCount = 1;
  new_priv.Privileges[0].Luid = restore_priv;
  new_priv.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

  if (!AdjustTokenPrivileges (token_hdl, FALSE, &new_priv,
			      sizeof orig_priv, &orig_priv, &size))
    goto out;
#if 0
  /* Disabled, otherwise every `attach' in an unprivileged user session
     would raise the "Failed to get SE_DEBUG_NAME privilege" warning in
     win32_attach(). */
  /* AdjustTokenPrivileges returns TRUE even if the privilege could not
     be enabled. GetLastError () returns an correct error code, though. */
  if (enable && GetLastError () == ERROR_NOT_ALL_ASSIGNED)
    goto out;
#endif

  ret = orig_priv.Privileges[0].Attributes == SE_PRIVILEGE_ENABLED ? 1 : 0;

out:
  if (token_hdl)
    CloseHandle (token_hdl);

  return ret;
}

/* Attach to process PID, then initialize for debugging it.  */
static void
win32_attach (char *args, int from_tty)
{
  BOOL ok;
  DWORD pid;

  if (!args)
    error_no_arg (_("process-id to attach"));

  if (set_process_privilege (SE_DEBUG_NAME, TRUE) < 0)
    {
      printf_unfiltered ("Warning: Failed to get SE_DEBUG_NAME privilege\n");
      printf_unfiltered ("This can cause attach to fail on Windows NT/2K/XP\n");
    }

  pid = strtoul (args, 0, 0);		/* Windows pid */

  win32_init_thread_list ();
  ok = DebugActiveProcess (pid);
  saw_create = 0;

  if (!ok)
    {
      /* Try fall back to Cygwin pid */
      pid = cygwin_internal (CW_CYGWIN_PID_TO_WINPID, pid);

      if (pid > 0)
	ok = DebugActiveProcess (pid);

      if (!ok)
	error (_("Can't attach to process."));
    }

  if (has_detach_ability ())
    DebugSetProcessKillOnExit (FALSE);

  attach_flag = 1;

  if (from_tty)
    {
      char *exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program `%s', %s\n", exec_file,
			   target_pid_to_str (pid_to_ptid (pid)));
      else
	printf_unfiltered ("Attaching to %s\n",
			   target_pid_to_str (pid_to_ptid (pid)));

      gdb_flush (gdb_stdout);
    }

  do_initial_win32_stuff (pid);
  target_terminal_ours ();
}

static void
win32_detach (char *args, int from_tty)
{
  int detached = 1;

  if (has_detach_ability ())
    {
      ptid_t ptid = {-1};
      win32_resume (ptid, 0, TARGET_SIGNAL_0);

      if (!DebugActiveProcessStop (current_event.dwProcessId))
	{
	  error (_("Can't detach process %lu (error %lu)"),
		 current_event.dwProcessId, GetLastError ());
	  detached = 0;
	}
      DebugSetProcessKillOnExit (FALSE);
    }
  if (detached && from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered ("Detaching from program: %s, Pid %lu\n", exec_file,
			 current_event.dwProcessId);
      gdb_flush (gdb_stdout);
    }
  inferior_ptid = null_ptid;
  unpush_target (&win32_ops);
}

static char *
win32_pid_to_exec_file (int pid)
{
  /* Try to find the process path using the Cygwin internal process list
     pid isn't a valid pid, unfortunately.  Use current_event.dwProcessId
     instead.  */
  /* TODO: Also find native Windows processes using CW_GETPINFO_FULL.  */

  static char path[MAX_PATH + 1];
  char *path_ptr = NULL;
  int cpid;
  struct external_pinfo *pinfo;

  cygwin_internal (CW_LOCK_PINFO, 1000);
  for (cpid = 0;
       (pinfo = (struct external_pinfo *)
	cygwin_internal (CW_GETPINFO, cpid | CW_NEXTPID));
       cpid = pinfo->pid)
    {
      if (pinfo->dwProcessId == current_event.dwProcessId) /* Got it */
       {
	 cygwin_conv_to_full_posix_path (pinfo->progname, path);
	 path_ptr = path;
	 break;
       }
    }
  cygwin_internal (CW_UNLOCK_PINFO);
  return path_ptr;
}

/* Print status information about what we're accessing.  */

static void
win32_files_info (struct target_ops *ignore)
{
  printf_unfiltered ("\tUsing the running image of %s %s.\n",
      attach_flag ? "attached" : "child", target_pid_to_str (inferior_ptid));
}

static void
win32_open (char *arg, int from_tty)
{
  error (_("Use the \"run\" command to start a Unix child process."));
}

/* Start an inferior win32 child process and sets inferior_ptid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().  */

static void
win32_create_inferior (char *exec_file, char *allargs, char **in_env,
		       int from_tty)
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  BOOL ret;
  DWORD flags;
  char *args;
  char real_path[MAXPATHLEN];
  char *toexec;
  char shell[MAX_PATH + 1]; /* Path to shell */
  const char *sh;
  int tty;
  int ostdin, ostdout, ostderr;
  const char *inferior_io_terminal = get_inferior_io_terminal ();

  if (!exec_file)
    error (_("No executable specified, use `target exec'."));

  memset (&si, 0, sizeof (si));
  si.cb = sizeof (si);

  if (!useshell)
    {
      flags = DEBUG_ONLY_THIS_PROCESS;
      cygwin_conv_to_win32_path (exec_file, real_path);
      toexec = real_path;
    }
  else
    {
      char *newallargs;
      sh = getenv ("SHELL");
      if (!sh)
	sh = "/bin/sh";
      cygwin_conv_to_win32_path (sh, shell);
      newallargs = alloca (sizeof (" -c 'exec  '") + strlen (exec_file)
			   + strlen (allargs) + 2);
      sprintf (newallargs, " -c 'exec %s %s'", exec_file, allargs);
      allargs = newallargs;
      toexec = shell;
      flags = DEBUG_PROCESS;
    }

  if (new_group)
    flags |= CREATE_NEW_PROCESS_GROUP;

  if (new_console)
    flags |= CREATE_NEW_CONSOLE;

  attach_flag = 0;

  args = alloca (strlen (toexec) + strlen (allargs) + 2);
  strcpy (args, toexec);
  strcat (args, " ");
  strcat (args, allargs);

  /* Prepare the environment vars for CreateProcess.  */
  cygwin_internal (CW_SYNC_WINENV);

  if (!inferior_io_terminal)
    tty = ostdin = ostdout = ostderr = -1;
  else
    {
      tty = open (inferior_io_terminal, O_RDWR | O_NOCTTY);
      if (tty < 0)
	{
	  print_sys_errmsg (inferior_io_terminal, errno);
	  ostdin = ostdout = ostderr = -1;
	}
      else
	{
	  ostdin = dup (0);
	  ostdout = dup (1);
	  ostderr = dup (2);
	  dup2 (tty, 0);
	  dup2 (tty, 1);
	  dup2 (tty, 2);
	}
    }

  win32_init_thread_list ();
  ret = CreateProcess (0,
		       args,	/* command line */
		       NULL,	/* Security */
		       NULL,	/* thread */
		       TRUE,	/* inherit handles */
		       flags,	/* start flags */
		       NULL,	/* environment */
		       NULL,	/* current directory */
		       &si,
		       &pi);
  if (tty >= 0)
    {
      close (tty);
      dup2 (ostdin, 0);
      dup2 (ostdout, 1);
      dup2 (ostderr, 2);
      close (ostdin);
      close (ostdout);
      close (ostderr);
    }

  if (!ret)
    error (_("Error creating process %s, (error %d)."),
	   exec_file, (unsigned) GetLastError ());

  CloseHandle (pi.hThread);
  CloseHandle (pi.hProcess);

  if (useshell && shell[0] != '\0')
    saw_create = -1;
  else
    saw_create = 0;

  do_initial_win32_stuff (pi.dwProcessId);

  /* win32_continue (DBG_CONTINUE, -1); */
}

static void
win32_mourn_inferior (void)
{
  (void) win32_continue (DBG_CONTINUE, -1);
  i386_cleanup_dregs();
  unpush_target (&win32_ops);
  generic_mourn_inferior ();
}

/* Send a SIGINT to the process group.  This acts just like the user typed a
   ^C on the controlling terminal. */

static void
win32_stop (void)
{
  DEBUG_EVENTS (("gdb: GenerateConsoleCtrlEvent (CTRLC_EVENT, 0)\n"));
  CHECK (GenerateConsoleCtrlEvent (CTRL_C_EVENT, current_event.dwProcessId));
  registers_changed ();		/* refresh register state */
}

static int
win32_xfer_memory (CORE_ADDR memaddr, gdb_byte *our, int len,
		   int write, struct mem_attrib *mem,
		   struct target_ops *target)
{
  DWORD done = 0;
  if (write)
    {
      DEBUG_MEM (("gdb: write target memory, %d bytes at 0x%08lx\n",
		  len, (DWORD) memaddr));
      if (!WriteProcessMemory (current_process_handle, (LPVOID) memaddr, our,
			       len, &done))
	done = 0;
      FlushInstructionCache (current_process_handle, (LPCVOID) memaddr, len);
    }
  else
    {
      DEBUG_MEM (("gdb: read target memory, %d bytes at 0x%08lx\n",
		  len, (DWORD) memaddr));
      if (!ReadProcessMemory (current_process_handle, (LPCVOID) memaddr, our,
			      len, &done))
	done = 0;
    }
  return done;
}

static void
win32_kill_inferior (void)
{
  CHECK (TerminateProcess (current_process_handle, 0));

  for (;;)
    {
      if (!win32_continue (DBG_CONTINUE, -1))
	break;
      if (!WaitForDebugEvent (&current_event, INFINITE))
	break;
      if (current_event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
	break;
    }

  CHECK (CloseHandle (current_process_handle));

  /* this may fail in an attached process so don't check. */
  if (current_thread && current_thread->h)
    (void) CloseHandle (current_thread->h);
  target_mourn_inferior ();	/* or just win32_mourn_inferior? */
}

static void
win32_prepare_to_store (struct regcache *regcache)
{
  /* Do nothing, since we can store individual regs */
}

static int
win32_can_run (void)
{
  return 1;
}

static void
win32_close (int x)
{
  DEBUG_EVENTS (("gdb: win32_close, inferior_ptid=%d\n",
		PIDGET (inferior_ptid)));
}

/* Convert pid to printable format. */
static char *
cygwin_pid_to_str (ptid_t ptid)
{
  static char buf[80];
  int pid = PIDGET (ptid);

  if ((DWORD) pid == current_event.dwProcessId)
    sprintf (buf, "process %d", pid);
  else
    sprintf (buf, "thread %ld.0x%x", current_event.dwProcessId, pid);
  return buf;
}

static LONGEST
win32_xfer_shared_libraries (struct target_ops *ops,
			     enum target_object object, const char *annex,
			     gdb_byte *readbuf, const gdb_byte *writebuf,
			     ULONGEST offset, LONGEST len)
{
  struct obstack obstack;
  const char *buf;
  LONGEST len_avail;
  struct so_list *so;

  if (writebuf)
    return -1;

  obstack_init (&obstack);
  obstack_grow_str (&obstack, "<library-list>\n");
  for (so = solib_start.next; so; so = so->next)
    win32_xfer_shared_library (so->so_name, so->lm_info->load_addr, &obstack);
  obstack_grow_str0 (&obstack, "</library-list>\n");

  buf = obstack_finish (&obstack);
  len_avail = strlen (buf);
  if (offset >= len_avail)
    return 0;

  if (len > len_avail - offset)
    len = len_avail - offset;
  memcpy (readbuf, buf + offset, len);

  obstack_free (&obstack, NULL);
  return len;
}

static LONGEST
win32_xfer_partial (struct target_ops *ops, enum target_object object,
		    const char *annex, gdb_byte *readbuf,
		    const gdb_byte *writebuf, ULONGEST offset, LONGEST len)
{
  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
      if (readbuf)
	return (*ops->deprecated_xfer_memory) (offset, readbuf,
					       len, 0/*write*/, NULL, ops);
      if (writebuf)
	return (*ops->deprecated_xfer_memory) (offset, (gdb_byte *) writebuf,
					       len, 1/*write*/, NULL, ops);
      return -1;

    case TARGET_OBJECT_LIBRARIES:
      return win32_xfer_shared_libraries (ops, object, annex, readbuf,
					  writebuf, offset, len);

    default:
      if (ops->beneath != NULL)
	return ops->beneath->to_xfer_partial (ops->beneath, object, annex,
					      readbuf, writebuf, offset, len);
      return -1;
    }
}

static void
init_win32_ops (void)
{
  win32_ops.to_shortname = "child";
  win32_ops.to_longname = "Win32 child process";
  win32_ops.to_doc = "Win32 child process (started by the \"run\" command).";
  win32_ops.to_open = win32_open;
  win32_ops.to_close = win32_close;
  win32_ops.to_attach = win32_attach;
  win32_ops.to_detach = win32_detach;
  win32_ops.to_resume = win32_resume;
  win32_ops.to_wait = win32_wait;
  win32_ops.to_fetch_registers = win32_fetch_inferior_registers;
  win32_ops.to_store_registers = win32_store_inferior_registers;
  win32_ops.to_prepare_to_store = win32_prepare_to_store;
  win32_ops.deprecated_xfer_memory = win32_xfer_memory;
  win32_ops.to_xfer_partial = win32_xfer_partial;
  win32_ops.to_files_info = win32_files_info;
  win32_ops.to_insert_breakpoint = memory_insert_breakpoint;
  win32_ops.to_remove_breakpoint = memory_remove_breakpoint;
  win32_ops.to_terminal_init = terminal_init_inferior;
  win32_ops.to_terminal_inferior = terminal_inferior;
  win32_ops.to_terminal_ours_for_output = terminal_ours_for_output;
  win32_ops.to_terminal_ours = terminal_ours;
  win32_ops.to_terminal_save_ours = terminal_save_ours;
  win32_ops.to_terminal_info = child_terminal_info;
  win32_ops.to_kill = win32_kill_inferior;
  win32_ops.to_create_inferior = win32_create_inferior;
  win32_ops.to_mourn_inferior = win32_mourn_inferior;
  win32_ops.to_can_run = win32_can_run;
  win32_ops.to_thread_alive = win32_win32_thread_alive;
  win32_ops.to_pid_to_str = cygwin_pid_to_str;
  win32_ops.to_stop = win32_stop;
  win32_ops.to_stratum = process_stratum;
  win32_ops.to_has_all_memory = 1;
  win32_ops.to_has_memory = 1;
  win32_ops.to_has_stack = 1;
  win32_ops.to_has_registers = 1;
  win32_ops.to_has_execution = 1;
  win32_ops.to_magic = OPS_MAGIC;
  win32_ops.to_pid_to_exec_file = win32_pid_to_exec_file;
}

static void
set_win32_aliases (char *argv0)
{
  add_info_alias ("dll", "sharedlibrary", 1);
}

void
_initialize_win32_nat (void)
{
  struct cmd_list_element *c;

  init_win32_ops ();

  c = add_com ("dll-symbols", class_files, dll_symbol_command,
	       _("Load dll library symbols from FILE."));
  set_cmd_completer (c, filename_completer);

  add_com_alias ("sharedlibrary", "dll-symbols", class_alias, 1);

  add_setshow_boolean_cmd ("shell", class_support, &useshell, _("\
Set use of shell to start subprocess."), _("\
Show use of shell to start subprocess."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("cygwin-exceptions", class_support, &cygwin_exceptions, _("\
Break when an exception is detected in the Cygwin DLL itself."), _("\
Show whether gdb breaks on exceptions in the Cygwin DLL itself."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("new-console", class_support, &new_console, _("\
Set creation of new console when creating child process."), _("\
Show creation of new console when creating child process."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("new-group", class_support, &new_group, _("\
Set creation of new group when creating child process."), _("\
Show creation of new group when creating child process."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("debugexec", class_support, &debug_exec, _("\
Set whether to display execution in child process."), _("\
Show whether to display execution in child process."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("debugevents", class_support, &debug_events, _("\
Set whether to display kernel events in child process."), _("\
Show whether to display kernel events in child process."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("debugmemory", class_support, &debug_memory, _("\
Set whether to display memory accesses in child process."), _("\
Show whether to display memory accesses in child process."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("debugexceptions", class_support,
			   &debug_exceptions, _("\
Set whether to display kernel exceptions in child process."), _("\
Show whether to display kernel exceptions in child process."), NULL,
			   NULL,
			   NULL, /* FIXME: i18n: */
			   &setlist, &showlist);

  add_prefix_cmd ("w32", class_info, info_w32_command,
		  _("Print information specific to Win32 debugging."),
		  &info_w32_cmdlist, "info w32 ", 0, &infolist);

  add_cmd ("selector", class_info, display_selectors,
	   _("Display selectors infos."),
	   &info_w32_cmdlist);
  add_target (&win32_ops);
  deprecated_init_ui_hook = set_win32_aliases;
}

/* Hardware watchpoint support, adapted from go32-nat.c code.  */

/* Pass the address ADDR to the inferior in the I'th debug register.
   Here we just store the address in dr array, the registers will be
   actually set up when win32_continue is called.  */
void
cygwin_set_dr (int i, CORE_ADDR addr)
{
  if (i < 0 || i > 3)
    internal_error (__FILE__, __LINE__,
		    _("Invalid register %d in cygwin_set_dr.\n"), i);
  dr[i] = (unsigned) addr;
  debug_registers_changed = 1;
  debug_registers_used = 1;
}

/* Pass the value VAL to the inferior in the DR7 debug control
   register.  Here we just store the address in D_REGS, the watchpoint
   will be actually set up in win32_wait.  */
void
cygwin_set_dr7 (unsigned val)
{
  dr[7] = val;
  debug_registers_changed = 1;
  debug_registers_used = 1;
}

/* Get the value of the DR6 debug status register from the inferior.
   Here we just return the value stored in dr[6]
   by the last call to thread_rec for current_event.dwThreadId id.  */
unsigned
cygwin_get_dr6 (void)
{
  return dr[6];
}

/* Determine if the thread referenced by "pid" is alive
   by "polling" it.  If WaitForSingleObject returns WAIT_OBJECT_0
   it means that the pid has died.  Otherwise it is assumed to be alive. */
static int
win32_win32_thread_alive (ptid_t ptid)
{
  int pid = PIDGET (ptid);

  return WaitForSingleObject (thread_rec (pid, FALSE)->h, 0) == WAIT_OBJECT_0 ?
    FALSE : TRUE;
}

void
_initialize_check_for_gdb_ini (void)
{
  char *homedir;
  if (inhibit_gdbinit)
    return;

  homedir = getenv ("HOME");
  if (homedir)
    {
      char *p;
      char *oldini = (char *) alloca (strlen (homedir) +
				      sizeof ("/gdb.ini"));
      strcpy (oldini, homedir);
      p = strchr (oldini, '\0');
      if (p > oldini && p[-1] != '/')
	*p++ = '/';
      strcpy (p, "gdb.ini");
      if (access (oldini, 0) == 0)
	{
	  int len = strlen (oldini);
	  char *newini = alloca (len + 1);
	  sprintf (newini, "%.*s.gdbinit",
	    (int) (len - (sizeof ("gdb.ini") - 1)), oldini);
	  warning (_("obsolete '%s' found. Rename to '%s'."), oldini, newini);
	}
    }
}
