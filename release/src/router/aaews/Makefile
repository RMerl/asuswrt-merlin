TARGET=aaews
WSLIB=libws.so
DOG=mastiff

include $(TOP)/.config
include ../common.mak

CURL_DIR=$(TOP)/curl-7.21.7
XML2_DIR=$(TOP)/libxml2
OPENSSL_DIR=$(TOP)/openssl
NVRAM_DIR=$(TOP)/nvram$(BCMEX)
SHARED_DIR=$(TOP)/shared
WB_DIR=$(TOP)/wb
QCSAPI_DIR=$(TOP)/libqcsapi_client

NATNL_DIR=$(TOP)/asusnatnl/natnl
ASUSNATNL_CFLAGS=-I$(NATNL_DIR)
ASUSNATNL_LDFLAGS=-L$(NATNL_DIR)
ASUSNATNL_LIBS=-lasusnatnl

LIBC_DIR=$(NATNL_DIR)/../
WS_SRC=$(WB_DIR)/ws_src
OUTPUT := $(WB_DIR)/output

SRC=$(wildcard *.c)
OBJS=$(patsubst %.c, %.o, $(SRC))
OBJS_MAIN=$(filter-out $(DOG).o stun.o, $(OBJS))
WSLIB_OBJS := $(WS_SOURCES:%.c=$(OUTPUT)/%.o)

ifeq ($(wildcard $(TOP)/aaews/*.c),)
all:
	-cp -f $(TOP)/aaews/prebuild$(BCMEX)/aaews aaews
	-cp -f $(TOP)/aaews/prebuild$(BCMEX)/mastiff mastiff
else
all: $(WSLIB) $(TARGET)
endif


ifeq ("$(MAKECMDGOALS)", $(filter "$(MAKECMDGOALS)", "router" "mastiff"))
ROUTER_CFLAGS=-DNVRAM=1 -DEMBEDDED -I$(SHARED_DIR)
ifeq ($(RTCONFIG_QTN),y)
ROUTER_LIBS=-lnvram -lshared -lqcsapi_client
ROUTER_LDFLAGS=-L$(NVRAM_DIR) -L$(SHARED_DIR) -L$(QCSAPI_DIR)
else
ROUTER_LIBS=-lnvram -lshared
ROUTER_LDFLAGS=-L$(NVRAM_DIR) -L$(SHARED_DIR)
endif
endif

CFLAGS = -I$(CURL_DIR)/include -I$(OPENSSL_DIR)/include -I$(XML2_DIR)/include -I. $(ASUSNATNL_CFLAGS)  -I$(WS_SRC) -I$(NVRAM_DIR) -I$(WB_DIR) -I$(SRCBASE)/include $(AICAM_CFLAGS) $(ROUTER_CFLAGS) -g

LIBS = -lcurl -lssl -lcrypto -lxml2 -ldl -pthread -lrt $(ROUTER_LIBS) -lws
LDFLAGS = -L$(OPENSSL_DIR) -L$(CURL_DIR)/lib/.libs -L$(XML2_DIR)/.libs -L. $(ROUTER_LDFLAGS) -L$(WS_SRC) -L$(WB_DIR) -L$(WB_DIR)/output

ifeq ($(wildcard $(TOP)/aaews/*.c),)
$(DOG):
	-cp -f $(TOP)/aaews/prebuild$(BCMEX)/mastiff mastiff
else
$(DOG): $(DOG).o nat_nvram.o  ws_caller.o nw_util.o natapi.o $(WSLIB_OBJS) stun.o
	$(CC) -fPIC $(CFLAGS) $(LDFLAGS) $(LIBS) $(ROUTER_LDFLAGS) $(ROUTER_LIBS) -o $@ $^ 
#	mipsel-uclibc-gcc -c $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $^ 
endif
aicam:$(TARGET)

ifeq ($(wildcard $(TOP)/aaews/*.c),)
router:
	-cp -f $(TOP)/aaews/prebuild$(BCMEX)/aaews aaews
else
router: $(WSLIB) $(TARGET)
endif
$(WSLIB):
	cd $(WB_DIR) && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) LD=$(LD) CFLAGS="-Os -Wall $(EXTRACFLAGS)" \
	make libws.so


$(TARGET):  main.o  $(WSLIB_OBJS) $(OBJS_MAIN) 
	$(CXX) -fPIC $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $^ 

%.o: %.c
	$(CC) -c  -fPIC $(CFLAGS) $< -o $@ 

#install: all
#	$(STRIP) $(WSLIB)
#	install -D $(WSLIB) $(INSTALLDIR)/lib/$(WSLIB)
	
clean:
	echo $(WB_DIR)
	echo $(TOP)
	rm -f *.o  *.so *.dylib *.a aaews mastiff -r output

