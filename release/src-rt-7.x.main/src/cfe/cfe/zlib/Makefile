
#ALLOBJS += adler32.o compress.o crc32.o gzio.o uncompr.o deflate.o trees.o \
#       zutil.o inflate.o infblock.o inftrees.o infcodes.o infutil.o inffast.o

#ALLOBJS += adler32.o compress.o crc32.o gzio.o uncompr.o deflate.o trees.o \
#       zutil.o inflate.o infblock.o inftrees.o infcodes.o infutil.o inffast.o

ALLOBJS += adler32.o uncompr.o inflate.o infblock.o inftrees.o infcodes.o infutil.o inffast.o zutil.o
CFLAGS += -DCFG_ZLIB=1 -DMY_ZCALLOC -DNO_MEMCPY

adler32.o: zlib.h zconf.h
compress.o: zlib.h zconf.h
crc32.o: zlib.h zconf.h
deflate.o: deflate.h zutil.h zlib.h zconf.h
example.o: zlib.h zconf.h
gzio.o: zutil.h zlib.h zconf.h
infblock.o: infblock.h inftrees.h infcodes.h infutil.h zutil.h zlib.h zconf.h
infcodes.o: zutil.h zlib.h zconf.h
infcodes.o: inftrees.h infblock.h infcodes.h infutil.h inffast.h
inffast.o: zutil.h zlib.h zconf.h inftrees.h
inffast.o: infblock.h infcodes.h infutil.h inffast.h
inflate.o: zutil.h zlib.h zconf.h infblock.h
inftrees.o: zutil.h zlib.h zconf.h inftrees.h
infutil.o: zutil.h zlib.h zconf.h infblock.h inftrees.h infcodes.h infutil.h
minigzip.o:  zlib.h zconf.h 
trees.o: deflate.h zutil.h zlib.h zconf.h trees.h
uncompr.o: zlib.h zconf.h
zutil.o: zutil.h zlib.h zconf.h  
