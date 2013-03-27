/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1997, 2003 Andrew Cagney

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


#ifndef _PSIM_C_
#define _PSIM_C_

#include "cpu.h" /* includes psim.h */
#include "idecode.h"
#include "options.h"

#include "tree.h"

#include <signal.h>

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <setjmp.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#include "bfd.h"
#include "libiberty.h"
#include "gdb/signals.h"

/* system structure, actual size of processor array determined at
   runtime */

struct _psim {
  event_queue *events;
  device *devices;
  mon *monitor;
  os_emul *os_emulation;
  core *memory;

  /* escape routine for inner functions */
  void *path_to_halt;
  void *path_to_restart;

  /* status from last halt */
  psim_status halt_status;

  /* the processors proper */
  int nr_cpus;
  int last_cpu; /* CPU that last (tried to) execute an instruction */
  cpu *processors[MAX_NR_PROCESSORS];
};


int current_target_byte_order;
int current_host_byte_order;
int current_environment;
int current_alignment;
int current_floating_point;
int current_model_issue = MODEL_ISSUE_IGNORE;
int current_stdio = DO_USE_STDIO;
model_enum current_model = WITH_DEFAULT_MODEL;


/* create the device tree */

INLINE_PSIM\
(device *)
psim_tree(void)
{
  device *root = tree_parse(NULL, "core");
  tree_parse(root, "/aliases");
  tree_parse(root, "/options");
  tree_parse(root, "/chosen");
  tree_parse(root, "/packages");
  tree_parse(root, "/cpus");
  tree_parse(root, "/openprom");
  tree_parse(root, "/openprom/init");
  tree_parse(root, "/openprom/trace");
  tree_parse(root, "/openprom/options");
  return root;
}

STATIC_INLINE_PSIM\
(char *)
find_arg(char *err_msg,
	 int *ptr_to_argp,
	 char **argv)
{
  *ptr_to_argp += 1;
  if (argv[*ptr_to_argp] == NULL)
    error(err_msg);
  return argv[*ptr_to_argp];
}

INLINE_PSIM\
(void)
psim_usage(int verbose)
{
  printf_filtered("Usage:\n");
  printf_filtered("\n");
  printf_filtered("\tpsim [ <psim-option> ... ] <image> [ <image-arg> ... ]\n");
  printf_filtered("\n");
  printf_filtered("Where\n");
  printf_filtered("\n");
  printf_filtered("\t<image>         Name of the PowerPC program to run.\n");
  if (verbose) {
  printf_filtered("\t                This can either be a PowerPC binary or\n");
  printf_filtered("\t                a text file containing a device tree\n");
  printf_filtered("\t                specification.\n");
  printf_filtered("\t                PSIM will attempt to determine from the\n");
  printf_filtered("\t                specified <image> the intended emulation\n");
  printf_filtered("\t                environment.\n");
  printf_filtered("\t                If PSIM gets it wrong, the emulation\n");
  printf_filtered("\t                environment can be specified using the\n");
  printf_filtered("\t                `-e' option (described below).\n");
  printf_filtered("\n"); }
  printf_filtered("\t<image-arg>     Argument to be passed to <image>\n");
  if (verbose) {
  printf_filtered("\t                These arguments will be passed to\n");
  printf_filtered("\t                <image> (as standard C argv, argc)\n");
  printf_filtered("\t                when <image> is started.\n");
  printf_filtered("\n"); }
  printf_filtered("\t<psim-option>   See below\n");
  printf_filtered("\n");
  printf_filtered("The following are valid <psim-option>s:\n");
  printf_filtered("\n");

  printf_filtered("\t-c <count>      Limit the simulation to <count> iterations\n");
  if (verbose) { 
  printf_filtered("\n");
  }

  printf_filtered("\t-i or -i2       Print instruction counting statistics\n");
  if (verbose) { 
  printf_filtered("\t                Specify -i2 for a more detailed display\n");
  printf_filtered("\n");
  }

  printf_filtered("\t-I              Print execution unit statistics\n");
  if (verbose) { printf_filtered("\n"); }

  printf_filtered("\t-e <os-emul>    specify an OS or platform to model\n");
  if (verbose) {
  printf_filtered("\t                Can be any of the following:\n");
  printf_filtered("\t                bug - OEA + MOTO BUG ROM calls\n");
  printf_filtered("\t                netbsd - UEA + NetBSD system calls\n");
  printf_filtered("\t                solaris - UEA + Solaris system calls\n");
  printf_filtered("\t                linux - UEA + Linux system calls\n");
  printf_filtered("\t                chirp - OEA + a few OpenBoot calls\n");
  printf_filtered("\n"); }

  printf_filtered("\t-E <endian>     Specify the endianness of the target\n");
  if (verbose) { 
  printf_filtered("\t                Can be any of the following:\n");
  printf_filtered("\t                big - big endian target\n");
  printf_filtered("\t                little - little endian target\n");
  printf_filtered("\n"); }

  printf_filtered("\t-f <file>       Merge <file> into the device tree\n");
  if (verbose) { printf_filtered("\n"); }

  printf_filtered("\t-h -? -H        give more detailed usage\n");
  if (verbose) { printf_filtered("\n"); }

  printf_filtered("\t-m <model>      Specify the processor to model (604)\n");
  if (verbose) {
  printf_filtered("\t                Selects the processor to use when\n");
  printf_filtered("\t                modeling execution units.  Includes:\n");
  printf_filtered("\t                604, 603 and 603e\n");
  printf_filtered("\n"); }

  printf_filtered("\t-n <nr-smp>     Specify the number of processors in SMP simulations\n");
  if (verbose) {
  printf_filtered("\t                Specifies the number of processors that are\n");
  printf_filtered("\t                to be modeled in a symetric multi-processor (SMP)\n");
  printf_filtered("\t                simulation\n");
  printf_filtered("\n"); }

  printf_filtered("\t-o <dev-spec>   Add device <dev-spec> to the device tree\n");
  if (verbose) { printf_filtered("\n"); }

  printf_filtered("\t-r <ram-size>   Set RAM size in bytes (OEA environments)\n");
  if (verbose) { printf_filtered("\n"); }

  printf_filtered("\t-t [!]<trace>   Enable (disable) <trace> option\n");
  if (verbose) { printf_filtered("\n"); }

  printf_filtered("\n");
  trace_usage(verbose);
  device_usage(verbose);
  if (verbose > 1) {
    printf_filtered("\n");
    print_options();
  }
  error("");
}

/* Test "string" for containing a string of digits that form a number
between "min" and "max".  The return value is the number or "err". */
static
int is_num( char *string, int min, int max, int err)
{
  int result = 0;

  for ( ; *string; ++string)
  {
    if (!isdigit(*string))
    {
      result = err;
      break;
    }
    result = result * 10 + (*string - '0');
  }
  if (result < min || result > max)
    result = err;

  return result;
}

INLINE_PSIM\
(char **)
psim_options(device *root,
	     char **argv)
{
  device *current = root;
  int argp;
  if (argv == NULL)
    return NULL;
  argp = 0;
  while (argv[argp] != NULL && argv[argp][0] == '-') {
    char *p = argv[argp] + 1;
    char *param;
    while (*p != '\0') {
      switch (*p) {
      default:
	psim_usage(0);
	error ("");
	break;
      case 'c':
	param = find_arg("Missing <count> option for -c (max-iterations)\n", &argp, argv);
	tree_parse(root, "/openprom/options/max-iterations %s", param);
	break;
      case 'e':
	param = find_arg("Missing <emul> option for -e (os-emul)\n", &argp, argv);
	tree_parse(root, "/openprom/options/os-emul %s", param);
	break;
      case 'E':
	/* endian spec, ignored for now */
	param = find_arg("Missing <endian> option for -E (target-endian)\n", &argp, argv);
	if (strcmp (param, "big") == 0)
	  tree_parse (root, "/options/little-endian? false");
	else if (strcmp (param, "little") == 0)
	  tree_parse (root, "/options/little-endian? true");
	else
	  {
	    printf_filtered ("Invalid <endian> option for -E (target-endian)\n");
	    psim_usage (0);
	  }
	break;
      case 'f':
	param = find_arg("Missing <file> option for -f\n", &argp, argv);
	psim_merge_device_file(root, param);
	break;
      case 'h':
      case '?':
	psim_usage(1);
	break;
      case 'H':
	psim_usage(2);
	break;
      case 'i':
	if (isdigit(p[1])) {
	  tree_parse(root, "/openprom/trace/print-info %c", p[1]);
	  p++;
	}
	else {
	  tree_parse(root, "/openprom/trace/print-info 1");
	}
	break;
      case 'I':
	tree_parse(root, "/openprom/trace/print-info 2");
	tree_parse(root, "/openprom/options/model-issue %d",
		   MODEL_ISSUE_PROCESS);
	break;
      case 'm':
	param = find_arg("Missing <model> option for -m (model)\n", &argp, argv);
	tree_parse(root, "/openprom/options/model \"%s", param);
	break;
      case 'n':
	param = find_arg("Missing <nr-smp> option for -n (smp)\n", &argp, argv);
	tree_parse(root, "/openprom/options/smp %s", param);
	break;
      case 'o':
	param = find_arg("Missing <dev-spec> option for -o\n", &argp, argv);
	if (memcmp(param, "mpc860c0", 8) == 0)
        {
          if (param[8] == '\0')
            tree_parse(root, "/options/mpc860c0 5");
          else if (param[8] == '=' && is_num(param+9, 1, 10, 0))
          {
            tree_parse(root, "/options/mpc860c0 %s", param+9);
          }
          else error("Invalid mpc860c0 option for -o\n");
        }
	else
          current = tree_parse(current, "%s", param);
	break;
      case 'r':
	param = find_arg("Missing <ram-size> option for -r (oea-memory-size)\n", &argp, argv);
	tree_parse(root, "/openprom/options/oea-memory-size %s",
			       param);
	break;
      case 't':
	param = find_arg("Missing <trace> option for -t (trace/*)\n", &argp, argv);
	if (param[0] == '!')
	  tree_parse(root, "/openprom/trace/%s 0", param+1);
	else
	  tree_parse(root, "/openprom/trace/%s 1", param);
	break;
      case '-':
	/* it's a long option of the form --optionname=optionvalue.
	   Such options can be passed through if we are invoked by
	   gdb.  */
	if (strstr(argv[argp], "architecture") != NULL) {
          /* we must consume the argument here, so that we get out
             of the loop.  */
	  p = argv[argp] + strlen(argv[argp]) - 1;
	  printf_filtered("Warning - architecture parameter ignored\n");
        }
	else
	  error("Unrecognized option");
	break;
      }
      p += 1;
    }
    argp += 1;
  }
  /* force the trace node to process its options now *before* the tree
     initialization occures */
  device_ioctl(tree_find_device(root, "/openprom/trace"),
	       NULL, 0,
	       device_ioctl_set_trace);

  {
    void semantic_init(device* root);
    semantic_init(root);
  }

  /* return where the options end */
  return argv + argp;
}

INLINE_PSIM\
(void)
psim_command(device *root,
	     char **argv)
{
  int argp = 0;
  if (argv[argp] == NULL) {
    return;
  }
  else if (strcmp(argv[argp], "trace") == 0) {
    const char *opt = find_arg("Missing <trace> option", &argp, argv);
    if (opt[0] == '!')
      trace_option(opt + 1, 0);
    else
      trace_option(opt, 1);
  }
  else if (strcmp(*argv, "change-media") == 0) {
    char *device = find_arg("Missing device name", &argp, argv);
    char *media = argv[++argp];
    device_ioctl(tree_find_device(root, device), NULL, 0,
		 device_ioctl_change_media, media);
  }
  else {
    printf_filtered("Unknown PSIM command %s, try\n", argv[argp]);
    printf_filtered("    trace <trace-option>\n");
    printf_filtered("    change-media <device> [ <new-image> ]\n");
  }
}


/* create the simulator proper from the device tree and executable */

INLINE_PSIM\
(psim *)
psim_create(const char *file_name,
	    device *root)
{
  int cpu_nr;
  const char *env;
  psim *system;
  os_emul *os_emulation;
  int nr_cpus;

  /* given this partially populated device tree, os_emul_create() uses
     it and file_name to determine the selected emulation and hence
     further populate the tree with any other required nodes. */

  os_emulation = os_emul_create(file_name, root);
  if (os_emulation == NULL)
    error("psim: either file %s was not reconized or unreconized or unknown os-emulation type\n", file_name);

  /* fill in the missing real number of CPU's */
  nr_cpus = tree_find_integer_property(root, "/openprom/options/smp");
  if (MAX_NR_PROCESSORS < nr_cpus)
    error("target and configured number of cpus conflict\n");

  /* fill in the missing TARGET BYTE ORDER information */
  current_target_byte_order
    = (tree_find_boolean_property(root, "/options/little-endian?")
       ? LITTLE_ENDIAN
       : BIG_ENDIAN);
  if (CURRENT_TARGET_BYTE_ORDER != current_target_byte_order)
    error("target and configured byte order conflict\n");

  /* fill in the missing HOST BYTE ORDER information */
  current_host_byte_order = (current_host_byte_order = 1,
			     (*(char*)(&current_host_byte_order)
			      ? LITTLE_ENDIAN
			      : BIG_ENDIAN));
  if (CURRENT_HOST_BYTE_ORDER != current_host_byte_order)
    error("host and configured byte order conflict\n");

  /* fill in the missing OEA/VEA information */
  env = tree_find_string_property(root, "/openprom/options/env");
  current_environment = ((strcmp(env, "user") == 0
			  || strcmp(env, "uea") == 0)
			 ? USER_ENVIRONMENT
			 : (strcmp(env, "virtual") == 0
			    || strcmp(env, "vea") == 0)
			 ? VIRTUAL_ENVIRONMENT
			 : (strcmp(env, "operating") == 0
			    || strcmp(env, "oea") == 0)
			 ? OPERATING_ENVIRONMENT
			 : 0);
  if (current_environment == 0)
    error("unreconized /options env property\n");
  if (CURRENT_ENVIRONMENT != current_environment)
    error("target and configured environment conflict\n");

  /* fill in the missing ALLIGNMENT information */
  current_alignment
    = (tree_find_boolean_property(root, "/openprom/options/strict-alignment?")
       ? STRICT_ALIGNMENT
       : NONSTRICT_ALIGNMENT);
  if (CURRENT_ALIGNMENT != current_alignment)
    error("target and configured alignment conflict\n");

  /* fill in the missing FLOATING POINT information */
  current_floating_point
    = (tree_find_boolean_property(root, "/openprom/options/floating-point?")
       ? HARD_FLOATING_POINT
       : SOFT_FLOATING_POINT);
  if (CURRENT_FLOATING_POINT != current_floating_point)
    error("target and configured floating-point conflict\n");

  /* fill in the missing STDIO information */
  current_stdio
    = (tree_find_boolean_property(root, "/openprom/options/use-stdio?")
       ? DO_USE_STDIO
       : DONT_USE_STDIO);
  if (CURRENT_STDIO != current_stdio)
    error("target and configured stdio interface conflict\n");

  /* sort out the level of detail for issue modeling */
  current_model_issue
    = tree_find_integer_property(root, "/openprom/options/model-issue");
  if (CURRENT_MODEL_ISSUE != current_model_issue)
    error("target and configured model-issue conflict\n");

  /* sort out our model architecture - wrong.

     FIXME: this should be obtaining the required information from the
     device tree via the "/chosen" property "cpu" which is an instance
     (ihandle) for the only executing processor. By converting that
     ihandle into the corresponding cpu's phandle and then querying
     the "name" property, the cpu type can be determined. Ok? */

  model_set(tree_find_string_property(root, "/openprom/options/model"));

  /* create things */
  system = ZALLOC(psim);
  system->events = event_queue_create();
  system->memory = core_from_device(root);
  system->monitor = mon_create();
  system->nr_cpus = nr_cpus;
  system->os_emulation = os_emulation;
  system->devices = root;

  /* now all the processors attaching to each their per-cpu information */
  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; cpu_nr++) {
    system->processors[cpu_nr] = cpu_create(system,
					    system->memory,
					    mon_cpu(system->monitor,
						    cpu_nr),
					    system->os_emulation,
					    cpu_nr);
  }

  /* dump out the contents of the device tree */
  if (ppc_trace[trace_print_device_tree] || ppc_trace[trace_dump_device_tree])
    tree_print(root);
  if (ppc_trace[trace_dump_device_tree])
    error("");

  return system;
}


/* allow the simulation to stop/restart abnormaly */

INLINE_PSIM\
(void)
psim_set_halt_and_restart(psim *system,
			  void *halt_jmp_buf,
			  void *restart_jmp_buf)
{
  system->path_to_halt = halt_jmp_buf;
  system->path_to_restart = restart_jmp_buf;
}

INLINE_PSIM\
(void)
psim_clear_halt_and_restart(psim *system)
{
  system->path_to_halt = NULL;
  system->path_to_restart = NULL;
}

INLINE_PSIM\
(void)
psim_restart(psim *system,
	     int current_cpu)
{
  ASSERT(current_cpu >= 0 && current_cpu < system->nr_cpus);
  ASSERT(system->path_to_restart != NULL);
  system->last_cpu = current_cpu;
  longjmp(*(jmp_buf*)(system->path_to_restart), current_cpu + 1);
}


static void
cntrl_c_simulation(void *data)
{
  psim *system = data;
  psim_halt(system,
	    psim_nr_cpus(system),
	    was_continuing,
	    TARGET_SIGNAL_INT);
}

INLINE_PSIM\
(void)
psim_stop(psim *system)
{
  event_queue_schedule_after_signal(psim_event_queue(system),
				    0 /*NOW*/,
				    cntrl_c_simulation,
				    system);
}

INLINE_PSIM\
(void)
psim_halt(psim *system,
	  int current_cpu,
	  stop_reason reason,
	  int signal)
{
  ASSERT(current_cpu >= 0 && current_cpu <= system->nr_cpus);
  ASSERT(system->path_to_halt != NULL);
  system->last_cpu = current_cpu;
  system->halt_status.reason = reason;
  system->halt_status.signal = signal;
  if (current_cpu == system->nr_cpus) {
    system->halt_status.cpu_nr = 0;
    system->halt_status.program_counter =
      cpu_get_program_counter(system->processors[0]);
  }
  else {
    system->halt_status.cpu_nr = current_cpu;
    system->halt_status.program_counter =
      cpu_get_program_counter(system->processors[current_cpu]);
  }
  longjmp(*(jmp_buf*)(system->path_to_halt), current_cpu + 1);
}


INLINE_PSIM\
(int)
psim_last_cpu(psim *system)
{
  return system->last_cpu;
}

INLINE_PSIM\
(int)
psim_nr_cpus(psim *system)
{
  return system->nr_cpus;
}

INLINE_PSIM\
(psim_status)
psim_get_status(psim *system)
{
  return system->halt_status;
}


INLINE_PSIM\
(cpu *)
psim_cpu(psim *system,
	 int cpu_nr)
{
  if (cpu_nr < 0 || cpu_nr >= system->nr_cpus)
    return NULL;
  else
    return system->processors[cpu_nr];
}


INLINE_PSIM\
(device *)
psim_device(psim *system,
	    const char *path)
{
  return tree_find_device(system->devices, path);
}

INLINE_PSIM\
(event_queue *)
psim_event_queue(psim *system)
{
  return system->events;
}



STATIC_INLINE_PSIM\
(void)
psim_max_iterations_exceeded(void *data)
{
  psim *system = data;
  psim_halt(system,
	    system->nr_cpus, /* halted during an event */
	    was_signalled,
	    -1);
}


INLINE_PSIM\
(void)
psim_init(psim *system)
{
  int cpu_nr;

  /* scrub the monitor */
  mon_init(system->monitor, system->nr_cpus);

  /* trash any pending events */
  event_queue_init(system->events);

  /* if needed, schedule a halt event.  FIXME - In the future this
     will be replaced by a more generic change to psim_command().  A
     new command `schedule NNN halt' being added. */
  if (tree_find_property(system->devices, "/openprom/options/max-iterations")) {
    event_queue_schedule(system->events,
			 tree_find_integer_property(system->devices,
						    "/openprom/options/max-iterations") - 2,
			 psim_max_iterations_exceeded,
			 system);
  }

  /* scrub all the cpus */
  for (cpu_nr = 0; cpu_nr < system->nr_cpus; cpu_nr++)
    cpu_init(system->processors[cpu_nr]);

  /* init all the devices (which updates the cpus) */
  tree_init(system->devices, system);

  /* and the emulation (which needs an initialized device tree) */
  os_emul_init(system->os_emulation, system->nr_cpus);

  /* now sync each cpu against the initialized state of its registers */
  for (cpu_nr = 0; cpu_nr < system->nr_cpus; cpu_nr++) {
    cpu *processor = system->processors[cpu_nr];
    cpu_synchronize_context(processor, cpu_get_program_counter(processor));
    cpu_page_tlb_invalidate_all(processor);
  }

  /* force loop to start with first cpu */
  system->last_cpu = -1;
}

INLINE_PSIM\
(void)
psim_stack(psim *system,
	   char **argv,
	   char **envp)
{
  /* pass the stack device the argv/envp and let it work out what to
     do with it */
  device *stack_device = tree_find_device(system->devices,
					  "/openprom/init/stack");
  if (stack_device != (device*)0) {
    unsigned_word stack_pointer;
    ASSERT (psim_read_register(system, 0, &stack_pointer, "sp",
			       cooked_transfer) > 0);
    device_ioctl(stack_device,
		 NULL, /*cpu*/
		 0, /*cia*/
		 device_ioctl_create_stack,
		 stack_pointer,
		 argv,
		 envp);
  }
}



/* SIMULATE INSTRUCTIONS, various different ways of achieving the same
   thing */

INLINE_PSIM\
(void)
psim_step(psim *system)
{
  volatile int keep_running = 0;
  idecode_run_until_stop(system, &keep_running,
			 system->events, system->processors, system->nr_cpus);
}

INLINE_PSIM\
(void)
psim_run(psim *system)
{
  idecode_run(system,
	      system->events, system->processors, system->nr_cpus);
}


/* storage manipulation functions */

INLINE_PSIM\
(int)
psim_read_register(psim *system,
		   int which_cpu,
		   void *buf,
		   const char reg[],
		   transfer_mode mode)
{
  register_descriptions description;
  char *cooked_buf;
  cpu *processor;

  /* find our processor */
  if (which_cpu == MAX_NR_PROCESSORS) {
    if (system->last_cpu == system->nr_cpus
	|| system->last_cpu == -1)
      which_cpu = 0;
    else
      which_cpu = system->last_cpu;
  }
  ASSERT(which_cpu >= 0 && which_cpu < system->nr_cpus);

  processor = system->processors[which_cpu];

  /* find the register description */
  description = register_description(reg);
  if (description.type == reg_invalid)
    return 0;
  cooked_buf = alloca (description.size);

  /* get the cooked value */
  switch (description.type) {

  case reg_gpr:
    *(gpreg*)cooked_buf = cpu_registers(processor)->gpr[description.index];
    break;

  case reg_spr:
    *(spreg*)cooked_buf = cpu_registers(processor)->spr[description.index];
    break;
    
  case reg_sr:
    *(sreg*)cooked_buf = cpu_registers(processor)->sr[description.index];
    break;

  case reg_fpr:
    *(fpreg*)cooked_buf = cpu_registers(processor)->fpr[description.index];
    break;

  case reg_pc:
    *(unsigned_word*)cooked_buf = cpu_get_program_counter(processor);
    break;

  case reg_cr:
    *(creg*)cooked_buf = cpu_registers(processor)->cr;
    break;

  case reg_msr:
    *(msreg*)cooked_buf = cpu_registers(processor)->msr;
    break;

  case reg_fpscr:
    *(fpscreg*)cooked_buf = cpu_registers(processor)->fpscr;
    break;

  case reg_insns:
    *(unsigned_word*)cooked_buf = mon_get_number_of_insns(system->monitor,
							  which_cpu);
    break;

  case reg_stalls:
    if (cpu_model(processor) == NULL)
      error("$stalls only valid if processor unit model enabled (-I)\n");
    *(unsigned_word*)cooked_buf = model_get_number_of_stalls(cpu_model(processor));
    break;

  case reg_cycles:
    if (cpu_model(processor) == NULL)
      error("$cycles only valid if processor unit model enabled (-I)\n");
    *(unsigned_word*)cooked_buf = model_get_number_of_cycles(cpu_model(processor));
    break;

#ifdef WITH_ALTIVEC
  case reg_vr:
    *(vreg*)cooked_buf = cpu_registers(processor)->altivec.vr[description.index];
    break;

  case reg_vscr:
    *(vscreg*)cooked_buf = cpu_registers(processor)->altivec.vscr;
    break;
#endif

#ifdef WITH_E500
  case reg_gprh:
    *(gpreg*)cooked_buf = cpu_registers(processor)->e500.gprh[description.index];
    break;

  case reg_evr:
    *(unsigned64*)cooked_buf = EVR(description.index);
    break;

  case reg_acc:
    *(accreg*)cooked_buf = cpu_registers(processor)->e500.acc;
    break;
#endif

  default:
    printf_filtered("psim_read_register(processor=0x%lx,buf=0x%lx,reg=%s) %s\n",
		    (unsigned long)processor, (unsigned long)buf, reg,
		    "read of this register unimplemented");
    break;

  }

  /* the PSIM internal values are in host order.  To fetch raw data,
     they need to be converted into target order and then returned */
  if (mode == raw_transfer) {
    /* FIXME - assumes that all registers are simple integers */
    switch (description.size) {
    case 1: 
      *(unsigned_1*)buf = H2T_1(*(unsigned_1*)cooked_buf);
      break;
    case 2:
      *(unsigned_2*)buf = H2T_2(*(unsigned_2*)cooked_buf);
      break;
    case 4:
      *(unsigned_4*)buf = H2T_4(*(unsigned_4*)cooked_buf);
      break;
    case 8:
      *(unsigned_8*)buf = H2T_8(*(unsigned_8*)cooked_buf);
      break;
#ifdef WITH_ALTIVEC
    case 16:
      if (CURRENT_HOST_BYTE_ORDER != CURRENT_TARGET_BYTE_ORDER)
        {
	  union { vreg v; unsigned_8 d[2]; } h, t;
          memcpy(&h.v/*dest*/, cooked_buf/*src*/, description.size);
	  { _SWAP_8(t.d[0] =, h.d[1]); }
	  { _SWAP_8(t.d[1] =, h.d[0]); }
          memcpy(buf/*dest*/, &t/*src*/, description.size);
          break;
        }
      else
        memcpy(buf/*dest*/, cooked_buf/*src*/, description.size);
      break;
#endif
    }
  }
  else {
    memcpy(buf/*dest*/, cooked_buf/*src*/, description.size);
  }

  return description.size;
}



INLINE_PSIM\
(int)
psim_write_register(psim *system,
		    int which_cpu,
		    const void *buf,
		    const char reg[],
		    transfer_mode mode)
{
  cpu *processor;
  register_descriptions description;
  char *cooked_buf;

  /* find our processor */
  if (which_cpu == MAX_NR_PROCESSORS) {
    if (system->last_cpu == system->nr_cpus
	|| system->last_cpu == -1)
      which_cpu = 0;
    else
      which_cpu = system->last_cpu;
  }

  /* find the description of the register */
  description = register_description(reg);
  if (description.type == reg_invalid)
    return 0;
  cooked_buf = alloca (description.size);

  if (which_cpu == -1) {
    int i;
    for (i = 0; i < system->nr_cpus; i++)
      psim_write_register(system, i, buf, reg, mode);
    return description.size;
  }
  ASSERT(which_cpu >= 0 && which_cpu < system->nr_cpus);

  processor = system->processors[which_cpu];

  /* If the data is comming in raw (target order), need to cook it
     into host order before putting it into PSIM's internal structures */
  if (mode == raw_transfer) {
    switch (description.size) {
    case 1: 
      *(unsigned_1*)cooked_buf = T2H_1(*(unsigned_1*)buf);
      break;
    case 2:
      *(unsigned_2*)cooked_buf = T2H_2(*(unsigned_2*)buf);
      break;
    case 4:
      *(unsigned_4*)cooked_buf = T2H_4(*(unsigned_4*)buf);
      break;
    case 8:
      *(unsigned_8*)cooked_buf = T2H_8(*(unsigned_8*)buf);
      break;
#ifdef WITH_ALTIVEC
    case 16:
      if (CURRENT_HOST_BYTE_ORDER != CURRENT_TARGET_BYTE_ORDER)
        {
	  union { vreg v; unsigned_8 d[2]; } h, t;
          memcpy(&t.v/*dest*/, buf/*src*/, description.size);
	  { _SWAP_8(h.d[0] =, t.d[1]); }
	  { _SWAP_8(h.d[1] =, t.d[0]); }
          memcpy(cooked_buf/*dest*/, &h/*src*/, description.size);
          break;
        }
      else
        memcpy(cooked_buf/*dest*/, buf/*src*/, description.size);
#endif
    }
  }
  else {
    memcpy(cooked_buf/*dest*/, buf/*src*/, description.size);
  }

  /* put the cooked value into the register */
  switch (description.type) {

  case reg_gpr:
    cpu_registers(processor)->gpr[description.index] = *(gpreg*)cooked_buf;
    break;

  case reg_fpr:
    cpu_registers(processor)->fpr[description.index] = *(fpreg*)cooked_buf;
    break;

  case reg_pc:
    cpu_set_program_counter(processor, *(unsigned_word*)cooked_buf);
    break;

  case reg_spr:
    cpu_registers(processor)->spr[description.index] = *(spreg*)cooked_buf;
    break;

  case reg_sr:
    cpu_registers(processor)->sr[description.index] = *(sreg*)cooked_buf;
    break;

  case reg_cr:
    cpu_registers(processor)->cr = *(creg*)cooked_buf;
    break;

  case reg_msr:
    cpu_registers(processor)->msr = *(msreg*)cooked_buf;
    break;

  case reg_fpscr:
    cpu_registers(processor)->fpscr = *(fpscreg*)cooked_buf;
    break;

#ifdef WITH_E500
  case reg_gprh:
    cpu_registers(processor)->e500.gprh[description.index] = *(gpreg*)cooked_buf;
    break;

  case reg_evr:
    {
      unsigned64 v;
      v = *(unsigned64*)cooked_buf;
      cpu_registers(processor)->e500.gprh[description.index] = v >> 32;
      cpu_registers(processor)->gpr[description.index] = v;
      break;
    }

  case reg_acc:
    cpu_registers(processor)->e500.acc = *(accreg*)cooked_buf;
    break;
#endif

#ifdef WITH_ALTIVEC
  case reg_vr:
    cpu_registers(processor)->altivec.vr[description.index] = *(vreg*)cooked_buf;
    break;

  case reg_vscr:
    cpu_registers(processor)->altivec.vscr = *(vscreg*)cooked_buf;
    break;
#endif

  default:
    printf_filtered("psim_write_register(processor=0x%lx,cooked_buf=0x%lx,reg=%s) %s\n",
		    (unsigned long)processor, (unsigned long)cooked_buf, reg,
		    "read of this register unimplemented");
    break;

  }

  return description.size;
}



INLINE_PSIM\
(unsigned)
psim_read_memory(psim *system,
		 int which_cpu,
		 void *buffer,
		 unsigned_word vaddr,
		 unsigned nr_bytes)
{
  cpu *processor;
  if (which_cpu == MAX_NR_PROCESSORS) {
    if (system->last_cpu == system->nr_cpus
	|| system->last_cpu == -1)
      which_cpu = 0;
    else
      which_cpu = system->last_cpu;
  }
  processor = system->processors[which_cpu];
  return vm_data_map_read_buffer(cpu_data_map(processor),
				 buffer, vaddr, nr_bytes,
				 NULL, -1);
}


INLINE_PSIM\
(unsigned)
psim_write_memory(psim *system,
		  int which_cpu,
		  const void *buffer,
		  unsigned_word vaddr,
		  unsigned nr_bytes,
		  int violate_read_only_section)
{
  cpu *processor;
  if (which_cpu == MAX_NR_PROCESSORS) {
    if (system->last_cpu == system->nr_cpus
	|| system->last_cpu == -1)
      which_cpu = 0;
    else
      which_cpu = system->last_cpu;
  }
  ASSERT(which_cpu >= 0 && which_cpu < system->nr_cpus);
  processor = system->processors[which_cpu];
  return vm_data_map_write_buffer(cpu_data_map(processor),
				  buffer, vaddr, nr_bytes, 1/*violate-read-only*/,
				  NULL, -1);
}


INLINE_PSIM\
(void)
psim_print_info(psim *system,
		int verbose)
{
  mon_print_info(system, system->monitor, verbose);
}


/* Merge a device tree and a device file. */

INLINE_PSIM\
(void)
psim_merge_device_file(device *root,
		       const char *file_name)
{
  FILE *description;
  int line_nr;
  char device_path[1000];
  device *current;

  /* try opening the file */
  description = fopen(file_name, "r");
  if (description == NULL) {
    perror(file_name);
    error("Invalid file %s specified", file_name);
  }

  line_nr = 0;
  current = root;
  while (fgets(device_path, sizeof(device_path), description)) {
    char *device;
    /* check that the full line was read */
    if (strchr(device_path, '\n') == NULL) {
      fclose(description);
      error("%s:%d: line to long - %s",
	    file_name, line_nr, device_path);
    }
    else
      *strchr(device_path, '\n') = '\0';
    line_nr++;
    /* skip comments ("#" or ";") and blank lines lines */
    for (device = device_path;
	 *device != '\0' && isspace(*device);
	 device++);
    if (device[0] == '#'
	|| device[0] == ';'
	|| device[0] == '\0')
      continue;
    /* merge any appended lines */
    while (device_path[strlen(device_path) - 1] == '\\') {
      int curlen = strlen(device_path) - 1;
      /* zap \ */
      device_path[curlen] = '\0';
      /* append the next line */
      if (!fgets(device_path + curlen, sizeof(device_path) - curlen, description)) {
	fclose(description);
	error("%s:%s: unexpected eof in line continuation - %s",
	      file_name, line_nr, device_path);
      }
      if (strchr(device_path, '\n') == NULL) {
	fclose(description);
	error("%s:%d: line to long - %s",
	    file_name, line_nr, device_path);
      }
      else
	*strchr(device_path, '\n') = '\0';
      line_nr++;
    }
    /* parse this line */
    current = tree_parse(current, "%s", device);
  }
  fclose(description);
}


#endif /* _PSIM_C_ */
