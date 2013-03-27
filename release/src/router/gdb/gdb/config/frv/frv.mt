# Target: Fujitsu FRV processor
TDEPFILES= frv-tdep.o frv-linux-tdep.o solib.o solib-frv.o corelow.o
DEPRECATED_TM_FILE= tm-frv.h
SIM_OBS = remote-sim.o
SIM = ../sim/frv/libsim.a
