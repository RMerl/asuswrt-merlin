# Target: Renesas M32R running GNU/Linux
TDEPFILES= m32r-tdep.o m32r-linux-tdep.o remote-m32r-sdi.o glibc-tdep.o solib.o solib-svr4.o solib-legacy.o symfile-mem.o

SIM_OBS = remote-sim.o
SIM = ../sim/m32r/libsim.a
