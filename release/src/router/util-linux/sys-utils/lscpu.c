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
#include "tt.h"
#include "path.h"

#define CACHE_MAX 100

/* /sys paths */
#define _PATH_SYS_SYSTEM	"/sys/devices/system"
#define _PATH_SYS_CPU		_PATH_SYS_SYSTEM "/cpu"
#define _PATH_PROC_XEN		"/proc/xen"
#define _PATH_PROC_XENCAP	_PATH_PROC_XEN "/capabilities"
#define _PATH_PROC_CPUINFO	"/proc/cpuinfo"
#define _PATH_PROC_PCIDEVS	"/proc/bus/pci/devices"
#define _PATH_PROC_SYSINFO	"/proc/sysinfo"

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
	HYPER_VMWARE,
	HYPER_IBM
};
const char *hv_vendors[] = {
	[HYPER_NONE]	= NULL,
	[HYPER_XEN]	= "Xen",
	[HYPER_KVM]	= "KVM",
	[HYPER_MSHV]	= "Microsoft",
	[HYPER_VMWARE]  = "VMware",
	[HYPER_IBM]	= "IBM"
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

/* dispatching modes */
enum {
	DISP_HORIZONTAL = 0,
	DISP_VERTICAL	= 1
};

const char *disp_modes[] = {
	[DISP_HORIZONTAL]	= N_("horizontal"),
	[DISP_VERTICAL]		= N_("vertical")
};

/* cpu polarization */
enum {
	POLAR_UNKNOWN	= 0,
	POLAR_VLOW,
	POLAR_VMEDIUM,
	POLAR_VHIGH,
	POLAR_HORIZONTAL
};

struct polarization_modes {
	char *parsable;
	char *readable;
};

struct polarization_modes polar_modes[] = {
	[POLAR_UNKNOWN]	   = {"U",  "-"},
	[POLAR_VLOW]	   = {"VL", "vert-low"},
	[POLAR_VMEDIUM]	   = {"VM", "vert-medium"},
	[POLAR_VHIGH]	   = {"VH", "vert-high"},
	[POLAR_HORIZONTAL] = {"H",  "horizontal"},
};

/* global description */
struct lscpu_desc {
	char	*arch;
	char	*vendor;
	char	*family;
	char	*model;
	char	*virtflag;	/* virtualization flag (vmx, svm) */
	char	*hypervisor;	/* hypervisor software */
	int	hyper;		/* hypervisor vendor ID */
	int	virtype;	/* VIRT_PARA|FULL|NONE ? */
	char	*mhz;
	char	*stepping;
	char    *bogomips;
	char	*flags;
	int	dispatching;	/* none, horizontal or vertical */
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

	int		*polarization;	/* cpu polarization */
	int		*addresses;	/* physical cpu addresses */
	int		*configured;	/* cpu configured */
};

enum {
	OUTPUT_SUMMARY	= 0,	/* default */
	OUTPUT_PARSABLE,	/* -p */
	OUTPUT_READABLE,	/* -e */
};

enum {
	SYSTEM_LIVE = 0,	/* analyzing a live system */
	SYSTEM_SNAPSHOT,	/* analyzing a snapshot of a different system */
};

struct lscpu_modifier {
	int		mode;		/* OUTPUT_* */
	int		system;		/* SYSTEM_* */
	unsigned int	hex:1,		/* print CPU masks rather than CPU lists */
			compat:1,	/* use backwardly compatible format */
			online:1,	/* print online CPUs */
			offline:1;	/* print offline CPUs */
};

static int maxcpus;		/* size in bits of kernel cpu mask */

#define is_cpu_online(_d, _cpu) \
		((_d) && (_d)->online ? \
			CPU_ISSET_S((_cpu), CPU_ALLOC_SIZE(maxcpus), (_d)->online) : 0)

/*
 * IDs
 */
enum {
	COL_CPU,
	COL_CORE,
	COL_SOCKET,
	COL_NODE,
	COL_BOOK,
	COL_CACHE,
	COL_POLARIZATION,
	COL_ADDRESS,
	COL_CONFIGURED,
	COL_ONLINE,
};

/* column description
 */
struct lscpu_coldesc {
	const char *name;
	const char *help;

	unsigned int  is_abbr:1;	/* name is abbreviation */
};

static struct lscpu_coldesc coldescs[] =
{
	[COL_CPU]          = { "CPU", N_("logical CPU number"), 1 },
	[COL_CORE]         = { "CORE", N_("logical core number") },
	[COL_SOCKET]       = { "SOCKET", N_("logical socket number") },
	[COL_NODE]         = { "NODE", N_("logical NUMA node number") },
	[COL_BOOK]         = { "BOOK", N_("logical book number") },
	[COL_CACHE]        = { "CACHE", N_("shows how caches are shared between CPUs") },
	[COL_POLARIZATION] = { "POLARIZATION", N_("CPU dispatching mode on virtual hardware") },
	[COL_ADDRESS]      = { "ADDRESS", N_("physical address of a CPU") },
	[COL_CONFIGURED]   = { "CONFIGURED", N_("shows if the hypervisor has allocated the CPU") },
	[COL_ONLINE]       = { "ONLINE", N_("shows if Linux currently makes use of the CPU") }
};

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(coldescs); i++) {
		const char *cn = coldescs[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
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
init_mode(struct lscpu_modifier *mod)
{
	int m = 0;

	if (mod->system == SYSTEM_SNAPSHOT)
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
read_basicinfo(struct lscpu_desc *desc, struct lscpu_modifier *mod)
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
		else if (lookup(buf, "bogomips per cpu", &desc->bogomips)) ; /* s390 */
		else
			continue;
	}

	desc->mode = init_mode(mod);

	if (desc->flags) {
		snprintf(buf, sizeof(buf), " %s ", desc->flags);
		if (strstr(buf, " svm "))
			desc->virtflag = xstrdup("svm");
		else if (strstr(buf, " vmx "))
			desc->virtflag = xstrdup("vmx");
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

	else if (mod->system == SYSTEM_LIVE)
		/* the root is '/' so we are working with data from the current kernel */
		maxcpus = get_max_number_of_cpus();

	if (maxcpus <= 0)
		/* error or we are reading some /sys snapshot instead of the
		 * real /sys, let's use any crazy number... */
		maxcpus = desc->ncpus > 2048 ? desc->ncpus : 2048;

	/* get mask for online CPUs */
	if (path_exist(_PATH_SYS_SYSTEM "/cpu/online")) {
		size_t setsize = CPU_ALLOC_SIZE(maxcpus);
		desc->online = path_cpulist(maxcpus, _PATH_SYS_SYSTEM "/cpu/online");
		desc->nthreads = CPU_COUNT_S(setsize, desc->online);
	}

	/* get dispatching mode */
	if (path_exist(_PATH_SYS_SYSTEM "/cpu/dispatching"))
		desc->dispatching = path_getnum(_PATH_SYS_SYSTEM "/cpu/dispatching");
	else
		desc->dispatching = -1;
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
	} else if (path_exist(_PATH_PROC_SYSINFO)) {
		FILE *fd = path_fopen("r", 0, _PATH_PROC_SYSINFO);
		char buf[BUFSIZ];

		desc->hyper = HYPER_IBM;
		desc->hypervisor = "PR/SM";
		desc->virtype = VIRT_FULL;
		while (fgets(buf, sizeof(buf), fd) != NULL) {
			char *str;

			if (!strstr(buf, "Control Program:"))
				continue;
			if (!strstr(buf, "KVM"))
				desc->hyper = HYPER_IBM;
			else
				desc->hyper = HYPER_KVM;
			str = strchr(buf, ':');
			if (!str)
				continue;
			if (asprintf(&str, "%s", str + 1) == -1)
				errx(EXIT_FAILURE, _("failed to allocate memory"));
			/* remove leading, trailing and repeating whitespace */
			while (*str == ' ')
				str++;
			desc->hypervisor = str;
			str += strlen(str) - 1;
			while ((*str == '\n') || (*str == ' '))
				*(str--) = '\0';
			while ((str = strstr(desc->hypervisor, "  ")))
				memmove(str, str + 1, strlen(str));
		}
		fclose(fd);
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

	thread_siblings = path_cpuset(maxcpus, _PATH_SYS_CPU
					"/cpu%d/topology/thread_siblings", num);
	core_siblings = path_cpuset(maxcpus, _PATH_SYS_CPU
					"/cpu%d/topology/core_siblings", num);
	book_siblings = NULL;
	if (path_exist(_PATH_SYS_CPU "/cpu%d/topology/book_siblings", num)) {
		book_siblings = path_cpuset(maxcpus, _PATH_SYS_CPU
					    "/cpu%d/topology/book_siblings", num);
	}

	if (!desc->coremaps) {
		int nbooks, nsockets, ncores, nthreads;
		size_t setsize = CPU_ALLOC_SIZE(maxcpus);

		/* threads within one core */
		nthreads = CPU_COUNT_S(setsize, thread_siblings);
		/* cores within one socket */
		ncores = CPU_COUNT_S(setsize, core_siblings) / nthreads;
		/* number of sockets within one book.
		 * Because of odd / non-present cpu maps and to keep
		 * calculation easy we make sure that nsockets and
		 * nbooks is at least 1.
		 */
		nsockets = desc->ncpus / nthreads / ncores ?: 1;
		/* number of books */
		nbooks = desc->ncpus / nthreads / ncores / nsockets ?: 1;

		/* all threads, see also read_basicinfo()
		 * -- fallback for kernels without
		 *    /sys/devices/system/cpu/online.
		 */
		if (!desc->nthreads)
			desc->nthreads = nbooks * nsockets * ncores * nthreads;
		/* For each map we make sure that it can have up to ncpus
		 * entries. This is because we cannot reliably calculate the
		 * number of cores, sockets and books on all architectures.
		 * E.g. completely virtualized architectures like s390 may
		 * have multiple sockets of different sizes.
		 */
		desc->coremaps = xcalloc(desc->ncpus, sizeof(cpu_set_t *));
		desc->socketmaps = xcalloc(desc->ncpus, sizeof(cpu_set_t *));
		if (book_siblings)
			desc->bookmaps = xcalloc(desc->ncpus, sizeof(cpu_set_t *));
	}

	add_cpuset_to_array(desc->socketmaps, &desc->nsockets, core_siblings);
	add_cpuset_to_array(desc->coremaps, &desc->ncores, thread_siblings);
	if (book_siblings)
		add_cpuset_to_array(desc->bookmaps, &desc->nbooks, book_siblings);
}
static void
read_polarization(struct lscpu_desc *desc, int num)
{
	char mode[64];

	if (desc->dispatching < 0)
		return;
	if (!path_exist(_PATH_SYS_CPU "/cpu%d/polarization", num))
		return;
	if (!desc->polarization)
		desc->polarization = xcalloc(desc->ncpus, sizeof(int));
	path_getstr(mode, sizeof(mode), _PATH_SYS_CPU "/cpu%d/polarization", num);
	if (strncmp(mode, "vertical:low", sizeof(mode)) == 0)
		desc->polarization[num] = POLAR_VLOW;
	else if (strncmp(mode, "vertical:medium", sizeof(mode)) == 0)
		desc->polarization[num] = POLAR_VMEDIUM;
	else if (strncmp(mode, "vertical:high", sizeof(mode)) == 0)
		desc->polarization[num] = POLAR_VHIGH;
	else if (strncmp(mode, "horizontal", sizeof(mode)) == 0)
		desc->polarization[num] = POLAR_HORIZONTAL;
	else
		desc->polarization[num] = POLAR_UNKNOWN;
}

static void
read_address(struct lscpu_desc *desc, int num)
{
	if (!path_exist(_PATH_SYS_CPU "/cpu%d/address", num))
		return;
	if (!desc->addresses)
		desc->addresses = xcalloc(desc->ncpus, sizeof(int));
	desc->addresses[num] = path_getnum(_PATH_SYS_CPU "/cpu%d/address", num);
}

static void
read_configured(struct lscpu_desc *desc, int num)
{
	if (!path_exist(_PATH_SYS_CPU "/cpu%d/configure", num))
		return;
	if (!desc->configured)
		desc->configured = xcalloc(desc->ncpus, sizeof(int));
	desc->configured[num] = path_getnum(_PATH_SYS_CPU "/cpu%d/configure", num);
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

		if (!path_exist(_PATH_SYS_SYSTEM "/cpu/cpu%d/cache/index%d",
				num, i))
			continue;
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
		map = path_cpuset(maxcpus,
				  _PATH_SYS_CPU "/cpu%d/cache/index%d/shared_cpu_map",
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
		desc->nodemaps[i] = path_cpuset(maxcpus,
					_PATH_SYS_SYSTEM "/node/node%d/cpumap",
					i);
}

static char *
get_cell_data(struct lscpu_desc *desc, int cpu, int col,
	      struct lscpu_modifier *mod,
	      char *buf, size_t bufsz)
{
	size_t setsize = CPU_ALLOC_SIZE(maxcpus);
	size_t idx;

	*buf = '\0';

	switch (col) {
	case COL_CPU:
		snprintf(buf, bufsz, "%d", cpu);
		break;
	case COL_CORE:
		if (cpuset_ary_isset(cpu, desc->coremaps,
				     desc->ncores, setsize, &idx) == 0)
			snprintf(buf, bufsz, "%zd", idx);
		break;
	case COL_SOCKET:
		if (cpuset_ary_isset(cpu, desc->socketmaps,
				     desc->nsockets, setsize, &idx) == 0)
			snprintf(buf, bufsz, "%zd", idx);
		break;
	case COL_NODE:
		if (cpuset_ary_isset(cpu, desc->nodemaps,
				     desc->nnodes, setsize, &idx) == 0)
			snprintf(buf, bufsz, "%zd", idx);
		break;
	case COL_BOOK:
		if (cpuset_ary_isset(cpu, desc->bookmaps,
				     desc->nbooks, setsize, &idx) == 0)
			snprintf(buf, bufsz, "%zd", idx);
		break;
	case COL_CACHE:
	{
		char *p = buf;
		size_t sz = bufsz;
		int j;

		for (j = desc->ncaches - 1; j >= 0; j--) {
			struct cpu_cache *ca = &desc->caches[j];

			if (cpuset_ary_isset(cpu, ca->sharedmaps,
					     ca->nsharedmaps, setsize, &idx) == 0) {
				int x = snprintf(p, sz, "%zd", idx);
				if (x <= 0 || (size_t) x + 2 >= sz)
					return NULL;
				p += x;
				sz -= x;
			}
			if (j != 0) {
				*p++ = mod->compat ? ',' : ':';
				*p = '\0';
				sz++;
			}
		}
		break;
	}
	case COL_POLARIZATION:
		if (desc->polarization) {
			int x = desc->polarization[cpu];

			snprintf(buf, bufsz, "%s",
				 mod->mode == OUTPUT_PARSABLE ?
						polar_modes[x].parsable :
						polar_modes[x].readable);
		}
		break;
	case COL_ADDRESS:
		if (desc->addresses)
			snprintf(buf, bufsz, "%d", desc->addresses[cpu]);
		break;
	case COL_CONFIGURED:
		if (!desc->configured)
			break;
		if (mod->mode == OUTPUT_PARSABLE)
			snprintf(buf, bufsz,
				 desc->configured[cpu] ? _("Y") : _("N"));
		else
			snprintf(buf, bufsz,
				 desc->configured[cpu] ? _("yes") : _("no"));
		break;
	case COL_ONLINE:
		if (!desc->online)
			break;
		if (mod->mode == OUTPUT_PARSABLE)
			snprintf(buf, bufsz,
				 is_cpu_online(desc, cpu) ? _("Y") : _("N"));
		else
			snprintf(buf, bufsz,
				 is_cpu_online(desc, cpu) ? _("yes") : _("no"));
		break;
	}
	return buf;
}

static char *
get_cell_header(struct lscpu_desc *desc, int col,
		struct lscpu_modifier *mod,
	        char *buf, size_t bufsz)
{
	*buf = '\0';

	if (col == COL_CACHE) {
		char *p = buf;
		size_t sz = bufsz;
		int i;

		for (i = desc->ncaches - 1; i >= 0; i--) {
			int x = snprintf(p, sz, "%s", desc->caches[i].name);
			if (x <= 0 || (size_t) x + 2 > sz)
				return NULL;
			sz -= x;
			p += x;
			if (i > 0) {
				*p++ = mod->compat ? ',' : ':';
				*p = '\0';
				sz++;
			}
		}
		if (desc->ncaches)
			return buf;
	}
	snprintf(buf, bufsz, "%s", coldescs[col].name);
	return buf;
}

/*
 * [-p] backend, we support two parsable formats:
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
print_parsable(struct lscpu_desc *desc, int cols[], int ncols,
	       struct lscpu_modifier *mod)
{
	char buf[BUFSIZ], *data;
	int i;

	/*
	 * Header
	 */
	printf(_(
	"# The following is the parsable format, which can be fed to other\n"
	"# programs. Each different item in every column has an unique ID\n"
	"# starting from zero.\n"));

	fputs("# ", stdout);
	for (i = 0; i < ncols; i++) {
		int col = cols[i];

		if (col == COL_CACHE) {
			if (mod->compat && !desc->ncaches)
				continue;
			if (mod->compat && i != 0)
				putchar(',');
		}
		if (i > 0)
			putchar(',');

		data = get_cell_header(desc, col, mod, buf, sizeof(buf));

		if (data && * data && col != COL_CACHE &&
		    !coldescs[col].is_abbr) {
			/*
			 * For normal column names use mixed case (e.g. "Socket")
			 */
			char *p = data + 1;

			while (p && *p != '\0')
				*p++ = tolower((unsigned int) *p);
		}
		fputs(data && *data ? data : "", stdout);
	}
	putchar('\n');

	/*
	 * Data
	 */
	for (i = 0; i < desc->ncpus; i++) {
		int c;

		if (!mod->offline && desc->online && !is_cpu_online(desc, i))
			continue;
		if (!mod->online && desc->online && is_cpu_online(desc, i))
			continue;
		for (c = 0; c < ncols; c++) {
			if (mod->compat && cols[c] == COL_CACHE) {
				if (!desc->ncaches)
					continue;
				if (c > 0)
					putchar(',');
			}
			if (c > 0)
				putchar(',');

			data = get_cell_data(desc, i, cols[c], mod,
					     buf, sizeof(buf));
			fputs(data && *data ? data : "", stdout);
		}
		putchar('\n');
	}
}

/*
 * [-e] backend
 */
static void
print_readable(struct lscpu_desc *desc, int cols[], int ncols,
	       struct lscpu_modifier *mod)
{
	int i;
	char buf[BUFSIZ], *data;
	struct tt *tt = tt_new_table(0);

	if (!tt)
		 err(EXIT_FAILURE, _("failed to initialize output table"));

	for (i = 0; i < ncols; i++) {
		data = get_cell_header(desc, cols[i], mod, buf, sizeof(buf));
		tt_define_column(tt, xstrdup(data), 0, 0);
	}

	for (i = 0; i < desc->ncpus; i++) {
		int c;
		struct tt_line *line;

		if (!mod->offline && desc->online && !is_cpu_online(desc, i))
			continue;
		if (!mod->online && desc->online && is_cpu_online(desc, i))
			continue;

		line = tt_add_line(tt, NULL);

		for (c = 0; c < ncols; c++) {
			data = get_cell_data(desc, i, cols[c], mod,
					     buf, sizeof(buf));
			tt_line_set_data(line, c, data && *data ? xstrdup(data) : "-");
		}
	}

	tt_print_table(tt);
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

/*
 * default output
 */
static void
print_summary(struct lscpu_desc *desc, struct lscpu_modifier *mod)
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
		print_cpuset(mod->hex ? _("On-line CPU(s) mask:") :
					_("On-line CPU(s) list:"),
				desc->online, mod->hex);

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
		print_cpuset(mod->hex ? _("Off-line CPU(s) mask:") :
					_("Off-line CPU(s) list:"),
			     set, mod->hex);
		cpuset_free(set);
	}

	if (desc->nsockets) {
		int cores_per_socket, sockets_per_book, books;

		cores_per_socket = sockets_per_book = books = 0;
		/* s390 detects its cpu topology via /proc/sysinfo, if present.
		 * Using simply the cpu topology masks in sysfs will not give
		 * usable results since everything is virtualized. E.g.
		 * virtual core 0 may have only 1 cpu, but virtual core 2 may
		 * five cpus.
		 * If the cpu topology is not exported (e.g. 2nd level guest)
		 * fall back to old calculation scheme.
		 */
		if (path_exist(_PATH_PROC_SYSINFO)) {
			FILE *fd = path_fopen("r", 0, _PATH_PROC_SYSINFO);
			char buf[BUFSIZ];
			int t0, t1, t2;

			while (fd && fgets(buf, sizeof(buf), fd) != NULL) {
				if (sscanf(buf, "CPU Topology SW:%d%d%d%d%d%d",
					   &t0, &t1, &t2, &books, &sockets_per_book,
					   &cores_per_socket) == 6)
					break;
			}
			if (fd)
				fclose(fd);
		}
		print_n(_("Thread(s) per core:"), desc->nthreads / desc->ncores);
		print_n(_("Core(s) per socket:"),
			cores_per_socket ?: desc->ncores / desc->nsockets);
		if (desc->nbooks) {
			print_n(_("Socket(s) per book:"),
				sockets_per_book ?: desc->nsockets / desc->nbooks);
			print_n(_("Book(s):"), books ?: desc->nbooks);
		} else {
			print_n(_("Socket(s):"), sockets_per_book ?: desc->nsockets);
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
	if (desc->hypervisor)
		print_s(_("Hypervisor:"), desc->hypervisor);
	if (desc->hyper) {
		print_s(_("Hypervisor vendor:"), hv_vendors[desc->hyper]);
		print_s(_("Virtualization type:"), _(virt_types[desc->virtype]));
	}
	if (desc->dispatching >= 0)
		print_s(_("Dispatching mode:"), _(disp_modes[desc->dispatching]));
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
		print_cpuset(buf, desc->nodemaps[i], mod->hex);
	}
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all               print online and offline CPUs (default for -e)\n"
		" -b, --online            print online CPUs only (default for -p)\n"
		" -c, --offline           print offline CPUs only\n"
		" -e, --extended[=<list>] print out an extended readable format\n"
		" -h, --help              print this help\n"
		" -p, --parse[=<list>]    print out a parsable format\n"
		" -s, --sysroot <dir>     use directory DIR as system root\n"
		" -V, --version           print version information and exit\n"
		" -x, --hex               print hexadecimal masks rather than lists of CPUs\n"), out);

	fprintf(out, _("\nAvailable columns:\n"));

	for (i = 0; i < ARRAY_SIZE(coldescs); i++)
		fprintf(out, " %13s  %s\n", coldescs[i].name, _(coldescs[i].help));

	fprintf(out, _("\nFor more details see lscpu(1).\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct lscpu_modifier _mod = { .mode = OUTPUT_SUMMARY }, *mod = &_mod;
	struct lscpu_desc _desc = { .flags = 0 }, *desc = &_desc;
	int c, i;
	int columns[ARRAY_SIZE(coldescs)], ncolumns = 0;

	static const struct option longopts[] = {
		{ "all",        no_argument,       0, 'a' },
		{ "online",     no_argument,       0, 'b' },
		{ "offline",    no_argument,       0, 'c' },
		{ "help",	no_argument,       0, 'h' },
		{ "extended",	optional_argument, 0, 'e' },
		{ "parse",	optional_argument, 0, 'p' },
		{ "sysroot",	required_argument, 0, 's' },
		{ "hex",	no_argument,	   0, 'x' },
		{ "version",	no_argument,	   0, 'V' },
		{ NULL,		0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "abce::hp::s:xV", longopts, NULL)) != -1) {

		if (mod->mode != OUTPUT_SUMMARY && strchr("ep", c))
			errx(EXIT_FAILURE,
			     _("extended and parsable formats are mutually exclusive"));
		if ((mod->online || mod->offline) && strchr("abc", c))
			errx(EXIT_FAILURE,
			     _("--all, --online and --offline options are mutually exclusive"));

		switch (c) {
		case 'a':
			mod->online = mod->offline = 1;
			break;
		case 'b':
			mod->online = 1;
			break;
		case 'c':
			mod->offline = 1;
			break;
		case 'h':
			usage(stdout);
		case 'p':
		case 'e':
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				ncolumns = string_to_idarray(optarg,
						columns, ARRAY_SIZE(columns),
						column_name_to_id);
				if (ncolumns < 0)
					return EXIT_FAILURE;
			}
			mod->mode = c == 'p' ? OUTPUT_PARSABLE : OUTPUT_READABLE;
			break;
		case 's':
			path_setprefix(optarg);
			mod->system = SYSTEM_SNAPSHOT;
			break;
		case 'x':
			mod->hex = 1;
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}
	}

	if (argc != optind)
		usage(stderr);

	/* set default cpu display mode if none was specified */
	if (!mod->online && !mod->offline) {
		mod->online = 1;
		mod->offline = mod->mode == OUTPUT_READABLE ? 1 : 0;
	}

	read_basicinfo(desc, mod);

	for (i = 0; i < desc->ncpus; i++) {
		read_topology(desc, i);
		read_cache(desc, i);
		read_polarization(desc, i);
		read_address(desc, i);
		read_configured(desc, i);
	}

	if (desc->caches)
		qsort(desc->caches, desc->ncaches,
				sizeof(struct cpu_cache), cachecmp);

	read_nodes(desc);
	read_hypervisor(desc);

	switch(mod->mode) {
	case OUTPUT_SUMMARY:
		print_summary(desc, mod);
		break;
	case OUTPUT_PARSABLE:
		if (!ncolumns) {
			columns[ncolumns++] = COL_CPU;
			columns[ncolumns++] = COL_CORE;
			columns[ncolumns++] = COL_SOCKET;
			columns[ncolumns++] = COL_NODE;
			columns[ncolumns++] = COL_CACHE;
			mod->compat = 1;
		}
		print_parsable(desc, columns, ncolumns, mod);
		break;
	case OUTPUT_READABLE:
		if (!ncolumns) {
			/* No list was given. Just print whatever is there. */
			columns[ncolumns++] = COL_CPU;
			if (desc->nodemaps)
				columns[ncolumns++] = COL_NODE;
			if (desc->bookmaps)
				columns[ncolumns++] = COL_BOOK;
			if (desc->socketmaps)
				columns[ncolumns++] = COL_SOCKET;
			if (desc->coremaps)
				columns[ncolumns++] = COL_CORE;
			if (desc->caches)
				columns[ncolumns++] = COL_CACHE;
			if (desc->online)
				columns[ncolumns++] = COL_ONLINE;
			if (desc->configured)
				columns[ncolumns++] = COL_CONFIGURED;
			if (desc->polarization)
				columns[ncolumns++] = COL_POLARIZATION;
			if (desc->addresses)
				columns[ncolumns++] = COL_ADDRESS;
		}
		print_readable(desc, columns, ncolumns, mod);
		break;
	}

	return EXIT_SUCCESS;
}
