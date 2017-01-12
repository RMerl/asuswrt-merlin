#include "orconfig.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

#include "crypto.h"
#include "compat.h"
#include "util.h"

static unsigned fill_a_buffer_memset(void) __attribute__((noinline));
static unsigned fill_a_buffer_memwipe(void) __attribute__((noinline));
static unsigned fill_a_buffer_nothing(void) __attribute__((noinline));
static unsigned fill_heap_buffer_memset(void) __attribute__((noinline));
static unsigned fill_heap_buffer_memwipe(void) __attribute__((noinline));
static unsigned fill_heap_buffer_nothing(void) __attribute__((noinline));
static unsigned check_a_buffer(void) __attribute__((noinline));

extern const char *s; /* Make the linkage global */
const char *s = NULL;

#define BUF_LEN 2048

#define FILL_BUFFER_IMPL()                                              \
  unsigned int i;                                                       \
  unsigned sum = 0;                                                     \
                                                                        \
  /* Fill up a 1k buffer with a recognizable pattern. */                \
  for (i = 0; i < BUF_LEN; i += strlen(s)) {                            \
    memcpy(buf+i, s, MIN(strlen(s), BUF_LEN-i));                        \
  }                                                                     \
                                                                        \
  /* Use the buffer as input to a computation so the above can't get */ \
  /* optimized away. */                                                 \
  for (i = 0; i < BUF_LEN; ++i) {                                       \
    sum += (unsigned char)buf[i];                                       \
  }

#ifdef __OpenBSD__
/* Disable some of OpenBSD's malloc protections for this test. This helps
 * us do bad things, such as access freed buffers, without crashing. */
const char *malloc_options="sufjj";
#endif

static unsigned
fill_a_buffer_memset(void)
{
  char buf[BUF_LEN];
  FILL_BUFFER_IMPL()
  memset(buf, 0, sizeof(buf));
  return sum;
}

static unsigned
fill_a_buffer_memwipe(void)
{
  char buf[BUF_LEN];
  FILL_BUFFER_IMPL()
  memwipe(buf, 0, sizeof(buf));
  return sum;
}

static unsigned
fill_a_buffer_nothing(void)
{
  char buf[BUF_LEN];
  FILL_BUFFER_IMPL()
  return sum;
}

static inline int
vmemeq(volatile char *a, const char *b, size_t n)
{
  while (n--) {
    if (*a++ != *b++)
      return 0;
  }
  return 1;
}

static unsigned
check_a_buffer(void)
{
  unsigned int i;
  volatile char buf[1024];
  unsigned sum = 0;

  /* See if this buffer has the string in it.

     YES, THIS DOES INVOKE UNDEFINED BEHAVIOR BY READING FROM AN UNINITIALIZED
     BUFFER.

     If you know a better way to figure out whether the compiler eliminated
     the memset/memwipe calls or not, please let me know.
   */
  for (i = 0; i < BUF_LEN - strlen(s); ++i) {
    if (vmemeq(buf+i, s, strlen(s)))
      ++sum;
  }

  return sum;
}

static char *heap_buf = NULL;

static unsigned
fill_heap_buffer_memset(void)
{
  char *buf = heap_buf = raw_malloc(BUF_LEN);
  FILL_BUFFER_IMPL()
  memset(buf, 0, BUF_LEN);
  raw_free(buf);
  return sum;
}

static unsigned
fill_heap_buffer_memwipe(void)
{
  char *buf = heap_buf = raw_malloc(BUF_LEN);
  FILL_BUFFER_IMPL()
  memwipe(buf, 0, BUF_LEN);
  raw_free(buf);
  return sum;
}

static unsigned
fill_heap_buffer_nothing(void)
{
  char *buf = heap_buf = raw_malloc(BUF_LEN);
  FILL_BUFFER_IMPL()
  raw_free(buf);
  return sum;
}

static unsigned
check_heap_buffer(void)
{
  unsigned int i;
  unsigned sum = 0;
  volatile char *buf = heap_buf;

  /* See if this buffer has the string in it.

     YES, THIS DOES INVOKE UNDEFINED BEHAVIOR BY READING FROM A FREED BUFFER.

     If you know a better way to figure out whether the compiler eliminated
     the memset/memwipe calls or not, please let me know.
   */
  for (i = 0; i < BUF_LEN - strlen(s); ++i) {
    if (vmemeq(buf+i, s, strlen(s)))
      ++sum;
  }

  return sum;
}

static struct testcase {
  const char *name;
  /* this spacing satisfies make check-spaces */
  unsigned
    (*fill_fn)(void);
  unsigned
    (*check_fn)(void);
} testcases[] = {
  { "nil", fill_a_buffer_nothing, check_a_buffer },
  { "nil-heap", fill_heap_buffer_nothing, check_heap_buffer },
  { "memset", fill_a_buffer_memset, check_a_buffer },
  { "memset-heap", fill_heap_buffer_memset, check_heap_buffer },
  { "memwipe", fill_a_buffer_memwipe, check_a_buffer },
  { "memwipe-heap", fill_heap_buffer_memwipe, check_heap_buffer },
  { NULL, NULL, NULL }
};

int
main(int argc, char **argv)
{
  unsigned x, x2;
  int i;
  int working = 1;
  unsigned found[6];
  (void) argc; (void) argv;

  s = "squamous haberdasher gallimaufry";

  memset(found, 0, sizeof(found));

  for (i = 0; testcases[i].name; ++i) {
    x = testcases[i].fill_fn();
    found[i] = testcases[i].check_fn();

    x2 = fill_a_buffer_nothing();

    if (x != x2) {
      working = 0;
    }
  }

  if (!working || !found[0] || !found[1]) {
    printf("It appears that this test case may not give you reliable "
           "information. Sorry.\n");
  }

  if (!found[2] && !found[3]) {
    printf("It appears that memset is good enough on this platform. Good.\n");
  }

  if (found[4] || found[5]) {
    printf("ERROR: memwipe does not wipe data!\n");
    return 1;
  } else {
    printf("OKAY: memwipe seems to work.\n");
    return 0;
  }
}

