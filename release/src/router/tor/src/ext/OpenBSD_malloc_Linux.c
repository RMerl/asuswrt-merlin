/* Version 1.83 for Linux.
 * Compilation: gcc -shared -fPIC -O2 OpenBSD_malloc_Linux.c -o malloc.so
 * Launching: LD_PRELOAD=/path/to/malloc.so firefox
 */

/*	$OpenBSD: malloc.c,v 1.83 2006/05/14 19:53:40 otto Exp $	*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/* We use this macro to remove some code that we don't actually want,
 * rather than to fix its warnings. */
#define BUILDING_FOR_TOR

/*
 * Defining MALLOC_EXTRA_SANITY will enable extra checks which are
 * related to internal conditions and consistency in malloc.c. This has
 * a noticeable runtime performance hit, and generally will not do you
 * any good unless you fiddle with the internals of malloc or want
 * to catch random pointer corruption as early as possible.
 */
#ifndef	MALLOC_EXTRA_SANITY
#undef	MALLOC_EXTRA_SANITY
#endif

/*
 * Defining MALLOC_STATS will enable you to call malloc_dump() and set
 * the [dD] options in the MALLOC_OPTIONS environment variable.
 * It has no run-time performance hit, but does pull in stdio...
 */
#ifndef	MALLOC_STATS
#undef	MALLOC_STATS
#endif

/*
 * What to use for Junk.  This is the byte value we use to fill with
 * when the 'J' option is enabled.
 */
#define SOME_JUNK	0xd0	/* as in "Duh" :-) */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <err.h>
/* For SIZE_MAX */
#include "torint.h"

//#include "thread_private.h"

/*
 * The basic parameters you can tweak.
 *
 * malloc_pageshift	pagesize = 1 << malloc_pageshift
 *			It's probably best if this is the native
 *			page size, but it shouldn't have to be.
 *
 * malloc_minsize	minimum size of an allocation in bytes.
 *			If this is too small it's too much work
 *			to manage them.  This is also the smallest
 *			unit of alignment used for the storage
 *			returned by malloc/realloc.
 *
 */

static int align = 0;
static size_t g_alignment = 0;

extern int __libc_enable_secure;

#ifndef HAVE_ISSETUGID
static int issetugid(void)
{
	if (__libc_enable_secure) return 1;
	if (getuid() != geteuid()) return 1;
	if (getgid() != getegid()) return 1;
	return 0;
}
#endif

#define PGSHIFT 12
#undef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#include <pthread.h>
static pthread_mutex_t gen_mutex = PTHREAD_MUTEX_INITIALIZER;

#define _MALLOC_LOCK_INIT() {;}
#define _MALLOC_LOCK() {pthread_mutex_lock(&gen_mutex);}
#define _MALLOC_UNLOCK() {pthread_mutex_unlock(&gen_mutex);}

#if defined(__sparc__) || defined(__alpha__)
#define	malloc_pageshift	13U
#endif
#if defined(__ia64__)
#define	malloc_pageshift	14U
#endif

#ifndef malloc_pageshift
#define malloc_pageshift	(PGSHIFT)
#endif

/*
 * No user serviceable parts behind this point.
 *
 * This structure describes a page worth of chunks.
 */
struct pginfo {
	struct pginfo	*next;	/* next on the free list */
	void		*page;	/* Pointer to the page */
	u_short		size;	/* size of this page's chunks */
	u_short		shift;	/* How far to shift for this size chunks */
	u_short		free;	/* How many free chunks */
	u_short		total;	/* How many chunk */
	u_long		bits[1];/* Which chunks are free */
};

/* How many bits per u_long in the bitmap */
#define	MALLOC_BITS	(int)((NBBY * sizeof(u_long)))

/*
 * This structure describes a number of free pages.
 */
struct pgfree {
	struct pgfree	*next;	/* next run of free pages */
	struct pgfree	*prev;	/* prev run of free pages */
	void		*page;	/* pointer to free pages */
	void		*pdir;	/* pointer to the base page's dir */
	size_t		size;	/* number of bytes free */
};

/*
 * Magic values to put in the page_directory
 */
#define MALLOC_NOT_MINE	((struct pginfo*) 0)
#define MALLOC_FREE	((struct pginfo*) 1)
#define MALLOC_FIRST	((struct pginfo*) 2)
#define MALLOC_FOLLOW	((struct pginfo*) 3)
#define MALLOC_MAGIC	((struct pginfo*) 4)

#ifndef malloc_minsize
#define malloc_minsize			16UL
#endif

#if !defined(malloc_pagesize)
#define malloc_pagesize			(1UL<<malloc_pageshift)
#endif

#if ((1UL<<malloc_pageshift) != malloc_pagesize)
#error	"(1UL<<malloc_pageshift) != malloc_pagesize"
#endif

#ifndef malloc_maxsize
#define malloc_maxsize			((malloc_pagesize)>>1)
#endif

/* A mask for the offset inside a page. */
#define malloc_pagemask	((malloc_pagesize)-1)

#define	pageround(foo)	(((foo) + (malloc_pagemask)) & ~malloc_pagemask)
#define	ptr2index(foo)	(((u_long)(foo) >> malloc_pageshift)+malloc_pageshift)
#define	index2ptr(idx)	((void*)(((idx)-malloc_pageshift)<<malloc_pageshift))

/* Set when initialization has been done */
static unsigned int	malloc_started;

/* Number of free pages we cache */
static unsigned int	malloc_cache = 16;

/* Structure used for linking discrete directory pages. */
struct pdinfo {
	struct pginfo	**base;
	struct pdinfo	*prev;
	struct pdinfo	*next;
	u_long		dirnum;
};
static struct pdinfo *last_dir;	/* Caches to the last and previous */
static struct pdinfo *prev_dir;	/* referenced directory pages. */

static size_t	pdi_off;
static u_long	pdi_mod;
#define	PD_IDX(num)	((num) / (malloc_pagesize/sizeof(struct pginfo *)))
#define	PD_OFF(num)	((num) & ((malloc_pagesize/sizeof(struct pginfo *))-1))
#define	PI_IDX(index)	((index) / pdi_mod)
#define	PI_OFF(index)	((index) % pdi_mod)

/* The last index in the page directory we care about */
static u_long	last_index;

/* Pointer to page directory. Allocated "as if with" malloc */
static struct pginfo **page_dir;

/* Free pages line up here */
static struct pgfree free_list;

/* Abort(), user doesn't handle problems. */
static int	malloc_abort = 2;

/* Are we trying to die ? */
static int	suicide;

#ifdef MALLOC_STATS
/* dump statistics */
static int	malloc_stats;
#endif

/* avoid outputting warnings? */
static int	malloc_silent;

/* always realloc ? */
static int	malloc_realloc;

/* mprotect free pages PROT_NONE? */
static int	malloc_freeprot;

/* use guard pages after allocations? */
static size_t	malloc_guard = 0;
static size_t	malloc_guarded;
/* align pointers to end of page? */
static int	malloc_ptrguard;

static int	malloc_hint = 1;

/* xmalloc behaviour ? */
static int	malloc_xmalloc;

/* zero fill ? */
static int	malloc_zero;

/* junk fill ? */
static int	malloc_junk;

#ifdef __FreeBSD__
/* utrace ? */
static int	malloc_utrace;

struct ut {
	void		*p;
	size_t		s;
	void		*r;
};

void		utrace(struct ut *, int);

#define UTRACE(a, b, c) \
	if (malloc_utrace) \
		{struct ut u; u.p=a; u.s = b; u.r=c; utrace(&u, sizeof u);}
#else				/* !__FreeBSD__ */
#define UTRACE(a,b,c)
#endif

/* Status of malloc. */
static int	malloc_active;

/* Allocated memory. */
static size_t	malloc_used;

/* My last break. */
static caddr_t	malloc_brk;

/* One location cache for free-list holders. */
static struct pgfree *px;

/* Compile-time options. */
char		*malloc_options;

/* Name of the current public function. */
static const char	*malloc_func;

#define MMAP(size) \
	mmap((void *)0, (size), PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, \
	    -1, (off_t)0)

/*
 * Necessary function declarations.
 */
static void	*imalloc(size_t size);
static void	ifree(void *ptr);
static void	*irealloc(void *ptr, size_t size);
static void	*malloc_bytes(size_t size);
void *memalign(size_t boundary, size_t size);
size_t malloc_good_size(size_t size);

/*
 * Function for page directory lookup.
 */
static int
pdir_lookup(u_long index, struct pdinfo ** pdi)
{
	struct pdinfo	*spi;
	u_long		pidx = PI_IDX(index);

	if (last_dir != NULL && PD_IDX(last_dir->dirnum) == pidx)
		*pdi = last_dir;
	else if (prev_dir != NULL && PD_IDX(prev_dir->dirnum) == pidx)
		*pdi = prev_dir;
	else if (last_dir != NULL && prev_dir != NULL) {
		if ((PD_IDX(last_dir->dirnum) > pidx) ?
		    (PD_IDX(last_dir->dirnum) - pidx) :
		    (pidx - PD_IDX(last_dir->dirnum))
		    < (PD_IDX(prev_dir->dirnum) > pidx) ?
		    (PD_IDX(prev_dir->dirnum) - pidx) :
		    (pidx - PD_IDX(prev_dir->dirnum)))
			*pdi = last_dir;
		else
			*pdi = prev_dir;

		if (PD_IDX((*pdi)->dirnum) > pidx) {
			for (spi = (*pdi)->prev;
			    spi != NULL && PD_IDX(spi->dirnum) > pidx;
			    spi = spi->prev)
				*pdi = spi;
			if (spi != NULL)
				*pdi = spi;
		} else
			for (spi = (*pdi)->next;
			    spi != NULL && PD_IDX(spi->dirnum) <= pidx;
			    spi = spi->next)
				*pdi = spi;
	} else {
		*pdi = (struct pdinfo *) ((caddr_t) page_dir + pdi_off);
		for (spi = *pdi;
		    spi != NULL && PD_IDX(spi->dirnum) <= pidx;
		    spi = spi->next)
			*pdi = spi;
	}

	return ((PD_IDX((*pdi)->dirnum) == pidx) ? 0 :
	    (PD_IDX((*pdi)->dirnum) > pidx) ? 1 : -1);
}

#ifdef MALLOC_STATS
void
malloc_dump(int fd)
{
	char		buf[1024];
	struct pginfo	**pd;
	struct pgfree	*pf;
	struct pdinfo	*pi;
	u_long		j;

	pd = page_dir;
	pi = (struct pdinfo *) ((caddr_t) pd + pdi_off);

	/* print out all the pages */
	for (j = 0; j <= last_index;) {
		snprintf(buf, sizeof buf, "%08lx %5lu ", j << malloc_pageshift, j);
		write(fd, buf, strlen(buf));
		if (pd[PI_OFF(j)] == MALLOC_NOT_MINE) {
			for (j++; j <= last_index && pd[PI_OFF(j)] == MALLOC_NOT_MINE;) {
				if (!PI_OFF(++j)) {
					if ((pi = pi->next) == NULL ||
					    PD_IDX(pi->dirnum) != PI_IDX(j))
						break;
					pd = pi->base;
					j += pdi_mod;
				}
			}
			j--;
			snprintf(buf, sizeof buf, ".. %5lu not mine\n", j);
			write(fd, buf, strlen(buf));
		} else if (pd[PI_OFF(j)] == MALLOC_FREE) {
			for (j++; j <= last_index && pd[PI_OFF(j)] == MALLOC_FREE;) {
				if (!PI_OFF(++j)) {
					if ((pi = pi->next) == NULL ||
					    PD_IDX(pi->dirnum) != PI_IDX(j))
						break;
					pd = pi->base;
					j += pdi_mod;
				}
			}
			j--;
			snprintf(buf, sizeof buf, ".. %5lu free\n", j);
			write(fd, buf, strlen(buf));
		} else if (pd[PI_OFF(j)] == MALLOC_FIRST) {
			for (j++; j <= last_index && pd[PI_OFF(j)] == MALLOC_FOLLOW;) {
				if (!PI_OFF(++j)) {
					if ((pi = pi->next) == NULL ||
					    PD_IDX(pi->dirnum) != PI_IDX(j))
						break;
					pd = pi->base;
					j += pdi_mod;
				}
			}
			j--;
			snprintf(buf, sizeof buf, ".. %5lu in use\n", j);
			write(fd, buf, strlen(buf));
		} else if (pd[PI_OFF(j)] < MALLOC_MAGIC) {
			snprintf(buf, sizeof buf, "(%p)\n", pd[PI_OFF(j)]);
			write(fd, buf, strlen(buf));
		} else {
			snprintf(buf, sizeof buf, "%p %d (of %d) x %d @ %p --> %p\n",
			    pd[PI_OFF(j)], pd[PI_OFF(j)]->free,
			    pd[PI_OFF(j)]->total, pd[PI_OFF(j)]->size,
			    pd[PI_OFF(j)]->page, pd[PI_OFF(j)]->next);
			write(fd, buf, strlen(buf));
		}
		if (!PI_OFF(++j)) {
			if ((pi = pi->next) == NULL)
				break;
			pd = pi->base;
			j += (1 + PD_IDX(pi->dirnum) - PI_IDX(j)) * pdi_mod;
		}
	}

	for (pf = free_list.next; pf; pf = pf->next) {
		snprintf(buf, sizeof buf, "Free: @%p [%p...%p[ %ld ->%p <-%p\n",
		    pf, pf->page, (char *)pf->page + pf->size,
		    pf->size, pf->prev, pf->next);
		write(fd, buf, strlen(buf));
		if (pf == pf->next) {
			snprintf(buf, sizeof buf, "Free_list loops\n");
			write(fd, buf, strlen(buf));
			break;
		}
	}

	/* print out various info */
	snprintf(buf, sizeof buf, "Minsize\t%lu\n", malloc_minsize);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof buf, "Maxsize\t%lu\n", malloc_maxsize);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof buf, "Pagesize\t%lu\n", malloc_pagesize);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof buf, "Pageshift\t%u\n", malloc_pageshift);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof buf, "In use\t%lu\n", (u_long) malloc_used);
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof buf, "Guarded\t%lu\n", (u_long) malloc_guarded);
	write(fd, buf, strlen(buf));
}
#endif /* MALLOC_STATS */

extern char	*__progname;

static void
wrterror(const char *p)
{
#ifndef BUILDING_FOR_TOR
	const char		*q = " error: ";
	struct iovec	iov[5];

	iov[0].iov_base = __progname;
	iov[0].iov_len = strlen(__progname);
	iov[1].iov_base = (char*)malloc_func;
	iov[1].iov_len = strlen(malloc_func);
	iov[2].iov_base = (char*)q;
	iov[2].iov_len = strlen(q);
	iov[3].iov_base = (char*)p;
	iov[3].iov_len = strlen(p);
	iov[4].iov_base = (char*)"\n";
	iov[4].iov_len = 1;
	writev(STDERR_FILENO, iov, 5);
#else
        (void)p;
#endif
	suicide = 1;
#ifdef MALLOC_STATS
	if (malloc_stats)
		malloc_dump(STDERR_FILENO);
#endif /* MALLOC_STATS */
	malloc_active--;
	if (malloc_abort)
		abort();
}

static void
wrtwarning(const char *p)
{
#ifndef BUILDING_FOR_TOR
	const char		*q = " warning: ";
	struct iovec	iov[5];
#endif

	if (malloc_abort)
		wrterror(p);
	else if (malloc_silent)
		return;

#ifndef BUILDING_FOR_TOR
	iov[0].iov_base = __progname;
	iov[0].iov_len = strlen(__progname);
	iov[1].iov_base = (char*)malloc_func;
	iov[1].iov_len = strlen(malloc_func);
	iov[2].iov_base = (char*)q;
	iov[2].iov_len = strlen(q);
	iov[3].iov_base = (char*)p;
	iov[3].iov_len = strlen(p);
	iov[4].iov_base = (char*)"\n";
	iov[4].iov_len = 1;

	(void) writev(STDERR_FILENO, iov, 5);
#else
        (void)p;
#endif
}

#ifdef MALLOC_STATS
static void
malloc_exit(void)
{
	char	*q = "malloc() warning: Couldn't dump stats\n";
	int	save_errno = errno, fd;

	fd = open("malloc.out", O_RDWR|O_APPEND);
	if (fd != -1) {
		malloc_dump(fd);
		close(fd);
	} else
		write(STDERR_FILENO, q, strlen(q));
	errno = save_errno;
}
#endif /* MALLOC_STATS */

/*
 * Allocate aligned mmaped chunk
 */

static void *MMAP_A(size_t pages, size_t alignment)
{
	void *j, *p;
	size_t first_size, rest, begin, end;
	if (pages%malloc_pagesize != 0)
		pages = pages - pages%malloc_pagesize + malloc_pagesize;
	first_size = pages + alignment - malloc_pagesize;
	p = MMAP(first_size);
	rest = ((size_t)p) % alignment;
	j = (rest == 0) ? p : (void*) ((size_t)p + alignment - rest);
	begin = (size_t)j - (size_t)p;
	if (begin != 0) munmap(p, begin);
	end = (size_t)p + first_size - ((size_t)j + pages);
	if(end != 0) munmap( (void*) ((size_t)j + pages), end);

	return j;
}


/*
 * Allocate a number of pages from the OS
 */
static void *
map_pages(size_t pages)
{
	struct pdinfo	*pi, *spi;
	struct pginfo	**pd;
	u_long		idx, pidx, lidx;
	caddr_t		result, tail;
	u_long		index, lindex;
	void 		*pdregion = NULL;
	size_t		dirs, cnt;

	pages <<= malloc_pageshift;
	if (!align)
		result = MMAP(pages + malloc_guard);
	else {
		result = MMAP_A(pages + malloc_guard, g_alignment);
	}
	if (result == MAP_FAILED) {
#ifdef MALLOC_EXTRA_SANITY
		wrtwarning("(ES): map_pages fails");
#endif /* MALLOC_EXTRA_SANITY */
		errno = ENOMEM;
		return (NULL);
	}
	index = ptr2index(result);
	tail = result + pages + malloc_guard;
	lindex = ptr2index(tail) - 1;
	if (malloc_guard)
		mprotect(result + pages, malloc_guard, PROT_NONE);

	pidx = PI_IDX(index);
	lidx = PI_IDX(lindex);

	if (tail > malloc_brk) {
		malloc_brk = tail;
		last_index = lindex;
	}

	dirs = lidx - pidx;

	/* Insert directory pages, if needed. */
	if (pdir_lookup(index, &pi) != 0)
		dirs++;

	if (dirs > 0) {
		pdregion = MMAP(malloc_pagesize * dirs);
		if (pdregion == MAP_FAILED) {
			munmap(result, tail - result);
#ifdef MALLOC_EXTRA_SANITY
		wrtwarning("(ES): map_pages fails");
#endif
			errno = ENOMEM;
			return (NULL);
		}
	}

	cnt = 0;
	for (idx = pidx, spi = pi; idx <= lidx; idx++) {
		if (pi == NULL || PD_IDX(pi->dirnum) != idx) {
			pd = (struct pginfo **)((char *)pdregion +
			    cnt * malloc_pagesize);
			cnt++;
			memset(pd, 0, malloc_pagesize);
			pi = (struct pdinfo *) ((caddr_t) pd + pdi_off);
			pi->base = pd;
			pi->prev = spi;
			pi->next = spi->next;
			pi->dirnum = idx * (malloc_pagesize /
			    sizeof(struct pginfo *));

			if (spi->next != NULL)
				spi->next->prev = pi;
			spi->next = pi;
		}
		if (idx > pidx && idx < lidx) {
			pi->dirnum += pdi_mod;
		} else if (idx == pidx) {
			if (pidx == lidx) {
				pi->dirnum += (u_long)(tail - result) >>
				    malloc_pageshift;
			} else {
				pi->dirnum += pdi_mod - PI_OFF(index);
			}
		} else {
			pi->dirnum += PI_OFF(ptr2index(tail - 1)) + 1;
		}
#ifdef MALLOC_EXTRA_SANITY
		if (PD_OFF(pi->dirnum) > pdi_mod || PD_IDX(pi->dirnum) > idx) {
			wrterror("(ES): pages directory overflow");
			errno = EFAULT;
			return (NULL);
		}
#endif /* MALLOC_EXTRA_SANITY */
		if (idx == pidx && pi != last_dir) {
			prev_dir = last_dir;
			last_dir = pi;
		}
		spi = pi;
		pi = spi->next;
	}
#ifdef MALLOC_EXTRA_SANITY
	if (cnt > dirs)
		wrtwarning("(ES): cnt > dirs");
#endif /* MALLOC_EXTRA_SANITY */
	if (cnt < dirs)
		munmap((char *)pdregion + cnt * malloc_pagesize,
		    (dirs - cnt) * malloc_pagesize);

	return (result);
}

/*
 * Initialize the world
 */
static void
malloc_init(void)
{
	char		*p, b[64];
	int		i, j, save_errno = errno;

	_MALLOC_LOCK_INIT();

#ifdef MALLOC_EXTRA_SANITY
	malloc_junk = 1;
#endif /* MALLOC_EXTRA_SANITY */

	for (i = 0; i < 3; i++) {
		switch (i) {
		case 0:
			j = (int) readlink("/etc/malloc.conf", b, sizeof b - 1);
			if (j <= 0)
				continue;
			b[j] = '\0';
			p = b;
			break;
		case 1:
			if (issetugid() == 0)
				p = getenv("MALLOC_OPTIONS");
			else
				continue;
			break;
		case 2:
			p = malloc_options;
			break;
		default:
			p = NULL;
		}

		for (; p != NULL && *p != '\0'; p++) {
			switch (*p) {
			case '>':
				malloc_cache <<= 1;
				break;
			case '<':
				malloc_cache >>= 1;
				break;
			case 'a':
				malloc_abort = 0;
				break;
			case 'A':
				malloc_abort = 1;
				break;
#ifdef MALLOC_STATS
			case 'd':
				malloc_stats = 0;
				break;
			case 'D':
				malloc_stats = 1;
				break;
#endif /* MALLOC_STATS */
			case 'f':
				malloc_freeprot = 0;
				break;
			case 'F':
				malloc_freeprot = 1;
				break;
			case 'g':
				malloc_guard = 0;
				break;
			case 'G':
				malloc_guard = malloc_pagesize;
				break;
			case 'h':
				malloc_hint = 0;
				break;
			case 'H':
				malloc_hint = 1;
				break;
			case 'j':
				malloc_junk = 0;
				break;
			case 'J':
				malloc_junk = 1;
				break;
			case 'n':
				malloc_silent = 0;
				break;
			case 'N':
				malloc_silent = 1;
				break;
			case 'p':
				malloc_ptrguard = 0;
				break;
			case 'P':
				malloc_ptrguard = 1;
				break;
			case 'r':
				malloc_realloc = 0;
				break;
			case 'R':
				malloc_realloc = 1;
				break;
#ifdef __FreeBSD__
			case 'u':
				malloc_utrace = 0;
				break;
			case 'U':
				malloc_utrace = 1;
				break;
#endif /* __FreeBSD__ */
			case 'x':
				malloc_xmalloc = 0;
				break;
			case 'X':
				malloc_xmalloc = 1;
				break;
			case 'z':
				malloc_zero = 0;
				break;
			case 'Z':
				malloc_zero = 1;
				break;
			default:
				j = malloc_abort;
				malloc_abort = 0;
				wrtwarning("unknown char in MALLOC_OPTIONS");
				malloc_abort = j;
				break;
			}
		}
	}

	UTRACE(0, 0, 0);

	/*
	 * We want junk in the entire allocation, and zero only in the part
	 * the user asked for.
	 */
	if (malloc_zero)
		malloc_junk = 1;

#ifdef MALLOC_STATS
	if (malloc_stats && (atexit(malloc_exit) == -1))
		wrtwarning("atexit(2) failed."
		    "  Will not be able to dump malloc stats on exit");
#endif /* MALLOC_STATS */

	if (malloc_pagesize != getpagesize()) {
		wrterror("malloc() replacement compiled with a different "
			 "page size from what we're running with.  Failing.");
		errno = ENOMEM;
		return;
	}

	/* Allocate one page for the page directory. */
	page_dir = (struct pginfo **)MMAP(malloc_pagesize);

	if (page_dir == MAP_FAILED) {
		wrterror("mmap(2) failed, check limits");
		errno = ENOMEM;
		return;
	}
	pdi_off = (malloc_pagesize - sizeof(struct pdinfo)) & ~(malloc_minsize - 1);
	pdi_mod = pdi_off / sizeof(struct pginfo *);

	last_dir = (struct pdinfo *) ((caddr_t) page_dir + pdi_off);
	last_dir->base = page_dir;
	last_dir->prev = last_dir->next = NULL;
	last_dir->dirnum = malloc_pageshift;

	/* Been here, done that. */
	malloc_started++;

	/* Recalculate the cache size in bytes, and make sure it's nonzero. */
	if (!malloc_cache)
		malloc_cache++;
	malloc_cache <<= malloc_pageshift;
	errno = save_errno;
}

/*
 * Allocate a number of complete pages
 */
static void *
malloc_pages(size_t size)
{
	void		*p, *delay_free = NULL, *tp;
	size_t		i;
	struct pginfo	**pd;
	struct pdinfo	*pi;
	u_long		pidx, index;
	struct pgfree	*pf;

	size = pageround(size) + malloc_guard;

	p = NULL;
	/* Look for free pages before asking for more */
	if (!align)
	for (pf = free_list.next; pf; pf = pf->next) {

#ifdef MALLOC_EXTRA_SANITY
		if (pf->size & malloc_pagemask) {
			wrterror("(ES): junk length entry on free_list");
			errno = EFAULT;
			return (NULL);
		}
		if (!pf->size) {
			wrterror("(ES): zero length entry on free_list");
			errno = EFAULT;
			return (NULL);
		}
		if (pf->page > (pf->page + pf->size)) {
			wrterror("(ES): sick entry on free_list");
			errno = EFAULT;
			return (NULL);
		}
		if ((pi = pf->pdir) == NULL) {
			wrterror("(ES): invalid page directory on free-list");
			errno = EFAULT;
			return (NULL);
		}
		if ((pidx = PI_IDX(ptr2index(pf->page))) != PD_IDX(pi->dirnum)) {
			wrterror("(ES): directory index mismatch on free-list");
			errno = EFAULT;
			return (NULL);
		}
		pd = pi->base;
		if (pd[PI_OFF(ptr2index(pf->page))] != MALLOC_FREE) {
			wrterror("(ES): non-free first page on free-list");
			errno = EFAULT;
			return (NULL);
		}
		pidx = PI_IDX(ptr2index((pf->page) + (pf->size)) - 1);
		for (pi = pf->pdir; pi != NULL && PD_IDX(pi->dirnum) < pidx;
		    pi = pi->next)
			;
		if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
			wrterror("(ES): last page not referenced in page directory");
			errno = EFAULT;
			return (NULL);
		}
		pd = pi->base;
		if (pd[PI_OFF(ptr2index((pf->page) + (pf->size)) - 1)] != MALLOC_FREE) {
			wrterror("(ES): non-free last page on free-list");
			errno = EFAULT;
			return (NULL);
		}
#endif /* MALLOC_EXTRA_SANITY */

		if (pf->size < size)
			continue;

		if (pf->size == size) {
			p = pf->page;
			pi = pf->pdir;
			if (pf->next != NULL)
				pf->next->prev = pf->prev;
			pf->prev->next = pf->next;
			delay_free = pf;
			break;
		}
		p = pf->page;
		pf->page = (char *) pf->page + size;
		pf->size -= size;
		pidx = PI_IDX(ptr2index(pf->page));
		for (pi = pf->pdir; pi != NULL && PD_IDX(pi->dirnum) < pidx;
		    pi = pi->next)
			;
		if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
			wrterror("(ES): hole in directories");
			errno = EFAULT;
			return (NULL);
		}
		tp = pf->pdir;
		pf->pdir = pi;
		pi = tp;
		break;
	}

	size -= malloc_guard;

#ifdef MALLOC_EXTRA_SANITY
	if (p != NULL && pi != NULL) {
		pidx = PD_IDX(pi->dirnum);
		pd = pi->base;
	}
	if (p != NULL && pd[PI_OFF(ptr2index(p))] != MALLOC_FREE) {
		wrterror("(ES): allocated non-free page on free-list");
		errno = EFAULT;
		return (NULL);
	}
#endif /* MALLOC_EXTRA_SANITY */

	if (p != NULL && (malloc_guard || malloc_freeprot))
		mprotect(p, size, PROT_READ | PROT_WRITE);

	size >>= malloc_pageshift;

	/* Map new pages */
	if (p == NULL)
		p = map_pages(size);

	if (p != NULL) {
		index = ptr2index(p);
		pidx = PI_IDX(index);
		pdir_lookup(index, &pi);
#ifdef MALLOC_EXTRA_SANITY
		if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
			wrterror("(ES): mapped pages not found in directory");
			errno = EFAULT;
			return (NULL);
		}
#endif /* MALLOC_EXTRA_SANITY */
		if (pi != last_dir) {
			prev_dir = last_dir;
			last_dir = pi;
		}
		pd = pi->base;
		pd[PI_OFF(index)] = MALLOC_FIRST;

		for (i = 1; i < size; i++) {
			if (!PI_OFF(index + i)) {
				pidx++;
				pi = pi->next;
#ifdef MALLOC_EXTRA_SANITY
				if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
					wrterror("(ES): hole in mapped pages directory");
					errno = EFAULT;
					return (NULL);
				}
#endif /* MALLOC_EXTRA_SANITY */
				pd = pi->base;
			}
			pd[PI_OFF(index + i)] = MALLOC_FOLLOW;
		}
		if (malloc_guard) {
			if (!PI_OFF(index + i)) {
				pidx++;
				pi = pi->next;
#ifdef MALLOC_EXTRA_SANITY
				if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
					wrterror("(ES): hole in mapped pages directory");
					errno = EFAULT;
					return (NULL);
				}
#endif /* MALLOC_EXTRA_SANITY */
				pd = pi->base;
			}
			pd[PI_OFF(index + i)] = MALLOC_FIRST;
		}

		malloc_used += size << malloc_pageshift;
		malloc_guarded += malloc_guard;

		if (malloc_junk)
			memset(p, SOME_JUNK, size << malloc_pageshift);
	}
	if (delay_free) {
		if (px == NULL)
			px = delay_free;
		else
			ifree(delay_free);
	}
	return (p);
}

/*
 * Allocate a page of fragments
 */

static __inline__ int
malloc_make_chunks(int bits)
{
	struct pginfo	*bp, **pd;
	struct pdinfo	*pi;
#ifdef	MALLOC_EXTRA_SANITY
	u_long		pidx;
#endif	/* MALLOC_EXTRA_SANITY */
	void		*pp;
	long		i, k;
	size_t		l;

	/* Allocate a new bucket */
	pp = malloc_pages((size_t) malloc_pagesize);
	if (pp == NULL)
		return (0);

	/* Find length of admin structure */
	l = sizeof *bp - sizeof(u_long);
	l += sizeof(u_long) *
	    (((malloc_pagesize >> bits) + MALLOC_BITS - 1) / MALLOC_BITS);

	/* Don't waste more than two chunks on this */

	/*
	 * If we are to allocate a memory protected page for the malloc(0)
	 * case (when bits=0), it must be from a different page than the
	 * pginfo page.
	 * --> Treat it like the big chunk alloc, get a second data page.
	 */
	if (bits != 0 && (1UL << (bits)) <= l + l) {
		bp = (struct pginfo *) pp;
	} else {
		bp = (struct pginfo *) imalloc(l);
		if (bp == NULL) {
			ifree(pp);
			return (0);
		}
	}

	/* memory protect the page allocated in the malloc(0) case */
	if (bits == 0) {
		bp->size = 0;
		bp->shift = 1;
		i = malloc_minsize - 1;
		while (i >>= 1)
			bp->shift++;
		bp->total = bp->free = malloc_pagesize >> bp->shift;
		bp->page = pp;

		k = mprotect(pp, malloc_pagesize, PROT_NONE);
		if (k < 0) {
			ifree(pp);
			ifree(bp);
			return (0);
		}
	} else {
		bp->size = (1UL << bits);
		bp->shift = bits;
		bp->total = bp->free = malloc_pagesize >> bits;
		bp->page = pp;
	}

	/* set all valid bits in the bitmap */
	k = bp->total;
	i = 0;

	/* Do a bunch at a time */
	for (; (k - i) >= MALLOC_BITS; i += MALLOC_BITS)
		bp->bits[i / MALLOC_BITS] = ~0UL;

	for (; i < k; i++)
		bp->bits[i / MALLOC_BITS] |= 1UL << (i % MALLOC_BITS);

	k = (long)l;
	if (bp == bp->page) {
		/* Mark the ones we stole for ourselves */
		for (i = 0; k > 0; i++) {
			bp->bits[i / MALLOC_BITS] &= ~(1UL << (i % MALLOC_BITS));
			bp->free--;
			bp->total--;
			k -= (1 << bits);
		}
	}
	/* MALLOC_LOCK */

	pdir_lookup(ptr2index(pp), &pi);
#ifdef MALLOC_EXTRA_SANITY
	pidx = PI_IDX(ptr2index(pp));
	if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
		wrterror("(ES): mapped pages not found in directory");
		errno = EFAULT;
		return (0);
	}
#endif /* MALLOC_EXTRA_SANITY */
	if (pi != last_dir) {
		prev_dir = last_dir;
		last_dir = pi;
	}
	pd = pi->base;
	pd[PI_OFF(ptr2index(pp))] = bp;

	bp->next = page_dir[bits];
	page_dir[bits] = bp;

	/* MALLOC_UNLOCK */
	return (1);
}

/*
 * Allocate a fragment
 */
static void *
malloc_bytes(size_t size)
{
	int		i, j;
	size_t		k;
	u_long		u, *lp;
	struct pginfo	*bp;

	/* Don't bother with anything less than this */
	/* unless we have a malloc(0) requests */
	if (size != 0 && size < malloc_minsize)
		size = malloc_minsize;

	/* Find the right bucket */
	if (size == 0)
		j = 0;
	else {
		size_t ii;
		j = 1;
		ii = size - 1;
		while (ii >>= 1)
			j++;
	}

	/* If it's empty, make a page more of that size chunks */
	if (page_dir[j] == NULL && !malloc_make_chunks(j))
		return (NULL);

	bp = page_dir[j];

	/* Find first word of bitmap which isn't empty */
	for (lp = bp->bits; !*lp; lp++);

	/* Find that bit, and tweak it */
	u = 1;
	k = 0;
	while (!(*lp & u)) {
		u += u;
		k++;
	}

	if (malloc_guard) {
		/* Walk to a random position. */
//		i = arc4random() % bp->free;
		i = rand() % bp->free;
		while (i > 0) {
			u += u;
			k++;
			if (k >= MALLOC_BITS) {
				lp++;
				u = 1;
				k = 0;
			}
#ifdef MALLOC_EXTRA_SANITY
			if (lp - bp->bits > (bp->total - 1) / MALLOC_BITS) {
				wrterror("chunk overflow");
				errno = EFAULT;
				return (NULL);
			}
#endif /* MALLOC_EXTRA_SANITY */
			if (*lp & u)
				i--;
		}
	}
	*lp ^= u;

	/* If there are no more free, remove from free-list */
	if (!--bp->free) {
		page_dir[j] = bp->next;
		bp->next = NULL;
	}
	/* Adjust to the real offset of that chunk */
	k += (lp - bp->bits) * MALLOC_BITS;
	k <<= bp->shift;

	if (malloc_junk && bp->size != 0)
		memset((char *)bp->page + k, SOME_JUNK, (size_t)bp->size);

	return ((u_char *) bp->page + k);
}

/*
 * Magic so that malloc(sizeof(ptr)) is near the end of the page.
 */
#define	PTR_GAP		(malloc_pagesize - sizeof(void *))
#define	PTR_SIZE	(sizeof(void *))
#define	PTR_ALIGNED(p)	(((unsigned long)p & malloc_pagemask) == PTR_GAP)

/*
 * Allocate a piece of memory
 */
static void *
imalloc(size_t size)
{
	void		*result;
	int		ptralloc = 0;

	if (!malloc_started)
		malloc_init();

	if (suicide)
		abort();

	/* does not matter if malloc_bytes fails */
	if (px == NULL)
		px = malloc_bytes(sizeof *px);

	if (malloc_ptrguard && size == PTR_SIZE) {
		ptralloc = 1;
		size = malloc_pagesize;
	}
	if (size > SIZE_MAX - malloc_pagesize) { /* Check for overflow */
		result = NULL;
		errno = ENOMEM;
	} else if (size <= malloc_maxsize)
		result = malloc_bytes(size);
	else
		result = malloc_pages(size);

	if (malloc_abort == 1 && result == NULL)
		wrterror("allocation failed");

	if (malloc_zero && result != NULL)
		memset(result, 0, size);

	if (result && ptralloc)
		return ((char *) result + PTR_GAP);
	return (result);
}

/*
 * Change the size of an allocation.
 */
static void *
irealloc(void *ptr, size_t size)
{
	void		*p;
	size_t		osize;
	u_long		index, i;
	struct pginfo	**mp;
	struct pginfo	**pd;
	struct pdinfo	*pi;
#ifdef	MALLOC_EXTRA_SANITY
	u_long		pidx;
#endif	/* MALLOC_EXTRA_SANITY */

	if (suicide)
		abort();

	if (!malloc_started) {
		wrtwarning("malloc() has never been called");
		return (NULL);
	}
	if (malloc_ptrguard && PTR_ALIGNED(ptr)) {
		if (size <= PTR_SIZE)
			return (ptr);

		p = imalloc(size);
		if (p)
			memcpy(p, ptr, PTR_SIZE);
		ifree(ptr);
		return (p);
	}
	index = ptr2index(ptr);

	if (index < malloc_pageshift) {
		wrtwarning("junk pointer, too low to make sense");
		return (NULL);
	}
	if (index > last_index) {
		wrtwarning("junk pointer, too high to make sense");
		return (NULL);
	}
	pdir_lookup(index, &pi);
#ifdef MALLOC_EXTRA_SANITY
	pidx = PI_IDX(index);
	if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
		wrterror("(ES): mapped pages not found in directory");
		errno = EFAULT;
		return (NULL);
	}
#endif /* MALLOC_EXTRA_SANITY */
	if (pi != last_dir) {
		prev_dir = last_dir;
		last_dir = pi;
	}
	pd = pi->base;
	mp = &pd[PI_OFF(index)];

	if (*mp == MALLOC_FIRST) {	/* Page allocation */

		/* Check the pointer */
		if ((u_long) ptr & malloc_pagemask) {
			wrtwarning("modified (page-) pointer");
			return (NULL);
		}
		/* Find the size in bytes */
		i = index;
		if (!PI_OFF(++i)) {
			pi = pi->next;
			if (pi != NULL && PD_IDX(pi->dirnum) != PI_IDX(i))
				pi = NULL;
			if (pi != NULL)
				pd = pi->base;
		}
		for (osize = malloc_pagesize;
		    pi != NULL && pd[PI_OFF(i)] == MALLOC_FOLLOW;) {
			osize += malloc_pagesize;
			if (!PI_OFF(++i)) {
				pi = pi->next;
				if (pi != NULL && PD_IDX(pi->dirnum) != PI_IDX(i))
					pi = NULL;
				if (pi != NULL)
					pd = pi->base;
			}
		}

		if (!malloc_realloc && size <= osize &&
		    size > osize - malloc_pagesize) {
			if (malloc_junk)
				memset((char *)ptr + size, SOME_JUNK, osize - size);
			return (ptr);	/* ..don't do anything else. */
		}
	} else if (*mp >= MALLOC_MAGIC) {	/* Chunk allocation */

		/* Check the pointer for sane values */
		if ((u_long) ptr & ((1UL << ((*mp)->shift)) - 1)) {
			wrtwarning("modified (chunk-) pointer");
			return (NULL);
		}
		/* Find the chunk index in the page */
		i = ((u_long) ptr & malloc_pagemask) >> (*mp)->shift;

		/* Verify that it isn't a free chunk already */
		if ((*mp)->bits[i / MALLOC_BITS] & (1UL << (i % MALLOC_BITS))) {
			wrtwarning("chunk is already free");
			return (NULL);
		}
		osize = (*mp)->size;

		if (!malloc_realloc && size <= osize &&
		    (size > osize / 2 || osize == malloc_minsize)) {
			if (malloc_junk)
				memset((char *) ptr + size, SOME_JUNK, osize - size);
			return (ptr);	/* ..don't do anything else. */
		}
	} else {
		wrtwarning("irealloc: pointer to wrong page");
		return (NULL);
	}

	p = imalloc(size);

	if (p != NULL) {
		/* copy the lesser of the two sizes, and free the old one */
		/* Don't move from/to 0 sized region !!! */
		if (osize != 0 && size != 0) {
			if (osize < size)
				memcpy(p, ptr, osize);
			else
				memcpy(p, ptr, size);
		}
		ifree(ptr);
	}
	return (p);
}

/*
 * Free a sequence of pages
 */
static __inline__ void
free_pages(void *ptr, u_long index, struct pginfo * info)
{
	u_long		i, pidx, lidx;
	size_t		l, cachesize = 0;
	struct pginfo	**pd;
	struct pdinfo	*pi, *spi;
	struct pgfree	*pf, *pt = NULL;
	caddr_t		tail;

	if (info == MALLOC_FREE) {
		wrtwarning("page is already free");
		return;
	}
	if (info != MALLOC_FIRST) {
		wrtwarning("free_pages: pointer to wrong page");
		return;
	}
	if ((u_long) ptr & malloc_pagemask) {
		wrtwarning("modified (page-) pointer");
		return;
	}
	/* Count how many pages and mark them free at the same time */
	pidx = PI_IDX(index);
	pdir_lookup(index, &pi);
#ifdef MALLOC_EXTRA_SANITY
	if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
		wrterror("(ES): mapped pages not found in directory");
		errno = EFAULT;
		return;
	}
#endif /* MALLOC_EXTRA_SANITY */

	spi = pi;		/* Save page index for start of region. */

	pd = pi->base;
	pd[PI_OFF(index)] = MALLOC_FREE;
	i = 1;
	if (!PI_OFF(index + i)) {
		pi = pi->next;
		if (pi == NULL || PD_IDX(pi->dirnum) != PI_IDX(index + i))
			pi = NULL;
		else
			pd = pi->base;
	}
	while (pi != NULL && pd[PI_OFF(index + i)] == MALLOC_FOLLOW) {
		pd[PI_OFF(index + i)] = MALLOC_FREE;
		i++;
		if (!PI_OFF(index + i)) {
			if ((pi = pi->next) == NULL ||
			    PD_IDX(pi->dirnum) != PI_IDX(index + i))
				pi = NULL;
			else
				pd = pi->base;
		}
	}

	l = i << malloc_pageshift;

	if (malloc_junk)
		memset(ptr, SOME_JUNK, l);

	malloc_used -= l;
	malloc_guarded -= malloc_guard;
	if (malloc_guard) {
#ifdef MALLOC_EXTRA_SANITY
		if (pi == NULL || PD_IDX(pi->dirnum) != PI_IDX(index + i)) {
			wrterror("(ES): hole in mapped pages directory");
			errno = EFAULT;
			return;
		}
#endif /* MALLOC_EXTRA_SANITY */
		pd[PI_OFF(index + i)] = MALLOC_FREE;
		l += malloc_guard;
	}
	tail = (caddr_t)ptr + l;

	if (malloc_hint)
		madvise(ptr, l, MADV_FREE);

	if (malloc_freeprot)
		mprotect(ptr, l, PROT_NONE);

	/* Add to free-list. */
	if (px == NULL && (px = malloc_bytes(sizeof *px)) == NULL)
			goto not_return;
	px->page = ptr;
	px->pdir = spi;
	px->size = l;

	if (free_list.next == NULL) {
		/* Nothing on free list, put this at head. */
		px->next = NULL;
		px->prev = &free_list;
		free_list.next = px;
		pf = px;
		px = NULL;
	} else {
		/*
		 * Find the right spot, leave pf pointing to the modified
		 * entry.
		 */

		/* Race ahead here, while calculating cache size. */
		for (pf = free_list.next;
		    (caddr_t)ptr > ((caddr_t)pf->page + pf->size)
		    && pf->next != NULL;
		    pf = pf->next)
			cachesize += pf->size;

		/* Finish cache size calculation. */
		pt = pf;
		while (pt) {
			cachesize += pt->size;
			pt = pt->next;
		}

		if ((caddr_t)pf->page > tail) {
			/* Insert before entry */
			px->next = pf;
			px->prev = pf->prev;
			pf->prev = px;
			px->prev->next = px;
			pf = px;
			px = NULL;
		} else if (((caddr_t)pf->page + pf->size) == ptr) {
			/* Append to the previous entry. */
			cachesize -= pf->size;
			pf->size += l;
			if (pf->next != NULL &&
			    pf->next->page == ((caddr_t)pf->page + pf->size)) {
				/* And collapse the next too. */
				pt = pf->next;
				pf->size += pt->size;
				pf->next = pt->next;
				if (pf->next != NULL)
					pf->next->prev = pf;
			}
		} else if (pf->page == tail) {
			/* Prepend to entry. */
			cachesize -= pf->size;
			pf->size += l;
			pf->page = ptr;
			pf->pdir = spi;
		} else if (pf->next == NULL) {
			/* Append at tail of chain. */
			px->next = NULL;
			px->prev = pf;
			pf->next = px;
			pf = px;
			px = NULL;
		} else {
			wrterror("freelist is destroyed");
			errno = EFAULT;
			return;
		}
	}

	if (pf->pdir != last_dir) {
		prev_dir = last_dir;
		last_dir = pf->pdir;
	}

	/* Return something to OS ? */
	if (pf->size > (malloc_cache - cachesize)) {

		/*
		 * Keep the cache intact.  Notice that the '>' above guarantees that
		 * the pf will always have at least one page afterwards.
		 */
		if (munmap((char *) pf->page + (malloc_cache - cachesize),
		    pf->size - (malloc_cache - cachesize)) != 0)
			goto not_return;
		tail = (caddr_t)pf->page + pf->size;
		lidx = ptr2index(tail) - 1;
		pf->size = malloc_cache - cachesize;

		index = ptr2index((caddr_t)pf->page + pf->size);

		pidx = PI_IDX(index);
		if (prev_dir != NULL && PD_IDX(prev_dir->dirnum) >= pidx)
			prev_dir = NULL;	/* Will be wiped out below ! */

		for (pi = pf->pdir; pi != NULL && PD_IDX(pi->dirnum) < pidx;
		    pi = pi->next)
			;

		spi = pi;
		if (pi != NULL && PD_IDX(pi->dirnum) == pidx) {
			pd = pi->base;

			for (i = index; i <= lidx;) {
				if (pd[PI_OFF(i)] != MALLOC_NOT_MINE) {
					pd[PI_OFF(i)] = MALLOC_NOT_MINE;
#ifdef MALLOC_EXTRA_SANITY
					if (!PD_OFF(pi->dirnum)) {
						wrterror("(ES): pages directory underflow");
						errno = EFAULT;
						return;
					}
#endif /* MALLOC_EXTRA_SANITY */
					pi->dirnum--;
				}
#ifdef MALLOC_EXTRA_SANITY
				else
					wrtwarning("(ES): page already unmapped");
#endif /* MALLOC_EXTRA_SANITY */
				i++;
				if (!PI_OFF(i)) {
					/*
					 * If no page in that dir, free
					 * directory page.
					 */
					if (!PD_OFF(pi->dirnum)) {
						/* Remove from list. */
						if (spi == pi)
							spi = pi->prev;
						if (pi->prev != NULL)
							pi->prev->next = pi->next;
						if (pi->next != NULL)
							pi->next->prev = pi->prev;
						pi = pi->next;
						munmap(pd, malloc_pagesize);
					} else
						pi = pi->next;
					if (pi == NULL ||
					    PD_IDX(pi->dirnum) != PI_IDX(i))
						break;
					pd = pi->base;
				}
			}
			if (pi && !PD_OFF(pi->dirnum)) {
				/* Resulting page dir is now empty. */
				/* Remove from list. */
				if (spi == pi)	/* Update spi only if first. */
					spi = pi->prev;
				if (pi->prev != NULL)
					pi->prev->next = pi->next;
				if (pi->next != NULL)
					pi->next->prev = pi->prev;
				pi = pi->next;
				munmap(pd, malloc_pagesize);
			}
		}
		if (pi == NULL && malloc_brk == tail) {
			/* Resize down the malloc upper boundary. */
			last_index = index - 1;
			malloc_brk = index2ptr(index);
		}

		/* XXX: We could realloc/shrink the pagedir here I guess. */
		if (pf->size == 0) {	/* Remove from free-list as well. */
			if (px)
				ifree(px);
			if ((px = pf->prev) != &free_list) {
				if (pi == NULL && last_index == (index - 1)) {
					if (spi == NULL) {
						malloc_brk = NULL;
						i = 11;
					} else {
						pd = spi->base;
						if (PD_IDX(spi->dirnum) < pidx)
							index =
							    ((PD_IDX(spi->dirnum) + 1) *
							    pdi_mod) - 1;
						for (pi = spi, i = index;
						    pd[PI_OFF(i)] == MALLOC_NOT_MINE;
						    i--)
#ifdef MALLOC_EXTRA_SANITY
							if (!PI_OFF(i)) {
								pi = pi->prev;
								if (pi == NULL || i == 0)
									break;
								pd = pi->base;
								i = (PD_IDX(pi->dirnum) + 1) * pdi_mod;
							}
#else /* !MALLOC_EXTRA_SANITY */
						{
						}
#endif /* MALLOC_EXTRA_SANITY */
						malloc_brk = index2ptr(i + 1);
					}
					last_index = i;
				}
				if ((px->next = pf->next) != NULL)
					px->next->prev = px;
			} else {
				if ((free_list.next = pf->next) != NULL)
					free_list.next->prev = &free_list;
			}
			px = pf;
			last_dir = prev_dir;
			prev_dir = NULL;
		}
	}
not_return:
	if (pt != NULL)
		ifree(pt);
}

/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */

/* ARGSUSED */
static __inline__ void
free_bytes(void *ptr, u_long index, struct pginfo * info)
{
	struct pginfo	**mp, **pd;
	struct pdinfo	*pi;
#ifdef	MALLOC_EXTRA_SANITY
	u_long		pidx;
#endif	/* MALLOC_EXTRA_SANITY */
	void		*vp;
	long		i;
	(void) index;

	/* Find the chunk number on the page */
	i = ((u_long) ptr & malloc_pagemask) >> info->shift;

	if ((u_long) ptr & ((1UL << (info->shift)) - 1)) {
		wrtwarning("modified (chunk-) pointer");
		return;
	}
	if (info->bits[i / MALLOC_BITS] & (1UL << (i % MALLOC_BITS))) {
		wrtwarning("chunk is already free");
		return;
	}
	if (malloc_junk && info->size != 0)
		memset(ptr, SOME_JUNK, (size_t)info->size);

	info->bits[i / MALLOC_BITS] |= 1UL << (i % MALLOC_BITS);
	info->free++;

	if (info->size != 0)
		mp = page_dir + info->shift;
	else
		mp = page_dir;

	if (info->free == 1) {
		/* Page became non-full */

		/* Insert in address order */
		while (*mp != NULL && (*mp)->next != NULL &&
		    (*mp)->next->page < info->page)
			mp = &(*mp)->next;
		info->next = *mp;
		*mp = info;
		return;
	}
	if (info->free != info->total)
		return;

	/* Find & remove this page in the queue */
	while (*mp != info) {
		mp = &((*mp)->next);
#ifdef MALLOC_EXTRA_SANITY
		if (!*mp) {
			wrterror("(ES): Not on queue");
			errno = EFAULT;
			return;
		}
#endif /* MALLOC_EXTRA_SANITY */
	}
	*mp = info->next;

	/* Free the page & the info structure if need be */
	pdir_lookup(ptr2index(info->page), &pi);
#ifdef MALLOC_EXTRA_SANITY
	pidx = PI_IDX(ptr2index(info->page));
	if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
		wrterror("(ES): mapped pages not found in directory");
		errno = EFAULT;
		return;
	}
#endif /* MALLOC_EXTRA_SANITY */
	if (pi != last_dir) {
		prev_dir = last_dir;
		last_dir = pi;
	}
	pd = pi->base;
	pd[PI_OFF(ptr2index(info->page))] = MALLOC_FIRST;

	/* If the page was mprotected, unprotect it before releasing it */
	if (info->size == 0)
		mprotect(info->page, malloc_pagesize, PROT_READ | PROT_WRITE);

	vp = info->page;	/* Order is important ! */
	if (vp != (void *) info)
		ifree(info);
	ifree(vp);
}

static void
ifree(void *ptr)
{
	struct pginfo	*info, **pd;
	u_long		index;
#ifdef	MALLOC_EXTRA_SANITY
	u_long		pidx;
#endif	/* MALLOC_EXTRA_SANITY */
	struct pdinfo	*pi;

	if (!malloc_started) {
		wrtwarning("malloc() has never been called");
		return;
	}
	/* If we're already sinking, don't make matters any worse. */
	if (suicide)
		return;

	if (malloc_ptrguard && PTR_ALIGNED(ptr))
		ptr = (char *) ptr - PTR_GAP;

	index = ptr2index(ptr);

	if (index < malloc_pageshift) {
		warnx("(%p)", ptr);
		wrtwarning("ifree: junk pointer, too low to make sense");
		return;
	}
	if (index > last_index) {
		warnx("(%p)", ptr);
		wrtwarning("ifree: junk pointer, too high to make sense");
		return;
	}
	pdir_lookup(index, &pi);
#ifdef MALLOC_EXTRA_SANITY
	pidx = PI_IDX(index);
	if (pi == NULL || PD_IDX(pi->dirnum) != pidx) {
		wrterror("(ES): mapped pages not found in directory");
		errno = EFAULT;
		return;
	}
#endif /* MALLOC_EXTRA_SANITY */
	if (pi != last_dir) {
		prev_dir = last_dir;
		last_dir = pi;
	}
	pd = pi->base;
	info = pd[PI_OFF(index)];

	if (info < MALLOC_MAGIC)
		free_pages(ptr, index, info);
	else
		free_bytes(ptr, index, info);

	/* does not matter if malloc_bytes fails */
	if (px == NULL)
		px = malloc_bytes(sizeof *px);

	return;
}

/*
 * Common function for handling recursion.  Only
 * print the error message once, to avoid making the problem
 * potentially worse.
 */
static void
malloc_recurse(void)
{
	static int	noprint;

	if (noprint == 0) {
		noprint = 1;
		wrtwarning("recursive call");
	}
	malloc_active--;
	_MALLOC_UNLOCK();
	errno = EDEADLK;
}

/*
 * These are the public exported interface routines.
 */
void *
malloc(size_t size)
{
	void		*r;

	if (!align)
	_MALLOC_LOCK();
	malloc_func = " in malloc():";
	if (malloc_active++) {
		malloc_recurse();
		return (NULL);
	}
	r = imalloc(size);
	UTRACE(0, size, r);
	malloc_active--;
	if (!align)
	_MALLOC_UNLOCK();
	if (malloc_xmalloc && r == NULL) {
		wrterror("out of memory");
		errno = ENOMEM;
	}
	return (r);
}

void
free(void *ptr)
{
	/* This is legal. XXX quick path */
	if (ptr == NULL)
		return;

	_MALLOC_LOCK();
	malloc_func = " in free():";
	if (malloc_active++) {
		malloc_recurse();
		return;
	}
	ifree(ptr);
	UTRACE(ptr, 0, 0);
	malloc_active--;
	_MALLOC_UNLOCK();
	return;
}

void *
realloc(void *ptr, size_t size)
{
	void		*r;

	_MALLOC_LOCK();
	malloc_func = " in realloc():";
	if (malloc_active++) {
		malloc_recurse();
		return (NULL);
	}

	if (ptr == NULL)
		r = imalloc(size);
	else
		r = irealloc(ptr, size);

	UTRACE(ptr, size, r);
	malloc_active--;
	_MALLOC_UNLOCK();
	if (malloc_xmalloc && r == NULL) {
		wrterror("out of memory");
		errno = ENOMEM;
	}
	return (r);
}

void *
calloc(size_t num, size_t size)
{
	void *p;
	
	if (num && SIZE_MAX / num < size) {
		fprintf(stderr,"OOOOPS");
		errno = ENOMEM;
		return NULL;
	}
	size *= num;
	p = malloc(size);
	if (p)
		memset(p, 0, size);
	return(p);
}

#ifndef BUILDING_FOR_TOR
static int ispowerof2 (size_t a) {
	size_t b;
	for (b = 1ULL << (sizeof(size_t)*NBBY - 1); b > 1; b >>= 1)
	  if (b == a)
		return 1;
	return 0;
}
#endif

#ifndef BUILDING_FOR_TOR
int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *r;
	size_t max;
	if ((alignment < PTR_SIZE) || (alignment%PTR_SIZE != 0)) return EINVAL;
	if (!ispowerof2(alignment)) return EINVAL;
	if (alignment < malloc_minsize) alignment = malloc_minsize;
	max = alignment > size ? alignment : size;
	if (alignment <= malloc_pagesize)
		r = malloc(max);
	else {
		_MALLOC_LOCK();
		align = 1;
		g_alignment = alignment;
		r = malloc(size);
		align=0;
		_MALLOC_UNLOCK();
	}
	*memptr = r;
	if (!r) return ENOMEM;
	return 0;
}

void *memalign(size_t boundary, size_t size)
{
	void *r;
	posix_memalign(&r, boundary, size);
	return r;
}

void *valloc(size_t size)
{
	void *r;
	posix_memalign(&r, malloc_pagesize, size);
	return r;
}
#endif

size_t malloc_good_size(size_t size)
{
	if (size == 0) {
		return 1;
	} else if (size <= malloc_maxsize) {
		int j;
		size_t ii;
		/* round up to the nearest power of 2, with same approach
		 * as malloc_bytes() uses. */
		j = 1;
		ii = size - 1;
		while (ii >>= 1)
			j++;
		return ((size_t)1) << j;
	} else {
		return pageround(size);
	}
}
