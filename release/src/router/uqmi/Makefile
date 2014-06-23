CFLAGS  := --std=gnu99 -g -Os -I. $(EXTRACFLAGS)
CFLAGS  += -Wall -Werror -Wmissing-declarations
CFLAGS  += -ffunction-sections -fdata-sections
#CFLAGS += -DDEBUG_PACKET -DDEBUG -g3
LDFLAGS += -Wl,--gc-sections -Llibubox -Wl,-Bstatic -lubox -Wl,-Bdynamic

TARGET = uqmi
SOURCES = main.c dev.c commands.c qmi-message.c

service_sources = $(foreach service,ctl dms nas pds wds wms,qmi-message-$(service).c)
service_headers = $(service_sources:.c=.h)

SOURCES += $(service_sources)
OBJS=$(SOURCES:.c=.o)


PREFIX=/usr

default_target: all
.PHONY : default_target

install: all
	install -D $(TARGET) $(INSTALLDIR)/$(PREFIX)/sbin/$(TARGET)
	$(STRIP) $(INSTALLDIR)/$(PREFIX)/sbin/$(TARGET)
.PHONY : install

clean:
	make -C libubox clean
	@rm -f $(OBJS) $(TARGET) $(service_sources) $(service_headers) qmi-errors.c
.PHONY : clean

# The main all target
all: gen-errors gen-headers $(TARGET)
.PHONY : all

gen-errors: qmi-errors.c
	@true
.PHONY : gen-errors

qmi-errors.c: qmi-errors.h data/gen-error-list.pl
	data/gen-error-list.pl $< > $@

gen-headers: $(service_headers)
	@true
.PHONY : gen-headers

qmi-message-%.h: data/qmi-service-%.json data/gen-header.pl data/gen-common.pm
	data/gen-header.pl $*_ $< > $@

qmi-message-%.c: data/qmi-service-%.json data/gen-code.pl data/gen-common.pm
	data/gen-code.pl $*_ $< > $@

qmi-message-%.o: CFLAGS += -Wno-unused

libubox:
	make -C libubox
.PHONY : libubox

$(TARGET): libubox $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.c.o::
	$(CC) $(CFLAGS) -c $<

# Dependencies
main.o: main.c uqmi.h commands.h
dev.o: dev.c qmi-errors.c qmi-errors.h uqmi.h $(service_headers)
commands.o: commands.c uqmi.h commands.h $(service_sources) $(service_headers)
qmi-message.o: qmi-message.c qmi-message.h $(service_headers)
