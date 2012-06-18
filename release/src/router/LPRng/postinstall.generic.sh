# 
# -- START --
# postinstall.generic.sh,v 1.1 2001/08/21 20:33:16 root Exp
#
# This is the shell script that does the postinstall
# dynamic fixup
#  It needs to be massaged with the information for
#  various paths.
# If you are building a package,  then you do NOT want
#  to have this executed - it will put the sample files
#  in place.  You need to do this during the postinstall
#  step in the package installation.
#
echo RUNNING postinstall - generic MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" INIT=$INIT cwd `pwd`
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
		sh mkinstalldirs $d
	fi
	if [ -f $v.sample ] ; then
		if [ $v.sample != $p.sample ] ; then ${INSTALL} -m 644 $v.sample $p.sample; fi
	elif [ -f $v ] ; then
		if [ $v != $p.sample ] ; then ${INSTALL} $v $p.sample; fi
	else
		echo "Do not have $v.sample or $v"
	fi
	if [ ! -f $p.sample ] ; then
		echo "Do not have $p.sample"
	elif [ ! -f $p ] ; then
		${INSTALL} -m 644 $p.sample $p;
	fi;
}
if [ "X$MAKEINSTALL" = "XYES" ] ; then
	if [ -f lpd.perms ] ; then fix lpd.perms "${DESTDIR}${LPD_PERMS_PATH}" ; fi;
	if [ -f lpd.conf ] ; then fix lpd.conf "${DESTDIR}${LPD_CONF_PATH}" ; fi;
	if [ -f printcap ] ; then fix printcap "${DESTDIR}${PRINTCAP_PATH}" ; fi;
	if [ "$INIT" != "no" ] ; then
		echo "Stopping LPD"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2;
		echo "Checking printcap"
		${SBINDIR}/checkpc -f
		echo "Starting LPD"
		${LPD_PATH}
	fi
fi
exit 0
