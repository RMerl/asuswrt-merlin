#! /sbin/nft -f

flush chain ip nat miniupnpd
flush chain ip nat miniupnpd-pcp-peer
flush chain ip filter miniupnpd
