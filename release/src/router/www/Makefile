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
	cp ../www/sysdep/$(BUILD_NAME)/www $(INSTALLDIR)/. -rf || true

	rm -rf $(INSTALLDIR)/www/Makefile
	rm -rf $(INSTALLDIR)/www/sysdep
	rm -rf $(INSTALLDIR)/www/gendict.pl
	rm -rf $(INSTALLDIR)/www/dict.tmp
	rm -rf $(INSTALLDIR)/www/encoding_check.sh
	rm -rf $(INSTALLDIR)/www/en_result.txt
	rm -rf $(INSTALLDIR)/www/temp.dict
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

ifeq ($(RTCONFIG_SFP),y)
	rm -rf $(INSTALLDIR)/www/images/PrinterServer
	rm -rf $(INSTALLDIR)/www/images/appinstall
endif
ifeq ($(RTCONFIG_4M_SFP),y)
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
	rm -rf $(INSTALLDIR)/www/images/qis/router_login.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/security_key.png
	rm -rf $(INSTALLDIR)/www/images/qis/select_wireless.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/win7.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/win7_network_icon1.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/win7_network_icon2.jpg
	rm -rf $(INSTALLDIR)/www/images/qis/winxp.jpg
	rm -rf $(INSTALLDIR)/www/Advanced_WANPort_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_PPTPAdvanced_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_PPTP_Content.asp
	rm -rf $(INSTALLDIR)/www/mediaserver.asp
	rm -rf $(INSTALLDIR)/www/Advanced_PerformanceTuning_Content.asp
	rm -rf $(INSTALLDIR)/www/Advanced_IPv6_Content.asp
	rm -rf $(INSTALLDIR)/www/Advacned_SwitchCtrl_Content.asp
	rm -rf $(INSTALLDIR)/www/ure*
	rm -rf $(INSTALLDIR)/www/disk*
	rm -rf $(INSTALLDIR)/www/fan.svg
	rm -rf $(INSTALLDIR)/www/detect_firmware.asp
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
endif
ifneq ($(RTCONFIG_CLOUDSYNC),y)
	rm -rf $(INSTALLDIR)/www/images/cloudsync
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt/*cloud*.png
	rm -rf $(INSTALLDIR)/www/update_cloudstatus.asp
	rm -rf $(INSTALLDIR)/www/get_webdavInfo.asp
	rm -rf $(INSTALLDIR)/www/*cloud*.asp
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
else
ifneq ($(RTCONFIG_USB),y)
	rm -rf $(INSTALLDIR)/www/images/New_ui/USBExt
	rm -rf $(INSTALLDIR)/www/images/New_ui/aidisk
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconPrinter.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconmodem.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/networkmap/map-iconUSBdisk.png
	rm -rf $(INSTALLDIR)/www/images/New_ui/advancesetting
	rm -rf $(INSTALLDIR)/www/aidisk
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
endif
endif
	cd $(INSTALLDIR)/www && ln -sf ../tmp/settings Settings_$(BUILD_NAME).CFG && cd $(TOP)/www 
ifeq ($(RTCONFIG_ODMPID), y)
ifeq ($(BUILD_NAME), RT-AC66U)
	cd $(INSTALLDIR)/www && ln -sf ../tmp/settings Settings_RT-AC66R.CFG && cd $(TOP)/www
endif
ifeq ($(BUILD_NAME), RT-N66U)
	cd $(INSTALLDIR)/www && ln -sf ../tmp/settings Settings_RT-N66R.CFG && cd $(TOP)/www
endif
ifeq ($(BUILD_NAME), RT-N65U)
	cd $(INSTALLDIR)/www && ln -sf ../tmp/settings Settings_RT-N65R.CFG && cd $(TOP)/www
endif
endif
