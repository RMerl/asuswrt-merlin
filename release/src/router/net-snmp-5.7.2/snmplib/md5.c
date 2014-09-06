/*
 * ** **************************************************************************
 * ** md5.c -- Implementation of MD5 Message Digest Algorithm                 **
 * ** Updated: 2/16/90 by Ronald L. Rivest                                    **
 * ** (C) 1990 RSA Data Security, Inc.                                        **
 * ** **************************************************************************
 */

/*
 * ** To use MD5:
 * **   -- Include md5.h in your program
 * **   -- Declare an MDstruct MD to hold the state of the digest computation.
 * **   -- Initialize MD using MDbegin(&MD)
 * **   -- For each full block (64 bytes) X you wish to process, call
 * **          MDupdate(&MD,X,512)
 * **      (512 is the number of bits in a full block.)
 * **   -- For the last block (less than 64 bytes) you wish to process,
 * **          MDupdate(&MD,X,n)
 * **      where n is the number of bits in the partial block. A partial
 * **      block terminates the computation, so every MD computation should
 * **      terminate by processing a partial block, even if it has n = 0.
 * **   -- The message digest is available in MD.buffer[0] ... MD.buffer[3].
 * **      (Least-significant byte of each word should be output first.)
 * **   -- You can print out the digest using MDprint(&MD)
 */

/*
 * Implementation notes:
 * ** This implementation assumes that ints are 32-bit quantities.
 * ** If the machine stores the least-significant byte of an int in the
 * ** least-addressed byte (eg., VAX and 8086), then LOWBYTEFIRST should be
 * ** set to TRUE.  Otherwise (eg., SUNS), LOWBYTEFIRST should be set to
 * ** FALSE.  Note that on machines with LOWBYTEFIRST FALSE the routine
 * ** MDupdate modifies has a side-effect on its input array (the order of bytes
 * ** in each word are reversed).  If this is undesired a call to MDreverse(X) can
 * ** reverse the bytes of X back into order after each call to MDupdate.
 */

/*
 * code uses WORDS_BIGENDIAN defined by configure now  -- WH 9/27/95 
 */

/*
 * Compile-time includes 
 */

#include <net-snmp/net-snmp-config.h>

#ifndef NETSNMP_DISABLE_MD5

#include <stdio.h>
#include <sys/types.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/utilities.h>
#include <net-snmp/library/md5.h>

/*
 * Compile-time declarations of MD5 ``magic constants''.
 */
#define I0  0x67452301          /* Initial values for MD buffer */
#define I1  0xefcdab89
#define I2  0x98badcfe
#define I3  0x10325476
#define fs1  7                  /* round 1 shift amounts */
#define fs2 12
#define fs3 17
#define fs4 22
#define gs1  5                  /* round 2 shift amounts */
#define gs2  9
#define gs3 14
#define gs4 20
#define hs1  4                  /* round 3 shift amounts */
#define hs2 11
#define hs3 16
#define hs4 23
#define is1  6                  /* round 4 shift amounts */
#define is2 10
#define is3 15
#define is4 21


/*
 * Compile-time macro declarations for MD5.
 * ** Note: The ``rot'' operator uses the variable ``tmp''.
 * ** It assumes tmp is declared as unsigned int, so that the >>
 * ** operator will shift in zeros rather than extending the sign bit.
 */
#define	f(X,Y,Z)             ((X&Y) | ((~X)&Z))
#define	g(X,Y,Z)             ((X&Z) | (Y&(~Z)))
#define h(X,Y,Z)             (X^Y^Z)
#define i_(X,Y,Z)            (Y ^ ((X) | (~Z)))
#define rot(X,S)             (tmp=X,(tmp<<S) | (tmp>>(32-S)))
#define ff(A,B,C,D,i,s,lp)   A = rot((A + f(B,C,D) + X[i] + lp),s) + B
#define gg(A,B,C,D,i,s,lp)   A = rot((A + g(B,C,D) + X[i] + lp),s) + B
#define hh(A,B,C,D,i,s,lp)   A = rot((A + h(B,C,D) + X[i] + lp),s) + B
#define ii(A,B,C,D,i,s,lp)   A = rot((A + i_(B,C,D) + X[i] + lp),s) + B

#ifdef STDC_HEADERS
#define Uns(num) num##U
#else
#define Uns(num) num
#endif                          /* STDC_HEADERS */

void            MDreverse(unsigned int *);
static void     MDblock(MDptr, const unsigned int *);

#ifdef NETSNMP_ENABLE_TESTING_CODE
/*
 * MDprint(MDp)
 * ** Print message digest buffer MDp as 32 hexadecimal digits.
 * ** Order is from low-order byte of buffer[0] to high-order byte of buffer[3].
 * ** Each byte is printed with high-order hexadecimal digit first.
 * ** This is a user-callable routine.
 */
void
MDprint(MDptr MDp)
{
    int             i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 32; j = j + 8)
            printf("%02x", (MDp->buffer[i] >> j) & 0xFF);
    printf("\n");
    fflush(stdout);
}
#endif                          /* NETSNMP_ENABLE_TESTING_CODE */

/*
 * MDbegin(MDp)
 * ** Initialize message digest buffer MDp. 
 * ** This is a user-callable routine.
 */
void
MDbegin(MDptr MDp)
{
    int             i;
    MDp->buffer[0] = I0;
    MDp->buffer[1] = I1;
    MDp->buffer[2] = I2;
    MDp->buffer[3] = I3;
    for (i = 0; i < 8; i++)
        MDp->count[i] = 0;
    MDp->done = 0;
}

/*
 * MDreverse(X)
 * ** Reverse the byte-ordering of every int in X.
 * ** Assumes X is an array of 16 ints.
 * ** The macro revx reverses the byte-ordering of the next word of X.
 */
#define revx { t = (*X << 16) | (*X >> 16); \
	       *X++ = ((t & 0xFF00FF00) >> 8) | ((t & 0x00FF00FF) << 8); }

void
MDreverse(unsigned int *X)
{
    register unsigned int t;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
    revx;
}

/*
 * MDblock(MDp,X)
 * ** Update message digest buffer MDp->buffer using 16-word data block X.
 * ** Assumes all 16 words of X are full of data.
 * ** Does not update MDp->count.
 * ** This routine is not user-callable. 
 */
static void
MDblock(MDptr MDp, const unsigned int *X)
{
    register unsigned int tmp, A, B, C, D;      /* hpux sysv sun */
#ifdef WORDS_BIGENDIAN
    MDreverse(X);
#endif
    A = MDp->buffer[0];
    B = MDp->buffer[1];
    C = MDp->buffer[2];
    D = MDp->buffer[3];

    /*
     * Update the message digest buffer 
     */
    ff(A, B, C, D, 0, fs1, Uns(3614090360));    /* Round 1 */
    ff(D, A, B, C, 1, fs2, Uns(3905402710));
    ff(C, D, A, B, 2, fs3, Uns(606105819));
    ff(B, C, D, A, 3, fs4, Uns(3250441966));
    ff(A, B, C, D, 4, fs1, Uns(4118548399));
    ff(D, A, B, C, 5, fs2, Uns(1200080426));
    ff(C, D, A, B, 6, fs3, Uns(2821735955));
    ff(B, C, D, A, 7, fs4, Uns(4249261313));
    ff(A, B, C, D, 8, fs1, Uns(1770035416));
    ff(D, A, B, C, 9, fs2, Uns(2336552879));
    ff(C, D, A, B, 10, fs3, Uns(4294925233));
    ff(B, C, D, A, 11, fs4, Uns(2304563134));
    ff(A, B, C, D, 12, fs1, Uns(1804603682));
    ff(D, A, B, C, 13, fs2, Uns(4254626195));
    ff(C, D, A, B, 14, fs3, Uns(2792965006));
    ff(B, C, D, A, 15, fs4, Uns(1236535329));
    gg(A, B, C, D, 1, gs1, Uns(4129170786));    /* Round 2 */
    gg(D, A, B, C, 6, gs2, Uns(3225465664));
    gg(C, D, A, B, 11, gs3, Uns(643717713));
    gg(B, C, D, A, 0, gs4, Uns(3921069994));
    gg(A, B, C, D, 5, gs1, Uns(3593408605));
    gg(D, A, B, C, 10, gs2, Uns(38016083));
    gg(C, D, A, B, 15, gs3, Uns(3634488961));
    gg(B, C, D, A, 4, gs4, Uns(3889429448));
    gg(A, B, C, D, 9, gs1, Uns(568446438));
    gg(D, A, B, C, 14, gs2, Uns(3275163606));
    gg(C, D, A, B, 3, gs3, Uns(4107603335));
    gg(B, C, D, A, 8, gs4, Uns(1163531501));
    gg(A, B, C, D, 13, gs1, Uns(2850285829));
    gg(D, A, B, C, 2, gs2, Uns(4243563512));
    gg(C, D, A, B, 7, gs3, Uns(1735328473));
    gg(B, C, D, A, 12, gs4, Uns(2368359562));
    hh(A, B, C, D, 5, hs1, Uns(4294588738));    /* Round 3 */
    hh(D, A, B, C, 8, hs2, Uns(2272392833));
    hh(C, D, A, B, 11, hs3, Uns(1839030562));
    hh(B, C, D, A, 14, hs4, Uns(4259657740));
    hh(A, B, C, D, 1, hs1, Uns(2763975236));
    hh(D, A, B, C, 4, hs2, Uns(1272893353));
    hh(C, D, A, B, 7, hs3, Uns(4139469664));
    hh(B, C, D, A, 10, hs4, Uns(3200236656));
    hh(A, B, C, D, 13, hs1, Uns(681279174));
    hh(D, A, B, C, 0, hs2, Uns(3936430074));
    hh(C, D, A, B, 3, hs3, Uns(3572445317));
    hh(B, C, D, A, 6, hs4, Uns(76029189));
    hh(A, B, C, D, 9, hs1, Uns(3654602809));
    hh(D, A, B, C, 12, hs2, Uns(3873151461));
    hh(C, D, A, B, 15, hs3, Uns(530742520));
    hh(B, C, D, A, 2, hs4, Uns(3299628645));
    ii(A, B, C, D, 0, is1, Uns(4096336452));    /* Round 4 */
    ii(D, A, B, C, 7, is2, Uns(1126891415));
    ii(C, D, A, B, 14, is3, Uns(2878612391));
    ii(B, C, D, A, 5, is4, Uns(4237533241));
    ii(A, B, C, D, 12, is1, Uns(1700485571));
    ii(D, A, B, C, 3, is2, Uns(2399980690));
    ii(C, D, A, B, 10, is3, Uns(4293915773));
    ii(B, C, D, A, 1, is4, Uns(2240044497));
    ii(A, B, C, D, 8, is1, Uns(1873313359));
    ii(D, A, B, C, 15, is2, Uns(4264355552));
    ii(C, D, A, B, 6, is3, Uns(2734768916));
    ii(B, C, D, A, 13, is4, Uns(1309151649));
    ii(A, B, C, D, 4, is1, Uns(4149444226));
    ii(D, A, B, C, 11, is2, Uns(3174756917));
    ii(C, D, A, B, 2, is3, Uns(718787259));
    ii(B, C, D, A, 9, is4, Uns(3951481745));

    MDp->buffer[0] += A;
    MDp->buffer[1] += B;
    MDp->buffer[2] += C;
    MDp->buffer[3] += D;
#ifdef WORDS_BIGENDIAN
    MDreverse(X);
#endif
}

/*
 * MDupdate(MDp,X,count)
 * ** Input: MDp -- an MDptr
 * **        X -- a pointer to an array of unsigned characters.
 * **        count -- the number of bits of X to use.
 * **                 (if not a multiple of 8, uses high bits of last byte.)
 * ** Update MDp using the number of bits of X given by count.
 * ** This is the basic input routine for an MD5 user.
 * ** The routine completes the MD computation when count < 512, so
 * ** every MD computation should end with one call to MDupdate with a
 * ** count less than 512.  A call with count 0 will be ignored if the
 * ** MD has already been terminated (done != 0), so an extra call with count
 * ** 0 can be given as a ``courtesy close'' to force termination if desired.
 * ** Returns : 0 if processing succeeds or was already done;
 * **          -1 if processing was already done
 * **          -2 if count was too large
 */
int
MDupdate(MDptr MDp, const unsigned char *X, unsigned int count)
{
    unsigned int    i, tmp, bit, byte, mask;
    unsigned char   XX[64];
    unsigned char  *p;
    /*
     * return with no error if this is a courtesy close with count
     * ** zero and MDp->done is true.
     */
    if (count == 0 && MDp->done)
        return 0;
    /*
     * check to see if MD is already done and report error 
     */
    if (MDp->done) {
        return -1;
    }
    /*
     * if (MDp->done) { fprintf(stderr,"\nError: MDupdate MD already done."); return; }
     */
    /*
     * Add count to MDp->count 
     */
    tmp = count;
    p = MDp->count;
    while (tmp) {
        tmp += *p;
        *p++ = tmp;
        tmp = tmp >> 8;
    }
    /*
     * Process data 
     */
    if (count == 512) {         /* Full block of data to handle */
        MDblock(MDp, (const unsigned int *) X);
    } else if (count > 512)     /* Check for count too large */
        return -2;
    /*
     * { fprintf(stderr,"\nError: MDupdate called with illegal count value %d.",count);
     * return;
     * }
     */
    else {                      /* partial block -- must be last block so finish up */
        /*
         * Find out how many bytes and residual bits there are 
         */
        int             copycount;
        byte = count >> 3;
        bit = count & 7;
        copycount = byte;
        if (bit)
            copycount++;
        /*
         * Copy X into XX since we need to modify it 
         */
        memset(XX, 0, sizeof(XX));
        memcpy(XX, X, copycount);

        /*
         * Add padding '1' bit and low-order zeros in last byte 
         */
        mask = ((unsigned long) 1) << (7 - bit);
        XX[byte] = (XX[byte] | mask) & ~(mask - 1);
        /*
         * If room for bit count, finish up with this block 
         */
        if (byte <= 55) {
            for (i = 0; i < 8; i++)
                XX[56 + i] = MDp->count[i];
            MDblock(MDp, (unsigned int *) XX);
        } else {                /* need to do two blocks to finish up */
            MDblock(MDp, (unsigned int *) XX);
            for (i = 0; i < 56; i++)
                XX[i] = 0;
            for (i = 0; i < 8; i++)
                XX[56 + i] = MDp->count[i];
            MDblock(MDp, (unsigned int *) XX);
        }
        /*
         * Set flag saying we're done with MD computation 
         */
        MDp->done = 1;
    }
    return 0;
}

/*
 * MDchecksum(data, len, MD5): do a checksum on an arbirtrary amount of data 
 */
int
MDchecksum(const u_char * data, size_t len, u_char * mac, size_t maclen)
{
    MDstruct        md;
    MDstruct       *MD = &md;
    int             rc = 0;

    MDbegin(MD);
    while (len >= 64) {
        rc = MDupdate(MD, data, 64 * 8);
        if (rc)
            goto check_end;
        data += 64;
        len -= 64;
    }
    rc = MDupdate(MD, data, len * 8);
    if (rc)
        goto check_end;

    /*
     * copy the checksum to the outgoing data (all of it that is requested). 
     */
    MDget(MD, mac, maclen);

  check_end:
    memset(&md, 0, sizeof(md));
    return rc;
}


/*
 * MDsign(data, len, MD5): do a checksum on an arbirtrary amount
 * of data, and prepended with a secret in the standard fashion 
 */
int
MDsign(const u_char * data, size_t len, u_char * mac, size_t maclen,
       const u_char * secret, size_t secretlen)
{
#define HASHKEYLEN 64

    MDstruct        MD;
    u_char          K1[HASHKEYLEN];
    u_char          K2[HASHKEYLEN];
    u_char          extendedAuthKey[HASHKEYLEN];
    u_char          buf[HASHKEYLEN];
    size_t          i;
    const u_char   *cp;
    u_char         *newdata = NULL;
    int             rc = 0;

    /*
     * memset(K1,0,HASHKEYLEN);
     * memset(K2,0,HASHKEYLEN);
     * memset(buf,0,HASHKEYLEN);
     * memset(extendedAuthKey,0,HASHKEYLEN);
     */

    if (secretlen != 16 || secret == NULL || mac == NULL || data == NULL ||
        len <= 0 || maclen <= 0) {
        /*
         * DEBUGMSGTL(("md5","MD5 signing not properly initialized")); 
         */
        return -1;
    }

    memset(extendedAuthKey, 0, HASHKEYLEN);
    memcpy(extendedAuthKey, secret, secretlen);
    for (i = 0; i < HASHKEYLEN; i++) {
        K1[i] = extendedAuthKey[i] ^ 0x36;
        K2[i] = extendedAuthKey[i] ^ 0x5c;
    }

    MDbegin(&MD);
    rc = MDupdate(&MD, K1, HASHKEYLEN * 8);
    if (rc)
        goto update_end;

    i = len;
    if (((uintptr_t) data) % sizeof(long) != 0) {
        /*
         * this relies on the ability to use integer math and thus we
         * must rely on data that aligns on 32-bit-word-boundries 
         */
        memdup(&newdata, data, len);
        cp = newdata;
    } else {
        cp = data;
    }

    while (i >= 64) {
        rc = MDupdate(&MD, cp, 64 * 8);
        if (rc)
            goto update_end;
        cp += 64;
        i -= 64;
    }

    rc = MDupdate(&MD, cp, i * 8);
    if (rc)
        goto update_end;

    memset(buf, 0, HASHKEYLEN);
    MDget(&MD, buf, HASHKEYLEN);

    MDbegin(&MD);
    rc = MDupdate(&MD, K2, HASHKEYLEN * 8);
    if (rc)
        goto update_end;
    rc = MDupdate(&MD, buf, 16 * 8);
    if (rc)
        goto update_end;

    /*
     * copy the sign checksum to the outgoing pointer 
     */
    MDget(&MD, mac, maclen);

  update_end:
    memset(buf, 0, HASHKEYLEN);
    memset(K1, 0, HASHKEYLEN);
    memset(K2, 0, HASHKEYLEN);
    memset(extendedAuthKey, 0, HASHKEYLEN);
    memset(&MD, 0, sizeof(MD));

    if (newdata)
        free(newdata);
    return rc;
}

void
MDget(MDstruct * MD, u_char * buf, size_t buflen)
{
    int             i, j;

    /*
     * copy the checksum to the outgoing data (all of it that is requested). 
     */
    for (i = 0; i < 4 && i * 4 < (int) buflen; i++)
        for (j = 0; j < 4 && i * 4 + j < (int) buflen; j++)
            buf[i * 4 + j] = (MD->buffer[i] >> j * 8) & 0xff;
}

/*
 * ** End of md5.c
 * ****************************(cut)****************************************
 */

#endif /* NETSNMP_DISABLE_MD5 */
