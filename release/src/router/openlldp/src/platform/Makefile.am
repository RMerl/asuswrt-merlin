noinst_LIBRARIES = libplatform.a

if FRAMER_LINUX
  FRAMER_SRCS = ${srcdir}/framehandlers/linux/lldp_linux_framer.h \
		\
		${srcdir}/framehandlers/linux/lldp_linux_framer.c
endif
if FRAMER_DARWIN
  FRAMER_SRCS = 
endif
if FRAMER_GENERIC
  FRAMER_SRCS = 
endif
if FRAMER_WINDOWS
  FRAMER_SRCS = 
endif
if FRAMER_BPF
  FRAMER_SRCS = ${srcdir}/framehandlers/bpf/bpflib.h framehandlers/bpf/lldp_bpf_framer.h \
		\
		${srcdir}/framehandlers/bpf/bpflib.c framehandlers/bpf/lldp_bpf_framer.c
endif

if LINUX
  PLATFORM_SRCS = 
endif
if DARWIN
  PLATFORM_SRCS = 
endif
if WINDOWS
  PLATFORM_SRCS = 
endif

libplatform_a_SOURCES = ${srcdir}/framehandler.h \
                        $(FRAMER_SRCS) $(PLATFORM_SRCS)

