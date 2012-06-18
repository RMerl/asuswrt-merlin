## Process this file with automake to produce Makefile.in.

info_TEXINFOS = zebra.texi

zebra_TEXINFOS = appendix.texi basic.texi bgpd.texi filter.texi install.texi \
	ipv6.texi kernel.texi main.texi ospf6d.texi ospfd.texi overview.texi \
	protocol.texi ripd.texi ripngd.texi routemap.texi snmp.texi vtysh.texi

man_MANS = vtysh.1 bgpd.8 ospf6d.8 ospfd.8 ripd.8 ripngd.8 zebra.8

EXTRA_DIST = BGP-TypeCode draft-zebra-00.ms draft-zebra-00.txt $(man_MANS) 

draft-zebra-00.txt:
	groff -T ascii -ms draft-zebra-00.ms > draft-zebra-00.txt
