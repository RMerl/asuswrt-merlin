export OS_CFLAGS   := $(CC_DEF)PJ_WIN32=1

export OS_CXXFLAGS := 

export OS_LDFLAGS  := $(CC_LIB)wsock32$(LIBEXT2) \
		      $(CC_LIB)ws2_32$(LIBEXT2)\
		      $(CC_LIB)ole32$(LIBEXT2)\
		      $(CC_LIB)m$(LIBEXT2)

export OS_SOURCES  := 


