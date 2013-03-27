/* Auxiliary vector support for GDB, the GNU debugger.

   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "target.h"
#include "gdbtypes.h"
#include "command.h"
#include "inferior.h"
#include "valprint.h"
#include "gdb_assert.h"

#include "auxv.h"
#include "elf/common.h"

#include <unistd.h>
#include <fcntl.h>


/* This function is called like a to_xfer_partial hook,
   but must be called with TARGET_OBJECT_AUXV.
   It handles access via /proc/PID/auxv, which is the common method.
   This function is appropriate for doing:
	   #define NATIVE_XFER_AUXV	procfs_xfer_auxv
   for a native target that uses inftarg.c's child_xfer_partial hook.  */

LONGEST
procfs_xfer_auxv (struct target_ops *ops,
		  int /* enum target_object */ object,
		  const char *annex,
		  gdb_byte *readbuf,
		  const gdb_byte *writebuf,
		  ULONGEST offset,
		  LONGEST len)
{
  char *pathname;
  int fd;
  LONGEST n;

  gdb_assert (object == TARGET_OBJECT_AUXV);
  gdb_assert (readbuf || writebuf);

  pathname = xstrprintf ("/proc/%d/auxv", PIDGET (inferior_ptid));
  fd = open (pathname, writebuf != NULL ? O_WRONLY : O_RDONLY);
  xfree (pathname);
  if (fd < 0)
    return -1;

  if (offset != (ULONGEST) 0
      && lseek (fd, (off_t) offset, SEEK_SET) != (off_t) offset)
    n = -1;
  else if (readbuf != NULL)
    n = read (fd, readbuf, len);
  else
    n = write (fd, writebuf, len);

  (void) close (fd);

  return n;
}

/* Read one auxv entry from *READPTR, not reading locations >= ENDPTR.
   Return 0 if *READPTR is already at the end of the buffer.
   Return -1 if there is insufficient buffer for a whole entry.
   Return 1 if an entry was read into *TYPEP and *VALP.  */
int
target_auxv_parse (struct target_ops *ops, gdb_byte **readptr,
		   gdb_byte *endptr, CORE_ADDR *typep, CORE_ADDR *valp)
{
  const int sizeof_auxv_field = TYPE_LENGTH (builtin_type_void_data_ptr);
  gdb_byte *ptr = *readptr;

  if (endptr == ptr)
    return 0;

  if (endptr - ptr < sizeof_auxv_field * 2)
    return -1;

  *typep = extract_unsigned_integer (ptr, sizeof_auxv_field);
  ptr += sizeof_auxv_field;
  *valp = extract_unsigned_integer (ptr, sizeof_auxv_field);
  ptr += sizeof_auxv_field;

  *readptr = ptr;
  return 1;
}

/* Extract the auxiliary vector entry with a_type matching MATCH.
   Return zero if no such entry was found, or -1 if there was
   an error getting the information.  On success, return 1 after
   storing the entry's value field in *VALP.  */
int
target_auxv_search (struct target_ops *ops, CORE_ADDR match, CORE_ADDR *valp)
{
  CORE_ADDR type, val;
  gdb_byte *data;
  LONGEST n = target_read_alloc (ops, TARGET_OBJECT_AUXV, NULL, &data);
  gdb_byte *ptr = data;
  int ents = 0;

  if (n <= 0)
    return n;

  while (1)
    switch (target_auxv_parse (ops, &ptr, data + n, &type, &val))
      {
      case 1:			/* Here's an entry, check it.  */
	if (type == match)
	  {
	    xfree (data);
	    *valp = val;
	    return 1;
	  }
	break;
      case 0:			/* End of the vector.  */
	xfree (data);
	return 0;
      default:			/* Bogosity.  */
	xfree (data);
	return -1;
      }

  /*NOTREACHED*/
}


/* Print the contents of the target's AUXV on the specified file. */
int
fprint_target_auxv (struct ui_file *file, struct target_ops *ops)
{
  CORE_ADDR type, val;
  gdb_byte *data;
  LONGEST len = target_read_alloc (ops, TARGET_OBJECT_AUXV, NULL,
				   &data);
  gdb_byte *ptr = data;
  int ents = 0;

  if (len <= 0)
    return len;

  while (target_auxv_parse (ops, &ptr, data + len, &type, &val) > 0)
    {
      extern int addressprint;
      const char *name = "???";
      const char *description = "";
      enum { dec, hex, str } flavor = hex;

      switch (type)
	{
#define TAG(tag, text, kind) \
	case tag: name = #tag; description = text; flavor = kind; break
	  TAG (AT_NULL, _("End of vector"), hex);
	  TAG (AT_IGNORE, _("Entry should be ignored"), hex);
	  TAG (AT_EXECFD, _("File descriptor of program"), dec);
	  TAG (AT_PHDR, _("Program headers for program"), hex);
	  TAG (AT_PHENT, _("Size of program header entry"), dec);
	  TAG (AT_PHNUM, _("Number of program headers"), dec);
	  TAG (AT_PAGESZ, _("System page size"), dec);
	  TAG (AT_BASE, _("Base address of interpreter"), hex);
	  TAG (AT_FLAGS, _("Flags"), hex);
	  TAG (AT_ENTRY, _("Entry point of program"), hex);
	  TAG (AT_NOTELF, _("Program is not ELF"), dec);
	  TAG (AT_UID, _("Real user ID"), dec);
	  TAG (AT_EUID, _("Effective user ID"), dec);
	  TAG (AT_GID, _("Real group ID"), dec);
	  TAG (AT_EGID, _("Effective group ID"), dec);
	  TAG (AT_CLKTCK, _("Frequency of times()"), dec);
	  TAG (AT_PLATFORM, _("String identifying platform"), str);
	  TAG (AT_HWCAP, _("Machine-dependent CPU capability hints"), hex);
	  TAG (AT_FPUCW, _("Used FPU control word"), dec);
	  TAG (AT_DCACHEBSIZE, _("Data cache block size"), dec);
	  TAG (AT_ICACHEBSIZE, _("Instruction cache block size"), dec);
	  TAG (AT_UCACHEBSIZE, _("Unified cache block size"), dec);
	  TAG (AT_IGNOREPPC, _("Entry should be ignored"), dec);
	  TAG (AT_SYSINFO, _("Special system info/entry points"), hex);
	  TAG (AT_SYSINFO_EHDR, _("System-supplied DSO's ELF header"), hex);
	  TAG (AT_SECURE, _("Boolean, was exec setuid-like?"), dec);
	  TAG (AT_SUN_UID, _("Effective user ID"), dec);
	  TAG (AT_SUN_RUID, _("Real user ID"), dec);
	  TAG (AT_SUN_GID, _("Effective group ID"), dec);
	  TAG (AT_SUN_RGID, _("Real group ID"), dec);
	  TAG (AT_SUN_LDELF, _("Dynamic linker's ELF header"), hex);
	  TAG (AT_SUN_LDSHDR, _("Dynamic linker's section headers"), hex);
	  TAG (AT_SUN_LDNAME, _("String giving name of dynamic linker"), str);
	  TAG (AT_SUN_LPAGESZ, _("Large pagesize"), dec);
	  TAG (AT_SUN_PLATFORM, _("Platform name string"), str);
	  TAG (AT_SUN_HWCAP, _("Machine-dependent CPU capability hints"), hex);
	  TAG (AT_SUN_IFLUSH, _("Should flush icache?"), dec);
	  TAG (AT_SUN_CPU, _("CPU name string"), str);
	  TAG (AT_SUN_EMUL_ENTRY, _("COFF entry point address"), hex);
	  TAG (AT_SUN_EMUL_EXECFD, _("COFF executable file descriptor"), dec);
	  TAG (AT_SUN_EXECNAME,
	       _("Canonicalized file name given to execve"), str);
	  TAG (AT_SUN_MMU, _("String for name of MMU module"), str);
	  TAG (AT_SUN_LDDATA, _("Dynamic linker's data segment address"), hex);
	}

      fprintf_filtered (file, "%-4s %-20s %-30s ",
			paddr_d (type), name, description);
      switch (flavor)
	{
	case dec:
	  fprintf_filtered (file, "%s\n", paddr_d (val));
	  break;
	case hex:
	  fprintf_filtered (file, "0x%s\n", paddr_nz (val));
	  break;
	case str:
	  if (addressprint)
	    fprintf_filtered (file, "0x%s", paddr_nz (val));
	  val_print_string (val, -1, 1, file);
	  fprintf_filtered (file, "\n");
	  break;
	}
      ++ents;
    }

  xfree (data);

  return ents;
}

static void
info_auxv_command (char *cmd, int from_tty)
{
  if (! target_has_stack)
    error (_("The program has no auxiliary information now."));
  else
    {
      int ents = fprint_target_auxv (gdb_stdout, &current_target);
      if (ents < 0)
	error (_("No auxiliary vector found, or failed reading it."));
      else if (ents == 0)
	error (_("Auxiliary vector is empty."));
    }
}


extern initialize_file_ftype _initialize_auxv; /* -Wmissing-prototypes; */

void
_initialize_auxv (void)
{
  add_info ("auxv", info_auxv_command,
	    _("Display the inferior's auxiliary vector.\n\
This is information provided by the operating system at program startup."));
}
