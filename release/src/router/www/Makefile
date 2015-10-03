include ../common.mak

ASUSWRTTOOLSDIR = $(SRCBASE)/router/tools/Lnx_AsusWrtDictPrep
ASUSWRTTOOLSDIR2 = $(SRCBASE)/router/tools/Lnx_ToolHelp

MODELS_HAVE_USB3 = "DSL-AC68U RT-AC3200 RT-AC87U RT-AC69U RT-AC68U RT-AC56S RT-AC56U RT-AC55U RT-AC55UHP RT-N18U RT-N65U RT-AC88U RT-AC3100 RT-AC5300"

all:

clean:

install:
	rm -rf $(INSTALLDIR)/www

	# INSTALLDIR is $(INSTALLDIR)/www here
	mkdir -p $(INSTALLDIR)
	cp ../www $(INSTALLDIR)/. -rf
	# INSTALL MODEL DEPENDENT files
	cp ../www/sysdep/$(BUILD_NAME)/www $(INSTALLDIR)/. -rfL || true
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
ifeq ($(RTCONFIG_8M_SFP),y)
	rm -rf $(INSTALLDIR)/www/images/cloudsync/invite_model.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/network_config.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/TimeLimits.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/QoS.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/parental-control.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/rt.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/re.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/ap.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/mb.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/login_bg.png
	rm -rf $(INSTALLDIR)/www/images/backtohome.png
	rm -rf $(INSTALLDIR)/www/images/backtohomeclick.png
	rm -rf $(INSTALLDIR)/www/images/wifi_hw_sw.png
	rm -rf $(INSTALLDIR)/www/images/qis/security_key.png
	rm -rf $(INSTALLDIR)/www/images/qis/select_wireless.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/client-listover.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/vender-listover.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/Bandwidth_Limiter.png
	cd $(INSTALLDIR)/www/images/New_ui/networkmap && ln -sf client-list.png client-listover.png && ln -sf vender-list.png vender-listover.png && cd $(TOP)/www
endif
ifeq ($(RTCONFIG_4M_SFP),y)
	rm -rf $(INSTALLDIR)/www/images/cloudsync/invite_model.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/bg.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/client-list.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/client-listover.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/vender-list.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/vender-listover.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/client.png
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
#	rm -rf $(INSTALLDIR)/www/mediaserver.asp
	rm -rf $(INSTALLDIR)/www/ajax_dms_status.asp
	rm -rf $(INSTALLDIR)/www/Advanced_PerformanceTuning_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_IPv6_Content.asp
	rm -rf $(INSTALLDIR)/www/Advacned_SwitchCtrl_Content.asp
	rm -rf $(INSTALLDIR)/www/ure*
	rm -rf $(INSTALLDIR)/www/disk*
	rm -rf $(INSTALLDIR)/www/fan.svg
#	rm -rf $(INSTALLDIR)/www/detect_firmware.asp
	rm -rf $(INSTALLDIR)/www/ajax_coretmp.asp
	rm -rf $(INSTALLDIR)/www/qis/QIS_internet_ruisp.htm
	rm -rf $(INSTALLDIR)/www/qis/ruisp.js
	rm -rf $(INSTALLDIR)/www/images/New_ui/network_config.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/TimeLimits.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/QoS.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/parental-control.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/rt.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/re.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/ap.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/mb.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/login_bg.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/Bandwidth_Limiter.png
endif
ifneq ($(RTCONFIG_PROXYSTA),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/mb.jpg
endif
	rm -rf $(INSTALLDIR)/www/MobileQIS_Login.asp
	rm -rf $(INSTALLDIR)/www/AjaxLogin.asp
	rm -rf $(INSTALLDIR)/www/Passpoint_hidden.asp
	rm -rf $(INSTALLDIR)/www/Advanced_WAdvPassPoint_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_WPasspoint_Content.asp
	rm -rf $(INSTALLDIR)/www/SMTP_Client.asp
	rm -rf $(INSTALLDIR)/www/images/tmo_modem_plug.png
	rm -rf $(INSTALLDIR)/www/images/tmo_modem_unplug.png
	rm -rf $(INSTALLDIR)/www/iui/cellspot_logo.png
	rm -rf $(INSTALLDIR)/www/iui/default.css
	rm -rf $(INSTALLDIR)/www/iui/registerEvent.js
	rm -rf $(INSTALLDIR)/www/iui/tmo-logo.png
ifneq ($(RTCONFIG_SNMPD),y)
	rm -rf $(INSTALLDIR)/www/Advanced_SNMP_Content.asp
endif
ifeq ($(RTCONFIG_DSL),y)
	rm -rf $(INSTALLDIR)/www/qis/QIS_welcome.htm
	rm -rf $(INSTALLDIR)/www/images/New_ui/rt.jpg
	rm -rf $(INSTALLDIR)/www/images/New_ui/ap.jpg
else
	rm -rf $(INSTALLDIR)/www/images/New_ui/export/line_export_notif.png
	rm -rf $(INSTALLDIR)/www/Main_Spectrum_Content.asp
	rm -rf $(INSTALLDIR)/www/SNR.svg
	rm -rf $(INSTALLDIR)/www/BPC.svg
	rm -rf $(INSTALLDIR)/www/ajax_signals.asp
endif

ifeq ($(BUILD_NAME),DSL-N55U)
ifeq ($(BUILD_NAME),DSL-N55U-B)
	rm -rf $(INSTALLDIR)/www/images/Model_product.png
endif
endif

ifneq ($(BUILD_NAME),DSL-N55U)
ifneq ($(BUILD_NAME),DSL-N55U-B)
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
endif
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
#	rm -rf $(INSTALLDIR)/www/mediaserver.asp
	rm -rf $(INSTALLDIR)/www/ajax_dms_status.asp
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
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/backgroud_move_8P_2.0.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/white_04.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/normal.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/blue.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/red.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/normal.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/USB2.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/USB_2.png
	rm -rf $(INSTALLDIR)/www/disk_msg.js
	rm -rf $(INSTALLDIR)/www/disk_fsck.xml
	rm -rf $(INSTALLDIR)/www/disk_scan.asp
	rm -rf $(INSTALLDIR)/www/diskmon_test.asp
	rm -rf $(INSTALLDIR)/www/disk_functions.js
endif
ifneq ($(MODEM),y)
	-rm -f $(INSTALLDIR)/www/qis/QIS_modem.htm
endif
endif

ifeq ($(BUILD_NAME), RT-N12)
	cd $(INSTALLDIR)/www && find . -name "*.dict"|xargs -n 1 -I @ sed -i "s/RT-N12/ZVDYNMODELVZ/g" @
endif

ifeq ($(RTCONFIG_OPENVPN), y)
	cd $(INSTALLDIR)/www && ln -sf ../etc/openvpn/server1/client.ovpn client1.ovpn && cd $(TOP)/www
	cd $(INSTALLDIR)/www && ln -sf ../etc/openvpn/server2/client.ovpn client2.ovpn && cd $(TOP)/www
else
	rm -rf $(INSTALLDIR)/www/ajax_openvpn_server.asp
ifneq ($(RTCONFIG_ACCEL_PPTPD), y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/vpnstatus.png
	rm -rf $(INSTALLDIR)/www/Advanced_VPN_PPTP.asp
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

ifneq ($(RTCONFIG_BWDPI),y)
	rm -rf $(INSTALLDIR)/www/AdaptiveQoS_Bandwidth_Monitor.asp
	rm -rf $(INSTALLDIR)/www/AdaptiveQoS_WebHistory.asp
	rm -rf $(INSTALLDIR)/www/AiProtection_HomeSecurity.asp
	rm -rf $(INSTALLDIR)/www/AiProtection_Group.asp
	rm -rf $(INSTALLDIR)/www/AiProtection_HomeProtection.asp
	rm -rf $(INSTALLDIR)/www/AiProtection_WebProtector.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/QoS_quick
ifneq ($(RTCONFIG_PARENTALCTRL),y)
	rm -rf $(INSTALLDIR)/www/blocking.asp
endif
	rm -rf $(INSTALLDIR)/www/getTraffic.asp
	rm -rf $(INSTALLDIR)/www/getWebHistory.asp
	rm -rf $(INSTALLDIR)/www/getAppTraffic.asp
	rm -rf $(INSTALLDIR)/www/getWanTraffic.asp
	rm -rf $(INSTALLDIR)/www/tm_eula.htm
	rm -rf $(INSTALLDIR)/www/JP_tm_eula.htm
	rm -rf $(INSTALLDIR)/www/images/New_ui/HomeProtection.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/TimeLimits.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/Web_Apps_Restriction.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/AiProtection.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/Home_Protection_Scenario.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/tm_logo.png	
	rm -rf $(INSTALLDIR)/www/images/New_ui/tm_logo_power.png	
	rm -rf $(INSTALLDIR)/www/images/New_ui/tm_logo_1.png	
	rm -rf $(INSTALLDIR)/www/images/New_ui/sample_35.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/app_icons
ifneq ($(RTCONFIG_ROG),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/indicator.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/speedmeter.png
endif
endif
ifneq ($(RTCONFIG_ADBLOCK),y)
	rm -rf $(INSTALLDIR)/www/AiProtection_AdBlock.asp
endif

ifeq ($(findstring $(BUILD_NAME),$(MODELS_HAVE_USB3)),)
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/USB3.png
endif

ifneq ($(RTCONFIG_DROPBOXCLIENT),y)
	rm -rf $(INSTALLDIR)/www/images/cloudsync/dropbox.png
	rm -rf $(INSTALLDIR)/www/dropbox_callback.htm
endif

ifneq ($(RTCONFIG_FTPCLIENT),y)
ifneq ($(RTCONFIG_SAMBACLIENT),y)
	rm -rf $(INSTALLDIR)/www/images/cloudsync/ftp_server.png
endif
endif

ifneq ($(RTCONFIG_SNMPD), y)	
	rm -rf $(INSTALLDIR)/www/Advanced_SNMP_Content.asp
endif

ifneq ($(BUILD_NAME),$(filter $(BUILD_NAME), RT-AC3200 RT-AC5300))
	rm -rf $(INSTALLDIR)/www/Advanced_Smart_Connect.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/smart_connect.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/smart_connect_concept.gif
endif

ifneq ($(RTCONFIG_FINDASUS), y)
	rm -rf $(INSTALLDIR)/www/find_device.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/wifi-logo.png
endif

ifneq ($(RTCONFIG_JFFS2USERICON), y)
	rm -rf $(INSTALLDIR)/www/ajax_uploadicon.asp
endif

ifneq ($(RTCONFIG_TR069), y)
	rm -rf $(INSTALLDIR)/www/Advanced_TR069_Content.asp
endif

ifneq ($(RTCONFIG_TOR), y)
	rm -rf $(INSTALLDIR)/www/Advanced_TOR_Content.asp
endif

ifneq ($(RTCONFIG_BWDPI),y)
	rm -rf $(INSTALLDIR)/www/TrafficAnalyzer_Statistic.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/TrafficAnalyzer.gif
	rm -rf $(INSTALLDIR)/www/images/New_ui/cancel.svg
	rm -rf $(INSTALLDIR)/www/chart.js
	rm -rf $(INSTALLDIR)/www/datepicker.css
endif

ifneq ($(RTCONFIG_KEY_GUARD), y)
	rm -rf $(INSTALLDIR)/www/AiProtection_Key_Guard.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/Key-Guard.png
endif

ifeq ($(BUILD_NAME),4G-AC55U)
	rm -rf $(INSTALLDIR)/www/Advanced_Modem_Content.asp
endif


ifneq ($(BUILD_NAME),$(filter $(BUILD_NAME), RT-AC88U RT-AC3100 RT-AC5300 RT-AC3200))
	rm -rf $(INSTALLDIR)/www/QIS_wizard_m.htm
	rm -rf $(INSTALLDIR)/www/css
	rm -rf $(INSTALLDIR)/www/qis_m
	rm -rf $(INSTALLDIR)/www/images/qis/icon_MAC_clone.svg
	rm -rf $(INSTALLDIR)/www/images/qis/loading.gif
	rm -rf $(INSTALLDIR)/www/images/qis/logo.png
	rm -rf $(INSTALLDIR)/www/images/qis/mainimage_img4.png
	rm -rf $(INSTALLDIR)/www/images/qis/pattern3-3_10_A15.png
	rm -rf $(INSTALLDIR)/www/images/qis/pattern3_05_4.png
endif

ifneq ($(RTCONFIG_DISK_MONITOR), y)
	rm -rf $(INSTALLDIR)/www/device-map/disk_utility.asp
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/backgroud_move_8P_2.0.gif
endif

ifneq ($(RTCONFIG_WTFAST), y)
	rm -rf $(INSTALLDIR)/www/Advanced_WTFast_Content.asp
	rm -rf $(INSTALLDIR)/www/js/fire.js
	rm -rf $(INSTALLDIR)/www/images/fire.jpg
	rm -rf $(INSTALLDIR)/www/images/wtfast_bg.jpg
	rm -rf $(INSTALLDIR)/www/images/wtfast_logo.png
endif
