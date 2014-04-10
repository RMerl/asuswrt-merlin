
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufslabel.c
 * - library routines to read/write the UFS disk label
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */

#include "dkopen.h"
#include <sys/types.h>
#include <sys/wait.h>
#ifndef linux
#include <sys/disk.h>
#endif
#include <fcntl.h>
#include <sys/errno.h>


#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 
#include <sys/stat.h>
#include <sys/time.h> 
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <sys/param.h>
#include <ufs/ffs/fs.h>

#include "ufslabel.h"

char ufs_label_magic[4] = UFS_LABEL_MAGIC;

typedef union {
    char	c[2];
    u_short	s;
} short_union_t;

typedef union {
    u_short 	s[2];
    long	l;
} long_union_t;

typedef struct {
    unsigned long state[5];
    unsigned long count[2];
    unsigned char buffer[64];
} SHA1_CTX;

#define VOLUMEUUIDVALUESIZE 2
typedef union VolumeUUID {
	unsigned long value[VOLUMEUUIDVALUESIZE];
	struct {
		unsigned long high;
		unsigned long low;
	} v;
} VolumeUUID;

#define VOLUMEUUIDLENGTH 16
typedef char VolumeUUIDString[VOLUMEUUIDLENGTH+1];

/*
 * Local Functions:
 */
static __inline__ void
reduce(int * sum)
{
    long_union_t l_util;

    l_util.l = *sum;
    *sum = l_util.s[0] + l_util.s[1];
    if (*sum > 65535)
	*sum -= 65535;
    return;
}

static unsigned short
in_cksum(void * data, int len)
{
	u_short * w;
	int sum = 0;

	w = (u_short *)data;
	while ((len -= 32) >= 0) {
	    sum += w[0]; sum += w[1]; 
	    sum += w[2]; sum += w[3];
	    sum += w[4]; sum += w[5]; 
	    sum += w[6]; sum += w[7];
	    sum += w[8]; sum += w[9]; 
	    sum += w[10]; sum += w[11];
	    sum += w[12]; sum += w[13]; 
	    sum += w[14]; sum += w[15];
	    w += 16;
	}
	len += 32;
	while ((len -= 8) >= 0) {
	    sum += w[0]; sum += w[1]; 
	    sum += w[2]; sum += w[3];
	    w += 4;
	}
	len += 8;
	if (len) {
	    reduce(&sum);
	    while ((len -= 2) >= 0) {
		sum += *w++;
	    }
	}
	if (len == -1) { /* odd-length data */
	    short_union_t s_util;

	    s_util.s = 0;
	    s_util.c[0] = *((char *)w);
	    s_util.c[1] = 0;
	    sum += s_util.s;
	}
	reduce(&sum);
	return (~sum & 0xffff);
}

static boolean_t
ufslabel_check(struct ufslabel * ul_p)
{
    if (bcmp(&ul_p->ul_magic, ufs_label_magic, 
	     sizeof(ul_p->ul_magic))) {
#ifdef DEBUG
	fprintf(stderr, "check_label: label has bad magic number\n");
#endif DEBUG
	return (FALSE);
    }
    if (ntohl(ul_p->ul_version) != UFS_LABEL_VERSION) {
#ifdef DEBUG
	fprintf(stderr, 
		"check_label: label has incorect version %d (should be %d)\n",
		ntohl(ul_p->ul_version), UFS_LABEL_VERSION);
#endif DEBUG
	return (FALSE);
    }
    if (ntohs(ul_p->ul_namelen) > UFS_MAX_LABEL_NAME) {
#ifdef DEBUG
	fprintf(stderr, "check_label: name length %d is too big (> %d)\n",
		ntohs(ul_p->ul_namelen), UFS_MAX_LABEL_NAME);
#endif DEBUG
	return (FALSE);
    }
    {
	u_int16_t	calc;	
	u_int16_t 	checksum = ul_p->ul_checksum;
	
	ul_p->ul_checksum = 0;
	calc = in_cksum(ul_p, sizeof(*ul_p));
	if (calc != checksum) {
#ifdef DEBUG
	    fprintf(stderr, "check_label: label checksum %x (should be %x)\n",
		    checksum, calc);
#endif DEBUG
	    return (FALSE);
	}
    }
    return (TRUE);
}

static void *
ufslabel_read_blocks(int fd, off_t * offset_p, ssize_t * size_p, 
		     struct ufslabel * * label_p)
{
    int			blocksize;
    off_t 		offset;
    ssize_t 		size;
    void *		ptr = NULL;
    struct ufslabel *	ul_p;

    *label_p = NULL;

#ifndef linux
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &blocksize) < 0) {
	fprintf(stderr, "DKIOCGETBLOCKSIZE failed, %s\n", strerror(errno));
	return (NULL);
    }
#else
    blocksize = DEV_BSIZE;
#endif

    offset = UFS_LABEL_OFFSET / blocksize * blocksize;
    size = (UFS_LABEL_SIZE + blocksize - 1) / blocksize * blocksize;

    if (dklseek(fd, offset, SEEK_SET) != offset) {
	fprintf(stderr, "ufslabel_read_blocks: lseek failed, %s\n",
		strerror(errno));
	return (NULL);
    }

    ptr = malloc(size);
    if (ptr == NULL) {
	fprintf(stderr, "ufslabel_read_blocks: malloc() failed\n");
	return (NULL);
    }
    ul_p = (struct ufslabel *)(ptr + (UFS_LABEL_OFFSET - offset));
    
    if (read(fd, ptr, size) != size) {
	fprintf(stderr, "ufslabel_read_blocks: read failed, %s\n",
		strerror(errno));
	goto fail;
    }
    if (label_p)
	*label_p = ul_p;
    if (offset_p)
	*offset_p = offset;
    if (size_p)
	*size_p = size;
    return (ptr);
 fail:
    if (ptr)
	free(ptr);
    return (NULL);
}

static boolean_t
ufslabel_read(int fd, struct ufslabel * label_p)
{
    struct ufslabel * 	ul_p;
    void * 		ptr;
    boolean_t		ret = TRUE;

    ptr = ufslabel_read_blocks(fd, NULL, NULL, &ul_p);
    if (ptr == NULL)
	return (FALSE);

    if (ufslabel_check(ul_p) == FALSE) {
	ret = FALSE;
	goto fail;
    }
    *label_p = *ul_p;

 fail:
    free(ptr);
    return (ret);
}

static boolean_t
ufslabel_write(int fd, struct ufslabel * label_p)
{
    off_t 		offset;
    ssize_t		size;
    void *		ptr = NULL;
    struct ufslabel *	ul_p;

    /* get blocks that will contain the new label */
    ptr = ufslabel_read_blocks(fd, &offset, &size, &ul_p);
    if (ptr == NULL)
	return (FALSE);

    /* copy the label into the raw blocks */
    *ul_p = *label_p;

    /* make sure the checksum is updated */
    ul_p->ul_checksum = 0;
    ul_p->ul_checksum = in_cksum(ul_p, sizeof(*ul_p));

    /* write the new label */
    if (dklseek(fd, offset, SEEK_SET) != offset) {
	fprintf(stderr, "ufslabel_write: lseek failed, %s\n",
		strerror(errno));
	goto fail;
    }
    if (write(fd, ptr, size) != (ssize_t)size) {
	fprintf(stderr, "ufslabel_write: write failed, %s\n",
		strerror(errno));
	goto fail;
    }
    if (ptr)
	free(ptr);
    return (TRUE);

 fail:
    if (ptr)
	free(ptr);
    return (FALSE);
}


/******************************************************************************
 *
 *  S H A - 1   I M P L E M E N T A T I O N   R O U T I N E S
 *
 *****************************************************************************/

/*
	Derived from SHA-1 in C
	By Steve Reid <steve@edmweb.com>
	100% Public Domain

	Test Vectors (from FIPS PUB 180-1)
		"abc"
			A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
			84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
		A million repetitions of "a"
			34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

/* #define LITTLE_ENDIAN * This should be #define'd if true. */
/* #define SHA1HANDSOFF * Copies data before messing with it. */

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifdef LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#else
#define blk0(i) block->l[i]
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#if TRACE_HASH
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);printf("t = %2d: %08lX %08lX %08lX %08lX %08lX\n", i, a, b, c, d, e);
#else
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);
#endif


/* Hash a single 512-bit block. This is the core of the algorithm. */

static void SHA1Transform(unsigned long state[5], unsigned char buffer[64])
{
unsigned long a, b, c, d, e;
typedef union {
    unsigned char c[64];
    unsigned long l[16];
} CHAR64LONG16;
CHAR64LONG16* block;
#ifdef SHA1HANDSOFF
static unsigned char workspace[64];
    block = (CHAR64LONG16*)workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif
    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
#if TRACE_HASH
    printf("            A        B        C        D        E\n");
    printf("        -------- -------- -------- -------- --------\n");
    printf("        %08lX %08lX %08lX %08lX %08lX\n", a, b, c, d, e);
#endif
    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    /* Wipe variables */
    a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */

static void SHA1Init(SHA1_CTX* context)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

static void SHA1Update(SHA1_CTX* context, void* data, size_t len)
{
	unsigned char *dataptr = data;
	unsigned int i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], dataptr, (i = 64-j));
        SHA1Transform(context->state, context->buffer);
        for ( ; i + 63 < len; i += 64) {
            SHA1Transform(context->state, &dataptr[i]);
        }
        j = 0;
    }
    else i = 0;
    memcpy(&context->buffer[j], &dataptr[i], len - i);
}


/* Add padding and return the message digest. */

static void SHA1Final(unsigned char digest[20], SHA1_CTX* context)
{
unsigned long i, j;
unsigned char finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
    }
    SHA1Update(context, (unsigned char *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        SHA1Update(context, (unsigned char *)"\0", 1);
    }
    SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform() */
    for (i = 0; i < 20; i++) {
        digest[i] = (unsigned char)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
    /* Wipe variables */
    i = j = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(&finalcount, 0, 8);
#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite it's own static vars */
    SHA1Transform(context->state, context->buffer);
#endif
}


static void FormatULong(unsigned long u, char *s) {
	unsigned long d;
	int i;
	char *digitptr = s;

	for (i = 0; i < 8; ++i) {
		d = ((u & 0xF0000000) >> 28) & 0x0000000F;
		if (d < 10) {
			*digitptr++ = (char)(d + '0');
		} else {
			*digitptr++ = (char)(d - 10 + 'A');
		};
		u = u << 4;
	};
}


void GenerateVolumeUUID(VolumeUUID *newVolumeID) {
	SHA1_CTX context;
	char randomInputBuffer[26];
	unsigned char digest[20];
	time_t now;
	clock_t uptime;
	int mib[2];
	int sysdata;
	char sysctlstring[128];
	size_t datalen;
	double sysloadavg[3];
	struct vmtotal sysvmtotal;
	
	do {
		/* Initialize the SHA-1 context for processing: */
		SHA1Init(&context);
		
		/* Now process successive bits of "random" input to seed the process: */
		
		/* The current system's uptime: */
		uptime = clock();
		SHA1Update(&context, &uptime, sizeof(uptime));
		
		/* The kernel's boot time: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_BOOTTIME;
		datalen = sizeof(sysdata);
		sysctl(mib, 2, &sysdata, &datalen, NULL, 0);
		SHA1Update(&context, &sysdata, datalen);
		
		/* The system's host id: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_HOSTID;
		datalen = sizeof(sysdata);
		sysctl(mib, 2, &sysdata, &datalen, NULL, 0);
		SHA1Update(&context, &sysdata, datalen);

		/* The system's host name: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_HOSTNAME;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1Update(&context, sysctlstring, datalen);

		/* The running kernel's OS release string: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_OSRELEASE;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1Update(&context, sysctlstring, datalen);

		/* The running kernel's version string: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_VERSION;
		datalen = sizeof(sysctlstring);
		sysctl(mib, 2, sysctlstring, &datalen, NULL, 0);
		SHA1Update(&context, sysctlstring, datalen);

		/* The system's load average: */
		datalen = sizeof(sysloadavg);
		getloadavg(sysloadavg, 3);
		SHA1Update(&context, &sysloadavg, datalen);

		/* The system's VM statistics: */
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		datalen = sizeof(sysvmtotal);
		sysctl(mib, 2, &sysvmtotal, &datalen, NULL, 0);
		SHA1Update(&context, &sysvmtotal, datalen);

		/* The current GMT (26 ASCII characters): */
		time(&now);
		strncpy(randomInputBuffer, asctime(gmtime(&now)), 26);	/* "Mon Mar 27 13:46:26 2000" */
		SHA1Update(&context, randomInputBuffer, 26);
		
		/* Pad the accumulated input and extract the final digest hash: */
		SHA1Final(digest, &context);
	
		memcpy(newVolumeID, digest, sizeof(*newVolumeID));
	} while ((newVolumeID->v.high == 0) || (newVolumeID->v.low == 0));
}


static void FormatUUID(VolumeUUID *volumeID, char *UUIDField) {
	FormatULong(volumeID->v.high, UUIDField);
	FormatULong(volumeID->v.low, UUIDField+8);

};


void ConvertVolumeUUIDStringToUUID(const char *UUIDString, VolumeUUID *volumeID) {
	int i;
	char c;
	unsigned long nextdigit;
	unsigned long high = 0;
	unsigned long low = 0;
	unsigned long carry;
	
	for (i = 0; (i < VOLUMEUUIDLENGTH) && ((c = UUIDString[i]) != (char)0) ; ++i) {
		if ((c >= '0') && (c <= '9')) {
			nextdigit = c - '0';
		} else if ((c >= 'A') && (c <= 'F')) {
			nextdigit = c - 'A' + 10;
		} else if ((c >= 'a') && (c <= 'f')) {
			nextdigit = c - 'a' + 10;
		} else {
			nextdigit = 0;
		};
		carry = ((low & 0xF0000000) >> 28) & 0x0000000F;
		high = (high << 4) | carry;
		low = (low << 4) | nextdigit;
	};
	
	volumeID->v.high = high;
	volumeID->v.low = low;
}



void ConvertVolumeUUIDToString(VolumeUUID *volumeID, char *UUIDString) {
	FormatUUID(volumeID, UUIDString);
	*(UUIDString+16) = (char)0;		/* Append a terminating null character */
}



/*
 * Exported Functions:
 */

void
ufslabel_get_name(struct ufslabel * ul_p, char * name, int * len)
{
    if (ntohs(ul_p->ul_namelen) < *len)
	*len = ntohs(ul_p->ul_namelen);
    bcopy(ul_p->ul_name, name, *len);
}

void
ufslabel_get_uuid(struct ufslabel * ul_p, char * uuid)
{
    u_int32_t *volumeUUID;
    VolumeUUID swappedUUID;
    
    volumeUUID = (u_int32_t *) &ul_p->ul_uuid;
	
    swappedUUID.v.high = ntohl(volumeUUID[0]);
    swappedUUID.v.low = ntohl(volumeUUID[1]);

    ConvertVolumeUUIDToString(&swappedUUID, uuid);
}

boolean_t
ufslabel_set_name(struct ufslabel * ul_p, char * name, int len)
{
    if (len > UFS_MAX_LABEL_NAME) {
	fprintf(stderr, "ufslabel_set_name: name length %d too long (>%d)\n", 
		len, UFS_MAX_LABEL_NAME);
	return (FALSE);
    }
    ul_p->ul_namelen = htons(len);
    bcopy(name, ul_p->ul_name, len);
    return (TRUE);
}

void
ufslabel_set_uuid(struct ufslabel * ul_p)
{
    u_int32_t *volumeUUID;
    VolumeUUID newUUID;

    GenerateVolumeUUID(&newUUID);
    
    volumeUUID = (u_int32_t *) &ul_p->ul_uuid;

    volumeUUID[0] = htonl(newUUID.v.high);
    volumeUUID[1] = htonl(newUUID.v.low);
}

void
ufslabel_init(struct ufslabel * ul_p)
{
    struct timeval tv;

    bzero(ul_p, sizeof(*ul_p));
    ul_p->ul_version = htonl(UFS_LABEL_VERSION);
    bcopy(ufs_label_magic, &ul_p->ul_magic, sizeof(ul_p->ul_magic));
    gettimeofday(&tv, 0);
    ul_p->ul_time = htonl(tv.tv_sec);
}


boolean_t
ufslabel_get(int fd, struct ufslabel * label)
{
    if (ufslabel_read(fd, label) == FALSE)
	return (FALSE);
    return (TRUE);
}


boolean_t
ufslabel_set(int fd, struct ufslabel * label)
{
    if (ufslabel_write(fd, label) == FALSE)
	return (FALSE);
    return (TRUE);
}
