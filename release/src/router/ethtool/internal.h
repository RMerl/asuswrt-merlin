/* Portions Copyright 2001 Sun Microsystems (thockin@sun.com) */
/* Portions Copyright 2002 Intel (scott.feldman@intel.com) */
#ifndef ETHTOOL_INTERNAL_H__
#define ETHTOOL_INTERNAL_H__

/* Some platforms (eg. ppc64) need __SANE_USERSPACE_TYPES__ before
 * <linux/types.h> to select 'int-ll64.h' and avoid compile warnings
 * when printing __u64 with %llu.
 */
#define __SANE_USERSPACE_TYPES__

#ifdef HAVE_CONFIG_H
#include "ethtool-config.h"
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <endian.h>
#include <sys/ioctl.h>
#include <net/if.h>

/* ethtool.h expects these to be defined by <linux/types.h> */
#ifndef HAVE_BE_TYPES
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef unsigned long long __be64;
#endif

typedef unsigned long long u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t s32;

/* ethtool.h epxects __KERNEL_DIV_ROUND_UP to be defined by <linux/kernel.h> */
#include <linux/kernel.h>
#ifndef __KERNEL_DIV_ROUND_UP
#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#include "ethtool-copy.h"
#include "net_tstamp-copy.h"

#if __BYTE_ORDER == __BIG_ENDIAN
static inline u16 cpu_to_be16(u16 value)
{
	return value;
}
static inline u32 cpu_to_be32(u32 value)
{
	return value;
}
static inline u64 cpu_to_be64(u64 value)
{
	return value;
}
#else
static inline u16 cpu_to_be16(u16 value)
{
	return (value >> 8) | (value << 8);
}
static inline u32 cpu_to_be32(u32 value)
{
	return cpu_to_be16(value >> 16) | (cpu_to_be16(value) << 16);
}
static inline u64 cpu_to_be64(u64 value)
{
	return cpu_to_be32(value >> 32) | ((u64)cpu_to_be32(value) << 32);
}
#endif

#define ntohll cpu_to_be64
#define htonll cpu_to_be64

#define BITS_PER_BYTE		8
#define BITS_PER_LONG		(BITS_PER_BYTE * sizeof(long))
#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_LONG)

static inline void set_bit(unsigned int nr, unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] |= 1UL << (nr % BITS_PER_LONG);
}

static inline void clear_bit(unsigned int nr, unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] &= ~(1UL << (nr % BITS_PER_LONG));
}

static inline int test_bit(unsigned int nr, const unsigned long *addr)
{
	return !!((1UL << (nr % BITS_PER_LONG)) &
		  (((unsigned long *)addr)[nr / BITS_PER_LONG]));
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef SIOCETHTOOL
#define SIOCETHTOOL     0x8946
#endif

/* Internal values for old-style offload flags.  Values and names
 * must not clash with the flags defined for ETHTOOL_{G,S}FLAGS.
 */
#define ETH_FLAG_RXCSUM		(1 << 0)
#define ETH_FLAG_TXCSUM		(1 << 1)
#define ETH_FLAG_SG		(1 << 2)
#define ETH_FLAG_TSO		(1 << 3)
#define ETH_FLAG_UFO		(1 << 4)
#define ETH_FLAG_GSO		(1 << 5)
#define ETH_FLAG_GRO		(1 << 6)
#define ETH_FLAG_INT_MASK	(ETH_FLAG_RXCSUM | ETH_FLAG_TXCSUM |	\
				 ETH_FLAG_SG | ETH_FLAG_TSO | ETH_FLAG_UFO | \
				 ETH_FLAG_GSO | ETH_FLAG_GRO),
/* Mask of all flags defined for ETHTOOL_{G,S}FLAGS. */
#define ETH_FLAG_EXT_MASK	(ETH_FLAG_LRO | ETH_FLAG_RXVLAN |	\
				 ETH_FLAG_TXVLAN | ETH_FLAG_NTUPLE |	\
				 ETH_FLAG_RXHASH)

/* internal API for link mode bitmap interaction with kernel. */

#define ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32		\
	(SCHAR_MAX)
#define ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBITS		\
	(32 * ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32)
#define ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBYTES	\
	(4 * ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32)
#define ETHTOOL_DECLARE_LINK_MODE_MASK(name)		\
	u32 name[ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32]

struct ethtool_link_usettings {
	struct {
		__u8 transceiver;
	} deprecated;
	struct ethtool_link_settings base;
	struct {
		ETHTOOL_DECLARE_LINK_MODE_MASK(supported);
		ETHTOOL_DECLARE_LINK_MODE_MASK(advertising);
		ETHTOOL_DECLARE_LINK_MODE_MASK(lp_advertising);
	} link_modes;
};

#define ethtool_link_mode_for_each_u32(index)			\
	for ((index) = 0;					\
	     (index) < ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NU32;	\
	     ++(index))

static inline void ethtool_link_mode_zero(u32 *dst)
{
	memset(dst, 0, ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBYTES);
}

static inline bool ethtool_link_mode_is_empty(const u32 *mask)
{
	unsigned int i;

	ethtool_link_mode_for_each_u32(i) {
		if (mask[i] != 0)
			return false;
	}

	return true;
}

static inline void ethtool_link_mode_copy(u32 *dst, const u32 *src)
{
	memcpy(dst, src, ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBYTES);
}

static inline int ethtool_link_mode_test_bit(unsigned int nr, const u32 *mask)
{
	if (nr >= ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBITS)
		return !!0;
	return !!(mask[nr / 32] & (1 << (nr % 32)));
}

static inline int ethtool_link_mode_set_bit(unsigned int nr, u32 *mask)
{
	if (nr >= ETHTOOL_LINK_MODE_MASK_MAX_KERNEL_NBITS)
		return -1;
	mask[nr / 32] |= (1 << (nr % 32));
	return 0;
}

/* Context for sub-commands */
struct cmd_context {
	const char *devname;	/* net device name */
	int fd;			/* socket suitable for ethtool ioctl */
	struct ifreq ifr;	/* ifreq suitable for ethtool ioctl */
	int argc;		/* number of arguments to the sub-command */
	char **argp;		/* arguments to the sub-command */
};

#ifdef TEST_ETHTOOL
int test_cmdline(const char *args);

struct cmd_expect {
	const void *cmd;	/* expected command; NULL at end of list */
	size_t cmd_len;		/* length to match (might be < sizeof struct) */
	int rc;			/* kernel return code */
	const void *resp;	/* response to write back; may be NULL */
	size_t resp_len;	/* length to write back */
};
int test_ioctl(const struct cmd_expect *expect, void *cmd);
#define TEST_IOCTL_MISMATCH (-2)

int test_main(int argc, char **argp);
void test_exit(int rc) __attribute__((noreturn));

#ifndef TEST_NO_WRAPPERS
#define main(...) test_main(__VA_ARGS__)
#undef exit
#define exit(rc) test_exit(rc)
void *test_malloc(size_t size);
#undef malloc
#define malloc(size) test_malloc(size)
void *test_calloc(size_t nmemb, size_t size);
#undef calloc
#define calloc(nmemb, size) test_calloc(nmemb, size)
char *test_strdup(const char *s);
#undef strdup
#define strdup(s) test_strdup(s)
void test_free(void *ptr);
#undef free
#define free(ptr) test_free(ptr)
void *test_realloc(void *ptr, size_t size);
#undef realloc
#define realloc(ptr, size) test_realloc(ptr, size)
int test_open(const char *pathname, int flag, ...);
#undef open
#define open(...) test_open(__VA_ARGS__)
int test_socket(int domain, int type, int protocol);
#undef socket
#define socket(...) test_socket(__VA_ARGS__)
int test_close(int fd);
#undef close
#define close(fd) test_close(fd)
FILE *test_fopen(const char *path, const char *mode);
#undef fopen
#define fopen(path, mode) test_fopen(path, mode)
int test_fclose(FILE *fh);
#undef fclose
#define fclose(fh) test_fclose(fh)
#endif
#endif

int send_ioctl(struct cmd_context *ctx, void *cmd);

void dump_hex(FILE *f, const u8 *data, int len, int offset);

/* National Semiconductor DP83815, DP83816 */
int natsemi_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);
int natsemi_dump_eeprom(struct ethtool_drvinfo *info,
	struct ethtool_eeprom *ee);

/* Digital/Intel 21040 and 21041 */
int de2104x_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Intel(R) PRO/1000 Gigabit Adapter Family */
int e1000_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

int igb_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* RealTek PCI */
int realtek_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Intel(R) PRO/100 Fast Ethernet Adapter Family */
int e100_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Tigon3 */
int tg3_dump_eeprom(struct ethtool_drvinfo *info, struct ethtool_eeprom *ee);

/* Advanced Micro Devices  AMD8111 based Adapter */
int amd8111e_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Advanced Micro Devices PCnet32 Adapter */
int pcnet32_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Motorola 8xx FEC Ethernet controller */
int fec_8xx_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* PowerPC 4xx on-chip Ethernet controller */
int ibm_emac_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Intel(R) PRO/10GBe Gigabit Adapter Family */
int ixgb_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

int ixgbe_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

int ixgbevf_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Broadcom Tigon3 Ethernet controller */
int tg3_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* SysKonnect Gigabit (Genesis and Yukon) */
int skge_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* SysKonnect Gigabit (Yukon2) */
int sky2_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Fabric7 VIOC */
int vioc_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* SMSC LAN911x/LAN921x embedded ethernet controller */
int smsc911x_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

int at76c50x_usb_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Solarflare Solarstorm controllers */
int sfc_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* STMMAC embedded ethernet controller */
int st_mac100_dump_regs(struct ethtool_drvinfo *info,
			struct ethtool_regs *regs);
int st_gmac_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Et131x ethernet controller */
int et131x_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Altera TSE 10/100/1000 ethernet controller */
int altera_tse_dump_regs(struct ethtool_drvinfo *info,
			 struct ethtool_regs *regs);

/* VMware vmxnet3 ethernet controller */
int vmxnet3_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs);

/* Rx flow classification */
int rxclass_parse_ruleopts(struct cmd_context *ctx,
			   struct ethtool_rx_flow_spec *fsp);
int rxclass_rule_getall(struct cmd_context *ctx);
int rxclass_rule_get(struct cmd_context *ctx, __u32 loc);
int rxclass_rule_ins(struct cmd_context *ctx,
		     struct ethtool_rx_flow_spec *fsp);
int rxclass_rule_del(struct cmd_context *ctx, __u32 loc);

/* Module EEPROM parsing code */
void sff8079_show_all(const __u8 *id);

/* Optics diagnostics */
void sff8472_show_all(const __u8 *id);

/* QSFP Optics diagnostics */
void sff8636_show_all(const __u8 *id, __u32 eeprom_len);

#endif /* ETHTOOL_INTERNAL_H__ */
