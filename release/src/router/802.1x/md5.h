#ifndef	__MD5_H__
#define	__MD5_H__

#ifndef ULONG
#define	INT		int
#define	UINT		u32
#define ULONG		u32
#define USHORT		u16
#define UCHAR		u8

#define BOOLEAN		u8
#define LONGLONG	s64
#define ULONGLONG	u64
#define VOID		void
#define	LONG		int
#endif	// ULONG

#define	NdisZeroMemory(buff, buff_size)		\
{						\
	memset(buff, 0, buff_size);		\
}

#define	NdisMoveMemory(buff, key, buff_size)	\
{						\
	memcpy(buff, key, buff_size);		\
}

#ifdef ASSERT
#undef ASSERT
#endif

#define	ASSERT(x)                                                                       \
{                                                                                       \
    if (!(x))                                                                           \
    {                                                                                   \
        printk(KERN_WARNING __FILE__ ":%d assert " #x "failed\n", __LINE__);            \
    }                                                                                   \
}

#define MD5_MAC_LEN 16

typedef struct _MD5_CTX {
	ULONG   Buf[4];             // buffers of four states
	ULONG   LenInBitCount[2];   // length counter for input message, 0 up to 64 bits
	UCHAR   Input[64];          // input message
}   MD5_CTX;

int	hostapd_get_rand(u8 *buf, size_t len);
void	MD5Init(MD5_CTX *pCtx);
void	MD5Update(MD5_CTX *pCtx, UCHAR *pData, ULONG LenInBytes);

void	MD5Final(UCHAR Digest[16], MD5_CTX *pCtx);
void	MD5Transform(ULONG Buf[4], ULONG Mes[16]);

void	md5_mac(UCHAR *key, ULONG key_len, UCHAR *data, ULONG data_len, UCHAR *mac);
void	hmac_md5(UCHAR *key, ULONG key_len, UCHAR *data, ULONG data_len, UCHAR *mac);

#endif // __MD5_H__

#ifndef RC4_H
#define RC4_H

void rc4(u8 *buf, size_t len, u8 *key, size_t key_len);

#endif /* RC4_H */
