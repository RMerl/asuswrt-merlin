/* Added for Tor. */
#include "crypto.h"
#define randombytes(b, n) \
  (crypto_strongest_rand((b), (n)))
