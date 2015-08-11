# You can create user.mak file in PJ root directory to specify
# additional flags to compiler and linker. For example:
#ifeq ($(PJ_ANDROID),1)
#ANDROID_LDFLAGS=-L$(SYSROOT)/usr/lib -llog
#ANDROID_CFLAGS=-I$(ANDORID_TOOLCHAIN)/sysroot -I$(PJ_DIR)/j_inc 
#else
#ANDROID_LDFLAGS=
#endif
export CFLAGS += -DUSE_TCP_CAND=1 -g
export LDFLAGS += 
