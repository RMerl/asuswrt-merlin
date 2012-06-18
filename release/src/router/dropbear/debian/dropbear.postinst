#!/bin/sh
set -e

test "$1" = 'configure' || exit 0

if test ! -e /etc/dropbear/dropbear_rsa_host_key; then
  if test -f /etc/ssh/ssh_host_rsa_key; then
    echo "Converting existing OpenSSH RSA host key to Dropbear format."
    /usr/lib/dropbear/dropbearconvert openssh dropbear \
      /etc/ssh/ssh_host_rsa_key /etc/dropbear/dropbear_rsa_host_key
  else
    echo "Generating Dropbear RSA key. Please wait."
    dropbearkey -t rsa -f /etc/dropbear/dropbear_rsa_host_key
  fi
fi
if test ! -e /etc/dropbear/dropbear_dss_host_key; then
  if test -f /etc/ssh/ssh_host_dsa_key; then
    echo "Converting existing OpenSSH RSA host key to Dropbear format."
    /usr/lib/dropbear/dropbearconvert openssh dropbear \
      /etc/ssh/ssh_host_dsa_key /etc/dropbear/dropbear_dss_host_key
  else
    echo "Generating Dropbear DSS key. Please wait."
    dropbearkey -t dss -f /etc/dropbear/dropbear_dss_host_key
  fi
fi
if test ! -s /etc/default/dropbear; then 
  # check whether OpenSSH seems to be installed.
  if test -x /usr/sbin/sshd; then
    cat <<EOT
OpenSSH appears to be installed.  Setting /etc/default/dropbear so that
Dropbear will not start by default.  Edit this file to change this behaviour.

EOT
    cat >>/etc/default/dropbear <<EOT
# disabled because OpenSSH is installed
# change to NO_START=0 to enable Dropbear
NO_START=1

EOT
  fi
  cat >>/etc/default/dropbear <<EOT
# the TCP port that Dropbear listens on
DROPBEAR_PORT=22

# any additional arguments for Dropbear
DROPBEAR_EXTRA_ARGS=

# specify an optional banner file containing a message to be
# sent to clients before they connect, such as "/etc/issue.net"
DROPBEAR_BANNER=""

# RSA hostkey file (default: /etc/dropbear/dropbear_rsa_host_key)
#DROPBEAR_RSAKEY="/etc/dropbear/dropbear_rsa_host_key"

# DSS hostkey file (default: /etc/dropbear/dropbear_dss_host_key)
#DROPBEAR_DSSKEY="/etc/dropbear/dropbear_dss_host_key"

# Receive window size - this is a tradeoff between memory and
# network performance
DROPBEAR_RECEIVE_WINDOW=65536
EOT
fi

if test -x /etc/init.d/dropbear; then
  update-rc.d dropbear defaults >/dev/null
  if test -x /usr/sbin/invoke-rc.d; then
    invoke-rc.d dropbear restart
  else
    /etc/init.d/dropbear restart
  fi
fi

if test -n "$2" && dpkg --compare-versions "$2" lt '0.50-4' &&
update-service --check dropbear 2>/dev/null; then
  update-service --remove /etc/dropbear 2>/dev/null || :
  sleep 6
  rm -rf /var/run/dropbear /var/run/dropbear.log
  update-service --add /etc/dropbear || :
fi
