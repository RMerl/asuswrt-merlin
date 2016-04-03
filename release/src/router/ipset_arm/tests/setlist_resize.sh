#!/bin/sh

# set -x

ipset=${IPSET_BIN:-../src/ipset}

loop=8

for x in ip_set_list_set ip_set_hash_netiface ip_set_hash_ipportnet \
	 ip_set_hash_netport ip_set_hash_net ip_set_hash_ipportip \
	 ip_set_hash_ipport ip_set_hash_ip ip_set_hash_netnet \
	 ip_set_hash_netportnet ip_set_hash_ipmark ip_set_hash_mac \
	 ip_set_bitmap_port ip_set_bitmap_ipmac \
	 ip_set_bitmap_ip xt_set ip_set; do
    rmmod $x
done

create() {
    n=$1
    while [ $n -le 1024 ]; do
      $ipset c test$n hash:ip
    	n=$((n+2))
    done
}

for x in `seq 1 $loop`; do
    # echo "test round $x"
    create 1 &
    create 2 &
    wait
    test `$ipset l -n | wc -l` -eq 1024 || exit 1
    $ipset x
    test `lsmod|grep -w ^ip_set_hash_ip | awk '{print $3}'` -eq 0 || exit 1
    rmmod ip_set_hash_ip
    rmmod ip_set
done
