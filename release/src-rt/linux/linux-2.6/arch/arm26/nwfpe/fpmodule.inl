/*
    NetWinder Floating Point Emulator
    (c) Rebel.COM, 1998,1999

    Direct questions, comments to Scott Bambrough <scottb@netwinder.org>

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

extern __inline__
unsigned int readRegister(const unsigned int nReg)
{
  /* Note: The CPU thinks it has dealt with the current instruction.  As
           a result the program counter has been advanced to the next
           instruction, and points 4 bytes beyond the actual instruction
           that caused the invalid instruction trap to occur.  We adjust
           for this in this routine.  LDF/STF instructions with Rn = PC
           depend on the PC being correct, as they use PC+8 in their 
           address calculations. */
  unsigned int *userRegisters = GET_USERREG();
  unsigned int val = userRegisters[nReg];
  if (REG_PC == nReg) val -= 4;
  return val;
}

extern __inline__
void writeRegister(const unsigned int nReg, const unsigned int val)
{
  unsigned int *userRegisters = GET_USERREG();
  userRegisters[nReg] = val;
}

extern __inline__
unsigned int readCPSR(void)
{
  return(readRegister(REG_CPSR));
}

extern __inline__
void writeCPSR(const unsigned int val)
{
  writeRegister(REG_CPSR,val);
}

extern __inline__
unsigned int readConditionCodes(void)
{
#ifdef __FPEM_TEST__
   return(0);
#else
   return(readCPSR() & CC_MASK);
#endif
}

extern __inline__
void writeConditionCodes(const unsigned int val)
{
  unsigned int *userRegisters = GET_USERREG();
  unsigned int rval;
  /*
   * Operate directly on userRegisters since
   * the CPSR may be the PC register itself.
   */
  rval = userRegisters[REG_CPSR] & ~CC_MASK;
  userRegisters[REG_CPSR] = rval | (val & CC_MASK);
}

extern __inline__
unsigned int readMemoryInt(unsigned int *pMem)
{
  return *pMem;
}
