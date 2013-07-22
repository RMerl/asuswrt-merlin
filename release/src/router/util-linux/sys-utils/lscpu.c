/*
 * lscpu - CPU architecture information helper
 *
 * Copyright (C) 2008 Cai Qian <qcai@redhat.com>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cpuset.h"
#include "nls.h"
#include "xalloc.h"
#include "c.h"
#include "strutils.h"
#include "bitops.h"


#define CACHE_MAX 100

/* /sys paths */
#define _PATH_SYS_SYSTEM	"/sys/devices/system"
#define _PATH_SYS_CPU		_PATH_SYS_SYSTEM "/cpu"
#define _PATH_PROC_XEN		"/proc/xen"
#define _PATH_PROC_XENCAP	_PATH_PROC_XEN "/capabilities"
#define _PATH_PROC_CPUINFO	"/proc/cpuinfo"
#define _PATH_PROC_PCIDEVS	"/proc/bus/pci/devices"

/* virtualization types */
enum {
	VIRT_NONE	= 0,
	VIRT_PARA,
	VIRT_FULL
};
const char *virt_types[] = {
	[VIRT_NONE]	= N_("none"),
	[VIRT_PARA]	= N_("para"),
	[VIRT_FULL]	= N_("full")
};

/* hypervisor vendors */
enum {
	HYPER_NONE	= 0,
	HYPER_XEN,
	HYPER_KVM,
	HYPER_MSHV,
	HYPER_VMWARE
};
const char *hv_vendors[] = {
	[HYPER_NONE]	= NULL,
	[HYPER_XEN]	= "Xen",
	[HYPER_KVM]	= "KVM",
	[HYPER_MSHV]	= "Microsoft",
	[HYPER_VMWARE]  = "VMware"
};

/* CPU modes */
enum {
	MODE_32BIT	= (1 << 1),
	MODE_64BIT	= (1 << 2)
};

/* cache(s) description */
struct cpu_cache {
	char		*name;
	char		*size;

	int		nsharedmaps;
	cpu_set_t	**sharedmaps;
};

/* global description */
struct lscpu_desc {
	char	*arch;
	char	*vendor;
	char	*family;
	char	*model;
	char	*virtflag;	/* virtualization flag (vmx, svm) */
	int	hyper;		/* hypervisor vendor ID */
	int	virtype;	/* VIRT_PARA|FULL|NONE ? */
	char	*mhz;
	char	*stepping;
	char    *bogomips;
	char	*flags;
	int	mode;		/* rm, lm or/and tm */

	int		ncpus;		/* number of CPUs */
	cpu_set_t	*online;	/* mask with online CPUs */

	int		nnodes;		/* number of NUMA modes */
	cpu_set_t	**nodemaps;	/* array with NUMA nodes */

	/* books -- based on book_siblings (internal kernel map of cpuX's
	 * hardware threads within the same book */
	int		nbooks;		/* number of all online books */
	cpu_set_t	**bookmaps;	/* unique book_siblings */

	/* sockets -- based on core_siblings (internal kernel map of cpuX's
	 * hardware threads within the same physical_package_id (socket)) */
	int		nsockets;	/* number of all online sockets */
	cpu_set_t	**socketmaps;	/* unique core_siblings */

	/* cores -- based on thread_siblings (internel kernel map of cpuX's
	 * hardware threads within the same core as cpuX) */
	int		ncores;		/* number of all online cores */
	cpu_set_t	**coremaps;	/* unique thread_siblings */

	int		nthreads;	/* number of online threads */

	int		ncaches;
	struct cpu_cache *caches;
};

static size_t sysrootlen;
static char pathbuf[PATH_MAX];
static int maxcpus;		/* size in bits of kernel cpu mask */

#define is_cpu_online(_d, _cpu) \
		((_d) && (_d)->online ? \
			CPU_ISSET_S((_cpu), CPU_ALLOC_SIZE(maxcpus), (_d)->online) : 0)

static FILE *path_fopen(const char *mode, int exit_on_err, const char *path, ...)
		__attribute__ ((__format__ (__printf__, 3, 4)));
static void path_getstr(char *result, size_t len, const char *path, ...)
		__attribute__ ((__format__ (__printf__, 3, 4)));
static int path_getnum(const char *path, ...)
		__attribute__ ((__format__ (__printf__, 1, 2)));
static int path_exist(const char *path, ...)
		__attribute__ ((__format__ (__printf__, 1, 2)));
static cpu_set_t *path_cpuset(const char *path, ...)
		__attribute__ ((__format__ (__printf__, 1, 2)));

/*
 * Parsable output
 */
enum {
	COL_CPU,
	COL_CORE,
	COL_SOCKET,
	COL_NODE,
	COL_BOOK,
	COL_CACHE
};

static const char *colnames[] =
{
	[COL_CPU] = "CPU",
	[COL_CORE] = "Core",
	[COL_SOCKET] = "Socket",
	[COL_NODE] = "Node",
	[COL_BOOK] = "Book",
	[COL_CACHE] = "Cache"
};


static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(colnames); i++) {
		const char *cn = colnames[i];

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static const char *
path_vcreate(const char *path, va_list ap)
{
	if (sysrootlen)
		vsnprintf(pathbuf + sysrootlen,
			  sizeof(pathbuf) - sysrootlen, path, ap);
	else
		vsnprintf(pathbuf, sizeof(pathbuf), path, ap);
	return pathbuf;
}

static FILE *
path_vfopen(const char *mode, int exit_on_error, const char *path, va_list ap)
{
	FILE *f;
	const char *p = path_vcreate(path, ap);

	f = fopen(p, mode);
	if (!f && exit_on_error)
		err(EXIT_FAILURE, _("error: cannot open %s"), p);
	return f;
}

static FILE *
path_fopen(const char *mode, int exit_on_error, const char *path, ...)
{
	FILE *fd;
	va_list ap;

	va_start(ap, path);
	fd = path_vfopen(mode, exit_on_error, path, ap);
	va_end(ap);

	return fd;
}

static void
path_getstr(char *result, size_t len, const char *path, ...)
{
	FILE *fd;
	va_list ap;

	va_start(ap, path);
	fd = path_vfopen("r", 1, path, ap);
	va_end(ap);

	if (!fgets(result, len, fd))
		err(EXIT_FAILURE, _("failed to read: %s"), pathbuf);
	fclose(fd);

	len = strlen(result);
	if (result[len - 1] == '\n')
		result[len - 1] = '\0';
}

static int
path_getnum(const char *path, ...)
{
	FILE *fd;
	va_list ap;
	int result;

	va_start(ap, path);
	fd = path_vfopen("r", 1, path, ap);
	va_end(ap);

	if (fscanf(fd, "%d", &result) != 1) {
		if (ferror(fd))
			err(EXIT_FAILURE, _("failed to read: %s"), pathbuf);
		else
			errx(EXIT_FAILURE, _("parse error: %s"), pathbuf);
	}
	fclose(fd);
	return result;
}

static int
path_exist(const char *path, ...)
{
	va_list ap;
	const char *p;

	va_start(ap, path);
	p = path_vcreate(path, ap);
	va_end(ap);

	return access(p, F_OK) == 0;
}

static cpu_set_t *
path_cpuparse(int islist, const char *path, va_list ap)
{
	FILE *fd;
	cpu_set_t *set;
	size_t setsize, len = maxcpus * 7;
	char buf[len];

	fd = path_vfopen("r", 1, path, ap);

	if (!fgets(buf, len, fd))
		err(EXIT_FAILURE, _("failed to read: %s"), pathbuf);
	fclose(fd);

	len = strlen(buf);
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	set = cpuset_alloc(maxcpus, &setsize, NULL);
	if (!set)
		err(EXIT_FAILURE, _("failed to callocate cpu set"));

	if (islist) {
		if (cpulist_parse(buf, set, setsize))
			errx(EXIT_FAILURE, _("failed to parse CPU list %s"), buf);
	} else {
		if (cpumask_parse(buf, set, setsize))
			errx(EXIT_FAILURE, _("failed to parse CPU mask %s"), buf);
	}
	return set;
}

static cpu_set_t *
path_cpuset(const char *path, ...)
{
	va_list ap;
	cpu_set_t *set;

	va_start(ap, path);
	set = path_cpuparse(0, path, ap);
	va_end(ap);

	return set;
}

static cpu_set_t *
path_cpulist(const char *path, ...)
{
	va_list ap;
	cpu_set_t *set;

	va_start(ap, path);
	set = path_cpuparse(1, path, ap);
	va_end(ap);

	return set;
}

/* Lookup a pattern and get the value from cpuinfo.
 * Format is:
 *
 *	"<pattern>   : <key>"
 */
int lookup(char *line, char *pattern, char **value)
{
	char *p, *v;
	int len = strlen(pattern);

	if (!*line)
		return 0;

	/* pattern */
	if (strncmp(line, pattern, len))
		return 0;

	/* white spaces */
	for (p = line + len; isspace(*p); p++);

	/* separator */
	if (*p != ':')
		return 0;

	/* white spaces */
	for (++p; isspace(*p); p++);

	/* value */
	if (!*p)
		return 0;
	v = p;

	/* end of value */
	len = strlen(line) - 1;
	for (p = line + len; isspace(*(p-1)); p--);
	*p = '\0';

	*value = xstrdup(v);
	return 1;
}

/* Don't init the mode for platforms where we are not able to
 * detect that CPU supports 64-bit mode.
 */
static int
init_mode(void)
{
	int m = 0;

	if (sysrootlen)
		/* reading info from any /{sys,proc} dump, don't mix it with
		 * information about our real CPU */
		return 0;

#if defined(__alpha__) || defined(__ia64__)
	m |= MODE_64BIT;	/* 64bit platforms only */
#endif
	/* platforms with 64bit flag in /proc/cpuinfo, define
	 * 32bit default here */
#if defined(__i386__) || defined(__x86_64__) || \
    defined(__s390x__) || defined(__s390__) || defined(__sparc_v9__)
	m |= MODE_32BIT;
#endif
	return m;
}

static void
read_basicinfo(struct lscpu_desc *desc)
{
	FILE *fp = path_fopen("r", 1, _PATH_PROC_CPUINFO);
	char buf[BUFSIZ];
	struct utsname utsbuf;

	/* architecture */
	if (uname(&utsbuf) == -1)
		err(EXIT_FAILURE, _("error: uname failed"));
	desc->arch = xstrdup(utsbuf.machine);

	/* count CPU(s) */
	while(path_exist(_PATH_SYS_SYSTEM "/cpu/cpu%d", desc->ncpus))
		desc->ncpus++;

	/* details */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (lookup(buf, "vendor", &desc->vendor)) ;
		else if (lookup(buf, "vendor_id", &desc->vendor)) ;
		else if (lookup(buf, "family", &desc->family)) ;
		else if (lookup(buf, "cpu family", &desc->family)) ;
		else if (lookup(buf, "model", &desc->model)) ;
		else if (lookup(buf, "stepping", &desc->stepping)) ;
		else if (lookup(buf, "cpu MHz", &desc->mhz)) ;
		else if (lookup(buf, "flags", &desc->flags)) ;		/* x86 */
		else if (lookup(buf, "features", &desc->flags)) ;	/* s390 */
		else if (lookup(buf, "type", &desc->flags)) ;		/* sparc64 */
		else if (lookup(buf, "bogomips", &desc->bogomips)) ;
		/* S390 */
		else if (lookup(buf, "bogomips per cpu", &desc->bogomips)) ;
		else
			continue;
	}

	desc->mode = init_mode();

	if (desc->flags) {
		snprintf(buf, sizeof(buf), " %s ", desc->flags);
		if (strstr(buf, " svm "))
			desc->virtflag = strdup("svm");
		else if (strstr(buf, " vmx "))
			desc->virtflag = strdup("vmx");
		if (strstr(buf, " lm "))
			desc->mode |= MODE_32BIT | MODE_64BIT;		/* x86_64 */
		if (strstr(buf, " zarch "))
			desc->mode |= MODE_32BIT | MODE_64BIT;		/* s390x */
		if (strstr(buf, " sun4v ") || strstr(buf, " sun4u "))
			desc->mode |= MODE_32BIT | MODE_64BIT;		/* sparc64 */
	}

	fclose(fp);

	if (path_exist(_PATH_SYS_SYSTEM "/cpu/kernel_max"))
		/* note that kernel_max is maximum index [NR_CPUS-1] */
		maxcpus = path_getnum(_PATH_SYS_SYSTEM "/cpu/kernel_max") + 1;

	else if (!sysrootlen)
		/* the root is '/' so we are working with data from the current kernel */
		maxcpus = get_max_number_of_cpus();
	else
		/* we are reading some /sys snapshot instead of the real /sys,
		 * let's use any crazy number... */
		maxcpus = desc->ncpus > 2048 ? desc->ncpus : 2048;

	/* get mask for online CPUs */
	if (path_exist(_PATH_SYS_SYSTEM "/cpu/online")) {
		size_t setsize = CPU_ALLOC_SIZE(maxcpus);
		desc->online = path_cpulist(_PATH_SYS_SYSTEM "/cpu/online");
		desc->nthreads = CPU_COUNT_S(setsize, desc->online);
	}
}

static int
has_pci_device(int vendor, int device)
{
	FILE *f;
	int num, fn, ven, dev;
	int res = 1;

	f = path_fopen("r", 0, _PATH_PROC_PCIDEVS);
	if (!f)
		return 0;

	 /* for more details about bus/pci/devices format see
	  * drivers/pci/proc.c in linux kernel
	  */
	while(fscanf(f, "%02x%02x\t%04x%04x\t%*[^\n]",
			&num, &fn, &ven, &dev) == 4) {

		if (ven == vendor && dev == device)
			goto found;
	}

	res = 0;
found:
	fclose(f);
	return res;
}

#if defined(__x86_64__) || defined(__i386__)

/*
 * This CPUID leaf returns the information about the hypervisor.
 * EAX : maximum input value for CPUID supported by the hypervisor.
 * EBX, ECX, EDX : Hypervisor vendor ID signature. E.g. VMwareVMware.
 */
#define HYPERVISOR_INFO_LEAF   0x40000000

static inline void
cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx,
			 unsigned int *ecx, unsigned int *edx)
{
	__asm__(
#if defined(__PIC__) && defined(__i386__)
		/* x86 PIC cannot clobber ebx -- gcc bitches */
		"pushl %%ebx;"
		"cpuid;"
		"movl %%ebx, %%esi;"
		"popl %%ebx;"
		: "=S" (*ebx),
#else
		"cpuid;"
		: "=b" (*ebx),
#endif
		  "=a" (*eax),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "1" (op), "c"(0));
}

static void
read_hypervisor_cpuid(struct lscpu_desc *desc)
{
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
	char hyper_vendor_id[13];

	memset(hyper_vendor_id, 0, sizeof(hyper_vendor_id));

	cpuid(HYPERVISOR_INFO_LEAF, &eax, &ebx, &ecx, &edx);
	memcpy(hyper_vendor_id + 0, &ebx, 4);
	memcpy(hyper_vendor_id + 4, &ecx, 4);
	memcpy(hyper_vendor_id + 8, &edx, 4);
	hyper_vendor_id[12] = '\0';

	if (!hyper_vendor_id[0])
		return;

	if (!strncmp("XenVMMXenVMM", hyper_vendor_id, 12))
		desc->hyper = HYPER_XEN;
	else if (!strncmp("KVMKVMKVM", hyper_vendor_id, 9))
		desc->hyper = HYPER_KVM;
	else if (!strncmp("Microsoft Hv", hyper_vendor_id, 12))
		desc->hyper = HYPER_MSHV;
	else if (!strncmp("VMwareVMware", hyper_vendor_id, 12))
		desc->hyper = HYPER_VMWARE;
}

#else	/* ! __x86_64__ */
static void
read_hypervisor_cpuid(struct lscpu_desc *desc)
{
}
#endif

static void
read_hypervisor(struct lscpu_desc *desc)
{
	read_hypervisor_cpuid(desc);

	if (desc->hyper)
		/* hvm */
		desc->virtype = VIRT_FULL;

	else if (path_exist(_PATH_PROC_XEN)) {
		/* Xen para-virt or dom0 */
		FILE *fd = path_fopen("r", 0, _PATH_PROC_XENCAP);
		int dom0 = 0;

		if (fd) {
			char buf[256];

			if (fscanf(fd, "%s", buf) == 1 &&
			    !strcmp(buf, "control_d"))
				dom0 = 1;
			fclose(fd);
		}
		desc->virtype = dom0 ? VIRT_NONE : VIRT_PARA;
		desc->hyper = HYPER_XEN;

	} else if (has_pci_device(0x5853, 0x0001)) {
		/* Xen full-virt on non-x86_64 */
		desc->hyper = HYPER_XEN;
		desc->virtype = VIRT_FULL;
	}
}

/* add @set to the @ary, unnecesary set is deallocated. */
static int add_cpuset_to_array(cpu_set_t **ary, int *items, cpu_set_t *set)
{
	int i;
	size_t setsize = CPU_ALLOC_SIZE(maxcpus);

	if (!ary)
		return -1;

	for (i = 0; i < *items; i++) {
		if (CPU_EQUAL_S(setsize, set, ary[i]))
			break;
	}
	if (i == *items) {
		ary[*items] = set;
		++*items;
		return 0;
	}
	CPU_FREE(set);
	return 1;
}

static void
read_topology(struct lscpu_desc *desc, int num)
{
	cpu_set_t *thread_siblings, *core_siblings, *book_siblings;

	if (!path_exist(_PATH_SYS_CPU "/cpu%d/topology/thread_siblings", num))
		return;

	thread_siblings = path_cpuset(_PATH_SYS_CPU
					"/cpu%d/topology/thread_siblings", num);
	core_siblings = path_cpuset(_PATH_SYS_CPU
					"/cpu%d/topology/core_siblings", num);
	book_siblings = NULL;
	if (path_exist(_PATH_SYS_CPU "/cpu%d/topology/book_siblings", num)) {
		book_siblings = path_cpuset(_PATH_SYS_CPU
					    "/cpu%d/topology/book_siblings", num);
	}

	if (!desc->coremaps) {
		int nbooks, nsockets, ncores, nthreads;
		size_t setsize = CPU_ALLOC_SIZE(maxcpus);

		/* threads within one core */
		nthreads = CPU_COUNT_S(setsize, thread_siblings);
		/* cores within one socket */
		ncores = CPU_COUNT_S(setsize, core_siblings) / nthreads;
		/* number of sockets within one book */
		nsockets = desc->ncpus / nthreads / ncores;
		/* number of books */
		nbooks = desc->ncpus / nthreads / ncores / nsockets;

		/* all threads, see also read_basicinfo()
		 * -- this is fallback for kernels where is not
		 *    /sys/devices/system/cpu/online.
		 */
		if (!desc->nthreads)
			desc->nthreads = nsockets * ncores * nthreads;
		if (book_siblings)
			desc->bookmaps = xcalloc(nbooks, sizeof(cpu_set_t *));

		desc->socketmaps = xcalloc(nsockets, sizeof(cpu_set_t *));
		desc->coremaps = xcalloc(ncores * nsockets, sizeof(cpu_set_t *));
	}

	add_cpuset_to_array(desc->socketmaps, &desc->nsockets, core_siblings);
	add_cpuset_to_array(desc->coremaps, &desc->ncores, thread_siblings);
	if (book_siblings)
		add_cpuset_to_array(desc->bookmaps, &desc->nbooks, book_siblings);
}

static int
cachecmp(const void *a, const void *b)
{
	struct cpu_cache *c1 = (struct cpu_cache *) a;
	struct cpu_cache *c2 = (struct cpu_cache *) b;

	return strcmp(c2->name, c1->name);
}

static void
read_cache(struct lscpu_desc *desc, int num)
{
	char buf[256];
	int i;

	if (!desc->ncaches) {
		while(path_exist(_PATH_SYS_SYSTEM "/cpu/cpu%d/cache/index%d",
					num, desc->ncaches))
			desc->ncaches++;

		if (!desc->ncaches)
			return;

		desc->caches = xcalloc(desc->ncaches, sizeof(*desc->caches));
	}
	for (i = 0; i < desc->ncaches; i++) {
		struct cpu_cache *ca = &desc->caches[i];
		cpu_set_t *map;

		if (!ca->name) {
			int type, level;

			/* cache type */
			path_getstr(buf, sizeof(buf),
					_PATH_SYS_CPU "/cpu%d/cache/index%d/type",
					num, i);
			if (!strcmp(buf, "Data"))
				type = 'd';
			else if (!strcmp(buf, "Instruction"))
				type = 'i';
			else
				type = 0;

			/* cache level */
			level = path_getnum(_PATH_SYS_CPU "/cpu%d/cache/index%d/level",
					num, i);
			if (type)
				snprintf(buf, sizeof(buf), "L%d%c", level, type);
			else
				snprintf(buf, sizeof(buf), "L%d", level);

			ca->name = xstrdup(buf);

			/* cache size */
			path_getstr(buf, sizeof(buf),
					_PATH_SYS_CPU "/cpu%d/cache/index%d/size",
					num, i);
			ca->size = xstrdup(buf);
		}

		/* information about how CPUs share different caches */
		map = path_cpuset(_PATH_SYS_CPU "/cpu%d/cache/index%d/shared_cpu_map",
				num, i);

		if (!ca->sharedmaps)
			ca->sharedmaps = xcalloc(desc->ncpus, sizeof(cpu_set_t *));
		add_cpuset_to_array(ca->sharedmaps, &ca->nsharedmaps, map);
	}
}

static void
read_nodes(struct lscpu_desc *desc)
{
	int i;

	/* number of NUMA node */
	while (path_exist(_PATH_SYS_SYSTEM "/node/node%d", desc->nnodes))
		desc->nnodes++;

	if (!desc->nnodes)
		return;

	desc->nodemaps = xcalloc(desc->nnodes, sizeof(cpu_set_t *));

	/* information about how nodes share different CPUs */
	for (i = 0; i < desc->nnodes; i++)
		desc->nodemaps[i] = path_cpuset(
					_PATH_SYS_SYSTEM "/node/node%d/cpumap",
					i);
}

static void
print_parsable_cell(struct lscpu_desc *desc, int i, int col, int compatible)
{
	int j;
	size_t setsize = CPU_ALLOC_SIZE(maxcpus);

	switch (col) {
	case COL_CPU:
		printf("%d", i);
		break;
	case COL_CORE:
		for (j = 0; j < desc->ncores; j++) {
			if (CPU_ISSET_S(i, setsize, desc->coremaps[j])) {
				printf("%d", j);
				break;
			}
		}
		break;
	case COL_SOCKET:
		for (j = 0; j < desc->nsockets; j++) {
			if (CPU_ISSET_S(i, setsize, desc->socketmaps[j])) {
				printf("%d", j);
				break;
			}
		}
		break;
	case COL_NODE:
		for (j = 0; j < desc->nnodes; j++) {
			if (CPU_ISSET_S(i, setsize, desc->nodemaps[j])) {
				printf("%d", j);
				break;
			}
		}
		break;
	case COL_BOOK:
		for (j = 0; j < desc->nbooks; j++) {
			if (CPU_ISSET_S(i, setsize, desc->bookmaps[j])) {
				printf("%d", j);
				break;
			}
		}
		break;
	case COL_CACHE:
		for (j = desc->ncaches - 1; j >= 0; j--) {
			struct cpu_cache *ca = &desc->caches[j];
			int x;

			for (x = 0; x < ca->nsharedmaps; x++) {
				if (CPU_ISSET_S(i, setsize, ca->sharedmaps[x])) {
					if (j != desc->ncaches - 1)
						putchar(compatible ? ',' : ':');
					printf("%d", x);
					break;
				}
			}
			if (x == ca->nsharedmaps)
				putchar(',');
		}
		break;
	}
}

/*
 * We support two formats:
 *
 * 1) "compatible" -- this format is compatible with the original lscpu(1)
 * output and it contains fixed set of the columns. The CACHE columns are at
 * the end of the line and the CACHE is not printed if the number of the caches
 * is zero. The CACHE columns are separated by two commas, for example:
 *
 *    $ lscpu --parse
 *    # CPU,Core,Socket,Node,,L1d,L1i,L2
 *    0,0,0,0,,0,0,0
 *    1,1,0,0,,1,1,0
 *
 * 2) "user defined output" -- this format prints always all columns without
 * special prefix for CACHE column. If there are not CACHEs then the column is
 * empty and the header "Cache" is printed rather than a real name of the cache.
 * The CACHE columns are separated by ':'.
 *
 *	$ lscpu --parse=CPU,CORE,SOCKET,NODE,CACHE
 *	# CPU,Core,Socket,Node,L1d:L1i:L2
 *	0,0,0,0,0:0:0
 *	1,1,0,0,1:1:0
 */
static void
print_parsable(struct lscpu_desc *desc, int cols[], int ncols, int compatible)
{
	int i, c;

	printf(_(
	"# The following is the parsable format, which can be fed to other\n"
	"# programs. Each different item in every column has an unique ID\n"
	"# starting from zero.\n"));

	fputs("# ", stdout);
	for (i = 0; i < ncols; i++) {
		if (cols[i] == COL_CACHE) {
			if (compatible && !desc->ncaches)
				continue;
			if (i > 0)
				putchar(',');
			if (compatible && i != 0)
				putchar(',');
			for (c = desc->ncaches - 1; c >= 0; c--) {
				printf("%s", desc->caches[c].name);
				if (c > 0)
					putchar(compatible ? ',' : ':');
			}
			if (!desc->ncaches)
				fputs(colnames[cols[i]], stdout);
		} else {
			if (i > 0)
				putchar(',');
			fputs(colnames[cols[i]], stdout);
		}
	}
	putchar('\n');

	for (i = 0; i < desc->ncpus; i++) {
		if (desc->online && !is_cpu_online(desc, i))
			continue;
		for (c = 0; c < ncols; c++) {
			if (compatible && cols[c] == COL_CACHE) {
				if (!desc->ncaches)
					continue;
				if (c > 0)
					putchar(',');
			}
			if (c > 0)
				putchar(',');
			print_parsable_cell(desc, i, cols[c], compatible);
		}
		putchar('\n');
	}
}


/* output formats "<key>  <value>"*/
#define print_s(_key, _val)	printf("%-23s%s\n", _key, _val)
#define print_n(_key, _val)	printf("%-23s%d\n", _key, _val)

static void
print_cpuset(const char *key, cpu_set_t *set, int hex)
{
	size_t setsize = CPU_ALLOC_SIZE(maxcpus);
	size_t setbuflen = 7 * maxcpus;
	char setbuf[setbuflen], *p;

	if (hex) {
		p = cpumask_create(setbuf, setbuflen, set, setsize);
		printf("%-23s0x%s\n", key, p);
	} else {
		p = cpulist_create(setbuf, setbuflen, set, setsize);
		print_s(key, p);
	}

}

static void
print_readable(struct lscpu_desc *desc, int hex)
{
	char buf[512];
	int i;
	size_t setsize = CPU_ALLOC_SIZE(maxcpus);

	print_s(_("Architecture:"), desc->arch);

	if (desc->mode) {
		char buf[64], *p = buf;

		if (desc->mode & MODE_32BIT) {
			strcpy(p, "32-bit, ");
			p += 8;
		}
		if (desc->mode & MODE_64BIT) {
			strcpy(p, "64-bit, ");
			p += 8;
		}
		*(p - 2) = '\0';
		print_s(_("CPU op-mode(s):"), buf);
	}
#if !defined(WORDS_BIGENDIAN)
	print_s(_("Byte Order:"), "Little Endian");
#else
	print_s(_("Byte Order:"), "Big Endian");
#endif
	print_n(_("CPU(s):"), desc->ncpus);

	if (desc->online)
		print_cpuset(hex ? _("On-line CPU(s) mask:") :
				   _("On-line CPU(s) list:"),
				desc->online, hex);

	if (desc->online && CPU_COUNT_S(setsize, desc->online) != desc->ncpus) {
		cpu_set_t *set;

		/* Linux kernel provides cpuset of off-line CPUs that contains
		 * all configured CPUs (see /sys/devices/system/cpu/offline),
		 * but want to print real (present in system) off-line CPUs only.
		 */
		set = cpuset_alloc(maxcpus, NULL, NULL);
		if (!set)
			err(EXIT_FAILURE, _("failed to callocate cpu set"));
		CPU_ZERO_S(setsize, set);
		for (i = 0; i < desc->ncpus; i++) {
			if (!is_cpu_online(desc, i))
				CPU_SET_S(i, setsize, set);
		}
		print_cpuset(hex ? _("Off-line CPU(s) mask:") :
				   _("Off-line CPU(s) list:"),
			     set, hex);
		cpuset_free(set);
	}

	if (desc->nsockets) {
		print_n(_("Thread(s) per core:"), desc->nthreads / desc->ncores);
		print_n(_("Core(s) per socket:"), desc->ncores / desc->nsockets);
		if (desc->nbooks) {
			print_n(_("Socket(s) per book:"), desc->nsockets / desc->nbooks);
			print_n(_("Book(s):"), desc->nbooks);
		} else {
			print_n(_("Socket(s):"), desc->nsockets);
		}
	}
	if (desc->nnodes)
		print_n(_("NUMA node(s):"), desc->nnodes);
	if (desc->vendor)
		print_s(_("Vendor ID:"), desc->vendor);
	if (desc->family)
		print_s(_("CPU family:"), desc->family);
	if (desc->model)
		print_s(_("Model:"), desc->model);
	if (desc->stepping)
		print_s(_("Stepping:"), desc->stepping);
	if (desc->mhz)
		print_s(_("CPU MHz:"), desc->mhz);
	if (desc->bogomips)
		print_s(_("BogoMIPS:"), desc->bogomips);
	if (desc->virtflag) {
		if (!strcmp(desc->virtflag, "svm"))
			print_s(_("Virtualization:"), "AMD-V");
		else if (!strcmp(desc->virtflag, "vmx"))
			print_s(_("Virtualization:"), "VT-x");
	}
	if (desc->hyper) {
		print_s(_("Hypervisor vendor:"), hv_vendors[desc->hyper]);
		print_s(_("Virtualization type:"), virt_types[desc->virtype]);
	}
	if (desc->ncaches) {
		char buf[512];
		int i;

		for (i = desc->ncaches - 1; i >= 0; i--) {
			snprintf(buf, sizeof(buf),
					_("%s cache:"), desc->caches[i].name);
			print_s(buf, desc->caches[i].size);
		}
	}

	for (i = 0; i < desc->nnodes; i++) {
		snprintf(buf, sizeof(buf), _("NUMA node%d CPU(s):"), i);
		print_cpuset(buf, desc->nodemaps[i], hex);
	}
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -h, --help          print this help\n"
		" -p, --parse <list>  print out a parsable instead of a readable format\n"
		" -s, --sysroot <dir> use directory DIR as system root\n"
		" -x, --hex           print hexadecimal masks rather than lists of CPUs\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct lscpu_desc _desc, *desc = &_desc;
	int parsable = 0, c, i, hex = 0;
	int columns[ARRAY_SIZE(colnames)], ncolumns = 0;
	int compatible = 0;

	static const struct option longopts[] = {
		{ "help",	no_argument,       0, 'h' },
		{ "parse",	optional_argument, 0, 'p' },
		{ "sysroot",	required_argument, 0, 's' },
		{ "hex",	no_argument,	   0, 'x' },
		{ NULL,		0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "hp::s:x", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(stdout);
		case 'p':
			parsable = 1;
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				ncolumns = string_to_idarray(optarg,
						columns, ARRAY_SIZE(columns),
						column_name_to_id);
				if (ncolumns < 0)
					return EXIT_FAILURE;
			} else {
				columns[ncolumns++] = COL_CPU;
				columns[ncolumns++] = COL_CORE;
				columns[ncolumns++] = COL_SOCKET,
				columns[ncolumns++] = COL_NODE,
				columns[ncolumns++] = COL_CACHE;
				compatible = 1;
			}
			break;
		case 's':
			sysrootlen = strlen(optarg);
			strncpy(pathbuf, optarg, sizeof(pathbuf));
			pathbuf[sizeof(pathbuf) - 1] = '\0';
			break;
		case 'x':
			hex = 1;
			break;
		default:
			usage(stderr);
		}
	}

	memset(desc, 0, sizeof(*desc));

	read_basicinfo(desc);

	for (i = 0; i < desc->ncpus; i++) {
		if (desc->online && !is_cpu_online(desc, i))
			continue;
		read_topology(desc, i);
		read_cache(desc, i);
	}

	qsort(desc->caches, desc->ncaches, sizeof(struct cpu_cache), cachecmp);

	read_nodes(desc);

	read_hypervisor(desc);

	/* Show time! */
	if (parsable)
		print_parsable(desc, columns, ncolumns, compatible);
	else
		print_readable(desc, hex);

	return EXIT_SUCCESS;
}
