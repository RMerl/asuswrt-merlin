
#define PROGRAM_NAME "recv_image"
#define _XOPEN_SOURCE 500
#define _BSD_SOURCE	/* struct ip_mreq */

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <crc32.h>
#include "mtd/mtd-user.h"
#include "mcast_image.h"

#include "common.h"

#define WBUF_SIZE 4096
struct eraseblock {
	uint32_t flash_offset;
	unsigned char wbuf[WBUF_SIZE];
	int wbuf_ofs;
	int nr_pkts;
	int *pkt_indices;
	uint32_t crc;
};

int main(int argc, char **argv)
{
	struct addrinfo *ai;
	struct addrinfo hints;
	struct addrinfo *runp;
	int ret;
	int sock;
	ssize_t len;
	int flfd;
	struct mtd_info_user meminfo;
	unsigned char *eb_buf, *decode_buf, **src_pkts;
	int nr_blocks = 0;
	int pkts_per_block;
	int block_nr = -1;
	uint32_t image_crc = 0;
	int total_pkts = 0;
	int ignored_pkts = 0;
	loff_t mtdoffset = 0;
	int badcrcs = 0;
	int duplicates = 0;
	int file_mode = 0;
	struct fec_parms *fec = NULL;
	int i;
	struct eraseblock *eraseblocks = NULL;
	uint32_t start_seq = 0;
	struct timeval start, now;
	unsigned long fec_time = 0, flash_time = 0, crc_time = 0,
		rflash_time = 0, erase_time = 0, net_time = 0;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <host> <port> <mtddev>\n",
			PROGRAM_NAME);
		exit(1);
	}
	/* Open the device */
	flfd = open(argv[3], O_RDWR);

	if (flfd >= 0) {
		/* Fill in MTD device capability structure */
		if (ioctl(flfd, MEMGETINFO, &meminfo) != 0) {
			perror("MEMGETINFO");
			close(flfd);
			flfd = -1;
		} else {
			printf("Receive to MTD device %s with erasesize %d\n",
			       argv[3], meminfo.erasesize);
		}
	}
	if (flfd == -1) {
		/* Try again, as if it's a file */
		flfd = open(argv[3], O_CREAT|O_TRUNC|O_RDWR, 0644);
		if (flfd < 0) {
			perror("open");
			exit(1);
		}
		meminfo.erasesize = 131072;
		file_mode = 1;
		printf("Receive to file %s with (assumed) erasesize %d\n",
		       argv[3], meminfo.erasesize);
	}

	pkts_per_block = (meminfo.erasesize + PKT_SIZE - 1) / PKT_SIZE;

	eb_buf = malloc(pkts_per_block * PKT_SIZE);
	decode_buf = malloc(pkts_per_block * PKT_SIZE);
	if (!eb_buf && !decode_buf) {
		fprintf(stderr, "No memory for eraseblock buffer\n");
		exit(1);
	}
	src_pkts = malloc(sizeof(unsigned char *) * pkts_per_block);
	if (!src_pkts) {
		fprintf(stderr, "No memory for decode packet pointers\n");
		exit(1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_DGRAM;

	ret = getaddrinfo(argv[1], argv[2], &hints, &ai);
	if (ret) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(1);
	}
	runp = ai;
	for (runp = ai; runp; runp = runp->ai_next) {
		sock = socket(runp->ai_family, runp->ai_socktype,
			      runp->ai_protocol);
		if (sock == -1) {
			perror("socket");
			continue;
		}
		if (runp->ai_family == AF_INET &&
		    IN_MULTICAST( ntohl(((struct sockaddr_in *)runp->ai_addr)->sin_addr.s_addr))) {
			struct ip_mreq rq;
			rq.imr_multiaddr = ((struct sockaddr_in *)runp->ai_addr)->sin_addr;
			rq.imr_interface.s_addr = INADDR_ANY;
			if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &rq, sizeof(rq))) {
				perror("IP_ADD_MEMBERSHIP");
				close(sock);
				continue;
			}

		} else if (runp->ai_family == AF_INET6 &&
			   ((struct sockaddr_in6 *)runp->ai_addr)->sin6_addr.s6_addr[0] == 0xff) {
			struct ipv6_mreq rq;
			rq.ipv6mr_multiaddr =  ((struct sockaddr_in6 *)runp->ai_addr)->sin6_addr;
			rq.ipv6mr_interface = 0;
			if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &rq, sizeof(rq))) {
				perror("IPV6_ADD_MEMBERSHIP");
				close(sock);
				continue;
			}
		}
		if (bind(sock, runp->ai_addr, runp->ai_addrlen)) {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	if (!runp)
		exit(1);

	while (1) {
		struct image_pkt thispkt;

		len = read(sock, &thispkt, sizeof(thispkt));

		if (len < 0) {
			perror("read socket");
			break;
		}
		if (len < sizeof(thispkt)) {
			fprintf(stderr, "Wrong length %zd bytes (expected %zu)\n",
				len, sizeof(thispkt));
			continue;
		}
		if (!eraseblocks) {
			image_crc = thispkt.hdr.totcrc;
			start_seq = ntohl(thispkt.hdr.pkt_sequence);

			if (meminfo.erasesize != ntohl(thispkt.hdr.blocksize)) {
				fprintf(stderr, "Erasesize mismatch (0x%x not 0x%x)\n",
					ntohl(thispkt.hdr.blocksize), meminfo.erasesize);
				exit(1);
			}
			nr_blocks = ntohl(thispkt.hdr.nr_blocks);

			fec = fec_new(pkts_per_block, ntohs(thispkt.hdr.nr_pkts));

			eraseblocks = malloc(nr_blocks * sizeof(*eraseblocks));
			if (!eraseblocks) {
				fprintf(stderr, "No memory for block map\n");
				exit(1);
			}
			for (i = 0; i < nr_blocks; i++) {
				eraseblocks[i].pkt_indices = malloc(sizeof(int) * pkts_per_block);
				if (!eraseblocks[i].pkt_indices) {
					fprintf(stderr, "Failed to allocate packet indices\n");
					exit(1);
				}
				eraseblocks[i].nr_pkts = 0;
				if (!file_mode) {
					if (mtdoffset >= meminfo.size) {
						fprintf(stderr, "Run out of space on flash\n");
						exit(1);
					}
#if 1 /* Deliberately use bad blocks... test write failures */
					while (ioctl(flfd, MEMGETBADBLOCK, &mtdoffset) > 0) {
						printf("Skipping flash bad block at %08x\n", (uint32_t)mtdoffset);
						mtdoffset += meminfo.erasesize;
					}
#endif
				}
				eraseblocks[i].flash_offset = mtdoffset;
				mtdoffset += meminfo.erasesize;
				eraseblocks[i].wbuf_ofs = 0;
			}
			gettimeofday(&start, NULL);
		}
		if (image_crc != thispkt.hdr.totcrc) {
			fprintf(stderr, "\nImage CRC changed from 0x%x to 0x%x. Aborting\n",
				ntohl(image_crc), ntohl(thispkt.hdr.totcrc));
			exit(1);
		}

		block_nr = ntohl(thispkt.hdr.block_nr);
		if (block_nr >= nr_blocks) {
			fprintf(stderr, "\nErroneous block_nr %d (> %d)\n",
				block_nr, nr_blocks);
			exit(1);
		}
		for (i=0; i<eraseblocks[block_nr].nr_pkts; i++) {
			if (eraseblocks[block_nr].pkt_indices[i] == ntohs(thispkt.hdr.pkt_nr)) {
//				printf("Discarding duplicate packet at %08x pkt %d\n",
//				       block_nr * meminfo.erasesize, eraseblocks[block_nr].pkt_indices[i]);
				duplicates++;
				break;
			}
		}
		if (i < eraseblocks[block_nr].nr_pkts) {
			continue;
		}

		if (eraseblocks[block_nr].nr_pkts >= pkts_per_block) {
			/* We have a block which we didn't really need */
			eraseblocks[block_nr].nr_pkts++;
			ignored_pkts++;
			continue;
		}

		if (mtd_crc32(-1, thispkt.data, PKT_SIZE) != ntohl(thispkt.hdr.thiscrc)) {
			printf("\nDiscard %08x pkt %d with bad CRC (%08x not %08x)\n",
			       block_nr * meminfo.erasesize, ntohs(thispkt.hdr.pkt_nr),
			       mtd_crc32(-1, thispkt.data, PKT_SIZE),
			       ntohl(thispkt.hdr.thiscrc));
			badcrcs++;
			continue;
		}
	pkt_again:
		eraseblocks[block_nr].pkt_indices[eraseblocks[block_nr].nr_pkts++] =
			ntohs(thispkt.hdr.pkt_nr);
		total_pkts++;
		if (!(total_pkts % 50) || total_pkts == pkts_per_block * nr_blocks) {
			uint32_t pkts_sent = ntohl(thispkt.hdr.pkt_sequence) - start_seq + 1;
			long time_msec;
			gettimeofday(&now, NULL);

			time_msec = ((now.tv_usec - start.tv_usec) / 1000) +
				(now.tv_sec - start.tv_sec) * 1000;

			printf("\rReceived %d/%d (%d%%) in %lds @%ldKiB/s, %d lost (%d%%), %d dup/xs    ",
			       total_pkts, nr_blocks * pkts_per_block,
			       total_pkts * 100 / nr_blocks / pkts_per_block,
			       time_msec / 1000,
			       total_pkts * PKT_SIZE / 1024 * 1000 / time_msec,
			       pkts_sent - total_pkts - duplicates - ignored_pkts,
			       (pkts_sent - total_pkts - duplicates - ignored_pkts) * 100 / pkts_sent,
			       duplicates + ignored_pkts);
			fflush(stdout);
		}

		if (eraseblocks[block_nr].wbuf_ofs + PKT_SIZE < WBUF_SIZE) {
			/* New packet doesn't full the wbuf */
			memcpy(eraseblocks[block_nr].wbuf + eraseblocks[block_nr].wbuf_ofs,
			       thispkt.data, PKT_SIZE);
			eraseblocks[block_nr].wbuf_ofs += PKT_SIZE;
		} else {
			int fits = WBUF_SIZE - eraseblocks[block_nr].wbuf_ofs;
			ssize_t wrotelen;
			static int faked = 1;

			memcpy(eraseblocks[block_nr].wbuf + eraseblocks[block_nr].wbuf_ofs,
			       thispkt.data, fits);
			wrotelen = pwrite(flfd, eraseblocks[block_nr].wbuf, WBUF_SIZE,
					  eraseblocks[block_nr].flash_offset);

			if (wrotelen < WBUF_SIZE || (block_nr == 5 && eraseblocks[block_nr].nr_pkts == 5 && !faked)) {
				faked = 1;
				if (wrotelen < 0)
					perror("\npacket write");
				else
					fprintf(stderr, "\nshort write of packet wbuf\n");

				if (!file_mode) {
					struct erase_info_user erase;
					/* FIXME: Perhaps we should store pkt crcs and try
					   to recover data from the offending eraseblock */

					/* We have increased nr_pkts but not yet flash_offset */
					erase.start = eraseblocks[block_nr].flash_offset &
						~(meminfo.erasesize - 1);
					erase.length = meminfo.erasesize;

					printf("Will erase at %08x len %08x (bad write was at %08x)\n",
					       erase.start, erase.length, eraseblocks[block_nr].flash_offset);
					if (ioctl(flfd, MEMERASE, &erase)) {
						perror("MEMERASE");
						exit(1);
					}
					if (mtdoffset >= meminfo.size) {
						fprintf(stderr, "Run out of space on flash\n");
						exit(1);
					}
					while (ioctl(flfd, MEMGETBADBLOCK, &mtdoffset) > 0) {
						printf("Skipping flash bad block at %08x\n", (uint32_t)mtdoffset);
						mtdoffset += meminfo.erasesize;
						if (mtdoffset >= meminfo.size) {
							fprintf(stderr, "Run out of space on flash\n");
							exit(1);
						}
					}
					eraseblocks[block_nr].flash_offset = mtdoffset;
					printf("Block #%d will now be at %08lx\n", block_nr, (long)mtdoffset);
					total_pkts -= eraseblocks[block_nr].nr_pkts;
					eraseblocks[block_nr].nr_pkts = 0;
					eraseblocks[block_nr].wbuf_ofs = 0;
					mtdoffset += meminfo.erasesize;
					goto pkt_again;
				}
				else /* Usually nothing we can do in file mode */
					exit(1);
			}
			eraseblocks[block_nr].flash_offset += WBUF_SIZE;
			/* Copy the remainder into the wbuf */
			memcpy(eraseblocks[block_nr].wbuf, &thispkt.data[fits], PKT_SIZE - fits);
			eraseblocks[block_nr].wbuf_ofs = PKT_SIZE - fits;
		}

		if (eraseblocks[block_nr].nr_pkts == pkts_per_block) {
			eraseblocks[block_nr].crc = ntohl(thispkt.hdr.block_crc);

			if (total_pkts == nr_blocks * pkts_per_block)
				break;
		}
	}
	printf("\n");
	gettimeofday(&now, NULL);
	net_time = (now.tv_usec - start.tv_usec) / 1000;
	net_time += (now.tv_sec - start.tv_sec) * 1000;
	close(sock);
	for (block_nr = 0; block_nr < nr_blocks; block_nr++) {
		ssize_t rwlen;
		gettimeofday(&start, NULL);
		eraseblocks[block_nr].flash_offset -= meminfo.erasesize;
		rwlen = pread(flfd, eb_buf, meminfo.erasesize, eraseblocks[block_nr].flash_offset);

		gettimeofday(&now, NULL);
		rflash_time += (now.tv_usec - start.tv_usec) / 1000;
		rflash_time += (now.tv_sec - start.tv_sec) * 1000;
		if (rwlen < 0) {
			perror("read");
			/* Argh. Perhaps we could go back and try again, but if the flash is
			   going to fail to read back what we write to it, and the whole point
			   in this program is to write to it, what's the point? */
			fprintf(stderr, "Packets we wrote to flash seem to be unreadable. Aborting\n");
			exit(1);
		}

		memcpy(eb_buf + meminfo.erasesize, eraseblocks[block_nr].wbuf,
		       eraseblocks[block_nr].wbuf_ofs);

		for (i=0; i < pkts_per_block; i++)
			src_pkts[i] = &eb_buf[i * PKT_SIZE];

		gettimeofday(&start, NULL);
		if (fec_decode(fec, src_pkts, eraseblocks[block_nr].pkt_indices, PKT_SIZE)) {
			/* Eep. This cannot happen */
			printf("The world is broken. fec_decode() returned error\n");
			exit(1);
		}
		gettimeofday(&now, NULL);
		fec_time += (now.tv_usec - start.tv_usec) / 1000;
		fec_time += (now.tv_sec - start.tv_sec) * 1000;

		for (i=0; i < pkts_per_block; i++)
			memcpy(&decode_buf[i*PKT_SIZE], src_pkts[i], PKT_SIZE);

		/* Paranoia */
		gettimeofday(&start, NULL);
		if (mtd_crc32(-1, decode_buf, meminfo.erasesize) != eraseblocks[block_nr].crc) {
			printf("\nCRC mismatch for block #%d: want %08x got %08x\n",
			       block_nr, eraseblocks[block_nr].crc,
			       mtd_crc32(-1, decode_buf, meminfo.erasesize));
			exit(1);
		}
		gettimeofday(&now, NULL);
		crc_time += (now.tv_usec - start.tv_usec) / 1000;
		crc_time += (now.tv_sec - start.tv_sec) * 1000;
		start = now;

		if (!file_mode) {
			struct erase_info_user erase;

			erase.start = eraseblocks[block_nr].flash_offset;
			erase.length = meminfo.erasesize;

			printf("\rErasing block at %08x...", erase.start);

			if (ioctl(flfd, MEMERASE, &erase)) {
				perror("MEMERASE");
				/* This block has dirty data on it. If the erase failed, we're screwed */
				fprintf(stderr, "Erase to clean FEC data from flash failed. Aborting\n");
				exit(1);
			}
			gettimeofday(&now, NULL);
			erase_time += (now.tv_usec - start.tv_usec) / 1000;
			erase_time += (now.tv_sec - start.tv_sec) * 1000;
			start = now;
		}
		else printf("\r");
	write_again:
		rwlen = pwrite(flfd, decode_buf, meminfo.erasesize, eraseblocks[block_nr].flash_offset);
		if (rwlen < meminfo.erasesize) {
			if (rwlen < 0) {
				perror("\ndecoded data write");
			} else
				fprintf(stderr, "\nshort write of decoded data\n");

			if (!file_mode) {
				struct erase_info_user erase;
				erase.start = eraseblocks[block_nr].flash_offset;
				erase.length = meminfo.erasesize;

				printf("Erasing failed block at %08x\n",
				       eraseblocks[block_nr].flash_offset);

				if (ioctl(flfd, MEMERASE, &erase)) {
					perror("MEMERASE");
					exit(1);
				}
				if (mtdoffset >= meminfo.size) {
					fprintf(stderr, "Run out of space on flash\n");
					exit(1);
				}
				while (ioctl(flfd, MEMGETBADBLOCK, &mtdoffset) > 0) {
					printf("Skipping flash bad block at %08x\n", (uint32_t)mtdoffset);
					mtdoffset += meminfo.erasesize;
					if (mtdoffset >= meminfo.size) {
						fprintf(stderr, "Run out of space on flash\n");
						exit(1);
					}
				}
				printf("Will try again at %08lx...", (long)mtdoffset);
				eraseblocks[block_nr].flash_offset = mtdoffset;

				goto write_again;
			}
			else /* Usually nothing we can do in file mode */
				exit(1);
		}
		gettimeofday(&now, NULL);
		flash_time += (now.tv_usec - start.tv_usec) / 1000;
		flash_time += (now.tv_sec - start.tv_sec) * 1000;

		printf("wrote image block %08x (%d pkts)    ",
		       block_nr * meminfo.erasesize, eraseblocks[block_nr].nr_pkts);
		fflush(stdout);
	}
	close(flfd);
	printf("Net rx   %ld.%03lds\n", net_time / 1000, net_time % 1000);
	printf("flash rd %ld.%03lds\n", rflash_time / 1000, rflash_time % 1000);
	printf("FEC time %ld.%03lds\n", fec_time / 1000, fec_time % 1000);
	printf("CRC time %ld.%03lds\n", crc_time / 1000, crc_time % 1000);
	printf("flash wr %ld.%03lds\n", flash_time / 1000, flash_time % 1000);
	printf("flash er %ld.%03lds\n", erase_time / 1000, erase_time % 1000);

	return 0;
}
