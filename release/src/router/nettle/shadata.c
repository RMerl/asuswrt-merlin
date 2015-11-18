#include <math.h>
#include <stdio.h>

static const unsigned primes[64] =
{
  2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 
  31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 
  73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 
  127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 
  179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 
  233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 
  283, 293, 307, 311
};

int main(int argc, char **argv)
{
  int i;
  static const double third = 1.0/3;

  printf("SHA-256 constants: \n");
  for (i = 0; i < 64; )
    {
      double root = pow(primes[i++], third);
      double fraction = root - floor(root);
      double value = floor(ldexp(fraction, 32));

      printf("0x%08lxUL, ", (unsigned long) value);
      if (!(i % 4))
	printf("\n");
    }

  printf("\nSHA-256 initial values: \n");

  for (i = 0; i < 8; )
    {
      double root = pow(primes[i++], 0.5);
      double fraction = root - (floor(root));
      double value = floor(ldexp(fraction, 32));

      printf("0x%08lxUL, ", (unsigned long) value);
      if (!(i % 4))
	printf("\n");
    }
  
  return 0;
}
