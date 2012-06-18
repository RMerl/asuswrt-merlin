# 
# Copyright (C) 2006-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=gdb
PKG_VERSION:=6.8

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_MD5SUM:=c9da266b884fb8fa54df786dfaadbc7a
PKG_SOURCE_URL:=http://ftp.gnu.org/gnu/gdb
PKG_CAT:=bzcat

STAGING_DIR_HOST:=$(TOOLCHAIN_DIR)
BUILD_DIR_HOST:=$(BUILD_DIR_TOOLCHAIN)

include $(INCLUDE_DIR)/host-build.mk

define Build/Configure
	(cd $(PKG_BUILD_DIR); \
		CFLAGS="-O2" \
		gdb_cv_func_sigsetjmp=yes \
		$(PKG_BUILD_DIR)/configure \
		--prefix=$(TOOLCHAIN_DIR) \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_HOST_NAME) \
		--target=$(REAL_GNU_TARGET_NAME) \
		--disable-werror \
		$(DISABLE_NLS) \
		--without-uiout \
		--disable-tui --disable-gdbtk --without-x \
		--without-included-gettext \
		--enable-threads \
	);
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)
	strip $(PKG_BUILD_DIR)/gdb/gdb
endef

define Build/Install
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gdb/gdb $(TOOLCHAIN_DIR)/bin/$(TARGET_CROSS)gdb
	ln -fs $(TARGET_CROSS)gdb $(TOOLCHAIN_DIR)/bin/$(GNU_TARGET_NAME)-gdb
endef

define Build/Clean
	rm -rf $(PKG_BUILD_DIR)
	rm -f $(TOOLCHAIN_DIR)/bin/$(TARGET_CROSS)gdb
	rm -f $(TOOLCHAIN_DIR)/bin/$(GNU_TARGET_NAME)-gdb
endef

$(eval $(call HostBuild))
