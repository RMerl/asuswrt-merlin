#
# GNUmakefile for wl/exe
#
# Copyright (c) 2000, Broadcom Corp.
# $Id: GNUmakefile 536165 2015-02-20 21:06:45Z $

WLAN_ComponentsInUse := bcmwifi ppr
include ../../makefiles/WLAN_Common.mk

ifndef	SRCBASE
    SRCBASE := $(WLAN_SrcBaseR)
endif

UNAME = $(shell uname)

ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)

#-----------------------------------------------------------------
# Windows build, don't include Makerules due to all: conflict

include ../../GNUmakefile.inc

# MfgTest XP and WIN7 WLU DLL builds are built from other standalone Makefiles
# from App and MfgTest teams
# To build XP WLU DLL (output is in obj/*/mfg_dll/{free,checked}/...)
#    $(MAKE) -C src/wl/exe -f GNUmakefile.wlu_dll
# To build Win7/Vista WLU DLL (output is in obj/*/mfg_dll/{Release,Debug}/...)
#    $(MAKE) -C src/wl/exe/win7

else # UNAME

#-----------------------------------------------------------------
# Linux build
#

# This should be one of values recognized in src/Makerules
# 2) not windows, need to include first to pick up TARGETENV dependent vars
include $(SRCBASE)/Makerules

# GNU make function to do compilation and dependency generation
# in one step.
define CompileAndMakedep
$(strip $1 -c -MMD -MF $(@D)/.$(@F).depend -MP $2)
endef


# Use newer 11ac ratespec for wl command line
CFLAGS += -DD11AC_IOTYPES

# Use opaque PPR structures
CFLAGS += -DPPR_API

#ifdef SR_DEBUG
CFLAGS += -DSR_DEBUG
#endif

#ifdef WLCNT
  CFLAGS += -DWLCNT
#endif

#ifdef WIFI_ACT_FRAME
  CFLAGS += -DWIFI_ACT_FRAME
#endif

#ifdef WLEXTLOG
  CFLAGS += -DWLEXTLOG
#endif

ifeq ($(WLTEST),1)
    CFLAGS += -DSERDOWNLOAD -DEVENT_LOG_COMPILE
    CFLAGS += -DPCIE_MFGTEST
endif

ifeq ($(OLYMPIC_RWL),1)
    CFLAGS += -DOLYMPIC_RWL
endif

ifeq ($(PCIE_MFGTEST),1)
    CFLAGS += -DPCIE_MFGTEST
endif

ifneq ($(findstring $(TARGETENV), "linux linuxmips linux26mips linuxmips_be linuxarm linuxarm_le android android_ndk_r6b"),)
 

#ifdef WLP2P
  CFLAGS += -DWLP2P
#endif

#ifdef WLMCHAN
  CFLAGS += -DWLMCHAN
#endif

#ifdef WLTDLS
 CFLAGS += -DWLTDLS
#endif

#ifdef WLNDOE
  CFLAGS += -DWLNDOE
#endif

#ifdef WLP2PO
  CFLAGS += -DWLP2PO
#endif

#ifdef WLANQPO
  CFLAGS += -DWLANQPO
#endif

#ifdef TRAFFIC_MGMT
  CFLAGS += -DTRAFFIC_MGMT
#endif

#ifdef WL_PROXDETECT
  CFLAGS += -DWL_PROXDETECT
#endif

# netlink driver interface
ifeq ($(NL80211),1)
  CFLAGS += -DNL80211
endif

#ifdef WLWNM
 CFLAGS += -DWLWNM
#endif


#ifdef WL_MPF
CFLAGS += -DWL_MPF
#endif

# extra warnings
ifneq ($(filter mips_be arm_le arm_android arm_android_ndk_r6b,$(TARGETARCH)),)
CFLAGS += -Wextra -Wall
else
CFLAGS += -Wextra -Wall -Werror
endif
CFLAGS += -DWLPFN -DWLPFN_AUTO_CONNECT -DWLOFFLD
ifeq ($(findstring $(TARGETENV), "freebsd"),)
CFLAGS += -DLINUX 
endif
CFLAGS += -DWLC_HIGH
CFLAGS += -I$(SRCBASE)/wl/sys -I$(SRCBASE)/wl/phy
CFLAGS += $(WLAN_ComponentIncPathR)

# Discard any "MMX" or other qualifications on x86 so that
# any TARGETARCH containing x86 is just "x86"
ifeq ($(findstring x86_mmx,$(TARGETARCH)),x86_mmx)
	TARGETARCH = x86
endif

ASD        ?= 0 
ifneq ($(ASD),1)
  override ASD:=
endif
ifneq ($(ASD),1)
  NOASDDIR    = _noasd
endif

RWL        ?= 1 
ifeq ($(RWL),0)
  override RWL:=
endif

RWL_DONGLE        ?= 1 
RWL_SOCKET        ?= 1 
RWL_WIFI          ?= 1
RWL_SERIAL        ?= 1

ifneq ($(RWL_DONGLE),0)
RWL_SERVERS += $(SERVER_DONGLE)
endif
ifneq ($(RWL_SOCKET),0)
RWL_SERVERS += $(SERVER_SOCKET)
endif
ifneq ($(RWL_WIFI),0)
RWL_SERVERS += $(SERVER_WIFI)
endif
ifneq ($(RWL_SERIAL),0)
RWL_SERVERS += $(SERVER_SERIAL)
endif
# $(TARGETARCH) is set based on TARGETENV in src/Makerules.* files
ifneq ($(findstring $(TARGETENV), "freebsd"),)
WL_OBJS      := wlu.o wlu_common.o wlu_bsd.o wlu_cmd.o wlu_iov.o wlu_rates_matrix.o miniopt.o bcmutils.o bcmwifi_channels.o bcm_app_utils.o wlc_ppr.o
else
WL_OBJS      := wlu.o wlu_common.o wlu_linux.o bcmutils.o bcmwifi_channels.o wlu_cmd.o wlu_iov.o wlu_client_shared.o wlu_pipe_linux.o wlu_pipe.o miniopt.o bcm_app_utils.o wlu_rates_matrix.o wlc_ppr.o bcmxtlv.o
endif
SERVER_OBJS  := wlu_server_linux.o wlu_pipe_linux.o wlu_pipe.o wlu_server_shared.o shellproc_linux.o
WLM_OBJS     := $(WL_OBJS) wlm.o
WLHIGHSTUB_OBJS := wlhighstub.o bcm_rpc_char.o bcm_rpc.o linux_rpc_osl.o
WLHIGHSTUB_OBJS += wlc_low_stubs.o bcm_xdr.o

# include build settings for nl80211 support
ifeq ($(NL80211),1)
include netlink.inc
endif

# Prefix obj/<type>/TARGETARCH to produced .obj files
WL_OBJS      := $(WL_OBJS:%.o=obj/wl$(NOASDDIR)/$(TARGETARCH)/%.o)
DONGLE_OBJS  := $(SERVER_OBJS:%.o=obj/dongle$(NOASDDIR)/$(TARGETARCH)/%.o)
SOCKET_OBJS  := $(SERVER_OBJS:%.o=obj/socket$(NOASDDIR)/$(TARGETARCH)/%.o)
WIFI_OBJS    := $(SERVER_OBJS:%.o=obj/wifi$(NOASDDIR)/$(TARGETARCH)/%.o)
SERIAL_OBJS  := $(SERVER_OBJS:%.o=obj/serial$(NOASDDIR)/$(TARGETARCH)/%.o)
WLM_OBJS     := $(WLM_OBJS:%.o=obj/wlm/$(TARGETARCH)/%.o)
WLHIGHSTUB_OBJS  := $(WLHIGHSTUB_OBJS:%.o=obj/wlhighstub/$(TARGETARCH)/%.o)

# Derive a final list of all known objects.
OBJS         := $(WL_OBJS) $(DONGLE_OBJS) $(SOCKET_OBJS) $(WIFI_OBJS) $(SERIAL_OBJS) $(WLM_OBJS) $(WLHIGHSTUB_OBJS)

# Make a list of dependency files, one per object file.
DEPENDS      := $(join $(dir $(OBJS)),$(addprefix .,$(addsuffix .depend,$(notdir $(OBJS)))))

# Include any dependency files which currently exist.
-include $(DEPENDS)

# TODO: Move final built objects to respective TARGETARCH dirs as well
# Final exe names
ifneq ($(TARGETARCH),x86)
	ARCH_SFX   = $(TARGETARCH)
endif
WL_EXE       := wl$(ARCH_SFX)
SERVER_SOCKET = socket$(NOASDDIR)/$(TARGETARCH)/wl_server_socket$(ARCH_SFX)
SERVER_DONGLE = dongle$(NOASDDIR)/$(TARGETARCH)/wl_server_dongle$(ARCH_SFX)
SERVER_WIFI   = wifi$(NOASDDIR)/$(TARGETARCH)/wl_server_wifi$(ARCH_SFX)
SERVER_SERIAL = serial$(NOASDDIR)/$(TARGETARCH)/wl_server_serial$(ARCH_SFX)
WLM_SO       := wlm/$(TARGETARCH)/wlm$(ARCH_SFX).so
WLHIGHSTUB   := obj/wlhighstub/$(TARGETARCH)/wlhighstub$(ARCH_SFX)

# ASD specific flags
ifeq ($(ASD),1)
  ASD_CFLAGS := -DRWLASD
  ASD_CFLAGS += -I../../tools/Wifi_ASD_test/inc
ifneq ($(filter mips_be arm_android arm_android_ndk_r6b x86_android_ndk_r6b,$(TARGETARCH)),)
  ASD_LDFLAGS:= -L../../tools/Wifi_ASD_test/lib/$(TARGETARCH) -lwfa
else
  ASD_LDFLAGS:= -L../../tools/Wifi_ASD_test/lib/$(TARGETARCH) -lwfa -lpthread
endif
endif # ASD

INSTALL_DIR ?= apps

vpath %.c $(SRCBASE)/shared $(SRCBASE)/wl/sys $(SRCBASE)/wl/phy $(WLAN_StdSrcDirsR) $(WLAN_ComponentSrcDirsR)

all:  build_deps

# Build servers for all but mips_be
ifeq ($(filter mips_be,$(TARGETARCH)),)
all:  $(WL_EXE) servers
else
all: $(WL_EXE)
endif

build_deps:
ifeq ($(ASD),1)
	$(MAKE) -C ../../tools/Wifi_ASD_test
endif # ASD

wlm: $(WLM_SO)

servers: $(RWL_SERVERS)

# Handle creation of directories for objects mentioned below.
$(addprefix obj/,$(addsuffix $(NOASDDIR)/$(TARGETARCH),wl dongle socket wifi serial) $(addsuffix /$(TARGETARCH),wlm wlhighstub)):
	@mkdir -pv $@

# Compilation targets
obj/wl$(NOASDDIR)/$(TARGETARCH)/%.o: %.c | obj/wl$(NOASDDIR)/$(TARGETARCH)
ifneq ($(RWL),)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -DRWL_SOCKET -DRWL_DONGLE -DRWL_WIFI -DRWL_SERIAL -o $@ $<)
else
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -o $@ $<)
endif

obj/dongle$(NOASDDIR)/$(TARGETARCH)/%.o: %.c | obj/dongle$(NOASDDIR)/$(TARGETARCH)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -DRWL_DONGLE -o $@ $<)

obj/socket$(NOASDDIR)/$(TARGETARCH)/%.o: %.c | obj/socket$(NOASDDIR)/$(TARGETARCH)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -DRWL_SOCKET -o $@ $<)

obj/wifi$(NOASDDIR)/$(TARGETARCH)/%.o: %.c | obj/wifi$(NOASDDIR)/$(TARGETARCH)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -DRWL_WIFI -o $@ $<)

obj/serial$(NOASDDIR)/$(TARGETARCH)/%.o: %.c | obj/serial$(NOASDDIR)/$(TARGETARCH)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -DRWL_SERIAL -o $@ $<)

obj/wlm/$(TARGETARCH)/%.o: %.c | obj/wlm/$(TARGETARCH)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) $(ASD_CFLAGS) -DRWL_DONGLE -DRWL_SOCKET -DRWL_WIFI -DRWL_SERIAL -DWLMSO -fPIC -o $@ $<)

obj/wlhighstub/$(TARGETARCH)/%.o: %.c | obj/wlhighstub/$(TARGETARCH)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) -o $@ $<)

$(WLHIGHSTUB): $(WLHIGHSTUB_OBJS)
	@mkdir -pv $(@D)

# Final link targets
$(WL_EXE): $(WL_OBJS)
	$(strip $(CC) $(LDFLAGS) -o $@ $^ $(LIBS))

# net_priv target is for building the dhd with NET_ADMIN privilege (i.e. to avoid running as root)
# note: The 'setcap' command is availble from the libcap2-bin package
# note: linux kernel < 2.6.33 doesn't support Security File Capabilities by default
net_priv: $(WL_EXE)
	sudo setcap CAP_NET_ADMIN=eip $^

$(SERVER_DONGLE): $(DONGLE_OBJS)
	@mkdir -pv $(@D)
	$(strip $(CC) $(LDFLAGS) -o $@ $^ $(ASD_LDFLAGS))

$(SERVER_SOCKET): $(SOCKET_OBJS)
	@mkdir -pv $(@D)
	$(strip $(CC) $(LDFLAGS) -o $@ $^ $(ASD_LDFLAGS))

$(SERVER_WIFI): $(WIFI_OBJS)
	@mkdir -pv $(@D)
	$(strip $(CC) $(LDFLAGS) -o $@ $^ $(ASD_LDFLAGS))

$(SERVER_SERIAL): $(SERIAL_OBJS)
	@mkdir -pv $(@D)
	$(strip $(CC) $(LDFLAGS) -o $@ $^ $(ASD_LDFLAGS))

$(WLM_SO): $(WLM_OBJS)
	@mkdir -pv $(@D)
	$(strip $(CC) $(LDFLAGS) -shared -o $@ $^)


wldefs: wlmain.o
	$(CC) $(LDFLAGS) -o $@ $<

.PHONY: all clean wlm release_bins net_priv

release_bins:
	@mkdir -p $(INSTALL_DIR)
	install -pv $(WL_EXE) $(INSTALL_DIR)
ifeq ($(filter mips_be,$(TARGETARCH)),)
	install -pv $(SERVER_SOCKET) $(INSTALL_DIR)
	install -pv $(SERVER_DONGLE) $(INSTALL_DIR)
	install -pv $(SERVER_WIFI) $(INSTALL_DIR)
	install -pv $(SERVER_SERIAL) $(INSTALL_DIR)
endif
ifeq ($(ASD),1)
	$(MAKE) -C ../../tools/Wifi_ASD_test release_bins
endif #ASD

clean::
	rm -fv $(WL_EXE) $(WL_OBJS) $(SERVER_SOCKET) $(SERVER_DONGLE) $(SERVER_WIFI) $(SERVER_SERIAL) $(SOCKET_OBJS) $(SERIAL_OBJS) $(WIFI_OBJS) $(DONGLE_OBJS) $(WLM_SO) $(WLM_OBJS) $(SERVER_SERIAL) $(DEPENDS)

endif # TARGETENV linux

#-----------------------------------------------------------------
# MacOS build
#

ifeq ($(TARGETENV), macos)

include $(SRCBASE)/Makerules.env

.PHONY: all everything clean wl wl_server_socket wl_server_wifi wl_dongle

PROJTGT	:= wl
BUILD	:= xcodebuild
PROJECT	:= wl.xcodeproj
OBJDIR	:= $(TARGETENV)/$(MACOS_VER)

all: wl wl_server_socket

everything: wl wl_server_socket wl_server_wifi wl_dongle

clean::
	rm -rf $(OBJDIR) build

wl wl_server_socket wl_server_wifi: | $(OBJDIR)

#
# dongle wl is located at ./build/Debug/wl
#
wl_dongle:
	$(BUILD) -project $(PROJECT) -target $(PROJTGT) -configuration Debug build

WL_EXE		:= $(OBJDIR)/wl
SERVER_SOCKET	:= $(OBJDIR)/socket/wl_server_socket
SERVER_WIFI	:= $(OBJDIR)/wifi/wl_server_wifi

WL_OBJS := $(addprefix $(OBJDIR)/,wlu.o wlu_common.o wlu_macos.o bcmutils.o bcmwifi_channels.o wlu_cmd.o wlu_iov.o miniopt.o bcm_app_utils.o wlu_rates_matrix.o wlc_ppr.o)
SERVER_OBJS := wlu_server_macos.o  wlu_pipe_linux.o wlu_pipe.o wlu_server_shared.o shellproc_linux.o
SOCKET_OBJS := $(SERVER_OBJS:%.o=$(OBJDIR)/socket/%.o)
WIFI_OBJS := $(SERVER_OBJS:%.o=$(OBJDIR)/wifi/%.o)

# Derive a final list of all known objects.
OBJS         := $(WL_OBJS) $(SOCKET_OBJS) $(WIFI_OBJS)

# Create the sub-directories needed to hold object files.
$(sort $(dir $(OBJS))):
	@mkdir -pv $@

# Make a list of dependency files, one per object file.
DEPENDS      := $(join $(dir $(OBJS)),$(addprefix .,$(addsuffix .depend,$(notdir $(OBJS)))))

# Include any dependency files which currently exist.
-include $(DEPENDS)

vpath %.c .. $(SRCBASE)/shared $(SRCBASE)/wl/sys $(SRCBASE)/wl/phy $(WLAN_StdSrcDirsR) $(WLAN_ComponentSrcDirsR)

MACOS_VER ?:= $(shell sw_vers -productVersion)

ifneq (,$(findstring 10.9,$(MACOS_VER)))
        GCDEFS  += -DRDR_5905993 -DWLP2P -DWLMCHAN -DWLAWDL -DWLOFFLD -DPPR_API
        GCDEFS  += -iwithsysroot /System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/IOKit/apple80211
else
ifneq (,$(findstring 10.8.3,$(MACOS_VER)))
        GCDEFS  += -DRDR_5905993 -DWLP2P -DWLMCHAN -DWLAWDL -DWLOFFLD -DPPR_API
        GCDEFS  += -I/usr/include
        GCDEFS  += -iwithsysroot /System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/IOKit/apple80211
else
ifneq (,$(findstring 10.8,$(MACOS_VER)))
	GCDEFS	:= $(GCDEFS) -DRDR_5905993 -DWLP2P -DWLMCHAN -DWLAWDL -DWLOFFLD -DPPR_API -I/usr/include -I/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/IOKit/apple80211
else
ifneq (,$(findstring 10.7,$(MACOS_VER)))
	GCDEFS	:= $(GCDEFS) -DRDR_5905993 -DWLP2P -DWLMCHAN -DWLOFFLD -DPPR_API
else
ifneq (,$(findstring 10.6,$(MACOS_VER)))
	GCDEFS	:= $(GCDEFS) -DRDR_5905993 -DWLP2P -DWLMCHAN -DWLOFFLD -DPPR_API
endif
endif
endif
endif
endif

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(call CompileAndMakedep,$(CC),$(CFLAGS) -o $@ $<)

wl: $(OBJDIR)/wl
$(OBJDIR)/wl: $(WL_OBJS)
ifneq ($(findstring $(MACOS_VER), 10.8 10.9),)
	$(CC) -L/usr/lib $(LDFLAGS) -o $@ $^
else
	$(CC) $(LDFLAGS) -o $@ $^
endif

wl_server_socket: $(SERVER_SOCKET)
$(SERVER_SOCKET): $(SOCKET_OBJS)
	$(strip $(CC) $(LDFLAGS) -o $@ $^)

$(OBJDIR)/socket/%.o: %.c | $(OBJDIR)/socket
	$(call CompileAndMakedep,$(CC),$(CFLAGS) -DRWL_SOCKET -DREMOTE_WL -o $@ $<)

wl_server_wifi: $(SERVER_WIFI)
$(SERVER_WIFI): $(WIFI_OBJS)
	$(strip $(CC) $(LDFLAGS) -o $@ $^)

$(OBJDIR)/wifi/%.o: %.c | $(OBJDIR)/wifi
	$(call CompileAndMakedep,$(CC),$(CFLAGS) -DRWL_WIFI -DREMOTE_WL -o $@ $<)

endif # TARGETENV macos

endif # UNAME
