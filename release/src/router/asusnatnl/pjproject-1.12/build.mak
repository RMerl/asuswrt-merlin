# build.mak.  Generated from build.mak.in by configure.
export MACHINE_NAME := auto
export OS_NAME := auto
export HOST_NAME := unix
export CC_NAME := gcc
export TARGET_NAME := arm-unknown-linux-gnu
export CROSS_COMPILE := arm-linux-
export LINUX_POLL := select 

export ac_prefix := /home/merlin/asuswrt.ac87/release/src/router/asusnatnl/pjproject-1.12/_inst

LIB_SUFFIX = $(TARGET_NAME).a

# Determine which party libraries to use
export APP_THIRD_PARTY_LIBS := -lresample-$(TARGET_NAME) -lmilenage-$(TARGET_NAME) -lsrtp-$(TARGET_NAME) -lbzip2-$(TARGET_NAME)
export APP_THIRD_PARTY_EXT :=
export APP_THIRD_PARTY_LIB_FILES = $(PJ_DIR)/third_party/lib/libresample-$(LIB_SUFFIX) $(PJ_DIR)/third_party/lib/libmilenage-$(LIB_SUFFIX) $(PJ_DIR)/third_party/lib/libsrtp-$(LIB_SUFFIX)

ifneq (,1)
ifeq (0,1)
# External GSM library
APP_THIRD_PARTY_EXT += -lgsm
else
APP_THIRD_PARTY_LIBS += -lgsmcodec-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libgsmcodec-$(LIB_SUFFIX)
endif
endif

ifneq (,1)
ifeq (0,1)
APP_THIRD_PARTY_EXT += -lspeex -lspeexdsp
else
APP_THIRD_PARTY_LIBS += -lspeex-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libspeex-$(LIB_SUFFIX)
endif
endif

ifneq (,1)
APP_THIRD_PARTY_LIBS += -lilbccodec-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libilbccodec-$(LIB_SUFFIX)
endif

ifneq (,1)
APP_THIRD_PARTY_LIBS += -lg7221codec-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libg7221codec-$(LIB_SUFFIX)
endif

ifneq ($(findstring pa,pa_unix),)
ifeq (0,1)
# External PA
APP_THIRD_PARTY_EXT += -lportaudio
else
APP_THIRD_PARTY_LIBS += -lportaudio-$(TARGET_NAME)
APP_THIRD_PARTY_LIB_FILES += $(PJ_DIR)/third_party/lib/libportaudio-$(LIB_SUFFIX)
endif
endif

# Additional flags


# CFLAGS, LDFLAGS, and LIBS to be used by applications
export PJDIR := /home/merlin/asuswrt.ac87/release/src/router/asusnatnl/pjproject-1.12
export APP_CC := arm-brcm-linux-uclibcgnueabi-gcc
export APP_CXX := arm-brcm-linux-uclibcgnueabi-g++
export APP_CFLAGS := -DPJ_AUTOCONF=1\
	-I/home/merlin/asuswrt.ac87/release/src-rt-6.x.4708/router/openssl/include -g -O2 -fPIC -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1\
	-I$(PJDIR)/pjlib/include\
	-I$(PJDIR)/pjlib-util/include\
	-I$(PJDIR)/pjnath/include\
	-I$(PJDIR)/pjmedia/include\
	-I$(PJDIR)/pjsip/include
export APP_CXXFLAGS := $(APP_CFLAGS)
export APP_LDFLAGS := -L$(PJDIR)/pjlib/lib\
	-L$(PJDIR)/pjlib-util/lib\
	-L$(PJDIR)/pjnath/lib\
	-L$(PJDIR)/pjmedia/lib\
	-L$(PJDIR)/pjsip/lib\
	-L$(PJDIR)/third_party/lib\
	-L/home/merlin/asuswrt.ac87/release/src-rt-6.x.4708/router/openssl -lstdc++
export APP_LDLIBS := -lpjsua-$(TARGET_NAME)\
	-lpjsip-ua-$(TARGET_NAME)\
	-lpjsip-simple-$(TARGET_NAME)\
	-lpjsip-$(TARGET_NAME)\
	-lpjmedia-codec-$(TARGET_NAME)\
	-lpjmedia-$(TARGET_NAME)\
	-lpjmedia-audiodev-$(TARGET_NAME)\
	-lpjnath-$(TARGET_NAME)\
	-lpjlib-util-$(TARGET_NAME)\
	$(APP_THIRD_PARTY_LIBS)\
	$(APP_THIRD_PARTY_EXT)\
	-lpj-$(TARGET_NAME)\
	-lm -lnsl -lrt -lpthread  -lssl -lcrypto  -lcrypto -lssl
export APP_LIB_FILES = $(PJ_DIR)/pjsip/lib/libpjsua-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjsip/lib/libpjsip-ua-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjsip/lib/libpjsip-simple-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjsip/lib/libpjsip-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjmedia/lib/libpjmedia-codec-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjmedia/lib/libpjmedia-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjmedia/lib/libpjmedia-audiodev-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjnath/lib/libpjnath-$(LIB_SUFFIX) \
	$(PJ_DIR)/pjlib-util/lib/libpjlib-util-$(LIB_SUFFIX) \
	$(APP_THIRD_PARTY_LIB_FILES) \
	$(PJ_DIR)/pjlib/lib/libpj-$(LIB_SUFFIX)

# Here are the variabels to use if application is using the library
# from within the source distribution
export PJ_DIR := $(PJDIR)
export PJ_CC := $(APP_CC)
export PJ_CXX := $(APP_CXX)
export PJ_CFLAGS := $(APP_CFLAGS)
export PJ_CXXFLAGS := $(APP_CXXFLAGS)
export PJ_LDFLAGS := $(APP_LDFLAGS)
export PJ_LDLIBS := $(APP_LDLIBS)
export PJ_LIB_FILES := $(APP_LIB_FILES)

# And here are the variables to use if application is using the
# library from the install location (i.e. --prefix)
export PJ_INSTALL_DIR := /home/merlin/asuswrt.ac87/release/src/router/asusnatnl/pjproject-1.12/_inst
export PJ_INSTALL_INC_DIR := $(PJ_INSTALL_DIR)/include
export PJ_INSTALL_LIB_DIR := $(PJ_INSTALL_DIR)/lib
export PJ_INSTALL_CFLAGS := -I$(PJ_INSTALL_INC_DIR) -DPJ_AUTOCONF=1	-I/home/merlin/asuswrt.ac87/release/src-rt-6.x.4708/router/openssl/include -g -O2 -fPIC -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1
export PJ_INSTALL_CXXFLAGS := $(PJ_INSTALL_CFLAGS)
export PJ_INSTALL_LDFLAGS := -L$(PJ_INSTALL_LIB_DIR) $(APP_LDLIBS)
