#!/bin/sh

touch /tmp/start_router_command

if [ "$1" == "wifi_led_off" ] ; then
	killall monitor_wifi
	echo "wifi_led_off" > /tmp/start_router_command
fi
if [ "$1" == "wifi_led_on" ] ; then
	killall monitor_wifi
	monitor_wifi &
	echo "wifi_led_on" > /tmp/start_router_command
fi
if [ "$1" == "update_start_stateless_slave" ] ; then
	killall monitor_wifi
	tftp -g 1.1.1.1 -r start-stateless-slave -l /scripts/start-stateless-slave
fi
if [ "$1" == "update_stateless_slave_config" ] ; then
	killall monitor_wifi
	tftp -g 1.1.1.1 -r stateless_slave_config -l /scripts/stateless_slave_config
fi
if [ "$1" == "get_file_from_qtn" ] ; then
	tftp -p $2 -r $4 -l $3
fi
if [ "$1" == "get_syslog_from_qtn" ] ; then
	logmsg -t time `uptime`
	logmsg -t time `date`
	tftp -p $2 -r syslog.qtn -l /tmp/syslog.log
fi
if [ "$1" == "put_file_to_qtn" ] ; then
	tftp -g $2 -r $3 -l $4
fi
if [ "$1" == "run_cmd" ] ; then
	chmod a+x $2
	$2
fi
if [ "$1" == "run_start_stateless_slave" ] ; then
	killall monitor_wifi
	start-stateless-slave &
fi
if [ "$1" == "wpa_cli_reconfigure" ] ; then
	wpa_cli reconfigure &
fi
if [ "$1" == "set_bf_on" ] ; then
	bfon &
fi
if [ "$1" == "set_bf_off" ] ; then
	bfoff &
fi
if [ "$1" == "80211h_on" ] ; then
	echo "on" > /tmp/80211h
	iwpriv wifi0 pc_override 1 &
fi
if [ "$1" == "80211h_off" ] ; then
	echo "off" > /tmp/80211h
	iwpriv wifi0 pc_override 0 &
fi

eth1_1_speed=`cat /sys/class/net/eth1_1/speed`
if [ "$1" == "get_eth_1000m" ] ; then
        if [ "$eth1_1_speed" == "1000" ] ; then
                return 0;
        else
                return 1;
        fi
fi
if [ "$1" == "get_eth_100m" ] ; then
        if [ "$eth1_1_speed" == "100" ] ; then
                return 0;
        else
                return 1;
        fi
fi
if [ "$1" == "get_eth_10m" ] ; then
        if [ "$eth1_1_speed" == "100" ] ; then
                return 0;
        else
                return 1;
        fi
fi

if [ "$1" == "del_cal_files" ] ; then
	/scripts/delete_dciq_cal_bootcfg
	/scripts/delete_pd_power_cal
	/scripts/delete_rxiq_cal
	/scripts/delete_txpower_cal_bootcfg
	return 0;
fi

if [ "$1" == "enable_telnet_srv" ] ; then
        if [ "$2" == "0" ] ; then
		echo "Telnet disabled"
		echo -n "Stopping inetd ..."
		killall -q inetd
		ifconfig br0:0 down
        else
		echo -n "Stopping inetd ..."
		killall -q inetd
		sleep 3
		echo "Starting inetd ..."
		/usr/sbin/inetd
		ifconfig br0:0 1.1.1.2 netmask 255.255.255.0
	fi
	return 0;
fi

if [ "$1" == "update_router_command" ] ; then
	cd /scripts ; rm ./router_command.sh ; tftp -g 1.1.1.1 -l router_command.sh -r router_command.sh ; chmod a+x /scripts/router_command.sh
	return 0;
fi

if [ "$1" == "lan4_led_ctrl" ]; then
	if [ "$2" == "on" ] ; then
	echo 'w 0x1f 0x7' > /proc/phy_reg0
	echo 'w 0x1e 0x2c' > /proc/phy_reg0
	echo 'w 0x1a 0x70' > /proc/phy_reg0
	echo 'w 0x1c 0x777' > /proc/phy_reg0
	echo 'w 0x1f 0x0' > /proc/phy_reg0
		else
	echo 'w 0x1f 0x7' > /proc/phy_reg0
	echo 'w 0x1e 0x2c' > /proc/phy_reg0
	echo 'w 0x1a 0x0' > /proc/phy_reg0
	echo 'w 0x1c 0x0' > /proc/phy_reg0
	echo 'w 0x1f 0x0' > /proc/phy_reg0
	fi
	exit 0
fi

if [ "$1" == "diagnostics" ]; then
	sed -i "s/QTN_RPC_CLIENT/${2}/g" /scripts/gather_info
	gather_info &
fi

if [ "$1" == "sync_time" ]; then
	date -s ${2}
fi

exit 0
