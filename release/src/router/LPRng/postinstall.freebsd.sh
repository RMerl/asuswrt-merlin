#
# -- START --
# postinstall.freebsd.sh,v 1.1 2001/08/21 20:33:16 root Exp
#
#  If you are building a PORT, see the
#  DISTRIBUTIONS/Freebsd directory for a complete port
#  building package.
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
echo RUNNING postinstall.freebsd.sh parms "'$0 $@'" MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" INIT="$INIT" cwd `pwd`
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
fix () {
    v=`echo $1 | sed -e 's/[:;].*//'`;
    p=`echo $2 | sed -e 's/:.*//'`; d=`dirname $p`;
    if expr "$p" : "\|" >/dev/null ; then
        echo "$v is a filter '$p'" 
        return 0
    fi
    echo "Putting $p in $d, using $v.sample"
    if [ ! -d "$d" ] ; then
        echo "Directory $d does not exist!"
        mkdir -p $d
    fi
    if [ -f $v.sample ] ; then
        if [ $v.sample != $p.sample ] ; then ${INSTALL} $v.sample $p.sample; fi
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
# we use the /usr/local/etc/rc.d method to start
# lpd
# we have to take them from one place and put in another
if [ "X$MAKEPACKAGE" = "XYES" ] ; then
    hold=${DESTDIR}${PREFIX}/etc/lpd
    echo "Setting up configuration files path for package" ${hold}
    # we put files into the destination
    if [ ! -d ${hold} ] ; then mkdir -p ${hold} ; fi;
    ${INSTALL} lpd.perms ${hold}/lpd.perms.sample
    ${INSTALL} lpd.conf ${hold}/lpd.conf.sample
    ${INSTALL} printcap ${hold}/printcap.sample
    if [ "$INIT" != no ] ; then
        ${INSTALL} -m 755 init.freebsd ${hold}/lprng.sh
    fi
elif [ "X$MAKEINSTALL" = XYES ] ; then
	# we have the port pre-install operation
	if [ "$MANDIR" = "/usr/man" -a ! -d ${DESTDIR}/usr/man ] ; then
		# we have the dreaded standard installation
		# try to make a symbolic link to 
		echo "Creating symbolic link from /usr/man to /usr/share/man"
		v=`ln -s ${DESTDIR}/usr/share/man ${DESTDIR}/usr/man`;
	fi
    hold=${DESTDIR}${PREFIX}/etc/lpd
    echo "Setting up configuration files path for installation" ${hold}
    if [ ! -d ${hold} ] ; then mkdir -p ${hold} ; fi;
    ${INSTALL} lpd.perms ${hold}/lpd.perms.sample
    ${INSTALL} lpd.conf ${hold}/lpd.conf.sample
    ${INSTALL} printcap ${hold}/printcap.sample

    fix ${hold}/lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"
    fix ${hold}/lpd.conf "${DESTDIR}${LPD_CONF_PATH}"
    fix ${hold}/printcap "${DESTDIR}${PRINTCAP_PATH}"

    if [ "$INIT" != no ] ; then
		if [ -f /etc/rc.conf ] ; then
			perl -spi.bak -e 's/^lpd_enable/#lpd_enable/;' ${DESTDIR}/etc/rc.conf 
		fi
		${INSTALL} -m 755 init.freebsd ${hold}/lprng.sh
		init=${DESTDIR}/usr/local/etc/rc.d/lprng.sh
		echo "Setting up init script $init using init.freebsd"
		if [ ! -d `dirname $init` ] ; then mkdir -p `dirname $init ` ; fi;
		rm -f $init
		${INSTALL} -m 755 init.freebsd $init

		echo "Stopping LPD"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2;
		# check the printcap information
		echo "Checking Printcap Info and fixing permissions"
		${SBINDIR}/checkpc -f
		# restart the server
		echo "Restarting server"
		sh $init start
    fi
elif [ "X$2" = "XPOST-INSTALL" ] ; then
    # when doing an install from a package we get the file from the package
    hold=etc/lpd
    if [ -f ${hold}/lpd.perms.sample ] ; then
        fix ${hold}/lpd.perms "${LPD_PERMS_PATH}"
        fix ${hold}/lpd.conf "${LPD_CONF_PATH}"
        fix ${hold}/printcap "${PRINTCAP_PATH}"
		if [ "$INIT" != no ] ; then
			init=/usr/local/etc/rc.d/lprng.sh
			${INSTALL} -m 755 ${hold}/lprng.sh $init;
			if [ -f /etc/rc.conf ] ; then
				perl -spi.bak -e 's/^lpd_enable/#lpd_enable/;' /etc/rc.conf 
			fi
		fi
    else
        echo "WARNING: configuration files missing from package! CWD " `pwd`
        ls
        exit 1
    fi
elif [ "X$2" = "XPRE-INSTALL" ] ; then
	# we have the port pre-install operation
	if [ "$MANDIR" = "/usr/man" -a ! -d /usr/man ] ; then
		# we have the dreaded standard installation
		# try to make a symbolic link to 
		echo "Creating symbolic link from /usr/man to /usr/share/man"
		v=`ln -s /usr/share/man /usr/man`;
	fi
fi
exit 0
