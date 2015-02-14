ALLOBJS += armv7_arena.o l2cache.o

# arm architecture
CFLAGS += -DCONFIG_ARM -marm -mabi=aapcs-linux -mno-thumb-interwork

# thumb will hang at ddr_init
CFLAGS += -fno-common -ffixed-r8 -msoft-float -march=armv6k
CFLAGS += -D__ARM_ARCH_7A__
