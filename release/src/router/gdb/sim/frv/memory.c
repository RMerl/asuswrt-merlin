/* frv memory model.
   Copyright (C) 1999, 2000, 2001, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "cgen-mem.h"
#include "bfd.h"

/* Check for alignment and access restrictions.  Return the corrected address.
 */
static SI
fr400_check_data_read_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  /* Check access restrictions for double word loads only.  */
  if (align_mask == 7)
    {
      if ((USI)address >= 0xfe800000 && (USI)address <= 0xfeffffff)
	frv_queue_data_access_error_interrupt (current_cpu, address);
    }
  return address;
}

static SI
fr500_check_data_read_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if (address & align_mask)
    {
      frv_queue_mem_address_not_aligned_interrupt (current_cpu, address);
      address &= ~align_mask;
    }

  if ((USI)address >= 0xfeff0600 && (USI)address <= 0xfeff7fff
      || (USI)address >= 0xfe800000 && (USI)address <= 0xfefeffff)
    frv_queue_data_access_error_interrupt (current_cpu, address);

  return address;
}

static SI
fr550_check_data_read_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if ((USI)address >= 0xfe800000 && (USI)address <= 0xfefeffff
      || (align_mask > 0x3
	  && ((USI)address >= 0xfeff0000 && (USI)address <= 0xfeffffff)))
    frv_queue_data_access_error_interrupt (current_cpu, address);

  return address;
}

static SI
check_data_read_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      address = fr400_check_data_read_address (current_cpu, address,
					       align_mask);
      break;
    case bfd_mach_frvtomcat:
    case bfd_mach_fr500:
    case bfd_mach_frv:
      address = fr500_check_data_read_address (current_cpu, address,
					       align_mask);
      break;
    case bfd_mach_fr550:
      address = fr550_check_data_read_address (current_cpu, address,
					       align_mask);
      break;
    default:
      break;
    }

  return address;
}

static SI
fr400_check_readwrite_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if (address & align_mask)
    {
      /* Make sure that this exception is not masked.  */
      USI isr = GET_ISR ();
      if (! GET_ISR_EMAM (isr))
	{
	  /* Bad alignment causes a data_access_error on fr400.  */
	  frv_queue_data_access_error_interrupt (current_cpu, address);
	}
      address &= ~align_mask;
    }
  /* Nothing to check.  */
  return address;
}

static SI
fr500_check_readwrite_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if ((USI)address >= 0xfe000000 && (USI)address <= 0xfe003fff
      || (USI)address >= 0xfe004000 && (USI)address <= 0xfe3fffff
      || (USI)address >= 0xfe400000 && (USI)address <= 0xfe403fff
      || (USI)address >= 0xfe404000 && (USI)address <= 0xfe7fffff)
    frv_queue_data_access_exception_interrupt (current_cpu);

  return address;
}

static SI
fr550_check_readwrite_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  /* No alignment restrictions on fr550 */

  if ((USI)address >= 0xfe000000 && (USI)address <= 0xfe3fffff
      || (USI)address >= 0xfe408000 && (USI)address <= 0xfe7fffff)
    frv_queue_data_access_exception_interrupt (current_cpu);
  else
    {
      USI hsr0 = GET_HSR0 ();
      if (! GET_HSR0_RME (hsr0)
	  && (USI)address >= 0xfe400000 && (USI)address <= 0xfe407fff)
	frv_queue_data_access_exception_interrupt (current_cpu);
    }

  return address;
}

static SI
check_readwrite_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      address = fr400_check_readwrite_address (current_cpu, address,
						    align_mask);
      break;
    case bfd_mach_frvtomcat:
    case bfd_mach_fr500:
    case bfd_mach_frv:
      address = fr500_check_readwrite_address (current_cpu, address,
						    align_mask);
      break;
    case bfd_mach_fr550:
      address = fr550_check_readwrite_address (current_cpu, address,
					       align_mask);
      break;
    default:
      break;
    }

  return address;
}

static PCADDR
fr400_check_insn_read_address (SIM_CPU *current_cpu, PCADDR address,
			       int align_mask)
{
  if (address & align_mask)
    {
      frv_queue_instruction_access_error_interrupt (current_cpu);
      address &= ~align_mask;
    }
  else if ((USI)address >= 0xfe800000 && (USI)address <= 0xfeffffff)
    frv_queue_instruction_access_error_interrupt (current_cpu);

  return address;
}

static PCADDR
fr500_check_insn_read_address (SIM_CPU *current_cpu, PCADDR address,
			       int align_mask)
{
  if (address & align_mask)
    {
      frv_queue_mem_address_not_aligned_interrupt (current_cpu, address);
      address &= ~align_mask;
    }

  if ((USI)address >= 0xfeff0600 && (USI)address <= 0xfeff7fff
      || (USI)address >= 0xfe800000 && (USI)address <= 0xfefeffff)
    frv_queue_instruction_access_error_interrupt (current_cpu);
  else if ((USI)address >= 0xfe004000 && (USI)address <= 0xfe3fffff
	   || (USI)address >= 0xfe400000 && (USI)address <= 0xfe403fff
	   || (USI)address >= 0xfe404000 && (USI)address <= 0xfe7fffff)
    frv_queue_instruction_access_exception_interrupt (current_cpu);
  else
    {
      USI hsr0 = GET_HSR0 ();
      if (! GET_HSR0_RME (hsr0)
	  && (USI)address >= 0xfe000000 && (USI)address <= 0xfe003fff)
	frv_queue_instruction_access_exception_interrupt (current_cpu);
    }

  return address;
}

static PCADDR
fr550_check_insn_read_address (SIM_CPU *current_cpu, PCADDR address,
			       int align_mask)
{
  address &= ~align_mask;

  if ((USI)address >= 0xfe800000 && (USI)address <= 0xfeffffff)
    frv_queue_instruction_access_error_interrupt (current_cpu);
  else if ((USI)address >= 0xfe008000 && (USI)address <= 0xfe7fffff)
    frv_queue_instruction_access_exception_interrupt (current_cpu);
  else
    {
      USI hsr0 = GET_HSR0 ();
      if (! GET_HSR0_RME (hsr0)
	  && (USI)address >= 0xfe000000 && (USI)address <= 0xfe007fff)
	frv_queue_instruction_access_exception_interrupt (current_cpu);
    }

  return address;
}

static PCADDR
check_insn_read_address (SIM_CPU *current_cpu, PCADDR address, int align_mask)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      address = fr400_check_insn_read_address (current_cpu, address,
					       align_mask);
      break;
    case bfd_mach_frvtomcat:
    case bfd_mach_fr500:
    case bfd_mach_frv:
      address = fr500_check_insn_read_address (current_cpu, address,
					       align_mask);
      break;
    case bfd_mach_fr550:
      address = fr550_check_insn_read_address (current_cpu, address,
					       align_mask);
      break;
    default:
      break;
    }

  return address;
}

/* Memory reads.  */
QI
frvbf_read_mem_QI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  USI hsr0 = GET_HSR0 ();
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 0);
  address = check_readwrite_address (current_cpu, address, 0);
  
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 1;
      CPU_LOAD_SIGNED (current_cpu) = 1;
      return 0xb7; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, QI, 1);
    }

  return GETMEMQI (current_cpu, pc, address);
}

UQI
frvbf_read_mem_UQI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  USI hsr0 = GET_HSR0 ();
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 0);
  address = check_readwrite_address (current_cpu, address, 0);
  
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 1;
      CPU_LOAD_SIGNED (current_cpu) = 0;
      return 0xb7; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, UQI, 1);
    }

  return GETMEMUQI (current_cpu, pc, address);
}

/* Read a HI which spans two cache lines */
static HI
read_mem_unaligned_HI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  HI value = frvbf_read_mem_QI (current_cpu, pc, address);
  value <<= 8;
  value |= frvbf_read_mem_UQI (current_cpu, pc, address + 1);
  return T2H_2 (value);
}

HI
frvbf_read_mem_HI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  USI hsr0;
  FRV_CACHE *cache;

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 1);
  address = check_readwrite_address (current_cpu, address, 1);
  
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  hsr0 = GET_HSR0 ();
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 2;
      CPU_LOAD_SIGNED (current_cpu) = 1;
      return 0xb711; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 2))
	    return read_mem_unaligned_HI (current_cpu, pc, address); 
	}
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, HI, 2);
    }

  return GETMEMHI (current_cpu, pc, address);
}

UHI
frvbf_read_mem_UHI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  USI hsr0;
  FRV_CACHE *cache;

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 1);
  address = check_readwrite_address (current_cpu, address, 1);
  
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  hsr0 = GET_HSR0 ();
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 2;
      CPU_LOAD_SIGNED (current_cpu) = 0;
      return 0xb711; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 2))
	    return read_mem_unaligned_HI (current_cpu, pc, address); 
	}
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, UHI, 2);
    }

  return GETMEMUHI (current_cpu, pc, address);
}

/* Read a SI which spans two cache lines */
static SI
read_mem_unaligned_SI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  unsigned hi_len = cache->line_size - (address & (cache->line_size - 1));
  char valarray[4];
  SI SIvalue;
  HI HIvalue;

  switch (hi_len)
    {
    case 1:
      valarray[0] = frvbf_read_mem_QI (current_cpu, pc, address);
      SIvalue = frvbf_read_mem_SI (current_cpu, pc, address + 1);
      SIvalue = H2T_4 (SIvalue);
      memcpy (valarray + 1, (char*)&SIvalue, 3);
      break;
    case 2:
      HIvalue = frvbf_read_mem_HI (current_cpu, pc, address);
      HIvalue = H2T_2 (HIvalue);
      memcpy (valarray, (char*)&HIvalue, 2);
      HIvalue = frvbf_read_mem_HI (current_cpu, pc, address + 2);
      HIvalue = H2T_2 (HIvalue);
      memcpy (valarray + 2, (char*)&HIvalue, 2);
      break;
    case 3:
      SIvalue = frvbf_read_mem_SI (current_cpu, pc, address - 1);
      SIvalue = H2T_4 (SIvalue);
      memcpy (valarray, (char*)&SIvalue, 3);
      valarray[3] = frvbf_read_mem_QI (current_cpu, pc, address + 3);
      break;
    default:
      abort (); /* can't happen */
    }
  return T2H_4 (*(SI*)valarray);
}

SI
frvbf_read_mem_SI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  FRV_CACHE *cache;
  USI hsr0;

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 3);
  address = check_readwrite_address (current_cpu, address, 3);
  
  hsr0 = GET_HSR0 ();
  cache = CPU_DATA_CACHE (current_cpu);
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 4;
      return 0x37111319; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 4))
	    return read_mem_unaligned_SI (current_cpu, pc, address); 
	}
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, SI, 4);
    }

  return GETMEMSI (current_cpu, pc, address);
}

SI
frvbf_read_mem_WI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  return frvbf_read_mem_SI (current_cpu, pc, address);
}

/* Read a SI which spans two cache lines */
static DI
read_mem_unaligned_DI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  unsigned hi_len = cache->line_size - (address & (cache->line_size - 1));
  DI value, value1;

  switch (hi_len)
    {
    case 1:
      value = frvbf_read_mem_QI (current_cpu, pc, address);
      value <<= 56;
      value1 = frvbf_read_mem_DI (current_cpu, pc, address + 1);
      value1 = H2T_8 (value1);
      value |= value1 & ((DI)0x00ffffff << 32);
      value |= value1 & 0xffffffffu;
      break;
    case 2:
      value = frvbf_read_mem_HI (current_cpu, pc, address);
      value = H2T_2 (value);
      value <<= 48;
      value1 = frvbf_read_mem_DI (current_cpu, pc, address + 2);
      value1 = H2T_8 (value1);
      value |= value1 & ((DI)0x0000ffff << 32);
      value |= value1 & 0xffffffffu;
      break;
    case 3:
      value = frvbf_read_mem_SI (current_cpu, pc, address - 1);
      value = H2T_4 (value);
      value <<= 40;
      value1 = frvbf_read_mem_DI (current_cpu, pc, address + 3);
      value1 = H2T_8 (value1);
      value |= value1 & ((DI)0x000000ff << 32);
      value |= value1 & 0xffffffffu;
      break;
    case 4:
      value = frvbf_read_mem_SI (current_cpu, pc, address);
      value = H2T_4 (value);
      value <<= 32;
      value1 = frvbf_read_mem_SI (current_cpu, pc, address + 4);
      value1 = H2T_4 (value1);
      value |= value1 & 0xffffffffu;
      break;
    case 5:
      value = frvbf_read_mem_DI (current_cpu, pc, address - 3);
      value = H2T_8 (value);
      value <<= 24;
      value1 = frvbf_read_mem_SI (current_cpu, pc, address + 5);
      value1 = H2T_4 (value1);
      value |= value1 & 0x00ffffff;
      break;
    case 6:
      value = frvbf_read_mem_DI (current_cpu, pc, address - 2);
      value = H2T_8 (value);
      value <<= 16;
      value1 = frvbf_read_mem_HI (current_cpu, pc, address + 6);
      value1 = H2T_2 (value1);
      value |= value1 & 0x0000ffff;
      break;
    case 7:
      value = frvbf_read_mem_DI (current_cpu, pc, address - 1);
      value = H2T_8 (value);
      value <<= 8;
      value1 = frvbf_read_mem_QI (current_cpu, pc, address + 7);
      value |= value1 & 0x000000ff;
      break;
    default:
      abort (); /* can't happen */
    }
  return T2H_8 (value);
}

DI
frvbf_read_mem_DI (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  USI hsr0;
  FRV_CACHE *cache;

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 7);
  address = check_readwrite_address (current_cpu, address, 7);
  
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  hsr0 = GET_HSR0 ();
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 8;
      return 0x37111319; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 8))
	    return read_mem_unaligned_DI (current_cpu, pc, address); 
	}
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, DI, 8);
    }

  return GETMEMDI (current_cpu, pc, address);
}

DF
frvbf_read_mem_DF (SIM_CPU *current_cpu, IADDR pc, SI address)
{
  USI hsr0;
  FRV_CACHE *cache;

  /* Check for access exceptions.  */
  address = check_data_read_address (current_cpu, address, 7);
  address = check_readwrite_address (current_cpu, address, 7);
  
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  hsr0 = GET_HSR0 ();
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 8;
      return 0x37111319; /* any random value */
    }

  if (GET_HSR0_DCE (hsr0))
    {
      int cycles;
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 8))
	    return read_mem_unaligned_DI (current_cpu, pc, address); 
	}
      cycles = frv_cache_read (cache, 0, address);
      if (cycles != 0)
	return CACHE_RETURN_DATA (cache, 0, address, DF, 8);
    }

  return GETMEMDF (current_cpu, pc, address);
}

USI
frvbf_read_imem_USI (SIM_CPU *current_cpu, PCADDR vpc)
{
  USI hsr0;
  vpc = check_insn_read_address (current_cpu, vpc, 3);

  hsr0 = GET_HSR0 ();
  if (GET_HSR0_ICE (hsr0))
    {
      FRV_CACHE *cache;
      USI value;

      /* We don't want this to show up in the cache statistics.  That read
	 is done in frvbf_simulate_insn_prefetch.  So read the cache or memory
	 passively here.  */
      cache = CPU_INSN_CACHE (current_cpu);
      if (frv_cache_read_passive_SI (cache, vpc, &value))
	return value;
    }
  return sim_core_read_unaligned_4 (current_cpu, vpc, read_map, vpc);
}

static SI
fr400_check_write_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if (align_mask == 7
      && address >= 0xfe800000 && address <= 0xfeffffff)
    frv_queue_program_interrupt (current_cpu, FRV_DATA_STORE_ERROR);

  return address;
}

static SI
fr500_check_write_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if (address & align_mask)
    {
      struct frv_interrupt_queue_element *item =
	frv_queue_mem_address_not_aligned_interrupt (current_cpu, address);
      /* Record the correct vliw slot with the interrupt.  */
      if (item != NULL)
	item->slot = frv_interrupt_state.slot;
      address &= ~align_mask;
    }
  if (address >= 0xfeff0600 && address <= 0xfeff7fff
      || address >= 0xfe800000 && address <= 0xfefeffff)
    frv_queue_program_interrupt (current_cpu, FRV_DATA_STORE_ERROR);

  return address;
}

static SI
fr550_check_write_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if ((USI)address >= 0xfe800000 && (USI)address <= 0xfefeffff
      || (align_mask > 0x3
	  && ((USI)address >= 0xfeff0000 && (USI)address <= 0xfeffffff)))
    frv_queue_program_interrupt (current_cpu, FRV_DATA_STORE_ERROR);

  return address;
}

static SI
check_write_address (SIM_CPU *current_cpu, SI address, int align_mask)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
      address = fr400_check_write_address (current_cpu, address, align_mask);
      break;
    case bfd_mach_frvtomcat:
    case bfd_mach_fr500:
    case bfd_mach_frv:
      address = fr500_check_write_address (current_cpu, address, align_mask);
      break;
    case bfd_mach_fr550:
      address = fr550_check_write_address (current_cpu, address, align_mask);
      break;
    default:
      break;
    }
  return address;
}

void
frvbf_write_mem_QI (SIM_CPU *current_cpu, IADDR pc, SI address, QI value)
{
  USI hsr0;
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_qi_write (current_cpu, frvbf_mem_set_QI, address, value);
  else
    sim_queue_mem_qi_write (current_cpu, address, value);
  frv_set_write_queue_slot (current_cpu);
}

void
frvbf_write_mem_UQI (SIM_CPU *current_cpu, IADDR pc, SI address, UQI value)
{
  frvbf_write_mem_QI (current_cpu, pc, address, value);
}

void
frvbf_write_mem_HI (SIM_CPU *current_cpu, IADDR pc, SI address, HI value)
{
  USI hsr0;
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_hi_write (current_cpu, frvbf_mem_set_HI, address, value);
  else
    sim_queue_mem_hi_write (current_cpu, address, value);
  frv_set_write_queue_slot (current_cpu);
}

void
frvbf_write_mem_UHI (SIM_CPU *current_cpu, IADDR pc, SI address, UHI value)
{
  frvbf_write_mem_HI (current_cpu, pc, address, value);
}

void
frvbf_write_mem_SI (SIM_CPU *current_cpu, IADDR pc, SI address, SI value)
{
  USI hsr0;
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_si_write (current_cpu, frvbf_mem_set_SI, address, value);
  else
    sim_queue_mem_si_write (current_cpu, address, value);
  frv_set_write_queue_slot (current_cpu);
}

void
frvbf_write_mem_WI (SIM_CPU *current_cpu, IADDR pc, SI address, SI value)
{
  frvbf_write_mem_SI (current_cpu, pc, address, value);
}

void
frvbf_write_mem_DI (SIM_CPU *current_cpu, IADDR pc, SI address, DI value)
{
  USI hsr0;
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_di_write (current_cpu, frvbf_mem_set_DI, address, value);
  else
    sim_queue_mem_di_write (current_cpu, address, value);
  frv_set_write_queue_slot (current_cpu);
}

void
frvbf_write_mem_DF (SIM_CPU *current_cpu, IADDR pc, SI address, DF value)
{
  USI hsr0;
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_df_write (current_cpu, frvbf_mem_set_DF, address, value);
  else
    sim_queue_mem_df_write (current_cpu, address, value);
  frv_set_write_queue_slot (current_cpu);
}

/* Memory writes.  These do the actual writing through the cache.  */
void
frvbf_mem_set_QI (SIM_CPU *current_cpu, IADDR pc, SI address, QI value)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);

  /* Check for access errors.  */
  address = check_write_address (current_cpu, address, 0);
  address = check_readwrite_address (current_cpu, address, 0);

  /* If we need to count cycles, then submit the write request to the cache
     and let it prioritize the request.  Otherwise perform the write now.  */
  if (model_insn)
    {
      int slot = UNIT_I0;
      frv_cache_request_store (cache, address, slot, (char *)&value,
			       sizeof (value));
    }
  else
    frv_cache_write (cache, address, (char *)&value, sizeof (value));
}

/* Write a HI which spans two cache lines */
static void
mem_set_unaligned_HI (SIM_CPU *current_cpu, IADDR pc, SI address, HI value)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  /* value is already in target byte order */
  frv_cache_write (cache, address, (char *)&value, 1);
  frv_cache_write (cache, address + 1, ((char *)&value + 1), 1);
}

void
frvbf_mem_set_HI (SIM_CPU *current_cpu, IADDR pc, SI address, HI value)
{
  FRV_CACHE *cache;

  /* Check for access errors.  */
  address = check_write_address (current_cpu, address, 1);
  address = check_readwrite_address (current_cpu, address, 1);

  /* If we need to count cycles, then submit the write request to the cache
     and let it prioritize the request.  Otherwise perform the write now.  */
  value = H2T_2 (value);
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      int slot = UNIT_I0;
      frv_cache_request_store (cache, address, slot,
			       (char *)&value, sizeof (value));
    }
  else
    {
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 2))
	    {
	      mem_set_unaligned_HI (current_cpu, pc, address, value); 
	      return;
	    }
	}
      frv_cache_write (cache, address, (char *)&value, sizeof (value));
    }
}

/* Write a SI which spans two cache lines */
static void
mem_set_unaligned_SI (SIM_CPU *current_cpu, IADDR pc, SI address, SI value)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  unsigned hi_len = cache->line_size - (address & (cache->line_size - 1));
  /* value is already in target byte order */
  frv_cache_write (cache, address, (char *)&value, hi_len);
  frv_cache_write (cache, address + hi_len, (char *)&value + hi_len, 4 - hi_len);
}

void
frvbf_mem_set_SI (SIM_CPU *current_cpu, IADDR pc, SI address, SI value)
{
  FRV_CACHE *cache;

  /* Check for access errors.  */
  address = check_write_address (current_cpu, address, 3);
  address = check_readwrite_address (current_cpu, address, 3);

  /* If we need to count cycles, then submit the write request to the cache
     and let it prioritize the request.  Otherwise perform the write now.  */
  cache = CPU_DATA_CACHE (current_cpu);
  value = H2T_4 (value);
  if (model_insn)
    {
      int slot = UNIT_I0;
      frv_cache_request_store (cache, address, slot,
			       (char *)&value, sizeof (value));
    }
  else
    {
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 4))
	    {
	      mem_set_unaligned_SI (current_cpu, pc, address, value); 
	      return;
	    }
	}
      frv_cache_write (cache, address, (char *)&value, sizeof (value));
    }
}

/* Write a DI which spans two cache lines */
static void
mem_set_unaligned_DI (SIM_CPU *current_cpu, IADDR pc, SI address, DI value)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  unsigned hi_len = cache->line_size - (address & (cache->line_size - 1));
  /* value is already in target byte order */
  frv_cache_write (cache, address, (char *)&value, hi_len);
  frv_cache_write (cache, address + hi_len, (char *)&value + hi_len, 8 - hi_len);
}

void
frvbf_mem_set_DI (SIM_CPU *current_cpu, IADDR pc, SI address, DI value)
{
  FRV_CACHE *cache;

  /* Check for access errors.  */
  address = check_write_address (current_cpu, address, 7);
  address = check_readwrite_address (current_cpu, address, 7);

  /* If we need to count cycles, then submit the write request to the cache
     and let it prioritize the request.  Otherwise perform the write now.  */
  value = H2T_8 (value);
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      int slot = UNIT_I0;
      frv_cache_request_store (cache, address, slot,
			       (char *)&value, sizeof (value));
    }
  else
    {
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 8))
	    {
	      mem_set_unaligned_DI (current_cpu, pc, address, value); 
	      return;
	    }
	}
      frv_cache_write (cache, address, (char *)&value, sizeof (value));
    }
}

void
frvbf_mem_set_DF (SIM_CPU *current_cpu, IADDR pc, SI address, DF value)
{
  FRV_CACHE *cache;

  /* Check for access errors.  */
  address = check_write_address (current_cpu, address, 7);
  address = check_readwrite_address (current_cpu, address, 7);

  /* If we need to count cycles, then submit the write request to the cache
     and let it prioritize the request.  Otherwise perform the write now.  */
  value = H2T_8 (value);
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      int slot = UNIT_I0;
      frv_cache_request_store (cache, address, slot,
			       (char *)&value, sizeof (value));
    }
  else
    {
      /* Handle access which crosses cache line boundary */
      SIM_DESC sd = CPU_STATE (current_cpu);
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	{
	  if (DATA_CROSSES_CACHE_LINE (cache, address, 8))
	    {
	      mem_set_unaligned_DI (current_cpu, pc, address, value); 
	      return;
	    }
	}
      frv_cache_write (cache, address, (char *)&value, sizeof (value));
    }
}

void
frvbf_mem_set_XI (SIM_CPU *current_cpu, IADDR pc, SI address, SI *value)
{
  int i;
  FRV_CACHE *cache;

  /* Check for access errors.  */
  address = check_write_address (current_cpu, address, 0xf);
  address = check_readwrite_address (current_cpu, address, 0xf);

  /* TODO -- reverse word order as well?  */
  for (i = 0; i < 4; ++i)
    value[i] = H2T_4 (value[i]);

  /* If we need to count cycles, then submit the write request to the cache
     and let it prioritize the request.  Otherwise perform the write now.  */
  cache = CPU_DATA_CACHE (current_cpu);
  if (model_insn)
    {
      int slot = UNIT_I0;
      frv_cache_request_store (cache, address, slot, (char*)value, 16);
    }
  else
    frv_cache_write (cache, address, (char*)value, 16);
}

/* Record the current VLIW slot on the element at the top of the write queue.
*/
void
frv_set_write_queue_slot (SIM_CPU *current_cpu)
{
  FRV_VLIW *vliw = CPU_VLIW (current_cpu);
  int slot = vliw->next_slot - 1;
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (current_cpu);
  int ix = CGEN_WRITE_QUEUE_INDEX (q) - 1;
  CGEN_WRITE_QUEUE_ELEMENT *item = CGEN_WRITE_QUEUE_ELEMENT (q, ix);
  CGEN_WRITE_QUEUE_ELEMENT_PIPE (item) = (*vliw->current_vliw)[slot];
}
