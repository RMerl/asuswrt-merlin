/* Interface to prologue value handling for GDB.
   Copyright 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef PROLOGUE_VALUE_H
#define PROLOGUE_VALUE_H

/* When we analyze a prologue, we're really doing 'abstract
   interpretation' or 'pseudo-evaluation': running the function's code
   in simulation, but using conservative approximations of the values
   it would have when it actually runs.  For example, if our function
   starts with the instruction:

      addi r1, 42     # add 42 to r1

   we don't know exactly what value will be in r1 after executing this
   instruction, but we do know it'll be 42 greater than its original
   value.

   If we then see an instruction like:

      addi r1, 22     # add 22 to r1

   we still don't know what r1's value is, but again, we can say it is
   now 64 greater than its original value.

   If the next instruction were:

      mov r2, r1      # set r2 to r1's value

   then we can say that r2's value is now the original value of r1
   plus 64.

   It's common for prologues to save registers on the stack, so we'll
   need to track the values of stack frame slots, as well as the
   registers.  So after an instruction like this:

      mov (fp+4), r2

   then we'd know that the stack slot four bytes above the frame
   pointer holds the original value of r1 plus 64.

   And so on.

   Of course, this can only go so far before it gets unreasonable.  If
   we wanted to be able to say anything about the value of r1 after
   the instruction:

      xor r1, r3      # exclusive-or r1 and r3, place result in r1

   then things would get pretty complex.  But remember, we're just
   doing a conservative approximation; if exclusive-or instructions
   aren't relevant to prologues, we can just say r1's value is now
   'unknown'.  We can ignore things that are too complex, if that loss
   of information is acceptable for our application.

   So when I say "conservative approximation" here, what I mean is an
   approximation that is either accurate, or marked "unknown", but
   never inaccurate.

   Once you've reached the current PC, or an instruction that you
   don't know how to simulate, you stop.  Now you can examine the
   state of the registers and stack slots you've kept track of.

   - To see how large your stack frame is, just check the value of the
     stack pointer register; if it's the original value of the SP
     minus a constant, then that constant is the stack frame's size.
     If the SP's value has been marked as 'unknown', then that means
     the prologue has done something too complex for us to track, and
     we don't know the frame size.

   - To see where we've saved the previous frame's registers, we just
     search the values we've tracked --- stack slots, usually, but
     registers, too, if you want --- for something equal to the
     register's original value.  If the ABI suggests a standard place
     to save a given register, then we can check there first, but
     really, anything that will get us back the original value will
     probably work.

   Sure, this takes some work.  But prologue analyzers aren't
   quick-and-simple pattern patching to recognize a few fixed prologue
   forms any more; they're big, hairy functions.  Along with inferior
   function calls, prologue analysis accounts for a substantial
   portion of the time needed to stabilize a GDB port.  So I think
   it's worthwhile to look for an approach that will be easier to
   understand and maintain.  In the approach used here:

   - It's easier to see that the analyzer is correct: you just see
     whether the analyzer properly (albiet conservatively) simulates
     the effect of each instruction.

   - It's easier to extend the analyzer: you can add support for new
     instructions, and know that you haven't broken anything that
     wasn't already broken before.

   - It's orthogonal: to gather new information, you don't need to
     complicate the code for each instruction.  As long as your domain
     of conservative values is already detailed enough to tell you
     what you need, then all the existing instruction simulations are
     already gathering the right data for you.

   A 'struct prologue_value' is a conservative approximation of the
   real value the register or stack slot will have.  */

struct prologue_value {

  /* What sort of value is this?  This determines the interpretation
     of subsequent fields.  */
  enum {

    /* We don't know anything about the value.  This is also used for
       values we could have kept track of, when doing so would have
       been too complex and we don't want to bother.  The bottom of
       our lattice.  */
    pvk_unknown,

    /* A known constant.  K is its value.  */
    pvk_constant,

    /* The value that register REG originally had *UPON ENTRY TO THE
       FUNCTION*, plus K.  If K is zero, this means, obviously, just
       the value REG had upon entry to the function.  REG is a GDB
       register number.  Before we start interpreting, we initialize
       every register R to { pvk_register, R, 0 }.  */
    pvk_register,

  } kind;

  /* The meanings of the following fields depend on 'kind'; see the
     comments for the specific 'kind' values.  */
  int reg;
  CORE_ADDR k;
};

typedef struct prologue_value pv_t;


/* Return the unknown prologue value --- { pvk_unknown, ?, ? }.  */
pv_t pv_unknown (void);

/* Return the prologue value representing the constant K.  */
pv_t pv_constant (CORE_ADDR k);

/* Return the prologue value representing the original value of
   register REG, plus the constant K.  */
pv_t pv_register (int reg, CORE_ADDR k);


/* Return conservative approximations of the results of the following
   operations.  */
pv_t pv_add (pv_t a, pv_t b);               /* a + b */
pv_t pv_add_constant (pv_t v, CORE_ADDR k); /* a + k */
pv_t pv_subtract (pv_t a, pv_t b);          /* a - b */
pv_t pv_logical_and (pv_t a, pv_t b);       /* a & b */


/* Return non-zero iff A and B are identical expressions.

   This is not the same as asking if the two values are equal; the
   result of such a comparison would have to be a pv_boolean, and
   asking whether two 'unknown' values were equal would give you
   pv_maybe.  Same for comparing, say, { pvk_register, R1, 0 } and {
   pvk_register, R2, 0}.

   Instead, this function asks whether the two representations are the
   same.  */
int pv_is_identical (pv_t a, pv_t b);


/* Return non-zero if A is known to be a constant.  */
int pv_is_constant (pv_t a);

/* Return non-zero if A is the original value of register number R
   plus some constant, zero otherwise.  */
int pv_is_register (pv_t a, int r);


/* Return non-zero if A is the original value of register R plus the
   constant K.  */
int pv_is_register_k (pv_t a, int r, CORE_ADDR k);

/* A conservative boolean type, including "maybe", when we can't
   figure out whether something is true or not.  */
enum pv_boolean {
  pv_maybe,
  pv_definite_yes,
  pv_definite_no,
};


/* Decide whether a reference to SIZE bytes at ADDR refers exactly to
   an element of an array.  The array starts at ARRAY_ADDR, and has
   ARRAY_LEN values of ELT_SIZE bytes each.  If ADDR definitely does
   refer to an array element, set *I to the index of the referenced
   element in the array, and return pv_definite_yes.  If it definitely
   doesn't, return pv_definite_no.  If we can't tell, return pv_maybe.

   If the reference does touch the array, but doesn't fall exactly on
   an element boundary, or doesn't refer to the whole element, return
   pv_maybe.  */
enum pv_boolean pv_is_array_ref (pv_t addr, CORE_ADDR size,
                                 pv_t array_addr, CORE_ADDR array_len,
                                 CORE_ADDR elt_size,
                                 int *i);


/* A 'struct pv_area' keeps track of values stored in a particular
   region of memory.  */
struct pv_area;

/* Create a new area, tracking stores relative to the original value
   of BASE_REG.  If BASE_REG is SP, then this effectively records the
   contents of the stack frame: the original value of the SP is the
   frame's CFA, or some constant offset from it.

   Stores to constant addresses, unknown addresses, or to addresses
   relative to registers other than BASE_REG will trash this area; see
   pv_area_store_would_trash.  */
struct pv_area *make_pv_area (int base_reg);

/* Free AREA.  */
void free_pv_area (struct pv_area *area);


/* Register a cleanup to free AREA.  */
struct cleanup *make_cleanup_free_pv_area (struct pv_area *area);


/* Store the SIZE-byte value VALUE at ADDR in AREA.

   If ADDR is not relative to the same base register we used in
   creating AREA, then we can't tell which values here the stored
   value might overlap, and we'll have to mark everything as
   unknown.  */
void pv_area_store (struct pv_area *area,
                    pv_t addr,
                    CORE_ADDR size,
                    pv_t value);

/* Return the SIZE-byte value at ADDR in AREA.  This may return
   pv_unknown ().  */
pv_t pv_area_fetch (struct pv_area *area, pv_t addr, CORE_ADDR size);

/* Return true if storing to address ADDR in AREA would force us to
   mark the contents of the entire area as unknown.  This could happen
   if, say, ADDR is unknown, since we could be storing anywhere.  Or,
   it could happen if ADDR is relative to a different register than
   the other stores base register, since we don't know the relative
   values of the two registers.

   If you've reached such a store, it may be better to simply stop the
   prologue analysis, and return the information you've gathered,
   instead of losing all that information, most of which is probably
   okay.  */
int pv_area_store_would_trash (struct pv_area *area, pv_t addr);


/* Search AREA for the original value of REGISTER.  If we can't find
   it, return zero; if we can find it, return a non-zero value, and if
   OFFSET_P is non-zero, set *OFFSET_P to the register's offset within
   AREA.  GDBARCH is the architecture of which REGISTER is a member.

   In the worst case, this takes time proportional to the number of
   items stored in AREA.  If you plan to gather a lot of information
   about registers saved in AREA, consider calling pv_area_scan
   instead, and collecting all your information in one pass.  */
int pv_area_find_reg (struct pv_area *area,
                      struct gdbarch *gdbarch,
                      int reg,
                      CORE_ADDR *offset_p);


/* For every part of AREA whose value we know, apply FUNC to CLOSURE,
   the value's address, its size, and the value itself.  */
void pv_area_scan (struct pv_area *area,
                   void (*func) (void *closure,
                                 pv_t addr,
                                 CORE_ADDR size,
                                 pv_t value),
                   void *closure);


#endif /* PROLOGUE_VALUE_H */
