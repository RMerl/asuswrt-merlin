#ifndef COMMON_H
#define COMMON_H

#include <endian.h>
#include <byteswap.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) bswap_16(n)
#define host_to_be16(n) bswap_16(n)
#else
#define le_to_host16(n) bswap_16(n)
#define host_to_le16(n) bswap_16(n)
#define be_to_host16(n) (n)
#define host_to_be16(n) (n)
#endif


#include <stdint.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

void hostapd_hexdump(const char *title, u8 *buf, size_t len);
int hwaddr_aton(char *txt, u8 *addr);

static inline void print_char(char c)
{
	if (c >= 32 && c < 127)
		printf("%c", c);
	else
		printf("<%02x>", c);
}

static inline void fprint_char(FILE *f, char c)
{
	if (c >= 32 && c < 127)
		fprintf(f, "%c", c);
	else
		fprintf(f, "<%02x>", c);
}

#endif /* COMMON_H */
