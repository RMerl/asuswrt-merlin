#
# written by Willi Burmeister (wib@cs.uni-kiel.de) for LPRng
#
echo RUNNING postremove.solaris
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
if [ -z "${PKG_INSTALL_ROOT}" ]; then
  # Send hup signal to inetd
  echo "Sending inetd SIGHUP"
  kill -HUP `ps ${PSHOWALL} | awk '/inetd/{ print $1;}'` >/dev/null 2>&1
  echo "Sent inetd SIGHUP"
fi
#
# end of file
#
