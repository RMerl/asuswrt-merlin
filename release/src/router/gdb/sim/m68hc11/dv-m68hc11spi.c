/*  dv-m68hc11spi.c -- Simulation of the 68HC11 SPI
    Copyright (C) 2000, 2002, 2003, 2007 Free Software Foundation, Inc.
    Written by Stephane Carrez (stcarrez@nerim.fr)
    (From a driver model Contributed by Cygnus Solutions.)

    This file is part of the program GDB, the GNU debugger.
    
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
#include "dv-sockser.h"
#include "sim-assert.h"


/* DEVICE

        m68hc11spi - m68hc11 SPI interface

   
   DESCRIPTION

        Implements the m68hc11 Synchronous Serial Peripheral Interface
        described in the m68hc11 user guide (Chapter 8 in pink book).
        The SPI I/O controller is directly connected to the CPU
        interrupt.  The simulator implements:

            - SPI clock emulation
            - Data transfer
            - Write collision detection
    

   PROPERTIES

        None

   
   PORTS

   reset (input)

        Reset port. This port is only used to simulate a reset of the SPI
        I/O controller. It should be connected to the RESET output of the cpu.

   */



/* port ID's */

enum
{
  RESET_PORT
};


static const struct hw_port_descriptor m68hc11spi_ports[] = 
{
  { "reset", RESET_PORT, 0, input_port, },
  { NULL, },
};


/* SPI */
struct m68hc11spi 
{
  /* Information about next character to be transmited.  */
  unsigned char tx_char;
  int           tx_bit;
  unsigned char mode;
  
  unsigned char rx_char;
  unsigned char rx_clear_scsr;
  unsigned char clk_pin;
  
  /* SPI clock rate (twice the real clock).  */
  unsigned int clock;
  
  /* Periodic SPI event.  */
  struct hw_event* spi_event;
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc */

static hw_io_read_buffer_method m68hc11spi_io_read_buffer;
static hw_io_write_buffer_method m68hc11spi_io_write_buffer;
static hw_port_event_method m68hc11spi_port_event;
static hw_ioctl_method m68hc11spi_ioctl;

#define M6811_SPI_FIRST_REG (M6811_SPCR)
#define M6811_SPI_LAST_REG  (M6811_SPDR)


static void
attach_m68hc11spi_regs (struct hw *me,
                        struct m68hc11spi *controller)
{
  hw_attach_address (hw_parent (me), M6811_IO_LEVEL, io_map,
                     M6811_SPI_FIRST_REG,
                     M6811_SPI_LAST_REG - M6811_SPI_FIRST_REG + 1,
		     me);
}

static void
m68hc11spi_finish (struct hw *me)
{
  struct m68hc11spi *controller;

  controller = HW_ZALLOC (me, struct m68hc11spi);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, m68hc11spi_io_read_buffer);
  set_hw_io_write_buffer (me, m68hc11spi_io_write_buffer);
  set_hw_ports (me, m68hc11spi_ports);
  set_hw_port_event (me, m68hc11spi_port_event);
#ifdef set_hw_ioctl
  set_hw_ioctl (me, m68hc11spi_ioctl);
#else
  me->to_ioctl = m68hc11spi_ioctl;
#endif

  /* Attach ourself to our parent bus.  */
  attach_m68hc11spi_regs (me, controller);

  /* Initialize to reset state.  */
  controller->spi_event = NULL;
  controller->rx_clear_scsr = 0;
}



/* An event arrives on an interrupt port */

static void
m68hc11spi_port_event (struct hw *me,
                       int my_port,
                       struct hw *source,
                       int source_port,
                       int level)
{
  SIM_DESC sd;
  struct m68hc11spi *controller;
  sim_cpu* cpu;
  unsigned8 val;
  
  controller = hw_data (me);
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);  
  switch (my_port)
    {
    case RESET_PORT:
      {
	HW_TRACE ((me, "SPI reset"));

        /* Reset the state of SPI registers.  */
        controller->rx_clear_scsr = 0;
        if (controller->spi_event)
          {
            hw_event_queue_deschedule (me, controller->spi_event);
            controller->spi_event = 0;
          }

        val = 0;
        m68hc11spi_io_write_buffer (me, &val, io_map,
                                    (unsigned_word) M6811_SPCR, 1);
        break;
      }

    default:
      hw_abort (me, "Event on unknown port %d", my_port);
      break;
    }
}

static void
set_bit_port (struct hw *me, sim_cpu *cpu, int port, int mask, int value)
{
  uint8 val;
  
  if (value)
    val = cpu->ios[port] | mask;
  else
    val = cpu->ios[port] & ~mask;

  /* Set the new value and post an event to inform other devices
     that pin 'port' changed.  */
  m68hc11cpu_set_port (me, cpu, port, val);
}


/* When a character is sent/received by the SPI, the PD2..PD5 line
   are driven by the following signals:

	      B7	B6
      -----+---------+--------+---/-+-------
 MOSI      |    |    |   |    |     |
 MISO	   +---------+--------+---/-+
		____      ___
 CLK	_______/    \____/   \__		CPOL=0, CPHA=0
	_______	     ____     __
	       \____/    \___/			CPOL=1, CPHA=0
	   ____	     ____     __
	__/    \____/    \___/			CPOL=0, CPHA=1
	__	____      ___
	  \____/    \____/   \__		CPOL=1, CPHA=1

 SS ___                                 ____
       \__________________________//___/

 MISO = PD2
 MOSI = PD3
 SCK  = PD4
 SS   = PD5

*/

#define SPI_START_BYTE 0
#define SPI_START_BIT  1
#define SPI_MIDDLE_BIT 2

void
m68hc11spi_clock (struct hw *me, void *data)
{
  SIM_DESC sd;
  struct m68hc11spi* controller;
  sim_cpu *cpu;
  int check_interrupt = 0;
  
  controller = hw_data (me);
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);

  /* Cleanup current event.  */
  if (controller->spi_event)
    {
      hw_event_queue_deschedule (me, controller->spi_event);
      controller->spi_event = 0;
    }

  /* Change a bit of data at each two SPI event.  */
  if (controller->mode == SPI_START_BIT)
    {
      /* Reflect the bit value on bit 2 of port D.  */
      set_bit_port (me, cpu, M6811_PORTD, (1 << 2),
                    (controller->tx_char & (1 << controller->tx_bit)));
      controller->tx_bit--;
      controller->mode = SPI_MIDDLE_BIT;
    }
  else if (controller->mode == SPI_MIDDLE_BIT)
    {
      controller->mode = SPI_START_BIT;
    }

  if (controller->mode == SPI_START_BYTE)
    {
      /* Start a new SPI transfer.  */
      
      /* TBD: clear SS output.  */
      controller->mode = SPI_START_BIT;
      controller->tx_bit = 7;
      set_bit_port (me, cpu, M6811_PORTD, (1 << 4), ~controller->clk_pin);
    }
  else
    {
      /* Change the SPI clock at each event on bit 4 of port D.  */
      controller->clk_pin = ~controller->clk_pin;
      set_bit_port (me, cpu, M6811_PORTD, (1 << 4), controller->clk_pin);
    }
  
  /* Transmit is now complete for this byte.  */
  if (controller->mode == SPI_START_BIT && controller->tx_bit < 0)
    {
      controller->rx_clear_scsr = 0;
      cpu->ios[M6811_SPSR] |= M6811_SPIF;
      if (cpu->ios[M6811_SPCR] & M6811_SPIE)
        check_interrupt = 1;
    }
  else
    {
      controller->spi_event = hw_event_queue_schedule (me, controller->clock,
                                                       m68hc11spi_clock,
                                                       NULL);
    }

  if (check_interrupt)
    interrupts_update_pending (&cpu->cpu_interrupts);
}

/* Flags of the SPCR register.  */
io_reg_desc spcr_desc[] = {
  { M6811_SPIE, "SPIE ", "Serial Peripheral Interrupt Enable" },
  { M6811_SPE,  "SPE  ",  "Serial Peripheral System Enable" },
  { M6811_DWOM, "DWOM ", "Port D Wire-OR mode option" },
  { M6811_MSTR, "MSTR ", "Master Mode Select" },
  { M6811_CPOL, "CPOL ", "Clock Polarity" },
  { M6811_CPHA, "CPHA ", "Clock Phase" },
  { M6811_SPR1, "SPR1 ", "SPI Clock Rate Select" },
  { M6811_SPR0, "SPR0 ", "SPI Clock Rate Select" },
  { 0,  0, 0 }
};


/* Flags of the SPSR register.  */
io_reg_desc spsr_desc[] = {
  { M6811_SPIF,	"SPIF ", "SPI Transfer Complete flag" },
  { M6811_WCOL, "WCOL ", "Write Collision" },
  { M6811_MODF, "MODF ", "Mode Fault" },
  { 0,  0, 0 }
};

static void
m68hc11spi_info (struct hw *me)
{
  SIM_DESC sd;
  uint16 base = 0;
  sim_cpu *cpu;
  struct m68hc11spi *controller;
  uint8 val;
  
  sd = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);
  
  sim_io_printf (sd, "M68HC11 SPI:\n");

  base = cpu_get_io_base (cpu);

  val = cpu->ios[M6811_SPCR];
  print_io_byte (sd, "SPCR", spcr_desc, val, base + M6811_SPCR);
  sim_io_printf (sd, "\n");

  val = cpu->ios[M6811_SPSR];
  print_io_byte (sd, "SPSR", spsr_desc, val, base + M6811_SPSR);
  sim_io_printf (sd, "\n");

  if (controller->spi_event)
    {
      signed64 t;

      sim_io_printf (sd, "  SPI has %d bits to send\n",
                     controller->tx_bit + 1);
      t = hw_event_remain_time (me, controller->spi_event);
      sim_io_printf (sd, "  SPI current bit-cycle finished in %s\n",
		     cycle_to_string (cpu, t, PRINT_TIME | PRINT_CYCLE));

      t += (controller->tx_bit + 1) * 2 * controller->clock;
      sim_io_printf (sd, "  SPI operation finished in %s\n",
		     cycle_to_string (cpu, t, PRINT_TIME | PRINT_CYCLE));
    }
}

static int
m68hc11spi_ioctl (struct hw *me,
                  hw_ioctl_request request,
                  va_list ap)
{
  m68hc11spi_info (me);
  return 0;
}

/* generic read/write */

static unsigned
m68hc11spi_io_read_buffer (struct hw *me,
                           void *dest,
                           int space,
                           unsigned_word base,
                           unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11spi *controller;
  sim_cpu *cpu;
  unsigned8 val;
  
  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  sd  = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);

  switch (base)
    {
    case M6811_SPSR:
      controller->rx_clear_scsr = cpu->ios[M6811_SCSR]
        & (M6811_SPIF | M6811_WCOL | M6811_MODF);
      
    case M6811_SPCR:
      val = cpu->ios[base];
      break;
      
    case M6811_SPDR:
      if (controller->rx_clear_scsr)
        {
          cpu->ios[M6811_SPSR] &= ~controller->rx_clear_scsr;
          controller->rx_clear_scsr = 0;
          interrupts_update_pending (&cpu->cpu_interrupts);
        }
      val = controller->rx_char;
      break;
      
    default:
      return 0;
    }
  *((unsigned8*) dest) = val;
  return 1;
}

static unsigned
m68hc11spi_io_write_buffer (struct hw *me,
                            const void *source,
                            int space,
                            unsigned_word base,
                            unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11spi *controller;
  sim_cpu *cpu;
  unsigned8 val;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));

  sd  = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);
  
  val = *((const unsigned8*) source);
  switch (base)
    {
    case M6811_SPCR:
      cpu->ios[M6811_SPCR] = val;

      /* The SPI clock rate is 2, 4, 16, 32 of the internal CPU clock.
         We have to drive the clock pin and need a 2x faster clock.  */
      switch (val & (M6811_SPR1 | M6811_SPR0))
        {
        case 0:
          controller->clock = 1;
          break;

        case 1:
          controller->clock = 2;
          break;

        case 2:
          controller->clock = 8;
          break;

        default:
          controller->clock = 16;
          break;
        }

      /* Set the clock pin.  */
      if ((val & M6811_CPOL)
          && (controller->spi_event == 0
              || ((val & M6811_CPHA) && controller->mode == 1)))
        controller->clk_pin = 1;
      else
        controller->clk_pin = 0;

      set_bit_port (me, cpu, M6811_PORTD, (1 << 4), controller->clk_pin);
      break;
      
      /* Can't write to SPSR.  */
    case M6811_SPSR:
      break;
      
    case M6811_SPDR:
      if (!(cpu->ios[M6811_SPCR] & M6811_SPE))
        {
          return 0;
        }

      if (controller->rx_clear_scsr)
        {
          cpu->ios[M6811_SPSR] &= ~controller->rx_clear_scsr;
          controller->rx_clear_scsr = 0;
          interrupts_update_pending (&cpu->cpu_interrupts);
        }

      /* If transfer is taking place, a write to SPDR
         generates a collision.  */
      if (controller->spi_event)
        {
          cpu->ios[M6811_SPSR] |= M6811_WCOL;
          break;
        }

      /* Refuse the write if there was no read of SPSR.  */
      /* ???? TBD. */

      /* Prepare to send a byte.  */
      controller->tx_char = val;
      controller->mode   = SPI_START_BYTE;

      /* Toggle clock pin internal value when CPHA is 0 so that
         it will really change in the middle of a bit.  */
      if (!(cpu->ios[M6811_SPCR] & M6811_CPHA))
        controller->clk_pin = ~controller->clk_pin;

      cpu->ios[M6811_SPDR] = val;

      /* Activate transmission.  */
      m68hc11spi_clock (me, NULL);
      break;

    default:
      return 0;
    }
  return nr_bytes;
}     


const struct hw_descriptor dv_m68hc11spi_descriptor[] = {
  { "m68hc11spi", m68hc11spi_finish },
  { "m68hc12spi", m68hc11spi_finish },
  { NULL },
};

