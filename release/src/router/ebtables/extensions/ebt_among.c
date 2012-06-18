/* ebt_among
 *
 * Authors:
 * Grzegorz Borowiak <grzes@gnu.univ.gda.pl>
 *
 * August, 2003
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <unistd.h>
#include <netinet/ether.h>
#include "../include/ebtables_u.h"
#include "../include/ethernetdb.h"
#include <linux/if_ether.h>
#include <linux/netfilter_bridge/ebt_among.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define AMONG_DST '1'
#define AMONG_SRC '2'
#define AMONG_DST_F '3'
#define AMONG_SRC_F '4'

static struct option opts[] = {
	{"among-dst", required_argument, 0, AMONG_DST},
	{"among-src", required_argument, 0, AMONG_SRC},
	{"among-dst-file", required_argument, 0, AMONG_DST_F},
	{"among-src-file", required_argument, 0, AMONG_SRC_F},
	{0}
};

#ifdef DEBUG
static void hexdump(const void *mem, int howmany)
{
	printf("\n");
	const unsigned char *p = mem;
	int i;

	for (i = 0; i < howmany; i++) {
		if (i % 32 == 0) {
			printf("\n%04x: ", i);
		}
		printf("%2.2x%c", p[i], ". "[i % 4 == 3]);
	}
	printf("\n");
}
#endif				/* DEBUG */

static void print_help()
{
	printf(
"`among' options:\n"
"--among-dst      [!] list      : matches if ether dst is in list\n"
"--among-src      [!] list      : matches if ether src is in list\n"
"--among-dst-file [!] file      : obtain dst list from file\n"
"--among-src-file [!] file      : obtain src list from file\n"
"list has form:\n"
" xx:xx:xx:xx:xx:xx[=ip.ip.ip.ip],yy:yy:yy:yy:yy:yy[=ip.ip.ip.ip]"
",...,zz:zz:zz:zz:zz:zz[=ip.ip.ip.ip][,]\n"
"Things in brackets are optional.\n"
"If you want to allow two (or more) IP addresses to one MAC address, you\n"
"can specify two (or more) pairs with the same MAC, e.g.\n"
" 00:00:00:fa:eb:fe=153.19.120.250,00:00:00:fa:eb:fe=192.168.0.1\n"
	);
}
static int old_size;

static void init(struct ebt_entry_match *match)
{
	struct ebt_among_info *amonginfo =
	    (struct ebt_among_info *) match->data;

	memset(amonginfo, 0, sizeof(struct ebt_among_info));
	old_size = sizeof(struct ebt_among_info);
}

static struct ebt_mac_wormhash *new_wormhash(int n)
{
	int size =
	    sizeof(struct ebt_mac_wormhash) +
	    n * sizeof(struct ebt_mac_wormhash_tuple);
	struct ebt_mac_wormhash *result =
	    (struct ebt_mac_wormhash *) malloc(size);

	if (!result)
		ebt_print_memory();
	memset(result, 0, size);
	result->poolsize = n;
	return result;
}

static void copy_wormhash(struct ebt_mac_wormhash *d,
			  const struct ebt_mac_wormhash *s)
{
	int dpoolsize = d->poolsize;
	int dsize, ssize, amount;

	dsize = ebt_mac_wormhash_size(d);
	ssize = ebt_mac_wormhash_size(s);
	amount = dsize < ssize ? dsize : ssize;
	memcpy(d, s, amount);
	d->poolsize = dpoolsize;
}

/* Returns:
 * -1 when '\0' reached
 * -2 when `n' bytes read and no delimiter found
 *  0 when no less than `n' bytes read and delimiter found
 * if `destbuf' is not NULL, it is filled by read bytes and ended with '\0'
 * *pp is set on the first byte not copied to `destbuf'
 */
static int read_until(const char **pp, const char *delimiters,
		      char *destbuf, int n)
{
	int count = 0;
	int ret = 0;
	char c;

	while (1) {
		c = **pp;
		if (!c) {
			ret = -1;
			break;
		}
		if (strchr(delimiters, c)) {
			ret = 0;
			break;
		}
		if (count == n) {
			ret = -2;
			break;
		}
		if (destbuf)
			destbuf[count++] = c;
		(*pp)++;
	}
	if (destbuf)
		destbuf[count] = 0;
	return ret;
}

static int fcmp(const void *va, const void *vb) {
	const struct ebt_mac_wormhash_tuple *a = va;
	const struct ebt_mac_wormhash_tuple *b = vb;
	int ca = ((const unsigned char*)a->cmp)[7];
	int cb = ((const unsigned char*)b->cmp)[7];

	return ca - cb;
}

static void index_table(struct ebt_mac_wormhash *wh)
{
	int ipool, itable;
	int c;

	for (itable = 0; itable <= 256; itable++) {
		wh->table[itable] = wh->poolsize;
	}
	ipool = 0;
	itable = 0;
	while (1) {
		wh->table[itable] = ipool;
		c = ((const unsigned char*)wh->pool[ipool].cmp)[7];
		if (itable <= c) {
			itable++;
		} else {
			ipool++;
		}
		if (ipool > wh->poolsize)
			break;
	}
}

static struct ebt_mac_wormhash *create_wormhash(const char *arg)
{
	const char *pc = arg;
	const char *anchor;
	char *endptr;
	struct ebt_mac_wormhash *workcopy, *result, *h;
	unsigned char mac[6];
	unsigned char ip[4];
	int nmacs = 0;
	int i;
	char token[4];

	if (!(workcopy = new_wormhash(1024))) {
		ebt_print_memory();
	}
	while (1) {
		/* remember current position, we'll need it on error */
		anchor = pc;

		/* collect MAC; all its bytes are followed by ':' (colon),
		 * except for the last one which can be followed by 
		 * ',' (comma), '=' or '\0' */
		for (i = 0; i < 5; i++) {
			if (read_until(&pc, ":", token, 2) < 0
			    || token[0] == 0) {
				ebt_print_error("MAC parse error: %.20s", anchor);
				return NULL;
			}
			mac[i] = strtol(token, &endptr, 16);
			if (*endptr) {
				ebt_print_error("MAC parse error: %.20s", anchor);
				return NULL;
			}
			pc++;
		}
		if (read_until(&pc, "=,", token, 2) == -2 || token[0] == 0) {
			ebt_print_error("MAC parse error: %.20s", anchor);
			return NULL;
		}
		mac[i] = strtol(token, &endptr, 16);
		if (*endptr) {
			ebt_print_error("MAC parse error: %.20s", anchor);
			return NULL;
		}
		if (*pc == '=') {
			/* an IP follows the MAC; collect similarly to MAC */
			pc++;
			anchor = pc;
			for (i = 0; i < 3; i++) {
				if (read_until(&pc, ".", token, 3) < 0 || token[0] == 0) {
					ebt_print_error("IP parse error: %.20s", anchor);
					return NULL;
				}
				ip[i] = strtol(token, &endptr, 10);
				if (*endptr) {
					ebt_print_error("IP parse error: %.20s", anchor);
					return NULL;
				}
				pc++;
			}
			if (read_until(&pc, ",", token, 3) == -2 || token[0] == 0) {
				ebt_print_error("IP parse error: %.20s", anchor);
				return NULL;
			}
			ip[3] = strtol(token, &endptr, 10);
			if (*endptr) {
				ebt_print_error("IP parse error: %.20s", anchor);
				return NULL;
			}
			if (*(uint32_t*)ip == 0) {
				ebt_print_error("Illegal IP 0.0.0.0");
				return NULL;
			}
		} else {
			/* no IP, we set it to 0.0.0.0 */
			memset(ip, 0, 4);
		}

		/* we have collected MAC and IP, so we add an entry */
		memcpy(((char *) workcopy->pool[nmacs].cmp) + 2, mac, 6);
		workcopy->pool[nmacs].ip = *(const uint32_t *) ip;
		nmacs++;

		/* re-allocate memory if needed */
		if (*pc && nmacs >= workcopy->poolsize) {
			if (!(h = new_wormhash(nmacs * 2))) {
				ebt_print_memory();
			}
			copy_wormhash(h, workcopy);
			free(workcopy);
			workcopy = h;
		}

		/* check if end of string was reached */
		if (!*pc) {
			break;
		}

		/* now `pc' points to comma if we are here; */
		/* increment this to the next char */
		/* but first assert :-> */
		if (*pc != ',') {
			ebt_print_error("Something went wrong; no comma...\n");
			return NULL;
		}
		pc++;

		/* again check if end of string was reached; */
		/* we allow an ending comma */
		if (!*pc) {
			break;
		}
	}
	if (!(result = new_wormhash(nmacs))) {
		ebt_print_memory();
	}
	copy_wormhash(result, workcopy);
	free(workcopy);
	qsort(&result->pool, result->poolsize,
			sizeof(struct ebt_mac_wormhash_tuple), fcmp);
	index_table(result);
	return result;
}

#define OPT_DST 0x01
#define OPT_SRC 0x02
static int parse(int c, char **argv, int argc,
		 const struct ebt_u_entry *entry, unsigned int *flags,
		 struct ebt_entry_match **match)
{
	struct ebt_among_info *info =
	    (struct ebt_among_info *) (*match)->data;
	struct ebt_mac_wormhash *wh;
	struct ebt_entry_match *h;
	int new_size;
	long flen;
	int fd;

	switch (c) {
	case AMONG_DST_F:
	case AMONG_SRC_F:
	case AMONG_DST:
	case AMONG_SRC:
		if (c == AMONG_DST || c == AMONG_DST_F) {
			ebt_check_option2(flags, OPT_DST);
		} else {
			ebt_check_option2(flags, OPT_SRC);
		}
		if (ebt_check_inverse2(optarg)) {
			if (c == AMONG_DST || c == AMONG_DST_F)
				info->bitmask |= EBT_AMONG_DST_NEG;
			else
				info->bitmask |= EBT_AMONG_SRC_NEG;
		}
		if (c == AMONG_DST_F || c == AMONG_SRC_F) {
			struct stat stats;

			if ((fd = open(optarg, O_RDONLY)) == -1)
				ebt_print_error("Couldn't open file '%s'", optarg);
			fstat(fd, &stats);
			flen = stats.st_size;
			/* use mmap because the file will probably be big */
			optarg = mmap(0, flen, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
			if (optarg == MAP_FAILED)
				ebt_print_error("Couldn't map file to memory");
			if (optarg[flen-1] != '\n')
				ebt_print_error("File should end with a newline");
			if (strchr(optarg, '\n') != optarg+flen-1)
				ebt_print_error("File should only contain one line");
			optarg[flen-1] = '\0';
			if (ebt_errormsg[0] != '\0') {
				munmap(argv, flen);
				close(fd);
				exit(-1);
			}
		}
		wh = create_wormhash(optarg);
		if (ebt_errormsg[0] != '\0')
			break;

		new_size = old_size+ebt_mac_wormhash_size(wh);
		h = malloc(sizeof(struct ebt_entry_match)+EBT_ALIGN(new_size));
		if (!h)
			ebt_print_memory();
		memcpy(h, *match, old_size+sizeof(struct ebt_entry_match));
		memcpy((char *)h+old_size+sizeof(struct ebt_entry_match), wh,
		       ebt_mac_wormhash_size(wh));
		h->match_size = EBT_ALIGN(new_size);
		info = (struct ebt_among_info *) h->data;
		if (c == AMONG_DST) {
			info->wh_dst_ofs = old_size;
		} else {
			info->wh_src_ofs = old_size;
		}
		old_size = new_size;
		free(*match);
		*match = h;
		free(wh);
		if (c == AMONG_DST_F || c == AMONG_SRC_F) {
			munmap(argv, flen);
			close(fd);
		}
		break;
	default:
		return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
			const struct ebt_entry_match *match,
			const char *name, unsigned int hookmask,
			unsigned int time)
{
}

#ifdef DEBUG
static void wormhash_debug(const struct ebt_mac_wormhash *wh)
{
	int i;

	printf("poolsize: %d\n", wh->poolsize);
	for (i = 0; i <= 256; i++) {
		printf("%02x ", wh->table[i]);
		if (i % 16 == 15) {
			printf("\n");
		}
	}
	printf("\n");
}
#endif /* DEBUG */

static void wormhash_printout(const struct ebt_mac_wormhash *wh)
{
	int i;
	unsigned char *ip;

	for (i = 0; i < wh->poolsize; i++) {
		const struct ebt_mac_wormhash_tuple *p;

		p = (const struct ebt_mac_wormhash_tuple *)(&wh->pool[i]);
		ebt_print_mac(((const unsigned char *) &p->cmp[0]) + 2);
		if (p->ip) {
			ip = (unsigned char *) &p->ip;
			printf("=%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
		}
		printf(",");
	}
	printf(" ");
}

static void print(const struct ebt_u_entry *entry,
		  const struct ebt_entry_match *match)
{
	struct ebt_among_info *info = (struct ebt_among_info *)match->data;

	if (info->wh_dst_ofs) {
		printf("--among-dst ");
		if (info->bitmask && EBT_AMONG_DST_NEG) {
			printf("! ");
		}
		wormhash_printout(ebt_among_wh_dst(info));
	}
	if (info->wh_src_ofs) {
		printf("--among-src ");
		if (info->bitmask && EBT_AMONG_SRC_NEG) {
			printf("! ");
		}
		wormhash_printout(ebt_among_wh_src(info));
	}
}

static int compare_wh(const struct ebt_mac_wormhash *aw,
		      const struct ebt_mac_wormhash *bw)
{
	int as, bs;

	as = ebt_mac_wormhash_size(aw);
	bs = ebt_mac_wormhash_size(bw);
	if (as != bs)
		return 0;
	if (as && memcmp(aw, bw, as))
		return 0;
	return 1;
}

static int compare(const struct ebt_entry_match *m1,
		   const struct ebt_entry_match *m2)
{
	struct ebt_among_info *a = (struct ebt_among_info *) m1->data;
	struct ebt_among_info *b = (struct ebt_among_info *) m2->data;

	if (!compare_wh(ebt_among_wh_dst(a), ebt_among_wh_dst(b)))
		return 0;
	if (!compare_wh(ebt_among_wh_src(a), ebt_among_wh_src(b)))
		return 0;
	if (a->bitmask != b->bitmask)
		return 0;
	return 1;
}

static struct ebt_u_match among_match = {
	.name 		= "among",
	.size 		= sizeof(struct ebt_among_info),
	.help 		= print_help,
	.init 		= init,
	.parse 		= parse,
	.final_check 	= final_check,
	.print 		= print,
	.compare 	= compare,
	.extra_ops 	= opts,
};

void _init(void)
{
	ebt_register_match(&among_match);
}
