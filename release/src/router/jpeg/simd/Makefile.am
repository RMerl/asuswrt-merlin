noinst_LTLIBRARIES = libsimd.la

BUILT_SOURCES = jsimdcfg.inc

EXTRA_DIST = nasm_lt.sh CMakeLists.txt \
	jccolext-mmx.asm   jcgryext-mmx.asm   jdcolext-mmx.asm   jdmrgext-mmx.asm \
	jccolext-sse2.asm  jcgryext-sse2.asm  jdcolext-sse2.asm  jdmrgext-sse2.asm \
	jccolext-sse2-64.asm  jcgryext-sse2-64.asm  jdcolext-sse2-64.asm \
	jdmrgext-sse2-64.asm

if SIMD_X86_64

libsimd_la_SOURCES = jsimd_x86_64.c jsimd.h jsimdcfg.inc.h jsimdext.inc \
	jcolsamp.inc jdct.inc jfdctflt-sse-64.asm \
	jccolor-sse2-64.asm   jcgray-sse2-64.asm    jcsample-sse2-64.asm \
	jdcolor-sse2-64.asm   jdmerge-sse2-64.asm   jdsample-sse2-64.asm \
	jfdctfst-sse2-64.asm  jfdctint-sse2-64.asm  jidctflt-sse2-64.asm \
	jidctfst-sse2-64.asm  jidctint-sse2-64.asm  jidctred-sse2-64.asm  \
	jquantf-sse2-64.asm   jquanti-sse2-64.asm

jccolor-sse2-64.lo:  jccolext-sse2-64.asm
jcgray-sse2-64.lo:   jcgryext-sse2-64.asm
jdcolor-sse2-64.lo:  jdcolext-sse2-64.asm
jdmerge-sse2-64.lo:  jdmrgext-sse2-64.asm

endif

if SIMD_I386

libsimd_la_SOURCES = jsimd_i386.c jsimd.h jsimdcfg.inc.h jsimdext.inc \
	jcolsamp.inc jdct.inc jsimdcpu.asm \
	jfdctflt-3dn.asm   jidctflt-3dn.asm   jquant-3dn.asm \
	jccolor-mmx.asm    jcgray-mmx.asm     jcsample-mmx.asm \
	jdcolor-mmx.asm    jdmerge-mmx.asm    jdsample-mmx.asm \
	jfdctfst-mmx.asm   jfdctint-mmx.asm   jidctfst-mmx.asm \
	jidctint-mmx.asm   jidctred-mmx.asm   jquant-mmx.asm \
	jfdctflt-sse.asm   jidctflt-sse.asm   jquant-sse.asm \
	jccolor-sse2.asm   jcgray-sse2.asm    jcsample-sse2.asm \
	jdcolor-sse2.asm   jdmerge-sse2.asm   jdsample-sse2.asm \
	jfdctfst-sse2.asm  jfdctint-sse2.asm  jidctflt-sse2.asm \
	jidctfst-sse2.asm  jidctint-sse2.asm  jidctred-sse2.asm  \
	jquantf-sse2.asm   jquanti-sse2.asm

jccolor-mmx.lo:   jccolext-mmx.asm
jcgray.-mmx.lo:   jcgryext-mmx.asm
jdcolor-mmx.lo:   jdcolext-mmx.asm
jdmerge-mmx.lo:   jdmrgext-mmx.asm
jccolor-sse2.lo:  jccolext-sse2.asm
jcgray-sse2.lo:   jcgryext-sse2.asm
jdcolor-sse2.lo:  jdcolext-sse2.asm
jdmerge-sse2.lo:  jdmrgext-sse2.asm

endif

if SIMD_ARM

libsimd_la_SOURCES = jsimd_arm.c jsimd_arm_neon.S

endif

if SIMD_ARM_64

libsimd_la_SOURCES = jsimd_arm64.c jsimd_arm64_neon.S

endif

if SIMD_MIPS

libsimd_la_SOURCES = jsimd_mips.c jsimd_mips_dspr2_asm.h jsimd_mips_dspr2.S

endif

AM_CPPFLAGS = -I$(top_srcdir)

.asm.lo:
	$(AM_V_GEN) $(LIBTOOL) $(AM_V_lt) --mode=compile --tag NASM $(srcdir)/nasm_lt.sh $(AM_V_lt) $(NASM) $(NAFLAGS) -I$(srcdir) -I. $< -o $@

jsimdcfg.inc: $(srcdir)/jsimdcfg.inc.h ../jpeglib.h ../jconfig.h ../jmorecfg.h
	$(AM_V_GEN) $(CPP) -I$(top_builddir) -I$(top_builddir)/simd $(srcdir)/jsimdcfg.inc.h | $(EGREP) "^[\;%]|^\ %" | sed 's%_cpp_protection_%%' | sed 's@% define@%define@g' > $@
