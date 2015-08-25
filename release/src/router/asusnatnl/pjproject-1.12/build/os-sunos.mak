export OS_CFLAGS   := $(CC_DEF)PJ_SUNOS=1

export OS_CXXFLAGS := 

export OS_LDFLAGS  := $(CC_LIB)pthread$(LIBEXT2) \
		      $(CC_LIB)socket$(LIBEXT2) \
		      $(CC_LIB)rt$(LIBEXT2) \
		      $(CC_LIB)nsl$(LIBEXT2) \
		      $(CC_LIB)m$(LIBEXT2)

export OS_SOURCES  := 


