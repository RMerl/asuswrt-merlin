TARGET_DRV 	:= drxvi314
INTERFACE 	:= src/Interface/usb/
COMMON		:= src/Common/

FLAG = $(shell if [ -d "$(SRCBASE)/linux/linux-2.6/drivers/usb/Beceem_driver/src" ]; then echo 1; else echo 0; fi)

ifeq ($(FLAG),1)
ifdef CONFIG_USB_BECEEM
obj-m += $(TARGET_DRV).o
endif
endif

$(TARGET_DRV)-objs := $(INTERFACE)InterfaceDld.o \
		$(INTERFACE)InterfaceIdleMode.o $(INTERFACE)InterfaceInit.o\
 		$(INTERFACE)InterfaceRx.o\
		$(INTERFACE)InterfaceIsr.o\
		$(INTERFACE)InterfaceMisc.o $(INTERFACE)InterfaceTx.o\
		$(COMMON)Arp.o $(COMMON)CmHost.o $(COMMON)Debug.o\
		$(COMMON)IPv6Protocol.o $(COMMON)Qos.o $(COMMON)Transmit.o\
		$(COMMON)Bcmnet.o $(COMMON)DDRInit.o $(COMMON)HandleControlPacket.o\
		$(COMMON)LeakyBucket.o $(COMMON)Misc.o $(COMMON)sort.o\
		$(COMMON)Bcmchar.o $(COMMON)hostmibs.o $(COMMON)PHSModule.o\
	 	$(COMMON)Osal_Misc.o $(COMMON)led_control.o $(COMMON)nvm.o $(COMMON)vendorspecificextn.o

