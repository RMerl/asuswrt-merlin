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

#include <bcmnvram.h>
#include <bcmdevs.h>
#include <wlutils.h>

#include "shutils.h"
#include "shared.h"


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
	const char *names[] = {	// order must be synced with def at shared.h
		"static6",	// IPV6_NATIVE
		"dhcp6",	// IPV6_NATIVE_DHCP
		"6to4",		// IPV6_6TO4
		"6in4",		// IPV6_6IN4
		"6rd",		// IPV6_6RD
		"other",	// IPV6_MANUAL
		NULL
	};
	int i;
	const char *p;

	p = nvram_safe_get("ipv6_service");
	for (i = 0; names[i] != NULL; ++i) {
		if (strcmp(p, names[i]) == 0) return i + 1;
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

const char *ipv6_gateway_address()
{
	char addr6[128], *naddr6;
	/* In addr6x, we store both 40-byte ':'-delimited ipv6 addresses.
	 * We read the non-delimited strings into the tail of the buffer
	 * using fscanf and then modify the buffer by shifting forward
	 * while inserting ':'s and the nul terminator for the first string.
	 * Hence the strings are at addr6x and addr6x+40.  This generates
	 * _much_ less code than the previous (upstream) approach. */
	char addr6x[80];
	char iface[16], flags[16];
	int iflags, metric, refcnt, use, prefix_len, slen;
	struct sockaddr_in6 snaddr6;
	int ret = 0;
	static char buf[INET6_ADDRSTRLEN];
	struct in6_addr addr;
	int found = 0;

	FILE *fp = fopen("/proc/net/ipv6_route", "r");

	memset(buf, 0, sizeof(buf));

	while (1) {
		int r;
		r = fscanf(fp, "%32s%x%*s%x%32s%x%x%x%x%s\n",
				addr6x+14, &prefix_len, &slen, addr6x+40+7,
				&metric, &use, &refcnt, &iflags, iface);
		if (r != 9) {
			if ((r < 0) && feof(fp)) { /* EOF with no (nonspace) chars read. */
				break;
			}
 ERROR:
			perror("fscanf");
			return buf;
		}

		/* Do the addr6x shift-and-insert changes to ':'-delimit addresses.
		 * For now, always do this to validate the proc route format, even
		 * if the interface is down. */
		{
			int i = 0;
			char *p = addr6x+14;

			do {
				if (!*p) {
					if (i == 40) { /* nul terminator for 1st address? */
						addr6x[39] = 0;	/* Fixup... need 0 instead of ':'. */
						++p;	/* Skip and continue. */
						continue;
					}
					goto ERROR;
				}
				addr6x[i++] = *p++;
				if (!((i+1) % 5)) {
					addr6x[i++] = ':';
				}
			} while (i < 40+28+7);
		}

		if (!(iflags & RTF_UP)) { /* Skip interfaces that are down. */
			continue;
		}

		ipv6_set_flags(flags, (iflags & IPV6_MASK));

		r = 0;
		do {
			inet_pton(AF_INET6, addr6x + r,
					  (struct sockaddr *) &snaddr6.sin6_addr);
			snaddr6.sin6_family = AF_INET6;
			naddr6 = INET6_rresolve((struct sockaddr_in6 *) &snaddr6,
						   0x0fff /* Apparently, upstream never resolves. */
						   );
			inet_pton(AF_INET6, naddr6, &addr);

			if (!r) {			/* 1st pass */
				snprintf(addr6, sizeof(addr6), "%s/%d", naddr6, prefix_len);
				r += 40;
				free(naddr6);
			} else {			/* 2nd pass */
				if (!strcmp(addr6, "::/0") && !strcmp(flags, "UGDA") &&
				    !strcmp(get_wan6face(), iface) &&
				    IN6_IS_ADDR_LINKLOCAL(&addr))
				{
					found = 1;
					snprintf(buf, sizeof(buf), "%s %s", naddr6, iface);
				}
				free(naddr6);
				if (found)
					return buf;
				break;
			}
		} while (1);
	}

	return buf;
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

const dns_list_t *get_dns(void)
{
	static dns_list_t dns;
	char s[512];
	int n;
	int i, j;
	struct in_addr ia;
	char d[4][22];
	unsigned short port;
	char *c;

	dns.count = 0;

	strlcpy(s, nvram_safe_get("wan_dns"), sizeof(s));
	if ((nvram_get_int("dns_addget")) || (s[0] == 0)) {
		n = strlen(s);
		snprintf(s + n, sizeof(s) - n, " %s", nvram_safe_get("wan_get_dns"));
	}

	n = sscanf(s, "%21s %21s %21s %21s", d[0], d[1], d[2], d[3]);
	for (i = 0; i < n; ++i) {
		port = 53;

		if ((c = strchr(d[i], ':')) != NULL) {
			*c++ = 0;
			if (((j = atoi(c)) < 1) || (j > 0xFFFF)) continue;
			port = j;
		}
		
		if (inet_pton(AF_INET, d[i], &ia) > 0) {
			for (j = dns.count - 1; j >= 0; --j) {
				if ((dns.dns[j].addr.s_addr == ia.s_addr) && (dns.dns[j].port == port)) break;
			}
			if (j < 0) {
				dns.dns[dns.count].port = port;
				dns.dns[dns.count++].addr.s_addr = ia.s_addr;
				if (dns.count == 3) break;
			}
		}
	}

	return &dns;
}

// -----------------------------------------------------------------------------

void set_action(int a)
{
	_dprintf("set_action %d\n", a);

	int r = 3;
	while (f_write("/var/lock/action", &a, sizeof(a), 0, 0) != sizeof(a)) {
		sleep(1);
		if (--r == 0) return;
	}
	if (a != ACT_IDLE) sleep(2);
}

int check_action(void)
{
	int a;
	int r = 3;

	while (f_read("/var/lock/action", &a, sizeof(a)) != sizeof(a)) {
		sleep(1);
		if (--r == 0) return ACT_UNKNOWN;
	}
	_dprintf("check_action %d\n", a);

	return a;
}

int wait_action_idle(int n)
{
	while (n-- > 0) {
		if (check_action() == ACT_IDLE) return 1;
		sleep(1);
	}
	return 0;
}

const char *get_wanip(void)
{	
	char tmp[100], prefix[]="wanXXXXXX_";
	int unit=0;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	return nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
}

const int get_wanstate(void)
{
	char tmp[100], prefix[]="wanXXXXX_";
	int unit=0;

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
	case IPV6_NATIVE:
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
//	return nvram_safe_get("ipv6_ifname");
	return "";
}

int update_6rd_info(void)
{
	if (get_ipv6_service() == IPV6_6RD && nvram_match("ipv6_6rd_dhcp", "1")) {
		char addr6[INET6_ADDRSTRLEN + 1];
		char *prefix;
		struct in6_addr addr;

		if (!nvram_invmatch("wan0_6rd_router", ""))
			return 0;

		/* try to compact IPv6 prefix */
		prefix = nvram_safe_get("wan0_6rd_prefix");
		if (inet_pton(AF_INET6, prefix, &addr) == 1)
			prefix = inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));

		nvram_set("ipv6_6rd_prefix", prefix);
		nvram_set("ipv6_6rd_router", nvram_safe_get("wan0_6rd_router"));
		nvram_set("ipv6_6rd_prefixlen", nvram_safe_get("wan0_6rd_prefixlen"));
		nvram_set("ipv6_6rd_ip4size", nvram_safe_get("wan0_6rd_ip4size"));
		return 1;
	}
	else return -1;
}
#endif

const char *getifaddr(char *ifname, int family, int linklocal)
{
	static char buf[INET6_ADDRSTRLEN];
	void *addr = NULL;
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
		_dprintf("getifaddrs failed: %s\n", strerror(errno));
		return NULL;
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if ((ifa->ifa_addr == NULL) ||
		    (strncmp(ifa->ifa_name, ifname, IFNAMSIZ) != 0) ||
		    (ifa->ifa_addr->sa_family != family))
			continue;

#ifdef RTCONFIG_IPV6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)(ifa->ifa_addr);
			if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr) ^ linklocal)
				continue;
			addr = (void *)&(s6->sin6_addr);
		}
		else
#endif
		{
			struct sockaddr_in *s = (struct sockaddr_in *)(ifa->ifa_addr);
			addr = (void *)&(s->sin_addr);
		}

		if ((addr) && inet_ntop(ifa->ifa_addr->sa_family, addr, buf, sizeof(buf)) != NULL) {
			freeifaddrs(ifap);
			return buf;
		}
	}

	freeifaddrs(ifap);
	return NULL;
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
			n = base64_decode(p, b, n);
			if (n > 0) r = (f_write(fname, b, n, 0, 0644) == n);
			free(b);
		}
	}
	return r;
/*
	char b[2048];
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
			n = base64_encode(in, out, max);
			out[n] = 0;
			nvram_set(key, out);
			free(out);
			r = 1;
		}
		free(in);
	}
	return r;
/*
	char a[2048];
	char b[4096];
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

uint32 crc_calc(uint32 crc, char *buf, int len)
{
	while (len-- > 0) {
		crc = crc_table[(crc ^ *((char *)buf)) & 0xFF] ^ (crc >> 8);
		buf++;
	}
	return crc;
}

// ugly solution
void bcmvlan_models(int model, char *vlan)
{
	if(model==MODEL_RTN16||model==MODEL_RTN15U||model==MODEL_RTN66U||model==MODEL_RTAC66U)
		strcpy(vlan, "vlan1");
	else if(model==MODEL_RTN53||model==MODEL_RTN12||model==MODEL_RTN12B1||model==MODEL_RTN12C1||model==MODEL_RTN12D1||model==MODEL_RTN12HP||model==MODEL_RTN10U||model==MODEL_RTN10D1)
		strcpy(vlan, "vlan0");
	else strcpy(vlan, "");
}

char *get_productid()
{
	char *productid = nvram_safe_get("productid");
#ifdef RTCONFIG_ODMPID
	char *odmpid = nvram_safe_get("odmpid");
	if (*odmpid)
		productid = odmpid;
#endif
	return productid;
}

int backup_rx;
int backup_tx;

unsigned int netdev_calc(char *ifname, char *ifname_desc, unsigned long *rx, unsigned long *tx, char *ifname_desc2, unsigned long *rx2, unsigned long *tx2)
{
	char word[100], word1[100], *next, *next1;
	char tmp[100];
	char modelvlan[32];
	int i, j, model;

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

			sprintf(tmp, "wl%d_vifnames", i);
			j = 0;
			foreach(word1, nvram_safe_get(tmp), next1) {
				if(strcmp(word1, ifname)==0) {
					sprintf(ifname_desc, "WIRELESS%d.%d", i, j);
					return 1;
				}
				j++;
			}

			i++;
		}

		// find wired interface
		strcpy(ifname_desc, "WIRED");

		// special handle for non-tag wan of broadcom solution
		// pretend vlanX is must called after ethX
		if(nvram_match("switch_wantag", "none")) { //Don't calc if select IPTV
		   if(strlen(modelvlan) && strcmp(ifname, modelvlan)==0) {
			backup_rx -= *rx;
			backup_tx -= *tx;

			*rx2 = backup_rx;
			*tx2 = backup_tx;				
			strcpy(ifname_desc2, "INTERNET");
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
	else if(nvram_contains_word("wan_ifnames", ifname))
	{
		if(strlen(modelvlan) && strcmp(ifname, "eth0")==0) { 
			backup_rx = *rx;
			backup_tx = *tx;
		}
		else {
			strcpy(ifname_desc, "INTERNET");
			return 1;	
		}
	}
	return 0;
}

// 0: Not private subnet, 1: A class, 2: B class, 3: C class.
int is_private_subnet(const char *ip){
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

// clean_mode: 0~3, clean_time: 1~(LONG_MAX-1), threshold(KB): 0: always act, >0: act when lower than.
int free_caches(const char *clean_mode, const int clean_time, const unsigned int threshold){
	int test_num;
	FILE *fp;
	char memdata[256] = {0};
	unsigned int memfree = 0;

	/* Paul add 2012/7/17, skip free caches for DSL model. */
	#ifdef RTCONFIG_DSL
		return 0;
	#endif

	if(!clean_mode || clean_time <= 0)
		return -1;

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
_dprintf("%s: waiting %d second...\n", __FUNCTION__, clean_time);
	sleep(clean_time);
_dprintf("%s: Finish.\n", __FUNCTION__);
	f_write_string("/proc/sys/vm/drop_caches", FREE_MEM_NONE, 0, 0);

	return 0;
}

