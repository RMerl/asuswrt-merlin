/*  dv-m68hc11sio.c -- Simulation of the 68HC11 serial device.
    Copyright (C) 1999, 2000, 2001, 2007 Free Software Foundation, Inc.
    Written by Stephane Carrez (stcarrez@worldnet.fr)
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

        m68hc11sio - m68hc11 serial I/O

   
   DESCRIPTION

        Implements the m68hc11 serial I/O controller described in the m68hc11
        user guide. The serial I/O controller is directly connected to the CPU
        interrupt. The simulator implements:

            - baud rate emulation
            - 8-bits transfers
    
   PROPERTIES

   backend {tcp | stdio}

        Use dv-sockser TCP-port backend or stdio for backend.  Default: stdio.

   
   PORTS

   reset (input)

        Reset port. This port is only used to simulate a reset of the serial
        I/O controller. It should be connected to the RESET output of the cpu.

   */



/* port ID's */

enum
{
  RESET_PORT
};


static const struct hw_port_descriptor m68hc11sio_ports[] = 
{
  { "reset", RESET_PORT, 0, input_port, },
  { NULL, },
};


/* Serial Controller information.  */
struct m68hc11sio 
{
  enum {sio_tcp, sio_stdio} backend; /* backend */

  /* Number of cpu cycles to send a bit on the wire.  */
  unsigned long baud_cycle;

  /* Length in bits of characters sent, this includes the
     start/stop and parity bits.  Together with baud_cycle, this
     is used to find the number of cpu cycles to send/receive a data.  */
  unsigned int  data_length;

  /* Information about next character to be transmited.  */
  unsigned char tx_has_char;
  unsigned char tx_char;

  unsigned char rx_char;
  unsigned char rx_clear_scsr;
  
  /* Periodic I/O polling.  */
  struct hw_event* tx_poll_event;
  struct hw_event* rx_poll_event;
};



/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc.  */

static hw_io_read_buffer_method m68hc11sio_io_read_buffer;
static hw_io_write_buffer_method m68hc11sio_io_write_buffer;
static hw_port_event_method m68hc11sio_port_event;
static hw_ioctl_method m68hc11sio_ioctl;

#define M6811_SCI_FIRST_REG (M6811_BAUD)
#define M6811_SCI_LAST_REG  (M6811_SCDR)


static void
attach_m68hc11sio_regs (struct hw *me,
                        struct m68hc11sio *controller)
{
  hw_attach_address (hw_parent (me), M6811_IO_LEVEL, io_map,
                     M6811_SCI_FIRST_REG,
                     M6811_SCI_LAST_REG - M6811_SCI_FIRST_REG + 1,
		     me);

  if (hw_find_property(me, "backend") != NULL)
    {
      const char *value = hw_find_string_property(me, "backend");
      if(! strcmp(value, "tcp"))
	controller->backend = sio_tcp;
      else if(! strcmp(value, "stdio"))
	controller->backend = sio_stdio;
      else
	hw_abort (me, "illegal value for backend parameter `%s':"
                  "use tcp or stdio", value);
    }
}


static void
m68hc11sio_finish (struct hw *me)
{
  struct m68hc11sio *controller;

  controller = HW_ZALLOC (me, struct m68hc11sio);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, m68hc11sio_io_read_buffer);
  set_hw_io_write_buffer (me, m68hc11sio_io_write_buffer);
  set_hw_ports (me, m68hc11sio_ports);
  set_hw_port_event (me, m68hc11sio_port_event);
#ifdef set_hw_ioctl
  set_hw_ioctl (me, m68hc11sio_ioctl);
#else
  me->to_ioctl = m68hc11sio_ioctl;
#endif

  /* Preset defaults.  */
  controller->backend = sio_stdio;

  /* Attach ourself to our parent bus.  */
  attach_m68hc11sio_regs (me, controller);

  /* Initialize to reset state.  */
  controller->tx_poll_event = NULL;
  controller->rx_poll_event = NULL;
  controller->tx_char       = 0;
  controller->tx_has_char   = 0;
  controller->rx_clear_scsr = 0;
  controller->rx_char       = 0;
}



/* An event arrives on an interrupt port.  */

static void
m68hc11sio_port_event (struct hw *me,
                       int my_port,
                       struct hw *source,
                       int source_port,
                       int level)
{
  SIM_DESC sd;
  struct m68hc11sio *controller;
  sim_cpu *cpu;
  unsigned8 val;
  
  controller = hw_data (me);
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);  
  switch (my_port)
    {
    case RESET_PORT:
      {
	HW_TRACE ((me, "SCI reset"));

        /* Reset the state of SCI registers.  */
        val = 0;
        m68hc11sio_io_write_buffer (me, &val, io_map,
                                    (unsigned_word) M6811_BAUD, 1);
        m68hc11sio_io_write_buffer (me, &val, io_map,
                                    (unsigned_word) M6811_SCCR1, 1);
        m68hc11sio_io_write_buffer (me, &val, io_map,
                                    (unsigned_word) M6811_SCCR2, 1);
        
        cpu->ios[M6811_SCSR]    = M6811_TC | M6811_TDRE;
        controller->rx_char     = 0;
        controller->tx_char     = 0;
        controller->tx_has_char = 0;
        controller->rx_clear_scsr = 0;
        if (controller->rx_poll_event)
          {
            hw_event_queue_deschedule (me, controller->rx_poll_event);
            controller->rx_poll_event = 0;
          }
        if (controller->tx_poll_event)
          {
            hw_event_queue_deschedule (me, controller->tx_poll_event);
            controller->tx_poll_event = 0;
          }

        /* In bootstrap mode, initialize the SCI to 1200 bauds to
           simulate some initial setup by the internal rom.  */
        if (((cpu->ios[M6811_HPRIO]) & (M6811_SMOD | M6811_MDA)) == M6811_SMOD)
          {
            unsigned char val = 0x33;
            
            m68hc11sio_io_write_buffer (me, &val, io_map,
                                        (unsigned_word) M6811_BAUD, 1);
            val = 0x12;
            m68hc11sio_io_write_buffer (me, &val, io_map,
                                        (unsigned_word) M6811_SCCR2, 1);
          }
        break;
      }

    default:
      hw_abort (me, "Event on unknown port %d", my_port);
      break;
    }
}


void
m68hc11sio_rx_poll (struct hw *me, void *data)
{
  SIM_DESC sd;
  struct m68hc11sio *controller;
  sim_cpu *cpu;
  char cc;
  int cnt;
  int check_interrupt = 0;
  
  controller = hw_data (me);
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);
  switch (controller->backend)
    {
    case sio_tcp:
      cnt = dv_sockser_read (sd);
      if (cnt != -1)
        {
          cc = (char) cnt;
          cnt = 1;
        }
      break;

    case sio_stdio:
      cnt = sim_io_poll_read (sd, 0 /* stdin */, &cc, 1);
      break;

    default:
      cnt = 0;
      break;
    }

  if (cnt == 1)
    {
      /* Raise the overrun flag if the previous character was not read.  */
      if (cpu->ios[M6811_SCSR] & M6811_RDRF)
        cpu->ios[M6811_SCSR] |= M6811_OR;

      cpu->ios[M6811_SCSR]     |= M6811_RDRF;
      controller->rx_char       = cc;
      controller->rx_clear_scsr = 0;
      check_interrupt = 1;
    }
  else
    {
      /* handle idle line detect here.  */
      ;
    }

  if (controller->rx_poll_event)
    {
      hw_event_queue_deschedule (me, controller->rx_poll_event);
      controller->rx_poll_event = 0;
    }

  if (cpu->ios[M6811_SCCR2] & M6811_RE)
    {
      unsigned long clock_cycle;

      /* Compute CPU clock cycles to wait for the next character.  */
      clock_cycle = controller->data_length * controller->baud_cycle;

      controller->rx_poll_event = hw_event_queue_schedule (me, clock_cycle,
                                                           m68hc11sio_rx_poll,
                                                           NULL);
    }

  if (check_interrupt)
      interrupts_update_pending (&cpu->cpu_interrupts);
}


void
m68hc11sio_tx_poll (struct hw *me, void *data)
{
  SIM_DESC sd;
  struct m68hc11sio *controller;
  sim_cpu *cpu;
  
  controller = hw_data (me);
  sd         = hw_system (me);
  cpu        = STATE_CPU (sd, 0);

  cpu->ios[M6811_SCSR] |= M6811_TDRE;
  cpu->ios[M6811_SCSR] |= M6811_TC;
  
  /* Transmitter is enabled and we have something to send.  */
  if ((cpu->ios[M6811_SCCR2] & M6811_TE) && controller->tx_has_char)
    {
      cpu->ios[M6811_SCSR] &= ~M6811_TDRE;
      cpu->ios[M6811_SCSR] &= ~M6811_TC;
      controller->tx_has_char = 0;
      switch (controller->backend)
        {
        case sio_tcp:
          dv_sockser_write (sd, controller->tx_char);
          break;

        case sio_stdio:
          sim_io_write_stdout (sd, &controller->tx_char, 1);
          sim_io_flush_stdout (sd);
          break;

        default:
          break;
        }
    }

  if (controller->tx_poll_event)
    {
      hw_event_queue_deschedule (me, controller->tx_poll_event);
      controller->tx_poll_event = 0;
    }
  
  if ((cpu->ios[M6811_SCCR2] & M6811_TE)
      && ((cpu->ios[M6811_SCSR] & M6811_TC) == 0))
    {
      unsigned long clock_cycle;
      
      /* Compute CPU clock cycles to wait for the next character.  */
      clock_cycle = controller->data_length * controller->baud_cycle;

      controller->tx_poll_event = hw_event_queue_schedule (me, clock_cycle,
                                                           m68hc11sio_tx_poll,
                                                           NULL);
    }

  interrupts_update_pending (&cpu->cpu_interrupts);
}

/* Descriptions of the SIO I/O ports.  These descriptions are only used to
   give information of the SIO device under GDB.  */
io_reg_desc sccr2_desc[] = {
  { M6811_TIE,   "TIE  ", "Transmit Interrupt Enable" },
  { M6811_TCIE,  "TCIE ", "Transmit Complete Interrupt Enable" },
  { M6811_RIE,   "RIE  ", "Receive Interrupt Enable" },
  { M6811_ILIE,  "ILIE ", "Idle Line Interrupt Enable" },
  { M6811_TE,    "TE   ", "Transmit Enable" },
  { M6811_RE,    "RE   ", "Receive Enable" },
  { M6811_RWU,   "RWU  ", "Receiver Wake Up" },
  { M6811_SBK,   "SBRK ", "Send Break" },
  { 0,  0, 0 }
};

io_reg_desc sccr1_desc[] = {
  { M6811_R8,    "R8   ", "Receive Data bit 8" },
  { M6811_T8,    "T8   ", "Transmit Data bit 8" },
  { M6811_M,     "M    ", "SCI Character length (0=8-bits, 1=9-bits)" },
  { M6811_WAKE,  "WAKE ", "Wake up method select (0=idle, 1=addr mark" },
  { 0,  0, 0 }
};

io_reg_desc scsr_desc[] = {
  { M6811_TDRE,  "TDRE ", "Transmit Data Register Empty" },
  { M6811_TC,    "TC   ", "Transmit Complete" },
  { M6811_RDRF,  "RDRF ", "Receive Data Register Full" },
  { M6811_IDLE,  "IDLE ", "Idle Line Detect" },
  { M6811_OR,    "OR   ", "Overrun Error" },
  { M6811_NF,    "NF   ", "Noise Flag" },
  { M6811_FE,    "FE   ", "Framing Error" },
  { 0,  0, 0 }
};

io_reg_desc baud_desc[] = {
  { M6811_TCLR,  "TCLR ", "Clear baud rate (test mode)" },
  { M6811_SCP1,  "SCP1 ", "SCI baud rate prescaler select (SCP1)" },
  { M6811_SCP0,  "SCP0 ", "SCI baud rate prescaler select (SCP0)" },
  { M6811_RCKB,  "RCKB ", "Baur Rate Clock Check (test mode)" },
  { M6811_SCR2,  "SCR2 ", "SCI Baud rate select (SCR2)" },
  { M6811_SCR1,  "SCR1 ", "SCI Baud rate select (SCR1)" },
  { M6811_SCR0,  "SCR0 ", "SCI Baud rate select (SCR0)" },
  { 0,  0, 0 }
};

static void
m68hc11sio_info (struct hw *me)
{
  SIM_DESC sd;
  uint16 base = 0;
  sim_cpu *cpu;
  struct m68hc11sio *controller;
  uint8 val;
  long clock_cycle;
  
  sd = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);
  
  sim_io_printf (sd, "M68HC11 SIO:\n");

  base = cpu_get_io_base (cpu);

  val  = cpu->ios[M6811_BAUD];
  print_io_byte (sd, "BAUD ", baud_desc, val, base + M6811_BAUD);
  sim_io_printf (sd, " (%ld baud)\n",
                 (cpu->cpu_frequency / 4) / controller->baud_cycle);

  val = cpu->ios[M6811_SCCR1];
  print_io_byte (sd, "SCCR1", sccr1_desc, val, base + M6811_SCCR1);
  sim_io_printf (sd, "  (%d bits) (%dN1)\n",
                 controller->data_length, controller->data_length - 2);

  val = cpu->ios[M6811_SCCR2];
  print_io_byte (sd, "SCCR2", sccr2_desc, val, base + M6811_SCCR2);
  sim_io_printf (sd, "\n");

  val = cpu->ios[M6811_SCSR];
  print_io_byte (sd, "SCSR ", scsr_desc, val, base + M6811_SCSR);
  sim_io_printf (sd, "\n");

  clock_cycle = controller->data_length * controller->baud_cycle;
  
  if (controller->tx_poll_event)
    {
      signed64 t;
      int n;

      t = hw_event_remain_time (me, controller->tx_poll_event);
      n = (clock_cycle - t) / controller->baud_cycle;
      n = controller->data_length - n;
      sim_io_printf (sd, "  Transmit finished in %s (%d bit%s)\n",
		     cycle_to_string (cpu, t, PRINT_TIME | PRINT_CYCLE),
                     n, (n > 1 ? "s" : ""));
    }
  if (controller->rx_poll_event)
    {
      signed64 t;

      t = hw_event_remain_time (me, controller->rx_poll_event);
      sim_io_printf (sd, "  Receive finished in %s\n",
		     cycle_to_string (cpu, t, PRINT_TIME | PRINT_CYCLE));
    }
  
}

static int
m68hc11sio_ioctl (struct hw *me,
                  hw_ioctl_request request,
                  va_list ap)
{
  m68hc11sio_info (me);
  return 0;
}

/* generic read/write */

static unsigned
m68hc11sio_io_read_buffer (struct hw *me,
                           void *dest,
                           int space,
                           unsigned_word base,
                           unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11sio *controller;
  sim_cpu *cpu;
  unsigned8 val;
  
  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  sd  = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);

  switch (base)
    {
    case M6811_SCSR:
      controller->rx_clear_scsr = cpu->ios[M6811_SCSR]
        & (M6811_RDRF | M6811_IDLE | M6811_OR | M6811_NF | M6811_FE);
      
    case M6811_BAUD:
    case M6811_SCCR1:
    case M6811_SCCR2:
      val = cpu->ios[base];
      break;
      
    case M6811_SCDR:
      if (controller->rx_clear_scsr)
        {
          cpu->ios[M6811_SCSR] &= ~controller->rx_clear_scsr;
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
m68hc11sio_io_write_buffer (struct hw *me,
                            const void *source,
                            int space,
                            unsigned_word base,
                            unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11sio *controller;
  sim_cpu *cpu;
  unsigned8 val;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));

  sd  = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);
  
  val = *((const unsigned8*) source);
  switch (base)
    {
    case M6811_BAUD:
      {
        long divisor;
        long baud;

        cpu->ios[M6811_BAUD] = val;        
        switch (val & (M6811_SCP1|M6811_SCP0))
          {
          case M6811_BAUD_DIV_1:
            divisor = 1 * 16;
            break;

          case M6811_BAUD_DIV_3:
            divisor = 3 * 16;
            break;

          case M6811_BAUD_DIV_4:
            divisor = 4 * 16;
            break;

          default:
          case M6811_BAUD_DIV_13:
            divisor = 13 * 16;
            break;
          }
        val &= (M6811_SCR2|M6811_SCR1|M6811_SCR0);
        divisor *= (1 << val);

        baud = (cpu->cpu_frequency / 4) / divisor;

        HW_TRACE ((me, "divide rate %ld, baud rate %ld",
                   divisor, baud));

        controller->baud_cycle = divisor;
      }
      break;
      
    case M6811_SCCR1:
      {
        if (val & M6811_M)
          controller->data_length = 11;
        else
          controller->data_length = 10;

        cpu->ios[M6811_SCCR1] = val;
      }
      break;
      
    case M6811_SCCR2:
      if ((val & M6811_RE) == 0)
        {
          val &= ~(M6811_RDRF|M6811_IDLE|M6811_OR|M6811_NF|M6811_NF);
          val |= (cpu->ios[M6811_SCCR2]
                  & (M6811_RDRF|M6811_IDLE|M6811_OR|M6811_NF|M6811_NF));
          cpu->ios[M6811_SCCR2] = val;
          break;
        }

      /* Activate reception.  */
      if (controller->rx_poll_event == 0)
        {
          long clock_cycle;
          
          /* Compute CPU clock cycles to wait for the next character.  */
          clock_cycle = controller->data_length * controller->baud_cycle;

          controller->rx_poll_event = hw_event_queue_schedule (me, clock_cycle,
                                                               m68hc11sio_rx_poll,
                                                               NULL);
        }      
      cpu->ios[M6811_SCCR2] = val;
      interrupts_update_pending (&cpu->cpu_interrupts);
      break;

      /* No effect.  */
    case M6811_SCSR:
      return 1;
      
    case M6811_SCDR:
      if (!(cpu->ios[M6811_SCSR] & M6811_TDRE))
        {
          return 0;
        }

      controller->tx_char     = val;
      controller->tx_has_char = 1;
      if ((cpu->ios[M6811_SCCR2] & M6811_TE)
          && controller->tx_poll_event == 0)
        {
          m68hc11sio_tx_poll (me, NULL);
        }
      return 1;
      
    default:
      return 0;
    }
  return nr_bytes;
}     


const struct hw_descriptor dv_m68hc11sio_descriptor[] = {
  { "m68hc11sio", m68hc11sio_finish },
  { "m68hc12sio", m68hc11sio_finish },
  { NULL },
};

