# samba 4 bits

PROG_LD = $(LD)
BNLD = $(CC)
HOSTLD = $(CC)
PARTLINK = $(PROG_LD) -r
MDLD = $(SHLD)
MDLD_FLAGS = $(LDSHFLAGS) 
shliboutputdir = bin/shared

samba4srcdir = $(srcdir)/../source4

# Flags used for the samba 4 files
# $(srcdir)/include is required for config.h
SAMBA4_CFLAGS = -I.. -I$(samba4srcdir) -I$(samba4srcdir)/include \
		 -I$(samba4srcdir)/../lib/replace -I$(samba4srcdir)/lib \
		 -I$(heimdalsrcdir)/lib/hcrypto -I$(tallocdir) \
		 -I$(srcdir)/include -D_SAMBA_BUILD_=4 -DHAVE_CONFIG_H

.SUFFIXES: .ho

# No cross compilation for now, thanks
.c.ho:
	@if (: >> $@ || : > $@) >/dev/null 2>&1; then rm -f $@; else \
	 dir=`echo $@ | sed 's,/[^/]*$$,,;s,^$$,.,'` $(MAKEDIR); fi
	@if test -n "$(CC_CHECKER)"; then \
	  echo "Checking  $*.c with '$(CC_CHECKER)'";\
	  $(CHECK_CC); \
	 fi
	@echo Compiling $*.c
	@$(COMPILE) && exit 0;\
		echo "The following command failed:" 1>&2;\
		echo "$(subst ",\",$(COMPILE_CC))" 1>&2;\
		$(COMPILE_CC) >/dev/null 2>&1

# The order really does matter here! GNU Make 3.80 will break if the more specific 
# overrides are not specified first.
ifeq ($(MAKE_VERSION),3.81)
%.o: CFLAGS+=$(FLAGS)
../librpc/gen_ndr/%_c.o: CFLAGS=$(SAMBA4_CFLAGS)
../librpc/gen_ndr/py_%.o: CFLAGS=$(SAMBA4_CFLAGS)
$(samba4srcdir)/%.o: CFLAGS=$(SAMBA4_CFLAGS)
$(samba4srcdir)/%.ho: CFLAGS=$(SAMBA4_CFLAGS)
$(heimdalsrcdir)/%.o: CFLAGS=-I$(heimdalbuildsrcdir) $(SAMBA4_CFLAGS) -I$(srcdir)
$(heimdalsrcdir)/%.ho: CFLAGS=-I$(heimdalbuildsrcdir) $(SAMBA4_CFLAGS) -I$(srcdir)
else
$(heimdalsrcdir)/%.o: CFLAGS=-I$(heimdalbuildsrcdir) $(SAMBA4_CFLAGS) -I$(srcdir)
$(heimdalsrcdir)/%.ho: CFLAGS=-I$(heimdalbuildsrcdir) $(SAMBA4_CFLAGS) -I$(srcdir)
$(samba4srcdir)/%.o: CFLAGS=$(SAMBA4_CFLAGS)
$(samba4srcdir)/%.ho: CFLAGS=$(SAMBA4_CFLAGS)
../librpc/gen_ndr/%_c.o: CFLAGS=$(SAMBA4_CFLAGS)
../librpc/gen_ndr/py_%.o: CFLAGS=$(SAMBA4_CFLAGS)
%.o: CFLAGS+=$(FLAGS)
endif

# Create a static library
%.a:
	@echo Linking $@
	@rm -f $@
	@mkdir -p $(@D)
	@$(AR) -rc $@ $^

pidldir = $(samba4srcdir)/../pidl
include $(pidldir)/config.mk
include samba4-config.mk
include samba4-templates.mk

zlibsrcdir := $(samba4srcdir)/../lib/zlib
dynconfigsrcdir := $(samba4srcdir)/dynconfig
heimdalsrcdir := $(samba4srcdir)/heimdal
dsdbsrcdir := $(samba4srcdir)/dsdb
smbdsrcdir := $(samba4srcdir)/smbd
clustersrcdir := $(samba4srcdir)/cluster
libnetsrcdir := $(samba4srcdir)/libnet
authsrcdir := $(samba4srcdir)/auth
nsswitchsrcdir := $(samba4srcdir)/../nsswitch
libwbclientsrcdir := $(nsswitchsrcdir)/libwbclient
libsrcdir := $(samba4srcdir)/lib
libsocketsrcdir := $(samba4srcdir)/lib/socket
libcharsetsrcdir := $(samba4srcdir)/../lib/util/charset
ldb_sambasrcdir := $(samba4srcdir)/lib/ldb-samba
libtlssrcdir := $(samba4srcdir)/lib/tls
libregistrysrcdir := $(samba4srcdir)/lib/registry
libmessagingsrcdir := $(samba4srcdir)/lib/messaging
libteventsrcdir := $(samba4srcdir)/../lib/tevent
libeventssrcdir := $(samba4srcdir)/lib/events
libcmdlinesrcdir := $(samba4srcdir)/lib/cmdline
poptsrcdir := $(samba4srcdir)/../lib/popt
socketwrappersrcdir := $(samba4srcdir)/../lib/socket_wrapper
nsswrappersrcdir := $(samba4srcdir)/../lib/nss_wrapper
uidwrappersrcdir := $(samba4srcdir)/../lib/uid_wrapper
libstreamsrcdir := $(samba4srcdir)/lib/stream
libutilsrcdir := $(samba4srcdir)/../lib/util
libtdrsrcdir := ../lib/tdr
libcryptosrcdir := $(samba4srcdir)/../lib/crypto
libtorturesrcdir := ../lib/torture
libcompressionsrcdir := $(samba4srcdir)/../lib/compression
libgencachesrcdir := $(samba4srcdir)/lib
paramsrcdir := $(samba4srcdir)/param
smb_serversrcdir := $(samba4srcdir)/smb_server
rpc_serversrcdir := $(samba4srcdir)/rpc_server
ldap_serversrcdir := $(samba4srcdir)/ldap_server
web_serversrcdir := $(samba4srcdir)/web_server
winbindsrcdir := $(samba4srcdir)/winbind
nbt_serversrcdir := $(samba4srcdir)/nbt_server
wrepl_serversrcdir := $(samba4srcdir)/wrepl_server
cldap_serversrcdir := $(samba4srcdir)/cldap_server
librpcsrcdir := $(samba4srcdir)/librpc
torturesrcdir := $(samba4srcdir)/torture
utilssrcdir := $(samba4srcdir)/utils
ntvfssrcdir := $(samba4srcdir)/ntvfs
ntptrsrcdir := $(samba4srcdir)/ntptr
clientsrcdir := $(samba4srcdir)/client
libclisrcdir := $(samba4srcdir)/libcli
libclinbtsrcdir := $(samba4srcdir)/../libcli/nbt
libclicommonsrcdir := $(samba4srcdir)/../libcli
pyscriptsrcdir := $(samba4srcdir)/scripting/python
kdcsrcdir := $(samba4srcdir)/kdc
smbreadlinesrcdir := $(samba4srcdir)/lib/smbreadline
ntp_signdsrcdir := $(samba4srcdir)/ntp_signd
tdbsrcdir := $(samba4srcdir)/../lib/tdb
ldbsrcdir := $(samba4srcdir)/lib/ldb
tallocsrcdir := $(samba4srcdir)/../lib/talloc
comsrcdir := $(samba4srcdir)/lib/com
override ASN1C = bin/asn1_compile4
override ET_COMPILER = bin/compile_et4
#include $(samba4srcdir)/build/make/python.mk
include samba4-data.mk
include $(samba4srcdir)/static_deps.mk

INSTALLPERMS = 0755
$(foreach SCRIPT,$(wildcard scripting/bin/*),$(eval $(call binary_install_template,$(SCRIPT))))

$(DESTDIR)$(bindir)/%4: bin/%4 installdirs
	@mkdir -p $(@D)
	@echo Installing $(@F) as $@
	@if test -f $@; then rm -f $@.old; mv $@ $@.old; fi
	@cp $< $@
	@chmod $(INSTALLPERMS) $@

$(DESTDIR)$(sbindir)/%4: bin/%4 installdirs
	@mkdir -p $(@D)
	@echo Installing $(@F) as $@
	@if test -f $@; then rm -f $@.old; mv $@ $@.old; fi
	@cp $< $@
	@chmod $(INSTALLPERMS) $@

clean:: 
	@echo Removing samba 4 objects
	@-find $(samba4srcdir) -name '*.o' -exec rm -f '{}' \;
	@echo Removing samba 4 hostcc objects
	@-find $(samba4srcdir) -name '*.ho' -exec rm -f '{}' \;
	@echo Removing samba 4 libraries
	@-rm -f $(STATIC_LIBS) $(SHARED_LIBS)
	@-rm -f bin/static/*.a $(shliboutputdir)/*.$(SHLIBEXT) bin/mergedobj/*.o
	@echo Removing samba 4 modules
	@-rm -f bin/modules/*/*.$(SHLIBEXT)
	@-rm -f bin/*_init_module.c
	@echo Removing samba 4 dummy targets
	@-rm -f bin/.*_*
	@echo Removing samba 4 generated files
	@-rm -f bin/*_init_module.c
	@-rm -rf $(samba4srcdir)/librpc/gen_* 

proto:: $(PROTO_HEADERS)
modules:: $(PLUGINS)

#pythonmods:: $(PYTHON_PYS) $(PYTHON_SO)

all:: bin/samba4 bin/regpatch4 bin/regdiff4 bin/regshell4 bin/regtree4 bin/smbclient4 setup plugins
torture:: bin/smbtorture4

#
## This is a fake rule to stop any python being invoked as currently the
## build system is broken in source3 with python. JRA.
#
installpython:: bin/smbtorture4

everything:: $(patsubst %,%4,$(BINARIES))
setup:
	@ln -sf ../source4/setup setup

S4_LD_LIBPATH_OVERRIDE = $(LIB_PATH_VAR)="$(builddir)/bin/shared:$$$(LIB_PATH_VAR)"

SELFTEST4 = $(S4_LD_LIBPATH_OVERRIDE) EXEEXT="4" PYTHON="$(PYTHON)" PERL="$(PERL)" \
    $(PERL) $(selftestdir)/selftest.pl --prefix=st4 \
    --builddir=$(builddir) --srcdir=$(samba4srcdir) \
    --exeext=4 \
    --expected-failures=$(samba4srcdir)/selftest/knownfail \
	--format=$(SELFTEST_FORMAT) \
    --exclude=$(samba4srcdir)/selftest/skip --testlist="$(samba4srcdir)/selftest/tests.sh|" \
    $(TEST4_OPTIONS) 

SELFTEST4_NOSLOW_OPTS = --exclude=$(samba4srcdir)/selftest/slow
SELFTEST4_QUICK_OPTS = $(SELFTEST4_NOSLOW_OPTS) --quick --include=$(samba4srcdir)/selftest/quick

slowtest4:: everything
	$(SELFTEST4) $(DEFAULT_TEST_OPTIONS) --immediate $(TESTS)

test4:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) $(DEFAULT_TEST_OPTIONS) --immediate \
		$(TESTS)

testone4:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) $(DEFAULT_TEST_OPTIONS) --one $(TESTS)

test4-swrap:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper --immediate $(TESTS)

test4-swrap-pcap:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper-pcap --immediate $(TESTS)

test4-swrap-keep-pcap:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper-keep-pcap --immediate $(TESTS)

test4-noswrap:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --immediate $(TESTS)

quicktest4:: all
	$(SELFTEST4) $(SELFTEST4_QUICK_OPTS) --socket-wrapper --immediate $(TESTS)

quicktestone4:: all
	$(SELFTEST4) $(SELFTEST4_QUICK_OPTS) --socket-wrapper --one $(TESTS)

testenv4:: everything
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper --testenv

testenv4-%:: everything
	SELFTEST_TESTENV=$* $(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper --testenv

test4-%:: 
	$(MAKE) test TESTS=$*

valgrindtest4:: valgrindtest-all

valgrindtest4-quick:: all
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/valgrind_run $(LD_LIBPATH_OVERRIDE)" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST4) $(SELFTEST4_QUICK_OPTS) --immediate --socket-wrapper $(TESTS)

valgrindtest4-all:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/valgrind_run $(LD_LIBPATH_OVERRIDE)" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --immediate --socket-wrapper $(TESTS)

valgrindtest4-env:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/valgrind_run $(LD_LIBPATH_OVERRIDE)" \
	VALGRIND="valgrind -q --num-callers=30 --log-file=${selftest_prefix}/valgrind.log" \
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper --testenv

gdbtest4:: gdbtest4-all

gdbtest4-quick:: all
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/gdb_run $(LD_LIBPATH_OVERRIDE)" \
	$(SELFTEST4) $(SELFTEST4_QUICK_OPTS) --immediate --socket-wrapper $(TESTS)

gdbtest4-all:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/gdb_run $(LD_LIBPATH_OVERRIDE)" \
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --immediate --socket-wrapper $(TESTS)

gdbtest4-env:: everything
	SAMBA_VALGRIND="xterm -n server -e $(selftestdir)/gdb_run $(LD_LIBPATH_OVERRIDE)" \
	$(SELFTEST4) $(SELFTEST4_NOSLOW_OPTS) --socket-wrapper --testenv

plugins: $(PLUGINS)
