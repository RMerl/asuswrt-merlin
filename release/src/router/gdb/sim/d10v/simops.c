#include "config.h"

#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "d10v_sim.h"
#include "simops.h"
#include "targ-vals.h"

extern char *strrchr ();

enum op_types {
  OP_VOID,
  OP_REG,
  OP_REG_OUTPUT,
  OP_DREG,
  OP_DREG_OUTPUT,
  OP_ACCUM,
  OP_ACCUM_OUTPUT,
  OP_ACCUM_REVERSE,
  OP_CR,
  OP_CR_OUTPUT,
  OP_CR_REVERSE,
  OP_FLAG,
  OP_FLAG_OUTPUT,
  OP_CONSTANT16,
  OP_CONSTANT8,
  OP_CONSTANT3,
  OP_CONSTANT4,
  OP_MEMREF,
  OP_MEMREF2,
  OP_MEMREF3,
  OP_POSTDEC,
  OP_POSTINC,
  OP_PREDEC,
  OP_R0,
  OP_R1,
  OP_R2,
};


enum {
  PSW_MASK = (PSW_SM_BIT
	      | PSW_EA_BIT
	      | PSW_DB_BIT
	      | PSW_IE_BIT
	      | PSW_RP_BIT
	      | PSW_MD_BIT
	      | PSW_FX_BIT
	      | PSW_ST_BIT
	      | PSW_F0_BIT
	      | PSW_F1_BIT
	      | PSW_C_BIT),
  /* The following bits in the PSW _can't_ be set by instructions such
     as mvtc. */
  PSW_HW_MASK = (PSW_MASK | PSW_DM_BIT)
};

reg_t
move_to_cr (int cr, reg_t mask, reg_t val, int psw_hw_p)
{
  /* A MASK bit is set when the corresponding bit in the CR should
     be left alone */
  /* This assumes that (VAL & MASK) == 0 */
  switch (cr)
    {
    case PSW_CR:
      if (psw_hw_p)
	val &= PSW_HW_MASK;
      else
	val &= PSW_MASK;
      if ((mask & PSW_SM_BIT) == 0)
	{
	  int new_psw_sm = (val & PSW_SM_BIT) != 0;
	  /* save old SP */
	  SET_HELD_SP (PSW_SM, GPR (SP_IDX));
	  if (PSW_SM != new_psw_sm)
	    /* restore new SP */
	    SET_GPR (SP_IDX, HELD_SP (new_psw_sm));
	}
      if ((mask & (PSW_ST_BIT | PSW_FX_BIT)) == 0)
	{
	  if (val & PSW_ST_BIT && !(val & PSW_FX_BIT))
	    {
	      (*d10v_callback->printf_filtered)
		(d10v_callback,
		 "ERROR at PC 0x%x: ST can only be set when FX is set.\n",
		 PC<<2);
	      State.exception = SIGILL;
	    }
	}
      /* keep an up-to-date psw around for tracing */
      State.trace.psw = (State.trace.psw & mask) | val;
      break;
    case BPSW_CR:
    case DPSW_CR:
      /* Just like PSW, mask things like DM out. */
      if (psw_hw_p)
	val &= PSW_HW_MASK;
      else
	val &= PSW_MASK;
      break;
    case MOD_S_CR:
    case MOD_E_CR:
      val &= ~1;
      break;
    default:
      break;
    }
  /* only issue an update if the register is being changed */
  if ((State.cregs[cr] & ~mask) != val)
    SLOT_PEND_MASK (State.cregs[cr], mask, val);
  return val;
}

#ifdef DEBUG
static void trace_input_func PARAMS ((char *name,
				      enum op_types in1,
				      enum op_types in2,
				      enum op_types in3));

#define trace_input(name, in1, in2, in3) do { if (d10v_debug) trace_input_func (name, in1, in2, in3); } while (0)

#ifndef SIZE_INSTRUCTION
#define SIZE_INSTRUCTION 8
#endif

#ifndef SIZE_OPERANDS
#define SIZE_OPERANDS 18
#endif

#ifndef SIZE_VALUES
#define SIZE_VALUES 13
#endif

#ifndef SIZE_LOCATION
#define SIZE_LOCATION 20
#endif

#ifndef SIZE_PC
#define SIZE_PC 6
#endif

#ifndef SIZE_LINE_NUMBER
#define SIZE_LINE_NUMBER 4
#endif

static void
trace_input_func (name, in1, in2, in3)
     char *name;
     enum op_types in1;
     enum op_types in2;
     enum op_types in3;
{
  char *comma;
  enum op_types in[3];
  int i;
  char buf[1024];
  char *p;
  long tmp;
  char *type;
  const char *filename;
  const char *functionname;
  unsigned int linenumber;
  bfd_vma byte_pc;

  if ((d10v_debug & DEBUG_TRACE) == 0)
    return;

  switch (State.ins_type)
    {
    default:
    case INS_UNKNOWN:		type = " ?"; break;
    case INS_LEFT:		type = " L"; break;
    case INS_RIGHT:		type = " R"; break;
    case INS_LEFT_PARALLEL:	type = "*L"; break;
    case INS_RIGHT_PARALLEL:	type = "*R"; break;
    case INS_LEFT_COND_TEST:	type = "?L"; break;
    case INS_RIGHT_COND_TEST:	type = "?R"; break;
    case INS_LEFT_COND_EXE:	type = "&L"; break;
    case INS_RIGHT_COND_EXE:	type = "&R"; break;
    case INS_LONG:		type = " B"; break;
    }

  if ((d10v_debug & DEBUG_LINE_NUMBER) == 0)
    (*d10v_callback->printf_filtered) (d10v_callback,
				       "0x%.*x %s: %-*s ",
				       SIZE_PC, (unsigned)PC,
				       type,
				       SIZE_INSTRUCTION, name);

  else
    {
      buf[0] = '\0';
      byte_pc = decode_pc ();
      if (text && byte_pc >= text_start && byte_pc < text_end)
	{
	  filename = (const char *)0;
	  functionname = (const char *)0;
	  linenumber = 0;
	  if (bfd_find_nearest_line (prog_bfd, text, (struct bfd_symbol **)0, byte_pc - text_start,
				     &filename, &functionname, &linenumber))
	    {
	      p = buf;
	      if (linenumber)
		{
		  sprintf (p, "#%-*d ", SIZE_LINE_NUMBER, linenumber);
		  p += strlen (p);
		}
	      else
		{
		  sprintf (p, "%-*s ", SIZE_LINE_NUMBER+1, "---");
		  p += SIZE_LINE_NUMBER+2;
		}

	      if (functionname)
		{
		  sprintf (p, "%s ", functionname);
		  p += strlen (p);
		}
	      else if (filename)
		{
		  char *q = strrchr (filename, '/');
		  sprintf (p, "%s ", (q) ? q+1 : filename);
		  p += strlen (p);
		}

	      if (*p == ' ')
		*p = '\0';
	    }
	}

      (*d10v_callback->printf_filtered) (d10v_callback,
					 "0x%.*x %s: %-*.*s %-*s ",
					 SIZE_PC, (unsigned)PC,
					 type,
					 SIZE_LOCATION, SIZE_LOCATION, buf,
					 SIZE_INSTRUCTION, name);
    }

  in[0] = in1;
  in[1] = in2;
  in[2] = in3;
  comma = "";
  p = buf;
  for (i = 0; i < 3; i++)
    {
      switch (in[i])
	{
	case OP_VOID:
	case OP_R0:
	case OP_R1:
	case OP_R2:
	  break;

	case OP_REG:
	case OP_REG_OUTPUT:
	case OP_DREG:
	case OP_DREG_OUTPUT:
	  sprintf (p, "%sr%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_CR:
	case OP_CR_OUTPUT:
	case OP_CR_REVERSE:
	  sprintf (p, "%scr%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_ACCUM:
	case OP_ACCUM_OUTPUT:
	case OP_ACCUM_REVERSE:
	  sprintf (p, "%sa%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_CONSTANT16:
	  sprintf (p, "%s%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_CONSTANT8:
	  sprintf (p, "%s%d", comma, SEXT8(OP[i]));
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_CONSTANT4:
	  sprintf (p, "%s%d", comma, SEXT4(OP[i]));
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_CONSTANT3:
	  sprintf (p, "%s%d", comma, SEXT3(OP[i]));
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_MEMREF:
	  sprintf (p, "%s@r%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_MEMREF2:
	  sprintf (p, "%s@(%d,r%d)", comma, (int16)OP[i], OP[i+1]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_MEMREF3:
	  sprintf (p, "%s@%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_POSTINC:
	  sprintf (p, "%s@r%d+", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_POSTDEC:
	  sprintf (p, "%s@r%d-", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_PREDEC:
	  sprintf (p, "%s@-r%d", comma, OP[i]);
	  p += strlen (p);
	  comma = ",";
	  break;

	case OP_FLAG:
	case OP_FLAG_OUTPUT:
	  if (OP[i] == 0)
	    sprintf (p, "%sf0", comma);

	  else if (OP[i] == 1)
	    sprintf (p, "%sf1", comma);

	  else
	    sprintf (p, "%sc", comma);

	  p += strlen (p);
	  comma = ",";
	  break;
	}
    }

  if ((d10v_debug & DEBUG_VALUES) == 0)
    {
      *p++ = '\n';
      *p = '\0';
      (*d10v_callback->printf_filtered) (d10v_callback, "%s", buf);
    }
  else
    {
      *p = '\0';
      (*d10v_callback->printf_filtered) (d10v_callback, "%-*s", SIZE_OPERANDS, buf);

      p = buf;
      for (i = 0; i < 3; i++)
	{
	  buf[0] = '\0';
	  switch (in[i])
	    {
	    case OP_VOID:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s", SIZE_VALUES, "");
	      break;

	    case OP_REG_OUTPUT:
	    case OP_DREG_OUTPUT:
	    case OP_CR_OUTPUT:
	    case OP_ACCUM_OUTPUT:
	    case OP_FLAG_OUTPUT:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s", SIZE_VALUES, "---");
	      break;

	    case OP_REG:
	    case OP_MEMREF:
	    case OP_POSTDEC:
	    case OP_POSTINC:
	    case OP_PREDEC:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16) GPR (OP[i]));
	      break;

	    case OP_MEMREF3:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "", (uint16) OP[i]);
	      break;

	    case OP_DREG:
	      tmp = (long)((((uint32) GPR (OP[i])) << 16) | ((uint32) GPR (OP[i] + 1)));
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.8lx", SIZE_VALUES-10, "", tmp);
	      break;

	    case OP_CR:
	    case OP_CR_REVERSE:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16) CREG (OP[i]));
	      break;

	    case OP_ACCUM:
	    case OP_ACCUM_REVERSE:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.2x%.8lx", SIZE_VALUES-12, "",
						 ((int)(ACC (OP[i]) >> 32) & 0xff),
						 ((unsigned long) ACC (OP[i])) & 0xffffffff);
	      break;

	    case OP_CONSTANT16:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16)OP[i]);
	      break;

	    case OP_CONSTANT4:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16)SEXT4(OP[i]));
	      break;

	    case OP_CONSTANT8:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16)SEXT8(OP[i]));
	      break;

	    case OP_CONSTANT3:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16)SEXT3(OP[i]));
	      break;

	    case OP_FLAG:
	      if (OP[i] == 0)
		(*d10v_callback->printf_filtered) (d10v_callback, "%*sF0 = %d", SIZE_VALUES-6, "",
						   PSW_F0 != 0);

	      else if (OP[i] == 1)
		(*d10v_callback->printf_filtered) (d10v_callback, "%*sF1 = %d", SIZE_VALUES-6, "",
						   PSW_F1 != 0);

	      else
		(*d10v_callback->printf_filtered) (d10v_callback, "%*sC = %d", SIZE_VALUES-5, "",
						   PSW_C != 0);

	      break;

	    case OP_MEMREF2:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16)OP[i]);
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16)GPR (OP[i + 1]));
	      i++;
	      break;

	    case OP_R0:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16) GPR (0));
	      break;

	    case OP_R1:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16) GPR (1));
	      break;

	    case OP_R2:
	      (*d10v_callback->printf_filtered) (d10v_callback, "%*s0x%.4x", SIZE_VALUES-6, "",
						 (uint16) GPR (2));
	      break;

	    }
	}
    }

  (*d10v_callback->flush_stdout) (d10v_callback);
}

static void
do_trace_output_flush (void)
{
  (*d10v_callback->flush_stdout) (d10v_callback);
}

static void
do_trace_output_finish (void)
{
  (*d10v_callback->printf_filtered) (d10v_callback,
				     " F0=%d F1=%d C=%d\n",
				     (State.trace.psw & PSW_F0_BIT) != 0,
				     (State.trace.psw & PSW_F1_BIT) != 0,
				     (State.trace.psw & PSW_C_BIT) != 0);
  (*d10v_callback->flush_stdout) (d10v_callback);
}

static void
trace_output_40 (uint64 val)
{
  if ((d10v_debug & (DEBUG_TRACE | DEBUG_VALUES)) == (DEBUG_TRACE | DEBUG_VALUES))
    {
      (*d10v_callback->printf_filtered) (d10v_callback,
					 " :: %*s0x%.2x%.8lx",
					 SIZE_VALUES - 12,
					 "",
					 ((int)(val >> 32) & 0xff),
					 ((unsigned long) val) & 0xffffffff);
      do_trace_output_finish ();
    }
}

static void
trace_output_32 (uint32 val)
{
  if ((d10v_debug & (DEBUG_TRACE | DEBUG_VALUES)) == (DEBUG_TRACE | DEBUG_VALUES))
    {
      (*d10v_callback->printf_filtered) (d10v_callback,
					 " :: %*s0x%.8x",
					 SIZE_VALUES - 10,
					 "",
					 (int) val);
      do_trace_output_finish ();
    }
}

static void
trace_output_16 (uint16 val)
{
  if ((d10v_debug & (DEBUG_TRACE | DEBUG_VALUES)) == (DEBUG_TRACE | DEBUG_VALUES))
    {
      (*d10v_callback->printf_filtered) (d10v_callback,
					 " :: %*s0x%.4x",
					 SIZE_VALUES - 6,
					 "",
					 (int) val);
      do_trace_output_finish ();
    }
}

static void
trace_output_void ()
{
  if ((d10v_debug & (DEBUG_TRACE | DEBUG_VALUES)) == (DEBUG_TRACE | DEBUG_VALUES))
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "\n");
      do_trace_output_flush ();
    }
}

static void
trace_output_flag ()
{
  if ((d10v_debug & (DEBUG_TRACE | DEBUG_VALUES)) == (DEBUG_TRACE | DEBUG_VALUES))
    {
      (*d10v_callback->printf_filtered) (d10v_callback,
					 " :: %*s",
					 SIZE_VALUES,
					 "");
      do_trace_output_finish ();
    }
}




#else
#define trace_input(NAME, IN1, IN2, IN3)
#define trace_output(RESULT)
#endif

/* abs */
void
OP_4607 ()
{
  int16 tmp;
  trace_input ("abs", OP_REG, OP_VOID, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  tmp = GPR(OP[0]);
  if (tmp < 0)
    {
      tmp = - tmp;
      SET_PSW_F0 (1);
    }
  else
    SET_PSW_F0 (0);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* abs */
void
OP_5607 ()
{
  int64 tmp;
  trace_input ("abs", OP_ACCUM, OP_VOID, OP_VOID);
  SET_PSW_F1 (PSW_F0);

  tmp = SEXT40 (ACC (OP[0]));
  if (tmp < 0 )
    {
      tmp = - tmp;
      if (PSW_ST)
	{
	  if (tmp > SEXT40(MAX32))
	    tmp = (MAX32);
	  else if (tmp < SEXT40(MIN32))
	    tmp = (MIN32);
	  else
	    tmp = (tmp & MASK40);
	}
      else
	tmp = (tmp & MASK40);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = (tmp & MASK40);
      SET_PSW_F0 (0);
    }
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* add */
void
OP_200 ()
{
  uint16 a = GPR (OP[0]);
  uint16 b = GPR (OP[1]);
  uint16 tmp = (a + b);
  trace_input ("add", OP_REG, OP_REG, OP_VOID);
  SET_PSW_C (a > tmp);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* add */
void
OP_1201 ()
{
  int64 tmp;
  tmp = SEXT40(ACC (OP[0])) + (SEXT16 (GPR (OP[1])) << 16 | GPR (OP[1] + 1));

  trace_input ("add", OP_ACCUM, OP_REG, OP_VOID);
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* add */
void
OP_1203 ()
{
  int64 tmp;
  tmp = SEXT40(ACC (OP[0])) + SEXT40(ACC (OP[1]));

  trace_input ("add", OP_ACCUM, OP_ACCUM, OP_VOID);
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* add2w */
void
OP_1200 ()
{
  uint32 tmp;
  uint32 a = (GPR (OP[0])) << 16 | GPR (OP[0] + 1);
  uint32 b = (GPR (OP[1])) << 16 | GPR (OP[1] + 1);
  trace_input ("add2w", OP_DREG, OP_DREG, OP_VOID);
  tmp = a + b;
  SET_PSW_C (tmp < a);
  SET_GPR (OP[0] + 0, (tmp >> 16));
  SET_GPR (OP[0] + 1, (tmp & 0xFFFF));
  trace_output_32 (tmp);
}

/* add3 */
void
OP_1000000 ()
{
  uint16 a = GPR (OP[1]);
  uint16 b = OP[2];
  uint16 tmp = (a + b);
  trace_input ("add3", OP_REG_OUTPUT, OP_REG, OP_CONSTANT16);
  SET_PSW_C (tmp < a);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* addac3 */
void
OP_17000200 ()
{
  int64 tmp;
  tmp = SEXT40(ACC (OP[2])) + SEXT40 ((GPR (OP[1]) << 16) | GPR (OP[1] + 1));

  trace_input ("addac3", OP_DREG_OUTPUT, OP_DREG, OP_ACCUM);
  SET_GPR (OP[0] + 0, ((tmp >> 16) & 0xffff));
  SET_GPR (OP[0] + 1, (tmp & 0xffff));
  trace_output_32 (tmp);
}

/* addac3 */
void
OP_17000202 ()
{
  int64 tmp;
  tmp = SEXT40(ACC (OP[1])) + SEXT40(ACC (OP[2]));

  trace_input ("addac3", OP_DREG_OUTPUT, OP_ACCUM, OP_ACCUM);
  SET_GPR (OP[0] + 0, (tmp >> 16) & 0xffff);
  SET_GPR (OP[0] + 1, tmp & 0xffff);
  trace_output_32 (tmp);
}

/* addac3s */
void
OP_17001200 ()
{
  int64 tmp;
  SET_PSW_F1 (PSW_F0);

  trace_input ("addac3s", OP_DREG_OUTPUT, OP_DREG, OP_ACCUM);
  tmp = SEXT40 (ACC (OP[2])) + SEXT40 ((GPR (OP[1]) << 16) | GPR (OP[1] + 1));
  if (tmp > SEXT40(MAX32))
    {
      tmp = (MAX32);
      SET_PSW_F0 (1);
    }      
  else if (tmp < SEXT40(MIN32))
    {
      tmp = (MIN32);
      SET_PSW_F0 (1);
    }      
  else
    {
      SET_PSW_F0 (0);
    }      
  SET_GPR (OP[0] + 0, (tmp >> 16) & 0xffff);
  SET_GPR (OP[0] + 1, (tmp & 0xffff));
  trace_output_32 (tmp);
}

/* addac3s */
void
OP_17001202 ()
{
  int64 tmp;
  SET_PSW_F1 (PSW_F0);

  trace_input ("addac3s", OP_DREG_OUTPUT, OP_ACCUM, OP_ACCUM);
  tmp = SEXT40(ACC (OP[1])) + SEXT40(ACC (OP[2]));
  if (tmp > SEXT40(MAX32))
    {
      tmp = (MAX32);
      SET_PSW_F0 (1);
    }      
  else if (tmp < SEXT40(MIN32))
    {
      tmp = (MIN32);
      SET_PSW_F0 (1);
    }      
  else
    {
      SET_PSW_F0 (0);
    }      
  SET_GPR (OP[0] + 0, (tmp >> 16) & 0xffff);
  SET_GPR (OP[0] + 1, (tmp & 0xffff));
  trace_output_32 (tmp);
}

/* addi */
void
OP_201 ()
{
  uint16 a = GPR (OP[0]);
  uint16 b;
  uint16 tmp;
  if (OP[1] == 0)
    OP[1] = 16;
  b = OP[1];
  tmp = (a + b);
  trace_input ("addi", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_C (tmp < a);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* and */
void
OP_C00 ()
{
  uint16 tmp = GPR (OP[0]) & GPR (OP[1]);
  trace_input ("and", OP_REG, OP_REG, OP_VOID);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* and3 */
void
OP_6000000 ()
{
  uint16 tmp = GPR (OP[1]) & OP[2];
  trace_input ("and3", OP_REG_OUTPUT, OP_REG, OP_CONSTANT16);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* bclri */
void
OP_C01 ()
{
  int16 tmp;
  trace_input ("bclri", OP_REG, OP_CONSTANT16, OP_VOID);
  tmp = (GPR (OP[0]) &~(0x8000 >> OP[1]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* bl.s */
void
OP_4900 ()
{
  trace_input ("bl.s", OP_CONSTANT8, OP_R0, OP_R1);
  SET_GPR (13, PC + 1);
  JMP( PC + SEXT8 (OP[0]));
  trace_output_void ();
}

/* bl.l */
void
OP_24800000 ()
{
  trace_input ("bl.l", OP_CONSTANT16, OP_R0, OP_R1);
  SET_GPR (13, (PC + 1));
  JMP (PC + OP[0]);
  trace_output_void ();
}

/* bnoti */
void
OP_A01 ()
{
  int16 tmp;
  trace_input ("bnoti", OP_REG, OP_CONSTANT16, OP_VOID);
  tmp = (GPR (OP[0]) ^ (0x8000 >> OP[1]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* bra.s */
void
OP_4800 ()
{
  trace_input ("bra.s", OP_CONSTANT8, OP_VOID, OP_VOID);
  JMP (PC + SEXT8 (OP[0]));
  trace_output_void ();
}

/* bra.l */
void
OP_24000000 ()
{
  trace_input ("bra.l", OP_CONSTANT16, OP_VOID, OP_VOID);
  JMP (PC + OP[0]);
  trace_output_void ();
}

/* brf0f.s */
void
OP_4A00 ()
{
  trace_input ("brf0f.s", OP_CONSTANT8, OP_VOID, OP_VOID);
  if (!PSW_F0)
    JMP (PC + SEXT8 (OP[0]));
  trace_output_flag ();
}

/* brf0f.l */
void
OP_25000000 ()
{
  trace_input ("brf0f.l", OP_CONSTANT16, OP_VOID, OP_VOID);
  if (!PSW_F0)
    JMP (PC + OP[0]);
  trace_output_flag ();
}

/* brf0t.s */
void
OP_4B00 ()
{
  trace_input ("brf0t.s", OP_CONSTANT8, OP_VOID, OP_VOID);
  if (PSW_F0)
    JMP (PC + SEXT8 (OP[0]));
  trace_output_flag ();
}

/* brf0t.l */
void
OP_25800000 ()
{
  trace_input ("brf0t.l", OP_CONSTANT16, OP_VOID, OP_VOID);
  if (PSW_F0)
    JMP (PC + OP[0]);
  trace_output_flag ();
}

/* bseti */
void
OP_801 ()
{
  int16 tmp;
  trace_input ("bseti", OP_REG, OP_CONSTANT16, OP_VOID);
  tmp = (GPR (OP[0]) | (0x8000 >> OP[1]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* btsti */
void
OP_E01 ()
{
  trace_input ("btsti", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((GPR (OP[0]) & (0x8000 >> OP[1])) ? 1 : 0);
  trace_output_flag ();
}

/* clrac */
void
OP_5601 ()
{
  trace_input ("clrac", OP_ACCUM_OUTPUT, OP_VOID, OP_VOID);
  SET_ACC (OP[0], 0);
  trace_output_40 (0);
}

/* cmp */
void
OP_600 ()
{
  trace_input ("cmp", OP_REG, OP_REG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 (((int16)(GPR (OP[0])) < (int16)(GPR (OP[1]))) ? 1 : 0);
  trace_output_flag ();
}

/* cmp */
void
OP_1603 ()
{
  trace_input ("cmp", OP_ACCUM, OP_ACCUM, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((SEXT40(ACC (OP[0])) < SEXT40(ACC (OP[1]))) ? 1 : 0);
  trace_output_flag ();
}

/* cmpeq */
void
OP_400 ()
{
  trace_input ("cmpeq", OP_REG, OP_REG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((GPR (OP[0]) == GPR (OP[1])) ? 1 : 0);
  trace_output_flag ();
}

/* cmpeq */
void
OP_1403 ()
{
  trace_input ("cmpeq", OP_ACCUM, OP_ACCUM, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 (((ACC (OP[0]) & MASK40) == (ACC (OP[1]) & MASK40)) ? 1 : 0);
  trace_output_flag ();
}

/* cmpeqi.s */
void
OP_401 ()
{
  trace_input ("cmpeqi.s", OP_REG, OP_CONSTANT4, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((GPR (OP[0]) == (reg_t) SEXT4 (OP[1])) ? 1 : 0);  
  trace_output_flag ();
}

/* cmpeqi.l */
void
OP_2000000 ()
{
  trace_input ("cmpeqi.l", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((GPR (OP[0]) == (reg_t)OP[1]) ? 1 : 0);  
  trace_output_flag ();
}

/* cmpi.s */
void
OP_601 ()
{
  trace_input ("cmpi.s", OP_REG, OP_CONSTANT4, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 (((int16)(GPR (OP[0])) < (int16)SEXT4(OP[1])) ? 1 : 0);  
  trace_output_flag ();
}

/* cmpi.l */
void
OP_3000000 ()
{
  trace_input ("cmpi.l", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 (((int16)(GPR (OP[0])) < (int16)(OP[1])) ? 1 : 0);  
  trace_output_flag ();
}

/* cmpu */
void
OP_4600 ()
{
  trace_input ("cmpu", OP_REG, OP_REG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((GPR (OP[0]) < GPR (OP[1])) ? 1 : 0);  
  trace_output_flag ();
}

/* cmpui */
void
OP_23000000 ()
{
  trace_input ("cmpui", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((GPR (OP[0]) < (reg_t)OP[1]) ? 1 : 0);  
  trace_output_flag ();
}

/* cpfg */
void
OP_4E09 ()
{
  uint8 val;
  
  trace_input ("cpfg", OP_FLAG_OUTPUT, OP_FLAG, OP_VOID);
  
  if (OP[1] == 0)
    val = PSW_F0;
  else if (OP[1] == 1)
    val = PSW_F1;
  else
    val = PSW_C;
  if (OP[0] == 0)
    SET_PSW_F0 (val);
  else
    SET_PSW_F1 (val);

  trace_output_flag ();
}

/* cpfg */
void
OP_4E0F ()
{
  uint8 val;
  
  trace_input ("cpfg", OP_FLAG_OUTPUT, OP_FLAG, OP_VOID);
  
  if (OP[1] == 0)
    val = PSW_F0;
  else if (OP[1] == 1)
    val = PSW_F1;
  else
    val = PSW_C;
  if (OP[0] == 0)
    SET_PSW_F0 (val);
  else
    SET_PSW_F1 (val);

  trace_output_flag ();
}

/* dbt */
void
OP_5F20 ()
{
  /* d10v_callback->printf_filtered(d10v_callback, "***** DBT *****  PC=%x\n",PC); */

  /* GDB uses the instruction pair ``dbt || nop'' as a break-point.
     The conditional below is for either of the instruction pairs
     ``dbt -> XXX'' or ``dbt <- XXX'' and treats them as as cases
     where the dbt instruction should be interpreted.

     The module `sim-break' provides a more effective mechanism for
     detecting GDB planted breakpoints.  The code below may,
     eventually, be changed to use that mechanism. */

  if (State.ins_type == INS_LEFT
      || State.ins_type == INS_RIGHT)
    {
      trace_input ("dbt", OP_VOID, OP_VOID, OP_VOID);
      SET_DPC (PC + 1);
      SET_DPSW (PSW);
      SET_HW_PSW (PSW_DM_BIT | (PSW & (PSW_F0_BIT | PSW_F1_BIT | PSW_C_BIT)));
      JMP (DBT_VECTOR_START);
      trace_output_void ();
    }
  else
    {
      State.exception = SIGTRAP;
    }
}

/* divs */
void
OP_14002800 ()
{
  uint16 foo, tmp, tmpf;
  uint16 hi;
  uint16 lo;

  trace_input ("divs", OP_DREG, OP_REG, OP_VOID);
  foo = (GPR (OP[0]) << 1) | (GPR (OP[0] + 1) >> 15);
  tmp = (int16)foo - (int16)(GPR (OP[1]));
  tmpf = (foo >= GPR (OP[1])) ? 1 : 0;
  hi = ((tmpf == 1) ? tmp : foo);
  lo = ((GPR (OP[0] + 1) << 1) | tmpf);
  SET_GPR (OP[0] + 0, hi);
  SET_GPR (OP[0] + 1, lo);
  trace_output_32 (((uint32) hi << 16) | lo);
}

/* exef0f */
void
OP_4E04 ()
{
  trace_input ("exef0f", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F0 == 0);
  trace_output_flag ();
}

/* exef0t */
void
OP_4E24 ()
{
  trace_input ("exef0t", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F0 != 0);
  trace_output_flag ();
}

/* exef1f */
void
OP_4E40 ()
{
  trace_input ("exef1f", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F1 == 0);
  trace_output_flag ();
}

/* exef1t */
void
OP_4E42 ()
{
  trace_input ("exef1t", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F1 != 0);
  trace_output_flag ();
}

/* exefaf */
void
OP_4E00 ()
{
  trace_input ("exefaf", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F0 == 0) & (PSW_F1 == 0);
  trace_output_flag ();
}

/* exefat */
void
OP_4E02 ()
{
  trace_input ("exefat", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F0 == 0) & (PSW_F1 != 0);
  trace_output_flag ();
}

/* exetaf */
void
OP_4E20 ()
{
  trace_input ("exetaf", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F0 != 0) & (PSW_F1 == 0);
  trace_output_flag ();
}

/* exetat */
void
OP_4E22 ()
{
  trace_input ("exetat", OP_VOID, OP_VOID, OP_VOID);
  State.exe = (PSW_F0 != 0) & (PSW_F1 != 0);
  trace_output_flag ();
}

/* exp */
void
OP_15002A00 ()
{
  uint32 tmp, foo;
  int i;

  trace_input ("exp", OP_REG_OUTPUT, OP_DREG, OP_VOID);
  if (((int16)GPR (OP[1])) >= 0)
    tmp = (GPR (OP[1]) << 16) | GPR (OP[1] + 1);
  else
    tmp = ~((GPR (OP[1]) << 16) | GPR (OP[1] + 1));
  
  foo = 0x40000000;
  for (i=1;i<17;i++)
    {
      if (tmp & foo)
	{
	  SET_GPR (OP[0], (i - 1));
	  trace_output_16 (i - 1);
	  return;
	}
      foo >>= 1;
    }
  SET_GPR (OP[0], 16);
  trace_output_16 (16);
}

/* exp */
void
OP_15002A02 ()
{
  int64 tmp, foo;
  int i;

  trace_input ("exp", OP_REG_OUTPUT, OP_ACCUM, OP_VOID);
  tmp = SEXT40(ACC (OP[1]));
  if (tmp < 0)
    tmp = ~tmp & MASK40;
  
  foo = 0x4000000000LL;
  for (i=1;i<25;i++)
    {
      if (tmp & foo)
	{
	  SET_GPR (OP[0], i - 9);
	  trace_output_16 (i - 9);
	  return;
	}
      foo >>= 1;
    }
  SET_GPR (OP[0], 16);
  trace_output_16 (16);
}

/* jl */
void
OP_4D00 ()
{
  trace_input ("jl", OP_REG, OP_R0, OP_R1);
  SET_GPR (13, PC + 1);
  JMP (GPR (OP[0]));
  trace_output_void ();
}

/* jmp */
void
OP_4C00 ()
{
  trace_input ("jmp", OP_REG,
	       (OP[0] == 13) ? OP_R0 : OP_VOID,
	       (OP[0] == 13) ? OP_R1 : OP_VOID);

  JMP (GPR (OP[0]));
  trace_output_void ();
}

/* ld */
void
OP_30000000 ()
{
  uint16 tmp;
  uint16 addr = OP[1] + GPR (OP[2]);
  trace_input ("ld", OP_REG_OUTPUT, OP_MEMREF2, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RW (addr);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ld */
void
OP_6401 ()
{
  uint16 tmp;
  uint16 addr = GPR (OP[1]);
  trace_input ("ld", OP_REG_OUTPUT, OP_POSTDEC, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RW (addr);
  SET_GPR (OP[0], tmp);
  if (OP[0] != OP[1])
    INC_ADDR (OP[1], -2);
  trace_output_16 (tmp);
}

/* ld */
void
OP_6001 ()
{
  uint16 tmp;
  uint16 addr = GPR (OP[1]);
  trace_input ("ld", OP_REG_OUTPUT, OP_POSTINC, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RW (addr);
  SET_GPR (OP[0], tmp);
  if (OP[0] != OP[1])
    INC_ADDR (OP[1], 2);
  trace_output_16 (tmp);
}

/* ld */
void
OP_6000 ()
{
  uint16 tmp;
  uint16 addr = GPR (OP[1]);
  trace_input ("ld", OP_REG_OUTPUT, OP_MEMREF, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RW (addr);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ld */
void
OP_32010000 ()
{
  uint16 tmp;
  uint16 addr = OP[1];
  trace_input ("ld", OP_REG_OUTPUT, OP_MEMREF3, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RW (addr);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ld2w */
void
OP_31000000 ()
{
  int32 tmp;
  uint16 addr = OP[1] + GPR (OP[2]);
  trace_input ("ld2w", OP_REG_OUTPUT, OP_MEMREF2, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RLW (addr);
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* ld2w */
void
OP_6601 ()
{
  uint16 addr = GPR (OP[1]);
  int32 tmp;
  trace_input ("ld2w", OP_REG_OUTPUT, OP_POSTDEC, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RLW (addr);
  SET_GPR32 (OP[0], tmp);
  if (OP[0] != OP[1] && ((OP[0] + 1) != OP[1]))
    INC_ADDR (OP[1], -4);
  trace_output_32 (tmp);
}

/* ld2w */
void
OP_6201 ()
{
  int32 tmp;
  uint16 addr = GPR (OP[1]);
  trace_input ("ld2w", OP_REG_OUTPUT, OP_POSTINC, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RLW (addr);
  SET_GPR32 (OP[0], tmp);
  if (OP[0] != OP[1] && ((OP[0] + 1) != OP[1]))
    INC_ADDR (OP[1], 4);
  trace_output_32 (tmp);
}

/* ld2w */
void
OP_6200 ()
{
  uint16 addr = GPR (OP[1]);
  int32 tmp;
  trace_input ("ld2w", OP_REG_OUTPUT, OP_MEMREF, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RLW (addr);
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* ld2w */
void
OP_33010000 ()
{
  int32 tmp;
  uint16 addr = OP[1];
  trace_input ("ld2w", OP_REG_OUTPUT, OP_MEMREF3, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  tmp = RLW (addr);
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* ldb */
void
OP_38000000 ()
{
  int16 tmp;
  trace_input ("ldb", OP_REG_OUTPUT, OP_MEMREF2, OP_VOID);
  tmp = SEXT8 (RB (OP[1] + GPR (OP[2])));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ldb */
void
OP_7000 ()
{
  int16 tmp;
  trace_input ("ldb", OP_REG_OUTPUT, OP_MEMREF, OP_VOID);
  tmp = SEXT8 (RB (GPR (OP[1])));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ldi.s */
void
OP_4001 ()
{
  int16 tmp;
  trace_input ("ldi.s", OP_REG_OUTPUT, OP_CONSTANT4, OP_VOID);
  tmp = SEXT4 (OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ldi.l */
void
OP_20000000 ()
{
  int16 tmp;
  trace_input ("ldi.l", OP_REG_OUTPUT, OP_CONSTANT16, OP_VOID);
  tmp = OP[1];
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ldub */
void
OP_39000000 ()
{
  int16 tmp;
  trace_input ("ldub", OP_REG_OUTPUT, OP_MEMREF2, OP_VOID);
  tmp = RB (OP[1] + GPR (OP[2]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* ldub */
void
OP_7200 ()
{
  int16 tmp;
  trace_input ("ldub", OP_REG_OUTPUT, OP_MEMREF, OP_VOID);
  tmp = RB (GPR (OP[1]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mac */
void
OP_2A00 ()
{
  int64 tmp;

  trace_input ("mac", OP_ACCUM, OP_REG, OP_REG);
  tmp = SEXT40 ((int16)(GPR (OP[1])) * (int16)(GPR (OP[2])));

  if (PSW_FX)
    tmp = SEXT40( (tmp << 1) & MASK40);

  if (PSW_ST && tmp > SEXT40(MAX32))
    tmp = (MAX32);

  tmp += SEXT40 (ACC (OP[0]));
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* macsu */
void
OP_1A00 ()
{
  int64 tmp;

  trace_input ("macsu", OP_ACCUM, OP_REG, OP_REG);
  tmp = SEXT40 ((int16) GPR (OP[1]) * GPR (OP[2]));
  if (PSW_FX)
    tmp = SEXT40 ((tmp << 1) & MASK40);
  tmp = ((SEXT40 (ACC (OP[0])) + tmp) & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* macu */
void
OP_3A00 ()
{
  uint64 tmp;
  uint32 src1;
  uint32 src2;

  trace_input ("macu", OP_ACCUM, OP_REG, OP_REG);
  src1 = (uint16) GPR (OP[1]);
  src2 = (uint16) GPR (OP[2]);
  tmp = src1 * src2;
  if (PSW_FX)
    tmp = (tmp << 1);
  tmp = ((ACC (OP[0]) + tmp) & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* max */
void
OP_2600 ()
{
  int16 tmp;
  trace_input ("max", OP_REG, OP_REG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  if ((int16) GPR (OP[1]) > (int16)GPR (OP[0]))
    {
      tmp = GPR (OP[1]);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = GPR (OP[0]);
      SET_PSW_F0 (0);    
    }
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* max */
void
OP_3600 ()
{
  int64 tmp;

  trace_input ("max", OP_ACCUM, OP_DREG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  tmp = SEXT16 (GPR (OP[1])) << 16 | GPR (OP[1] + 1);
  if (tmp > SEXT40 (ACC (OP[0])))
    {
      tmp = (tmp & MASK40);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = ACC (OP[0]);
      SET_PSW_F0 (0);
    }
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* max */
void
OP_3602 ()
{
  int64 tmp;
  trace_input ("max", OP_ACCUM, OP_ACCUM, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  if (SEXT40 (ACC (OP[1])) > SEXT40 (ACC (OP[0])))
    {
      tmp = ACC (OP[1]);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = ACC (OP[0]);
      SET_PSW_F0 (0);
    }
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}


/* min */
void
OP_2601 ()
{
  int16 tmp;
  trace_input ("min", OP_REG, OP_REG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  if ((int16)GPR (OP[1]) < (int16)GPR (OP[0]))
    {
      tmp = GPR (OP[1]);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = GPR (OP[0]);
      SET_PSW_F0 (0);    
    }
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* min */
void
OP_3601 ()
{
  int64 tmp;

  trace_input ("min", OP_ACCUM, OP_DREG, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  tmp = SEXT16 (GPR (OP[1])) << 16 | GPR (OP[1] + 1);
  if (tmp < SEXT40(ACC (OP[0])))
    {
      tmp = (tmp & MASK40);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = ACC (OP[0]);
      SET_PSW_F0 (0);
    }
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* min */
void
OP_3603 ()
{
  int64 tmp;
  trace_input ("min", OP_ACCUM, OP_ACCUM, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  if (SEXT40(ACC (OP[1])) < SEXT40(ACC (OP[0])))
    {
      tmp = ACC (OP[1]);
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = ACC (OP[0]);
      SET_PSW_F0 (0);
    }
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* msb */
void
OP_2800 ()
{
  int64 tmp;

  trace_input ("msb", OP_ACCUM, OP_REG, OP_REG);
  tmp = SEXT40 ((int16)(GPR (OP[1])) * (int16)(GPR (OP[2])));

  if (PSW_FX)
    tmp = SEXT40 ((tmp << 1) & MASK40);

  if (PSW_ST && tmp > SEXT40(MAX32))
    tmp = (MAX32);

  tmp = SEXT40(ACC (OP[0])) - tmp;
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    {
      tmp = (tmp & MASK40);
    }
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* msbsu */
void
OP_1800 ()
{
  int64 tmp;

  trace_input ("msbsu", OP_ACCUM, OP_REG, OP_REG);
  tmp = SEXT40 ((int16)GPR (OP[1]) * GPR (OP[2]));
  if (PSW_FX)
    tmp = SEXT40( (tmp << 1) & MASK40);
  tmp = ((SEXT40 (ACC (OP[0])) - tmp) & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* msbu */
void
OP_3800 ()
{
  uint64 tmp;
  uint32 src1;
  uint32 src2;

  trace_input ("msbu", OP_ACCUM, OP_REG, OP_REG);
  src1 = (uint16) GPR (OP[1]);
  src2 = (uint16) GPR (OP[2]);
  tmp = src1 * src2;
  if (PSW_FX)
    tmp = (tmp << 1);
  tmp = ((ACC (OP[0]) - tmp) & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* mul */
void
OP_2E00 ()
{
  int16 tmp;
  trace_input ("mul", OP_REG, OP_REG, OP_VOID);
  tmp = GPR (OP[0]) * GPR (OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mulx */
void
OP_2C00 ()
{
  int64 tmp;

  trace_input ("mulx", OP_ACCUM_OUTPUT, OP_REG, OP_REG);
  tmp = SEXT40 ((int16)(GPR (OP[1])) * (int16)(GPR (OP[2])));

  if (PSW_FX)
    tmp = SEXT40 ((tmp << 1) & MASK40);

  if (PSW_ST && tmp > SEXT40(MAX32))
    tmp = (MAX32);
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* mulxsu */
void
OP_1C00 ()
{
  int64 tmp;

  trace_input ("mulxsu", OP_ACCUM_OUTPUT, OP_REG, OP_REG);
  tmp = SEXT40 ((int16)(GPR (OP[1])) * GPR (OP[2]));

  if (PSW_FX)
    tmp <<= 1;
  tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* mulxu */
void
OP_3C00 ()
{
  uint64 tmp;
  uint32 src1;
  uint32 src2;

  trace_input ("mulxu", OP_ACCUM_OUTPUT, OP_REG, OP_REG);
  src1 = (uint16) GPR (OP[1]);
  src2 = (uint16) GPR (OP[2]);
  tmp = src1 * src2;
  if (PSW_FX)
    tmp <<= 1;
  tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* mv */
void
OP_4000 ()
{
  int16 tmp;
  trace_input ("mv", OP_REG_OUTPUT, OP_REG, OP_VOID);
  tmp = GPR (OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mv2w */
void
OP_5000 ()
{
  int32 tmp;
  trace_input ("mv2w", OP_DREG_OUTPUT, OP_DREG, OP_VOID);
  tmp = GPR32 (OP[1]);
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* mv2wfac */
void
OP_3E00 ()
{
  int32 tmp;
  trace_input ("mv2wfac", OP_DREG_OUTPUT, OP_ACCUM, OP_VOID);
  tmp = ACC (OP[1]);
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* mv2wtac */
void
OP_3E01 ()
{
  int64 tmp;
  trace_input ("mv2wtac", OP_DREG, OP_ACCUM_OUTPUT, OP_VOID);
  tmp = ((SEXT16 (GPR (OP[0])) << 16 | GPR (OP[0] + 1)) & MASK40);
  SET_ACC (OP[1], tmp);
  trace_output_40 (tmp);
}

/* mvac */
void
OP_3E03 ()
{
  int64 tmp;
  trace_input ("mvac", OP_ACCUM_OUTPUT, OP_ACCUM, OP_VOID);
  tmp = ACC (OP[1]);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* mvb */
void
OP_5400 ()
{
  int16 tmp;
  trace_input ("mvb", OP_REG_OUTPUT, OP_REG, OP_VOID);
  tmp = SEXT8 (GPR (OP[1]) & 0xff);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mvf0f */
void
OP_4400 ()
{
  int16 tmp;
  trace_input ("mvf0f", OP_REG_OUTPUT, OP_REG, OP_VOID);
  if (PSW_F0 == 0)
    {
      tmp = GPR (OP[1]);
      SET_GPR (OP[0], tmp);
    }
  else
    tmp = GPR (OP[0]);
  trace_output_16 (tmp);
}

/* mvf0t */
void
OP_4401 ()
{
  int16 tmp;
  trace_input ("mvf0t", OP_REG_OUTPUT, OP_REG, OP_VOID);
  if (PSW_F0)
    {
      tmp = GPR (OP[1]);
      SET_GPR (OP[0], tmp);
    }
  else
    tmp = GPR (OP[0]);
  trace_output_16 (tmp);
}

/* mvfacg */
void
OP_1E04 ()
{
  int16 tmp;
  trace_input ("mvfacg", OP_REG_OUTPUT, OP_ACCUM, OP_VOID);
  tmp = ((ACC (OP[1]) >> 32) & 0xff);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mvfachi */
void
OP_1E00 ()
{
  int16 tmp;
  trace_input ("mvfachi", OP_REG_OUTPUT, OP_ACCUM, OP_VOID);
  tmp = (ACC (OP[1]) >> 16);  
  SET_GPR (OP[0], tmp);  
  trace_output_16 (tmp);
}

/* mvfaclo */
void
OP_1E02 ()
{
  int16 tmp;
  trace_input ("mvfaclo", OP_REG_OUTPUT, OP_ACCUM, OP_VOID);
  tmp = ACC (OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mvfc */
void
OP_5200 ()
{
  int16 tmp;
  trace_input ("mvfc", OP_REG_OUTPUT, OP_CR, OP_VOID);
  tmp = CREG (OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* mvtacg */
void
OP_1E41 ()
{
  int64 tmp;
  trace_input ("mvtacg", OP_REG, OP_ACCUM, OP_VOID);
  tmp = ((ACC (OP[1]) & MASK32)
	 | ((int64)(GPR (OP[0]) & 0xff) << 32));
  SET_ACC (OP[1], tmp);
  trace_output_40 (tmp);
}

/* mvtachi */
void
OP_1E01 ()
{
  uint64 tmp;
  trace_input ("mvtachi", OP_REG, OP_ACCUM, OP_VOID);
  tmp = ACC (OP[1]) & 0xffff;
  tmp = ((SEXT16 (GPR (OP[0])) << 16 | tmp) & MASK40);
  SET_ACC (OP[1], tmp);
  trace_output_40 (tmp);
}

/* mvtaclo */
void
OP_1E21 ()
{
  int64 tmp;
  trace_input ("mvtaclo", OP_REG, OP_ACCUM, OP_VOID);
  tmp = ((SEXT16 (GPR (OP[0]))) & MASK40);
  SET_ACC (OP[1], tmp);
  trace_output_40 (tmp);
}

/* mvtc */
void
OP_5600 ()
{
  int16 tmp;
  trace_input ("mvtc", OP_REG, OP_CR_OUTPUT, OP_VOID);
  tmp = GPR (OP[0]);
  tmp = SET_CREG (OP[1], tmp);
  trace_output_16 (tmp);
}

/* mvub */
void
OP_5401 ()
{
  int16 tmp;
  trace_input ("mvub", OP_REG_OUTPUT, OP_REG, OP_VOID);
  tmp = (GPR (OP[1]) & 0xff);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* neg */
void
OP_4605 ()
{
  int16 tmp;
  trace_input ("neg", OP_REG, OP_VOID, OP_VOID);
  tmp = - GPR (OP[0]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* neg */
void
OP_5605 ()
{
  int64 tmp;

  trace_input ("neg", OP_ACCUM, OP_VOID, OP_VOID);
  tmp = -SEXT40(ACC (OP[0]));
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}


/* nop */
void
OP_5E00 ()
{
  trace_input ("nop", OP_VOID, OP_VOID, OP_VOID);

  ins_type_counters[ (int)State.ins_type ]--;	/* don't count nops as normal instructions */
  switch (State.ins_type)
    {
    default:
      ins_type_counters[ (int)INS_UNKNOWN ]++;
      break;

    case INS_LEFT_PARALLEL:
      /* Don't count a parallel op that includes a NOP as a true parallel op */
      ins_type_counters[ (int)INS_RIGHT_PARALLEL ]--;
      ins_type_counters[ (int)INS_RIGHT ]++;
      ins_type_counters[ (int)INS_LEFT_NOPS ]++;
      break;

    case INS_LEFT:
    case INS_LEFT_COND_EXE:
      ins_type_counters[ (int)INS_LEFT_NOPS ]++;
      break;

    case INS_RIGHT_PARALLEL:
      /* Don't count a parallel op that includes a NOP as a true parallel op */
      ins_type_counters[ (int)INS_LEFT_PARALLEL ]--;
      ins_type_counters[ (int)INS_LEFT ]++;
      ins_type_counters[ (int)INS_RIGHT_NOPS ]++;
      break;

    case INS_RIGHT:
    case INS_RIGHT_COND_EXE:
      ins_type_counters[ (int)INS_RIGHT_NOPS ]++;
      break;
    }

  trace_output_void ();
}

/* not */
void
OP_4603 ()
{
  int16 tmp;
  trace_input ("not", OP_REG, OP_VOID, OP_VOID);
  tmp = ~GPR (OP[0]);  
  SET_GPR (OP[0], tmp);  
  trace_output_16 (tmp);
}

/* or */
void
OP_800 ()
{
  int16 tmp;
  trace_input ("or", OP_REG, OP_REG, OP_VOID);
  tmp = (GPR (OP[0]) | GPR (OP[1]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* or3 */
void
OP_4000000 ()
{
  int16 tmp;
  trace_input ("or3", OP_REG_OUTPUT, OP_REG, OP_CONSTANT16);
  tmp = (GPR (OP[1]) | OP[2]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* rac */
void
OP_5201 ()
{
  int64 tmp;
  int shift = SEXT3 (OP[2]);

  trace_input ("rac", OP_DREG_OUTPUT, OP_ACCUM, OP_CONSTANT3);
  if (OP[1] != 0)
    {
      (*d10v_callback->printf_filtered) (d10v_callback,
					 "ERROR at PC 0x%x: instruction only valid for A0\n",
					 PC<<2);
      State.exception = SIGILL;
    }

  SET_PSW_F1 (PSW_F0);
  tmp = SEXT56 ((ACC (0) << 16) | (ACC (1) & 0xffff));
  if (shift >=0)
    tmp <<= shift;
  else
    tmp >>= -shift;
  tmp += 0x8000;
  tmp >>= 16; /* look at bits 0:43 */
  if (tmp > SEXT44 (SIGNED64 (0x0007fffffff)))
    {
      tmp = 0x7fffffff;
      SET_PSW_F0 (1);
    } 
  else if (tmp < SEXT44 (SIGNED64 (0xfff80000000)))
    {
      tmp = 0x80000000;
      SET_PSW_F0 (1);
    } 
  else
    {
      SET_PSW_F0 (0);
    }
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* rachi */
void
OP_4201 ()
{
  signed64 tmp;
  int shift = SEXT3 (OP[2]);

  trace_input ("rachi", OP_REG_OUTPUT, OP_ACCUM, OP_CONSTANT3);
  SET_PSW_F1 (PSW_F0);
  if (shift >=0)
    tmp = SEXT40 (ACC (OP[1])) << shift;
  else
    tmp = SEXT40 (ACC (OP[1])) >> -shift;
  tmp += 0x8000;

  if (tmp > SEXT44 (SIGNED64 (0x0007fffffff)))
    {
      tmp = 0x7fff;
      SET_PSW_F0 (1);
    }
  else if (tmp < SEXT44 (SIGNED64 (0xfff80000000)))
    {
      tmp = 0x8000;
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = (tmp >> 16);
      SET_PSW_F0 (0);
    }
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* rep */
void
OP_27000000 ()
{
  trace_input ("rep", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_RPT_S (PC + 1);
  SET_RPT_E (PC + OP[1]);
  SET_RPT_C (GPR (OP[0]));
  SET_PSW_RP (1);
  if (GPR (OP[0]) == 0)
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: rep with count=0 is illegal.\n");
      State.exception = SIGILL;
    }
  if (OP[1] < 4)
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: rep must include at least 4 instructions.\n");
      State.exception = SIGILL;
    }
  trace_output_void ();
}

/* repi */
void
OP_2F000000 ()
{
  trace_input ("repi", OP_CONSTANT16, OP_CONSTANT16, OP_VOID);
  SET_RPT_S (PC + 1);
  SET_RPT_E (PC + OP[1]);
  SET_RPT_C (OP[0]);
  SET_PSW_RP (1);
  if (OP[0] == 0)
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: repi with count=0 is illegal.\n");
      State.exception = SIGILL;
    }
  if (OP[1] < 4)
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: repi must include at least 4 instructions.\n");
      State.exception = SIGILL;
    }
  trace_output_void ();
}

/* rtd */
void
OP_5F60 ()
{
  trace_input ("rtd", OP_VOID, OP_VOID, OP_VOID);
  SET_CREG (PSW_CR, DPSW);
  JMP(DPC);
  trace_output_void ();
}

/* rte */
void
OP_5F40 ()
{
  trace_input ("rte", OP_VOID, OP_VOID, OP_VOID);
  SET_CREG (PSW_CR, BPSW);
  JMP(BPC);
  trace_output_void ();
}

/* sac */
void OP_5209 ()
{
  int64 tmp;

  trace_input ("sac", OP_REG_OUTPUT, OP_ACCUM, OP_VOID);

  tmp = SEXT40(ACC (OP[1]));

  SET_PSW_F1 (PSW_F0);

  if (tmp > SEXT40(MAX32))
    {
      tmp = (MAX32);
      SET_PSW_F0 (1);
    }
  else if (tmp < SEXT40(MIN32))
    {
      tmp = 0x80000000;
      SET_PSW_F0 (1);
    }
  else
    {
      tmp = (tmp & MASK32);
      SET_PSW_F0 (0);
    }

  SET_GPR32 (OP[0], tmp);

  trace_output_40 (tmp);
}

/* sachi */
void
OP_4209 ()
{
  int64 tmp;

  trace_input ("sachi", OP_REG_OUTPUT, OP_ACCUM, OP_VOID);

  tmp = SEXT40(ACC (OP[1]));

  SET_PSW_F1 (PSW_F0);

  if (tmp > SEXT40(MAX32))
    {
      tmp = 0x7fff;
      SET_PSW_F0 (1);
    }
  else if (tmp < SEXT40(MIN32))
    {
      tmp = 0x8000;
      SET_PSW_F0 (1);
    }
  else
    {
      tmp >>= 16;
      SET_PSW_F0 (0);
    }

  SET_GPR (OP[0], tmp);

  trace_output_16 (OP[0]);
}

/* sadd */
void
OP_1223 ()
{
  int64 tmp;

  trace_input ("sadd", OP_ACCUM, OP_ACCUM, OP_VOID);
  tmp = SEXT40(ACC (OP[0])) + (SEXT40(ACC (OP[1])) >> 16);
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* setf0f */
void
OP_4611 ()
{
  int16 tmp;
  trace_input ("setf0f", OP_REG_OUTPUT, OP_VOID, OP_VOID);
  tmp = ((PSW_F0 == 0) ? 1 : 0);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* setf0t */
void
OP_4613 ()
{
  int16 tmp;
  trace_input ("setf0t", OP_REG_OUTPUT, OP_VOID, OP_VOID);
  tmp = ((PSW_F0 == 1) ? 1 : 0);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* slae */
void
OP_3220 ()
{
  int64 tmp;
  int16 reg;

  trace_input ("slae", OP_ACCUM, OP_REG, OP_VOID);

  reg = SEXT16 (GPR (OP[1]));

  if (reg >= 17 || reg <= -17)
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: shift value %d too large.\n", reg);
      State.exception = SIGILL;
      return;
    }

  tmp = SEXT40 (ACC (OP[0]));

  if (PSW_ST && (tmp < SEXT40 (MIN32) || tmp > SEXT40 (MAX32)))
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: accumulator value 0x%.2x%.8lx out of range\n", ((int)(tmp >> 32) & 0xff), ((unsigned long) tmp) & 0xffffffff);
      State.exception = SIGILL;
      return;
    }

  if (reg >= 0 && reg <= 16)
    {
      tmp = SEXT56 ((SEXT56 (tmp)) << (GPR (OP[1])));
      if (PSW_ST)
	{
	  if (tmp > SEXT40(MAX32))
	    tmp = (MAX32);
	  else if (tmp < SEXT40(MIN32))
	    tmp = (MIN32);
	  else
	    tmp = (tmp & MASK40);
	}
      else
	tmp = (tmp & MASK40);
    }
  else
    {
      tmp = (SEXT40 (ACC (OP[0]))) >> (-GPR (OP[1]));
    }

  SET_ACC(OP[0], tmp);

  trace_output_40(tmp);
}

/* sleep */
void
OP_5FC0 ()
{
  trace_input ("sleep", OP_VOID, OP_VOID, OP_VOID);
  SET_PSW_IE (1);
  trace_output_void ();
}

/* sll */
void
OP_2200 ()
{
  int16 tmp;
  trace_input ("sll", OP_REG, OP_REG, OP_VOID);
  tmp = (GPR (OP[0]) << (GPR (OP[1]) & 0xf));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* sll */
void
OP_3200 ()
{
  int64 tmp;
  trace_input ("sll", OP_ACCUM, OP_REG, OP_VOID);
  if ((GPR (OP[1]) & 31) <= 16)
    tmp = SEXT40 (ACC (OP[0])) << (GPR (OP[1]) & 31);
  else
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: shift value %d too large.\n", GPR (OP[1]) & 31);
      State.exception = SIGILL;
      return;
    }

  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* slli */
void
OP_2201 ()
{
  int16 tmp;
  trace_input ("slli", OP_REG, OP_CONSTANT16, OP_VOID);
  tmp = (GPR (OP[0]) << OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* slli */
void
OP_3201 ()
{
  int64 tmp;

  if (OP[1] == 0)
    OP[1] = 16;

  trace_input ("slli", OP_ACCUM, OP_CONSTANT16, OP_VOID);
  tmp = SEXT40(ACC (OP[0])) << OP[1];

  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* slx */
void
OP_460B ()
{
  int16 tmp;
  trace_input ("slx", OP_REG, OP_VOID, OP_VOID);
  tmp = ((GPR (OP[0]) << 1) | PSW_F0);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* sra */
void
OP_2400 ()
{
  int16 tmp;
  trace_input ("sra", OP_REG, OP_REG, OP_VOID);
  tmp = (((int16)(GPR (OP[0]))) >> (GPR (OP[1]) & 0xf));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* sra */
void
OP_3400 ()
{
  trace_input ("sra", OP_ACCUM, OP_REG, OP_VOID);
  if ((GPR (OP[1]) & 31) <= 16)
    {
      int64 tmp = ((SEXT40(ACC (OP[0])) >> (GPR (OP[1]) & 31)) & MASK40);
      SET_ACC (OP[0], tmp);
      trace_output_40 (tmp);
    }
  else
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: shift value %d too large.\n", GPR (OP[1]) & 31);
      State.exception = SIGILL;
      return;
    }
}

/* srai */
void
OP_2401 ()
{
  int16 tmp;
  trace_input ("srai", OP_REG, OP_CONSTANT16, OP_VOID);
  tmp = (((int16)(GPR (OP[0]))) >> OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* srai */
void
OP_3401 ()
{
  int64 tmp;
  if (OP[1] == 0)
    OP[1] = 16;

  trace_input ("srai", OP_ACCUM, OP_CONSTANT16, OP_VOID);
  tmp = ((SEXT40(ACC (OP[0])) >> OP[1]) & MASK40);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* srl */
void
OP_2000 ()
{
  int16 tmp;
  trace_input ("srl", OP_REG, OP_REG, OP_VOID);
  tmp = (GPR (OP[0]) >>  (GPR (OP[1]) & 0xf));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* srl */
void
OP_3000 ()
{
  trace_input ("srl", OP_ACCUM, OP_REG, OP_VOID);
  if ((GPR (OP[1]) & 31) <= 16)
    {
      int64 tmp = ((uint64)((ACC (OP[0]) & MASK40) >> (GPR (OP[1]) & 31)));
      SET_ACC (OP[0], tmp);
      trace_output_40 (tmp);
    }
  else
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: shift value %d too large.\n", GPR (OP[1]) & 31);
      State.exception = SIGILL;
      return;
    }

}

/* srli */
void
OP_2001 ()
{
  int16 tmp;
  trace_input ("srli", OP_REG, OP_CONSTANT16, OP_VOID);
  tmp = (GPR (OP[0]) >> OP[1]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* srli */
void
OP_3001 ()
{
  int64 tmp;
  if (OP[1] == 0)
    OP[1] = 16;

  trace_input ("srli", OP_ACCUM, OP_CONSTANT16, OP_VOID);
  tmp = ((uint64)(ACC (OP[0]) & MASK40) >> OP[1]);
  SET_ACC (OP[0], tmp);
  trace_output_40 (tmp);
}

/* srx */
void
OP_4609 ()
{
  uint16 tmp;
  trace_input ("srx", OP_REG, OP_VOID, OP_VOID);
  tmp = PSW_F0 << 15;
  tmp = ((GPR (OP[0]) >> 1) | tmp);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* st */
void
OP_34000000 ()
{
  uint16 addr = OP[1] + GPR (OP[2]);
  trace_input ("st", OP_REG, OP_MEMREF2, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr, GPR (OP[0]));
  trace_output_void ();
}

/* st */
void
OP_6800 ()
{
  uint16 addr = GPR (OP[1]);
  trace_input ("st", OP_REG, OP_MEMREF, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr, GPR (OP[0]));
  trace_output_void ();
}

/* st */
/* st Rsrc1,@-SP */
void
OP_6C1F ()
{
  uint16 addr = GPR (OP[1]) - 2;
  trace_input ("st", OP_REG, OP_PREDEC, OP_VOID);
  if (OP[1] != 15)
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: cannot pre-decrement any registers but r15 (SP).\n");
      State.exception = SIGILL;
      return;
    }
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr, GPR (OP[0]));
  SET_GPR (OP[1], addr);
  trace_output_void ();
}

/* st */
void
OP_6801 ()
{
  uint16 addr = GPR (OP[1]);
  trace_input ("st", OP_REG, OP_POSTINC, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr, GPR (OP[0]));
  INC_ADDR (OP[1], 2);
  trace_output_void ();
}

/* st */
void
OP_6C01 ()
{
  uint16 addr = GPR (OP[1]);
  trace_input ("st", OP_REG, OP_POSTDEC, OP_VOID);
  if ( OP[1] == 15 )
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: cannot post-decrement register r15 (SP).\n");
      State.exception = SIGILL;
      return;
    }
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr, GPR (OP[0]));
  INC_ADDR (OP[1], -2);
  trace_output_void ();
}

/* st */
void
OP_36010000 ()
{
  uint16 addr = OP[1];
  trace_input ("st", OP_REG, OP_MEMREF3, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr, GPR (OP[0]));
  trace_output_void ();
}

/* st2w */
void
OP_35000000 ()
{
  uint16 addr = GPR (OP[2])+ OP[1];
  trace_input ("st2w", OP_DREG, OP_MEMREF2, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr + 0, GPR (OP[0] + 0));
  SW (addr + 2, GPR (OP[0] + 1));
  trace_output_void ();
}

/* st2w */
void
OP_6A00 ()
{
  uint16 addr = GPR (OP[1]);
  trace_input ("st2w", OP_DREG, OP_MEMREF, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr + 0, GPR (OP[0] + 0));
  SW (addr + 2, GPR (OP[0] + 1));
  trace_output_void ();
}

/* st2w */
void
OP_6E1F ()
{
  uint16 addr = GPR (OP[1]) - 4;
  trace_input ("st2w", OP_DREG, OP_PREDEC, OP_VOID);
  if ( OP[1] != 15 )
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: cannot pre-decrement any registers but r15 (SP).\n");
      State.exception = SIGILL;
      return;
    }
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr + 0, GPR (OP[0] + 0));
  SW (addr + 2, GPR (OP[0] + 1));
  SET_GPR (OP[1], addr);
  trace_output_void ();
}

/* st2w */
void
OP_6A01 ()
{
  uint16 addr = GPR (OP[1]);
  trace_input ("st2w", OP_DREG, OP_POSTINC, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr + 0, GPR (OP[0] + 0));
  SW (addr + 2, GPR (OP[0] + 1));
  INC_ADDR (OP[1], 4);
  trace_output_void ();
}

/* st2w */
void
OP_6E01 ()
{
  uint16 addr = GPR (OP[1]);
  trace_input ("st2w", OP_DREG, OP_POSTDEC, OP_VOID);
  if ( OP[1] == 15 )
    {
      (*d10v_callback->printf_filtered) (d10v_callback, "ERROR: cannot post-decrement register r15 (SP).\n");
      State.exception = SIGILL;
      return;
    }
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr + 0, GPR (OP[0] + 0));
  SW (addr + 2, GPR (OP[0] + 1));
  INC_ADDR (OP[1], -4);
  trace_output_void ();
}

/* st2w */
void
OP_37010000 ()
{
  uint16 addr = OP[1];
  trace_input ("st2w", OP_DREG, OP_MEMREF3, OP_VOID);
  if ((addr & 1))
    {
      State.exception = SIG_D10V_BUS;
      State.pc_changed = 1; /* Don't increment the PC. */
      trace_output_void ();
      return;
    }
  SW (addr + 0, GPR (OP[0] + 0));
  SW (addr + 2, GPR (OP[0] + 1));
  trace_output_void ();
}

/* stb */
void
OP_3C000000 ()
{
  trace_input ("stb", OP_REG, OP_MEMREF2, OP_VOID);
  SB (GPR (OP[2]) + OP[1], GPR (OP[0]));
  trace_output_void ();
}

/* stb */
void
OP_7800 ()
{
  trace_input ("stb", OP_REG, OP_MEMREF, OP_VOID);
  SB (GPR (OP[1]), GPR (OP[0]));
  trace_output_void ();
}

/* stop */
void
OP_5FE0 ()
{
  trace_input ("stop", OP_VOID, OP_VOID, OP_VOID);
  State.exception = SIG_D10V_STOP;
  trace_output_void ();
}

/* sub */
void
OP_0 ()
{
  uint16 a = GPR (OP[0]);
  uint16 b = GPR (OP[1]);
  uint16 tmp = (a - b);
  trace_input ("sub", OP_REG, OP_REG, OP_VOID);
  /* see ../common/sim-alu.h for a more extensive discussion on how to
     compute the carry/overflow bits. */
  SET_PSW_C (a >= b);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* sub */
void
OP_1001 ()
{
  int64 tmp;

  trace_input ("sub", OP_ACCUM, OP_DREG, OP_VOID);
  tmp = SEXT40(ACC (OP[0])) - (SEXT16 (GPR (OP[1])) << 16 | GPR (OP[1] + 1));
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);

  trace_output_40 (tmp);
}

/* sub */

void
OP_1003 ()
{
  int64 tmp;

  trace_input ("sub", OP_ACCUM, OP_ACCUM, OP_VOID);
  tmp = SEXT40(ACC (OP[0])) - SEXT40(ACC (OP[1]));
  if (PSW_ST)
    {
      if (tmp > SEXT40(MAX32))
	tmp = (MAX32);
      else if (tmp < SEXT40(MIN32))
	tmp = (MIN32);
      else
	tmp = (tmp & MASK40);
    }
  else
    tmp = (tmp & MASK40);
  SET_ACC (OP[0], tmp);

  trace_output_40 (tmp);
}

/* sub2w */
void
OP_1000 ()
{
  uint32 tmp, a, b;

  trace_input ("sub2w", OP_DREG, OP_DREG, OP_VOID);
  a = (uint32)((GPR (OP[0]) << 16) | GPR (OP[0] + 1));
  b = (uint32)((GPR (OP[1]) << 16) | GPR (OP[1] + 1));
  /* see ../common/sim-alu.h for a more extensive discussion on how to
     compute the carry/overflow bits */
  tmp = a - b;
  SET_PSW_C (a >= b);
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* subac3 */
void
OP_17000000 ()
{
  int64 tmp;

  trace_input ("subac3", OP_DREG_OUTPUT, OP_DREG, OP_ACCUM);
  tmp = SEXT40 ((GPR (OP[1]) << 16) | GPR (OP[1] + 1)) - SEXT40 (ACC (OP[2]));
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* subac3 */
void
OP_17000002 ()
{
  int64 tmp;

  trace_input ("subac3", OP_DREG_OUTPUT, OP_ACCUM, OP_ACCUM);
  tmp = SEXT40 (ACC (OP[1])) - SEXT40(ACC (OP[2]));
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* subac3s */
void
OP_17001000 ()
{
  int64 tmp;

  trace_input ("subac3s", OP_DREG_OUTPUT, OP_DREG, OP_ACCUM);
  SET_PSW_F1 (PSW_F0);
  tmp = SEXT40 ((GPR (OP[1]) << 16) | GPR (OP[1] + 1)) - SEXT40(ACC (OP[2]));
  if (tmp > SEXT40(MAX32))
    {
      tmp = (MAX32);
      SET_PSW_F0 (1);
    }      
  else if (tmp < SEXT40(MIN32))
    {
      tmp = (MIN32);
      SET_PSW_F0 (1);
    }      
  else
    {
      SET_PSW_F0 (0);
    }      
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* subac3s */
void
OP_17001002 ()
{
  int64 tmp;

  trace_input ("subac3s", OP_DREG_OUTPUT, OP_ACCUM, OP_ACCUM);
  SET_PSW_F1 (PSW_F0);
  tmp = SEXT40(ACC (OP[1])) - SEXT40(ACC (OP[2]));
  if (tmp > SEXT40(MAX32))
    {
      tmp = (MAX32);
      SET_PSW_F0 (1);
    }      
  else if (tmp < SEXT40(MIN32))
    {
      tmp = (MIN32);
      SET_PSW_F0 (1);
    }      
  else
    {
      SET_PSW_F0 (0);
    }      
  SET_GPR32 (OP[0], tmp);
  trace_output_32 (tmp);
}

/* subi */
void
OP_1 ()
{
  unsigned tmp;
  if (OP[1] == 0)
    OP[1] = 16;

  trace_input ("subi", OP_REG, OP_CONSTANT16, OP_VOID);
  /* see ../common/sim-alu.h for a more extensive discussion on how to
     compute the carry/overflow bits. */
  /* since OP[1] is never <= 0, -OP[1] == ~OP[1]+1 can never overflow */
  tmp = ((unsigned)(unsigned16) GPR (OP[0])
	 + (unsigned)(unsigned16) ( - OP[1]));
  SET_PSW_C (tmp >= (1 << 16));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* trap */
void
OP_5F00 ()
{
  trace_input ("trap", OP_CONSTANT4, OP_VOID, OP_VOID);
  trace_output_void ();

  switch (OP[0])
    {
    default:
#if (DEBUG & DEBUG_TRAP) == 0
      {
	uint16 vec = OP[0] + TRAP_VECTOR_START;
	SET_BPC (PC + 1);
	SET_BPSW (PSW);
	SET_PSW (PSW & PSW_SM_BIT);
	JMP (vec);
	break;
      }
#else			/* if debugging use trap to print registers */
      {
	int i;
	static int first_time = 1;

	if (first_time)
	  {
	    first_time = 0;
	    (*d10v_callback->printf_filtered) (d10v_callback, "Trap  #     PC ");
	    for (i = 0; i < 16; i++)
	      (*d10v_callback->printf_filtered) (d10v_callback, "  %sr%d", (i > 9) ? "" : " ", i);
	    (*d10v_callback->printf_filtered) (d10v_callback, "         a0         a1 f0 f1 c\n");
	  }

	(*d10v_callback->printf_filtered) (d10v_callback, "Trap %2d 0x%.4x:", (int)OP[0], (int)PC);

	for (i = 0; i < 16; i++)
	  (*d10v_callback->printf_filtered) (d10v_callback, " %.4x", (int) GPR (i));

	for (i = 0; i < 2; i++)
	  (*d10v_callback->printf_filtered) (d10v_callback, " %.2x%.8lx",
					     ((int)(ACC (i) >> 32) & 0xff),
					     ((unsigned long) ACC (i)) & 0xffffffff);

	(*d10v_callback->printf_filtered) (d10v_callback, "  %d  %d %d\n",
					   PSW_F0 != 0, PSW_F1 != 0, PSW_C != 0);
	(*d10v_callback->flush_stdout) (d10v_callback);
	break;
      }
#endif
    case 15:			/* new system call trap */
      /* Trap 15 is used for simulating low-level I/O */
      {
	unsigned32 result = 0;
	errno = 0;

/* Registers passed to trap 0 */

#define FUNC   GPR (4)	/* function number */
#define PARM1  GPR (0)	/* optional parm 1 */
#define PARM2  GPR (1)	/* optional parm 2 */
#define PARM3  GPR (2)	/* optional parm 3 */
#define PARM4  GPR (3)	/* optional parm 3 */

/* Registers set by trap 0 */

#define RETVAL(X)   do { result = (X); SET_GPR (0, result); } while (0)
#define RETVAL32(X) do { result = (X); SET_GPR (0, result >> 16); SET_GPR (1, result); } while (0)
#define RETERR(X) SET_GPR (4, (X))		/* return error code */

/* Turn a pointer in a register into a pointer into real memory. */

#define MEMPTR(x) ((char *)(dmem_addr(x)))

	switch (FUNC)
	  {
#if !defined(__GO32__) && !defined(_WIN32)
	  case TARGET_SYS_fork:
	    trace_input ("<fork>", OP_VOID, OP_VOID, OP_VOID);
	    RETVAL (fork ());
	    trace_output_16 (result);
	    break;

#define getpid() 47
	  case TARGET_SYS_getpid:
	    trace_input ("<getpid>", OP_VOID, OP_VOID, OP_VOID);
	    RETVAL (getpid ());
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_kill:
	    trace_input ("<kill>", OP_R0, OP_R1, OP_VOID);
	    if (PARM1 == getpid ())
	      {
		trace_output_void ();
		State.exception = PARM2;
	      }
	    else
	      {
		int os_sig = -1;
		switch (PARM2)
		  {
#ifdef SIGHUP
		  case 1: os_sig = SIGHUP;	break;
#endif
#ifdef SIGINT
		  case 2: os_sig = SIGINT;	break;
#endif
#ifdef SIGQUIT
		  case 3: os_sig = SIGQUIT;	break;
#endif
#ifdef SIGILL
		  case 4: os_sig = SIGILL;	break;
#endif
#ifdef SIGTRAP
		  case 5: os_sig = SIGTRAP;	break;
#endif
#ifdef SIGABRT
		  case 6: os_sig = SIGABRT;	break;
#elif defined(SIGIOT)
		  case 6: os_sig = SIGIOT;	break;
#endif
#ifdef SIGEMT
		  case 7: os_sig = SIGEMT;	break;
#endif
#ifdef SIGFPE
		  case 8: os_sig = SIGFPE;	break;
#endif
#ifdef SIGKILL
		  case 9: os_sig = SIGKILL;	break;
#endif
#ifdef SIGBUS
		  case 10: os_sig = SIGBUS;	break;
#endif
#ifdef SIGSEGV
		  case 11: os_sig = SIGSEGV;	break;
#endif
#ifdef SIGSYS
		  case 12: os_sig = SIGSYS;	break;
#endif
#ifdef SIGPIPE
		  case 13: os_sig = SIGPIPE;	break;
#endif
#ifdef SIGALRM
		  case 14: os_sig = SIGALRM;	break;
#endif
#ifdef SIGTERM
		  case 15: os_sig = SIGTERM;	break;
#endif
#ifdef SIGURG
		  case 16: os_sig = SIGURG;	break;
#endif
#ifdef SIGSTOP
		  case 17: os_sig = SIGSTOP;	break;
#endif
#ifdef SIGTSTP
		  case 18: os_sig = SIGTSTP;	break;
#endif
#ifdef SIGCONT
		  case 19: os_sig = SIGCONT;	break;
#endif
#ifdef SIGCHLD
		  case 20: os_sig = SIGCHLD;	break;
#elif defined(SIGCLD)
		  case 20: os_sig = SIGCLD;	break;
#endif
#ifdef SIGTTIN
		  case 21: os_sig = SIGTTIN;	break;
#endif
#ifdef SIGTTOU
		  case 22: os_sig = SIGTTOU;	break;
#endif
#ifdef SIGIO
		  case 23: os_sig = SIGIO;	break;
#elif defined (SIGPOLL)
		  case 23: os_sig = SIGPOLL;	break;
#endif
#ifdef SIGXCPU
		  case 24: os_sig = SIGXCPU;	break;
#endif
#ifdef SIGXFSZ
		  case 25: os_sig = SIGXFSZ;	break;
#endif
#ifdef SIGVTALRM
		  case 26: os_sig = SIGVTALRM;	break;
#endif
#ifdef SIGPROF
		  case 27: os_sig = SIGPROF;	break;
#endif
#ifdef SIGWINCH
		  case 28: os_sig = SIGWINCH;	break;
#endif
#ifdef SIGLOST
		  case 29: os_sig = SIGLOST;	break;
#endif
#ifdef SIGUSR1
		  case 30: os_sig = SIGUSR1;	break;
#endif
#ifdef SIGUSR2
		  case 31: os_sig = SIGUSR2;	break;
#endif
		  }

		if (os_sig == -1)
		  {
		    trace_output_void ();
		    (*d10v_callback->printf_filtered) (d10v_callback, "Unknown signal %d\n", PARM2);
		    (*d10v_callback->flush_stdout) (d10v_callback);
		    State.exception = SIGILL;
		  }
		else
		  {
		    RETVAL (kill (PARM1, PARM2));
		    trace_output_16 (result);
		  }
	      }
	    break;

	  case TARGET_SYS_execve:
	    trace_input ("<execve>", OP_R0, OP_R1, OP_R2);
	    RETVAL (execve (MEMPTR (PARM1), (char **) MEMPTR (PARM2),
			     (char **)MEMPTR (PARM3)));
	    trace_output_16 (result);
	    break;

#ifdef TARGET_SYS_execv
	  case TARGET_SYS_execv:
	    trace_input ("<execv>", OP_R0, OP_R1, OP_VOID);
	    RETVAL (execve (MEMPTR (PARM1), (char **) MEMPTR (PARM2), NULL));
	    trace_output_16 (result);
	    break;
#endif

	  case TARGET_SYS_pipe:
	    {
	      reg_t buf;
	      int host_fd[2];

	      trace_input ("<pipe>", OP_R0, OP_VOID, OP_VOID);
	      buf = PARM1;
	      RETVAL (pipe (host_fd));
	      SW (buf, host_fd[0]);
	      buf += sizeof(uint16);
	      SW (buf, host_fd[1]);
	      trace_output_16 (result);
	    }
	  break;

#if 0
#ifdef TARGET_SYS_wait
	  case TARGET_SYS_wait:
	    {
	      int status;
	      trace_input ("<wait>", OP_R0, OP_VOID, OP_VOID);
	      RETVAL (wait (&status));
	      if (PARM1)
		SW (PARM1, status);
	      trace_output_16 (result);
	    }
	  break;
#endif
#endif
#else
	  case TARGET_SYS_getpid:
	    trace_input ("<getpid>", OP_VOID, OP_VOID, OP_VOID);
	    RETVAL (1);
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_kill:
	    trace_input ("<kill>", OP_REG, OP_REG, OP_VOID);
	    trace_output_void ();
	    State.exception = PARM2;
	    break;
#endif

	  case TARGET_SYS_read:
	    trace_input ("<read>", OP_R0, OP_R1, OP_R2);
	    RETVAL (d10v_callback->read (d10v_callback, PARM1, MEMPTR (PARM2),
					  PARM3));
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_write:
	    trace_input ("<write>", OP_R0, OP_R1, OP_R2);
	    if (PARM1 == 1)
	      RETVAL ((int)d10v_callback->write_stdout (d10v_callback,
							 MEMPTR (PARM2), PARM3));
	    else
	      RETVAL ((int)d10v_callback->write (d10v_callback, PARM1,
						  MEMPTR (PARM2), PARM3));
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_lseek:
	    trace_input ("<lseek>", OP_R0, OP_R1, OP_R2);
	    RETVAL32 (d10v_callback->lseek (d10v_callback, PARM1,
					    ((((unsigned long) PARM2) << 16)
					     || (unsigned long) PARM3),
					    PARM4));
	    trace_output_32 (result);
	    break;

	  case TARGET_SYS_close:
	    trace_input ("<close>", OP_R0, OP_VOID, OP_VOID);
	    RETVAL (d10v_callback->close (d10v_callback, PARM1));
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_open:
	    trace_input ("<open>", OP_R0, OP_R1, OP_R2);
	    RETVAL (d10v_callback->open (d10v_callback, MEMPTR (PARM1), PARM2));
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_exit:
	    trace_input ("<exit>", OP_R0, OP_VOID, OP_VOID);
	    State.exception = SIG_D10V_EXIT;
	    trace_output_void ();
	    break;

#ifdef TARGET_SYS_stat
	  case TARGET_SYS_stat:
	    trace_input ("<stat>", OP_R0, OP_R1, OP_VOID);
	    /* stat system call */
	    {
	      struct stat host_stat;
	      reg_t buf;

	      RETVAL (stat (MEMPTR (PARM1), &host_stat));

	      buf = PARM2;

	      /* The hard-coded offsets and sizes were determined by using
	       * the D10V compiler on a test program that used struct stat.
	       */
	      SW  (buf,    host_stat.st_dev);
	      SW  (buf+2,  host_stat.st_ino);
	      SW  (buf+4,  host_stat.st_mode);
	      SW  (buf+6,  host_stat.st_nlink);
	      SW  (buf+8,  host_stat.st_uid);
	      SW  (buf+10, host_stat.st_gid);
	      SW  (buf+12, host_stat.st_rdev);
	      SLW (buf+16, host_stat.st_size);
	      SLW (buf+20, host_stat.st_atime);
	      SLW (buf+28, host_stat.st_mtime);
	      SLW (buf+36, host_stat.st_ctime);
	    }
	    trace_output_16 (result);
	    break;
#endif

	  case TARGET_SYS_chown:
	    trace_input ("<chown>", OP_R0, OP_R1, OP_R2);
	    RETVAL (chown (MEMPTR (PARM1), PARM2, PARM3));
	    trace_output_16 (result);
	    break;

	  case TARGET_SYS_chmod:
	    trace_input ("<chmod>", OP_R0, OP_R1, OP_R2);
	    RETVAL (chmod (MEMPTR (PARM1), PARM2));
	    trace_output_16 (result);
	    break;

#if 0
#ifdef TARGET_SYS_utime
	  case TARGET_SYS_utime:
	    trace_input ("<utime>", OP_R0, OP_R1, OP_R2);
	    /* Cast the second argument to void *, to avoid type mismatch
	       if a prototype is present.  */
	    RETVAL (utime (MEMPTR (PARM1), (void *) MEMPTR (PARM2)));
	    trace_output_16 (result);
	    break;
#endif
#endif

#if 0
#ifdef TARGET_SYS_time
	  case TARGET_SYS_time:
	    trace_input ("<time>", OP_R0, OP_R1, OP_R2);
	    RETVAL32 (time (PARM1 ? MEMPTR (PARM1) : NULL));
	    trace_output_32 (result);
	    break;
#endif
#endif
	    
	  default:
	    d10v_callback->error (d10v_callback, "Unknown syscall %d", FUNC);
	  }
	if ((uint16) result == (uint16) -1)
	  RETERR (d10v_callback->get_errno(d10v_callback));
	else
	  RETERR (0);
	break;
      }
    }
}

/* tst0i */
void
OP_7000000 ()
{
  trace_input ("tst0i", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_F1 (PSW_F0);;
  SET_PSW_F0 ((GPR (OP[0]) & OP[1]) ? 1 : 0);
  trace_output_flag ();
}

/* tst1i */
void
OP_F000000 ()
{
  trace_input ("tst1i", OP_REG, OP_CONSTANT16, OP_VOID);
  SET_PSW_F1 (PSW_F0);
  SET_PSW_F0 ((~(GPR (OP[0])) & OP[1]) ? 1 : 0);
  trace_output_flag ();
}

/* wait */
void
OP_5F80 ()
{
  trace_input ("wait", OP_VOID, OP_VOID, OP_VOID);
  SET_PSW_IE (1);
  trace_output_void ();
}

/* xor */
void
OP_A00 ()
{
  int16 tmp;
  trace_input ("xor", OP_REG, OP_REG, OP_VOID);
  tmp = (GPR (OP[0]) ^ GPR (OP[1]));
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}

/* xor3 */
void
OP_5000000 ()
{
  int16 tmp;
  trace_input ("xor3", OP_REG_OUTPUT, OP_REG, OP_CONSTANT16);
  tmp = (GPR (OP[1]) ^ OP[2]);
  SET_GPR (OP[0], tmp);
  trace_output_16 (tmp);
}
