/*
	iqos.c for TrendMicro iQoS / qosd
	iqos : 
		only run DPI engine related services
	qosd :
		only run qosd and build tc rule
*/

#include "bwdpi.h"
char *dev_wan = NULL;
char *dev_lan = NULL;

void erase_symbol(char *old, char *sym)
{
	char buf[13];
	int strLen;

	char *FindPos = strstr(old, sym);
	if((!FindPos) || (!sym)){
		printf("can't found symbol\n");
		return;
	}

	while(FindPos != NULL){
		//dbg("FindPos=%s, old=%s\n", FindPos, old);
		memset(buf, 0,sizeof(buf));
		strLen = FindPos - old;
		strncpy(buf, old, strLen);
		strcat(buf, FindPos+1);
		strcpy(old, buf);
		FindPos = strstr(old, sym);
	}
	
	//dbg("macaddr=%s\n", old);
}

static void set_prio_mac(FILE *fp, int dev_prio)
{
	char *buf, *g, *p;
	char *desc, *addr, *port, *prio, *transferred, *proto;
	int class_num;

	g = buf = strdup(nvram_safe_get("qos_rulelist"));
	while (g) {

		/* ASUSWRT 
  		qos_rulelist : 
			desc>addr>port>proto>transferred>prio
			
			addr  : (source) IP or MAC or IP-range
		 	prio  : 0-4, 0 is the highest		
  		*/
		if((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;
		class_num = atoi(prio);
		//dbg("set_prio: addr=%s, class_num=%d, dev_prio=%d\n", addr, class_num, dev_prio);
		if(strstr(addr, ":") == NULL) continue;
		if(class_num != dev_prio) continue;
		
		erase_symbol(addr, ":");
		fprintf(fp, "mac=%s\n", addr);
	}
	free(buf);
}

void setup_qos_conf()
{
	FILE *fp;
	int ibw ,obw;

	ibw = strtoul(nvram_safe_get("qos_ibw"), NULL, 10);
	obw = strtoul(nvram_safe_get("qos_obw"), NULL, 10);

	if(ibw == 0 || obw == 0){
		printf("setup_qos_conf : ibw/obw is NULL\n");
		return;
	}

	if((fp = fopen(QOS_CONF, "w")) == NULL){
		printf("fail to open qosd.conf\n");
		return;
	}

	fprintf(fp, "ceil_down=%dkbps\n", ibw);
	fprintf(fp, "ceil_up=%dkbps\n"	, obw);
	// set app rule
	fprintf(fp, "[0, 40%s, 100%s, 40%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[1, 30%s, 100%s, 30%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[2, 20%s, 100%s, 20%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[3, 10%s, 100%s, 10%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[4,  5%s, 100%s,  5%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[5,  5%s, 100%s,  5%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[6,  5%s, 100%s,  5%s, 100%s]\n", "%", "%", "%", "%");
	fprintf(fp, "[7,  5%s, 100%s,  5%s, 100%s]\n", "%", "%", "%", "%");
	// set dev rule (only by mac)
	fprintf(fp, "{0, 40%s, 100%s, 40%s, 100%s}\n", "%", "%", "%", "%");
	set_prio_mac(fp, 0);
	fprintf(fp, "{1, 30%s, 100%s, 30%s, 100%s}\n", "%", "%", "%", "%");
	set_prio_mac(fp, 1);
	fprintf(fp, "{2, 20%s, 100%s, 20%s, 100%s}\n", "%", "%", "%", "%");
	set_prio_mac(fp, 2);
	fprintf(fp, "{3, 15%s, 100%s, 15%s, 100%s}\n", "%", "%", "%", "%");
	fprintf(fp, "cat=na\n");
	fprintf(fp, "{4,  5%s, 100%s,  5%s, 100%s}\n", "%", "%", "%", "%");
	set_prio_mac(fp, 4);
	fclose(fp);
}

void stop_tm_qos()
{
	// step1. remove module
	eval("rmmod", "bw_forward.ko");
	eval("rmmod", "IDP.ko");

	// step2. remove dev nodes
	eval("rm", "-f", "/dev/detector");
	eval("rm", "-f", "/dev/idpfw");

	// step3. flush iptables rules
	eval("iptables", "-t", "mangle", "-F");
}

void start_tm_qos()
{
	char buf[16];
	dev_wan = get_wan_ifname(0);
	dev_lan = "br0";

	if(!strcmp(nvram_safe_get("qos_enable"), "0")){
		printf("tm_qos is disabled\n");
		return;
	}

	// step1. create dev node
	eval("mknod", "/dev/detector", "c", "190", "0");
	eval("mknod", "/dev/idpfw", "c", "191", "0");

	// step2. setup iptables rules
	eval("iptables", "-t", "mangle", "-N", "BWDPI_FILTER");
	eval("iptables", "-t", "mangle", "-F", "BWDPI_FILTER");
	eval("iptables", "-t", "mangle", "-A", "BWDPI_FILTER", "-i", dev_wan, "-p", "udp", "--sport", "68", "--dport", "67", "-j", "DROP");
	eval("iptables", "-t", "mangle", "-A", "BWDPI_FILTER", "-i", dev_wan, "-p", "udp", "--sport", "67", "--dport", "68", "-j", "DROP");
	eval("iptables", "-t", "mangle", "-A", "PREROUTING", "-i", dev_wan, "-p", "udp", "-j", "BWDPI_FILTER");

	// step3. insert DPI engine
	sprintf(buf, "dev_wan=%s", dev_wan);
	eval("insmod", "/usr/bwdpi/IDP.ko");
	eval("insmod", "/usr/bwdpi/bw_forward.ko", buf);

	// step4. run bwdpi-rule-agent
	if(!f_exists(DATABASE)){
		eval("cp", "-rf", "/usr/bwdpi/rule.trf", DATABASE);
		chdir(TMP_BWDPI);
		eval("bwdpi-rule-agent", "-g");
		chdir("/");
		eval("rm", "-rf", DATABASE);
	}

	// step5. setup qos.conf
	setup_qos_conf();
}

int tm_qos_main(char *cmd)
{
	if(!f_exists(TMP_BWDPI))
		eval("mkdir", "-p", TMP_BWDPI);

	if(!strcmp(cmd, "restart")){
		stop_tm_qos();
		start_tm_qos();
	}
	else if(!strcmp(cmd, "stop")){
		stop_tm_qos();
	}
	else if(!strcmp(cmd, "start")){
		start_tm_qos();
	}
	return 1;
}

void stop_qosd()
{
	dev_wan = get_wan_ifname(0);
	dev_lan = "br0";

	eval("killall", "qosd");
	eval("tc", "qdisc", "del", "dev", dev_wan, "root");
	eval("tc", "qdisc", "del", "dev", dev_lan, "root");
}

void start_qosd()
{
	char buff[128];
	dev_wan = get_wan_ifname(0);
	dev_lan = "br0";
	const int user_max = 253;
	
	if(!f_exists(QOS_CONF))
		setup_qos_conf();
	
	sprintf(buff, "qosd -i /dev/idpfw -f %s -w %s -l %s -u %d -b &", QOS_CONF, dev_wan, dev_lan, user_max);
	//dbg("start_qosd=%s\n", buff);
	system(buff);
}

int qosd_main(char *cmd)
{
	if(!f_exists(TMP_BWDPI))
		eval("mkdir", "-p", TMP_BWDPI);

	if(!strcmp(cmd, "restart")){
		stop_qosd();
		start_qosd();
	}
	else if(!strcmp(cmd, "stop")){
		stop_qosd();
	}
	else if(!strcmp(cmd, "start")){
		start_qosd();
	}
	return 1;
}
