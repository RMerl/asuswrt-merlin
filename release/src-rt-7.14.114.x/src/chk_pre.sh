#~/bin/sh

cd router/

if [ -f shared/prebuilt/tcode.o ]; then
	echo shared/tcode.o... checked
else
	mkdir shared/prebuilt/
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/shared/prebuilt/tcode.o shared/prebuilt/tcode.o
	echo cp shared/tcode.o... done
fi



if [ -f libupnp_arm_7114/prebuilt/libupnp.so ]; then
	echo libupnp_arm_7114/prebuilt/libupnp.so... checked
else
	mkdir libupnp_arm_7114/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/libupnp_arm_7114/prebuilt/libupnp.so libupnp_arm_7114/prebuilt/libupnp.so 
	echo cp libupnp_arm_7114/prebuilt/libupnp.so... done
fi

if [ -f rtl_bin/prebuilt/rtl8365mb.o ]; then
	echo rtl_bin/prebuilt/rtl8365mb.o... checked
else
	mkdir rtl_bin/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rtl_bin/prebuilt/rtl8365mb.o rtl_bin/prebuilt/rtl8365mb.o 
	echo cp rtl_bin/prebuilt/rtl8365mb.o... done
fi

if [ -f wl_arm_7114/prebuilt/dhd.o ]; then
	echo wl_arm_7114/prebuilt/dhd.o... checked
else
	mkdir wl_arm_7114/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/wl_arm_7114/prebuilt/dhd.o wl_arm_7114/prebuilt/dhd.o 
	echo cp ... done
fi

if [ -f emf_arm_7114/emfconf/prebuilt/emf ]; then
	echo emf_arm_7114/emfconf/prebuilt/emf... checked
else
	mkdir emf_arm_7114/emfconf/prebuilt
	mkdir emf_arm_7114/igsconf/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/emf_arm_7114/igs.o emf_arm_7114/igs.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/emf_arm_7114/emf.o emf_arm_7114/emf.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/emf_arm_7114/emfconf/prebuilt/emf emf_arm_7114/emfconf/prebuilt/emf 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/emf_arm_7114/igsconf/prebuilt/igs emf_arm_7114/igsconf/prebuilt/igs 
	echo cp emf_arm_7114/igsconf/prebuilt/igs... done
fi

if [ -f lacp/linux/lacp.o ]; then
	echo lacp/linux/lacp.o... checked
else
	mkdir lacp/linux
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/lacp/linux/lacp.o lacp/linux/lacp.o 
	echo cp lacp/linux/lacp.o... done
fi

if [ -f rc/prebuild/agg_brcm.o ]; then
	echo rc/prebuild/... checked
else
	mkdir rc/prebuild
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/agg_brcm.o rc/prebuild/agg_brcm.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/ate-broadcom.o rc/prebuild/ate-broadcom.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/dsl_fb.o rc/prebuild/dsl_fb.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/dualwan.o rc/prebuild/dualwan.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/pwdec.o rc/prebuild/pwdec.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/tcode_brcm.o rc/prebuild/tcode_brcm.o 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/rc/prebuild/tcode_rc.o rc/prebuild/tcode_rc.o 
	echo cp rc/prebuild/... done
fi


if [ -f bwdpi/prebuilt/wrs ]; then
	echo bwdpi/prebuilt/wrs... checked
else
	mkdir bwdpi/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/bwdpi/prebuilt/wrs bwdpi/prebuilt/wrs 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/bwdpi/prebuilt/libbwdpi.so bwdpi/prebuilt/libbwdpi.so
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/bwdpi/prebuilt/bwdpi.h bwdpi/prebuilt/bwdpi.h
	echo cp bwdpi/prebuilt/bwdpi.h... done
fi

if [ -f bwdpi_sqlite/prebuilt/libbwdpi_sql.so ]; then
	echo bwdpi_sqlite/prebuilt... checked
else
	mkdir bwdpi_sqlite/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/bwdpi_sqlite/prebuilt/libbwdpi_sql.so bwdpi_sqlite/prebuilt/libbwdpi_sql.so 
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/bwdpi_sqlite/prebuilt/bwdpi_sqlite bwdpi_sqlite/prebuilt/bwdpi_sqlite 
	echo cp bwdpi_sqlite/prebuilt... done
fi


if [ -f wps_arm_7114/prebuilt/wps_monitor ]; then
	echo wps_arm_7114/prebuilt/wps_monitor... checked
else
	mkdir wps_arm_7114/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/wps_arm_7114/prebuilt/wps_monitor wps_arm_7114/prebuilt/wps_monitor 
	echo cp wps_arm_7114/prebuilt/wps_monitor... done
fi

if [ -f dhd_monitor/prebuilt/dhd_monitor ]; then
	echo dhd_monitor/prebuilt/dhd_monitor... checked
else
	mkdir dhd_monitor/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/dhd_monitor/prebuilt/dhd_monitor dhd_monitor/prebuilt/dhd_monitor 
	echo cp dhd_monitor/prebuilt/dhd_monitor... done
fi

if [ -f wlconf_arm_7114/prebuilt/wlconf ]; then
	echo wlconf_arm_7114/prebuilt/wlconf... checked
else
	mkdir wlconf_arm_7114/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/wlconf_arm_7114/prebuilt/wlconf wlconf_arm_7114/prebuilt/wlconf 
	echo cp wlconf_arm_7114/prebuilt/wlconf... done
fi

if [ -f nas_arm_7114/nas/prebuilt/nas ]; then
	echo nas_arm_7114/nas/prebuilt/nas... checked
else
	mkdir nas_arm_7114/nas/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/nas_arm_7114/nas/prebuilt/nas nas_arm_7114/nas/prebuilt/nas 
	echo cp nas_arm_7114/nas/prebuilt/nas... done
fi

if [ -f eapd_arm_7114/linux/prebuilt/eapd ]; then
	echo eapd_arm_7114/linux/prebuilt/eapd... checked
else
	mkdir eapd_arm_7114/linux/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/eapd_arm_7114/linux/prebuilt/eapd eapd_arm_7114/linux/prebuilt/eapd 
	echo cp eapd_arm_7114/linux/prebuilt/eapd... done
fi

if [ -f inotify/prebuild/inotify ]; then
	echo inotify/prebuild/inotify... checked
else
	mkdir inotify/prebuild
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/inotify/prebuild/inotify inotify/prebuild/inotify 
	echo cp inotify/prebuild/inotify... done
fi

if [ -f acsd_arm_7114/prebuilt/acsd ]; then
	echo acsd_arm_7114/prebuilt/acsd... checked
else
	mkdir acsd_arm_7114/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/acsd_arm_7114/prebuilt/acsd acsd_arm_7114/prebuilt/acsd 
	echo cp acsd_arm_7114/prebuilt/acsd... done
fi

if [ -f bsd/prebuilt/bsd ]; then
	echo bsd/prebuilt/bsd... checked
else
	mkdir bsd/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src/router/bsd/prebuilt/bsd bsd/prebuilt/bsd 
	echo cp bsd/prebuilt/bsd... done
fi

cd ../

if [ -f wl/exe/prebuilt/wl ]; then
	echo wl/exe/prebuilt/wl... checked
else
	mkdir wl/exe/prebuilt
	cp /home/ham/asuswrt-3004/asuswrt.prebuilt/release/src-rt-7.14.114.x/src/wl/exe/prebuilt/wl wl/exe/prebuilt/wl 
	echo cp wl/exe/prebuilt/wl... done
fi


echo ALL Done !
