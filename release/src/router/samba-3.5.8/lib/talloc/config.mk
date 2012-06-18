[LIBRARY::LIBTALLOC]
OUTPUT_TYPE = MERGED_OBJ
CFLAGS = -I$(tallocsrcdir)

LIBTALLOC_OBJ_FILES = $(tallocsrcdir)/talloc.o

MANPAGES += $(tallocdir)/talloc.3
