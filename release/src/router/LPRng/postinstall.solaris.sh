#
# -- START --
# postinstall.solaris.sh,v 1.1 2001/08/21 20:33:17 root Exp
#
# We use this when we are building a package or doing an install
#
#
echo RUNNING postinstall.solaris MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" INIT="$INIT" cwd `pwd`
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
fix () {
	v=`echo $1 | sed -e 's/[:;].*//'`;
    p=`echo $2 | sed -e 's/:.*//'`; d=`dirname $p`;
	if expr "$p" : "\|" >/dev/null ; then
		echo "$v is a filter '$p'" 
		return 0
	fi
	echo "Checking for $v.sample in $d"
	if [ ! -d "$d" ] ; then
		echo "Directory $d does not exist!"
		mkdir -p $d;
	fi
	if [ -f $v.sample ] ; then
		if [ $v.sample != $p.sample ] ; then ${INSTALL} -m 644 $v.sample $p.sample; fi
	elif [ -f $v ] ; then
		if [ $v != $p.sample ] ; then ${INSTALL} -m 644 $v $p.sample; fi
	else
		echo "Do not have $v.sample or $v"
	fi
	if [ ! -f $p.sample ] ; then
		echo "Do not have $p.sample"
	elif [ ! -f $p ] ; then
		${INSTALL} -m 644 $p.sample $p;
	fi;
}
echo "Installing configuration files"
init=${DESTDIR}/etc/init.d/lprng
if [ -f lpd.perms ] ; then fix lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"; fi;
if [ -f lpd.conf ] ; then fix lpd.conf "${DESTDIR}${LPD_CONF_PATH}"; fi;
if [ -f printcap ] ; then fix printcap "${DESTDIR}${PRINTCAP_PATH}"; fi;
fix "${DESTDIR}${LPD_PERMS_PATH}" "${DESTDIR}${LPD_PERMS_PATH}"
fix "${DESTDIR}${LPD_CONF_PATH}" "${DESTDIR}${LPD_CONF_PATH}"
fix "${DESTDIR}${PRINTCAP_PATH}" "${DESTDIR}${PRINTCAP_PATH}"
#
# Now we reconfigure the printer 
#
if [ "$INIT" != no ] ; then
	if [ -f init.solaris ] ; then
		if [ ! -d `dirname $init` ] ; then mkdir -p `dirname $init ` ; fi;
		${INSTALL} -m 755 init.solaris $init
	fi
	for i in rc2.d/S80lprng rc1.d/K39lprng \
		rc0.d/K39lprng rcS.d/K39lprng ; do
		s=${DESTDIR}/etc/$i;
		if [ ! -d `dirname $s` ] ; then mkdir -p `dirname $s` ; fi;
		rm -f $s;
		echo ln -s ../init.d/lprng $s;
		ln -s ../init.d/lprng $s;
	done
	if [ "$MAKEPACKAGE" != "YES" ]; then
		if grep '^printer' /etc/inetd.conf >/dev/null; then
			echo "Removing printer service from inetd.conf"
			${INSTALL} -m 644 /etc/inetd.conf /etc/inetd.conf.orig
			sed -e 's/^printer/#printer/' < /etc/inetd.conf.orig >/etc/inetd.conf
			echo "Restarting inetd" 
			kill -HUP `ps ${PSHOWALL} | awk '/inetd/{ print $1;}'` >/dev/null 2>&1
		fi
		if [ -x /usr/sbin/lpshut ] ; then
			echo "Stopping lpsched"
			lpshut
		fi;
		for i in `ls /etc/*.d/*lp 2>/dev/null` ; do
			echo saving $i
			f=`basename $i`; d=`dirname $i`;
			mv $i $d/UNUSED.$f.orig
		done
		echo "Stopping lpd" 
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2
		echo "Checking printcap" 
		${SBINDIR}/checkpc -f
		echo "Starting lprng lpd server"
		sh $init start
	fi;
fi;
exit 0
