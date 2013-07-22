/*
 * partitiontype.c, aeb, 2001-09-10
 *
 * call: partitiontype device
 *
 * either exit(1), or exit(0) with a single line of output
 * DOS: sector 0 has a DOS signature.
 */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

struct aix_label {
	unsigned int   magic;
	/* more ... */
};

#define	AIX_LABEL_MAGIC		0xc9c2d4c1
#define	AIX_LABEL_MAGIC_SWAPPED	0xc1d4c2c9

struct bsd_label {
	unsigned int   magic;
	unsigned char  stuff[128];
	unsigned int   magic2;
	/* more ... */
};

#define BSD_LABEL_MAGIC         0x82564557

struct sgi_label {
	unsigned int   magic;
	/* more ... */
};

#define	SGI_LABEL_MAGIC		0x0be5a941
#define	SGI_LABEL_MAGIC_SWAPPED	0x41a9e50b

struct sun_label {
	unsigned char stuff[508];
	unsigned short magic;      /* Magic number */
	unsigned short csum;       /* Label xor'd checksum */
};

#define SUN_LABEL_MAGIC          0xDABE
#define SUN_LABEL_MAGIC_SWAPPED  0xBEDA

int
main(int argc, char **argv) {
	int fd, n;
	unsigned char buf[1024];
	struct aix_label *paix;
	struct bsd_label *pbsd;
	struct sgi_label *psgi;
	struct sun_label *psun;

	if (argc != 2) {
		fprintf(stderr, "call: %s device\n", argv[0]);
		exit(1);
	}
	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror(argv[1]);
		fprintf(stderr, "%s: cannot open device %s\n",
			argv[0], argv[1]);
		exit(1);
	}
	n = read(fd, buf, sizeof(buf));
	if (n != sizeof(buf)) {
		if (n == -1)
			perror(argv[1]);
		fprintf(stderr, "%s: cannot read device %s\n",
			argv[0], argv[1]);
		exit(1);
	}

	psun = (struct sun_label *)(&buf);
	if (psun->magic == SUN_LABEL_MAGIC ||
	    psun->magic == SUN_LABEL_MAGIC_SWAPPED) {
		unsigned short csum = 0, *p;
		int i;

		for (p = (unsigned short *)(&buf);
		     p < (unsigned short *)(&buf[512]); p++)
			csum ^= *p;

		if (csum == 0) {
			printf("SUN\n");
			exit(0);
		}
	}

	pbsd = (struct bsd_label *)(&buf[512]);
	if (pbsd->magic == BSD_LABEL_MAGIC &&
	    pbsd->magic2 == BSD_LABEL_MAGIC) {
		printf("BSD\n");
		exit(0);
	}

	pbsd = (struct bsd_label *)(&buf[64]);
	if (pbsd->magic == BSD_LABEL_MAGIC &&
	    pbsd->magic2 == BSD_LABEL_MAGIC) {
		printf("BSD\n");
		exit(0);
	}

	paix = (struct aix_label *)(&buf);
	if (paix->magic == AIX_LABEL_MAGIC ||
	    paix->magic == AIX_LABEL_MAGIC_SWAPPED) {
		printf("AIX\n");
		exit(0);
	}

	psgi = (struct sgi_label *)(&buf);
	if (psgi->magic == SGI_LABEL_MAGIC ||
	    psgi->magic == SGI_LABEL_MAGIC_SWAPPED) {
		printf("SGI\n");
		exit(0);
	}

	if (buf[510] == 0x55 && buf[511] == 0xaa) {
		printf("DOS\n");
		exit(0);
	}
#if 0
	fprintf(stderr, "%s: do not recognize any label on %s\n",
		argv[0], argv[1]);
#endif
	exit(1);		/* unknown */
}

