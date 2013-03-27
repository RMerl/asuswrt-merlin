/* Tests regarding examination of prologues.  */

#ifdef PROTOTYPES
int
inner (int z)
#else
int
inner (z)
     int z;
#endif
{
  return 2 * z;
}

#ifdef PROTOTYPES
int
middle (int x)
#else
int
middle (x)
     int x;
#endif
{
  if (x == 0)
    return inner (5);
  else
    return inner (6);
}

#ifdef PROTOTYPES
int
top (int y)
#else
int
top (y)
     int y;
#endif
{
  return middle (y + 1);
}

#ifdef PROTOTYPES
int
main (int argc, char **argv)
#else
int
main (argc, argv)
     int argc;
     char **argv;
#endif
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  return top (-1) + top (1);
}
