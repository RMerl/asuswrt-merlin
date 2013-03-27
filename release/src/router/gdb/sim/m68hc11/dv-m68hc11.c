/*  dv-m68hc11.c -- CPU 68HC11&68HC12 as a device.
    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2007
    Free Software Foundation, Inc.
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
#include "sim-hw.h"
#include "hw-main.h"
#include "sim-options.h"
#include "hw-base.h"
#include <limits.h>

/* DEVICE

        m68hc11cpu - m68hc11 cpu virtual device
        m68hc12cpu - m68hc12 cpu virtual device
   
   DESCRIPTION

        Implements the external m68hc11/68hc12 functionality.  This includes
        the delivery of of interrupts generated from other devices and the
        handling of device specific registers.


   PROPERTIES

   reg <base> <size>

        Register base (should be 0x1000 0x03f for C11, 0x0000 0x3ff for HC12).

   clock <hz>

        Frequency of the quartz used by the processor.

   mode [single | expanded | bootstrap | test]

        Cpu operating mode (the MODA and MODB external pins).


   PORTS

   reset (input)

        Reset the cpu and generates a cpu-reset event (used to reset
        other devices).

   nmi (input)

        Deliver a non-maskable interrupt to the processor.


   set-port-a (input)
   set-port-c (input)
   set-pord-d (input)

        Allow an external device to set the value of port A, C or D inputs.


   cpu-reset (output)

        Event generated after the CPU performs a reset.


   port-a (output)
   port-b (output)
   port-c (output)
   port-d (output)

        Event generated when the value of the output port A, B, C or D
	changes.


   BUGS

        When delivering an interrupt, this code assumes that there is only
        one processor (number 0).

   */

enum
{
  OPTION_OSC_SET = OPTION_START,
  OPTION_OSC_CLEAR,
  OPTION_OSC_INFO
};

static DECLARE_OPTION_HANDLER (m68hc11_option_handler);

static const OPTION m68hc11_options[] =
{
  { {"osc-set", required_argument, NULL, OPTION_OSC_SET },
      '\0', "BIT,FREQ", "Set the oscillator on input port BIT",
      m68hc11_option_handler },
  { {"osc-clear", required_argument, NULL, OPTION_OSC_CLEAR },
      '\0', "BIT", "Clear oscillator on input port BIT",
      m68hc11_option_handler },
  { {"osc-info", no_argument, NULL, OPTION_OSC_INFO },
      '\0', NULL, "Print information about current input oscillators",
      m68hc11_option_handler },
  
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

struct input_osc
{
  signed64         on_time;
  signed64         off_time;
  signed64         repeat;
  struct hw_event *event;
  const char      *name;
  uint8            mask;
  uint8            value;
  uint16           addr;
};

#define NR_PORT_A_OSC (4)
#define NR_PORT_B_OSC (0)
#define NR_PORT_C_OSC (8)
#define NR_PORT_D_OSC (6)
#define NR_OSC (NR_PORT_A_OSC + NR_PORT_B_OSC + NR_PORT_C_OSC + NR_PORT_D_OSC)
struct m68hc11cpu {
  /* Pending interrupts for delivery by event handler.  */
  int              pending_reset;
  int              pending_nmi;
  int              pending_level;
  struct hw_event  *event;
  unsigned_word    attach_address;
  int              attach_size;
  int              attach_space;
  int              last_oscillator;
  struct input_osc oscillators[NR_OSC];
};



/* input port ID's */ 

enum {
  RESET_PORT,
  NMI_PORT,
  IRQ_PORT,
  CPU_RESET_PORT,
  SET_PORT_A,
  SET_PORT_C,
  SET_PORT_D,
  CPU_WRITE_PORT,
  PORT_A,
  PORT_B,
  PORT_C,
  PORT_D,
  CAPTURE
};


static const struct hw_port_descriptor m68hc11cpu_ports[] = {

  /* Interrupt inputs.  */
  { "reset",     RESET_PORT,     0, input_port, },
  { "nmi",       NMI_PORT,       0, input_port, },
  { "irq",       IRQ_PORT,       0, input_port, },

  { "set-port-a", SET_PORT_A,    0, input_port, },
  { "set-port-c", SET_PORT_C,    0, input_port, },
  { "set-port-d", SET_PORT_D,    0, input_port, },

  { "cpu-write-port", CPU_WRITE_PORT,    0, input_port, },

  /* Events generated for connection to other devices.  */
  { "cpu-reset", CPU_RESET_PORT, 0, output_port, },

  /* Events generated when the corresponding port is
     changed by the program.  */
  { "port-a",    PORT_A,         0, output_port, },
  { "port-b",    PORT_B,         0, output_port, },
  { "port-c",    PORT_C,         0, output_port, },
  { "port-d",    PORT_D,         0, output_port, },

  { "capture",   CAPTURE,        0, output_port, },

  { NULL, },
};

static hw_io_read_buffer_method m68hc11cpu_io_read_buffer;
static hw_io_write_buffer_method m68hc11cpu_io_write_buffer;
static hw_ioctl_method m68hc11_ioctl;

/* Finish off the partially created hw device.  Attach our local
   callbacks.  Wire up our port names etc.  */

static hw_port_event_method m68hc11cpu_port_event;

static void make_oscillator (struct m68hc11cpu *controller,
                             const char *id, uint16 addr, uint8 mask);
static struct input_osc *find_oscillator (struct m68hc11cpu *controller,
                                          const char *id);
static void reset_oscillators (struct hw *me);

static void
dv_m6811_attach_address_callback (struct hw *me,
                                  int level,
                                  int space,
                                  address_word addr,
                                  address_word nr_bytes,
                                  struct hw *client)
{
  HW_TRACE ((me, "attach - level=%d, space=%d, addr=0x%lx, sz=%ld, client=%s",
	     level, space, (unsigned long) addr, (unsigned long) nr_bytes,
             hw_path (client)));

  if (space != io_map)
    {
      sim_core_attach (hw_system (me),
		       NULL, /*cpu*/
		       level,
		       access_read_write_exec,
		       space, addr,
		       nr_bytes,
		       0, /* modulo */
		       client,
		       NULL);
    }
  else
    {
      /*printf("Attach from sub device: %d\n", (long) addr);*/
      sim_core_attach (hw_system (me),
		       NULL, /*cpu*/
		       level,
		       access_io,
		       space, addr,
		       nr_bytes,
		       0, /* modulo */
		       client,
		       NULL);
    }
}

static void
dv_m6811_detach_address_callback (struct hw *me,
                                  int level,
                                  int space,
                                  address_word addr,
                                  address_word nr_bytes,
                                  struct hw *client)
{
  sim_core_detach (hw_system (me), NULL, /*cpu*/
                   level, space, addr);
}

static void
m68hc11_delete (struct hw* me)
{
  struct m68hc11cpu *controller;
  
  controller = hw_data (me);

  reset_oscillators (me);
  hw_detach_address (me, M6811_IO_LEVEL,
		     controller->attach_space,
		     controller->attach_address,
		     controller->attach_size, me);
}


static void
attach_m68hc11_regs (struct hw *me,
		     struct m68hc11cpu *controller)
{
  SIM_DESC sd;
  sim_cpu *cpu;
  reg_property_spec reg;
  const char *cpu_mode;
  
  if (hw_find_property (me, "reg") == NULL)
    hw_abort (me, "Missing \"reg\" property");

  if (!hw_find_reg_array_property (me, "reg", 0, &reg))
    hw_abort (me, "\"reg\" property must contain one addr/size entry");

  hw_unit_address_to_attach_address (hw_parent (me),
				     &reg.address,
				     &controller->attach_space,
				     &controller->attach_address,
				     me);
  hw_unit_size_to_attach_size (hw_parent (me),
			       &reg.size,
			       &controller->attach_size, me);

  hw_attach_address (hw_parent (me), M6811_IO_LEVEL,
		     controller->attach_space,
                     controller->attach_address,
                     controller->attach_size,
		     me);
  set_hw_delete (me, m68hc11_delete);

  /* Get cpu frequency.  */
  sd = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  if (hw_find_property (me, "clock") != NULL)
    {
      cpu->cpu_frequency = hw_find_integer_property (me, "clock");
    }
  else
    {
      cpu->cpu_frequency = 8*1000*1000;
    }

  if (hw_find_property (me, "use_bank") != NULL)
    hw_attach_address (hw_parent (me), 0,
                       exec_map,
                       cpu->bank_start,
                       cpu->bank_end - cpu->bank_start,
                       me);

  cpu_mode = "expanded";
  if (hw_find_property (me, "mode") != NULL)
    cpu_mode = hw_find_string_property (me, "mode");

  if (strcmp (cpu_mode, "test") == 0)
    cpu->cpu_mode = M6811_MDA | M6811_SMOD;
  else if (strcmp (cpu_mode, "bootstrap") == 0)
    cpu->cpu_mode = M6811_SMOD;
  else if (strcmp (cpu_mode, "single") == 0)
    cpu->cpu_mode = 0;
  else
    cpu->cpu_mode = M6811_MDA;

  controller->last_oscillator = 0;

  /* Create oscillators for input port A.  */
  make_oscillator (controller, "A7", M6811_PORTA, 0x80);
  make_oscillator (controller, "A2", M6811_PORTA, 0x04);
  make_oscillator (controller, "A1", M6811_PORTA, 0x02);
  make_oscillator (controller, "A0", M6811_PORTA, 0x01);

  /* port B is output only.  */

  /* Create oscillators for input port C.  */
  make_oscillator (controller, "C0", M6811_PORTC, 0x01);
  make_oscillator (controller, "C1", M6811_PORTC, 0x02);
  make_oscillator (controller, "C2", M6811_PORTC, 0x04);
  make_oscillator (controller, "C3", M6811_PORTC, 0x08);
  make_oscillator (controller, "C4", M6811_PORTC, 0x10);
  make_oscillator (controller, "C5", M6811_PORTC, 0x20);
  make_oscillator (controller, "C6", M6811_PORTC, 0x40);
  make_oscillator (controller, "C7", M6811_PORTC, 0x80);

  /* Create oscillators for input port D.  */
  make_oscillator (controller, "D0", M6811_PORTD, 0x01);
  make_oscillator (controller, "D1", M6811_PORTD, 0x02);
  make_oscillator (controller, "D2", M6811_PORTD, 0x04);
  make_oscillator (controller, "D3", M6811_PORTD, 0x08);
  make_oscillator (controller, "D4", M6811_PORTD, 0x10);
  make_oscillator (controller, "D5", M6811_PORTD, 0x20);

  /* Add oscillator commands.  */
  sim_add_option_table (sd, 0, m68hc11_options);
}

static void
m68hc11cpu_finish (struct hw *me)
{
  struct m68hc11cpu *controller;

  controller = HW_ZALLOC (me, struct m68hc11cpu);
  set_hw_data (me, controller);
  set_hw_io_read_buffer (me, m68hc11cpu_io_read_buffer);
  set_hw_io_write_buffer (me, m68hc11cpu_io_write_buffer);
  set_hw_ports (me, m68hc11cpu_ports);
  set_hw_port_event (me, m68hc11cpu_port_event);
  set_hw_attach_address (me, dv_m6811_attach_address_callback);
  set_hw_detach_address (me, dv_m6811_detach_address_callback);
#ifdef set_hw_ioctl
  set_hw_ioctl (me, m68hc11_ioctl);
#else
  me->to_ioctl = m68hc11_ioctl;
#endif

  /* Initialize the pending interrupt flags.  */
  controller->pending_level = 0;
  controller->pending_reset = 0;
  controller->pending_nmi = 0;
  controller->event = NULL;

  attach_m68hc11_regs (me, controller);
}

/* An event arrives on an interrupt port.  */

static void
deliver_m68hc11cpu_interrupt (struct hw *me, void *data)
{
}

static void
make_oscillator (struct m68hc11cpu *controller, const char *name,
                 uint16 addr, uint8 mask)
{
  struct input_osc *osc;

  if (controller->last_oscillator >= NR_OSC)
    hw_abort (0, "Too many oscillators");

  osc = &controller->oscillators[controller->last_oscillator];
  osc->name = name;
  osc->addr = addr;
  osc->mask = mask;
  controller->last_oscillator++;
}

/* Find the oscillator given the input port name.  */
static struct input_osc *
find_oscillator (struct m68hc11cpu *controller, const char *name)
{
  int i;

  for (i = 0; i < controller->last_oscillator; i++)
    if (strcasecmp (controller->oscillators[i].name, name) == 0)
      return &controller->oscillators[i];

  return 0;
}

static void
oscillator_handler (struct hw *me, void *data)
{
  struct input_osc *osc = (struct input_osc*) data;
  SIM_DESC sd;
  sim_cpu *cpu;
  signed64 dt;
  uint8 val;

  sd = hw_system (me);
  cpu = STATE_CPU (sd, 0);

  /* Change the input bit.  */
  osc->value ^= osc->mask;
  val = cpu->ios[osc->addr] & ~osc->mask;
  val |= osc->value;
  m68hc11cpu_set_port (me, cpu, osc->addr, val);

  /* Setup event to toggle the bit.  */
  if (osc->value)
    dt = osc->on_time;
  else
    dt = osc->off_time;

  if (dt && --osc->repeat >= 0)
    {
      sim_events *events = STATE_EVENTS (sd);

      dt += events->nr_ticks_to_process;
      osc->event = hw_event_queue_schedule (me, dt, oscillator_handler, osc);
    }
  else
    osc->event = 0;
}

static void
reset_oscillators (struct hw *me)
{
  struct m68hc11cpu *controller = hw_data (me);
  int i;

  for (i = 0; i < controller->last_oscillator; i++)
    {
      if (controller->oscillators[i].event)
        {
          hw_event_queue_deschedule (me, controller->oscillators[i].event);
          controller->oscillators[i].event = 0;
        }
    }
}
      
static void
m68hc11cpu_port_event (struct hw *me,
                       int my_port,
                       struct hw *source,
                       int source_port,
                       int level)
{
  struct m68hc11cpu *controller = hw_data (me);
  SIM_DESC sd;
  sim_cpu* cpu;
  
  sd  = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  switch (my_port)
    {
    case RESET_PORT:
      HW_TRACE ((me, "port-in reset"));

      /* The reset is made in 3 steps:
         - First, cleanup the current sim_cpu struct.
         - Reset the devices.
         - Restart the cpu for the reset (get the CPU mode from the
           CONFIG register that gets initialized by EEPROM device).  */
      cpu_reset (cpu);
      reset_oscillators (me);
      hw_port_event (me, CPU_RESET_PORT, 1);
      cpu_restart (cpu);
      break;
      
    case NMI_PORT:
      controller->pending_nmi = 1;
      HW_TRACE ((me, "port-in nmi"));
      break;
      
    case IRQ_PORT:
      /* level == 0 means that the interrupt was cleared.  */
      if(level == 0)
	controller->pending_level = -1; /* signal end of interrupt */
      else
	controller->pending_level = level;
      HW_TRACE ((me, "port-in level=%d", level));
      break;

    case SET_PORT_A:
      m68hc11cpu_set_port (me, cpu, M6811_PORTA, level);
      break;
      
    case SET_PORT_C:
      m68hc11cpu_set_port (me, cpu, M6811_PORTC, level);
      break;

    case SET_PORT_D:
      m68hc11cpu_set_port (me, cpu, M6811_PORTD, level);
      break;

    case CPU_WRITE_PORT:
      break;

    default:
      hw_abort (me, "bad switch");
      break;
    }

  /* Schedule an event to be delivered immediately after current
     instruction.  */
  if(controller->event != NULL)
    hw_event_queue_deschedule(me, controller->event);
  controller->event =
    hw_event_queue_schedule (me, 0, deliver_m68hc11cpu_interrupt, NULL);
}


io_reg_desc config_desc[] = {
  { M6811_NOSEC, "NOSEC ", "Security Mode Disable" },
  { M6811_NOCOP, "NOCOP ", "COP System Disable" },
  { M6811_ROMON, "ROMON ", "Enable On-chip Rom" },
  { M6811_EEON,  "EEON  ", "Enable On-chip EEprom" },
  { 0,  0, 0 }
};

io_reg_desc hprio_desc[] = {
  { M6811_RBOOT, "RBOOT ", "Read Bootstrap ROM" },
  { M6811_SMOD,  "SMOD  ", "Special Mode" },
  { M6811_MDA,   "MDA   ", "Mode Select A" },
  { M6811_IRV,   "IRV   ", "Internal Read Visibility" },
  { 0,  0, 0 }
};

io_reg_desc option_desc[] = {
  { M6811_ADPU,  "ADPU  ", "A/D Powerup" },
  { M6811_CSEL,  "CSEL  ", "A/D/EE Charge pump clock source select" },
  { M6811_IRQE,  "IRQE  ", "IRQ Edge/Level sensitive" },
  { M6811_DLY,   "DLY   ", "Stop exit turn on delay" },
  { M6811_CME,   "CME   ", "Clock Monitor Enable" },
  { M6811_CR1,   "CR1   ", "COP timer rate select (CR1)" },
  { M6811_CR0,   "CR0   ", "COP timer rate select (CR0)" },
  { 0,  0, 0 }
};

static void
m68hc11_info (struct hw *me)
{
  SIM_DESC sd;
  uint16 base = 0;
  sim_cpu *cpu;
  struct m68hc11sio *controller;
  uint8 val;
  
  sd = hw_system (me);
  cpu = STATE_CPU (sd, 0);
  controller = hw_data (me);

  base = cpu_get_io_base (cpu);
  sim_io_printf (sd, "M68HC11:\n");

  val = cpu->ios[M6811_HPRIO];
  print_io_byte (sd, "HPRIO ", hprio_desc, val, base + M6811_HPRIO);
  switch (cpu->cpu_mode)
    {
    case M6811_MDA | M6811_SMOD:
      sim_io_printf (sd, "[test]\n");
      break;
    case M6811_SMOD:
      sim_io_printf (sd, "[bootstrap]\n");
      break;
    case M6811_MDA:
      sim_io_printf (sd, "[extended]\n");
      break;
    default:
      sim_io_printf (sd, "[single]\n");
      break;
    }

  val = cpu->ios[M6811_CONFIG];
  print_io_byte (sd, "CONFIG", config_desc, val, base + M6811_CONFIG);
  sim_io_printf (sd, "\n");

  val = cpu->ios[M6811_OPTION];
  print_io_byte (sd, "OPTION", option_desc, val, base + M6811_OPTION);
  sim_io_printf (sd, "\n");

  val = cpu->ios[M6811_INIT];
  print_io_byte (sd, "INIT  ", 0, val, base + M6811_INIT);
  sim_io_printf (sd, "Ram = 0x%04x IO = 0x%04x\n",
		 (((uint16) (val & 0xF0)) << 8),
		 (((uint16) (val & 0x0F)) << 12));


  cpu_info (sd, cpu);
  interrupts_info (sd, &cpu->cpu_interrupts);
}

static int
m68hc11_ioctl (struct hw *me,
	       hw_ioctl_request request,
	       va_list ap)
{
  m68hc11_info (me);
  return 0;
}

/* Setup an oscillator on an input port.

   TON represents the time in seconds that the input port should be set to 1.
   TOFF is the time in seconds for the input port to be set to 0.

   The oscillator frequency is therefore 1 / (ton + toff).

   REPEAT indicates the number of 1 <-> 0 transitions until the oscillator
   stops.  */
int
m68hc11cpu_set_oscillator (SIM_DESC sd, const char *port,
                           double ton, double toff, signed64 repeat)
{
  sim_cpu *cpu;
  struct input_osc *osc;
  double f;

  cpu = STATE_CPU (sd, 0);

  /* Find oscillator that corresponds to the input port.  */
  osc = find_oscillator (hw_data (cpu->hw_cpu), port);
  if (osc == 0)
    return -1;

  /* Compute the ON time in cpu cycles.  */
  f = (double) (cpu->cpu_frequency) * ton;
  osc->on_time = (signed64) (f / 4.0);
  if (osc->on_time < 1)
    osc->on_time = 1;

  /* Compute the OFF time in cpu cycles.  */
  f = (double) (cpu->cpu_frequency) * toff;
  osc->off_time = (signed64) (f / 4.0);
  if (osc->off_time < 1)
    osc->off_time = 1;

  osc->repeat = repeat;
  if (osc->event)
    hw_event_queue_deschedule (cpu->hw_cpu, osc->event);

  osc->event = hw_event_queue_schedule (cpu->hw_cpu,
                                        osc->value ? osc->on_time
                                        : osc->off_time,
                                        oscillator_handler, osc);
  return 0;
}

/* Clear the oscillator.  */
int
m68hc11cpu_clear_oscillator (SIM_DESC sd, const char *port)
{
  sim_cpu *cpu;
  struct input_osc *osc;

  cpu = STATE_CPU (sd, 0);
  osc = find_oscillator (hw_data (cpu->hw_cpu), port);
  if (osc == 0)
    return -1;

  if (osc->event)
    hw_event_queue_deschedule (cpu->hw_cpu, osc->event);
  osc->event = 0;
  osc->repeat = 0;
  return 0;
}

static int
get_frequency (const char *s, double *f)
{
  char *p;
  
  *f = strtod (s, &p);
  if (s == p)
    return -1;

  if (*p)
    {
      if (strcasecmp (p, "khz") == 0)
        *f = *f * 1000.0;
      else if (strcasecmp (p, "mhz") == 0)
        *f = *f  * 1000000.0;
      else if (strcasecmp (p, "hz") != 0)
        return -1;
    }
  return 0;
}

static SIM_RC
m68hc11_option_handler (SIM_DESC sd, sim_cpu *cpu,
                        int opt, char *arg, int is_command)
{
  struct m68hc11cpu *controller;
  double f;
  char *p;
  int i;
  int title_printed = 0;
  
  if (cpu == 0)
    cpu = STATE_CPU (sd, 0);

  controller = hw_data (cpu->hw_cpu);
  switch (opt)
    {
    case OPTION_OSC_SET:
      p = strchr (arg, ',');
      if (p)
        *p++ = 0;

      if (p == 0)
        sim_io_eprintf (sd, "No frequency specified\n");
      else if (get_frequency (p, &f) < 0 || f < 1.0e-8)
        sim_io_eprintf (sd, "Invalid frequency: '%s'\n", p);
      else if (m68hc11cpu_set_oscillator (sd, arg,
                                          1.0 / (f * 2.0),
                                          1.0 / (f * 2.0), LONG_MAX))
        sim_io_eprintf (sd, "Invalid input port: '%s'\n", arg);
      break;

    case OPTION_OSC_CLEAR:
      if (m68hc11cpu_clear_oscillator (sd, arg) != 0)
        sim_io_eprintf (sd, "Invalid input port: '%s'\n", arg);
      break;

    case OPTION_OSC_INFO:
      for (i = 0; i < controller->last_oscillator; i++)
        {
          signed64 t;
          struct input_osc *osc;

          osc = &controller->oscillators[i];
          if (osc->event)
            {
              double f;
              int cur_value;
              int next_value;
              char freq[32];

              if (title_printed == 0)
                {
                  title_printed = 1;
                  sim_io_printf (sd, " PORT  Frequency   Current"
                                 "    Next    Transition time\n");
                }

              f = (double) (osc->on_time + osc->off_time);
              f = (double) (cpu->cpu_frequency / 4) / f;
              t = hw_event_remain_time (cpu->hw_cpu, osc->event);

              if (f > 10000.0)
                sprintf (freq, "%6.2f", f / 1000.0);
              else
                sprintf (freq, "%6.2f", f);
              cur_value = osc->value ? 1 : 0;
              next_value = osc->value ? 0 : 1;
              if (f > 10000.0)
                sim_io_printf (sd, " %4.4s  %8.8s khz"
                               "      %d       %d    %35.35s\n",
                               osc->name, freq,
                               cur_value, next_value,
                               cycle_to_string (cpu, t,
                                                PRINT_TIME | PRINT_CYCLE));
              else
                sim_io_printf (sd, " %4.4s  %8.8s hz "
                               "      %d       %d    %35.35s\n",
                               osc->name, freq,
                               cur_value, next_value,
                               cycle_to_string (cpu, t,
                                                PRINT_TIME | PRINT_CYCLE));
            }
        }
      break;      
    }

  return SIM_RC_OK;
}

/* generic read/write */

static unsigned
m68hc11cpu_io_read_buffer (struct hw *me,
			   void *dest,
			   int space,
			   unsigned_word base,
			   unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11cpu *controller = hw_data (me);
  sim_cpu *cpu;
  unsigned byte = 0;
  int result;
  
  HW_TRACE ((me, "read 0x%08lx %d", (long) base, (int) nr_bytes));

  sd  = hw_system (me);
  cpu = STATE_CPU (sd, 0);

  if (base >= cpu->bank_start && base < cpu->bank_end)
    {
      address_word virt_addr = phys_to_virt (cpu, base);
      if (virt_addr != base)
        return sim_core_read_buffer (sd, cpu, space, dest,
                                     virt_addr, nr_bytes);
    }

  /* Handle reads for the sub-devices.  */
  base -= controller->attach_address;
  result = sim_core_read_buffer (sd, cpu,
				 io_map, dest, base, nr_bytes);
  if (result > 0)
    return result;
  
  while (nr_bytes)
    {
      if (base >= controller->attach_size)
	break;

      memcpy (dest, &cpu->ios[base], 1);
      dest = (char*) dest + 1;
      base++;
      byte++;
      nr_bytes--;
    }
  return byte;
}     

void
m68hc11cpu_set_port (struct hw *me, sim_cpu *cpu,
                     unsigned addr, uint8 val)
{
  uint8 mask;
  uint8 delta;
  int check_interrupts = 0;
  int i;
  
  switch (addr)
    {
    case M6811_PORTA:
      if (cpu->ios[M6811_PACTL] & M6811_DDRA7)
        mask = 3;
      else
        mask = 0x83;

      val = val & mask;
      val |= cpu->ios[M6811_PORTA] & ~mask;
      delta = val ^ cpu->ios[M6811_PORTA];
      cpu->ios[M6811_PORTA] = val;
      if (delta & 0x80)
        {
          /* Pulse accumulator is enabled.  */
          if ((cpu->ios[M6811_PACTL] & M6811_PAEN)
              && !(cpu->ios[M6811_PACTL] & M6811_PAMOD))
            {
              int inc;

              /* Increment event counter according to rising/falling edge.  */
              if (cpu->ios[M6811_PACTL] & M6811_PEDGE)
                inc = (val & 0x80) ? 1 : 0;
              else
                inc = (val & 0x80) ? 0 : 1;

              cpu->ios[M6811_PACNT] += inc;

              /* Event counter overflowed.  */
              if (inc && cpu->ios[M6811_PACNT] == 0)
                {
                  cpu->ios[M6811_TFLG2] |= M6811_PAOVI;
                  check_interrupts = 1;
                }
            }
        }

      /* Scan IC3, IC2 and IC1.  Bit number is 3 - i.  */
      for (i = 0; i < 3; i++)
        {
          uint8 mask = (1 << i);
          
          if (delta & mask)
            {
              uint8 edge;
              int captured;

              edge = cpu->ios[M6811_TCTL2];
              edge = (edge >> (2 * i)) & 0x3;
              switch (edge)
                {
                case 0:
                  captured = 0;
                  break;
                case 1:
                  captured = (val & mask) != 0;
                  break;
                case 2:
                  captured = (val & mask) == 0;
                  break;
                default:
                  captured = 1;
                  break;
                }
              if (captured)
                {
                  cpu->ios[M6811_TFLG1] |= (1 << i);
                  hw_port_event (me, CAPTURE, M6811_TIC1 + 3 - i);
                  check_interrupts = 1;
                }
            }
        }
      break;

    case M6811_PORTC:
      mask = cpu->ios[M6811_DDRC];
      val = val & mask;
      val |= cpu->ios[M6811_PORTC] & ~mask;
      cpu->ios[M6811_PORTC] = val;
      break;

    case M6811_PORTD:
      mask = cpu->ios[M6811_DDRD];
      val = val & mask;
      val |= cpu->ios[M6811_PORTD] & ~mask;
      cpu->ios[M6811_PORTD] = val;
      break;

    default:
      break;
    }

  if (check_interrupts)
    interrupts_update_pending (&cpu->cpu_interrupts);
}

static void
m68hc11cpu_io_write (struct hw *me, sim_cpu *cpu,
                     unsigned_word addr, uint8 val)
{
  switch (addr)
    {
    case M6811_PORTA:
      hw_port_event (me, PORT_A, val);
      break;

    case M6811_PIOC:
      break;

    case M6811_PORTC:
      hw_port_event (me, PORT_C, val);
      break;

    case M6811_PORTB:
      hw_port_event (me, PORT_B, val);
      break;

    case M6811_PORTCL:
      break;

    case M6811_DDRC:
      break;

    case M6811_PORTD:
      hw_port_event (me, PORT_D, val);
      break;

    case M6811_DDRD:
      break;

    case M6811_TMSK2:
      
      break;
      
      /* Change the RAM and I/O mapping.  */
    case M6811_INIT:
      {
	uint8 old_bank = cpu->ios[M6811_INIT];
	
	cpu->ios[M6811_INIT] = val;

	/* Update IO mapping.  Detach from the old address
	   and attach to the new one.  */
	if ((old_bank & 0x0F) != (val & 0x0F))
	  {
            struct m68hc11cpu *controller = hw_data (me);

            hw_detach_address (hw_parent (me), M6811_IO_LEVEL,
                               controller->attach_space,
                               controller->attach_address,
                               controller->attach_size,
                               me);
            controller->attach_address = (val & 0x0F0) << 12;
            hw_attach_address (hw_parent (me), M6811_IO_LEVEL,
                               controller->attach_space,
                               controller->attach_address,
                               controller->attach_size,
                               me);
	  }
	if ((old_bank & 0xF0) != (val & 0xF0))
	  {
	    ;
	  }
	return;
      }

    /* Writing the config is similar to programing the eeprom.
       The config register value is the last byte of the EEPROM.
       This last byte is not mapped in memory (that's why we have
       to add '1' to 'end_addr').  */
    case M6811_CONFIG:
      {
        return;
      }
      

      /* COP reset.  */
    case M6811_COPRST:
      if (val == 0xAA && cpu->ios[addr] == 0x55)
	{
          val = 0;
          /* COP reset here.  */
	}
      break;
      
    default:
      break;

    }
  cpu->ios[addr] = val;
}

static unsigned
m68hc11cpu_io_write_buffer (struct hw *me,
			    const void *source,
			    int space,
			    unsigned_word base,
			    unsigned nr_bytes)
{
  SIM_DESC sd;
  struct m68hc11cpu *controller = hw_data (me);
  unsigned byte;
  sim_cpu *cpu;
  int result;

  HW_TRACE ((me, "write 0x%08lx %d", (long) base, (int) nr_bytes));

  sd = hw_system (me); 
  cpu = STATE_CPU (sd, 0);  

  if (base >= cpu->bank_start && base < cpu->bank_end)
    {
      address_word virt_addr = phys_to_virt (cpu, base);
      if (virt_addr != base)
        return sim_core_write_buffer (sd, cpu, space, source,
                                      virt_addr, nr_bytes);
    }
  base -= controller->attach_address;
  result = sim_core_write_buffer (sd, cpu,
				  io_map, source, base, nr_bytes);
  if (result > 0)
    return result;

  byte = 0;
  while (nr_bytes)
    {
      uint8 val;
      if (base >= controller->attach_size)
	break;

      val = *((uint8*) source);
      m68hc11cpu_io_write (me, cpu, base, val);
      source = (char*) source + 1;
      base++;
      byte++;
      nr_bytes--;
    }
  return byte;
}

const struct hw_descriptor dv_m68hc11_descriptor[] = {
  { "m68hc11", m68hc11cpu_finish },
  { "m68hc12", m68hc11cpu_finish },
  { NULL },
};

