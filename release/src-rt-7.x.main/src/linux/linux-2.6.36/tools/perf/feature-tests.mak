define SOURCE_HELLO
#include <stdio.h>
int main(void)
{
	return puts(\"hi\");
}
endef

ifndef NO_DWARF
define SOURCE_DWARF
#include <dwarf.h>
#include <libdw.h>
#include <version.h>
#ifndef _ELFUTILS_PREREQ
#error
#endif

int main(void)
{
	Dwarf *dbg = dwarf_begin(0, DWARF_C_READ);
	return (long)dbg;
}
endef
endif

define SOURCE_LIBELF
#include <libelf.h>

int main(void)
{
	Elf *elf = elf_begin(0, ELF_C_READ, 0);
	return (long)elf;
}
endef

define SOURCE_GLIBC
#include <gnu/libc-version.h>

int main(void)
{
	const char *version = gnu_get_libc_version();
	return (long)version;
}
endef

define SOURCE_ELF_MMAP
#include <libelf.h>
int main(void)
{
	Elf *elf = elf_begin(0, ELF_C_READ_MMAP, 0);
	return (long)elf;
}
endef

ifndef NO_NEWT
define SOURCE_NEWT
#include <newt.h>

int main(void)
{
	newtInit();
	newtCls();
	return newtFinished();
}
endef
endif

ifndef NO_LIBPERL
define SOURCE_PERL_EMBED
#include <EXTERN.h>
#include <perl.h>

int main(void)
{
perl_alloc();
return 0;
}
endef
endif

ifndef NO_LIBPYTHON
define SOURCE_PYTHON_EMBED
#include <Python.h>

int main(void)
{
	Py_Initialize();
	return 0;
}
endef
endif

define SOURCE_BFD
#include <bfd.h>

int main(void)
{
	bfd_demangle(0, 0, 0);
	return 0;
}
endef

define SOURCE_CPLUS_DEMANGLE
extern char *cplus_demangle(const char *, int);

int main(void)
{
	cplus_demangle(0, 0);
	return 0;
}
endef

# try-cc
# Usage: option = $(call try-cc, source-to-build, cc-options)
try-cc = $(shell sh -c						  \
	'TMP="$(OUTPUT)$(TMPOUT).$$$$";				  \
	 echo "$(1)" |						  \
	 $(CC) -x c - $(2) -o "$$TMP" > /dev/null 2>&1 && echo y; \
	 rm -f "$$TMP"')
