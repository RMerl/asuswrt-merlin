include ../../../common.mak

LIBEAP_NAME = libeap
LIBEAP_CURRENT = 0
LIBEAP_REVISION = 0
LIBEAP_AGE = 0

LIBEAP = $(LIBEAP_NAME).so.$(LIBEAP_CURRENT).$(LIBEAP_REVISION).$(LIBEAP_AGE)
LIBEAP_SO = $(LIBEAP_NAME).so.$(LIBEAP_CURRENT)

.PHONY: all clean install uninstall

all: $(LIBEAP)
	ln -fs $(LIBEAP) $(LIBEAP_SO)
	ln -fs $(LIBEAP) $(LIBEAP_NAME).so

ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O0 -Wall -g
endif

CONFIG_TLS=openssl

INCLUDE_INSTALL_DIR=/usr/include/eap_peer

# Got to use override all across the board, otherwise a 'make
# CFLAGS=XX' will kill us because the command line's CFLAGS will
# overwrite Make's and we'll loose all the infrastructure it sets.
override CFLAGS += -I. -I.. -I../crypto -I../utils -I../common 
ifeq ($(RTCONFIG_BCMARM),y)
override CFLAGS += -lgcc_s
endif

# at least for now, need to include config_ssid.h and config_blob.h from
# wpa_supplicant directory
override CFLAGS += -I ../../wpa_supplicant

OBJS_both += ../utils/common.o
OBJS_both += ../utils/os_unix.o
OBJS_both += ../utils/wpa_debug.o
OBJS_both += ../utils/base64.o
OBJS_both += ../utils/wpabuf.o
OBJS_both += ../crypto/md5.o
OBJS_both += ../crypto/sha1.o
OBJS_both += ../crypto/sha1-tlsprf.o
OBJS_both += ../crypto/aes-encblock.o
OBJS_both += ../crypto/aes-wrap.o
OBJS_both += ../crypto/aes-ctr.o
OBJS_both += ../crypto/aes-eax.o
OBJS_both += ../crypto/aes-omac1.o
OBJS_both += ../crypto/ms_funcs.o
OBJS_both += ../crypto/sha256.o


OBJS_both += ../eap_common/eap_peap_common.o
OBJS_both += ../eap_common/eap_psk_common.o
OBJS_both += ../eap_common/eap_pax_common.o
OBJS_both += ../eap_common/eap_sake_common.o
OBJS_both += ../eap_common/eap_gpsk_common.o
OBJS_both += ../eap_common/chap.o

OBJS_peer += ../eap_peer/eap_tls.o
OBJS_peer += ../eap_peer/eap_peap.o
OBJS_peer += ../eap_peer/eap_ttls.o
OBJS_peer += ../eap_peer/eap_md5.o
OBJS_peer += ../eap_peer/eap_mschapv2.o
OBJS_peer += ../eap_peer/mschapv2.o
OBJS_peer += ../eap_peer/eap_otp.o
OBJS_peer += ../eap_peer/eap_gtc.o
OBJS_peer += ../eap_peer/eap_leap.o
OBJS_peer += ../eap_peer/eap_psk.o
OBJS_peer += ../eap_peer/eap_pax.o
OBJS_peer += ../eap_peer/eap_sake.o
OBJS_peer += ../eap_peer/eap_gpsk.o
OBJS_peer += ../eap_peer/eap.o
OBJS_peer += ../eap_common/eap_common.o
OBJS_peer += ../eap_peer/eap_methods.o
OBJS_peer += ../eap_peer/eap_tls_common.o

override CFLAGS += -DEAP_TLS
override CFLAGS += -DEAP_PEAP
override CFLAGS += -DEAP_TTLS
override CFLAGS += -DEAP_MD5
override CFLAGS += -DEAP_MSCHAPv2
override CFLAGS += -DEAP_GTC
override CFLAGS += -DEAP_OTP
override CFLAGS += -DEAP_LEAP
override CFLAGS += -DEAP_PSK
override CFLAGS += -DEAP_PAX
override CFLAGS += -DEAP_SAKE
override CFLAGS += -DEAP_GPSK -DEAP_GPSK_SHA256
override CFLAGS += -DEAP_TLS_FUNCS

override CFLAGS += -DIEEE8021X_EAPOL

ifeq ($(CONFIG_TLS), openssl)
override CFLAGS += -DEAP_TLS_OPENSSL
override CFLAGS += -I$(TOP)/openssl/include
OBJS_both += ../crypto/tls_openssl.o
OBJS_both += ../crypto/crypto_openssl.o
LIBS += -L$(TOP)/openssl -lssl -lcrypto
override CFLAGS += -DINTERNAL_SHA256
endif

ifeq ($(CONFIG_TLS), internal)
OBJS_both += ../crypto/tls_internal.o
OBJS_both += ../tls/tlsv1_common.o ../../tls/tlsv1_record.o
OBJS_both += ../tls/tlsv1_cred.o
OBJS_both += ../tls/asn1.o ../../tls/x509v3.o
OBJS_both += ../crypto/crypto_internal.o ../../tls/rsa.o ../../tls/bignum.o

OBJS_peer += ../tls/tlsv1_client.o
OBJS_peer += ../tls/tlsv1_client_write.o ../../tls/tlsv1_client_read.o
override CFLAGS += -DCONFIG_TLS_INTERNAL_CLIENT

OBJS_server += ../tls/tlsv1_server.o
OBJS_server += ../tls/tlsv1_server_write.o ../../tls/tlsv1_server_read.o
override CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER

override CFLAGS += -DCONFIG_TLS_INTERNAL
override CFLAGS += -DCONFIG_CRYPTO_INTERNAL
override CFLAGS += -DCONFIG_INTERNAL_X509
override CFLAGS += -DINTERNAL_AES
override CFLAGS += -DINTERNAL_SHA1
override CFLAGS += -DINTERNAL_SHA256
override CFLAGS += -DINTERNAL_MD5
override CFLAGS += -DINTERNAL_MD4
override CFLAGS += -DINTERNAL_DES
ifdef CONFIG_INTERNAL_LIBTOMMATH
override CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
else
LIBS += -ltommath
endif
endif

ifndef LDO
LDO=$(CC)
endif


OBJS_lib=$(OBJS_both) $(OBJS_peer)

 #$(OBJS_server)

override CFLAGS  += -fPIC -DPIC
LDFLAGS += -shared
ifeq ($(RTCONFIG_BCMARM),y)
LDFLAGS += -lgcc_s
endif

$(LIBEAP): $(OBJS_lib)
	$(LDO) $(LDFLAGS) $(OBJS_lib) -Wl,-soname -Wl,$(LIBEAP_SO) -o $(LIBEAP) $(LIBS)


UTIL_HEADERS = ../utils/includes.h ../utils/common.h \
	../utils/wpabuf.h ../utils/build_config.h \
	../utils/os.h ../utils/wpa_debug.h
COMMON_HEADERS = ../common/defs.h 
EAP_COMMON_HEADERS = ../eap_common/eap_defs.h 
MAIN_HEADERS = eap.h eap_methods.h eap_config.h
CRYPTO_HEADERS =  ../crypto/tls.h  

install: 

	mkdir -p $(DESTDIR)/usr/lib
#	copy the lib file to std lib location
	cp $(LIBEAP) $(DESTDIR)/usr/lib
	ln -fs $(LIBEAP_SO) $(DESTDIR)/usr/lib/$(LIBEAP_NAME).so

#	copy the headers reqd by apps using eap peer library in its own subfolder under /usr/include
	mkdir -p \
		$(DESTDIR)/$(INCLUDE_INSTALL_DIR)/eap_common \
		$(DESTDIR)/$(INCLUDE_INSTALL_DIR)/common \
		$(DESTDIR)/$(INCLUDE_INSTALL_DIR)/util \
		$(DESTDIR)/$(INCLUDE_INSTALL_DIR)/crypto
	install -m 0644 $(EAP_COMMON_HEADERS) $(DESTDIR)/$(INCLUDE_INSTALL_DIR)/eap_common
	install -m 0644 $(COMMON_HEADERS) $(DESTDIR)/$(INCLUDE_INSTALL_DIR)/common
	install -m 0644 $(CRYPTO_HEADERS) $(DESTDIR)/$(INCLUDE_INSTALL_DIR)/crypto
	install -m 0644 $(UTIL_HEADERS) $(DESTDIR)/$(INCLUDE_INSTALL_DIR)/util
	install -m 0644 $(MAIN_HEADERS) $(DESTDIR)/$(INCLUDE_INSTALL_DIR)/

	mkdir -p $(DESTDIR)/usr/lib/pkgconfig
	cp libeap0.pc $(DESTDIR)/usr/lib/pkgconfig

uninstall: 

	rm $(DESTDIR)/usr/lib/$(LIBEAP)
	rm -fr $(DESTDIR)/$(INCLUDE_INSTALL_DIR)
	rm -f $(DESTDIR)/usr/lib/pkgconfig/libeap0.pc

clean:
	rm -f *~ *.o *.so *.d libeap.* $(LIBEAP) $(OBJS_lib)

-include $(OBJS:%.o=%.d)
