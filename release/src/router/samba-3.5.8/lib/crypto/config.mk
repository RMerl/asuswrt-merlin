##############################
# Start SUBSYSTEM LIBCRYPTO
[SUBSYSTEM::LIBCRYPTO]
# End SUBSYSTEM LIBCRYPTO
##############################

LIBCRYPTO_OBJ_FILES = $(addprefix $(libcryptosrcdir)/, \
					 crc32.o md5.o hmacmd5.o md4.o \
					 arcfour.o sha256.o hmacsha256.o \
					 aes.o rijndael-alg-fst.o)

[SUBSYSTEM::TORTURE_LIBCRYPTO]
PRIVATE_DEPENDENCIES = LIBCRYPTO

TORTURE_LIBCRYPTO_OBJ_FILES = $(addprefix $(libcryptosrcdir)/, \
		md4test.o md5test.o hmacmd5test.o)

$(eval $(call proto_header_template,$(libcryptosrcdir)/test_proto.h,$(TORTURE_LIBCRYPTO_OBJ_FILES:.o=.c)))
