/* interp.c -- Simulator for Motorola 68HC11/68HC12
   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2007
   Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@nerim.fr)

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
#include "sim-assert.h"
#include "sim-hw.h"
#include "sim-options.h"
#include "hw-tree.h"
#include "hw-device.h"
#include "hw-ports.h"
#include "elf32-m68hc1x.h"

#ifndef MONITOR_BASE
# define MONITOR_BASE (0x0C000)
# define MONITOR_SIZE (0x04000)
#endif

static void sim_get_info (SIM_DESC sd, char *cmd);


char *interrupt_names[] = {
  "reset",
  "nmi",
  "int",
  NULL
};

#ifndef INLINE
#if defined(__GNUC__) && defined(__OPTIMIZE__)
#define INLINE __inline__
#else
#define INLINE
#endif
#endif

struct sim_info_list
{
  const char *name;
  const char *device;
};

struct sim_info_list dev_list_68hc11[] = {
  {"cpu", "/m68hc11"},
  {"timer", "/m68hc11/m68hc11tim"},
  {"sio", "/m68hc11/m68hc11sio"},
  {"spi", "/m68hc11/m68hc11spi"},
  {"eeprom", "/m68hc11/m68hc11eepr"},
  {0, 0}
};

struct sim_info_list dev_list_68hc12[] = {
  {"cpu", "/m68hc12"},
  {"timer", "/m68hc12/m68hc12tim"},
  {"sio", "/m68hc12/m68hc12sio"},
  {"spi", "/m68hc12/m68hc12spi"},
  {"eeprom", "/m68hc12/m68hc12eepr"},
  {0, 0}
};

/* Cover function of sim_state_free to free the cpu buffers as well.  */

static void
free_state (SIM_DESC sd)
{
  if (STATE_MODULES (sd) != NULL)
    sim_module_uninstall (sd);

  sim_state_free (sd);
}

/* Give some information about the simulator.  */
static void
sim_get_info (SIM_DESC sd, char *cmd)
{
  sim_cpu *cpu;

  cpu = STATE_CPU (sd, 0);
  if (cmd != 0 && (cmd[0] == ' ' || cmd[0] == '-'))
    {
      int i;
      struct hw *hw_dev;
      struct sim_info_list *dev_list;
      const struct bfd_arch_info *arch;

      arch = STATE_ARCHITECTURE (sd);
      cmd++;

      if (arch->arch == bfd_arch_m68hc11)
        dev_list = dev_list_68hc11;
      else
        dev_list = dev_list_68hc12;

      for (i = 0; dev_list[i].name; i++)
	if (strcmp (cmd, dev_list[i].name) == 0)
	  break;

      if (dev_list[i].name == 0)
	{
	  sim_io_eprintf (sd, "Device '%s' not found.\n", cmd);
	  sim_io_eprintf (sd, "Valid devices: cpu timer sio eeprom\n");
	  return;
	}
      hw_dev = sim_hw_parse (sd, dev_list[i].device);
      if (hw_dev == 0)
	{
	  sim_io_eprintf (sd, "Device '%s' not found\n", dev_list[i].device);
	  return;
	}
      hw_ioctl (hw_dev, 23, 0);
      return;
    }

  cpu_info (sd, cpu);
  interrupts_info (sd, &cpu->cpu_interrupts);
}


void
sim_board_reset (SIM_DESC sd)
{
  struct hw *hw_cpu;
  sim_cpu *cpu;
  const struct bfd_arch_info *arch;
  const char *cpu_type;

  cpu = STATE_CPU (sd, 0);
  arch = STATE_ARCHITECTURE (sd);

  /*  hw_cpu = sim_hw_parse (sd, "/"); */
  if (arch->arch == bfd_arch_m68hc11)
    {
      cpu->cpu_type = CPU_M6811;
      cpu_type = "/m68hc11";
    }
  else
    {
      cpu->cpu_type = CPU_M6812;
      cpu_type = "/m68hc12";
    }
  
  hw_cpu = sim_hw_parse (sd, cpu_type);
  if (hw_cpu == 0)
    {
      sim_io_eprintf (sd, "%s cpu not found in device tree.", cpu_type);
      return;
    }

  cpu_reset (cpu);
  hw_port_event (hw_cpu, 3, 0);
  cpu_restart (cpu);
}

static int
sim_hw_configure (SIM_DESC sd)
{
  const struct bfd_arch_info *arch;
  struct hw *device_tree;
  sim_cpu *cpu;
  
  arch = STATE_ARCHITECTURE (sd);
  if (arch == 0)
    return 0;

  cpu = STATE_CPU (sd, 0);
  cpu->cpu_configured_arch = arch;
  device_tree = sim_hw_parse (sd, "/");
  if (arch->arch == bfd_arch_m68hc11)
    {
      cpu->cpu_interpretor = cpu_interp_m6811;
      if (hw_tree_find_property (device_tree, "/m68hc11/reg") == 0)
	{
	  /* Allocate core managed memory */

	  /* the monitor  */
	  sim_do_commandf (sd, "memory region 0x%lx@%d,0x%lx",
			   /* MONITOR_BASE, MONITOR_SIZE */
			   0x8000, M6811_RAM_LEVEL, 0x8000);
	  sim_do_commandf (sd, "memory region 0x000@%d,0x8000",
			   M6811_RAM_LEVEL);
	  sim_hw_parse (sd, "/m68hc11/reg 0x1000 0x03F");
          if (cpu->bank_start < cpu->bank_end)
            {
              sim_do_commandf (sd, "memory region 0x%lx@%d,0x100000",
                               cpu->bank_virtual, M6811_RAM_LEVEL);
              sim_hw_parse (sd, "/m68hc11/use_bank 1");
            }
	}
      if (cpu->cpu_start_mode)
        {
          sim_hw_parse (sd, "/m68hc11/mode %s", cpu->cpu_start_mode);
        }
      if (hw_tree_find_property (device_tree, "/m68hc11/m68hc11sio/reg") == 0)
	{
	  sim_hw_parse (sd, "/m68hc11/m68hc11sio/reg 0x2b 0x5");
	  sim_hw_parse (sd, "/m68hc11/m68hc11sio/backend stdio");
	  sim_hw_parse (sd, "/m68hc11 > cpu-reset reset /m68hc11/m68hc11sio");
	}
      if (hw_tree_find_property (device_tree, "/m68hc11/m68hc11tim/reg") == 0)
	{
	  /* M68hc11 Timer configuration. */
	  sim_hw_parse (sd, "/m68hc11/m68hc11tim/reg 0x1b 0x5");
	  sim_hw_parse (sd, "/m68hc11 > cpu-reset reset /m68hc11/m68hc11tim");
          sim_hw_parse (sd, "/m68hc11 > capture capture /m68hc11/m68hc11tim");
	}

      /* Create the SPI device.  */
      if (hw_tree_find_property (device_tree, "/m68hc11/m68hc11spi/reg") == 0)
	{
	  sim_hw_parse (sd, "/m68hc11/m68hc11spi/reg 0x28 0x3");
	  sim_hw_parse (sd, "/m68hc11 > cpu-reset reset /m68hc11/m68hc11spi");
	}
      if (hw_tree_find_property (device_tree, "/m68hc11/nvram/reg") == 0)
	{
	  /* M68hc11 persistent ram configuration. */
	  sim_hw_parse (sd, "/m68hc11/nvram/reg 0x0 256");
	  sim_hw_parse (sd, "/m68hc11/nvram/file m68hc11.ram");
	  sim_hw_parse (sd, "/m68hc11/nvram/mode save-modified");
	  /*sim_hw_parse (sd, "/m68hc11 > cpu-reset reset /m68hc11/pram"); */
	}
      if (hw_tree_find_property (device_tree, "/m68hc11/m68hc11eepr/reg") == 0)
	{
	  sim_hw_parse (sd, "/m68hc11/m68hc11eepr/reg 0xb000 512");
	  sim_hw_parse (sd, "/m68hc11 > cpu-reset reset /m68hc11/m68hc11eepr");
	}
      sim_hw_parse (sd, "/m68hc11 > port-a cpu-write-port /m68hc11");
      sim_hw_parse (sd, "/m68hc11 > port-b cpu-write-port /m68hc11");
      sim_hw_parse (sd, "/m68hc11 > port-c cpu-write-port /m68hc11");
      sim_hw_parse (sd, "/m68hc11 > port-d cpu-write-port /m68hc11");
      cpu->hw_cpu = sim_hw_parse (sd, "/m68hc11");
    }
  else
    {
      cpu->cpu_interpretor = cpu_interp_m6812;
      if (hw_tree_find_property (device_tree, "/m68hc12/reg") == 0)
	{
	  /* Allocate core external memory.  */
	  sim_do_commandf (sd, "memory region 0x%lx@%d,0x%lx",
			   0x8000, M6811_RAM_LEVEL, 0x8000);
	  sim_do_commandf (sd, "memory region 0x000@%d,0x8000",
			   M6811_RAM_LEVEL);
          if (cpu->bank_start < cpu->bank_end)
            {
              sim_do_commandf (sd, "memory region 0x%lx@%d,0x100000",
                               cpu->bank_virtual, M6811_RAM_LEVEL);
              sim_hw_parse (sd, "/m68hc12/use_bank 1");
            }
	  sim_hw_parse (sd, "/m68hc12/reg 0x0 0x3FF");
	}

      if (!hw_tree_find_property (device_tree, "/m68hc12/m68hc12sio@1/reg"))
	{
	  sim_hw_parse (sd, "/m68hc12/m68hc12sio@1/reg 0xC0 0x8");
	  sim_hw_parse (sd, "/m68hc12/m68hc12sio@1/backend stdio");
	  sim_hw_parse (sd, "/m68hc12 > cpu-reset reset /m68hc12/m68hc12sio@1");
	}
      if (hw_tree_find_property (device_tree, "/m68hc12/m68hc12tim/reg") == 0)
	{
	  /* M68hc11 Timer configuration. */
	  sim_hw_parse (sd, "/m68hc12/m68hc12tim/reg 0x1b 0x5");
	  sim_hw_parse (sd, "/m68hc12 > cpu-reset reset /m68hc12/m68hc12tim");
          sim_hw_parse (sd, "/m68hc12 > capture capture /m68hc12/m68hc12tim");
	}

      /* Create the SPI device.  */
      if (hw_tree_find_property (device_tree, "/m68hc12/m68hc12spi/reg") == 0)
	{
	  sim_hw_parse (sd, "/m68hc12/m68hc12spi/reg 0x28 0x3");
	  sim_hw_parse (sd, "/m68hc12 > cpu-reset reset /m68hc12/m68hc12spi");
	}
      if (hw_tree_find_property (device_tree, "/m68hc12/nvram/reg") == 0)
	{
	  /* M68hc11 persistent ram configuration. */
	  sim_hw_parse (sd, "/m68hc12/nvram/reg 0x2000 8192");
	  sim_hw_parse (sd, "/m68hc12/nvram/file m68hc12.ram");
	  sim_hw_parse (sd, "/m68hc12/nvram/mode save-modified");
	}
      if (hw_tree_find_property (device_tree, "/m68hc12/m68hc12eepr/reg") == 0)
	{
	  sim_hw_parse (sd, "/m68hc12/m68hc12eepr/reg 0x0800 2048");
	  sim_hw_parse (sd, "/m68hc12 > cpu-reset reset /m68hc12/m68hc12eepr");
	}

      sim_hw_parse (sd, "/m68hc12 > port-a cpu-write-port /m68hc12");
      sim_hw_parse (sd, "/m68hc12 > port-b cpu-write-port /m68hc12");
      sim_hw_parse (sd, "/m68hc12 > port-c cpu-write-port /m68hc12");
      sim_hw_parse (sd, "/m68hc12 > port-d cpu-write-port /m68hc12");
      cpu->hw_cpu = sim_hw_parse (sd, "/m68hc12");
    }
  return 1;
}

/* Get the memory bank parameters by looking at the global symbols
   defined by the linker.  */
static int
sim_get_bank_parameters (SIM_DESC sd, bfd* abfd)
{
  sim_cpu *cpu;
  long symsize;
  long symbol_count, i;
  unsigned size;
  asymbol** asymbols;
  asymbol** current;

  cpu = STATE_CPU (sd, 0);

  symsize = bfd_get_symtab_upper_bound (abfd);
  if (symsize < 0)
    {
      sim_io_eprintf (sd, "Cannot read symbols of program");
      return 0;
    }
  asymbols = (asymbol **) xmalloc (symsize);
  symbol_count = bfd_canonicalize_symtab (abfd, asymbols);
  if (symbol_count < 0)
    {
      sim_io_eprintf (sd, "Cannot read symbols of program");
      return 0;
    }

  size = 0;
  for (i = 0, current = asymbols; i < symbol_count; i++, current++)
    {
      const char* name = bfd_asymbol_name (*current);

      if (strcmp (name, BFD_M68HC11_BANK_START_NAME) == 0)
        {
          cpu->bank_start = bfd_asymbol_value (*current);
        }
      else if (strcmp (name, BFD_M68HC11_BANK_SIZE_NAME) == 0)
        {
          size = bfd_asymbol_value (*current);
        }
      else if (strcmp (name, BFD_M68HC11_BANK_VIRTUAL_NAME) == 0)
        {
          cpu->bank_virtual = bfd_asymbol_value (*current);
        }
    }
  free (asymbols);

  cpu->bank_end = cpu->bank_start + size;
  cpu->bank_shift = 0;
  for (; size > 1; size >>= 1)
    cpu->bank_shift++;

  return 0;
}

static int
sim_prepare_for_program (SIM_DESC sd, bfd* abfd)
{
  sim_cpu *cpu;
  int elf_flags = 0;

  cpu = STATE_CPU (sd, 0);

  if (abfd != NULL)
    {
      asection *s;

      if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
        elf_flags = elf_elfheader (abfd)->e_flags;

      cpu->cpu_elf_start = bfd_get_start_address (abfd);
      /* See if any section sets the reset address */
      cpu->cpu_use_elf_start = 1;
      for (s = abfd->sections; s && cpu->cpu_use_elf_start; s = s->next) 
        {
          if (s->flags & SEC_LOAD)
            {
              bfd_size_type size;

              size = bfd_get_section_size (s);
              if (size > 0)
                {
                  bfd_vma lma;

                  if (STATE_LOAD_AT_LMA_P (sd))
                    lma = bfd_section_lma (abfd, s);
                  else
                    lma = bfd_section_vma (abfd, s);

                  if (lma <= 0xFFFE && lma+size >= 0x10000)
                    cpu->cpu_use_elf_start = 0;
                }
            }
        }

      if (elf_flags & E_M68HC12_BANKS)
        {
          if (sim_get_bank_parameters (sd, abfd) != 0)
            sim_io_eprintf (sd, "Memory bank parameters are not initialized\n");
        }
    }

  if (!sim_hw_configure (sd))
    return SIM_RC_FAIL;

  /* reset all state information */
  sim_board_reset (sd);

  return SIM_RC_OK;
}

SIM_DESC
sim_open (SIM_OPEN_KIND kind, host_callback *callback,
          bfd *abfd, char **argv)
{
  SIM_DESC sd;
  sim_cpu *cpu;

  sd = sim_state_alloc (kind, callback);
  cpu = STATE_CPU (sd, 0);

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* for compatibility */
  current_alignment = NONSTRICT_ALIGNMENT;
  current_target_byte_order = BIG_ENDIAN;

  cpu_initialize (sd, cpu);

  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* getopt will print the error message so we just have to exit if this fails.
     FIXME: Hmmm...  in the case of gdb we need getopt to call
     print_filtered.  */
  if (sim_parse_args (sd, argv) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
         file descriptor leaks, etc.  */
      free_state (sd);
      return 0;
    }

  /* Check for/establish the a reference program image.  */
  if (sim_analyze_program (sd,
			   (STATE_PROG_ARGV (sd) != NULL
			    ? *STATE_PROG_ARGV (sd)
			    : NULL), abfd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* Establish any remaining configuration options.  */
  if (sim_config (sd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  if (sim_post_argv_init (sd) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
         file descriptor leaks, etc.  */
      free_state (sd);
      return 0;
    }
  if (sim_prepare_for_program (sd, abfd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }      

  /* Fudge our descriptor.  */
  return sd;
}


void
sim_close (SIM_DESC sd, int quitting)
{
  /* shut down modules */
  sim_module_uninstall (sd);

  /* Ensure that any resources allocated through the callback
     mechanism are released: */
  sim_io_shutdown (sd);

  /* FIXME - free SD */
  sim_state_free (sd);
  return;
}

void
sim_set_profile (int n)
{
}

void
sim_set_profile_size (int n)
{
}

/* Generic implementation of sim_engine_run that works within the
   sim_engine setjmp/longjmp framework. */

void
sim_engine_run (SIM_DESC sd,
                int next_cpu_nr,	/* ignore */
		int nr_cpus,	/* ignore */
		int siggnal)	/* ignore */
{
  sim_cpu *cpu;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  cpu = STATE_CPU (sd, 0);
  while (1)
    {
      cpu_single_step (cpu);

      /* process any events */
      if (sim_events_tickn (sd, cpu->cpu_current_cycle))
	{
	  sim_events_process (sd);
	}
    }
}

int
sim_trace (SIM_DESC sd)
{
  sim_resume (sd, 0, 0);
  return 1;
}

void
sim_info (SIM_DESC sd, int verbose)
{
  const char *cpu_type;
  const struct bfd_arch_info *arch;

  /* Nothing to do if there is no verbose flag set.  */
  if (verbose == 0 && STATE_VERBOSE_P (sd) == 0)
    return;

  arch = STATE_ARCHITECTURE (sd);
  if (arch->arch == bfd_arch_m68hc11)
    cpu_type = "68HC11";
  else
    cpu_type = "68HC12";

  sim_io_eprintf (sd, "Simulator info:\n");
  sim_io_eprintf (sd, "  CPU Motorola %s\n", cpu_type);
  sim_get_info (sd, 0);
  sim_module_info (sd, verbose || STATE_VERBOSE_P (sd));
}

SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *abfd,
                     char **argv, char **env)
{
  return sim_prepare_for_program (sd, abfd);
}


void
sim_set_callbacks (host_callback *p)
{
  /*  m6811_callback = p; */
}


int
sim_fetch_register (SIM_DESC sd, int rn, unsigned char *memory, int length)
{
  sim_cpu *cpu;
  uint16 val;
  int size = 2;

  cpu = STATE_CPU (sd, 0);
  switch (rn)
    {
    case A_REGNUM:
      val = cpu_get_a (cpu);
      size = 1;
      break;

    case B_REGNUM:
      val = cpu_get_b (cpu);
      size = 1;
      break;

    case D_REGNUM:
      val = cpu_get_d (cpu);
      break;

    case X_REGNUM:
      val = cpu_get_x (cpu);
      break;

    case Y_REGNUM:
      val = cpu_get_y (cpu);
      break;

    case SP_REGNUM:
      val = cpu_get_sp (cpu);
      break;

    case PC_REGNUM:
      val = cpu_get_pc (cpu);
      break;

    case PSW_REGNUM:
      val = cpu_get_ccr (cpu);
      size = 1;
      break;

    case PAGE_REGNUM:
      val = cpu_get_page (cpu);
      size = 1;
      break;

    default:
      val = 0;
      break;
    }
  if (size == 1)
    {
      memory[0] = val;
    }
  else
    {
      memory[0] = val >> 8;
      memory[1] = val & 0x0FF;
    }
  return size;
}

int
sim_store_register (SIM_DESC sd, int rn, unsigned char *memory, int length)
{
  uint16 val;
  sim_cpu *cpu;

  cpu = STATE_CPU (sd, 0);

  val = *memory++;
  if (length == 2)
    val = (val << 8) | *memory;

  switch (rn)
    {
    case D_REGNUM:
      cpu_set_d (cpu, val);
      break;

    case A_REGNUM:
      cpu_set_a (cpu, val);
      return 1;

    case B_REGNUM:
      cpu_set_b (cpu, val);
      return 1;

    case X_REGNUM:
      cpu_set_x (cpu, val);
      break;

    case Y_REGNUM:
      cpu_set_y (cpu, val);
      break;

    case SP_REGNUM:
      cpu_set_sp (cpu, val);
      break;

    case PC_REGNUM:
      cpu_set_pc (cpu, val);
      break;

    case PSW_REGNUM:
      cpu_set_ccr (cpu, val);
      return 1;

    case PAGE_REGNUM:
      cpu_set_page (cpu, val);
      return 1;

    default:
      break;
    }

  return 2;
}

void
sim_size (int s)
{
  ;
}

void
sim_do_command (SIM_DESC sd, char *cmd)
{
  char *mm_cmd = "memory-map";
  char *int_cmd = "interrupt";
  sim_cpu *cpu;

  cpu = STATE_CPU (sd, 0);
  /* Commands available from GDB:   */
  if (sim_args_command (sd, cmd) != SIM_RC_OK)
    {
      if (strncmp (cmd, "info", sizeof ("info") - 1) == 0)
	sim_get_info (sd, &cmd[4]);
      else if (strncmp (cmd, mm_cmd, strlen (mm_cmd) == 0))
	sim_io_eprintf (sd,
			"`memory-map' command replaced by `sim memory'\n");
      else if (strncmp (cmd, int_cmd, strlen (int_cmd)) == 0)
	sim_io_eprintf (sd, "`interrupt' command replaced by `sim watch'\n");
      else
	sim_io_eprintf (sd, "Unknown command `%s'\n", cmd);
    }

  /* If the architecture changed, re-configure.  */
  if (STATE_ARCHITECTURE (sd) != cpu->cpu_configured_arch)
    sim_hw_configure (sd);
}

/* Halt the simulator after just one instruction */

static void
has_stepped (SIM_DESC sd,
	     void *data)
{
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_engine_halt (sd, NULL, NULL, NULL_CIA, sim_stopped, SIM_SIGTRAP);
}


/* Generic resume - assumes the existance of sim_engine_run */

void
sim_resume (SIM_DESC sd,
	    int step,
	    int siggnal)
{
  sim_engine *engine = STATE_ENGINE (sd);
  jmp_buf buf;
  int jmpval;

  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* we only want to be single stepping the simulator once */
  if (engine->stepper != NULL)
    {
      sim_events_deschedule (sd, engine->stepper);
      engine->stepper = NULL;
    }
  sim_module_resume (sd);

  /* run/resume the simulator */
  engine->jmpbuf = &buf;
  jmpval = setjmp (buf);
  if (jmpval == sim_engine_start_jmpval
      || jmpval == sim_engine_restart_jmpval)
    {
      int last_cpu_nr = sim_engine_last_cpu_nr (sd);
      int next_cpu_nr = sim_engine_next_cpu_nr (sd);
      int nr_cpus = sim_engine_nr_cpus (sd);

      sim_events_preprocess (sd, last_cpu_nr >= nr_cpus, next_cpu_nr >= nr_cpus);
      if (next_cpu_nr >= nr_cpus)
	next_cpu_nr = 0;

      /* Only deliver the siggnal ]sic] the first time through - don't
         re-deliver any siggnal during a restart. */
      if (jmpval == sim_engine_restart_jmpval)
	siggnal = 0;

      /* Install the stepping event after having processed some
         pending events.  This is necessary for HC11/HC12 simulator
         because the tick counter is incremented by the number of cycles
         the instruction took.  Some pending ticks to process can still
         be recorded internally by the simulator and sim_events_preprocess
         will handle them.  If the stepping event is inserted before,
         these pending ticks will raise the event and the simulator will
         stop without having executed any instruction.  */
      if (step)
        engine->stepper = sim_events_schedule (sd, 0, has_stepped, sd);

#ifdef SIM_CPU_EXCEPTION_RESUME
      {
	sim_cpu* cpu = STATE_CPU (sd, next_cpu_nr);
	SIM_CPU_EXCEPTION_RESUME(sd, cpu, siggnal);
      }
#endif

      sim_engine_run (sd, next_cpu_nr, nr_cpus, siggnal);
    }
  engine->jmpbuf = NULL;

  sim_module_suspend (sd);
}
