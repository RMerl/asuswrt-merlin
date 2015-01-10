#include <rtconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/mman.h>

int page_size;
#define PAGE_SIZE page_size
#define PAGE_MASK (~(PAGE_SIZE-1))

static char *cmdname;

static void usage(void)
{
	printf("Usage: %s [[-l length] [-u 1|2|4]] [address]\n", cmdname);
	exit(-1);
}

static unsigned long limit_length(unsigned long addr, unsigned long len)
{
#if defined(RTCONFIG_QCA)
	unsigned long boundary_addr;
	struct region_s {
		unsigned long first_addr;
		unsigned long last_addr;
	} region_tbl[] = {
		{ 0x18000000, 0x1800015C },
		{ 0x18018000, 0x18018080 },
		{ 0x18020000, 0x18020018 },
		{ 0x18030000, 0x1803000C },
		{ 0x18040000, 0x1804006C },
		{ 0x18050000, 0x18050048 },
		{ 0x18060000, 0x1806405C },
		{ 0x18070000, 0x18070010 },
		{ 0x18080000, 0x1808305C },
		{ 0x180A0000, 0x180A006C },
		{ 0x180A9000, 0x180A9030 },
		{ 0x180B0000, 0x180B0018 },
		{ 0x180B8000, 0x180B8024 },
		{ 0x180F0000, 0x180F005C },
		{ 0x18100008, 0x18100104 },
		{ 0x18100800, 0x18100A44 },
		{ 0x18101000, 0x18101F04 },
		{ 0x18104000, 0x1810409C },
		{ 0x18107000, 0x18107058 },
		{ 0x18108000, 0x1810E000 },
		{ 0x180C0000, 0x180C003E },
		{ 0x18116200, 0x18116C88 },
		{ 0x18116CC0, 0x18116CC4 },
		{ 0x18116DC0, 0x18116DC8 },
		{ 0x18116D00, 0x18116D08 },
		{ 0x18400000, 0x18400054 },
		{ 0x18500000, 0x18500010 },
		{ 0x19000000, 0x190001D8 },
		{ 0x1A000000, 0x1A0001D8 },
		{ 0x1B000000, 0x1B0001FC },
		{ 0x1B400000, 0x1B4001FC },
		{ 0x1B000200, 0x1B0002B4 },
		{ 0x18127800, 0x18127D18 },
		{ 0x00000000, 0x00000F18 },
		{ 0x1F000000, 0x1F000F18 },
		{ 0x18116180, 0x18116188 },
		{ 0x181161C0, 0x181161C8 },
		{ 0x18116200, 0x18116208 },
		{ 0x18116240, 0x18116248 },
		{ 0x18116C00, 0x18116C08 },

		{ 0, 0 },
	}, *p;

	addr &= 0x3FFFFFFF;	/* strip highest two bit */
	for (p = &region_tbl[0]; p->last_addr; ++p) {
		if (addr < p->first_addr || addr > p->last_addr)
			continue;

		boundary_addr = p->last_addr + 4;
		if ((addr + len) >= boundary_addr) {
			len = boundary_addr - addr;
			printf("Adjust length to 0x%lx\n", len);
		}
		break;
	}
#elif defined(RTCONFIG_RALINK)
#endif

	return len;
}

int get_var(unsigned long addr, int len, int unit)
{
	off_t offset = addr & PAGE_MASK;
	int i = 0, ret, l;
	char *map;
	static int kfd = -1;
	unsigned int *p4;
	unsigned char *p1;
	unsigned short int *p2;

	if (len <= 0 || !(unit == 1 || unit == 2 || unit == 4))
		return -1;

	kfd = open("/dev/mem", O_RDONLY);
	if (kfd < 0) {
		perror("open");
		return -2;
	}

	map = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, kfd, offset);
	if (map == MAP_FAILED) {
		perror("mmap");
		exit(-1);
	}
	p4 = (unsigned int*) (map + (addr - offset));
	p2 = (unsigned short*) p4;
	p1 = (unsigned char*) p4;

	for (i = 0, l = len; l > 0; i += unit, l -= unit) {
		if (!(i % 16))
			printf("\n0x%08lx: ", addr + i);

		switch (unit) {
		case 4:
			printf("%08x ", *p4++);
			break;
		case 2:
			printf("%04x ", *p2++);
			break;
		case 1:
			printf("%02x%c", *p1++, (((i+1) % 16) && !((i+1) % 8) && (l - unit))? '-':' ');
			break;
		}
	}
	printf("\n");

	ret = munmap(map, PAGE_SIZE);
	if (ret)
		printf("munmap() return %d errno %d(%s)\n", ret, errno, strerror(errno));

	return 0;
}

int readmem_main(int argc, char **argv)
{
	int len = 128, unit = 4, tmp, opt, ret;
	char *addr_str = "0x18040000";
	unsigned long addr;

	cmdname = argv[0];
	page_size = getpagesize();
	while ((opt = getopt(argc, argv, "l:u:")) != -1) {
		switch (opt) {
		case 'u':
			tmp = atoi(optarg);
			if (tmp == 1 || tmp == 2 || tmp == 4)
				unit = tmp;
			else
				usage();
			break;
		case 'l':
			tmp = atoi(optarg);
			if (tmp > 0 && tmp <= PAGE_SIZE)
				len = tmp;
			else
				usage();
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0)
		addr_str = argv[0];

	addr = strtoul(addr_str, NULL, 16);
	printf("addr: 0x%08lx, length: 0x%x, unit: %d\n", addr, len, unit);
	len = limit_length(addr, len);
	ret = get_var(addr, len, unit);

	return ret;
}
