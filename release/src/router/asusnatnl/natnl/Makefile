#Modify this to point to the PJSIP location.
include ../version.mak
RTSRCBASE=../../..
PJBASE=../pjproject-1.12
UMEMBASE=../umem-1.0.1
UDTBASE=../udt
LIB_UDT=udt
SCTPBASE=../pjproject-1.12/third_party/usrsctp/usrsctplib
NATBASE=.
include $(PJBASE)/build.mak

CC      = $(PJ_CC)
CXX		= $(PJ_CXX)
AR      = $(PJ_AR)
LD	= $(PJ_LD)
#STRIP	= $(PJ_STRIP)   
LDFLAGS = $(PJ_LDFLAGS)
LDLIBS  = $(PJ_LDLIBS)
LDLIBS += -L$(UDTBASE) -l$(LIB_UDT) -L$(NATBASE)
CFLAGS  = $(PJ_CFLAGS) 
CFLAGS += -g -I$(NATBASE) -I$(NATBASE)/msvc_prjs/asusnatnl 
CFLAGS += -DTUNNEL_V=\"$(VERSION)\"
CFLAGS += -I$(UDTBASE) -I$(SCTPBASE)
#CFLAGS += -Wall
CPPFLAGS += -fPIC

ifeq ($(AR),)
AR=ar
endif

ifeq ($(router), yes)
CFLAGS += -DROUTER=1
#for wl_ioctl used.
CFLAGS += -I$(RTSHAREDBASE)
CFLAGS += -I$(SRCBASE)/include
endif

#check if make goal is android-lib or linux-lib
ifeq ("$(MAKECMDGOALS)", $(filter "$(MAKECMDGOALS)", "android-lib" "linux-lib" "ios-dylib" "static"))
CFLAGS += -DNATNL_LIB=1
endif

ifeq ("$(MAKECMDGOALS)", $(filter "$(MAKECMDGOALS)", "android-lib"))
ifeq ($(findstring i686, $(PJ_CC)), i686)
ARCH=i686
STL_DIR=x86
LDLIBS +=~/android-toolchain-x86/i686-linux-android/lib/libstdc++.a
endif
ifeq ($(findstring aarch64, $(PJ_CC)), aarch64)
ARCH=aarch64
STL_DIR=arm64
LDLIBS +=~/android-toolchain-arm64/aarch64-linux-android/lib/libstdc++.a
endif
ifeq ($(findstring arm, $(PJ_CC)), arm)
ARCH=arm
EABI=eabi
STL_DIR=arm
LDLIBS += ~/android-toolchain-arm/arm-linux-androideabi/lib/libstdc++.a
endif
ANDROID_CROSS=$(ARCH)-linux-android$(EABI)
ANDROID_GCC=$(ANDROID_CROSS)-gcc
ANDROID_STRIP=$(ANDROID_CROSS)-strip
ANDROID_AR=$(ANDROID_CROSS)-ar
ANDROID_RANLIB=$(ANDROID_CROSS)-ranlib
JNI_OBJ=natjni.o
CPPFLAGS= ${CFLAGS} -frtti -fexceptions -DANDROID -DPJ_ANDROID=1 
endif


OBJS = main.o natnl.o \
		natnl_codec.o \
		socket.o message.o client.o list.o tunnel.o \
		im_handler.o \
		rtsp_handler.o \
		$(JNI_OBJ)
		#config.o \

TMP_OBJS=$(patsubst natnl.o,,$(OBJS)) 
APP_OBJS = $(TMP_OBJS)		
OBJ_LIBS = main.o $(OBJS)

# If your application is in a file named main.cpp or main.c
# this is the line you will need to build the binary.
#all: natnl
all: natnl

android-lib: main.c $(OBJS)
	$(CXX) -shared -o libnatnl.so $(CPPFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS)
#$(STRIP) libnatnl.so
#	NATLIB_NAME=libnatnl.so
ifeq ($(wildcard ./*.c),)
#For prebuild on router platform. Don't remove it.
linux-lib:
	-cp -f $(TOP)/asusnatnl/natnl/prebuild$(BCMEX)/libasusnatnl.so libasusnatnl.so
else
linux-lib: main.c $(OBJS)
	$(CXX) -shared -o libasusnatnl.so $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS)
	NATLIB_NAME=libasusnatnl.so
endif
#ios-static: main.c $(OBJS)
#	$(CXX)  -Wl $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -static -all_load -o libasusnatnl.a

ios-dylib: main.c $(OBJS)
	$(CXX)  -Wl $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -dynamiclib -o libasusnatnl.dylib
	NATLIB_NAME=libasusnatnl.dylib
	#$(CXX) -dynamiclib -o libasusnatnl.dylib -Wl $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS)
	#NATLIB_NAME=libasusnatnl.dylib

OS = $(shell uname -s)
LIB_NAME=libasusnatnl
LIBRARY = $(LIB_NAME).a
ifeq ($(OS),Darwin)
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
LIBRARY = $(LIB_NAME)-$(XXX).a
endif

$(LIBRARY): main.o $(OBJS)
	$(AR) crs $@ $?


static: $(LIBRARY) 
	echo '<<<<< YYY=$(YYY), XXX=$(XXX)>>>>>'

natnl: main.c $(APP_OBJS)
	$(CXX) -o $@ $< $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(APP_OBJS) $(LDLIBS)

%.o: %.c
	$(CXX) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LDLIBS)  $<

clean:
	rm -f *.o natnl *.so *.dylib *.a

install:
	mkdir -p ./inst/$(TARGET_NAME)
	cp lib*.* ./inst/$(TARGET_NAME)

