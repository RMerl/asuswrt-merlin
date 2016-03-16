#!/bin/sh
# @(#) configures environment to use Broadcom MIPS toolchain

export PATH=$PATH:/opt/brcm/hndtools-mipsel-uclibc/bin
export BRCM_CC=mipsel-uclibc-gcc
export STRIP=mipsel-uclibc-strip

