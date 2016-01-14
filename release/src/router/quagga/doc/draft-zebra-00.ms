.pl 10.0i
.po 0
.ll 7.2i
.lt 7.2i
.nr LL 7.2i
.nr LT 7.2i
.ds LF Ishiguro
.ds RF FORMFEED[Page %]
.ds CF
.ds LH RFC DRAFT
.ds RH March 1998
.ds CH
.hy 0
.ad l
Network Working Group                                        K. Ishiguro
Request for Comments: DRAFT                     Digital Magic Labs, Inc.
                                                              March 1998
.sp 2
.ce
Zebra Protocol Draft
.sp 2
.fi
.ne 4
Status of this Memo
.sp
.in 3
This draft is very eary beta version.
.sp
.in 0
.ne 4
Introduction
.sp
.in 3
The zebra protocol is a communication protocol between kernel
routing table manager and routing protocol daemon. It is built over
TCP/IP protocol suite.
.sp
.in 0
.ne 4
Request message formats
.sp
.in 3
zebra is TCP-based protocol.
.sp
Below is request packet format.
.sp
.in 0
.DS
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Length (2)          |   Command (1) |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
.DE
.sp
.in 3
Length is total packet length.
.sp
Here is summary of command list.
.sp
.in 0
.DS
1 - ZEBRA_IPV4_ROUTE_ADD
2 - ZEBRA_IPV4_ROUTE_DELETE
3 - ZEBRA_IPV6_ROUTE_ADD
4 - ZEBRA_IPV6_ROUTE_DELETE
5 - ZEBRA_GET_ONE_INTERFACE
6 - ZEBRA_GET_ALL_INTERFACE
7 - ZEBRA_GET_HOSTINFO
.DE
.sp
.in 0
.ne 4
IPv4 reply message formats
.sp
.in 0
.DS
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+
|    Type (1)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Gateway (4)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
.DE
.sp
.in 3
Type field specify route's origin type.
.sp
.in 0
.DS
1 - ZEBRA_ROUTE_RESERVE
2 - ZEBRA_ROUTE_CONNECT
3 - ZEBRA_ROUTE_STATIC
4 - ZEBRA_ROUTE_RIP
5 - ZEBRA_ROUTE_RIPNG
6 - ZEBRA_ROUTE_BGP
7 - ZEBRA_ROUTE_RADIX
.DE
.sp
.in 3
After above message there can be variale length IPv4 prefix data.
Each IPv4 prefix is encoded as a two tuple of the form <masklength,
prefix>
.sp
.in 0
.DS
+----------------------+
|Subnet mask (1 octet) |
+----------------------+
|IPv4 prefix (variable)|
+----------------------+
.DE
.sp
.in 0
.ne 4
IPv6 reply message formats
.sp
.in 0
.DS
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+
|    Type (1)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Gateway (16)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
.DE
.sp
.in 3
Type field specify route's origin type.
.sp
.in 0
.DS
1 - ZEBRA_ROUTE_RESERVE
2 - ZEBRA_ROUTE_CONNECT
3 - ZEBRA_ROUTE_STATIC
4 - ZEBRA_ROUTE_RIP
5 - ZEBRA_ROUTE_RIPNG
6 - ZEBRA_ROUTE_BGP
7 - ZEBRA_ROUTE_RADIX
.DE
.sp
.in 0
.DS
+----------------------+
|  ifindex   (4 octet) |
+----------------------+
|  prefixlen  (1 octet)|
+----------------------+
|IPv6 prefix (variable)|
+----------------------+
.DE
.sp
.in 3
I am not sure but it seems some operation systems IPv6
implementation may need interface index when add and delete
linklocal routes.
.sp
I have added ifindex field to specify IPv6 routes interface
index. If this index is value zero, it will ignored.
.sp
.in 0
.ne 4
Interface information message format.
.sp
.in 0
.DS
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Interface name (20)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Index (1)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Inteface flag (4)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Inteface metric (4)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Inteface MTU (4)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Inteface Address count (4)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
.DE
.sp
.in 3
Address message format.
.sp
.in 0
.ne 4
Host inforamtion message format.
.sp
.in 0
.DS
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|IPv4 forwarding|IPv6 forwarding|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
.DE
.sp
.in 3
Host information contain IPv4/IPv6 forwarding information.
