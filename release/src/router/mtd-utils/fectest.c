#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mcast_image.h"
#include <crc32.h>

#define ERASE_SIZE 131072
//#define PKT_SIZE 1400
#define NR_PKTS ((ERASE_SIZE + PKT_SIZE - 1) / PKT_SIZE)
#define DROPS 8

int main(void)
{
	int i, j;
	unsigned char buf[NR_PKTS * PKT_SIZE];
	unsigned char pktbuf[(NR_PKTS + DROPS) * PKT_SIZE];
	struct fec_parms *fec;
	unsigned char *srcs[NR_PKTS];
	unsigned char *pkt[NR_PKTS + DROPS];
	int pktnr[NR_PKTS + DROPS];
	struct timeval then, now;

	srand(3453);
	for (i=0; i < sizeof(buf); i++)
		if (i < ERASE_SIZE)
			buf[i] = rand();
		else
			buf[i] = 0;

	for (i=0; i < NR_PKTS + DROPS; i++)
		srcs[i] = buf + (i * PKT_SIZE);

	for (i=0; i < NR_PKTS + DROPS; i++) {
		pkt[i] = malloc(PKT_SIZE);
		pktnr[i] = -1;
	}
	fec = fec_new(NR_PKTS, NR_PKTS + DROPS);
	if (!fec) {
		printf("fec_init() failed\n");
		exit(1);
	}
	j = 0;
	for (i=0; i < NR_PKTS + DROPS; i++) {
#if 1
		if (i == 27  || i == 40  || i == 44 || i == 45 || i == 56 )
			continue;
#endif
		if (i == 69 || i == 93 || i == 103)
			continue;
		fec_encode(fec, srcs, pkt[j], i, PKT_SIZE);
		pktnr[j] = i;
		j++;
	}
	gettimeofday(&then, NULL);
	if (fec_decode(fec, pkt, pktnr, PKT_SIZE)) {
		printf("Decode failed\n");
		exit(1);
	}

	for (i=0; i < NR_PKTS; i++)
		memcpy(pktbuf + (i*PKT_SIZE), pkt[i], PKT_SIZE);
	gettimeofday(&now, NULL);
	now.tv_sec -= then.tv_sec;
	now.tv_usec -= then.tv_usec;
	if (now.tv_usec < 0) {
		now.tv_usec += 1000000;
		now.tv_sec--;
	}

	if (memcmp(pktbuf, buf, ERASE_SIZE)) {
		int fd;
		printf("Compare failed\n");
		fd = open("before", O_WRONLY|O_TRUNC|O_CREAT, 0644);
		if (fd >= 0)
			write(fd, buf, ERASE_SIZE);
		close(fd);
		fd = open("after", O_WRONLY|O_TRUNC|O_CREAT, 0644);
		if (fd >= 0)
			write(fd, pktbuf, ERASE_SIZE);
		
		exit(1);
	}

	printf("Decoded in %ld.%06lds\n", now.tv_sec, now.tv_usec);
	return 0;
}
