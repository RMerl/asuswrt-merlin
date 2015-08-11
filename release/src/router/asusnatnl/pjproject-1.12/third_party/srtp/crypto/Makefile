# Makefile for libcryptomodule.a
#
# David A. McGrew
# Cisco Systems, Inc.

srcdir = .
top_srcdir = ..
top_builddir = ../


CC	= gcc
INCDIR	= -Iinclude -I$(srcdir)/include
DEFS	= -DHAVE_CONFIG_H
CPPFLAGS= 
CFLAGS	= -Wall -O4 -fexpensive-optimizations -funroll-loops
LIBS	= 
LDFLAGS	=  -L.
COMPILE = $(CC) $(DEFS) $(INCDIR) $(CPPFLAGS) $(CFLAGS)
CRYPTOLIB = -lcryptomodule

RANLIB	= ranlib

# EXE defines the suffix on executables - it's .exe for cygwin, and
# null on linux, bsd, and OS X and other OSes.  we define this so that
# `make clean` will work on the cygwin platform
EXE = 
# Random source.
RNG_OBJS = rand_source.o

ifdef ARCH
  DEFS += -D$(ARCH)=1
endif

ifdef sysname
  DEFS += -D$(sysname)=1
endif

.PHONY: dummy all runtest clean superclean

dummy : all runtest 

# test applications 

testapp = test/cipher_driver$(EXE) test/datatypes_driver$(EXE) \
	  test/stat_driver$(EXE) test/sha1_driver$(EXE) \
	  test/kernel_driver$(EXE) test/aes_calc$(EXE) test/rand_gen$(EXE) \
	  test/env$(EXE)

# data values used to test the aes_calc application

k=000102030405060708090a0b0c0d0e0f
p=00112233445566778899aabbccddeeff
c=69c4e0d86a7b0430d8cdb78070b4c55a

runtest: libcryptomodule.a $(testapp)
	test/env$(EXE) # print out information on the build environment
	@echo "running libcryptomodule test applications..."
	test `test/aes_calc $k $p` = $c
	test/cipher_driver$(EXE) -v >/dev/null
	test/datatypes_driver$(EXE) -v >/dev/null
	test/stat_driver$(EXE) >/dev/null
	test/sha1_driver$(EXE) -v >/dev/null
	test/kernel_driver$(EXE) -v >/dev/null
	test/rand_gen$(EXE) -n 256 >/dev/null
	@echo "libcryptomodule test applications passed."

# libcryptomodule.a (the crypto engine) 

ciphers = cipher/cipher.o cipher/null_cipher.o      \
          cipher/aes.o cipher/aes_icm.o             \
          cipher/aes_cbc.o

hashes  = hash/null_auth.o hash/sha1.o \
          hash/hmac.o hash/auth.o

math    = math/datatypes.o math/stat.o

rng     = rng/$(RNG_OBJS) rng/rand_source.o rng/prng.o rng/ctr_prng.o

err     = kernel/err.o

kernel  = kernel/crypto_kernel.o  kernel/alloc.o   \
          kernel/key.o $(rng) $(err)

xfm     = ae_xfm/xfm.o

cryptobj =  $(ciphers) $(hashes) $(math) $(stat) $(kernel) $(xfm)

# the rule for making object files and test apps

%.o: %.c
	$(COMPILE) -c $< -o $@

%$(EXE): %.c libcryptomodule.a 
	$(COMPILE) $(LDFLAGS) $< -o $@ $(CRYPTOLIB) $(LIBS)

ifndef AR
  AR=ar
endif

# and the crypto module library itself

libcryptomodule.a: $(cryptobj) 
	$(AR) cr libcryptomodule.a $(cryptobj) 
	$(RANLIB) libcryptomodule.a

all: libcryptomodule.a $(testapp)

# housekeeping functions

clean:
	rm -f libcryptomodule.a
	rm -f $(testapp) *.o */*.o 
	for a in * .* */*; do if [ -f "$$a~" ] ; then rm $$a~; fi; done;
	rm -f `find . -name "*.[ch]~*~"`
	rm -rf latex

superclean: clean
	rm -f *core TAGS ktrace.out


# the target 'package' builds a compressed tar archive of the source code

distname = crypto-$(shell cat VERSION)

package: superclean
	cd ..; tar cvzf $(distname).tgz crypto/


# EOF
