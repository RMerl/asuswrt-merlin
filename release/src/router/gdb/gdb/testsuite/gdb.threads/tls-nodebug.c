/* Test accessing TLS based variable without any debug info compiled.  */

#include <pthread.h>

__thread int thread_local = 42;

int main(void)
{
  return 0;
}
