# Target: Renesas M32C family
TDEPFILES = m32c-tdep.o prologue-value.o

# There may also be a SID / CGEN simulator for this, but we do have DJ
# Delorie's mini-sim.
SIM_OBS = remote-sim.o
SIM = ../sim/m32c/libsim.a
