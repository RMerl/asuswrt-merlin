static char bytes[256];

static short shorts[256];

static void
initialize (void)
{
  int i;
  for (i = 0; i < sizeof (bytes); i++)
    {
      bytes[i] = i;
      shorts[i] = i * 2;
    }
}

int
main ()
{
  initialize ();
}
