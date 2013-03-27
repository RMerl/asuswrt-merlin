struct simops 
{
  unsigned long   opcode;
  unsigned long   mask;
  int (* func) PARAMS ((void));
  int    numops;
  int    operands[12];
};
