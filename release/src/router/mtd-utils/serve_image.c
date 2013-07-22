#define PROGRAM_NAME "serve_image"
#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <crc32.h>
#include <inttypes.h>

#include "mcast_image.h"

int tx_rate = 80000;
int pkt_delay;

#undef RANDOMDROP

int main(int argc, char **argv)
{
	struct addrinfo *ai;
	struct addrinfo hints;
	struct addrinfo *runp;
	int ret;
	int sock;
	struct image_pkt pktbuf;
	int rfd;
	struct stat st;
	int writeerrors = 0;
	uint32_t erasesize;
	unsigned char *image, *blockptr = NULL;
	uint32_t block_nr, pkt_nr;
	int nr_blocks;
	struct timeval then, now, nextpkt;
	long time_msecs;
	int pkts_per_block;
	int total_pkts_per_block;
	struct fec_parms *fec;
	unsigned char *last_block;
	uint32_t *block_crcs;
	long tosleep;
	uint32_t sequence = 0;

	if (argc == 6) {
		tx_rate = atol(argv[5]) * 1024;
		if (tx_rate < PKT_SIZE || tx_rate > 20000000) {
			fprintf(stderr, "Bogus TX rate %d KiB/s\n", tx_rate);
			exit(1);
		}
		argc = 5;
	}
	if (argc != 5) {
		fprintf(stderr, "usage: %s <host> <port> <image> <erasesize> [<tx_rate>]\n",
			PROGRAM_NAME);
		exit(1);
	}
	pkt_delay = (sizeof(pktbuf) * 1000000) / tx_rate;
	printf("Inter-packet delay (avg): %dµs\n", pkt_delay);
	printf("Transmit rate: %d KiB/s\n", tx_rate / 1024);

	erasesize = atol(argv[4]);
	if (!erasesize) {
		fprintf(stderr, "erasesize cannot be zero\n");
		exit(1);
	}

	pkts_per_block = (erasesize + PKT_SIZE - 1) / PKT_SIZE;
	total_pkts_per_block = pkts_per_block * 3 / 2;

	/* We have to pad it with zeroes, so can't use it in-place */
	last_block = malloc(pkts_per_block * PKT_SIZE);
	if (!last_block) {
		fprintf(stderr, "Failed to allocate last-block buffer\n");
		exit(1);
	}

	fec = fec_new(pkts_per_block, total_pkts_per_block);
	if (!fec) {
		fprintf(stderr, "Error initialising FEC\n");
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
		if (connect(sock, runp->ai_addr, runp->ai_addrlen) == 0)
			break;
		perror("connect");
		close(sock);
	}
	if (!runp)
		exit(1);

	rfd = open(argv[3], O_RDONLY);
	if (rfd < 0) {
		perror("open");
		exit(1);
	}

	if (fstat(rfd, &st)) {
		perror("fstat");
		exit(1);
	}

	if (st.st_size % erasesize) {
		fprintf(stderr, "Image size %" PRIu64 " bytes is not a multiple of erasesize %d bytes\n",
				st.st_size, erasesize);
		exit(1);
	}
	image = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, rfd, 0);
	if (image == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	nr_blocks = st.st_size / erasesize;

	block_crcs = malloc(nr_blocks * sizeof(uint32_t));
	if (!block_crcs) {
		fprintf(stderr, "Failed to allocate memory for CRCs\n");
		exit(1);
	}

	memcpy(last_block, image + (nr_blocks - 1) * erasesize, erasesize);
	memset(last_block + erasesize, 0, (PKT_SIZE * pkts_per_block) - erasesize);

	printf("Checking CRC....");
	fflush(stdout);

	pktbuf.hdr.resend = 0;
	pktbuf.hdr.totcrc = htonl(mtd_crc32(-1, image, st.st_size));
	pktbuf.hdr.nr_blocks = htonl(nr_blocks);
	pktbuf.hdr.blocksize = htonl(erasesize);
	pktbuf.hdr.thislen = htonl(PKT_SIZE);
	pktbuf.hdr.nr_pkts = htons(total_pkts_per_block);

	printf("%08x\n", ntohl(pktbuf.hdr.totcrc));
	printf("Checking block CRCs....");
	fflush(stdout);
	for (block_nr=0; block_nr < nr_blocks; block_nr++) {
		printf("\rChecking block CRCS.... %d/%d",
		       block_nr + 1, nr_blocks);
		fflush(stdout);
		block_crcs[block_nr] = mtd_crc32(-1, image + (block_nr * erasesize), erasesize);
	}

	printf("\nImage size %ld KiB (0x%08lx). %d blocks at %d pkts/block\n"
	       "Estimated transmit time per cycle: %ds\n",
	       (long)st.st_size / 1024, (long) st.st_size,
	       nr_blocks, pkts_per_block,
	       nr_blocks * pkts_per_block * pkt_delay / 1000000);
	gettimeofday(&then, NULL);
	nextpkt = then;

#ifdef RANDOMDROP
	srand((unsigned)then.tv_usec);
	printf("Random seed %u\n", (unsigned)then.tv_usec);
#endif
	while (1) for (pkt_nr=0; pkt_nr < total_pkts_per_block; pkt_nr++) {

		if (blockptr && pkt_nr == 0) {
 			unsigned long amt_sent = total_pkts_per_block * nr_blocks * sizeof(pktbuf);
			gettimeofday(&now, NULL);

			time_msecs = (now.tv_sec - then.tv_sec) * 1000;
			time_msecs += ((int)(now.tv_usec - then.tv_usec)) / 1000;
			printf("\n%ld KiB sent in %ldms (%ld KiB/s)\n",
			       amt_sent / 1024, time_msecs,
			       amt_sent / 1024 * 1000 / time_msecs);
			then = now;
		}

		for (block_nr = 0; block_nr < nr_blocks; block_nr++) {

			int actualpkt;

			/* Calculating the redundant FEC blocks is expensive;
			   the first $pkts_per_block are cheap enough though
			   because they're just copies. So alternate between
			   simple and complex stuff, so that we don't start
			   to choke and fail to keep up with the expected
			   bitrate in the second half of the sequence */
			if (block_nr & 1)
				actualpkt = pkt_nr;
			else
				actualpkt = total_pkts_per_block - 1 - pkt_nr;

			blockptr = image + (erasesize * block_nr);
			if (block_nr == nr_blocks - 1)
				blockptr = last_block;

			fec_encode_linear(fec, blockptr, pktbuf.data, actualpkt, PKT_SIZE);

			pktbuf.hdr.thiscrc = htonl(mtd_crc32(-1, pktbuf.data, PKT_SIZE));
			pktbuf.hdr.block_crc = htonl(block_crcs[block_nr]);
			pktbuf.hdr.block_nr = htonl(block_nr);
			pktbuf.hdr.pkt_nr = htons(actualpkt);
			pktbuf.hdr.pkt_sequence = htonl(sequence++);

			printf("\rSending data block %08x packet %3d/%d",
			       block_nr * erasesize,
			       pkt_nr, total_pkts_per_block);

			if (pkt_nr && !block_nr) {
				unsigned long amt_sent = pkt_nr * nr_blocks * sizeof(pktbuf);

				gettimeofday(&now, NULL);

				time_msecs = (now.tv_sec - then.tv_sec) * 1000;
				time_msecs += ((int)(now.tv_usec - then.tv_usec)) / 1000;
				printf("    (%ld KiB/s)    ",
				       amt_sent / 1024 * 1000 / time_msecs);
			}

			fflush(stdout);

#ifdef RANDOMDROP
			if ((rand() % 1000) < 20) {
				printf("\nDropping packet %d of block %08x\n", pkt_nr+1, block_nr * erasesize);
				continue;
			}
#endif
			gettimeofday(&now, NULL);
#if 1
			tosleep = nextpkt.tv_usec - now.tv_usec +
				(1000000 * (nextpkt.tv_sec - now.tv_sec));

			/* We need hrtimers for this to actually work */
			if (tosleep > 0) {
				struct timespec req;

				req.tv_nsec = (tosleep % 1000000) * 1000;
				req.tv_sec = tosleep / 1000000;

				nanosleep(&req, NULL);
			}
#else
			while (now.tv_sec < nextpkt.tv_sec ||
				 (now.tv_sec == nextpkt.tv_sec &&
				  now.tv_usec < nextpkt.tv_usec)) {
				gettimeofday(&now, NULL);
			}
#endif
			nextpkt.tv_usec += pkt_delay;
			if (nextpkt.tv_usec >= 1000000) {
				nextpkt.tv_sec += nextpkt.tv_usec / 1000000;
				nextpkt.tv_usec %= 1000000;
			}

			/* If the time for the next packet has already
			   passed (by some margin), then we've lost time
			   Adjust our expected timings accordingly. If
			   we're only a little way behind, don't slip yet */
			if (now.tv_usec > (now.tv_usec + (5 * pkt_delay) +
					1000000 * (nextpkt.tv_sec - now.tv_sec))) {
				nextpkt = now;
			}

			if (write(sock, &pktbuf, sizeof(pktbuf)) < 0) {
				perror("write");
				writeerrors++;
				if (writeerrors > 10) {
					fprintf(stderr, "Too many consecutive write errors\n");
					exit(1);
				}
			} else
				writeerrors = 0;



		}
	}
	munmap(image, st.st_size);
	close(rfd);
	close(sock);
	return 0;
}
