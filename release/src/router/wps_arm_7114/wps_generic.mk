#
# Broadcom Linux Router WPS generic Makefile
# 
# Copyright (C) 2013, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
# $Id: $
#

# Include wps common make file
include	$(WPSSRC)/common/config/$(WPS_CONF_FILE)
ifneq ($(CONFIG_WFI),y)
WPS_WFI = 0
endif
ifneq ($(CONFIG_LIBUPNP),y)
WPS_UPNP_DEVICE = 0
endif
include $(WPSSRC)/common/config/wps.mk
EXTRA_CFLAGS = ${WPS_INCS} ${WPS_FLAGS} ${WLAN_ComponentIncPath}


WPS_SOURCE := $(WPS_FILES)

vpath %.c $(SRCBASE)/../

WPS_OBJS := $(foreach file, $(WPS_SOURCE), \
	  $(patsubst src/%.c, obj/%.o,$(file)))

WPS_DIRS   := $(foreach file, $(WPS_OBJS), \
	$(dir $(file)))

WPS_DIRLIST = $(sort $(WPS_DIRS)) 


all: dirs $(LIB)

dirs:
	mkdir -p $(WPS_DIRLIST)
	@echo "==> $(WPS_CONF_FILE)"

libobjs: dirs
	+$(MAKE) parallel=true $(WPS_OBJS)

# library rules
$(LIB) : libobjs
	@mkdir -pv $(@D)
	$(AR) cr $@ $(WPS_OBJS)

$(WPS_OBJS) : obj%.o: $(addprefix $(SRCBASE)/../,src%.c)
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

clean:
	rm -rf obj
