
CFG_MLONG64=0

CFE_DIRS =  ../include ../arch/mips/cpu/sb1250/include ../lib ../verif ../api
CFE_INCLUDES =  $(patsubst %,-I%,$(subst :, ,$(CFE_DIRS)))

VPATH = $(CFE_DIRS)

INCLUDES =  -I. $(CFE_INCLUDES)
COMFLAGS = -g -c -mcpu=sb1 -ffreestanding
CFLAGS = $(INCLUDES) $(COMFLAGS) -O -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -G 0 -DCFE_API_ALL

GCC = $(TOOLS)sb1-elf-gcc
GLD = $(TOOLS)sb1-elf-ld
OBJCOPY = $(TOOLS)sb1-elf-objcopy

ifeq (${CFG_MLONG64},1)
CFLAGS += -mlong64 -D__long64
endif

LDFLAGS = --script cfe_applet.lds -g 

ifeq (CFG_LITTLE,1)
CFLAGS += -EL
LDFLAGS += -EL
else
CFLAGS += -EB
LDFLAGS += -EB
endif


%.o : %.c
	$(GCC) $(CFLAGS) -o $@ $<

%.o : %.S
	$(GCC) $(CFLAGS) -o $@ $<


LIBOBJS = lib_printf.o lib_string.o cfe_api.o

all : test vapitest
	echo done

test : test.o minicrt0.o $(LIBOBJS) 
	$(GLD) -o test.elfraw -Map test.mapraw $(LDFLAGS) minicrt0.o test.o $(LIBOBJS) 
	$(GLD) -o test.elf -Ttext 0x20000000 -Map test.map $(LDFLAGS) minicrt0.o test.o $(LIBOBJS) 
	$(OBJCOPY) -O binary test.elfraw test
	$(OBJCOPY) -O srec test.elfraw test.srec

download : download.o minicrt0.o $(LIBOBJS) 
	$(GLD) -o download.elf -Ttext 0x80001000 -Map download.map $(LDFLAGS) minicrt0.o download.o $(LIBOBJS) 
	$(OBJCOPY) --strip-unneeded download.elf s.download.elf
	gzip --best -c s.download.elf > download.elf.gz
	$(GLD) -Tdownload.lds -bbinary -o download.data download.elf.gz

vapitest : vapitest.o 
	$(GLD) -o vapitest -Ttext 0x80020000 -Map vapitest.map vapitest.o
	$(OBJCOPY) -O srec vapitest vapitest.srec

clean :
	rm -f *.o *~ *.map *.mapraw
	rm -f test test.elf test.srec test.elfraw
	rm -f *download.elf *.gz
	rm -f vapitest vapitest.srec
