#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if 1
# define BYTE_FORMAT "0x%02x"
# define BYTE_COLUMNS 8
#else
# define BYTE_FORMAT "%3d"
# define BYTE_COLUMNS 0x10
#endif

#define WORD_FORMAT "0x%08lx"
#define WORD_COLUMNS 4

unsigned char sbox[0x100];
unsigned char isbox[0x100];

unsigned char gf2_log[0x100];
unsigned char gf2_exp[0x100];

unsigned long dtable[4][0x100];
unsigned long itable[4][0x100];
unsigned long mtable[4][0x100];

static unsigned
xtime(unsigned x)
{
  assert (x < 0x100);

  x <<= 1;
  if (x & 0x100)
    x ^= 0x11b;

  assert (x < 0x100);

  return x;
}

/* Computes the exponentiatiom and logarithm tables for GF_2, to the
 * base x+1 (0x03). The unit element is 1 (0x01).*/
static void
compute_log(void)
{
  unsigned i = 0;
  unsigned x = 1;

  memset(gf2_log, 0, 0x100);
  
  for (i = 0; i < 0x100; i++, x = x ^ xtime(x))
    {
      gf2_exp[i] = x;
      gf2_log[x] = i;
    }
  /* Invalid. */
  gf2_log[0] = 0;
  /* The loop above sets gf2_log[1] = 0xff, which is correct,
   * but gf2_log[1] = 0 is nicer. */
  gf2_log[1] = 0;
}

static unsigned
mult(unsigned a, unsigned b)
{
  return (a && b) ? gf2_exp[ (gf2_log[a] + gf2_log[b]) % 255] : 0;
}

static unsigned
invert(unsigned x)
{
  return x ? gf2_exp[0xff - gf2_log[x]] : 0;
}

static unsigned
affine(unsigned x)
{
  return 0xff &
    (0x63^x^(x>>4)^(x<<4)^(x>>5)^(x<<3)^(x>>6)^(x<<2)^(x>>7)^(x<<1));
}
     
static void
compute_sbox(void)
{
  unsigned i;
  for (i = 0; i<0x100; i++)
    {
      sbox[i] = affine(invert(i));
      isbox[sbox[i]] = i;
    }
}

/* Generate little endian tables, i.e. the first row of the AES state
 * arrays occupies the least significant byte of the words.
 *
 * The sbox values are multiplied with the column of GF2 coefficients
 * of the polynomial 03 x^3 + x^2 + x + 02. */
static void
compute_dtable(void)
{
  unsigned i;
  for (i = 0; i<0x100; i++)
    {
      unsigned s = sbox[i];
      unsigned j;
      unsigned long t  =( ( (s ^ xtime(s)) << 24)
		     | (s << 16) | (s << 8)
		     | xtime(s) );

      for (j = 0; j<4; j++, t = (t << 8) | (t >> 24))
	dtable[j][i] = t;
    }
}

/* The inverse sbox values are multiplied with the column of GF2 coefficients
 * of the polynomial inverse 0b x^3 + 0d x^2 + 09 x + 0e. */
static void
compute_itable(void)
{
  unsigned i;
  for (i = 0; i<0x100; i++)
    {
      unsigned s = isbox[i];
      unsigned j;
      unsigned long t = ( (mult(s, 0xb) << 24)
			| (mult(s, 0xd) << 16)
			| (mult(s, 0x9) << 8)
			| (mult(s, 0xe) ));
      
      for (j = 0; j<4; j++, t = (t << 8) | (t >> 24))
	itable[j][i] = t;
    }
}

/* Used for key inversion, inverse mix column. No sbox. */
static void
compute_mtable(void)
{
  unsigned i;
  for (i = 0; i<0x100; i++)
    {
      unsigned j;
      unsigned long t = ( (mult(i, 0xb) << 24)
			| (mult(i, 0xd) << 16)
			| (mult(i, 0x9) << 8)
			| (mult(i, 0xe) ));
      
      for (j = 0; j<4; j++, t = (t << 8) | (t >> 24))
	mtable[j][i] = t;
    }
}

static void
display_byte_table(const char *name, unsigned char *table)
{
  unsigned i, j;

  printf("uint8_t %s[0x100] =\n{", name);

  for (i = 0; i<0x100; i+= BYTE_COLUMNS)
    {
      printf("\n  ");
      for (j = 0; j<BYTE_COLUMNS; j++)
	printf(BYTE_FORMAT ",", table[i + j]);
    }

  printf("\n};\n\n");
}

static void
display_table(const char *name, unsigned long table[][0x100])
{
  unsigned i, j, k;
  
  printf("uint32_t %s[4][0x100] =\n{\n  ", name);

  for (k = 0; k<4; k++)
    {
      printf("{ ");
      for (i = 0; i<0x100; i+= WORD_COLUMNS)
	{
	  printf("\n    ");
	  for (j = 0; j<WORD_COLUMNS; j++)
	    printf(WORD_FORMAT ",", table[k][i + j]);
	}
      printf("\n  },");
    }
  printf("\n};\n\n");
}

static void
display_polynomial(const unsigned *p)
{
  printf("(%x x^3 + %x x^2 + %x x + %x)",
	 p[3], p[2], p[1], p[0]);
}

int
main(int argc, char **argv)
{
  compute_log();
  if (argc == 1)
    {
      display_byte_table("gf2_log", gf2_log);
      display_byte_table("gf2_exp", gf2_exp);

      compute_sbox();
      display_byte_table("sbox", sbox);
      display_byte_table("isbox", isbox);

      compute_dtable();
      display_table("dtable", dtable);

      compute_itable();
      display_table("itable", itable);

      compute_mtable();
      display_table("mtable", mtable);

      return 0;
    }
  else if (argc == 2)
    {
      unsigned a;
      for (a = 1; a<0x100; a++)
	{
	  unsigned a1 = invert(a);
	  unsigned b;
	  unsigned u;
	  if (a1 == 0)
	    printf("invert(%x) = 0 !\n", a);

	  u = mult(a, a1);
	  if (u != 1)
	    printf("invert(%x) = %x; product = %x\n",
		   a, a1, u);
	  
	  for (b = 1; b<0x100; b++)
	    {
	      unsigned b1 = invert(b);
	      unsigned c = mult(a, b);

	      if (c == 0)
		printf("%x x %x = 0\n", a, b);

	      u = mult(c, a1);
	      if (u != b)
		printf("%x x %x = %x, invert(%x) = %x, %x x %x = %x\n",
		       a, b, c, a, a1, c, a1, u);
	      
	      u = mult(c, b1);
	      if (u != a)
		printf("%x x %x = %x, invert(%x) = %x, %x x %x = %x\n",
		       a, b, c, b, b1, c, b1, u);
	    }
	}
      return 0;
    }
  else if (argc == 4)
    {
      unsigned a, b, c;
      int op = argv[2][0];
      a = strtoul(argv[1], NULL, 16);
      b = strtoul(argv[3], NULL, 16);
      switch (op)
	{
	case '+':
	  c = a ^ b;
	  break;
	case '*':
	case 'x':
	  c = mult(a,b);
	  break;
	case '/':
	  c = mult(a, invert(b));
	  break;
	default:
	  return 1;
	}
      printf("%x %c %x = %x\n", a, op, b, c);
      return 0;
    }
#if 0
  else if (argc == 5)
    {
      /* Compute gcd(a, x^4+1) */
      unsigned d[4];
      unsigned u[4];
      
      for (i = 0; i<4; i++)
	a[i] = strtoul(argv[1+i], NULL, 16);
    }
#endif
  else if (argc == 9)
    {
      unsigned a[4];
      unsigned b[4];
      unsigned c[4];
      unsigned i;
      for (i = 0; i<4; i++)
	{
	  a[i] = strtoul(argv[1+i], NULL, 16);
	  b[i] = strtoul(argv[5+i], NULL, 16);
	}

      c[0] = mult(a[0],b[0])^mult(a[3],b[1])^mult(a[2],b[2])^mult(a[1],b[3]);
      c[1] = mult(a[1],b[0])^mult(a[0],b[1])^mult(a[3],b[2])^mult(a[2],b[3]);
      c[2] = mult(a[2],b[0])^mult(a[1],b[1])^mult(a[0],b[2])^mult(a[3],b[3]);
      c[3] = mult(a[3],b[0])^mult(a[2],b[1])^mult(a[1],b[2])^mult(a[0],b[3]);

      display_polynomial(a); printf(" * "); display_polynomial(b);
      printf(" = "); display_polynomial(c); printf("\n");
    }
  return 1;
}
