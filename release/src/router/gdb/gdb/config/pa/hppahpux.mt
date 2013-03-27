# Target: HP PA-RISC running hpux
MT_CFLAGS = -DPA_SOM_ONLY=1
TDEPFILES= hppa-tdep.o hppa-hpux-tdep.o corelow.o somread.o solib-som.o solib-pa64.o solib.o
DEPRECATED_TM_FILE= tm-hppa.h
