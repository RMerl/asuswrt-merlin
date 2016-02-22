#!/bin/sh
filedir=/etc/openvpn/dns
filebase=$(echo $filedir/$dev | sed 's/\(tun\|tap\)1/client/;s/\(tun\|tap\)2/server/')
conffile=$filebase\.conf
resolvfile=$filebase\.resolv
dnsscript=$(echo /etc/openvpn/fw/$(echo $dev)-dns\.sh | sed 's/\(tun\|tap\)1/client/;s/\(tun\|tap\)2/server/')
fileexists=
instance=$(echo $dev | sed "s/tun1//;s/tun2*/0/")


create_client_list(){
	server=$1
	VPN_IP_LIST=$(nvram get vpn_client$(echo $instance)_clientlist)

	IFS="<"

	for ENTRY in $VPN_IP_LIST
	do
		if [ "$ENTRY" = "" ]
		then
			continue
		fi
		TARGET_ROUTE=$(echo $ENTRY | cut -d ">" -f 4)
		if [ "$TARGET_ROUTE" = "VPN" ]
		then
			VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
			if [ "$VPN_IP" != "0.0.0.0" ]
			then
				echo iptables -t nat -A DNSVPN$instance -s $VPN_IP -j DNAT --to-destination $server >> $dnsscript
			fi
			logger -t "openvpn-updown" "Forcing $VPN_IP to use DNS server $server"
		fi
	done
	IFS=$OLDIFS
}


if [ ! -d $filedir ]; then mkdir $filedir; fi
if [ -f $conffile ]; then rm $conffile; fileexists=1; fi
if [ -f $resolvfile ]; then rm $resolvfile; fileexists=1; fi

if [ $script_type == 'up' ]
then
	echo iptables -t nat -N DNSVPN$instance > $dnsscript

	if [ $instance != 0 -a $(nvram get vpn_client$(echo $instance)_rgw) == 2 -a $(nvram get vpn_client$(echo $instance)_adns) == 3 ]
	then
		setdns=0
	else
		setdns=-1
	fi

	for optionname in $(set | grep "^foreign_option_" | sed "s/^\(.*\)=.*$/\1/g")
	do
		option=$(eval "echo \\$$optionname")
		if echo $option | grep "dhcp-option WINS "; then echo $option | sed "s/ WINS /=44,/" >> $conffile; fi
		if echo $option | grep "dhcp-option DNS"
		then
			echo $option | sed "s/dhcp-option DNS/nameserver/" >> $resolvfile
			if [ $setdns == 0 ]
			then
				create_client_list $(echo $option | sed "s/dhcp-option DNS//")
				setdns=1
			fi
		fi
		if echo $option | grep "dhcp-option DOMAIN"; then echo $option | sed "s/dhcp-option DOMAIN/search/" >> $resolvfile; fi
	done

	if [ $setdns == 1 ]
	then
		echo iptables -t nat -A PREROUTING -p udp -m udp --dport 53 -j DNSVPN$instance >> $dnsscript
		echo iptables -t nat -A PREROUTING -p tcp -m tcp --dport 53 -j DNSVPN$instance >> $dnsscript
	fi
fi


if [ $script_type == 'down' -a $instance != 0 ]
then
	/usr/sbin/iptables -t nat -D PREROUTING -p udp -m udp --dport 53 -j DNSVPN$instance
	/usr/sbin/iptables -t nat -D PREROUTING -p tcp -m tcp --dport 53 -j DNSVPN$instance
	/usr/sbin/iptables -t nat -F DNSVPN$instance
	/usr/sbin/iptables -t nat -X DNSVPN$instance
fi

if [ -f $conffile -o -f $resolvfile -o -n "$fileexists" ]
then
	if [ $script_type == 'up' ] ; then
		if [ -f $dnsscript ]
		then
			sh $dnsscript
		fi
		service updateresolv
	elif [ $script_type == 'down' ]; then
		rm $dnsscript
		service updateresolv
		service restart_dnsmasq
	fi
fi

rmdir $filedir
rmdir /etc/openvpn

if [ -f /jffs/scripts/openvpn-event ]
then
	logger -t "custom script" "Running /jffs/scripts/openvpn-event (args: $*)"
	sh /jffs/scripts/openvpn-event $*
fi

exit 0
