/* pubkey.c  -	pubkey dispatcher
 * Copyright (C) 1998, 1999, 2000, 2002, 2003, 2005,
 *               2007, 2008, 2011 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "mpi.h"
#include "cipher.h"
#include "ath.h"


static gcry_err_code_t pubkey_decrypt (int algo, gcry_mpi_t *result,
                                       gcry_mpi_t *data, gcry_mpi_t *skey,
                                       int flags);
static gcry_err_code_t pubkey_sign (int algo, gcry_mpi_t *resarr,
                                    gcry_mpi_t hash, gcry_mpi_t *skey);
static gcry_err_code_t pubkey_verify (int algo, gcry_mpi_t hash,
                                      gcry_mpi_t *data, gcry_mpi_t *pkey,
				     int (*cmp) (void *, gcry_mpi_t),
                                      void *opaque);


/* A dummy extraspec so that we do not need to tests the extraspec
   field from the module specification against NULL and instead
   directly test the respective fields of extraspecs.  */
static pk_extra_spec_t dummy_extra_spec;


/* This is the list of the default public-key ciphers included in
   libgcrypt.  FIPS_ALLOWED indicated whether the algorithm is used in
   FIPS mode. */
static struct pubkey_table_entry
{
  gcry_pk_spec_t *pubkey;
  pk_extra_spec_t *extraspec;
  unsigned int algorithm;
  int fips_allowed;
} pubkey_table[] =
  {
#if USE_RSA
    { &_gcry_pubkey_spec_rsa,
      &_gcry_pubkey_extraspec_rsa,   GCRY_PK_RSA, 1},
#endif
#if USE_ELGAMAL
    { &_gcry_pubkey_spec_elg,
      &_gcry_pubkey_extraspec_elg,    GCRY_PK_ELG   },
    { &_gcry_pubkey_spec_elg,
      &_gcry_pubkey_extraspec_elg,    GCRY_PK_ELG_E },
#endif
#if USE_DSA
    { &_gcry_pubkey_spec_dsa,
      &_gcry_pubkey_extraspec_dsa,   GCRY_PK_DSA, 1   },
#endif
#if USE_ECC
    { &_gcry_pubkey_spec_ecdsa,
      &_gcry_pubkey_extraspec_ecdsa, GCRY_PK_ECDSA, 0 },
    { &_gcry_pubkey_spec_ecdh,
      &_gcry_pubkey_extraspec_ecdsa, GCRY_PK_ECDH, 0 },
#endif
    { NULL, 0 },
  };

/* List of registered ciphers.  */
static gcry_module_t pubkeys_registered;

/* This is the lock protecting PUBKEYS_REGISTERED.  */
static ath_mutex_t pubkeys_registered_lock = ATH_MUTEX_INITIALIZER;;

/* Flag to check whether the default pubkeys have already been
   registered.  */
static int default_pubkeys_registered;

/* Convenient macro for registering the default digests.  */
#define REGISTER_DEFAULT_PUBKEYS                   \
  do                                               \
    {                                              \
      ath_mutex_lock (&pubkeys_registered_lock);   \
      if (! default_pubkeys_registered)            \
        {                                          \
          pk_register_default ();                  \
          default_pubkeys_registered = 1;          \
        }                                          \
      ath_mutex_unlock (&pubkeys_registered_lock); \
    }                                              \
  while (0)

/* These dummy functions are used in case a cipher implementation
   refuses to provide it's own functions.  */

static gcry_err_code_t
dummy_generate (int algorithm, unsigned int nbits, unsigned long dummy,
                gcry_mpi_t *skey, gcry_mpi_t **retfactors)
{
  (void)algorithm;
  (void)nbits;
  (void)dummy;
  (void)skey;
  (void)retfactors;
  fips_signal_error ("using dummy public key function");
  return GPG_ERR_NOT_IMPLEMENTED;
}

static gcry_err_code_t
dummy_check_secret_key (int algorithm, gcry_mpi_t *skey)
{
  (void)algorithm;
  (void)skey;
  fips_signal_error ("using dummy public key function");
  return GPG_ERR_NOT_IMPLEMENTED;
}

static gcry_err_code_t
dummy_encrypt (int algorithm, gcry_mpi_t *resarr, gcry_mpi_t data,
               gcry_mpi_t *pkey, int flags)
{
  (void)algorithm;
  (void)resarr;
  (void)data;
  (void)pkey;
  (void)flags;
  fips_signal_error ("using dummy public key function");
  return GPG_ERR_NOT_IMPLEMENTED;
}

static gcry_err_code_t
dummy_decrypt (int algorithm, gcry_mpi_t *result, gcry_mpi_t *data,
               gcry_mpi_t *skey, int flags)
{
  (void)algorithm;
  (void)result;
  (void)data;
  (void)skey;
  (void)flags;
  fips_signal_error ("using dummy public key function");
  return GPG_ERR_NOT_IMPLEMENTED;
}

static gcry_err_code_t
dummy_sign (int algorithm, gcry_mpi_t *resarr, gcry_mpi_t data,
            gcry_mpi_t *skey)
{
  (void)algorithm;
  (void)resarr;
  (void)data;
  (void)skey;
  fips_signal_error ("using dummy public key function");
  return GPG_ERR_NOT_IMPLEMENTED;
}

static gcry_err_code_t
dummy_verify (int algorithm, gcry_mpi_t hash, gcry_mpi_t *data,
              gcry_mpi_t *pkey,
	      int (*cmp) (void *, gcry_mpi_t), void *opaquev)
{
  (void)algorithm;
  (void)hash;
  (void)data;
  (void)pkey;
  (void)cmp;
  (void)opaquev;
  fips_signal_error ("using dummy public key function");
  return GPG_ERR_NOT_IMPLEMENTED;
}

static unsigned
dummy_get_nbits (int algorithm, gcry_mpi_t *pkey)
{
  (void)algorithm;
  (void)pkey;
  fips_signal_error ("using dummy public key function");
  return 0;
}

/* Internal function.  Register all the pubkeys included in
   PUBKEY_TABLE.  Returns zero on success or an error code.  */
static void
pk_register_default (void)
{
  gcry_err_code_t err = 0;
  int i;

  for (i = 0; (! err) && pubkey_table[i].pubkey; i++)
    {
#define pubkey_use_dummy(func)                       \
      if (! pubkey_table[i].pubkey->func)            \
	pubkey_table[i].pubkey->func = dummy_##func;

      pubkey_use_dummy (generate);
      pubkey_use_dummy (check_secret_key);
      pubkey_use_dummy (encrypt);
      pubkey_use_dummy (decrypt);
      pubkey_use_dummy (sign);
      pubkey_use_dummy (verify);
      pubkey_use_dummy (get_nbits);
#undef pubkey_use_dummy

      err = _gcry_module_add (&pubkeys_registered,
			      pubkey_table[i].algorithm,
			      (void *) pubkey_table[i].pubkey,
			      (void *) pubkey_table[i].extraspec,
                              NULL);
    }

  if (err)
    BUG ();
}

/* Internal callback function.  Used via _gcry_module_lookup.  */
static int
gcry_pk_lookup_func_name (void *spec, void *data)
{
  gcry_pk_spec_t *pubkey = (gcry_pk_spec_t *) spec;
  char *name = (char *) data;
  const char **aliases = pubkey->aliases;
  int ret = stricmp (name, pubkey->name);

  while (ret && *aliases)
    ret = stricmp (name, *aliases++);

  return ! ret;
}

/* Internal function.  Lookup a pubkey entry by it's name.  */
static gcry_module_t
gcry_pk_lookup_name (const char *name)
{
  gcry_module_t pubkey;

  pubkey = _gcry_module_lookup (pubkeys_registered, (void *) name,
				gcry_pk_lookup_func_name);

  return pubkey;
}

/* Register a new pubkey module whose specification can be found in
   PUBKEY.  On success, a new algorithm ID is stored in ALGORITHM_ID
   and a pointer representhing this module is stored in MODULE.  */
gcry_error_t
_gcry_pk_register (gcry_pk_spec_t *pubkey,
                   pk_extra_spec_t *extraspec,
                   unsigned int *algorithm_id,
                   gcry_module_t *module)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;
  gcry_module_t mod;

  /* We do not support module loading in fips mode.  */
  if (fips_mode ())
    return gpg_error (GPG_ERR_NOT_SUPPORTED);

  ath_mutex_lock (&pubkeys_registered_lock);
  err = _gcry_module_add (&pubkeys_registered, 0,
			  (void *) pubkey,
			  (void *)(extraspec? extraspec : &dummy_extra_spec),
                          &mod);
  ath_mutex_unlock (&pubkeys_registered_lock);

  if (! err)
    {
      *module = mod;
      *algorithm_id = mod->mod_id;
    }

  return err;
}

/* Unregister the pubkey identified by ID, which must have been
   registered with gcry_pk_register.  */
void
gcry_pk_unregister (gcry_module_t module)
{
  ath_mutex_lock (&pubkeys_registered_lock);
  _gcry_module_release (module);
  ath_mutex_unlock (&pubkeys_registered_lock);
}

static void
release_mpi_array (gcry_mpi_t *array)
{
  for (; *array; array++)
    {
      mpi_free(*array);
      *array = NULL;
    }
}

/****************
 * Map a string to the pubkey algo
 */
int
gcry_pk_map_name (const char *string)
{
  gcry_module_t pubkey;
  int algorithm = 0;

  if (!string)
    return 0;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = gcry_pk_lookup_name (string);
  if (pubkey)
    {
      algorithm = pubkey->mod_id;
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return algorithm;
}


/* Map the public key algorithm whose ID is contained in ALGORITHM to
   a string representation of the algorithm name.  For unknown
   algorithm IDs this functions returns "?". */
const char *
gcry_pk_algo_name (int algorithm)
{
  gcry_module_t pubkey;
  const char *name;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      name = ((gcry_pk_spec_t *) pubkey->spec)->name;
      _gcry_module_release (pubkey);
    }
  else
    name = "?";
  ath_mutex_unlock (&pubkeys_registered_lock);

  return name;
}


/* A special version of gcry_pk_algo name to return the first aliased
   name of the algorithm.  This is required to adhere to the spki
   specs where the algorithm names are lowercase. */
const char *
_gcry_pk_aliased_algo_name (int algorithm)
{
  const char *name = NULL;
  gcry_module_t module;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (module)
    {
      gcry_pk_spec_t *pubkey = (gcry_pk_spec_t *) module->spec;

      name = pubkey->aliases? *pubkey->aliases : NULL;
      if (!name || !*name)
        name = pubkey->name;
      _gcry_module_release (module);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return name;
}


static void
disable_pubkey_algo (int algorithm)
{
  gcry_module_t pubkey;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      if (! (pubkey-> flags & FLAG_MODULE_DISABLED))
	pubkey->flags |= FLAG_MODULE_DISABLED;
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);
}


/****************
 * A USE of 0 means: don't care.
 */
static gcry_err_code_t
check_pubkey_algo (int algorithm, unsigned use)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;
  gcry_pk_spec_t *pubkey;
  gcry_module_t module;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (module)
    {
      pubkey = (gcry_pk_spec_t *) module->spec;

      if (((use & GCRY_PK_USAGE_SIGN)
	   && (! (pubkey->use & GCRY_PK_USAGE_SIGN)))
	  || ((use & GCRY_PK_USAGE_ENCR)
	      && (! (pubkey->use & GCRY_PK_USAGE_ENCR))))
	err = GPG_ERR_WRONG_PUBKEY_ALGO;
      else if (module->flags & FLAG_MODULE_DISABLED)
	err = GPG_ERR_PUBKEY_ALGO;
      _gcry_module_release (module);
    }
  else
    err = GPG_ERR_PUBKEY_ALGO;
  ath_mutex_unlock (&pubkeys_registered_lock);

  return err;
}


/****************
 * Return the number of public key material numbers
 */
static int
pubkey_get_npkey (int algorithm)
{
  gcry_module_t pubkey;
  int npkey = 0;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      npkey = strlen (((gcry_pk_spec_t *) pubkey->spec)->elements_pkey);
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return npkey;
}

/****************
 * Return the number of secret key material numbers
 */
static int
pubkey_get_nskey (int algorithm)
{
  gcry_module_t pubkey;
  int nskey = 0;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      nskey = strlen (((gcry_pk_spec_t *) pubkey->spec)->elements_skey);
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return nskey;
}

/****************
 * Return the number of signature material numbers
 */
static int
pubkey_get_nsig (int algorithm)
{
  gcry_module_t pubkey;
  int nsig = 0;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      nsig = strlen (((gcry_pk_spec_t *) pubkey->spec)->elements_sig);
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return nsig;
}

/****************
 * Return the number of encryption material numbers
 */
static int
pubkey_get_nenc (int algorithm)
{
  gcry_module_t pubkey;
  int nenc = 0;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      nenc = strlen (((gcry_pk_spec_t *) pubkey->spec)->elements_enc);
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return nenc;
}


/* Generate a new public key with algorithm ALGORITHM of size NBITS
   and return it at SKEY.  USE_E depends on the ALGORITHM.  GENPARMS
   is passed to the algorithm module if it features an extended
   generation function.  RETFACTOR is used by some algorithms to
   return certain additional information which are in general not
   required.

   The function returns the error code number or 0 on success. */
static gcry_err_code_t
pubkey_generate (int algorithm,
                 unsigned int nbits,
                 unsigned long use_e,
                 gcry_sexp_t genparms,
                 gcry_mpi_t *skey, gcry_mpi_t **retfactors,
                 gcry_sexp_t *r_extrainfo)
{
  gcry_err_code_t ec = GPG_ERR_PUBKEY_ALGO;
  gcry_module_t pubkey;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      pk_extra_spec_t *extraspec = pubkey->extraspec;

      if (extraspec && extraspec->ext_generate)
        {
          /* Use the extended generate function.  */
          ec = extraspec->ext_generate
            (algorithm, nbits, use_e, genparms, skey, retfactors, r_extrainfo);
        }
      else
        {
          /* Use the standard generate function.  */
          ec = ((gcry_pk_spec_t *) pubkey->spec)->generate
            (algorithm, nbits, use_e, skey, retfactors);
        }
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return ec;
}


static gcry_err_code_t
pubkey_check_secret_key (int algorithm, gcry_mpi_t *skey)
{
  gcry_err_code_t err = GPG_ERR_PUBKEY_ALGO;
  gcry_module_t pubkey;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    {
      err = ((gcry_pk_spec_t *) pubkey->spec)->check_secret_key
        (algorithm, skey);
      _gcry_module_release (pubkey);
    }
  ath_mutex_unlock (&pubkeys_registered_lock);

  return err;
}


/****************
 * This is the interface to the public key encryption.  Encrypt DATA
 * with PKEY and put it into RESARR which should be an array of MPIs
 * of size PUBKEY_MAX_NENC (or less if the algorithm allows this -
 * check with pubkey_get_nenc() )
 */
static gcry_err_code_t
pubkey_encrypt (int algorithm, gcry_mpi_t *resarr, gcry_mpi_t data,
                gcry_mpi_t *pkey, int flags)
{
  gcry_pk_spec_t *pubkey;
  gcry_module_t module;
  gcry_err_code_t rc;
  int i;

  /* Note: In fips mode DBG_CIPHER will enver evaluate to true but as
     an extra failsafe protection we explicitly test for fips mode
     here. */
  if (DBG_CIPHER && !fips_mode ())
    {
      log_debug ("pubkey_encrypt: algo=%d\n", algorithm);
      for(i = 0; i < pubkey_get_npkey (algorithm); i++)
	log_mpidump ("  pkey:", pkey[i]);
      log_mpidump ("  data:", data);
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (module)
    {
      pubkey = (gcry_pk_spec_t *) module->spec;
      rc = pubkey->encrypt (algorithm, resarr, data, pkey, flags);
      _gcry_module_release (module);
      goto ready;
    }
  rc = GPG_ERR_PUBKEY_ALGO;

 ready:
  ath_mutex_unlock (&pubkeys_registered_lock);

  if (!rc && DBG_CIPHER && !fips_mode ())
    {
      for(i = 0; i < pubkey_get_nenc (algorithm); i++)
	log_mpidump("  encr:", resarr[i] );
    }
  return rc;
}


/****************
 * This is the interface to the public key decryption.
 * ALGO gives the algorithm to use and this implicitly determines
 * the size of the arrays.
 * result is a pointer to a mpi variable which will receive a
 * newly allocated mpi or NULL in case of an error.
 */
static gcry_err_code_t
pubkey_decrypt (int algorithm, gcry_mpi_t *result, gcry_mpi_t *data,
                gcry_mpi_t *skey, int flags)
{
  gcry_pk_spec_t *pubkey;
  gcry_module_t module;
  gcry_err_code_t rc;
  int i;

  *result = NULL; /* so the caller can always do a mpi_free */
  if (DBG_CIPHER && !fips_mode ())
    {
      log_debug ("pubkey_decrypt: algo=%d\n", algorithm);
      for(i = 0; i < pubkey_get_nskey (algorithm); i++)
	log_mpidump ("  skey:", skey[i]);
      for(i = 0; i < pubkey_get_nenc (algorithm); i++)
	log_mpidump ("  data:", data[i]);
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (module)
    {
      pubkey = (gcry_pk_spec_t *) module->spec;
      rc = pubkey->decrypt (algorithm, result, data, skey, flags);
      _gcry_module_release (module);
      goto ready;
    }

  rc = GPG_ERR_PUBKEY_ALGO;

 ready:
  ath_mutex_unlock (&pubkeys_registered_lock);

  if (!rc && DBG_CIPHER && !fips_mode ())
    log_mpidump (" plain:", *result);

  return rc;
}


/****************
 * This is the interface to the public key signing.
 * Sign data with skey and put the result into resarr which
 * should be an array of MPIs of size PUBKEY_MAX_NSIG (or less if the
 * algorithm allows this - check with pubkey_get_nsig() )
 */
static gcry_err_code_t
pubkey_sign (int algorithm, gcry_mpi_t *resarr, gcry_mpi_t data,
             gcry_mpi_t *skey)
{
  gcry_pk_spec_t *pubkey;
  gcry_module_t module;
  gcry_err_code_t rc;
  int i;

  if (DBG_CIPHER && !fips_mode ())
    {
      log_debug ("pubkey_sign: algo=%d\n", algorithm);
      for(i = 0; i < pubkey_get_nskey (algorithm); i++)
	log_mpidump ("  skey:", skey[i]);
      log_mpidump("  data:", data );
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (module)
    {
      pubkey = (gcry_pk_spec_t *) module->spec;
      rc = pubkey->sign (algorithm, resarr, data, skey);
      _gcry_module_release (module);
      goto ready;
    }

  rc = GPG_ERR_PUBKEY_ALGO;

 ready:
  ath_mutex_unlock (&pubkeys_registered_lock);

  if (!rc && DBG_CIPHER && !fips_mode ())
    for (i = 0; i < pubkey_get_nsig (algorithm); i++)
      log_mpidump ("   sig:", resarr[i]);

  return rc;
}

/****************
 * Verify a public key signature.
 * Return 0 if the signature is good
 */
static gcry_err_code_t
pubkey_verify (int algorithm, gcry_mpi_t hash, gcry_mpi_t *data,
               gcry_mpi_t *pkey,
	       int (*cmp)(void *, gcry_mpi_t), void *opaquev)
{
  gcry_pk_spec_t *pubkey;
  gcry_module_t module;
  gcry_err_code_t rc;
  int i;

  if (DBG_CIPHER && !fips_mode ())
    {
      log_debug ("pubkey_verify: algo=%d\n", algorithm);
      for (i = 0; i < pubkey_get_npkey (algorithm); i++)
	log_mpidump ("  pkey", pkey[i]);
      for (i = 0; i < pubkey_get_nsig (algorithm); i++)
	log_mpidump ("   sig", data[i]);
      log_mpidump ("  hash", hash);
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (module)
    {
      pubkey = (gcry_pk_spec_t *) module->spec;
      rc = pubkey->verify (algorithm, hash, data, pkey, cmp, opaquev);
      _gcry_module_release (module);
      goto ready;
    }

  rc = GPG_ERR_PUBKEY_ALGO;

 ready:
  ath_mutex_unlock (&pubkeys_registered_lock);
  return rc;
}


/* Turn VALUE into an octet string and store it in an allocated buffer
   at R_FRAME or - if R_RAME is NULL - copy it into the caller
   provided buffer SPACE; either SPACE or R_FRAME may be used.  If
   SPACE if not NULL, the caller must provide a buffer of at least
   NBYTES.  If the resulting octet string is shorter than NBYTES pad
   it to the left with zeroes.  If VALUE does not fit into NBYTES
   return an error code.  */
static gpg_err_code_t
octet_string_from_mpi (unsigned char **r_frame, void *space,
                       gcry_mpi_t value, size_t nbytes)
{
  gpg_err_code_t rc;
  size_t nframe, noff, n;
  unsigned char *frame;

  if (!r_frame == !space)
    return GPG_ERR_INV_ARG;  /* Only one may be used.  */

  if (r_frame)
    *r_frame = NULL;

  rc = gcry_err_code (gcry_mpi_print (GCRYMPI_FMT_USG,
                                      NULL, 0, &nframe, value));
  if (rc)
    return rc;
  if (nframe > nbytes)
    return GPG_ERR_TOO_LARGE; /* Value too long to fit into NBYTES.  */

  noff = (nframe < nbytes)? nbytes - nframe : 0;
  n = nframe + noff;
  if (space)
    frame = space;
  else
    {
      frame = mpi_is_secure (value)? gcry_malloc_secure (n) : gcry_malloc (n);
      if (!frame)
        {
          rc = gpg_err_code_from_syserror ();
          return rc;
        }
    }
  if (noff)
    memset (frame, 0, noff);
  nframe += noff;
  rc = gcry_err_code (gcry_mpi_print (GCRYMPI_FMT_USG,
                                      frame+noff, nframe-noff, NULL, value));
  if (rc)
    {
      gcry_free (frame);
      return rc;
    }

  if (r_frame)
    *r_frame = frame;
  return 0;
}


/* Encode {VALUE,VALUELEN} for an NBITS keys using the pkcs#1 block
   type 2 padding.  On sucess the result is stored as a new MPI at
   R_RESULT.  On error the value at R_RESULT is undefined.

   If {RANDOM_OVERRIDE, RANDOM_OVERRIDE_LEN} is given it is used as
   the seed instead of using a random string for it.  This feature is
   only useful for regression tests.  Note that this value may not
   contain zero bytes.

   We encode the value in this way:

     0  2  RND(n bytes)  0  VALUE

   0   is a marker we unfortunately can't encode because we return an
       MPI which strips all leading zeroes.
   2   is the block type.
   RND are non-zero random bytes.

   (Note that OpenPGP includes the cipher algorithm and a checksum in
   VALUE; the caller needs to prepare the value accordingly.)
  */
static gcry_err_code_t
pkcs1_encode_for_encryption (gcry_mpi_t *r_result, unsigned int nbits,
			     const unsigned char *value, size_t valuelen,
                             const unsigned char *random_override,
                             size_t random_override_len)
{
  gcry_err_code_t rc = 0;
  gcry_error_t err;
  unsigned char *frame = NULL;
  size_t nframe = (nbits+7) / 8;
  int i;
  size_t n;
  unsigned char *p;

  if (valuelen + 7 > nframe || !nframe)
    {
      /* Can't encode a VALUELEN value in a NFRAME bytes frame.  */
      return GPG_ERR_TOO_SHORT; /* The key is too short.  */
    }

  if ( !(frame = gcry_malloc_secure (nframe)))
    return gpg_err_code_from_syserror ();

  n = 0;
  frame[n++] = 0;
  frame[n++] = 2; /* block type */
  i = nframe - 3 - valuelen;
  gcry_assert (i > 0);

  if (random_override)
    {
      int j;

      if (random_override_len != i)
        {
          gcry_free (frame);
          return GPG_ERR_INV_ARG;
        }
      /* Check that random does not include a zero byte.  */
      for (j=0; j < random_override_len; j++)
        if (!random_override[j])
          {
            gcry_free (frame);
            return GPG_ERR_INV_ARG;
          }
      memcpy (frame + n, random_override, random_override_len);
      n += random_override_len;
    }
  else
    {
      p = gcry_random_bytes_secure (i, GCRY_STRONG_RANDOM);
      /* Replace zero bytes by new values. */
      for (;;)
        {
          int j, k;
          unsigned char *pp;

          /* Count the zero bytes. */
          for (j=k=0; j < i; j++)
            {
              if (!p[j])
                k++;
            }
          if (!k)
            break; /* Okay: no (more) zero bytes. */

          k += k/128 + 3; /* Better get some more. */
          pp = gcry_random_bytes_secure (k, GCRY_STRONG_RANDOM);
          for (j=0; j < i && k; )
            {
              if (!p[j])
                p[j] = pp[--k];
              if (p[j])
                j++;
            }
          gcry_free (pp);
        }
      memcpy (frame+n, p, i);
      n += i;
      gcry_free (p);
    }

  frame[n++] = 0;
  memcpy (frame+n, value, valuelen);
  n += valuelen;
  gcry_assert (n == nframe);

  err = gcry_mpi_scan (r_result, GCRYMPI_FMT_USG, frame, n, &nframe);
  if (err)
    rc = gcry_err_code (err);
  else if (DBG_CIPHER)
    log_mpidump ("PKCS#1 block type 2 encoded data", *r_result);
  gcry_free (frame);

  return rc;
}


/* Decode a plaintext in VALUE assuming pkcs#1 block type 2 padding.
   NBITS is the size of the secret key.  On success the result is
   stored as a newly allocated buffer at R_RESULT and its valid length at
   R_RESULTLEN.  On error NULL is stored at R_RESULT.  */
static gcry_err_code_t
pkcs1_decode_for_encryption (unsigned char **r_result, size_t *r_resultlen,
                             unsigned int nbits, gcry_mpi_t value)
{
  gcry_error_t err;
  unsigned char *frame = NULL;
  size_t nframe = (nbits+7) / 8;
  size_t n;

  *r_result = NULL;

  if ( !(frame = gcry_malloc_secure (nframe)))
    return gpg_err_code_from_syserror ();

  err = gcry_mpi_print (GCRYMPI_FMT_USG, frame, nframe, &n, value);
  if (err)
    {
      gcry_free (frame);
      return gcry_err_code (err);
    }

  nframe = n; /* Set NFRAME to the actual length.  */

  /* FRAME = 0x00 || 0x02 || PS || 0x00 || M

     pkcs#1 requires that the first byte is zero.  Our MPIs usually
     strip leading zero bytes; thus we are not able to detect them.
     However due to the way gcry_mpi_print is implemented we may see
     leading zero bytes nevertheless.  We handle this by making the
     first zero byte optional.  */
  if (nframe < 4)
    {
      gcry_free (frame);
      return GPG_ERR_ENCODING_PROBLEM;  /* Too short.  */
    }
  n = 0;
  if (!frame[0])
    n++;
  if (frame[n++] != 0x02)
    {
      gcry_free (frame);
      return GPG_ERR_ENCODING_PROBLEM;  /* Wrong block type.  */
    }

  /* Skip the non-zero random bytes and the terminating zero byte.  */
  for (; n < nframe && frame[n] != 0x00; n++)
    ;
  if (n+1 >= nframe)
    {
      gcry_free (frame);
      return GPG_ERR_ENCODING_PROBLEM; /* No zero byte.  */
    }
  n++; /* Skip the zero byte.  */

  /* To avoid an extra allocation we reuse the frame buffer.  The only
     caller of this function will anyway free the result soon.  */
  memmove (frame, frame + n, nframe - n);
  *r_result = frame;
  *r_resultlen = nframe - n;

  if (DBG_CIPHER)
    log_printhex ("value extracted from PKCS#1 block type 2 encoded data:",
                  *r_result, *r_resultlen);

  return 0;
}


/* Encode {VALUE,VALUELEN} for an NBITS keys and hash algorith ALGO
   using the pkcs#1 block type 1 padding.  On success the result is
   stored as a new MPI at R_RESULT.  On error the value at R_RESULT is
   undefined.

   We encode the value in this way:

     0  1  PAD(n bytes)  0  ASN(asnlen bytes) VALUE(valuelen bytes)

   0   is a marker we unfortunately can't encode because we return an
       MPI which strips all leading zeroes.
   1   is the block type.
   PAD consists of 0xff bytes.
   0   marks the end of the padding.
   ASN is the DER encoding of the hash algorithm; along with the VALUE
       it yields a valid DER encoding.

   (Note that PGP prior to version 2.3 encoded the message digest as:
      0   1   MD(16 bytes)   0   PAD(n bytes)   1
    The MD is always 16 bytes here because it's always MD5.  GnuPG
    does not not support pre-v2.3 signatures, but I'm including this
    comment so the information is easily found if needed.)
*/
static gcry_err_code_t
pkcs1_encode_for_signature (gcry_mpi_t *r_result, unsigned int nbits,
			    const unsigned char *value, size_t valuelen,
			    int algo)
{
  gcry_err_code_t rc = 0;
  gcry_error_t err;
  byte asn[100];
  byte *frame = NULL;
  size_t nframe = (nbits+7) / 8;
  int i;
  size_t n;
  size_t asnlen, dlen;

  asnlen = DIM(asn);
  dlen = gcry_md_get_algo_dlen (algo);

  if (gcry_md_algo_info (algo, GCRYCTL_GET_ASNOID, asn, &asnlen))
    {
      /* We don't have yet all of the above algorithms.  */
      return GPG_ERR_NOT_IMPLEMENTED;
    }

  if ( valuelen != dlen )
    {
      /* Hash value does not match the length of digest for
         the given algorithm.  */
      return GPG_ERR_CONFLICT;
    }

  if ( !dlen || dlen + asnlen + 4 > nframe)
    {
      /* Can't encode an DLEN byte digest MD into an NFRAME byte
         frame.  */
      return GPG_ERR_TOO_SHORT;
    }

  if ( !(frame = gcry_malloc (nframe)) )
    return gpg_err_code_from_syserror ();

  /* Assemble the pkcs#1 block type 1. */
  n = 0;
  frame[n++] = 0;
  frame[n++] = 1; /* block type */
  i = nframe - valuelen - asnlen - 3 ;
  gcry_assert (i > 1);
  memset (frame+n, 0xff, i );
  n += i;
  frame[n++] = 0;
  memcpy (frame+n, asn, asnlen);
  n += asnlen;
  memcpy (frame+n, value, valuelen );
  n += valuelen;
  gcry_assert (n == nframe);

  /* Convert it into an MPI. */
  err = gcry_mpi_scan (r_result, GCRYMPI_FMT_USG, frame, n, &nframe);
  if (err)
    rc = gcry_err_code (err);
  else if (DBG_CIPHER)
    log_mpidump ("PKCS#1 block type 1 encoded data", *r_result);
  gcry_free (frame);

  return rc;
}


/* Mask generation function for OAEP.  See RFC-3447 B.2.1.  */
static gcry_err_code_t
mgf1 (unsigned char *output, size_t outlen, unsigned char *seed, size_t seedlen,
      int algo)
{
  size_t dlen, nbytes, n;
  int idx;
  gcry_md_hd_t hd;
  gcry_error_t err;

  err = gcry_md_open (&hd, algo, 0);
  if (err)
    return gpg_err_code (err);

  dlen = gcry_md_get_algo_dlen (algo);

  /* We skip step 1 which would be assert(OUTLEN <= 2^32).  The loop
     in step 3 is merged with step 4 by concatenating no more octets
     than what would fit into OUTPUT.  The ceiling for the counter IDX
     is implemented indirectly.  */
  nbytes = 0;  /* Step 2.  */
  idx = 0;
  while ( nbytes < outlen )
    {
      unsigned char c[4], *digest;

      if (idx)
        gcry_md_reset (hd);

      c[0] = (idx >> 24) & 0xFF;
      c[1] = (idx >> 16) & 0xFF;
      c[2] = (idx >> 8) & 0xFF;
      c[3] = idx & 0xFF;
      idx++;

      gcry_md_write (hd, seed, seedlen);
      gcry_md_write (hd, c, 4);
      digest = gcry_md_read (hd, 0);

      n = (outlen - nbytes < dlen)? (outlen - nbytes) : dlen;
      memcpy (output+nbytes, digest, n);
      nbytes += n;
    }

  gcry_md_close (hd);
  return GPG_ERR_NO_ERROR;
}


/* RFC-3447 (pkcs#1 v2.1) OAEP encoding.  NBITS is the length of the
   key measured in bits.  ALGO is the hash function; it must be a
   valid and usable algorithm.  {VALUE,VALUELEN} is the message to
   encrypt.  {LABEL,LABELLEN} is the optional label to be associated
   with the message, if LABEL is NULL the default is to use the empty
   string as label.  On success the encoded ciphertext is returned at
   R_RESULT.

   If {RANDOM_OVERRIDE, RANDOM_OVERRIDE_LEN} is given it is used as
   the seed instead of using a random string for it.  This feature is
   only useful for regression tests.

   Here is figure 1 from the RFC depicting the process:

                             +----------+---------+-------+
                        DB = |  lHash   |    PS   |   M   |
                             +----------+---------+-------+
                                            |
                  +----------+              V
                  |   seed   |--> MGF ---> xor
                  +----------+              |
                        |                   |
               +--+     V                   |
               |00|    xor <----- MGF <-----|
               +--+     |                   |
                 |      |                   |
                 V      V                   V
               +--+----------+----------------------------+
         EM =  |00|maskedSeed|          maskedDB          |
               +--+----------+----------------------------+
  */
static gcry_err_code_t
oaep_encode (gcry_mpi_t *r_result, unsigned int nbits, int algo,
             const unsigned char *value, size_t valuelen,
             const unsigned char *label, size_t labellen,
             const void *random_override, size_t random_override_len)
{
  gcry_err_code_t rc = 0;
  gcry_error_t err;
  unsigned char *frame = NULL;
  size_t nframe = (nbits+7) / 8;
  unsigned char *p;
  size_t hlen;
  size_t n;

  *r_result = NULL;

  /* Set defaults for LABEL.  */
  if (!label || !labellen)
    {
      label = (const unsigned char*)"";
      labellen = 0;
    }

  hlen = gcry_md_get_algo_dlen (algo);

  /* We skip step 1a which would be to check that LABELLEN is not
     greater than 2^61-1.  See rfc-3447 7.1.1. */

  /* Step 1b.  Note that the obsolete rfc-2437 uses the check:
     valuelen > nframe - 2 * hlen - 1 .  */
  if (valuelen > nframe - 2 * hlen - 2 || !nframe)
    {
      /* Can't encode a VALUELEN value in a NFRAME bytes frame. */
      return GPG_ERR_TOO_SHORT; /* The key is too short.  */
    }

  /* Allocate the frame.  */
  frame = gcry_calloc_secure (1, nframe);
  if (!frame)
    return gpg_err_code_from_syserror ();

  /* Step 2a: Compute the hash of the label.  We store it in the frame
     where later the maskedDB will commence.  */
  gcry_md_hash_buffer (algo, frame + 1 + hlen, label, labellen);

  /* Step 2b: Set octet string to zero.  */
  /* This has already been done while allocating FRAME.  */

  /* Step 2c: Create DB by concatenating lHash, PS, 0x01 and M.  */
  n = nframe - valuelen - 1;
  frame[n] = 0x01;
  memcpy (frame + n + 1, value, valuelen);

  /* Step 3d: Generate seed.  We store it where the maskedSeed will go
     later. */
  if (random_override)
    {
      if (random_override_len != hlen)
        {
          gcry_free (frame);
          return GPG_ERR_INV_ARG;
        }
      memcpy (frame + 1, random_override, hlen);
    }
  else
    gcry_randomize (frame + 1, hlen, GCRY_STRONG_RANDOM);

  /* Step 2e and 2f: Create maskedDB.  */
  {
    unsigned char *dmask;

    dmask = gcry_malloc_secure (nframe - hlen - 1);
    if (!dmask)
      {
        rc = gpg_err_code_from_syserror ();
        gcry_free (frame);
        return rc;
      }
    rc = mgf1 (dmask, nframe - hlen - 1, frame+1, hlen, algo);
    if (rc)
      {
        gcry_free (dmask);
        gcry_free (frame);
        return rc;
      }
    for (n = 1 + hlen, p = dmask; n < nframe; n++)
      frame[n] ^= *p++;
    gcry_free (dmask);
  }

  /* Step 2g and 2h: Create maskedSeed.  */
  {
    unsigned char *smask;

    smask = gcry_malloc_secure (hlen);
    if (!smask)
      {
        rc = gpg_err_code_from_syserror ();
        gcry_free (frame);
        return rc;
      }
    rc = mgf1 (smask, hlen, frame + 1 + hlen, nframe - hlen - 1, algo);
    if (rc)
      {
        gcry_free (smask);
        gcry_free (frame);
        return rc;
      }
    for (n = 1, p = smask; n < 1 + hlen; n++)
      frame[n] ^= *p++;
    gcry_free (smask);
  }

  /* Step 2i: Concatenate 0x00, maskedSeed and maskedDB.  */
  /* This has already been done by using in-place operations.  */

  /* Convert the stuff into an MPI as expected by the caller.  */
  err = gcry_mpi_scan (r_result, GCRYMPI_FMT_USG, frame, nframe, NULL);
  if (err)
    rc = gcry_err_code (err);
  else if (DBG_CIPHER)
    log_mpidump ("OAEP encoded data", *r_result);
  gcry_free (frame);

  return rc;
}


/* RFC-3447 (pkcs#1 v2.1) OAEP decoding.  NBITS is the length of the
   key measured in bits.  ALGO is the hash function; it must be a
   valid and usable algorithm.  VALUE is the raw decrypted message
   {LABEL,LABELLEN} is the optional label to be associated with the
   message, if LABEL is NULL the default is to use the empty string as
   label.  On success the plaintext is returned as a newly allocated
   buffer at R_RESULT; its valid length is stored at R_RESULTLEN.  On
   error NULL is stored at R_RESULT.  */
static gcry_err_code_t
oaep_decode (unsigned char **r_result, size_t *r_resultlen,
             unsigned int nbits, int algo,
             gcry_mpi_t value, const unsigned char *label, size_t labellen)
{
  gcry_err_code_t rc;
  unsigned char *frame = NULL; /* Encoded messages (EM).  */
  unsigned char *masked_seed;  /* Points into FRAME.  */
  unsigned char *masked_db;    /* Points into FRAME.  */
  unsigned char *seed = NULL;  /* Allocated space for the seed and DB.  */
  unsigned char *db;           /* Points into SEED.  */
  unsigned char *lhash = NULL; /* Hash of the label.  */
  size_t nframe;               /* Length of the ciphertext (EM).  */
  size_t hlen;                 /* Length of the hash digest.  */
  size_t db_len;               /* Length of DB and masked_db.  */
  size_t nkey = (nbits+7)/8;   /* Length of the key in bytes.  */
  int failed = 0;              /* Error indicator.  */
  size_t n;

  *r_result = NULL;

  /* This code is implemented as described by rfc-3447 7.1.2.  */

  /* Set defaults for LABEL.  */
  if (!label || !labellen)
    {
      label = (const unsigned char*)"";
      labellen = 0;
    }

  /* Get the length of the digest.  */
  hlen = gcry_md_get_algo_dlen (algo);

  /* Hash the label right away.  */
  lhash = gcry_malloc (hlen);
  if (!lhash)
    return gpg_err_code_from_syserror ();
  gcry_md_hash_buffer (algo, lhash, label, labellen);

  /* Turn the MPI into an octet string.  If the octet string is
     shorter than the key we pad it to the left with zeroes.  This may
     happen due to the leading zero in OAEP frames and due to the
     following random octets (seed^mask) which may have leading zero
     bytes.  This all is needed to cope with our leading zeroes
     suppressing MPI implementation.  The code implictly implements
     Step 1b (bail out if NFRAME != N).  */
  rc = octet_string_from_mpi (&frame, NULL, value, nkey);
  if (rc)
    {
      gcry_free (lhash);
      return GPG_ERR_ENCODING_PROBLEM;
    }
  nframe = nkey;

  /* Step 1c: Check that the key is long enough.  */
  if ( nframe < 2 * hlen + 2 )
    {
      gcry_free (frame);
      gcry_free (lhash);
      return GPG_ERR_ENCODING_PROBLEM;
    }

  /* Step 2 has already been done by the caller and the
     gcry_mpi_aprint above.  */

  /* Allocate space for SEED and DB.  */
  seed = gcry_malloc_secure (nframe - 1);
  if (!seed)
    {
      rc = gpg_err_code_from_syserror ();
      gcry_free (frame);
      gcry_free (lhash);
      return rc;
    }
  db = seed + hlen;

  /* To avoid choosen ciphertext attacks from now on we make sure to
     run all code even in the error case; this avoids possible timing
     attacks as described by Manger.  */

  /* Step 3a: Hash the label.  */
  /* This has already been done.  */

  /* Step 3b: Separate the encoded message.  */
  masked_seed = frame + 1;
  masked_db   = frame + 1 + hlen;
  db_len      = nframe - 1 - hlen;

  /* Step 3c and 3d: seed = maskedSeed ^ mgf(maskedDB, hlen).  */
  if (mgf1 (seed, hlen, masked_db, db_len, algo))
    failed = 1;
  for (n = 0; n < hlen; n++)
    seed[n] ^= masked_seed[n];

  /* Step 3e and 3f: db = maskedDB ^ mgf(seed, db_len).  */
  if (mgf1 (db, db_len, seed, hlen, algo))
    failed = 1;
  for (n = 0; n < db_len; n++)
    db[n] ^= masked_db[n];

  /* Step 3g: Check lhash, an possible empty padding string terminated
     by 0x01 and the first byte of EM being 0.  */
  if (memcmp (lhash, db, hlen))
    failed = 1;
  for (n = hlen; n < db_len; n++)
    if (db[n] == 0x01)
      break;
  if (n == db_len)
    failed = 1;
  if (frame[0])
    failed = 1;

  gcry_free (lhash);
  gcry_free (frame);
  if (failed)
    {
      gcry_free (seed);
      return GPG_ERR_ENCODING_PROBLEM;
    }

  /* Step 4: Output M.  */
  /* To avoid an extra allocation we reuse the seed buffer.  The only
     caller of this function will anyway free the result soon.  */
  n++;
  memmove (seed, db + n, db_len - n);
  *r_result = seed;
  *r_resultlen = db_len - n;
  seed = NULL;

  if (DBG_CIPHER)
    log_printhex ("value extracted from OAEP encoded data:",
                  *r_result, *r_resultlen);

  return 0;
}


/* RFC-3447 (pkcs#1 v2.1) PSS encoding.  Encode {VALUE,VALUELEN} for
   an NBITS key.  Note that VALUE is already the mHash from the
   picture below.  ALGO is a valid hash algorithm and SALTLEN is the
   length of salt to be used.  On success the result is stored as a
   new MPI at R_RESULT.  On error the value at R_RESULT is undefined.

   If {RANDOM_OVERRIDE, RANDOM_OVERRIDE_LEN} is given it is used as
   the salt instead of using a random string for the salt.  This
   feature is only useful for regression tests.

   Here is figure 2 from the RFC (errata 595 applied) depicting the
   process:

                                  +-----------+
                                  |     M     |
                                  +-----------+
                                        |
                                        V
                                      Hash
                                        |
                                        V
                          +--------+----------+----------+
                     M' = |Padding1|  mHash   |   salt   |
                          +--------+----------+----------+
                                         |
               +--------+----------+     V
         DB =  |Padding2| salt     |   Hash
               +--------+----------+     |
                         |               |
                         V               |    +----+
                        xor <--- MGF <---|    |0xbc|
                         |               |    +----+
                         |               |      |
                         V               V      V
               +-------------------+----------+----+
         EM =  |    maskedDB       |     H    |0xbc|
               +-------------------+----------+----+

  */
static gcry_err_code_t
pss_encode (gcry_mpi_t *r_result, unsigned int nbits, int algo,
	    const unsigned char *value, size_t valuelen, int saltlen,
            const void *random_override, size_t random_override_len)
{
  gcry_err_code_t rc = 0;
  gcry_error_t err;
  size_t hlen;                 /* Length of the hash digest.  */
  unsigned char *em = NULL;    /* Encoded message.  */
  size_t emlen = (nbits+7)/8;  /* Length in bytes of EM.  */
  unsigned char *h;            /* Points into EM.  */
  unsigned char *buf = NULL;   /* Help buffer.  */
  size_t buflen;               /* Length of BUF.  */
  unsigned char *mhash;        /* Points into BUF.  */
  unsigned char *salt;         /* Points into BUF.  */
  unsigned char *dbmask;       /* Points into BUF.  */
  unsigned char *p;
  size_t n;

  /* This code is implemented as described by rfc-3447 9.1.1.  */

  /* Get the length of the digest.  */
  hlen = gcry_md_get_algo_dlen (algo);
  gcry_assert (hlen);  /* We expect a valid ALGO here.  */

  /* Allocate a help buffer and setup some pointers.  */
  buflen = 8 + hlen + saltlen + (emlen - hlen - 1);
  buf = gcry_malloc (buflen);
  if (!buf)
    {
      rc = gpg_err_code_from_syserror ();
      goto leave;
    }
  mhash = buf + 8;
  salt  = mhash + hlen;
  dbmask= salt + saltlen;

  /* Step 2: That would be: mHash = Hash(M) but our input is already
     mHash thus we do only a consistency check and copy to MHASH.  */
  if (valuelen != hlen)
    {
      rc = GPG_ERR_INV_LENGTH;
      goto leave;
    }
  memcpy (mhash, value, hlen);

  /* Step 3: Check length constraints.  */
  if (emlen < hlen + saltlen + 2)
    {
      rc = GPG_ERR_TOO_SHORT;
      goto leave;
    }

  /* Allocate space for EM.  */
  em = gcry_malloc (emlen);
  if (!em)
    {
      rc = gpg_err_code_from_syserror ();
      goto leave;
    }
  h = em + emlen - 1 - hlen;

  /* Step 4: Create a salt.  */
  if (saltlen)
    {
      if (random_override)
        {
          if (random_override_len != saltlen)
            {
              rc = GPG_ERR_INV_ARG;
              goto leave;
            }
          memcpy (salt, random_override, saltlen);
        }
      else
        gcry_randomize (salt, saltlen, GCRY_STRONG_RANDOM);
    }

  /* Step 5 and 6: M' = Hash(Padding1 || mHash || salt).  */
  memset (buf, 0, 8);  /* Padding.  */
  gcry_md_hash_buffer (algo, h, buf, 8 + hlen + saltlen);

  /* Step 7 and 8: DB = PS || 0x01 || salt.  */
  /* Note that we use EM to store DB and later Xor in-place.  */
  p = em + emlen - 1 - hlen - saltlen - 1;
  memset (em, 0, p - em);
  *p++ = 0x01;
  memcpy (p, salt, saltlen);

  /* Step 9: dbmask = MGF(H, emlen - hlen - 1).  */
  mgf1 (dbmask, emlen - hlen - 1, h, hlen, algo);

  /* Step 10: maskedDB = DB ^ dbMask */
  for (n = 0, p = dbmask; n < emlen - hlen - 1; n++, p++)
    em[n] ^= *p;

  /* Step 11: Set the leftmost bits to zero.  */
  em[0] &= 0xFF >> (8 * emlen - nbits);

  /* Step 12: EM = maskedDB || H || 0xbc.  */
  em[emlen-1] = 0xbc;

  /* Convert EM into an MPI.  */
  err = gcry_mpi_scan (r_result, GCRYMPI_FMT_USG, em, emlen, NULL);
  if (err)
    rc = gcry_err_code (err);
  else if (DBG_CIPHER)
    log_mpidump ("PSS encoded data", *r_result);

 leave:
  if (em)
    {
      wipememory (em, emlen);
      gcry_free (em);
    }
  if (buf)
    {
      wipememory (buf, buflen);
      gcry_free (buf);
    }
  return rc;
}


/* Verify a signature assuming PSS padding.  VALUE is the hash of the
   message (mHash) encoded as an MPI; its length must match the digest
   length of ALGO.  ENCODED is the output of the RSA public key
   function (EM).  NBITS is the size of the public key.  ALGO is the
   hash algorithm and SALTLEN is the length of the used salt.  The
   function returns 0 on success or on error code.  */
static gcry_err_code_t
pss_verify (gcry_mpi_t value, gcry_mpi_t encoded, unsigned int nbits, int algo,
            size_t saltlen)
{
  gcry_err_code_t rc = 0;
  size_t hlen;                 /* Length of the hash digest.  */
  unsigned char *em = NULL;    /* Encoded message.  */
  size_t emlen = (nbits+7)/8;  /* Length in bytes of EM.  */
  unsigned char *salt;         /* Points into EM.  */
  unsigned char *h;            /* Points into EM.  */
  unsigned char *buf = NULL;   /* Help buffer.  */
  size_t buflen;               /* Length of BUF.  */
  unsigned char *dbmask;       /* Points into BUF.  */
  unsigned char *mhash;        /* Points into BUF.  */
  unsigned char *p;
  size_t n;

  /* This code is implemented as described by rfc-3447 9.1.2.  */

  /* Get the length of the digest.  */
  hlen = gcry_md_get_algo_dlen (algo);
  gcry_assert (hlen);  /* We expect a valid ALGO here.  */

  /* Allocate a help buffer and setup some pointers.
     This buffer is used for two purposes:
        +------------------------------+-------+
     1. | dbmask                       | mHash |
        +------------------------------+-------+
           emlen - hlen - 1              hlen

        +----------+-------+---------+-+-------+
     2. | padding1 | mHash | salt    | | mHash |
        +----------+-------+---------+-+-------+
             8       hlen    saltlen     hlen
  */
  buflen = 8 + hlen + saltlen;
  if (buflen < emlen - hlen - 1)
    buflen = emlen - hlen - 1;
  buflen += hlen;
  buf = gcry_malloc (buflen);
  if (!buf)
    {
      rc = gpg_err_code_from_syserror ();
      goto leave;
    }
  dbmask = buf;
  mhash = buf + buflen - hlen;

  /* Step 2: That would be: mHash = Hash(M) but our input is already
     mHash thus we only need to convert VALUE into MHASH.  */
  rc = octet_string_from_mpi (NULL, mhash, value, hlen);
  if (rc)
    goto leave;

  /* Convert the signature into an octet string.  */
  rc = octet_string_from_mpi (&em, NULL, encoded, emlen);
  if (rc)
    goto leave;

  /* Step 3: Check length of EM.  Because we internally use MPI
     functions we can't do this properly; EMLEN is always the length
     of the key because octet_string_from_mpi needs to left pad the
     result with zero to cope with the fact that our MPIs suppress all
     leading zeroes.  Thus what we test here are merely the digest and
     salt lengths to the key.  */
  if (emlen < hlen + saltlen + 2)
    {
      rc = GPG_ERR_TOO_SHORT; /* For the hash and saltlen.  */
      goto leave;
    }

  /* Step 4: Check last octet.  */
  if (em[emlen - 1] != 0xbc)
    {
      rc = GPG_ERR_BAD_SIGNATURE;
      goto leave;
    }

  /* Step 5: Split EM.  */
  h = em + emlen - 1 - hlen;

  /* Step 6: Check the leftmost bits.  */
  if ((em[0] & ~(0xFF >> (8 * emlen - nbits))))
    {
      rc = GPG_ERR_BAD_SIGNATURE;
      goto leave;
    }

  /* Step 7: dbmask = MGF(H, emlen - hlen - 1).  */
  mgf1 (dbmask, emlen - hlen - 1, h, hlen, algo);

  /* Step 8: maskedDB = DB ^ dbMask.  */
  for (n = 0, p = dbmask; n < emlen - hlen - 1; n++, p++)
    em[n] ^= *p;

  /* Step 9: Set leftmost bits in DB to zero.  */
  em[0] &= 0xFF >> (8 * emlen - nbits);

  /* Step 10: Check the padding of DB.  */
  for (n = 0; n < emlen - hlen - saltlen - 2 && !em[n]; n++)
    ;
  if (n != emlen - hlen - saltlen - 2 || em[n++] != 1)
    {
      rc = GPG_ERR_BAD_SIGNATURE;
      goto leave;
    }

  /* Step 11: Extract salt from DB.  */
  salt = em + n;

  /* Step 12:  M' = (0x)00 00 00 00 00 00 00 00 || mHash || salt */
  memset (buf, 0, 8);
  memcpy (buf+8, mhash, hlen);
  memcpy (buf+8+hlen, salt, saltlen);

  /* Step 13:  H' = Hash(M').  */
  gcry_md_hash_buffer (algo, buf, buf, 8 + hlen + saltlen);

  /* Step 14:  Check H == H'.   */
  rc = memcmp (h, buf, hlen) ? GPG_ERR_BAD_SIGNATURE : GPG_ERR_NO_ERROR;

 leave:
  if (em)
    {
      wipememory (em, emlen);
      gcry_free (em);
    }
  if (buf)
    {
      wipememory (buf, buflen);
      gcry_free (buf);
    }
  return rc;
}


/* Callback for the pubkey algorithm code to verify PSS signatures.
   OPAQUE is the data provided by the actual caller.  The meaning of
   TMP depends on the actual algorithm (but there is only RSA); now
   for RSA it is the output of running the public key function on the
   input.  */
static int
pss_verify_cmp (void *opaque, gcry_mpi_t tmp)
{
  struct pk_encoding_ctx *ctx = opaque;
  gcry_mpi_t hash = ctx->verify_arg;

  return pss_verify (hash, tmp, ctx->nbits - 1, ctx->hash_algo, ctx->saltlen);
}


/* Internal function.   */
static gcry_err_code_t
sexp_elements_extract (gcry_sexp_t key_sexp, const char *element_names,
		       gcry_mpi_t *elements, const char *algo_name)
{
  gcry_err_code_t err = 0;
  int i, idx;
  const char *name;
  gcry_sexp_t list;

  for (name = element_names, idx = 0; *name && !err; name++, idx++)
    {
      list = gcry_sexp_find_token (key_sexp, name, 1);
      if (!list)
	elements[idx] = NULL;
      else
	{
	  elements[idx] = gcry_sexp_nth_mpi (list, 1, GCRYMPI_FMT_USG);
	  gcry_sexp_release (list);
	  if (!elements[idx])
	    err = GPG_ERR_INV_OBJ;
	}
    }

  if (!err)
    {
      /* Check that all elements are available.  */
      for (name = element_names, idx = 0; *name; name++, idx++)
        if (!elements[idx])
          break;
      if (*name)
        {
          err = GPG_ERR_NO_OBJ;
          /* Some are missing.  Before bailing out we test for
             optional parameters.  */
          if (algo_name && !strcmp (algo_name, "RSA")
              && !strcmp (element_names, "nedpqu") )
            {
              /* This is RSA.  Test whether we got N, E and D and that
                 the optional P, Q and U are all missing.  */
              if (elements[0] && elements[1] && elements[2]
                  && !elements[3] && !elements[4] && !elements[5])
                err = 0;
            }
        }
    }


  if (err)
    {
      for (i = 0; i < idx; i++)
        if (elements[i])
          gcry_free (elements[i]);
    }
  return err;
}


/* Internal function used for ecc.  Note, that this function makes use
   of its intimate knowledge about the ECC parameters from ecc.c. */
static gcry_err_code_t
sexp_elements_extract_ecc (gcry_sexp_t key_sexp, const char *element_names,
                           gcry_mpi_t *elements, pk_extra_spec_t *extraspec)

{
  gcry_err_code_t err = 0;
  int idx;
  const char *name;
  gcry_sexp_t list;

  /* Clear the array for easier error cleanup. */
  for (name = element_names, idx = 0; *name; name++, idx++)
    elements[idx] = NULL;
  gcry_assert (idx >= 5); /* We know that ECC has at least 5 elements
                             (params only) or 6 (full public key).  */
  if (idx == 5)
    elements[5] = NULL;   /* Extra clear for the params only case.  */


  /* Init the array with the available curve parameters. */
  for (name = element_names, idx = 0; *name && !err; name++, idx++)
    {
      list = gcry_sexp_find_token (key_sexp, name, 1);
      if (!list)
	elements[idx] = NULL;
      else
	{
	  elements[idx] = gcry_sexp_nth_mpi (list, 1, GCRYMPI_FMT_USG);
	  gcry_sexp_release (list);
	  if (!elements[idx])
            {
              err = GPG_ERR_INV_OBJ;
              goto leave;
            }
	}
    }

  /* Check whether a curve parameter has been given and then fill any
     missing elements.  */
  list = gcry_sexp_find_token (key_sexp, "curve", 5);
  if (list)
    {
      if (extraspec->get_param)
        {
          char *curve;
          gcry_mpi_t params[6];

          for (idx = 0; idx < DIM(params); idx++)
            params[idx] = NULL;

          curve = _gcry_sexp_nth_string (list, 1);
          gcry_sexp_release (list);
          if (!curve)
            {
              /* No curve name given (or out of core). */
              err = GPG_ERR_INV_OBJ;
              goto leave;
            }
          err = extraspec->get_param (curve, params);
          gcry_free (curve);
          if (err)
            goto leave;

          for (idx = 0; idx < DIM(params); idx++)
            {
              if (!elements[idx])
                elements[idx] = params[idx];
              else
                mpi_free (params[idx]);
            }
        }
      else
        {
          gcry_sexp_release (list);
          err = GPG_ERR_INV_OBJ; /* "curve" given but ECC not supported. */
          goto leave;
        }
    }

  /* Check that all parameters are known.  */
  for (name = element_names, idx = 0; *name; name++, idx++)
    if (!elements[idx])
      {
        err = GPG_ERR_NO_OBJ;
        goto leave;
      }

 leave:
  if (err)
    {
      for (name = element_names, idx = 0; *name; name++, idx++)
        if (elements[idx])
          gcry_free (elements[idx]);
    }
  return err;
}



/****************
 * Convert a S-Exp with either a private or a public key to our
 * internal format. Currently we do only support the following
 * algorithms:
 *    dsa
 *    rsa
 *    openpgp-dsa
 *    openpgp-rsa
 *    openpgp-elg
 *    openpgp-elg-sig
 *    ecdsa
 *    ecdh
 * Provide a SE with the first element be either "private-key" or
 * or "public-key". It is followed by a list with its first element
 * be one of the above algorithm identifiers and the remaning
 * elements are pairs with parameter-id and value.
 * NOTE: we look through the list to find a list beginning with
 * "private-key" or "public-key" - the first one found is used.
 *
 * If OVERRIDE_ELEMS is not NULL those elems override the parameter
 * specification taken from the module.  This ise used by
 * gcry_pk_get_curve.
 *
 * Returns: A pointer to an allocated array of MPIs if the return value is
 *	    zero; the caller has to release this array.
 *
 * Example of a DSA public key:
 *  (private-key
 *    (dsa
 *	(p <mpi>)
 *	(g <mpi>)
 *	(y <mpi>)
 *	(x <mpi>)
 *    )
 *  )
 * The <mpi> are expected to be in GCRYMPI_FMT_USG
 */
static gcry_err_code_t
sexp_to_key (gcry_sexp_t sexp, int want_private, const char *override_elems,
             gcry_mpi_t **retarray, gcry_module_t *retalgo)
{
  gcry_err_code_t err = 0;
  gcry_sexp_t list, l2;
  char *name;
  const char *elems;
  gcry_mpi_t *array;
  gcry_module_t module;
  gcry_pk_spec_t *pubkey;
  pk_extra_spec_t *extraspec;
  int is_ecc;

  /* Check that the first element is valid.  */
  list = gcry_sexp_find_token (sexp,
                               want_private? "private-key":"public-key", 0);
  if (!list)
    return GPG_ERR_INV_OBJ; /* Does not contain a key object.  */

  l2 = gcry_sexp_cadr( list );
  gcry_sexp_release ( list );
  list = l2;
  name = _gcry_sexp_nth_string (list, 0);
  if (!name)
    {
      gcry_sexp_release ( list );
      return GPG_ERR_INV_OBJ;      /* Invalid structure of object. */
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = gcry_pk_lookup_name (name);
  ath_mutex_unlock (&pubkeys_registered_lock);

  /* Fixme: We should make sure that an ECC key is always named "ecc"
     and not "ecdsa".  "ecdsa" should be used for the signature
     itself.  We need a function to test whether an algorithm given
     with a key is compatible with an application of the key (signing,
     encryption).  For RSA this is easy, but ECC is the first
     algorithm which has many flavours.  */
  is_ecc = ( !strcmp (name, "ecdsa")
             || !strcmp (name, "ecdh")
             || !strcmp (name, "ecc") );
  gcry_free (name);

  if (!module)
    {
      gcry_sexp_release (list);
      return GPG_ERR_PUBKEY_ALGO; /* Unknown algorithm. */
    }
  else
    {
      pubkey = (gcry_pk_spec_t *) module->spec;
      extraspec = module->extraspec;
    }

  if (override_elems)
    elems = override_elems;
  else if (want_private)
    elems = pubkey->elements_skey;
  else
    elems = pubkey->elements_pkey;
  array = gcry_calloc (strlen (elems) + 1, sizeof (*array));
  if (!array)
    err = gpg_err_code_from_syserror ();
  if (!err)
    {
      if (is_ecc)
        err = sexp_elements_extract_ecc (list, elems, array, extraspec);
      else
        err = sexp_elements_extract (list, elems, array, pubkey->name);
    }

  gcry_sexp_release (list);

  if (err)
    {
      gcry_free (array);

      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }
  else
    {
      *retarray = array;
      *retalgo = module;
    }

  return err;
}


static gcry_err_code_t
sexp_to_sig (gcry_sexp_t sexp, gcry_mpi_t **retarray,
	     gcry_module_t *retalgo)
{
  gcry_err_code_t err = 0;
  gcry_sexp_t list, l2;
  char *name;
  const char *elems;
  gcry_mpi_t *array;
  gcry_module_t module;
  gcry_pk_spec_t *pubkey;

  /* Check that the first element is valid.  */
  list = gcry_sexp_find_token( sexp, "sig-val" , 0 );
  if (!list)
    return GPG_ERR_INV_OBJ; /* Does not contain a signature value object.  */

  l2 = gcry_sexp_nth (list, 1);
  if (!l2)
    {
      gcry_sexp_release (list);
      return GPG_ERR_NO_OBJ;   /* No cadr for the sig object.  */
    }
  name = _gcry_sexp_nth_string (l2, 0);
  if (!name)
    {
      gcry_sexp_release (list);
      gcry_sexp_release (l2);
      return GPG_ERR_INV_OBJ;  /* Invalid structure of object.  */
    }
  else if (!strcmp (name, "flags"))
    {
      /* Skip flags, since they are not used but here just for the
	 sake of consistent S-expressions.  */
      gcry_free (name);
      gcry_sexp_release (l2);
      l2 = gcry_sexp_nth (list, 2);
      if (!l2)
	{
	  gcry_sexp_release (list);
	  return GPG_ERR_INV_OBJ;
	}
      name = _gcry_sexp_nth_string (l2, 0);
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = gcry_pk_lookup_name (name);
  ath_mutex_unlock (&pubkeys_registered_lock);
  gcry_free (name);
  name = NULL;

  if (!module)
    {
      gcry_sexp_release (l2);
      gcry_sexp_release (list);
      return GPG_ERR_PUBKEY_ALGO;  /* Unknown algorithm. */
    }
  else
    pubkey = (gcry_pk_spec_t *) module->spec;

  elems = pubkey->elements_sig;
  array = gcry_calloc (strlen (elems) + 1 , sizeof *array );
  if (!array)
    err = gpg_err_code_from_syserror ();

  if (!err)
    err = sexp_elements_extract (list, elems, array, NULL);

  gcry_sexp_release (l2);
  gcry_sexp_release (list);

  if (err)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);

      gcry_free (array);
    }
  else
    {
      *retarray = array;
      *retalgo = module;
    }

  return err;
}

static inline int
get_hash_algo (const char *s, size_t n)
{
  static const struct { const char *name; int algo; } hashnames[] = {
    { "sha1",   GCRY_MD_SHA1 },
    { "md5",    GCRY_MD_MD5 },
    { "sha256", GCRY_MD_SHA256 },
    { "ripemd160", GCRY_MD_RMD160 },
    { "rmd160", GCRY_MD_RMD160 },
    { "sha384", GCRY_MD_SHA384 },
    { "sha512", GCRY_MD_SHA512 },
    { "sha224", GCRY_MD_SHA224 },
    { "md2",    GCRY_MD_MD2 },
    { "md4",    GCRY_MD_MD4 },
    { "tiger",  GCRY_MD_TIGER },
    { "haval",  GCRY_MD_HAVAL },
    { NULL, 0 }
  };
  int algo;
  int i;

  for (i=0; hashnames[i].name; i++)
    {
      if ( strlen (hashnames[i].name) == n
	   && !memcmp (hashnames[i].name, s, n))
	break;
    }
  if (hashnames[i].name)
    algo = hashnames[i].algo;
  else
    {
      /* In case of not listed or dynamically allocated hash
	 algorithm we fall back to this somewhat slower
	 method.  Further, it also allows to use OIDs as
	 algorithm names. */
      char *tmpname;

      tmpname = gcry_malloc (n+1);
      if (!tmpname)
	algo = 0;  /* Out of core - silently give up.  */
      else
	{
	  memcpy (tmpname, s, n);
	  tmpname[n] = 0;
	  algo = gcry_md_map_name (tmpname);
	  gcry_free (tmpname);
	}
    }
  return algo;
}


/****************
 * Take sexp and return an array of MPI as used for our internal decrypt
 * function.
 * s_data = (enc-val
 *           [(flags [raw, pkcs1, oaep, no-blinding])]
 *           [(hash-algo <algo>)]
 *           [(label <label>)]
 *	      (<algo>
 *		(<param_name1> <mpi>)
 *		...
 *		(<param_namen> <mpi>)
 *	      ))
 * HASH-ALGO and LABEL are specific to OAEP.
 * RET_MODERN is set to true when at least an empty flags list has been found.
 * CTX is used to return encoding information; it may be NULL in which
 * case raw encoding is used.
 */
static gcry_err_code_t
sexp_to_enc (gcry_sexp_t sexp, gcry_mpi_t **retarray, gcry_module_t *retalgo,
             int *ret_modern, int *flags, struct pk_encoding_ctx *ctx)
{
  gcry_err_code_t err = 0;
  gcry_sexp_t list = NULL, l2 = NULL;
  gcry_pk_spec_t *pubkey = NULL;
  gcry_module_t module = NULL;
  char *name = NULL;
  size_t n;
  int parsed_flags = 0;
  const char *elems;
  gcry_mpi_t *array = NULL;

  *ret_modern = 0;

  /* Check that the first element is valid.  */
  list = gcry_sexp_find_token (sexp, "enc-val" , 0);
  if (!list)
    {
      err = GPG_ERR_INV_OBJ; /* Does not contain an encrypted value object.  */
      goto leave;
    }

  l2 = gcry_sexp_nth (list, 1);
  if (!l2)
    {
      err = GPG_ERR_NO_OBJ; /* No cdr for the data object.  */
      goto leave;
    }

  /* Extract identifier of sublist.  */
  name = _gcry_sexp_nth_string (l2, 0);
  if (!name)
    {
      err = GPG_ERR_INV_OBJ; /* Invalid structure of object.  */
      goto leave;
    }

  if (!strcmp (name, "flags"))
    {
      /* There is a flags element - process it.  */
      const char *s;
      int i;

      *ret_modern = 1;
      for (i = gcry_sexp_length (l2) - 1; i > 0; i--)
        {
          s = gcry_sexp_nth_data (l2, i, &n);
          if (! s)
            ; /* Not a data element - ignore.  */
          else if (n == 3 && !memcmp (s, "raw", 3)
                   && ctx->encoding == PUBKEY_ENC_UNKNOWN)
            ctx->encoding = PUBKEY_ENC_RAW;
          else if (n == 5 && !memcmp (s, "pkcs1", 5)
                   && ctx->encoding == PUBKEY_ENC_UNKNOWN)
	    ctx->encoding = PUBKEY_ENC_PKCS1;
          else if (n == 4 && !memcmp (s, "oaep", 4)
                   && ctx->encoding == PUBKEY_ENC_UNKNOWN)
	    ctx->encoding = PUBKEY_ENC_OAEP;
          else if (n == 3 && !memcmp (s, "pss", 3)
                   && ctx->encoding == PUBKEY_ENC_UNKNOWN)
	    {
	      err = GPG_ERR_CONFLICT;
	      goto leave;
	    }
          else if (n == 11 && ! memcmp (s, "no-blinding", 11))
            parsed_flags |= PUBKEY_FLAG_NO_BLINDING;
          else
            {
              err = GPG_ERR_INV_FLAG;
              goto leave;
            }
        }
      gcry_sexp_release (l2);

      /* Get the OAEP parameters HASH-ALGO and LABEL, if any. */
      if (ctx->encoding == PUBKEY_ENC_OAEP)
	{
	  /* Get HASH-ALGO. */
	  l2 = gcry_sexp_find_token (list, "hash-algo", 0);
	  if (l2)
	    {
	      s = gcry_sexp_nth_data (l2, 1, &n);
	      if (!s)
		err = GPG_ERR_NO_OBJ;
	      else
		{
		  ctx->hash_algo = get_hash_algo (s, n);
		  if (!ctx->hash_algo)
		    err = GPG_ERR_DIGEST_ALGO;
		}
	      gcry_sexp_release (l2);
	      if (err)
		goto leave;
	    }

	  /* Get LABEL. */
	  l2 = gcry_sexp_find_token (list, "label", 0);
	  if (l2)
	    {
	      s = gcry_sexp_nth_data (l2, 1, &n);
	      if (!s)
		err = GPG_ERR_NO_OBJ;
	      else if (n > 0)
		{
		  ctx->label = gcry_malloc (n);
		  if (!ctx->label)
		    err = gpg_err_code_from_syserror ();
		  else
		    {
		      memcpy (ctx->label, s, n);
		      ctx->labellen = n;
		    }
		}
	      gcry_sexp_release (l2);
	      if (err)
		goto leave;
	    }
	}

      /* Get the next which has the actual data - skip HASH-ALGO and LABEL. */
      for (i = 2; (l2 = gcry_sexp_nth (list, i)) != NULL; i++)
	{
	  s = gcry_sexp_nth_data (l2, 0, &n);
	  if (!(n == 9 && !memcmp (s, "hash-algo", 9))
	      && !(n == 5 && !memcmp (s, "label", 5))
	      && !(n == 15 && !memcmp (s, "random-override", 15)))
	    break;
	  gcry_sexp_release (l2);
	}

      if (!l2)
        {
          err = GPG_ERR_NO_OBJ; /* No cdr for the data object. */
          goto leave;
        }

      /* Extract sublist identifier.  */
      gcry_free (name);
      name = _gcry_sexp_nth_string (l2, 0);
      if (!name)
        {
          err = GPG_ERR_INV_OBJ; /* Invalid structure of object. */
          goto leave;
        }

      gcry_sexp_release (list);
      list = l2;
      l2 = NULL;
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = gcry_pk_lookup_name (name);
  ath_mutex_unlock (&pubkeys_registered_lock);

  if (!module)
    {
      err = GPG_ERR_PUBKEY_ALGO; /* Unknown algorithm.  */
      goto leave;
    }
  pubkey = (gcry_pk_spec_t *) module->spec;

  elems = pubkey->elements_enc;
  array = gcry_calloc (strlen (elems) + 1, sizeof (*array));
  if (!array)
    {
      err = gpg_err_code_from_syserror ();
      goto leave;
    }

  err = sexp_elements_extract (list, elems, array, NULL);

 leave:
  gcry_sexp_release (list);
  gcry_sexp_release (l2);
  gcry_free (name);

  if (err)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
      gcry_free (array);
      gcry_free (ctx->label);
      ctx->label = NULL;
    }
  else
    {
      *retarray = array;
      *retalgo = module;
      *flags = parsed_flags;
    }

  return err;
}

/* Take the hash value and convert into an MPI, suitable for
   passing to the low level functions.  We currently support the
   old style way of passing just a MPI and the modern interface which
   allows to pass flags so that we can choose between raw and pkcs1
   padding - may be more padding options later.

   (<mpi>)
   or
   (data
    [(flags [raw, pkcs1, oaep, pss, no-blinding])]
    [(hash <algo> <value>)]
    [(value <text>)]
    [(hash-algo <algo>)]
    [(label <label>)]
    [(salt-length <length>)]
    [(random-override <data>)]
   )

   Either the VALUE or the HASH element must be present for use
   with signatures.  VALUE is used for encryption.

   HASH-ALGO and LABEL are specific to OAEP.

   SALT-LENGTH is for PSS.

   RANDOM-OVERRIDE is used to replace random nonces for regression
   testing.  */
static gcry_err_code_t
sexp_data_to_mpi (gcry_sexp_t input, gcry_mpi_t *ret_mpi,
		  struct pk_encoding_ctx *ctx)
{
  gcry_err_code_t rc = 0;
  gcry_sexp_t ldata, lhash, lvalue;
  int i;
  size_t n;
  const char *s;
  int unknown_flag=0;
  int parsed_flags = 0;

  *ret_mpi = NULL;
  ldata = gcry_sexp_find_token (input, "data", 0);
  if (!ldata)
    { /* assume old style */
      *ret_mpi = gcry_sexp_nth_mpi (input, 0, 0);
      return *ret_mpi ? GPG_ERR_NO_ERROR : GPG_ERR_INV_OBJ;
    }

  /* see whether there is a flags object */
  {
    gcry_sexp_t lflags = gcry_sexp_find_token (ldata, "flags", 0);
    if (lflags)
      { /* parse the flags list. */
        for (i=gcry_sexp_length (lflags)-1; i > 0; i--)
          {
            s = gcry_sexp_nth_data (lflags, i, &n);
            if (!s)
              ; /* not a data element*/
            else if ( n == 3 && !memcmp (s, "raw", 3)
                      && ctx->encoding == PUBKEY_ENC_UNKNOWN)
              ctx->encoding = PUBKEY_ENC_RAW;
            else if ( n == 5 && !memcmp (s, "pkcs1", 5)
                      && ctx->encoding == PUBKEY_ENC_UNKNOWN)
              ctx->encoding = PUBKEY_ENC_PKCS1;
            else if ( n == 4 && !memcmp (s, "oaep", 4)
                      && ctx->encoding == PUBKEY_ENC_UNKNOWN)
              ctx->encoding = PUBKEY_ENC_OAEP;
            else if ( n == 3 && !memcmp (s, "pss", 3)
                      && ctx->encoding == PUBKEY_ENC_UNKNOWN)
              ctx->encoding = PUBKEY_ENC_PSS;
	    else if (n == 11 && ! memcmp (s, "no-blinding", 11))
	      parsed_flags |= PUBKEY_FLAG_NO_BLINDING;
            else
              unknown_flag = 1;
          }
        gcry_sexp_release (lflags);
      }
  }

  if (ctx->encoding == PUBKEY_ENC_UNKNOWN)
    ctx->encoding = PUBKEY_ENC_RAW; /* default to raw */

  /* Get HASH or MPI */
  lhash = gcry_sexp_find_token (ldata, "hash", 0);
  lvalue = lhash? NULL : gcry_sexp_find_token (ldata, "value", 0);

  if (!(!lhash ^ !lvalue))
    rc = GPG_ERR_INV_OBJ; /* none or both given */
  else if (unknown_flag)
    rc = GPG_ERR_INV_FLAG;
  else if (ctx->encoding == PUBKEY_ENC_RAW && lvalue)
    {
      *ret_mpi = gcry_sexp_nth_mpi (lvalue, 1, GCRYMPI_FMT_USG);
      if (!*ret_mpi)
        rc = GPG_ERR_INV_OBJ;
    }
  else if (ctx->encoding == PUBKEY_ENC_PKCS1 && lvalue
	   && ctx->op == PUBKEY_OP_ENCRYPT)
    {
      const void * value;
      size_t valuelen;
      gcry_sexp_t list;
      void *random_override = NULL;
      size_t random_override_len = 0;

      if ( !(value=gcry_sexp_nth_data (lvalue, 1, &valuelen)) || !valuelen )
        rc = GPG_ERR_INV_OBJ;
      else
        {
          /* Get optional RANDOM-OVERRIDE.  */
          list = gcry_sexp_find_token (ldata, "random-override", 0);
          if (list)
            {
              s = gcry_sexp_nth_data (list, 1, &n);
              if (!s)
                rc = GPG_ERR_NO_OBJ;
              else if (n > 0)
                {
                  random_override = gcry_malloc (n);
                  if (!random_override)
                    rc = gpg_err_code_from_syserror ();
                  else
                    {
                      memcpy (random_override, s, n);
                      random_override_len = n;
                    }
                }
              gcry_sexp_release (list);
              if (rc)
                goto leave;
            }

          rc = pkcs1_encode_for_encryption (ret_mpi, ctx->nbits,
                                            value, valuelen,
                                            random_override,
                                            random_override_len);
          gcry_free (random_override);
        }
    }
  else if (ctx->encoding == PUBKEY_ENC_PKCS1 && lhash
	   && (ctx->op == PUBKEY_OP_SIGN || ctx->op == PUBKEY_OP_VERIFY))
    {
      if (gcry_sexp_length (lhash) != 3)
        rc = GPG_ERR_INV_OBJ;
      else if ( !(s=gcry_sexp_nth_data (lhash, 1, &n)) || !n )
        rc = GPG_ERR_INV_OBJ;
      else
        {
          const void * value;
          size_t valuelen;

	  ctx->hash_algo = get_hash_algo (s, n);

          if (!ctx->hash_algo)
            rc = GPG_ERR_DIGEST_ALGO;
          else if ( !(value=gcry_sexp_nth_data (lhash, 2, &valuelen))
                    || !valuelen )
            rc = GPG_ERR_INV_OBJ;
          else
	    rc = pkcs1_encode_for_signature (ret_mpi, ctx->nbits,
					     value, valuelen,
					     ctx->hash_algo);
        }
    }
  else if (ctx->encoding == PUBKEY_ENC_OAEP && lvalue
	   && ctx->op == PUBKEY_OP_ENCRYPT)
    {
      const void * value;
      size_t valuelen;

      if ( !(value=gcry_sexp_nth_data (lvalue, 1, &valuelen)) || !valuelen )
	rc = GPG_ERR_INV_OBJ;
      else
	{
	  gcry_sexp_t list;
          void *random_override = NULL;
          size_t random_override_len = 0;

	  /* Get HASH-ALGO. */
	  list = gcry_sexp_find_token (ldata, "hash-algo", 0);
	  if (list)
	    {
	      s = gcry_sexp_nth_data (list, 1, &n);
	      if (!s)
		rc = GPG_ERR_NO_OBJ;
	      else
		{
		  ctx->hash_algo = get_hash_algo (s, n);
		  if (!ctx->hash_algo)
		    rc = GPG_ERR_DIGEST_ALGO;
		}
	      gcry_sexp_release (list);
	      if (rc)
		goto leave;
	    }

	  /* Get LABEL. */
	  list = gcry_sexp_find_token (ldata, "label", 0);
	  if (list)
	    {
	      s = gcry_sexp_nth_data (list, 1, &n);
	      if (!s)
		rc = GPG_ERR_NO_OBJ;
	      else if (n > 0)
		{
		  ctx->label = gcry_malloc (n);
		  if (!ctx->label)
		    rc = gpg_err_code_from_syserror ();
		  else
		    {
		      memcpy (ctx->label, s, n);
		      ctx->labellen = n;
		    }
		}
	      gcry_sexp_release (list);
	      if (rc)
		goto leave;
	    }
          /* Get optional RANDOM-OVERRIDE.  */
          list = gcry_sexp_find_token (ldata, "random-override", 0);
          if (list)
            {
              s = gcry_sexp_nth_data (list, 1, &n);
              if (!s)
                rc = GPG_ERR_NO_OBJ;
              else if (n > 0)
                {
                  random_override = gcry_malloc (n);
                  if (!random_override)
                    rc = gpg_err_code_from_syserror ();
                  else
                    {
                      memcpy (random_override, s, n);
                      random_override_len = n;
                    }
                }
              gcry_sexp_release (list);
              if (rc)
                goto leave;
            }

	  rc = oaep_encode (ret_mpi, ctx->nbits, ctx->hash_algo,
			    value, valuelen,
			    ctx->label, ctx->labellen,
                            random_override, random_override_len);

          gcry_free (random_override);
	}
    }
  else if (ctx->encoding == PUBKEY_ENC_PSS && lhash
	   && ctx->op == PUBKEY_OP_SIGN)
    {
      if (gcry_sexp_length (lhash) != 3)
        rc = GPG_ERR_INV_OBJ;
      else if ( !(s=gcry_sexp_nth_data (lhash, 1, &n)) || !n )
        rc = GPG_ERR_INV_OBJ;
      else
        {
          const void * value;
          size_t valuelen;
          void *random_override = NULL;
          size_t random_override_len = 0;

	  ctx->hash_algo = get_hash_algo (s, n);

          if (!ctx->hash_algo)
            rc = GPG_ERR_DIGEST_ALGO;
          else if ( !(value=gcry_sexp_nth_data (lhash, 2, &valuelen))
                    || !valuelen )
            rc = GPG_ERR_INV_OBJ;
          else
	    {
	      gcry_sexp_t list;

	      /* Get SALT-LENGTH. */
	      list = gcry_sexp_find_token (ldata, "salt-length", 0);
	      if (list)
		{
		  s = gcry_sexp_nth_data (list, 1, &n);
		  if (!s)
		    {
		      rc = GPG_ERR_NO_OBJ;
		      goto leave;
		    }
		  ctx->saltlen = (unsigned int)strtoul (s, NULL, 10);
		  gcry_sexp_release (list);
		}

              /* Get optional RANDOM-OVERRIDE.  */
              list = gcry_sexp_find_token (ldata, "random-override", 0);
              if (list)
                {
                  s = gcry_sexp_nth_data (list, 1, &n);
                  if (!s)
                    rc = GPG_ERR_NO_OBJ;
                  else if (n > 0)
                    {
                      random_override = gcry_malloc (n);
                      if (!random_override)
                        rc = gpg_err_code_from_syserror ();
                      else
                        {
                          memcpy (random_override, s, n);
                          random_override_len = n;
                        }
                    }
                  gcry_sexp_release (list);
                  if (rc)
                    goto leave;
                }

              /* Encode the data.  (NBITS-1 is due to 8.1.1, step 1.) */
	      rc = pss_encode (ret_mpi, ctx->nbits - 1, ctx->hash_algo,
			       value, valuelen, ctx->saltlen,
                               random_override, random_override_len);

              gcry_free (random_override);
	    }
        }
    }
  else if (ctx->encoding == PUBKEY_ENC_PSS && lhash
	   && ctx->op == PUBKEY_OP_VERIFY)
    {
      if (gcry_sexp_length (lhash) != 3)
        rc = GPG_ERR_INV_OBJ;
      else if ( !(s=gcry_sexp_nth_data (lhash, 1, &n)) || !n )
        rc = GPG_ERR_INV_OBJ;
      else
        {
	  ctx->hash_algo = get_hash_algo (s, n);

          if (!ctx->hash_algo)
            rc = GPG_ERR_DIGEST_ALGO;
	  else
	    {
	      *ret_mpi = gcry_sexp_nth_mpi (lhash, 2, GCRYMPI_FMT_USG);
	      if (!*ret_mpi)
		rc = GPG_ERR_INV_OBJ;
	      ctx->verify_cmp = pss_verify_cmp;
	      ctx->verify_arg = *ret_mpi;
	    }
	}
    }
  else
    rc = GPG_ERR_CONFLICT;

 leave:
  gcry_sexp_release (ldata);
  gcry_sexp_release (lhash);
  gcry_sexp_release (lvalue);

  if (!rc)
    ctx->flags = parsed_flags;
  else
    {
      gcry_free (ctx->label);
      ctx->label = NULL;
    }

  return rc;
}

static void
init_encoding_ctx (struct pk_encoding_ctx *ctx, enum pk_operation op,
		   unsigned int nbits)
{
  ctx->op = op;
  ctx->nbits = nbits;
  ctx->encoding = PUBKEY_ENC_UNKNOWN;
  ctx->flags = 0;
  ctx->hash_algo = GCRY_MD_SHA1;
  ctx->label = NULL;
  ctx->labellen = 0;
  ctx->saltlen = 20;
  ctx->verify_cmp = NULL;
  ctx->verify_arg = NULL;
}


/*
   Do a PK encrypt operation

   Caller has to provide a public key as the SEXP pkey and data as a
   SEXP with just one MPI in it. Alternatively S_DATA might be a
   complex S-Expression, similar to the one used for signature
   verification.  This provides a flag which allows to handle PKCS#1
   block type 2 padding.  The function returns a sexp which may be
   passed to to pk_decrypt.

   Returns: 0 or an errorcode.

   s_data = See comment for sexp_data_to_mpi
   s_pkey = <key-as-defined-in-sexp_to_key>
   r_ciph = (enc-val
               (<algo>
                 (<param_name1> <mpi>)
                 ...
                 (<param_namen> <mpi>)
               ))

*/
gcry_error_t
gcry_pk_encrypt (gcry_sexp_t *r_ciph, gcry_sexp_t s_data, gcry_sexp_t s_pkey)
{
  gcry_mpi_t *pkey = NULL, data = NULL, *ciph = NULL;
  const char *algo_name, *algo_elems;
  struct pk_encoding_ctx ctx;
  gcry_err_code_t rc;
  gcry_pk_spec_t *pubkey = NULL;
  gcry_module_t module = NULL;

  *r_ciph = NULL;

  REGISTER_DEFAULT_PUBKEYS;

  /* Get the key. */
  rc = sexp_to_key (s_pkey, 0, NULL, &pkey, &module);
  if (rc)
    goto leave;

  gcry_assert (module);
  pubkey = (gcry_pk_spec_t *) module->spec;

  /* If aliases for the algorithm name exists, take the first one
     instead of the regular name to adhere to SPKI conventions.  We
     assume that the first alias name is the lowercase version of the
     regular one.  This change is required for compatibility with
     1.1.12 generated S-expressions. */
  algo_name = pubkey->aliases? *pubkey->aliases : NULL;
  if (!algo_name || !*algo_name)
    algo_name = pubkey->name;

  algo_elems = pubkey->elements_enc;

  /* Get the stuff we want to encrypt. */
  init_encoding_ctx (&ctx, PUBKEY_OP_ENCRYPT, gcry_pk_get_nbits (s_pkey));
  rc = sexp_data_to_mpi (s_data, &data, &ctx);
  if (rc)
    goto leave;

  /* Now we can encrypt DATA to CIPH. */
  ciph = gcry_calloc (strlen (algo_elems) + 1, sizeof (*ciph));
  if (!ciph)
    {
      rc = gpg_err_code_from_syserror ();
      goto leave;
    }
  rc = pubkey_encrypt (module->mod_id, ciph, data, pkey, ctx.flags);
  mpi_free (data);
  data = NULL;
  if (rc)
    goto leave;

  /* We did it.  Now build the return list */
  if (ctx.encoding == PUBKEY_ENC_OAEP
      || ctx.encoding == PUBKEY_ENC_PKCS1)
    {
      /* We need to make sure to return the correct length to avoid
         problems with missing leading zeroes.  We know that this
         encoding does only make sense with RSA thus we don't need to
         build the S-expression on the fly.  */
      unsigned char *em;
      size_t emlen = (ctx.nbits+7)/8;

      rc = octet_string_from_mpi (&em, NULL, ciph[0], emlen);
      if (rc)
        goto leave;
      rc = gcry_err_code (gcry_sexp_build (r_ciph, NULL,
                                           "(enc-val(%s(a%b)))",
                                           algo_name, (int)emlen, em));
      gcry_free (em);
      if (rc)
        goto leave;
    }
  else
    {
      char *string, *p;
      int i;
      size_t nelem = strlen (algo_elems);
      size_t needed = 19 + strlen (algo_name) + (nelem * 5);
      void **arg_list;

      /* Build the string.  */
      string = p = gcry_malloc (needed);
      if (!string)
        {
          rc = gpg_err_code_from_syserror ();
          goto leave;
        }
      p = stpcpy ( p, "(enc-val(" );
      p = stpcpy ( p, algo_name );
      for (i=0; algo_elems[i]; i++ )
        {
          *p++ = '(';
          *p++ = algo_elems[i];
          p = stpcpy ( p, "%m)" );
        }
      strcpy ( p, "))" );

      /* And now the ugly part: We don't have a function to pass an
       * array to a format string, so we have to do it this way :-(.  */
      /* FIXME: There is now such a format specifier, so we can
         change the code to be more clear. */
      arg_list = malloc (nelem * sizeof *arg_list);
      if (!arg_list)
        {
          rc = gpg_err_code_from_syserror ();
          goto leave;
        }

      for (i = 0; i < nelem; i++)
        arg_list[i] = ciph + i;

      rc = gcry_sexp_build_array (r_ciph, NULL, string, arg_list);
      free (arg_list);
      if (rc)
        BUG ();
      gcry_free (string);
    }

 leave:
  if (pkey)
    {
      release_mpi_array (pkey);
      gcry_free (pkey);
    }

  if (ciph)
    {
      release_mpi_array (ciph);
      gcry_free (ciph);
    }

  if (module)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }

  gcry_free (ctx.label);

  return gcry_error (rc);
}

/*
   Do a PK decrypt operation

   Caller has to provide a secret key as the SEXP skey and data in a
   format as created by gcry_pk_encrypt.  For historic reasons the
   function returns simply an MPI as an S-expression part; this is
   deprecated and the new method should be used which returns a real
   S-expressionl this is selected by adding at least an empty flags
   list to S_DATA.

   Returns: 0 or an errorcode.

   s_data = (enc-val
              [(flags [raw, pkcs1, oaep])]
              (<algo>
                (<param_name1> <mpi>)
                ...
                (<param_namen> <mpi>)
              ))
   s_skey = <key-as-defined-in-sexp_to_key>
   r_plain= Either an incomplete S-expression without the parentheses
            or if the flags list is used (even if empty) a real S-expression:
            (value PLAIN).  In raw mode (or no flags given) the returned value
            is to be interpreted as a signed MPI, thus it may have an extra
            leading zero octet even if not included in the original data.
            With pkcs1 or oaep decoding enabled the returned value is a
            verbatim octet string.
 */
gcry_error_t
gcry_pk_decrypt (gcry_sexp_t *r_plain, gcry_sexp_t s_data, gcry_sexp_t s_skey)
{
  gcry_mpi_t *skey = NULL, *data = NULL, plain = NULL;
  unsigned char *unpad = NULL;
  size_t unpadlen = 0;
  int modern, flags;
  struct pk_encoding_ctx ctx;
  gcry_err_code_t rc;
  gcry_module_t module_enc = NULL, module_key = NULL;

  *r_plain = NULL;
  ctx.label = NULL;

  REGISTER_DEFAULT_PUBKEYS;

  rc = sexp_to_key (s_skey, 1, NULL, &skey, &module_key);
  if (rc)
    goto leave;

  init_encoding_ctx (&ctx, PUBKEY_OP_DECRYPT, gcry_pk_get_nbits (s_skey));
  rc = sexp_to_enc (s_data, &data, &module_enc, &modern, &flags, &ctx);
  if (rc)
    goto leave;

  if (module_key->mod_id != module_enc->mod_id)
    {
      rc = GPG_ERR_CONFLICT; /* Key algo does not match data algo. */
      goto leave;
    }

  rc = pubkey_decrypt (module_key->mod_id, &plain, data, skey, flags);
  if (rc)
    goto leave;

  /* Do un-padding if necessary. */
  switch (ctx.encoding)
    {
    case PUBKEY_ENC_PKCS1:
      rc = pkcs1_decode_for_encryption (&unpad, &unpadlen,
                                        gcry_pk_get_nbits (s_skey), plain);
      mpi_free (plain);
      plain = NULL;
      if (!rc)
        rc = gcry_err_code (gcry_sexp_build (r_plain, NULL, "(value %b)",
                                             (int)unpadlen, unpad));
      break;

    case PUBKEY_ENC_OAEP:
      rc = oaep_decode (&unpad, &unpadlen,
                        gcry_pk_get_nbits (s_skey), ctx.hash_algo,
			plain, ctx.label, ctx.labellen);
      mpi_free (plain);
      plain = NULL;
      if (!rc)
        rc = gcry_err_code (gcry_sexp_build (r_plain, NULL, "(value %b)",
                                             (int)unpadlen, unpad));
      break;

    default:
      /* Raw format.  For backward compatibility we need to assume a
         signed mpi by using the sexp format string "%m".  */
      rc = gcry_err_code (gcry_sexp_build
                          (r_plain, NULL, modern? "(value %m)" : "%m", plain));
      break;
    }

 leave:
  gcry_free (unpad);

  if (skey)
    {
      release_mpi_array (skey);
      gcry_free (skey);
    }

  mpi_free (plain);

  if (data)
    {
      release_mpi_array (data);
      gcry_free (data);
    }

  if (module_key || module_enc)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      if (module_key)
	_gcry_module_release (module_key);
      if (module_enc)
	_gcry_module_release (module_enc);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }

  gcry_free (ctx.label);

  return gcry_error (rc);
}



/*
   Create a signature.

   Caller has to provide a secret key as the SEXP skey and data
   expressed as a SEXP list hash with only one element which should
   instantly be available as a MPI. Alternatively the structure given
   below may be used for S_HASH, it provides the abiliy to pass flags
   to the operation; the flags defined by now are "pkcs1" which does
   PKCS#1 block type 1 style padding and "pss" for PSS encoding.

   Returns: 0 or an errorcode.
            In case of 0 the function returns a new SEXP with the
            signature value; the structure of this signature depends on the
            other arguments but is always suitable to be passed to
            gcry_pk_verify

   s_hash = See comment for sexp_data_to_mpi

   s_skey = <key-as-defined-in-sexp_to_key>
   r_sig  = (sig-val
              (<algo>
                (<param_name1> <mpi>)
                ...
                (<param_namen> <mpi>))
             [(hash algo)])

  Note that (hash algo) in R_SIG is not used.
*/
gcry_error_t
gcry_pk_sign (gcry_sexp_t *r_sig, gcry_sexp_t s_hash, gcry_sexp_t s_skey)
{
  gcry_mpi_t *skey = NULL, hash = NULL, *result = NULL;
  gcry_pk_spec_t *pubkey = NULL;
  gcry_module_t module = NULL;
  const char *algo_name, *algo_elems;
  struct pk_encoding_ctx ctx;
  int i;
  gcry_err_code_t rc;

  *r_sig = NULL;

  REGISTER_DEFAULT_PUBKEYS;

  rc = sexp_to_key (s_skey, 1, NULL, &skey, &module);
  if (rc)
    goto leave;

  gcry_assert (module);
  pubkey = (gcry_pk_spec_t *) module->spec;
  algo_name = pubkey->aliases? *pubkey->aliases : NULL;
  if (!algo_name || !*algo_name)
    algo_name = pubkey->name;

  algo_elems = pubkey->elements_sig;

  /* Get the stuff we want to sign.  Note that pk_get_nbits does also
      work on a private key. */
  init_encoding_ctx (&ctx, PUBKEY_OP_SIGN, gcry_pk_get_nbits (s_skey));
  rc = sexp_data_to_mpi (s_hash, &hash, &ctx);
  if (rc)
    goto leave;

  result = gcry_calloc (strlen (algo_elems) + 1, sizeof (*result));
  if (!result)
    {
      rc = gpg_err_code_from_syserror ();
      goto leave;
    }
  rc = pubkey_sign (module->mod_id, result, hash, skey);
  if (rc)
    goto leave;

  if (ctx.encoding == PUBKEY_ENC_PSS
      || ctx.encoding == PUBKEY_ENC_PKCS1)
    {
      /* We need to make sure to return the correct length to avoid
         problems with missing leading zeroes.  We know that this
         encoding does only make sense with RSA thus we don't need to
         build the S-expression on the fly.  */
      unsigned char *em;
      size_t emlen = (ctx.nbits+7)/8;

      rc = octet_string_from_mpi (&em, NULL, result[0], emlen);
      if (rc)
        goto leave;
      rc = gcry_err_code (gcry_sexp_build (r_sig, NULL,
                                           "(sig-val(%s(s%b)))",
                                           algo_name, (int)emlen, em));
      gcry_free (em);
      if (rc)
        goto leave;
    }
  else
    {
      /* General purpose output encoding.  Do it on the fly.  */
      char *string, *p;
      size_t nelem, needed = strlen (algo_name) + 20;
      void **arg_list;

      nelem = strlen (algo_elems);

      /* Count elements, so that we can allocate enough space. */
      needed += 10 * nelem;

      /* Build the string. */
      string = p = gcry_malloc (needed);
      if (!string)
        {
          rc = gpg_err_code_from_syserror ();
          goto leave;
        }
      p = stpcpy (p, "(sig-val(");
      p = stpcpy (p, algo_name);
      for (i = 0; algo_elems[i]; i++)
        {
          *p++ = '(';
          *p++ = algo_elems[i];
          p = stpcpy (p, "%M)");
        }
      strcpy (p, "))");

      arg_list = malloc (nelem * sizeof *arg_list);
      if (!arg_list)
        {
          rc = gpg_err_code_from_syserror ();
          goto leave;
        }

      for (i = 0; i < nelem; i++)
        arg_list[i] = result + i;

      rc = gcry_sexp_build_array (r_sig, NULL, string, arg_list);
      free (arg_list);
      if (rc)
        BUG ();
      gcry_free (string);
    }

 leave:
  if (skey)
    {
      release_mpi_array (skey);
      gcry_free (skey);
    }

  if (hash)
    mpi_free (hash);

  if (result)
    {
      release_mpi_array (result);
      gcry_free (result);
    }

  return gcry_error (rc);
}


/*
   Verify a signature.

   Caller has to supply the public key pkey, the signature sig and his
   hashvalue data.  Public key has to be a standard public key given
   as an S-Exp, sig is a S-Exp as returned from gcry_pk_sign and data
   must be an S-Exp like the one in sign too.  */
gcry_error_t
gcry_pk_verify (gcry_sexp_t s_sig, gcry_sexp_t s_hash, gcry_sexp_t s_pkey)
{
  gcry_module_t module_key = NULL, module_sig = NULL;
  gcry_mpi_t *pkey = NULL, hash = NULL, *sig = NULL;
  struct pk_encoding_ctx ctx;
  gcry_err_code_t rc;

  REGISTER_DEFAULT_PUBKEYS;

  rc = sexp_to_key (s_pkey, 0, NULL, &pkey, &module_key);
  if (rc)
    goto leave;

  rc = sexp_to_sig (s_sig, &sig, &module_sig);
  if (rc)
    goto leave;

  /* Fixme: Check that the algorithm of S_SIG is compatible to the one
     of S_PKEY.  */

  if (module_key->mod_id != module_sig->mod_id)
    {
      rc = GPG_ERR_CONFLICT;
      goto leave;
    }

  /* Get the stuff we want to verify. */
  init_encoding_ctx (&ctx, PUBKEY_OP_VERIFY, gcry_pk_get_nbits (s_pkey));
  rc = sexp_data_to_mpi (s_hash, &hash, &ctx);
  if (rc)
    goto leave;

  rc = pubkey_verify (module_key->mod_id, hash, sig, pkey,
		      ctx.verify_cmp, &ctx);

 leave:
  if (pkey)
    {
      release_mpi_array (pkey);
      gcry_free (pkey);
    }
  if (sig)
    {
      release_mpi_array (sig);
      gcry_free (sig);
    }
  if (hash)
    mpi_free (hash);

  if (module_key || module_sig)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      if (module_key)
	_gcry_module_release (module_key);
      if (module_sig)
	_gcry_module_release (module_sig);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }

  return gcry_error (rc);
}


/*
   Test a key.

   This may be used either for a public or a secret key to see whether
   the internal structure is okay.

   Returns: 0 or an errorcode.

   s_key = <key-as-defined-in-sexp_to_key> */
gcry_error_t
gcry_pk_testkey (gcry_sexp_t s_key)
{
  gcry_module_t module = NULL;
  gcry_mpi_t *key = NULL;
  gcry_err_code_t rc;

  REGISTER_DEFAULT_PUBKEYS;

  /* Note we currently support only secret key checking. */
  rc = sexp_to_key (s_key, 1, NULL, &key, &module);
  if (! rc)
    {
      rc = pubkey_check_secret_key (module->mod_id, key);
      release_mpi_array (key);
      gcry_free (key);
    }
  return gcry_error (rc);
}


/*
  Create a public key pair and return it in r_key.
  How the key is created depends on s_parms:
  (genkey
   (algo
     (parameter_name_1 ....)
      ....
     (parameter_name_n ....)
  ))
  The key is returned in a format depending on the
  algorithm. Both, private and secret keys are returned
  and optionally some additional informatin.
  For elgamal we return this structure:
  (key-data
   (public-key
     (elg
 	(p <mpi>)
 	(g <mpi>)
 	(y <mpi>)
     )
   )
   (private-key
     (elg
 	(p <mpi>)
 	(g <mpi>)
 	(y <mpi>)
 	(x <mpi>)
     )
   )
   (misc-key-info
      (pm1-factors n1 n2 ... nn)
   ))
 */
gcry_error_t
gcry_pk_genkey (gcry_sexp_t *r_key, gcry_sexp_t s_parms)
{
  gcry_pk_spec_t *pubkey = NULL;
  gcry_module_t module = NULL;
  gcry_sexp_t list = NULL;
  gcry_sexp_t l2 = NULL;
  gcry_sexp_t l3 = NULL;
  char *name = NULL;
  size_t n;
  gcry_err_code_t rc = GPG_ERR_NO_ERROR;
  int i, j;
  const char *algo_name = NULL;
  int algo;
  const char *sec_elems = NULL, *pub_elems = NULL;
  gcry_mpi_t skey[12];
  gcry_mpi_t *factors = NULL;
  gcry_sexp_t extrainfo = NULL;
  unsigned int nbits = 0;
  unsigned long use_e = 0;

  skey[0] = NULL;
  *r_key = NULL;

  REGISTER_DEFAULT_PUBKEYS;

  list = gcry_sexp_find_token (s_parms, "genkey", 0);
  if (!list)
    {
      rc = GPG_ERR_INV_OBJ; /* Does not contain genkey data. */
      goto leave;
    }

  l2 = gcry_sexp_cadr (list);
  gcry_sexp_release (list);
  list = l2;
  l2 = NULL;
  if (! list)
    {
      rc = GPG_ERR_NO_OBJ; /* No cdr for the genkey. */
      goto leave;
    }

  name = _gcry_sexp_nth_string (list, 0);
  if (!name)
    {
      rc = GPG_ERR_INV_OBJ; /* Algo string missing.  */
      goto leave;
    }

  ath_mutex_lock (&pubkeys_registered_lock);
  module = gcry_pk_lookup_name (name);
  ath_mutex_unlock (&pubkeys_registered_lock);
  gcry_free (name);
  name = NULL;
  if (!module)
    {
      rc = GPG_ERR_PUBKEY_ALGO; /* Unknown algorithm.  */
      goto leave;
    }

  pubkey = (gcry_pk_spec_t *) module->spec;
  algo = module->mod_id;
  algo_name = pubkey->aliases? *pubkey->aliases : NULL;
  if (!algo_name || !*algo_name)
    algo_name = pubkey->name;
  pub_elems = pubkey->elements_pkey;
  sec_elems = pubkey->elements_skey;
  if (strlen (sec_elems) >= DIM(skey))
    BUG ();

  /* Handle the optional rsa-use-e element.  Actually this belong into
     the algorithm module but we have this parameter in the public
     module API, so we need to parse it right here.  */
  l2 = gcry_sexp_find_token (list, "rsa-use-e", 0);
  if (l2)
    {
      char buf[50];
      const char *s;

      s = gcry_sexp_nth_data (l2, 1, &n);
      if ( !s || n >= DIM (buf) - 1 )
        {
          rc = GPG_ERR_INV_OBJ; /* No value or value too large.  */
          goto leave;
        }
      memcpy (buf, s, n);
      buf[n] = 0;
      use_e = strtoul (buf, NULL, 0);
      gcry_sexp_release (l2);
      l2 = NULL;
    }
  else
    use_e = 65537; /* Not given, use the value generated by old versions. */


  /* Get the "nbits" parameter.  */
  l2 = gcry_sexp_find_token (list, "nbits", 0);
  if (l2)
    {
      char buf[50];
      const char *s;

      s = gcry_sexp_nth_data (l2, 1, &n);
      if (!s || n >= DIM (buf) - 1 )
        {
          rc = GPG_ERR_INV_OBJ; /* NBITS given without a cdr.  */
          goto leave;
        }
      memcpy (buf, s, n);
      buf[n] = 0;
      nbits = (unsigned int)strtoul (buf, NULL, 0);
      gcry_sexp_release (l2); l2 = NULL;
    }
  else
    nbits = 0;

  /* Pass control to the algorithm module. */
  rc = pubkey_generate (module->mod_id, nbits, use_e, list, skey,
                        &factors, &extrainfo);
  gcry_sexp_release (list); list = NULL;
  if (rc)
    goto leave;

  /* Key generation succeeded: Build an S-expression.  */
  {
    char *string, *p;
    size_t nelem=0, nelem_cp = 0, needed=0;
    gcry_mpi_t mpis[30];
    int percent_s_idx = -1;

    /* Estimate size of format string.  */
    nelem = strlen (pub_elems) + strlen (sec_elems);
    if (factors)
      {
        for (i = 0; factors[i]; i++)
          nelem++;
      }
    nelem_cp = nelem;

    needed += nelem * 10;
    /* (+5 is for EXTRAINFO ("%S")).  */
    needed += 2 * strlen (algo_name) + 300 + 5;
    if (nelem > DIM (mpis))
      BUG ();

    /* Build the string. */
    nelem = 0;
    string = p = gcry_malloc (needed);
    if (!string)
      {
        rc = gpg_err_code_from_syserror ();
        goto leave;
      }
    p = stpcpy (p, "(key-data");
    p = stpcpy (p, "(public-key(");
    p = stpcpy (p, algo_name);
    for(i = 0; pub_elems[i]; i++)
      {
        *p++ = '(';
        *p++ = pub_elems[i];
        p = stpcpy (p, "%m)");
        mpis[nelem++] = skey[i];
      }
    if (extrainfo && (algo == GCRY_PK_ECDSA || algo == GCRY_PK_ECDH))
      {
        /* Very ugly hack to insert the used curve parameter into the
           list of public key parameters.  */
        percent_s_idx = nelem;
        p = stpcpy (p, "%S");
      }
    p = stpcpy (p, "))");
    p = stpcpy (p, "(private-key(");
    p = stpcpy (p, algo_name);
    for (i = 0; sec_elems[i]; i++)
      {
        *p++ = '(';
        *p++ = sec_elems[i];
        p = stpcpy (p, "%m)");
        mpis[nelem++] = skey[i];
      }
    p = stpcpy (p, "))");

    /* Hack to make release_mpi_array() work.  */
    skey[i] = NULL;

    if (extrainfo && percent_s_idx == -1)
      {
        /* If we have extrainfo we should not have any factors.  */
        p = stpcpy (p, "%S");
      }
    else if (factors && factors[0])
      {
        p = stpcpy (p, "(misc-key-info(pm1-factors");
        for(i = 0; factors[i]; i++)
          {
            p = stpcpy (p, "%m");
            mpis[nelem++] = factors[i];
          }
        p = stpcpy (p, "))");
      }
    strcpy (p, ")");
    gcry_assert (p - string < needed);

    while (nelem < DIM (mpis))
      mpis[nelem++] = NULL;

    {
      int elem_n = strlen (pub_elems) + strlen (sec_elems);
      void **arg_list;

      /* Allocate one extra for EXTRAINFO ("%S").  */
      arg_list = gcry_calloc (nelem_cp+1, sizeof *arg_list);
      if (!arg_list)
        {
          rc = gpg_err_code_from_syserror ();
          goto leave;
        }
      for (i = j = 0; i < elem_n; i++)
        {
          if (i == percent_s_idx)
            arg_list[j++] = &extrainfo;
          arg_list[j++] = mpis + i;
        }
      if (extrainfo && percent_s_idx == -1)
        arg_list[j] = &extrainfo;
      else if (factors && factors[0])
        {
          for (; i < nelem_cp; i++)
            arg_list[j++] = factors + i - elem_n;
        }
      rc = gcry_sexp_build_array (r_key, NULL, string, arg_list);
      gcry_free (arg_list);
      if (rc)
	BUG ();
      gcry_assert (DIM (mpis) == 30); /* Reminder to make sure that
                                         the array gets increased if
                                         new parameters are added. */
    }
    gcry_free (string);
  }

 leave:
  gcry_free (name);
  gcry_sexp_release (extrainfo);
  release_mpi_array (skey);
  /* Don't free SKEY itself, it is an stack allocated array. */

  if (factors)
    {
      release_mpi_array ( factors );
      gcry_free (factors);
    }

  gcry_sexp_release (l3);
  gcry_sexp_release (l2);
  gcry_sexp_release (list);

  if (module)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }

  return gcry_error (rc);
}


/*
   Get the number of nbits from the public key.

   Hmmm: Should we have really this function or is it better to have a
   more general function to retrieve different properties of the key?  */
unsigned int
gcry_pk_get_nbits (gcry_sexp_t key)
{
  gcry_module_t module = NULL;
  gcry_pk_spec_t *pubkey;
  gcry_mpi_t *keyarr = NULL;
  unsigned int nbits = 0;
  gcry_err_code_t rc;

  REGISTER_DEFAULT_PUBKEYS;

  rc = sexp_to_key (key, 0, NULL, &keyarr, &module);
  if (rc == GPG_ERR_INV_OBJ)
    rc = sexp_to_key (key, 1, NULL, &keyarr, &module);
  if (rc)
    return 0; /* Error - 0 is a suitable indication for that. */

  pubkey = (gcry_pk_spec_t *) module->spec;
  nbits = (*pubkey->get_nbits) (module->mod_id, keyarr);

  ath_mutex_lock (&pubkeys_registered_lock);
  _gcry_module_release (module);
  ath_mutex_unlock (&pubkeys_registered_lock);

  release_mpi_array (keyarr);
  gcry_free (keyarr);

  return nbits;
}


/* Return the so called KEYGRIP which is the SHA-1 hash of the public
   key parameters expressed in a way depending on the algorithm.

   ARRAY must either be 20 bytes long or NULL; in the latter case a
   newly allocated array of that size is returned, otherwise ARRAY or
   NULL is returned to indicate an error which is most likely an
   unknown algorithm.  The function accepts public or secret keys. */
unsigned char *
gcry_pk_get_keygrip (gcry_sexp_t key, unsigned char *array)
{
  gcry_sexp_t list = NULL, l2 = NULL;
  gcry_pk_spec_t *pubkey = NULL;
  gcry_module_t module = NULL;
  pk_extra_spec_t *extraspec;
  const char *s;
  char *name = NULL;
  int idx;
  const char *elems;
  gcry_md_hd_t md = NULL;
  int okay = 0;

  REGISTER_DEFAULT_PUBKEYS;

  /* Check that the first element is valid. */
  list = gcry_sexp_find_token (key, "public-key", 0);
  if (! list)
    list = gcry_sexp_find_token (key, "private-key", 0);
  if (! list)
    list = gcry_sexp_find_token (key, "protected-private-key", 0);
  if (! list)
    list = gcry_sexp_find_token (key, "shadowed-private-key", 0);
  if (! list)
    return NULL; /* No public- or private-key object. */

  l2 = gcry_sexp_cadr (list);
  gcry_sexp_release (list);
  list = l2;
  l2 = NULL;

  name = _gcry_sexp_nth_string (list, 0);
  if (!name)
    goto fail; /* Invalid structure of object. */

  ath_mutex_lock (&pubkeys_registered_lock);
  module = gcry_pk_lookup_name (name);
  ath_mutex_unlock (&pubkeys_registered_lock);

  if (!module)
    goto fail; /* Unknown algorithm.  */

  pubkey = (gcry_pk_spec_t *) module->spec;
  extraspec = module->extraspec;

  elems = pubkey->elements_grip;
  if (!elems)
    goto fail; /* No grip parameter.  */

  if (gcry_md_open (&md, GCRY_MD_SHA1, 0))
    goto fail;

  if (extraspec && extraspec->comp_keygrip)
    {
      /* Module specific method to compute a keygrip.  */
      if (extraspec->comp_keygrip (md, list))
        goto fail;
    }
  else
    {
      /* Generic method to compute a keygrip.  */
      for (idx = 0, s = elems; *s; s++, idx++)
        {
          const char *data;
          size_t datalen;
          char buf[30];

          l2 = gcry_sexp_find_token (list, s, 1);
          if (! l2)
            goto fail;
          data = gcry_sexp_nth_data (l2, 1, &datalen);
          if (! data)
            goto fail;

          snprintf (buf, sizeof buf, "(1:%c%u:", *s, (unsigned int)datalen);
          gcry_md_write (md, buf, strlen (buf));
          gcry_md_write (md, data, datalen);
          gcry_sexp_release (l2);
          l2 = NULL;
          gcry_md_write (md, ")", 1);
        }
    }

  if (!array)
    {
      array = gcry_malloc (20);
      if (! array)
        goto fail;
    }

  memcpy (array, gcry_md_read (md, GCRY_MD_SHA1), 20);
  okay = 1;

 fail:
  gcry_free (name);
  gcry_sexp_release (l2);
  gcry_md_close (md);
  gcry_sexp_release (list);
  return okay? array : NULL;
}



const char *
gcry_pk_get_curve (gcry_sexp_t key, int iterator, unsigned int *r_nbits)
{
  gcry_mpi_t *pkey = NULL;
  gcry_sexp_t list = NULL;
  gcry_sexp_t l2;
  gcry_module_t module = NULL;
  pk_extra_spec_t *extraspec;
  char *name = NULL;
  const char *result = NULL;
  int want_private = 1;

  if (r_nbits)
    *r_nbits = 0;

  REGISTER_DEFAULT_PUBKEYS;

  if (key)
    {
      iterator = 0;

      /* Check that the first element is valid. */
      list = gcry_sexp_find_token (key, "public-key", 0);
      if (list)
        want_private = 0;
      if (!list)
        list = gcry_sexp_find_token (key, "private-key", 0);
      if (!list)
        return NULL; /* No public- or private-key object. */

      l2 = gcry_sexp_cadr (list);
      gcry_sexp_release (list);
      list = l2;
      l2 = NULL;

      name = _gcry_sexp_nth_string (list, 0);
      if (!name)
        goto leave; /* Invalid structure of object. */

      /* Get the key.  We pass the names of the parameters for
         override_elems; this allows to call this function without the
         actual public key parameter.  */
      if (sexp_to_key (key, want_private, "pabgn", &pkey, &module))
        goto leave;
    }
  else
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      module = gcry_pk_lookup_name ("ecc");
      ath_mutex_unlock (&pubkeys_registered_lock);
      if (!module)
        goto leave;
    }

  extraspec = module->extraspec;
  if (!extraspec || !extraspec->get_curve)
    goto leave;

  result = extraspec->get_curve (pkey, iterator, r_nbits);

 leave:
  if (pkey)
    {
      release_mpi_array (pkey);
      gcry_free (pkey);
    }
  if (module)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }
  gcry_free (name);
  gcry_sexp_release (list);
  return result;
}



gcry_sexp_t
gcry_pk_get_param (int algo, const char *name)
{
  gcry_module_t module = NULL;
  pk_extra_spec_t *extraspec;
  gcry_sexp_t result = NULL;

  if (algo != GCRY_PK_ECDSA && algo != GCRY_PK_ECDH)
    return NULL;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  module = gcry_pk_lookup_name ("ecc");
  ath_mutex_unlock (&pubkeys_registered_lock);
  if (module)
    {
      extraspec = module->extraspec;
      if (extraspec && extraspec->get_curve_param)
        result = extraspec->get_curve_param (name);

      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }
  return result;
}



gcry_error_t
gcry_pk_ctl (int cmd, void *buffer, size_t buflen)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;

  REGISTER_DEFAULT_PUBKEYS;

  switch (cmd)
    {
    case GCRYCTL_DISABLE_ALGO:
      /* This one expects a buffer pointing to an integer with the
         algo number.  */
      if ((! buffer) || (buflen != sizeof (int)))
	err = GPG_ERR_INV_ARG;
      else
	disable_pubkey_algo (*((int *) buffer));
      break;

    default:
      err = GPG_ERR_INV_OP;
    }

  return gcry_error (err);
}


/* Return information about the given algorithm

   WHAT selects the kind of information returned:

    GCRYCTL_TEST_ALGO:
        Returns 0 when the specified algorithm is available for use.
        Buffer must be NULL, nbytes  may have the address of a variable
        with the required usage of the algorithm. It may be 0 for don't
        care or a combination of the GCRY_PK_USAGE_xxx flags;

    GCRYCTL_GET_ALGO_USAGE:
        Return the usage flags for the given algo.  An invalid algo
        returns 0.  Disabled algos are ignored here because we
        only want to know whether the algo is at all capable of
        the usage.

   Note: Because this function is in most cases used to return an
   integer value, we can make it easier for the caller to just look at
   the return value.  The caller will in all cases consult the value
   and thereby detecting whether a error occurred or not (i.e. while
   checking the block size) */
gcry_error_t
gcry_pk_algo_info (int algorithm, int what, void *buffer, size_t *nbytes)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;

  switch (what)
    {
    case GCRYCTL_TEST_ALGO:
      {
	int use = nbytes ? *nbytes : 0;
	if (buffer)
	  err = GPG_ERR_INV_ARG;
	else if (check_pubkey_algo (algorithm, use))
	  err = GPG_ERR_PUBKEY_ALGO;
	break;
      }

    case GCRYCTL_GET_ALGO_USAGE:
      {
	gcry_module_t pubkey;
	int use = 0;

	REGISTER_DEFAULT_PUBKEYS;

	ath_mutex_lock (&pubkeys_registered_lock);
	pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
	if (pubkey)
	  {
	    use = ((gcry_pk_spec_t *) pubkey->spec)->use;
	    _gcry_module_release (pubkey);
	  }
	ath_mutex_unlock (&pubkeys_registered_lock);

	/* FIXME? */
	*nbytes = use;

	break;
      }

    case GCRYCTL_GET_ALGO_NPKEY:
      {
	/* FIXME?  */
	int npkey = pubkey_get_npkey (algorithm);
	*nbytes = npkey;
	break;
      }
    case GCRYCTL_GET_ALGO_NSKEY:
      {
	/* FIXME?  */
	int nskey = pubkey_get_nskey (algorithm);
	*nbytes = nskey;
	break;
      }
    case GCRYCTL_GET_ALGO_NSIGN:
      {
	/* FIXME?  */
	int nsign = pubkey_get_nsig (algorithm);
	*nbytes = nsign;
	break;
      }
    case GCRYCTL_GET_ALGO_NENCR:
      {
	/* FIXME?  */
	int nencr = pubkey_get_nenc (algorithm);
	*nbytes = nencr;
	break;
      }

    default:
      err = GPG_ERR_INV_OP;
    }

  return gcry_error (err);
}


/* Explicitly initialize this module.  */
gcry_err_code_t
_gcry_pk_init (void)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;

  REGISTER_DEFAULT_PUBKEYS;

  return err;
}


gcry_err_code_t
_gcry_pk_module_lookup (int algorithm, gcry_module_t *module)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;
  gcry_module_t pubkey;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  pubkey = _gcry_module_lookup_id (pubkeys_registered, algorithm);
  if (pubkey)
    *module = pubkey;
  else
    err = GPG_ERR_PUBKEY_ALGO;
  ath_mutex_unlock (&pubkeys_registered_lock);

  return err;
}


void
_gcry_pk_module_release (gcry_module_t module)
{
  ath_mutex_lock (&pubkeys_registered_lock);
  _gcry_module_release (module);
  ath_mutex_unlock (&pubkeys_registered_lock);
}

/* Get a list consisting of the IDs of the loaded pubkey modules.  If
   LIST is zero, write the number of loaded pubkey modules to
   LIST_LENGTH and return.  If LIST is non-zero, the first
   *LIST_LENGTH algorithm IDs are stored in LIST, which must be of
   according size.  In case there are less pubkey modules than
   *LIST_LENGTH, *LIST_LENGTH is updated to the correct number.  */
gcry_error_t
gcry_pk_list (int *list, int *list_length)
{
  gcry_err_code_t err = GPG_ERR_NO_ERROR;

  ath_mutex_lock (&pubkeys_registered_lock);
  err = _gcry_module_list (pubkeys_registered, list, list_length);
  ath_mutex_unlock (&pubkeys_registered_lock);

  return err;
}


/* Run the selftests for pubkey algorithm ALGO with optional reporting
   function REPORT.  */
gpg_error_t
_gcry_pk_selftest (int algo, int extended, selftest_report_func_t report)
{
  gcry_module_t module = NULL;
  pk_extra_spec_t *extraspec = NULL;
  gcry_err_code_t ec = 0;

  REGISTER_DEFAULT_PUBKEYS;

  ath_mutex_lock (&pubkeys_registered_lock);
  module = _gcry_module_lookup_id (pubkeys_registered, algo);
  if (module && !(module->flags & FLAG_MODULE_DISABLED))
    extraspec = module->extraspec;
  ath_mutex_unlock (&pubkeys_registered_lock);
  if (extraspec && extraspec->selftest)
    ec = extraspec->selftest (algo, extended, report);
  else
    {
      ec = GPG_ERR_PUBKEY_ALGO;
      if (report)
        report ("pubkey", algo, "module",
                module && !(module->flags & FLAG_MODULE_DISABLED)?
                "no selftest available" :
                module? "algorithm disabled" : "algorithm not found");
    }

  if (module)
    {
      ath_mutex_lock (&pubkeys_registered_lock);
      _gcry_module_release (module);
      ath_mutex_unlock (&pubkeys_registered_lock);
    }
  return gpg_error (ec);
}


/* This function is only used by ac.c!  */
gcry_err_code_t
_gcry_pk_get_elements (int algo, char **enc, char **sig)
{
  gcry_module_t pubkey;
  gcry_pk_spec_t *spec;
  gcry_err_code_t err;
  char *enc_cp;
  char *sig_cp;

  REGISTER_DEFAULT_PUBKEYS;

  enc_cp = NULL;
  sig_cp = NULL;
  spec = NULL;

  pubkey = _gcry_module_lookup_id (pubkeys_registered, algo);
  if (! pubkey)
    {
      err = GPG_ERR_INTERNAL;
      goto out;
    }
  spec = pubkey->spec;

  if (enc)
    {
      enc_cp = strdup (spec->elements_enc);
      if (! enc_cp)
	{
	  err = gpg_err_code_from_syserror ();
	  goto out;
	}
    }

  if (sig)
    {
      sig_cp = strdup (spec->elements_sig);
      if (! sig_cp)
	{
	  err = gpg_err_code_from_syserror ();
	  goto out;
	}
    }

  if (enc)
    *enc = enc_cp;
  if (sig)
    *sig = sig_cp;
  err = 0;

 out:

  _gcry_module_release (pubkey);
  if (err)
    {
      free (enc_cp);
      free (sig_cp);
    }

  return err;
}
