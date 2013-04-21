# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
#include $(INCLUDE_DIR)/target.mk

PKG_NAME:=ccache
PKG_VERSION:=3.1.3

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://samba.org/ftp/ccache/
PKG_MD5SUM:=b952d75e9ca37209d608ea58d84135cd
PKG_CAT:=zcat

include $(INCLUDE_DIR)/host-build.mk

ifneq ($(strip $(shell which ccache >/dev/null && echo found)),found)
  define Build/Compile
	$(MAKE) CC="$(HOSTCC)" -C $(PKG_BUILD_DIR)
  endef

  define Build/Install
	$(MAKE) -C $(PKG_BUILD_DIR) \
		DESTDIR="$(STAGING_DIR_HOST)" \
		install
  endef

  define Build/Clean
	-$(MAKE) -C $(PKG_BUILD_DIR) uninstall
	-$(MAKE) -C $(PKG_BUILD_DIR) clean
	$(call Build/Clean/Default)
  endef
else
  define Build/Prepare
  endef
  define Build/Configure
  endef
  define Build/Compile
  endef
  define Build/Clean
  endef
endif

$(eval $(call HostBuild))
