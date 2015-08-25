LOCAL_PATH := external/dnsmasq/src

#########################

include $(CLEAR_VARS)
LOCAL_SRC_FILES :=  bpf.c cache.c dbus.c dhcp.c dnsmasq.c \
                    forward.c helper.c lease.c log.c \
                    netlink.c network.c option.c rfc1035.c \
		    rfc2131.c tftp.c util.c conntrack.c \
		    dhcp6.c rfc3315.c dhcp-common.c outpacket.c \
		    radv.c slaac.c auth.c ipset.c domain.c \
	            dnssec.c dnssec-openssl.c blockdata.c tables.c \
		    loop.c inotify.c poll.c

LOCAL_MODULE := dnsmasq

LOCAL_C_INCLUDES := external/dnsmasq/src

LOCAL_CFLAGS := -O2 -g -W -Wall -D__ANDROID__ -DNO_IPV6 -DNO_TFTP -DNO_SCRIPT
LOCAL_SYSTEM_SHARED_LIBRARIES := libc libcutils

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog

include $(BUILD_EXECUTABLE)
