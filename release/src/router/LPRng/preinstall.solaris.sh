#!/bin/sh
# preinstall.solaris.sh,v 1.1 2001/08/21 20:33:17 root Exp
# This is an effort to automate the setup
#  needed to install the LPRng software on the
#  Solaris OS.  This is effectively a one way path.
#  You are warned.
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
PATH=/etc:/usr/etc:/usr/bin:/bin:/sbin:/usr/sbin:$PATH
# remove the init.d entry and links
for i in /etc/rc*.d/*lp ; do
	b=`basename $i`;
	d=`dirname $i`;
	mv $i $d/UNUSED.$b.UNUSED
done
# rename files
renameit () {
	for i in $* ; do
		if [ -f $i -a '!' -f $i.old ] ; then
			echo "renaming $i $i.old";
			mv $i $i.old
		fi
	done
}
renameit /usr/bin/lp /usr/bin/lpstat /usr/sbin/lpadmin /usr/sbin/lpfilter \
	/usr/sbin/lpforms /usr/sbin/lpmove /usr/sbin/lpshut /usr/sbin/lpsystem \
	/usr/sbin/lpusers /usr/ucb/lpc /usr/ucb/lpq /usr/ucb/lpr /usr/ucb/lprm \
	/usr/ucb/lptest /usr/lib/lp/lpsched /usr/lib/lp/lpNet
# remove the cron entry
if [ -f /var/spool/cron/crontabs/lp ] ; then
	mv /var/spool/cron/crontabs/lp /var/spool/cron/UNUSED.crontabs.lp
fi
# comment out inetd.conf entry
if egrep '^printer' /etc/inetd.conf >/dev/null 2>/dev/null ; then
	mv /etc/inetd.conf /etc/inetd.conf.bak
	sed -e 's/^printer/# printer/' </etc/inetd.conf.bak >/etc/inetd.conf
fi
# remove the nlsadmin entry
nlsadmin -r lpd tcp
nlsadmin -r lp tcp
# echo REBOOT SYSTEM
