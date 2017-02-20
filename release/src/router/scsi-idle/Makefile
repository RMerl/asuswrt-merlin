include ../common.mak

ifeq ($(RTCONFIG_QCA),y)
ifeq ($(CONFIG_LINUX3_14),y)
CFLAGS += -I$(KDIR)/include
endif
endif

CFLAGS += -Wall -Os $(EXTRACFLAGS) -I$(SRCBASE) -I$(SRCBASE)/include -I$(LINUXDIR)/include -I$(LINUXDIR)/arch/arm/include -I$(LINUXDIR)/arch/arm/plat-brcm/include -I.

ifeq ($(CONFIG_LINUX26),y)
CFLAGS += -DSCSI_SG
endif

TARGETS = scsi-start

all: $(TARGETS)

scsi-start:	scsi-start.c
	@$(CC) -o $@ $^ $(CFLAGS)

scsi-idle:	scsi-idle.c
	@$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o $(TARGETS) *~

install:	all
	@install -d $(INSTALLDIR)/usr/sbin
	@install -D scsi-start $(INSTALLDIR)/usr/sbin/scsi-start
	@$(STRIP) -s $(INSTALLDIR)/usr/sbin/scsi-start

	# scsi-stop is a symbolic link to scsi-start
	cd $(INSTALLDIR)/usr/sbin && \
		ln -sf scsi-start scsi-stop
