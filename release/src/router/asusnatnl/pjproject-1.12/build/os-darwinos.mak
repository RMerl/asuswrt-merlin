export OS_CFLAGS   := $(CC_DEF)PJ_DARWINOS=1

export OS_CXXFLAGS := 

export OS_LDFLAGS  := $(CC_LIB)pthread$(LIBEXT2) -framework CoreAudio -lm

export OS_SOURCES  := 


