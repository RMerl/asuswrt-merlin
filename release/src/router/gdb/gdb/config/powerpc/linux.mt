# Target: Motorola PPC on Linux
TDEPFILES= rs6000-tdep.o ppc-linux-tdep.o ppc-sysv-tdep.o solib.o \
	solib-svr4.o solib-legacy.o corelow.o symfile-mem.o
DEPRECATED_TM_FILE= tm-ppc-eabi.h

SIM_OBS = remote-sim.o
SIM = ../sim/ppc/libsim.a
