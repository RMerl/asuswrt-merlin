6relayd - Embedded DHCPv6/RA Server & Relay

** Abstract **

6relayd is a daemon for serving and relaying IPv6 management protocols to
configure clients and downstream routers. It tries to follow the RFC 6204
requirements for IPv6 home routers.

6relayd provides server services for RA, stateless and stateful DHCPv6,
prefix delegation and can be used to relay RA, DHCPv6 and NDP between routed
(non-bridged) interfaces in case no delegated prefixes are available.

It is optimized for embedded Linux routers and compiles to <40 KB.


** Features **

1. Router Discovery support (solicitations and advertisements) with 2 modes
   server:	RD server for slave interfaces
   a) automatic detection of prefixes, delegated prefix and default routes, MTU
   b) automatic reannouncement when changes to prefixes or routes occur

   relay:	RD relay between master and slave interfaces
   a) support for rewriting announced DNS-server addresses in relay mode
   
3. DHCPv6-support with 2 modes of operation
   server:	minimalistic server mode
   a) stateless and stateful address assignment
   b) prefix delegation support
   c) dynamic reconfiguration in case prefixes change
   d) hostname detection and hosts-file creation

   relay: 	mostly standards-compliant DHCPv6-relay
   a) support for rewriting announced DNS-server addresses
   
4. Proxy for Neighbor Discovery messages (solicitations and advertisments)
   a) support for auto-learning routes to the local routing table
   b) support for marking interfaces "external" not proxying NDP for them
      and only serving NDP for DAD and for traffic to the router itself
      [Warning: you should provide additional firewall rules for security]


** Compiling **

6relayd uses cmake:
* To prepare a Makefile use:  "cmake ." 
* To build / install use: "make" / "make install" afterwards.
* To build DEB or RPM packages use: "make package" afterwards.


** Server Mode **

0. Server mode is used as a minimalistic alternative for full-blown servers
   like radvd or ISC DHCP if simplicity or a small footprint matter.
   Note: The master interface is unused in this mode. It should be set to '.'.

1. If there are non-local addresses assigned to the slave interface when a
   router solicitation is received, said prefixes are announced automatically
   for stateless autoconfiguration and also offered via stateful DHCPv6.
   If all prefixes are bigger than /64 all but the first /64 of these prefixes
   is offered via DHCPv6-PD to downstream routers.

2. If DNS servers should be announced (DHCPv6 server-mode) then a local DNS-
   proxy (e.g. dnsmasq) needs to be run on the router itself because 6relayd
   will always announce a local address as DNS-server (if not otherwise
   configured).

3. 6relayd is run with the appropriate parameters (e.g. -S . eth0).
   See 6relayd -h for command line parameters.


** Relay Mode **

0. Relay mode is used when a /64-bit IPv6-Prefix should be distributed over
   several links / isolated layer 2 domains (e.g. if no prefix delegation
   is available). In this mode NDP (namely Router Discovery and Neighbor
   Discovery) messages and DHCPv6-messages are proxied. For DHCPv6 also
   server mode can be used instead of relaying if desired.

1. When starting 6relayd it is required that the master interface - where
   IPv6-service is already provided - is configured and usable.
   
2. If the upstream router doesn't provide or announce a DNS-service that is
   reachable in an at least site-local scope, a local DNS proxy (e.g. dnsmasq)
   needs to be run and configued on the same router where 6relayd is running.
   (This step can most likely be skipped in most environments.)
   
3. 6relayd is run with the appropriate parameters (e.g. -A eth0 eth1).
   See 6relayd -h for command line paoffered.
