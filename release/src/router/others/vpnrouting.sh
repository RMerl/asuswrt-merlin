#!/bin/sh

update_client_list(){
	# delete rules for IPs not on list
	OLDIFS=$IFS
	IP_LIST=`ip rule show|awk '$2 == "from" && $4=="lookup" && $5==ENVIRON["VPN_TBL"] {print $3}'`
	for IP in $IP_LIST
	do
		DEL_IP="y"
		IFS="<"
		for ENTRY in $VPN_IP_LIST
		do
			VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
			if [ "$IP" == "$VPN_IP" ]
			then
				DEL_IP=
			fi
		done
		IFS=$OLDIFS
		if [ "$DEL_IP" == "y" ]
		then
			ip rule del from $IP table $VPN_TBL
			logger -t "openvpn-routing" "Removing $IP from selective routing"
		fi
	done

	# add missing rules
	IFS="<"
	for ENTRY in $VPN_IP_LIST
	do
		VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
		if [ "$VPN_IP" != "" ]
		then
			IP_LIST=`ip rule show|awk '$2=="from" && $3==ENVIRON["VPN_IP"] && $4=="lookup" && $5==ENVIRON["VPN_TBL"] {print $3}'`
			if [ "$IP_LIST" == "" ]
			then
				ip rule add from $VPN_IP table $VPN_TBL
				logger -t "openvpn-routing" "Adding $VPN_IP to selective routing"
			fi
		fi
	done
	IFS=$OLDIFS
}

purge_client_list(){
	IP_LIST=`ip rule show|awk '$2 == "from" && $4=="lookup" && $5==ENVIRON["VPN_TBL"] {print $3}'`
	for IP in $IP_LIST
	do
		ip rule del from $IP table $VPN_TBL
		logger -t "openvpn-routing" "Removing $IP from selective routing"
	done
}

run_custom_script(){
	if [ -f /jffs/scripts/openvpn-event ]
	then
		sh /jffs/scripts/openvpn-event $*
	fi
}



if [ "$dev" == "tun11" ]
then
	VPN_IP_LIST=$(nvram get vpn_client1_clientlist)
	VPN_TBL=111
	VPN_REDIR=$(nvram get vpn_client1_rgw)
	VPN_FORCE=$(nvram get vpn_client1_enforce)
elif [ "$dev" == "tun12" ]
then
	VPN_IP_LIST=$(nvram get vpn_client2_clientlist)
	VPN_TBL=112
	VPN_REDIR=$(nvram get vpn_client2_rgw)
	VPN_FORCE=$(nvram get vpn_client2_enforce)
else
	run_custom_script
	exit 0
fi

export VPN_GW VPN_IP VPN_TBL VPN_FORCE

if [ $script_type = "rmupdate" ]
then
	logger -t "openvpn-routing" "Refreshing client rules"
	if [ $VPN_FORCE == "1" && $VPN_REDIR == "2" ]
	then
		ip route add unreachable default table $VPN_TBL
		update_client_list
	else
		purge_client_list
		ip route del unreachable default table $VPN_TBL
	fi
	ip route flush cache
	exit 0
fi

if [ $script_type == "route-up" && $VPN_REDIR != "2" ]
then
	logger -t "openvpn-routing" "Skipping, not in selective redir mode"
	run_custom_script
	exit 0
fi

logger -t "openvpn-routing" "Configuring selective routing for $dev"

if [ $script_type == "route-pre-down" ]
then

# Delete tunnel's default route
	ip route del default table $VPN_TBL

        if [ $VPN_FORCE == "1" ]
        then
# Prevent WAN access
		logger -t "openvpn-routing" "Tunnel down - VPN client access blocked"
                ip route add unreachable default table $VPN_TBL
		update_client_list
        else
# Delete rules
		purge_client_list
	fi
fi	# End route down


# Delete existing VPN routes (both up and down events)
NET_LIST=`ip route show|awk '$2=="via" && $3==ENVIRON["route_vpn_gateway"] && $4=="dev" && $5==ENVIRON["dev"] {print $1}'`
for NET in $NET_LIST
do
        ip route del $NET dev $dev
        logger -t "openvpn-routing" "Removing route for $NET to $dev"
done


if [ $script_type == "route-up" ]
then
	update_client_list

# Setup table default route
	if [ "$VPN_IP_LIST" != "" ]
	then
		if [ "$VPN_FORCE" == "1" ]
		then
			logger -t "openvpn-routing" "Tunnel re-established, restoring WAN access to clients"
			ip route del unreachable default table $VPN_TBL
		fi
		ip route add default via $route_vpn_gateway table $VPN_TBL
	fi

	if [ "$route_net_gateway" != "" ]
	then
		ip route del default
		ip route add default via $route_net_gateway
	fi
fi	# End route-up

ip route flush cache
logger -t "openvpn-routing" "Completed routing configuration"
run_custom_script

exit 0
