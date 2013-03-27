/*  dv-m68hc11eepr.c -- Simulation of the 68HC11 Internal EEPROM.
    Copyright (C) 1999, 2000, 2001, 2002, 2007 Free Software Foundation, Inc.
    Written by Stephane Carrez (stcarrez@nerim.fr)
    (From a driver model Contributed by Cygnus Solutions.)
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    */


#include "sim-main.h"
#include "hw-main.h"
#include "sim-assert.h"
#include "sim-events.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>



/* DEVICE

        m68hc11eepr - m68hc11 EEPROM

   
   DESCRIPTION

        Implements the 68HC11 eeprom device described in the m68hc11
        user guide (Chapter 4 in the pink book).


   PROPERTIES

   reg <base> <length>

        Base of eeprom and its length.

   file <path>

        Path of the EEPROM file.  The default is 'm6811.eeprom'.


   PORTS

        None

   */



/* static functions */


/* port ID's */

enum
{
  RESET_PORT
};


static const struct hw_port_descriptor m68hc11eepr_ports[] = 
{
  { "reset", RESET_PORT, 0, input_port, },
  { NULL, },
};



/* The timer/counter register internal state.  Note that we store
   state using the control register images, in host endian order.  */

struct m68hc11eepr 
{
  address_word  base_address; /* control register base */
  int           attach_space;
  unsigned      size;
  int           mapped;
  
  /* Current state of the eeprom programing:
     - eeprom_wmode indicates whether the EEPROM address and byte have
       been latched.
     - eeprom_waddr indicates the EEPROM address that was latched
       and eeprom_wbyte is the byte that was latched.
     - eeprom_wcycle indicates the CPU absolute cycle type when
       the high voltage was applied (successfully) on the EEPROM.
   
     These data members are setup only when we detect good EEPROM programing
     conditions (see Motorola EEPROM Programming and PPROG register usage).
     When the high voltage is switched off, we look at the CPU absolute
     cycle time to see if the EEPROM command must succeeds or not.
     The EEPROM content is updated and saved only at that time.
     (EEPROM command is: byte zero bits program, byte erase, row erase
     and bulk erase).
    
     The CONFIG register is programmed in the same way.  It is physically
     located at the end of the EEPROM (eeprom size + 1).  It is not mapped
     in memory but it's saved in the EEPROM file.  */
  unsigned long		eeprom_wcycle;
  uint16		eeprom_waddr;
  uint8			eeprom_wbyte;
  uint8			eeprom_wmode;

  uint8*		eeprom;
  
  /* Minimum time in CPU cycles for programming the EEPROM.  */
  unsigned long         eeprom_min_cycles;

  const char*           file_name;
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc.  */

static hw_io_read_buffer_method m68hc11eepr_io_read_buffer;
static hw_io_write_buffer_method m68hc11eepr_io_write_buffer;
static hw_ioctl_method m68hc11eepr_ioctl;

/* Read or write the memory bank content from/to a file.
   Returns 0 if the operation succeeded and -1 if it failed.  */
static int
m6811eepr_memory_rw (struct m68hc11eepr *controller, int mode)
{
  const char *name = controller->file_name;
  int fd;
  size_t size;
  
  size = controller->size;
  fd = open (name, mode, 0644);
  if (fd < 0)
    {
      if (mode == O_RDONLY)
        {
          memset (controller->eeprom, 0xFF, size);
          /* Default value for CONFIG register (0xFF should be ok):
             controller->eeprom[size - 1] = M6811_NOSEC | M6811_NOCOP
                                            | M6811_ROMON | M6811_EEON;  */
          return 0;
        }
      return -1;
    }

  if (mode == O_RDONLY)
    {
      if (read (fd, controller->eeprom, size) != size)
	{
	  close (fd);
	  return -1;
	}
    }
  else
    {
      if (write (fd, controller->eeprom, size) != size)
	{
	  close (fd);
	  return -1;
	}
    }
  close (fd);

  return 0;
}




static void
attach_m68hc11eepr_regs (struct hw *me,
                         struct m68hc11eepr *controller)
{
  unsigned_word attach_address;
  int attach_space;
  unsigned attach_size;
  reg_property_spec reg;

  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property (me, "reg", 0, &reg))
    hw_abort (me, "\"reg\" property must contain one addr/size entry");

  hw_unit_address_to_attach_address (hw_parent (me),
				     &reg.address,
				     &attach_space,
				     &attach_address,
				     me);
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &attach_size, me);

  /* Attach the two IO registers that control the EEPROM.
     The EEPROM is only attached at reset time because it may
     be enabled/disabled by the EEON bit in the CONFIG register.  */
  hw_attach_address (hw_parent (me), M6811_IO_LEVEL,
                     io_map, M6811_PPROG, 1, me);
  hw_attach_address (hw_parent (me), M6811_IO_LEVEL,
                     io_map, M6811_CONFIG, 1, me);

  if (hw_find_property (me, "file") == NULL)
    controller->file_name = "m6811.eeprom";
  else
    controller->file_name = hw_find_string_property (me, "file");
  
  controller->attach_space = attach_space;
  controller->base_address = attach_address;
  controller->eeprom = (char*) hw_malloc (me, attach_size + 1);
  controller->eeprom_min_cycles = 10000;
  controller->size = attach_size + 1;
  controller->mapped = 0;
  
  m6811eepr_memory_rw (controller, O_RDONLY);
}


/* An event arrives on an interrupt port.  */

static void
m68hc11eepr_port_event (struct hw *me,
                        int my_port,
                        struct hw *source,
                        int source_port,
                        int level)
{
  SIM_DESC sd;
  struct m68hc11eepr *controller;
  sim_cpu *cpu;
  
  controller = hw_data (me);
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);
  switch (my_port)
    {
    case RESET_PORT:
      {
	HW_TRACE ((me, "EEPROM reset"));

        /* Re-read the EEPROM from the file.  This gives the chance
           to users to erase this file before doing a reset and have
           a fresh EEPROM taken into account.  */
        m6811eepr_memory_rw (controller, O_RDONLY);

        /* Reset the state of EEPROM programmer.  The CONFIG register
           is also initialized from the EEPROM/file content.  */
        cpu->ios[M6811_PPROG]    = 0;
        if (cpu->cpu_use_local_config)
          cpu->ios[M6811_CONFIG] = cpu->cpu_config;
        else
          cpu->ios[M6811_CONFIG]   = controller->eeprom[controller->size-1];
        controller->eeprom_wmode = 0;
        controller->eeprom_waddr = 0;
        controller->eeprom_wbyte = 0;

        /* Attach or detach to the bus depending on the EEPROM enable bit.
           The EEPROM CONFIG register is still enabled and can be programmed
           for a next configuration (taken into account only after a reset,
           see Motorola spec).  */
        if (!(cpu->ios[M6811_CONFIG] & M6811_EEON))
          {
            if (controller->mapped)
              hw_detach_address (hw_parent (me), M6811_EEPROM_LEVEL,
                                 controller->attach_space,
                                 controller->base_address,
                                 controller->size - 1,
                                 me);
            controller->mapped = 0;
          }
        else
          {
            if (!controller->mapped)
              hw_attach_address (hw_parent (me), M6811_EEPROM_LEVEL,
                                 controller->attach_space,
                                 controller->base_address,
                                 controller->size - 1,
                                 me);
            controller->mapped = 1;
          }
        break;
      }

    default:
      hw_abort (me, "Event on unknown port %d", my_port);
      break;
    }
}


static void
m68hc11eepr_finish (struct hw *me)
{
  struct m68hc11eepr *controller;

  controller = HW_ZALLOC (me, struct m68hc11eepr);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, m68hc11eepr_io_read_buffer);
  set_hw_io_write_buffer (me, m68hc11eepr_io_write_buffer);
  set_hw_ports (me, m68hc11eepr_ports);
  set_hw_port_event (me, m68hc11eepr_port_event);
#ifdef set_hw_ioctl
  set_hw_ioctl (me, m68hc11eepr_ioctl);
#else
  me->to_ioctl = m68hc11eepr_ioctl;
#endif

  attach_m68hc11eepr_regs (me, controller);
}
 


static io_reg_desc pprog_desc[] = {
  { M6811_BYTE,  "BYTE  ", "Byte Program Mode" },
  { M6811_ROW,   "ROW   ", "Row Program Mode" },
  { M6811_ERASE, "ERASE ", "Erase Mode" },
  { M6811_EELAT, "EELAT ", "EEProm Latch Control" },
  { M6811_EEPGM, "EEPGM ", "EEProm Programming Voltable Enable" },
  { 0,  0, 0 }
};
extern io_reg_desc config_desc[];


/* Describe the state of the EEPROM device.  */
static void
m68hc11eepr_info (struct hw *me)
{
  SIM_DESC sd;
  uint16 base = 0;
  sim_cpu *cpu;
  struct m68hc11eepr *controller;
  uint8 val;
  
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);
  controller = hw_data (me);
  base       = cpu_get_io_base (cpu);
  
  sim_io_printf (sd, "M68HC11 EEprom:\n");

  val = cpu->ios[M6811_PPROG];
  print_io_byte (sd, "PPROG  ", pprog_desc, val, base + M6811_PPROG);
  sim_io_printf (sd, "\n");

  val = cpu->ios[M6811_CONFIG];
  print_io_byte (sd, "CONFIG ", config_desc, val, base + M6811_CONFIG);
  sim_io_printf (sd, "\n");

  val = controller->eeprom[controller->size - 1];
  print_io_byte (sd, "(*NEXT*) ", config_desc, val, base + M6811_CONFIG);
  sim_io_printf (sd, "\n");
  
  /* Describe internal state of EEPROM.  */
  if (controller->eeprom_wmode)
    {
      if (controller->eeprom_waddr == controller->size - 1)
        sim_io_printf (sd, "  Programming CONFIG register ");
      else
        sim_io_printf (sd, "  Programming: 0x%04x ",
                       controller->eeprom_waddr + controller->base_address);

      sim_io_printf (sd, "with 0x%02x\n",
		     controller->eeprom_wbyte);
    }

  sim_io_printf (sd, "  EEProm file: %s\n",
                 controller->file_name);
}

static int
m68hc11eepr_ioctl (struct hw *me,
		   hw_ioctl_request request,
		   va_list ap)
{
  m68hc11eepr_info (me);
  return 0;
}

/* generic read/write */

static unsigned
m68hc11eepr_io_read_buffer (struct hw *me,
			    void *dest,
			    int space,
			    unsigned_word base,
			    unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11eepr *controller;
  sim_cpu *cpu;
  
  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  sd         = hw_system (me);
  controller = hw_data (me);
  cpu        = STATE_CPU (sd, 0);

  if (space == io_map)
    {
      unsigned cnt = 0;
      
      while (nr_bytes != 0)
        {
          switch (base)
            {
            case M6811_PPROG:
            case M6811_CONFIG:
              *((uint8*) dest) = cpu->ios[base];
              break;

            default:
              hw_abort (me, "reading wrong register 0x%04x", base);
            }
          dest = (uint8*) (dest) + 1;
          base++;
          nr_bytes--;
          cnt++;
        }
      return cnt;
    }

  /* In theory, we can't read the EEPROM when it's being programmed.  */
  if ((cpu->ios[M6811_PPROG] & M6811_EELAT) != 0
      && cpu_is_running (cpu))
    {
      sim_memory_error (cpu, SIM_SIGBUS, base,
			"EEprom not configured for reading");
    }

  base = base - controller->base_address;
  memcpy (dest, &controller->eeprom[base], nr_bytes);
  return nr_bytes;
}


static unsigned
m68hc11eepr_io_write_buffer (struct hw *me,
			     const void *source,
			     int space,
			     unsigned_word base,
			     unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11eepr *controller;
  sim_cpu *cpu;
  uint8 val;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));

  sd         = hw_system (me);
  controller = hw_data (me);
  cpu        = STATE_CPU (sd, 0);

  /* Programming several bytes at a time is not possible.  */
  if (space != io_map && nr_bytes != 1)
    {
      sim_memory_error (cpu, SIM_SIGBUS, base,
			"EEprom write error (only 1 byte can be programmed)");
      return 0;
    }
  
  if (nr_bytes != 1)
    hw_abort (me, "Cannot write more than 1 byte to EEPROM device at a time");

  val = *((const uint8*) source);

  /* Write to the EEPROM control register.  */
  if (space == io_map && base == M6811_PPROG)
    {
      uint8 wrong_bits;
      uint16 addr;
      
      addr = base + cpu_get_io_base (cpu);

      /* Setting EELAT and EEPGM at the same time is an error.
         Clearing them both is ok.  */
      wrong_bits = (cpu->ios[M6811_PPROG] ^ val) & val;
      wrong_bits &= (M6811_EELAT | M6811_EEPGM);

      if (wrong_bits == (M6811_EEPGM|M6811_EELAT))
	{
	  sim_memory_error (cpu, SIM_SIGBUS, addr,
			    "Wrong eeprom programing value");
	  return 0;
	}

      if ((val & M6811_EELAT) == 0)
	{
	  val = 0;
	}
      if ((val & M6811_EEPGM) && !(cpu->ios[M6811_PPROG] & M6811_EELAT))
	{
	  sim_memory_error (cpu, SIM_SIGBUS, addr,
			    "EEProm high voltage applied after EELAT");
	}
      if ((val & M6811_EEPGM) && controller->eeprom_wmode == 0)
	{
	  sim_memory_error (cpu, SIM_SIGSEGV, addr,
			    "EEProm high voltage applied without address");
	}
      if (val & M6811_EEPGM)
	{
	  controller->eeprom_wcycle = cpu_current_cycle (cpu);
	}
      else if (cpu->ios[M6811_PPROG] & M6811_PPROG)
	{
	  int i;
	  unsigned long t = cpu_current_cycle (cpu);

	  t -= controller->eeprom_wcycle;
	  if (t < controller->eeprom_min_cycles)
	    {
	      sim_memory_error (cpu, SIM_SIGILL, addr,
				"EEprom programmed only for %lu cycles",
				t);
	    }

	  /* Program the byte by clearing some bits.  */
	  if (!(cpu->ios[M6811_PPROG] & M6811_ERASE))
	    {
	      controller->eeprom[controller->eeprom_waddr]
		&= controller->eeprom_wbyte;
	    }

	  /* Erase a byte, row or the complete eeprom.  Erased value is 0xFF.
             Ignore row or complete eeprom erase when we are programming the
             CONFIG register (last EEPROM byte).  */
	  else if ((cpu->ios[M6811_PPROG] & M6811_BYTE)
                   || controller->eeprom_waddr == controller->size - 1)
	    {
	      controller->eeprom[controller->eeprom_waddr] = 0xff;
	    }
	  else if (cpu->ios[M6811_BYTE] & M6811_ROW)
	    {
              size_t max_size;

              /* Size of EEPROM (-1 because the last byte is the
                 CONFIG register.  */
              max_size = controller->size;
	      controller->eeprom_waddr &= 0xFFF0;
	      for (i = 0; i < 16
                     && controller->eeprom_waddr < max_size; i++)
		{
		  controller->eeprom[controller->eeprom_waddr] = 0xff;
		  controller->eeprom_waddr ++;
		}
	    }
	  else
	    {
              size_t max_size;

              max_size = controller->size;
	      for (i = 0; i < max_size; i++)
		{
		  controller->eeprom[i] = 0xff;
		}
	    }

	  /* Save the eeprom in a file.  We have to save after each
	     change because the simulator can be stopped or crash...  */
	  if (m6811eepr_memory_rw (controller, O_WRONLY | O_CREAT) != 0)
	    {
	      sim_memory_error (cpu, SIM_SIGABRT, addr,
				"EEPROM programing failed: errno=%d", errno);
	    }
	  controller->eeprom_wmode = 0;
	}
      cpu->ios[M6811_PPROG] = val;
      return 1;
    }

  /* The CONFIG IO register is mapped at end of EEPROM.
     It's not visible.  */
  if (space == io_map && base == M6811_CONFIG)
    {
      base = controller->size - 1;
    }
  else
    {
      base = base - controller->base_address;
    }

  /* Writing the memory is allowed for the Debugger or simulator
     (cpu not running).  */
  if (cpu_is_running (cpu))
    {
      if ((cpu->ios[M6811_PPROG] & M6811_EELAT) == 0)
	{
	  sim_memory_error (cpu, SIM_SIGSEGV, base,
			    "EEprom not configured for writing");
	  return 0;
	}
      if (controller->eeprom_wmode != 0)
	{
	  sim_memory_error (cpu, SIM_SIGSEGV, base,
			    "EEprom write error");
	  return 0;
	}
      controller->eeprom_wmode = 1;
      controller->eeprom_waddr = base;
      controller->eeprom_wbyte = val;
    }
  else
    {
      controller->eeprom[base] = val;
      m6811eepr_memory_rw (controller, O_WRONLY);
    }
  
  return 1;
}

const struct hw_descriptor dv_m68hc11eepr_descriptor[] = {
  { "m68hc11eepr", m68hc11eepr_finish },
  { "m68hc12eepr", m68hc11eepr_finish },
  { NULL },
};

