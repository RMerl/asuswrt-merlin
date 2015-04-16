export SRCBASE := $(shell pwd)
export SRCBASEDIR := $(shell pwd | sed 's/.*release\///g')

include ./platform.mak
include $(SRCBASE)/router/common.mak
include upb.inc
-include .config
-include $(LINUXDIR)/.config
-include $(SRCBASE)/router/.config

ifeq ($(ASUSWRTTARGETMAKDIR),)
include ./target.mak
else
include $(ASUSWRTTARGETMAKDIR)/target.mak
endif

ifeq ($(ASUSWRTVERSIONCONFDIR),)
include ./version.conf
else
include $(ASUSWRTVERSIONCONFDIR)/version.conf
endif

-include $(SRCBASE)/router/extendno.conf

ifeq ($(BUILD_NAME),)
$(error BUILD_NAME is not defined.!!!)
endif

#################################################################################################
# If SRCBASE = /PATH/TO/asuswrt/router/src-ra-3.0, PBPREFIX is not defined,
#    BUILD_NAME=RT-N65U, and firmware version is 3.0.0.4.308 thus
#
# SRCPREFIX:	/PATH/TO/asuswrt/release
# TOPPREFIX:	/PATH/TO/asuswrt/release/src/router
# PBDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308
# PBPREFIX:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release
# PBSRCDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release/src-ra-3.0
# PBTOPDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release/src/router
# PBLINUXDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release/src-ra-3.0/linux/linux-3.x
#################################################################################################
export SRCPREFIX = $(shell pwd | sed 's,^\(.*release/\)src.*,\1,')
export TOPPREFIX = $(shell pwd | sed 's,^\(.*release/\)src.*,\1,')/src/router
ifeq ($(PBDIR),)
export PBDIR	= $(abspath $(SRCBASE)/../../../asuswrt.prebuilt/$(BUILD_NAME).$(KERNEL_VER).$(FS_VER).$(SERIALNO).$(EXTENDNO))
export PBPREFIX = $(abspath $(SRCBASE)/../../../asuswrt.prebuilt/$(BUILD_NAME).$(KERNEL_VER).$(FS_VER).$(SERIALNO).$(EXTENDNO)/release)
else
export PBPREFIX = $(abspath $(PBDIR)/release)
endif
export PBSRCDIR = $(PBPREFIX)/$(SRCBASEDIR)
export PBTOPDIR = $(PBPREFIX)/src/router
export PBLINUXDIR := $(PBPREFIX)/$(SRCBASEDIR)/linux/$(shell basename $(LINUXDIR))

pb-$(RTCONFIG_WEBDAV)		+= lighttpd-1.4.29
pb-$(RTCONFIG_CLOUDSYNC)	+= asuswebstorage
pb-$(RTCONFIG_USB_BECEEM)	+= Beceem_BCMS250
pb-$(CONFIG_USB_BECEEM)		+= Beceem_driver
pb-$(RTCONFIG_CLOUDSYNC)	+= inotify
pb-$(RTCONFIG_SWEBDAVCLIENT)	+= webdav_client
pb-y				+= extendno
pb-y				+= rc
pb-y				+= httpd

$(info SRCPREFIX $(SRCPREFIX))
$(info PBPREFIX $(PBPREFIX))

ALL_FILES = $(shell find $(PBPREFIX) \( -path "$(PBPREFIX)/src/router/Beceem_BCMS250/prebuild/rom" \) -prune -o -type f -print)
SO_FILES = $(filter %.so %.so.l %/libxvi020.so.05.02.93, $(ALL_FILES))
OBJ_FILES = $(filter %.o %.obj, $(ALL_FILES))
KO_FILES = $(filter %.ko, $(ALL_FILES))
TXT_FILES = $(filter %.conf %.txt, $(ALL_FILES))
SRC_FILES = $(filter %.h %.c, $(ALL_FILES))

EXEC_FILES = $(filter-out $(SO_FILES) $(OBJ_FILES) $(KO_FILES) $(TXT_FILES) $(SRC_FILES), $(ALL_FILES))

all: $(pb-y) $(pb-m)
	[ ! -e upb-platform.mak ] || $(MAKE) -f upb-platform.mak
	$(MAKE) -f upb.mak strip

strip:
	@( \
		for f in $(SO_FILES) $(OBJ_FILES) $(KO_FILES) ; do \
			if [ -z "$(V)" ] ; then echo "  [STRIP]  `basename $${f}`" ; \
			else echo "$(STRIP) --strip-debug $${f}" ; fi ;\
			$(STRIP) --strip-debug $${f} ; \
		done ; \
		for f in $(EXEC_FILES) ; do \
			if [ -z "$(V)" ] ; then echo "  [STRIP ALL]  `basename $${f}`" ; \
			else echo "$(STRIP) --strip-all $${f}" ; fi ;\
			$(STRIP) --strip-all $${f} ; \
		done \
	 )

clean:
	-$(RM) -fr $(PBDIR)

############################################################################
# Generic short variable for source/destination directory.
# NOTE: Only one variable can be defined in one target.
############################################################################
$(pb-y) $(pb-m): S=$(TOPPREFIX)/$@
$(pb-y) $(pb-m): D=$(PBTOPDIR)/$@/prebuild


############################################################################
# Override special S or D variable here.
# NOTE: Only one variable can be defined in one target.
############################################################################
lighttpd-1.4.29: S=$(TOPPREFIX)/$@/src/.libs
Beceem_BCMS250: S=$(TOPPREFIX)/$@/src
Beceem_driver: S=$(LINUXDIR)/drivers/usb/$@
Beceem_driver: D=$(PBTOPDIR)/Beceem_BCMS250/prebuild/lib/modules/$(LINUX_KERNEL)$(BCMSUB)/kernel/drivers/usb/$@
extendno: S=$(TOPPREFIX)/
extendno: D=$(PBTOPDIR)/

############################################################################
# Copy binary to prebuilt directory.
############################################################################
asuswebstorage:
	$(call inst,$(S),$(D),$@)

Beceem_BCMS250: C=$(S)/CSCM_v1.1.6.0
Beceem_BCMS250: T=$(TOOLS)
Beceem_BCMS250: RT_VER=$(RTVER)
Beceem_BCMS250:
	$(call _inst,$(S)/bcmWiMaxSwitch/switchmode,$(D)/sbin/switchmode)
	$(call _inst,$(S)/API/bin_linux/bin/libxvi020.so.05.02.93,$(D)/lib/libxvi020.so.05.02.93)
	$(call _ln,libxvi020.so.05.02.93,$(D)/lib/libxvi020.so)
	$(call _inst,$(C)/bin_pc_linux/bin/wimaxc,$(D)/sbin/wimaxc)
	$(call _inst,$(C)/bin_pc_linux/bin/wimaxd,$(D)/sbin/wimaxd)
	$(call _inst,$(C)/bin_pc_linux/bin/libeap_supplicant.so,$(D)/lib/libeap_supplicant.so)
	$(call _inst,$(C)/bin_pc_linux/bin/libengine_beceem.so,$(D)/lib/libengine_beceem.so)
	$(call _inst,$(T)/lib/librt-$(RT_VER).so,$(D)/lib/librt-$(RT_VER).so)
	$(call _ln,librt-$(RT_VER).so,$(D)/lib/librt.so)
	$(call _ln,librt-$(RT_VER).so,$(D)/lib/librt.so.0)
	$(call _ln,/tmp/Beceem_firmware,$(D)/lib/firmware)
	$(call _inst,$(S)/API/bin_linux/bin/RemoteProxy.cfg,$(D)/rom/Beceem_firmware/RemoteProxy.cfg)
	$(call _inst,$(S)/macxvi200.bin.normal,$(D)/rom/Beceem_firmware/macxvi200.bin.normal)
	$(call _inst,$(S)/macxvi200.bin.giraffe,$(D)/rom/Beceem_firmware/macxvi200.bin.giraffe)
	$(call _inst,$(S)/config_files/macxvi.cfg.giraffe,$(D)/rom/Beceem_firmware/macxvi.cfg.giraffe)
	$(call _inst,$(S)/config_files/macxvi.cfg.gmc,$(D)/rom/Beceem_firmware/macxvi.cfg.gmc)
	$(call _inst,$(S)/config_files/macxvi.cfg.yota,$(D)/rom/Beceem_firmware/macxvi.cfg.yota)
	$(call _inst,$(S)/config_files/macxvi.cfg.freshtel,$(D)/rom/Beceem_firmware/macxvi.cfg.freshtel)
	$(call _inst,$(S)/config_files/Server_CA.pem.yota,$(D)/rom/Beceem_firmware/Server_CA.pem.yota)

Beceem_driver:
	$(call inst,$(S),$(D),drxvi314.ko)

inotify:
	$(call inst,$(S),$(D),$@)

lighttpd-1.4.29:
	$(call inst_so,$(S),$(D),mod_smbdav.so)
	$(call inst_so,$(S),$(D),mod_aidisk_access.so)
	$(call inst_so,$(S),$(D),mod_sharelink.so)
	$(call inst_so,$(S),$(D),mod_aicloud_auth.so)

webdav_client: D=$(PBTOPDIR)/$@/prebuilt
webdav_client:
	$(call inst,$(S),$(D),$@)

extendno:
	$(call inst,$(S),$(D),extendno.conf)

rc:
ifeq ($(RTCONFIG_DSL),y)
ifeq ($(RTCONFIG_DSL_TCLINUX),y)
	$(call inst,$(S),$(D),dsl_fb.o)
	$(call inst,$(S),$(D),dsl_diag.o)
endif
endif
ifeq ($(RTCONFIG_HTTPS),y)
	$(call inst,$(S),$(D),pwdec.o)	
endif

httpd:
ifeq ($(RTCONFIG_HTTPS),y)
	$(call inst,$(S),$(D),pwenc.o)	
endif

spectrum:
ifeq ($(RTCONFIG_DSL),y)
	$(call inst,$(S),$(D),$@)
endif

dsl_drv_tool:
ifeq ($(RTCONFIG_DSL),y)
	$(call inst,$(S)/adslate,$(S)/adslate/prebuild,adslate)
	$(call inst,$(S)/auto_det,$(S)/auto_det/prebuild,auto_det)
	$(call inst,$(S)/req_dsl_drv,$(S)/req_dsl_drv/prebuild,req_dsl_drv)
	$(call inst,$(S)/tp_init,$(S)/tp_init/prebuild,tp_init)
endif

.PHONY: all clean strip $(pb-y) $(pb-m)
