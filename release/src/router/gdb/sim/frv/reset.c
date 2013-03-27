/* frv simulator support code
   Copyright (C) 1999, 2000, 2001, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

This file is part of the GNU simulators.

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

#define WANT_CPU
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "bfd.h"

/* Initialize the frv simulator.  */
void
frv_initialize (SIM_CPU *current_cpu, SIM_DESC sd)
{
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (current_cpu);
  PROFILE_DATA *p = CPU_PROFILE_DATA (current_cpu);
  FRV_CACHE *insn_cache = CPU_INSN_CACHE (current_cpu);
  FRV_CACHE *data_cache = CPU_DATA_CACHE (current_cpu);
  int insn_cache_enabled = CACHE_INITIALIZED (insn_cache);
  int data_cache_enabled = CACHE_INITIALIZED (data_cache);
  USI hsr0;

  /* Initialize the register control information first since some of the
     register values are used in further configuration.  */
  frv_register_control_init (current_cpu);

  /* We need to ensure that the caches are initialized even if they are not
     initially enabled (via commandline) because they can be enabled by
     software.  */
  if (! insn_cache_enabled)
    frv_cache_init (current_cpu, CPU_INSN_CACHE (current_cpu));
  if (! data_cache_enabled)
    frv_cache_init (current_cpu, CPU_DATA_CACHE (current_cpu));

  /* Set the default cpu frequency if it has not been set on the command
     line.  */
  if (PROFILE_CPU_FREQ (p) == 0)
    PROFILE_CPU_FREQ (p) = 266000000; /* 266MHz */

  /* Allocate one cache line of memory containing the address of the reset
     register Use the largest of the insn cache line size and the data cache
     line size.  */
  {
    int addr = RSTR_ADDRESS;
    void *aligned_buffer;
    int bytes;

    if (CPU_INSN_CACHE (current_cpu)->line_size
	> CPU_DATA_CACHE (current_cpu)->line_size)
      bytes = CPU_INSN_CACHE (current_cpu)->line_size;
    else
      bytes = CPU_DATA_CACHE (current_cpu)->line_size;

    /* 'bytes' is a power of 2. Calculate the starting address of the
       cache line.  */
    addr &= ~(bytes - 1);
    aligned_buffer = zalloc (bytes); /* clear */
    sim_core_attach (sd, NULL, 0, access_read_write, 0, addr, bytes,
		     0, NULL, aligned_buffer);
  }

  PROFILE_INFO_CPU_CALLBACK(p) = frv_profile_info;
  ps->insn_fetch_address = -1;
  ps->branch_address = -1;

  cgen_init_accurate_fpu (current_cpu, CGEN_CPU_FPU (current_cpu),
			  frvbf_fpu_error);

  /* Now perform power-on reset.  */
  frv_power_on_reset (current_cpu);

  /* Make sure that HSR0.ICE and HSR0.DCE are set properly.  */
  hsr0 = GET_HSR0 ();
  if (insn_cache_enabled)
    SET_HSR0_ICE (hsr0);
  else
    CLEAR_HSR0_ICE (hsr0);
  if (data_cache_enabled)
    SET_HSR0_DCE (hsr0);
  else
    CLEAR_HSR0_DCE (hsr0);
  SET_HSR0 (hsr0);
}

/* Initialize the frv simulator.  */
void
frv_term (SIM_DESC sd)
{
  /* If the timer is enabled, and model profiling was not originally enabled,
     then turn it off again.  This is the only place we can currently gain
     control to do this.  */
  if (frv_interrupt_state.timer.enabled && ! frv_save_profile_model_p)
    sim_profile_set_option (current_state, "-model", PROFILE_MODEL_IDX, "0");
}

/* Perform a power on reset.  */
void
frv_power_on_reset (SIM_CPU *cpu)
{
  /* GR, FR and CPR registers are undefined at initialization time.  */
  frv_initialize_spr (cpu);
  /* Initialize the RSTR register (in memory).  */
  if (frv_cache_enabled (CPU_DATA_CACHE (cpu)))
    frvbf_mem_set_SI (cpu, CPU_PC_GET (cpu), RSTR_ADDRESS, RSTR_INITIAL_VALUE);
  else
    SETMEMSI (cpu, CPU_PC_GET (cpu), RSTR_ADDRESS, RSTR_INITIAL_VALUE);
}

/* Perform a hardware reset.  */
void
frv_hardware_reset (SIM_CPU *cpu)
{
  /* GR, FR and CPR registers are undefined at hardware reset.  */
  frv_initialize_spr (cpu);
  /* Reset the RSTR register (in memory).  */
  if (frv_cache_enabled (CPU_DATA_CACHE (cpu)))
    frvbf_mem_set_SI (cpu, CPU_PC_GET (cpu), RSTR_ADDRESS, RSTR_HARDWARE_RESET);
  else
    SETMEMSI (cpu, CPU_PC_GET (cpu), RSTR_ADDRESS, RSTR_HARDWARE_RESET);
  /* Reset the insn and data caches.  */
  frv_cache_invalidate_all (CPU_INSN_CACHE (cpu), 0/* no flush */);
  frv_cache_invalidate_all (CPU_DATA_CACHE (cpu), 0/* no flush */);
}

/* Perform a software reset.  */
void
frv_software_reset (SIM_CPU *cpu)
{
  /* GR, FR and CPR registers are undefined at software reset.  */
  frv_reset_spr (cpu);
  /* Reset the RSTR register (in memory).  */
  if (frv_cache_enabled (CPU_DATA_CACHE (cpu)))
    frvbf_mem_set_SI (cpu, CPU_PC_GET (cpu), RSTR_ADDRESS, RSTR_SOFTWARE_RESET);
  else
    SETMEMSI (cpu, CPU_PC_GET (cpu), RSTR_ADDRESS, RSTR_SOFTWARE_RESET);
}
