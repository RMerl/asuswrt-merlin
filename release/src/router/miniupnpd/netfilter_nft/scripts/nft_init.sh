#! /bin/sh

nft list table nat > /dev/null
nft_nat_exists=$?
nft list table filter > /dev/null
nft_filter_exists=$?
nft list table mangle > /dev/null
nft_mangle_exists=$?

if [ $nft_nat_exists -eq "1" ]; then
	echo "create nat"
	nft "add table nat"
fi
if [ $nft_filter_exists -eq "1" ]; then
	echo "create filter"
	nft "add table filter"
fi
if [ $nft_mangle_exists -eq "1" ]; then
	echo "create mangle"
	nft "add table mangle"
fi

nft list chain nat miniupnpd > /dev/null
nft_nat_miniupnpd_exists=$?
nft list chain nat miniupnpd-pcp-peer > /dev/null
nft_nat_miniupnpd_pcp_peer_exists=$?
nft list chain filter miniupnpd > /dev/null
nft_filter_miniupnpd_exists=$?
nft list chain mangle miniupnpd > /dev/null
nft_mangle_miniupnpd_exists=$?

if [ $nft_nat_miniupnpd_exists -eq "1" ]; then
	echo "create chain in nat"
	nft "add chain nat miniupnpd"
fi
if [ $nft_nat_miniupnpd_pcp_peer_exists -eq "1" ]; then
	echo "create pcp peer chain in nat"
	nft "add chain nat miniupnpd-pcp-peer"
fi
if [ $nft_filter_miniupnpd_exists -eq "1" ]; then
	echo "create chain in filter "
	nft "add chain filter miniupnpd"
fi
if [ $nft_mangle_miniupnpd_exists -eq "1" ]; then
	echo "create chain in mangle"
	nft "add chain mangle miniupnpd"
fi
