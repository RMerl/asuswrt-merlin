# Target: Renesas/Super-H 64 bit with simulator
TDEPFILES= sh-tdep.o sh64-tdep.o

SIM_OBS = remote-sim.o
SIM = ../sim/sh64/libsim.a
