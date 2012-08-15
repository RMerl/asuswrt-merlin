/**
 * ntfsdecrypt - Decrypt ntfs encrypted files.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2005 Yuval Fledel
 * Copyright (c) 2005-2007 Anton Altaparmakov
 * Copyright (c) 2007 Yura Pakhuchiy
 *
 * This utility will decrypt files and print the decrypted data on the standard
 * output.
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
 */

#include "config.h"

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
#ifdef HAVE_GETOPT_H
#include <getopt.h>
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
#include <gcrypt.h>
#include <gnutls/pkcs12.h>

#include "types.h"
#include "attrib.h"
#include "utils.h"
#include "volume.h"
#include "debug.h"
#include "dir.h"
#include "layout.h"
#include "version.h"

typedef gcry_sexp_t ntfs_rsa_private_key;

#define NTFS_SHA1_THUMBPRINT_SIZE 0x14

#define NTFS_CRED_TYPE_CERT_THUMBPRINT const_cpu_to_le32(3)

#define NTFS_EFS_CERT_PURPOSE_OID_DDF "1.3.6.1.4.1.311.10.3.4"
#define NTFS_EFS_CERT_PURPOSE_OID_DRF "1.3.6.1.4.1.311.10.3.4.1"

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
typedef struct {
	gcry_cipher_hd_t gcry_cipher_hd;
	le32 alg_id;
	u8 *key_data;
	gcry_cipher_hd_t *des_gcry_cipher_hd_ptr;
} ntfs_fek;

/* DESX-MS128 implementation for libgcrypt. */
static gcry_module_t ntfs_desx_module;
static int ntfs_desx_algorithm_id = -1;

typedef struct {
	u64 in_whitening, out_whitening;
	gcry_cipher_hd_t gcry_cipher_hd;
} ntfs_desx_ctx;

struct options {
	char *keyfile;	/* .pfx file containing the user's private key. */
	char *device;		/* Device/File to work with */
	char *file;		/* File to display */
	s64 inode;		/* Inode to work with */
	ATTR_TYPES attr;	/* Attribute type to display */
	int force;		/* Override common sense */
	int quiet;		/* Less output */
	int verbose;		/* Extra output */
};

static const char *EXEC_NAME = "ntfsdecrypt";
static struct options opts;

static ntfschar EFS[5] = {
	const_cpu_to_le16('$'), const_cpu_to_le16('E'), const_cpu_to_le16('F'),
	const_cpu_to_le16('S'), const_cpu_to_le16('\0')
};

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s) - Decrypt files and print on the "
			"standard output.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2005 Yuval Fledel\n");
	ntfs_log_info("Copyright (c) 2005 Anton Altaparmakov\n");
	ntfs_log_info("\n%s\n%s%s\n", ntfs_gpl, ntfs_bugs, ntfs_home);
}

/**
 * usage - Print a list of the parameters to the program
 *
 * Print a list of the parameters and options for the program.
 *
 * Return:  none
 */
static void usage(void)
{
	ntfs_log_info("\nUsage: %s [options] -k name.pfx device [file]\n\n"
	       "    -i, --inode num         Display this inode\n\n"
	       "    -k  --keyfile name.pfx  Use file name as the user's private key file.\n"
	       "    -f  --force             Use less caution\n"
	       "    -h  --help              Print this help\n"
	       "    -q  --quiet             Less output\n"
	       "    -V  --version           Version information\n"
	       "    -v  --verbose           More output\n\n",
	       EXEC_NAME);
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

/**
 * parse_options - Read and validate the programs command line
 *
 * Read the command line, verify the syntax and parse the options.
 * This function is very long, but quite simple.
 *
 * Return:  1 Success
 *	    0 Error, one or more problems
 */
static int parse_options(int argc, char **argv)
{
	static const char *sopt = "-fh?i:k:qVv";
	static const struct option lopt[] = {
		{"force", no_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{"inode", required_argument, NULL, 'i'},
		{"keyfile", required_argument, NULL, 'k'},
		{"quiet", no_argument, NULL, 'q'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	int c = -1;
	int err = 0;
	int ver = 0;
	int help = 0;

	opterr = 0;		/* We'll handle the errors, thank you. */

	opts.inode = -1;

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device)
				opts.device = argv[optind - 1];
			else if (!opts.file)
				opts.file = argv[optind - 1];
			else {
				ntfs_log_error("You must specify exactly one "
					"file.\n");
				err++;
			}
			break;
		case 'f':
			opts.force++;
			break;
		case 'h':
		case '?':
			help++;
			break;
		case 'k':
			if (!opts.keyfile)
				opts.keyfile = argv[optind - 1];
			else {
				ntfs_log_error("You must specify exactly one "
						"key file.\n");
				err++;
			}
			break;
		case 'i':
			if (opts.inode != -1)
				ntfs_log_error("You must specify exactly one "
						"inode.\n");
			else if (utils_parse_size(optarg, &opts.inode, FALSE))
				break;
			else
				ntfs_log_error("Couldn't parse inode number.\n");
			err++;
			break;
		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 'V':
			ver++;
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		default:
			ntfs_log_error("Unknown option '%s'.\n",
				argv[optind - 1]);
			err++;
			break;
		}
	}

	if (help || ver) {
		opts.quiet = 0;
		ntfs_log_set_levels(NTFS_LOG_LEVEL_QUIET);
	} else {
		if (!opts.keyfile) {
			ntfs_log_error("You must specify a key file.\n");
			err++;
		} else if (opts.device == NULL) {
			ntfs_log_error("You must specify a device.\n");
			err++;
		} else if (opts.file == NULL && opts.inode == -1) {
			ntfs_log_error("You must specify a file or inode with "
				"the -i option.\n");
			err++;
		} else if (opts.file != NULL && opts.inode != -1) {
			ntfs_log_error("You can't specify both a file and "
				"inode.\n");
			err++;
		}
		if (opts.quiet && opts.verbose) {
			ntfs_log_error("You may not use --quiet and --verbose "
				"at the same time.\n");
			err++;
		}
	}

	if (ver)
		version();
	if (help || err)
		usage();

	return (!err && !help && !ver);
}

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
 * ntfs_crypto_init
 */
static int ntfs_crypto_init(void)
{
	int err;

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
	return 0;
}

/**
 * ntfs_crypto_deinit
 */
static void ntfs_crypto_deinit(void)
{
	gnutls_global_deinit();
	if (ntfs_desx_module) {
		gcry_cipher_unregister(ntfs_desx_module);
		ntfs_desx_module = NULL;
		ntfs_desx_algorithm_id = -1;
	}
}

/**
 * ntfs_rsa_private_key_import_from_gnutls
 */
static ntfs_rsa_private_key ntfs_rsa_private_key_import_from_gnutls(
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
	/* Convert each RSA parameter to mpi format. */
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
	/* Release the no longer needed mpi values. */
	for (j = 0; j < i; j++)
		gcry_mpi_release(rm[j]);
	return (ntfs_rsa_private_key)rsa_key;
}

/**
 * ntfs_rsa_private_key_release
 */
static void ntfs_rsa_private_key_release(ntfs_rsa_private_key rsa_key)
{
	gcry_sexp_release((gcry_sexp_t)rsa_key);
}

/**
 * ntfs_pkcs12_extract_rsa_key
 */
static ntfs_rsa_private_key ntfs_pkcs12_extract_rsa_key(u8 *pfx, int pfx_size,
		char *password, char *thumbprint, int thumbprint_size,
		NTFS_DF_TYPES *df_type)
{
	int err, bag_index, flags;
	gnutls_datum_t dpfx, dkey;
	gnutls_pkcs12_t pkcs12 = NULL;
	gnutls_pkcs12_bag_t bag = NULL;
	gnutls_x509_privkey_t pkey = NULL;
	gnutls_x509_crt_t crt = NULL;
	ntfs_rsa_private_key rsa_key = NULL;
	char purpose_oid[100];
	size_t purpose_oid_size = sizeof(purpose_oid);
	size_t tp_size = thumbprint_size;
	BOOL have_thumbprint = FALSE;

	*df_type = DF_TYPE_UNKNOWN;
	/* Create a pkcs12 structure. */
	err = gnutls_pkcs12_init(&pkcs12);
	if (err) {
		ntfs_log_error("Failed to initialize PKCS#12 structure: %s\n",
				gnutls_strerror(err));
		return NULL;
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
	 * to get passwordless .pfx files generated with Windows XP SP1 (and
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
		ntfs_log_error("Failed to verify the MAC: %s  Is the "
				"password correct?\n", gnutls_strerror(err));
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
			rsa_key = ntfs_rsa_private_key_import_from_gnutls(pkey);
			if (!rsa_key)
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
			purpose_oid[purpose_oid_size - 1] = '\0';
			if (!strcmp(purpose_oid,
					NTFS_EFS_CERT_PURPOSE_OID_DRF))
				*df_type = DF_TYPE_DRF;
			else if (!strcmp(purpose_oid,
					NTFS_EFS_CERT_PURPOSE_OID_DDF))
				*df_type = DF_TYPE_DDF;
			else {
				ntfs_log_error("Certificate has unknown "
						"purpose OID %s.\n",
						purpose_oid);
				err = EINVAL;
				goto err;
			}
			/* Return the thumbprint to the caller. */
			err = gnutls_x509_crt_get_fingerprint(crt,
					GNUTLS_DIG_SHA1, thumbprint, &tp_size);
			if (err) {
				ntfs_log_error("Failed to get thumbprint: "
						"%s\n", gnutls_strerror(err));
				goto err;
			}
			if (tp_size != NTFS_SHA1_THUMBPRINT_SIZE) {
				ntfs_log_error("Invalid thumbprint size %zd.  "
						"Should be %d.\n", tp_size,
						thumbprint_size);
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
	if (rsa_key && (err || *df_type == DF_TYPE_UNKNOWN ||
			!have_thumbprint)) {
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
		ntfs_rsa_private_key rsa_key)
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
	err = gcry_pk_decrypt(&fek_sexp2, fek_sexp, (gcry_sexp_t)rsa_key);
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

//#define DO_CRYPTO_TESTS 1

#ifdef DO_CRYPTO_TESTS

/* Do not remove this test code from this file! AIA */
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

#else /* !defined(DO_CRYPTO_TESTS) */

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

#endif /* !defined(DO_CRYPTO_TESTS) */

/**
 * ntfs_fek_import_from_raw
 */
static ntfs_fek *ntfs_fek_import_from_raw(u8 *fek_buf, unsigned fek_size)
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
	//ntfs_log_debug("alg_id 0x%x\n", le32_to_cpu(fek->alg_id));
	fek->key_data = (u8*)fek + ((sizeof(*fek) + 7) & ~7);
	memcpy(fek->key_data, fek_buf + 16, key_size);
	fek->des_gcry_cipher_hd_ptr = NULL;
	*(gcry_cipher_hd_t***)(fek->key_data + ((key_size + 7) & ~7)) =
			&fek->des_gcry_cipher_hd_ptr;
	switch (fek->alg_id) {
	case CALG_DESX:
		if (!ntfs_desx_module) {
			if (!ntfs_desx_key_expand_test() || !ntfs_des_test()) {
				err = EINVAL;
				goto out;
			}
			err = gcry_cipher_register(&ntfs_desx_cipher,
					&ntfs_desx_algorithm_id,
					&ntfs_desx_module);
			if (err != GPG_ERR_NO_ERROR) {
				ntfs_log_error("Failed to register desx "
						"cipher: %s\n",
						gcry_strerror(err));
				err = EINVAL;
				goto out;
			}
		}
		wanted_key_size = 16;
		gcry_algo = ntfs_desx_algorithm_id;
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
		ntfs_rsa_private_key rsa_key, char *thumbprint,
		int thumbprint_size)
{
	EFS_DF_HEADER *df_header;
	EFS_DF_CREDENTIAL_HEADER *df_cred;
	EFS_DF_CERT_THUMBPRINT_HEADER *df_cert;
	u8 *fek_buf;
	ntfs_fek *fek;
	u32 df_count, fek_size;
	unsigned i;

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
				thumbprint, thumbprint_size)) {
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
		ntfs_rsa_private_key rsa_key, char *thumbprint,
		int thumbprint_size, NTFS_DF_TYPES df_type)
{
	EFS_ATTR_HEADER *efs;
	EFS_DF_ARRAY_HEADER *df_array = NULL;
	ntfs_fek *fek = NULL;

	/* Obtain the $EFS contents. */
	efs = ntfs_attr_readall(inode, AT_LOGGED_UTILITY_STREAM, EFS, 4, NULL);
	if (!efs) {
		ntfs_log_perror("Failed to read $EFS attribute");
		return NULL;
	}
	/*
	 * Depending on whether the key is a normal key or a data recovery key,
	 * iterate through the DDF or DRF array, respectively.
	 */
	if (df_type == DF_TYPE_DDF) {
		if (efs->offset_to_ddf_array)
			df_array = (EFS_DF_ARRAY_HEADER*)((u8*)efs +
					le32_to_cpu(efs->offset_to_ddf_array));
		else
			ntfs_log_error("There are no entries in the DDF "
					"array.\n");
	} else if (df_type == DF_TYPE_DRF) {
		if (efs->offset_to_drf_array)
			df_array = (EFS_DF_ARRAY_HEADER*)((u8*)efs +
					le32_to_cpu(efs->offset_to_drf_array));
		else
			ntfs_log_error("There are no entries in the DRF "
					"array.\n");
	} else
		ntfs_log_error("Invalid DF type.\n");
	if (df_array)
		fek = ntfs_df_array_fek_get(df_array, rsa_key, thumbprint,
				thumbprint_size);
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
		/* All other algos (Des, 3Des, DesX) use the same IV. */
		((le64*)data)[0] ^= cpu_to_le64(0x169119629891ad13ULL + offset);
	}
	return 512;
}

/**
 * ntfs_cat_decrypt - Decrypt the contents of an encrypted file to stdout.
 * @inode:	An encrypted file's inode structure, as obtained by
 * 		ntfs_inode_open().
 * @fek:	A file encryption key. As obtained by ntfs_inode_fek_get().
 */
static int ntfs_cat_decrypt(ntfs_inode *inode, ntfs_fek *fek)
{
	int bufsize = 512;
	unsigned char *buffer;
	ntfs_attr *attr;
	s64 bytes_read, written, offset, total;
	s64 old_data_size, old_initialized_size;
	int i;

	buffer = malloc(bufsize);
	if (!buffer)
		return 1;
	attr = ntfs_attr_open(inode, AT_DATA, NULL, 0);
	if (!attr) {
		ntfs_log_error("Cannot cat a directory.\n");
		free(buffer);
		return 1;
	}
	total = attr->data_size;

	// hack: make sure attr will not be commited to disk if you use this.
	// clear the encrypted bit, otherwise the library won't allow reading.
	NAttrClearEncrypted(attr);
	// extend the size, we may need to read past the end of the stream.
	old_data_size = attr->data_size;
	old_initialized_size = attr->initialized_size;
	attr->data_size = attr->initialized_size = attr->allocated_size;

	offset = 0;
	while (total > 0) {
		bytes_read = ntfs_attr_pread(attr, offset, 512, buffer);
		if (bytes_read == -1) {
			ntfs_log_perror("ERROR: Couldn't read file");
			break;
		}
		if (!bytes_read)
			break;
		if ((i = ntfs_fek_decrypt_sector(fek, buffer, offset)) <
				bytes_read) {
			ntfs_log_perror("ERROR: Couldn't decrypt all data!");
			ntfs_log_error("%u/%lld/%lld/%lld\n", i,
				(long long)bytes_read, (long long)offset,
				(long long)total);
			break;
		}
		if (bytes_read > total)
			bytes_read = total;
		written = fwrite(buffer, 1, bytes_read, stdout);
		if (written != bytes_read) {
			ntfs_log_perror("ERROR: Couldn't output all data!");
			break;
		}
		offset += bytes_read;
		total -= bytes_read;
	}
	attr->data_size = old_data_size;
	attr->initialized_size = old_initialized_size;
	NAttrSetEncrypted(attr);
	ntfs_attr_close(attr);
	free(buffer);
	return 0;
}

/**
 * main - Begin here
 *
 * Start from here.
 *
 * Return:  0  Success, the program worked
 *	    1  Error, something went wrong
 */
int main(int argc, char *argv[])
{
	u8 *pfx_buf;
	char *password;
	ntfs_rsa_private_key rsa_key;
	ntfs_volume *vol;
	ntfs_inode *inode;
	ntfs_fek *fek;
	unsigned pfx_size;
	int res;
	NTFS_DF_TYPES df_type;
	char thumbprint[NTFS_SHA1_THUMBPRINT_SIZE];

	ntfs_log_set_handler(ntfs_log_handler_stderr);

	if (!parse_options(argc, argv))
		return 1;
	utils_set_locale();

	/* Initialize crypto in ntfs. */
	if (ntfs_crypto_init()) {
		ntfs_log_error("Failed to initialize crypto.  Aborting.\n");
		return 1;
	}
	/* Load the PKCS#12 (.pfx) file containing the user's private key. */
	if (ntfs_pkcs12_load_pfxfile(opts.keyfile, &pfx_buf, &pfx_size)) {
		ntfs_log_error("Failed to load key file.  Aborting.\n");
		ntfs_crypto_deinit();
		return 1;
	}
	/* Ask the user for their password. */
	password = getpass("Enter the password with which the private key was "
			"encrypted: ");
	if (!password) {
		ntfs_log_perror("Failed to obtain user password");
		free(pfx_buf);
		ntfs_crypto_deinit();
		return 1;
	}
	/* Obtain the user's private RSA key from the key file. */
	rsa_key = ntfs_pkcs12_extract_rsa_key(pfx_buf, pfx_size, password,
			thumbprint, sizeof(thumbprint), &df_type);
	/* Destroy the password. */
	memset(password, 0, strlen(password));
	/* No longer need the pfx file contents. */
	free(pfx_buf);
	if (!rsa_key) {
		ntfs_log_error("Failed to extract the private RSA key.\n");
		ntfs_crypto_deinit();
		return 1;
	}
	/* Mount the ntfs volume. */
	vol = utils_mount_volume(opts.device, NTFS_MNT_RDONLY |
			(opts.force ? NTFS_MNT_FORCE : 0));
	if (!vol) {
		ntfs_log_error("Failed to mount ntfs volume.  Aborting.\n");
		ntfs_rsa_private_key_release(rsa_key);
		ntfs_crypto_deinit();
		return 1;
	}
	/* Open the encrypted ntfs file. */
	if (opts.inode != -1)
		inode = ntfs_inode_open(vol, opts.inode);
	else
		inode = ntfs_pathname_to_inode(vol, NULL, opts.file);
	if (!inode) {
		ntfs_log_error("Failed to open encrypted file.  Aborting.\n");
		ntfs_umount(vol, FALSE);
		ntfs_rsa_private_key_release(rsa_key);
		ntfs_crypto_deinit();
		return 1;
	}
	/* Obtain the file encryption key of the encrypted file. */
	fek = ntfs_inode_fek_get(inode, rsa_key, thumbprint,
			sizeof(thumbprint), df_type);
	ntfs_rsa_private_key_release(rsa_key);
	if (fek) {
		res = ntfs_cat_decrypt(inode, fek);
		ntfs_fek_release(fek);
	} else {
		ntfs_log_error("Failed to obtain file encryption key.  "
				"Aborting.\n");
		res = 1;
	}
	ntfs_inode_close(inode);
	ntfs_umount(vol, FALSE);
	ntfs_crypto_deinit();
	return res;
}
