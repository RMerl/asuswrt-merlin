# Makefile for OpenWrt
#
# Copyright (C) 2007 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

TOPDIR:=${CURDIR}
LC_ALL:=C
LANG:=C
export TOPDIR LC_ALL LANG 


world:

include $(TOPDIR)/include/host.mk

ifneq ($(OPENWRT_BUILD),1)
  # XXX: these three lines are normally defined by rules.mk
  # but we can't include that file in this context
  empty:=
  space:= $(empty) $(empty)
  _SINGLE=MAKEFLAGS=$(space)

  override OPENWRT_BUILD=1
  export OPENWRT_BUILD
  include $(TOPDIR)/include/debug.mk
  include $(TOPDIR)/include/depends.mk
  include $(TOPDIR)/include/toplevel.mk
else
  include rules.mk
  include $(INCLUDE_DIR)/depends.mk
  include $(INCLUDE_DIR)/subdir.mk
  include tools/Makefile
  include toolchain/Makefile

$(toolchain/stamp-install): $(tools/stamp-install)
#$(target/stamp-compile): $(toolchain/stamp-install) $(tools/stamp-install) $(BUILD_DIR)/.prepared
#$(package/stamp-cleanup): $(target/stamp-compile)
#$(package/stamp-compile): $(target/stamp-compile) $(package/stamp-cleanup)
#$(package/stamp-install): $(package/stamp-compile)
#$(package/stamp-rootfs-prepare): $(package/stamp-install)
#$(target/stamp-install): $(package/stamp-compile) $(package/stamp-install) $(package/stamp-rootfs-prepare)


$(BUILD_DIR)/.prepared: Makefile
	@mkdir -p $$(dirname $@)
	@touch $@

clean: FORCE
	rm -rf $(BUILD_DIR) $(BIN_DIR)

dirclean: clean
	rm -rf $(STAGING_DIR) $(STAGING_DIR_HOST) $(STAGING_DIR_TOOLCHAIN) $(TOOLCHAIN_DIR) $(BUILD_DIR_HOST)
	rm -rf $(TMP_DIR)

# check prerequisites before starting to build
prereq: ;

prepare: .config $(tools/stamp-install) $(toolchain/stamp-install)

# build toolchain
world: prepare FORCE
# Strip binaries
	$(RSTRIP) $(TOOLCHAIN_DIR)/bin
	$(RSTRIP) $(TOOLCHAIN_DIR)/$(ARCH)-linux-uclibc/bin
# Fix includes
ifeq ($(CONFIG_LINUX_2_4),y)
	-rm $(TOOLCHAIN_DIR)/include/sys/inotify.h
endif
	-tar -C toolchain/kernel-headers/extras/ --exclude='.svn' -cf - include | tar -C $(TOOLCHAIN_DIR) -xf -

# Create addtional symlinks
	ln -sf $(ARCH)-linux-gcc		$(TOOLCHAIN_DIR)/bin/$(ARCH)-linux-cc
	ln -sf $(ARCH)-linux-uclibc-g\+\+	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-g\+\+
	ln -sf $(ARCH)-linux-uclibc-ar		$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-ar
	ln -sf $(ARCH)-linux-uclibc-gcc		$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-gcc
	ln -sf $(ARCH)-linux-uclibc-gcc-$(GCCV)	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-gcc-$(GCCV)
	ln -sf $(ARCH)-linux-uclibc-ld		$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-ld
	ln -sf $(ARCH)-linux-uclibc-nm		$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-nm
	ln -sf $(ARCH)-linux-uclibc-objcopy	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-objcopy
	ln -sf $(ARCH)-linux-uclibc-objdump	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-objdump
	ln -sf $(ARCH)-linux-uclibc-ranlib	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-ranlib
	ln -sf $(ARCH)-linux-uclibc-size	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-size
	ln -sf $(ARCH)-linux-uclibc-strings	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-strings
	ln -sf $(ARCH)-linux-uclibc-strip	$(TOOLCHAIN_DIR)/bin/$(ARCH)-uclibc-strip


#world: prepare $(target/stamp-compile) $(package/stamp-cleanup) $(package/stamp-compile) $(package/stamp-install) $(package/stamp-rootfs-prepare) $(target/stamp-install) FORCE
#	$(SUBMAKE) package/index

# update all feeds, re-create index files, install symlinks
#package/symlinks:
#	$(SCRIPT_DIR)/feeds update -a
#	$(SCRIPT_DIR)/feeds install -a

# re-create index files, install symlinks
#package/symlinks-install:
#	$(SCRIPT_DIR)/feeds update -i
#	$(SCRIPT_DIR)/feeds install -a

# remove all symlinks, don't touch ./feeds
#package/symlinks-clean:
#	$(SCRIPT_DIR)/feeds uninstall -a

.PHONY: clean dirclean prereq prepare world package/symlinks package/symlinks-install package/symlinks-clean

endif
