# You can create user.mak file in PJ root directory to specify
# additional flags to compiler and linker. For example:
ifeq ($(PJ_CC),mipsel-linux-gcc)
export CFLAGS += -DPJ_MIPS
endif
export CFLAGS += -DUSE_TCP_CAND=1 -g
CFLAGS += -DSCTP_DEBUG
export LDFLAGS += 
