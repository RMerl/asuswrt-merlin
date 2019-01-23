#!/bin/sh

PARAM=$*
if [ "$PARAM" == "" ]
then
	# Add paramaters equivalent to those passed for up command
	PARAM="$dev $tun_mtu $link_mtu $ifconfig_local $ifconfig_remote"
fi

my_logger(){
	if [ "$VPN_LOGGING" -gt "3" ]
	then
		logger -t "openvpn-routing" "$1"
	fi
}


create_client_list(){
	OLDIFS=$IFS
	IFS="<"

	for ENTRY in $VPN_IP_LIST
	do
		if [ "$ENTRY" = "" ]
		then
			continue
		fi
		TARGET_ROUTE=$(echo $ENTRY | cut -d ">" -f 4)
		if [ "$TARGET_ROUTE" = "WAN" ]
		then
			TARGET_LOOKUP="main"
			WAN_PRIO=$((WAN_PRIO+1))
			RULE_PRIO=$WAN_PRIO
			TARGET_NAME="WAN"
		else
			TARGET_LOOKUP=$VPN_TBL
			VPN_PRIO=$((VPN_PRIO+1))
			RULE_PRIO=$VPN_PRIO
			TARGET_NAME="VPN client "$VPN_UNIT
		fi
		VPN_IP=$(echo $ENTRY | cut -d ">" -f 2)
		if [ "$VPN_IP" != "0.0.0.0" ]
		then
			SRCC="from"
			SRCA="$VPN_IP"
		else
			SRCC=""
			SRCA=""
		fi
		DST_IP=$(echo $ENTRY | cut -d ">" -f 3)
		if [ "$DST_IP" != "0.0.0.0" ]
		then
			DSTC="to"
			DSTA="$DST_IP"
		else
			DSTC=""
			DSTA=""
		fi
		if [ "$SRCC" != "" -o "$DSTC" != "" ]
		then
			ip rule add $SRCC $SRCA $DSTC $DSTA table $TARGET_LOOKUP priority $RULE_PRIO
			my_logger "Adding route for $VPN_IP to $DST_IP through $TARGET_NAME"
		fi
	done
	IFS=$OLDIFS
}

purge_client_list(){
	IP_LIST=$(ip rule show | cut -d ":" -f 1)
	for PRIO in $IP_LIST
	do
		if [ $PRIO -ge $START_PRIO -a $PRIO -le $END_PRIO ]
		then
			ip rule del prio $PRIO
			my_logger "Removing rule $PRIO from routing policy"
		fi
	done
}

run_custom_script(){
	if [ -f /jffs/scripts/openvpn-event ]
	then
		logger -t "custom_script" "Running /jffs/scripts/openvpn-event (args: $PARAM)"
		sh /jffs/scripts/openvpn-event $PARAM
	fi
}

init_table(){
	my_logger "Creating VPN routing table (mode $VPN_REDIR)"
	ip route flush table $VPN_TBL

# Fill it with copy of existing main table
	if [ "$VPN_REDIR" == "3" ]
	then
		LANIFNAME=$(nvram get lan_ifname)
		ip route show table main dev $LANIFNAME | while read ROUTE
		do
			ip route add table $VPN_TBL $ROUTE dev $LANIFNAME
		done
		ip route show table main dev $dev | while read ROUTE
		do
			ip route add table $VPN_TBL $ROUTE dev $dev
		done
	elif [ "$VPN_REDIR" == "2" ]
	then
		ip route show table main | while read ROUTE
		do
			ip route add table $VPN_TBL $ROUTE
		done
	fi
}

# Begin
if [ "$dev" == "tun11" ]
then
	VPN_IP_LIST=$(nvram get vpn_client1_clientlist)
	VPN_REDIR=$(nvram get vpn_client1_rgw)
	VPN_FORCE=$(nvram get vpn_client1_enforce)
	VPN_UNIT=1
	VPN_LOGGING=$(nvram get vpn_client1_verb)
elif [ "$dev" == "tun12" ]
then
	VPN_IP_LIST=$(nvram get vpn_client2_clientlist)
	VPN_REDIR=$(nvram get vpn_client2_rgw)
	VPN_FORCE=$(nvram get vpn_client2_enforce)
	VPN_UNIT=2
	VPN_LOGGING=$(nvram get vpn_client2_verb)
elif [ "$dev" == "tun13" ]
then
	VPN_IP_LIST=$(nvram get vpn_client3_clientlist)
	VPN_REDIR=$(nvram get vpn_client3_rgw)
	VPN_FORCE=$(nvram get vpn_client3_enforce)
	VPN_UNIT=3
	VPN_LOGGING=$(nvram get vpn_client3_verb)
elif [ "$dev" == "tun14" ]
then
	VPN_IP_LIST=$(nvram get vpn_client4_clientlist)
	VPN_REDIR=$(nvram get vpn_client4_rgw)
	VPN_FORCE=$(nvram get vpn_client4_enforce)
	VPN_UNIT=4
	VPN_LOGGING=$(nvram get vpn_client4_verb)
elif [ "$dev" == "tun15" ]
then
	VPN_IP_LIST=$(nvram get vpn_client5_clientlist)
	VPN_REDIR=$(nvram get vpn_client5_rgw)
	VPN_FORCE=$(nvram get vpn_client5_enforce)
	VPN_UNIT=5
	VPN_LOGGING=$(nvram get vpn_client5_verb)
else
	run_custom_script
	exit 0
fi

VPN_TBL="ovpnc"$VPN_UNIT
START_PRIO=$((10000+(200*($VPN_UNIT-1))))
END_PRIO=$(($START_PRIO+199))
WAN_PRIO=$START_PRIO
VPN_PRIO=$(($START_PRIO+100))

export VPN_GW VPN_IP VPN_TBL VPN_FORCE


# webui reports that vpn_force changed while vpn client was down
if [ $script_type = "rmupdate" ]
then
	my_logger "Refreshing policy rules for client $VPN_UNIT"
	purge_client_list

	if [ $VPN_FORCE == "1" -a $VPN_REDIR -ge "2" ]
	then
		init_table
		my_logger "Tunnel down - VPN client access blocked"
		ip route del default table $VPN_TBL
		ip route add prohibit default table $VPN_TBL
		create_client_list
	else
		my_logger "Allow WAN access to all VPN clients"
		ip route flush table $VPN_TBL
	fi
	ip route flush cache
	exit 0
fi

if [ $script_type == "route-up" -a $VPN_REDIR -lt "2" ]
then
	my_logger "Skipping, client $VPN_UNIT not in routing policy mode"
	run_custom_script
	exit 0
fi

logger -t "openvpn-routing" "Configuring policy rules for client $VPN_UNIT"

if [ $script_type == "route-pre-down" ]
then
	purge_client_list

	if [ $VPN_FORCE == "1" -a $VPN_REDIR -ge "2" ]
	then
		logger -t "openvpn-routing" "Tunnel down - VPN client access blocked"
		ip route change prohibit default table $VPN_TBL
		create_client_list
	else
		ip route flush table $VPN_TBL
		my_logger "Flushing client routing table"
	fi
fi	# End route down



if [ $script_type == "route-up" ]
then
	init_table

# Delete existing VPN routes that were pushed by server on table main
	NET_LIST=$(ip route show|awk '$2=="via" && $3==ENVIRON["route_vpn_gateway"] && $4=="dev" && $5==ENVIRON["dev"] {print $1}')
	for NET in $NET_LIST
	do
		ip route del $NET dev $dev
		my_logger "Removing route for $NET to $dev from main routing table"
	done

# Update policy rules
        purge_client_list
        create_client_list

# Setup table default route
	if [ "$VPN_IP_LIST" != "" ]
	then
		if [ "$VPN_FORCE" == "1" ]
		then
			logger -t "openvpn-routing" "Tunnel re-established, restoring WAN access to clients"
		fi
		if [ "$route_net_gateway" != "" ]
		then
			ip route del default table $VPN_TBL
			ip route add default via $route_vpn_gateway table $VPN_TBL
		else
			logger -t "openvpn-routing" "WARNING: no VPN gateway provided, routing might not work properly!"
		fi
	fi

	if [ "$route_net_gateway" != "" ]
	then
		ip route del default
		ip route add default via $route_net_gateway
	fi
fi	# End route-up

ip route flush cache
my_logger "Completed routing policy configuration for client $VPN_UNIT"
run_custom_script

exit 0
