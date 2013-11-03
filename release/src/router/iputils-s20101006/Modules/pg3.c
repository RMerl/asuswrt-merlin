/* pg3.c: Packet Generator for packet performance testing.
 *
 * Copyright 2001 by Robert Olsson <robert.olsson@its.uu.se>
 *                                 Uppsala University, Sweden
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 *
 */

/*

A tool for loading a network with a preconfigurated packets. The tool is
implemented as a linux module. Parameters as output device IPG interpacket
packet, number of packets can be configured. pg uses already intalled
device driver output routine.


Additional hacking by:

Jens.Laas@data.slu.se
Improved by ANK. 010120.
Improved by ANK even more. 010212.
MAC address typo fixed. 010417 --ro


TODO:
* could release kernel lock yet.


HOWTO:

1. Compile module pg3.o and install it in the place where modprobe may find it.
2. Cut script "ipg" (see below).
3. Edit script to set preferred device and destination IP address.
4. . ipg
5. After this two commands are defined:
   A. "pg" to start generator and to get results.
   B. "pgset" to change generator parameters. F.e.
      pgset "pkt_size 9014"   sets packet size to 9014
      pgset "frags 5"         packet will consist of 5 fragments
      pgset "count 200000"    sets number of packets to send
      pgset "ipg 5000"        sets artificial gap inserted between packets
			      to 5000 nanoseconds
      pgset "dst 10.0.0.1"    sets IP destination address
			      (BEWARE! This generator is very aggressive!)
      pgset "dstmac 00:00:00:00:00:00"    sets MAC destination address
      pgset stop    	      aborts injection

  Also, ^C aborts generator.

---- cut here

#! /bin/sh

modprobe pg3.o

function pgset() {
    local result

    echo $1 > /proc/net/pg

    result=`cat /proc/net/pg | fgrep "Result: OK:"`
    if [ "$result" = "" ]; then
	 cat /proc/net/pg | fgrep Result:
    fi
}

function pg() {
    echo inject > /proc/net/pg
    cat /proc/net/pg
}

pgset "odev eth0"
pgset "dst 0.0.0.0"

---- cut here
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <net/checksum.h>

static char version[] __initdata =
  "pg3.c: v1.0 010812: Packet Generator for packet performance testing.\n";



/* Parameters */

char pg_outdev[32], pg_dst[32];
int pkt_size=ETH_ZLEN;
int nfrags=0;
__u32 pg_count = 100000;  /* Default No packets to send */
__u32 pg_ipg = 0;  /* Default Interpacket gap in nsec */

/* Globar vars */

int debug;
int forced_stop;
int pg_cpu_speed;
int pg_busy;

static __u8 hh[14] = {
    0x00, 0x80, 0xC8, 0x79, 0xB3, 0xCB,

    /* We fill in SRC address later */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00
};

unsigned char *pg_dstmac = hh;
char pg_result[512];


static struct net_device *pg_setup_inject(u32 *saddrp)
{
	int p1, p2;
	struct net_device *odev;
	u32 saddr;

	rtnl_lock();
	odev = __dev_get_by_name(pg_outdev);
	if (!odev) {
		sprintf(pg_result, "No such netdevice: \"%s\"", pg_outdev);
		goto out_unlock;
	}

	if (odev->type != ARPHRD_ETHER) {
		sprintf(pg_result, "Not ethernet device: \"%s\"", pg_outdev);
		goto out_unlock;
	}

	if (!netif_running(odev)) {
		sprintf(pg_result, "Device is down: \"%s\"", pg_outdev);
		goto out_unlock;
	}

	for(p1=6,p2=0; p1 < odev->addr_len+6;p1++)
		hh[p1]=odev->dev_addr[p2++];

	saddr = 0;
	if (odev->ip_ptr) {
		struct in_device *in_dev = odev->ip_ptr;

		if (in_dev->ifa_list)
			saddr = in_dev->ifa_list->ifa_address;
	}
	atomic_inc(&odev->refcnt);
	rtnl_unlock();

	*saddrp = saddr;
	return odev;

out_unlock:
	rtnl_unlock();
	return NULL;
}


u32 idle_acc_lo, idle_acc_hi;

void nanospin(int pg_ipg)
{
	u32 idle_start, idle;

	idle_start = get_cycles();

	for (;;) {
		barrier();
		idle = get_cycles() - idle_start;
		if (idle*1000 >= pg_ipg*pg_cpu_speed)
			break;
	}
	idle_acc_lo += idle;
	if (idle_acc_lo < idle)
		idle_acc_hi++;
}

int calc_mhz(void)
{
	struct timeval start, stop;
	u32 start_s, elapsed;

	do_gettimeofday(&start);
	start_s = get_cycles();
	do {
		barrier();
		elapsed = get_cycles() - start_s;
		if (elapsed == 0)
			return 0;
	} while (elapsed < 1000*50000);
	do_gettimeofday(&stop);
	return elapsed/(stop.tv_usec-start.tv_usec+1000000*(stop.tv_sec-start.tv_sec));
}

static void cycles_calibrate(void)
{
	int i;

	for (i=0; i<3; i++) {
		int res = calc_mhz();
		if (res > pg_cpu_speed)
			pg_cpu_speed = res;
	}
}

struct sk_buff *
fill_packet(struct net_device *odev, __u32 saddr)
{
	struct sk_buff *skb;
	__u8 *eth;
	struct udphdr *udph;
	int datalen, iplen;
	struct iphdr *iph;

	skb = alloc_skb(pkt_size+64+16, GFP_ATOMIC);
	if (!skb) {
		sprintf(pg_result, "No memory");
		return NULL;
	}

	skb_reserve(skb, 16);

	/*  Reserve for ethernet and IP header  */
	eth = (__u8 *) skb_push(skb, 14);
	iph = (struct iphdr*)skb_put(skb, sizeof( struct iphdr));
	udph = (struct udphdr*)skb_put(skb, sizeof( struct udphdr));

	/*  Copy the ethernet header  */
	memcpy(eth, hh, 14);

	datalen = pkt_size-14-20-8; /* Eth + IPh + UDPh */
	if (datalen < 0)
		datalen = 0;

	udph->source= htons(9);
	udph->dest= htons(9);
	udph->len= htons(datalen+8); /* DATA + udphdr */
	udph->check=0;  /* No checksum */

	iph->ihl=5;
	iph->version=4;
	iph->ttl=3;
	iph->tos=0;
	iph->protocol = IPPROTO_UDP; /* UDP */
	iph->saddr =  saddr;
	iph->daddr =  in_aton(pg_dst);
	iph->frag_off = 0;
	iplen = 20 + 8 + datalen;
	iph->tot_len = htons(iplen);
	iph->check = 0;
	iph->check = ip_fast_csum((void *)iph, iph->ihl);
	skb->protocol = __constant_htons(ETH_P_IP);
	skb->mac.raw = ((u8*)iph) - 14;
	skb->dev = odev;
	skb->pkt_type = PACKET_HOST;

	if (nfrags<=0) {
		skb_put(skb, datalen);
	} else {
		int frags = nfrags;
		int i;

		if (frags > MAX_SKB_FRAGS)
			frags = MAX_SKB_FRAGS;
		if (datalen > frags*PAGE_SIZE) {
			skb_put(skb, datalen-frags*PAGE_SIZE);
			datalen = frags*PAGE_SIZE;
		}

		i = 0;
		while (datalen > 0) {
			struct page *page = alloc_pages(GFP_KERNEL, 0);
			skb_shinfo(skb)->frags[i].page = page;
			skb_shinfo(skb)->frags[i].page_offset = 0;
			skb_shinfo(skb)->frags[i].size = (datalen < PAGE_SIZE ? datalen : PAGE_SIZE);
			datalen -= skb_shinfo(skb)->frags[i].size;
			skb->len += skb_shinfo(skb)->frags[i].size;
			skb->data_len += skb_shinfo(skb)->frags[i].size;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}

		while (i < frags) {
			int rem;

			if (i == 0)
				break;

			rem = skb_shinfo(skb)->frags[i-1].size/2;
			if (rem == 0)
				break;

			skb_shinfo(skb)->frags[i-1].size -= rem;

			skb_shinfo(skb)->frags[i] = skb_shinfo(skb)->frags[i-1];
			get_page(skb_shinfo(skb)->frags[i].page);
			skb_shinfo(skb)->frags[i].page = skb_shinfo(skb)->frags[i-1].page;
			skb_shinfo(skb)->frags[i].page_offset += skb_shinfo(skb)->frags[i-1].size;
			skb_shinfo(skb)->frags[i].size = rem;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}
	}

	return skb;
}


static void pg_inject(void)
{
	u32 saddr;
	struct net_device *odev;
	struct sk_buff *skb;
	struct timeval start, stop;
	u32 total, idle;
	int pc, lcount;

	odev = pg_setup_inject(&saddr);
	if (!odev)
		return;

	skb = fill_packet(odev, saddr);
	if (skb == NULL)
		goto out_reldev;

	forced_stop = 0;
	idle_acc_hi = 0;
	idle_acc_lo = 0;
	pc = 0;
	lcount = pg_count;
	do_gettimeofday(&start);

	for(;;) {
		spin_lock_bh(&odev->xmit_lock);
		atomic_inc(&skb->users);
		if (!netif_queue_stopped(odev)) {
			if (odev->hard_start_xmit(skb, odev)) {
				kfree_skb(skb);
				if (net_ratelimit())
					printk(KERN_INFO "Hard xmit error\n");
			}
			pc++;
		} else {
			kfree_skb(skb);
		}
		spin_unlock_bh(&odev->xmit_lock);

		if (pg_ipg)
			nanospin(pg_ipg);
		if (forced_stop)
			goto out_intr;
		if (signal_pending(current))
			goto out_intr;

		if (--lcount == 0) {
			if (atomic_read(&skb->users) != 1) {
				u32 idle_start, idle;

				idle_start = get_cycles();
				while (atomic_read(&skb->users) != 1) {
					if (signal_pending(current))
						goto out_intr;
					schedule();
				}
				idle = get_cycles() - idle_start;
				idle_acc_lo += idle;
				if (idle_acc_lo < idle)
					idle_acc_hi++;
			}
			break;
		}

		if (netif_queue_stopped(odev) || current->need_resched) {
			u32 idle_start, idle;

			idle_start = get_cycles();
			do {
				if (signal_pending(current))
					goto out_intr;
				if (!netif_running(odev))
					goto out_intr;
				if (current->need_resched)
					schedule();
				else
					do_softirq();
			} while (netif_queue_stopped(odev));
			idle = get_cycles() - idle_start;
			idle_acc_lo += idle;
			if (idle_acc_lo < idle)
				idle_acc_hi++;
		}
	}

	do_gettimeofday(&stop);

	total = (stop.tv_sec - start.tv_sec)*1000000 +
		stop.tv_usec - start.tv_usec;

	idle = (((idle_acc_hi<<20)/pg_cpu_speed)<<12)+idle_acc_lo/pg_cpu_speed;

	if (1) {
		char *p = pg_result;

		p += sprintf(p, "OK: %u(c%u+d%u) usec, %u (%dbyte,%dfrags) %upps %uMB/sec",
			     total, total-idle, idle,
			     pc, skb->len, skb_shinfo(skb)->nr_frags,
			     ((pc*1000)/(total/1000)),
			     (((pc*1000)/(total/1000))*pkt_size)/1024/1024
			     );
	}

out_relskb:
	kfree_skb(skb);
out_reldev:
	dev_put(odev);
	return;

out_intr:
	sprintf(pg_result, "Interrupted");
	goto out_relskb;
}

/* proc/net/pg */

static struct proc_dir_entry *pg_proc_ent = 0;
static struct proc_dir_entry *pg_busy_proc_ent = 0;

int proc_pg_busy_read(char *buf , char **start, off_t offset,
		      int len, int *eof, void *data)
{
	char *p;

	p = buf;
	p += sprintf(p, "%d\n", pg_busy);
	*eof = 1;

	return p-buf;
}

int proc_pg_read(char *buf , char **start, off_t offset,
		 int len, int *eof, void *data)
{
	char *p;
	int i;

	p = buf;
	p += sprintf(p, "Params: count=%u pkt_size=%u frags %d ipg %u odev \"%s\" dst %s dstmac ",
		     pg_count, pkt_size, nfrags, pg_ipg,
		     pg_outdev, pg_dst);
	for(i=0;i<6;i++)
		p += sprintf(p, "%02X%s", pg_dstmac[i], i == 5 ? "\n" : ":");

	if(pg_result[0])
		p += sprintf(p, "Result: %s\n", pg_result);
	else
		p += sprintf(p, "Result: Idle\n");
	*eof = 1;
	return p-buf;
}

int count_trail_chars(const char *buffer, unsigned int maxlen)
{
	int i;

	for(i=0; i<maxlen;i++) {
		switch(buffer[i]) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '=':
			break;
		default:
			goto done;
		}
	}
done:
	return i;
}

unsigned long num_arg(const char *buffer, unsigned long maxlen,
		      unsigned long *num)
{
	int i=0;
	*num = 0;

	for(; i<maxlen;i++) {
		if( (buffer[i] >= '0') && (buffer[i] <= '9')) {
			*num *= 10;
			*num += buffer[i] -'0';
		}
		else
			break;
	}
	return i;
}

int strn_len(const char *buffer, unsigned int maxlen)
{
	int i=0;

	for(; i<maxlen;i++)
		switch(buffer[i]) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
			goto done_str;
		default:
		}
done_str:
	return i;
}

int proc_pg_write(struct file *file, const char *buffer,
		     unsigned long count, void *data)
{
	int i=0, max, len;
	char name[16], valstr[32];
	unsigned long value = 0;

	if (count < 1) {
		sprintf(pg_result, "Wrong command format");
		return -EINVAL;
	}

	max = count -i;
	i += count_trail_chars(&buffer[i], max);

	/* Read variable name */

	len = strn_len(&buffer[i], sizeof(name)-1);
	memset(name, 0, sizeof(name));
	strncpy(name, &buffer[i], len);
	i += len;

	max = count -i;
	len = count_trail_chars(&buffer[i], max);
	i += len;

	if (debug)
		printk("pg: %s,%lu\n", name, count);

	/* Only stop is allowed when we are running */

	if(!strcmp(name, "stop")) {
		forced_stop=1;
		if (pg_busy)
			strcpy(pg_result, "Stopping");
		return count;
	}

	if (pg_busy) {
		strcpy(pg_result, "Busy");
		return -EINVAL;
	}

	if(!strcmp(name, "pkt_size")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		if (value < 14+20+8)
			value = 14+20+8;
		pkt_size = value;
		sprintf(pg_result, "OK: pkt_size=%u", pkt_size);
		return count;
	}
	if(!strcmp(name, "frags")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		nfrags = value;
		sprintf(pg_result, "OK: frags=%u", nfrags);
		return count;
	}
	if(!strcmp(name, "ipg")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		pg_ipg = value;
		sprintf(pg_result, "OK: ipg=%u", pg_ipg);
		return count;
	}
	if(!strcmp(name, "count")) {
		len = num_arg(&buffer[i], 10, &value);
		i += len;
		pg_count = value;
		sprintf(pg_result, "OK: count=%u", pg_count);
		return count;
	}
	if(!strcmp(name, "odev")) {
		len = strn_len(&buffer[i], sizeof(pg_outdev)-1);
		memset(pg_outdev, 0, sizeof(pg_outdev));
		strncpy(pg_outdev, &buffer[i], len);
		i += len;
		sprintf(pg_result, "OK: odev=%s", pg_outdev);
		return count;
	}
	if(!strcmp(name, "dst")) {
		len = strn_len(&buffer[i], sizeof(pg_dst)-1);
		memset(pg_dst, 0, sizeof(pg_dst));
		strncpy(pg_dst, &buffer[i], len);
		if(debug)
			printk("pg: dst set to: %s\n", pg_dst);
		i += len;
		sprintf(pg_result, "OK: dst=%s", pg_dst);
		return count;
	}
	if(!strcmp(name, "dstmac")) {
		char *v = valstr;
		unsigned char *m = pg_dstmac;

		len = strn_len(&buffer[i], sizeof(valstr)-1);
		memset(valstr, 0, sizeof(valstr));
		strncpy(valstr, &buffer[i], len);
		i += len;

		for(*m = 0;*v && m < pg_dstmac+6;v++) {
			if(*v >= '0' && *v <= '9') {
				*m *= 16;
				*m += *v - '0';
			}
			if(*v >= 'A' && *v <= 'F') {
				*m *= 16;
				*m += *v - 'A' + 10;
			}
			if(*v >= 'a' && *v <= 'f') {
				*m *= 16;
				*m += *v - 'a' + 10;
			}
			if(*v == ':') {
				m++;
				*m = 0;
			}
		}
		sprintf(pg_result, "OK: dstmac");
		return count;
	}

	if (!strcmp(name, "inject") || !strcmp(name, "start") ) {
		MOD_INC_USE_COUNT;
		pg_busy = 1;
		strcpy(pg_result, "Starting");
		pg_inject();
		pg_busy = 0;
		MOD_DEC_USE_COUNT;
		return count;
	}

	sprintf(pg_result, "No such parameter \"%s\"", name);
	return -EINVAL;
}

static int pg_init(void)
{
	printk(version);
	cycles_calibrate();
	if (pg_cpu_speed == 0) {
		printk("pg3: Error: your machine does not have working cycle counter.\n");
		return -EINVAL;
	}
	if(!pg_proc_ent) {
		pg_proc_ent = create_proc_entry("net/pg", 0600, 0);
		if (pg_proc_ent) {
			pg_proc_ent->read_proc = proc_pg_read;
			pg_proc_ent->write_proc = proc_pg_write;
			pg_proc_ent->data = 0;
		}
		pg_busy_proc_ent = create_proc_entry("net/pg_busy", 0, 0);
		if (pg_busy_proc_ent) {
			pg_busy_proc_ent->read_proc = proc_pg_busy_read;
			pg_busy_proc_ent->data = 0;
		}
	}
	return 0;
}

void pg_cleanup(void)
{
	if (pg_proc_ent) {
		remove_proc_entry("net/pg", NULL);
		pg_proc_ent = 0;
		remove_proc_entry("net/pg_busy", NULL);
		pg_busy_proc_ent = 0;
	}
}

module_init(pg_init);
module_exit(pg_cleanup);


#if LINUX_VERSION_CODE > 0x20118
MODULE_AUTHOR("Robert Olsson <robert.olsson@its.uu.se");
MODULE_DESCRIPTION("Packet Generator tool");
MODULE_PARM(pg_count, "i");
MODULE_PARM(pg_ipg, "i");
MODULE_PARM(pg_cpu_speed, "i");
#endif

/*
 * Local variables:
 * compile-command: "gcc -DMODULE -D__KERNEL__ -I/usr/src/linux/include -Wall -Wstrict-prototypes -O2 -c pg3.c"
 * End:
 */
