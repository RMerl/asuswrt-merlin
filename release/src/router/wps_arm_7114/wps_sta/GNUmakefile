#
# Linux router wps for STA Makefile
#
# Copyright (C) 2014, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id:$
#

include $(TOP)/.config

# linux kernel config
include $(LINUXDIR)/.config

WL_MODE = sta
WPS_CONF_FILE = wpsconfig_router_sta

LIB = ../lib/libwps_sta.a

include ../wps_generic.mk
