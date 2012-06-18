
include $(TOPDIR)/rules.mk

PKG_NAME:=xl2tpd
PKG_VERSION:=1.2.6
PKG_RELEASE:=1
PKG_MD5SUM:=ab5656eb5a3d1973f7f69b039675332e-NEEDSUPDATING

PKG_SOURCE_URL:=http://www.xelerance.com/software/xl2tpd/
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_CAT:=zcat

include $(TOPDIR)/package/rules.mk

$(eval $(call PKG_template,XL2TPD,xl2tpd,$(PKG_VERSION)-$(PKG_RELEASE),$(ARCH)))

FLAGS := $(TARGET_CFLAGS) -I$(PKG_BUILD_DIR)/linux/include -L$(STAGING_DIR)/usr/lib -I$(STAGING_DIR)/usr/include

$(PKG_BUILD_DIR)/.built:
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		ARCH="mips" \
		LD_LIBRARY_PATH="$(STAGING_DIR)/usr/lib" \
		EXTRA_INCLUDE="-I$(STAGING_DIR)/usr/include" \
		all

$(IPKG_XL2TPD):
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		DESTDIR="$(IDIR_XL2TPD)" \
		ARCH="mips" \
		install
	-$(STRIP) $(IDIR_XL2TPD)/usr/sbin/xl2tpd
	rm -rf $(IDIR_XL2TPD)/usr/share
	rm -rf $(IDIR_XL2TPD)/usr/man
	rm -rf $(IDIR_XL2TPD)/var
	rm -rf $(IDIR_XL2TPD)/etc/rc.d/rc*.d
	mkdir -p $(IDIR_XL2TPD)/etc/init.d
	mkdir -p $(IDIR_XL2TPD)/etc/ppp
	mkdir -p $(IDIR_XL2TPD)/etc/xl2tpd
	cp $(PKG_BUILD_DIR)/examples/ppp-options.xl2tpd  $(IDIR_XL2TPD)/etc/ppp/options.xl2tpd
	cp $(PKG_BUILD_DIR)/examples/xl2tpd.conf  $(IDIR_XL2TPD)/etc/xl2tpd
	cp $(PKG_BUILD_DIR)/packaging/openwrt  $(IDIR_XL2TPD)/etc/init.d/
	find $(PKG_BUILD_DIR) -name \*.old | xargs rm -rf
	mkdir -p $(PACKAGE_DIR)
	$(IPKG_BUILD) $(IDIR_XL2TPD) $(PACKAGE_DIR)
