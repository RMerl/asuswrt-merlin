/*
 * WPS Common Utility
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_utils.c 383924 2013-02-08 04:14:39Z $
 */

#include <ctype.h>
#include <wps_dh.h>
#include <wps_sha256.h>

#include <tutrace.h>
#include <wpstypes.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <reg_protomsg.h>
#include <sminfo.h>
#include <reg_proto.h>

char *wps_strncpy(char *dest, const char *src, size_t n)
{
	char *ret;

	if (dest == NULL || src == NULL)
		return NULL;

	ret = strncpy(dest, src, n);

	/* always set null termination at last byte */
	dest[n - 1] = 0;
	return ret;
}

uint32
wps_gen_pin(char *devPwd, int devPwd_len)
{
	unsigned long PIN;
	unsigned long int accum = 0;
	unsigned char rand_bytes[8];
	int digit;
	char local_devPwd[32];


	/*
	 * buffer size needs to big enough to hold 8 digits plus the string terminition
	 * character '\0'
	*/
	if (devPwd_len < 9)
		return WPS_ERR_BUFFER_TOO_SMALL;

	/* Generate random bytes and compute the checksum */
	RAND_bytes(rand_bytes, 8);
	sprintf(local_devPwd, "%08u", *(uint32 *)rand_bytes);
	local_devPwd[7] = '\0';
	PIN = strtoul(local_devPwd, NULL, 10);

	PIN *= 10;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);

	digit = (accum % 10);
	accum = (10 - digit) % 10;

	PIN += accum;
	sprintf(local_devPwd, "%08u", (unsigned int)PIN);
	local_devPwd[8] = '\0';

	/* Output result */
	strncpy(devPwd, local_devPwd, devPwd_len);

	return WPS_SUCCESS;
}

bool
wps_validate_pin(char *pinStr)
{
	char pin[9];
	uint32 pinLen;
	unsigned long int pinNum;
	unsigned long int accum = 0;

	if (pinStr == NULL)
		return FALSE;

	/* WSC 2.0, session 7.4.3 "1234-5670" */
	if (strlen(pinStr) == 9 &&
	    (pinStr[4] < '0' || pinStr[4] > '9')) {
		/* remove middle character */
		memcpy(pin, pinStr, 4);
		memcpy(pin+4, pinStr+5, 4);
	}
	else
		strncpy(pin, pinStr, 8);

	pin[8] = '\0';
	pinLen = (uint32)strlen(pin);

	/* Checksum validation is not necessary */
	if (pinLen == 4)
		return TRUE;

	if (pinLen != 8)
		return FALSE;

	pinNum = strtoul(pin, NULL, 10);

	/* Do checksum */
	accum += 3 * ((pinNum / 10000000) % 10);
	accum += 1 * ((pinNum / 1000000) % 10);
	accum += 3 * ((pinNum / 100000) % 10);
	accum += 1 * ((pinNum / 10000) % 10);
	accum += 3 * ((pinNum / 1000) % 10);
	accum += 1 * ((pinNum / 100) % 10);
	accum += 3 * ((pinNum / 10) % 10);
	accum += 1 * ((pinNum / 1) % 10);

	return ((accum % 10) == 0);
}

bool
wps_gen_ssid(char *ssid, int ssid_len, char *prefix, char *hwaddr)
{
	int i;
	char mac[18] = {0};
	unsigned short ssid_length;
	unsigned char random_ssid[SIZE_SSID_LENGTH] = {0};

	if (ssid == NULL || ssid_len == 0)
		return FALSE;

	if (hwaddr)
		strncpy(mac, hwaddr, sizeof(mac));

	RAND_bytes((unsigned char *)&ssid_length, sizeof(ssid_length));
	ssid_length = (unsigned short)((((long)ssid_length + 56791)*13579)%8) + 1;

	RAND_bytes((unsigned char *)random_ssid, ssid_length);

	for (i = 0; i < ssid_length; i++) {
		if ((random_ssid[i] < 48) || (random_ssid[i] > 57))
			random_ssid[i] = random_ssid[i]%9 + 48;
	}

	random_ssid[ssid_length++] = tolower(mac[6]);
	random_ssid[ssid_length++] = tolower(mac[7]);
	random_ssid[ssid_length++] = tolower(mac[9]);
	random_ssid[ssid_length++] = tolower(mac[10]);
	random_ssid[ssid_length++] = tolower(mac[12]);
	random_ssid[ssid_length++] = tolower(mac[13]);
	random_ssid[ssid_length++] = tolower(mac[15]);
	random_ssid[ssid_length++] = tolower(mac[16]);

	memset(ssid, 0, ssid_len);
	if (prefix)
		WPS_snprintf(ssid, ssid_len - 1, "%s", prefix);
	else
		WPS_snprintf(ssid, ssid_len - 1, "%s", "ASUS_");

	strncat(ssid, (char *)random_ssid, SIZE_SSID_LENGTH - strlen(ssid) - 1);

	return TRUE;
}

/* generate a printable key string */
bool
wps_gen_key(char *key, int key_len)
{
	unsigned short key_length;
	unsigned char random_key[64] = {0};
	int i = 0;

	if (key == NULL || key_len == 0)
		return FALSE;

	/* key_length < 16 */
	RAND_bytes((unsigned char *)&key_length, sizeof(key_length));
	key_length = (unsigned short)((((long)key_length + 56791)*13579)%8) + 8;

	while (i < key_length) {
		RAND_bytes(&random_key[i], 1);
		if ((islower(random_key[i]) || isdigit(random_key[i])) &&
			(random_key[i] < 0x7f)) {
			i++;
		}
	}

	wps_strncpy(key, (char *)random_key, key_len);

	return TRUE;
}

/* Generate a OOB (Out-Of-Band) Device Password
 * Public key 192 bytes
 * 2bytes Device Password ID between 0x0010~0xFFFF
 * 16 ~ 32 bytes Device Password.
 */
void
wps_gen_oob_dev_pw(unsigned char *pre_privkey, unsigned char *pub_key_hash,
	unsigned short *dev_pw_id, unsigned char *pin, int pin_size)
{
	/* Device Password */
	if (pin_size < 16) {
		TUTRACE((TUTRACE_ERR, "DevInfo pin element size %d too short\n", pin_size));
	}
	if (pin_size > 32) {
		TUTRACE((TUTRACE_ERR, "DevInfo pin element size %d too big\n", pin_size));
	}

	/* Private key */
	if (pre_privkey && pub_key_hash)
		reg_proto_generate_priv_key(pre_privkey, pub_key_hash);

	/* Device Password ID */
	if (dev_pw_id) {
		RAND_bytes((unsigned char *)dev_pw_id, sizeof(unsigned short));
		if (*dev_pw_id < 0x10)
			*dev_pw_id += 0x10;
	}

	/* Device Password */
	if (pin && pin_size)
		RAND_bytes((unsigned char *)pin, pin_size);

	return;
}

char *
wps_hex2str(char *str, int slen, unsigned char *hex, int hlen)
{
	int i;
	char *sptr = str;

	if (!hex || !str || !hlen || !slen)
		return NULL;

	if (hlen * 2 >= slen)
		return NULL;

	for (i = 0; i < hlen; i++) {
		sprintf(sptr, "%.2X", hex[i]);
		sptr += 2;
	}

	sptr = '\0';

	return str;
}

int
wps_str2hex(unsigned char *hex, int hlen, char *str)
{
	int i, slen;
	long int hvalue;
	char tmp[3] = {0};
	char *aptr = str;

	if (!hex || !str || !hlen)
		return 0;

	slen = strlen(str);
	if (hlen * 2 < slen)
		return 0;

	if (slen % 2)
		return 0;

	for (i = 0; i < slen; i += 2) {
		memcpy(tmp, aptr, 2);
		hvalue = strtol(tmp, NULL, 16);
		*hex++ = (unsigned char)hvalue;
		aptr += 2;
	}

	return (slen / 2);
}
