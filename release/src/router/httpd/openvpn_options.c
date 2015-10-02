#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>

#include <shared.h>
#include "httpd.h"
#include "openvpn_options.h"
#define TYPEDEF_BOOL
#include <bcmnvram.h>
#include <shared.h>
#include "shutils.h"

struct buffer
alloc_buf (size_t size)
{
	struct buffer buf;

	if (!buf_size_valid (size))
		buf_size_error (size);
	buf.capacity = (int)size;
	buf.offset = 0;
	buf.len = 0;
	buf.data = calloc (1, size);

	return buf;
}

void
buf_size_error (const size_t size)
{
	logmessage ("OVPN", "fatal buffer size error, size=%lu", (unsigned long)size);
}

bool
buf_printf (struct buffer *buf, const char *format, ...)
{
	int ret = false;
	if (buf_defined (buf))
	{
		va_list arglist;
		uint8_t *ptr = buf_bend (buf);
		int cap = buf_forward_capacity (buf);

		if (cap > 0)
		{
			int stat;
			va_start (arglist, format);
			stat = vsnprintf ((char *)ptr, cap, format, arglist);
			va_end (arglist);
			*(buf->data + buf->capacity - 1) = 0; /* windows vsnprintf needs this */
			buf->len += (int) strlen ((char *)ptr);
			if (stat >= 0 && stat < cap)
				ret = true;
		}
	}
	return ret;
}

bool
buf_parse (struct buffer *buf, const int delim, char *line, const int size)
{
	bool eol = false;
	int n = 0;
	int c;

	do
	{
		c = buf_read_u8 (buf);
		if (c < 0)
			eol = true;
		if (c <= 0 || c == delim)
			c = 0;
		if (n >= size)
			break;
		line[n++] = c;
	}
	while (c);

	line[size-1] = '\0';
	return !(eol && !strlen (line));
}

void
buf_clear (struct buffer *buf)
{
	if (buf->capacity > 0)
		memset (buf->data, 0, buf->capacity);
	buf->len = 0;
	buf->offset = 0;
}

void
free_buf (struct buffer *buf)
{
	if (buf->data)
		free (buf->data);
	CLEAR (*buf);
}

char *
string_alloc (const char *str)
{
	if (str)
	{
		const int n = strlen (str) + 1;
		char *ret;

		ret = calloc(n, 1);
		if(!ret)
			return NULL;

		memcpy (ret, str, n);
		return ret;
	}
	else
		return NULL;
}

static inline bool
space (unsigned char c)
{
	return c == '\0' || isspace (c);
}

int
parse_line (const char *line, char *p[], const int n, const int line_num)
{
	const int STATE_INITIAL = 0;
	const int STATE_READING_QUOTED_PARM = 1;
	const int STATE_READING_UNQUOTED_PARM = 2;
	const int STATE_DONE = 3;
	const int STATE_READING_SQUOTED_PARM = 4;

	int ret = 0;
	const char *c = line;
	int state = STATE_INITIAL;
	bool backslash = false;
	char in, out;

	char parm[OPTION_PARM_SIZE];
	unsigned int parm_len = 0;

	do
	{
		in = *c;
		out = 0;

		if (!backslash && in == '\\' && state != STATE_READING_SQUOTED_PARM)
		{
			backslash = true;
		}
		else
		{
			if (state == STATE_INITIAL)
			{
				if (!space (in))
				{
					if (in == ';' || in == '#') /* comment */
						break;
					if (!backslash && in == '\"')
						state = STATE_READING_QUOTED_PARM;
					else if (!backslash && in == '\'')
						state = STATE_READING_SQUOTED_PARM;
					else
					{
						out = in;
						state = STATE_READING_UNQUOTED_PARM;
					}
				}
			}
			else if (state == STATE_READING_UNQUOTED_PARM)
			{
				if (!backslash && space (in))
					state = STATE_DONE;
				else
					out = in;
			}
			else if (state == STATE_READING_QUOTED_PARM)
			{
				if (!backslash && in == '\"')
					state = STATE_DONE;
				else
					out = in;
			}
			else if (state == STATE_READING_SQUOTED_PARM)
			{
				if (in == '\'')
					state = STATE_DONE;
				else
					out = in;
			}

			if (state == STATE_DONE)
			{
				p[ret] = calloc (parm_len + 1, 1);
				memcpy (p[ret], parm, parm_len);
				p[ret][parm_len] = '\0';
				state = STATE_INITIAL;
				parm_len = 0;
				++ret;
			}

			if (backslash && out)
			{
				if (!(out == '\\' || out == '\"' || space (out)))
				{
					logmessage ("OVPN", "Options warning: Bad backslash ('\\') usage in %d", line_num);
					return 0;
				}
			}
			backslash = false;
		}

		/* store parameter character */
		if (out)
		{
			if (parm_len >= SIZE (parm))
			{
				parm[SIZE (parm) - 1] = 0;
				logmessage ("OVPN", "Options error: Parameter at %d is too long (%d chars max): %s",
					line_num, (int) SIZE (parm), parm);
				return 0;
			}
			parm[parm_len++] = out;
		}

		/* avoid overflow if too many parms in one config file line */
		if (ret >= n)
			break;

	} while (*c++ != '\0');


	if (state == STATE_READING_QUOTED_PARM)
	{
		logmessage ("OVPN", "Options error: No closing quotation (\") in %d", line_num);
		return 0;
	}
	if (state == STATE_READING_SQUOTED_PARM)
	{
		logmessage ("OVPN", "Options error: No closing single quotation (\') in %d", line_num);
		return 0;
	}
	if (state != STATE_INITIAL)
	{
		logmessage ("OVPN", "Options error: Residual parse state (%d) in %d", line_num);
		return 0;
	}

	return ret;
}

static void
bypass_doubledash (char **p)
{
	if (strlen (*p) >= 3 && !strncmp (*p, "--", 2))
		*p += 2;
}

static bool
in_src_get (const struct in_src *is, char *line, const int size)
{
	if (is->type == IS_TYPE_FP)
	{
		return BOOL_CAST (fgets (line, size, is->u.fp));
	}
	else if (is->type == IS_TYPE_BUF)
	{
		bool status = buf_parse (is->u.multiline, '\n', line, size);
		if ((int) strlen (line) + 1 < size)
			strcat (line, "\n");
		return status;
	}
	else
	{
		return false;
	}
}

static char *
read_inline_file (struct in_src *is, const char *close_tag)
{
	char line[OPTION_LINE_SIZE];
	struct buffer buf = alloc_buf (10000);
	char *ret;
	while (in_src_get (is, line, sizeof (line)))
	{
		if (!strncmp (line, close_tag, strlen (close_tag)))
			break;
		buf_printf (&buf, "%s", line);
	}
	ret = string_alloc (buf_str (&buf));
	buf_clear (&buf);
	free_buf (&buf);
	CLEAR (line);
	return ret;
}

static bool
check_inline_file (struct in_src *is, char *p[])
{
	bool ret = false;
	if (p[0] && !p[1])
	{
		char *arg = p[0];
		if (arg[0] == '<' && arg[strlen(arg)-1] == '>')
		{
			struct buffer close_tag;
			arg[strlen(arg)-1] = '\0';
			p[0] = string_alloc (arg+1);
			p[1] = string_alloc (INLINE_FILE_TAG);
			close_tag = alloc_buf (strlen(p[0]) + 4);
			buf_printf (&close_tag, "</%s>", p[0]);
			p[2] = read_inline_file (is, buf_str (&close_tag));
			p[3] = NULL;
			free_buf (&close_tag);
			ret = true;
		}
	}
	return ret;
}

static bool
check_inline_file_via_fp (FILE *fp, char *p[])
{
	struct in_src is;
	is.type = IS_TYPE_FP;
	is.u.fp = fp;
	return check_inline_file (&is, p);
}

void
add_custom(char *nv, char *p[])
{
	char *custom = nvram_safe_get(nv);
	char *param = NULL;
	char *final_custom = NULL;
	int i = 0, size = 0;

	if(!p[0])
		return;

	while(p[i]) {
		size += strlen(p[i]) + 1;
		i++;
	}

	param = (char*)calloc(size, sizeof(char));

	if(!param)
		return;

	i = 0;
	while(p[i]) {
		if(*param)
			strcat(param, " ");
		strcat(param, p[i]);
		i++;
	}

	if(custom) {
		final_custom = calloc(strlen(custom) + strlen(param) + 2, sizeof(char));
		if(final_custom) {
			if(*custom) {
				strcat(final_custom, custom);
				strcat(final_custom, "\n");
			}
			strcat(final_custom, param);
			nvram_set(nv, final_custom);
			free(final_custom);
		}
	}
	else
		nvram_set(nv, param);

	free(param);
}

static int
add_option (char *p[], int line, int unit)
{
	char buf[32] = {0};
	FILE *fp;
	char file_path[128] ={0};

	if  (streq (p[0], "dev") && p[1])
	{
		sprintf(buf, "vpn_client%d_if", unit);
		if(!strncmp(p[1], "tun", 3))
			nvram_set(buf, "tun");
		else if(!strncmp(p[1], "tap", 3))
			nvram_set(buf, "tap");
	}
	else if  (streq (p[0], "proto") && p[1])
	{
		sprintf(buf, "vpn_client%d_proto", unit);
		nvram_set(buf, p[1]);
	}
	else if  (streq (p[0], "remote") && p[1])
	{
		sprintf(buf, "vpn_client%d_addr", unit);
		nvram_set(buf, p[1]);

		sprintf(buf, "vpn_client%d_port", unit);
		if(p[2])
			nvram_set(buf, p[2]);
		else
			nvram_set(buf, "1194");
	}
	else if (streq (p[0], "resolv-retry") && p[1])
	{
		sprintf(buf, "vpn_client%d_retry", unit);
		if (streq (p[1], "infinite"))
			nvram_set(buf, "-1");
		else
			nvram_set(buf, p[1]);
	}
	else if (streq (p[0], "comp-lzo"))
	{
		sprintf(buf, "vpn_client%d_comp", unit);
		if(p[1])
			nvram_set(buf, p[1]);
		else
			nvram_set(buf, "adaptive");
	}
	else if (streq (p[0], "cipher") && p[1])
	{
		sprintf(buf, "vpn_client%d_cipher", unit);
		nvram_set(buf, p[1]);
	}
	else if (streq (p[0], "redirect-gateway") && (!p[1] || streq (p[1], "def1")))	// Only handle if default GW
	{
		sprintf(buf, "vpn_client%d_rgw", unit);
		nvram_set(buf, "1");
	}
	else if (streq (p[0], "verb") && p[1])
	{
		nvram_set("vpn_loglevel", p[1]);
	}
	else if  (streq (p[0], "ca") && p[1])
	{
		sprintf(buf, "vpn_client%d_crypt", unit);
		nvram_set(buf, "tls");
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_ca", unit);
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, buf);
			fp = fopen(file_path, "w");
			if(fp) {
				chmod(file_path, S_IRUSR|S_IWUSR);
				fprintf(fp, "%s", strstr(p[2], "-----BEGIN"));
				fclose(fp);
			}
			else
#endif
			write_encoded_crt(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_CA_CERT;
		}
	}
	else if  (streq (p[0], "cert") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_crt", unit);
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, buf);
			fp = fopen(file_path, "w");
			if(fp) {
				chmod(file_path, S_IRUSR|S_IWUSR);
				fprintf(fp, "%s", strstr(p[2], "-----BEGIN"));
				fclose(fp);
			}
			else
#endif
			write_encoded_crt(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_CERT;
		}
	}
	else if  (streq (p[0], "key") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_key", unit);
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, buf);
			fp = fopen(file_path, "w");
			if(fp) {
				chmod(file_path, S_IRUSR|S_IWUSR);
				fprintf(fp, "%s", strstr(p[2], "-----BEGIN"));
				fclose(fp);
			}
			else
#endif
			write_encoded_crt(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_KEY;
		}
	}
	else if (streq (p[0], "tls-auth") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_static", unit);
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, buf);
			fp = fopen(file_path, "w");
			if(fp) {
				chmod(file_path, S_IRUSR|S_IWUSR);
				fprintf(fp, "%s", strstr(p[2], "-----BEGIN"));
				fclose(fp);
			}
			else
#endif
			write_encoded_crt(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			if(p[2]) {
				sprintf(buf, "vpn_client%d_hmac", unit);
				nvram_set(buf, p[2]);
			}
			return VPN_UPLOAD_NEED_STATIC;
		}
	}
	else if (streq (p[0], "secret") && p[1])
	{
		sprintf(buf, "vpn_client%d_crypt", unit);
		nvram_set(buf, "secret");
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_static", unit);
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, buf);
			fp = fopen(file_path, "w");
			if(fp) {
				chmod(file_path, S_IRUSR|S_IWUSR);
				fprintf(fp, "%s", strstr(p[2], "-----BEGIN"));
				fclose(fp);
			}
			else
#endif
			write_encoded_crt(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_STATIC;
		}
	}
	else if (streq (p[0], "extra-certs") && p[1])
	{
		if (streq (p[1], INLINE_FILE_TAG) && p[2])
		{
			sprintf(buf, "vpn_crt_client%d_extra", unit);
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
			snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, buf);
			fp = fopen(file_path, "w");
			if(fp) {
				chmod(file_path, S_IRUSR|S_IWUSR);
				fprintf(fp, "%s", strstr(p[2], "-----BEGIN"));
				fclose(fp);
			}
			else
#endif
			write_encoded_crt(buf, strstr(p[2], "-----BEGIN"));
		}
		else
		{
			return VPN_UPLOAD_NEED_EXTRA;
		}
	}

	else if (streq (p[0], "auth-user-pass"))
	{
		sprintf(buf, "vpn_client%d_userauth", unit);
		nvram_set(buf, "1");
	}
	else if (streq (p[0], "tls-remote") && p[1])
	{
		sprintf(buf, "vpn_client%d_tlsremote", unit);
		nvram_set(buf, "1");
		sprintf(buf, "vpn_client%d_cn", unit);
		nvram_set(buf, p[1]);
	}
	else if (streq (p[0], "key-direction") && p[1])
	{
		sprintf(buf, "vpn_client%d_hmac", unit);
		nvram_set(buf, p[1]);
	}
	// These are already added by us
	else if (streq (p[0], "client") ||
		 streq (p[0], "nobind") ||
		 streq (p[0], "persist-key") ||
		 streq (p[0], "persist-tun"))
	{
		return 0;	// Don't duplicate them
	}
	else if (streq (p[0], "crl-verify") && p[1])
	{
		if (p[2] && streq(p[2], "dir"))
			;//TODO: not support?
		return VPN_UPLOAD_NEED_CRL;
	}
	else
	{
		sprintf(buf, "vpn_client%d_custom", unit);
		add_custom(buf, p);
	}
	return 0;
}

int
read_config_file (const char *file, int unit)
{
	FILE *fp;
	int line_num;
	char line[OPTION_LINE_SIZE];
	char *p[MAX_PARMS];
	int ret = 0;

	fp = fopen (file, "r");
	if (fp)
	{
		line_num = 0;
		while (fgets(line, sizeof (line), fp))
		{
			int offset = 0;
			CLEAR (p);
			++line_num;
			/* Ignore UTF-8 BOM at start of stream */
			if (line_num == 1 && strncmp (line, "\xEF\xBB\xBF", 3) == 0)
				offset = 3;
			if (parse_line (line + offset, p, SIZE (p), line_num))
			{
				bypass_doubledash (&p[0]);
				check_inline_file_via_fp (fp, p);
				ret |= add_option (p, line_num, unit);
			}
		}
		fclose (fp);
	}
	else
	{
		logmessage ("OVPN", "Error opening configuration file");
	}

	CLEAR (line);
	CLEAR (p);

	return ret;
}

void reset_client_setting(int unit){
	char nv[32];
	char file_path[128] ={0};

	sprintf(nv, "vpn_client%d_custom", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_comp", unit);
	nvram_set(nv, "no");
	sprintf(nv, "vpn_client%d_reneg", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_client%d_hmac", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_client%d_retry", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_client%d_cipher", unit);
	nvram_set(nv, "default");
	sprintf(nv, "vpn_client%d_rgw", unit);
	nvram_set(nv, "0");
	sprintf(nv, "vpn_client%d_tlsremote", unit);
	nvram_set(nv, "0");
	sprintf(nv, "vpn_client%d_cn", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_userauth", unit);
	nvram_set(nv, "0");
	sprintf(nv, "vpn_client%d_username", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_password", unit);
	nvram_set(nv, "");
	sprintf(nv, "vpn_client%d_comp", unit);
	nvram_set(nv, "-1");
	sprintf(nv, "vpn_crt_client%d_ca", unit);
	nvram_set(nv, "");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, nv);
	unlink(file_path);
#endif
	sprintf(nv, "vpn_crt_client%d_crt", unit);
	nvram_set(nv, "");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, nv);
	unlink(file_path);
#endif
	sprintf(nv, "vpn_crt_client%d_key", unit);
	nvram_set(nv, "");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, nv);
	unlink(file_path);
#endif
	sprintf(nv, "vpn_crt_client%d_static", unit);
	nvram_set(nv, "");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, nv);
	unlink(file_path);
#endif
	sprintf(nv, "vpn_crt_client%d_crl", unit);
	nvram_set(nv, "");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, nv);
	unlink(file_path);
#endif
	sprintf(nv, "vpn_crt_client%d_extra", unit);
	nvram_set(nv, "");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	snprintf(file_path, sizeof(file_path) -1, "%s/%s", OVPN_FS_PATH, nv);
	unlink(file_path);
#endif
}

void parse_openvpn_status(int unit){
	FILE *fpi, *fpo;
	char buf[512];
	char *token;
	char nv_name[32] = "";
	char prefix_vpn[] = "vpn_serverXX_";

	sprintf(buf, "/etc/openvpn/server%d/status", unit);
	fpi = fopen(buf, "r");

	sprintf(buf, "/etc/openvpn/server%d/client_status", unit);
	fpo = fopen(buf, "w");

	snprintf(prefix_vpn, sizeof(prefix_vpn), "vpn_server%d_", unit);

	if(fpi && fpo) {
		while(!feof(fpi)){
			CLEAR(buf);
			if (!fgets(buf, sizeof(buf), fpi))
				break;
			if(!strncmp(buf, "CLIENT_LIST", 11)) {
				//printf("%s", buf);
				token = strtok(buf, ",");	//CLIENT_LIST
				token = strtok(NULL, ",");	//Common Name
				token = strtok(NULL, ",");	//Real Address
				if(token)
					fprintf(fpo, "%s ", token);
				else
					fprintf(fpo, "NoRealAddress ");
				snprintf(nv_name, sizeof(nv_name) -1, "vpn_server%d_if", unit);

				if(nvram_match(strcat_r(prefix_vpn, "if", nv_name), "tap")
					&& nvram_match(strcat_r(prefix_vpn, "dhcp", nv_name), "1")) {
					fprintf(fpo, "VirtualAddressAssignedByDhcp ");
				}
				else {
					token = strtok(NULL, ",");	//Virtual Address
					if(token)
						fprintf(fpo, "%s ", token);
					else
						fprintf(fpo, "NoVirtualAddress ");
				}
				token = strtok(NULL, ",");	//Bytes Received
				token = strtok(NULL, ",");	//Bytes Sent
				token = strtok(NULL, ",");	//Connected Since
				token = strtok(NULL, ",");	//Connected Since (time_t)
				token = strtok(NULL, ",");	//Username, include'\n'
				if(token)
					fprintf(fpo, "%s", token);
				else
					fprintf(fpo, "NoUsername");
			}
		}
		fclose(fpi);
		fclose(fpo);
	}
}

