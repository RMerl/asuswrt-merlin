# Target: PowerPC running eabi and including the simulator
TDEPFILES= rs6000-tdep.o monitor.o dsrec.o ppcbug-rom.o dink32-rom.o ppc-sysv-tdep.o solib.o solib-svr4.o
DEPRECATED_TM_FILE= tm-ppc-eabi.h

SIM_OBS = remote-sim.o
SIM = ../sim/ppc/libsim.a
