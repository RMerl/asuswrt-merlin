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
exit 0
