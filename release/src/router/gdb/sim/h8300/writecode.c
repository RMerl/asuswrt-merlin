/* Code generator for the Hitachi H8/300 architecture simulator.

   Written by Steve Chamberlain of Cygnus Support.
   sac@cygnus.com

   This file is part of H8/300 sim


		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

/* This program reads the H8/300 opcode table and writes out
   a large switch statement to understand the opcodes (with ifs if
   there is more than one opcode per case) and code to do the stuff  */

#include <stdio.h>

#define DEFINE_TABLE
#define INSIM
#include"opcode/h8300.h"

#define MAXSAME 140

#define PTWO 256
static struct h8_opcode *h8_opcodes_sorted[PTWO][MAXSAME];

char *cs = "/*";
char *ce = "*/";

/* How to get at nibble n from the instruction */
char *nibs[] =
{
  "foo",
  "(b0&0xf)",
  "((b1>>4)&0xf)",
  "((b1)&0xf)",
  "((pc[1]>>12)&0xf)",
  "((pc[1]>>8)&0xf)",
  "((pc[1]>>4)&0xf)",
  "((pc[1])&0xf)",
  0, 0};

/* how to get at the 3 bit immediate in the instruction */
char *imm3[] =
{"foo",
 "foo",
 "((b1>>4)&0x7)",
 "foo",
 "foo",
 "foo",
 "(pc[1]>>4)&0x7"};

/* How to get at a byte register from an index in the instruction at
   nibble n */
char *breg[] =
{"foo",
 "*(blow[b0])",
 "*(bhigh[b1])",
 "*(blow[b1])",
 0, 0,
 "*(bhigh[pc[1]>>8])"};

/* How to get at a word register from an index in the instruction at
   nibble n */

char *wreg[] =
{"foo",
 "*(wlow[b0])",
 "*(whigh[b1])",
 "*(wlow[b1])"};

#define sorted_key noperands

/* sort the opcode table into h8_opcodes_sorted[0..255] */
static void
init ()
{
  unsigned int i;
  struct h8_opcode *p;

  for (p = h8_opcodes; p->name; p++)
    {
      int n1 = 0;
      int n2 = 0;
      int j;
#if 0
      for (j = 0; p->data.nib[j] != E; j++)
	{
	  if ((int) p->data.nib[j] == ABS16ORREL8SRC)
	    p->data.nib[j] = ABS16SRC;
	  if ((int) p->data.nib[j] == ABS16OR8SRC)
	    p->data.nib[j] = ABS16SRC;
	  if ((int) p->data.nib[j] == ABS16OR8DST)
	    p->data.nib[j] = ABS16DST;
	}
#endif

      if ((int) p->data.nib[0] < 16)
	{
	  n1 = (int) p->data.nib[0];
	}
      else
	n1 = 0;
      if ((int) p->data.nib[1] < 16)
	{
	  n2 = (int) p->data.nib[1];
	}
      else
	n2 = 0;
      for (i = 0; i < MAXSAME; i++)
	{
	  int j = /* ((n3 >> 3) * 512) + ((n4 >> 3) * 256) + */ n1 * 16 + n2;

	  if (h8_opcodes_sorted[j][i] == (struct h8_opcode *) NULL)
	    {
	      h8_opcodes_sorted[j][i] = p;
	      p->sorted_key = j;
	      break;
	    }
	}

      if (i == MAXSAME)
	abort ();

      /* Just make sure there are an even number of nibbles in it, and
       that the count is the same s the length */
      for (i = 0; p->data.nib[i] != E; i++)
	/*EMPTY*/ ;
      if (i & 1)
	abort ();
      p->length = i / 2;
    }
  for (i = 0; i < PTWO; i++)
    {
      if (h8_opcodes_sorted[i][0])
	p = h8_opcodes_sorted[i][0];
      else
	h8_opcodes_sorted[i][0] = p;
    }
}

/* decode the lvalues, creating a pointer in real space to object -
 remember if the thing has to be swapped out of where it is */


int swap[2];

lval (p)
     struct h8_opcode *p;
{
  int i;

  for (i = 0; p->data.nib[i] != E; i++)
    {
      int x = p->data.nib[i];
      int size;
      int op;
      op = (x & DST) ? 1 : 0;

      switch (x & SIZE)
	{
	case L_32:
	  size = 32;
	  break;
	case L_16:
	  size = 16;
	  break;
	case L_8:
	  size = 8;
	  break;
	default:
	  size = 1234;
	}

      if (x & REG)
	{
	  printf ("ir%d = GET_LVAL_%d_REG(%d);\n", op, size, i);
	}
      else if (x & IMM)
	{
	  printf ("/* Imm has no lvalue */\n");
	}

    }



}

void
decode (p, fetch, size)
     struct h8_opcode *p;
     int fetch;
     int size;
{
  if (fetch)
    {
      lval (p);
    }

}



static void
esleep ()
{
  printf ("saved_state.exception = SIGSTOP;\n");
}

static void
mov (p, s, sz)
     struct h8_opcode *p;
     char *s;
     int sz;
{
  printf ("dst = srca;\n");
}

static void
andc (p)
     struct h8_opcode *p;
{
  printf ("SET_CCR(GET_CCR() & srca);\n");
}

static void
addx (p)
     struct h8_opcode *p;
{
  printf ("dst = srca + srcb+ (c != 0);\n");
}

static void
subx (p)
     struct h8_opcode *p;
{
  printf ("dst = srcb - srca - (c != 0);\n");
}

static void
add (p, s, sz)
     struct h8_opcode *p;
     char *s;
     int sz;
{
  printf ("%s;\n", s);
}

static void
adds (p, s)
     struct h8_opcode *p;
     char *s;
{
  printf ("%s;\n", s);
}

static void
bra (p, a)
     struct h8_opcode *p;
     char *a;
{
  printf ("if (%s) npc += ((char )b1)>>1;\n", a);
}

static void
bsr (p, a)
     struct h8_opcode *p;
     char *a;
{
  printf ("reg[7]-=2;\n");
  printf ("tmp = reg[7];\n");
  printf ("SET_WORD_MEM(tmp, (npc-saved_state.mem)*2);\n");
  printf ("npc += ((char)b1)>>1;\n");
}

static void
cmp (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  decode (p, 1, s);
  printf ("srca = -srca;\n");
  printf ("dst = srca + srcb;\n");
}

static
void
jsr (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("if (b1 == 0xc4) {\n");
  printf ("printf(\"%%c\", reg[2]);\n");
  printf ("}\n");
  printf ("else {\n");
  printf ("reg[7]-=2;\n");
  printf ("tmp = reg[7];\n");
  printf ("SET_WORD_MEM(tmp, (npc-saved_state.mem)*2);\n");
  printf ("npc = (lval>>1) + saved_state.mem;\n");
  printf ("}");
}

static void
jmp (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("npc = (lval>>1) + saved_state.mem;\n");
}

static void
rts (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("tmp = reg[7];\n");
  printf ("reg[7]+=2;\n");
  printf ("npc = saved_state.mem + (WORD_MEM(tmp)>>1);\n");
}

static void
rte (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("reg[7]+=2;\n");
  printf ("tmp = reg[7];\n");
  printf ("reg[7]+=2;\n");
  printf ("SET_CCR(tmp);\n");
  printf ("npc = saved_state.mem + (WORD_MEM(tmp)>>1);\n");
}

static void
setf (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("tmp = GET_CCR();\n");
  printf ("tmp %s= srca;\n", a);
}

static void
bpt (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("saved_state.exception = SIGTRAP;\n");
  printf ("npc = pc;\n");
}

static void
log (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("dst = srcb %s srca;\n", a);
}

static void
ulog (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("dst = %s srcb ;\n", a);
}

static void
nop ()
{
}

static void
rotl ()
{
  printf ("c = srcb & 0x80;\n");
  printf ("dst = srcb << 1;\n");
  printf ("if (c) dst|=1;\n");
}

static void
rotr ()
{
  printf ("c = srcb & 1;\n");
  printf ("dst = srcb >> 1;\n");
  printf ("if (c) dst|=0x80;\n");
}

static void
rotxl ()
{
  printf ("tmp = srcb & 0x80;\n");
  printf ("dst = srcb << 1;\n");
  printf ("if (c) dst|=1;\n");
  printf ("c = tmp;\n");
}

static void
rotxr ()
{
  printf ("tmp = srcb & 1;\n");
  printf ("dst = srcb >> 1;\n");
  printf ("if (c) dst|=0x80;\n");
  printf ("c = tmp;\n");
}

static void
shal ()
{
  printf ("c = srcb&0x80;\n");
  printf ("dst = srcb << 1;\n");
}

static
void
shar ()
{
  printf ("c = srcb&0x1;\n");
  printf ("if (srcb&0x80) dst = (srcb>>1) | 0x80;\n");
  printf ("else  dst = (srcb>>1) &~ 0x80;\n");
}

static
void
shll ()
{
  printf ("c = srcb&0x80;\n");
  printf ("dst = srcb << 1;\n");
}

static
void
shlr ()
{
  printf ("c = srcb&0x1;\n");
  printf ("dst = (srcb>>1) &~ 0x80;\n");
}

static
void
divxu ()
{
  printf ("srca = %s;\n", breg[2]);
  printf ("srcb = %s;\n", wreg[3]);
  printf ("n = srca & 0x80;\n");
  printf ("z = !srca;\n");
  printf ("if (srca) dst = srcb / srca;tmp = srcb %% srca;\n");
  printf ("%s = (dst & 0xff) | (tmp << 8);\n", wreg[3]);
}

static
void
mulxu ()
{
  printf ("srca = %s;\n", breg[2]);
  printf ("srcb = %s;\n", wreg[3]);

  printf ("dst = (srcb&0xff) * srca;\n");
  printf ("%s = dst;\n", wreg[3]);
}

static
void
inc ()
{
  printf ("dst = %s;\n", breg[3]);
  printf ("v = (dst==0x7f);\n");
  printf ("dst++;\n");
  printf ("%s= dst;\n", breg[3]);
}

static
void
bit (p, a, s)
     struct h8_opcode *p;
     char *a;
     int s;
{
  printf ("%s\n", a);
}

static
void
dec ()
{
  printf ("dst = %s;\n", breg[3]);
  printf ("v = (dst==0x80);\n");
  printf ("dst--;\n");
  printf ("%s = dst;\n", breg[3]);
}

char saf[] = "goto setflags;";
char sf[] = "goto shiftflags;";
char af8[] = "goto aluflags8;";
char af16[] = "goto aluflags16;";
char lf[] = "goto logflags;";
char icf[] = "goto incflags;";
char mf8[] = "goto movflags8;";
char mf16[] = "goto movflags16;";
char nx[] = "goto next;";

struct
{
  char *ftype;
  int decode;
  char *name;
  void (*func) ();
  char *arg;
  int size;

}

table[] =
{
  {
    nx, 1, "bld", bit, "dst = srcb; c = (srcb>>srca)&1;", 8
  }
  ,
  {
    nx, 1, "bild", bit, "dst = srcb; c = !((srcb>>srca)&1);", 8
  }
  ,
  {
    nx, 1, "band", bit, "dst = srcb; c = C &&((srcb>>srca)&1);", 8
  }
  ,
  {
    nx, 1, "biand", bit, "dst = srcb; c = C &&(!((srcb>>srca)&1));", 8
  }
  ,
  {
    nx, 1, "bior", bit, "dst = srcb; c = C ||(!((srcb>>srca)&1));", 8
  }
  ,
  {
    nx, 1, "bor", bit, "dst = srcb; c = C ||(((srcb>>srca)&1));", 8
  }
  ,
  {
    nx, 1, "bixor", bit, "dst = srcb; c = C ^(!((srcb>>srca)&1));", 8
  }
  ,
  {
    nx, 1, "bxor", bit, "dst = srcb; c = C ^(((srcb>>srca)&1));", 8
  }
  ,
  {
    nx, 1, "bnot", bit, "dst = srcb ^ (1<<srca);", 8
  }
  ,
  {
    nx, 1, "bclr", bit, "dst = srcb & ~(1<<srca);", 8
  }
  ,
  {
    nx, 1, "bset", bit, "dst = srcb | (1<<srca);", 8
  }
  ,
  {
    nx, 1, "bst", bit, "dst = (srcb & ~(1<<srca))| ((C)<<srca);", 8
  }
  ,
  {
    nx, 1, "bist", bit, "dst = (srcb & ~(1<<srca))| ((!C)<<srca);", 8
  }
  ,
  {
    nx, 1, "btst", bit, "dst = srcb; z = !((srcb>>srca)&1);", 8
  }
  ,
  {
    icf, 0, "dec", dec, 0, 0
  }
  ,
  {
    icf, 0, "inc", inc, 0, 0
  }
  ,
  {
    saf, 1, "orc", setf, "|", 0
  }
  ,
  {
    saf, 1, "xorc", setf, "^", 0
  }
  ,
  {
    saf, 1, "andc", setf, "&", 0
  }
  ,
  {
    nx, 1, "nop", nop, 0, 0
  }
  ,
  {
    nx, 1, "bra", bra, "1", 0
  }
  ,
  {
    nx, 1, "brn", bra, "0", 0
  }
  ,
  {
    nx, 1, "bhi", bra, "(C||Z)==0", 0
  }
  ,
  {
    nx, 1, "bls", bra, "(C||Z)==1", 0
  }
  ,
  {
    nx, 1, "bcs", bra, "C==1", 0
  }
  ,
  {
    nx, 1, "bcc", bra, "C==0", 0
  }
  ,
  {
    nx, 1, "bpl", bra, "N==0", 0
  }
  ,
  {
    nx, 1, "bmi", bra, "N==1", 0
  }
  ,
  {
    nx, 1, "bvs", bra, "V==1", 0
  }
  ,
  {
    nx, 1, "bvc", bra, "V==0", 0
  }
  ,
  {
    nx, 1, "bge", bra, "(N^V)==0", 0
  }
  ,
  {
    nx, 1, "bgt", bra, "(Z|(N^V))==0", 0
  }
  ,
  {
    nx, 1, "blt", bra, "(N^V)==1", 0
  }
  ,
  {
    nx, 1, "ble", bra, "(Z|(N^V))==1", 0
  }
  ,
  {
    nx, 1, "beq", bra, "Z==1", 0
  }
  ,
  {
    nx, 1, "bne", bra, "Z==0", 0
  }
  ,
  {
    nx, 1, "bsr", bsr, "", 0
  }
  ,
  {
    nx, 1, "jsr", jsr, 0, 0
  }
  ,
  {
    nx, 1, "jmp", jmp, 0, 0
  }
  ,
  {
    nx, 0, "rts", rts, 0, 0
  }
  ,
  {
    nx, 0, "rte", rte, 0, 0
  }
  ,
  {
    nx, 1, "andc", andc, 0, 0
  }
  ,
  {
    sf, 1, "shal", shal, 0, 0
  }
  ,
  {
    sf, 1, "shar", shar, 0, 0
  }
  ,
  {
    sf, 1, "shll", shll, 0, 0
  }
  ,
  {
    sf, 1, "shlr", shlr, 0, 0
  }
  ,
  {
    sf, 1, "rotxl", rotxl, 0, 0
  }
  ,
  {
    sf, 1, "rotxr", rotxr, 0, 0
  }
  ,
  {
    sf, 1, "rotl", rotl, 0, 0
  }
  ,
  {
    sf, 1, "rotr", rotr, 0, 0
  }
  ,
  {
    lf, 1, "xor", log, "^", 0
  }
  ,
  {
    lf, 1, "and", log, "&", 0
  }
  ,
  {
    lf, 1, "or", log, "|", 0
  }
  ,
  {
    lf, 1, "not", ulog, " ~", 0
  }
  ,
  {
    lf, 1, "neg", ulog, " - ", 0
  }
  ,
  {
    nx, 1, "adds", adds, "dst = srca + srcb", 0
  }
  ,
  {
    nx, 1, "subs", adds, "srca = -srca; dst = srcb + srca", 0
  }
  ,
  {
    af8, 1, "add.b", add, "dst = srca + srcb", 8
  }
  ,
  {
    af16, 1, "add.w", add, "dst = srca + srcb", 16
  }
  ,
  {
    af16, 1, "sub.w", add, "srca = -srca; dst = srcb + srca", 16
  }
  ,
  {
    af8, 1, "sub.b", add, "srca = -srca; dst = srcb + srca", 8
  }
  ,
  {
    af8, 1, "addx", addx, 0, 8
  }
  ,
  {
    af8, 1, "subx", subx, 0, 8
  }
  ,
  {
    af8, 0, "cmp.b", cmp, 0, 8
  }
  ,
  {
    af16, 0, "cmp.w", cmp, 0, 16
  }
  ,
  {
    nx, 1, "sleep", esleep, 0, 0
  }
  ,
  {
    nx, 0, "bpt", bpt, 0, 8
  }
  ,
  {
    nx, 0, "divxu", divxu, 0, 0
  }
  ,
  {
    nx, 0, "mulxu", mulxu, 0, 0
  }
  ,
  {
    mf8, 1, "mov.b", mov, 0, 8
  }
  ,
  {
    mf8, 1, "movtpe", mov, 0, 8
  }
  ,
  {
    mf8, 1, "movfpe", mov, 0, 8
  }
  ,
  {
    mf16, 1, "mov.w", mov, 0, 16
  }
  ,
  {
    0
  }
};

static
void
edo (p)
     struct h8_opcode *p;
{
  int i;

  printf ("%s %s %s\n", cs, p->name, ce);

  for (i = 0; table[i].name; i++)
    {
      if (strcmp (table[i].name, p->name) == 0)
	{
	  printf ("{\n");
	  if (table[i].decode)
	    decode (p, 1, table[i].size);
	  printf ("cycles += %d;\n", p->time);
	  printf ("npc = pc + %d;\n", p->length / 2);
	  table[i].func (p, table[i].arg, table[i].size);
	  if (table[i].decode)
	    decode (p, 0, table[i].size);
	  if (table[i].ftype)
	    printf (table[i].ftype);
	  else
	    printf ("goto next;\n");
	  printf ("}\n");
	  return;
	}
    }
  printf ("%s not found %s\n", cs, ce);
  printf ("saved_state.exception = SIGILL;\n");
  printf ("break;\n");
}

static
int
owrite (i)
     int i;
{
  /* write if statements to select the right opcode */
  struct h8_opcode **p;
  int needand = 1;

  p = h8_opcodes_sorted[i];
  printf ("case 0x%03x:\n", i);

  if (p[1] == 0)
    {
      /* See if the next few also match */
      while (h8_opcodes_sorted[i + 1][0] == *p)
	{
	  i++;
	  printf ("case 0x%03x:\n", i);
	}

      /* Don't need any if's this is the only one */
      edo (*p);
    }
  else
    {
      while (*p)
	{
	  /* start two nibbles in since we know we match in the first byte */
	  int c;
	  int nib = 2;
	  int byte = 1;
	  int mask1[5];
	  int mask0[5];
	  int nibshift = 4;
	  int any = 0;

	  for (c = 0; c < 5; c++)
	    {
	      mask1[c] = 0;
	      mask0[c] = 0;
	    }
	  printf ("%s %x%x", cs, (*p)->data.nib[0], (*p)->data.nib[1]);
	  while ((c = (*p)->data.nib[nib]) != E)
	    {
	      if (c & B30)
		{
		  /* bit 3 must be zero */
		  mask0[byte] |= 0x8 << nibshift;
		  printf ("0");
		  any = 1;
		}
	      else if (c & B31)
		{
		  /* bit 3 must be one */
		  mask1[byte] |= 0x8 << nibshift;
		  printf ("8");
		  any = 1;
		}
	      else if (c <= HexF)
		{
		  mask0[byte] |= ((~c) & 0xf) << nibshift;
		  mask1[byte] |= (c & 0xf) << nibshift;
		  printf ("%x", c);
		  any = 1;
		}
	      else
		{
		  printf ("x");
		}
	      nib++;
	      if (nibshift == 4)
		{
		  nibshift = 0;
		}
	      else
		{
		  byte++;
		  nibshift = 4;
		}
	    }
	  printf ("*/\n");
	  if (any)
	    {
	      printf ("if (");
	      needand = 0;
	      for (c = 1; c < byte; c++)
		{
		  if (mask0[c] | mask1[c])
		    {
		      int sh;

		      if (needand)
			printf ("\n&&");
		      if (c & 1)
			sh = 0;
		      else
			sh = 8;
		      if (c / 2 == 0 && sh == 0)
			printf ("((b1&0x%x)==0x%x)", mask0[c] | mask1[c],
				mask1[c]);
		      else
			{
			  printf ("((pc[%d]&(0x%02x<<%d))==(0x%x<<%d))",
				  c / 2, mask0[c] | mask1[c], sh,
				  mask1[c], sh);
			}

		      needand = 1;
		    }
		}
	      printf (")\n");
	    }
	  edo (*p);
	  p++;

	}
    }
  return i;
}

static
void
remove_dups ()
{
  struct h8_opcode *s;
  struct h8_opcode *d;

  for (d = s = h8_opcodes; s->name; s++)
    {
      int doit = 1;

      if (strcmp (s->name, "push") == 0)
	doit = 0;
      if (strcmp (s->name, "bhs") == 0)
	doit = 0;
      if (strcmp (s->name, "blo") == 0)
	doit = 0;
      if (strcmp (s->name, "bt") == 0)
	doit = 0;
      if (strcmp (s->name, "bf") == 0)
	doit = 0;
      if (strcmp (s->name, "pop") == 0)
	doit = 0;
      if (doit)
	{
	  *d++ = *s;
	}
    }
  *d++ = *s++;
}

int
main ()
{
  int i;

  remove_dups ();
  init ();

  printf ("%s do the operation %s\n", cs, ce);
  printf ("switch (b0) \n{\n");
  for (i = 0; i < PTWO; i++)
    {
      i = owrite (i);
    }
  printf ("}\n");

  return 0;
}
