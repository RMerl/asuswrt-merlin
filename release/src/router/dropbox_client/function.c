#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/hmac.h>
#include <math.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include<signal.h>
#include "function.h"

char *case_conflict_name = "case-conflict";
extern int exit_loop;

char *my_str_malloc(size_t len){

    //printf("len = %d\n",len);

    char *s;
    s = (char *)malloc(sizeof(char)*len);
    if(s == NULL)
    {
        printf("Out of memory.\n");
        exit(1);
    }

    memset(s,'\0',sizeof(char)*len);
    return s;
}

static void *xmalloc_fatal(size_t size) {
    if (size==0) return NULL;
    fprintf(stderr, "Out of memory.");
    exit(1);
}

void *xmalloc (size_t size) {
    void *ptr = malloc (size);
    if (ptr == NULL) return xmalloc_fatal(size);
    return ptr;
}

void *xcalloc (size_t nmemb, size_t size) {
    void *ptr = calloc (nmemb, size);
    if (ptr == NULL) return xmalloc_fatal(nmemb*size);
    return ptr;
}

void *xrealloc (void *ptr, size_t size) {
    void *p = realloc (ptr, size);
    if (p == NULL) return xmalloc_fatal(size);
    return p;
}

char *xstrdup (const char *s) {
    void *ptr = xmalloc(strlen(s)+1);
    strcpy (ptr, s);
    return (char*) ptr;
}

/**
 * Base64 encode one byte
 */
char oauth_b64_encode(unsigned char u) {
    if(u < 26)  return 'A'+u;
    if(u < 52)  return 'a'+(u-26);
    if(u < 62)  return '0'+(u-52);
    if(u == 62) return '+';
    return '/';
}

/**
 * Decode a single base64 character.
 */
unsigned char oauth_b64_decode(char c) {
    if(c >= 'A' && c <= 'Z') return(c - 'A');
    if(c >= 'a' && c <= 'z') return(c - 'a' + 26);
    if(c >= '0' && c <= '9') return(c - '0' + 52);
    if(c == '+')             return 62;
    return 63;
}

/**
 * Return TRUE if 'c' is a valid base64 character, otherwise FALSE
 */
int oauth_b64_is_base64(char c) {
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
       (c >= '0' && c <= '9') || (c == '+')             ||
       (c == '/')             || (c == '=')) {
        return 1;
    }
    return 0;
}

/**
 * Base64 encode and return size data in 'src'. The caller must free the
 * returned string.
 *
 * @param size The size of the data in src
 * @param src The data to be base64 encode
 * @return encoded string otherwise NULL
 */
char *oauth_encode_base64(int size, const unsigned char *src) {
    int i;
    char *out, *p;

    if(!src) return NULL;
    if(!size) size= strlen((char *)src);
    out= (char*) xcalloc(sizeof(char), size*4/3+4);
    p= out;

    for(i=0; i<size; i+=3) {
        unsigned char b1=0, b2=0, b3=0, b4=0, b5=0, b6=0, b7=0;
        b1= src[i];
        if(i+1<size) b2= src[i+1];
        if(i+2<size) b3= src[i+2];

        b4= b1>>2;
        b5= ((b1&0x3)<<4)|(b2>>4);
        b6= ((b2&0xf)<<2)|(b3>>6);
        b7= b3&0x3f;

        *p++= oauth_b64_encode(b4);
        *p++= oauth_b64_encode(b5);

        if(i+1<size) *p++= oauth_b64_encode(b6);
        else *p++= '=';

        if(i+2<size) *p++= oauth_b64_encode(b7);
        else *p++= '=';
    }
    return out;
}

/**
 * Decode the base64 encoded string 'src' into the memory pointed to by
 * 'dest'.
 *
 * @param dest Pointer to memory for holding the decoded string.
 * Must be large enough to receive the decoded string.
 * @param src A base64 encoded string.
 * @return the length of the decoded string if decode
 * succeeded otherwise 0.
 */
int oauth_decode_base64(unsigned char *dest, const char *src) {
    if(src && *src) {
        unsigned char *p= dest;
        int k, l= strlen(src)+1;
        unsigned char *buf= (unsigned char*) xcalloc(sizeof(unsigned char), l);

        /* Ignore non base64 chars as per the POSIX standard */
        for(k=0, l=0; src[k]; k++) {
            if(oauth_b64_is_base64(src[k])) {
                buf[l++]= src[k];
            }
        }

        for(k=0; k<l; k+=4) {
            char c1='A', c2='A', c3='A', c4='A';
            unsigned char b1=0, b2=0, b3=0, b4=0;
            c1= buf[k];

            if(k+1<l) c2= buf[k+1];
            if(k+2<l) c3= buf[k+2];
            if(k+3<l) c4= buf[k+3];

            b1= oauth_b64_decode(c1);
            b2= oauth_b64_decode(c2);
            b3= oauth_b64_decode(c3);
            b4= oauth_b64_decode(c4);

            *p++=((b1<<2)|(b2>>4) );

            if(c3 != '=') *p++=(((b2&0xf)<<4)|(b3>>2) );
            if(c4 != '=') *p++=(((b3&0x3)<<6)|b4 );
        }
        free(buf);
        dest[p-dest]='\0';
        return(p-dest);
    }
    return 0;
}




/**
 * Escape 'string' according to RFC3986 and
 * http://oauth.net/core/1.0/#encoding_parameters.
 *
 * @param string The data to be encoded
 * @return encoded string otherwise NULL
 * The caller must free the returned string.
 */
char *oauth_url_escape(const char *string) {
    size_t alloc, newlen;
    char *ns = NULL, *testing_ptr = NULL;
    unsigned char in;
    size_t strindex=0;
    size_t length;

    if (!string) return xstrdup("");

    alloc = strlen(string)+1;
    newlen = alloc;

    ns = (char*) xmalloc(alloc);

    length = alloc-1;
    while(length--) {
        in = *string;

        switch(in){
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '_': case '~': case '.': case '-':
            ns[strindex++]=in;
            break;
        default:
            newlen += 2; /* this'll become a %XX */
            if(newlen > alloc) {
                alloc *= 2;
                testing_ptr = (char*) xrealloc(ns, alloc);
                ns = testing_ptr;
            }
            snprintf(&ns[strindex], 4, "%%%02X", in);
            strindex+=3;
            break;
        }
        string++;
    }
    ns[strindex]=0;
    return ns;
}

#ifndef ISXDIGIT
# define ISXDIGIT(x) (isxdigit((int) ((unsigned char)x)))
#endif

/**
 * Parse RFC3986 encoded 'string' back to  unescaped version.
 *
 * @param string The data to be unescaped
 * @param olen unless NULL the length of the returned string is stored there.
 * @return decoded string or NULL
 * The caller must free the returned string.
 */
char *oauth_url_unescape(const char *string, size_t *olen) {
    size_t alloc, strindex=0;
    char *ns = NULL;
    unsigned char in;
    long hex;

    if (!string) return NULL;
    alloc = strlen(string)+1;
    ns = (char*) xmalloc(alloc);

    while(--alloc > 0) {
        in = *string;
        if(('%' == in) && ISXDIGIT(string[1]) && ISXDIGIT(string[2])) {
            char hexstr[3]; // '%XX'
            hexstr[0] = string[1];
            hexstr[1] = string[2];
            hexstr[2] = 0;
            hex = strtol(hexstr, NULL, 16);
            in = (unsigned char)hex; /* hex is always < 256 */
            string+=2;
            alloc-=2;
        }
        ns[strindex++] = in;
        string++;
    }
    ns[strindex]=0;
    if(olen) *olen = strindex;
    return ns;
}

/*   HMAC-SHA1 */
char *oauth_sign_hmac_sha1_raw (const char *m, const size_t ml, const char *k, const size_t kl) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int resultlen = 0;

    HMAC(EVP_sha1(), k, kl,
         (unsigned char*) m, ml,
         result, &resultlen);

    return(oauth_encode_base64(resultlen, result));
}

char *oauth_sign_hmac_sha1 (const char *m, const char *k) {
    return(oauth_sign_hmac_sha1_raw (m, strlen(m), k, strlen(k)));
}

/*   PLAINTEXT */
char *oauth_sign_plaintext (const char *m, const char *k) {
    return(oauth_url_escape(k));
}

/*
//general nonce
char *oauth_gen_nonce() {
  char *nc;
  static int rndinit = 1;
  const char *chars = "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "0123456789_";
  unsigned int max = strlen( chars );
  int i, len;

  if(rndinit) {srand(time(NULL)
#ifndef WIN32 // quick windows check.
    * getpid()
#endif
  ); rndinit=0;} // seed random number generator - FIXME: we can do better ;)

  len=15+floor(rand()*16.0/(double)RAND_MAX);
  nc = (char*) xmalloc((len+1)*sizeof(char));
  for(i=0;i<len; i++) {
    nc[i] = chars[ rand() % max ];
  }
  nc[i]='\0';
  return (nc);
}
*/
/****** MD5 Code *******/
typedef unsigned int u32;
//typedef unsigned long u32;

/* original code from header - function names have changed */

struct MD5Context {
    u32 buf[4];
    u32 bits[2];
    unsigned char in[64];
    unsigned char digest[16];
};

static void MD5Init(struct MD5Context *context);
static void MD5Update(struct MD5Context *context,
                      const unsigned char *buf,
                      unsigned len);
static void MD5Final(struct MD5Context *context);
static void MD5Transform(u32 buf[4], u32 const in[16]);


/* original code from C file - GNU configurised */

#ifndef WORDS_BIGENDIAN
#define byteReverse(buf, len)	/* Nothing */
#else
static void byteReverse(unsigned char *buf, unsigned longs);

/* ASM ifdef removed */

/*
 * Note: this code is harmless on little-endian machines.
 */
static void byteReverse(unsigned char *buf, unsigned longs)
{
    u32 t;
    do {
        t = (u32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
            ((unsigned) buf[1] << 8 | buf[0]);
        *(u32 *) buf = t;
        buf += 4;
    } while (--longs);
}
#endif /* WORDS_BIGENDIAN */


/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void MD5Init(struct MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD5Update(struct MD5Context *ctx,
                      const unsigned char* buf,
                      unsigned len)
{
    u32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((u32) len << 3)) < t)
        ctx->bits[1]++;		/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */

    if (t) {
        unsigned char *p = (unsigned char *) ctx->in + t;

        t = 64 - t;
        if (len < t) {
            memcpy(p, buf, len);
            return;
        }
        memcpy(p, buf, t);
        byteReverse(ctx->in, 16);
        MD5Transform(ctx->buf, (u32 *) ctx->in);
        buf += t;
        len -= t;
    }

    /* Process data in 64-byte chunks */

    while (len >= 64) {
        memcpy(ctx->in, buf, 64);
        byteReverse(ctx->in, 16);
        MD5Transform(ctx->buf, (u32 *) ctx->in);
        buf += 64;
        len -= 64;
    }

    /* Handle any remaining bytes of data. */

    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */

/* Interface altered by DJB to write digest into pre-allocated context */
static void MD5Final(struct MD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
     always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
        /* Two lots of padding:  Pad the first block to 64 bytes */
        memset(p, 0, count);
        byteReverse(ctx->in, 16);
        MD5Transform(ctx->buf, (u32 *) ctx->in);

        /* Now fill the next block with 56 bytes */
        memset(ctx->in, 0, 56);
    } else {
        /* Pad block to 56 bytes */
        memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((u32 *) ctx->in)[14] = ctx->bits[0];
    ((u32 *) ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (u32 *) ctx->in);
    byteReverse((unsigned char *) ctx->buf, 4);
    memcpy(ctx->digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));	/* In case it's sensitive */
}


/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

        /*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
        static void MD5Transform(u32 buf[4], u32 const in[16])
{
    register u32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}
//
void my_mkdir(char *path)
{
    //char error_message[NORMALSIZE];
    DIR *pDir;
    pDir=opendir(path);
    if(NULL == pDir)
    {
        if(-1 == mkdir(path,0777))
        {
            //snprintf(error_message,NORMALSIZE,"mkdir %s fail",path);
            //handle_error(S_MKDIR_FAIL,error_message,__FILE__,__LINE__);
            return ;
        }
    }
    else
        closedir(pDir);
}
void my_mkdir_r(char *path)
{
    int i,len;
    char str[512];

    strncpy(str,path,512);
    len = strlen(str);
    for(i=0; i < len ; i++)
    {
        if(str[i] == '/' && i != 0)
        {
            str[i] = '\0';
            if(access(str,F_OK) != 0)
            {
                my_mkdir(str);
            }
            str[i] = '/';
        }
    }

    if(len > 0 && access(str,F_OK) != 0)
    {
        my_mkdir(str);
    }

}
int test_if_dir_empty(char *dir)
{
    struct dirent* ent = NULL;
    DIR *pDir;
    int i = 0;
    pDir=opendir(dir);

    if(pDir != NULL )
    {
        while (NULL != (ent=readdir(pDir)))
        {

            if(ent->d_name[0] == '.')
                continue;
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            i++;

        }
        closedir(pDir);
    }

    return  (i == 0) ? 1 : 0;
}
/*
 *find 'chr' in 'str' at the 'n' times,'n' need to >=1;
 *返回'chr'在'str'中第'n'次出现的位置
*/
char *my_nstrchr(const char chr,char *str,int n){

    if(n<1)
    {
#ifdef DEBUG
        printf("my_nstrchr need n>=1\n");
#endif
        return NULL;
    }

    char *p1,*p2;
    int i = 1;
    p1 = str;

    do{
        p2 = strchr(p1,chr);
        p1 = p2;
        p1++;
        i++;
    }while(p2!=NULL && i<=n);

    if(i<n)
    {
        return NULL;
    }

    return p2;
}
/*
 大小写转换函数
 big to low
*/
char *strlwr(char *s)
{
    char *str;
    str = s;
    while(*str != '\0')
    {
        if(*str >= 'A' && *str <= 'Z'){
            *str += 'a'-'A';
        }
        str++;
    }
    return s;
}
int is_local_exist(char *folder,char *filename)
{
    DIR *pDir;
    struct dirent *ent = NULL;
    pDir = opendir(folder);

    if(NULL == pDir)
    {
        return 0;
    }
    while(NULL != (ent = readdir(pDir)))
    {
        if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
            continue;

        if(!strcmp(strlwr(ent->d_name),strlwr(filename)) && strcmp(ent->d_name,filename))
        {
            return 1;
        }

        closedir(pDir);
    }
    return 0;
}
char *get_case_conflict_localfilename(char *localfolder,char *filename,char *serfolder,int index)
{
    DIR *pDir;
    struct dirent *ent = NULL;
    pDir = opendir(localfolder);
    char *suffix = "(big-low-conflict)";
    int exist = 0;
    if(NULL == pDir)
    {
        return NULL;
    }
    char *tmp_name = NULL;
    while(NULL != (ent = readdir(pDir)))
    {
        if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
            continue;

        if(!strcmp(strlwr(ent->d_name),strlwr(filename)) && strcmp(ent->d_name,filename))
        {
            char *serfullname = malloc(strlen(serfolder)+strlen(ent->d_name)+1+1);
            memset(serfullname,0,strlen(serfolder)+strlen(ent->d_name)+1+1);
            sprintf(serfullname,"%s/%s",serfolder,ent->d_name);
            exist=is_server_exist(serfolder,serfullname,index);
            my_free(serfullname);
            if(exist)
            {
                tmp_name = malloc(strlen(ent->d_name)+strlen(suffix)+1);
                memset(tmp_name,0,strlen(ent->d_name)+strlen(suffix)+1);
                sprintf(tmp_name,"%s",ent->d_name,suffix);
                break;
            }
        }
    }
    closedir(pDir);

    return tmp_name;
}

char *check_if_exist_big_low_conflict()
{
    int exist = 0;
    //exist = is_local_exist();
}
/*

*/
int is_number(char *str)
{
    if(str == NULL)
        return -1;

    if(!strcmp(str,"0"))
        return 0;

    int i;
    int len = strlen(str);
    for(i=0;i<len;i++)
    {
        if(!isdigit(str[i]))
            return 0;
    }

    if(i == len && i > 0)
        return 1;

    return 0;
}
char *get_prefix_name(const char *fullname,int isfolder)
{
    char *parse_name = NULL;
    char *p = NULL;

    char *filename = NULL;

    filename = parse_name_from_path(fullname);

    p = strrchr(filename,'.');

    if(p && filename[0] != '.' && !isfolder)
    {
        parse_name = my_str_malloc(strlen(filename)-strlen(p)+1);
        strncpy(parse_name,filename,strlen(filename)-strlen(p));
    }
    else
    {
        parse_name = my_str_malloc(strlen(filename)+1);
        strcpy(parse_name,filename);
    }
    my_free(filename);
    return parse_name;

}
char *get_confilicted_name_first(const char *fullname,int isfolder)
{
    char *confilicted_name = NULL;
    char prefix_name[NORMALSIZE];
    char suffix_name[256];
    char parse_name[NORMALSIZE];
    char *p = NULL;
    char *p1 = NULL;
    //char *p2 = NULL;
    char seq[8];
    int  num = 0;
    //int have_suf = 0;
    char *filename = NULL;
    char *path;
    int n = 0,j=0;
    //char seq_num[8];
    char con_filename[256];

    memset(prefix_name,0,sizeof(prefix_name));
    memset(suffix_name,0,sizeof(suffix_name));
    memset(parse_name,0,sizeof(parse_name));
    memset(con_filename,0,sizeof(con_filename));

    filename = parse_name_from_path(fullname);

    path = my_str_malloc(strlen(fullname)-strlen(filename)+1);
    strncpy(path,fullname,strlen(fullname)-strlen(filename)-1);

    confilicted_name = (char *)malloc(sizeof(char)*NORMALSIZE);


    if(isfolder)
    {
        strcpy(parse_name,filename);
    }
    else
    {
        p = strrchr(filename,'.');

        //printf("p=%s\n",p);

        if(p && filename[0] != '.')
        {
            strncpy(parse_name,filename,strlen(filename)-strlen(p));
            strcpy(suffix_name,p);
        }
        else
        {
            strcpy(parse_name,filename);
        }
    }

    //printf("parse_name=%s,suffix_name=%s\n",parse_name,suffix_name);

    if(num == 0)
    {
        strcpy(prefix_name,parse_name);
    }

    snprintf(prefix_name,252-strlen(case_conflict_name)-strlen(suffix_name),"%s",prefix_name);
    snprintf(con_filename,256,"%s(%s)%s",prefix_name,case_conflict_name,suffix_name);
    snprintf(confilicted_name,NORMALSIZE,"%s/%s",path,con_filename);

    //printf("------ prefix name is %s,num is %d,suffix name is %s -----\n",prefix_name,num,suffix_name);

    my_free(filename);
    my_free(path);

    return confilicted_name;
}
char *get_confilicted_name(const char *fullname,int isfolder)
{
    char *confilicted_name = NULL;
    char prefix_name[NORMALSIZE];
    char suffix_name[256];
    char parse_name[NORMALSIZE];
    char *p = NULL;
    char *p1 = NULL;
    //char *p2 = NULL;
    char seq[8];
    int  num = 0;
    //int have_suf = 0;
    char *filename = NULL;
    char path[512];
    int n = 0,j=0;
    //char seq_num[8];
    char con_filename[256];

    memset(prefix_name,0,sizeof(prefix_name));
    memset(suffix_name,0,sizeof(suffix_name));
    memset(parse_name,0,sizeof(parse_name));
    memset(path,0,sizeof(path));
    memset(con_filename,0,sizeof(con_filename));

    filename = parse_name_from_path(fullname);

    strncpy(path,fullname,strlen(fullname)-strlen(filename)-1);

    confilicted_name = (char *)malloc(sizeof(char)*NORMALSIZE);


    if(isfolder)
    {
        strcpy(parse_name,filename);
    }
    else
    {
        p = strrchr(filename,'.');

        //printf("p=%s\n",p);

        if(p && filename[0] != '.')
        {
            strncpy(parse_name,filename,strlen(filename)-strlen(p));
            strcpy(suffix_name,p);
            //have_suf = 1;
        }
        else
        {
            strcpy(parse_name,filename);
        }
    }

    p = NULL;

    p = strrchr(parse_name,'(');

    if(p)
    {
        p1 = strchr(p,')');
        if(p1)
        {
            p++;
            memset(seq,0,sizeof(seq));
            strncpy(seq,p,strlen(p)-strlen(p1));
            if(is_number(seq))
            {
                num = atoi(seq);
                num++;
                //printf("seq is %s,num is %d\n",seq,num);
                n = num;
                while((n=(n/10)))
                {
                    j++;
                }


                strncpy(prefix_name,parse_name,strlen(parse_name)-strlen(p)-1);
            }
        }
    }

    //printf("parse_name=%s,suffix_name=%s\n",parse_name,suffix_name);

    if(num == 0)
    {
        num = 1;
        strcpy(prefix_name,parse_name);
    }

    snprintf(prefix_name,252-j-strlen(suffix_name),"%s",prefix_name);
    snprintf(con_filename,256,"%s(%d)%s",prefix_name,num,suffix_name);
    snprintf(confilicted_name,NORMALSIZE,"%s/%s",path,con_filename);

    //printf("------ prefix name is %s,num is %d,suffix name is %s -----\n",prefix_name,num,suffix_name);

    my_free(filename);

    return confilicted_name;
}

char *get_confilicted_name_case(const char *fullname,int is_folder)
{
    char *g_newname = NULL;
    char *tmp_name = malloc(strlen(fullname)+1);
    memset(tmp_name,0,strlen(fullname)+1);
    sprintf(tmp_name,"%s",fullname);

    char *prefix_name = get_prefix_name(tmp_name,is_folder);//a.txt --> a
    //printf("%s\n",prefix_name);
    g_newname = get_confilicted_name_first(tmp_name,is_folder);//a.txt-->a(case-conflict).txt
    //printf("%s\n",g_newname);
    if(access(g_newname,F_OK) == 0)//a(case-conflict).txt-->a(case-conflict(n)).txt
    {
        //printf("111\n");
        my_free(tmp_name);
        tmp_name = malloc(strlen(g_newname)+1);
        memset(tmp_name,0,strlen(g_newname)+1);
        sprintf(tmp_name,"%s",g_newname);
        my_free(g_newname);
        while(!exit_loop)
        {
            g_newname = get_confilicted_name_second(tmp_name,is_folder,prefix_name);
            //printf("confilicted_name=%s\n",confilicted_name);
            if(access(g_newname,F_OK) == 0)
            {
                my_free(tmp_name);
                tmp_name = malloc(strlen(g_newname)+1);
                memset(tmp_name,0,strlen(g_newname)+1);
                sprintf(tmp_name,"%s",g_newname);
                my_free(g_newname);
                //have_same = 1;
            }
            else
                break;
        }
        my_free(tmp_name);
    }else
    {
        //printf("222\n");
        my_free(tmp_name);
    }
    my_free(prefix_name);
    return g_newname;
}

char *get_confilicted_name_second(const char *fullname,int isfolder,char *prefix_name)
{
    char *confilicted_name = NULL;
    char suffix_name[256];
    char parse_name[NORMALSIZE];
    char *p = NULL;
    char *p1 = NULL;
    //char *p2 = NULL;
    char seq[8];
    int  num = 0;
    //int have_suf = 0;
    char *filename = NULL;
    char *path;
    int n = 0,j=0;
    //char seq_num[8];
    char con_filename[256];
    char cmp_name[128] = {0};

    memset(suffix_name,0,sizeof(suffix_name));
    memset(parse_name,0,sizeof(parse_name));
    memset(con_filename,0,sizeof(con_filename));

    filename = parse_name_from_path(fullname);

    path = my_str_malloc(strlen(fullname)-strlen(filename)+1);
    strncpy(path,fullname,strlen(fullname)-strlen(filename)-1);

    //printf("%s\n",path);
    confilicted_name = (char *)malloc(sizeof(char)*NORMALSIZE);


    if(isfolder)
    {
        strcpy(parse_name,filename);
    }
    else
    {
        p = strrchr(filename,'.');

        //printf("p=%s\n",p);

        if(p && filename[0] != '.')
        {
            strncpy(parse_name,filename,strlen(filename)-strlen(p));
            strcpy(suffix_name,p);
            //have_suf = 1;
        }
        else
        {
            strcpy(parse_name,filename);
        }
    }

    p = NULL;

    p = strrchr(parse_name,'(');

    if(p)
    {
        p1 = strchr(p,')');
        if(p1)
        {
            p++;
            memset(seq,0,sizeof(seq));
            strncpy(seq,p,strlen(p)-strlen(p1));
            if(is_number(seq))
            {
                num = atoi(seq);
                num++;
                //printf("seq is %s,num is %d\n",seq,num);
                n = num;
                while((n=(n/10)))
                {
                    j++;
                }
            }
        }
    }

    //printf("parse_name=%s,suffix_name=%s\n",parse_name,suffix_name);

    if(num == 0)
    {
        sprintf(cmp_name,"(%s(1))",case_conflict_name);

    }
    else
    {
        sprintf(cmp_name,"(%s(%d))",case_conflict_name,num);
    }
    //printf("%s\n",cmp_name);
    //printf("%s\n",prefix_name);
    snprintf(prefix_name,252-strlen(cmp_name)-strlen(suffix_name),"%s",prefix_name);
    //printf("%s\n",prefix_name);
    snprintf(con_filename,256,"%s%s%s",prefix_name,cmp_name,suffix_name);
    //printf("%s\n",con_filename);
    snprintf(confilicted_name,NORMALSIZE,"%s/%s",path,con_filename);
    //printf("%s\n",confilicted_name);

    //printf("------ prefix name is %s,num is %d,suffix name is %s -----\n",prefix_name,num,suffix_name);

    my_free(filename);
    my_free(path);

    return confilicted_name;
}
