#include "testutils.h"
#include "yarrow.h"
#include "knuth-lfib.h"

#include "macros.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lagged fibonacci sequence as described in Knuth 3.6 */

struct knuth_lfib_ctx lfib;

static int
get_event(FILE *f, struct sha256_ctx *hash,
          unsigned *key, unsigned *time)
{
  static int t = 0;
  uint8_t buf[1];
  
  int c = getc(f);
  if (c == EOF)
    return 0;

  buf[0] = c;
  sha256_update(hash, sizeof(buf), buf);
    
  *key = c;

  t += (knuth_lfib_get(&lfib) % 10000);
  *time = t;

  return 1;
}

static FILE *
open_file(const char *name)
{
  /* Tries opening the file in $srcdir, if set, otherwise the current
   * working directory */

  const char *srcdir = getenv("srcdir");
  if (srcdir && srcdir[0])
    {
      FILE *f;
      char *buf = xalloc(strlen(name) + strlen(srcdir) + 10);
      sprintf(buf, "%s/%s", srcdir, name);

      f = fopen(buf, "r");
      free(buf);
      return f;
    }

  /* Opens the file in text mode. */
  return fopen(name, "r");
}

void
test_main(void)
{
  FILE *input;
  
  struct yarrow256_ctx yarrow;
  struct yarrow_key_event_ctx estimator;

  struct yarrow_source sources[2];

  struct sha256_ctx output_hash;
  struct sha256_ctx input_hash;
  uint8_t digest[SHA256_DIGEST_SIZE];

  uint8_t seed_file[YARROW256_SEED_FILE_SIZE];

  const uint8_t *expected_output
    = H("dd304aacac3dc95e 70d684a642967c89"
	"58501f7c8eb88b79 43b2ffccde6f0f79");

  const uint8_t *expected_input
    = H("e0596cf006025506 65d1195f32a87e4a"
	"5c354910dfbd0a31 e2105b262f5ce3d8");

  const uint8_t *expected_seed_file
    = H("b03518f32b1084dd 983e6a445d47bb6f"
	"13bb7b998740d570 503d6aaa62e28901");
  
  unsigned c; unsigned t;

  unsigned processed = 0;
  unsigned output = 0;

  unsigned i;
  
  static const char zeroes[100];

  yarrow256_init(&yarrow, 2, sources);
  
  yarrow_key_event_init(&estimator);
  sha256_init(&input_hash);
  sha256_init(&output_hash);

  knuth_lfib_init(&lfib, 31416);

  /* Fake input to source 0 */
  yarrow256_update(&yarrow, 0, 200, sizeof(zeroes), zeroes);

  if (verbose)
    printf("source 0 entropy: %d\n",
	   sources[0].estimate[YARROW_SLOW]);
  
  ASSERT(!yarrow256_is_seeded(&yarrow));

  input = open_file("gold-bug.txt");

  if (!input)
    {
      fprintf(stderr, "Couldn't open `gold-bug.txt', errno = %d\n",
              errno);
      FAIL();
    }
  
  while (get_event(input, &input_hash, &c, &t))
    {
      uint8_t buf[8];

      processed++;
      
      WRITE_UINT32(buf, c);
      WRITE_UINT32(buf + 4, t);
      yarrow256_update(&yarrow, 1,
                       yarrow_key_event_estimate(&estimator, c, t),
                       sizeof(buf), buf);

      if (yarrow256_is_seeded(&yarrow))
        {
          static const unsigned sizes[4] = { 1, 16, 500, 37 };
          unsigned size = sizes[processed % 4];
          
          uint8_t buf[500];

          if (verbose && !output)
            printf("Generator was seeded after %d events\n",
		   processed);
          
          yarrow256_random(&yarrow, size, buf);

          sha256_update(&output_hash, size, buf);

	  if (verbose)
	    {
	      printf("%02x ", buf[0]);
	      if (! (processed % 16))
		printf("\n");
	    }
          output += size;
        }
    }

  fclose(input);

  if (verbose)
    {
      printf("\n");
      
      for (i = 0; i<2; i++)
	printf("source %d, (fast, slow) entropy: (%d, %d)\n",
	       i,
	       sources[i].estimate[YARROW_FAST],
	       sources[i].estimate[YARROW_SLOW]); 
      
      printf("Processed input: %d octets\n", processed);
      printf("         sha256:");
    }

  sha256_digest(&input_hash, sizeof(digest), digest);

  if (verbose)
    {
      print_hex(sizeof(digest), digest);
      printf("\n");
    }
  
  ASSERT (memcmp(digest, expected_input, sizeof(digest)) == 0);

  yarrow256_random(&yarrow, sizeof(seed_file), seed_file);
  if (verbose)
    {
      printf("New seed file: ");
      print_hex(sizeof(seed_file), seed_file);
      printf("\n");
    }

  ASSERT (memcmp(seed_file, expected_seed_file, sizeof(seed_file)) == 0);
  
  if (verbose)
    {
      printf("Generated output: %d octets\n", output);
      printf("          sha256:");
    }
  
  sha256_digest(&output_hash, sizeof(digest), digest);

  if (verbose)
    {
      print_hex(sizeof(digest), digest);
      printf("\n");
    }
  
  ASSERT (memcmp(digest, expected_output, sizeof(digest)) == 0);
}
