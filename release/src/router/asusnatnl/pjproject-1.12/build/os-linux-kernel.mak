
include $(KERNEL_DIR)/.config

#
# Basic kernel compilation flags.
#
export OS_CFLAGS   := $(CC_DEF)PJ_LINUX_KERNEL=1 -D__KERNEL__  \
		      -I$(KERNEL_DIR)/include -iwithprefix include \
		      -nostdinc -msoft-float

#
# Additional kernel compilation flags are taken from the kernel Makefile
# itself.
#

KERNEL_CFLAGS := \
    $(shell cd $(KERNEL_DIR) ; \
    make script SCRIPT='@echo $$(CFLAGS) $$(CFLAGS_MODULE)' $(KERNEL_ARCH))

export OS_CFLAGS += $(KERNEL_CFLAGS)

#		      -DMODULE -I$(KERNEL_DIR)/include  -nostdinc \
#		      -Wstrict-prototypes \
#		      -Wno-trigraphs -fno-strict-aliasing -fno-common \
#		      -msoft-float -m32 -fno-builtin-sprintf -fno-builtin-log2\
#		      -fno-builtin-puts -mpreferred-stack-boundary=2 \
#		      -fno-unit-at-a-time -march=i686 -mregparm=3 \
#		      -iwithprefix include

#export OS_CFLAGS += -U__i386__ -Ui386 -D__arch_um__ -DSUBARCH=\"i386\" \
#		    -D_LARGEFILE64_SOURCE -I$(KERNEL_DIR)/arch/um/include \
#		    -Derrno=kernel_errno \
#		    -I$(KERNEL_DIR)/arch/um/kernel/tt/include \
#		    -I$(KERNEL_DIR)/arch/um/kernel/skas/include \
		    

export OS_CXXFLAGS := 

export OS_LDFLAGS  :=  

export OS_SOURCES  := 


