#################################
# Start SUBSYSTEM TORTURE_LOCAL
[MODULE::TORTURE_LOCAL]
SUBSYSTEM = smbtorture
OUTPUT_TYPE = MERGED_OBJ
INIT_FUNCTION = torture_local_init
PRIVATE_DEPENDENCIES = \
		RPC_NDR_ECHO \
		TDR \
		LIBCLI_SMB \
		MESSAGING \
		ICONV \
		POPT_CREDENTIALS \
		TORTURE_AUTH \
		TORTURE_UTIL \
		TORTURE_NDR \
		TORTURE_LIBCRYPTO \
		share \
		torture_registry \
		PROVISION \
		NSS_WRAPPER
# End SUBSYSTEM TORTURE_LOCAL
#################################

TORTURE_LOCAL_OBJ_FILES = \
		$(torturesrcdir)/../../lib/util/charset/tests/iconv.o \
		$(torturesrcdir)/../../lib/talloc/testsuite.o \
		$(torturesrcdir)/../../lib/replace/test/getifaddrs.o \
		$(torturesrcdir)/../../lib/replace/test/os2_delete.o \
		$(torturesrcdir)/../../lib/replace/test/strptime.o \
		$(torturesrcdir)/../../lib/replace/test/testsuite.o \
		$(torturesrcdir)/../lib/messaging/tests/messaging.o \
		$(torturesrcdir)/../lib/messaging/tests/irpc.o \
		$(torturesrcdir)/../librpc/tests/binding_string.o \
		$(torturesrcdir)/../../lib/util/tests/idtree.o \
		$(torturesrcdir)/../lib/socket/testsuite.o \
		$(torturesrcdir)/../../lib/socket_wrapper/testsuite.o \
		$(torturesrcdir)/../../lib/nss_wrapper/testsuite.o \
		$(torturesrcdir)/../libcli/resolve/testsuite.o \
		$(torturesrcdir)/../../lib/util/tests/strlist.o \
		$(torturesrcdir)/../../lib/util/tests/parmlist.o \
		$(torturesrcdir)/../../lib/util/tests/str.o \
		$(torturesrcdir)/../../lib/util/tests/time.o \
		$(torturesrcdir)/../../lib/util/tests/data_blob.o \
		$(torturesrcdir)/../../lib/util/tests/file.o \
		$(torturesrcdir)/../../lib/util/tests/genrand.o \
		$(torturesrcdir)/../../lib/compression/testsuite.o \
		$(torturesrcdir)/../../lib/util/charset/tests/charset.o \
		$(torturesrcdir)/../libcli/security/tests/sddl.o \
		$(libtdrsrcdir)/testsuite.o \
		$(torturesrcdir)/../../lib/tevent/testsuite.o \
		$(torturesrcdir)/../param/tests/share.o \
		$(torturesrcdir)/../param/tests/loadparm.o \
		$(torturesrcdir)/../auth/credentials/tests/simple.o \
		$(torturesrcdir)/local/local.o \
		$(torturesrcdir)/local/dbspeed.o \
		$(torturesrcdir)/local/torture.o \
		$(torturesrcdir)/ldb/ldb.o

$(eval $(call proto_header_template,$(torturesrcdir)/local/proto.h,$(TORTURE_LOCAL_OBJ_FILES:.o=.c)))
