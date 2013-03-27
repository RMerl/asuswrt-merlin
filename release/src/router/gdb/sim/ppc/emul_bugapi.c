/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    */


#ifndef _EMUL_BUGAPI_C_
#define _EMUL_BUGAPI_C_

/* Note: this module is called via a table.  There is no benefit in
   making it inline */

#include "emul_generic.h"
#include "emul_bugapi.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


/* EMULATION

   BUG - Motorola's embeded firmware BUG interface

   DESCRIPTION

   

   */


/* from PowerPCBug Debugging Package User's Manual, part 2 of 2 and also bug.S - Dale Rahn */
#define _INCHR		0x000		/* Input character */
#define _INSTAT		0x001		/* Input serial port status */
#define _INLN		0x002		/* Input line (pointer / pointer format) */
#define _READSTR	0x003		/* Input string (pointer / count format) */
#define _READLN		0x004		/* Input line (pointer / count format) */
#define _CHKBRK		0x005		/* Check for break */
#define _DSKRD		0x010		/* Disk read */
#define _DSKWR		0x011		/* Disk write */
#define _DSKCFIG	0x012		/* Disk configure */
#define _DSKFMT		0x014		/* Disk format */
#define _DSKCTRL	0x015		/* Disk control */
#define _NETRD		0x018		/* Read from host */
#define _NETWR		0x019		/* Write to host */
#define _NETCFIG	0x01a		/* Configure network parameters */
#define _NETOPN		0x01b		/* Open file for reading */
#define _NETFRD		0x01c		/* Retreive specified file blocks */
#define _NETCTRL	0x01d		/* Implement special control functions */
#define _OUTCHR		0x020		/* Output character (pointer / pointer format) */
#define _OUTSTR		0x021		/* Output string (pointer / pointer format) */
#define _OUTLN		0x022		/* Output line (pointer / pointer format) */
#define _WRITE		0x023		/* Output string (pointer / count format) */
#define _WRITELN	0x024		/* Output line (pointer / count format) */
#define _WRITDLN	0x025		/* Output line with data (pointer / count format) */
#define _PCRLF		0x026		/* Output carriage return and line feed */
#define _ERASLN		0x027		/* Erase line */
#define _WRITD		0x028		/* Output string with data (pointer / count format) */
#define _SNDBRK		0x029		/* Send break */
#define _DELAY		0x043		/* Timer delay */
#define _RTC_TM		0x050		/* Time initialization for RTC */
#define _RTC_DT		0x051		/* Date initialization for RTC */
#define _RTC_DSP	0x052		/* Display RTC time and date */
#define _RTC_RD		0x053		/* Read the RTC registers */
#define _REDIR		0x060		/* Redirect I/O of a system call function */
#define _REDIR_I	0x061		/* Redirect input */
#define _REDIR_O	0x062		/* Redirect output */
#define _RETURN		0x063		/* Return to PPCbug */
#define _BINDEC		0x064		/* Convert binary to binary coded decimal (BCD) */
#define _CHANGEV	0x067		/* Parse value */
#define _STRCMP		0x068		/* Compare two strings (pointer / count format) */
#define _MULU32		0x069		/* Multiply two 32-bit unsigned integers */
#define _DIVU32		0x06a		/* Divide two 32-bit unsigned integers */
#define _CHK_SUM	0x06b		/* Generate checksum */
#define _BRD_ID		0x070		/* Return pointer to board ID packet */
#define _ENVIRON	0x071		/* Access boot environment parameters */
#define _DIAGFCN	0x074		/* Diagnostic function(s) */
#define _SIOPEPS	0x090		/* Retrieve SCSI pointers */
#define _IOINQ		0x120		/* Port inquire */
#define _IOINFORM	0x124		/* Port inform */
#define _IOCONFIG	0x128		/* Port configure */
#define _IODELETE	0x12c		/* Port delete */
#define _SYMBOLTA	0x130		/* Attach symbol table */
#define _SYMBOLDA	0x131		/* Detach symbol table */

struct bug_map {
  int value;
  const char *info;
};

static const struct bug_map bug_mapping[] = {
  { _INCHR,	".INCHR -- Input character" },
  { _INSTAT,	".INSTAT -- Input serial port status" },
  { _INLN,	".INLN -- Input line (pointer / pointer format)" },
  { _READSTR,	".READSTR -- Input string (pointer / count format)" },
  { _READLN,	".READLN -- Input line (pointer / count format)" },
  { _CHKBRK,	".CHKBRK -- Check for break" },
  { _DSKRD,	".DSKRD -- Disk read" },
  { _DSKWR,	".DSKWR -- Disk write" },
  { _DSKCFIG,	".DSKCFIG -- Disk configure" },
  { _DSKFMT,	".DSKFMT -- Disk format" },
  { _DSKCTRL,	".DSKCTRL -- Disk control" },
  { _NETRD,	".NETRD -- Read from host" },
  { _NETWR,	".NETWR -- Write to host" },
  { _NETCFIG,	".NETCFIG -- Configure network parameters" },
  { _NETOPN,	".NETOPN -- Open file for reading" },
  { _NETFRD,	".NETFRD -- Retreive specified file blocks" },
  { _NETCTRL,	".NETCTRL -- Implement special control functions" },
  { _OUTCHR,	".OUTCHR -- Output character" },
  { _OUTSTR,	".OUTSTR -- Output string (pointer / pointer format)" },
  { _OUTLN,	".OUTLN -- Output line (pointer / pointer format)" },
  { _WRITE,	".WRITE -- Output string (pointer / count format)" },
  { _WRITELN,	".WRITELN -- Output line (pointer / count format)" },
  { _WRITDLN,	".WRITDLN -- Output line with data (pointer / count format)" },
  { _PCRLF,	".PCRLF -- Output carriage return and line feed" },
  { _ERASLN,	".ERASLN -- Erase line" },
  { _WRITD,	".WRITD -- Output string with data (pointer / count format)" },
  { _SNDBRK,	".SNDBRK -- Send break" },
  { _DELAY,	".DELAY -- Timer delay" },
  { _RTC_TM,	".RTC_TM -- Time initialization for RTC" },
  { _RTC_DT,	".RTC_DT -- Date initialization for RTC" },
  { _RTC_DSP,	".RTC_DSP -- Display RTC time and date" },
  { _RTC_RD,	".RTC_RD -- Read the RTC registers" },
  { _REDIR,	".REDIR -- Redirect I/O of a system call function" },
  { _REDIR,	".REDIR -- Redirect input" },
  { _REDIR,	".REDIR -- Redirect output" },
  { _RETURN,	".RETURN -- Return to PPCbug" },
  { _BINDEC,	".BINDEC -- Convert binary to binary coded decimal (BCD)" },
  { _CHANGEV,	".CHANGEV -- Parse value" },
  { _STRCMP,	".STRCMP -- Compare two strings (pointer / count format)" },
  { _MULU32,	".MULU32 -- Multiply two 32-bit unsigned integers" },
  { _DIVU32,	".DIVU32 -- Divide two 32-bit unsigned integers" },
  { _CHK_SUM,	".CHK_SUM -- Generate checksum" },
  { _BRD_ID,	".BRD_ID -- Return pointer to board ID packet" },
  { _ENVIRON,	".ENVIRON -- Access boot environment parameters" },
  { _DIAGFCN,	".DIAGFCN -- Diagnostic function(s)" },
  { _SIOPEPS,	".SIOPEPS -- Retrieve SCSI pointers" },
  { _IOINQ,	".IOINQ -- Port inquire" },
  { _IOINFORM,	".IOINFORM -- Port inform" },
  { _IOCONFIG,	".IOCONFIG -- Port configure" },
  { _IODELETE,	".IODELETE -- Port delete" },
  { _SYMBOLTA,	".SYMBOLTA -- Attach symbol table" },
  { _SYMBOLDA,	".SYMBOLDA -- Detach symbol table" },
};

#ifndef BUGAPI_END_ADDRESS
#define BUGAPI_END_ADDRESS 0x100000
#endif

enum {
  nr_bugapi_disks = 2,
};


struct _os_emul_data {
  device *root;
  unsigned_word memory_size;
  unsigned_word top_of_stack;
  int interrupt_prefix;
  unsigned_word interrupt_vector_address;
  unsigned_word system_call_address;
  unsigned_word stall_cpu_loop_address;
  int little_endian;
  int floating_point_available;
  /* I/O devices */
  device_instance *output;
  device_instance *input;
  device_instance *(disk[nr_bugapi_disks]);
};


static os_emul_data *
emul_bugapi_create(device *root,
		   bfd *image,
		   const char *name)
{
  device *node;
  os_emul_data *bugapi;
  char *filename;

  /* check it really is for us */
  if (name != NULL
      && strcmp(name, "bugapi") != 0
      && strcmp(name, "bug") != 0)
    return NULL;
  if (image != NULL
      && name == NULL
      && bfd_get_start_address(image) >= BUGAPI_END_ADDRESS)
    return NULL;

  bugapi = ZALLOC(os_emul_data);

  /* options */
  emul_add_tree_options(root, image, "bug", "oea",
			1 /*oea-interrupt-prefix*/);

  /* add some real hardware, include eeprom memory for the eeprom trap
     addresses */
  emul_add_tree_hardware(root);
  node = tree_parse(root, "/openprom/memory@0xfff00000");
  tree_parse(node, "./psim,description \"eeprom trap addresses");
  tree_parse(node, "./reg 0xfff00000 0x3000");

  bugapi->root = root;

  bugapi->memory_size
    = tree_find_integer_property(root, "/openprom/options/oea-memory-size");
  bugapi->interrupt_prefix =
    tree_find_integer_property(root, "/openprom/options/oea-interrupt-prefix");
  bugapi->interrupt_vector_address = (bugapi->interrupt_prefix
				      ? MASK(0, 43)
				      : 0);
  bugapi->system_call_address = (bugapi->interrupt_vector_address + 0x00c00);
  bugapi->stall_cpu_loop_address = (bugapi->system_call_address + 0x000f0);
  bugapi->top_of_stack = bugapi->memory_size - 0x1000;
  bugapi->little_endian
    = tree_find_boolean_property(root, "/options/little-endian?");
  bugapi->floating_point_available
    = tree_find_boolean_property(root, "/openprom/options/floating-point?");
  bugapi->input = NULL;
  bugapi->output = NULL;

  /* initialization */
  if (image != NULL)
    tree_parse(root, "/openprom/init/register/0.pc 0x%lx",
	       (unsigned long)bfd_get_start_address(image));
  tree_parse(root, "/openprom/init/register/pc 0x%lx",
	     (unsigned long)bugapi->stall_cpu_loop_address);
  tree_parse(root, "/openprom/init/register/sp 0x%lx",
	     (unsigned long)(bugapi->top_of_stack - 16));
  tree_parse(root, "/openprom/init/register/msr 0x%x",
	     (msr_recoverable_interrupt
	      | (bugapi->little_endian
		 ? (msr_little_endian_mode
		    | msr_interrupt_little_endian_mode)
		 : 0)
	      | (bugapi->floating_point_available
		 ? msr_floating_point_available
		 : 0)
	      | (bugapi->interrupt_prefix
		 ? msr_interrupt_prefix
		 : 0)
	      ));

  /* patch the system call instruction to call this emulation and then
     do an rfi */
  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)bugapi->system_call_address);
  tree_parse(node, "./psim,description \"system-call trap instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)bugapi->system_call_address);
  tree_parse(node, "./data 0x%x", emul_call_instruction);
  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)bugapi->system_call_address + 4);
  tree_parse(node, "./psim,description \"return from interrupt instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)bugapi->system_call_address + 4);
  tree_parse(node, "./data 0x%x",
	     emul_rfi_instruction);

  /* patch the end of the system call instruction so that it contains
     a loop to self instruction and point all the cpu's at this */
  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)bugapi->stall_cpu_loop_address);
  tree_parse(node, "./psim,description \"cpu-loop instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)bugapi->stall_cpu_loop_address);
  tree_parse(node, "./data 0x%lx",
	     (unsigned long)emul_loop_instruction);

  if (image != NULL)
    tree_parse(root, "/openprom/init/stack/stack-type %s",
	       (image->xvec->flavour == bfd_target_elf_flavour
		? "ppc-elf"
		: "ppc-xcoff"));

  if (image != NULL)
    {
      filename = tree_quote_property (bfd_get_filename(image));
      tree_parse(root, "/openprom/init/load-binary/file-name %s",
		 filename);
      free (filename);
    }

  return bugapi;
}

static void
emul_bugapi_init(os_emul_data *bugapi,
		 int nr_cpus)
{
  int i;
  /* get the current input/output devices that were created during
     device tree initialization */
  bugapi->input = tree_find_ihandle_property(bugapi->root, "/chosen/stdin");
  bugapi->output = tree_find_ihandle_property(bugapi->root, "/chosen/stdout");
  /* if present, extract the selected disk devices */
  for (i = 0; i < nr_bugapi_disks; i++) {
    char disk[32];
    char *chp;
    strcpy(disk, "/chosen/disk0");
    ASSERT(sizeof(disk) > strlen(disk));
    chp = strchr(disk, '0');
    *chp = *chp + i;
    if (tree_find_property(bugapi->root, disk) != NULL)
      bugapi->disk[i] = tree_find_ihandle_property(bugapi->root, disk);
  }
}

static const char *
emul_bugapi_instruction_name(int call_id)
{
  static char buffer[40];
  int i;

  for (i = 0; i < sizeof (bug_mapping) / sizeof (bug_mapping[0]); i++)
    {
      if (bug_mapping[i].value == call_id)
	return bug_mapping[i].info;
    }

  (void) sprintf (buffer, "Unknown bug call 0x%x", call_id);
  return buffer;
}

static int
emul_bugapi_do_read(os_emul_data *bugapi,
		    cpu *processor,
		    unsigned_word cia,
		    unsigned_word buf,
		    int nbytes)
{
  unsigned char *scratch_buffer;
  int status;

  /* get a tempoary bufer */
  scratch_buffer = (unsigned char *) zalloc(nbytes);

  /* check if buffer exists by reading it */
  emul_read_buffer((void *)scratch_buffer, buf, nbytes, processor, cia);

  /* read */
  status = device_instance_read(bugapi->input,
				(void *)scratch_buffer, nbytes);

  /* -1 = error, -2 = nothing available - see "serial" [IEEE1275] */
  if (status < 0) {
    status = 0;
  }

  if (status > 0) {
    emul_write_buffer((void *)scratch_buffer, buf, status, processor, cia);

    /* Bugapi chops off the trailing n, but leaves it in the buffer */
    if (scratch_buffer[status-1] == '\n' || scratch_buffer[status-1] == '\r')
      status--;
  }

  zfree(scratch_buffer);
  return status;
}

static void
emul_bugapi_do_diskio(os_emul_data *bugapi,
		      cpu *processor,
		      unsigned_word cia,
		      unsigned_word descriptor_addr,
		      int call_id)
{
  struct dskio_descriptor {
    unsigned_1 ctrl_lun;
    unsigned_1 dev_lun;
    unsigned_2 status;
    unsigned_word pbuffer;
    unsigned_4 blk_num;
    unsigned_2 blk_cnt;
    unsigned_1 flag;
#define BUG_FILE_MARK	 0x80
#define IGNORE_FILENUM	 0x02
#define END_OF_FILE	 0x01
    unsigned_1 addr_mod;
  } descriptor;
  int block;
  emul_read_buffer(&descriptor, descriptor_addr, sizeof(descriptor),
		   processor, cia);
  T2H(descriptor.ctrl_lun);
  T2H(descriptor.dev_lun);
  T2H(descriptor.status);
  T2H(descriptor.pbuffer);
  T2H(descriptor.blk_num);
  T2H(descriptor.blk_cnt);
  T2H(descriptor.flag);
  T2H(descriptor.addr_mod);
  if (descriptor.dev_lun >= nr_bugapi_disks
      || bugapi->disk[descriptor.dev_lun] == NULL) {
    error("emul_bugapi_do_diskio: attempt to access unconfigured disk /chosen/disk%d",
	  descriptor.dev_lun);
  }
  else {
    for (block = 0; block < descriptor.blk_cnt; block++) {
      device_instance *disk = bugapi->disk[descriptor.dev_lun];
      unsigned_1 buf[512]; /*????*/
      unsigned_word block_nr = descriptor.blk_num + block;
      unsigned_word byte_nr = block_nr * sizeof(buf);
      unsigned_word block_addr = descriptor.pbuffer + block*sizeof(buf);
      if (device_instance_seek(disk, 0, byte_nr) < 0)
	error("emul_bugapi_do_diskio: bad seek\n");
      switch (call_id) {
      case _DSKRD:
	if (device_instance_read(disk, buf, sizeof(buf)) != sizeof(buf))
	  error("emul_`bugapi_do_diskio: bad read\n");
	emul_write_buffer(buf, block_addr, sizeof(buf), processor, cia);
	break;
      case _DSKWR:
	emul_read_buffer(buf, block_addr, sizeof(buf), processor, cia);
	if (device_instance_write(disk, buf, sizeof(buf)) != sizeof(buf))
	  error("emul_bugapi_do_diskio: bad write\n");
	break;
      default:
	error("emul_bugapi_do_diskio: bad switch\n");
      }
    }
  }
}

static void
emul_bugapi_do_write(os_emul_data *bugapi,
		     cpu *processor,
		     unsigned_word cia,
		     unsigned_word buf,
		     int nbytes,
		     const char *suffix)
{
  void *scratch_buffer = NULL;

  /* get a tempoary bufer */
  if (nbytes > 0)
    {
      scratch_buffer = zalloc(nbytes);

      /* copy in */
      emul_read_buffer(scratch_buffer, buf, nbytes,
		       processor, cia);

      /* write */
      device_instance_write(bugapi->output, scratch_buffer, nbytes);

      zfree(scratch_buffer);
    }

  if (suffix)
    device_instance_write(bugapi->output, suffix, strlen(suffix));

  flush_stdoutput ();
}

static int
emul_bugapi_instruction_call(cpu *processor,
			     unsigned_word cia,
			     unsigned_word ra,
			     os_emul_data *bugapi)
{
  const int call_id = cpu_registers(processor)->gpr[10];
  unsigned char uc;

#define MY_INDEX itable_instruction_call
  ITRACE (trace_os_emul,
	  (" 0x%x %s, r3 = 0x%lx, r4 = 0x%lx\n",
	   call_id, emul_bugapi_instruction_name (call_id),
	   (long)cpu_registers(processor)->gpr[3],
	   (long)cpu_registers(processor)->gpr[4]));;

  /* check that this isn't an invalid instruction */
  if (cia != bugapi->system_call_address)
    return 0;

  switch (call_id) {
  default:
    error("emul-bugapi: unimplemented bugapi %s from address 0x%lx\n",
	  emul_bugapi_instruction_name (call_id), SRR0);
    break;

  /* read a single character, output r3 = byte */
  /* FIXME: Add support to unbuffer input */
  case _INCHR:
    if (device_instance_read(bugapi->input, (void *)&uc, 1) <= 0)
      uc = 0;
    cpu_registers(processor)->gpr[3] = uc;
    break;

  /* read a line of at most 256 bytes, r3 = ptr to 1st byte, output r3 = ptr to last byte+1  */
  case _INLN:
    cpu_registers(processor)->gpr[3] += emul_bugapi_do_read(bugapi,
							    processor, cia,
							    cpu_registers(processor)->gpr[3],
							    256);
    break;

  /* output a character, r3 = character */
  case _OUTCHR:
    {
      char out = (char)cpu_registers(processor)->gpr[3];
      device_instance_write(bugapi->output, &out, 1);
      break;
    }

  /* output a string, r3 = ptr to 1st byte, r4 = ptr to last byte+1 */
  case _OUTSTR:
    emul_bugapi_do_write(bugapi,
			 processor, cia,
			 cpu_registers(processor)->gpr[3],
			 cpu_registers(processor)->gpr[4] - cpu_registers(processor)->gpr[3],
			 (const char *)0);
    break;

  /* output a string followed by \r\n, r3 = ptr to 1st byte, r4 = ptr to last byte+1 */
  case _OUTLN:

    emul_bugapi_do_write(bugapi,
			 processor, cia,
			 cpu_registers(processor)->gpr[3],
			 cpu_registers(processor)->gpr[4] - cpu_registers(processor)->gpr[3],
			 "\n");
    break;

  /* output a \r\n */
  case _PCRLF:
    device_instance_write(bugapi->output, "\n", 1);
    break;

  /* read/write blocks of data to/from the disk */
  case _DSKWR:
  case _DSKRD:
    emul_bugapi_do_diskio(bugapi, processor, cia,
			  cpu_registers(processor)->gpr[3],
			  call_id);
    break;

  /* return to ppcbug monitor (exiting with gpr[3] as status is not
     part of the bug monitor) */
  case _RETURN:
    cpu_halt(processor, cia, was_exited, cpu_registers(processor)->gpr[3]);
    break;
  }
  return 1;
  /* the instruction following this one is a RFI.  Thus by just
     continuing the return from system call is performed */
}

const os_emul emul_bugapi = {
  "bugapi",
  emul_bugapi_create,
  emul_bugapi_init,
  0, /*system_call*/
  emul_bugapi_instruction_call,
  0 /*data*/
};

#endif
