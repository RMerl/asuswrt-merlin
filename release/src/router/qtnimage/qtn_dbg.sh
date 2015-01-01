#!/bin/sh

while [ 1 = 1 ] ;
do
	echo "=============================="
	echo "5G channel: " `qcsapi_sockrpc get_channel wifi0`
	echo "5G bandwidth: " `qcsapi_sockrpc get_bw wifi0`
	sleep 1
	assoc_count=`qcsapi_sockrpc get_count_assoc wifi0`
	let ii=0
	while [ $ii -lt $assoc_count ] ;
	do
		echo "-----------------------"
		echo "vendor of client:" $ii " " `qcsapi_sockrpc get_vendor wifi0 $ii`
		echo "IP of client:" $ii " " `qcsapi_sockrpc get_associated_device_ip_addr wifi0 $ii`
		echo "bandwidth of client:" $ii " " `qcsapi_sockrpc get_assoc_bw wifi0 $ii`
		echo "-----------------------"
		let ++ii
	done
done

