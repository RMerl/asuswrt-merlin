#
# ALSA SoC Makefile for the Broadcom BCM947XX family of SOCs
#
# Copyright (C) 2009, Broadcom Corporation
# All Rights Reserved.
# 
# THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
# KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
# SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
#
# $Id: Makefile,v 1.1 2009/10/30 20:37:14 Exp $
#

# BCM947xx Platform Support
snd-soc-bcm947xx-i2s-objs := bcm947xx-i2s.o
snd-soc-bcm947xx-pcm-objs := bcm947xx-pcm.o

obj-$(CONFIG_SND_BCM947XX_SOC) += snd-soc-bcm947xx-i2s.o
obj-$(CONFIG_SND_BCM947XX_SOC) += snd-soc-bcm947xx-pcm.o

# BCM947xx Machine Support
# snd-soc-bcm947xx-objs := bcm94717bu.o
snd-soc-bcm947xx-objs := bcm94717ap.o

obj-$(CONFIG_SND_BCM947XX_SOC) += snd-soc-bcm947xx.o
