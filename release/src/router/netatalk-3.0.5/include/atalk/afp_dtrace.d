provider afp {
   probe afpfunc__start(int func, char *funcname);
   probe afpfunc__done(int func, char *funcname);
   probe read__start(long size);
   probe read__done();
   probe write__start(long size);
   probe write__done();
   probe cnid__start(char *cnidfunc);
   probe cnid__done();
};
