export OS_CFLAGS   := $(CC_DEF)PJ_LINUX=1

export OS_CXXFLAGS := 

export OS_LDFLAGS  := -lportaudio-$(TARGET_NAME) -lgsmcodec-$(TARGET_NAME) -lilbccodec-$(TARGET_NAME) -lspeex-$(TARGET_NAME) -lresample-$(TARGET_NAME) $(CC_LIB)pthread$(LIBEXT2) -lm

export OS_SOURCES  := 


