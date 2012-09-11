/*
 * Taken from:
 * WPA Supplicant / Configuration parser and common functions
 * Copyright (c) 2003-2008, Jouni Malinen <j@w1.fi>
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "config.h" 

char * rel2abs_path(const char *rel_path)
{
	char *buf = NULL, *cwd, *ret;
	size_t len = 128, cwd_len, rel_len, ret_len;
	int last_errno;

	if (rel_path[0] == '/')
		return strdup(rel_path);

	for (;;) {
		buf = malloc(len);
		if (buf == NULL)
			return NULL;
		cwd = getcwd(buf, len);
		if (cwd == NULL) {
			last_errno = errno;
			free(buf);
			if (last_errno != ERANGE)
				return NULL;
			len *= 2;
			if (len > 2000)
				return NULL;
		} else {
			buf[len - 1] = '\0';
			break;
		}
	}

	cwd_len = strlen(cwd);
	rel_len = strlen(rel_path);
	ret_len = cwd_len + 1 + rel_len + 1;
	ret = malloc(ret_len);
	if (ret) {
		memcpy(ret, cwd, cwd_len);
		ret[cwd_len] = '/';
		memcpy(ret + cwd_len + 1, rel_path, rel_len);
		ret[ret_len - 1] = '\0';
	}
	free(buf);
	return ret;
}


struct parse_data {
	/* Configuration variable name */
	char *name;

	/* Parser function for this variable */
	int (*parser)(const struct parse_data *data, struct gct_config *config,
		      int line, const char *value);

	/* Variable specific parameters for the parser. */
	void *param1, *param2, *param3, *param4;

	/* 0 = this variable can be included in debug output and ctrl_iface
	 * 1 = this variable contains key/private data and it must not be
	 *     included in debug output unless explicitly requested. In
	 *     addition, this variable will not be readable through the
	 *     ctrl_iface.
	 */
	int key_data;
};


static 
char * gct_config_parse_string(const char *value, size_t *len)
{
	if (*value == '"') {
		const char *pos;
		char *str;
		value++;
		pos = strrchr(value, '"');
		if (pos == NULL || pos[1] != '\0')
			return NULL;
		*len = pos - value;
		str = malloc(*len + 1);
		if (str == NULL)
			return NULL;
		memcpy(str, value, *len);
		str[*len] = '\0';
		return str;
	} else {
		u8 *str;
		size_t tlen, hlen = strlen(value);
		if (hlen & 1)
			return NULL;
		tlen = hlen / 2;
		str = malloc(tlen + 1);
		if (str == NULL)
			return NULL;
		if (hexstr2bin(value, str, tlen)) {
			free(str);
			return NULL;
		}
		str[tlen] = '\0';
		*len = tlen;
		return (char *) str;
	}
}


static 
int gct_config_parse_str(const struct parse_data *data,
				struct gct_config *config,
				int line, const char *value)
{
	size_t res_len, *dst_len;
	char **dst, *tmp;

	if (strcmp(value, "NULL") == 0) {
		wmlog_msg(4, "Unset configuration string '%s'",
			   data->name);
		tmp = NULL;
		res_len = 0;
		goto set;
	}

	tmp = gct_config_parse_string(value, &res_len);
	if (tmp == NULL) {
		wmlog_msg(0, "Line %d: failed to parse %s '%s'.",
			   line, data->name,
			   data->key_data ? "[KEY DATA REMOVED]" : value);
		return -1;
	}
	wmlog_dumphexasc(4, (u8 *) tmp, res_len, "%s", data->name);
/*
	if (data->key_data) {
		gct_hexdump_ascii_key(MSG_MSGDUMP, data->name,
				      (u8 *) tmp, res_len);
	} else {
		gct_hexdump_ascii(MSG_MSGDUMP, data->name,
				  (u8 *) tmp, res_len);
	}
*/
	if (data->param3 && res_len < (size_t) data->param3) {
		wmlog_msg(0, "E: Line %d: too short %s (len=%lu "
			   "min_len=%ld)", line, data->name,
			   (unsigned long) res_len, (long) data->param3);
		free(tmp);
		return -1;
	}

	if (data->param4 && res_len > (size_t) data->param4) {
		wmlog_msg(0, "E: Line %d: too long %s (len=%lu "
			   "max_len=%ld)", line, data->name,
			   (unsigned long) res_len, (long) data->param4);
		free(tmp);
		return -1;
	}

set:
	dst = (char **) (((u8 *) config) + (long) data->param1);
	dst_len = (size_t *) (((u8 *) config) + (long) data->param2);
	free(*dst);
	*dst = tmp;
	if (data->param2)
		*dst_len = res_len;

	return 0;
}


static 
int gct_config_parse_int(const struct parse_data *data,
				struct gct_config *config,
				int line, const char *value)
{
	int *dst;

	dst = (int *) (((u8 *) config) + (long) data->param1);
	*dst = atoi(value);
	wmlog_msg(4, "%s=%d (0x%x)", data->name, *dst, *dst);

	if (data->param3 && *dst < (long) data->param3) {
		wmlog_msg(0, "E: Line %d: too small %s (value=%d "
			   "min_value=%ld)", line, data->name, *dst,
			   (long) data->param3);
		*dst = (long) data->param3;
		return -1;
	}

	if (data->param4 && *dst > (long) data->param4) {
		wmlog_msg(0, "E: Line %d: too large %s (value=%d "
			   "max_value=%ld)", line, data->name, *dst,
			   (long) data->param4);
		*dst = (long) data->param4;
		return -1;
	}

	return 0;
}

static 
int gct_config_parse_eap(const struct parse_data *data,
				struct gct_config *config, int line,
				const char *value)
{
	int last, errors = 0;
	char *start, *end, *buf;
	struct eap_method_type *methods = NULL, *tmp;
	size_t num_methods = 0;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		tmp = methods;
		methods = realloc(methods,
				     (num_methods + 1) * sizeof(*methods));
		if (methods == NULL) {
			free(tmp);
			free(buf);
			return -1;
		}
		methods[num_methods].method = eap_peer_get_type(
			start, &methods[num_methods].vendor);
		if (methods[num_methods].vendor == EAP_VENDOR_IETF &&
		    methods[num_methods].method == EAP_TYPE_NONE) {
			wmlog_msg(0, "Line %d: unknown EAP method "
				   "'%s'", line, start);
			wmlog_msg(0, "You may need to add support for"
				   " this EAP method during wpa_supplicant\n"
				   "build time configuration.\n"
				   "See README for more information.");
			errors++;
		}/* else if (methods[num_methods].vendor == EAP_VENDOR_IETF &&
			   methods[num_methods].method == EAP_TYPE_LEAP)
			ssid->leap++;
		else
			ssid->non_leap++;*/
		num_methods++;
		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	tmp = methods;
	methods = realloc(methods, (num_methods + 1) * sizeof(*methods));
	if (methods == NULL) {
		free(tmp);
		return -1;
	}
	methods[num_methods].vendor = EAP_VENDOR_IETF;
	methods[num_methods].method = EAP_TYPE_NONE;
	num_methods++;

	wmlog_dumphexasc(4, (u8 *) methods, 
		num_methods * sizeof(*methods),"eap methods");
	config->eap_methods = methods;
	return errors ? -1 : 0;
}


static 
int gct_config_parse_password(const struct parse_data *data,
				     struct gct_config *config, int line,
				     const char *value)
{
	u8 *hash;

	if (strcmp(value, "NULL") == 0) {
		wmlog_msg(4, "Unset configuration string 'password'\n");
		free(config->password);
		config->password = NULL;
		config->password_len = 0;
		return 0;
	}

	if (strncmp(value, "hash:", 5) != 0) {
		char *tmp;
		size_t res_len;

		tmp = gct_config_parse_string(value, &res_len);
		if (tmp == NULL) {
			wmlog_msg(0, "E: Line %d: failed to parse "
				   "password.", line);
			return -1;
		}
		wmlog_dumphexasc(4, (u8 *) tmp, res_len,"%s",data->name);
//		gct_hexdump_ascii_key(MSG_MSGDUMP, data->name,
//				      (u8 *) tmp, res_len);

		free(config->password);
		config->password = (u8 *) tmp;
		config->password_len = res_len;
		config->flags &= ~EAP_CONFIG_FLAGS_PASSWORD_NTHASH;

		return 0;
	}


	/* NtPasswordHash: hash:<32 hex digits> */
	if (strlen(value + 5) != 2 * 16) {
		wmlog_msg(0, "E: Line %d: Invalid password hash length "
			   "(expected 32 hex digits)", line);
		return -1;
	}

	hash = malloc(16);
	if (hash == NULL)
		return -1;

	if (hexstr2bin(value + 5, hash, 16)) {
		free(hash);
		wmlog_msg(0, "E: Line %d: Invalid password hash", line);
		return -1;
	}

	wmlog_dumphexasc(4,	hash, 16,"%s",data->name);
//	gct_hexdump_key(MSG_MSGDUMP, data->name, hash, 16);

	free(config->password);
	config->password = hash;
	config->password_len = 16;
	config->flags |= EAP_CONFIG_FLAGS_PASSWORD_NTHASH;

	return 0;
}


#define OFFSET(v) ((void *) &((struct gct_config *) 0)->v)

#define _STR(f) #f, gct_config_parse_str, OFFSET(f)
#define STR(f) _STR(f), NULL, NULL, NULL, 0
#define STR_KEY(f) _STR(f), NULL, NULL, NULL, 1
#define _STR_LEN(f) _STR(f), OFFSET(f ## _len)
#define STR_LEN(f) _STR_LEN(f), NULL, NULL, 0
#define _INT(f) #f, gct_config_parse_int, OFFSET(f), (void *) 0
#define INT(f) _INT(f), NULL, NULL, 0
#define INT_RANGE(f, min, max) _INT(f), (void *) (min), (void *) (max), 0

#define _FUNC(f) #f, gct_config_parse_ ## f, NULL, NULL, NULL, NULL
#define FUNC(f) _FUNC(f), 0
#define FUNC_KEY(f) _FUNC(f), 1

static 
const struct parse_data ssid_fields[] = {

	{ INT_RANGE(nspid, 0, 0xffffff) },
	{ INT_RANGE(use_pkm, 0, 1) },
	{ INT_RANGE(use_nv, 0, 1) },
	{ INT_RANGE(eap_type, -1, 7) },	
	{ INT_RANGE(ca_cert_null, 0, 1) },	
	{ INT_RANGE(dev_cert_null, 0, 1) },	
	{ INT_RANGE(cert_nv, 0, 1) },	
	{ INT_RANGE(wimax_verbose_level, -1, 3) },	
	{ INT_RANGE(wpa_debug_level, 0, 4) },
	{ STR(log_file) },
	{ STR(event_script) },
	{ FUNC(eap) },
	{ STR_LEN(identity) },
	{ STR_LEN(anonymous_identity) },
	{ FUNC_KEY(password) },
	{ STR(ca_cert) },
	{ STR(client_cert) },
	{ STR(private_key) },	
	{ STR_KEY(private_key_passwd) },
	{ STR(phase1) },
	{ STR(phase2) },
};

#undef OFFSET
#undef _STR
#undef STR
#undef STR_KEY
#undef _STR_LEN
#undef STR_LEN
#undef _INT
#undef INT
#undef INT_RANGE
#undef _FUNC
#undef FUNC
#undef FUNC_KEY
#define NUM_SSID_FIELDS (sizeof(ssid_fields) / sizeof(ssid_fields[0]))

static
int gct_config_set(struct gct_config *config, const char *var, const char *value,
		   int line)
{
	size_t i;
	int ret = 0;

	if (config == NULL || var == NULL || value == NULL)
		return -1;

	for (i = 0; i < NUM_SSID_FIELDS; i++) {
		const struct parse_data *field = &ssid_fields[i];
		if (strcmp(var, field->name) != 0)
			continue;

		if (field->parser(field, config, line, value)) {
			if (line) {
				wmlog_msg(0, "E: Line %d: failed to "
					   "parse %s '%s'.", line, var, value);
			}
			ret = -1;
		}
		break;
	}
	if (i == NUM_SSID_FIELDS) {
		if (line) {
			wmlog_msg(0, "E: Line %d: unknown network field "
				   "'%s'.", line, var);
		}
		ret = -1;
	}

	return ret;
}


static 
char * gct_config_get_line(char *s, int size, FILE *stream, int *line,
				  char **_pos)
{
	char *pos, *end, *sstart;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		/* Skip white space from the beginning of line. */
		while (*pos == ' ' || *pos == '\t' || *pos == '\r')
			pos++;

		/* Skip comment lines and empty lines */
		if (*pos == '#' || *pos == '\n' || *pos == '\0' || *pos == '[')
			continue;

		/*
		 * Remove # comments unless they are within a double quoted
		 * string.
		 */
		sstart = strchr(pos, '"');
		if (sstart)
			sstart = strrchr(sstart + 1, '"');
		if (!sstart)
			sstart = pos;
		end = strchr(sstart, '#');
		if (end)
			*end-- = '\0';
		else
			end = pos + strlen(pos) - 1;

		/* Remove trailing white space. */
		while (end > pos &&
		       (*end == '\n' || *end == ' ' || *end == '\t' ||
			*end == '\r'))
			*end-- = '\0';

		if (*pos == '\0')
			continue;

		if (_pos)
			*_pos = pos;
		return pos;
	}

	if (_pos)
		*_pos = NULL;
	return NULL;
}


static 
int gct_config_validate_network(struct gct_config *config)
{
	int errors = 0;
	
	if (config->use_pkm == 0)
		errors++;
	if (config->eap_type == 0)
		errors++;		
		
	return errors;	
}


static
int gct_config_read_network(struct gct_config *config, FILE *f, int *line)
{
	int errors = 0;
	char buf[256], *pos, *pos2;

	while (gct_config_get_line(buf, sizeof(buf), f, line, &pos)) {
	
		pos2 = strchr(pos, '=');
		if (pos2 == NULL) {
			wmlog_msg(0, "E: Line %d: Invalid CFG line "
				   "'%s'.", *line, pos);
			errors++;
			continue;
		}	
	
		*pos2++ = '\0';
		if (*pos2 == '"') {
			if (strchr(pos2 + 1, '"') == NULL) {
				wmlog_msg(0, "E: Line %d: invalid "
					   "quotation '%s'.", *line, pos2);
				errors++;
				continue;
			}
		}
		
		if (gct_config_set(config, pos, pos2, *line) < 0)
			errors++;	
		
	}
	
	errors += gct_config_validate_network(config);	
	
	return errors;
}


static
struct gct_config * gct_config_alloc_empty()
{
	struct gct_config *config;

	config = calloc(1, sizeof(*config));
	if (config == NULL)
		return NULL;
	return config;
}


void gct_config_free(struct gct_config *config)
{
	free(config->eap_methods);
	free(config->identity);
	free(config->anonymous_identity);
	free(config->password);
	free(config->ca_cert);	
	free(config->client_cert);
	free(config->private_key);
	free(config->private_key_passwd);		
	free(config->phase1);
	free(config->phase2);	
	free(config);
}	

	
struct gct_config * gct_config_read(const char *name)
{
	FILE *f;
//	char buf[256], *pos;
	int errors = 0, line = 0;
	struct gct_config *config;
	
	config = gct_config_alloc_empty();
	if (config == NULL)
		return NULL;
	if (eap_peer_register_methods() < 0)
		return NULL;
	wmlog_msg(3, "Reading configuration file '%s'", name);
	f = fopen(name, "r");
	if (f == NULL) {
		free(config);
		wmlog_msg(0, "Configuration file error: %s",strerror(errno));
		return NULL;
	}

//	while (gct_config_get_line(buf, sizeof(buf), f, &line, &pos)) {
		errors += gct_config_read_network(config, f, &line);
//	}
	
	fclose(f);
	
	if (errors) {
		gct_config_free(config);
		config = NULL;
	}
	eap_peer_unregister_methods();
	return config;
}

