#define _LARGEFILE64_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#include "libubi.h"
#include "common.h"

struct erase_block_info;
struct volume_info;
struct ubi_device_info;

struct write_info
{
	struct write_info *next;
	struct erase_block_info *erase_block;
	int offset_within_block; /* Offset within erase block */
	off64_t offset; /* Offset within volume */
	int size;
	int random_seed;
};

struct erase_block_info
{
	struct volume_info *volume;
	int block_number;
	off64_t offset; /* Offset within volume */
	off64_t top_of_data;
	int touched; /* Have we done anything at all with this erase block */
	int erased; /* This erased block is currently erased */
	struct write_info *writes;
};

struct volume_fd
{
	struct volume_fd *next;
	struct volume_info *volume;
	int fd;
};

struct volume_info
{
	struct volume_info *next;
	struct ubi_device_info *ubi_device;
	struct volume_fd *fds;
	struct erase_block_info *erase_blocks;
	const char *device_file_name;
	struct ubi_vol_info info;
};

struct ubi_device_info
{
	struct volume_info *volumes;
	const char *device_file_name;
	struct ubi_dev_info info;
};

struct open_volume_fd
{
	struct open_volume_fd *next;
	struct volume_fd *vol_fd;
};

#define MAX_UBI_DEVICES 64

static libubi_t libubi;

static struct ubi_info info;
static struct ubi_device_info ubi_array[MAX_UBI_DEVICES];

static uint64_t total_written = 0;
static uint64_t total_space = 0;

static struct open_volume_fd *open_volumes;
static int open_volume_count = 0;

static const char *ubi_module_load_string;

static unsigned char *write_buffer = NULL;
static unsigned char *read_buffer = NULL;

static long long max_ebs_per_vol = 0; /* max number of ebs per vol (zero => no max) */

static unsigned long next_seed = 1;

static unsigned get_next_seed()
{
	next_seed = next_seed * 1103515245 + 12345;
	return ((unsigned) (next_seed / 65536) % 32768);
}

static void error_exit(const char *msg)
{
	int eno = errno;
	fprintf(stderr,"UBI Integrity Test Error: %s\n",msg);
	if (eno) {
		fprintf(stderr, "errno = %d\n", eno);
		fprintf(stderr, "strerror = %s\n", strerror(eno));
	}
	exit(1);
}

static void *allocate(size_t n)
{
	void *p = malloc(n);
	if (!p)
		error_exit("Memory allocation failure");
	memset(p, 0, n);
	return p;
}

static unsigned get_random_number(unsigned n)
{
	uint64_t r, b;

	if (n < 1)
		return 0;
	r = rand();
	r *= n;
	b = RAND_MAX;
	b += 1;
	r /= b;
	return r;
}

static struct volume_fd *open_volume(struct volume_info *vol)
{
	struct volume_fd *s;
	struct open_volume_fd *ofd;
	int fd;

	if (vol->fds) {
		/* If already open dup it */
		fd = dup(vol->fds->fd);
		if (fd == -1)
			error_exit("Failed to dup volume device file des");
	} else {
		fd = open(vol->device_file_name, O_RDWR | O_LARGEFILE);
		if (fd == -1)
			error_exit("Failed to open volume device file");
	}
	s = allocate(sizeof(*s));
	s->fd = fd;
	s->volume = vol;
	s->next = vol->fds;
	vol->fds = s;
	/* Add to open volumes list */
	ofd = allocate(sizeof(*ofd));
	ofd->vol_fd = s;
	ofd->next = open_volumes;
	open_volumes = ofd;
	open_volume_count += 1;
	return 0;
}

static void close_volume(struct volume_fd *vol_fd)
{
	struct volume_fd *vfd, *vfd_last;
	struct open_volume_fd *ofd, *ofd_last;
	int fd = vol_fd->fd;

	/* Remove from open volumes list */
	ofd_last = NULL;
	ofd = open_volumes;
	while (ofd) {
		if (ofd->vol_fd == vol_fd) {
			if (ofd_last)
				ofd_last->next = ofd->next;
			else
				open_volumes = ofd->next;
			free(ofd);
			open_volume_count -= 1;
			break;
		}
		ofd_last = ofd;
		ofd = ofd->next;
	}
	/* Remove from volume fd list */
	vfd_last = NULL;
	vfd = vol_fd->volume->fds;
	while (vfd) {
		if (vfd == vol_fd) {
			if (vfd_last)
				vfd_last->next = vfd->next;
			else
				vol_fd->volume->fds = vfd->next;
			free(vfd);
			break;
		}
		vfd_last = vfd;
		vfd = vfd->next;
	}
	/* Close volume device file */
	if (close(fd) == -1)
		error_exit("Failed to close volume file descriptor");
}

static void set_random_data(unsigned seed, unsigned char *buf, int size)
{
	int i;
	unsigned r;

	r = rand();
	srand(seed);
	for (i = 0; i < size; ++i)
		buf[i] = rand();
	srand(r);
}

static void check_erase_block(struct erase_block_info *erase_block, int fd)
{
	struct write_info *w;
	off64_t gap_end;
	int leb_size = erase_block->volume->info.leb_size;
	ssize_t bytes_read;

	w = erase_block->writes;
	gap_end = erase_block->offset + leb_size;
	while (w) {
		if (w->offset + w->size < gap_end) {
			/* There is a gap. Check all 0xff */
			off64_t gap_start = w->offset + w->size;
			ssize_t size = gap_end - gap_start;
			if (lseek64(fd, gap_start, SEEK_SET) != gap_start)
				error_exit("lseek64 failed");
			memset(read_buffer, 0 , size);
			errno = 0;
			bytes_read = read(fd, read_buffer, size);
			if (bytes_read != size)
				error_exit("read failed in gap");
			while (size)
				if (read_buffer[--size] != 0xff) {
					fprintf(stderr, "block no. = %d\n" , erase_block->block_number);
					fprintf(stderr, "offset = %lld\n" , (long long) gap_start);
					fprintf(stderr, "size = %ld\n" , (long) bytes_read);
					error_exit("verify 0xff failed");
				}
		}
		if (lseek64(fd, w->offset, SEEK_SET) != w->offset)
			error_exit("lseek64 failed");
		memset(read_buffer, 0 , w->size);
		errno = 0;
		bytes_read = read(fd, read_buffer, w->size);
		if (bytes_read != w->size) {
			fprintf(stderr, "offset = %lld\n" , (long long) w->offset);
			fprintf(stderr, "size = %ld\n" , (long) w->size);
			fprintf(stderr, "bytes_read = %ld\n" , (long) bytes_read);
			error_exit("read failed");
		}
		set_random_data(w->random_seed, write_buffer, w->size);
		if (memcmp(read_buffer, write_buffer, w->size))
			error_exit("verify failed");
		gap_end = w->offset;
		w = w->next;
	}
	if (gap_end > erase_block->offset) {
		/* Check all 0xff */
		off64_t gap_start = erase_block->offset;
		ssize_t size = gap_end - gap_start;
		if (lseek64(fd, gap_start, SEEK_SET) != gap_start)
			error_exit("lseek64 failed");
		memset(read_buffer, 0 , size);
		errno = 0;
		bytes_read = read(fd, read_buffer, size);
		if (bytes_read != size)
			error_exit("read failed in gap");
		while (size)
			if (read_buffer[--size] != 0xff) {
				fprintf(stderr, "block no. = %d\n" , erase_block->block_number);
				fprintf(stderr, "offset = %lld\n" , (long long) gap_start);
				fprintf(stderr, "size = %ld\n" , (long) bytes_read);
				error_exit("verify 0xff failed!");
			}
	}
}

static int write_to_erase_block(struct erase_block_info *erase_block, int fd)
{
	int page_size = erase_block->volume->ubi_device->info.min_io_size;
	int leb_size = erase_block->volume->info.leb_size;
	int next_offset = 0;
	int space, size;
	off64_t offset;
	unsigned seed;
	struct write_info *w;

	if (erase_block->writes)
		next_offset = erase_block->writes->offset_within_block + erase_block->writes->size;
	space = leb_size - next_offset;
	if (space <= 0)
		return 0; /* No space */
	if (!get_random_number(10)) {
		/* 1 time in 10 leave a gap */
		next_offset += get_random_number(space);
		next_offset = (next_offset / page_size) * page_size;
		space = leb_size - next_offset;
	}
	if (get_random_number(2))
		size = 1 * page_size;
	else if (get_random_number(2))
		size = 2 * page_size;
	else if (get_random_number(2))
		size = 3 * page_size;
	else if (get_random_number(2))
		size = 4 * page_size;
	else {
		if (get_random_number(4))
			size = get_random_number(space);
		else
			size = space;
		size = (size / page_size) * page_size;
	}
	if (size == 0 || size > space)
		size = page_size;
	if (next_offset + size > leb_size)
		error_exit("internal error");
	offset = erase_block->offset + next_offset;
	if (offset < erase_block->top_of_data)
		error_exit("internal error!");
	if (lseek64(fd, offset, SEEK_SET) != offset)
		error_exit("lseek64 failed");
	/* Do write */
	seed = get_next_seed();
	if (!seed)
		seed = 1;
	set_random_data(seed, write_buffer, size);
	if (write(fd, write_buffer, size) != size)
		error_exit("write failed");
	erase_block->top_of_data = offset + size;
	/* Make write info and add to eb */
	w = allocate(sizeof(*w));
	w->offset_within_block = next_offset;
	w->offset = offset;
	w->size = size;
	w->random_seed = seed;
	w->next = erase_block->writes;
	erase_block->writes = w;
	erase_block->touched = 1;
	erase_block->erased = 0;
	total_written += size;
	return 1;
}

static void erase_erase_block(struct erase_block_info *erase_block, int fd)
{
	struct write_info *w;
	uint32_t eb_no;
	int res;

	eb_no = erase_block->block_number;
	res = ioctl(fd, UBI_IOCEBER, &eb_no);
	if (res)
		error_exit("Failed to erase an erase block");
	/* Remove writes from this eb */
	while (erase_block->writes) {
		w = erase_block->writes;
		erase_block->writes = erase_block->writes->next;
		free(w);
	}
	erase_block->erased = 1;
	erase_block->touched = 1;
	erase_block->top_of_data = erase_block->offset;
}

static void operate_on_erase_block(struct erase_block_info *erase_block, int fd)
{
	/*
	Possible operations:
		read from it and verify
		write to it
		erase it
	*/
	int work_done = 1;
	static int no_work_done_count = 0;

	if (!get_random_number(10) && no_work_done_count <= 5) {
		check_erase_block(erase_block, fd);
		work_done = 0;
	} else if (get_random_number(100)) {
		if (!write_to_erase_block(erase_block, fd)) {
			/* The erase block was full */
			if (get_random_number(2) || no_work_done_count > 5)
				erase_erase_block(erase_block, fd);
			else
				work_done = 0;
		}
	} else
		erase_erase_block(erase_block, fd);
	if (work_done)
		no_work_done_count = 0;
	else
		no_work_done_count += 1;
}

static void operate_on_open_volume(struct volume_fd *vol_fd)
{
	/*
	Possible operations:
		operate on an erase block
		close volume
	*/
	if (get_random_number(100) == 0)
		close_volume(vol_fd);
	else {
		/* Pick an erase block at random */
		int eb_no = get_random_number(vol_fd->volume->info.rsvd_lebs);
		operate_on_erase_block(&vol_fd->volume->erase_blocks[eb_no], vol_fd->fd);
	}
}

static void operate_on_volume(struct volume_info *vol)
{
	/*
	Possible operations:
		open it
		resize it (must close fd's first) <- TODO
		delete it (must close fd's first) <- TODO
	*/
	open_volume(vol);
}

static int ubi_major(const char *device_file_name)
{
	struct stat buf;
	static int maj = 0;

	if (maj)
		return maj;
	if (stat(device_file_name, &buf) == -1)
		error_exit("Failed to stat ubi device file");
	maj = major(buf.st_rdev);
	return maj;
}

static void operate_on_ubi_device(struct ubi_device_info *ubi_device)
{
	/*
	TODO:
	Possible operations:
		create a new volume
		operate on existing volume
	*/
	/*
	Simplified operation (i.e. only have 1 volume):
		If there are no volumes create 1 volumne
		Then operate on the volume
	*/
	if (ubi_device->info.vol_count == 0) {
		/* Create the one-and-only volume we will use */
		char dev_name[1024];
		int i, n, maj, fd;
		struct volume_info *s;
		struct ubi_mkvol_request req;

		req.vol_id = UBI_VOL_NUM_AUTO;
		req.alignment = 1; /* TODO: What is this? */
		req.bytes = ubi_device->info.leb_size * max_ebs_per_vol;
		if (req.bytes == 0 || req.bytes > ubi_device->info.avail_bytes)
			req.bytes = ubi_device->info.avail_bytes;
		req.vol_type = UBI_DYNAMIC_VOLUME;
		req.name = "integ-test-vol";
		if (ubi_mkvol(libubi, ubi_device->device_file_name, &req))
			error_exit("ubi_mkvol failed");
		s = allocate(sizeof(*s));
		s->ubi_device = ubi_device;
		if (ubi_get_vol_info1(libubi, ubi_device->info.dev_num, req.vol_id, &s->info))
			error_exit("ubi_get_vol_info failed");
		n = s->info.rsvd_lebs;
		s->erase_blocks = allocate(sizeof(struct erase_block_info) * n);
		for (i = 0; i < n; ++i) {
			s->erase_blocks[i].volume = s;
			s->erase_blocks[i].block_number = i;
			s->erase_blocks[i].offset = i * (off64_t) s->info.leb_size;
			s->erase_blocks[i].top_of_data = s->erase_blocks[i].offset;
		}
		/* FIXME: Correctly get device file name */
		sprintf(dev_name, "%s_%d", ubi_device->device_file_name, req.vol_id);
		s->device_file_name = strdup(dev_name);
		ubi_device->volumes = s;
		ubi_device->info.vol_count += 1;
		sleep(1);
		fd = open(s->device_file_name, O_RDONLY);
		if (fd == -1) {
			/* FIXME: Correctly make node */
			maj = ubi_major(ubi_device->device_file_name);
			sprintf(dev_name, "mknod %s c %d %d", s->device_file_name, maj, req.vol_id + 1);
			system(dev_name);
		} else if (close(fd) == -1)
			error_exit("Failed to close volume device file");
	}
	operate_on_volume(ubi_device->volumes);
}

static void do_an_operation(void)
{
	int too_few = (open_volume_count < info.dev_count * 3);
	int too_many = (open_volume_count > info.dev_count * 5);

	if (too_many || (!too_few && get_random_number(1000) > 0)) {
		/* Operate on an open volume */
		size_t pos;
		struct open_volume_fd *ofd;
		pos = get_random_number(open_volume_count);
		for (ofd = open_volumes; pos && ofd && ofd->next; --pos)
			ofd = ofd->next;
		operate_on_open_volume(ofd->vol_fd);
	} else if (info.dev_count > 0) {
		/* Operate on a ubi device */
		size_t ubi_pos = 0;
		if (info.dev_count > 1)
			ubi_pos = get_random_number(info.dev_count - 1);
		operate_on_ubi_device(&ubi_array[ubi_pos]);
	} else
		error_exit("Internal error");
}

static void get_ubi_devices_info(void)
{
	int i, ubi_pos = 0;
	char dev_name[1024];
	ssize_t buf_size = 1024 * 128;

	if (ubi_get_info(libubi, &info))
		error_exit("ubi_get_info failed");
	if (info.dev_count > MAX_UBI_DEVICES)
		error_exit("Too many ubi devices");
	for (i = info.lowest_dev_num; i <= info.highest_dev_num; ++i) {
		struct ubi_device_info *s;
		s = &ubi_array[ubi_pos++];
		if (ubi_get_dev_info1(libubi, i, &s->info))
			error_exit("ubi_get_dev_info1 failed");
		if (s->info.vol_count)
			error_exit("There are existing volumes");
		/* FIXME: Correctly get device file name */
		sprintf(dev_name, "/dev/ubi%d", i);
		s->device_file_name = strdup(dev_name);
		if (buf_size < s->info.leb_size)
			buf_size = s->info.leb_size;
		if (max_ebs_per_vol && s->info.leb_size * max_ebs_per_vol < s->info.avail_bytes)
			total_space += s->info.leb_size * max_ebs_per_vol;
		else
			total_space += s->info.avail_bytes;
	}
	write_buffer = allocate(buf_size);
	read_buffer = allocate(buf_size);
}

static void load_ubi(void)
{
	system("rmmod ubi");
	if (system(ubi_module_load_string) != 0)
		error_exit("Failed to load UBI module");
	sleep(1);
}

static void do_some_operations(void)
{
	unsigned i = 0;
	total_written = 0;
	printf("Total space: %llu\n", (unsigned long long) total_space);
	while (total_written < total_space * 3) {
		do_an_operation();
		if (i++ % 10000 == 0)
			printf("Total written: %llu\n", (unsigned long long) total_written);
	}
	printf("Total written: %llu\n", (unsigned long long) total_written);
}

static void reload_ubi(void)
{
	/* Remove module */
	if (system("rmmod ubi") != 0)
		error_exit("Failed to remove UBI module");
	/* Install module */
	if (system(ubi_module_load_string) != 0)
		error_exit("Failed to load UBI module");
	sleep(1);
}

static void integ_check_volume(struct volume_info *vol)
{
	struct erase_block_info *eb = vol->erase_blocks;
	int pos;
	int fd;

	fd = open(vol->device_file_name, O_RDWR | O_LARGEFILE);
	if (fd == -1)
		error_exit("Failed to open volume device file");
	for (pos = 0; pos < vol->info.rsvd_lebs; ++pos)
		check_erase_block(eb++, fd);
	if (close(fd) == -1)
		error_exit("Failed to close volume device file");
}

static void check_ubi_device(struct ubi_device_info *ubi_device)
{
	struct volume_info *vol;

	vol = ubi_device->volumes;
	while (vol) {
		integ_check_volume(vol);
		vol = vol->next;
	}
}

static void check_ubi(void)
{
	int i;

	for (i = 0; i < info.dev_count; ++i)
		check_ubi_device(&ubi_array[i]);
}

static int is_all_digits(const char *s)
{
	const char *digits = "0123456789";
	if (!s || !*s)
		return 0;
	for (;*s;++s)
		if (!strchr(digits,*s))
			return 0;
	return 1;
}

static int get_short_arg(int *pos,const char *name,long long *result,int argc,char *argv[])
{
	const char *p = NULL;
	int i = *pos;
	size_t n = strlen(name);

	if (strlen(argv[i]) > n)
		p = argv[i] + n;
	else if (++i < argc)
		p = argv[i];
	if (!is_all_digits(p))
		return 1;
	*result = atoll(p);
	*pos = i;
	return 0;
}

static int get_long_arg(int *pos,const char *name,long long *result,int argc,char *argv[])
{
	const char *p = NULL;
	int i = *pos;
	size_t n = strlen(name);

	if (strlen(argv[i]) > n)
		p = argv[i] + n;
	else if (++i < argc)
		p = argv[i];
	if (p && *p == '=') {
		p += 1;
		if (!*p && ++i < argc)
			p = argv[i];
	}
	if (!is_all_digits(p))
		return 1;
	*result = atoll(p);
	*pos = i;
	return 0;
}

static int remove_all_volumes(void)
{
	int i;

	for (i = 0; i < info.dev_count; ++i) {
		struct ubi_device_info *ubi_device = &ubi_array[i];
		struct volume_info *vol;
		vol = ubi_device->volumes;
		while (vol) {
			int res = ubi_rmvol(libubi,
					    ubi_device->device_file_name,
					    vol->info.vol_id);
			if (res)
				return res;
			vol = vol->next;
		}
	}
	return 0;
}

int main(int argc,char *argv[])
{
	int i;
	long long r, repeat = 1;
	int initial_seed = 1, args_ok = 1;

	printf("UBI Integrity Test\n");

	/* Get arguments */
	ubi_module_load_string = 0;
	for (i = 1; i < argc; ++i) {
		if (strncmp(argv[i], "-h", 2) == 0)
			args_ok = 0;
		else if (strncmp(argv[i], "--help", 6) == 0)
			args_ok = 0;
		else if (strncmp(argv[i], "-n", 2) == 0) {
			if (get_short_arg(&i, "-n", &repeat, argc, argv))
				args_ok = 0;
		} else if (strncmp(argv[i], "--repeat", 8) == 0) {
			if (get_long_arg(&i, "--repeat", &repeat, argc, argv))
				args_ok = 0;
		} else if (strncmp(argv[i], "-m", 2) == 0) {
			if (get_short_arg(&i,"-m", &max_ebs_per_vol, argc, argv))
				args_ok = 0;
		} else if (strncmp(argv[i], "--maxebs", 8) == 0) {
			if (get_long_arg(&i, "--maxebs", &max_ebs_per_vol, argc, argv))
				args_ok = 0;
		} else if (!ubi_module_load_string)
			ubi_module_load_string = argv[i];
		else
			args_ok = 0;
	}
	if (!args_ok || !ubi_module_load_string) {
		fprintf(stderr, "Usage is: ubi_integ [<options>] <UBI Module load command>\n");
		fprintf(stderr, "    Options: \n");
		fprintf(stderr, "        -h, --help              Help\n");
		fprintf(stderr, "        -n arg, --repeat=arg    Repeat test arg times\n");
		fprintf(stderr, "        -m arg, --maxebs=arg    Max no. of erase blocks\n");
		return 1;
	}

	next_seed = initial_seed = seed_random_generator();
	printf("Initial seed = %u\n", (unsigned) initial_seed);
	load_ubi();

	libubi = libubi_open();
	if (!libubi)
		error_exit("Failed to open libubi");

	get_ubi_devices_info();

	r = 0;
	while (repeat == 0 || r++ < repeat) {
		printf("Cycle %lld\n", r);
		do_some_operations();

		/* Close all volumes */
		while (open_volumes)
			close_volume(open_volumes->vol_fd);

		check_ubi();

		libubi_close(libubi);

		reload_ubi();

		libubi = libubi_open();
		if (!libubi)
			error_exit("Failed to open libubi");

		check_ubi();
	}

	if (remove_all_volumes())
		error_exit("Failed to remove all volumes");

	libubi_close(libubi);

	printf("UBI Integrity Test completed ok\n");
	return 0;
}
