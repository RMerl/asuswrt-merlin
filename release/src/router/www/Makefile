include ../common.mak

ASUSWRTTOOLSDIR = $(SRCBASE)/router/tools/Lnx_AsusWrtDictPrep
ASUSWRTTOOLSDIR2 = $(SRCBASE)/router/tools/Lnx_ToolHelp

all:

clean:

install:
	rm -rf $(INSTALLDIR)/www

	# INSTALLDIR is $(INSTALLDIR)/www here
	mkdir -p $(INSTALLDIR)
	cp ../www $(INSTALLDIR)/. -rf
	# INSTALL MODEL DEPENDENT files
	cp ../www/sysdep/$(BUILD_NAME)/www $(INSTALLDIR)/. -rfL || true
ifeq ($(RTN14U),y)   
	### patch file
	./model_patch $(INSTALLDIR)/www ../www/sysdep/$(BUILD_NAME)_patch/www || true
endif
	rm -rf $(INSTALLDIR)/www/Makefile
	rm -rf $(INSTALLDIR)/www/sysdep
	rm -rf $(INSTALLDIR)/www/gendict.pl
	rm -rf $(INSTALLDIR)/www/dict.tmp
	rm -rf $(INSTALLDIR)/www/encoding_check.sh
	rm -rf $(INSTALLDIR)/www/en_result.txt
	rm -rf $(INSTALLDIR)/www/temp.dict
	rm -rf $(INSTALLDIR)/www/model_patch
	rm -rf $(INSTALLDIR)/www/.gitignore
	rm -rf `find $(INSTALLDIR) -name "*.bak"`

	chmod -x `find $(INSTALLDIR)/www/ -name "*.asp"`
	chmod -x `find $(INSTALLDIR)/www/ -name "*.htm"`
	chmod -x `find $(INSTALLDIR)/www/ -name "*.css"`
	chmod -x `find $(INSTALLDIR)/www/ -name "*.js"`
	chmod -x `find $(INSTALLDIR)/www/ -name "*.svg"`
	chmod -x `find $(INSTALLDIR)/www/ -name "*.htc"`
	chmod -x `find $(INSTALLDIR)/www/ -name "*.dict"`

ifeq ($(RTCONFIG_AUTODICT), y)
	# now dict prep will generate necessary dict files to dest dir
	rm -f $(INSTALLDIR)/www/*.dict
	# Tool dir , dictctrl.txt , modem name , header mapping , src , dst , output msg file
	$(ASUSWRTTOOLSDIR)/LnxDictPrep $(ASUSWRTTOOLSDIR) dictctrl.txt $(BUILD_NAME) HdrMapping.txt . $(INSTALLDIR)/www $(SRCBASE)/image/dictenum.txt $(SRCBASE)/image/dict_prep.txt
	# generate sh file for clear up html and dict auto enum
	# this is for compression
	$(ASUSWRTTOOLSDIR2)/genshfile.pl $(ASUSWRTTOOLSDIR2) $(INSTALLDIR)/www $(SRCBASE)/image/compress.sh $(SRCBASE)/image/dictenum.txt $(SRCBASE)/image/output2.txt $(BUILD_NAME) $(SRCBASE)/image/output3.txt
	chmod 777 $(SRCBASE)/image/compress.sh
	sh $(SRCBASE)/image/compress.sh
else
	for F in $(wildcard *.dict); do \
		./gendict.pl $(BUILD_NAME) $$F $(INSTALLDIR)/www/$$F; \
	done
endif
	echo " WWW FOR $(BUILD_NAME)"

	rm -rf $(INSTALLDIR)/www/images/New_ui/asustitle_ie6.png
#ifneq ($(RTCONFIG_FANCTRL),y)
#	rm -rf $(INSTALLDIR)/www/Advanced_PerformanceTuning_Content.asp
#	rm -rf $(INSTALLDIR)/www/fan.svg
#endif
ifeq ($(RTCONFIG_SFP),y)
	rm -rf $(INSTALLDIR)/www/images/PrinterServer
	rm -rf $(INSTALLDIR)/www/images/appinstall
endif
ifeq ($(RTCONFIG_4M_SFP),y)
	rm -rf $(INSTALLDIR)/www/Main_ConnStatus_Content.asp
	rm -rf $(INSTALLDIR)/www/svghtc
	rm -rf $(INSTALLDIR)/www/svg.js
	rm -rf $(INSTALLDIR)/www/Main_TrafficMonitor_daily.asp
	rm -rf $(INSTALLDIR)/www/Main_TrafficMonitor_last24.asp
	rm -rf $(INSTALLDIR)/www/Main_TrafficMonitor_realtime.asp
	rm -rf $(INSTALLDIR)/www/tm.svg
	rm -rf $(INSTALLDIR)/www/tmcal.js
	rm -rf $(INSTALLDIR)/www/tmhist.js
	rm -rf $(INSTALLDIR)/www/tmmenu.css
	rm -rf $(INSTALLDIR)/www/tmmenu.js
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconClient-d.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconInternet-d.png
	rm -rf $(INSTALLDIR)/www/images/qis/android*
	rm -rf $(INSTALLDIR)/www/images/qis/iOS*
	rm -rf $(INSTALLDIR)/www/images/qis/mac.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/osx_network_icon.jpg
#	rm -rf $(INSTALLDIR)/www/images/qis/router_login.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/security_key.png
	rm -rf $(INSTALLDIR)/www/images/qis/select_wireless.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/win7.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/win7_network_icon1.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/win7_network_icon2.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/winxp.jpg
#	rm -rf $(INSTALLDIR)/www/Advanced_WANPort_Content.asp
	rm -rf $(INSTALLDIR)/www/mediaserver.asp
	rm -rf $(INSTALLDIR)/www/Advanced_PerformanceTuning_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_IPv6_Content.asp
	rm -rf $(INSTALLDIR)/www/Advacned_SwitchCtrl_Content.asp
	rm -rf $(INSTALLDIR)/www/ure*
	rm -rf $(INSTALLDIR)/www/disk*
	rm -rf $(INSTALLDIR)/www/fan.svg
#	rm -rf $(INSTALLDIR)/www/detect_firmware.asp
	rm -rf $(INSTALLDIR)/www/ajax_coretmp.asp
	rm -rf $(INSTALLDIR)/www/qis/QIS_tutorial.htm
	rm -rf $(INSTALLDIR)/www/qis/QIS_internet_ruisp.htm
	rm -rf $(INSTALLDIR)/www/qis/ruisp.js
	rm -rf $(INSTALLDIR)/www/images/New_ui/network_config.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/parental-control.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/QoS.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/rt.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/re.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/ap.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/mb.jpg
endif
ifneq ($(RTCONFIG_PROXYSTA),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/mb.jpg
endif
ifeq ($(RTCONFIG_DSL),y)
	rm -rf $(INSTALLDIR)/www/images/Model_producticon.png
	rm -rf $(INSTALLDIR)/www/qis/QIS_welcome.htm
endif
ifneq ($(BUILD_NAME),DSL-N55U)
ifneq ($(BUILD_NAME),DSL-N55U-B)
	rm -rf $(INSTALLDIR)/www/Main_Spectrum_Content.asp
	rm -rf $(INSTALLDIR)/www/SNR.svg
	rm -rf $(INSTALLDIR)/www/BPC.svg
	rm -rf $(INSTALLDIR)/www/ajax_signals.asp
ifneq ($(BUILD_NAME),RT-N56U)
ifneq ($(RTCONFIG_CLOUDSYNC),y)
	rm -rf $(INSTALLDIR)/www/images/cloudsync
	rm -rf $(INSTALLDIR)/www/images/aicloud_logo.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt/*cloud*.png
	rm -rf $(INSTALLDIR)/www/update_cloudstatus.asp
	rm -rf $(INSTALLDIR)/www/get_webdavInfo.asp
	rm -rf $(INSTALLDIR)/www/*cloud*.asp
endif
endif
else
	rm -rf $(INSTALLDIR)/www/images/New_ui/rt.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/ap.jpg
endif
else
	rm -rf $(INSTALLDIR)/www/images/New_ui/rt.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/ap.jpg
endif

ifneq ($(RTCONFIG_IPV6),y)
	rm -rf $(INSTALLDIR)/www/Main_IPV6Status_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_IPv6_Content.asp
	rm -rf $(INSTALLDIR)/www/update_IPv6state.asp
	rm -rf $(INSTALLDIR)/www/Advanced_Firewall_IPv6_Content.asp
endif

ifeq ($(RTCONFIG_NO_USBSTORAGE),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt
	rm -rf $(INSTALLDIR)/www/images/New_ui/aidisk
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconPrinter.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconmodem.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconUSBdisk.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/advancesetting
	rm -rf $(INSTALLDIR)/www/aidisk
	rm -rf $(INSTALLDIR)/www/getfoldertree.asp
	rm -rf $(INSTALLDIR)/www/gettree.asp
	rm -rf $(INSTALLDIR)/www/images/Tree
	# disk utility
	rm -rf $(INSTALLDIR)/www/device-map/disk_utility.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/backgroud_move_8P_2.0.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/white_04.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/normal.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/blue.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/red.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/USB_2.png
else
ifneq ($(RTCONFIG_USB),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt
	rm -rf $(INSTALLDIR)/www/images/New_ui/aidisk
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconPrinter.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconmodem.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconUSBdisk.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/advancesetting
	rm -rf $(INSTALLDIR)/www/images/WANunplug_usb.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/usbstatus.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/printstatus.png
	rm -rf $(INSTALLDIR)/www/aidisk
	rm -rf $(INSTALLDIR)/www/mediaserver.asp
	rm -rf $(INSTALLDIR)/www/Advanced_AiDisk_ftp.asp
	rm -rf $(INSTALLDIR)/www/Advanced_AiDisk_samba.asp
	rm -rf $(INSTALLDIR)/www/Advanced_AiDisk_others.asp
	rm -rf $(INSTALLDIR)/www/Advanced_AiDisk_webdav.asp
	rm -rf $(INSTALLDIR)/www/Advanced_APPList_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_Modem_Content.asp
	rm -rf $(INSTALLDIR)/www/aidisk.asp
	rm -rf $(INSTALLDIR)/www/PrinterServer.asp
	rm -rf $(INSTALLDIR)/www/APP_Installation.asp
	rm -rf $(INSTALLDIR)/www/app_installation.css
	rm -rf $(INSTALLDIR)/www/cdma2000_list.js
	rm -rf $(INSTALLDIR)/www/wcdma_list.js
	rm -rf $(INSTALLDIR)/www/wimax_list.js
	rm -rf $(INSTALLDIR)/www/td-scdma_list.js
	rm -rf $(INSTALLDIR)/www/device-map/printer.asp
	rm -rf $(INSTALLDIR)/www/device-map/modem.asp
	rm -rf $(INSTALLDIR)/www/device-map/disk.asp
	rm -rf $(INSTALLDIR)/www/getfoldertree.asp
	rm -rf $(INSTALLDIR)/www/gettree.asp
	rm -rf $(INSTALLDIR)/www/update_appstate.asp
	rm -rf $(INSTALLDIR)/www/images/Tree
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/devicebt_default.png
	rm -rf $(INSTALLDIR)/www/switch_AiDisk_app.asp
	rm -rf $(INSTALLDIR)/www/update_diskinfo.asp
	rm -rf $(INSTALLDIR)/www/device-map/safely_remove_disk.asp
	rm -rf $(INSTALLDIR)/www/device-map/disk.asp
	# disk utility
	rm -rf $(INSTALLDIR)/www/device-map/disk_utility.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/backgroud_move_3_02.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/white_04.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/yellow_04.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/red_04.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/normal.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/orange.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/red.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/USB_2.png
	rm -rf $(INSTALLDIR)/www/disk_msg.js
	rm -rf $(INSTALLDIR)/www/disk_fsck.xml
	rm -rf $(INSTALLDIR)/www/disk_scan.asp
	rm -rf $(INSTALLDIR)/www/diskmon_test.asp
	rm -rf $(INSTALLDIR)/www/disk_functions.js
endif
ifneq ($(MODEM),y)
	-rm -f $(INSTALLDIR)/www/images/modem_unplug.png
	-rm -f $(INSTALLDIR)/www/images/modem_plug.png
	-rm -f $(INSTALLDIR)/www/qis/QIS_modem.htm
	-rm -f $(INSTALLDIR)/www/qis/QIS_resetModem2.htm
	-rm -f $(INSTALLDIR)/www/qis/QIS_resetModem.htm
endif
endif

ifeq ($(BUILD_NAME), RT-N12)
	cd $(INSTALLDIR)/www && find . -name "*.dict"|xargs -n 1 -I @ sed -i "s/RT-N12/ZVDYNMODELVZ/g" @
endif

ifeq ($(RTCONFIG_OPENVPN), y)
	cd $(INSTALLDIR)/www && ln -sf ../etc/openvpn/server1/client.ovpn client1.ovpn && cd $(TOP)/www
	cd $(INSTALLDIR)/www && ln -sf ../etc/openvpn/server2/client.ovpn client2.ovpn && cd $(TOP)/www
else
ifneq ($(RTCONFIG_ACCEL_PPTPD), y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/vpnstatus.png
	rm -rf $(INSTALLDIR)/www/ajax_openvpn_server.asp
	rm -rf $(INSTALLDIR)/www/Advanced_VPNAdvanced_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_VPN_Content.asp
endif
endif
ifneq ($(RTCONFIG_VPNC), y)	
	rm -rf $(INSTALLDIR)/www/Advanced_VPNClient_Content.asp
endif	
ifneq ($(RTCONFIG_TIMEMACHINE),y)
	rm -rf $(INSTALLDIR)/www/Advanced_TimeMachine.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt/time_machine_banner.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt/TimeMachine.png
endif
ifneq ($(RTCONFIG_PROXYSTA),y)
ifneq ($(RTCONFIG_WIRELESSREPEATER),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/re.jpg
	rm -rf $(INSTALLDIR)/www/qis/QIS_sitesurvey.htm
endif
endif

ifneq ($(RTCONFIG_YANDEXDNS),y)
	rm -rf $(INSTALLDIR)/www/YandexDNS.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/yandex*.png
endif
ifneq ($(RTCONFIG_KYIVSTAR),y)
	rm -rf $(INSTALLDIR)/www/userRpm/SoftwareUpgradeRpm.htm
endif

ifeq ($(RTCONFIG_ODMPID),y)
	@if [ ! -e $(INSTALLDIR)/www/images/$(subst U,R,$(BUILD_NAME))Model_producticon.png ] ; then \
		 ln -sf Model_producticon.png $(INSTALLDIR)/www/images/$(subst U,R,$(BUILD_NAME))Model_producticon.png ; \
	fi
endif
