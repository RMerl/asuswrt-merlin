# Target: ARM based machine running GNU/Linux
DEPRECATED_TM_FILE= tm-arm.h
TDEPFILES= arm-tdep.o arm-linux-tdep.o glibc-tdep.o solib.o \
  solib-svr4.o solib-legacy.o symfile-mem.o \
  corelow.o

