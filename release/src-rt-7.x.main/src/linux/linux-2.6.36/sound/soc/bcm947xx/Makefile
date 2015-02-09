#
# ALSA SoC Makefile for the Broadcom BCM947XX family of SOCs
#
# Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
