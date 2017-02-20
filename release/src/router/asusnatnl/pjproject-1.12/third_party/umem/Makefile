ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif
 
#CC      = gcc
#C++	= g++

CFLAGS += -g -I. -I./sys 
ifeq ($(router), yes)
  CFLAGS += -DROUTER=1
endif

ifeq ($(asustor), yes)
  CFLAGS += -DASUSTOR
endif

ifeq ($(os), ANDROID)
  CFLAGS += -DANDROID
endif

ifeq ($(os), IOS)
  CFLAGS += -DIOS
endif

ifeq ($(arch), ARM)                                                                                                                                                                                        
  CFLAGS += -DARM -fPIC
endif


ifeq ($(arch), ARM64)
  CFLAGS += -DARM64 -DUMEM_PTHREAD_MUTEX_TOO_BIG
endif

ifeq ($(arch), MIPS)
  CFLAGS += -DMIPS
endif

ifeq ($(arch), X86_64)
  CFLAGS += -DX86_64 -DUMEM_PTHREAD_MUTEX_TOO_BIG -fPIC
endif

OUTPUT=output-${os}

SRC=$(wildcard usrsctplib/*.c)
OBJS=$(patsubst %c, %o, $(SRC)) 
OUT_OBJS=$(OUTPUT)/init_lib.o $(OUTPUT)/umem_agent_support.o $(OUTPUT)/umem_fail.o $(OUTPUT)/umem_fork.o $(OUTPUT)/umem_update_thread.o $(OUTPUT)/vmem_mmap.o $(OUTPUT)/vmem_sbrk.o $(OUTPUT)/envvar.o $(OUTPUT)/getpcstack.o $(OUTPUT)/misc.o $(OUTPUT)/vmem_base.o $(OUTPUT)/umem.o $(OUTPUT)/vmem.o
DIR = $(shell pwd)
OS = $(shell uname -s)
LIB_NAME=libumem
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

$(OUTPUT)/%.o:%.c
	mkdir -p $(dir $@)
	$(CC)	-c $(CPPFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)  $< -o $@

#%.o: %.c %.h
#	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) $< 

libusrsctp.so: $(OBJS)
ifneq ($(os), OSX)
	$(CC) -shared -o $(LDLIBS) $@ $^
else
	$(CC) -dynamiclib -o libusrsctp.dylib -lstdc -lpthread -lm $^
endif

#$(LIBRARY): $(OBJS)
$(LIBRARY): $(OUT_OBJS)
	$(AR) -rcs $@ $^

clean:
	rm -f *.o *.so *.dylib *.a 
	rm -rf output-*

install:
	export LD_LIBRARY_PATH=$(DIR):$$LD_LIBRARY_PATH
