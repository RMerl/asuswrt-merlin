ifeq ($(RTCONFIG_NVRAM_64K), y)
ifeq ($(RTCONFIG_BCMARM),y)
export EXTRACFLAGS := -DBCMARM -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DBCMWPA2 -fno-strict-aliasing -marm -DRTCONFIG_NVRAM_64K
else
export EXTRACFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DBCMWPA2 -funit-at-a-time -Wno-pointer-sign -mtune=mips32 -mips32 -DRTCONFIG_NVRAM_64K 
endif
else
export EXTRACFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DBCMWPA2 -funit-at-a-time -Wno-pointer-sign -mtune=mips32 -mips32
endif
