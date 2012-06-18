EXEC = networkmap
OBJS = function.o networkmap.o

CFLAGS  += -Wall -DASUS -DASUS_NVRAM -DBCMNVRAM -I. -I$(TOP)/shared -I$(LINUXDIR)/include 
CFLAGS  += -s -O2 -DNO_PARALLEL
LDFLAGS += -L$(TOP)/shared -lshared -L$(TOP)/nvram -lnvram 

#CFLAGS	+= -DDEBUG -DDEBUG_MORE

all: $(EXEC) #arpstorm

$(EXEC): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

#arpstorm: shutils.o arpstorm.o
#	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

install:
	install -D $(EXEC) $(INSTALLDIR)/usr/sbin/$(EXEC)
	$(STRIP) $(INSTALLDIR)/usr/sbin/$(EXEC)

clean: 
	rm -rf *.o $(EXEC) *~
