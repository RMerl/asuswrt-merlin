# Target: H8300 with HMS monitor and H8 simulator
TDEPFILES= h8300-tdep.o monitor.o dsrec.o

SIM_OBS = remote-sim.o
SIM = ../sim/h8300/libsim.a
