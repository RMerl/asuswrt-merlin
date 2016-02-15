lib_LTLIBRARIES = libjpeg.la
libjpeg_la_LDFLAGS = -version-info ${LIBTOOL_CURRENT}:${SO_MINOR_VERSION}:${SO_AGE} -no-undefined
include_HEADERS = jerror.h jmorecfg.h jpeglib.h

if WITH_TURBOJPEG
lib_LTLIBRARIES += libturbojpeg.la
libturbojpeg_la_LDFLAGS = -version-info 1:0:1 -no-undefined
include_HEADERS += turbojpeg.h
endif

nodist_include_HEADERS = jconfig.h


HDRS = jchuff.h jdct.h jdhuff.h jerror.h jinclude.h jmemsys.h jmorecfg.h \
	jpegint.h jpeglib.h jversion.h jsimd.h jsimddct.h jpegcomp.h \
	jpeg_nbits_table.h

libjpeg_la_SOURCES = $(HDRS) jcapimin.c jcapistd.c jccoefct.c jccolor.c \
	jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c \
	jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c jctrans.c \
	jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c \
	jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c \
	jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c \
	jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c \
	jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c

if WITH_ARITH
libjpeg_la_SOURCES += jaricom.c
endif

if WITH_ARITH_ENC
libjpeg_la_SOURCES += jcarith.c
endif

if WITH_ARITH_DEC
libjpeg_la_SOURCES += jdarith.c
endif


SUBDIRS = java


if WITH_TURBOJPEG

libturbojpeg_la_SOURCES = $(libjpeg_la_SOURCES) turbojpeg.c turbojpeg.h \
	transupp.c transupp.h jdatadst-tj.c jdatasrc-tj.c

if WITH_JAVA

libturbojpeg_la_SOURCES += turbojpeg-jni.c
libturbojpeg_la_CFLAGS = ${JNI_CFLAGS}
TJMAPFILE = turbojpeg-mapfile.jni

else

TJMAPFILE = turbojpeg-mapfile

endif

libturbojpeg_la_SOURCES += $(TJMAPFILE)

if VERSION_SCRIPT
libturbojpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)$(srcdir)/$(TJMAPFILE)
endif

endif


if VERSION_SCRIPT
libjpeg_la_LDFLAGS += $(VERSION_SCRIPT_FLAG)libjpeg.map
endif


if WITH_SIMD

SUBDIRS += simd
libjpeg_la_LIBADD = simd/libsimd.la
libturbojpeg_la_LIBADD = simd/libsimd.la

else

libjpeg_la_SOURCES += jsimd_none.c

endif


bin_PROGRAMS = cjpeg djpeg jpegtran rdjpgcom wrjpgcom
noinst_PROGRAMS = jcstest


if WITH_TURBOJPEG

bin_PROGRAMS += tjbench

noinst_PROGRAMS += tjunittest

tjbench_SOURCES = tjbench.c bmp.h bmp.c tjutil.h tjutil.c rdbmp.c rdppm.c \
	wrbmp.c wrppm.c

tjbench_LDADD = libturbojpeg.la libjpeg.la -lm

tjbench_CFLAGS = -DBMP_SUPPORTED -DPPM_SUPPORTED

tjunittest_SOURCES = tjunittest.c tjutil.h tjutil.c

tjunittest_LDADD = libturbojpeg.la

endif


cjpeg_SOURCES = cdjpeg.h cderror.h cdjpeg.c cjpeg.c rdgif.c rdppm.c rdswitch.c
if WITH_12BIT
else
cjpeg_SOURCES += rdbmp.c rdtarga.c
endif

cjpeg_LDADD = libjpeg.la

cjpeg_CFLAGS = -DGIF_SUPPORTED -DPPM_SUPPORTED
if WITH_12BIT
else
cjpeg_CFLAGS += -DBMP_SUPPORTED -DTARGA_SUPPORTED
endif

djpeg_SOURCES = cdjpeg.h cderror.h cdjpeg.c djpeg.c rdcolmap.c rdswitch.c \
	wrgif.c wrppm.c
if WITH_12BIT
else
djpeg_SOURCES += wrbmp.c wrtarga.c
endif

djpeg_LDADD = libjpeg.la

djpeg_CFLAGS = -DGIF_SUPPORTED -DPPM_SUPPORTED
if WITH_12BIT
else
djpeg_CFLAGS += -DBMP_SUPPORTED -DTARGA_SUPPORTED
endif

jpegtran_SOURCES = jpegtran.c rdswitch.c cdjpeg.c transupp.c transupp.h

jpegtran_LDADD = libjpeg.la

rdjpgcom_SOURCES = rdjpgcom.c

rdjpgcom_LDADD = libjpeg.la

wrjpgcom_SOURCES = wrjpgcom.c

wrjpgcom_LDADD = libjpeg.la

jcstest_SOURCES = jcstest.c

jcstest_LDADD = libjpeg.la

dist_man1_MANS = cjpeg.1 djpeg.1 jpegtran.1 rdjpgcom.1 wrjpgcom.1

DOCS= coderules.txt jconfig.txt change.log rdrle.c wrrle.c BUILDING.txt \
	ChangeLog.txt

dist_doc_DATA = README README-turbo.txt libjpeg.txt structure.txt usage.txt \
	wizard.txt

exampledir = $(docdir)
dist_example_DATA = example.c


EXTRA_DIST = win release $(DOCS) testimages CMakeLists.txt \
	sharedlib/CMakeLists.txt cmakescripts libjpeg.map.in doc doxygen.config \
	doxygen-extra.css jccolext.c jdcolext.c jdcol565.c jdmrgext.c jdmrg565.c \
	jstdhuff.c LICENSE.txt

dist-hook:
	rm -rf `find $(distdir) -name .svn`


SUBDIRS += md5

if WITH_12BIT

TESTORIG = testorig12.jpg
MD5_JPEG_RGB_ISLOW = 9620f424569594bb9242b48498ad801f
MD5_PPM_RGB_ISLOW = f3301d2219783b8b3d942b7239fa50c0
MD5_JPEG_422_IFAST_OPT = 7322e3bd2f127f7de4b40d4480ce60e4
MD5_PPM_422_IFAST = 79807fa552899e66a04708f533e16950
MD5_PPM_422M_IFAST = 07737bfe8a7c1c87aaa393a0098d16b0
MD5_JPEG_420_IFAST_Q100_PROG = a1da220b5604081863a504297ed59e55
MD5_PPM_420_Q100_IFAST = 1b3730122709f53d007255e8dfd3305e
MD5_PPM_420M_Q100_IFAST = 980a1a3c5bf9510022869d30b7d26566
MD5_JPEG_GRAY_ISLOW = 235c90707b16e2e069f37c888b2636d9
MD5_PPM_GRAY_ISLOW = 7213c10af507ad467da5578ca5ee1fca
MD5_PPM_GRAY_ISLOW_RGB = e96ee81c30a6ed422d466338bd3de65d
MD5_JPEG_420S_IFAST_OPT = 7af8e60be4d9c227ec63ac9b6630855e
MD5_JPEG_3x2_FLOAT_PROG_SSE = a8c17daf77b457725ec929e215b603f8
MD5_PPM_3x2_FLOAT_SSE = 42876ab9e5c2f76a87d08db5fbd57956
MD5_JPEG_3x2_FLOAT_PROG_32BIT = a8c17daf77b457725ec929e215b603f8
MD5_PPM_3x2_FLOAT_32BIT = 42876ab9e5c2f76a87d08db5fbd57956
MD5_PPM_3x2_FLOAT_64BIT = d6fbc71153b3d8ded484dbc17c7b9cf4
MD5_JPEG_3x2_IFAST_PROG = 1396cc2b7185cfe943d408c9d305339e
MD5_PPM_3x2_IFAST = 3975985ef6eeb0a2cdc58daa651ccc00
MD5_PPM_420M_ISLOW_2_1 = 4ca6be2a6f326ff9eaab63e70a8259c0
MD5_PPM_420M_ISLOW_15_8 = 12aa9f9534c1b3d7ba047322226365eb
MD5_PPM_420M_ISLOW_13_8 = f7e22817c7b25e1393e4ec101e9d4e96
MD5_PPM_420M_ISLOW_11_8 = 800a16f9f4dc9b293197bfe11be10a82
MD5_PPM_420M_ISLOW_9_8 = 06b7a92a9bc69f4dc36ec40f1937d55c
MD5_PPM_420M_ISLOW_7_8 = 3ec444a14a4ab4eab88ffc49c48eca43
MD5_PPM_420M_ISLOW_3_4 = 3e726b7ea872445b19437d1c1d4f0d93
MD5_PPM_420M_ISLOW_5_8 = a8a771abdc94301d20ffac119b2caccd
MD5_PPM_420M_ISLOW_1_2 = b419124dd5568b085787234866102866
MD5_PPM_420M_ISLOW_3_8 = 343d19015531b7bbe746124127244fa8
MD5_PPM_420M_ISLOW_1_4 = 35fd59d866e44659edfa3c18db2a3edb
MD5_PPM_420M_ISLOW_1_8 = ccaed48ac0aedefda5d4abe4013f4ad7
MD5_JPEG_CROP = cdb35ff4b4519392690ea040c56ea99c

else

TESTORIG = testorig.jpg
MD5_JPEG_RGB_ISLOW = 768e970dd57b340ff1b83c9d3d47c77b
MD5_PPM_RGB_ISLOW = 00a257f5393fef8821f2b88ac7421291
MD5_BMP_RGB_ISLOW_565 = f07d2e75073e4bb10f6c6f4d36e2e3be
MD5_BMP_RGB_ISLOW_565D = 4cfa0928ef3e6bb626d7728c924cfda4
MD5_JPEG_422_IFAST_OPT = 2540287b79d913f91665e660303ab2c8
MD5_PPM_422_IFAST = 35bd6b3f833bad23de82acea847129fa
MD5_PPM_422M_IFAST = 8dbc65323d62cca7c91ba02dd1cfa81d
MD5_BMP_422M_IFAST_565 = 3294bd4d9a1f2b3d08ea6020d0db7065
MD5_BMP_422M_IFAST_565D = da98c9c7b6039511be4a79a878a9abc1
MD5_JPEG_420_IFAST_Q100_PROG = 990cbe0329c882420a2094da7e5adade
MD5_PPM_420_Q100_IFAST = 5a732542015c278ff43635e473a8a294
MD5_PPM_420M_Q100_IFAST = ff692ee9323a3b424894862557c092f1
MD5_JPEG_GRAY_ISLOW = 72b51f894b8f4a10b3ee3066770aa38d
MD5_PPM_GRAY_ISLOW = 8d3596c56eace32f205deccc229aa5ed
MD5_PPM_GRAY_ISLOW_RGB = 116424ac07b79e5e801f00508eab48ec
MD5_BMP_GRAY_ISLOW_565 = 12f78118e56a2f48b966f792fedf23cc
MD5_BMP_GRAY_ISLOW_565D = bdbbd616441a24354c98553df5dc82db
MD5_JPEG_420S_IFAST_OPT = 388708217ac46273ca33086b22827ed8
# See README-turbo.txt for more details on why this next bit is necessary.
MD5_JPEG_3x2_FLOAT_PROG_SSE = 343e3f8caf8af5986ebaf0bdc13b5c71
MD5_PPM_3x2_FLOAT_SSE = 1a75f36e5904d6fc3a85a43da9ad89bb
MD5_JPEG_3x2_FLOAT_PROG_32BIT = 9bca803d2042bd1eb03819e2bf92b3e5
MD5_PPM_3x2_FLOAT_32BIT = f6bfab038438ed8f5522fbd33595dcdc
MD5_PPM_3x2_FLOAT_64BIT = 0e917a34193ef976b679a6b069b1be26
MD5_JPEG_3x2_IFAST_PROG = 1ee5d2c1a77f2da495f993c8c7cceca5
MD5_PPM_3x2_IFAST = fd283664b3b49127984af0a7f118fccd
MD5_JPEG_420_ISLOW_ARI = e986fb0a637a8d833d96e8a6d6d84ea1
MD5_JPEG_444_ISLOW_PROGARI = 0a8f1c8f66e113c3cf635df0a475a617
MD5_PPM_420M_IFAST_ARI = 72b59a99bcf1de24c5b27d151bde2437
MD5_JPEG_420_ISLOW = 9a68f56bc76e466aa7e52f415d0f4a5f
MD5_PPM_420M_ISLOW_2_1 = 9f9de8c0612f8d06869b960b05abf9c9
MD5_PPM_420M_ISLOW_15_8 = b6875bc070720b899566cc06459b63b7
MD5_PPM_420M_ISLOW_13_8 = bc3452573c8152f6ae552939ee19f82f
MD5_PPM_420M_ISLOW_11_8 = d8cc73c0aaacd4556569b59437ba00a5
MD5_PPM_420M_ISLOW_9_8 = d25e61bc7eac0002f5b393aa223747b6
MD5_PPM_420M_ISLOW_7_8 = ddb564b7c74a09494016d6cd7502a946
MD5_PPM_420M_ISLOW_3_4 = 8ed8e68808c3fbc4ea764fc9d2968646
MD5_PPM_420M_ISLOW_5_8 = a3363274999da2366a024efae6d16c9b
MD5_PPM_420M_ISLOW_1_2 = e692a315cea26b988c8e8b29a5dbcd81
MD5_PPM_420M_ISLOW_3_8 = 79eca9175652ced755155c90e785a996
MD5_PPM_420M_ISLOW_1_4 = 79cd778f8bf1a117690052cacdd54eca
MD5_PPM_420M_ISLOW_1_8 = 391b3d4aca640c8567d6f8745eb2142f
MD5_BMP_420_ISLOW_256 = 4980185e3776e89bd931736e1cddeee6
MD5_BMP_420_ISLOW_565 = bf9d13e16c4923b92e1faa604d7922cb
MD5_BMP_420_ISLOW_565D = 6bde71526acc44bcff76f696df8638d2
MD5_BMP_420M_ISLOW_565 = 8dc0185245353cfa32ad97027342216f
MD5_BMP_420M_ISLOW_565D =d1be3a3339166255e76fa50a0d70d73e
MD5_JPEG_CROP = b4197f377e621c4e9b1d20471432610d

endif

.PHONY: test
test: tjquicktest tjbittest bittest

tjquicktest: testclean all

if WITH_TURBOJPEG
if WITH_JAVA
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -bi
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv -noyuvpad
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv -bi
	$(JAVA) -cp java/turbojpeg.jar -Djava.library.path=.libs TJUnitTest -yuv -bi -noyuvpad
endif
	./tjunittest
	./tjunittest -alloc
	./tjunittest -yuv
	./tjunittest -yuv -alloc
	./tjunittest -yuv -noyuvpad
endif
	echo GREAT SUCCESS!

tjbittest: testclean all

if WITH_TURBOJPEG

MD5_PPM_GRAY_TILE = 89d3ca21213d9d864b50b4e4e7de4ca6
MD5_PPM_420_8x8_TILE = 847fceab15c5b7b911cb986cf0f71de3
MD5_PPM_420_16x16_TILE = ca45552a93687e078f7137cc4126a7b0
MD5_PPM_420_32x32_TILE = d8676f1d6b68df358353bba9844f4a00
MD5_PPM_420_64x64_TILE = 4e4c1a3d7ea4bace4f868bcbe83b7050
MD5_PPM_420_128x128_TILE = f24c3429c52265832beab9df72a0ceae
MD5_PPM_420M_8x8_TILE = bc25320e1f4c31ce2e610e43e9fd173c
MD5_PPM_420M_TILE = 75ffdf14602258c5c189522af57fa605
MD5_PPM_422_8x8_TILE = d83dacd9fc73b0a6f10c09acad64eb1e
MD5_PPM_422_16x16_TILE = 35077fb610d72dd743b1eb0cbcfe10fb
MD5_PPM_422_32x32_TILE = e6902ed8a449ecc0f0d6f2bf945f65f7
MD5_PPM_422_64x64_TILE = 2b4502a8f316cedbde1da7bce3d2231e
MD5_PPM_422_128x128_TILE = f0b5617d578f5e13c8eee215d64d4877
MD5_PPM_422M_8x8_TILE = 828941d7f41cd6283abd6beffb7fd51d
MD5_PPM_422M_TILE = e877ae1324c4a280b95376f7f018172f
MD5_PPM_444_TILE = 7964e41e67cfb8d0a587c0aa4798f9c3

# Test compressing from/decompressing to an arbitrary subregion of a larger
# image buffer
	cp $(srcdir)/testimages/testorig.ppm testout_tile.ppm
	./tjbench testout_tile.ppm 95 -rgb -quiet -tile -benchtime 0.01 >/dev/null 2>&1
	for i in 8 16 32 64 128; do \
		md5/md5cmp $(MD5_PPM_GRAY_TILE) testout_tile_GRAY_Q95_$$i\x$$i.ppm; \
	done
	md5/md5cmp $(MD5_PPM_420_8x8_TILE) testout_tile_420_Q95_8x8.ppm
	md5/md5cmp $(MD5_PPM_420_16x16_TILE) testout_tile_420_Q95_16x16.ppm
	md5/md5cmp $(MD5_PPM_420_32x32_TILE) testout_tile_420_Q95_32x32.ppm
	md5/md5cmp $(MD5_PPM_420_64x64_TILE) testout_tile_420_Q95_64x64.ppm
	md5/md5cmp $(MD5_PPM_420_128x128_TILE) testout_tile_420_Q95_128x128.ppm
	md5/md5cmp $(MD5_PPM_422_8x8_TILE) testout_tile_422_Q95_8x8.ppm
	md5/md5cmp $(MD5_PPM_422_16x16_TILE) testout_tile_422_Q95_16x16.ppm
	md5/md5cmp $(MD5_PPM_422_32x32_TILE) testout_tile_422_Q95_32x32.ppm
	md5/md5cmp $(MD5_PPM_422_64x64_TILE) testout_tile_422_Q95_64x64.ppm
	md5/md5cmp $(MD5_PPM_422_128x128_TILE) testout_tile_422_Q95_128x128.ppm
	for i in 8 16 32 64 128; do \
		md5/md5cmp $(MD5_PPM_444_TILE) testout_tile_444_Q95_$$i\x$$i.ppm; \
	done
	rm testout_tile_GRAY_* testout_tile_420_* testout_tile_422_* testout_tile_444_*

	./tjbench testout_tile.ppm 95 -rgb -fastupsample -quiet -tile -benchtime 0.01 >/dev/null 2>&1
	md5/md5cmp $(MD5_PPM_420M_8x8_TILE) testout_tile_420_Q95_8x8.ppm
	for i in 16 32 64 128; do \
		md5/md5cmp $(MD5_PPM_420M_TILE) testout_tile_420_Q95_$$i\x$$i.ppm; \
	done
	md5/md5cmp $(MD5_PPM_422M_8x8_TILE) testout_tile_422_Q95_8x8.ppm
	for i in 16 32 64 128; do \
		md5/md5cmp $(MD5_PPM_422M_TILE) testout_tile_422_Q95_$$i\x$$i.ppm; \
	done
	rm testout_tile_GRAY_* testout_tile_420_* testout_tile_422_* testout_tile_444_* testout_tile.ppm
	echo GREAT SUCCESS!

endif

bittest: testclean all

# These tests are carefully crafted to provide full coverage of as many of the
# underlying algorithms as possible (including all of the SIMD-accelerated
# ones.)

# CC: null  SAMP: fullsize  FDCT: islow  ENT: huff
	./cjpeg -rgb -dct int -outfile testout_rgb_islow.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_RGB_ISLOW) testout_rgb_islow.jpg
# CC: null  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -ppm -outfile testout_rgb_islow.ppm testout_rgb_islow.jpg
	md5/md5cmp $(MD5_PPM_RGB_ISLOW) testout_rgb_islow.ppm
	rm testout_rgb_islow.ppm
if WITH_12BIT
	rm testout_rgb_islow.jpg
else
# CC: RGB->RGB565  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb565 -dither none -bmp -outfile testout_rgb_islow_565.bmp testout_rgb_islow.jpg
	md5/md5cmp $(MD5_BMP_RGB_ISLOW_565) testout_rgb_islow_565.bmp
	rm testout_rgb_islow_565.bmp
# CC: RGB->RGB565 (dithered)  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb565 -bmp -outfile testout_rgb_islow_565D.bmp testout_rgb_islow.jpg
	md5/md5cmp $(MD5_BMP_RGB_ISLOW_565D) testout_rgb_islow_565D.bmp
	rm testout_rgb_islow_565D.bmp testout_rgb_islow.jpg
endif

# CC: RGB->YCC  SAMP: fullsize/h2v1  FDCT: ifast  ENT: 2-pass huff
	./cjpeg -sample 2x1 -dct fast -opt -outfile testout_422_ifast_opt.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_422_IFAST_OPT) testout_422_ifast_opt.jpg
# CC: YCC->RGB  SAMP: fullsize/h2v1 fancy  IDCT: ifast  ENT: huff
	./djpeg -dct fast -outfile testout_422_ifast.ppm testout_422_ifast_opt.jpg
	md5/md5cmp $(MD5_PPM_422_IFAST) testout_422_ifast.ppm
	rm testout_422_ifast.ppm
# CC: YCC->RGB  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
	./djpeg -dct fast -nosmooth -outfile testout_422m_ifast.ppm testout_422_ifast_opt.jpg
	md5/md5cmp $(MD5_PPM_422M_IFAST) testout_422m_ifast.ppm
	rm testout_422m_ifast.ppm
if WITH_12BIT
	rm testout_422_ifast_opt.jpg
else
# CC: YCC->RGB565  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
	./djpeg -dct int -nosmooth -rgb565 -dither none -bmp -outfile testout_422m_ifast_565.bmp testout_422_ifast_opt.jpg
	md5/md5cmp $(MD5_BMP_422M_IFAST_565) testout_422m_ifast_565.bmp
	rm testout_422m_ifast_565.bmp
# CC: YCC->RGB565 (dithered)  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
	./djpeg -dct int -nosmooth -rgb565 -bmp -outfile testout_422m_ifast_565D.bmp testout_422_ifast_opt.jpg
	md5/md5cmp $(MD5_BMP_422M_IFAST_565D) testout_422m_ifast_565D.bmp
	rm testout_422m_ifast_565D.bmp testout_422_ifast_opt.jpg
endif

# CC: RGB->YCC  SAMP: fullsize/h2v2  FDCT: ifast  ENT: prog huff
	./cjpeg -sample 2x2 -quality 100 -dct fast -prog -outfile testout_420_q100_ifast_prog.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_420_IFAST_Q100_PROG) testout_420_q100_ifast_prog.jpg
# CC: YCC->RGB  SAMP: fullsize/h2v2 fancy  IDCT: ifast  ENT: prog huff
	./djpeg -dct fast -outfile testout_420_q100_ifast.ppm testout_420_q100_ifast_prog.jpg
	md5/md5cmp $(MD5_PPM_420_Q100_IFAST) testout_420_q100_ifast.ppm
	rm testout_420_q100_ifast.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: ifast  ENT: prog huff
	./djpeg -dct fast -nosmooth -outfile testout_420m_q100_ifast.ppm testout_420_q100_ifast_prog.jpg
	md5/md5cmp $(MD5_PPM_420M_Q100_IFAST) testout_420m_q100_ifast.ppm
	rm testout_420m_q100_ifast.ppm testout_420_q100_ifast_prog.jpg

# CC: RGB->Gray  SAMP: fullsize  FDCT: islow  ENT: huff
	./cjpeg -gray -dct int -outfile testout_gray_islow.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_GRAY_ISLOW) testout_gray_islow.jpg
# CC: Gray->Gray  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -outfile testout_gray_islow.ppm testout_gray_islow.jpg
	md5/md5cmp $(MD5_PPM_GRAY_ISLOW) testout_gray_islow.ppm
	rm testout_gray_islow.ppm
# CC: Gray->RGB  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb -outfile testout_gray_islow_rgb.ppm testout_gray_islow.jpg
	md5/md5cmp $(MD5_PPM_GRAY_ISLOW_RGB) testout_gray_islow_rgb.ppm
	rm testout_gray_islow_rgb.ppm
if WITH_12BIT
	rm testout_gray_islow.jpg
else
# CC: Gray->RGB565  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb565 -dither none -bmp -outfile testout_gray_islow_565.bmp testout_gray_islow.jpg
	md5/md5cmp $(MD5_BMP_GRAY_ISLOW_565) testout_gray_islow_565.bmp
	rm testout_gray_islow_565.bmp
# CC: Gray->RGB565 (dithered)  SAMP: fullsize  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb565 -bmp -outfile testout_gray_islow_565D.bmp testout_gray_islow.jpg
	md5/md5cmp $(MD5_BMP_GRAY_ISLOW_565D) testout_gray_islow_565D.bmp
	rm testout_gray_islow_565D.bmp testout_gray_islow.jpg
endif

# CC: RGB->YCC  SAMP: fullsize smooth/h2v2 smooth  FDCT: islow
# ENT: 2-pass huff
	./cjpeg -sample 2x2 -smooth 1 -dct int -opt -outfile testout_420s_ifast_opt.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_420S_IFAST_OPT) testout_420s_ifast_opt.jpg
	rm testout_420s_ifast_opt.jpg

# The output of the floating point tests is not validated by default, because
# the output differs depending on the type of floating point math used, and
# this is only deterministic if the DCT/IDCT are implemented using SIMD
# instructions on a particular platform.  Pass one of the following on the make
# command line to validate the floating point tests against one of the expected # results:
#
# FLOATTEST=sse  validate against the expected results from the libjpeg-turbo
#                SSE SIMD extensions
# FLOATTEST=32bit  validate against the expected results from the C code
#                  when running on a 32-bit FPU (or when SSE is being used for
#                  floating point math, which is generally the default with
#                  x86-64 compilers)
# FLOATTEST=64bit  validate against the exepected results from the C code
#                  when running on a 64-bit FPU

# CC: RGB->YCC  SAMP: fullsize/int  FDCT: float  ENT: prog huff
	./cjpeg -sample 3x2 -dct float -prog -outfile testout_3x2_float_prog.jpg $(srcdir)/testimages/testorig.ppm
	if [ "${FLOATTEST}" = "sse" ]; then \
		md5/md5cmp $(MD5_JPEG_3x2_FLOAT_PROG_SSE) testout_3x2_float_prog.jpg; \
	elif [ "${FLOATTEST}" = "32bit" -o "${FLOATTEST}" = "64bit" ]; then \
		md5/md5cmp $(MD5_JPEG_3x2_FLOAT_PROG_32BIT) testout_3x2_float_prog.jpg; \
	fi
# CC: YCC->RGB  SAMP: fullsize/int  IDCT: float  ENT: prog huff
	./djpeg -dct float -outfile testout_3x2_float.ppm testout_3x2_float_prog.jpg
	if [ "${FLOATTEST}" = "sse" ]; then \
		md5/md5cmp $(MD5_PPM_3x2_FLOAT_SSE) testout_3x2_float.ppm; \
	elif [ "${FLOATTEST}" = "32bit" ]; then \
		md5/md5cmp $(MD5_PPM_3x2_FLOAT_32BIT) testout_3x2_float.ppm; \
	elif [ "${FLOATTEST}" = "64bit" ]; then \
		md5/md5cmp $(MD5_PPM_3x2_FLOAT_64BIT) testout_3x2_float.ppm; \
	fi
	rm testout_3x2_float.ppm testout_3x2_float_prog.jpg

# CC: RGB->YCC  SAMP: fullsize/int  FDCT: ifast  ENT: prog huff
	./cjpeg -sample 3x2 -dct fast -prog -outfile testout_3x2_ifast_prog.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_3x2_IFAST_PROG) testout_3x2_ifast_prog.jpg
# CC: YCC->RGB  SAMP: fullsize/int  IDCT: ifast  ENT: prog huff
	./djpeg -dct fast -outfile testout_3x2_ifast.ppm testout_3x2_ifast_prog.jpg
	md5/md5cmp $(MD5_PPM_3x2_IFAST) testout_3x2_ifast.ppm
	rm testout_3x2_ifast.ppm testout_3x2_ifast_prog.jpg

if WITH_ARITH_ENC
# CC: YCC->RGB  SAMP: fullsize/h2v2  FDCT: islow  ENT: arith
	./cjpeg -dct int -arithmetic -outfile testout_420_islow_ari.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_420_ISLOW_ARI) testout_420_islow_ari.jpg
	rm testout_420_islow_ari.jpg
	./jpegtran -arithmetic -outfile testout_420_islow_ari.jpg $(srcdir)/testimages/testimgint.jpg
	md5/md5cmp $(MD5_JPEG_420_ISLOW_ARI) testout_420_islow_ari.jpg
	rm testout_420_islow_ari.jpg
# CC: YCC->RGB  SAMP: fullsize  FDCT: islow  ENT: prog arith
	./cjpeg -sample 1x1 -dct int -progressive -arithmetic -outfile testout_444_islow_progari.jpg $(srcdir)/testimages/testorig.ppm
	md5/md5cmp $(MD5_JPEG_444_ISLOW_PROGARI) testout_444_islow_progari.jpg
	rm testout_444_islow_progari.jpg
endif
if WITH_ARITH_DEC
# CC: RGB->YCC  SAMP: h2v2 merged  IDCT: ifast  ENT: arith
	./djpeg -fast -ppm -outfile testout_420m_ifast_ari.ppm $(srcdir)/testimages/testimgari.jpg
	md5/md5cmp $(MD5_PPM_420M_IFAST_ARI) testout_420m_ifast_ari.ppm
	rm testout_420m_ifast_ari.ppm
	./jpegtran -outfile testout_420_islow.jpg $(srcdir)/testimages/testimgari.jpg
	md5/md5cmp $(MD5_JPEG_420_ISLOW) testout_420_islow.jpg
	rm testout_420_islow.jpg
endif

# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 16x16 islow  ENT: huff
	./djpeg -dct int -scale 2/1 -nosmooth -ppm -outfile testout_420m_islow_2_1.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_2_1) testout_420m_islow_2_1.ppm
	rm testout_420m_islow_2_1.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 15x15 islow  ENT: huff
	./djpeg -dct int -scale 15/8 -nosmooth -ppm -outfile testout_420m_islow_15_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_15_8) testout_420m_islow_15_8.ppm
	rm testout_420m_islow_15_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 13x13 islow  ENT: huff
	./djpeg -dct int -scale 13/8 -nosmooth -ppm -outfile testout_420m_islow_13_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_13_8) testout_420m_islow_13_8.ppm
	rm testout_420m_islow_13_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 11x11 islow  ENT: huff
	./djpeg -dct int -scale 11/8 -nosmooth -ppm -outfile testout_420m_islow_11_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_11_8) testout_420m_islow_11_8.ppm
	rm testout_420m_islow_11_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 9x9 islow  ENT: huff
	./djpeg -dct int -scale 9/8 -nosmooth -ppm -outfile testout_420m_islow_9_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_9_8) testout_420m_islow_9_8.ppm
	rm testout_420m_islow_9_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 7x7 islow/14x14 islow  ENT: huff
	./djpeg -dct int -scale 7/8 -nosmooth -ppm -outfile testout_420m_islow_7_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_7_8) testout_420m_islow_7_8.ppm
	rm testout_420m_islow_7_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 6x6 islow/12x12 islow  ENT: huff
	./djpeg -dct int -scale 3/4 -nosmooth -ppm -outfile testout_420m_islow_3_4.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_3_4) testout_420m_islow_3_4.ppm
	rm testout_420m_islow_3_4.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 5x5 islow/10x10 islow  ENT: huff
	./djpeg -dct int -scale 5/8 -nosmooth -ppm -outfile testout_420m_islow_5_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_5_8) testout_420m_islow_5_8.ppm
	rm testout_420m_islow_5_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 4x4 islow/8x8 islow  ENT: huff
	./djpeg -dct int -scale 1/2 -nosmooth -ppm -outfile testout_420m_islow_1_2.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_1_2) testout_420m_islow_1_2.ppm
	rm testout_420m_islow_1_2.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 3x3 islow/6x6 islow  ENT: huff
	./djpeg -dct int -scale 3/8 -nosmooth -ppm -outfile testout_420m_islow_3_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_3_8) testout_420m_islow_3_8.ppm
	rm testout_420m_islow_3_8.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 2x2 islow/4x4 islow  ENT: huff
	./djpeg -dct int -scale 1/4 -nosmooth -ppm -outfile testout_420m_islow_1_4.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_1_4) testout_420m_islow_1_4.ppm
	rm testout_420m_islow_1_4.ppm
# CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 1x1 islow/2x2 islow  ENT: huff
	./djpeg -dct int -scale 1/8 -nosmooth -ppm -outfile testout_420m_islow_1_8.ppm $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_PPM_420M_ISLOW_1_8) testout_420m_islow_1_8.ppm
	rm testout_420m_islow_1_8.ppm
if WITH_12BIT
else
# CC: YCC->RGB (dithered)  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
	./djpeg -dct int -colors 256 -bmp -outfile testout_420_islow_256.bmp $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_BMP_420_ISLOW_256) testout_420_islow_256.bmp
	rm testout_420_islow_256.bmp
# CC: YCC->RGB565  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb565 -dither none -bmp -outfile testout_420_islow_565.bmp $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_BMP_420_ISLOW_565) testout_420_islow_565.bmp
	rm testout_420_islow_565.bmp
# CC: YCC->RGB565 (dithered)  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
	./djpeg -dct int -rgb565 -bmp -outfile testout_420_islow_565D.bmp $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_BMP_420_ISLOW_565D) testout_420_islow_565D.bmp
	rm testout_420_islow_565D.bmp
# CC: YCC->RGB565  SAMP: h2v2 merged  IDCT: islow  ENT: huff
	./djpeg -dct int -nosmooth -rgb565 -dither none -bmp -outfile testout_420m_islow_565.bmp $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_BMP_420M_ISLOW_565) testout_420m_islow_565.bmp
	rm testout_420m_islow_565.bmp
# CC: YCC->RGB565 (dithered)  SAMP: h2v2 merged  IDCT: islow  ENT: huff
	./djpeg -dct int -nosmooth -rgb565 -bmp -outfile testout_420m_islow_565D.bmp $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_BMP_420M_ISLOW_565D) testout_420m_islow_565D.bmp
	rm testout_420m_islow_565D.bmp
endif

	./jpegtran -crop 120x90+20+50 -transpose -perfect -outfile testout_crop.jpg $(srcdir)/testimages/$(TESTORIG)
	md5/md5cmp $(MD5_JPEG_CROP) testout_crop.jpg
	rm testout_crop.jpg
	echo GREAT SUCCESS!


testclean:
	rm -f testout*
	rm -f *_GRAY_*.bmp
	rm -f *_GRAY_*.png
	rm -f *_GRAY_*.ppm
	rm -f *_GRAY_*.jpg
	rm -f *_GRAY.yuv
	rm -f *_420_*.bmp
	rm -f *_420_*.png
	rm -f *_420_*.ppm
	rm -f *_420_*.jpg
	rm -f *_420.yuv
	rm -f *_422_*.bmp
	rm -f *_422_*.png
	rm -f *_422_*.ppm
	rm -f *_422_*.jpg
	rm -f *_422.yuv
	rm -f *_444_*.bmp
	rm -f *_444_*.png
	rm -f *_444_*.ppm
	rm -f *_444_*.jpg
	rm -f *_444.yuv
	rm -f *_440_*.bmp
	rm -f *_440_*.png
	rm -f *_440_*.ppm
	rm -f *_440_*.jpg
	rm -f *_440.yuv
	rm -f *_411_*.bmp
	rm -f *_411_*.png
	rm -f *_411_*.ppm
	rm -f *_411_*.jpg
	rm -f *_411.yuv


tjtest:
	sh ./tjbenchtest
	sh ./tjbenchtest -alloc
	sh ./tjbenchtest -yuv
	sh ./tjbenchtest -yuv -alloc
if WITH_JAVA
	sh ./tjbenchtest.java
	sh ./tjbenchtest.java -yuv
endif


pkgscripts/libjpeg-turbo.spec: pkgscripts/libjpeg-turbo.spec.tmpl
	cat pkgscripts/libjpeg-turbo.spec.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__bindir}@$(bindir)@g | sed s@%{__datadir}@$(datadir)@g | \
		sed s@%{__docdir}@$(docdir)@g | sed s@%{__includedir}@$(includedir)@g | \
		sed s@%{__libdir}@$(libdir)@g | sed s@%{__mandir}@$(mandir)@g \
		> pkgscripts/libjpeg-turbo.spec

rpm: all pkgscripts/libjpeg-turbo.spec
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	ln -fs `pwd` $$TMPDIR/BUILD; \
	rm -f ${PKGNAME}-${VERSION}.${RPMARCH}.rpm; \
	rpmbuild -bb --define "_blddir $$TMPDIR/buildroot"  \
		--define "_topdir $$TMPDIR" \
		--target ${RPMARCH} pkgscripts/libjpeg-turbo.spec; \
	cp $$TMPDIR/RPMS/${RPMARCH}/${PKGNAME}-${VERSION}-${BUILD}.${RPMARCH}.rpm \
		${PKGNAME}-${VERSION}.${RPMARCH}.rpm; \
	rm -rf $$TMPDIR

srpm: dist-gzip pkgscripts/libjpeg-turbo.spec
	TMPDIR=`mktemp -d /tmp/${PACKAGE_NAME}-build.XXXXXX`; \
	mkdir -p $$TMPDIR/RPMS; \
	mkdir -p $$TMPDIR/SRPMS; \
	mkdir -p $$TMPDIR/BUILD; \
	mkdir -p $$TMPDIR/SOURCES; \
	mkdir -p $$TMPDIR/SPECS; \
	rm -f ${PKGNAME}-${VERSION}.src.rpm; \
	cp ${PACKAGE_NAME}-${VERSION}.tar.gz $$TMPDIR/SOURCES; \
	cat pkgscripts/libjpeg-turbo.spec | sed s/%{_blddir}/%{_tmppath}/g \
		| sed s/#--\>//g \
		> $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	rpmbuild -bs --define "_topdir $$TMPDIR" $$TMPDIR/SPECS/libjpeg-turbo.spec; \
	cp $$TMPDIR/SRPMS/${PKGNAME}-${VERSION}-${BUILD}.src.rpm \
		${PKGNAME}-${VERSION}.src.rpm; \
	rm -rf $$TMPDIR

pkgscripts/makedpkg: pkgscripts/makedpkg.tmpl
	cat pkgscripts/makedpkg.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__docdir}@$(docdir)@g | sed s@%{__libdir}@$(libdir)@g \
		> pkgscripts/makedpkg

deb: all pkgscripts/makedpkg
	sh pkgscripts/makedpkg

pkgscripts/uninstall: pkgscripts/uninstall.tmpl
	cat pkgscripts/uninstall.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__bindir}@$(bindir)@g | sed s@%{__datadir}@$(datadir)@g | \
		sed s@%{__includedir}@$(includedir)@g | sed s@%{__libdir}@$(libdir)@g | \
		sed s@%{__mandir}@$(mandir)@g > pkgscripts/uninstall

pkgscripts/makemacpkg: pkgscripts/makemacpkg.tmpl
	cat pkgscripts/makemacpkg.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__bindir}@$(bindir)@g | sed s@%{__docdir}@$(docdir)@g | \
		sed s@%{__libdir}@$(libdir)@g > pkgscripts/makemacpkg

if X86_64

udmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg -build32 ${BUILDDIR32}

iosdmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg -build32 ${BUILDDIR32} -buildarmv6 ${BUILDDIRARMV6} -buildarmv7 ${BUILDDIRARMV7} -buildarmv7s ${BUILDDIRARMV7S} -buildarmv8 ${BUILDDIRARMV8} -lipo "${LIPO}"

else

iosdmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg -buildarmv6 ${BUILDDIRARMV6} -buildarmv7 ${BUILDDIRARMV7} -buildarmv7s ${BUILDDIRARMV7S} -buildarmv8 ${BUILDDIRARMV8} -lipo "${LIPO}"

endif

dmg: all pkgscripts/makemacpkg pkgscripts/uninstall
	sh pkgscripts/makemacpkg

pkgscripts/makecygwinpkg: pkgscripts/makecygwinpkg.tmpl
	cat pkgscripts/makecygwinpkg.tmpl | sed s@%{__prefix}@$(prefix)@g | \
		sed s@%{__docdir}@$(docdir)@g | sed s@%{__libdir}@$(libdir)@g \
		> pkgscripts/makecygwinpkg

cygwinpkg: all pkgscripts/makecygwinpkg
	sh pkgscripts/makecygwinpkg
