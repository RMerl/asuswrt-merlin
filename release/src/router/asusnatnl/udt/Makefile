#C++ = g++ -g
PJDIR=../pjproject-1.12

include ../pjproject-1.12/build.mak
include ../pjproject-1.12/build/host-$(HOST_NAME).mak
-include ../user.mak
include ../version.mak

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif

#CCFLAGS = -fPIC -Wall -Wextra -D$(os) -finline-functions -O3 -fno-strict-aliasing -Wreorder #-msse3
#CCFLAGS += -I$(PJDIR)/pjlib/include
#CCFLAGS += -I$(PJDIR)/pjsip/include
#CCFLAGS += -I$(PJDIR)/pjlib-util/include
#CCFLAGS += -I$(PJDIR)/pjmedia/include
#CCFLAGS += -I$(PJDIR)/pjnath/include

CC      = $(PJ_CC)
C++	= $(PJ_CXX)
LDFLAGS = $(PJ_LDFLAGS)
LDLIBS  = $(PJ_LDLIBS)
CFLAGS  = $(PJ_CFLAGS)
CPPFLAGS= $(PJ_CXXFLAGS) ${CFLAGS}
CFLAGS += -I$(PJDIR)/pjsip-apps/src/pjsua
#CFLAGS += -DDEBUGP
#CFLAGS += -g

OUTPUT=output-${os}${arch}

ifeq ($(arch), IA32)
   CFLAGS += -DIA32
endif

ifeq ($(arch), POWERPC)
   CFLAGS += -mcpu=powerpc
endif

ifeq ($(arch), SPARC)
   CFLAGS += -DSPARC
endif

ifeq ($(arch), IA64)
   CFLAGS += -DIA64
endif

ifeq ($(arch), AMD64)
   CFLAGS += -DAMD64
endif


ifeq ($(arch), ARM)
   CPPFLAGS += -frtti -fexceptions 
  # LDLIBS += -L/home/charles0000/android-toolchain/arm-linux-androideabi/lib 
   LDFLAGS = -lstdc++
   ANDROID_ARCH=arm
   EABI=eabi
endif

ifeq ($(arch), ARM64)
   CPPFLAGS += -frtti -fexceptions
   LDFLAGS = -lstdc++
   CFLAGS += -DARM64
   ANDROID_ARCH=aarch64
endif

ifeq ($(arch), i686)
  ANDROID_ARCH=i686
endif

ifeq ($(arch), MIPS)
LDFLAGS= -lstdc++
endif

#ifeq ($(os), ANDROID)
#   CFLAGS += -DANDROID
#endif

SRC=$(wildcard *.cpp)
OBJS=$(patsubst %c, %o, $(SRC)) 
#OBJS = md5.o common.o window.o list.o buffer.o packet.o channel.o queue.o ccc.o cache.o core.o epoll.o api.o wrap.o
OUT_OBJS=$(OUTPUT)/md5.o $(OUTPUT)/common.o $(OUTPUT)/window.o $(OUTPUT)/list.o $(OUTPUT)/buffer.o $(OUTPUT)/packet.o $(OUTPUT)/channel.o $(OUTPUT)/queue.o $(OUTPUT)/ccc.o $(OUTPUT)/cache.o $(OUTPUT)/core.o $(OUTPUT)/epoll.o $(OUTPUT)/api.o $(OUTPUT)/wrap.o
DIR = $(shell pwd)
OS = $(shell uname -s)
#all: libudt.so libudt.a udt
LIB_NAME=libudt
LIBRARY = $(LIB_NAME).a
ifeq ($(OS), Darwin)
YYY = $(CFLAGS) 
XXX = $(findstring armv7s, $(YYY))
ifeq ($(XXX),)
XXX = $(findstring armv7, $(YYY))
ifeq ($(XXX),)
XXX = $(findstring i386, $(YYY))
ifeq ($(XXX),)
XXX = $(findstring arm64, $(YYY))
ifeq ($(XXX),)
XXX = $(findstring x86_64, $(YYY))
endif
endif
endif
endif
ifeq ($(XXX),)
LIBRARY = $(LIB_NAME).a
else
LIBRARY = $(LIB_NAME)-$(XXX).a
endif
endif

AR=ar
ANDROID_GCC=$(ANDROID_ARCH)-linux-android$(EABI)-gcc
ANDROID_STRIP=$(ANDROID_ARCH)-linux-android$(EABI)-strip
ANDROID_AR=$(ANDROID_ARCH)-linux-android$(EABI)-ar
ANDROID_RANLIB=$(ANDROID_ARCH)-linux-android$(EABI)-ranlib
ifeq ($(PJ_CC),$(ANDROID_GCC))
RANLIB=$(ANDROID_RANLIB)
AR=$(ANDROID_AR)
endif



all: $(LIBRARY)

$(OUT_OBJS):$(OUTPUT)/%.o:%.cpp
	mkdir -p $(dir $@)
	$(C++)	-c $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)  $< -o $@

%.o: %.cpp %.h udt.h
	$(C++) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) $< -c 

libudt.so: $(OBJS)
ifneq ($(os), OSX)
	$(C++) -shared -o $(LDLIBS) -lstdc++ $@ $^
#	$(C++) -shared -o $@ $^
else
	$(C++) -dynamiclib -o libudt.dylib -lstdc++ -lpthread -lm $^
endif

#$(LIBRARY): $(OBJS)
$(LIBRARY): $(OUT_OBJS)
	$(AR) -rcs $@ $^

#udt:
#	cp udt.h udt

clean:
	rm -f *.o *.so *.dylib *.a udt
	rm -rf output-*

install:
	export LD_LIBRARY_PATH=$(DIR):$$LD_LIBRARY_PATH
