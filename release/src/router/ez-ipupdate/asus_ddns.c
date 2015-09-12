#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define IF_LOOKUP 1
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "md5.h"
#include "asus_ddns.h"
#if 0
#include <nvram_linux.h>
#else //2007.03.14 Yau add for WL500gp2
#include <bcmnvram.h>
#endif
#include <syslog.h>
#include <shared.h>

#define MAX_DOMAIN_LEN	50	// hostname len (32) + "asuscomm.com" ~~ 45, reserve 5 char

//#define DUMP_KEY
#ifdef DUMP_KEY
#define DUMP(k)	dump (k, sizeof (k))
#else
#define DUMP(k)
#endif	// DUMP_KEY

enum {
  REGISTERES_OK = 0,
  REGISTERES_ERROR,
  REGISTERES_SHUTDOWN,
};

/**************************************** Copy from ez-ipupdate.c * start ********************************/
#ifdef DEBUG
#  define BUFFER_SIZE (16*1024)
#else
#  define BUFFER_SIZE (4*1024-1)
#endif

#define OPT_DEBUG       0x0001
#define OPT_DAEMON      0x0004
#define OPT_QUIET       0x0008
#define OPT_FOREGROUND  0x0010
#define OPT_OFFLINE     0x0020

enum {
  UPDATERES_OK = 0,
  UPDATERES_ERROR,
  UPDATERES_SHUTDOWN,
};
/**************************************** Copy from ez-ipupdate.c * end **********************************/


#ifdef DEBUG
#	define PRINT(fmt,args...)	fprintf (stderr, fmt, ## args);
#	define DBG(fmt,args...) 	fprintf (stderr, fmt, ## args);
#else
#	define PRINT(fmt,args...)	show_message(fmt,args...);
//#	define PRINT(fmt,args...) 	syslog (LOG_NOTICE, fmt, ## args);
#	define DBG(fmt,args...)
#endif

int g_asus_ddns_mode = 0; /* 0: disable;  1: register domain;  2: update ip */


static void hm(text, text_len, key, key_len, digest)
unsigned char *text;			/* pointer to data stream */
int text_len;					/* length of data stream */
unsigned char *key;				/* pointer to authentication key */
int key_len;					/* length of authentication key */
unsigned char *digest;			/* caller digest to be filled in */

{
	struct md5_ctx context;
	unsigned char k_ipad[65];	/* inner padding -
								 * key XORd with ipad
								 */
	unsigned char k_opad[65];	/* outer padding -
								 * key XORd with opad
								 */
	unsigned char tk[16];
	int i;
	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
		struct md5_ctx tctx;
		md5_init_ctx(&tctx);
		md5_process_bytes(key, key_len, &tctx);
		md5_finish_ctx(&tctx, tk);

		key = tk;
		key_len = 16;
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	bzero(k_ipad, sizeof k_ipad);
	bzero(k_opad, sizeof k_opad);
	bcopy(key, k_ipad, key_len);
	bcopy(key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	/*
	 * perform inner MD5
	 */
	md5_init_ctx(&context);						/* init context for 1st * pass */
	md5_process_bytes(k_ipad, 64, &context);	/* start with inner pad */
	md5_process_bytes(text, text_len, &context);/* then text of datagram */
	md5_finish_ctx(&context, digest);			/* finish up 1st pass */
	/*
	 * perform outer MD5
	 */
	md5_init_ctx(&context);						/* init context for 2nd * pass */
	md5_process_bytes(k_opad, 64, &context);	/* start with outer pad */
	md5_process_bytes(digest, 16, &context);	/* then results of 1st * hash */
	md5_finish_ctx(&context, digest);			/* finish up 2nd pass */
}


#ifdef TEST_HMAC_MD5
/*
HMAC-MD5 Test Vectors (Trailing '\0' of a character string not included in test):

  key =         0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b
  key_len =     16 bytes
  data =        "Hi There"
  data_len =    8  bytes
  digest =      0x9294727a3638bb1c13f48ef8158bfc9d

  key =         "Jefe"
  data =        "what do ya want for nothing?"
  data_len =    28 bytes
  digest =      0x750c783e6ab0b503eaa86e310a5db738

  key =         0xAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
  key_len       16 bytes
  data =        0xDDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD...
                ..DDDDDDDDDDDDDDDDDDDD
  data_len =    50 bytes
  digest =      0x56be34521d144c88dbb8c733f0e8b3f6

  ------------------------------------------------
  HOW TO COMPILE TEST VECTORS?
  gcc -Wall -c -o test_hmac_md5.o -DTEST_HMAC_MD5 -DDEBUG -DUSE_MD5 asus_ddns.c
  gcc -Wall -c -o md5_x86.o -DTEST_HMAC_MD5 -DDEBUG -DUSE_MD5 md5.c
  gcc -Wall -o test_hmac_md5 test_hmac_md5.o md5_x86.o

  ------------------------------------------------
  NOTES: If your device is big-endian platform, you have to define WORDS_BIGENDIAN as 1.

  The test_hmac_md5 is test program. It will print three hash values and match with above.
*/
static void dump (unsigned char *p, unsigned int l)
{
	unsigned int i;
	if (p == NULL || l == 0)
		return;

	printf ("0x");
	for (i = 0; i < l; ++i)
		printf ("%02x", p[i]);
	printf ("\n\n");
}

int main(void)
{
	unsigned char key[16], data[256], digest[16];

	// case 1
	memset (key, 0, sizeof (key));
	memset (key, 0x0b, 16);
	memset (data, 0, sizeof (data));
	strcpy ((char*)data, "Hi There");
	hm(data, 8, key, 16, digest);
	printf ("case 1:"); dump (digest, 16);
	
	// case 2
	memset (key, 0, sizeof (key));
	strcpy ((char*)key, "Jefe");
	memset (data, 0, sizeof (data));
	strcpy ((char*)data, "what do ya want for nothing?");
	hm(data, 28, key, 4, digest);
	printf ("case 2:"); dump (digest, 16);
	
	// case 3
	memset (key, 0, sizeof (key));
	memset (key, 0xAA, 16);
	memset (data, 0, sizeof (data));
	memset (data, 0xDD, 50);
	hm(data, 50, key, 16, digest);
	printf ("case 3:"); dump (digest, 16);
	

	return 0;
}
#else	// !TEST_HMAC_MD5

#define MULTICAST_BIT  0x0001
#define UNIQUE_OUI_BIT 0x0002
int TransferMacAddr(const char* mac, char* transfer_mac)
{
        int sec_byte;
        int i = 0, s = 0;
	char *p_mac;
	p_mac = transfer_mac;

        if( strlen(mac)!=17 || !strcmp("00:00:00:00:00:00", mac) )
                return 0;

        while( *mac && i<12 ) {
                if( isxdigit(*mac) ) {
                        if(i==1) {
                                sec_byte= strtol(mac, NULL, 16);
                                if((sec_byte & MULTICAST_BIT)||(sec_byte & UNIQUE_OUI_BIT))
                                        break;
                        }
                        i++;
		        memcpy(p_mac, mac, 1);
                	p_mac++;
                }
                else if( *mac==':') {
                        if( i==0 || i/2-1!=s )
                                break;
                        ++s;
                }
                ++mac;
        }
        return( i==12 && s==5 );
}

int asus_reg_domain (void)
{
	char buf[BUFFER_SIZE + 1];
	char *p, *bp = buf;
	char ret_buf[64];
	char sug_name[MAX_DOMAIN_LEN], old_name[MAX_DOMAIN_LEN];
	int bytes;
	int btot;
	int ret;
	int retval = REGISTERES_OK;

	buf[BUFFER_SIZE] = '\0';

	if (do_connect((int *) &client_sockfd, server, port) != 0) {
		PRINT ("error connecting to %s:%s\n", server, port);
		show_message("error connecting to %s:%s\n", server, port);
		nvram_set ("ddns_return_code", "connect_fail");
		nvram_set ("ddns_return_code_chk", "connect_fail");
		return (REGISTERES_ERROR);
	}

	char *ddns_transfer;
	char old_mac[13];
	memset(old_mac, 0, 13);
	if(TransferMacAddr(nvram_safe_get("ddns_transfer"), old_mac)) {
		snprintf(buf, BUFFER_SIZE, "GET /ddns/register.jsp?hostname=%s&myip=%s&oldmac=%s HTTP/1.0\015\012", 
			 host, address, old_mac);
	}
	else {
		snprintf(buf, BUFFER_SIZE, "GET /ddns/register.jsp?hostname=%s&myip=%s HTTP/1.0\015\012", host, address);
	}
	output(buf);
	snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
	output(buf);
	snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
	output(buf);
	snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
	output(buf);
	snprintf(buf, BUFFER_SIZE, "\015\012");
	output(buf);

	memset (buf, 0, sizeof (buf));
	bp = buf;
	bytes = 0;
	btot = 0;
	ret = 0;
	while ((bytes = read_input(bp, BUFFER_SIZE - btot)) > 0) {
		bp += bytes;
		btot += bytes;
	}
	close(client_sockfd);
	buf[btot] = '\0';
	//show_message("Asus Reg domain:: return: %s\n", buf);
	if(btot) { // TODO: according to server response, parsing code have to rewrite
		if (sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1) {
			ret = -1;
		}

		// looking for 2-th '|'
		p = strchr (buf, '|');
		if (p != NULL)
			p = strchr (p+1, '|');
		if (p == NULL)	{
			p = "";
		}
		snprintf (ret_buf, sizeof (ret_buf), "%s,%d", "register", ret);
	}

	nvram_set ("ddns_return_code", ret_buf);
	nvram_set ("ddns_return_code_chk", ret_buf);
	switch (ret) {
	case -1:
		PRINT ("strange server response, are you connecting to the right server?\n");
		retval = REGISTERES_ERROR;
		break;

	case 200:
		PRINT ("Registration success.\n");
		break;

	case 203:
		PRINT ("Registration failed. (203)\n");
		memset (sug_name, 0, sizeof (sug_name));
		sscanf (p, "|%[^|\r\n]c", sug_name);
		nvram_set ("ddns_suggest_name", sug_name);
		
		retval = REGISTERES_ERROR;
		break;

	case 220:
		PRINT ("Registration same domain success.\n");
		break;

	case 230:
		PRINT ("Registration new domain success.\n");
		memset (old_name, 0, sizeof (old_name));
		sscanf (p, "|%[^|\r\n]c", old_name);
		nvram_set ("ddns_old_name", old_name);
		break;

	case 233:
		PRINT ("Registration failed. (233)\n");
		memset (old_name, 0, sizeof (old_name));
		sscanf (p, "|%[^|\r\n]c", old_name);
		nvram_set ("ddns_old_name", old_name);
		retval = REGISTERES_ERROR;
		break;

	case 297:
		PRINT ("Invalid hostname.\n");
		retval = REGISTERES_ERROR;
		break;

	case 298:
		PRINT ("Invalid domain name.\n");
		retval = REGISTERES_ERROR;
		break;

	case 299:
		PRINT ("Invalid IP format.\n");
		retval = REGISTERES_ERROR;
		break;

	case 401:
		PRINT ("authentication failure\n");
		retval = REGISTERES_SHUTDOWN;
		break;

	case 407:
		PRINT ("Proxy authentication Required.\n");
		retval = REGISTERES_SHUTDOWN;
		break;

	default:
		// reuse the auth buffer
		*auth = '\0';
		sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
		if (ret >= 500)	{
			retval = REGISTERES_SHUTDOWN;
			nvram_set ("ddns_return_code","unknown_error");
			nvram_set ("ddns_return_code_chk","unknown_error");
		} else {
			retval = REGISTERES_ERROR;
			nvram_set ("ddns_return_code","Time-out");
			nvram_set ("ddns_return_code_chk","Time-out");
		}
		retval = UPDATERES_SHUTDOWN;
		break;
	}

	show_message("retval= %d, ddns_return_code (%s) ddns_suggest_name (%s) ddns_old_name (%s)", retval, nvram_safe_get ("ddns_return_code"), nvram_safe_get ("ddns_suggest_name"), nvram_safe_get ("ddns_old_name"));
	return (retval);
}


int asus_update_entry(void)
{
	char buf[BUFFER_SIZE + 1];
	char *p, *bp = buf;
	char ret_buf[64];
	char sug_name[MAX_DOMAIN_LEN], old_name[MAX_DOMAIN_LEN];
	int bytes;
	int btot;
	int ret;
	int retval = UPDATERES_OK;

	buf[BUFFER_SIZE] = '\0';

	if (do_connect((int *) &client_sockfd, server, port) != 0) {
		show_message("error connecting to %s:%s\n", server, port);
		nvram_set ("ddns_return_code", "connect_fail");
		nvram_set ("ddns_return_code_chk", "connect_fail");
		return (UPDATERES_ERROR);
	}

        char *ddns_transfer;
        char old_mac[13];
        memset(old_mac, 0, 13);
        if(TransferMacAddr(nvram_safe_get("ddns_transfer"), old_mac)) {
                snprintf(buf, BUFFER_SIZE, "GET /ddns/update.jsp?hostname=%s&myip=%s&oldmac=%s HTTP/1.0\015\012",
                         host, address, old_mac);
        }
        else {
                snprintf(buf, BUFFER_SIZE, "GET /ddns/update.jsp?hostname=%s&myip=%s HTTP/1.0\015\012", host, address);
        }
	output(buf);
	snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
	output(buf);
	snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
	output(buf);
	snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
	output(buf);
	snprintf(buf, BUFFER_SIZE, "\015\012");
	output(buf);

	bp = buf;
	bytes = 0;
	btot = 0;

        while ((bytes = read_input(bp, BUFFER_SIZE - btot)) > 0) {
                bp += bytes;
                btot += bytes;
        }
        close(client_sockfd);
        buf[btot] = '\0';
        show_message("Asus update entry:: return: %s\n", buf);
        if(btot) { // TODO: according to server response, parsing code have to rewrite
                if (sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1) {
                        ret = -1;
                }

                // looking for 2-th '|'
                p = strchr (buf, '|');
                if (p != NULL)
                        p = strchr (p+1, '|');
                if (p == NULL)  {
                        p = "";
                }
                snprintf (ret_buf, sizeof (ret_buf), "%s,%d", "", ret);
        }

	nvram_set ("ddns_return_code", ret_buf);
	nvram_set ("ddns_return_code_chk", ret_buf);
	switch (ret) {
	case -1:
		PRINT("strange server response, are you connecting to the right server?\n");
		retval = UPDATERES_ERROR;
		break;

	case 200:
		PRINT("Update IP successful\n");
		break;

        case 203:
                PRINT ("Update failed. (203)\n");
                memset (sug_name, 0, sizeof (sug_name));
                sscanf (p, "|%[^|\r\n]c", sug_name);
                nvram_set ("ddns_suggest_name", sug_name);

                retval = REGISTERES_ERROR;
                break;

        case 230:
                PRINT ("Update new domain success.\n");
                memset (old_name, 0, sizeof (old_name));
                sscanf (p, "|%[^|\r\n]c", old_name);
                nvram_set ("ddns_old_name", old_name);
                break;

        case 233:
                PRINT ("Update failed. (233)\n");
                memset (old_name, 0, sizeof (old_name));
                sscanf (p, "|%[^|\r\n]c", old_name);
                nvram_set ("ddns_old_name", old_name);
                retval = REGISTERES_ERROR;
                break;

	case 220:
		PRINT("Update same domain success.\n");
		break;

	case 297:
		PRINT("Invalid hostname\n");
		retval = UPDATERES_ERROR;
		break;

	case 298:
		PRINT("Invalid domain name\n");
		retval = UPDATERES_ERROR;
		break;

	case 299:
		PRINT("Invalid IP format\n");
		retval = UPDATERES_ERROR;
		break;

	case 401:
		PRINT("Authentication failure\n");
		retval = UPDATERES_SHUTDOWN;
		break;

	case 407:
		PRINT ("Proxy authentication Required.\n");
		retval = UPDATERES_SHUTDOWN;
		break;

	default:
		// reuse the auth buffer
		*auth = '\0';
		sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
		if (ret >= 500)	{
			retval = UPDATERES_SHUTDOWN;
		} else {
			retval = UPDATERES_ERROR;
		}
		retval = UPDATERES_SHUTDOWN;
		break;
	}

	show_message("retval= %d, ddns_return_code (%s)", retval, nvram_safe_get ("ddns_return_code"));
	return (retval);
}


static int
alnum_cpy (unsigned char *d, unsigned char *s, unsigned int max_dlen)
{
	unsigned char *old_d, *max_d;
	
	if (d == NULL || s == NULL || max_dlen == 0)
		return 0;

	old_d = d;
	max_d = d + max_dlen;
	while (*s != '\0' && d < max_d)	{
		if (isalnum (*s))
			*d++ = *s;
		s++;
	}

	// out of range
	if (d == max_d)	{
		*(d-1) = '\0';
	}

	return (int) (d - old_d);
}


#ifdef DUMP_KEY
static void dump (unsigned char *p, unsigned int l)
{
	unsigned int i;
	if (p == NULL || l == 0)
		return;

	printf ("0x");
	for (i = 0; i < l; ++i)
		printf ("%02x", p[i]);
	printf ("\n\n");
}
#endif	// DUMP_KEY


static int 
wl_wscPincheck(char *pin_string)
{
    unsigned long PIN = strtoul(pin_string, NULL, 10 );;
    unsigned long int accum = 0;
 
    accum += 3 * ((PIN / 10000000) % 10); 
    accum += 1 * ((PIN / 1000000) % 10); 
    accum += 3 * ((PIN / 100000) % 10); 
    accum += 1 * ((PIN / 10000) % 10); 
    accum += 3 * ((PIN / 1000) % 10); 
    accum += 1 * ((PIN / 100) % 10); 
    accum += 3 * ((PIN / 10) % 10); 
    accum += 1 * ((PIN / 1) % 10); 
 
    if (0 == (accum % 10))
	return 0;   // The PIN code is Vaild.
    else
	return 1;    // Invalid
}


// Generate password according to MAC address
int asus_private(void)
{
#define MAX_SECRET_CODE_LEN	8
	int i, l, c;
	unsigned long secret;
	unsigned char *p, user[256], hwaddr[6], hwaddr_str[18], key[64], msg[256], ipbuf[20], bin_pwd[16];

	memset (hwaddr, 0, sizeof (hwaddr));
	memset (key, 0, sizeof (key));
	memset (msg, 0, sizeof (msg));
	memset (user, 0, sizeof (user));
	memset (bin_pwd, 0, sizeof (bin_pwd));
#ifdef RTCONFIG_RGMII_BRCM5301X
	p = nvram_get ("et1macaddr");
#else
	if (get_model() == MODEL_RTN56U)
		p = nvram_get ("et1macaddr");
	else
	p = nvram_get ("et0macaddr");
#endif
	if (p == NULL)	{
		PRINT ("ERROR: %s() can not take MAC address from et0macaddr\n");
		return -1;
	}
	strncpy (hwaddr_str, p, sizeof (hwaddr_str));
	p = hwaddr_str;
	strtok (hwaddr_str, ":");
	for (i = 0; i < 6; ++i)	{
		if (p == NULL)	{
			PRINT ("ERROR: %s() can not convert MAC address\n", __FUNCTION__);
			return -1;
		}
		hwaddr[i] = strtoul (p, NULL, 16);
		p = strtok (NULL, ":");
	}
	//DBG ("MAC %02X:%02X:%02X:%02X:%02X:%02X\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);

	p = nvram_get ("secret_code");
	if (p == NULL)	{
		PRINT ("ERROR: secret_code not found.\n");
		return -1;
	}
	strncpy (key, p, sizeof (key));
	c = wl_wscPincheck (key);
	//DBG ("secret code (%s) is %s\n", key, (c == 0)?"valid":"INVALID");
	if (c)
		PRINT ("WARNING: invalid secret code (%s)?\n", key);

	DUMP (key);

	// If address is invalid or ez-ipupdate is running as daemon, take IP address from interface.
	if (address == NULL || *address == '\0' || options & OPT_DAEMON) {
#ifdef IF_LOOKUP
		struct sockaddr_in sin;
		int sock;

		if (interface == NULL)	{
			PRINT ("ERROR: %s() invalid address and interface\n", __FUNCTION__);
			return -1;
		}

		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (get_if_addr(sock, interface, &sin) != 0) {
			PRINT ("ERROR: %s() get IP address of interface fail\n", __FUNCTION__);
			return -1;
		}
		close(sock);
		snprintf(ipbuf, sizeof(ipbuf), "%s", inet_ntoa(sin.sin_addr));
		if (address)	{free(address);}
		address = strdup (ipbuf);
#else
#error interface lookup not enabled at compile time.
#endif
	} else {
		snprintf(ipbuf, sizeof(ipbuf), "%s", address);
	}

	// generate m
	i = alnum_cpy (msg, host, sizeof (msg));
	alnum_cpy (msg + i, ipbuf, sizeof (msg) - strlen (msg));
	//DBG ("%s(): m is (%s) len %d\n", __FUNCTION__, msg, strlen (msg));

	// generate password
	hm(msg, strlen (msg), key, 64, bin_pwd);

	//2007.03.19 Yau change snprintf -> sprintf
	// This function is used to override "auth"
	for (i = 0; i < 6; ++i)
		//snprintf (user, sizeof (user), "%s%02X", user, hwaddr[i]);
		sprintf (user, "%s%02X", user, hwaddr[i]);
	//snprintf (user, sizeof (user), "%s:", user);
	sprintf (user, "%s:", user);

	for (i = 0; i < 16; ++i)
		//snprintf (user, sizeof (user), "%s%02X", user, bin_pwd[i]);
		sprintf (user, "%s%02X", user, bin_pwd[i]);

	//DBG ("%s(): user and password (%s)\n", __FUNCTION__, user);
	base64Encode(user, auth);
	//PRINT("auth[] is overrode.\n");
	//DBG("new auth[] is (%s)\n", auth);

	return 0;
}
#endif	// TEST_HMAC_MD5
