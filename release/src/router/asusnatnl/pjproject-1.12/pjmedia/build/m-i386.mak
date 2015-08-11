
#
# We need to define PA_LITTLE_ENDIAN when compiling PortAudio on Linux i386
#

export M_CFLAGS += $(CC_DEF)PA_LITTLE_ENDIAN
