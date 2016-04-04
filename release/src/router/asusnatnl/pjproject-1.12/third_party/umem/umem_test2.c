#include <stdio.h>
#include <string.h>

#include "umem.h"

static const char *TESTSTRINGS[] = {
  "fred",
  "fredfredfred",
  "thisisabitlongerthantheotherstrings",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890",
};

#define N_TESTSTRINGS (sizeof(TESTSTRINGS) / sizeof(TESTSTRINGS[0]))
#define N_TESTS 1000

int
main (int argc, char *argv[])
{
  char *testcases[N_TESTSTRINGS][N_TESTS + 1];
  size_t len[N_TESTSTRINGS];
  int i, j;

  memset(testcases, 0, sizeof(testcases));
  
  umem_startup(NULL, 0, 0, NULL, NULL);

  for (i = 0; i < N_TESTSTRINGS; ++i)
  {
    len[i] = strlen(TESTSTRINGS[i]) + 1;
  }

  puts("Allocating...");

  for (j = 0; j < N_TESTS; ++j)
  {
    for (i = 0; i < N_TESTSTRINGS; ++i)
    {
      testcases[i][j] = umem_alloc(len[i], UMEM_DEFAULT);
      strcpy(testcases[i][j], TESTSTRINGS[i]);
    }
  }

  puts("Deallocating...");

  for (j = 0; j < N_TESTS; ++j)
  {
    for (i = N_TESTSTRINGS - 1; i >= 0; --i)
    {
      umem_free(testcases[i][j], len[i]);
    }

    if ((j % 25) == 0)
    {
      puts("Reaping...");
      umem_reap();
    }
  }

  puts("Done");

  return 0;
}
