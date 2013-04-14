#!/bin/sh
filedir=/etc/openvpn/dns
filebase=`echo $filedir/$dev | sed 's/\(tun\|tap\)1/client/;s/\(tun\|tap\)2/server/'`
conffile=$filebase\.conf
resolvfile=$filebase\.resolv
fileexists=
if [ ! -d $filedir ]; then mkdir $filedir; fi
if [ -f $conffile ]; then rm $conffile; fileexists=1; fi
if [ -f $resolvfile ]; then rm $resolvfile; fileexists=1; fi

if [ $script_type == 'up' ]
then
	for optionname in `set | grep "^foreign_option_" | sed "s/^\(.*\)=.*$/\1/g"`
	do
		option=`eval "echo \\$$optionname"`
		if echo $option | grep "dhcp-option WINS "; then echo $option | sed "s/ WINS /=44,/" >> $conffile; fi
		if echo $option | grep "dhcp-option DNS"; then echo $option | sed "s/dhcp-option DNS/nameserver/" >> $resolvfile; fi
		if echo $option | grep "dhcp-option DOMAIN"; then echo $option | sed "s/dhcp-option DOMAIN/search/" >> $resolvfile; fi
	done
fi

if [ -f $conffile -o -f $resolvfile -o -n "$fileexists" ]; then service updateresolv; fi
rmdir $filedir
rmdir /etc/openvpn

if [ -f /jffs/scripts/openvpn-event ]
then
	sh /jffs/scripts/openvpn-event $*
fi
exit 0
