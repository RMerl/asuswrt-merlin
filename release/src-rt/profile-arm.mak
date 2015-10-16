EXTRACFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DBCMWPA2

ifeq ($(RTCONFIG_BCMARM),y)
EXTRACFLAGS += -DBCMARM -fno-strict-aliasing -marm
else
EXTRACFLAGS += -funit-at-a-time -Wno-pointer-sign -mtune=mips32 -mips32
endif

ifeq ($(RTCONFIG_NVRAM_64K), y)
EXTRACFLAGS += -DRTCONFIG_NVRAM_64K
endif

ifeq ($(RTCONFIG_NV128),y)
EXTRACFLAGS += -DCONFIG_NVSIZE_128
endif

export EXTRACFLAGS
