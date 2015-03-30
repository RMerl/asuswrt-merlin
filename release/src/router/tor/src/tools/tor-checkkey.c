/* Copyright (c) 2008-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include "crypto.h"
#include "torlog.h"
#include "../common/util.h"
#include "compat.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>

int
main(int c, char **v)
{
  crypto_pk_t *env;
  char *str;
  RSA *rsa;
  int wantdigest=0;
  int fname_idx;
  char *fname=NULL;
  init_logging();

  if (c < 2) {
    fprintf(stderr, "Hi. I'm tor-checkkey.  Tell me a filename that "
            "has a PEM-encoded RSA public key (like in a cert) and I'll "
            "dump the modulus.  Use the --digest option too and I'll "
            "dump the digest.\n");
    return 1;
  }

  if (crypto_global_init(0, NULL, NULL)) {
    fprintf(stderr, "Couldn't initialize crypto library.\n");
    return 1;
  }

  if (!strcmp(v[1], "--digest")) {
    wantdigest = 1;
    fname_idx = 2;
    if (c<3) {
      fprintf(stderr, "too few arguments");
      return 1;
    }
  } else {
    wantdigest = 0;
    fname_idx = 1;
  }

  fname = expand_filename(v[fname_idx]);
  str = read_file_to_str(fname, 0, NULL);
  tor_free(fname);
  if (!str) {
    fprintf(stderr, "Couldn't read %s\n", v[fname_idx]);
    return 1;
  }

  env = crypto_pk_new();
  if (crypto_pk_read_public_key_from_string(env, str, strlen(str))<0) {
    fprintf(stderr, "Couldn't parse key.\n");
    return 1;
  }
  tor_free(str);

  if (wantdigest) {
    char digest[HEX_DIGEST_LEN+1];
    if (crypto_pk_get_fingerprint(env, digest, 0)<0)
      return 1;
    printf("%s\n",digest);
  } else {
    rsa = crypto_pk_get_rsa_(env);
    str = BN_bn2hex(rsa->n);

    printf("%s\n", str);
  }

  return 0;
}

