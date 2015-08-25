/*
 * Based on PuTTY's import.c for importing/exporting OpenSSH and SSH.com
 * keyfiles.
 *
 * Modifications copyright 2003 Matt Johnston
 *
 * PuTTY is copyright 1997-2003 Simon Tatham.
 * 
 * Portions copyright Robert de Bath, Joris van Rantwijk, Delian
 * Delchev, Andreas Schultz, Jeroen Massar, Wez Furlong, Nicolas Barry,
 * Justin Bradford, and CORE SDI S.A.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "keyimport.h"
#include "bignum.h"
#include "buffer.h"
#include "dbutil.h"
#include "ecc.h"

static const unsigned char OID_SEC256R1_BLOB[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
static const unsigned char OID_SEC384R1_BLOB[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
static const unsigned char OID_SEC521R1_BLOB[] = {0x2b, 0x81, 0x04, 0x00, 0x23};

#define PUT_32BIT(cp, value) do { \
  (cp)[3] = (unsigned char)(value); \
  (cp)[2] = (unsigned char)((value) >> 8); \
  (cp)[1] = (unsigned char)((value) >> 16); \
  (cp)[0] = (unsigned char)((value) >> 24); } while (0)

#define GET_32BIT(cp) \
	(((unsigned long)(unsigned char)(cp)[0] << 24) | \
	((unsigned long)(unsigned char)(cp)[1] << 16) | \
	((unsigned long)(unsigned char)(cp)[2] << 8) | \
	((unsigned long)(unsigned char)(cp)[3]))

static int openssh_encrypted(const char *filename);
static sign_key *openssh_read(const char *filename, char *passphrase);
static int openssh_write(const char *filename, sign_key *key,
				  char *passphrase);

static int dropbear_write(const char*filename, sign_key * key);
static sign_key *dropbear_read(const char* filename);

#if 0
static int sshcom_encrypted(const char *filename, char **comment);
static struct ssh2_userkey *sshcom_read(const char *filename, char *passphrase);
static int sshcom_write(const char *filename, struct ssh2_userkey *key,
				 char *passphrase);
#endif

int import_encrypted(const char* filename, int filetype) {

	if (filetype == KEYFILE_OPENSSH) {
		return openssh_encrypted(filename);
#if 0
	} else if (filetype == KEYFILE_SSHCOM) {
		return sshcom_encrypted(filename, NULL);
#endif
	}
	return 0;
}

sign_key *import_read(const char *filename, char *passphrase, int filetype) {

	if (filetype == KEYFILE_OPENSSH) {
		return openssh_read(filename, passphrase);
	} else if (filetype == KEYFILE_DROPBEAR) {
		return dropbear_read(filename);
#if 0
	} else if (filetype == KEYFILE_SSHCOM) {
		return sshcom_read(filename, passphrase);
#endif
	}
	return NULL;
}

int import_write(const char *filename, sign_key *key, char *passphrase,
		int filetype) {

	if (filetype == KEYFILE_OPENSSH) {
		return openssh_write(filename, key, passphrase);
	} else if (filetype == KEYFILE_DROPBEAR) {
		return dropbear_write(filename, key);
#if 0
	} else if (filetype == KEYFILE_SSHCOM) {
		return sshcom_write(filename, key, passphrase);
#endif
	}
	return 0;
}

static sign_key *dropbear_read(const char* filename) {

	buffer * buf = NULL;
	sign_key *ret = NULL;
	enum signkey_type type;

	buf = buf_new(MAX_PRIVKEY_SIZE);
	if (buf_readfile(buf, filename) == DROPBEAR_FAILURE) {
		goto error;
	}

	buf_setpos(buf, 0);
	ret = new_sign_key();

	type = DROPBEAR_SIGNKEY_ANY;
	if (buf_get_priv_key(buf, ret, &type) == DROPBEAR_FAILURE){
		goto error;
	}
	buf_free(buf);

	ret->type = type;

	return ret;

error:
	if (buf) {
		buf_free(buf);
	}
	if (ret) {
		sign_key_free(ret);
	}
	return NULL;
}

/* returns 0 on fail, 1 on success */
static int dropbear_write(const char*filename, sign_key * key) {

	buffer * buf;
	FILE*fp;
	int len;
	int ret;

	buf = buf_new(MAX_PRIVKEY_SIZE);
	buf_put_priv_key(buf, key, key->type);

	fp = fopen(filename, "w");
	if (!fp) {
		ret = 0;
		goto out;
	}

	buf_setpos(buf, 0);
	do {
		len = fwrite(buf_getptr(buf, buf->len - buf->pos),
				1, buf->len - buf->pos, fp);
		buf_incrpos(buf, len);
	} while (len > 0 && buf->len != buf->pos);

	fclose(fp);

	if (buf->pos != buf->len) {
		ret = 0;
	} else {
		ret = 1;
	}
out:
	buf_free(buf);
	return ret;
}


/* ----------------------------------------------------------------------
 * Helper routines. (The base64 ones are defined in sshpubk.c.)
 */

#define isbase64(c) (	((c) >= 'A' && (c) <= 'Z') || \
						 ((c) >= 'a' && (c) <= 'z') || \
						 ((c) >= '0' && (c) <= '9') || \
						 (c) == '+' || (c) == '/' || (c) == '=' \
						 )

/* cpl has to be less than 100 */
static void base64_encode_fp(FILE * fp, unsigned char *data,
		int datalen, int cpl)
{
	unsigned char out[100];
    int n;
	unsigned long outlen;
	int rawcpl;
	rawcpl = cpl * 3 / 4;
	dropbear_assert((unsigned int)cpl < sizeof(out));

    while (datalen > 0) {
		n = (datalen < rawcpl ? datalen : rawcpl);
		outlen = sizeof(out);
		base64_encode(data, n, out, &outlen);
		data += n;
		datalen -= n;
		fwrite(out, 1, outlen, fp);
		fputc('\n', fp);
    }
}
/*
 * Read an ASN.1/BER identifier and length pair.
 * 
 * Flags are a combination of the #defines listed below.
 * 
 * Returns -1 if unsuccessful; otherwise returns the number of
 * bytes used out of the source data.
 */

/* ASN.1 tag classes. */
#define ASN1_CLASS_UNIVERSAL		(0 << 6)
#define ASN1_CLASS_APPLICATION	  (1 << 6)
#define ASN1_CLASS_CONTEXT_SPECIFIC (2 << 6)
#define ASN1_CLASS_PRIVATE		  (3 << 6)
#define ASN1_CLASS_MASK			 (3 << 6)

/* Primitive versus constructed bit. */
#define ASN1_CONSTRUCTED			(1 << 5)

static int ber_read_id_len(void *source, int sourcelen,
						   int *id, int *length, int *flags)
{
	unsigned char *p = (unsigned char *) source;

	if (sourcelen == 0)
		return -1;

	*flags = (*p & 0xE0);
	if ((*p & 0x1F) == 0x1F) {
		*id = 0;
		while (*p & 0x80) {
			*id = (*id << 7) | (*p & 0x7F);
			p++, sourcelen--;
			if (sourcelen == 0)
				return -1;
		}
		*id = (*id << 7) | (*p & 0x7F);
		p++, sourcelen--;
	} else {
		*id = *p & 0x1F;
		p++, sourcelen--;
	}

	if (sourcelen == 0)
		return -1;

	if (*p & 0x80) {
		int n = *p & 0x7F;
		p++, sourcelen--;
		if (sourcelen < n)
			return -1;
		*length = 0;
		while (n--)
			*length = (*length << 8) | (*p++);
		sourcelen -= n;
	} else {
		*length = *p;
		p++, sourcelen--;
	}

	return p - (unsigned char *) source;
}

/*
 * Write an ASN.1/BER identifier and length pair. Returns the
 * number of bytes consumed. Assumes dest contains enough space.
 * Will avoid writing anything if dest is NULL, but still return
 * amount of space required.
 */
static int ber_write_id_len(void *dest, int id, int length, int flags)
{
	unsigned char *d = (unsigned char *)dest;
	int len = 0;

	if (id <= 30) {
		/*
		 * Identifier is one byte.
		 */
		len++;
		if (d) *d++ = id | flags;
	} else {
		int n;
		/*
		 * Identifier is multiple bytes: the first byte is 11111
		 * plus the flags, and subsequent bytes encode the value of
		 * the identifier, 7 bits at a time, with the top bit of
		 * each byte 1 except the last one which is 0.
		 */
		len++;
		if (d) *d++ = 0x1F | flags;
		for (n = 1; (id >> (7*n)) > 0; n++)
			continue;					   /* count the bytes */
		while (n--) {
			len++;
			if (d) *d++ = (n ? 0x80 : 0) | ((id >> (7*n)) & 0x7F);
		}
	}

	if (length < 128) {
		/*
		 * Length is one byte.
		 */
		len++;
		if (d) *d++ = length;
	} else {
		int n;
		/*
		 * Length is multiple bytes. The first is 0x80 plus the
		 * number of subsequent bytes, and the subsequent bytes
		 * encode the actual length.
		 */
		for (n = 1; (length >> (8*n)) > 0; n++)
			continue;					   /* count the bytes */
		len++;
		if (d) *d++ = 0x80 | n;
		while (n--) {
			len++;
			if (d) *d++ = (length >> (8*n)) & 0xFF;
		}
	}

	return len;
}


/* Simple structure to point to an mp-int within a blob. */
struct mpint_pos { void *start; int bytes; };

/* ----------------------------------------------------------------------
 * Code to read and write OpenSSH private keys.
 */

enum { OSSH_DSA, OSSH_RSA, OSSH_EC };
struct openssh_key {
	int type;
	int encrypted;
	char iv[32];
	unsigned char *keyblob;
	unsigned int keyblob_len, keyblob_size;
};

static struct openssh_key *load_openssh_key(const char *filename)
{
	struct openssh_key *ret;
	FILE *fp = NULL;
	char buffer[256];
	char *errmsg = NULL, *p = NULL;
	int headers_done;
	unsigned long len, outlen;

	ret = (struct openssh_key*)m_malloc(sizeof(struct openssh_key));
	ret->keyblob = NULL;
	ret->keyblob_len = ret->keyblob_size = 0;
	ret->encrypted = 0;
	memset(ret->iv, 0, sizeof(ret->iv));

	if (strlen(filename) == 1 && filename[0] == '-') {
		fp = stdin;
	} else {
		fp = fopen(filename, "r");
	}
	if (!fp) {
		errmsg = "Unable to open key file";
		goto error;
	}
	if (!fgets(buffer, sizeof(buffer), fp) ||
		0 != strncmp(buffer, "-----BEGIN ", 11) ||
		0 != strcmp(buffer+strlen(buffer)-17, "PRIVATE KEY-----\n")) {
		errmsg = "File does not begin with OpenSSH key header";
		goto error;
	}
	if (!strcmp(buffer, "-----BEGIN RSA PRIVATE KEY-----\n"))
		ret->type = OSSH_RSA;
	else if (!strcmp(buffer, "-----BEGIN DSA PRIVATE KEY-----\n"))
		ret->type = OSSH_DSA;
	else if (!strcmp(buffer, "-----BEGIN EC PRIVATE KEY-----\n"))
		ret->type = OSSH_EC;
	else {
		errmsg = "Unrecognised key type";
		goto error;
	}

	headers_done = 0;
	while (1) {
		if (!fgets(buffer, sizeof(buffer), fp)) {
			errmsg = "Unexpected end of file";
			goto error;
		}
		if (0 == strncmp(buffer, "-----END ", 9) &&
			0 == strcmp(buffer+strlen(buffer)-17, "PRIVATE KEY-----\n"))
			break;					   /* done */
		if ((p = strchr(buffer, ':')) != NULL) {
			if (headers_done) {
				errmsg = "Header found in body of key data";
				goto error;
			}
			*p++ = '\0';
			while (*p && isspace((unsigned char)*p)) p++;
			if (!strcmp(buffer, "Proc-Type")) {
				if (p[0] != '4' || p[1] != ',') {
					errmsg = "Proc-Type is not 4 (only 4 is supported)";
					goto error;
				}
				p += 2;
				if (!strcmp(p, "ENCRYPTED\n"))
					ret->encrypted = 1;
			} else if (!strcmp(buffer, "DEK-Info")) {
				int i, j;

				if (strncmp(p, "DES-EDE3-CBC,", 13)) {
					errmsg = "Ciphers other than DES-EDE3-CBC not supported";
					goto error;
				}
				p += 13;
				for (i = 0; i < 8; i++) {
					if (1 != sscanf(p, "%2x", &j))
						break;
					ret->iv[i] = j;
					p += 2;
				}
				if (i < 8) {
					errmsg = "Expected 16-digit iv in DEK-Info";
					goto error;
				}
			}
		} else {
			headers_done = 1;
			len = strlen(buffer);
			outlen = len*4/3;
			if (ret->keyblob_len + outlen > ret->keyblob_size) {
				ret->keyblob_size = ret->keyblob_len + outlen + 256;
				ret->keyblob = (unsigned char*)m_realloc(ret->keyblob,
						ret->keyblob_size);
			}
			outlen = ret->keyblob_size - ret->keyblob_len;
			if (base64_decode((const unsigned char *)buffer, len,
						ret->keyblob + ret->keyblob_len, &outlen) != CRYPT_OK){
				errmsg = "Error decoding base64";
				goto error;
			}
			ret->keyblob_len += outlen;
		}
	}

	if (ret->keyblob_len == 0 || !ret->keyblob) {
		errmsg = "Key body not present";
		goto error;
	}

	if (ret->encrypted && ret->keyblob_len % 8 != 0) {
		errmsg = "Encrypted key blob is not a multiple of cipher block size";
		goto error;
	}

	m_burn(buffer, sizeof(buffer));
	return ret;

	error:
	m_burn(buffer, sizeof(buffer));
	if (ret) {
		if (ret->keyblob) {
			m_burn(ret->keyblob, ret->keyblob_size);
			m_free(ret->keyblob);
		}
		m_free(ret);
	}
	if (fp) {
		fclose(fp);
	}
	if (errmsg) {
		fprintf(stderr, "Error: %s\n", errmsg);
	}
	return NULL;
}

static int openssh_encrypted(const char *filename)
{
	struct openssh_key *key = load_openssh_key(filename);
	int ret;

	if (!key)
		return 0;
	ret = key->encrypted;
	m_burn(key->keyblob, key->keyblob_size);
	m_free(key->keyblob);
	m_free(key);
	return ret;
}

static sign_key *openssh_read(const char *filename, char * UNUSED(passphrase))
{
	struct openssh_key *key;
	unsigned char *p;
	int ret, id, len, flags;
	int i, num_integers = 0;
	sign_key *retval = NULL;
	char *errmsg;
	unsigned char *modptr = NULL;
	int modlen = -9999;
	enum signkey_type type;

	sign_key *retkey;
	buffer * blobbuf = NULL;

	retkey = new_sign_key();

	key = load_openssh_key(filename);

	if (!key)
		return NULL;

	if (key->encrypted) {
		errmsg = "encrypted keys not supported currently";
		goto error;
#if 0
		/* matt TODO */
		/*
		 * Derive encryption key from passphrase and iv/salt:
		 * 
		 *  - let block A equal MD5(passphrase || iv)
		 *  - let block B equal MD5(A || passphrase || iv)
		 *  - block C would be MD5(B || passphrase || iv) and so on
		 *  - encryption key is the first N bytes of A || B
		 */
		struct MD5Context md5c;
		unsigned char keybuf[32];

		MD5Init(&md5c);
		MD5Update(&md5c, (unsigned char *)passphrase, strlen(passphrase));
		MD5Update(&md5c, (unsigned char *)key->iv, 8);
		MD5Final(keybuf, &md5c);

		MD5Init(&md5c);
		MD5Update(&md5c, keybuf, 16);
		MD5Update(&md5c, (unsigned char *)passphrase, strlen(passphrase));
		MD5Update(&md5c, (unsigned char *)key->iv, 8);
		MD5Final(keybuf+16, &md5c);

		/*
		 * Now decrypt the key blob.
		 */
		des3_decrypt_pubkey_ossh(keybuf, (unsigned char *)key->iv,
								 key->keyblob, key->keyblob_len);

		memset(&md5c, 0, sizeof(md5c));
		memset(keybuf, 0, sizeof(keybuf));
#endif 
	}

	/*
	 * Now we have a decrypted key blob, which contains an ASN.1
	 * encoded private key. We must now untangle the ASN.1.
	 *
	 * We expect the whole key blob to be formatted as a SEQUENCE
	 * (0x30 followed by a length code indicating that the rest of
	 * the blob is part of the sequence). Within that SEQUENCE we
	 * expect to see a bunch of INTEGERs. What those integers mean
	 * depends on the key type:
	 *
	 *  - For RSA, we expect the integers to be 0, n, e, d, p, q,
	 *	dmp1, dmq1, iqmp in that order. (The last three are d mod
	 *	(p-1), d mod (q-1), inverse of q mod p respectively.)
	 *
	 *  - For DSA, we expect them to be 0, p, q, g, y, x in that
	 *	order.
	 */
	
	p = key->keyblob;

	/* Expect the SEQUENCE header. Take its absence as a failure to decrypt. */
	ret = ber_read_id_len(p, key->keyblob_len, &id, &len, &flags);
	p += ret;
	if (ret < 0 || id != 16) {
		errmsg = "ASN.1 decoding failure - wrong password?";
		goto error;
	}

	/* Expect a load of INTEGERs. */
	if (key->type == OSSH_RSA)
		num_integers = 9;
	else if (key->type == OSSH_DSA)
		num_integers = 6;
	else if (key->type == OSSH_EC)
		num_integers = 1;

	/*
	 * Space to create key blob in.
	 */
	blobbuf = buf_new(3000);

#ifdef DROPBEAR_DSS
	if (key->type == OSSH_DSA) {
		buf_putstring(blobbuf, "ssh-dss", 7);
		retkey->type = DROPBEAR_SIGNKEY_DSS;
	} 
#endif
#ifdef DROPBEAR_RSA
	if (key->type == OSSH_RSA) {
		buf_putstring(blobbuf, "ssh-rsa", 7);
		retkey->type = DROPBEAR_SIGNKEY_RSA;
	}
#endif

	for (i = 0; i < num_integers; i++) {
		ret = ber_read_id_len(p, key->keyblob+key->keyblob_len-p,
							  &id, &len, &flags);
		p += ret;
		if (ret < 0 || id != 2 ||
			key->keyblob+key->keyblob_len-p < len) {
			errmsg = "ASN.1 decoding failure";
			goto error;
		}

		if (i == 0) {
			/* First integer is a version indicator */
			int expected = -1;
			switch (key->type) {
				case OSSH_RSA:
				case OSSH_DSA:
					expected = 0;
					break;
				case OSSH_EC:
					expected = 1;
					break;
			}
			if (len != 1 || p[0] != expected) {
				errmsg = "Version number mismatch";
				goto error;
			}
		} else if (key->type == OSSH_RSA) {
			/*
			 * OpenSSH key order is n, e, d, p, q, dmp1, dmq1, iqmp
			 * but we want e, n, d, p, q
			 */
			if (i == 1) {
				/* Save the details for after we deal with number 2. */
				modptr = p;
				modlen = len;
			} else if (i >= 2 && i <= 5) {
				buf_putstring(blobbuf, (const char*)p, len);
				if (i == 2) {
					buf_putstring(blobbuf, (const char*)modptr, modlen);
				}
			}
		} else if (key->type == OSSH_DSA) {
			/*
			 * OpenSSH key order is p, q, g, y, x,
			 * we want the same.
			 */
			buf_putstring(blobbuf, (const char*)p, len);
		}

		/* Skip past the number. */
		p += len;
	}

#ifdef DROPBEAR_ECDSA
	if (key->type == OSSH_EC) {
		unsigned char* private_key_bytes = NULL;
		int private_key_len = 0;
		unsigned char* public_key_bytes = NULL;
		int public_key_len = 0;
		ecc_key *ecc = NULL;
		const struct dropbear_ecc_curve *curve = NULL;

		/* See SEC1 v2, Appendix C.4 */
		/* OpenSSL (so OpenSSH) seems to include the optional parts. */

		/* privateKey OCTET STRING, */
		ret = ber_read_id_len(p, key->keyblob+key->keyblob_len-p,
							  &id, &len, &flags);
		p += ret;
		/* id==4 for octet string */
		if (ret < 0 || id != 4 ||
			key->keyblob+key->keyblob_len-p < len) {
			errmsg = "ASN.1 decoding failure";
			goto error;
		}
		private_key_bytes = p;
		private_key_len = len;
		p += len;

		/* parameters [0] ECDomainParameters {{ SECGCurveNames }} OPTIONAL, */
		ret = ber_read_id_len(p, key->keyblob+key->keyblob_len-p,
							  &id, &len, &flags);
		p += ret;
		/* id==0 */
		if (ret < 0 || id != 0) {
			errmsg = "ASN.1 decoding failure";
			goto error;
		}

		ret = ber_read_id_len(p, key->keyblob+key->keyblob_len-p,
							  &id, &len, &flags);
		p += ret;
		/* id==6 for object */
		if (ret < 0 || id != 6 ||
			key->keyblob+key->keyblob_len-p < len) {
			errmsg = "ASN.1 decoding failure";
			goto error;
		}

		if (0) {}
#ifdef DROPBEAR_ECC_256
		else if (len == sizeof(OID_SEC256R1_BLOB) 
			&& memcmp(p, OID_SEC256R1_BLOB, len) == 0) {
			retkey->type = DROPBEAR_SIGNKEY_ECDSA_NISTP256;
			curve = &ecc_curve_nistp256;
		} 
#endif
#ifdef DROPBEAR_ECC_384
		else if (len == sizeof(OID_SEC384R1_BLOB)
			&& memcmp(p, OID_SEC384R1_BLOB, len) == 0) {
			retkey->type = DROPBEAR_SIGNKEY_ECDSA_NISTP384;
			curve = &ecc_curve_nistp384;
		} 
#endif
#ifdef DROPBEAR_ECC_521
		else if (len == sizeof(OID_SEC521R1_BLOB)
			&& memcmp(p, OID_SEC521R1_BLOB, len) == 0) {
			retkey->type = DROPBEAR_SIGNKEY_ECDSA_NISTP521;
			curve = &ecc_curve_nistp521;
		} 
#endif
		else {
			errmsg = "Unknown ECC key type";
			goto error;
		}
		p += len;

		/* publicKey [1] BIT STRING OPTIONAL */
		ret = ber_read_id_len(p, key->keyblob+key->keyblob_len-p,
							  &id, &len, &flags);
		p += ret;
		/* id==1 */
		if (ret < 0 || id != 1) {
			errmsg = "ASN.1 decoding failure";
			goto error;
		}

		ret = ber_read_id_len(p, key->keyblob+key->keyblob_len-p,
							  &id, &len, &flags);
		p += ret;
		/* id==3 for bit string */
		if (ret < 0 || id != 3 ||
			key->keyblob+key->keyblob_len-p < len) {
			errmsg = "ASN.1 decoding failure";
			goto error;
		}
		public_key_bytes = p+1;
		public_key_len = len-1;
		p += len;

		buf_putbytes(blobbuf, public_key_bytes, public_key_len);
		ecc = buf_get_ecc_raw_pubkey(blobbuf, curve);
		if (!ecc) {
			errmsg = "Error parsing ECC key";
			goto error;
		}
		m_mp_alloc_init_multi((mp_int**)&ecc->k, NULL);
		if (mp_read_unsigned_bin(ecc->k, private_key_bytes, private_key_len)
			!= MP_OKAY) {
			errmsg = "Error parsing ECC key";
			goto error;
		}

		*signkey_key_ptr(retkey, retkey->type) = ecc;
	}
#endif /* DROPBEAR_ECDSA */

	/*
	 * Now put together the actual key. Simplest way to do this is
	 * to assemble our own key blobs and feed them to the createkey
	 * functions; this is a bit faffy but it does mean we get all
	 * the sanity checks for free.
	 */
	if (key->type == OSSH_RSA || key->type == OSSH_DSA) {
		buf_setpos(blobbuf, 0);
		type = DROPBEAR_SIGNKEY_ANY;
		if (buf_get_priv_key(blobbuf, retkey, &type)
				!= DROPBEAR_SUCCESS) {
			errmsg = "unable to create key structure";
			sign_key_free(retkey);
			retkey = NULL;
			goto error;
		}
	}

	errmsg = NULL;					 /* no error */
	retval = retkey;

	error:
	if (blobbuf) {
		buf_burn(blobbuf);
		buf_free(blobbuf);
	}
	m_burn(key->keyblob, key->keyblob_size);
	m_free(key->keyblob);
	m_burn(key, sizeof(*key));
	m_free(key);
	if (errmsg) {
		fprintf(stderr, "Error: %s\n", errmsg);
	}
	return retval;
}

static int openssh_write(const char *filename, sign_key *key,
				  char *passphrase)
{
	buffer * keyblob = NULL;
	buffer * extrablob = NULL; /* used for calculated values to write */
	unsigned char *outblob = NULL;
	int outlen = -9999;
	struct mpint_pos numbers[9];
	int nnumbers = -1, pos = 0, len = 0, seqlen, i;
	char *header = NULL, *footer = NULL;
	char zero[1];
	int ret = 0;
	FILE *fp;

#ifdef DROPBEAR_RSA
	mp_int dmp1, dmq1, iqmp, tmpval; /* for rsa */
#endif

	if (
#ifdef DROPBEAR_RSA
			key->type == DROPBEAR_SIGNKEY_RSA ||
#endif
#ifdef DROPBEAR_DSS
			key->type == DROPBEAR_SIGNKEY_DSS ||
#endif
			0)
	{
		/*
		 * Fetch the key blobs.
		 */
		keyblob = buf_new(3000);
		buf_put_priv_key(keyblob, key, key->type);

		buf_setpos(keyblob, 0);
		/* skip the "ssh-rsa" or "ssh-dss" header */
		buf_incrpos(keyblob, buf_getint(keyblob));

		/*
		 * Find the sequence of integers to be encoded into the OpenSSH
		 * key blob, and also decide on the header line.
		 */
		numbers[0].start = zero; numbers[0].bytes = 1; zero[0] = '\0';

	#ifdef DROPBEAR_RSA
		if (key->type == DROPBEAR_SIGNKEY_RSA) {

			if (key->rsakey->p == NULL || key->rsakey->q == NULL) {
				fprintf(stderr, "Pre-0.33 Dropbear keys cannot be converted to OpenSSH keys.\n");
				goto error;
			}

			/* e */
			numbers[2].bytes = buf_getint(keyblob);
			numbers[2].start = buf_getptr(keyblob, numbers[2].bytes);
			buf_incrpos(keyblob, numbers[2].bytes);
			
			/* n */
			numbers[1].bytes = buf_getint(keyblob);
			numbers[1].start = buf_getptr(keyblob, numbers[1].bytes);
			buf_incrpos(keyblob, numbers[1].bytes);
			
			/* d */
			numbers[3].bytes = buf_getint(keyblob);
			numbers[3].start = buf_getptr(keyblob, numbers[3].bytes);
			buf_incrpos(keyblob, numbers[3].bytes);
			
			/* p */
			numbers[4].bytes = buf_getint(keyblob);
			numbers[4].start = buf_getptr(keyblob, numbers[4].bytes);
			buf_incrpos(keyblob, numbers[4].bytes);
			
			/* q */
			numbers[5].bytes = buf_getint(keyblob);
			numbers[5].start = buf_getptr(keyblob, numbers[5].bytes);
			buf_incrpos(keyblob, numbers[5].bytes);

			/* now calculate some extra parameters: */
			m_mp_init(&tmpval);
			m_mp_init(&dmp1);
			m_mp_init(&dmq1);
			m_mp_init(&iqmp);

			/* dmp1 = d mod (p-1) */
			if (mp_sub_d(key->rsakey->p, 1, &tmpval) != MP_OKAY) {
				fprintf(stderr, "Bignum error for p-1\n");
				goto error;
			}
			if (mp_mod(key->rsakey->d, &tmpval, &dmp1) != MP_OKAY) {
				fprintf(stderr, "Bignum error for dmp1\n");
				goto error;
			}

			/* dmq1 = d mod (q-1) */
			if (mp_sub_d(key->rsakey->q, 1, &tmpval) != MP_OKAY) {
				fprintf(stderr, "Bignum error for q-1\n");
				goto error;
			}
			if (mp_mod(key->rsakey->d, &tmpval, &dmq1) != MP_OKAY) {
				fprintf(stderr, "Bignum error for dmq1\n");
				goto error;
			}

			/* iqmp = (q^-1) mod p */
			if (mp_invmod(key->rsakey->q, key->rsakey->p, &iqmp) != MP_OKAY) {
				fprintf(stderr, "Bignum error for iqmp\n");
				goto error;
			}

			extrablob = buf_new(2000);
			buf_putmpint(extrablob, &dmp1);
			buf_putmpint(extrablob, &dmq1);
			buf_putmpint(extrablob, &iqmp);
			buf_setpos(extrablob, 0);
			mp_clear(&dmp1);
			mp_clear(&dmq1);
			mp_clear(&iqmp);
			mp_clear(&tmpval);
			
			/* dmp1 */
			numbers[6].bytes = buf_getint(extrablob);
			numbers[6].start = buf_getptr(extrablob, numbers[6].bytes);
			buf_incrpos(extrablob, numbers[6].bytes);
			
			/* dmq1 */
			numbers[7].bytes = buf_getint(extrablob);
			numbers[7].start = buf_getptr(extrablob, numbers[7].bytes);
			buf_incrpos(extrablob, numbers[7].bytes);
			
			/* iqmp */
			numbers[8].bytes = buf_getint(extrablob);
			numbers[8].start = buf_getptr(extrablob, numbers[8].bytes);
			buf_incrpos(extrablob, numbers[8].bytes);

			nnumbers = 9;
			header = "-----BEGIN RSA PRIVATE KEY-----\n";
			footer = "-----END RSA PRIVATE KEY-----\n";
		}
	#endif /* DROPBEAR_RSA */

	#ifdef DROPBEAR_DSS
		if (key->type == DROPBEAR_SIGNKEY_DSS) {

			/* p */
			numbers[1].bytes = buf_getint(keyblob);
			numbers[1].start = buf_getptr(keyblob, numbers[1].bytes);
			buf_incrpos(keyblob, numbers[1].bytes);

			/* q */
			numbers[2].bytes = buf_getint(keyblob);
			numbers[2].start = buf_getptr(keyblob, numbers[2].bytes);
			buf_incrpos(keyblob, numbers[2].bytes);

			/* g */
			numbers[3].bytes = buf_getint(keyblob);
			numbers[3].start = buf_getptr(keyblob, numbers[3].bytes);
			buf_incrpos(keyblob, numbers[3].bytes);

			/* y */
			numbers[4].bytes = buf_getint(keyblob);
			numbers[4].start = buf_getptr(keyblob, numbers[4].bytes);
			buf_incrpos(keyblob, numbers[4].bytes);

			/* x */
			numbers[5].bytes = buf_getint(keyblob);
			numbers[5].start = buf_getptr(keyblob, numbers[5].bytes);
			buf_incrpos(keyblob, numbers[5].bytes);

			nnumbers = 6;
			header = "-----BEGIN DSA PRIVATE KEY-----\n";
			footer = "-----END DSA PRIVATE KEY-----\n";
		}
	#endif /* DROPBEAR_DSS */

		/*
		 * Now count up the total size of the ASN.1 encoded integers,
		 * so as to determine the length of the containing SEQUENCE.
		 */
		len = 0;
		for (i = 0; i < nnumbers; i++) {
			len += ber_write_id_len(NULL, 2, numbers[i].bytes, 0);
			len += numbers[i].bytes;
		}
		seqlen = len;
		/* Now add on the SEQUENCE header. */
		len += ber_write_id_len(NULL, 16, seqlen, ASN1_CONSTRUCTED);
		/* Round up to the cipher block size, ensuring we have at least one
		 * byte of padding (see below). */
		outlen = len;
		if (passphrase)
			outlen = (outlen+8) &~ 7;

		/*
		 * Now we know how big outblob needs to be. Allocate it.
		 */
		outblob = (unsigned char*)m_malloc(outlen);

		/*
		 * And write the data into it.
		 */
		pos = 0;
		pos += ber_write_id_len(outblob+pos, 16, seqlen, ASN1_CONSTRUCTED);
		for (i = 0; i < nnumbers; i++) {
			pos += ber_write_id_len(outblob+pos, 2, numbers[i].bytes, 0);
			memcpy(outblob+pos, numbers[i].start, numbers[i].bytes);
			pos += numbers[i].bytes;
		}
	} /* end RSA and DSS handling */

#ifdef DROPBEAR_ECDSA
	if (key->type == DROPBEAR_SIGNKEY_ECDSA_NISTP256
		|| key->type == DROPBEAR_SIGNKEY_ECDSA_NISTP384
		|| key->type == DROPBEAR_SIGNKEY_ECDSA_NISTP521) {

		/*  SEC1 V2 appendix c.4
		ECPrivateKey ::= SEQUENCE {
			version INTEGER { ecPrivkeyVer1(1) } (ecPrivkeyVer1),
			privateKey OCTET STRING,
			parameters [0] ECDomainParameters {{ SECGCurveNames }} OPTIONAL, 
			publicKey [1] BIT STRING OPTIONAL
		}
		*/
		buffer *seq_buf = buf_new(400);
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, key->type);
		const long curve_size = (*eck)->dp->size;
		int curve_oid_len = 0;
		const void* curve_oid = NULL;
		unsigned long pubkey_size = 2*curve_size+1;
		int k_size;
		int err = 0;

		/* version. less than 10 bytes */
		buf_incrwritepos(seq_buf,
			ber_write_id_len(buf_getwriteptr(seq_buf, 10), 2, 1, 0));
		buf_putbyte(seq_buf, 1);

		/* privateKey */
		k_size = mp_unsigned_bin_size((*eck)->k);
		dropbear_assert(k_size <= curve_size);
		buf_incrwritepos(seq_buf,
			ber_write_id_len(buf_getwriteptr(seq_buf, 10), 4, k_size, 0));
	    mp_to_unsigned_bin((*eck)->k, buf_getwriteptr(seq_buf, k_size));
		buf_incrwritepos(seq_buf, k_size);

		/* SECGCurveNames */
		switch (key->type)
		{
			case DROPBEAR_SIGNKEY_ECDSA_NISTP256:
				curve_oid_len = sizeof(OID_SEC256R1_BLOB);
				curve_oid = OID_SEC256R1_BLOB;
				break;
			case DROPBEAR_SIGNKEY_ECDSA_NISTP384:
				curve_oid_len = sizeof(OID_SEC384R1_BLOB);
				curve_oid = OID_SEC384R1_BLOB;
				break;
			case DROPBEAR_SIGNKEY_ECDSA_NISTP521:
				curve_oid_len = sizeof(OID_SEC521R1_BLOB);
				curve_oid = OID_SEC521R1_BLOB;
				break;
			default:
				dropbear_exit("Internal error");
		}

		buf_incrwritepos(seq_buf,
			ber_write_id_len(buf_getwriteptr(seq_buf, 10), 0, 2+curve_oid_len, 0xa0));
		/* object == 6 */
		buf_incrwritepos(seq_buf,
			ber_write_id_len(buf_getwriteptr(seq_buf, 10), 6, curve_oid_len, 0));
		buf_putbytes(seq_buf, curve_oid, curve_oid_len);

		buf_incrwritepos(seq_buf,
			ber_write_id_len(buf_getwriteptr(seq_buf, 10), 1, 2+1+pubkey_size, 0xa0));
		buf_incrwritepos(seq_buf,
			ber_write_id_len(buf_getwriteptr(seq_buf, 10), 3, 1+pubkey_size, 0));
		buf_putbyte(seq_buf, 0);
		err = ecc_ansi_x963_export(*eck, buf_getwriteptr(seq_buf, pubkey_size), &pubkey_size);
		if (err != CRYPT_OK) {
			dropbear_exit("ECC error");
		}
		buf_incrwritepos(seq_buf, pubkey_size);

		buf_setpos(seq_buf, 0);
			
		outblob = (unsigned char*)m_malloc(1000);

		pos = 0;
		pos += ber_write_id_len(outblob+pos, 16, seq_buf->len, ASN1_CONSTRUCTED);
		memcpy(&outblob[pos], seq_buf->data, seq_buf->len);
		pos += seq_buf->len;
		len = pos;
		outlen = len;

		buf_burn(seq_buf);
		buf_free(seq_buf);
		seq_buf = NULL;

		header = "-----BEGIN EC PRIVATE KEY-----\n";
		footer = "-----END EC PRIVATE KEY-----\n";
	}
#endif

	/*
	 * Padding on OpenSSH keys is deterministic. The number of
	 * padding bytes is always more than zero, and always at most
	 * the cipher block length. The value of each padding byte is
	 * equal to the number of padding bytes. So a plaintext that's
	 * an exact multiple of the block size will be padded with 08
	 * 08 08 08 08 08 08 08 (assuming a 64-bit block cipher); a
	 * plaintext one byte less than a multiple of the block size
	 * will be padded with just 01.
	 * 
	 * This enables the OpenSSL key decryption function to strip
	 * off the padding algorithmically and return the unpadded
	 * plaintext to the next layer: it looks at the final byte, and
	 * then expects to find that many bytes at the end of the data
	 * with the same value. Those are all removed and the rest is
	 * returned.
	 */
	dropbear_assert(pos == len);
	while (pos < outlen) {
		outblob[pos++] = outlen - len;
	}

	/*
	 * Encrypt the key.
	 */
	if (passphrase) {
		fprintf(stderr, "Encrypted keys aren't supported currently\n");
		goto error;
	}

	/*
	 * And save it. We'll use Unix line endings just in case it's
	 * subsequently transferred in binary mode.
	 */
	if (strlen(filename) == 1 && filename[0] == '-') {
		fp = stdout;
	} else {
		fp = fopen(filename, "wb");	  /* ensure Unix line endings */
	}
	if (!fp) {
		fprintf(stderr, "Failed opening output file\n");
		goto error;
	}
	fputs(header, fp);
	base64_encode_fp(fp, outblob, outlen, 64);
	fputs(footer, fp);
	fclose(fp);
	ret = 1;

	error:
	if (outblob) {
		memset(outblob, 0, outlen);
		m_free(outblob);
	}
	if (keyblob) {
		buf_burn(keyblob);
		buf_free(keyblob);
	}
	if (extrablob) {
		buf_burn(extrablob);
		buf_free(extrablob);
	}
	return ret;
}

#if 0
/* XXX TODO ssh.com stuff isn't going yet */

/* ----------------------------------------------------------------------
 * Code to read ssh.com private keys.
 */

/*
 * The format of the base64 blob is largely ssh2-packet-formatted,
 * except that mpints are a bit different: they're more like the
 * old ssh1 mpint. You have a 32-bit bit count N, followed by
 * (N+7)/8 bytes of data.
 * 
 * So. The blob contains:
 * 
 *  - uint32 0x3f6ff9eb	   (magic number)
 *  - uint32 size			 (total blob size)
 *  - string key-type		 (see below)
 *  - string cipher-type	  (tells you if key is encrypted)
 *  - string encrypted-blob
 * 
 * (The first size field includes the size field itself and the
 * magic number before it. All other size fields are ordinary ssh2
 * strings, so the size field indicates how much data is to
 * _follow_.)
 * 
 * The encrypted blob, once decrypted, contains a single string
 * which in turn contains the payload. (This allows padding to be
 * added after that string while still making it clear where the
 * real payload ends. Also it probably makes for a reasonable
 * decryption check.)
 * 
 * The payload blob, for an RSA key, contains:
 *  - mpint e
 *  - mpint d
 *  - mpint n  (yes, the public and private stuff is intermixed)
 *  - mpint u  (presumably inverse of p mod q)
 *  - mpint p  (p is the smaller prime)
 *  - mpint q  (q is the larger)
 * 
 * For a DSA key, the payload blob contains:
 *  - uint32 0
 *  - mpint p
 *  - mpint g
 *  - mpint q
 *  - mpint y
 *  - mpint x
 * 
 * Alternatively, if the parameters are `predefined', that
 * (0,p,g,q) sequence can be replaced by a uint32 1 and a string
 * containing some predefined parameter specification. *shudder*,
 * but I doubt we'll encounter this in real life.
 * 
 * The key type strings are ghastly. The RSA key I looked at had a
 * type string of
 * 
 *   `if-modn{sign{rsa-pkcs1-sha1},encrypt{rsa-pkcs1v2-oaep}}'
 * 
 * and the DSA key wasn't much better:
 * 
 *   `dl-modp{sign{dsa-nist-sha1},dh{plain}}'
 * 
 * It isn't clear that these will always be the same. I think it
 * might be wise just to look at the `if-modn{sign{rsa' and
 * `dl-modp{sign{dsa' prefixes.
 * 
 * Finally, the encryption. The cipher-type string appears to be
 * either `none' or `3des-cbc'. Looks as if this is SSH2-style
 * 3des-cbc (i.e. outer cbc rather than inner). The key is created
 * from the passphrase by means of yet another hashing faff:
 * 
 *  - first 16 bytes are MD5(passphrase)
 *  - next 16 bytes are MD5(passphrase || first 16 bytes)
 *  - if there were more, they'd be MD5(passphrase || first 32),
 *	and so on.
 */

#define SSHCOM_MAGIC_NUMBER 0x3f6ff9eb

struct sshcom_key {
	char comment[256];				 /* allowing any length is overkill */
	unsigned char *keyblob;
	int keyblob_len, keyblob_size;
};

static struct sshcom_key *load_sshcom_key(const char *filename)
{
	struct sshcom_key *ret;
	FILE *fp;
	char buffer[256];
	int len;
	char *errmsg, *p;
	int headers_done;
	char base64_bit[4];
	int base64_chars = 0;

	ret = snew(struct sshcom_key);
	ret->comment[0] = '\0';
	ret->keyblob = NULL;
	ret->keyblob_len = ret->keyblob_size = 0;

	fp = fopen(filename, "r");
	if (!fp) {
		errmsg = "Unable to open key file";
		goto error;
	}
	if (!fgets(buffer, sizeof(buffer), fp) ||
		0 != strcmp(buffer, "---- BEGIN SSH2 ENCRYPTED PRIVATE KEY ----\n")) {
		errmsg = "File does not begin with ssh.com key header";
		goto error;
	}

	headers_done = 0;
	while (1) {
		if (!fgets(buffer, sizeof(buffer), fp)) {
			errmsg = "Unexpected end of file";
			goto error;
		}
		if (!strcmp(buffer, "---- END SSH2 ENCRYPTED PRIVATE KEY ----\n"))
			break;					 /* done */
		if ((p = strchr(buffer, ':')) != NULL) {
			if (headers_done) {
				errmsg = "Header found in body of key data";
				goto error;
			}
			*p++ = '\0';
			while (*p && isspace((unsigned char)*p)) p++;
			/*
			 * Header lines can end in a trailing backslash for
			 * continuation.
			 */
			while ((len = strlen(p)) > (int)(sizeof(buffer) - (p-buffer) -1) ||
				   p[len-1] != '\n' || p[len-2] == '\\') {
				if (len > (int)((p-buffer) + sizeof(buffer)-2)) {
					errmsg = "Header line too long to deal with";
					goto error;
				}
				if (!fgets(p+len-2, sizeof(buffer)-(p-buffer)-(len-2), fp)) {
					errmsg = "Unexpected end of file";
					goto error;
				}
			}
			p[strcspn(p, "\n")] = '\0';
			if (!strcmp(buffer, "Comment")) {
				/* Strip quotes in comment if present. */
				if (p[0] == '"' && p[strlen(p)-1] == '"') {
					p++;
					p[strlen(p)-1] = '\0';
				}
				strncpy(ret->comment, p, sizeof(ret->comment));
				ret->comment[sizeof(ret->comment)-1] = '\0';
			}
		} else {
			headers_done = 1;

			p = buffer;
			while (isbase64(*p)) {
				base64_bit[base64_chars++] = *p;
				if (base64_chars == 4) {
					unsigned char out[3];

					base64_chars = 0;

					len = base64_decode_atom(base64_bit, out);

					if (len <= 0) {
						errmsg = "Invalid base64 encoding";
						goto error;
					}

					if (ret->keyblob_len + len > ret->keyblob_size) {
						ret->keyblob_size = ret->keyblob_len + len + 256;
						ret->keyblob = sresize(ret->keyblob, ret->keyblob_size,
											   unsigned char);
					}

					memcpy(ret->keyblob + ret->keyblob_len, out, len);
					ret->keyblob_len += len;
				}

				p++;
			}
		}
	}

	if (ret->keyblob_len == 0 || !ret->keyblob) {
		errmsg = "Key body not present";
		goto error;
	}

	return ret;

	error:
	if (ret) {
		if (ret->keyblob) {
			memset(ret->keyblob, 0, ret->keyblob_size);
			m_free(ret->keyblob);
		}
		memset(&ret, 0, sizeof(ret));
		m_free(ret);
	}
	return NULL;
}

int sshcom_encrypted(const char *filename, char **comment)
{
	struct sshcom_key *key = load_sshcom_key(filename);
	int pos, len, answer;

	*comment = NULL;
	if (!key)
		return 0;

	/*
	 * Check magic number.
	 */
	if (GET_32BIT(key->keyblob) != 0x3f6ff9eb)
		return 0;					  /* key is invalid */

	/*
	 * Find the cipher-type string.
	 */
	answer = 0;
	pos = 8;
	if (key->keyblob_len < pos+4)
		goto done;					 /* key is far too short */
	pos += 4 + GET_32BIT(key->keyblob + pos);   /* skip key type */
	if (key->keyblob_len < pos+4)
		goto done;					 /* key is far too short */
	len = GET_32BIT(key->keyblob + pos);   /* find cipher-type length */
	if (key->keyblob_len < pos+4+len)
		goto done;					 /* cipher type string is incomplete */
	if (len != 4 || 0 != memcmp(key->keyblob + pos + 4, "none", 4))
		answer = 1;

	done:
	*comment = dupstr(key->comment);
	memset(key->keyblob, 0, key->keyblob_size);
	m_free(key->keyblob);
	memset(&key, 0, sizeof(key));
	m_free(key);
	return answer;
}

static int sshcom_read_mpint(void *data, int len, struct mpint_pos *ret)
{
	int bits;
	int bytes;
	unsigned char *d = (unsigned char *) data;

	if (len < 4)
		goto error;
	bits = GET_32BIT(d);

	bytes = (bits + 7) / 8;
	if (len < 4+bytes)
		goto error;

	ret->start = d + 4;
	ret->bytes = bytes;
	return bytes+4;

	error:
	ret->start = NULL;
	ret->bytes = -1;
	return len;						/* ensure further calls fail as well */
}

static int sshcom_put_mpint(void *target, void *data, int len)
{
	unsigned char *d = (unsigned char *)target;
	unsigned char *i = (unsigned char *)data;
	int bits = len * 8 - 1;

	while (bits > 0) {
		if (*i & (1 << (bits & 7)))
			break;
		if (!(bits-- & 7))
			i++, len--;
	}

	PUT_32BIT(d, bits+1);
	memcpy(d+4, i, len);
	return len+4;
}

sign_key *sshcom_read(const char *filename, char *passphrase)
{
	struct sshcom_key *key = load_sshcom_key(filename);
	char *errmsg;
	int pos, len;
	const char prefix_rsa[] = "if-modn{sign{rsa";
	const char prefix_dsa[] = "dl-modp{sign{dsa";
	enum { RSA, DSA } type;
	int encrypted;
	char *ciphertext;
	int cipherlen;
	struct ssh2_userkey *ret = NULL, *retkey;
	const struct ssh_signkey *alg;
	unsigned char *blob = NULL;
	int blobsize, publen, privlen;

	if (!key)
		return NULL;

	/*
	 * Check magic number.
	 */
	if (GET_32BIT(key->keyblob) != SSHCOM_MAGIC_NUMBER) {
		errmsg = "Key does not begin with magic number";
		goto error;
	}

	/*
	 * Determine the key type.
	 */
	pos = 8;
	if (key->keyblob_len < pos+4 ||
		(len = GET_32BIT(key->keyblob + pos)) > key->keyblob_len - pos - 4) {
		errmsg = "Key blob does not contain a key type string";
		goto error;
	}
	if (len > sizeof(prefix_rsa) - 1 &&
		!memcmp(key->keyblob+pos+4, prefix_rsa, sizeof(prefix_rsa) - 1)) {
		type = RSA;
	} else if (len > sizeof(prefix_dsa) - 1 &&
		!memcmp(key->keyblob+pos+4, prefix_dsa, sizeof(prefix_dsa) - 1)) {
		type = DSA;
	} else {
		errmsg = "Key is of unknown type";
		goto error;
	}
	pos += 4+len;

	/*
	 * Determine the cipher type.
	 */
	if (key->keyblob_len < pos+4 ||
		(len = GET_32BIT(key->keyblob + pos)) > key->keyblob_len - pos - 4) {
		errmsg = "Key blob does not contain a cipher type string";
		goto error;
	}
	if (len == 4 && !memcmp(key->keyblob+pos+4, "none", 4))
		encrypted = 0;
	else if (len == 8 && !memcmp(key->keyblob+pos+4, "3des-cbc", 8))
		encrypted = 1;
	else {
		errmsg = "Key encryption is of unknown type";
		goto error;
	}
	pos += 4+len;

	/*
	 * Get hold of the encrypted part of the key.
	 */
	if (key->keyblob_len < pos+4 ||
		(len = GET_32BIT(key->keyblob + pos)) > key->keyblob_len - pos - 4) {
		errmsg = "Key blob does not contain actual key data";
		goto error;
	}
	ciphertext = (char *)key->keyblob + pos + 4;
	cipherlen = len;
	if (cipherlen == 0) {
		errmsg = "Length of key data is zero";
		goto error;
	}

	/*
	 * Decrypt it if necessary.
	 */
	if (encrypted) {
		/*
		 * Derive encryption key from passphrase and iv/salt:
		 * 
		 *  - let block A equal MD5(passphrase)
		 *  - let block B equal MD5(passphrase || A)
		 *  - block C would be MD5(passphrase || A || B) and so on
		 *  - encryption key is the first N bytes of A || B
		 */
		struct MD5Context md5c;
		unsigned char keybuf[32], iv[8];

		if (cipherlen % 8 != 0) {
			errmsg = "Encrypted part of key is not a multiple of cipher block"
				" size";
			goto error;
		}

		MD5Init(&md5c);
		MD5Update(&md5c, (unsigned char *)passphrase, strlen(passphrase));
		MD5Final(keybuf, &md5c);

		MD5Init(&md5c);
		MD5Update(&md5c, (unsigned char *)passphrase, strlen(passphrase));
		MD5Update(&md5c, keybuf, 16);
		MD5Final(keybuf+16, &md5c);

		/*
		 * Now decrypt the key blob.
		 */
		memset(iv, 0, sizeof(iv));
		des3_decrypt_pubkey_ossh(keybuf, iv, (unsigned char *)ciphertext,
								 cipherlen);

		memset(&md5c, 0, sizeof(md5c));
		memset(keybuf, 0, sizeof(keybuf));

		/*
		 * Hereafter we return WRONG_PASSPHRASE for any parsing
		 * error. (But only if we've just tried to decrypt it!
		 * Returning WRONG_PASSPHRASE for an unencrypted key is
		 * automatic doom.)
		 */
		if (encrypted)
			ret = SSH2_WRONG_PASSPHRASE;
	}

	/*
	 * Strip away the containing string to get to the real meat.
	 */
	len = GET_32BIT(ciphertext);
	if (len > cipherlen-4) {
		errmsg = "containing string was ill-formed";
		goto error;
	}
	ciphertext += 4;
	cipherlen = len;

	/*
	 * Now we break down into RSA versus DSA. In either case we'll
	 * construct public and private blobs in our own format, and
	 * end up feeding them to alg->createkey().
	 */
	blobsize = cipherlen + 256;
	blob = snewn(blobsize, unsigned char);
	privlen = 0;
	if (type == RSA) {
		struct mpint_pos n, e, d, u, p, q;
		int pos = 0;
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &e);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &d);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &n);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &u);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &p);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &q);
		if (!q.start) {
			errmsg = "key data did not contain six integers";
			goto error;
		}

		alg = &ssh_rsa;
		pos = 0;
		pos += put_string(blob+pos, "ssh-rsa", 7);
		pos += put_mp(blob+pos, e.start, e.bytes);
		pos += put_mp(blob+pos, n.start, n.bytes);
		publen = pos;
		pos += put_string(blob+pos, d.start, d.bytes);
		pos += put_mp(blob+pos, q.start, q.bytes);
		pos += put_mp(blob+pos, p.start, p.bytes);
		pos += put_mp(blob+pos, u.start, u.bytes);
		privlen = pos - publen;
	} else if (type == DSA) {
		struct mpint_pos p, q, g, x, y;
		int pos = 4;
		if (GET_32BIT(ciphertext) != 0) {
			errmsg = "predefined DSA parameters not supported";
			goto error;
		}
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &p);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &g);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &q);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &y);
		pos += sshcom_read_mpint(ciphertext+pos, cipherlen-pos, &x);
		if (!x.start) {
			errmsg = "key data did not contain five integers";
			goto error;
		}

		alg = &ssh_dss;
		pos = 0;
		pos += put_string(blob+pos, "ssh-dss", 7);
		pos += put_mp(blob+pos, p.start, p.bytes);
		pos += put_mp(blob+pos, q.start, q.bytes);
		pos += put_mp(blob+pos, g.start, g.bytes);
		pos += put_mp(blob+pos, y.start, y.bytes);
		publen = pos;
		pos += put_mp(blob+pos, x.start, x.bytes);
		privlen = pos - publen;
	}

	dropbear_assert(privlen > 0);			   /* should have bombed by now if not */

	retkey = snew(struct ssh2_userkey);
	retkey->alg = alg;
	retkey->data = alg->createkey(blob, publen, blob+publen, privlen);
	if (!retkey->data) {
		m_free(retkey);
		errmsg = "unable to create key data structure";
		goto error;
	}
	retkey->comment = dupstr(key->comment);

	errmsg = NULL; /* no error */
	ret = retkey;

	error:
	if (blob) {
		memset(blob, 0, blobsize);
		m_free(blob);
	}
	memset(key->keyblob, 0, key->keyblob_size);
	m_free(key->keyblob);
	memset(&key, 0, sizeof(key));
	m_free(key);
	return ret;
}

int sshcom_write(const char *filename, sign_key *key,
				 char *passphrase)
{
	unsigned char *pubblob, *privblob;
	int publen, privlen;
	unsigned char *outblob;
	int outlen;
	struct mpint_pos numbers[6];
	int nnumbers, initial_zero, pos, lenpos, i;
	char *type;
	char *ciphertext;
	int cipherlen;
	int ret = 0;
	FILE *fp;

	/*
	 * Fetch the key blobs.
	 */
	pubblob = key->alg->public_blob(key->data, &publen);
	privblob = key->alg->private_blob(key->data, &privlen);
	outblob = NULL;

	/*
	 * Find the sequence of integers to be encoded into the OpenSSH
	 * key blob, and also decide on the header line.
	 */
	if (key->alg == &ssh_rsa) {
		int pos;
		struct mpint_pos n, e, d, p, q, iqmp;

		pos = 4 + GET_32BIT(pubblob);
		pos += ssh2_read_mpint(pubblob+pos, publen-pos, &e);
		pos += ssh2_read_mpint(pubblob+pos, publen-pos, &n);
		pos = 0;
		pos += ssh2_read_mpint(privblob+pos, privlen-pos, &d);
		pos += ssh2_read_mpint(privblob+pos, privlen-pos, &p);
		pos += ssh2_read_mpint(privblob+pos, privlen-pos, &q);
		pos += ssh2_read_mpint(privblob+pos, privlen-pos, &iqmp);

		dropbear_assert(e.start && iqmp.start); /* can't go wrong */

		numbers[0] = e;
		numbers[1] = d;
		numbers[2] = n;
		numbers[3] = iqmp;
		numbers[4] = q;
		numbers[5] = p;

		nnumbers = 6;
		initial_zero = 0;
		type = "if-modn{sign{rsa-pkcs1-sha1},encrypt{rsa-pkcs1v2-oaep}}";
	} else if (key->alg == &ssh_dss) {
		int pos;
		struct mpint_pos p, q, g, y, x;

		pos = 4 + GET_32BIT(pubblob);
		pos += ssh2_read_mpint(pubblob+pos, publen-pos, &p);
		pos += ssh2_read_mpint(pubblob+pos, publen-pos, &q);
		pos += ssh2_read_mpint(pubblob+pos, publen-pos, &g);
		pos += ssh2_read_mpint(pubblob+pos, publen-pos, &y);
		pos = 0;
		pos += ssh2_read_mpint(privblob+pos, privlen-pos, &x);

		dropbear_assert(y.start && x.start); /* can't go wrong */

		numbers[0] = p;
		numbers[1] = g;
		numbers[2] = q;
		numbers[3] = y;
		numbers[4] = x;

		nnumbers = 5;
		initial_zero = 1;
		type = "dl-modp{sign{dsa-nist-sha1},dh{plain}}";
	} else {
		dropbear_assert(0);					 /* zoinks! */
	}

	/*
	 * Total size of key blob will be somewhere under 512 plus
	 * combined length of integers. We'll calculate the more
	 * precise size as we construct the blob.
	 */
	outlen = 512;
	for (i = 0; i < nnumbers; i++)
		outlen += 4 + numbers[i].bytes;
	outblob = snewn(outlen, unsigned char);

	/*
	 * Create the unencrypted key blob.
	 */
	pos = 0;
	PUT_32BIT(outblob+pos, SSHCOM_MAGIC_NUMBER); pos += 4;
	pos += 4;							   /* length field, fill in later */
	pos += put_string(outblob+pos, type, strlen(type));
	{
		char *ciphertype = passphrase ? "3des-cbc" : "none";
		pos += put_string(outblob+pos, ciphertype, strlen(ciphertype));
	}
	lenpos = pos;					   /* remember this position */
	pos += 4;							   /* encrypted-blob size */
	pos += 4;							   /* encrypted-payload size */
	if (initial_zero) {
		PUT_32BIT(outblob+pos, 0);
		pos += 4;
	}
	for (i = 0; i < nnumbers; i++)
		pos += sshcom_put_mpint(outblob+pos,
								numbers[i].start, numbers[i].bytes);
	/* Now wrap up the encrypted payload. */
	PUT_32BIT(outblob+lenpos+4, pos - (lenpos+8));
	/* Pad encrypted blob to a multiple of cipher block size. */
	if (passphrase) {
		int padding = -(pos - (lenpos+4)) & 7;
		while (padding--)
			outblob[pos++] = random_byte();
	}
	ciphertext = (char *)outblob+lenpos+4;
	cipherlen = pos - (lenpos+4);
	dropbear_assert(!passphrase || cipherlen % 8 == 0);
	/* Wrap up the encrypted blob string. */
	PUT_32BIT(outblob+lenpos, cipherlen);
	/* And finally fill in the total length field. */
	PUT_32BIT(outblob+4, pos);

	dropbear_assert(pos < outlen);

	/*
	 * Encrypt the key.
	 */
	if (passphrase) {
		/*
		 * Derive encryption key from passphrase and iv/salt:
		 * 
		 *  - let block A equal MD5(passphrase)
		 *  - let block B equal MD5(passphrase || A)
		 *  - block C would be MD5(passphrase || A || B) and so on
		 *  - encryption key is the first N bytes of A || B
		 */
		struct MD5Context md5c;
		unsigned char keybuf[32], iv[8];

		MD5Init(&md5c);
		MD5Update(&md5c, (unsigned char *)passphrase, strlen(passphrase));
		MD5Final(keybuf, &md5c);

		MD5Init(&md5c);
		MD5Update(&md5c, (unsigned char *)passphrase, strlen(passphrase));
		MD5Update(&md5c, keybuf, 16);
		MD5Final(keybuf+16, &md5c);

		/*
		 * Now decrypt the key blob.
		 */
		memset(iv, 0, sizeof(iv));
		des3_encrypt_pubkey_ossh(keybuf, iv, (unsigned char *)ciphertext,
								 cipherlen);

		memset(&md5c, 0, sizeof(md5c));
		memset(keybuf, 0, sizeof(keybuf));
	}

	/*
	 * And save it. We'll use Unix line endings just in case it's
	 * subsequently transferred in binary mode.
	 */
	fp = fopen(filename, "wb");	  /* ensure Unix line endings */
	if (!fp)
		goto error;
	fputs("---- BEGIN SSH2 ENCRYPTED PRIVATE KEY ----\n", fp);
	fprintf(fp, "Comment: \"");
	/*
	 * Comment header is broken with backslash-newline if it goes
	 * over 70 chars. Although it's surrounded by quotes, it
	 * _doesn't_ escape backslashes or quotes within the string.
	 * Don't ask me, I didn't design it.
	 */
	{
		int slen = 60;					   /* starts at 60 due to "Comment: " */
		char *c = key->comment;
		while ((int)strlen(c) > slen) {
			fprintf(fp, "%.*s\\\n", slen, c);
			c += slen;
			slen = 70;					   /* allow 70 chars on subsequent lines */
		}
		fprintf(fp, "%s\"\n", c);
	}
	base64_encode_fp(fp, outblob, pos, 70);
	fputs("---- END SSH2 ENCRYPTED PRIVATE KEY ----\n", fp);
	fclose(fp);
	ret = 1;

	error:
	if (outblob) {
		memset(outblob, 0, outlen);
		m_free(outblob);
	}
	if (privblob) {
		memset(privblob, 0, privlen);
		m_free(privblob);
	}
	if (pubblob) {
		memset(pubblob, 0, publen);
		m_free(pubblob);
	}
	return ret;
}
#endif /* ssh.com stuff disabled */
