# Target: Motorola 68HC11 processor
TDEPFILES= m68hc11-tdep.o
SIM_OBS= remote-sim.o
SIM= ../sim/m68hc11/libsim.a -lm

