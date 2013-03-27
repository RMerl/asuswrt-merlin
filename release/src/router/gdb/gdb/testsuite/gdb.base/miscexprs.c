void
marker1 ()
{
   
}

int
main ()
{
  STORAGE struct {
    char c[100];
  } cbig;

  STORAGE struct {
    int i[800];
  } ibig;

  STORAGE struct {
    long l[900];
  } lbig;

  STORAGE struct {
    float f[200];
  } fbig;

  STORAGE struct {
    double d[300];
  } dbig;

  STORAGE struct {
    short s[400];
  } sbig;

  ibig.i[100] = 5;
  cbig.c[0] = '\0';
  cbig.c[99] = 'A';
  fbig.f[100] = 11.99999;
  dbig.d[202] = 9.99999999;
  sbig.s[90] = 255;
  lbig.l[333] = 999999999;
    
#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif

  marker1 ();
  return 0;
}
