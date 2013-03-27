# Target: Embedded Renesas Super-H with ICE and simulator
TDEPFILES= sh-tdep.o sh64-tdep.o monitor.o dsrec.o 

SIM_OBS = remote-sim.o
SIM = ../sim/sh/libsim.a
