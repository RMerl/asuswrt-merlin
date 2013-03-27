# Target: GNU/Linux Super-H
TDEPFILES= sh-tdep.o sh64-tdep.o sh-linux-tdep.o \
	monitor.o dsrec.o \
	solib.o solib-svr4.o symfile-mem.o

SIM_OBS = remote-sim.o
SIM = ../sim/sh/libsim.a
