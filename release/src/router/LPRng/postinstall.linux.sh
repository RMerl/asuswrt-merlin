# 
# -- START --
# postinstall.linux.sh,v 1.1 2001/08/21 20:33:17 root Exp
#
#  If you are building an RPM package,  please see the
#  DISTRIBUTIONS/RPM directory(s) for a RPM Spec file
#  that makes a package
# This script is used from the Makefile when we are doing
#  a source level install and NOT building a package.
# We first install the sample files
#
echo RUNNING postinstall.linux.sh MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" DESTDIR="$DESTDIR" INIT="$INIT" cwd `pwd`
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
init=${DESTDIR}/etc/rc.d/init.d/lpd
if [ "X$MAKEINSTALL" = "XYES" ] ; then
	fix lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"
	fix lpd.conf "${DESTDIR}${LPD_CONF_PATH}"
	fix printcap "${DESTDIR}${PRINTCAP_PATH}"
	if [ "$INIT" != "no" ] ; then
		if [ ! -d `dirname $init` ] ; then mkdir -p `dirname $init ` ; fi;
        if [ -f /etc/redhat-release ] ; then
			${INSTALL} -m 755 init.redhat $init;
		elif [ -d /lib/lsb ] ; then
			${INSTALL} -m 755 init.linuxsb $init;
		else
			${INSTALL} -m 755 init.linux $init;
		fi
	fi;
else
	fix "${LPD_PERMS_PATH}" "${DESTDIR}${LPD_PERMS_PATH}"
	fix "${LPD_CONF_PATH}" "${DESTDIR}${LPD_CONF_PATH}"
	fix "${PRINTCAP_PATH}" "${DESTDIR}${PRINTCAP_PATH}"
fi
if [ "X$MAKEPACKAGE" != "XYES" -a "$INIT" != no ] ; then
    echo "Configuring startup scripts"
    if [ ! -f $init ] ; then
        echo "Missing $init";
    fi
    if [ -f /etc/redhat-release -a -f /sbin/chkconfig ] ; then
		echo "RedHat Linux - running chkconfig"
		(
		/sbin/chkconfig lpr off
		/sbin/chkconfig --del lpr
		/sbin/chkconfig lpr off
		/sbin/chkconfig --del lpr
		/sbin/chkconfig lprng off
		/sbin/chkconfig --del lprng
		)
		echo "Stopping server"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2
		echo "Checking Printcap"
		${SBINDIR}/checkpc -f
		echo "Installing Printer Startup Scripts"
		/sbin/chkconfig --add lprng
		/sbin/chkconfig --list lprng
		/sbin/chkconfig lprng on
		echo "Starting Printer"
		sh $init start
		echo "Printer Started"
    else
		echo "Stopping server"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2
		echo "Checking Printcap"
		${SBINDIR}/checkpc -f
		echo "Starting Printer"
		${LPD_PATH}
		echo "Printer Started"
		cat <<EOF
# 
# You will have to install the run time startup files by hand.
# The $init file contains the standard startup/shutdown sequence.
# You usually need to put in links to this file in /etc/rc.d/rcX.d
# to have it executed by the appropriate run level startup/shutdown
# script.
#
# You can use the following a template for your installation
#
	ln -s ../init.d/lprng /etc/rc0.d/K60lprng
	ln -s ../init.d/lprng /etc/rc1.d/K60lprng
	ln -s ../init.d/lprng /etc/rc2.d/S60lprng
	ln -s ../init.d/lprng /etc/rc3.d/S60lprng
	ln -s ../init.d/lprng /etc/rc4.d/S60lprng
	ln -s ../init.d/lprng /etc/rc5.d/S60lprng
	ln -s ../init.d/lprng /etc/rc6.d/K60lprng

EOF
	fi;
fi;
