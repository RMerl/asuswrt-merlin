include ../common.mak

CFLAGS = -O2 -Wall
CFLAGS += -I../shared -I../../include -I.
ifeq ($(STATIC),1)
CFLAGS += -static
endif

all: filegen

	
filegen: filegen.o
	@echo " [filegen] CC $@"
	$(CC) $(CFLAGS) -o $@ filegen.o $(LIBDIR)/libm.a
	$(SIZECHECK)
	cp filegen ~/tomato/	
	
install: all

clean:
	rm -f *.o filegen

	
%.o: %.c .%.depend
	@echo " [filegen] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
