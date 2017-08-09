#!/usr/bin/env python

#
# Use a TSIG-signed DDNS update to update our hostname-to-address
# mapping.
#
# usage: ddns.py <ip-address>
#
# On linux systems, you can automatically update your DNS any time an
# interface comes up by adding an ifup-local script that invokes this
# python code.
#
# E.g. on my systems I have this
#
#	#!/bin/sh
#
#	DEVICE=$1
#
#	if [ "X${DEVICE}" == "Xeth0" ]; then
#        	IPADDR=`LANG= LC_ALL= ifconfig ${DEVICE} | grep 'inet addr' |
#                	awk -F: '{ print $2 } ' | awk '{ print $1 }'`
#		/usr/local/sbin/ddns.py $IPADDR
#	fi
#
# in /etc/ifup-local.
#

import sys

import dns.update
import dns.query
import dns.tsigkeyring

#
# Replace the keyname and secret with appropriate values for your
# configuration.
#
keyring = dns.tsigkeyring.from_text({
    'keyname.' : 'NjHwPsMKjdN++dOfE5iAiQ=='
    })

#
# Replace "example." with your domain, and "host" with your hostname.
#
update = dns.update.Update('example.', keyring=keyring)
update.replace('host', 300, 'A', sys.argv[1])

#
# Replace "10.0.0.1" with the IP address of your master server.
#
response = dns.query.tcp(update, '10.0.0.1', timeout=10)
