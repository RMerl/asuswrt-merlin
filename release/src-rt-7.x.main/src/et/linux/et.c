/*
 * et driver ioctl swiss army knife command.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: et.c 474541 2014-05-01 18:46:52Z $
 */

#include <stdio.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <typedefs.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <etioctl.h>
#include <proto/ethernet.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#include <linux/sockios.h>
#include <linux/ethtool.h>

static void usage(char *av0);
static void syserr(char *s);
static void et_find(int s, struct ifreq *ifr);
static int et_check(int s, struct ifreq *ifr);



char buf[16 * 1024];

#define VECLEN		5

/* some OSes (FC4) have trouble allocating (kmalloc) 128KB worth of memory,
 * hence keeping ET_DUMP_BUF_LEN below that
 */
#define ET_DUMP_BUF_LEN (127 * 1024)
#define DUMP_BUF_ALLOC(a, b, l) \
{ \
	(a) = 1; \
	(b) = malloc(ET_DUMP_BUF_LEN); \
	(l) = ET_DUMP_BUF_LEN; \
	if (!(b)) { \
		(a) = 0; \
		(b) = buf; \
		(l) = sizeof(buf); \
	} \
	memset(b, 0, l); \
}
#define DUMP_BUF_FREE(a, b)	if ((a)) free(b)


int
main(int ac, char *av[])
{
	char *interface = NULL;
	struct ifreq ifr;
	char *endptr;
	int arg;
	int vecarg[VECLEN];
	int s;
	static int optind;
	et_var_t var;
	char *dbuf;
	int dbuf_len, dbuf_alloc = 0;

	if (ac < 2)
		usage(av[0]);

	optind = 1;

	if (av[1][0] == '-') {
		if ((av[1][1] != 'a') && (av[1][1] != 'i'))
			usage(av[0]);
		if (ac < 4)
			usage(av[0]);
		interface = av[2];
		optind += 2;
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		syserr("socket");

	if (interface)
		strncpy(ifr.ifr_name, interface, sizeof(ifr.ifr_name));
	else
		et_find(s, &ifr);

	if (!*ifr.ifr_name) {
		fprintf(stderr, "et interface not found\n");
		exit(1);
	}

	if (strcmp(av[optind], "up") == 0) {
		if (ioctl(s, SIOCSETCUP, (caddr_t)&ifr) < 0)
			syserr("etcup");
	} else if (strcmp(av[optind], "down") == 0) {
		if (ioctl(s, SIOCSETCDOWN, (caddr_t)&ifr) < 0)
			syserr("etcdown");
	} else if (strcmp(av[optind], "loop") == 0) {
		if (optind >= (ac -1))
			usage(av[0]);
		arg = atoi(av[optind + 1]);
		ifr.ifr_data = (caddr_t) &arg;
		if (ioctl(s, SIOCSETCLOOP, (caddr_t)&ifr) < 0)
			syserr("etcloop");
	} else if ((strcmp(av[optind], "dump") == 0) && (optind == (ac - 1))) {
		DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
		var.cmd = IOV_DUMP;
		var.buf = dbuf;
		var.len = dbuf_len;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
			DUMP_BUF_FREE(dbuf_alloc, dbuf);
			syserr("etcdump");
		}

		printf("%s\n", dbuf);
		DUMP_BUF_FREE(dbuf_alloc, dbuf);
	} else if (strcmp(av[optind], "msglevel") == 0) {
		if (optind >= (ac -1))
			usage(av[0]);
		arg = strtol(av[optind + 1], &endptr, 0);
		ifr.ifr_data = (caddr_t) &arg;
		if (ioctl(s, SIOCSETCSETMSGLEVEL, (caddr_t)&ifr) < 0)
			syserr("etcsetmsglevel");
	} else if (strcmp(av[optind], "promisc") == 0) {
		if (optind >= (ac -1))
			usage(av[0]);
		arg = atoi(av[optind + 1]);
		ifr.ifr_data = (caddr_t) &arg;
		if (ioctl(s, SIOCSETCPROMISC, (caddr_t)&ifr) < 0)
			syserr("etcpromisc");
	} else if (strcmp(av[optind], "qos") == 0) {
		if (optind >= (ac -1))
			usage(av[0]);
		arg = atoi(av[optind + 1]);
		ifr.ifr_data = (caddr_t) &arg;
		if (ioctl(s, SIOCSETCQOS, (caddr_t)&ifr) < 0)
			syserr("etcqos");
	} else if (strcmp(av[optind], "speed") == 0) {
		if (optind >= (ac -1))
			usage(av[0]);
		if (strcmp(av[optind+1], "auto") == 0)
			arg = -1;
		else if (strcmp(av[optind+1], "10half") == 0)
			arg = 0;
		else if (strcmp(av[optind+1], "10full") == 0)
			arg = 1;
		else if (strcmp(av[optind+1], "100half") == 0)
			arg = 2;
		else if (strcmp(av[optind+1], "100full") == 0)
			arg = 3;
		else if (strcmp(av[optind+1], "1000full") == 0)
			arg = 5;
		else
			usage(av[0]);

		ifr.ifr_data = (caddr_t) &arg;
		if (ioctl(s, SIOCSETCSPEED, (caddr_t)&ifr) < 0)
			syserr("etcspeed");
	}
	else if (strcmp(av[optind], "phyrd") == 0) {
		int cmd = -1;

		if ((ac < (optind + 2)) || (ac > (optind + 3))) {
			usage(av[0]);
		} else if (ac == (optind + 3)) {
			/* PHY address provided */
			vecarg[0] = strtoul(av[optind + 1], NULL, 0) << 16;
			vecarg[0] |= strtoul(av[optind + 2], NULL, 0) & 0xffff;
			cmd = SIOCGETCPHYRD2;
		} else {
			/* "My" PHY address implied */
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
			cmd = SIOCGETCPHYRD;
		}
		ifr.ifr_data = (caddr_t) vecarg;
		if (ioctl(s, cmd, (caddr_t)&ifr) < 0)
			syserr("etcphyrd");

		printf("0x%04x\n", vecarg[1]);
	} else if (strcmp(av[optind], "phywr") == 0) {
		int cmd = -1;

		if ((ac < (optind + 3)) || (ac > (optind + 4))) {
			usage(av[0]);
		} else if (ac == (optind + 4)) {
			vecarg[0] = strtoul(av[optind + 1], NULL, 0) << 16;
			vecarg[0] |= strtoul(av[optind + 2], NULL, 0) & 0xffff;
			vecarg[1] = strtoul(av[optind + 3], NULL, 0);
			cmd = SIOCSETCPHYWR2;
		} else {
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
			vecarg[1] = strtoul(av[optind + 2], NULL, 0);
			cmd = SIOCSETCPHYWR;
		}
		ifr.ifr_data = (caddr_t) vecarg;
		if (ioctl(s, cmd, (caddr_t)&ifr) < 0)
			syserr("etcphywr");
	} else if (strcmp(av[optind], "robord") == 0) {
		if ((ac != (optind + 3)) && (ac != (optind + 4)))
			usage(av[0]);

		vecarg[1] = 2;
		if (av[optind + 3])
			vecarg[1] = strtoul(av[optind + 3], NULL, 0);
		/* only 1, 2, 4, 6, and 8 bytes are valid */
		if ((vecarg[1] != 1) && (vecarg[1] != 8) && (0xF9 & vecarg[1]))
			usage(av[0]);

		vecarg[0] = strtoul(av[optind + 1], NULL, 0) << 16;
		vecarg[0] |= strtoul(av[optind + 2], NULL, 0) & 0xffff;

		ifr.ifr_data = (caddr_t) vecarg;
		if (ioctl(s, SIOCGETCROBORD, (caddr_t)&ifr) < 0)
			syserr("etcrobord");

		/* For SPI mode, the length can only be 1, 2, and 4 bytes */
		if (vecarg[1] == -1)
			printf("Invalid length. For SPI mode, "
			"the length can only be 1, 2, and 4 bytes.\n");
		else
			printf("0x%0.*llx\n", (2*vecarg[1]), *((unsigned long long *)&vecarg[2]));
	} else if (strcmp(av[optind], "robowr") == 0) {
		if ((ac != (optind + 4)) && (ac != (optind + 5)))
			usage(av[0]);

		vecarg[1] = 2;
		if (av[optind + 4])
			vecarg[1] = strtoul(av[optind + 4], NULL, 0);
		/* only 1, 2, 4, 6, and 8 bytes are valid */
		if ((vecarg[1] != 1) && (vecarg[1] != 8) && (0xF9 & vecarg[1]))
			usage(av[0]);

		vecarg[0] = strtoul(av[optind + 1], NULL, 0) << 16;
		vecarg[0] |= strtoul(av[optind + 2], NULL, 0) & 0xffff;
		*((unsigned long long *)&vecarg[2]) = strtoull(av[optind + 3], NULL, 0);

		ifr.ifr_data = (caddr_t) vecarg;
		if (ioctl(s, SIOCSETCROBOWR, (caddr_t)&ifr) < 0)
			syserr("etcrobowr");

		/* For SPI mode, the length can only be 1, 2, and 4 bytes */
		if (vecarg[1] == -1)
			printf("Invalid length. "
			"For SPI mode, the length can only be 1, 2, and 4 bytes.\n");
	} else if (strcmp(av[optind], "macrd") == 0) {
		uint offset;
		if (ac != (optind + 2)) {
			usage(av[0]);
		}

		vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		offset = vecarg[0];

		var.set = 0;
		var.len = sizeof(int);
		var.cmd = IOV_MACRD;
		var.buf = &vecarg;
		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("etcmacrd");

		printf("Offset[0x%08x] = 0x%08x\n", offset, vecarg[0]);

	} else if (strcmp(av[optind], "macwr") == 0) {
		if (ac != (optind + 3)) {
			usage(av[0]);
		}
		vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		vecarg[1] = strtoul(av[optind + 2], NULL, 0);

		var.set = 1;
		var.len = 2 * sizeof(int);
		var.cmd = IOV_MACWR;
		var.buf = &vecarg;
		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("etcmacwr");

	} else if (strcmp(av[optind], "port_status") == 0) {
		if (ac >= (optind + 3))
			usage(av[0]);
		var.set = 0;
		var.cmd = IOV_PORTSTATS;
		var.buf = buf;
		if (ac == (optind + 1) || (strcmp(av[optind+1], "all") == 0))
			*(uint *)buf = 0xFF;
		else
			*(uint *)buf = atoi(av[optind + 1]);

		var.len = sizeof(buf);

		ifr.ifr_data = (caddr_t) &var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("etcportstats");

		printf("%s\n", buf);
	} else if (strcmp(av[optind], "sw_mctbl") == 0) {
		if ((ac > (optind + 3)))
			usage(av[0]);

		if (ac == (optind + 1)) {
			*(uint *)buf = 0xff;
		} else if (ac == (optind + 3) && strcmp(av[optind+1], "port") == 0) {
			*(uint *)buf  = 1 << 16;
			*(uint *)buf |= atoi(av[optind + 2]) & 0xffff;
		} else if (ac == (optind + 3) && strcmp(av[optind+1], "vid") == 0) {
			*(uint *)buf = atoi(av[optind + 2]) & 0xffff;
		} else {
			usage(av[0]);
		}

		var.set = 0;
		var.cmd = IOV_SW_MCTBL;
		var.buf = buf;
		var.len = sizeof(buf);

		ifr.ifr_data = (caddr_t) &var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("etcswmctbl");
		printf("%s\n", buf);
	} else if (strcmp(av[optind], "clear_dump") == 0) {
		if ((ac > (optind + 2)))
			usage(av[0]);

		var.set = 1;
		var.cmd = IOV_ET_CLEAR_DUMP;
		var.buf = NULL;
		ifr.ifr_data = (caddr_t) &var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("etccleardump");
	} else if ((strcmp(av[optind], "pktc") == 0) ||
	           (strcmp(av[optind], "pktcbnd") == 0)) {
		/* Get pktc or pktcbnd */
		if (ac == (optind + 1))
			var.set = 0;
		else {
			var.set = 1;
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		}
		var.len = sizeof(int);
		var.cmd = strcmp(av[optind], "pktc") == 0 ? IOV_PKTC : IOV_PKTCBND;
		var.buf = &vecarg;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("pktc");

		if (!var.set)
			printf("%d\n", vecarg[0]);
	} else if (strcmp(av[optind], "quota") == 0) {
		if (ac == (optind + 1))
			var.set = 0;
		else {
			var.set = 1;
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		}
		var.len = sizeof(int);
		var.cmd = IOV_RXQUOTA;
		var.buf = &vecarg;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("quota");

		if (!var.set)
			printf("%d\n", vecarg[0]);
	} else if (strcmp(av[optind], "rxlazyto") == 0) { /* rxlazy timeout */
		if (ac == (optind + 1))
			var.set = 0;
		else {
			var.set = 1;
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		}
		var.len = sizeof(int);
		var.cmd = IOV_RXLAZYTO;
		var.buf = &vecarg;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("rxlazyto");

		if (!var.set)
			printf("%d\n", vecarg[0]);
	} else if (strcmp(av[optind], "rxlazyfc") == 0) { /* rxlazy framecnt */
		if (ac == (optind + 1))
			var.set = 0;
		else {
			var.set = 1;
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		}
		var.len = sizeof(int);
		var.cmd = IOV_RXLAZYFC;
		var.buf = &vecarg;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("rxlazyfc");

		if (!var.set)
			printf("%d\n", vecarg[0]);
	} else if ((strcmp(av[optind], "counters") == 0)) {
		if (ac == (optind + 1))
			var.set = 0;
		else
			syserr("counters");

		DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
		var.cmd = IOV_COUNTERS;
		var.buf = dbuf;
		var.len = dbuf_len;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
			DUMP_BUF_FREE(dbuf_alloc, dbuf);
			syserr("counters");
		}

		printf("%s\n", dbuf);
		DUMP_BUF_FREE(dbuf_alloc, dbuf);
        } else if ((strcmp(av[optind], "dump_oops") == 0)) {
		var.set = 0;
		var.cmd = IOV_DUMP_OOPS;
		var.buf = NULL;
		var.len = 0;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
			syserr("dump_oops");
		}
	} else if ((strcmp(av[optind], "dump") == 0) && (optind < (ac - 1))) {
		if (strcmp(av[optind + 1], "ctf") == 0) {
			if (ac == (optind + 2))
				var.set = 0;
			else
				syserr("dump ctf");

			DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
			var.cmd = IOV_DUMP_CTF;
			var.buf = dbuf;
			var.len = dbuf_len;

			ifr.ifr_data = (caddr_t)&var;
			if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
				DUMP_BUF_FREE(dbuf_alloc, dbuf);
				syserr("dump ctf");
			}

			printf("%s\n", dbuf);
		} else if (strcmp(av[optind + 1], "ctrace") == 0) {
			if (ac == (optind + 2))
				var.set = 0;
			else
				syserr("dump ctrace");

			DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
			var.cmd = IOV_DUMP_CTRACE;
			var.buf = dbuf;
			var.len = dbuf_len;

			ifr.ifr_data = (caddr_t)&var;
			if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
				DUMP_BUF_FREE(dbuf_alloc, dbuf);
				syserr("dump ctrace");
			}

			printf("%s\n", dbuf);
		} else if (strcmp(av[optind + 1], "fa") == 0) {
			if (ac == (optind + 2))
				var.set = 0;
			else
				syserr("dump fa");

			DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
			var.cmd = IOV_FA_DUMP;
			var.buf = dbuf;
			var.len = dbuf_len;

			ifr.ifr_data = (caddr_t)&var;
			if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
				DUMP_BUF_FREE(dbuf_alloc, dbuf);
				syserr("dump fa");
			}

			printf("%s\n", dbuf);
		} else if (strcmp(av[optind + 1], "fwd") == 0) {
			if (ac == (optind + 2))
				var.set = 0;
			else
				syserr("dump fwd");

			DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
			var.cmd = IOV_DUMP_FWDER;
			var.buf = dbuf;
			var.len = dbuf_len;

			ifr.ifr_data = (caddr_t)&var;
			if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
				DUMP_BUF_FREE(dbuf_alloc, dbuf);
				syserr("dump fwd");
			}

			printf("%s\n", dbuf);
		}

		DUMP_BUF_FREE(dbuf_alloc, dbuf);
#ifdef ET_INGRESS_QOS
	} else if ((strcmp(av[optind], "dma_rx_thresh") == 0) ||
		(strcmp(av[optind], "dma_rx_policy") == 0)) {
		/* Get case */
		if (ac == (optind + 1))
			var.set = 0;
		else {
			var.set = 1;
			vecarg[0] = strtoul(av[optind + 1], NULL, 0);
		}
		var.len = sizeof(int);
		var.cmd = (strcmp(av[optind], "dma_rx_thresh") == 0) ?
			IOV_DMA_RX_THRESH : IOV_DMA_RX_POLICY;
		var.buf = &vecarg;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
			syserr("dma_rx_policy_usage");

		if (!var.set)
		        printf("%d\n", vecarg[0]);
#endif /* ET_INGRESS_QOS */
	} else if ((strcmp(av[optind], "cap") == 0)) {
		if (ac == (optind + 1))
			var.set = 0;
		else
			syserr("cap");

		DUMP_BUF_ALLOC(dbuf_alloc, dbuf, dbuf_len);
		var.cmd = IOV_CAP;
		var.buf = dbuf;
		var.len = dbuf_len;

		ifr.ifr_data = (caddr_t)&var;
		if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0) {
			DUMP_BUF_FREE(dbuf_alloc, dbuf);
			syserr("cap");
		}

		printf("%s\n", dbuf);
		DUMP_BUF_FREE(dbuf_alloc, dbuf);
	} else {
		if (strcmp(av[optind], "switch_mode") == 0) {
			int all = 0;

			/* GET case */
			if (ac == (optind + 1)) {
				var.set = 0;
				vecarg[0] = VECLEN;
				all = 1;
			} else if (ac == (optind + 2)) {
				var.set = 0;
				vecarg[0] = strtoul(av[optind + 1], NULL, 0);
				all = (int)(vecarg[0] == VECLEN);
			} else {
				if (ac != (optind + 3))
					usage(av[0]);

				vecarg[0] = strtoul(av[optind + 1], NULL, 0);
				vecarg[1] = strtoul(av[optind + 2], NULL, 0);
				if (vecarg[1] > 3)
					usage(av[0]);

				var.set = 1;
			}

			var.len = VECLEN * sizeof(int);
			var.cmd = IOV_ET_POWER_SAVE_MODE;
			var.buf = &vecarg;

			ifr.ifr_data = (caddr_t) &var;
			if (ioctl(s, SIOCSETGETVAR, (caddr_t)&ifr) < 0)
				syserr("etcswitchmode");

			if (!var.set) {
				if (all)
					printf("phy power save mode for all phys:"
						" %d %d %d %d %d \n",
						vecarg[0], vecarg[1], vecarg[2],
						vecarg[3], vecarg[4]);
				else
					printf("phy power save mode for phy %d mode %d\n",
						vecarg[0], vecarg[1]);
			}
		} else {
			usage(av[0]);
		}
	}

	return (0);
}

static void
usage(char *av0)
{
	fprintf(stderr, "usage: %s [ [ -a | -i ] interface ] and one of:\n"
		"\tup\n"
		"\tdown\n"
		"\tloop <0 or 1>\n"
		"\tdump [ctf | ctrace | fa | fwd]\n"
		"\tclear_dump\n"
		"\tmsglevel <bitvec> (error=1, trace=2, prhdr=4, prpkt=8)\n"
		"\tpromisc <0 or 1>\n"
		"\tqos <0 or 1>\n"
		"\tspeed <auto, 10half, 10full, 100half, 100full, 1000full>\n"
		"\tport_status <0, 1, 2, 3, 4, all>\n"
		"\tsw_mctbl [<port <port#>] | [vid <vid#>]\n"
		"\tphyrd [<phyaddr>] <reg>\n"
		"\tphywr [<phyaddr>] <reg> <val>\n"
		"\trobord <page> <reg> [length] "
		"(length can be 1, 2, 4, 6, 8 bytes. Default length is 2 bytes)\n"
		"\trobowr <page> <reg> <val> [length] "
		"(length can be 1, 2, 4, 6, 8 bytes. Default length is 2 bytes)\n"
		"\tmacrd <reg_offset>\n"
		"\tmacwr <reg_offset> <val>\n"
		"\tswitch_mode <phy> <mode> (mode normal=0, auto=1, manual=2, both=3)\n"
		"\tpktc <0 or 1>\n"
		"\tpktcbnd <val>\n"
		"\tquota <val>\n"
		"\trxlazyto <val>\n"
		"\trxlazyfc <val>\n",
		av0);
	exit(1);
}

static void
et_find(int s, struct ifreq *ifr)
{
	char proc_net_dev[] = "/proc/net/dev";
	FILE *fp;
	char buf[512], *c, *name;

	ifr->ifr_name[0] = '\0';

	/* eat first two lines */
	if (!(fp = fopen(proc_net_dev, "r")) ||
	    !fgets(buf, sizeof(buf), fp) ||
	    !fgets(buf, sizeof(buf), fp))
		return;

	while (fgets(buf, sizeof(buf), fp)) {
		c = buf;
		while (isspace(*c))
			c++;
		if (!(name = strsep(&c, ":")))
			continue;
		if (!strncmp(name, "aux", 3))
			continue;
		strncpy(ifr->ifr_name, name, IFNAMSIZ);
		if (et_check(s, ifr) == 0)
			break;
		ifr->ifr_name[0] = '\0';
	}

	fclose(fp);
}

static int
et_check(int s, struct ifreq *ifr)
{
	struct ethtool_drvinfo info;

	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	ifr->ifr_data = (caddr_t)&info;
	if (ioctl(s, SIOCETHTOOL, (caddr_t)ifr) < 0) {
		/* print a good diagnostic if not superuser */
		if (errno == EPERM)
			syserr("siocethtool");
		return (-1);
	}

	if (!strncmp(info.driver, "et", 2))
		return (0);
	else if (!strncmp(info.driver, "bcm57", 5))
		return (0);

	return (-1);
}

static void
syserr(char *s)
{
	perror(s);
	exit(1);
}
