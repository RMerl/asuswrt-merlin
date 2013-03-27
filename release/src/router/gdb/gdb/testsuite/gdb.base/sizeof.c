#include <stdio.h>

typedef char padding[16];

struct {
  padding p1;
  char v;
  padding p2;
} padding_char;

struct {
  padding p1;
  short v;
  padding p2;
} padding_short;

struct {
  padding p1;
  int v;
  padding p2;
} padding_int;

struct {
  padding p1;
  long v;
  padding p2;
} padding_long;

struct {
  padding p1;
  long long v;
  padding p2;
} padding_long_long;

struct {
  padding p1;
  float v;
  padding p2;
} padding_float;

struct {
  padding p1;
  double v;
  padding p2;
} padding_double;

struct {
  padding p1;
  long double v;
  padding p2;
} padding_long_double;

static void
fill (void *buf, long sizeof_buf)
{
  char *p = buf;
  int i;
  for (i = 0; i < sizeof_buf; i++)
    p[i] = "The quick brown dingo jumped over the layzy dog."[i];
}

void
fill_structs (void)
{
  fill (&padding_char.p1, sizeof (padding));
  fill (&padding_char.v, sizeof (padding_char.v));
  fill (&padding_char.p2, sizeof (padding));

  fill (&padding_short.p1, sizeof (padding));
  fill (&padding_short.v, sizeof (padding_short.v));
  fill (&padding_short.p2, sizeof (padding));

  fill (&padding_int.p1, sizeof (padding));
  fill (&padding_int.v, sizeof (padding_int.v));
  fill (&padding_int.p2, sizeof (padding));

  fill (&padding_long.p1, sizeof (padding));
  fill (&padding_long.v, sizeof (padding_long.v));
  fill (&padding_long.p2, sizeof (padding));

  fill (&padding_long_long.p1, sizeof (padding));
  fill (&padding_long_long.v, sizeof (padding_long_long.v));
  fill (&padding_long_long.p2, sizeof (padding));

  fill (&padding_float.p1, sizeof (padding));
  fill (&padding_float.v, sizeof (padding_float.v));
  fill (&padding_float.p2, sizeof (padding));

  fill (&padding_double.p1, sizeof (padding));
  fill (&padding_double.v, sizeof (padding_double.v));
  fill (&padding_double.p2, sizeof (padding));

  fill (&padding_long_double.p1, sizeof (padding));
  fill (&padding_long_double.v, sizeof (padding_long_double.v));
  fill (&padding_long_double.p2, sizeof (padding));
}

int
main ()
{
  fill_structs ();

  printf ("sizeof (char) == %d\n", sizeof (char));
  printf ("sizeof (short) == %d\n", sizeof (short));
  printf ("sizeof (int) == %d\n", sizeof (int));
  printf ("sizeof (long) == %d\n", sizeof (long));
  printf ("sizeof (long long) == %d\n", sizeof (long long));

  printf ("sizeof (void *) == %d\n", sizeof (void*));
  printf ("sizeof (void (*)(void)) == %d\n", sizeof (void (*)(void)));

  printf ("sizeof (float) == %d\n", sizeof (float));
  printf ("sizeof (double) == %d\n", sizeof (double));
  printf ("sizeof (long double) == %d\n", sizeof (long double));

  /* Signed char?  */
  printf ("valueof ('\\377') == %d\n", '\377');
  printf ("valueof ((int) (char) -1) == %d\n", (int) (char) -1);
  printf ("valueof ((int) (signed char) -1) == %d\n", (int) (signed char) -1);
  printf ("valueof ((int) (unsigned char) -1) == %d\n", (int) (unsigned char) -1);

  return 0;
}
