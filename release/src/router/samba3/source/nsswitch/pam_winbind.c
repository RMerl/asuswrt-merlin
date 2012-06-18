/* pam_winbind module

   Copyright Andrew Tridgell <tridge@samba.org> 2000
   Copyright Tim Potter <tpot@samba.org> 2000
   Copyright Andrew Bartlett <abartlet@samba.org> 2002
   Copyright Guenther Deschner <gd@samba.org> 2005-2007

   largely based on pam_userdb by Cristian Gafton <gafton@redhat.com> 
   also contains large slabs of code from pam_unix by Elliot Lee <sopwith@redhat.com>
   (see copyright below for full details)
*/

#include "pam_winbind.h"

#define _PAM_LOG_FUNCTION_ENTER(function, pamh, ctrl, flags) \
	do { \
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "[pamh: 0x%08x] ENTER: " function " (flags: 0x%04x)", (uint32) pamh, flags); \
		_pam_log_state(pamh, ctrl); \
	} while (0)

#define _PAM_LOG_FUNCTION_LEAVE(function, pamh, ctrl, retval) \
	do { \
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "[pamh: 0x%08x] LEAVE: " function " returning %d", (uint32) pamh, retval); \
		_pam_log_state(pamh, ctrl); \
	} while (0)

/* data tokens */

#define MAX_PASSWD_TRIES	3

/*
 * Work around the pam API that has functions with void ** as parameters.
 * These lead to strict aliasing warnings with gcc.
 */
static int _pam_get_item(const pam_handle_t *pamh, int item_type,
			 const void *_item)
{
	const void **item = (const void **)_item;
	return pam_get_item(pamh, item_type, item);
}
static int _pam_get_data(const pam_handle_t *pamh,
			 const char *module_data_name, const void *_data)
{
	const void **data = (const void **)_data;
	return pam_get_data(pamh, module_data_name, data);
}

/* some syslogging */

#ifdef HAVE_PAM_VSYSLOG
static void _pam_log_int(const pam_handle_t *pamh, int err, const char *format, va_list args)
{
	pam_vsyslog(pamh, err, format, args);
}
#else
static void _pam_log_int(const pam_handle_t *pamh, int err, const char *format, va_list args)
{
	char *format2 = NULL;
	const char *service;

	_pam_get_item(pamh, PAM_SERVICE, &service);

	format2 = malloc(strlen(MODULE_NAME)+strlen(format)+strlen(service)+5);
	if (format2 == NULL) {
		/* what else todo ? */
		vsyslog(err, format, args);
		return;
	}

	sprintf(format2, "%s(%s): %s", MODULE_NAME, service, format);
	vsyslog(err, format2, args);
	SAFE_FREE(format2);
}
#endif /* HAVE_PAM_VSYSLOG */

static BOOL _pam_log_is_silent(int ctrl)
{
	return on(ctrl, WINBIND_SILENT);
}

static void _pam_log(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...) PRINTF_ATTRIBUTE(4,5);
static void _pam_log(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...)
{
	va_list args;

	if (_pam_log_is_silent(ctrl)) {
		return;
	}

	va_start(args, format);
	_pam_log_int(pamh, err, format, args);
	va_end(args);
}

static BOOL _pam_log_is_debug_enabled(int ctrl)
{
	if (ctrl == -1) {
		return False;
	}

	if (_pam_log_is_silent(ctrl)) {
		return False;
	}

	if (!(ctrl & WINBIND_DEBUG_ARG)) {
		return False;
	}

	return True;
}

static BOOL _pam_log_is_debug_state_enabled(int ctrl)
{
	if (!(ctrl & WINBIND_DEBUG_STATE)) {
		return False;
	}

	return _pam_log_is_debug_enabled(ctrl);
}

static void _pam_log_debug(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...) PRINTF_ATTRIBUTE(4,5);
static void _pam_log_debug(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...)
{
	va_list args;

	if (!_pam_log_is_debug_enabled(ctrl)) {
		return;
	}

	va_start(args, format);
	_pam_log_int(pamh, err, format, args);
	va_end(args);
}

static void _pam_log_state_datum(const pam_handle_t *pamh, int ctrl, int item_type, const char *key, int is_string)
{
	const void *data = NULL;
	if (item_type != 0) {
		pam_get_item(pamh, item_type, &data);
	} else {
		pam_get_data(pamh, key, &data);
	}
	if (data != NULL) {
		const char *type = (item_type != 0) ? "ITEM" : "DATA";
		if (is_string != 0) {
			_pam_log_debug(pamh, ctrl, LOG_DEBUG, "[pamh: 0x%08x] STATE: %s(%s) = \"%s\" (0x%08x)", (uint32) pamh, type, key, (const char *) data, (uint32) data);
		} else {
			_pam_log_debug(pamh, ctrl, LOG_DEBUG, "[pamh: 0x%08x] STATE: %s(%s) = 0x%08x", (uint32) pamh, type, key, (uint32) data);
		}
	}
}

#define _PAM_LOG_STATE_DATA_POINTER(pamh, ctrl, module_data_name) \
	_pam_log_state_datum(pamh, ctrl, 0, module_data_name, 0)

#define _PAM_LOG_STATE_DATA_STRING(pamh, ctrl, module_data_name) \
	_pam_log_state_datum(pamh, ctrl, 0, module_data_name, 1)

#define _PAM_LOG_STATE_ITEM_POINTER(pamh, ctrl, item_type) \
	_pam_log_state_datum(pamh, ctrl, item_type, #item_type, 0)

#define _PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, item_type) \
	_pam_log_state_datum(pamh, ctrl, item_type, #item_type, 1)

#ifdef DEBUG_PASSWORD
#define _LOG_PASSWORD_AS_STRING 1
#else
#define _LOG_PASSWORD_AS_STRING 0
#endif

#define _PAM_LOG_STATE_ITEM_PASSWORD(pamh, ctrl, item_type) \
	_pam_log_state_datum(pamh, ctrl, item_type, #item_type, _LOG_PASSWORD_AS_STRING)

static void _pam_log_state(const pam_handle_t *pamh, int ctrl)
{
	if (!_pam_log_is_debug_state_enabled(ctrl)) {
		return;
	}

	_PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, PAM_SERVICE);
	_PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, PAM_USER);
	_PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, PAM_TTY);
	_PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, PAM_RHOST);
	_PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, PAM_RUSER);
	_PAM_LOG_STATE_ITEM_PASSWORD(pamh, ctrl, PAM_OLDAUTHTOK);
	_PAM_LOG_STATE_ITEM_PASSWORD(pamh, ctrl, PAM_AUTHTOK);
	_PAM_LOG_STATE_ITEM_STRING(pamh, ctrl, PAM_USER_PROMPT);
	_PAM_LOG_STATE_ITEM_POINTER(pamh, ctrl, PAM_CONV);
#ifdef PAM_FAIL_DELAY
	_PAM_LOG_STATE_ITEM_POINTER(pamh, ctrl, PAM_FAIL_DELAY);
#endif
#ifdef PAM_REPOSITORY
	_PAM_LOG_STATE_ITEM_POINTER(pamh, ctrl, PAM_REPOSITORY);
#endif

	_PAM_LOG_STATE_DATA_STRING(pamh, ctrl, PAM_WINBIND_HOMEDIR);
	_PAM_LOG_STATE_DATA_STRING(pamh, ctrl, PAM_WINBIND_LOGONSCRIPT);
	_PAM_LOG_STATE_DATA_STRING(pamh, ctrl, PAM_WINBIND_LOGONSERVER);
	_PAM_LOG_STATE_DATA_STRING(pamh, ctrl, PAM_WINBIND_PROFILEPATH);
	_PAM_LOG_STATE_DATA_STRING(pamh, ctrl, PAM_WINBIND_NEW_AUTHTOK_REQD); /* Use atoi to get PAM result code */
	_PAM_LOG_STATE_DATA_STRING(pamh, ctrl, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH);
	_PAM_LOG_STATE_DATA_POINTER(pamh, ctrl, PAM_WINBIND_PWD_LAST_SET);
}

static int _pam_parse(const pam_handle_t *pamh, int flags, int argc, const char **argv, dictionary **result_d)
{
	int ctrl = 0;
	const char *config_file = NULL;
	int i;
	const char **v;
	dictionary *d = NULL;

	if (flags & PAM_SILENT) {
		ctrl |= WINBIND_SILENT;
	}

	for (i=argc,v=argv; i-- > 0; ++v) {
		if (!strncasecmp(*v, "config", strlen("config"))) {
			ctrl |= WINBIND_CONFIG_FILE;
			config_file = v[i];
			break;
		}
	}

	if (config_file == NULL) {
		config_file = PAM_WINBIND_CONFIG_FILE;
	}

	d = iniparser_load(config_file);
	if (d == NULL) {
		goto config_from_pam;
	}

	if (iniparser_getboolean(d, "global:debug", False)) {
		ctrl |= WINBIND_DEBUG_ARG;
	}

	if (iniparser_getboolean(d, "global:debug_state", False)) {
		ctrl |= WINBIND_DEBUG_STATE;
	}

	if (iniparser_getboolean(d, "global:cached_login", False)) {
		ctrl |= WINBIND_CACHED_LOGIN;
	}

	if (iniparser_getboolean(d, "global:krb5_auth", False)) {
		ctrl |= WINBIND_KRB5_AUTH;
	}

	if (iniparser_getboolean(d, "global:silent", False)) {
		ctrl |= WINBIND_SILENT;
	}

	if (iniparser_getstr(d, "global:krb5_ccache_type") != NULL) {
		ctrl |= WINBIND_KRB5_CCACHE_TYPE;
	}

	if ((iniparser_getstr(d, "global:require-membership-of") != NULL) ||
	    (iniparser_getstr(d, "global:require_membership_of") != NULL)) {
		ctrl |= WINBIND_REQUIRED_MEMBERSHIP;
	}

	if (iniparser_getboolean(d, "global:try_first_pass", False)) {
		ctrl |= WINBIND_TRY_FIRST_PASS_ARG;
	}

config_from_pam:
	/* step through arguments */
	for (i=argc,v=argv; i-- > 0; ++v) {

		/* generic options */
		if (!strcmp(*v,"debug"))
			ctrl |= WINBIND_DEBUG_ARG;
		else if (!strcasecmp(*v, "debug_state"))
			ctrl |= WINBIND_DEBUG_STATE;
		else if (!strcasecmp(*v, "use_authtok"))
			ctrl |= WINBIND_USE_AUTHTOK_ARG;
		else if (!strcasecmp(*v, "use_first_pass"))
			ctrl |= WINBIND_USE_FIRST_PASS_ARG;
		else if (!strcasecmp(*v, "try_first_pass"))
			ctrl |= WINBIND_TRY_FIRST_PASS_ARG;
		else if (!strcasecmp(*v, "unknown_ok"))
			ctrl |= WINBIND_UNKNOWN_OK_ARG;
		else if (!strncasecmp(*v, "require_membership_of", strlen("require_membership_of")))
			ctrl |= WINBIND_REQUIRED_MEMBERSHIP;
		else if (!strncasecmp(*v, "require-membership-of", strlen("require-membership-of")))
			ctrl |= WINBIND_REQUIRED_MEMBERSHIP;
		else if (!strcasecmp(*v, "krb5_auth"))
			ctrl |= WINBIND_KRB5_AUTH;
		else if (!strncasecmp(*v, "krb5_ccache_type", strlen("krb5_ccache_type")))
			ctrl |= WINBIND_KRB5_CCACHE_TYPE;
		else if (!strcasecmp(*v, "cached_login"))
			ctrl |= WINBIND_CACHED_LOGIN;
		else {
			_pam_log(pamh, ctrl, LOG_ERR, "pam_parse: unknown option: %s", *v);
			return -1;
		}

	}

	if (result_d) {
		*result_d = d;
	} else {
		if (d) {
			iniparser_freedict(d);
		}
	}

	return ctrl;
};

static void _pam_winbind_cleanup_func(pam_handle_t *pamh, void *data, int error_status)
{
	int ctrl = _pam_parse(pamh, 0, 0, NULL, NULL);
	if (_pam_log_is_debug_state_enabled(ctrl)) {
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "[pamh: 0x%08x] CLEAN: cleaning up PAM data 0x%08x (error_status = %d)", (uint32) pamh, (uint32) data, error_status);
	}
	SAFE_FREE(data);
}


static const struct ntstatus_errors {
	const char *ntstatus_string;
	const char *error_string;
} ntstatus_errors[] = {
	{"NT_STATUS_OK", "Success"},
	{"NT_STATUS_BACKUP_CONTROLLER", "No primary Domain Controler available"},
	{"NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND", "No domain controllers found"},
	{"NT_STATUS_NO_LOGON_SERVERS", "No logon servers"},
	{"NT_STATUS_PWD_TOO_SHORT", "Password too short"},
	{"NT_STATUS_PWD_TOO_RECENT", "The password of this user is too recent to change"},
	{"NT_STATUS_PWD_HISTORY_CONFLICT", "Password is already in password history"},
	{"NT_STATUS_PASSWORD_EXPIRED", "Your password has expired"},
	{"NT_STATUS_PASSWORD_MUST_CHANGE", "You need to change your password now"},
	{"NT_STATUS_INVALID_WORKSTATION", "You are not allowed to logon from this workstation"},
	{"NT_STATUS_INVALID_LOGON_HOURS", "You are not allowed to logon at this time"},
	{"NT_STATUS_ACCOUNT_EXPIRED", "Your account has expired. Please contact your System administrator"}, /* SCNR */
	{"NT_STATUS_ACCOUNT_DISABLED", "Your account is disabled. Please contact your System administrator"}, /* SCNR */
	{"NT_STATUS_ACCOUNT_LOCKED_OUT", "Your account has been locked. Please contact your System administrator"}, /* SCNR */
	{"NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT", "Invalid Trust Account"},
	{"NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT", "Invalid Trust Account"},
	{"NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT", "Invalid Trust Account"},
	{"NT_STATUS_ACCESS_DENIED", "Access is denied"},
	{NULL, NULL}
};

const char *_get_ntstatus_error_string(const char *nt_status_string) 
{
	int i;
	for (i=0; ntstatus_errors[i].ntstatus_string != NULL; i++) {
		if (!strcasecmp(ntstatus_errors[i].ntstatus_string, nt_status_string)) {
			return ntstatus_errors[i].error_string;
		}
	}
	return NULL;
}

/* --- authentication management functions --- */

/* Attempt a conversation */

static int converse(pam_handle_t *pamh, int nargs,
		    struct pam_message **message,
		    struct pam_response **response)
{
	int retval;
	struct pam_conv *conv;

	retval = _pam_get_item(pamh, PAM_CONV, &conv );
	if (retval == PAM_SUCCESS) {
		retval = conv->conv(nargs, (const struct pam_message **)message,
				    response, conv->appdata_ptr);
	}
	
	return retval; /* propagate error status */
}


static int _make_remark(pam_handle_t * pamh, int flags, int type, const char *text)
{
	int retval = PAM_SUCCESS;

	struct pam_message *pmsg[1], msg[1];
	struct pam_response *resp;
	
	if (flags & WINBIND_SILENT) {
		return PAM_SUCCESS;
	}

	pmsg[0] = &msg[0];
	msg[0].msg = CONST_DISCARD(char *, text);
	msg[0].msg_style = type;
	
	resp = NULL;
	retval = converse(pamh, 1, pmsg, &resp);
	
	if (resp) {
		_pam_drop_reply(resp, 1);
	}
	return retval;
}

static int _make_remark_v(pam_handle_t * pamh, int flags, int type, const char *format, va_list args)
{
	char *var;
	int ret;

	ret = vasprintf(&var, format, args);
	if (ret < 0) {
		_pam_log(pamh, 0, LOG_ERR, "memory allocation failure");
		return ret;
	}

	ret = _make_remark(pamh, flags, type, var);
	SAFE_FREE(var);
	return ret;
}

static int _make_remark_format(pam_handle_t * pamh, int flags, int type, const char *format, ...) PRINTF_ATTRIBUTE(4,5);
static int _make_remark_format(pam_handle_t * pamh, int flags, int type, const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = _make_remark_v(pamh, flags, type, format, args);
	va_end(args);
	return ret;
}

static int pam_winbind_request(pam_handle_t * pamh, int ctrl,
			       enum winbindd_cmd req_type,
			       struct winbindd_request *request,
			       struct winbindd_response *response)
{
	/* Fill in request and send down pipe */
	init_request(request, req_type);
	
	if (write_sock(request, sizeof(*request), 0, 0) == -1) {
		_pam_log(pamh, ctrl, LOG_ERR, "pam_winbind_request: write to socket failed!");
		close_sock();
		return PAM_SERVICE_ERR;
	}
	
	/* Wait for reply */
	if (read_reply(response) == -1) {
		_pam_log(pamh, ctrl, LOG_ERR, "pam_winbind_request: read from socket failed!");
		close_sock();
		return PAM_SERVICE_ERR;
	}

	/* We are done with the socket - close it and avoid mischeif */
	close_sock();

	/* Copy reply data from socket */
	if (response->result == WINBINDD_OK) {
		return PAM_SUCCESS;
	}

	/* no need to check for pam_error codes for getpwnam() */
	switch (req_type) {

		case WINBINDD_GETPWNAM:
		case WINBINDD_LOOKUPNAME:
			if (strlen(response->data.auth.nt_status_string) > 0) {
				_pam_log(pamh, ctrl, LOG_ERR, "request failed, NT error was %s", 
				response->data.auth.nt_status_string);
			} else {
				_pam_log(pamh, ctrl, LOG_ERR, "request failed");
			}
			return PAM_USER_UNKNOWN;
		default:
			break;
	}

	if (response->data.auth.pam_error != PAM_SUCCESS) {
		_pam_log(pamh, ctrl, LOG_ERR, "request failed: %s, PAM error was %s (%d), NT error was %s", 
			 response->data.auth.error_string,
			 pam_strerror(pamh, response->data.auth.pam_error),
			 response->data.auth.pam_error,
			 response->data.auth.nt_status_string);
		return response->data.auth.pam_error;
	} 
	
	_pam_log(pamh, ctrl, LOG_ERR, "request failed, but PAM error 0!");

	return PAM_SERVICE_ERR;
}

static int pam_winbind_request_log(pam_handle_t * pamh,
				   int ctrl,
				   enum winbindd_cmd req_type,
				   struct winbindd_request *request,
				   struct winbindd_response *response,
				   const char *user)
{
	int retval;

	retval = pam_winbind_request(pamh, ctrl, req_type, request, response);

	switch (retval) {
	case PAM_AUTH_ERR:
		/* incorrect password */
		_pam_log(pamh, ctrl, LOG_WARNING, "user '%s' denied access (incorrect password or invalid membership)", user);
		return retval;
	case PAM_ACCT_EXPIRED:
		/* account expired */
		_pam_log(pamh, ctrl, LOG_WARNING, "user '%s' account expired", user);
		return retval;
	case PAM_AUTHTOK_EXPIRED:
		/* password expired */
		_pam_log(pamh, ctrl, LOG_WARNING, "user '%s' password expired", user);
		return retval;
	case PAM_NEW_AUTHTOK_REQD:
		/* new password required */
		_pam_log(pamh, ctrl, LOG_WARNING, "user '%s' new password required", user);
		return retval;
	case PAM_USER_UNKNOWN:
		/* the user does not exist */
		_pam_log_debug(pamh, ctrl, LOG_NOTICE, "user '%s' not found", user);
		if (ctrl & WINBIND_UNKNOWN_OK_ARG) {
			return PAM_IGNORE;
		}	 
		return retval;
	case PAM_SUCCESS:
		/* Otherwise, the authentication looked good */
		switch (req_type) {
			case WINBINDD_INFO:
				break;
			case WINBINDD_PAM_AUTH:
				_pam_log(pamh, ctrl, LOG_NOTICE, "user '%s' granted access", user);
				break;
			case WINBINDD_PAM_CHAUTHTOK:
				_pam_log(pamh, ctrl, LOG_NOTICE, "user '%s' password changed", user);
				break;
			default:
				_pam_log(pamh, ctrl, LOG_NOTICE, "user '%s' OK", user);
				break;
		}
	
		return retval;
	default:
		/* we don't know anything about this return value */
		_pam_log(pamh, ctrl, LOG_ERR, "internal module error (retval = %d, user = '%s')",
			 retval, user);
		return retval;
	}
}

/**
 * send a password expiry message if required
 * 
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param next_change expected (calculated) next expiry date.
 * @param already_expired pointer to a boolean to indicate if the password is
 *        already expired.
 *
 * @return boolean Returns True if message has been sent, False if not.
 */

static BOOL _pam_send_password_expiry_message(pam_handle_t *pamh, int ctrl, time_t next_change, time_t now, BOOL *already_expired)
{
	int days = 0;
	struct tm tm_now, tm_next_change;

	if (already_expired) {
		*already_expired = False;
	}

	if (next_change <= now) {
		PAM_WB_REMARK_DIRECT(pamh, ctrl, "NT_STATUS_PASSWORD_EXPIRED");
		if (already_expired) {
			*already_expired = True;
		}
		return True;
	}

	if ((next_change < 0) ||
	    (next_change > now + DAYS_TO_WARN_BEFORE_PWD_EXPIRES * SECONDS_PER_DAY)) {
		return False;
	}

	if ((localtime_r(&now, &tm_now) == NULL) || 
	    (localtime_r(&next_change, &tm_next_change) == NULL)) {
		return False;
	}

	days = (tm_next_change.tm_yday+tm_next_change.tm_year*365) - (tm_now.tm_yday+tm_now.tm_year*365);

	if (days == 0) {
		_make_remark(pamh, ctrl, PAM_TEXT_INFO, "Your password expires today");
		return True;
	} 
	
	if (days > 0 && days < DAYS_TO_WARN_BEFORE_PWD_EXPIRES) {
		_make_remark_format(pamh, ctrl, PAM_TEXT_INFO, "Your password will expire in %d %s", 
			days, (days > 1) ? "days":"day");
		return True;
	}

	return False;
}

/**
 * Send a warning if the password expires in the near future
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param response The full authentication response structure.
 * @param already_expired boolean, is the pwd already expired?
 *
 * @return void.
 */

static void _pam_warn_password_expiry(pam_handle_t *pamh, 
				      int flags, 
				      const struct winbindd_response *response,
				      BOOL *already_expired)
{
	time_t now = time(NULL);
	time_t next_change = 0;

	if (already_expired) {
		*already_expired = False;
	}

	/* accounts with ACB_PWNOEXP set never receive a warning */
	if (response->data.auth.info3.acct_flags & ACB_PWNOEXP) {
		return;
	}

	/* no point in sending a warning if this is a grace logon */
	if (PAM_WB_GRACE_LOGON(response->data.auth.info3.user_flgs)) {
		return;
	}

	/* check if the info3 must change timestamp has been set */
	next_change = response->data.auth.info3.pass_must_change_time;

	if (_pam_send_password_expiry_message(pamh, flags, next_change, now, 
					      already_expired)) {
		return;
	}

	/* now check for the global password policy */
	/* good catch from Ralf Haferkamp: an expiry of "never" is translated
	 * to -1 */
	if (response->data.auth.policy.expire <= 0) {
		return;
	}

	next_change = response->data.auth.info3.pass_last_set_time + 
		      response->data.auth.policy.expire;

	if (_pam_send_password_expiry_message(pamh, flags, next_change, now, 
					      already_expired)) {
		return;
	}

	/* no warning sent */
}

#define IS_SID_STRING(name) (strncmp("S-", name, 2) == 0)

static BOOL safe_append_string(char *dest,
			const char *src,
			int dest_buffer_size)
/**
 * Append a string, making sure not to overflow and to always return a NULL-terminated
 * string.
 *
 * @param dest Destination string buffer (must already be NULL-terminated).
 * @param src Source string buffer.
 * @param dest_buffer_size Size of dest buffer in bytes.
 *
 * @return False if dest buffer is not big enough (no bytes copied), True on success.
 */
{
	int dest_length = strlen(dest);
	int src_length = strlen(src);

	if ( dest_length + src_length + 1 > dest_buffer_size ) {
		return False;
	}

	memcpy(dest + dest_length, src, src_length + 1);
	return True;
}

static BOOL winbind_name_to_sid_string(pam_handle_t *pamh,
				int ctrl,
				const char *user,
				const char *name,
				char *sid_list_buffer,
				int sid_list_buffer_size)
/**
 * Convert a names into a SID string, appending it to a buffer.
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param user User in PAM request.
 * @param name Name to convert.
 * @param sid_list_buffer Where to append the string sid.
 * @param sid_list_buffer Size of sid_list_buffer (in bytes).
 *
 * @return False on failure, True on success.
 */
{
	const char* sid_string;
	struct winbindd_response sid_response;

	/* lookup name? */ 
	if (IS_SID_STRING(name)) {
		sid_string = name;
	} else {
		struct winbindd_request sid_request;

		ZERO_STRUCT(sid_request);
		ZERO_STRUCT(sid_response);

		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "no sid given, looking up: %s\n", name);

		/* fortunatly winbindd can handle non-separated names */
		strncpy(sid_request.data.name.name, name,
			sizeof(sid_request.data.name.name) - 1);

		if (pam_winbind_request_log(pamh, ctrl, WINBINDD_LOOKUPNAME, &sid_request, &sid_response, user)) {
			_pam_log(pamh, ctrl, LOG_INFO, "could not lookup name: %s\n", name); 
			return False;
		}

		sid_string = sid_response.data.sid.sid;
	}

	if (!safe_append_string(sid_list_buffer, sid_string, sid_list_buffer_size)) {
		return False;
	}

	return True;
}

static BOOL winbind_name_list_to_sid_string_list(pam_handle_t *pamh,
				int ctrl,
				const char *user,
				const char *name_list,
				char *sid_list_buffer,
				int sid_list_buffer_size)
/**
 * Convert a list of names into a list of sids.
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param user User in PAM request.
 * @param name_list List of names or string sids, separated by commas.
 * @param sid_list_buffer Where to put the list of string sids.
 * @param sid_list_buffer Size of sid_list_buffer (in bytes).
 *
 * @return False on failure, True on success.
 */
{
	BOOL result = False;
	char *current_name = NULL;
	const char *search_location;
	const char *comma;

	if ( sid_list_buffer_size > 0 ) {
		sid_list_buffer[0] = 0;
	}

	search_location = name_list;
	while ( (comma = strstr(search_location, ",")) != NULL ) {
		current_name = strndup(search_location, comma - search_location);
		if (NULL == current_name) {
			goto out;
		}

		if (!winbind_name_to_sid_string(pamh, ctrl, user, current_name, sid_list_buffer, sid_list_buffer_size)) {
			goto out;
		}

		SAFE_FREE(current_name);

		if (!safe_append_string(sid_list_buffer, ",", sid_list_buffer_size)) {
			goto out;
		}

		search_location = comma + 1;
	}

	if (!winbind_name_to_sid_string(pamh, ctrl, user, search_location, sid_list_buffer, sid_list_buffer_size)) {
		goto out;
	}

	result = True;

out:
	SAFE_FREE(current_name);
	return result;
}

/**
 * put krb5ccname variable into environment
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param krb5ccname env variable retrieved from winbindd.
 *
 * @return void.
 */

static void _pam_setup_krb5_env(pam_handle_t *pamh, int ctrl, const char *krb5ccname)
{
	char var[PATH_MAX];
	int ret;

	if (off(ctrl, WINBIND_KRB5_AUTH)) {
		return;
	}

	if (!krb5ccname || (strlen(krb5ccname) == 0)) {
		return;
	}

	_pam_log_debug(pamh, ctrl, LOG_DEBUG, "request returned KRB5CCNAME: %s", krb5ccname);
	
	if (snprintf(var, sizeof(var), "KRB5CCNAME=%s", krb5ccname) == -1) {
		return;
	}
	
	ret = pam_putenv(pamh, var);
	if (ret) {
		_pam_log(pamh, ctrl, LOG_ERR, "failed to set KRB5CCNAME to %s: %s", 
			var, pam_strerror(pamh, ret));
	}
}	

/**
 * Set string into the PAM stack.
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param data_name Key name for pam_set_data.
 * @param value String value.
 *
 * @return void.
 */

static void _pam_set_data_string(pam_handle_t *pamh, int ctrl, const char *data_name, const char *value)
{
	int ret;

	if ( !data_name || !value || (strlen(data_name) == 0) || (strlen(value) == 0) ) {
		return;
	}

	ret = pam_set_data(pamh, data_name, (void *)strdup(value), _pam_winbind_cleanup_func);
	if (ret) {
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "Could not set data %s: %s\n", 
			data_name, pam_strerror(pamh, ret));
	}

}

/**
 * Set info3 strings into the PAM stack.
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param data_name Key name for pam_set_data.
 * @param value String value.
 *
 * @return void.
 */

static void _pam_set_data_info3(pam_handle_t *pamh, int ctrl, struct winbindd_response *response)
{
	_pam_set_data_string(pamh, ctrl, PAM_WINBIND_HOMEDIR, response->data.auth.info3.home_dir);
	_pam_set_data_string(pamh, ctrl, PAM_WINBIND_LOGONSCRIPT, response->data.auth.info3.logon_script);
	_pam_set_data_string(pamh, ctrl, PAM_WINBIND_LOGONSERVER, response->data.auth.info3.logon_srv);
	_pam_set_data_string(pamh, ctrl, PAM_WINBIND_PROFILEPATH, response->data.auth.info3.profile_path);
}

/**
 * Free info3 strings in the PAM stack.
 *
 * @param pamh PAM handle
 *
 * @return void.
 */

static void _pam_free_data_info3(pam_handle_t *pamh)
{
	pam_set_data(pamh, PAM_WINBIND_HOMEDIR, NULL, NULL);
	pam_set_data(pamh, PAM_WINBIND_LOGONSCRIPT, NULL, NULL);
	pam_set_data(pamh, PAM_WINBIND_LOGONSERVER, NULL, NULL);
	pam_set_data(pamh, PAM_WINBIND_PROFILEPATH, NULL, NULL);
}

/**
 * Send PAM_ERROR_MSG for cached or grace logons.
 *
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param username User in PAM request.
 * @param info3_user_flgs Info3 flags containing logon type bits.
 *
 * @return void.
 */

static void _pam_warn_logon_type(pam_handle_t *pamh, int ctrl, const char *username, uint32 info3_user_flgs)
{
	/* inform about logon type */
	if (PAM_WB_GRACE_LOGON(info3_user_flgs)) {

		_make_remark(pamh, ctrl, PAM_ERROR_MSG, 
			"Grace login. Please change your password as soon you're online again");
		_pam_log_debug(pamh, ctrl, LOG_DEBUG,
			"User %s logged on using grace logon\n", username);

	} else if (PAM_WB_CACHED_LOGON(info3_user_flgs)) {

		_make_remark(pamh, ctrl, PAM_ERROR_MSG, 
			"Domain Controller unreachable, using cached credentials instead. Network resources may be unavailable");
		_pam_log_debug(pamh, ctrl, LOG_DEBUG,
			"User %s logged on using cached credentials\n", username);
	}
}

/**
 * Compose Password Restriction String for a PAM_ERROR_MSG conversation.
 *
 * @param response The struct winbindd_response.
 *
 * @return string (caller needs to free).
 */

static char *_pam_compose_pwd_restriction_string(struct winbindd_response *response)
{
	char *str = NULL;
	size_t offset = 0, ret = 0, str_size = 1024;

	str = (char *)malloc(str_size);
	if (!str) {
		return NULL;
	}

	memset(str, '\0', str_size);

	offset = snprintf(str, str_size, "Your password ");
	if (offset == -1) {
		goto failed;
	}

	if (response->data.auth.policy.min_length_password > 0) {
		ret = snprintf(str+offset, str_size-offset,
			     "must be at least %d characters; ",
			     response->data.auth.policy.min_length_password);
		if (ret == -1) {
			goto failed;
		}
		offset += ret;
	}
	
	if (response->data.auth.policy.password_history > 0) {
		ret = snprintf(str+offset, str_size-offset,
			     "cannot repeat any of your previous %d passwords; ",
			     response->data.auth.policy.password_history);
		if (ret == -1) {
			goto failed;
		}
		offset += ret;
	}
	
	if (response->data.auth.policy.password_properties & DOMAIN_PASSWORD_COMPLEX) {
		ret = snprintf(str+offset, str_size-offset,
			     "must contain capitals, numerals or punctuation; "
			     "and cannot contain your account or full name; ");
		if (ret == -1) {
			goto failed;
		}
		offset += ret;
	}

	ret = snprintf(str+offset, str_size-offset, 
		     "Please type a different password. "
		     "Type a password which meets these requirements in both text boxes.");
	if (ret == -1) {
		goto failed;
	}

	return str;

 failed:
 	SAFE_FREE(str);
	return NULL;
}

/* talk to winbindd */
static int winbind_auth_request(pam_handle_t * pamh,
				int ctrl, 
				const char *user, 
				const char *pass, 
				const char *member, 
				const char *cctype,
				struct winbindd_response *p_response,
				time_t *pwd_last_set,
				char **user_ret)
{
	struct winbindd_request request;
	struct winbindd_response response;
	int ret;
	BOOL already_expired = False;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (pwd_last_set) {
		*pwd_last_set = 0;
	}

	strncpy(request.data.auth.user, user, 
		sizeof(request.data.auth.user)-1);

	strncpy(request.data.auth.pass, pass, 
		sizeof(request.data.auth.pass)-1);

	request.data.auth.krb5_cc_type[0] = '\0';
	request.data.auth.uid = -1;
	
	request.flags = WBFLAG_PAM_INFO3_TEXT |
			WBFLAG_PAM_GET_PWD_POLICY |
			WBFLAG_PAM_CONTACT_TRUSTDOM;

	if (ctrl & (WINBIND_KRB5_AUTH|WINBIND_CACHED_LOGIN)) {
		struct passwd *pwd = NULL;

		pwd = getpwnam(user);
		if (pwd == NULL) {
			return PAM_USER_UNKNOWN;
		}
		request.data.auth.uid = pwd->pw_uid;
	}

	if (ctrl & WINBIND_KRB5_AUTH) {

		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "enabling krb5 login flag\n"); 

		request.flags |= WBFLAG_PAM_KRB5 | WBFLAG_PAM_FALLBACK_AFTER_KRB5;
	}

	if (ctrl & WINBIND_CACHED_LOGIN) {
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "enabling cached login flag\n"); 
		request.flags |= WBFLAG_PAM_CACHED_LOGIN;
	}

	if (user_ret) {
		*user_ret = NULL;
		request.flags |= WBFLAG_PAM_UNIX_NAME;
	}

	if (cctype != NULL) {
		strncpy(request.data.auth.krb5_cc_type, cctype, 
			sizeof(request.data.auth.krb5_cc_type) - 1);
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "enabling request for a %s krb5 ccache\n", cctype); 
	}

	request.data.auth.require_membership_of_sid[0] = '\0';

	if (member != NULL) {

		if (!winbind_name_list_to_sid_string_list(pamh, ctrl, user, member,
			request.data.auth.require_membership_of_sid,
			sizeof(request.data.auth.require_membership_of_sid))) {

			_pam_log_debug(pamh, ctrl, LOG_ERR, "failed to serialize membership of sid \"%s\"\n", member);
			return PAM_AUTH_ERR;
		}
	}

	ret = pam_winbind_request_log(pamh, ctrl, WINBINDD_PAM_AUTH, &request, &response, user);

	if (pwd_last_set) {
		*pwd_last_set = response.data.auth.info3.pass_last_set_time;
	}

	if (p_response) {
		/* We want to process the response in the caller. */
		*p_response = response;
		return ret;
	}

	if (ret) {
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_PASSWORD_EXPIRED");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_PASSWORD_MUST_CHANGE");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_INVALID_WORKSTATION");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_INVALID_LOGON_HOURS");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_ACCOUNT_EXPIRED");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_ACCOUNT_DISABLED");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_ACCOUNT_LOCKED_OUT");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_NO_LOGON_SERVERS");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_WRONG_PASSWORD");
		PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_ACCESS_DENIED");
	}

	if (ret == PAM_SUCCESS) {

		/* warn a user if the password is about to expire soon */
		_pam_warn_password_expiry(pamh, ctrl, &response, &already_expired);

		if (already_expired == True) {
			_pam_log_debug(pamh, ctrl, LOG_DEBUG, "Password has expired "
				       "(Password was last set: %lld, the policy says "
				       "it should expire here %lld (now it's: %lu))\n",
				       response.data.auth.info3.pass_last_set_time, 
				       response.data.auth.info3.pass_last_set_time +
				       response.data.auth.policy.expire,
				       time(NULL));

			return PAM_AUTHTOK_EXPIRED;
		}

		/* inform about logon type */
		_pam_warn_logon_type(pamh, ctrl, user, response.data.auth.info3.user_flgs);

		/* set some info3 info for other modules in the stack */
		_pam_set_data_info3(pamh, ctrl, &response);

		/* put krb5ccname into env */
		_pam_setup_krb5_env(pamh, ctrl, response.data.auth.krb5ccname);

		/* If winbindd returned a username, return the pointer to it here. */
		if (user_ret && response.extra_data.data) {
			/* We have to trust it's a null terminated string. */
			*user_ret = (char *)response.extra_data.data;
		}
	}

	return ret;
}

/* talk to winbindd */
static int winbind_chauthtok_request(pam_handle_t * pamh,
				     int ctrl,
				     const char *user, 
				     const char *oldpass,
				     const char *newpass,
				     time_t pwd_last_set) 
{
	struct winbindd_request request;
	struct winbindd_response response;
	int ret;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (request.data.chauthtok.user == NULL) return -2;

	strncpy(request.data.chauthtok.user, user, 
		sizeof(request.data.chauthtok.user) - 1);

	if (oldpass != NULL) {
		strncpy(request.data.chauthtok.oldpass, oldpass, 
			sizeof(request.data.chauthtok.oldpass) - 1);
	} else {
		request.data.chauthtok.oldpass[0] = '\0';
	}
	
	if (newpass != NULL) {
		strncpy(request.data.chauthtok.newpass, newpass, 
			sizeof(request.data.chauthtok.newpass) - 1);
	} else {
		request.data.chauthtok.newpass[0] = '\0';
	}

	if (ctrl & WINBIND_KRB5_AUTH) {
		request.flags = WBFLAG_PAM_KRB5 | WBFLAG_PAM_CONTACT_TRUSTDOM;
	}

	if (ctrl & WINBIND_CACHED_LOGIN) {
		request.flags |= WBFLAG_PAM_CACHED_LOGIN;
	}

	ret = pam_winbind_request_log(pamh, ctrl, WINBINDD_PAM_CHAUTHTOK, &request, &response, user);

	if (ret == PAM_SUCCESS) {
		return ret;
	}

	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_BACKUP_CONTROLLER");
	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND");
	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_NO_LOGON_SERVERS");
	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_ACCESS_DENIED");

	/* TODO: tell the min pwd length ? */
	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_PWD_TOO_SHORT");

	/* TODO: tell the minage ? */
	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_PWD_TOO_RECENT");

	/* TODO: tell the history length ? */
	PAM_WB_REMARK_CHECK_RESPONSE_RET(pamh, ctrl, response, "NT_STATUS_PWD_HISTORY_CONFLICT");

	if (!strcasecmp(response.data.auth.nt_status_string, "NT_STATUS_PASSWORD_RESTRICTION")) {

		char *pwd_restriction_string = NULL;

		/* FIXME: avoid to send multiple PAM messages after another */
		switch (response.data.auth.reject_reason) {
			case -1:
				break;
			case REJECT_REASON_OTHER:
				if ((response.data.auth.policy.min_passwordage > 0) &&
				    (pwd_last_set + response.data.auth.policy.min_passwordage > time(NULL))) {
					PAM_WB_REMARK_DIRECT(pamh, ctrl, "NT_STATUS_PWD_TOO_RECENT");
				}
				break;
			case REJECT_REASON_TOO_SHORT:
				PAM_WB_REMARK_DIRECT(pamh, ctrl, "NT_STATUS_PWD_TOO_SHORT");
				break;
			case REJECT_REASON_IN_HISTORY:
				PAM_WB_REMARK_DIRECT(pamh, ctrl, "NT_STATUS_PWD_HISTORY_CONFLICT");
				break;
			case REJECT_REASON_NOT_COMPLEX:
				_make_remark(pamh, ctrl, PAM_ERROR_MSG, "Password does not meet complexity requirements");
				break;
			default:
				_pam_log_debug(pamh, ctrl, LOG_DEBUG,
					       "unknown password change reject reason: %d", 
					       response.data.auth.reject_reason);
				break;
		}

		pwd_restriction_string = _pam_compose_pwd_restriction_string(&response);
		if (pwd_restriction_string) {
			_make_remark(pamh, ctrl, PAM_ERROR_MSG, pwd_restriction_string);
			SAFE_FREE(pwd_restriction_string);
		}
	}

	return ret;
}

/*
 * Checks if a user has an account
 *
 * return values:
 *	 1  = User not found
 *	 0  = OK
 * 	-1  = System error
 */
static int valid_user(pam_handle_t *pamh, int ctrl, const char *user)
{
	/* check not only if the user is available over NSS calls, also make
	 * sure it's really a winbind user, this is important when stacking PAM
	 * modules in the 'account' or 'password' facility. */

	struct passwd *pwd = NULL;
	struct winbindd_request request;
	struct winbindd_response response;
	int ret;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	pwd = getpwnam(user);
	if (pwd == NULL) {
		return 1;
	}

	strncpy(request.data.username, user,
		sizeof(request.data.username) - 1);

	ret = pam_winbind_request_log(pamh, ctrl, WINBINDD_GETPWNAM, &request, &response, user);

	switch (ret) {
		case PAM_USER_UNKNOWN:
			return 1;
		case PAM_SUCCESS:
			return 0;
		default:
			break;
	}
	return -1;
}

static char *_pam_delete(register char *xx)
{
	_pam_overwrite(xx);
	_pam_drop(xx);
	return NULL;
}

/*
 * obtain a password from the user
 */

static int _winbind_read_password(pam_handle_t * pamh,
				  unsigned int ctrl,
				  const char *comment,
				  const char *prompt1,
				  const char *prompt2,
				  const char **pass)
{
	int authtok_flag;
	int retval;
	const char *item;
	char *token;

	_pam_log(pamh, ctrl, LOG_DEBUG, "getting password (0x%08x)", ctrl);

	/*
	 * make sure nothing inappropriate gets returned
	 */

	*pass = token = NULL;

	/*
	 * which authentication token are we getting?
	 */

	authtok_flag = on(WINBIND__OLD_PASSWORD, ctrl) ? PAM_OLDAUTHTOK : PAM_AUTHTOK;

	/*
	 * should we obtain the password from a PAM item ?
	 */

	if (on(WINBIND_TRY_FIRST_PASS_ARG, ctrl) || on(WINBIND_USE_FIRST_PASS_ARG, ctrl)) {
		retval = _pam_get_item(pamh, authtok_flag, &item);
		if (retval != PAM_SUCCESS) {
			/* very strange. */
			_pam_log(pamh, ctrl, LOG_ALERT, 
				 "pam_get_item returned error to unix-read-password"
			    );
			return retval;
		} else if (item != NULL) {	/* we have a password! */
			*pass = item;
			item = NULL;
			_pam_log(pamh, ctrl, LOG_DEBUG, 
				 "pam_get_item returned a password");
			return PAM_SUCCESS;
		} else if (on(WINBIND_USE_FIRST_PASS_ARG, ctrl)) {
			return PAM_AUTHTOK_RECOVER_ERR;		/* didn't work */
		} else if (on(WINBIND_USE_AUTHTOK_ARG, ctrl)
			   && off(WINBIND__OLD_PASSWORD, ctrl)) {
			return PAM_AUTHTOK_RECOVER_ERR;
		}
	}
	/*
	 * getting here implies we will have to get the password from the
	 * user directly.
	 */

	{
		struct pam_message msg[3], *pmsg[3];
		struct pam_response *resp;
		int i, replies;

		/* prepare to converse */

		if (comment != NULL && off(ctrl, WINBIND_SILENT)) {
			pmsg[0] = &msg[0];
			msg[0].msg_style = PAM_TEXT_INFO;
			msg[0].msg = CONST_DISCARD(char *, comment);
			i = 1;
		} else {
			i = 0;
		}

		pmsg[i] = &msg[i];
		msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
		msg[i++].msg = CONST_DISCARD(char *, prompt1);
		replies = 1;

		if (prompt2 != NULL) {
			pmsg[i] = &msg[i];
			msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
			msg[i++].msg = CONST_DISCARD(char *, prompt2);
			++replies;
		}
		/* so call the conversation expecting i responses */
		resp = NULL;
		retval = converse(pamh, i, pmsg, &resp);

		if (resp != NULL) {

			/* interpret the response */

			if (retval == PAM_SUCCESS) {	/* a good conversation */

				token = x_strdup(resp[i - replies].resp);
				if (token != NULL) {
					if (replies == 2) {
						/* verify that password entered correctly */
						if (!resp[i - 1].resp
						    || strcmp(token, resp[i - 1].resp)) {
							_pam_delete(token);	/* mistyped */
							retval = PAM_AUTHTOK_RECOVER_ERR;
							_make_remark(pamh, ctrl, PAM_ERROR_MSG, MISTYPED_PASS);
						}
					}
				} else {
					_pam_log(pamh, ctrl, LOG_NOTICE, "could not recover authentication token");
					retval = PAM_AUTHTOK_RECOVER_ERR;
				}

			}
			/*
			 * tidy up the conversation (resp_retcode) is ignored
			 * -- what is it for anyway? AGM
			 */

			_pam_drop_reply(resp, i);

		} else {
			retval = (retval == PAM_SUCCESS)
			    ? PAM_AUTHTOK_RECOVER_ERR : retval;
		}
	}

	if (retval != PAM_SUCCESS) {
		_pam_log_debug(pamh, ctrl, LOG_DEBUG,
			         "unable to obtain a password");
		return retval;
	}
	/* 'token' is the entered password */

	/* we store this password as an item */
	
	retval = pam_set_item(pamh, authtok_flag, token);
	_pam_delete(token);	/* clean it up */
	if (retval != PAM_SUCCESS || 
	    (retval = _pam_get_item(pamh, authtok_flag, &item)) != PAM_SUCCESS) {
		
		_pam_log(pamh, ctrl, LOG_CRIT, "error manipulating password");
		return retval;
		
	}

	*pass = item;
	item = NULL;		/* break link to password */

	return PAM_SUCCESS;
}

const char *get_conf_item_string(const pam_handle_t *pamh,
				 int argc, 
				 const char **argv, 
				 int ctrl,
				 dictionary *d,
				 const char *item, 
				 int config_flag)
{
	int i = 0;
	const char *parm_opt = NULL;
	char *key = NULL;

	if (!(ctrl & config_flag)) {
		goto out;
	}

	/* let the pam opt take precedence over the pam_winbind.conf option */

	if (d != NULL) {

		if (!asprintf(&key, "global:%s", item)) {
			goto out;
		}

		parm_opt = iniparser_getstr(d, key);
		SAFE_FREE(key);
	}

	for ( i=0; i<argc; i++ ) {

		if ((strncmp(argv[i], item, strlen(item)) == 0)) {
			char *p;

			if ( (p = strchr( argv[i], '=' )) == NULL) {
				_pam_log(pamh, ctrl, LOG_INFO, "no \"=\" delimiter for \"%s\" found\n", item);
				goto out;
			}
			_pam_log_debug(pamh, ctrl, LOG_INFO, "PAM config: %s '%s'\n", item, p+1);
			return p + 1;
		}
	}

	if (d != NULL) {
		_pam_log_debug(pamh, ctrl, LOG_INFO, "CONFIG file: %s '%s'\n", item, parm_opt);
	}
out:
	return parm_opt;
}

const char *get_krb5_cc_type_from_config(const pam_handle_t *pamh, int argc, const char **argv, int ctrl, dictionary *d)
{
	return get_conf_item_string(pamh, argc, argv, ctrl, d, "krb5_ccache_type", WINBIND_KRB5_CCACHE_TYPE);
}

const char *get_member_from_config(const pam_handle_t *pamh, int argc, const char **argv, int ctrl, dictionary *d)
{
	const char *ret = NULL;
	ret = get_conf_item_string(pamh, argc, argv, ctrl, d, "require_membership_of", WINBIND_REQUIRED_MEMBERSHIP);
	if (ret) {
		return ret;
	}
	return get_conf_item_string(pamh, argc, argv, ctrl, d, "require-membership-of", WINBIND_REQUIRED_MEMBERSHIP);
}

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
	const char *username;
	const char *password;
	const char *member = NULL;
	const char *cctype = NULL;
	int retval = PAM_AUTH_ERR;
	dictionary *d = NULL;
	char *username_ret = NULL;
	char *new_authtok_required = NULL;
	char *real_username = NULL;

	/* parse arguments */
	int ctrl = _pam_parse(pamh, flags, argc, argv, &d);
	if (ctrl == -1) {
		retval = PAM_SYSTEM_ERR;
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_authenticate", pamh, ctrl, flags);

	/* Get the username */
	retval = pam_get_user(pamh, &username, NULL);
	if ((retval != PAM_SUCCESS) || (!username)) {
		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "can not get the username");
		retval = PAM_SERVICE_ERR;
		goto out;
	}

#if defined(AIX)
	/* Decode the user name since AIX does not support logn user
	   names by default.  The name is encoded as _#uid.  */

	if ( username[0] == '_' ) {
		uid_t id = atoi( &username[1] );
		struct passwd *pw = NULL;		

		if ( (id!=0) && ((pw = getpwuid( id )) != NULL) ) {
			real_username = strdup( pw->pw_name );
		}
	}
#endif

	if ( !real_username ) {
		/* Just making a copy of the username we got from PAM */
		if ( (real_username = strdup( username )) == NULL ) {
			_pam_log_debug(pamh, ctrl, LOG_DEBUG, 
				       "memory allocation failure when copying username");
			retval = PAM_SERVICE_ERR;
			goto out;
		}
	}	

	retval = _winbind_read_password(pamh, ctrl, NULL, 
					"Password: ", NULL,
					&password);

	if (retval != PAM_SUCCESS) {
		_pam_log(pamh, ctrl, LOG_ERR, "Could not retrieve user's password");
		retval = PAM_AUTHTOK_ERR;
		goto out;
	}

	/* Let's not give too much away in the log file */

#ifdef DEBUG_PASSWORD
	_pam_log_debug(pamh, ctrl, LOG_INFO, "Verify user '%s' with password '%s'", 
		       real_username, password);
#else
	_pam_log_debug(pamh, ctrl, LOG_INFO, "Verify user '%s'", real_username);
#endif

	member = get_member_from_config(pamh, argc, argv, ctrl, d);

	cctype = get_krb5_cc_type_from_config(pamh, argc, argv, ctrl, d);

	/* Now use the username to look up password */
	retval = winbind_auth_request(pamh, ctrl, username, password, member,
				      cctype, NULL, NULL, &username_ret);

	if (retval == PAM_NEW_AUTHTOK_REQD ||
	    retval == PAM_AUTHTOK_EXPIRED) {

		char *new_authtok_required_during_auth = NULL;

		if (!asprintf(&new_authtok_required, "%d", retval)) {
			retval = PAM_BUF_ERR;
			goto out;
		}

		pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD, new_authtok_required, _pam_winbind_cleanup_func);

		retval = PAM_SUCCESS;

		if (!asprintf(&new_authtok_required_during_auth, "%d", True)) {
			retval = PAM_BUF_ERR;
			goto out;
		}

		pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH, 
			     new_authtok_required_during_auth, _pam_winbind_cleanup_func);

		goto out;
	}

out:
	if (username_ret) {
		pam_set_item (pamh, PAM_USER, username_ret);
		_pam_log_debug(pamh, ctrl, LOG_INFO, "Returned user was '%s'", username_ret);
		free(username_ret);
	}

	if ( real_username ) {		
		free( real_username );
	}	
			
	if (d) {
		iniparser_freedict(d);
	}

	if (!new_authtok_required) {
		pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD, NULL, NULL);
	}

	if (retval != PAM_SUCCESS) {
		_pam_free_data_info3(pamh);
	}

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_authenticate", pamh, ctrl, retval);

	return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
	int ret = PAM_SYSTEM_ERR;
	dictionary *d = NULL;

	/* parse arguments */
	int ctrl = _pam_parse(pamh, flags, argc, argv, &d);
	if (ctrl == -1) {
		ret = PAM_SYSTEM_ERR;
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_setcred", pamh, ctrl, flags);

	switch (flags & ~PAM_SILENT) {

		case PAM_DELETE_CRED:
			ret = pam_sm_close_session(pamh, flags, argc, argv);
			break;
		case PAM_REFRESH_CRED:
			_pam_log_debug(pamh, ctrl, LOG_WARNING, "PAM_REFRESH_CRED not implemented");
			ret = PAM_SUCCESS;
			break;
		case PAM_REINITIALIZE_CRED:
			_pam_log_debug(pamh, ctrl, LOG_WARNING, "PAM_REINITIALIZE_CRED not implemented");
			ret = PAM_SUCCESS;
			break;
		case PAM_ESTABLISH_CRED:
			_pam_log_debug(pamh, ctrl, LOG_WARNING, "PAM_ESTABLISH_CRED not implemented");
			ret = PAM_SUCCESS;
			break;
		default:
			ret = PAM_SYSTEM_ERR;
			break;
	}

 out:
	if (d) {
		iniparser_freedict(d);
	}

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_setcred", pamh, ctrl, ret);
	
	return ret;
}

/*
 * Account management. We want to verify that the account exists 
 * before returning PAM_SUCCESS
 */
PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
	const char *username;
	int ret = PAM_USER_UNKNOWN;
	void *tmp = NULL;
	dictionary *d = NULL;

	/* parse arguments */
	int ctrl = _pam_parse(pamh, flags, argc, argv, &d);
	if (ctrl == -1) {
		return PAM_SYSTEM_ERR;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_acct_mgmt", pamh, ctrl, flags);


	/* Get the username */
	ret = pam_get_user(pamh, &username, NULL);
	if ((ret != PAM_SUCCESS) || (!username)) {
		_pam_log_debug(pamh, ctrl, LOG_DEBUG,"can not get the username");
		ret = PAM_SERVICE_ERR;
		goto out;
	}

	/* Verify the username */
	ret = valid_user(pamh, ctrl, username);
	switch (ret) {
	case -1:
		/* some sort of system error. The log was already printed */
		ret = PAM_SERVICE_ERR;
		goto out;
	case 1:
		/* the user does not exist */
		_pam_log_debug(pamh, ctrl, LOG_NOTICE, "user '%s' not found", username);
		if (ctrl & WINBIND_UNKNOWN_OK_ARG) {
			ret = PAM_IGNORE;
			goto out;
		}
		ret = PAM_USER_UNKNOWN;
		goto out;
	case 0:
		pam_get_data( pamh, PAM_WINBIND_NEW_AUTHTOK_REQD, (const void **)&tmp);
		if (tmp != NULL) {
			ret = atoi((const char *)tmp);
			switch (ret) {
			case PAM_AUTHTOK_EXPIRED:
				/* fall through, since new token is required in this case */
			case PAM_NEW_AUTHTOK_REQD:
				_pam_log(pamh, ctrl, LOG_WARNING, "pam_sm_acct_mgmt success but %s is set", 
					 PAM_WINBIND_NEW_AUTHTOK_REQD);
				_pam_log(pamh, ctrl, LOG_NOTICE, "user '%s' needs new password", username);
				/* PAM_AUTHTOKEN_REQD does not exist, but is documented in the manpage */
				ret = PAM_NEW_AUTHTOK_REQD;
				goto out;
			default:
				_pam_log(pamh, ctrl, LOG_WARNING, "pam_sm_acct_mgmt success");
				_pam_log(pamh, ctrl, LOG_NOTICE, "user '%s' granted access", username);
				ret = PAM_SUCCESS;
				goto out;
			}
		}

		/* Otherwise, the authentication looked good */
		_pam_log(pamh, ctrl, LOG_NOTICE, "user '%s' granted access", username);
		ret = PAM_SUCCESS;
		goto out;
	default:
		/* we don't know anything about this return value */
		_pam_log(pamh, ctrl, LOG_ERR, "internal module error (ret = %d, user = '%s')", 
			 ret, username);
		ret = PAM_SERVICE_ERR;
		goto out;
	}

	/* should not be reached */
	ret = PAM_IGNORE;

 out:

 	if (d) {
		iniparser_freedict(d);
	}

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_acct_mgmt", pamh, ctrl, ret);
	
	return ret;
}

PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
	int ret = PAM_SYSTEM_ERR;
	dictionary *d = NULL;

	/* parse arguments */
	int ctrl = _pam_parse(pamh, flags, argc, argv, &d);
	if (ctrl == -1) {
		ret = PAM_SYSTEM_ERR;
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_open_session", pamh, ctrl, flags);

	ret = PAM_SUCCESS;

 out:
	if (d) {
		iniparser_freedict(d);
	}

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_open_session", pamh, ctrl, ret);
	
	return ret;
}

PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh, int flags,
			 int argc, const char **argv)
{
	dictionary *d = NULL;
	int retval = PAM_SUCCESS;

	/* parse arguments */
	int ctrl = _pam_parse(pamh, flags, argc, argv, &d);
	if (ctrl == -1) {
		retval = PAM_SYSTEM_ERR;
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_close_session", pamh, ctrl, flags);

	if (!(flags & PAM_DELETE_CRED)) {
		retval = PAM_SUCCESS;
		goto out;
	}

	if (ctrl & WINBIND_KRB5_AUTH) {

		/* destroy the ccache here */
		struct winbindd_request request;
		struct winbindd_response response;
		const char *user;
		const char *ccname = NULL;
		struct passwd *pwd = NULL;

		ZERO_STRUCT(request);
		ZERO_STRUCT(response);

		retval = pam_get_user(pamh, &user, "Username: ");
		if (retval) {
			_pam_log(pamh, ctrl, LOG_ERR, "could not identify user");
			goto out;
		}

		if (user == NULL) {
			_pam_log(pamh, ctrl, LOG_ERR, "username was NULL!");
			retval = PAM_USER_UNKNOWN;
			goto out;
		}

		_pam_log_debug(pamh, ctrl, LOG_DEBUG, "username [%s] obtained", user);

		ccname = pam_getenv(pamh, "KRB5CCNAME");
		if (ccname == NULL) {
			_pam_log_debug(pamh, ctrl, LOG_DEBUG, "user has no KRB5CCNAME environment");
		}

		strncpy(request.data.logoff.user, user,
			sizeof(request.data.logoff.user) - 1);

		if (ccname) {
			strncpy(request.data.logoff.krb5ccname, ccname,
				sizeof(request.data.logoff.krb5ccname) - 1);
		}

		pwd = getpwnam(user);
		if (pwd == NULL) {
			retval = PAM_USER_UNKNOWN;
			goto out;
		}
		request.data.logoff.uid = pwd->pw_uid;

		request.flags = WBFLAG_PAM_KRB5 | WBFLAG_PAM_CONTACT_TRUSTDOM;

	        retval = pam_winbind_request_log(pamh, ctrl, WINBINDD_PAM_LOGOFF, &request, &response, user);
	}

out:
	if (d) {
		iniparser_freedict(d);
	}

	/*
	 * Delete the krb5 ccname variable from the PAM environment
	 * if it was set by winbind.
	 */
	if (ctrl & WINBIND_KRB5_AUTH) {
		pam_putenv(pamh, "KRB5CCNAME");
	}

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_close_session", pamh, ctrl, retval);
	
	return retval;
}

/**
 * evaluate whether we need to re-authenticate with kerberos after a password change
 * 
 * @param pamh PAM handle
 * @param ctrl PAM winbind options.
 * @param user The username
 *
 * @return boolean Returns True if required, False if not.
 */

static BOOL _pam_require_krb5_auth_after_chauthtok(pam_handle_t *pamh, int ctrl, const char *user)
{

	/* Make sure that we only do this if
	 * a) the chauthtok got initiated during a logon attempt (authenticate->acct_mgmt->chauthtok)
	 * b) any later password change via the "passwd" command if done by the user itself 
	 *
	 * NB. If we login from gdm or xdm and the password expires,
	 * we change the password, but there is no memory cache.
	 * Thus, even for passthrough login, we should do the
	 * authentication again to update memory cache.
	 * --- BoYang
	 * */

	char *new_authtok_reqd_during_auth = NULL;
	struct passwd *pwd = NULL;

	_pam_get_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH, &new_authtok_reqd_during_auth);
	pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH, NULL, NULL);

	if (new_authtok_reqd_during_auth) {
		return True;
	}

	pwd = getpwnam(user);
	if (!pwd) {
		return False;
	}

	if (getuid() == pwd->pw_uid) {
		return True;
	}

	return False;
}


PAM_EXTERN 
int pam_sm_chauthtok(pam_handle_t * pamh, int flags,
		     int argc, const char **argv)
{
	unsigned int lctrl;
	int ret;
	unsigned int ctrl;
	bool cached_login = False;

	/* <DO NOT free() THESE> */
	const char *user;
	char *pass_old, *pass_new;
	/* </DO NOT free() THESE> */

	char *Announce;
	
	int retry = 0;
	dictionary *d = NULL;
	char *username_ret = NULL;
	struct winbindd_response response;

	ZERO_STRUCT(response);

	ctrl = _pam_parse(pamh, flags, argc, argv, &d);
	if (ctrl == -1) {
		ret = PAM_SYSTEM_ERR;
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_chauthtok", pamh, ctrl, flags);

	cached_login = (ctrl & WINBIND_CACHED_LOGIN);

	/* clearing offline bit for auth */
	ctrl &= ~WINBIND_CACHED_LOGIN;

	/*
	 * First get the name of a user
	 */
	ret = pam_get_user(pamh, &user, "Username: ");
	if (ret) {
		_pam_log(pamh, ctrl, LOG_ERR,
			 "password - could not identify user");
		goto out;
	}

	if (user == NULL) {
		_pam_log(pamh, ctrl, LOG_ERR, "username was NULL!");
		ret = PAM_USER_UNKNOWN;
		goto out;
	}

	_pam_log_debug(pamh, ctrl, LOG_DEBUG, "username [%s] obtained", user);

	/* check if this is really a user in winbindd, not only in NSS */
	ret = valid_user(pamh, ctrl, user);
	switch (ret) {
		case 1:
			ret = PAM_USER_UNKNOWN;
			goto out;
		case -1:
			ret = PAM_SYSTEM_ERR;
			goto out;
		default:
			break;
	}
		
	/*
	 * obtain and verify the current password (OLDAUTHTOK) for
	 * the user.
	 */

	if (flags & PAM_PRELIM_CHECK) {
		time_t pwdlastset_prelim = 0;
		
		/* instruct user what is happening */
#define greeting "Changing password for "
		Announce = (char *) malloc(sizeof(greeting) + strlen(user));
		if (Announce == NULL) {
			_pam_log(pamh, ctrl, LOG_CRIT, "password - out of memory");
			ret = PAM_BUF_ERR;
			goto out;
		}
		(void) strcpy(Announce, greeting);
		(void) strcpy(Announce + sizeof(greeting) - 1, user);
#undef greeting
		
		lctrl = ctrl | WINBIND__OLD_PASSWORD;
		ret = _winbind_read_password(pamh, lctrl,
						Announce,
						"(current) NT password: ",
						NULL,
						(const char **) &pass_old);
		if (ret != PAM_SUCCESS) {
			_pam_log(pamh, ctrl, LOG_NOTICE, "password - (old) token not obtained");
			goto out;
		}

		/* verify that this is the password for this user */
		
		ret = winbind_auth_request(pamh, ctrl, user, pass_old,
					NULL, NULL, &response, &pwdlastset_prelim, NULL);

		if (ret != PAM_ACCT_EXPIRED && 
		    ret != PAM_AUTHTOK_EXPIRED &&
		    ret != PAM_NEW_AUTHTOK_REQD &&
		    ret != PAM_SUCCESS) {
			pass_old = NULL;
			goto out;
		}
		
		pam_set_data(pamh, PAM_WINBIND_PWD_LAST_SET, (void *)pwdlastset_prelim, NULL);

		ret = pam_set_item(pamh, PAM_OLDAUTHTOK, (const void *) pass_old);
		pass_old = NULL;
		if (ret != PAM_SUCCESS) {
			_pam_log(pamh, ctrl, LOG_CRIT, "failed to set PAM_OLDAUTHTOK");
		}
	} else if (flags & PAM_UPDATE_AUTHTOK) {
	
		time_t pwdlastset_update = 0;
		
		/*
		 * obtain the proposed password
		 */
		
		/*
		 * get the old token back. 
		 */
		
		ret = _pam_get_item(pamh, PAM_OLDAUTHTOK, &pass_old);
		
		if (ret != PAM_SUCCESS) {
			_pam_log(pamh, ctrl, LOG_NOTICE, "user not authenticated");
			goto out;
		}
		
		lctrl = ctrl & ~WINBIND_TRY_FIRST_PASS_ARG;
		
		if (on(WINBIND_USE_AUTHTOK_ARG, lctrl)) {
			lctrl |= WINBIND_USE_FIRST_PASS_ARG;
		}
		retry = 0;
		ret = PAM_AUTHTOK_ERR;
		while ((ret != PAM_SUCCESS) && (retry++ < MAX_PASSWD_TRIES)) {
			/*
			 * use_authtok is to force the use of a previously entered
			 * password -- needed for pluggable password strength checking
			 */
			
			ret = _winbind_read_password(pamh, lctrl,
							NULL,
							"Enter new NT password: ",
							"Retype new NT password: ",
							(const char **) &pass_new);
			
			if (ret != PAM_SUCCESS) {
				_pam_log_debug(pamh, ctrl, LOG_ALERT
					 ,"password - new password not obtained");
				pass_old = NULL;/* tidy up */
				goto out;
			}

			/*
			 * At this point we know who the user is and what they
			 * propose as their new password. Verify that the new
			 * password is acceptable.
			 */
			
			if (pass_new[0] == '\0') {/* "\0" password = NULL */
				pass_new = NULL;
			}
		}
		
		/*
		 * By reaching here we have approved the passwords and must now
		 * rebuild the password database file.
		 */
		_pam_get_data( pamh, PAM_WINBIND_PWD_LAST_SET,
			       &pwdlastset_update);

		/*
		 * if cached creds were enabled, make sure to set the
		 * WINBIND_CACHED_LOGIN bit here in order to have winbindd
		 * update the cached creds storage - gd
		 */
		if (cached_login) {
			ctrl |= WINBIND_CACHED_LOGIN;
		}

		ret = winbind_chauthtok_request(pamh, ctrl, user, pass_old, pass_new, pwdlastset_update);
		if (ret) {
			_pam_overwrite(pass_new);
			_pam_overwrite(pass_old);
			pass_old = pass_new = NULL;
			goto out;
		}

		if (_pam_require_krb5_auth_after_chauthtok(pamh, ctrl, user)) {

			const char *member = get_member_from_config(pamh, argc, argv, ctrl, d);
			const char *cctype = get_krb5_cc_type_from_config(pamh, argc, argv, ctrl, d);

                        /* Keep the WINBIND_CACHED_LOGIN bit for
                         * authentication after changing the password.
                         * This will update the cached credentials in case
                         * that winbindd_dual_pam_chauthtok() fails
                         * to update them.
                         * --- BoYang
                         * */

			ret = winbind_auth_request(pamh, ctrl, user, pass_new,
							member, cctype, &response, NULL, &username_ret);
			_pam_overwrite(pass_new);
			_pam_overwrite(pass_old);
			pass_old = pass_new = NULL;

			if (ret == PAM_SUCCESS) {
			
				/* warn a user if the password is about to expire soon */
				_pam_warn_password_expiry(pamh, ctrl, &response, NULL);

				/* set some info3 info for other modules in the stack */
				_pam_set_data_info3(pamh, ctrl, &response);

				/* put krb5ccname into env */
				_pam_setup_krb5_env(pamh, ctrl, response.data.auth.krb5ccname);

				if (username_ret) {
					pam_set_item (pamh, PAM_USER, username_ret);
					_pam_log_debug(pamh, ctrl, LOG_INFO, "Returned user was '%s'", username_ret);
					free(username_ret);
				}
			}

			goto out;
		}
	} else {
		ret = PAM_SERVICE_ERR;
	}

out:
	if (d) {
		iniparser_freedict(d);
	}

	/* Deal with offline errors. */
	PAM_WB_REMARK_CHECK_RESPONSE(pamh, ctrl, response, "NT_STATUS_NO_LOGON_SERVERS");
	PAM_WB_REMARK_CHECK_RESPONSE(pamh, ctrl, response, "NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND");
	PAM_WB_REMARK_CHECK_RESPONSE(pamh, ctrl, response, "NT_STATUS_ACCESS_DENIED");

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_chauthtok", pamh, ctrl, ret);
	
	return ret;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_winbind_modstruct = {
	MODULE_NAME,
	pam_sm_authenticate,
	pam_sm_setcred,
	pam_sm_acct_mgmt,
	pam_sm_open_session,
	pam_sm_close_session,
	pam_sm_chauthtok
};

#endif

/*
 * Copyright (c) Andrew Tridgell  <tridge@samba.org>   2000
 * Copyright (c) Tim Potter       <tpot@samba.org>     2000
 * Copyright (c) Andrew Bartlettt <abartlet@samba.org> 2002
 * Copyright (c) Guenther Deschner <gd@samba.org>      2005-2007
 * Copyright (c) Jan Rêkorajski 1999.
 * Copyright (c) Andrew G. Morgan 1996-8.
 * Copyright (c) Alex O. Yuriev, 1996.
 * Copyright (c) Cristian Gafton 1996.
 * Copyright (C) Elliot Lee <sopwith@redhat.com> 1996, Red Hat Software. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED `AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
