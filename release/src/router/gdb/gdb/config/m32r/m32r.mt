# Target: Renesas m32r processor
TDEPFILES= m32r-tdep.o monitor.o m32r-rom.o dsrec.o remote-m32r-sdi.o
SIM_OBS = remote-sim.o
SIM = ../sim/m32r/libsim.a
