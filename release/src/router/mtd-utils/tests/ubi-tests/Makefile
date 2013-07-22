LIBUBI_PATH = ../../ubi-utils/
LIBUBI_HEADER_PATH = $(LIBUBI_PATH)/include
UBIUTILS_PATH=../../ubi-utils/

KERNELHDR := ../../include

LIBS = libubi
TARGETS=io_update volrefcnt integ io_paral io_read io_basic \
          mkvol_basic mkvol_bad mkvol_paral rsvol

CFLAGS += -I$(LIBUBI_HEADER_PATH) -I $(KERNELHDR) -lpthread

include ../../common.mk

# Compile ubilib with the udevsettle hack
libubi.a: $(LIBUBI_PATH)/libubi.c  $(LIBUBI_HEADER_PATH)/libubi.h  $(LIBUBI_PATH)/libubi_int.h
	$(CC) $(CFLAGS) -I $(LIBUBI_PATH) -I../../include -DUDEV_SETTLE_HACK -c $(LIBUBI_PATH)/libubi.c -o libubi.o
	ar cr libubi.a libubi.o

$(TARGETS): $(addprefix $(BUILDDIR)/, common.o) libubi.a

clean::
	rm -f libubi.a
