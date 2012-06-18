#
# -- START --
# preremove.solaris.sh,v 1.1 2001/08/21 20:33:17 root Exp
#
# This is the shell script that does the preremove
echo RUNNING preremove.solaris.sh
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
echo "Stopping LPD"
pkill -INT lpd
exit 0
