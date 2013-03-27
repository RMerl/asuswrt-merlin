include ../common.mak

DM_VERSION=3.1.0.59
HTTP_VERSION=3.1.0.65

ifeq ($(RTCONFIG_BCMARM),y)
	pkg_type=arm
else
	pkg_type=mipsel
endif

all:
	@echo "" >rom/etc/motd
	@echo "" >>rom/etc/motd
	@echo "ASUSWRT `cat ../shared/version`" >>rom/etc/motd

clean:
	rm -f rom/etc/motd

install:
	mkdir -p $(INSTALLDIR)
	( tar --exclude=.svn --exclude=*.bak -cpf - rom ) | ( cd $(INSTALLDIR) && tar -xpf - )

ifeq ($(RTCONFIG_APP_PREINSTALLED),y)
	install -m 0644 optware.asus.$(pkg_type) $(INSTALLDIR)/rom/optware.asus
ifneq ($(RTCONFIG_BCMARM),y)
	install -m 0644 optware.oleg $(INSTALLDIR)/rom

	-install -m 0644 packages/libuclibc++_*_$(pkg_type).ipk $(INSTALLDIR)/rom
endif
	install -m 0644 packages/asus_base_apps_$(pkg_type).tgz $(INSTALLDIR)/rom # Don't need if installing from Internet.
	-install -m 0644 packages/downloadmaster_$(DM_VERSION)_$(pkg_type).ipk $(INSTALLDIR)/rom
	#-install -m 0644 packages/asuslighttpd_$(HTTP_VERSION)_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/openssl_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/zlib_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/libcurl_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/libevent_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/ncurses_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/libxml2_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/libsigc++_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/libpar2_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/pcre_*_$(pkg_type).ipk $(INSTALLDIR)/rom
	-install -m 0644 packages/spawn-fcgi_*_$(pkg_type).ipk $(INSTALLDIR)/rom
endif

	install -m 0755 apps_scripts/asusrouter $(INSTALLDIR)/rom/.asusrouter
	mkdir -p $(INSTALLDIR)/usr/sbin
	install -m 0755 apps_scripts/*.sh $(INSTALLDIR)/usr/sbin
	install -m 0755 webs_scripts/*.sh $(INSTALLDIR)/usr/sbin

	# remove comments, compress whitespace
	sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < rom/etc/services > $(INSTALLDIR)/rom/etc/services
	sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < rom/etc/protocols > $(INSTALLDIR)/rom/etc/protocols
	sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < rom/etc/ethertypes > $(INSTALLDIR)/rom/etc/ethertypes

ifeq ($(RTCONFIG_HTTPS),y)
	sed 's/#.*//g;s/[ \t]\+/ /g;s/^[ \t]*//;s/[ \t]*$$//;/^$$/d' < rom/etc/openssl.cnf > $(INSTALLDIR)/rom/etc/openssl.cnf
else
	rm -f $(INSTALLDIR)/rom/etc/openssl.cnf
endif

ifeq ($(RTCONFIG_DUALWAN),y)
	cp -rf rom/etc/static_routes $(INSTALLDIR)/rom/etc/
endif

ifeq ($(RTCONFIG_RALINK_RT3883),y)
	mkdir -p $(INSTALLDIR)/rom/wlan
	cp -rf wlan/RT3092_PCIe_LNA_2T2R_ALC_V1_2.bin $(INSTALLDIR)/rom/wlan/
	cp -rf wlan/RT3662_AP_2T2R_V0_0.bin $(INSTALLDIR)/rom/wlan/
endif
