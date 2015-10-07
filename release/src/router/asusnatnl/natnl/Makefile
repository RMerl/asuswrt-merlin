#Modify this to point to the PJSIP location.
include ../version.mak
include ../../common.mak
PJBASE=../pjproject-1.12
UPNPBASE=../miniupnpc-1.6
UDTBASE=../udt
LIB_MINIUPNPC=miniupnpc
LIB_UDT=udt
NATBASE=.
ARM_TOOLCHAIN=/home/charles0000/android-toolchain-arm
include $(PJBASE)/build.mak

CC      = $(PJ_CC)
CXX		= $(PJ_CXX)
AR      = $(PJ_AR)
LD	= $(PJ_LD)
#STRIP	= $(PJ_STRIP)   
LDFLAGS = $(PJ_LDFLAGS)
LDLIBS  = $(PJ_LDLIBS)
#LDLIBS += -L$(UPNPBASE) -l$(LIB_MINIUPNPC) -L$(UDTBASE) -l$(LIB_UDT) -L$(NATBASE) -L$(SYSROOT)/usr/lib #-llog#-lminiupnpc -L$(SYSROOT)/usr/lib -llog
LDLIBS += -L$(UPNPBASE) -l$(LIB_MINIUPNPC) -L$(UDTBASE) -l$(LIB_UDT) -L$(NATBASE) #-llog#-lminiupnpc -L$(SYSROOT)/usr/lib -llog
CFLAGS  = $(PJ_CFLAGS) 
CFLAGS += -g -I$(NATBASE) -I$(NATBASE)/msvc_prjs/asusnatnl #-I$(ARM_TOOLCHAIN)/usr/include
CFLAGS += -I$(UPNPBASE) -DTUNNEL_V=\"$(VERSION)\"
CFLAGS += -I$(UDTBASE)
#CFLAGS += -Wall
CPPFLAGS += -fPIC

ifeq ($(AR),)
AR=ar
endif

#check if make goal is android-lib or linux-lib
ifeq ("$(MAKECMDGOALS)", $(filter "$(MAKECMDGOALS)", "android-lib" "linux-lib" "ios-dylib" "static"))
CFLAGS += -DNATNL_LIB=1
endif

ifeq ("$(MAKECMDGOALS)", $(filter "$(MAKECMDGOALS)", "android-lib"))
ifeq ($(findstring i686, $(PJ_CC)), i686)
ARCH=i686
STL_DIR=x86
endif
ifeq ($(findstring aarch64, $(PJ_CC)), aarch64)
ARCH=aarch64
STL_DIR=arm64
endif
ifeq ($(findstring arm, $(PJ_CC)), arm)
ARCH=arm
EABI=eabi
STL_DIR=arm
endif
ANDROID_CROSS=$(ARCH)-linux-android$(EABI)
ANDROID_GCC=$(ANDROID_CROSS)-gcc
ANDROID_STRIP=$(ANDROID_CROSS)-strip
ANDROID_AR=$(ANDROID_CROSS)-ar
ANDROID_RANLIB=$(ANDROID_CROSS)-ranlib
JNI_OBJ=natjni.o
CPPFLAGS= ${CFLAGS} -frtti -fexceptions -DANDROID -DPJ_ANDROID=1 
CFLAGS += -DNATNL_LIB=1
LDLIBS += -L$(NATBASE)/gnustl/$(STL_DIR) -lgnustl_static 
endif


OBJS = main.o natnl.o \
		natnl_codec.o \
		socket.o message.o client.o list.o tunnel.o \
		upnpc.o \
		im_handler.o \
		$(JNI_OBJ)
		#config.o \

TMP_OBJS=$(patsubst natnl.o,,$(OBJS)) 
APP_OBJS = config.o $(TMP_OBJS)		
OBJ_LIBS = main.o $(OBJS)

# If your application is in a file named main.cpp or main.c
# this is the line you will need to build the binary.
#all: natnl
all: natnl

#lib: main.c $(OBJS)
#lib: $(OBJS) main.o
#android-lib: main.c $(OBJ_LIBS)
#	$(CXX) -shared -o libnatnl.so $(CPPFLAGS) $(LDFLAGS) $(OBJ_LIBS) $(LDLIBS) #-lminiupnpc
#	NATLIB_NAME=libnatnl.so

android-lib: main.c $(OBJS)
	$(CXX) -shared -o libnatnl.so $(CPPFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) #-lminiupnpc
#$(STRIP) libnatnl.so
#	NATLIB_NAME=libnatnl.so
ifeq ($(wildcard $(TOP)/asusnatnl/natnl/*.c),)
linux-lib:
	-cp -f $(TOP)/asusnatnl/natnl/prebuild$(BCMEX)/libasusnatnl.so libasusnatnl.so
else
linux-lib: main.c $(OBJS)
	$(CXX) -shared -o libasusnatnl.so $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS)
	NATLIB_NAME=libasusnatnl.so
	#$(CC) -shared -o libasusnatnl.so $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS)
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

	#$(CXX) -WL $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -static -o libasusnatnl.a

#ios-static-lib: main.c $(OBJ_LIBS)
#	echo "LDFLAGS= {${LDFLAGS}} "
#	echo "LDLIBS = {${LDLIBS}}"
#	$(AR) rcs AAA.a main.o ${OBJ_LIBS}
#	#libtool -static -o libasusnatnl.a  main.o $(OBJ_LIBS) $(LDFLAGS) $(LDLIBS)

#	$(CC) -static -o libasusnatnl.a $(CFLAGS) $(LDFLAGS) $(OBJ_LIBS) $(LDLIBS)
	
#natnl: main.c $(OBJS)
#$(CC) -o $@ $< $(CPPFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) #-lminiupnpc
natnl: main.c $(APP_OBJS)
	$(CXX) -o $@ $< $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(APP_OBJS) $(LDLIBS) #-lminiupnpc

%.o: %.c
	$(CXX) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LDLIBS)  $<

clean:
	rm -f *.o natnl *.so *.dylib *.a

install:
	mkdir -p ./inst/$(TARGET_NAME)
	cp lib*.* ./inst/$(TARGET_NAME)

