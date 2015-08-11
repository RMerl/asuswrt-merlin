EXTRACFLAGS := -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DBCMWPA2 -funit-at-a-time -Wno-pointer-sign -mtune=mips32r2 -mips32r2

ifeq ($(RTCONFIG_NVRAM_64K), y)
EXTRACFLAGS += -DRTCONFIG_NVRAM_64K 
endif
ifeq ($(RTCONFIG_DUAL_TRX), y)
EXTRACFLAGS += -DRTCONFIG_DUAL_TRX                    
endif


export EXTRACFLAGS
