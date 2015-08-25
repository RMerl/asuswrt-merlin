include ../common.mak

WSLIB=libws.so

CURL_DIR=$(TOP)/curl-7.21.7
XML2_DIR=$(TOP)/libxml2
OPENSSL_DIR=$(TOP)/openssl

WS_SRC=ws_src
OUTPUT=output

SRC=$(wildcard *.c)
OBJS=$(patsubst %.c, %.o, $(SRC))
#WS_SOURCES := $(WS_SRC)/curl_api.c $(WS_SRC)/parse_xml.c $(WS_SRC)/wb.c $(WS_SRC)/wb_util.c $(WS_SRC)/ssl_api.c $(WS_SRC)/log.c
WS_SOURCES := $(wildcard $(WS_SRC)/*.c)
WSLIB_OBJS := $(WS_SOURCES:%.c=$(OUTPUT)/%.o)

ifeq ($(wildcard $(TOP)/wb/ws_src/*.c),)
all:
	-cp -f $(TOP)/wb/prebuild/libws$(BCMEX).so libws.so
else
all: $(WSLIB)
endif

CFLAGS = -fPIC -I$(CURL_DIR)/include -I$(OPENSSL_DIR)/include -I$(XML2_DIR)/include -I.  -I$(WS_SRC)

LDFLAGS = -L$(OPENSSL_DIR) -lcrypto -lssl
LDFLAGS += -L$(CURL_DIR)/lib/.libs -lcurl
LDFLAGS += -L$(XML2_DIR)/.libs -lxml2

$(WSLIB): $(WSLIB_OBJS)
	$(LD) -shared -o $@ $^ $(LDFLAGS)

$(WSLIB_OBJS): $(OUTPUT)/%.o: %.c
	mkdir -p $(dir $@)	
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

install: all
	$(STRIP) $(WSLIB)
	install -D $(WSLIB) $(INSTALLDIR)/lib/$(WSLIB)

clean:
	rm -rf *.o  *.so output

