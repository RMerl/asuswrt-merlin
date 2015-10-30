/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include <bcmnvram.h>
#include <bcmdevs.h>
#include <wlutils.h>

#include "shutils.h"
#include "shared.h"

#ifndef ETHER_ADDR_LEN
#define	ETHER_ADDR_LEN		6
#endif

extern char *read_whole_file(const char *target){
	FILE *fp;
	char *buffer, *new_str;
	int i;
	unsigned int read_bytes = 0;
	unsigned int each_size = 1024;

	if((fp = fopen(target, "r")) == NULL)
		return NULL;

	buffer = (char *)malloc(sizeof(char)*each_size);
	if(buffer == NULL){
		_dprintf("No memory \"buffer\".\n");
		fclose(fp);
		return NULL;
	}
	memset(buffer, 0, each_size);

	while ((i = fread(buffer+read_bytes, each_size * sizeof(char), 1, fp)) == 1){
		read_bytes += each_size;
		new_str = (char *)malloc(sizeof(char)*(each_size+read_bytes));
		if(new_str == NULL){
			_dprintf("No memory \"new_str\".\n");
			free(buffer);
			fclose(fp);
			return NULL;
		}
		memset(new_str, 0, sizeof(char)*(each_size+read_bytes));
		memcpy(new_str, buffer, read_bytes);

		free(buffer);
		buffer = new_str;
	}

	fclose(fp);
	return buffer;
}

extern char *get_line_from_buffer(const char *buf, char *line, const int line_size){
	int buf_len, len;
	char *ptr;

	if(buf == NULL || (buf_len = strlen(buf)) <= 0)
		return NULL;

	if((ptr = strchr(buf, '\n')) == NULL)
		ptr = (char *)(buf+buf_len);

	if((len = ptr-buf) < 0)
		len = buf-ptr;
	++len; // include '\n'.

	memset(line, 0, line_size);
	strncpy(line, buf, len);

	return line;
}

extern char *get_upper_str(const char *const str, char **target){
	int len, i;
	char *ptr;

	len = strlen(str);
	*target = (char *)malloc(sizeof(char)*(len+1));
	if(*target == NULL){
		_dprintf("No memory \"*target\".\n");
		return NULL;
	}
	ptr = *target;
	for(i = 0; i < len; ++i)
		ptr[i] = toupper(str[i]);
	ptr[len] = 0;

	return ptr;
}

extern int upper_strcmp(const char *const str1, const char *const str2){
	char *upper_str1, *upper_str2;
	int ret;

	if(str1 == NULL || str2 == NULL)
		return -1;

	if(get_upper_str(str1, &upper_str1) == NULL)
		return -1;

	if(get_upper_str(str2, &upper_str2) == NULL){
		free(upper_str1);
		return -1;
	}

	ret = strcmp(upper_str1, upper_str2);
	free(upper_str1);
	free(upper_str2);

	return ret;
}

extern int upper_strncmp(const char *const str1, const char *const str2, int count){
	char *upper_str1, *upper_str2;
	int ret;

	if(str1 == NULL || str2 == NULL)
		return -1;

	if(get_upper_str(str1, &upper_str1) == NULL)
		return -1;

	if(get_upper_str(str2, &upper_str2) == NULL){
		free(upper_str1);
		return -1;
	}

	ret = strncmp(upper_str1, upper_str2, count);
	free(upper_str1);
	free(upper_str2);

	return ret;
}

extern char *upper_strstr(const char *const str, const char *const target){
	char *upper_str, *upper_target;
	char *ret;
	int len;

	if(str == NULL || target == NULL)
		return NULL;

	if(get_upper_str(str, &upper_str) == NULL)
		return NULL;

	if(get_upper_str(target, &upper_target) == NULL){
		free(upper_str);
		return NULL;
	}

	ret = strstr(upper_str, upper_target);
	if(ret == NULL){
		free(upper_str);
		free(upper_target);
		return NULL;
	}

	if((len = upper_str-ret) < 0)
		len = ret-upper_str;

	free(upper_str);
	free(upper_target);

	return (char *)(str+len);
}

#ifdef RTCONFIG_IPV6
int get_ipv6_service(void)
{
	static const struct {
		char *name;
		int service;
	} services[] = {
		{ "dhcp6",	IPV6_NATIVE_DHCP },
		{ "6to4",	IPV6_6TO4 },
		{ "6in4",	IPV6_6IN4 },
		{ "6rd",	IPV6_6RD },
		{ "other",	IPV6_MANUAL },
		{ "static6",	IPV6_MANUAL }, /* legacy */
		{ NULL }
	};
	char *value;
	int i;

	value = nvram_safe_get("ipv6_service");
	for (i = 0; services[i].name; i++) {
		if (strcmp(value, services[i].name) == 0)
			return services[i].service;
	}
	return IPV6_DISABLED;
}

const char *ipv6_router_address(struct in6_addr *in6addr)
{
	char *p;
	struct in6_addr addr;
	static char addr6[INET6_ADDRSTRLEN];

	addr6[0] = '\0';

	if ((p = nvram_get("ipv6_rtr_addr")) && *p) {
		inet_pton(AF_INET6, p, &addr);
	}
	else if ((p = nvram_get("ipv6_prefix")) && *p) {
		inet_pton(AF_INET6, p, &addr);
		addr.s6_addr16[7] = htons(0x0001);
	}
	else {
		return addr6;
	}

	inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
	if (in6addr)
		memcpy(in6addr, &addr, sizeof(addr));

	return addr6;
}

// trim useless 0 from IPv6 address
const char *ipv6_address(const char *ipaddr6)
{
	struct in6_addr addr;
	static char addr6[INET6_ADDRSTRLEN];

	addr6[0] = '\0';

	if (inet_pton(AF_INET6, ipaddr6, &addr) > 0)
		inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));

	return addr6;
}

// extract prefix from configured IPv6 address
const char *ipv6_prefix(struct in6_addr *in6addr)
{
	static char prefix[INET6_ADDRSTRLEN + 1];
	struct in6_addr addr;
	int i, r;

	prefix[0] = '\0';

	if (inet_pton(AF_INET6, nvram_safe_get("ipv6_rtr_addr"), &addr) > 0) {
		r = nvram_get_int("ipv6_prefix_length") ? : 64;
		for (r = 128 - r, i = 15; r > 0; r -= 8) {
			if (r >= 8)
				addr.s6_addr[i--] = 0;
			else
				addr.s6_addr[i--] &= (0xff << r);
		}
		inet_ntop(AF_INET6, &addr, prefix, sizeof(prefix));
	}

	return prefix;
}

void reset_ipv6_linklocal_addr(const char *ifname, int flush)
{
	static char buf[INET6_ADDRSTRLEN];
	struct in6_addr addr;
	struct ifreq ifr;
	char *mac;
	int fd;

	if (!ifname ||
	    !strcmp(ifname, "lo") || !strncmp(ifname, "ppp", 3))
		return;

	if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	mac = (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) ?
		NULL : ifr.ifr_hwaddr.sa_data;
	close(fd);

	if (mac == NULL)
		return;

	addr.s6_addr32[0] = htonl(0xfe800000);
	addr.s6_addr32[1] = 0;
	memcpy(&addr.s6_addr[8], mac, 3);
	memcpy(&addr.s6_addr[13], mac + 3, 3);
	addr.s6_addr[8] ^= 0x02;
	addr.s6_addr[11] = 0xff;
	addr.s6_addr[12] = 0xfe;

	if (flush)
		eval("ip", "-6", "addr", "flush", "dev", (char *) ifname);
	if (inet_ntop(AF_INET6, &addr, buf, sizeof(buf)))
		eval("ip", "-6", "addr", "add", buf, "dev", (char *) ifname);
}

int with_ipv6_linklocal_addr(const char *ifname)
{
	return (getifaddr((char *) ifname, AF_INET6, GIF_LINKLOCAL) != NULL);
}

#if 1 /* temporary till httpd route table redo */
static const unsigned flagvals[] = { /* Must agree with flagchars[]. */
	RTF_GATEWAY,
	RTF_HOST,
	RTF_REINSTATE,
	RTF_DYNAMIC,
	RTF_MODIFIED,
	RTF_DEFAULT,
	RTF_ADDRCONF,
	RTF_CACHE
};

static const char flagchars[] =
	"GHRDM"
	"DAC"
;
const char str_default[] = "default";

void ipv6_set_flags(char *flagstr, int flags)
{
	int i;

	*flagstr++ = 'U';

	for (i = 0; (*flagstr = flagchars[i]) != 0; i++) {
		if (flags & flagvals[i]) {
			++flagstr;
		}
	}
}

char* INET6_rresolve(struct sockaddr_in6 *sin6, int numeric)
{
	char name[128];
	int s;

	if (sin6->sin6_family != AF_INET6) {
		fprintf(stderr, "rresolve: unsupported address family %d!",
			sin6->sin6_family);
		errno = EAFNOSUPPORT;
		return NULL;
	}
	if (numeric & 0x7FFF) {
		inet_ntop(AF_INET6, &sin6->sin6_addr, name, sizeof(name));
		return strdup(name);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (numeric & 0x8000)
			return strdup(str_default);
		return strdup("*");
	}

	s = getnameinfo((struct sockaddr *) sin6, sizeof(struct sockaddr_in6),
				name, sizeof(name), NULL, 0, 0);
	if (s) {
		perror("getnameinfo failed");
		return NULL;
	}
	return strdup(name);
}
#endif

static int ipv6_expand(char *buf, char *str, struct in6_addr *addr)
{
	char *dst = buf;
	char *src = str;
	int i;

	for (i = 1; dst < src && *src; i++) {
		*dst++ = *src++;
		if (i % 4 == 0) *dst++ = ':';
	}
	return inet_pton(AF_INET6, buf, addr);
}

const char *ipv6_gateway_address(void)
{
	static char buf[INET6_ADDRSTRLEN] = "";
	FILE *fp;
	struct in6_addr addr;
	char dest[41], nexthop[41], dev[17];
	int mask, prefix, metric, flags;
	int maxprefix, minmetric = minmetric;

	fp = fopen("/proc/net/ipv6_route", "r");
	if (fp == NULL) {
		perror("/proc/net/ipv6_route");
		return NULL;
	}

	maxprefix = -1;
	while (fscanf(fp, "%32s%x%*s%*x%32s%x%*x%*x%x%16s\n",
		      &dest[7], &prefix, &nexthop[7], &metric, &flags, dev) == 6) {
		/* Skip interfaces that are down and host routes */
		if ((flags & (RTF_UP | RTF_HOST)) != RTF_UP)
			continue;

		/* Skip dst not in "::/0 - 2000::/3" */
		if (prefix > 3)
			continue;
		if (ipv6_expand(dest, &dest[7], &addr) <= 0)
			continue;
		mask = htons((0xffff0000 >> prefix) & 0xffff);
		if ((addr.s6_addr16[0] & mask) != (htons(0x2000) & mask))
			continue;

		/* Skip dups & worse routes */
		if ((maxprefix > prefix) ||
		    (maxprefix == prefix && minmetric <= metric))
			continue;

		if (flags & RTF_GATEWAY) {
			if (ipv6_expand(nexthop, &nexthop[7], &addr) <= 0)
				continue;
			inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
		} else
			snprintf(buf, sizeof(buf), "::");
		maxprefix = prefix;
		minmetric = metric;

		if (prefix == 0)
			break;
	}
	fclose(fp);

	return *buf ? buf : NULL;
}
#endif

int wl_client(int unit, int subunit)
{
	char *mode = nvram_safe_get(wl_nvname("mode", unit, subunit));

	return ((strcmp(mode, "sta") == 0) || (strcmp(mode, "wet") == 0));
}

int foreach_wif(int include_vifs, void *param,
	int (*func)(int idx, int unit, int subunit, void *param))
{
	char ifnames[256];
	char name[64], ifname[64], *next = NULL;
	int unit = -1, subunit = -1;
	int i;
	int ret = 0;

	snprintf(ifnames, sizeof(ifnames), "%s %s",
		 nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
	remove_dups(ifnames, sizeof(ifnames));

	i = 0;
	foreach(name, ifnames, next) {
		if (nvifname_to_osifname(name, ifname, sizeof(ifname)) != 0)
			continue;

#ifdef CONFIG_BCMWL5
		if (wl_probe(ifname) || wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			continue;
#endif

		// Convert eth name to wl name
		if (osifname_to_nvifname(name, ifname, sizeof(ifname)) != 0)
			continue;

#ifdef CONFIG_BCMWL5
		// Slave intefaces have a '.' in the name
		if (strchr(ifname, '.') && !include_vifs)
			continue;
#endif

		if (get_ifname_unit(ifname, &unit, &subunit) < 0)
			continue;

		ret |= func(i++, unit, subunit, param);
	}
	return ret;
}

void notice_set(const char *path, const char *format, ...)
{
	char p[256];
	char buf[2048];
	va_list args;

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	mkdir("/var/notice", 0755);
	snprintf(p, sizeof(p), "/var/notice/%s", path);
	f_write_string(p, buf, 0, 0);
	if (buf[0]) syslog(LOG_INFO, "notice[%s]: %s", path, buf);
}


//	#define _x_dprintf(args...)	syslog(LOG_DEBUG, args);
#define _x_dprintf(args...)	do { } while (0);


// -----------------------------------------------------------------------------
#define ACT_COMM_LEN	20
struct action_s {
	char comm[ACT_COMM_LEN];
	pid_t pid;
	int action;
};

void set_action(int a)
{
	int r = 3;
	struct action_s act;
	char *s, *p, stat[sizeof("/proc/XXXXXX/statXXXXXX")];

	act.action = a;
	act.pid = getpid();
	snprintf(stat, sizeof(stat), "/proc/%d/stat", act.pid);
	s = file2str(stat);
	if (s) {
		if ((p = strrchr(s, ')')) != NULL)
			*p = '\0';
		if ((p = strchr(s, '(')) != NULL)
			snprintf(act.comm, sizeof(act.comm), "%s", ++p);
		free(s);
		s = (p && *act.comm) ? act.comm : NULL;
	}
	if (!s)
		snprintf(act.comm, sizeof(act.comm), "%d <UNKNOWN>", act.pid);
	_dprintf("%d: set_action %d\n", getpid(), act.action);
	while (f_write_excl(ACTION_LOCK, &act, sizeof(act), 0, 0600) != sizeof(act)) {
		sleep(1);
		if (--r == 0) return;
	}
	if (a != ACT_IDLE) sleep(2);
}

static int __check_action(struct action_s *pa)
{
	int r = 3;
	struct action_s act;

	while (f_read_excl(ACTION_LOCK, &act, sizeof(act)) != sizeof(act)) {
		sleep(1);
		if (--r == 0) return ACT_UNKNOWN;
	}
	if (pa)
		*pa = act;
	_dprintf("%d: check_action %d\n", getpid(), act.action);

	return act.action;
}

int check_action(void)
{
	return __check_action(NULL);
}

int wait_action_idle(int n)
{
	int r;
	struct action_s act;

	while (n-- > 0) {
		act.pid = 0;
		if (__check_action(&act) == ACT_IDLE) return 1;
		if (act.pid > 0 && !process_exists(act.pid)) {
			if (!(r = unlink(ACTION_LOCK)) || errno == ENOENT) {
				_dprintf("Terminated process, pid %d %s, hold action lock %d !!!\n",
					act.pid, act.comm, act.action);
				return 1;
			}
			_dprintf("Remove " ACTION_LOCK " failed. errno %d (%s)\n", errno, strerror(errno));
		}
		sleep(1);
	}
	_dprintf("pid %d %s hold action lock %d !!!\n", act.pid, act.comm, act.action);
	return 0;
}

const char *get_wanip(void)
{
	char tmp[100], prefix[]="wanXXXXXX_";
	int unit=0;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	return nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
}

int get_wanstate(void)
{
	char tmp[100], prefix[]="wanXXXXX_";

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_primary_ifunit());

	return nvram_get_int(strcat_r(prefix, "state_t", tmp));
}

const char *get_wanface(void)
{
	return get_wan_ifname(0);
}

#ifdef RTCONFIG_IPV6
const char *get_wan6face(void)
{
	switch (get_ipv6_service()) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
		return get_wan6_ifname(0);
	case IPV6_6TO4:
		return "v6to4";
	case IPV6_6IN4:
		return "v6in4";
	case IPV6_6RD:
		return "6rd";
	}
	return "";
}

int update_6rd_info(void)
{
	char tmp[100], prefix[]="wanXXXXX_";
	char addr6[INET6_ADDRSTRLEN + 1], *value;
	struct in6_addr addr;
	int unit = 0;

	if (get_ipv6_service() != IPV6_6RD || !nvram_get_int("ipv6_6rd_dhcp"))
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	value = nvram_safe_get(strcat_r(prefix, "6rd_prefix", tmp));
	if (*value ) {
		/* try to compact IPv6 prefix */
		if (inet_pton(AF_INET6, value, &addr) > 0)
			value = (char *) inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
		nvram_set("ipv6_6rd_prefix", value);
		nvram_set("ipv6_6rd_router", nvram_safe_get(strcat_r(prefix, "6rd_router", tmp)));
		nvram_set("ipv6_6rd_prefixlen", nvram_safe_get(strcat_r(prefix, "6rd_prefixlen", tmp)));
		nvram_set("ipv6_6rd_ip4size", nvram_safe_get(strcat_r(prefix, "6rd_ip4size", tmp)));
		return 1;
	}

	return 0;
}
#endif

const char *_getifaddr(char *ifname, int family, int flags, char *buf, int size)
{
	struct ifaddrs *ifap, *ifa;
	unsigned char *addr, *netmask;
	unsigned char len, maxlen;

	if (getifaddrs(&ifap) != 0) {
		_dprintf("getifaddrs failed: %s\n", strerror(errno));
		return NULL;
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != family ||
		    strncmp(ifa->ifa_name, ifname, IFNAMSIZ) != 0)
			continue;

		switch (ifa->ifa_addr->sa_family) {
#ifdef RTCONFIG_IPV6
		case AF_INET6:
			addr = (void *) &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
			if (IN6_IS_ADDR_LINKLOCAL(addr) ^ !!(flags & GIF_LINKLOCAL))
				continue;
			netmask = (void *) &((struct sockaddr_in6 *) ifa->ifa_netmask)->sin6_addr;
			maxlen = 128;
			break;
#endif
		case AF_INET:
			addr = (void *) &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			netmask = (void *) &((struct sockaddr_in *) ifa->ifa_netmask)->sin_addr;
			maxlen = 32;
			break;
		default:
			continue;
		}

		if (addr && inet_ntop(ifa->ifa_addr->sa_family, addr, buf, size) != NULL) {
			if (netmask && (flags & GIF_PREFIXLEN)) {
				for (len = 0; len < maxlen && *netmask == 0xff; netmask++)
					len += 8;
				if (len < maxlen && *netmask) {
					switch(*netmask) {
					case 0xfe: len += 7; break;
					case 0xfc: len += 6; break;
					case 0xf8: len += 5; break;
					case 0xf0: len += 4; break;
					case 0xe0: len += 3; break;
					case 0xc0: len += 2; break;
					case 0x80: len += 1; break;
					default:
						_dprintf("getifaddrs netmask error: %x\n", *netmask);
						goto error;
					}
				}
				if (len < maxlen) {
					netmask = memchr(buf, 0, size);
					if (netmask)
						snprintf(netmask, size - ((char *) netmask - buf), "/%d", len);
				}
			}
			freeifaddrs(ifap);
			return buf;
		}
	}

error:
	freeifaddrs(ifap);
	return NULL;
}

const char *getifaddr(char *ifname, int family, int flags)
{
	static char buf[INET6_ADDRSTRLEN];

	return _getifaddr(ifname, family, flags, buf, sizeof(buf));
}


int is_intf_up(const char* ifname)
{
	struct ifreq ifr;
	int sfd;
	int ret = 0;

	if (!((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0))
	{
		strcpy(ifr.ifr_name, ifname);
		if (!ioctl(sfd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP))
			ret = 1;

		close(sfd);
	}

	return ret;
}

char *wl_nvname(const char *nv, int unit, int subunit)
{
	static char tmp[128];
	char prefix[] = "wlXXXXXXXXXX_";

	if (unit < 0)
		strcpy(prefix, "wl_");
	else if (subunit > 0)
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	return strcat_r(prefix, nv, tmp);
}

// -----------------------------------------------------------------------------

int mtd_getinfo(const char *mtdname, int *part, int *size)
{
	FILE *f;
	char s[256];
	char t[256];
	int r;

	r = 0;
	if ((strlen(mtdname) < 128)) { // && (strcmp(mtdname, "pmon") != 0)) { //Yau dbg
		sprintf(t, "\"%s\"", mtdname);
		if ((f = fopen("/proc/mtd", "r")) != NULL) {
			while (fgets(s, sizeof(s), f) != NULL) {
				if ((sscanf(s, "mtd%d: %x", part, size) == 2) && (strstr(s, t) != NULL)) {
					// don't accidentally mess with bl (0)
					if (*part >= 0) r = 1; //Yau > -> >=
					r =1;
					break;
				}
			}
			fclose(f);
		}
	}
	if (!r) {
		*size = 0;
		*part = -1;
	}
	return r;
}

#if defined(RTCONFIG_UBIFS)
#define UBI_SYSFS_DIR	"/sys/class/ubi"
/* return device number, volume id, and volume size in bytes
 * @return:
 *      1:	ubi volume not found
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	UBI not exist (open /sys/class/ubi failed)
 *     <0:	error
 */
int ubi_getinfo(const char *ubiname, int *dev, int *part, int *size)
{
	DIR *dir;
	int d, p, l, cmp, ret = 1;
	char *s1, *s2, path[PATH_MAX];
	struct dirent *ent;

	if (!ubiname || *ubiname == '\0' || !dev || !part || !size)
		return -1;

	if ((dir = opendir(UBI_SYSFS_DIR)) == NULL)
		return -2;

	while (ret && (ent = readdir(dir)) != NULL) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || strncmp(ent->d_name, "ubi", 3))
			continue;
		if (sscanf(ent->d_name, "ubi%d_%d", &d, &p) != 2)
			continue;

		snprintf(path, sizeof(path), "%s/ubi%d_%d/name", UBI_SYSFS_DIR, d, p);
		if (!(s1 = file2str(path)))
			continue;
		/* remove tailed line-feed character */
		l = strlen(s1);
		if (*(s1+l-1) == '\xa')
			*(s1+l-1) = '\0';
		cmp = strcmp(ubiname, s1);
		free(s1);
		if (cmp)
			continue;

		snprintf(path, sizeof(path), "%s/ubi%d_%d/data_bytes", UBI_SYSFS_DIR, d, p);
		if (!(s2 = file2str(path)))
			continue;
		*dev = d;
		*part = p;
		*size = atoi(s2);
		free(s2);
		ret = 0;
	}
	closedir(dir);

	return ret;
}
#endif

// -----------------------------------------------------------------------------

int nvram_get_int(const char *key)
{
	return atoi(nvram_safe_get(key));
}

int nvram_set_int(const char *key, int value)
{
	char nvramstr[16];

	snprintf(nvramstr, sizeof(nvramstr), "%d", value);
	return nvram_set(key, nvramstr);
}

#if defined(RTCONFIG_SSH) || defined(RTCONFIG_HTTPS)
int nvram_get_file(const char *key, const char *fname, int max)
{
	int n;
	char *p;
	char *b;
	int r;

	r = 0;
	p = nvram_safe_get(key);
	n = strlen(p);
	if (n <= max) {
		if ((b = malloc(base64_decoded_len(n) + 128)) != NULL) {
			n = base64_decode(p, (unsigned char *) b, n);
			if (n > 0) r = (f_write(fname, b, n, 0, 0644) == n);
			free(b);
		}
	}
	return r;
/*
	char b[3500];
	int n;
	char *p;

	p = nvram_safe_get(key);
	n = strlen(p);
	if (n <= max) {
		n = base64_decode(p, b, n);
		if (n > 0) return (f_write(fname, b, n, 0, 0700) == n);
	}
	return 0;
*/
}

int nvram_set_file(const char *key, const char *fname, int max)
{
	char *in;
	char *out;
	long len;
	int n;
	int r;

	if ((len = f_size(fname)) > max) return 0;
	max = (int)len;
	r = 0;
	if (f_read_alloc(fname, &in, max) == max) {
		if ((out = malloc(base64_encoded_len(max) + 128)) != NULL) {
			n = base64_encode((unsigned char *) in, out, max);
			out[n] = 0;
			nvram_set(key, out);
			free(out);
			r = 1;
		}
		free(in);
	}
	return r;
/*
	char a[3500];
	char b[7000];
	int n;

	if (((n = f_read(fname, &a, sizeof(a))) > 0) && (n <= max)) {
		n = base64_encode(a, b, n);
		b[n] = 0;
		nvram_set(key, b);
		return 1;
	}
	return 0;
*/
}
#endif

int nvram_contains_word(const char *key, const char *word)
{
	return (find_word(nvram_safe_get(key), word) != NULL);
}

int nvram_is_empty(const char *key)
{
	char *p;
	return (((p = nvram_get(key)) == NULL) || (*p == 0));
}

void nvram_commit_x(void)
{
	if (!nvram_get_int("debug_nocommit")) nvram_commit();
}

void chld_reap(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static uint32 crc_table[256]={ 0x00000000,0x77073096,0xEE0E612C,0x990951BA,
0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,
0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,
0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,
0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,
0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,
0xDCD60DCF,0xABD13D59,0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,
0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,0x98D220BC,
0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,
0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,0x6B6B51F4,
0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,
0xFBD44C65,0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,
0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,
0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,
0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,
0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,
0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,
0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,
0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,
0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,
0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,
0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,
0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,
0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,
0x0BDBDF21,0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,
0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,
0xF862AE69,0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,
0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,
0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,
0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D};

uint32_t crc_calc(uint32_t crc, const char *buf, int len)
{
	crc = crc ^ 0xffffffff;
	while (len-- > 0) {
		crc = crc_table[(crc ^ *((unsigned char *)buf)) & 0xFF] ^ (crc >> 8);
		buf++;
	}
	crc = crc ^ 0xffffffff;
	return crc;
}

// ugly solution and doesn't support wantag
void bcmvlan_models(int model, char *vlan)
{
	switch (model) {
	case MODEL_DSLAC68U:
	case MODEL_RPAC68U:
	case MODEL_RTAC68U:
	case MODEL_RTAC87U:
	case MODEL_RTAC56S:
	case MODEL_RTAC56U:
	case MODEL_RTAC66U:
	case MODEL_RTN66U:
	case MODEL_RTN16:
	case MODEL_RTN18U:
	case MODEL_RTN15U:
	case MODEL_RTAC53U:
	case MODEL_RTAC3200:
	case MODEL_RTAC88U:
	case MODEL_RTAC3100:
	case MODEL_RTAC5300:
	case MODEL_RTAC1200G:
	case MODEL_RTAC1200GP:
		strcpy(vlan, "vlan1");
		break;
	case MODEL_RTN53:
	case MODEL_RTN12:
	case MODEL_RTN12B1:
	case MODEL_RTN12C1:
	case MODEL_RTN12D1:
	case MODEL_RTN12VP:
	case MODEL_RTN12HP:
	case MODEL_RTN12HP_B1:
	case MODEL_RTN14UHP:
	case MODEL_RTN10U:
	case MODEL_RTN10P:
	case MODEL_RTN10D1:
	case MODEL_RTN10PV2:
		strcpy(vlan, "vlan0");
		break;
	default:
		strcpy(vlan, "");
		break;
	}
}

char *get_productid(void)
{
	char *productid = nvram_safe_get("productid");
#ifdef RTCONFIG_ODMPID
	char *odmpid = nvram_safe_get("odmpid");
	if (*odmpid)
		productid = odmpid;
#endif
	return productid;
}

long backup_rx = 0;
long backup_tx = 0;
int backup_set = 0;

unsigned int netdev_calc(char *ifname, char *ifname_desc, unsigned long *rx, unsigned long *tx, char *ifname_desc2, unsigned long *rx2, unsigned long *tx2
)		
{
	char word[100], word1[100], *next, *next1;
	char tmp[100];
	char modelvlan[32];
	int i, j, model, unit;
	strcpy(ifname_desc2, "");

	model = get_model();
	bcmvlan_models(model, modelvlan);

	// find in LAN interface
	if(nvram_contains_word("lan_ifnames", ifname))
	{
		// find Wireless interface
		i=0;
		foreach(word, nvram_safe_get("wl_ifnames"), next) {
			if(strcmp(word, ifname)==0) {
				sprintf(ifname_desc, "WIRELESS%d", i);
				return 1;
			}

#ifdef RTCONFIG_RALINK
			if(model == MODEL_RTN65U && strncmp(ifname, "rai", 3) == 0) // invalid traffic amount for interface
				return 0;
#endif

			sprintf(tmp, "wl%d_vifnames", i);
			j = 0;
			foreach(word1, nvram_safe_get(tmp), next1) {
				if(strcmp(word1, wif_to_vif(ifname))==0)
				{
					sprintf(ifname_desc, "WIRELESS%d.%d", i, j);
					return 1;
				}
				j++;
			}

			i++;
		}

#if defined(RTCONFIG_RALINK) && defined(RTCONFIG_WLMODULE_RT3352_INIC_MII)
		if(model == MODEL_RTN65U)
		{
			i = 0;
			j = 0;
			foreach(word, nvram_safe_get("nic_lan_ifnames"), next) {
				if(strcmp(word, ifname)==0)
				{
					sprintf(tmp, "wl%d_vifnames", 0);
					foreach(word1, nvram_safe_get(tmp), next1) {
						char vifname[32];
						sprintf(vifname, "%s_ifname", word1);
						if(strlen(nvram_safe_get(vifname)) > 0)
						{
							if(i-- == 0)
								break;
						}
						j++;
					}
				sprintf(ifname_desc, "WIRELESS%d.%d", 0, j);
					return 1;
				}
				i++;
			}
		}
#endif

		// find wired interface
		strcpy(ifname_desc, "WIRED");

		if(model == MODEL_DSLAC68U)
			return 1;

#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
			return 1;
#endif

		// special handle for non-tag wan of broadcom solution
		// pretend vlanX is must called after ethX
		if(nvram_match("switch_wantag", "none")) { //Don't calc if select IPTV
			if(backup_set && strlen(modelvlan) && strcmp(ifname, modelvlan)==0) {
				backup_set  = 0;
				backup_rx -= *rx;
				backup_tx -= *tx;

				*rx2 = backup_rx;
				*tx2 = backup_tx;				
				/* Cherry Cho modified for RT-AC3200 Bug#202 in 2014/11/4. */	
				unit = get_wan_unit("eth0");

#ifdef RTCONFIG_DUALWAN			
				if ( (unit == wan_primary_ifunit()) || ( !strstr(nvram_safe_get("wans_dualwan"), "none") && nvram_match("wans_mode", "lb")) )
				{
					if (unit == WAN_UNIT_FIRST)
						strcpy(ifname_desc2, "INTERNET");
					else
						sprintf(ifname_desc2,"INTERNET%d", unit);
				}									
#else
				if(unit == wan_primary_ifunit())
					strcpy(ifname_desc2, "INTERNET");					
#endif	/* RTCONFIG_DUALWAN */
			}
		}//End of switch_wantag

		return 1;
	}
	// find bridge interface
	else if(nvram_match("lan_ifname", ifname))
	{
		strcpy(ifname_desc, "BRIDGE");
		return 1;
	}
	// find in WAN interface
	else if (ifname && (unit = get_wan_unit(ifname)) >= 0)	{
		if (dualwan_unit__nonusbif(unit)) {
#if defined(RA_ESW)
#if defined(RTCONFIG_RALINK_MT7620)
			get_mt7620_wan_unit_bytecount(unit, tx, rx);
#elif defined(RTCONFIG_RALINK_MT7621)
			get_mt7621_wan_unit_bytecount(unit, tx, rx);
#endif			
#endif
#ifndef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
			if(strlen(modelvlan) && strcmp(ifname, "eth0")==0) {
				backup_rx = *rx;
				backup_tx = *tx;
				backup_set  = 1;
			}
			else{
#endif
#ifdef RTCONFIG_DUALWAN
				if ( (unit == wan_primary_ifunit()) || ( !strstr(nvram_safe_get("wans_dualwan"), "none") && nvram_match("wans_mode", "lb")) )
				{
					if (unit == WAN_UNIT_FIRST) {	
						strcpy(ifname_desc, "INTERNET");
						return 1;
					}
					else {
						sprintf(ifname_desc,"INTERNET%d", unit);
						return 1;
					}
				}
#else
				if(unit == wan_primary_ifunit()){
					strcpy(ifname_desc, "INTERNET");
					return 1;
				}			
#endif	/* RTCONFIG_DUALWAN */
#ifndef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
			}
#endif
		}
		else if (dualwan_unit__usbif(unit)) {
#ifdef RTCONFIG_DUALWAN
			if ( (unit == wan_primary_ifunit()) || ( !strstr(nvram_safe_get("wans_dualwan"), "none") && nvram_match("wans_mode", "lb")) )
			{
				if(unit == WAN_UNIT_FIRST){//Cherry Cho modified in 2014/11/4.
					strcpy(ifname_desc, "INTERNET");
					return 1;
				}
				else{
					sprintf(ifname_desc,"INTERNET%d", unit);
					return 1;
				}
			}					
#else
			if(unit == wan_primary_ifunit()){
				strcpy(ifname_desc, "INTERNET");
				return 1;
			}	
#endif	/* RTCONFIG_DUALWAN */
		}
		else {
			_dprintf("%s: unknown ifname %s\n", __func__, ifname);
		}
	}

	return 0;
}


// 0: Not private subnet, 1: A class, 2: B class, 3: C class.
int is_private_subnet(const char *ip)
{
	unsigned long long ip_num;
	unsigned long long A_class_start, A_class_end;
	unsigned long long B_class_start, B_class_end;
	unsigned long long C_class_start, C_class_end;

	if(ip == NULL)
		return 0;

	A_class_start = inet_network("10.0.0.0");
	A_class_end = inet_network("10.255.255.255");
	B_class_start = inet_network("172.16.0.0");
	B_class_end = inet_network("172.31.255.255");
	C_class_start = inet_network("192.168.0.0");
	C_class_end = inet_network("192.168.255.255");

	ip_num = inet_network(ip);

	if(ip_num > A_class_start && ip_num < A_class_end)
		return 1;
	else if(ip_num > B_class_start && ip_num < B_class_end)
		return 2;
	else if(ip_num > C_class_start && ip_num < C_class_end)
		return 3;
	else
		return 0;
}

// clean_mode: 0~3, clean_time: 0~(LONG_MAX-1), threshold(KB): 0: always act, >0: act when lower than.
int free_caches(const char *clean_mode, const int clean_time, const unsigned int threshold){
	int test_num;
	FILE *fp;
	char memdata[256] = {0};
	unsigned int memfree = 0;

#ifdef RTCONFIG_BCMARM
	return 0;
#endif

	/* Paul add 2012/7/17, skip free caches for DSL model. */
	#ifdef RTCONFIG_DSL
		return 0;
	#endif

	if(!clean_mode || clean_time < 0)
		return -1;

_dprintf("clean_mode(%s) clean_time(%d) threshold(%u)\n", clean_mode, clean_time, threshold);
	test_num = strtol(clean_mode, NULL, 10);
	if(test_num == LONG_MIN || test_num == LONG_MAX
			|| test_num < atoi(FREE_MEM_NONE) || test_num > atoi(FREE_MEM_ALL)
			)
		return -1;

	if(threshold > 0){
		if((fp = fopen("/proc/meminfo", "r")) != NULL){
			while(fgets(memdata, 255, fp) != NULL){
				if(strstr(memdata, "MemFree") != NULL){
					sscanf(memdata, "MemFree: %d kB", &memfree);
_dprintf("%s: memfree=%u.\n", __FUNCTION__, memfree);
					break;
				}
			}
			fclose(fp);
			if(memfree > threshold){
_dprintf("%s: memfree > threshold.\n", __FUNCTION__);
				return 0;
			}
		}
	}

_dprintf("%s: Start syncing...\n", __FUNCTION__);
	sync();

_dprintf("%s: Start cleaning...\n", __FUNCTION__);
	f_write_string("/proc/sys/vm/drop_caches", clean_mode, 0, 0);
	if(clean_time > 0){
_dprintf("%s: waiting %d second...\n", __FUNCTION__, clean_time);
		sleep(clean_time);
_dprintf("%s: Finish.\n", __FUNCTION__);
		f_write_string("/proc/sys/vm/drop_caches", FREE_MEM_NONE, 0, 0);
	}

	return 0;
}

void logmessage_normal(char *logheader, char *fmt, ...){
  va_list args;
  char buf[512];
  int level;

  va_start(args, fmt);

  vsnprintf(buf, sizeof(buf), fmt, args);

  level = nvram_get_int("message_loglevel");
  if (level > 7) level = 7;

  openlog(logheader, 0, 0);
  syslog(level, buf);
  closelog();
  va_end(args);
}

char *get_logfile_path(void)
{
	static char prefix[] = "/jffsXXXXXX";

#if defined(RTCONFIG_PSISTLOG)
	strcpy(prefix, "/jffs");
	if (!check_if_dir_writable(prefix)) {
		_dprintf("logfile output directory: /tmp.\n");
		strcpy(prefix, "/tmp");
	}
#else
	strcpy(prefix, "/tmp");
#endif

	return prefix;
}

char *get_syslog_fname(unsigned int idx)
{
	char prefix[] = "/jffsXXXXXX";
	static char buf[PATH_MAX];

#if defined(RTCONFIG_PSISTLOG)
	strcpy(prefix, "/jffs");
	if (!check_if_dir_writable(prefix)) {
		_dprintf("syslog output directory: /tmp.\n");
		strcpy(prefix, "/tmp");
	}
#else
	strcpy(prefix, "/tmp");
#endif

	if (!idx)
		sprintf(buf, "%s/syslog.log", prefix);
	else
		sprintf(buf, "%s/syslog.log-%d", prefix, idx);

	return buf;
}

#ifdef RTCONFIG_USB_MODEM
char *get_modemlog_fname(void){
	char prefix[] = "/tmpXXXXXX";
	static char buf[PATH_MAX];

	strcpy(prefix, "/tmp");
	sprintf(buf, "%s/3ginfo.txt", prefix);

	return buf;
}
#endif

#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA
int is_psta(int unit)
{
	if (unit < 0) return 0;
	if ((nvram_get_int("sw_mode") == SW_MODE_AP) &&
		(nvram_get_int("wlc_psta") == 1) &&
		((nvram_get_int("wlc_band") == unit)
#ifdef PXYSTA_DUALBAND
		|| (nvram_match("exband", "1") && nvram_get_int("wlc_band_ex") == unit)
#endif
		))
		return 1;

	return 0;
}

int is_psr(int unit)
{
	if (unit < 0) return 0;
#ifdef RTCONFIG_QTN
	if (unit == 1) return 0;
#endif
	if ((nvram_get_int("sw_mode") == SW_MODE_AP) &&
		(nvram_get_int("wlc_psta") == 2) &&
		((nvram_get_int("wlc_band") == unit)
#ifdef PXYSTA_DUALBAND
		||  (nvram_match("exband", "1") && nvram_get_int("wlc_band_ex") == unit)
#endif
		))
		return 1;

	return 0;
}

int psta_exist()
{
	char word[256], *next;
	int idx = 0;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (is_psta(idx)) return 1;
		idx++;
	}

	return 0;
}

int psta_exist_except(int unit)
{
	char word[256], *next;
	int idx = 0;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (idx == unit) goto END;
		if (is_psta(idx)) return 1;
END:
		idx++;
	}

	return 0;
}

int psr_exist()
{
	char word[256], *next;
	int idx = 0;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (is_psr(idx)) return 1;
		idx++;
	}

	return 0;
}

int psr_exist_except(int unit)
{
	char word[256], *next;
	int idx = 0;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (idx == unit) goto END;
		if (is_psr(idx)) return 1;
END:
		idx++;
	}

	return 0;
}
#endif
#endif

#ifdef RTCONFIG_OPENVPN
char *get_parsed_crt(const char *name, char *buf, size_t buf_len)
{
	char *value;
	int len, i;
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	FILE *fp;
	char tmpBuf[256] = {0};
	char *p = buf;
#endif

	value = nvram_safe_get(name);
	len = strlen(value);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	if(!check_if_dir_exist(OVPN_FS_PATH))
		mkdir(OVPN_FS_PATH, S_IRWXU);
	snprintf(tmpBuf, sizeof(tmpBuf) -1, "%s/%s", OVPN_FS_PATH, name);
#endif

	if(len) {
		for (i=0; (i < len); i++) {
			if (value[i] == '>')
				buf[i] = '\n';
			else
				buf[i] = value[i];
		}
		buf[i] = '\0';

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		//save to file and then clear nvram value
		fp = fopen(tmpBuf, "w");
		if(fp) {
			chmod(tmpBuf, S_IRUSR|S_IWUSR);
			fprintf(fp, "%s", buf);
			fclose(fp);
			nvram_set(name, "");
		}
#endif
	}
	else {
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		//nvram value cleard, get from file
		fp = fopen(tmpBuf, "r");
		if(fp) {
			while(fgets(buf, buf_len, fp)) {
				if(!strncmp(buf, "-----BEGIN", 10))
					break;
			}
			if(feof(fp)) {
				fclose(fp);
				memset(buf, 0, buf_len);
				return buf;
			}
			p += strlen(buf);
			memset(tmpBuf, 0, sizeof(tmpBuf));
			while(fgets(tmpBuf, sizeof(tmpBuf), fp)) {
				strncpy(p, tmpBuf, strlen(tmpBuf));
				p += strlen(tmpBuf);
			}
			*p = '\0';
			fclose(fp);
		}
#endif
	}
	return buf;
}

int set_crt_parsed(const char *name, char *file_path)
{
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	char target_file_path[128] ={0};

	if(!check_if_dir_exist(OVPN_FS_PATH))
		mkdir(OVPN_FS_PATH, S_IRWXU);

	if(check_if_file_exist(file_path)) {
		snprintf(target_file_path, sizeof(target_file_path) -1, "%s/%s", OVPN_FS_PATH, name);
		return eval("cp", file_path, target_file_path);
	}
	else {
		return -1;
	}
#else
	FILE *fp=fopen(file_path, "r");
	char buffer[4000] = {0};
	char buffer2[256] = {0};
	char *p = buffer;

// TODO: Ensure that Asus's routine can handle CRLF too, otherwise revert to
//       the code we currently use in httpd.

	if(fp) {
		while(fgets(buffer, sizeof(buffer), fp)) {
			if(!strncmp(buffer, "-----BEGIN", 10))
				break;
		}
		if(feof(fp)) {
			fclose(fp);
			return -EINVAL;
		}
		p += strlen(buffer);
		//if( *(p-1) == '\n' )
			//*(p-1) = '>';
		while(fgets(buffer2, sizeof(buffer2), fp)) {
			strncpy(p, buffer2, strlen(buffer2));
			p += strlen(buffer2);
			//if( *(p-1) == '\n' )
				//*(p-1) = '>';
		}
		*p = '\0';
		nvram_set(name, buffer);
		fclose(fp);
		return 0;
	}
	else
		return -ENOENT;
#endif
}

int ovpn_crt_is_empty(const char *name)
{
	char file_path[128] ={0};
	struct stat st;

	if( nvram_is_empty(name) ) {
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		//check file
		if(d_exists(OVPN_FS_PATH)) {
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, name);
			if(stat(file_path, &st) == 0) {
				if( !S_ISDIR(st.st_mode) && st.st_size ) {
					return 0;
				}
				else {
					return 1;
				}
			}
			else {
				return 1;
			}
		}
		else {
			mkdir(OVPN_FS_PATH, S_IRWXU);
			return 1;
		}
#else
		return 1;
#endif
	}
	else {
		return 0;
	}
}
#endif

#ifdef RTCONFIG_DUALWAN
/**
 * Get wan_unit of USB interface
 * @return
 *  >= 0:	wan_unit
 *  <  0:	not found
 */
int get_usbif_dualwan_unit(void)
{
	int wan_unit;

	for (wan_unit = WAN_UNIT_FIRST; wan_unit < WAN_UNIT_MAX; ++wan_unit)
		if (dualwan_unit__usbif(wan_unit))
			break;

	if (wan_unit == WAN_UNIT_MAX)
		wan_unit = -1;

	return wan_unit;
}

/**
 * Get unit of primary interface (WAN/LAN/USB only)
 * @return
 *  >= 0:	wan_unit
 *  <  0:	not found
 */
int get_primaryif_dualwan_unit(void)
{
	int unit, wan_type;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		if (unit != wan_primary_ifunit() && !nvram_match("wans_mode", "lb"))
			continue;

		wan_type = get_dualwan_by_unit(unit);
		if (wan_type != WANS_DUALWAN_IF_WAN
		    && wan_type != WANS_DUALWAN_IF_LAN
		    && wan_type != WANS_DUALWAN_IF_USB)
			continue;

		break;
	}

	if(unit == WAN_UNIT_MAX)
		unit = -1;

	return unit;
}
#endif

/* Return WiFi unit number in accordance with interface name.
 * @wif:	pointer to WiFi interface name.
 * @return:
 * 	< 0:	invalid
 *  otherwise:	unit
 */
int get_wifi_unit(char *wif)
{
	int i;
	char word[256], *next, *ifn, nv[20];

	if (!wif || *wif == '\0')
		return -1;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		if (strncmp(word, wif, strlen(word)))
			continue;

		for (i = 0; i <= 1; ++i) {
			sprintf(nv, "wl%d_ifname", i);
			ifn = nvram_safe_get(nv);
			if (!strncmp(word, ifn, strlen(word)))
				return i;
		}
	}
	return -1;
}


#ifdef RTCONFIG_YANDEXDNS
int get_yandex_dns(int family, int mode, char **server, int max_count)
{
	static const struct {
		int mode;
		int family;
		char *server;
	} table[] = {
		{ YADNS_BASIC, AF_INET, "77.88.8.8" },			/* dns.yandex.ru  */
		{ YADNS_BASIC, AF_INET, "77.88.8.1" },			/* secondary.dns.yandex.ru */
		{ YADNS_SAFE, AF_INET, "77.88.8.88" },			/* safe.dns.yandex.ru */
		{ YADNS_SAFE, AF_INET, "77.88.8.2" },			/* secondary.safe.dns.yandex.ru */
		{ YADNS_FAMILY, AF_INET, "77.88.8.7" },			/* family.dns.yandex.ru */
		{ YADNS_FAMILY, AF_INET, "77.88.8.3" },			/* secondary.family.dns.yandex.ru */
#ifdef RTCONFIG_IPV6
		{ YADNS_BASIC, AF_INET6, "2a02:6b8::feed:0ff" },	/* dns.yandex.ru  */
		{ YADNS_BASIC, AF_INET6, "2a02:6b8:0:1::feed:0ff" },	/* secondary.dns.yandex.ru */
		{ YADNS_SAFE, AF_INET6, "2a02:6b8::feed:bad" },		/* safe.dns.yandex.ru */
		{ YADNS_SAFE, AF_INET6, "2a02:6b8:0:1::feed:bad" },	/* secondary.safe.dns.yandex.ru */
		{ YADNS_FAMILY, AF_INET6, "2a02:6b8::feed:a11" },	/* family.dns.yandex.ru */
		{ YADNS_FAMILY, AF_INET6, "2a02:6b8:0:1::feed:a11" },	/* secondary.family.dns.yandex.ru */
#endif
	};
	int i, count = 0;

	if (mode < YADNS_FIRST || mode >= YADNS_COUNT)
		return -1;

	for (i = 0; i < sizeof(table)/sizeof(table[0]) && count < max_count; i++) {
		if ((mode == table[i].mode) &&
		    (family == AF_UNSPEC || family == table[i].family))
			server[count++] = table[i].server;
	}

	return count;
}
#endif


#ifdef RTCONFIG_BWDPI
/*
	usage in rc or bwdpi for checking service
*/
int check_bwdpi_nvram_setting()
{
	int enabled = 1;
	int debug = nvram_get_int("bwdpi_debug");

	// check no qos service
	if(nvram_get_int("wrs_enable") == 0 && nvram_get_int("wrs_app_enable") == 0 && 
		nvram_get_int("wrs_vp_enable") == 0 && nvram_get_int("wrs_cc_enable") == 0 &&
		nvram_get_int("wrs_mals_enable") == 0 &&
		nvram_get_int("wrs_adblock_popup") == 0 && nvram_get_int("wrs_adblock_stream") == 0 &&
		nvram_get_int("bwdpi_db_enable") == 0 &&
		nvram_get_int("qos_enable") == 0)
		enabled = 0;

	// check traditional qos service
	if(nvram_get_int("wrs_enable") == 0 && nvram_get_int("wrs_app_enable") == 0 && 
		nvram_get_int("wrs_vp_enable") == 0 && nvram_get_int("wrs_cc_enable") == 0 &&
		nvram_get_int("wrs_mals_enable") == 0 &&
		nvram_get_int("wrs_adblock_popup") == 0 && nvram_get_int("wrs_adblock_stream") == 0 &&
		nvram_get_int("bwdpi_db_enable") == 0 &&
		nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0)
		enabled = 0;

	if(debug) dbg("[check_bwdpi_nvram_setting] enabled= %d\n", enabled);

	return enabled;
}
#endif

/*
	transfer timestamp into date
	ex. date = 2014-07-14 19:20:10
*/
void StampToDate(unsigned long timestamp, char *date)
{
	struct tm *local;
	time_t now;
	
	now = timestamp;
	local = localtime(&now);
	strftime(date, 30, "%Y-%m-%d %H:%M:%S", local);
}

int get_iface_hwaddr(char *name, unsigned char *hwaddr)
{
	struct ifreq ifr;
	int ret = 0;
	int s;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return errno;
	}

	/* do it */
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name)-1);
	ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';
	if ((ret = ioctl(s, SIOCGIFHWADDR, &ifr)) == 0)
		memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

	/* cleanup */
	close(s);
	return ret;
}

int _xstart(const char *cmd, ...)
{
        va_list ap;
        char *argv[16];
        int argc;
        int pid;

        argv[0] = (char *)cmd;
        argc = 1;
        va_start(ap, cmd);
        while ((argv[argc++] = va_arg(ap, char *)) != NULL) {
                //
        }
        va_end(ap);

        return _eval(argv, NULL, 0, &pid);
}

long fappend(FILE *out, const char *fname)
{
	FILE *in;
	char buf[1024];
	int n;
	long r;

	if ((in = fopen(fname, "r")) == NULL) return -1;
	r = 0;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			r = -1;
			break;
		}
		else {
			r += n;
		}
	}
	fclose(in);
	return r;
}

void run_custom_script(char *name, char *args)
{
	char script[120];

	snprintf(script, sizeof(script), "/jffs/scripts/%s", name);

	if(f_exists(script)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom script", "Found %s, but custom script execution is disabled!", name);
			return;
		}
		if (args)
			logmessage("custom script" ,"Running %s (args: %s)", script, args);
		else
			logmessage("custom script" ,"Running %s", script);
		xstart(script, args);
	}
}

void run_custom_script_blocking(char *name, char *args)
{
	char script[120];

	snprintf(script, sizeof(script), "/jffs/scripts/%s", name);

	if(f_exists(script)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom script", "Found %s, but custom script execution is disabled!", name);
			return;
		}
		if (args)
			logmessage("custom script" ,"Running %s (args: %s)", script, args);
		else
			logmessage("custom script" ,"Running %s", script);
		eval(script, args);
	}

}

void run_postconf(char *name, char *config)
{
	char filename[64];

	snprintf(filename, sizeof (filename), "%s.postconf", name);
	run_custom_script_blocking(filename, config);
}


void use_custom_config(char *config, char *target)
{
        char filename[256];

        snprintf(filename, sizeof(filename), "/jffs/configs/%s", config);

	if (check_if_file_exist(filename)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom config", "Found %s, but custom configs are disabled!", filename);
			return;
		}
		logmessage("custom config", "Using custom %s config file.", filename);
		eval("cp", filename, target, NULL);
	}
}


void append_custom_config(char *config, FILE *fp)
{
	char filename[256];

	snprintf(filename, sizeof(filename), "/jffs/configs/%s.add", config);

	if (check_if_file_exist(filename)) {
		if (nvram_match("jffs2_scripts", "0")) {
			logmessage("custom config", "Found %s, but custom configs are disabled!", filename);
			return;
		}
		logmessage("custom config", "Appending content of %s.", filename);
		fappend(fp, filename);
	}
}

