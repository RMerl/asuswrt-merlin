
#
# We need to define PA_LITTLE_ENDIAN when compiling PortAudio on Linux x86_64
#

export M_CFLAGS += $(CC_DEF)PA_LITTLE_ENDIAN
