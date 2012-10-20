/*

	Tomato Firmware
	Copyright (C) 2006-2007 Jonathan Zarate
	$Id: qos.c 241383 2011-02-18 03:30:06Z stakita $

*/
#include "rc.h"
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <shared.h>

static const char *qosfn = "/tmp/qos";
static const char *mangle_fn = "/tmp/mangle_rules";
#ifdef RTCONFIG_IPV6
static const char *mangle_fn_ipv6 = "/tmp/mangle_rules_ipv6";
#endif
static unsigned calc(unsigned bw, unsigned pct)
{
	unsigned n = ((unsigned long)bw * pct) / 100;
	return (n < 2) ? 2 : n;
}


#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
int etable_flag = 0;
void del_EbtablesRules(void)
{
	/* Flush all rules in nat table of ebtable*/
	eval("ebtables", "-t", "nat", "-F");
	etable_flag = 0;
}

void add_EbtablesRules(void)
{
	if(etable_flag == 1) return;
	char *nv, *p, *g;
	nv = g = strdup(nvram_safe_get("wl_ifnames"));
	if(nv){
		while ((p = strsep(&g, " ")) != NULL){
			//fprintf(stderr, "%s: g=%s, p=%s\n", __FUNCTION__, g, p); //tmp test
			eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", p, "-j", "mark", "--set-mark", "6", "--mark-target", "ACCEPT");
			eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", p, "-j", "mark", "--set-mark", "6", "--mark-target", "ACCEPT");
		}
		free(nv);
	}

	// for MultiSSID
	int UnitNum = 2; 	// wl0.x, wl1.x
	int GuestNum = 3;	// wlx.0, wlx.1, wlx.2
	char mssid_if[32];
	char mssid_enable[32];
	int i, j;
	for( i = 0; i < UnitNum; i++){
		for( j = 1; j <= GuestNum; j++ ){
			snprintf(mssid_if, sizeof(mssid_if), "wl%d.%d", i, j);
			snprintf(mssid_enable, sizeof(mssid_enable), "%s_bss_enabled", mssid_if);
			//fprintf(stderr, "%s: mssid_enable=%s\n", __FUNCTION__, mssid_enable); //tmp test
			if(!strcmp(nvram_safe_get(mssid_enable), "1")){
				eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", mssid_if, "-j", "mark", "--set-mark", "6", "--mark-target", "ACCEPT");
				eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", mssid_if, "-j", "mark", "--set-mark", "6", "--mark-target", "ACCEPT");
			}
		}
	}

	etable_flag = 1;
}
#endif

void del_iQosRules(void)
{
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	del_EbtablesRules(); // flush ebtables nat table
#endif

	/* Flush all rules in mangle table */
	eval("iptables", "-t", "mangle", "-F");
#ifdef RTCONFIG_IPV6
	eval("ip6tables", "-t", "mangle", "-F");
#endif
}

int add_iQosRules(char *pcWANIF)
{
	FILE *fn;
#ifdef RTCONFIG_IPV6
	FILE *fn_ipv6;
#endif
	char *buf;
	char *g;
	char *p;
	char *desc, *addr, *port, *prio, *transferred, *proto;
	int class_num;
	int down_class_num=6; // for download class_num = 0x6 / 0x106
	int proto_num;
	int i, inuse;
	char q_inuse[32]; // for inuse 
	char dport[192], saddr[192], conn[256], end[256];
	int method;
	int gum;
	int sticky_enable;
	const char *chain;
	int v4v6_ok;

	if (pcWANIF == NULL || !nvram_match("qos_enable", "1")) return -1;
	if ((fn = fopen(mangle_fn, "w")) == NULL) return -2;
#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && (fn_ipv6 = fopen(mangle_fn_ipv6, "w")) == NULL) return -3;
#endif

	inuse = sticky_enable = 0;
	
	if (nvram_match("qos_sticky", "0"))
		sticky_enable = 1;

	del_iQosRules(); // flush all rules in mangle table
	fprintf(stderr, "[qos] iptables START\n");

	fprintf(fn, 
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		":QOSO - [0:0]\n"
		"-A QOSO -m mark --mark 0xd001 -j RETURN\n"
		"-A QOSO -j CONNMARK --restore-mark --mask 0xff\n"
		"-A QOSO -m connmark ! --mark 0/0xff00 -j RETURN\n"
		);
#ifdef RTCONFIG_IPV6
	if (ipv6_enabled())
	fprintf(fn_ipv6,
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		":QOSO - [0:0]\n"
		"-A QOSO -j CONNMARK --restore-mark --mask 0xff\n"
		"-A QOSO -m connmark ! --mark 0/0xff00 -j RETURN\n"
		);
#endif
	g = buf = strdup(nvram_safe_get("qos_rulelist"));
	while (g) {
		
		/* ASUSWRT 
  		qos_rulelist : 
			desc>addr>port>proto>transferred>prio
			
			addr  : source ip or mac
			port  : dest port
			proto : tcp, udp, tcp/udp, any , (icmp, igmp)
			transferred : min:max
		 	prio  : 0-4, 0 is the highest		
  		*/
	
		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;
		class_num = atoi(prio);
		if ((class_num < 0) || (class_num > 4)) continue;

		i = 1 << class_num;
		++class_num;
		
		//if (method == 1) class_num |= 0x200;
		if ((inuse & i) == 0) {
			inuse |= i;
			fprintf(stderr, "[qos] iptable creates, inuse=%d\n", inuse);
		}

		v4v6_ok = IPT_V4;
#ifdef RTCONFIG_IPV6
		if (ipv6_enabled())
			v4v6_ok |= IPT_V6;
#endif

		/* Beginning of the Rule */
		/* 
 			if transferred != NULL, class_num must bt 0x1~0x6, not 0x101~0x106
			0x1~0x6		: keep tracing this connection.
			0x101~0x106 	: connection will be considered as marked connection, won't detect again.
		*/
#if 0
		if(strcmp(transferred, "") != 0 ) 
			method = 1;
		else
			method = atoi(nvram_safe_get("qos_method"));	// strict rule ordering
		gum = (method == 0) ? 0x100 : 0;
#else
		method = 1;
		gum = 0;
#endif     
	        class_num |= gum;
		down_class_num |= gum; // for download

		chain = "QOSO";                     // chain name
                sprintf(end , " -j CONNMARK --set-return 0x%x/0xFF\n", class_num);	// CONNMARK string

		/*************************************************/
		/*                        addr                   */
		/*                 src mac or src ip             */
		/*************************************************/
		char tmp[20];
		char *tmp_addr, *q_ip, *q_mac; 

		sprintf(tmp, "%s", addr);
		tmp_addr = tmp;
		q_ip  = strsep(&tmp_addr, ":");
		q_mac = tmp_addr;

		if(strcmp(addr, "") == 0 ) 
			sprintf(saddr, "%s", addr);      // src addr
		else{
			if (q_mac == NULL){
				sprintf(saddr, "-s %s", addr);      // src addr

				v4v6_ok &= ipt_addr_compact(addr, v4v6_ok, (v4v6_ok==IPT_V4));
				if (!v4v6_ok) continue;
			}
			else{
				sprintf(saddr, "-m mac --mac-source %s", addr); // src mac
			}
		}
		//fprintf(stderr, "[qos] tmp=%s, ip=%s, mac=%s, addr=%s\n", tmp, q_ip, q_mac, addr ); // tmp test
		
		/*************************************************/
		/*                      port                     */
		/*            single port or multi-ports         */
		/*************************************************/
		char *tmp_port, *q_port, *q_leave;

		sprintf(tmp, "%s", port);
		tmp_port = tmp;
		q_port = strsep(&tmp_port, ",");
		q_leave = tmp_port;

		if(strcmp(port, "") == 0 ){
			sprintf(dport, "");
		}
		else{
			if(q_leave != NULL) 
				sprintf(dport, "-m multiport --dport %s", port); // multi port
			else
				sprintf(dport, "--dport %s", port); // single port
		}
		//fprintf(stderr, "[qos] tmp=%s, q_port=%s, q_leave=%s, port=%s\n", tmp, q_port, q_leave, port ); // tmp test

		/*************************************************/
		/*                   transferred                  */
		/*   --connbytes min:max                         */
 		/*   --connbytes-dir (original/reply/both)       */
 		/*   --connbytes-mode (packets/bytes/avgpkt)     */
		/*************************************************/
		char *tmp_trans, *q_min, *q_max;
		long min, max ;	
	
		sprintf(tmp, "%s", transferred);
		tmp_trans = tmp;
		q_min = strsep(&tmp_trans, "~");
		q_max = tmp_trans;

		if (strcmp(transferred,"") == 0){
			sprintf(conn, ""); 
		}
		else{
			sprintf(tmp, "%s", q_min);
			min = atol(tmp);

			if(strcmp(q_max,"") == 0) // q_max == NULL
				sprintf(conn, "-m connbytes --connbytes %ld:%s --connbytes-dir both --connbytes-mode bytes", min*1024, q_max);
			else{// q_max != NULL
				sprintf(tmp, "%s", q_max);
				max = atol(tmp);
				sprintf(conn, "-m connbytes --connbytes %ld:%ld --connbytes-dir both --connbytes-mode bytes", min*1024, max*1024-1);
			}
		}
		//fprintf(stderr, "[qos] tmp=%s, transferred=%s, min=%ld, max=%ld, q_max=%s, conn=%s\n", tmp,  transferred, min*1024, max*1024-1, q_max, conn ); // tmp test
	

		/*************************************************/
		/*                      proto                    */
		/*        tcp, udp, tcp/udp, any, (icmp, igmp)   */
		/*************************************************/
		if(strcmp(proto, "tcp") == 0 )
		{
			if (v4v6_ok & IPT_V4)
			fprintf(fn, "-A %s -p %s %s %s %s %s", chain, "tcp", dport, saddr, conn, end);
#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && (v4v6_ok & IPT_V6))
			fprintf(fn_ipv6, "-A %s -p %s %s %s %s %s", chain, "tcp", dport, saddr, conn, end);
#endif
		}
		else if(strcmp(proto, "udp") == 0 )
		{
			if (v4v6_ok & IPT_V4)
			fprintf(fn, "-A %s -p %s %s %s %s %s", chain, "udp", dport, saddr, conn, end);
#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && (v4v6_ok & IPT_V6))
			fprintf(fn_ipv6, "-A %s -p %s %s %s %s %s", chain, "udp", dport, saddr, conn, end);
#endif
		}
		//else if(strcmp(proto, "icmp") == 0 || strcmp(proto, "igmp") == 0  )
		else if(strcmp(proto, "any") == 0)
		{
			if (v4v6_ok & IPT_V4)
			fprintf(fn, "-A %s %s %s %s", chain, saddr, conn, end);
#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && (v4v6_ok & IPT_V6))
			fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr, conn, end);
#endif
		}
		else if(strcmp(proto, "tcp/udp") == 0 ){
			if (v4v6_ok & IPT_V4)
			{
				fprintf(fn, "-A %s -p %s %s %s %s %s", chain, "tcp", dport, saddr, conn, end);
				fprintf(fn, "-A %s -p %s %s %s %s %s", chain, "udp", dport, saddr, conn, end);
			}
#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && (v4v6_ok & IPT_V6))
			{
				fprintf(fn_ipv6, "-A %s -p %s %s %s %s %s", chain, "tcp", dport, saddr, conn, end);
				fprintf(fn_ipv6, "-A %s -p %s %s %s %s %s", chain, "udp", dport, saddr, conn, end);
			}
#endif
		}
		else
			fprintf(stderr, "[qos] proto doesn't exist!!\n");
		
		//fprintf(stderr,"[qos] -A %s -p %s %s %s %s %s", chain, "tcp", port, saddr, conn, end); //tmp test

	}
	free(buf);

	/* lan_addr for iptables use (LAN download) */
	char *a, *b, *c, *d;
	char lan_addr[20];
	g = buf = strdup(nvram_safe_get("lan_ipaddr"));
	if((vstrsep(g, ".", &a, &b, &c, &d)) != 4){
		fprintf(stderr,"[qos] lan_ipaddr doesn't exist!!\n");
	}
	else{
		//fprintf(stderr,"[qos] lan_ipaddr exist!!\n");
		sprintf(lan_addr, "%s.%s.%s.0/24", a, b, c);
		fprintf(stderr,"[qos] lan_addr=%s\n", lan_addr);
	}
	free(buf);

	//fprintf(stderr, "[qos] down_class_num=%x\n", down_class_num);

	/* The default class */
        i = nvram_get_int("qos_default");
        if ((i < 0) || (i > 4)) i = 3;  // "lowest"
        class_num = i + 1;

#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	if(strncmp(pcWANIF, "ppp", 3)==0){
		// ppp related interface doesn't need physdev
		// do nothing
	}
	else{
		/* for WLAN to LAN bridge packet */
		// ebtables : identify bridge packet
		add_EbtablesRules();

		// for multicast
		fprintf(fn, "-A QOSO -d 224.0.0.0/4 -j CONNMARK --set-return 0x%x/0xFF\n",  down_class_num);
		// for download (LAN or wireless)
		fprintf(fn, "-A QOSO -d %s -j CONNMARK --set-return 0x%x/0xFF\n", lan_addr, down_class_num);
/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
		// for WLAN to LAN bridge issue
		fprintf(fn, "-A POSTROUTING -d %s -m physdev --physdev-is-in -j CONNMARK --set-return 0x6/0xFF\n", lan_addr);
*/
		// for download, interface br0
		fprintf(fn, "-A POSTROUTING -o br0 -j QOSO\n");
	}
#endif
		fprintf(fn,
			"-A QOSO -j CONNMARK --set-return 0x%x\n"
	                "-A FORWARD -o %s -j QOSO\n"
        	        "-A OUTPUT -o %s -j QOSO\n",
                	        class_num, pcWANIF, pcWANIF);

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && *wan6face) {
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
		if(strncmp(wan6face, "ppp", 3)==0){
			// ppp related interface doesn't need physdev
			// do nothing
		}
		else{
			/* for WLAN to LAN bridge packet */
			// ebtables : identify bridge packet
			add_EbtablesRules();

			// for multicast
			fprintf(fn_ipv6, "-A QOSO -d 224.0.0.0/4 -j CONNMARK --set-return 0x%x/0xFF\n",  down_class_num);
			// for download (LAN or wireless)
        		fprintf(fn_ipv6, "-A QOSO -d %s -j CONNMARK --set-return 0x%x/0xFF\n", lan_addr, down_class_num);
/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
			// for WLAN to LAN bridge issue
			fprintf(fn_ipv6, "-A POSTROUTING -d %s -m physdev --physdev-is-in -j CONNMARK --set-return 0x6/0xFF\n", lan_addr);
*/
			// for download, interface br0
			fprintf(fn_ipv6, "-A POSTROUTING -o br0 -j QOSO\n");
		}
#endif
        	fprintf(fn_ipv6,
                	"-A QOSO -j CONNMARK --set-return 0x%x\n"
                	"-A FORWARD -o %s -j QOSO\n"
                	"-A OUTPUT -o %s -j QOSO\n",
                        	class_num, wan6face, wan6face);
	}
#endif

        inuse |= (1 << i) | 1;  // default and highest are always built
        sprintf(q_inuse, "%d", inuse);
        nvram_set("qos_inuse", q_inuse);
	fprintf(stderr, "[qos] qos_inuse=%d\n", inuse);

	/* Ingress rules */
        g = buf = strdup(nvram_safe_get("qos_irates"));
        for (i = 0; i < 10; ++i) {
                if ((!g) || ((p = strsep(&g, ",")) == NULL)) continue;
                if ((inuse & (1 << i)) == 0) continue;
                if (atoi(p) > 0) {
                        fprintf(fn, "-A PREROUTING -i %s -j CONNMARK --restore-mark --mask 0xff\n", pcWANIF);
#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && *wan6face)
			fprintf(fn_ipv6, "-A PREROUTING -i %s -j CONNMARK --restore-mark --mask 0xff\n", wan6face);
#endif
                        break;
                }
        }
        free(buf);

	fprintf(fn, "COMMIT\n");
	fclose(fn);
	chmod(mangle_fn, 0700);
	eval("iptables-restore", "/tmp/mangle_rules");
#ifdef RTCONFIG_IPV6
	if (ipv6_enabled())
	{
		fprintf(fn_ipv6, "COMMIT\n");
		fclose(fn_ipv6);
		chmod(mangle_fn_ipv6, 0700);
		eval("ip6tables-restore", "/tmp/mangle_rules_ipv6");
	}
#endif
	fprintf(stderr, "[qos] iptables DONE!\n");

	return 0;
}


/*******************************************************************/
// The definations of all partations
// eth0 : WAN
// 1:1  : upload   
// 1:2  : download   (1000000Kbits)
// 1:10 : highest
// 1:20 : high
// 1:30 : middle
// 1:40 : low        (default)
// 1:50 : lowest
// 1:60 : ALL Download (WAN to LAN and LAN to LAN) (1000000kbits)
/*******************************************************************/

/* Tc */
int start_iQos(void)
{
	int i;
	char *buf, *g, *p;
	unsigned int rate;
	unsigned int ceil;
	unsigned int ibw, obw, bw;
	unsigned int mtu;
	FILE *f;
	int x;
	int inuse;
	char s[256];
	int first;
	char burst_root[32];
	char burst_leaf[32];
	char *protocol;

	// judge interface by get_wan_ifname
	// add Qos iptable rules in mangle table,
	// move it to firewall - mangle_setting
	// add_iQosRules(get_wan_ifname(0)); // iptables start

	if (!nvram_match("qos_enable", "1")) return -1;

	ibw = strtoul(nvram_safe_get("qos_ibw"), NULL, 10);
	obw = strtoul(nvram_safe_get("qos_obw"), NULL, 10);
	if(ibw==0||obw==0) return -1;

	if ((f = fopen(qosfn, "w")) == NULL) return -2;

	fprintf(stderr, "[qos] tc START!\n");

	/* qos_burst */
	i = atoi(nvram_safe_get("qos_burst0"));
	if (i > 0) sprintf(burst_root, "burst %dk", i);
		else burst_root[0] = 0;
	i = atoi(nvram_safe_get("qos_burst1"));

	if (i > 0) sprintf(burst_leaf, "burst %dk", i);
		else burst_leaf[0] = 0;

	/* Egress OBW  -- set the HTB shaper (Classful Qdisc)  
	* the BW is set here for each class 
	*/

	mtu = strtoul(nvram_safe_get("wan_mtu"), NULL, 10);
	bw = obw;

	/* WAN */
	fprintf(f,
		"#!/bin/sh\n"
		"#LAN/WAN\n"
		"I=%s\n"
		"SFQ=\"sfq perturb 10\"\n"
		"TQA=\"tc qdisc add dev $I\"\n"
		"TCA=\"tc class add dev $I\"\n"
		"TFA=\"tc filter add dev $I\"\n"
		"case \"$1\" in\n"
		"start)\n"
		"#LAN/WAN\n"
		"\ttc qdisc del dev $I root 2>/dev/null\n"
		"\t$TQA root handle 1: htb default %u\n"
		"# upload 1:1\n"
		"\t$TCA parent 1: classid 1:1 htb rate %ukbit ceil %ukbit %s\n",
			get_wan_ifname(0), // judge WAN interface 
			(atoi(nvram_safe_get("qos_default")) + 1) * 10,	
			bw, bw, burst_root);

	/* LAN protocol: 802.1q */
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	protocol = strdup("802.1q");
	fprintf(f,
		"# download 1:2\n"
		"\t$TCA parent 1: classid 1:2 htb rate 1000000kbit ceil 1000000kbit burst 10000 cburst 10000\n"
		"# 1:60 ALL Download for BCM\n"
		"\t$TCA parent 1:2 classid 1:60 htb rate 1000000kbit ceil 1000000kbit burst 10000 cburst 10000 prio 6\n"
		"\t$TQA parent 1:60 handle 60: pfifo\n"
		"\t$TFA parent 1: prio 6 protocol %s handle 6 fw flowid 1:60\n", protocol
		);
#endif

	inuse = atoi(nvram_safe_get("qos_inuse"));

	g = buf = strdup(nvram_safe_get("qos_orates"));
	for (i = 0; i < 5; ++i) { // 0~4 , 0:highest, 4:lowest

		if ((!g) || ((p = strsep(&g, ",")) == NULL)) break;

		if ((inuse & (1 << i)) == 0){
			fprintf(stderr, "[qos] egress %d doesn't create, inuse=%d\n", i, inuse );
			continue;
		}
		else
			fprintf(stderr, "[qos] egress %d creates\n", i);

		if ((sscanf(p, "%u-%u", &rate, &ceil) != 2) || (rate < 1)) continue; 

		if (ceil > 0) sprintf(s, "ceil %ukbit ", calc(bw, ceil));
			else s[0] = 0;
		x = (i + 1) * 10;

		fprintf(f,
			"# egress %d: %u-%u%%\n"
			"\t$TCA parent 1:1 classid 1:%d htb rate %ukbit %s %s prio %d quantum %u\n"
			"\t$TQA parent 1:%d handle %d: $SFQ\n"
			"\t$TFA parent 1: prio %d protocol ip handle %d fw flowid 1:%d\n",
				i, rate, ceil,
				x, calc(bw, rate), s, burst_leaf, (i >= 6) ? 7 : (i + 1), mtu,
				x, x,
				x, i + 1, x);
	}
	free(buf);

	/*
		10000 = ACK
		00100 = RST
		00010 = SYN
		00001 = FIN
	*/

	if (nvram_match("qos_ack", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 14 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "		// total length (0-63)
			"match u8 0x10 0xff at 33 "			// ACK only
			"flowid 1:10\n");
	}
	if (nvram_match("qos_syn", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 15 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "		// total length (0-63)
			"match u8 0x02 0x02 at 33 "			// SYN,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_fin", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 17 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "		// total length (0-63)
			"match u8 0x01 0x01 at 33 "			// FIN,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_rst", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 19 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "		// total length (0-63)
			"match u8 0x04 0x04 at 33 "			// RST,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_icmp", "on")) {
		fputs("\n\t$TFA parent 1: prio 13 protocol ip u32 match ip protocol 1 0xff flowid 1:10\n", f);
	}

	// ingress
	first = 1;
	bw = ibw;

	if (bw > 0) {
		g = buf = strdup(nvram_safe_get("qos_irates"));
		for (i = 0; i < 5; ++i) { // 0~4 , 0:highest, 4:lowest 
			if ((!g) || ((p = strsep(&g, ",")) == NULL)) break;
			if ((inuse & (1 << i)) == 0) continue;
			if ((rate = atoi(p)) < 1) continue;	// 0 = off

			if (first) {
				first = 0;
				fprintf(f,
					"\n"
					"\ttc qdisc del dev $I ingress 2>/dev/null\n"
					"\t$TQA handle ffff: ingress\n");
			}	

			// rate in kb/s
			unsigned int u = calc(bw, rate);

			// burst rate
			unsigned int v = u / 2;
			if (v < 50) v = 50;

			x = i + 1;
			fprintf(f,
				"# ingress %d: %u%%\n"
				"\t$TFA parent ffff: prio %d protocol ip handle %d"
					" fw police rate %ukbit burst %ukbit drop flowid ffff:%d\n",
					i, rate, x, x, u, v, x);
		}
		free(buf);
	}

	fputs(
		"\t;;\n"
		"stop)\n"
		"\ttc qdisc del dev $I root 2>/dev/null\n"
		"\ttc qdisc del dev $I ingress 2>/dev/null\n"
		"\t;;\n"
		"*)\n"
		"\ttc -s -d class ls dev $I\n"
		"\techo\n"
		"\ttc -s -d qdisc ls dev $I\n"
		"esac\n",
		f);

	fclose(f);
	chmod(qosfn, 0700);
	eval((char *)qosfn, "start");
	fprintf(stderr,"[qos] tc done!\n");

	return 0;
}


void stop_iQos(void)
{
	eval((char *)qosfn, "stop");
}
