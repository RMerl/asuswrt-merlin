include $(TOP)/.config

obj-m := bled.o

bled-objs := common.o check-netdev.o

bled-objs += $(if $(RTCONFIG_USB),check-usbbus.o)
bled-objs += $(if $(QCA),gpio-qca955x.o)

QCA8337_MODEL_LIST = "RT-AC55U" "4G-AC55U"
MT7620ESW_MODEL_LIST = "RT-N11P RT-N14U RT-AC51U RT-AC52U RT-AC1200HP"
MT7621ESW_MODEL_LIST = "RT-N56UB1"
# QCA platform
# AR8216 phy + QCA8337N switch
ifneq ($(findstring $(BUILD_NAME),$(QCA8337_MODEL_LIST)),)
bled-objs += check-ar8216-qca8337.o
endif

# Ralink/MTK platform
# MT7620 embedded switch
ifneq ($(findstring $(BUILD_NAME),$(MT7620ESW_MODEL_LIST)),)
bled-objs += gpio-mt7620.o check-mt7620esw.o
endif

# MT7621 embedded switch
ifneq ($(findstring $(BUILD_NAME),$(MT7621ESW_MODEL_LIST)),)
bled-objs += gpio-mt7621.o check-mt7621esw.o
endif
