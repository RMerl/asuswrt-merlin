lib_LTLIBRARIES = libumem.la libumem_malloc.la
noinst_PROGRAMS = umem_test umem_test2 umem_test3

libumem_la_LDFLAGS = -lpthread -ldl

umem_test_SOURCES = umem_test.c
umem_test_LDADD = -lumem

umem_test2_SOURCES = umem_test2.c
umem_test2_LDADD = -lumem

umem_test3_SOURCES = umem_test3.c
umem_test3_LDADD = -lumem -lumem_malloc

libumem_la_SOURCES =	init_lib.c \
			umem_agent_support.c \
			umem_fail.c \
			umem_fork.c \
			umem_update_thread.c \
			vmem_mmap.c \
			vmem_sbrk.c \
			envvar.c \
			getpcstack.c \
			misc.c \
			misc.h \
			vmem_base.c \
			vmem_base.h \
			vmem_stand.h \
			umem.c \
			umem.h \
			umem_base.h \
			umem_impl.h \
			sol_compat.h \
			vmem.c \
			sys/vmem.h \
			sys/vmem_impl_user.h

libumem_malloc_la_SOURCES = malloc.c
libumem_malloc_la_LDFLAGS = -lpthread -ldl -R$(libdir) -lumem 

man3_MANS = umem_alloc.3 umem_cache_create.3 umem_debug.3
EXTRA_DIST = COPYRIGHT OPENSOLARIS.LICENSE umem.spec Doxyfile umem_test4 \
	$(man3_MANS)

nobase_include_HEADERS = umem.h sys/vmem.h

TESTS = umem_test umem_test2 umem_test3 umem_test4

html-local:
	mkdir -p docs
	doxygen Doxyfile

clean-local:
	rm -rf docs

.PHONY:	rpm
rpm:	dist-bzip2
	rpmbuild -ta $(distdir).tar.bz2

# XXX: Non-i386: SPARC asm. x86_64?
# Convert this to GNU as format: i386_subr_sol.s
# <http://cvs.opensolaris.org/source/xref/on/usr/src/lib/libumem/>
