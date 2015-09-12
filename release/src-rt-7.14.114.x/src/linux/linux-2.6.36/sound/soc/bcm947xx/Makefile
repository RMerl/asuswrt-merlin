#
# ALSA SoC Makefile for the Broadcom BCM947XX family of SOCs
#
# Copyright (C) 1999-2015, Broadcom Corporation
# 
#      Unless you and Broadcom execute a separate written software license
# agreement governing use of this software, this software is licensed to you
# under the terms of the GNU General Public License version 2 (the "GPL"),
# available at http://www.broadcom.com/licenses/GPLv2.php, with the
# following added to such license:
# 
#      As a special exception, the copyright holders of this software give you
# permission to link this software with independent modules, and to copy and
# distribute the resulting executable under terms of your choice, provided that
# you also meet, for each linked independent module, the terms and conditions of
# the license of that module.  An independent module is a module which is not
# derived from this software.  The special exception does not apply to any
# modifications of the software.
# 
#      Notwithstanding the above, under no circumstances may you combine this
# software in any way with any other Broadcom software provided under a license
# other than the GPL, without Broadcom's express prior written consent.
#
# $Id: Makefile,v 1.2 2010-05-13 23:47:08 $
#

# BCM947xx Platform Support
snd-soc-bcm947xx-i2s-objs := bcm947xx-i2s.o
snd-soc-bcm947xx-pcm-objs := bcm947xx-pcm.o

obj-$(CONFIG_SND_BCM947XX_SOC) += snd-soc-bcm947xx-i2s.o
obj-$(CONFIG_SND_BCM947XX_SOC) += snd-soc-bcm947xx-pcm.o

# BCM947xx Machine Support
snd-soc-bcm947xx-objs := bcm947xx.o

obj-$(CONFIG_SND_BCM947XX_SOC) += snd-soc-bcm947xx.o
