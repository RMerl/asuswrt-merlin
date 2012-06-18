/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/
#include <stdio.h>
#include <string.h>

/*

111111  110000  000011  111111
0         1         2

echo -n "aaa" | uuencode -m -
begin-base64 644 -
YWFh

echo -n "a" | uuencode -m -
begin-base64 644 -
YQ==

echo -n "aa" | uuencode -m -
begin-base64 644 -
YWE=

*/

static const char base64_xlat[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


int base64_encode(const unsigned char *in, char *out, int inlen)
{
	char *o;

	o = out;
	while (inlen >= 3) {
		*out++ = base64_xlat[*in >> 2];
		*out++ = base64_xlat[((*in << 4) & 0x3F) | (*(in + 1) >> 4)];
		++in;	// note: gcc complains if *(++in)
		*out++ = base64_xlat[((*in << 2) & 0x3F) | (*(in + 1) >> 6)];
		++in;
		*out++ = base64_xlat[*in++ & 0x3F];
		inlen -= 3;
	}
	if (inlen > 0) {
		*out++ = base64_xlat[*in >> 2];
		if (inlen == 2) {
			*out++ = base64_xlat[((*in << 4) & 0x3F) | (*(in + 1) >> 4)];
			++in;
			*out++ = base64_xlat[((*in << 2) & 0x3F)];
		}
		else {
			*out++ = base64_xlat[(*in << 4) & 0x3F];
			*out++ = '=';
		}
		*out++ = '=';
	}
	return out - o;
}


int base64_decode(const char *in, unsigned char *out, int inlen)
{
	char *p;
	int n;
	unsigned long x;
	unsigned char *o;
	char c;

	o = out;
	n = 0;
	x = 0;
	while (inlen-- > 0) {
		if (*in == '=') break;
		if ((p = strchr(base64_xlat, c = *in++)) == NULL) {
//			printf("ignored - %x %c\n", c, c);
			continue;	// note: invalid characters are just ignored
		}
		x = (x << 6) | (p - base64_xlat);
		if (++n == 4) {
			*out++ = x >> 16;
			*out++ = (x >> 8) & 0xFF;
			*out++ = x & 0xFF;
			n = 0;
			x = 0;
		}
	}
	if (n == 3) {
		*out++ = x >> 10;
		*out++ = (x >> 2) & 0xFF;
	}
	else if (n == 2) {
		*out++ = x >> 4;
	}
	return out - o;
}

int base64_encoded_len(int len)
{
	return ((len + 2) / 3) * 4;
}

int base64_decoded_len(int len)
{
	return ((len + 3) / 4) * 3;
}

/*
int main(int argc, char **argv)
{
	char *test = "a";
	char buf[128];
	char buf2[128];
	const char *in;
	int len;

	memset(buf, 0, sizeof(buf));
	base64_encode(test, buf, strlen(test));
	printf("buf=%s\n", buf);

	memset(buf2, 0, sizeof(buf2));
	base64_decode(buf, buf2, base64_encoded_len(strlen(test)));
	printf("buf2=%s\n", buf2);
}
*/
