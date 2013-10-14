include ../common.mak

#EXEC = hw_nat acl ac mtr
EXEC = hw_nat

HWNAT_OBJS = hw_nat.o hwnat_api.o util.o
AC_OBJS = ac.o ac_api.o util.o
ACL_OBJS = acl.o acl_api.o util.o
MTR_OBJS = mtr.o mtr_api.o util.o
CONF_H  = $(LINUXDIR)/include/linux/autoconf.h
CFLAGS += -I$(LINUXDIR)/net/nat/hw_nat

ifneq ($(wildcard ./prebuild),)
all:
	-cp prebuild/hw_nat hw_nat
#	-cp prebuild/acl acl
#	-cp prebuild/ac ac
#	-cp prebuild/mtr mtr
else
all: $(EXEC)
endif

hw_nat: $(HWNAT_OBJS) $(CONF_H)
	$(CC) $(LDFLAGS) -o $@ $(HWNAT_OBJS) 

acl: $(ACL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(ACL_OBJS) 

ac: $(AC_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(AC_OBJS) 

mtr: $(MTR_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(MTR_OBJS) 

install: all
	install -D hw_nat $(INSTALLDIR)/bin/hw_nat
#	install -D acl $(INSTALLDIR)/bin/acl
#	install -D ac $(INSTALLDIR)/bin/ac
#	install -D mtr $(INSTALLDIR)/bin/mtr
	$(STRIP) $(INSTALLDIR)/bin/hw_nat
#	$(STRIP) $(INSTALLDIR)/bin/acl
#	$(STRIP) $(INSTALLDIR)/bin/ac
#	$(STRIP) $(INSTALLDIR)/bin/mtr

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o
	-rm -rf prebuild

