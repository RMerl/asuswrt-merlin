/**
 * crypto.c - Routines for dealing with encrypted files.  Part of the
 *            Linux-NTFS project.
 *
 * Copyright (c) 2005      Yuval Fledel
 * Copyright (c) 2005-2007 Anton Altaparmakov
 * Copyright (c) 2007      Yura Pakhuchiy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * TODO: Cleanup this file. Write nice descriptions for non-exported functions
 * and maybe clean up namespace (not necessary for all functions to belong to
 * ntfs_crypto, we can have ntfs_fek, ntfs_rsa, etc.., but there should be
 * maximum 2-3 namespaces, not every function begins with it own namespace
 * like now).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "attrib.h"
#include "types.h"
#include "volume.h"
#include "debug.h"
#include "dir.h"
#include "layout.h"
#include "crypto.h"

#ifdef ENABLE_CRYPTO

#include <gcrypt.h>
#include <gnutls/pkcs12.h>
#include <gnutls/x509.h>

#include <libconfig.h>

#define NTFS_CONFIG_PATH_SYSTEM		"/etc/libntfs/config"
#define NTFS_CONFIG_PATH_USER		".libntfs/config"

#define NTFS_SHA1_THUMBPRINT_SIZE	0x14

#define NTFS_CRED_TYPE_CERT_THUMBPRINT	const_cpu_to_le32(3)

#define NTFS_EFS_CERT_PURPOSE_OID_DDF	"1.3.6.1.4.1.311.10.3.4"
#define NTFS_EFS_CERT_PURPOSE_OID_DRF	"1.3.6.1.4.1.311.10.3.4.1"

#define NTFS_EFS_SECTOR_SIZE		512

typedef enum {
	DF_TYPE_UNKNOWN,
	DF_TYPE_DDF,
	DF_TYPE_DRF,
} NTFS_DF_TYPES;

/**
 * enum NTFS_CRYPTO_ALGORITHMS - List of crypto algorithms used by EFS (32 bit)
 *
 * To choose which one is used in Windows, create or set the REG_DWORD registry
 * key HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\EFS\
 * AlgorithmID to the value of your chosen crypto algorithm, e.g. to use DesX,
 * set AlgorithmID to 0x6604.
 *
 * Note that the Windows versions I have tried so far (all are high crypto
 * enabled) ignore the AlgorithmID value if it is not one of CALG_3DES,
 * CALG_DESX, or CALG_AES_256, i.e. you cannot select CALG_DES at all using
 * this registry key.  It would be interesting to check out encryption on one
 * of the "crippled" crypto Windows versions...
 */
typedef enum {
	CALG_DES	= const_cpu_to_le32(0x6601),
	/* If not one of the below three, fall back to standard Des. */
	CALG_3DES	= const_cpu_to_le32(0x6603),
	CALG_DESX	= const_cpu_to_le32(0x6604),
	CALG_AES_256	= const_cpu_to_le32(0x6610),
} NTFS_CRYPTO_ALGORITHMS;

/**
 * struct ntfs_fek - Decrypted, in-memory file encryption key.
 */
struct _ntfs_fek {
	gcry_cipher_hd_t gcry_cipher_hd;
	le32 alg_id;
	u8 *key_data;
	gcry_cipher_hd_t *des_gcry_cipher_hd_ptr;
};

typedef struct _ntfs_fek ntfs_fek;

struct _ntfs_crypto_attr {
	ntfs_fek *fek;
};

typedef struct {
	u64 in_whitening, out_whitening;
	gcry_cipher_hd_t gcry_cipher_hd;
} ntfs_desx_ctx;

ntfschar NTFS_EFS[5] = {
	const_cpu_to_le16('$'), const_cpu_to_le16('E'), const_cpu_to_le16('F'),
	const_cpu_to_le16('S'), const_cpu_to_le16(0)
};

typedef struct {
	gcry_sexp_t key;
	NTFS_DF_TYPES df_type;
	char thumbprint[NTFS_SHA1_THUMBPRINT_SIZE];
} ntfs_rsa_private_key_t;

/*
 * Yes, global variables sucks, but we need to keep whether we performed
 * gcrypt/gnutls global initialization and keep user's RSA keys.
 */
typedef struct {
	int initialized;
	int desx_alg_id;
	gcry_module_t desx_module;
	ntfs_rsa_private_key_t **rsa_key;
	int nr_rsa_keys;
} ntfs_crypto_ctx_t;

static ntfs_crypto_ctx_t ntfs_crypto_ctx = {
	.desx_alg_id = -1,
	.desx_module = NULL,
};

/**
 * ntfs_pkcs12_load_pfxfile
 */
static int ntfs_pkcs12_load_pfxfile(const char *keyfile, u8 **pfx,
		unsigned *pfx_size)
{
	int f, to_read, total, attempts, br;
	struct stat key_stat;

	if (!keyfile || !pfx || !pfx_size) {
		ntfs_log_error("You have to specify the key file, a pointer "
				"to hold the key file contents, and a pointer "
				"to hold the size of the key file contents.\n");
		return -1;
	}
	f = open(keyfile, O_RDONLY);
	if (f == -1) {
		ntfs_log_perror("Failed to open key file");
		return -1;
	}
	if (fstat(f, &key_stat) == -1) {
		ntfs_log_perror("Failed to stat key file");
		goto file_out;
	}
	if (!S_ISREG(key_stat.st_mode)) {
		ntfs_log_error("Key file is not a regular file, cannot read "
				"it.\n");
		goto file_out;
	}
	if (!key_stat.st_size) {
		ntfs_log_error("Key file has zero size.\n");
		goto file_out;
	}
	*pfx = malloc(key_stat.st_size + 1);
	if (!*pfx) {
		ntfs_log_perror("Failed to allocate buffer for key file "
			"contents");
		goto file_out;
	}
	to_read = key_stat.st_size;
	total = attempts = 0;
	do {
		br = read(f, *pfx + total, to_read);
		if (br == -1) {
			ntfs_log_perror("Failed to read from key file");
			goto free_out;
		}
		if (!br)
			attempts++;
		to_read -= br;
		total += br;
	} while (to_read > 0 && attempts < 3);
	close(f);
	/* Make sure it is zero terminated. */
	(*pfx)[key_stat.st_size] = 0;
	*pfx_size = key_stat.st_size;
	return 0;
free_out:
	free(*pfx);
file_out:
	close(f);
	return -1;
}

/**
 * ntfs_rsa_private_key_import_from_gnutls
 */
static gcry_sexp_t ntfs_rsa_private_key_import_from_gnutls(
		gnutls_x509_privkey_t priv_key)
{
	int i, j;
	size_t tmp_size;
	gnutls_datum_t rd[6];
	gcry_mpi_t rm[6];
	gcry_sexp_t rsa_key;

	/* Extract the RSA parameters from the GNU TLS private key. */
	if (gnutls_x509_privkey_export_rsa_raw(priv_key, &rd[0], &rd[1],
			&rd[2], &rd[3], &rd[4], &rd[5])) {
		ntfs_log_error("Failed to export rsa parameters.  (Is the "
				"key an RSA private key?)\n");
		return NULL;
	}
	/* Convert each RSA parameter to MPI format. */
	for (i = 0; i < 6; i++) {
		if (gcry_mpi_scan(&rm[i], GCRYMPI_FMT_USG, rd[i].data,
				rd[i].size, &tmp_size) != GPG_ERR_NO_ERROR) {
			ntfs_log_error("Failed to convert RSA parameter %i "
					"to mpi format (size %d)\n", i,
					rd[i].size);
			rsa_key = NULL;
			break;
		}
	}
	/* Release the no longer needed datum values. */
	for (j = 0; j < 6; j++) {
		if (rd[j].data && rd[j].size)
			gnutls_free(rd[j].data);
	}
	/*
	 * Build the gcrypt private key, note libgcrypt uses p and q inversed
	 * to what gnutls uses.
	 */
	if (i == 6 && gcry_sexp_build(&rsa_key, NULL,
			"(private-key(rsa(n%m)(e%m)(d%m)(p%m)(q%m)(u%m)))",
			rm[0], rm[1], rm[2], rm[4], rm[3], rm[5]) !=
			GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to build RSA private key s-exp.\n");
		rsa_key = NULL;
	}
	/* Release the no longer needed MPI values. */
	for (j = 0; j < i; j++)
		gcry_mpi_release(rm[j]);
	return rsa_key;
}

/**
 * ntfs_rsa_private_key_release
 */
static void ntfs_rsa_private_key_release(ntfs_rsa_private_key_t *rsa_key)
{
	if (rsa_key) {
		if (rsa_key->key)
			gcry_sexp_release(rsa_key->key);
		free(rsa_key);
	}
}

/**
 * ntfs_pkcs12_extract_rsa_key
 */
static ntfs_rsa_private_key_t *ntfs_pkcs12_extract_rsa_key(u8 *pfx,
		int pfx_size, const char *password)
{
	int err, bag_index, flags;
	gnutls_datum_t dpfx, dkey;
	gnutls_pkcs12_t pkcs12 = NULL;
	gnutls_pkcs12_bag_t bag = NULL;
	gnutls_x509_privkey_t pkey = NULL;
	gnutls_x509_crt_t crt = NULL;
	ntfs_rsa_private_key_t *rsa_key = NULL;
	char purpose_oid[100];
	size_t purpose_oid_size = sizeof(purpose_oid);
	size_t tp_size;
	BOOL have_thumbprint = FALSE;

	rsa_key = malloc(sizeof(ntfs_rsa_private_key_t));
	if (!rsa_key) {
		ntfs_log_perror("%s", __FUNCTION__);
		return NULL;
	}
	rsa_key->df_type = DF_TYPE_UNKNOWN;
	rsa_key->key = NULL;
	tp_size = sizeof(rsa_key->thumbprint);
	/* Create a pkcs12 structure. */
	err = gnutls_pkcs12_init(&pkcs12);
	if (err) {
		ntfs_log_error("Failed to initialize PKCS#12 structure: %s\n",
				gnutls_strerror(err));
		goto err;
	}
	/* Convert the PFX file (DER format) to native pkcs12 format. */
	dpfx.data = pfx;
	dpfx.size = pfx_size;
	err = gnutls_pkcs12_import(pkcs12, &dpfx, GNUTLS_X509_FMT_DER, 0);
	if (err) {
		ntfs_log_error("Failed to convert the PFX file from DER to "
				"native PKCS#12 format: %s\n",
				gnutls_strerror(err));
		goto err;
	}
	/*
	 * Verify that the password is correct and that the key file has not
	 * been tampered with.  Note if the password has zero length and the
	 * verification fails, retry with password set to NULL.  This is needed
	 * to get password less .pfx files generated with Windows XP SP1 (and
	 * probably earlier versions of Windows) to work.
	 */
retry_verify:
	err = gnutls_pkcs12_verify_mac(pkcs12, password);
	if (err) {
		if (err == GNUTLS_E_MAC_VERIFY_FAILED &&
				password && !strlen(password)) {
			password = NULL;
			goto retry_verify;
		}
		ntfs_log_error("You are probably misspelled password to PFX "
				"file.\n");
		goto err;
	}
	for (bag_index = 0; ; bag_index++) {
		err = gnutls_pkcs12_bag_init(&bag);
		if (err) {
			ntfs_log_error("Failed to initialize PKCS#12 Bag "
					"structure: %s\n",
					gnutls_strerror(err));
			goto err;
		}
		err = gnutls_pkcs12_get_bag(pkcs12, bag_index, bag);
		if (err) {
			if (err == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
				err = 0;
				break;
			}
			ntfs_log_error("Failed to obtain Bag from PKCS#12 "
					"structure: %s\n",
					gnutls_strerror(err));
			goto err;
		}
check_again:
		err = gnutls_pkcs12_bag_get_count(bag);
		if (err < 0) {
			ntfs_log_error("Failed to obtain Bag count: %s\n",
					gnutls_strerror(err));
			goto err;
		}
		err = gnutls_pkcs12_bag_get_type(bag, 0);
		if (err < 0) {
			ntfs_log_error("Failed to determine Bag type: %s\n",
					gnutls_strerror(err));
			goto err;
		}
		flags = 0;
		switch (err) {
		case GNUTLS_BAG_PKCS8_KEY:
			flags = GNUTLS_PKCS_PLAIN;
		case GNUTLS_BAG_PKCS8_ENCRYPTED_KEY:
			err = gnutls_pkcs12_bag_get_data(bag, 0, &dkey);
			if (err < 0) {
				ntfs_log_error("Failed to obtain Bag data: "
						"%s\n", gnutls_strerror(err));
				goto err;
			}
			err = gnutls_x509_privkey_init(&pkey);
			if (err) {
				ntfs_log_error("Failed to initialized "
						"private key structure: %s\n",
						gnutls_strerror(err));
				goto err;
			}
			/* Decrypt the private key into GNU TLS format. */
			err = gnutls_x509_privkey_import_pkcs8(pkey, &dkey,
					GNUTLS_X509_FMT_DER, password, flags);
			if (err) {
				ntfs_log_error("Failed to convert private "
						"key from DER to GNU TLS "
						"format: %s\n",
						gnutls_strerror(err));
				goto err;
			}
#if 0
			/*
			 * Export the key again, but unencrypted, and output it
			 * to stderr.  Note the output has an RSA header so to
			 * compare to openssl pkcs12 -nodes -in myfile.pfx
			 * output need to ignore the part of the key between
			 * the first "MII..." up to the second "MII...".  The
			 * actual RSA private key begins at the second "MII..."
			 * and in my testing at least was identical to openssl
			 * output and was also identical both on big and little
			 * endian so gnutls should be endianness safe.
			 */
			char *buf = malloc(8192);
			size_t bufsize = 8192;
			err = gnutls_x509_privkey_export_pkcs8(pkey,
				GNUTLS_X509_FMT_PEM, "", GNUTLS_PKCS_PLAIN, buf,
				&bufsize);
			if (err) {
				ntfs_log_error("eek1\n");
				exit(1);
			}
			ntfs_log_error("%s\n", buf);
			free(buf);
#endif
			/* Convert the private key to our internal format. */
			rsa_key->key =
				ntfs_rsa_private_key_import_from_gnutls(pkey);
			if (!rsa_key->key)
				goto err;
			break;
		case GNUTLS_BAG_ENCRYPTED:
			err = gnutls_pkcs12_bag_decrypt(bag, password);
			if (err) {
				ntfs_log_error("Failed to decrypt Bag: %s\n",
						gnutls_strerror(err));
				goto err;
			}
			goto check_again;
		case GNUTLS_BAG_CERTIFICATE:
			err = gnutls_pkcs12_bag_get_data(bag, 0, &dkey);
			if (err < 0) {
				ntfs_log_error("Failed to obtain Bag data: "
						"%s\n", gnutls_strerror(err));
				goto err;
			}
			err = gnutls_x509_crt_init(&crt);
			if (err) {
				ntfs_log_error("Failed to initialize "
						"certificate structure: %s\n",
						gnutls_strerror(err));
				goto err;
			}
			err = gnutls_x509_crt_import(crt, &dkey,
					GNUTLS_X509_FMT_DER);
			if (err) {
				ntfs_log_error("Failed to convert certificate "
						"from DER to GNU TLS format: "
						"%s\n", gnutls_strerror(err));
				goto err;
			}
			err = gnutls_x509_crt_get_key_purpose_oid(crt, 0,
					purpose_oid, &purpose_oid_size, NULL);
			if (err) {
				ntfs_log_error("Failed to get key purpose "
						"OID: %s\n",
						gnutls_strerror(err));
				goto err;
			}
			purpose_oid[purpose_oid_size - 1] = 0;
			if (!strcmp(purpose_oid,
					NTFS_EFS_CERT_PURPOSE_OID_DRF))
				rsa_key->df_type = DF_TYPE_DRF;
			else if (!strcmp(purpose_oid,
					NTFS_EFS_CERT_PURPOSE_OID_DDF))
				rsa_key->df_type = DF_TYPE_DDF;
			else {
				ntfs_log_error("Certificate has unknown "
						"purpose OID %s.\n",
						purpose_oid);
				err = EINVAL;
				goto err;
			}
			/* Return the thumbprint to the caller. */
			err = gnutls_x509_crt_get_fingerprint(crt,
					GNUTLS_DIG_SHA1, rsa_key->thumbprint,
					&tp_size);
			if (err) {
				ntfs_log_error("Failed to get thumbprint: "
						"%s\n", gnutls_strerror(err));
				goto err;
			}
			if (tp_size != NTFS_SHA1_THUMBPRINT_SIZE) {
				ntfs_log_error("Invalid thumbprint size %zd.  "
						"Should be %d.\n", tp_size,
						sizeof(rsa_key->thumbprint));
				err = EINVAL;
				goto err;
			}
			have_thumbprint = TRUE;
			gnutls_x509_crt_deinit(crt);
			crt = NULL;
			break;
		default:
			/* We do not care about other types. */
			break;
		}
		gnutls_pkcs12_bag_deinit(bag);
	}
err:
	if (err || !rsa_key->key || rsa_key->df_type == DF_TYPE_UNKNOWN ||
			!have_thumbprint) {
		if (!err)
			ntfs_log_error("Key type or thumbprint not found, "
					"aborting.\n");
		ntfs_rsa_private_key_release(rsa_key);
		rsa_key = NULL;
	}
	if (crt)
		gnutls_x509_crt_deinit(crt);
	if (pkey)
		gnutls_x509_privkey_deinit(pkey);
	if (bag)
		gnutls_pkcs12_bag_deinit(bag);
	if (pkcs12)
		gnutls_pkcs12_deinit(pkcs12);
	return rsa_key;
}

/**
 * ntfs_buffer_reverse -
 *
 * This is a utility function for reversing the order of a buffer in place.
 * Users of this function should be very careful not to sweep byte order
 * problems under the rug.
 */
static inline void ntfs_buffer_reverse(u8 *buf, unsigned buf_size)
{
	unsigned i;
	u8 t;

	for (i = 0; i < buf_size / 2; i++) {
		t = buf[i];
		buf[i] = buf[buf_size - i - 1];
		buf[buf_size - i - 1] = t;
	}
}

#ifndef HAVE_STRNLEN
/**
 * strnlen - strnlen is a gnu extension so emulate it if not present
 */
static size_t strnlen(const char *s, size_t maxlen)
{
	const char *p, *end;

	/* Look for a '\0' character. */
	for (p = s, end = s + maxlen; p < end && *p; p++)
		;
	return p - s;
}
#endif /* ! HAVE_STRNLEN */

/**
 * ntfs_raw_fek_decrypt -
 *
 * Note: decrypting into the input buffer.
 */
static unsigned ntfs_raw_fek_decrypt(u8 *fek, u32 fek_size,
		ntfs_rsa_private_key_t *rsa_key)
{
	gcry_mpi_t fek_mpi;
	gcry_sexp_t fek_sexp, fek_sexp2;
	gcry_error_t err;
	size_t size, padding;

	/* Reverse the raw FEK. */
	ntfs_buffer_reverse(fek, fek_size);
	/* Convert the FEK to internal MPI format. */
	err = gcry_mpi_scan(&fek_mpi, GCRYMPI_FMT_USG, fek, fek_size, NULL);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to convert file encryption key to "
				"internal MPI format: %s\n",
				gcry_strerror(err));
		return 0;
	}
	/* Create an internal S-expression from the FEK. */
	err = gcry_sexp_build(&fek_sexp, NULL,
			"(enc-val (flags) (rsa (a %m)))", fek_mpi);
	gcry_mpi_release(fek_mpi);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to create internal S-expression of "
				"the file encryption key: %s\n",
				gcry_strerror(err));
		return 0;
	}
	/* Decrypt the FEK. */
	err = gcry_pk_decrypt(&fek_sexp2, fek_sexp, rsa_key->key);
	gcry_sexp_release(fek_sexp);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to decrypt the file encryption key: "
				"%s\n", gcry_strerror(err));
		return 0;
	}
	/* Extract the actual FEK from the decrypted raw S-expression. */
	fek_sexp = gcry_sexp_find_token(fek_sexp2, "value", 0);
	gcry_sexp_release(fek_sexp2);
	if (!fek_sexp) {
		ntfs_log_error("Failed to find the decrypted file encryption "
				"key in the internal S-expression.\n");
		return 0;
	}
	/* Convert the decrypted FEK S-expression into MPI format. */
	fek_mpi = gcry_sexp_nth_mpi(fek_sexp, 1, GCRYMPI_FMT_USG);
	gcry_sexp_release(fek_sexp);
	if (!fek_mpi) {
		ntfs_log_error("Failed to convert the decrypted file "
				"encryption key S-expression to internal MPI "
				"format.\n");
		return 0;
	}
	/* Convert the decrypted FEK from MPI format to binary data. */
	err = gcry_mpi_print(GCRYMPI_FMT_USG, fek, fek_size, &size, fek_mpi);
	gcry_mpi_release(fek_mpi);
	if (err != GPG_ERR_NO_ERROR || !size) {
		ntfs_log_error("Failed to convert decrypted file encryption "
				"key from internal MPI format to binary data: "
				"%s\n", gcry_strerror(err));
		return 0;
	}
	/*
	 * Finally, remove the PKCS#1 padding and return the size of the
	 * decrypted FEK.
	 */
	padding = strnlen((char *)fek, size) + 1;
	if (padding > size) {
		ntfs_log_error("Failed to remove PKCS#1 padding from "
				"decrypted file encryption key.\n");
		return 0;
	}
	size -= padding;
	memmove(fek, fek + padding, size);
	return size;
}

/**
 * ntfs_desx_key_expand - expand a 128-bit desx key to the needed 192-bit key
 * @src:	source buffer containing 128-bit key
 *
 * Expands the on-disk 128-bit desx key to the needed des key, the in-, and the
 * out-whitening keys required to perform desx {de,en}cryption.
 */
static gcry_error_t ntfs_desx_key_expand(const u8 *src, u32 *des_key,
		u64 *out_whitening, u64 *in_whitening)
{
	static const u8 *salt1 = (const u8*)"Dan Simon  ";
	static const u8 *salt2 = (const u8*)"Scott Field";
	static const int salt_len = 12;
	gcry_md_hd_t hd1, hd2;
	u32 *md;
	gcry_error_t err;

	err = gcry_md_open(&hd1, GCRY_MD_MD5, 0);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to open MD5 digest.\n");
		return err;
	}
	/* Hash the on-disk key. */
	gcry_md_write(hd1, src, 128 / 8);
	/* Copy the current hash for efficiency. */
	err = gcry_md_copy(&hd2, hd1);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to copy MD5 digest object.\n");
		goto out;
	}
	/* Hash with the first salt and store the result. */
	gcry_md_write(hd1, salt1, salt_len);
	md = (u32*)gcry_md_read(hd1, 0);
	des_key[0] = md[0] ^ md[1];
	des_key[1] = md[2] ^ md[3];
	/* Hash with the second salt and store the result. */
	gcry_md_write(hd2, salt2, salt_len);
	md = (u32*)gcry_md_read(hd2, 0);
	*out_whitening = *(u64*)md;
	*in_whitening = *(u64*)(md + 2);
	gcry_md_close(hd2);
out:
	gcry_md_close(hd1);
	return err;
}

/**
 * ntfs_desx_setkey - libgcrypt set_key implementation for DES-X-MS128
 * @context:	pointer to a variable of type ntfs_desx_ctx
 * @key:	the 128 bit DES-X-MS128 key, concated with the DES handle
 * @keylen:	must always be 16
 *
 * This is the libgcrypt set_key implementation for DES-X-MS128.
 */
static gcry_err_code_t ntfs_desx_setkey(void *context, const u8 *key,
		unsigned keylen)
{
	ntfs_desx_ctx *ctx = context;
	gcry_error_t err;
	u8 des_key[8];

	if (keylen != 16) {
		ntfs_log_error("Key length for desx must be 16.\n");
		return GPG_ERR_INV_KEYLEN;
	}
	err = gcry_cipher_open(&ctx->gcry_cipher_hd, GCRY_CIPHER_DES,
			GCRY_CIPHER_MODE_ECB, 0);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to open des cipher (error 0x%x).\n",
				err);
		return err;
	}
	err = ntfs_desx_key_expand(key, (u32*)des_key, &ctx->out_whitening,
			&ctx->in_whitening);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to expand desx key (error 0x%x).\n",
				err);
		gcry_cipher_close(ctx->gcry_cipher_hd);
		return err;
	}
	err = gcry_cipher_setkey(ctx->gcry_cipher_hd, des_key, sizeof(des_key));
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to set des key (error 0x%x).\n", err);
		gcry_cipher_close(ctx->gcry_cipher_hd);
		return err;
	}
	/*
	 * Take a note of the ctx->gcry_cipher_hd since we need to close it at
	 * ntfs_decrypt_data_key_close() time.
	 */
	**(gcry_cipher_hd_t***)(key + ((keylen + 7) & ~7)) =
			&ctx->gcry_cipher_hd;
	return GPG_ERR_NO_ERROR;
}

/**
 * ntfs_desx_decrypt
 */
static void ntfs_desx_decrypt(void *context, u8 *outbuf, const u8 *inbuf)
{
	ntfs_desx_ctx *ctx = context;
	gcry_error_t err;

	err = gcry_cipher_reset(ctx->gcry_cipher_hd);
	if (err != GPG_ERR_NO_ERROR)
		ntfs_log_error("Failed to reset des cipher (error 0x%x).\n",
				err);
	*(u64*)outbuf = *(const u64*)inbuf ^ ctx->out_whitening;
	err = gcry_cipher_encrypt(ctx->gcry_cipher_hd, outbuf, 8, NULL, 0);
	if (err != GPG_ERR_NO_ERROR)
		ntfs_log_error("Des decryption failed (error 0x%x).\n", err);
	*(u64*)outbuf ^= ctx->in_whitening;
}

static gcry_cipher_spec_t ntfs_desx_cipher = {
	.name = "DES-X-MS128",
	.blocksize = 8,
	.keylen = 128,
	.contextsize = sizeof(ntfs_desx_ctx),
	.setkey = ntfs_desx_setkey,
	.decrypt = ntfs_desx_decrypt,
};

#ifdef NTFS_TEST
/*
 * Do not remove this test code from this file!  (AIA)
 * It would be nice to move all tests (these and runlist) out of the library
 * (at least, into the separate file{,s}), so they would not annoy eyes.  (Yura)
 */

/**
 * ntfs_desx_key_expand_test
 */
static BOOL ntfs_desx_key_expand_test(void)
{
	const u8 known_desx_on_disk_key[16] = {
		0xa1, 0xf9, 0xe0, 0xb2, 0x53, 0x23, 0x9e, 0x8f,
		0x0f, 0x91, 0x45, 0xd9, 0x8e, 0x20, 0xec, 0x30
	};
	const u8 known_des_key[8] = {
		0x27, 0xd1, 0x93, 0x09, 0xcb, 0x78, 0x93, 0x1f,
	};
	const u8 known_out_whitening[8] = {
		0xed, 0xda, 0x4c, 0x47, 0x60, 0x49, 0xdb, 0x8d,
	};
	const u8 known_in_whitening[8] = {
		0x75, 0xf6, 0xa0, 0x1a, 0xc0, 0xca, 0x28, 0x1e
	};
	u64 test_out_whitening, test_in_whitening;
	union {
		u64 u64;
		u32 u32[2];
	} test_des_key;
	gcry_error_t err;
	BOOL res;

	err = ntfs_desx_key_expand(known_desx_on_disk_key, test_des_key.u32,
			&test_out_whitening, &test_in_whitening);
	if (err != GPG_ERR_NO_ERROR)
		res = FALSE;
	else
		res = test_des_key.u64 == *(u64*)known_des_key &&
				test_out_whitening ==
				*(u64*)known_out_whitening &&
				test_in_whitening ==
				*(u64*)known_in_whitening;
	ntfs_log_error("Testing whether ntfs_desx_key_expand() works: %s\n",
			res ? "SUCCESS" : "FAILED");
	return res;
}

/**
 * ntfs_des_test
 */
static BOOL ntfs_des_test(void)
{
	const u8 known_des_key[8] = {
		0x27, 0xd1, 0x93, 0x09, 0xcb, 0x78, 0x93, 0x1f
	};
	const u8 known_des_encrypted_data[8] = {
		0xdc, 0xf7, 0x68, 0x2a, 0xaf, 0x48, 0x53, 0x0f
	};
	const u8 known_decrypted_data[8] = {
		0xd8, 0xd9, 0x15, 0x23, 0x5b, 0x88, 0x0e, 0x09
	};
	u8 test_decrypted_data[8];
	int res;
	gcry_error_t err;
	gcry_cipher_hd_t gcry_cipher_hd;

	err = gcry_cipher_open(&gcry_cipher_hd, GCRY_CIPHER_DES,
			GCRY_CIPHER_MODE_ECB, 0);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to open des cipher (error 0x%x).\n",
				err);
		return FALSE;
	}
	err = gcry_cipher_setkey(gcry_cipher_hd, known_des_key,
			sizeof(known_des_key));
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to set des key (error 0x%x.\n", err);
		gcry_cipher_close(gcry_cipher_hd);
		return FALSE;
	}
	/*
	 * Apply DES decryption (ntfs actually uses encryption when decrypting).
	 */
	err = gcry_cipher_encrypt(gcry_cipher_hd, test_decrypted_data,
			sizeof(test_decrypted_data), known_des_encrypted_data,
			sizeof(known_des_encrypted_data));
	gcry_cipher_close(gcry_cipher_hd);
	if (err) {
		ntfs_log_error("Failed to des decrypt test data (error "
				"0x%x).\n", err);
		return FALSE;
	}
	res = !memcmp(test_decrypted_data, known_decrypted_data,
			sizeof(known_decrypted_data));
	ntfs_log_error("Testing whether des decryption works: %s\n",
			res ? "SUCCESS" : "FAILED");
	return res;
}

#else /* !defined(NTFS_TEST) */

/**
 * ntfs_desx_key_expand_test
 */
static inline BOOL ntfs_desx_key_expand_test(void)
{
	return TRUE;
}

/**
 * ntfs_des_test
 */
static inline BOOL ntfs_des_test(void)
{
	return TRUE;
}

#endif /* !defined(NTFS_TEST) */

/**
 * ntfs_fek_import_from_raw
 */
static ntfs_fek *ntfs_fek_import_from_raw(u8 *fek_buf,
		unsigned fek_size)
{
	ntfs_fek *fek;
	u32 key_size, wanted_key_size, gcry_algo;
	gcry_error_t err;

	key_size = le32_to_cpup(fek_buf);
	ntfs_log_debug("key_size 0x%x\n", key_size);
	if (key_size + 16 > fek_size) {
		ntfs_log_debug("Invalid FEK.  It was probably decrypted with "
				"the incorrect RSA key.");
		errno = EINVAL;
		return NULL;
	}
	fek = malloc(((((sizeof(*fek) + 7) & ~7) + key_size + 7) & ~7) +
			sizeof(gcry_cipher_hd_t));
	if (!fek) {
		errno = ENOMEM;
		return NULL;
	}
	fek->alg_id = *(le32*)(fek_buf + 8);
	ntfs_log_debug("algorithm_id 0x%x\n", le32_to_cpu(fek->alg_id));
	fek->key_data = (u8*)fek + ((sizeof(*fek) + 7) & ~7);
	memcpy(fek->key_data, fek_buf + 16, key_size);
	fek->des_gcry_cipher_hd_ptr = NULL;
	*(gcry_cipher_hd_t***)(fek->key_data + ((key_size + 7) & ~7)) =
			&fek->des_gcry_cipher_hd_ptr;
	switch (fek->alg_id) {
	case CALG_DESX:
		if (!ntfs_crypto_ctx.desx_module) {
			if (!ntfs_desx_key_expand_test() || !ntfs_des_test()) {
				err = EINVAL;
				goto out;
			}
			err = gcry_cipher_register(&ntfs_desx_cipher,
					&ntfs_crypto_ctx.desx_alg_id,
					&ntfs_crypto_ctx.desx_module);
			if (err != GPG_ERR_NO_ERROR) {
				ntfs_log_error("Failed to register desx "
						"cipher: %s\n",
						gcry_strerror(err));
				err = EINVAL;
				goto out;
			}
		}
		wanted_key_size = 16;
		gcry_algo = ntfs_crypto_ctx.desx_alg_id;
		break;
	case CALG_3DES:
		wanted_key_size = 24;
		gcry_algo = GCRY_CIPHER_3DES;
		break;
	case CALG_AES_256:
		wanted_key_size = 32;
		gcry_algo = GCRY_CIPHER_AES256;
		break;
	default:
		wanted_key_size = 8;
		gcry_algo = GCRY_CIPHER_DES;
		if (fek->alg_id == CALG_DES)
			ntfs_log_error("DES is not supported at present\n");
		else
			ntfs_log_error("Unknown crypto algorithm 0x%x\n",
					le32_to_cpu(fek->alg_id));
		ntfs_log_error(".  Please email %s and say that you saw this "
				"message.  We will then try to implement "
				"support for this algorithm.\n", NTFS_DEV_LIST);
		err = EOPNOTSUPP;
		goto out;
	}
	if (key_size != wanted_key_size) {
		ntfs_log_error("%s key of %u bytes but needed size is %u "
				"bytes, assuming corrupt or incorrect key.  "
				"Aborting.\n",
				gcry_cipher_algo_name(gcry_algo),
				(unsigned)key_size, (unsigned)wanted_key_size);
		err = EIO;
		goto out;
	}
	err = gcry_cipher_open(&fek->gcry_cipher_hd, gcry_algo,
			GCRY_CIPHER_MODE_CBC, 0);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("gcry_cipher_open() failed: %s\n",
				gcry_strerror(err));
		err = EINVAL;
		goto out;
	}
	err = gcry_cipher_setkey(fek->gcry_cipher_hd, fek->key_data, key_size);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("gcry_cipher_setkey() failed: %s\n",
				gcry_strerror(err));
		gcry_cipher_close(fek->gcry_cipher_hd);
		err = EINVAL;
		goto out;
	}
	return fek;
out:
	free(fek);
	errno = err;
	return NULL;
}

/**
 * ntfs_fek_release
 */
static void ntfs_fek_release(ntfs_fek *fek)
{
	if (fek->des_gcry_cipher_hd_ptr)
		gcry_cipher_close(*fek->des_gcry_cipher_hd_ptr);
	gcry_cipher_close(fek->gcry_cipher_hd);
	free(fek);
}

/**
 * ntfs_df_array_fek_get
 */
static ntfs_fek *ntfs_df_array_fek_get(EFS_DF_ARRAY_HEADER *df_array,
		ntfs_rsa_private_key_t *rsa_key)
{
	EFS_DF_HEADER *df_header;
	EFS_DF_CREDENTIAL_HEADER *df_cred;
	EFS_DF_CERT_THUMBPRINT_HEADER *df_cert;
	u8 *fek_buf;
	ntfs_fek *fek;
	u32 df_count, fek_size;
	unsigned i, thumbprint_size = sizeof(rsa_key->thumbprint);

	df_count = le32_to_cpu(df_array->df_count);
	if (!df_count)
		ntfs_log_error("There are no elements in the DF array.\n");
	df_header = (EFS_DF_HEADER*)(df_array + 1);
	for (i = 0; i < df_count; i++, df_header = (EFS_DF_HEADER*)(
			(u8*)df_header + le32_to_cpu(df_header->df_length))) {
		df_cred = (EFS_DF_CREDENTIAL_HEADER*)((u8*)df_header +
				le32_to_cpu(df_header->cred_header_offset));
		if (df_cred->type != NTFS_CRED_TYPE_CERT_THUMBPRINT) {
			ntfs_log_debug("Credential type is not certificate "
					"thumbprint, skipping DF entry.\n");
			continue;
		}
		df_cert = (EFS_DF_CERT_THUMBPRINT_HEADER*)((u8*)df_cred +
				le32_to_cpu(
				df_cred->cert_thumbprint_header_offset));
		if (le32_to_cpu(df_cert->thumbprint_size) != thumbprint_size) {
			ntfs_log_error("Thumbprint size %d is not valid "
					"(should be %d), skipping this DF "
					"entry.\n",
					le32_to_cpu(df_cert->thumbprint_size),
					thumbprint_size);
			continue;
		}
		if (memcmp((u8*)df_cert +
				le32_to_cpu(df_cert->thumbprint_offset),
				rsa_key->thumbprint, thumbprint_size)) {
			ntfs_log_debug("Thumbprints do not match, skipping "
					"this DF entry.\n");
			continue;
		}
		/*
		 * The thumbprints match so this is probably the DF entry
		 * matching the RSA key.  Try to decrypt the FEK with it.
		 */
		fek_size = le32_to_cpu(df_header->fek_size);
		fek_buf = (u8*)df_header + le32_to_cpu(df_header->fek_offset);
		/* Decrypt the FEK.  Note: This is done in place. */
		fek_size = ntfs_raw_fek_decrypt(fek_buf, fek_size, rsa_key);
		if (fek_size) {
			/* Convert the FEK to our internal format. */
			fek = ntfs_fek_import_from_raw(fek_buf, fek_size);
			if (fek)
				return fek;
			ntfs_log_error("Failed to convert the decrypted file "
					"encryption key to internal format.\n");
		} else
			ntfs_log_error("Failed to decrypt the file "
					"encryption key.\n");
	}
	return NULL;
}

/**
 * ntfs_inode_fek_get -
 */
static ntfs_fek *ntfs_inode_fek_get(ntfs_inode *inode,
		ntfs_rsa_private_key_t *rsa_key)
{
	EFS_ATTR_HEADER *efs;
	EFS_DF_ARRAY_HEADER *df_array = NULL;
	ntfs_fek *fek = NULL;

	/* Obtain the $EFS contents. */
	efs = ntfs_attr_readall(inode, AT_LOGGED_UTILITY_STREAM, NTFS_EFS, 4,
			NULL);
	if (!efs) {
		ntfs_log_perror("Failed to read $EFS attribute");
		return NULL;
	}
	/*
	 * Depending on whether the key is a normal key or a data recovery key,
	 * iterate through the DDF or DRF array, respectively.
	 */
	if (rsa_key->df_type == DF_TYPE_DDF) {
		if (efs->offset_to_ddf_array)
			df_array = (EFS_DF_ARRAY_HEADER*)((u8*)efs +
					le32_to_cpu(efs->offset_to_ddf_array));
		else
			ntfs_log_error("There are no entries in the DDF "
					"array.\n");
	} else if (rsa_key->df_type == DF_TYPE_DRF) {
		if (efs->offset_to_drf_array)
			df_array = (EFS_DF_ARRAY_HEADER*)((u8*)efs +
					le32_to_cpu(efs->offset_to_drf_array));
		else
			ntfs_log_error("There are no entries in the DRF "
					"array.\n");
	} else
		ntfs_log_error("Invalid DF type.\n");
	if (df_array)
		fek = ntfs_df_array_fek_get(df_array, rsa_key);
	free(efs);
	return fek;
}

/**
 * ntfs_fek_decrypt_sector
 */
static int ntfs_fek_decrypt_sector(ntfs_fek *fek, u8 *data, const u64 offset)
{
	gcry_error_t err;

	err = gcry_cipher_reset(fek->gcry_cipher_hd);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to reset cipher: %s\n",
				gcry_strerror(err));
		return -1;
	}
	/*
	 * Note: You may wonder why we are not calling gcry_cipher_setiv() here
	 * instead of doing it by hand after the decryption.  The answer is
	 * that gcry_cipher_setiv() wants an iv of length 8 bytes but we give
	 * it a length of 16 for AES256 so it does not like it.
	 */
	err = gcry_cipher_decrypt(fek->gcry_cipher_hd, data, 512, NULL, 0);
	if (err != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Decryption failed: %s\n", gcry_strerror(err));
		return -1;
	}
	/* Apply the IV. */
	if (fek->alg_id == CALG_AES_256) {
		((le64*)data)[0] ^= cpu_to_le64(0x5816657be9161312ULL + offset);
		((le64*)data)[1] ^= cpu_to_le64(0x1989adbe44918961ULL + offset);
	} else {
		/* All other algorithms (Des, 3Des, DesX) use the same IV. */
		((le64*)data)[0] ^= cpu_to_le64(0x169119629891ad13ULL + offset);
	}
	return 512;
}

/**
 * ntfs_crypto_deinit - perform library-wide crypto deinitialization
 */
static void ntfs_crypto_deinit(void)
{
	int i;

	if (!ntfs_crypto_ctx.initialized)
		return;

	for (i = 0; i < ntfs_crypto_ctx.nr_rsa_keys; i++)
		ntfs_rsa_private_key_release(ntfs_crypto_ctx.rsa_key[i]);
	free(ntfs_crypto_ctx.rsa_key);
	ntfs_crypto_ctx.rsa_key = NULL;
	ntfs_crypto_ctx.nr_rsa_keys = 0;
	gnutls_global_deinit();
	if (ntfs_crypto_ctx.desx_module) {
		gcry_cipher_unregister(ntfs_crypto_ctx.desx_module);
		ntfs_crypto_ctx.desx_module = NULL;
		ntfs_crypto_ctx.desx_alg_id = -1;
	}
	ntfs_crypto_ctx.initialized = 0;
}


static void ntfs_crypto_parse_config(struct config_t *cfg)
{
	ntfs_crypto_ctx_t *ctx = &ntfs_crypto_ctx;
	config_setting_t *cfg_keys, *cfg_key;
	const char *pfx_file, *pfx_pwd;
	ntfs_rsa_private_key_t *key;
	u8 *pfx_buf;
	unsigned pfx_size;
	int i;

	/* Search for crypto.keys list. */
	cfg_keys = config_lookup(cfg, "crypto.keys");
	if (!cfg_keys) {
		ntfs_log_error("Unable to find crypto.keys in config file.\n");
		return;
	}
	/* Iterate trough list of records about keys. */
	for (i = 0; (cfg_key = config_setting_get_elem(cfg_keys, i)); i++) {
		/* Get path and password to key. */
		pfx_file = config_setting_get_string_elem(cfg_key, 0);
		pfx_pwd = config_setting_get_string_elem(cfg_key, 1);
		if (!pfx_file) {
			ntfs_log_error("Entry number %d in section crypto.keys "
					"of configuration file formed "
					"incorrectly.\n", i + 1);
			continue;
		}
		if (!pfx_pwd)
			pfx_pwd = "";
		/* Load the PKCS#12 file containing the user's private key. */
		if (ntfs_pkcs12_load_pfxfile(pfx_file, &pfx_buf, &pfx_size)) {
			ntfs_log_error("Failed to load key file %s.\n",
					pfx_file);
			continue;
		}
		/*
		 * Check whether we need to allocate memory for new key pointer.
		 * If yes, allocate memory for it and for 3 more pointers.
		 */
		if (!(ctx->nr_rsa_keys % 4)) {
			ntfs_rsa_private_key_t **new;

			new = realloc(ctx->rsa_key,
					sizeof(ntfs_rsa_private_key_t *) *
					(ctx->nr_rsa_keys + 4));
			if (!new) {
				ntfs_log_perror("Unable to store all keys");
				break;
			}
			ctx->rsa_key = new;
		}
		/* Obtain the user's private RSA key from the key file. */
		key = ntfs_pkcs12_extract_rsa_key(pfx_buf, pfx_size, pfx_pwd);
		if (key)
			ctx->rsa_key[ctx->nr_rsa_keys++] = key;
		else
			ntfs_log_error("Failed to obtain RSA key from %s\n",
					pfx_file);
		/* No longer need the pfx file contents. */
		free(pfx_buf);
	}
}


static void ntfs_crypto_read_configs(void)
{
	struct config_t cfg;
	char *home;
	int fd = -1;

	config_init(&cfg);
	/* Load system configuration file. */
	if (config_read_file(&cfg, NTFS_CONFIG_PATH_SYSTEM))
		ntfs_crypto_parse_config(&cfg);
	else
		if (config_error_line(&cfg)) /* Do not cry if file absent. */
			ntfs_log_error("Failed to read system configuration "
					"file: %s (line %d).\n",
					config_error_text(&cfg),
					config_error_line(&cfg));
	/* Load user configuration file. */
	fd = open(".", O_RDONLY); /* Save current working directory. */
	if (fd == -1) {
		ntfs_log_error("Failed to open working directory.\n");
		goto out;
	}
	home = getenv("HOME");
	if (!home) {
		ntfs_log_error("Environment variable HOME is not set.\n");
		goto out;
	}
	if (chdir(home) == -1) {
		ntfs_log_perror("chdir() to home directory failed");
		goto out;
	}
	if (config_read_file(&cfg, NTFS_CONFIG_PATH_USER))
		ntfs_crypto_parse_config(&cfg);
	else
		if (config_error_line(&cfg)) /* Do not cry if file absent. */
			ntfs_log_error("Failed to read user configuration "
					"file: %s (line %d).\n",
					config_error_text(&cfg),
					config_error_line(&cfg));
	if (fchdir(fd) == -1)
		ntfs_log_error("Failed to restore original working "
				"directory.\n");
out:
	if (fd != -1)
		close(fd);
	config_destroy(&cfg);
}

/**
 * ntfs_crypto_init - perform library-wide crypto initializations
 *
 * This function is called during first call of ntfs_crypto_attr_open and
 * performs gcrypt and GNU TLS initializations, then read list of PFX files
 * from configuration files and load RSA keys from them.
 */
static int ntfs_crypto_init(void)
{
	int err;

	if (ntfs_crypto_ctx.initialized)
		return 0;

	/* Initialize gcrypt library.  Note: Must come before GNU TLS init. */
	if (gcry_control(GCRYCTL_DISABLE_SECMEM, 0) != GPG_ERR_NO_ERROR) {
		ntfs_log_error("Failed to initialize the gcrypt library.\n");
		return -1;
	}
	/* Initialize GNU TLS library.  Note: Must come after libgcrypt init. */
	err = gnutls_global_init();
	if (err < 0) {
		ntfs_log_error("Failed to initialize GNU TLS library: %s\n",
				gnutls_strerror(err));
		return -1;
	}
	/* Read crypto related sections of libntfs configuration files. */
	ntfs_crypto_read_configs();

	ntfs_crypto_ctx.initialized = 1;
	atexit(ntfs_crypto_deinit);
	return 0;
}


/**
 * ntfs_crypto_attr_open - perform crypto related initialization for attribute
 * @na:		ntfs attribute to perform initialization for
 *
 * This function is called from ntfs_attr_open for encrypted attributes and
 * tries to decrypt FEK enumerating all user submitted RSA keys. If we
 * successfully obtained FEK, then @na->crypto is allocated and FEK stored
 * inside. In the other case @na->crypto is set to NULL.
 *
 * Return 0 on success and -1 on error with errno set to the error code.
 */
int ntfs_crypto_attr_open(ntfs_attr *na)
{
	ntfs_fek *fek;
	int i;

	na->crypto = NULL;
	if (!na || !NAttrEncrypted(na)) {
		errno = EINVAL;
		return -1;
	}
	if (ntfs_crypto_init()) {
		errno = EACCES;
		return -1;
	}

	for (i = 0; i < ntfs_crypto_ctx.nr_rsa_keys; i++) {
		fek = ntfs_inode_fek_get(na->ni, ntfs_crypto_ctx.rsa_key[i]);
		if (fek) {
			na->crypto = ntfs_malloc(sizeof(ntfs_crypto_attr));
			if (!na->crypto)
				return -1;
			na->crypto->fek = fek;
			return 0;
		}
	}

	errno = EACCES;
	return -1;
}


/**
 * ntfs_crypto_attr_close - perform crypto related deinit for attribute
 * @na:		ntfs attribute to perform deinitialization for
 *
 * This function is called from ntfs_attr_close for encrypted attributes and
 * frees memory that were allocated for it handling.
 */
void ntfs_crypto_attr_close(ntfs_attr *na)
{
	if (!na || !NAttrEncrypted(na))
		return;

	if (na->crypto) {
		ntfs_fek_release(na->crypto->fek);
		free(na->crypto);
	}
}


/**
 * ntfs_crypto_attr_pread - read from an encrypted attribute
 * @na:		ntfs attribute to read from
 * @pos:	byte position in the attribute to begin reading from
 * @count:	number of bytes to read
 * @b:		output data buffer
 *
 * This function is called from ntfs_attr_pread for encrypted attributes and
 * should behave as described in ntfs_attr_pread description.
 */
s64 ntfs_crypto_attr_pread(ntfs_attr *na, const s64 pos, s64 count, void *b)
{
	unsigned char *buffer;
	s64 bytes_read, offset, total, length;
	int i;

	if (!na || pos < 0 || count < 0 || !b || !NAttrEncrypted(na)) {
		errno = EINVAL;
		return -1;
	}
	if (!count)
		return 0;

	if (!na->crypto) {
		errno = EACCES;
		return -1;
	}

	buffer = malloc(NTFS_EFS_SECTOR_SIZE);
	if (!buffer)
		return -1;

	ntfs_attr_map_runlist_range(na, pos >> na->ni->vol->cluster_size_bits,
			(pos + count - 1) >> na->ni->vol->cluster_size_bits);

	total = 0;
	offset = ROUND_DOWN(pos, 9);
	while (total < count && offset < na->data_size) {
		/* Calculate number of bytes we actually want. */
		length = NTFS_EFS_SECTOR_SIZE;
		if (offset + length > pos + count)
			length = pos + count - offset;
		if (offset + length > na->data_size)
			length = na->data_size - offset;

		if (length < 0) {
			total = -1;
			errno = EIO;
			ntfs_log_error("LIBRARY BUG!!! Please report that you "
					"saw this message to %s. Thanks!",
					NTFS_DEV_LIST);
			break;
		}

		/* Just write zeros if @offset fully beyond initialized size. */
		if (offset >= na->initialized_size) {
			memset(b + total, 0, length);
			total += length;
			continue;
		}

		bytes_read = ntfs_rl_pread(na->ni->vol, na->rl, offset,
				NTFS_EFS_SECTOR_SIZE, buffer);
		if (!bytes_read)
			break;
		if (bytes_read != NTFS_EFS_SECTOR_SIZE) {
			ntfs_log_perror("%s(): ntfs_rl_pread returned %lld "
					"bytes", __FUNCTION__, bytes_read);
			break;
		}
		if ((i = ntfs_fek_decrypt_sector(na->crypto->fek, buffer,
				offset)) < bytes_read) {
			ntfs_log_error("%s(): Couldn't decrypt all data "
					"(%u/%lld/%lld/%lld)!", __FUNCTION__,
					i, (long long)bytes_read,
					(long long)offset, (long long)total);
			break;
		}

		/* Handle partially in initialized size situation. */
		if (offset + length > na->initialized_size)
			memset(buffer + (na->initialized_size - offset), 0,
					offset + length - na->initialized_size);

		if (offset >= pos)
			memcpy(b + total, buffer, length);
		else {
			length -= (pos - offset);
			memcpy(b + total, buffer + (pos - offset), length);
		}
		total += length;
		offset += bytes_read;
	}

	free(buffer);
	return total;
}

#else /* !ENABLE_CRYPTO */

/* Stubs for crypto-disabled version of libntfs. */

int ntfs_crypto_attr_open(ntfs_attr *na)
{
	na->crypto = NULL;
	errno = EACCES;
	return -1;
}

void ntfs_crypto_attr_close(ntfs_attr *na)
{
}

s64 ntfs_crypto_attr_pread(ntfs_attr *na, const s64 pos, s64 count,
		void *b)
{
	errno = EACCES;
	return -1;
}

#endif /* !ENABLE_CRYPTO */

