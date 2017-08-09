/* pam_winbind module

   Copyright Andrew Tridgell <tridge@samba.org> 2000
   Copyright Tim Potter <tpot@samba.org> 2000
   Copyright Andrew Bartlett <abartlet@samba.org> 2002
   Copyright Guenther Deschner <gd@samba.org> 2005-2008

   largely based on pam_userdb by Cristian Gafton <gafton@redhat.com> also
   contains large slabs of code from pam_unix by Elliot Lee
   <sopwith@redhat.com> (see copyright below for full details)
*/

#include "pam_winbind.h"
#define CONST_DISCARD(type,ptr) ((type)(void *)ptr)


static int wbc_error_to_pam_error(wbcErr status)
{
	switch (status) {
		case WBC_ERR_SUCCESS:
			return PAM_SUCCESS;
		case WBC_ERR_NOT_IMPLEMENTED:
			return PAM_SERVICE_ERR;
		case WBC_ERR_UNKNOWN_FAILURE:
			break;
		case WBC_ERR_NO_MEMORY:
			return PAM_BUF_ERR;
		case WBC_ERR_INVALID_SID:
		case WBC_ERR_INVALID_PARAM:
			break;
		case WBC_ERR_WINBIND_NOT_AVAILABLE:
			return PAM_AUTHINFO_UNAVAIL;
		case WBC_ERR_DOMAIN_NOT_FOUND:
			return PAM_AUTHINFO_UNAVAIL;
		case WBC_ERR_INVALID_RESPONSE:
			return PAM_BUF_ERR;
		case WBC_ERR_NSS_ERROR:
			return PAM_USER_UNKNOWN;
		case WBC_ERR_AUTH_ERROR:
			return PAM_AUTH_ERR;
		case WBC_ERR_UNKNOWN_USER:
			return PAM_USER_UNKNOWN;
		case WBC_ERR_UNKNOWN_GROUP:
			return PAM_USER_UNKNOWN;
		case WBC_ERR_PWD_CHANGE_FAILED:
			break;
	}

	/* be paranoid */
	return PAM_AUTH_ERR;
}

static const char *_pam_error_code_str(int err)
{
	switch (err) {
		case PAM_SUCCESS:
			return "PAM_SUCCESS";
		case PAM_OPEN_ERR:
			return "PAM_OPEN_ERR";
		case PAM_SYMBOL_ERR:
			return "PAM_SYMBOL_ERR";
		case PAM_SERVICE_ERR:
			return "PAM_SERVICE_ERR";
		case PAM_SYSTEM_ERR:
			return "PAM_SYSTEM_ERR";
		case PAM_BUF_ERR:
			return "PAM_BUF_ERR";
		case PAM_PERM_DENIED:
			return "PAM_PERM_DENIED";
		case PAM_AUTH_ERR:
			return "PAM_AUTH_ERR";
		case PAM_CRED_INSUFFICIENT:
			return "PAM_CRED_INSUFFICIENT";
		case PAM_AUTHINFO_UNAVAIL:
			return "PAM_AUTHINFO_UNAVAIL";
		case PAM_USER_UNKNOWN:
			return "PAM_USER_UNKNOWN";
		case PAM_MAXTRIES:
			return "PAM_MAXTRIES";
		case PAM_NEW_AUTHTOK_REQD:
			return "PAM_NEW_AUTHTOK_REQD";
		case PAM_ACCT_EXPIRED:
			return "PAM_ACCT_EXPIRED";
		case PAM_SESSION_ERR:
			return "PAM_SESSION_ERR";
		case PAM_CRED_UNAVAIL:
			return "PAM_CRED_UNAVAIL";
		case PAM_CRED_EXPIRED:
			return "PAM_CRED_EXPIRED";
		case PAM_CRED_ERR:
			return "PAM_CRED_ERR";
		case PAM_NO_MODULE_DATA:
			return "PAM_NO_MODULE_DATA";
		case PAM_CONV_ERR:
			return "PAM_CONV_ERR";
		case PAM_AUTHTOK_ERR:
			return "PAM_AUTHTOK_ERR";
		case PAM_AUTHTOK_RECOVER_ERR:
			return "PAM_AUTHTOK_RECOVER_ERR";
		case PAM_AUTHTOK_LOCK_BUSY:
			return "PAM_AUTHTOK_LOCK_BUSY";
		case PAM_AUTHTOK_DISABLE_AGING:
			return "PAM_AUTHTOK_DISABLE_AGING";
		case PAM_TRY_AGAIN:
			return "PAM_TRY_AGAIN";
		case PAM_IGNORE:
			return "PAM_IGNORE";
		case PAM_ABORT:
			return "PAM_ABORT";
		case PAM_AUTHTOK_EXPIRED:
			return "PAM_AUTHTOK_EXPIRED";
#ifdef PAM_MODULE_UNKNOWN
		case PAM_MODULE_UNKNOWN:
			return "PAM_MODULE_UNKNOWN";
#endif
#ifdef PAM_BAD_ITEM
		case PAM_BAD_ITEM:
			return "PAM_BAD_ITEM";
#endif
#ifdef PAM_CONV_AGAIN
		case PAM_CONV_AGAIN:
			return "PAM_CONV_AGAIN";
#endif
#ifdef PAM_INCOMPLETE
		case PAM_INCOMPLETE:
			return "PAM_INCOMPLETE";
#endif
		default:
			return NULL;
	}
}

#define _PAM_LOG_FUNCTION_ENTER(function, ctx) \
	do { \
		_pam_log_debug(ctx, LOG_DEBUG, "[pamh: %p] ENTER: " \
			       function " (flags: 0x%04x)", ctx->pamh, ctx->flags); \
		_pam_log_state(ctx); \
	} while (0)

#define _PAM_LOG_FUNCTION_LEAVE(function, ctx, retval) \
	do { \
		_pam_log_debug(ctx, LOG_DEBUG, "[pamh: %p] LEAVE: " \
			       function " returning %d (%s)", ctx->pamh, retval, \
			       _pam_error_code_str(retval)); \
		_pam_log_state(ctx); \
	} while (0)

/* data tokens */

#define MAX_PASSWD_TRIES	3

#ifdef HAVE_GETTEXT
static char initialized = 0;

static inline void textdomain_init(void);
static inline void textdomain_init(void)
{
	if (!initialized) {
		bindtextdomain(MODULE_NAME, LOCALEDIR);
		initialized = 1;
	}
	return;
}
#endif


/*
 * Work around the pam API that has functions with void ** as parameters
 * These lead to strict aliasing warnings with gcc.
 */
static int _pam_get_item(const pam_handle_t *pamh,
			 int item_type,
			 const void *_item)
{
	const void **item = (const void **)_item;
	return pam_get_item(pamh, item_type, item);
}
static int _pam_get_data(const pam_handle_t *pamh,
			 const char *module_data_name,
			 const void *_data)
{
	const void **data = (const void **)_data;
	return pam_get_data(pamh, module_data_name, data);
}

/* some syslogging */

#ifdef HAVE_PAM_VSYSLOG
static void _pam_log_int(const pam_handle_t *pamh,
			 int err,
			 const char *format,
			 va_list args)
{
	pam_vsyslog(pamh, err, format, args);
}
#else
static void _pam_log_int(const pam_handle_t *pamh,
			 int err,
			 const char *format,
			 va_list args)
{
	char *format2 = NULL;
	const char *service;

	_pam_get_item(pamh, PAM_SERVICE, &service);

	format2 = (char *)malloc(strlen(MODULE_NAME)+strlen(format)+strlen(service)+5);
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

static bool _pam_log_is_silent(int ctrl)
{
	return on(ctrl, WINBIND_SILENT);
}

static void _pam_log(struct pwb_context *r, int err, const char *format, ...) PRINTF_ATTRIBUTE(3,4);
static void _pam_log(struct pwb_context *r, int err, const char *format, ...)
{
	va_list args;

	if (_pam_log_is_silent(r->ctrl)) {
		return;
	}

	va_start(args, format);
	_pam_log_int(r->pamh, err, format, args);
	va_end(args);
}
static void __pam_log(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...) PRINTF_ATTRIBUTE(4,5);
static void __pam_log(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...)
{
	va_list args;

	if (_pam_log_is_silent(ctrl)) {
		return;
	}

	va_start(args, format);
	_pam_log_int(pamh, err, format, args);
	va_end(args);
}

static bool _pam_log_is_debug_enabled(int ctrl)
{
	if (ctrl == -1) {
		return false;
	}

	if (_pam_log_is_silent(ctrl)) {
		return false;
	}

	if (!(ctrl & WINBIND_DEBUG_ARG)) {
		return false;
	}

	return true;
}

static bool _pam_log_is_debug_state_enabled(int ctrl)
{
	if (!(ctrl & WINBIND_DEBUG_STATE)) {
		return false;
	}

	return _pam_log_is_debug_enabled(ctrl);
}

static void _pam_log_debug(struct pwb_context *r, int err, const char *format, ...) PRINTF_ATTRIBUTE(3,4);
static void _pam_log_debug(struct pwb_context *r, int err, const char *format, ...)
{
	va_list args;

	if (!_pam_log_is_debug_enabled(r->ctrl)) {
		return;
	}

	va_start(args, format);
	_pam_log_int(r->pamh, err, format, args);
	va_end(args);
}
static void __pam_log_debug(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...) PRINTF_ATTRIBUTE(4,5);
static void __pam_log_debug(const pam_handle_t *pamh, int ctrl, int err, const char *format, ...)
{
	va_list args;

	if (!_pam_log_is_debug_enabled(ctrl)) {
		return;
	}

	va_start(args, format);
	_pam_log_int(pamh, err, format, args);
	va_end(args);
}

static void _pam_log_state_datum(struct pwb_context *ctx,
				 int item_type,
				 const char *key,
				 int is_string)
{
	const void *data = NULL;
	if (item_type != 0) {
		pam_get_item(ctx->pamh, item_type, &data);
	} else {
		pam_get_data(ctx->pamh, key, &data);
	}
	if (data != NULL) {
		const char *type = (item_type != 0) ? "ITEM" : "DATA";
		if (is_string != 0) {
			_pam_log_debug(ctx, LOG_DEBUG,
				       "[pamh: %p] STATE: %s(%s) = \"%s\" (%p)",
				       ctx->pamh, type, key, (const char *)data,
				       data);
		} else {
			_pam_log_debug(ctx, LOG_DEBUG,
				       "[pamh: %p] STATE: %s(%s) = %p",
				       ctx->pamh, type, key, data);
		}
	}
}

#define _PAM_LOG_STATE_DATA_POINTER(ctx, module_data_name) \
	_pam_log_state_datum(ctx, 0, module_data_name, 0)

#define _PAM_LOG_STATE_DATA_STRING(ctx, module_data_name) \
	_pam_log_state_datum(ctx, 0, module_data_name, 1)

#define _PAM_LOG_STATE_ITEM_POINTER(ctx, item_type) \
	_pam_log_state_datum(ctx, item_type, #item_type, 0)

#define _PAM_LOG_STATE_ITEM_STRING(ctx, item_type) \
	_pam_log_state_datum(ctx, item_type, #item_type, 1)

#ifdef DEBUG_PASSWORD
#define _LOG_PASSWORD_AS_STRING 1
#else
#define _LOG_PASSWORD_AS_STRING 0
#endif

#define _PAM_LOG_STATE_ITEM_PASSWORD(ctx, item_type) \
	_pam_log_state_datum(ctx, item_type, #item_type, \
			     _LOG_PASSWORD_AS_STRING)

static void _pam_log_state(struct pwb_context *ctx)
{
	if (!_pam_log_is_debug_state_enabled(ctx->ctrl)) {
		return;
	}

	_PAM_LOG_STATE_ITEM_STRING(ctx, PAM_SERVICE);
	_PAM_LOG_STATE_ITEM_STRING(ctx, PAM_USER);
	_PAM_LOG_STATE_ITEM_STRING(ctx, PAM_TTY);
	_PAM_LOG_STATE_ITEM_STRING(ctx, PAM_RHOST);
	_PAM_LOG_STATE_ITEM_STRING(ctx, PAM_RUSER);
	_PAM_LOG_STATE_ITEM_PASSWORD(ctx, PAM_OLDAUTHTOK);
	_PAM_LOG_STATE_ITEM_PASSWORD(ctx, PAM_AUTHTOK);
	_PAM_LOG_STATE_ITEM_STRING(ctx, PAM_USER_PROMPT);
	_PAM_LOG_STATE_ITEM_POINTER(ctx, PAM_CONV);
#ifdef PAM_FAIL_DELAY
	_PAM_LOG_STATE_ITEM_POINTER(ctx, PAM_FAIL_DELAY);
#endif
#ifdef PAM_REPOSITORY
	_PAM_LOG_STATE_ITEM_POINTER(ctx, PAM_REPOSITORY);
#endif

	_PAM_LOG_STATE_DATA_STRING(ctx, PAM_WINBIND_HOMEDIR);
	_PAM_LOG_STATE_DATA_STRING(ctx, PAM_WINBIND_LOGONSCRIPT);
	_PAM_LOG_STATE_DATA_STRING(ctx, PAM_WINBIND_LOGONSERVER);
	_PAM_LOG_STATE_DATA_STRING(ctx, PAM_WINBIND_PROFILEPATH);
	_PAM_LOG_STATE_DATA_STRING(ctx,
				   PAM_WINBIND_NEW_AUTHTOK_REQD);
				   /* Use atoi to get PAM result code */
	_PAM_LOG_STATE_DATA_STRING(ctx,
				   PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH);
	_PAM_LOG_STATE_DATA_POINTER(ctx, PAM_WINBIND_PWD_LAST_SET);
}

static int _pam_parse(const pam_handle_t *pamh,
		      int flags,
		      int argc,
		      const char **argv,
		      dictionary **result_d)
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

	d = iniparser_load(CONST_DISCARD(char *, config_file));
	if (d == NULL) {
		goto config_from_pam;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:debug"), false)) {
		ctrl |= WINBIND_DEBUG_ARG;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:debug_state"), false)) {
		ctrl |= WINBIND_DEBUG_STATE;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:cached_login"), false)) {
		ctrl |= WINBIND_CACHED_LOGIN;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:krb5_auth"), false)) {
		ctrl |= WINBIND_KRB5_AUTH;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:silent"), false)) {
		ctrl |= WINBIND_SILENT;
	}

	if (iniparser_getstring(d, CONST_DISCARD(char *, "global:krb5_ccache_type"), NULL) != NULL) {
		ctrl |= WINBIND_KRB5_CCACHE_TYPE;
	}

	if ((iniparser_getstring(d, CONST_DISCARD(char *, "global:require-membership-of"), NULL)
	     != NULL) ||
	    (iniparser_getstring(d, CONST_DISCARD(char *, "global:require_membership_of"), NULL)
	     != NULL)) {
		ctrl |= WINBIND_REQUIRED_MEMBERSHIP;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:try_first_pass"), false)) {
		ctrl |= WINBIND_TRY_FIRST_PASS_ARG;
	}

	if (iniparser_getint(d, CONST_DISCARD(char *, "global:warn_pwd_expire"), 0)) {
		ctrl |= WINBIND_WARN_PWD_EXPIRE;
	}

	if (iniparser_getboolean(d, CONST_DISCARD(char *, "global:mkhomedir"), false)) {
		ctrl |= WINBIND_MKHOMEDIR;
	}

config_from_pam:
	/* step through arguments */
	for (i=argc,v=argv; i-- > 0; ++v) {

		/* generic options */
		if (!strcmp(*v,"debug"))
			ctrl |= WINBIND_DEBUG_ARG;
		else if (!strcasecmp(*v, "debug_state"))
			ctrl |= WINBIND_DEBUG_STATE;
		else if (!strcasecmp(*v, "silent"))
			ctrl |= WINBIND_SILENT;
		else if (!strcasecmp(*v, "use_authtok"))
			ctrl |= WINBIND_USE_AUTHTOK_ARG;
		else if (!strcasecmp(*v, "use_first_pass"))
			ctrl |= WINBIND_USE_FIRST_PASS_ARG;
		else if (!strcasecmp(*v, "try_first_pass"))
			ctrl |= WINBIND_TRY_FIRST_PASS_ARG;
		else if (!strcasecmp(*v, "unknown_ok"))
			ctrl |= WINBIND_UNKNOWN_OK_ARG;
		else if (!strncasecmp(*v, "require_membership_of",
				      strlen("require_membership_of")))
			ctrl |= WINBIND_REQUIRED_MEMBERSHIP;
		else if (!strncasecmp(*v, "require-membership-of",
				      strlen("require-membership-of")))
			ctrl |= WINBIND_REQUIRED_MEMBERSHIP;
		else if (!strcasecmp(*v, "krb5_auth"))
			ctrl |= WINBIND_KRB5_AUTH;
		else if (!strncasecmp(*v, "krb5_ccache_type",
				      strlen("krb5_ccache_type")))
			ctrl |= WINBIND_KRB5_CCACHE_TYPE;
		else if (!strcasecmp(*v, "cached_login"))
			ctrl |= WINBIND_CACHED_LOGIN;
		else if (!strcasecmp(*v, "mkhomedir"))
			ctrl |= WINBIND_MKHOMEDIR;
		else {
			__pam_log(pamh, ctrl, LOG_ERR,
				 "pam_parse: unknown option: %s", *v);
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

static int _pam_winbind_free_context(struct pwb_context *ctx)
{
	if (!ctx) {
		return 0;
	}

	if (ctx->dict) {
		iniparser_freedict(ctx->dict);
	}

	return 0;
}

static int _pam_winbind_init_context(pam_handle_t *pamh,
				     int flags,
				     int argc,
				     const char **argv,
				     struct pwb_context **ctx_p)
{
	struct pwb_context *r = NULL;

#ifdef HAVE_GETTEXT
	textdomain_init();
#endif

	r = TALLOC_ZERO_P(NULL, struct pwb_context);
	if (!r) {
		return PAM_BUF_ERR;
	}

	talloc_set_destructor(r, _pam_winbind_free_context);

	r->pamh = pamh;
	r->flags = flags;
	r->argc = argc;
	r->argv = argv;
	r->ctrl = _pam_parse(pamh, flags, argc, argv, &r->dict);
	if (r->ctrl == -1) {
		TALLOC_FREE(r);
		return PAM_SYSTEM_ERR;
	}

	*ctx_p = r;

	return PAM_SUCCESS;
}

static void _pam_winbind_cleanup_func(pam_handle_t *pamh,
				      void *data,
				      int error_status)
{
	int ctrl = _pam_parse(pamh, 0, 0, NULL, NULL);
	if (_pam_log_is_debug_state_enabled(ctrl)) {
		__pam_log_debug(pamh, ctrl, LOG_DEBUG,
			       "[pamh: %p] CLEAN: cleaning up PAM data %p "
			       "(error_status = %d)", pamh, data,
			       error_status);
	}
	TALLOC_FREE(data);
}


static const struct ntstatus_errors {
	const char *ntstatus_string;
	const char *error_string;
} ntstatus_errors[] = {
	{"NT_STATUS_OK",
		N_("Success")},
	{"NT_STATUS_BACKUP_CONTROLLER",
		N_("No primary Domain Controler available")},
	{"NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND",
		N_("No domain controllers found")},
	{"NT_STATUS_NO_LOGON_SERVERS",
		N_("No logon servers")},
	{"NT_STATUS_PWD_TOO_SHORT",
		N_("Password too short")},
	{"NT_STATUS_PWD_TOO_RECENT",
		N_("The password of this user is too recent to change")},
	{"NT_STATUS_PWD_HISTORY_CONFLICT",
		N_("Password is already in password history")},
	{"NT_STATUS_PASSWORD_EXPIRED",
		N_("Your password has expired")},
	{"NT_STATUS_PASSWORD_MUST_CHANGE",
		N_("You need to change your password now")},
	{"NT_STATUS_INVALID_WORKSTATION",
		N_("You are not allowed to logon from this workstation")},
	{"NT_STATUS_INVALID_LOGON_HOURS",
		N_("You are not allowed to logon at this time")},
	{"NT_STATUS_ACCOUNT_EXPIRED",
		N_("Your account has expired. "
		   "Please contact your System administrator")}, /* SCNR */
	{"NT_STATUS_ACCOUNT_DISABLED",
		N_("Your account is disabled. "
		   "Please contact your System administrator")}, /* SCNR */
	{"NT_STATUS_ACCOUNT_LOCKED_OUT",
		N_("Your account has been locked. "
		   "Please contact your System administrator")}, /* SCNR */
	{"NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT",
		N_("Invalid Trust Account")},
	{"NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT",
		N_("Invalid Trust Account")},
	{"NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT",
		N_("Invalid Trust Account")},
	{"NT_STATUS_ACCESS_DENIED",
		N_("Access is denied")},
	{NULL, NULL}
};

static const char *_get_ntstatus_error_string(const char *nt_status_string)
{
	int i;
	for (i=0; ntstatus_errors[i].ntstatus_string != NULL; i++) {
		if (!strcasecmp(ntstatus_errors[i].ntstatus_string,
				nt_status_string)) {
			return _(ntstatus_errors[i].error_string);
		}
	}
	return NULL;
}

/* --- authentication management functions --- */

/* Attempt a conversation */

static int converse(const pam_handle_t *pamh,
		    int nargs,
		    struct pam_message **message,
		    struct pam_response **response)
{
	int retval;
	struct pam_conv *conv;

	retval = _pam_get_item(pamh, PAM_CONV, &conv);
	if (retval == PAM_SUCCESS) {
		retval = conv->conv(nargs,
				    (const struct pam_message **)message,
				    response, conv->appdata_ptr);
	}

	return retval; /* propagate error status */
}


static int _make_remark(struct pwb_context *ctx,
			int type,
			const char *text)
{
	int retval = PAM_SUCCESS;

	struct pam_message *pmsg[1], msg[1];
	struct pam_response *resp;

	if (ctx->flags & WINBIND_SILENT) {
		return PAM_SUCCESS;
	}

	pmsg[0] = &msg[0];
	msg[0].msg = discard_const_p(char, text);
	msg[0].msg_style = type;

	resp = NULL;
	retval = converse(ctx->pamh, 1, pmsg, &resp);

	if (resp) {
		_pam_drop_reply(resp, 1);
	}
	return retval;
}

static int _make_remark_v(struct pwb_context *ctx,
			  int type,
			  const char *format,
			  va_list args)
{
	char *var;
	int ret;

	ret = vasprintf(&var, format, args);
	if (ret < 0) {
		_pam_log(ctx, LOG_ERR, "memory allocation failure");
		return ret;
	}

	ret = _make_remark(ctx, type, var);
	SAFE_FREE(var);
	return ret;
}

static int _make_remark_format(struct pwb_context *ctx, int type, const char *format, ...) PRINTF_ATTRIBUTE(3,4);
static int _make_remark_format(struct pwb_context *ctx, int type, const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = _make_remark_v(ctx, type, format, args);
	va_end(args);
	return ret;
}

static int pam_winbind_request_log(struct pwb_context *ctx,
				   int retval,
				   const char *user,
				   const char *fn)
{
	switch (retval) {
	case PAM_AUTH_ERR:
		/* incorrect password */
		_pam_log(ctx, LOG_WARNING, "user '%s' denied access "
			 "(incorrect password or invalid membership)", user);
		return retval;
	case PAM_ACCT_EXPIRED:
		/* account expired */
		_pam_log(ctx, LOG_WARNING, "user '%s' account expired",
			 user);
		return retval;
	case PAM_AUTHTOK_EXPIRED:
		/* password expired */
		_pam_log(ctx, LOG_WARNING, "user '%s' password expired",
			 user);
		return retval;
	case PAM_NEW_AUTHTOK_REQD:
		/* new password required */
		_pam_log(ctx, LOG_WARNING, "user '%s' new password "
			 "required", user);
		return retval;
	case PAM_USER_UNKNOWN:
		/* the user does not exist */
		_pam_log_debug(ctx, LOG_NOTICE, "user '%s' not found",
			       user);
		if (ctx->ctrl & WINBIND_UNKNOWN_OK_ARG) {
			return PAM_IGNORE;
		}
		return retval;
	case PAM_SUCCESS:
		/* Otherwise, the authentication looked good */
		if (strcmp(fn, "wbcLogonUser") == 0) {
			_pam_log(ctx, LOG_NOTICE,
				 "user '%s' granted access", user);
		} else {
			_pam_log(ctx, LOG_NOTICE,
				 "user '%s' OK", user);
		}
		return retval;
	default:
		/* we don't know anything about this return value */
		_pam_log(ctx, LOG_ERR,
			 "internal module error (retval = %s(%d), user = '%s')",
			_pam_error_code_str(retval), retval, user);
		return retval;
	}
}

static int wbc_auth_error_to_pam_error(struct pwb_context *ctx,
				       struct wbcAuthErrorInfo *e,
				       wbcErr status,
				       const char *username,
				       const char *fn)
{
	int ret = PAM_AUTH_ERR;

	if (WBC_ERROR_IS_OK(status)) {
		_pam_log_debug(ctx, LOG_DEBUG, "request %s succeeded",
			fn);
		ret = PAM_SUCCESS;
		return pam_winbind_request_log(ctx, ret, username, fn);
	}

	if (e) {
		if (e->pam_error != PAM_SUCCESS) {
			_pam_log(ctx, LOG_ERR,
				 "request %s failed: %s, "
				 "PAM error: %s (%d), NTSTATUS: %s, "
				 "Error message was: %s",
				 fn,
				 wbcErrorString(status),
				 _pam_error_code_str(e->pam_error),
				 e->pam_error,
				 e->nt_string,
				 e->display_string);
			ret = e->pam_error;
			return pam_winbind_request_log(ctx, ret, username, fn);
		}

		_pam_log(ctx, LOG_ERR, "request %s failed, but PAM error 0!", fn);

		ret = PAM_SERVICE_ERR;
		return pam_winbind_request_log(ctx, ret, username, fn);
	}

	ret = wbc_error_to_pam_error(status);
	return pam_winbind_request_log(ctx, ret, username, fn);
}

#if defined(HAVE_PAM_RADIO_TYPE)
static bool _pam_winbind_change_pwd(struct pwb_context *ctx)
{
	struct pam_message msg, *pmsg;
	struct pam_response *resp = NULL;
	const char *prompt;
	int ret;
	bool retval = false;
	prompt = _("Do you want to change your password now?");
	pmsg = &msg;
	msg.msg_style = PAM_RADIO_TYPE;
	msg.msg = prompt;
	ret = converse(ctx->pamh, 1, &pmsg, &resp);
	if (resp == NULL) {
		if (ret == PAM_SUCCESS) {
			_pam_log(ctx, LOG_CRIT, "pam_winbind: system error!\n");
			return false;
		}
	}
	if (ret != PAM_SUCCESS) {
		return false;
	}
	_pam_log(ctx, LOG_CRIT, "Received [%s] reply from application.\n", resp->resp);

	if ((resp->resp != NULL) && (strcasecmp(resp->resp, "yes") == 0)) {
		retval = true;
	}

	_pam_drop_reply(resp, 1);
	return retval;
}
#else
static bool _pam_winbind_change_pwd(struct pwb_context *ctx)
{
	return false;
}
#endif

/**
 * send a password expiry message if required
 *
 * @param ctx PAM winbind context.
 * @param next_change expected (calculated) next expiry date.
 * @param already_expired pointer to a boolean to indicate if the password is
 *        already expired.
 *
 * @return boolean Returns true if message has been sent, false if not.
 */

static bool _pam_send_password_expiry_message(struct pwb_context *ctx,
					      time_t next_change,
					      time_t now,
					      int warn_pwd_expire,
					      bool *already_expired,
					      bool *change_pwd)
{
	int days = 0;
	struct tm tm_now, tm_next_change;
	bool retval = false;
	int ret;

	if (already_expired) {
		*already_expired = false;
	}

	if (change_pwd) {
		*change_pwd = false;
	}

	if (next_change <= now) {
		PAM_WB_REMARK_DIRECT(ctx, "NT_STATUS_PASSWORD_EXPIRED");
		if (already_expired) {
			*already_expired = true;
		}
		return true;
	}

	if ((next_change < 0) ||
	    (next_change > now + warn_pwd_expire * SECONDS_PER_DAY)) {
		return false;
	}

	if ((localtime_r(&now, &tm_now) == NULL) ||
	    (localtime_r(&next_change, &tm_next_change) == NULL)) {
		return false;
	}

	days = (tm_next_change.tm_yday+tm_next_change.tm_year*365) -
	       (tm_now.tm_yday+tm_now.tm_year*365);

	if (days == 0) {
		ret = _make_remark(ctx, PAM_TEXT_INFO,
				_("Your password expires today.\n"));

		/*
		 * If change_pwd and already_expired is null.
		 * We are just sending a notification message.
		 * We don't expect any response in this case.
		 */

		if (!change_pwd && !already_expired) {
			return true;
		}

		/*
		 * successfully sent the warning message.
		 * Give the user a chance to change pwd.
		 */
		if (ret == PAM_SUCCESS) {
			if (change_pwd) {
				retval = _pam_winbind_change_pwd(ctx);
				if (retval) {
					*change_pwd = true;
				}
			}
		}
		return true;
	}

	if (days > 0 && days < warn_pwd_expire) {

		ret = _make_remark_format(ctx, PAM_TEXT_INFO,
					_("Your password will expire in %d %s.\n"),
					days, (days > 1) ? _("days"):_("day"));
		/*
		 * If change_pwd and already_expired is null.
		 * We are just sending a notification message.
		 * We don't expect any response in this case.
		 */

		if (!change_pwd && !already_expired) {
			return true;
		}

		/*
		 * successfully sent the warning message.
		 * Give the user a chance to change pwd.
		 */
		if (ret == PAM_SUCCESS) {
			if (change_pwd) {
				retval = _pam_winbind_change_pwd(ctx);
				if (retval) {
					*change_pwd = true;
				}
			}
		}
		return true;
	}

	return false;
}

/**
 * Send a warning if the password expires in the near future
 *
 * @param ctx PAM winbind context.
 * @param response The full authentication response structure.
 * @param already_expired boolean, is the pwd already expired?
 *
 * @return void.
 */

static void _pam_warn_password_expiry(struct pwb_context *ctx,
				      const struct wbcAuthUserInfo *info,
				      const struct wbcUserPasswordPolicyInfo *policy,
				      int warn_pwd_expire,
				      bool *already_expired,
				      bool *change_pwd)
{
	time_t now = time(NULL);
	time_t next_change = 0;

	if (!info || !policy) {
		return;
	}

	if (already_expired) {
		*already_expired = false;
	}

	if (change_pwd) {
		*change_pwd = false;
	}

	/* accounts with WBC_ACB_PWNOEXP set never receive a warning */
	if (info->acct_flags & WBC_ACB_PWNOEXP) {
		return;
	}

	/* no point in sending a warning if this is a grace logon */
	if (PAM_WB_GRACE_LOGON(info->user_flags)) {
		return;
	}

	/* check if the info3 must change timestamp has been set */
	next_change = info->pass_must_change_time;

	if (_pam_send_password_expiry_message(ctx, next_change, now,
					      warn_pwd_expire,
					      already_expired,
					      change_pwd)) {
		return;
	}

	/* now check for the global password policy */
	/* good catch from Ralf Haferkamp: an expiry of "never" is translated
	 * to -1 */
	if ((policy->expire == (int64_t)-1) ||
	    (policy->expire == 0)) {
		return;
	}

	next_change = info->pass_last_set_time + policy->expire;

	if (_pam_send_password_expiry_message(ctx, next_change, now,
					      warn_pwd_expire,
					      already_expired,
					      change_pwd)) {
		return;
	}

	/* no warning sent */
}

#define IS_SID_STRING(name) (strncmp("S-", name, 2) == 0)

/**
 * Append a string, making sure not to overflow and to always return a
 * NULL-terminated string.
 *
 * @param dest Destination string buffer (must already be NULL-terminated).
 * @param src Source string buffer.
 * @param dest_buffer_size Size of dest buffer in bytes.
 *
 * @return false if dest buffer is not big enough (no bytes copied), true on
 * success.
 */

static bool safe_append_string(char *dest,
			       const char *src,
			       int dest_buffer_size)
{
	int dest_length = strlen(dest);
	int src_length = strlen(src);

	if (dest_length + src_length + 1 > dest_buffer_size) {
		return false;
	}

	memcpy(dest + dest_length, src, src_length + 1);
	return true;
}

/**
 * Convert a names into a SID string, appending it to a buffer.
 *
 * @param ctx PAM winbind context.
 * @param user User in PAM request.
 * @param name Name to convert.
 * @param sid_list_buffer Where to append the string sid.
 * @param sid_list_buffer Size of sid_list_buffer (in bytes).
 *
 * @return false on failure, true on success.
 */
static bool winbind_name_to_sid_string(struct pwb_context *ctx,
				       const char *user,
				       const char *name,
				       char *sid_list_buffer,
				       int sid_list_buffer_size)
{
	char sid_string[WBC_SID_STRING_BUFLEN];

	/* lookup name? */
	if (IS_SID_STRING(name)) {
		strlcpy(sid_string, name, sizeof(sid_string));
	} else {
		wbcErr wbc_status;
		struct wbcDomainSid sid;
		enum wbcSidType type;

		_pam_log_debug(ctx, LOG_DEBUG,
			       "no sid given, looking up: %s\n", name);

		wbc_status = wbcLookupName("", name, &sid, &type);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			_pam_log(ctx, LOG_INFO,
				 "could not lookup name: %s\n", name);
			return false;
		}

		wbcSidToStringBuf(&sid, sid_string, sizeof(sid_string));
	}

	if (!safe_append_string(sid_list_buffer, sid_string,
				sid_list_buffer_size)) {
		return false;
	}
	return true;
}

/**
 * Convert a list of names into a list of sids.
 *
 * @param ctx PAM winbind context.
 * @param user User in PAM request.
 * @param name_list List of names or string sids, separated by commas.
 * @param sid_list_buffer Where to put the list of string sids.
 * @param sid_list_buffer Size of sid_list_buffer (in bytes).
 *
 * @return false on failure, true on success.
 */
static bool winbind_name_list_to_sid_string_list(struct pwb_context *ctx,
						 const char *user,
						 const char *name_list,
						 char *sid_list_buffer,
						 int sid_list_buffer_size)
{
	bool result = false;
	char *current_name = NULL;
	const char *search_location;
	const char *comma;
	int len;

	if (sid_list_buffer_size > 0) {
		sid_list_buffer[0] = 0;
	}

	search_location = name_list;
	while ((comma = strchr(search_location, ',')) != NULL) {
		current_name = strndup(search_location,
				       comma - search_location);
		if (NULL == current_name) {
			goto out;
		}

		if (!winbind_name_to_sid_string(ctx, user,
						current_name,
						sid_list_buffer,
						sid_list_buffer_size)) {
			/*
			 * If one group name failed, we must not fail
			 * the authentication totally, continue with
			 * the following group names. If user belongs to
			 * one of the valid groups, we must allow it
			 * login. -- BoYang
			 */

			_pam_log(ctx, LOG_INFO, "cannot convert group %s to sid, "
				 "check if group %s is valid group.", current_name,
				 current_name);
			_make_remark_format(ctx, PAM_TEXT_INFO, _("Cannot convert group %s "
					"to sid, please contact your administrator to see "
					"if group %s is valid."), current_name, current_name);
			SAFE_FREE(current_name);
			search_location = comma + 1;
			continue;
		}

		SAFE_FREE(current_name);

		if (!safe_append_string(sid_list_buffer, ",",
					sid_list_buffer_size)) {
			goto out;
		}

		search_location = comma + 1;
	}

	if (!winbind_name_to_sid_string(ctx, user, search_location,
					sid_list_buffer,
					sid_list_buffer_size)) {
		_pam_log(ctx, LOG_INFO, "cannot convert group %s to sid, "
			 "check if group %s is valid group.", search_location,
			 search_location);
		_make_remark_format(ctx, PAM_TEXT_INFO, _("Cannot convert group %s "
				"to sid, please contact your administrator to see "
				"if group %s is valid."), search_location, search_location);

		/* If no valid groups were converted we should fail outright */
		if (name_list != NULL && strlen(sid_list_buffer) == 0) {
			result = false;
			goto out;
		}
		/*
		 * The lookup of the last name failed..
		 * It results in require_member_of_sid ends with ','
		 * It is malformated parameter here, overwrite the last ','.
		 */
		len = strlen(sid_list_buffer);
		if ((len != 0) && (sid_list_buffer[len - 1] == ',')) {
			sid_list_buffer[len - 1] = '\0';
		}
	}

	result = true;

out:
	SAFE_FREE(current_name);
	return result;
}

/**
 * put krb5ccname variable into environment
 *
 * @param ctx PAM winbind context.
 * @param krb5ccname env variable retrieved from winbindd.
 *
 * @return void.
 */

static void _pam_setup_krb5_env(struct pwb_context *ctx,
				struct wbcLogonUserInfo *info)
{
	char var[PATH_MAX];
	int ret;
	uint32_t i;
	const char *krb5ccname = NULL;

	if (off(ctx->ctrl, WINBIND_KRB5_AUTH)) {
		return;
	}

	if (!info) {
		return;
	}

	for (i=0; i < info->num_blobs; i++) {
		if (strcasecmp(info->blobs[i].name, "krb5ccname") == 0) {
			krb5ccname = (const char *)info->blobs[i].blob.data;
			break;
		}
	}

	if (!krb5ccname || (strlen(krb5ccname) == 0)) {
		return;
	}

	_pam_log_debug(ctx, LOG_DEBUG,
		       "request returned KRB5CCNAME: %s", krb5ccname);

	if (snprintf(var, sizeof(var), "KRB5CCNAME=%s", krb5ccname) == -1) {
		return;
	}

	ret = pam_putenv(ctx->pamh, var);
	if (ret) {
		_pam_log(ctx, LOG_ERR,
			 "failed to set KRB5CCNAME to %s: %s",
			 var, pam_strerror(ctx->pamh, ret));
	}
}

/**
 * Copy unix username if available (further processed in PAM).
 *
 * @param ctx PAM winbind context
 * @param user_ret A pointer that holds a pointer to a string
 * @param unix_username A username
 *
 * @return void.
 */

static void _pam_setup_unix_username(struct pwb_context *ctx,
				     char **user_ret,
				     struct wbcLogonUserInfo *info)
{
	const char *unix_username = NULL;
	uint32_t i;

	if (!user_ret || !info) {
		return;
	}

	for (i=0; i < info->num_blobs; i++) {
		if (strcasecmp(info->blobs[i].name, "unix_username") == 0) {
			unix_username = (const char *)info->blobs[i].blob.data;
			break;
		}
	}

	if (!unix_username || !unix_username[0]) {
		return;
	}

	*user_ret = strdup(unix_username);
}

/**
 * Set string into the PAM stack.
 *
 * @param ctx PAM winbind context.
 * @param data_name Key name for pam_set_data.
 * @param value String value.
 *
 * @return void.
 */

static void _pam_set_data_string(struct pwb_context *ctx,
				 const char *data_name,
				 const char *value)
{
	int ret;

	if (!data_name || !value || (strlen(data_name) == 0) ||
	     (strlen(value) == 0)) {
		return;
	}

	ret = pam_set_data(ctx->pamh, data_name, talloc_strdup(NULL, value),
			   _pam_winbind_cleanup_func);
	if (ret) {
		_pam_log_debug(ctx, LOG_DEBUG,
			       "Could not set data %s: %s\n",
			       data_name, pam_strerror(ctx->pamh, ret));
	}
}

/**
 * Set info3 strings into the PAM stack.
 *
 * @param ctx PAM winbind context.
 * @param data_name Key name for pam_set_data.
 * @param value String value.
 *
 * @return void.
 */

static void _pam_set_data_info3(struct pwb_context *ctx,
				const struct wbcAuthUserInfo *info)
{
	_pam_set_data_string(ctx, PAM_WINBIND_HOMEDIR,
			     info->home_directory);
	_pam_set_data_string(ctx, PAM_WINBIND_LOGONSCRIPT,
			     info->logon_script);
	_pam_set_data_string(ctx, PAM_WINBIND_LOGONSERVER,
			     info->logon_server);
	_pam_set_data_string(ctx, PAM_WINBIND_PROFILEPATH,
			     info->profile_path);
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
 * @param ctx PAM winbind context.
 * @param username User in PAM request.
 * @param info3_user_flgs Info3 flags containing logon type bits.
 *
 * @return void.
 */

static void _pam_warn_logon_type(struct pwb_context *ctx,
				 const char *username,
				 uint32_t info3_user_flgs)
{
	/* inform about logon type */
	if (PAM_WB_GRACE_LOGON(info3_user_flgs)) {

		_make_remark(ctx, PAM_ERROR_MSG,
			     _("Grace login. "
			       "Please change your password as soon you're "
			       "online again"));
		_pam_log_debug(ctx, LOG_DEBUG,
			       "User %s logged on using grace logon\n",
			       username);

	} else if (PAM_WB_CACHED_LOGON(info3_user_flgs)) {

		_make_remark(ctx, PAM_ERROR_MSG,
			     _("Domain Controller unreachable, "
			       "using cached credentials instead. "
			       "Network resources may be unavailable"));
		_pam_log_debug(ctx, LOG_DEBUG,
			       "User %s logged on using cached credentials\n",
			       username);
	}
}

/**
 * Send PAM_ERROR_MSG for krb5 errors.
 *
 * @param ctx PAM winbind context.
 * @param username User in PAM request.
 * @param info3_user_flgs Info3 flags containing logon type bits.
 *
 * @return void.
 */

static void _pam_warn_krb5_failure(struct pwb_context *ctx,
				   const char *username,
				   uint32_t info3_user_flgs)
{
	if (PAM_WB_KRB5_CLOCK_SKEW(info3_user_flgs)) {
		_make_remark(ctx, PAM_ERROR_MSG,
			     _("Failed to establish your Kerberos Ticket cache "
			       "due time differences\n"
			       "with the domain controller.  "
			       "Please verify the system time.\n"));
		_pam_log_debug(ctx, LOG_DEBUG,
			       "User %s: Clock skew when getting Krb5 TGT\n",
			       username);
	}
}

static bool _pam_check_remark_auth_err(struct pwb_context *ctx,
				       const struct wbcAuthErrorInfo *e,
				       const char *nt_status_string,
				       int *pam_err)
{
	const char *ntstatus = NULL;
	const char *error_string = NULL;

	if (!e || !pam_err) {
		return false;
	}

	ntstatus = e->nt_string;
	if (!ntstatus) {
		return false;
	}

	if (strcasecmp(ntstatus, nt_status_string) == 0) {

		error_string = _get_ntstatus_error_string(nt_status_string);
		if (error_string) {
			_make_remark(ctx, PAM_ERROR_MSG, error_string);
			*pam_err = e->pam_error;
			return true;
		}

		if (e->display_string) {
			_make_remark(ctx, PAM_ERROR_MSG, _(e->display_string));
			*pam_err = e->pam_error;
			return true;
		}

		_make_remark(ctx, PAM_ERROR_MSG, nt_status_string);
		*pam_err = e->pam_error;

		return true;
	}

	return false;
};

/**
 * Compose Password Restriction String for a PAM_ERROR_MSG conversation.
 *
 * @param i The wbcUserPasswordPolicyInfo struct.
 *
 * @return string (caller needs to talloc_free).
 */

static char *_pam_compose_pwd_restriction_string(struct pwb_context *ctx,
						 struct wbcUserPasswordPolicyInfo *i)
{
	char *str = NULL;

	if (!i) {
		goto failed;
	}

	str = talloc_asprintf(ctx, _("Your password "));
	if (!str) {
		goto failed;
	}

	if (i->min_length_password > 0) {
		str = talloc_asprintf_append(str,
			       _("must be at least %d characters; "),
			       i->min_length_password);
		if (!str) {
			goto failed;
		}
	}

	if (i->password_history > 0) {
		str = talloc_asprintf_append(str,
			       _("cannot repeat any of your previous %d "
			        "passwords; "),
			       i->password_history);
		if (!str) {
			goto failed;
		}
	}

	if (i->password_properties & WBC_DOMAIN_PASSWORD_COMPLEX) {
		str = talloc_asprintf_append(str,
			       _("must contain capitals, numerals "
			         "or punctuation; "
			         "and cannot contain your account "
			         "or full name; "));
		if (!str) {
			goto failed;
		}
	}

	str = talloc_asprintf_append(str,
		       _("Please type a different password. "
		         "Type a password which meets these requirements in "
		         "both text boxes."));
	if (!str) {
		goto failed;
	}

	return str;

 failed:
	TALLOC_FREE(str);
	return NULL;
}

static int _pam_create_homedir(struct pwb_context *ctx,
			       const char *dirname,
			       mode_t mode)
{
	struct stat sbuf;

	if (stat(dirname, &sbuf) == 0) {
		return PAM_SUCCESS;
	}

	if (mkdir(dirname, mode) != 0) {

		_make_remark_format(ctx, PAM_TEXT_INFO,
				    _("Creating directory: %s failed: %s"),
				    dirname, strerror(errno));
		_pam_log(ctx, LOG_ERR, "could not create dir: %s (%s)",
		 dirname, strerror(errno));
		 return PAM_PERM_DENIED;
	}

	return PAM_SUCCESS;
}

static int _pam_chown_homedir(struct pwb_context *ctx,
			      const char *dirname,
			      uid_t uid,
			      gid_t gid)
{
	if (chown(dirname, uid, gid) != 0) {
		_pam_log(ctx, LOG_ERR, "failed to chown user homedir: %s (%s)",
			 dirname, strerror(errno));
		return PAM_PERM_DENIED;
	}

	return PAM_SUCCESS;
}

static int _pam_mkhomedir(struct pwb_context *ctx)
{
	struct passwd *pwd = NULL;
	char *token = NULL;
	char *create_dir = NULL;
	char *user_dir = NULL;
	int ret;
	const char *username;
	mode_t mode = 0700;
	char *safe_ptr = NULL;
	char *p = NULL;

	/* Get the username */
	ret = pam_get_user(ctx->pamh, &username, NULL);
	if ((ret != PAM_SUCCESS) || (!username)) {
		_pam_log_debug(ctx, LOG_DEBUG, "can not get the username");
		return PAM_SERVICE_ERR;
	}

	pwd = getpwnam(username);
	if (pwd == NULL) {
		_pam_log_debug(ctx, LOG_DEBUG, "can not get the username");
		return PAM_USER_UNKNOWN;
	}
	_pam_log_debug(ctx, LOG_DEBUG, "homedir is: %s", pwd->pw_dir);

	ret = _pam_create_homedir(ctx, pwd->pw_dir, 0700);
	if (ret == PAM_SUCCESS) {
		ret = _pam_chown_homedir(ctx, pwd->pw_dir,
					 pwd->pw_uid,
					 pwd->pw_gid);
	}

	if (ret == PAM_SUCCESS) {
		return ret;
	}

	/* maybe we need to create parent dirs */
	create_dir = talloc_strdup(ctx, "/");
	if (!create_dir) {
		return PAM_BUF_ERR;
	}

	/* find final directory */
	user_dir = strrchr(pwd->pw_dir, '/');
	if (!user_dir) {
		return PAM_BUF_ERR;
	}
	user_dir++;

	_pam_log(ctx, LOG_DEBUG, "final directory: %s", user_dir);

	p = pwd->pw_dir;

	while ((token = strtok_r(p, "/", &safe_ptr)) != NULL) {

		mode = 0755;

		p = NULL;

		_pam_log_debug(ctx, LOG_DEBUG, "token is %s", token);

		create_dir = talloc_asprintf_append(create_dir, "%s/", token);
		if (!create_dir) {
			return PAM_BUF_ERR;
		}
		_pam_log_debug(ctx, LOG_DEBUG, "current_dir is %s", create_dir);

		if (strcmp(token, user_dir) == 0) {
			_pam_log_debug(ctx, LOG_DEBUG, "assuming last directory: %s", token);
			mode = 0700;
		}

		ret = _pam_create_homedir(ctx, create_dir, mode);
		if (ret) {
			return ret;
		}
	}

	return _pam_chown_homedir(ctx, create_dir,
				  pwd->pw_uid,
				  pwd->pw_gid);
}

/* talk to winbindd */
static int winbind_auth_request(struct pwb_context *ctx,
				const char *user,
				const char *pass,
				const char *member,
				const char *cctype,
				const int warn_pwd_expire,
				struct wbcAuthErrorInfo **p_error,
				struct wbcLogonUserInfo **p_info,
				struct wbcUserPasswordPolicyInfo **p_policy,
				time_t *pwd_last_set,
				char **user_ret)
{
	wbcErr wbc_status;

	struct wbcLogonUserParams logon;
	char membership_of[1024];
	uid_t user_uid = -1;
	uint32_t flags = WBFLAG_PAM_INFO3_TEXT |
			 WBFLAG_PAM_GET_PWD_POLICY;

	struct wbcLogonUserInfo *info = NULL;
	struct wbcAuthUserInfo *user_info = NULL;
	struct wbcAuthErrorInfo *error = NULL;
	struct wbcUserPasswordPolicyInfo *policy = NULL;

	int ret = PAM_AUTH_ERR;
	int i;
	const char *codes[] = {
		"NT_STATUS_PASSWORD_EXPIRED",
		"NT_STATUS_PASSWORD_MUST_CHANGE",
		"NT_STATUS_INVALID_WORKSTATION",
		"NT_STATUS_INVALID_LOGON_HOURS",
		"NT_STATUS_ACCOUNT_EXPIRED",
		"NT_STATUS_ACCOUNT_DISABLED",
		"NT_STATUS_ACCOUNT_LOCKED_OUT",
		"NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT",
		"NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT",
		"NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT",
		"NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND",
		"NT_STATUS_NO_LOGON_SERVERS",
		"NT_STATUS_WRONG_PASSWORD",
		"NT_STATUS_ACCESS_DENIED"
	};

	if (pwd_last_set) {
		*pwd_last_set = 0;
	}

	/* Krb5 auth always has to go against the KDC of the user's realm */

	if (ctx->ctrl & WINBIND_KRB5_AUTH) {
		flags		|= WBFLAG_PAM_CONTACT_TRUSTDOM;
	}

	if (ctx->ctrl & (WINBIND_KRB5_AUTH|WINBIND_CACHED_LOGIN)) {
		struct passwd *pwd = NULL;

		pwd = getpwnam(user);
		if (pwd == NULL) {
			return PAM_USER_UNKNOWN;
		}
		user_uid	= pwd->pw_uid;
	}

	if (ctx->ctrl & WINBIND_KRB5_AUTH) {

		_pam_log_debug(ctx, LOG_DEBUG,
			       "enabling krb5 login flag\n");

		flags		|= WBFLAG_PAM_KRB5 |
				   WBFLAG_PAM_FALLBACK_AFTER_KRB5;
	}

	if (ctx->ctrl & WINBIND_CACHED_LOGIN) {
		_pam_log_debug(ctx, LOG_DEBUG,
			       "enabling cached login flag\n");
		flags		|= WBFLAG_PAM_CACHED_LOGIN;
	}

	if (user_ret) {
		*user_ret = NULL;
		flags		|= WBFLAG_PAM_UNIX_NAME;
	}

	if (cctype != NULL) {
		_pam_log_debug(ctx, LOG_DEBUG,
			       "enabling request for a %s krb5 ccache\n",
			       cctype);
	}

	if (member != NULL) {

		ZERO_STRUCT(membership_of);

		if (!winbind_name_list_to_sid_string_list(ctx, user, member,
							  membership_of,
							  sizeof(membership_of))) {
			_pam_log_debug(ctx, LOG_ERR,
				       "failed to serialize membership of sid "
				       "\"%s\"\n", member);
			return PAM_AUTH_ERR;
		}
	}

	ZERO_STRUCT(logon);

	logon.username			= user;
	logon.password			= pass;

	if (cctype) {
		wbc_status = wbcAddNamedBlob(&logon.num_blobs,
					     &logon.blobs,
					     "krb5_cc_type",
					     0,
					     (uint8_t *)cctype,
					     strlen(cctype)+1);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			goto done;
		}
	}

	wbc_status = wbcAddNamedBlob(&logon.num_blobs,
				     &logon.blobs,
				     "flags",
				     0,
				     (uint8_t *)&flags,
				     sizeof(flags));
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		goto done;
	}

	wbc_status = wbcAddNamedBlob(&logon.num_blobs,
				     &logon.blobs,
				     "user_uid",
				     0,
				     (uint8_t *)&user_uid,
				     sizeof(user_uid));
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		goto done;
	}

	if (member) {
		wbc_status = wbcAddNamedBlob(&logon.num_blobs,
					     &logon.blobs,
					     "membership_of",
					     0,
					     (uint8_t *)membership_of,
					     sizeof(membership_of));
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			goto done;
		}
	}

	wbc_status = wbcLogonUser(&logon, &info, &error, &policy);
	ret = wbc_auth_error_to_pam_error(ctx, error, wbc_status,
					  user, "wbcLogonUser");
	wbcFreeMemory(logon.blobs);
	logon.blobs = NULL;

	if (info && info->info) {
		user_info = info->info;
	}

	if (pwd_last_set && user_info) {
		*pwd_last_set = user_info->pass_last_set_time;
	}

	if (p_info && info) {
		*p_info = info;
	}

	if (p_policy && policy) {
		*p_policy = policy;
	}

	if (p_error && error) {
		/* We want to process the error in the caller. */
		*p_error = error;
		return ret;
	}

	for (i=0; i<ARRAY_SIZE(codes); i++) {
		int _ret = ret;
		if (_pam_check_remark_auth_err(ctx, error, codes[i], &_ret)) {
			ret = _ret;
			goto done;
		}
	}

	if ((ret == PAM_SUCCESS) && user_info && policy && info) {

		bool already_expired = false;
		bool change_pwd = false;

		/* warn a user if the password is about to expire soon */
		_pam_warn_password_expiry(ctx, user_info, policy,
					  warn_pwd_expire,
					  &already_expired,
					  &change_pwd);

		if (already_expired == true) {

			SMB_TIME_T last_set = user_info->pass_last_set_time;

			_pam_log_debug(ctx, LOG_DEBUG,
				       "Password has expired "
				       "(Password was last set: %lld, "
				       "the policy says it should expire here "
				       "%lld (now it's: %ld))\n",
				       (long long int)last_set,
				       (long long int)last_set +
				       policy->expire,
				       (long)time(NULL));

			return PAM_AUTHTOK_EXPIRED;
		}

		if (change_pwd) {
			ret = PAM_NEW_AUTHTOK_REQD;
			goto done;
		}

		/* inform about logon type */
		_pam_warn_logon_type(ctx, user, user_info->user_flags);

		/* inform about krb5 failures */
		_pam_warn_krb5_failure(ctx, user, user_info->user_flags);

		/* set some info3 info for other modules in the stack */
		_pam_set_data_info3(ctx, user_info);

		/* put krb5ccname into env */
		_pam_setup_krb5_env(ctx, info);

		/* If winbindd returned a username, return the pointer to it
		 * here. */
		_pam_setup_unix_username(ctx, user_ret, info);
	}

 done:
	wbcFreeMemory(logon.blobs);
	if (info && info->blobs && !p_info) {
		wbcFreeMemory(info->blobs);
	}
	if (error && !p_error) {
		wbcFreeMemory(error);
	}
	if (info && !p_info) {
		wbcFreeMemory(info);
	}
	if (policy && !p_policy) {
		wbcFreeMemory(policy);
	}

	return ret;
}

/* talk to winbindd */
static int winbind_chauthtok_request(struct pwb_context *ctx,
				     const char *user,
				     const char *oldpass,
				     const char *newpass,
				     time_t pwd_last_set)
{
	wbcErr wbc_status;
	struct wbcChangePasswordParams params;
	struct wbcAuthErrorInfo *error = NULL;
	struct wbcUserPasswordPolicyInfo *policy = NULL;
	enum wbcPasswordChangeRejectReason reject_reason = -1;
	uint32_t flags = 0;

	int i;
	const char *codes[] = {
		"NT_STATUS_BACKUP_CONTROLLER",
		"NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND",
		"NT_STATUS_NO_LOGON_SERVERS",
		"NT_STATUS_ACCESS_DENIED",
		"NT_STATUS_PWD_TOO_SHORT", /* TODO: tell the min pwd length ? */
		"NT_STATUS_PWD_TOO_RECENT", /* TODO: tell the minage ? */
		"NT_STATUS_PWD_HISTORY_CONFLICT" /* TODO: tell the history length ? */
	};
	int ret = PAM_AUTH_ERR;

	ZERO_STRUCT(params);

	if (ctx->ctrl & WINBIND_KRB5_AUTH) {
		flags |= WBFLAG_PAM_KRB5 |
			 WBFLAG_PAM_CONTACT_TRUSTDOM;
	}

	if (ctx->ctrl & WINBIND_CACHED_LOGIN) {
		flags |= WBFLAG_PAM_CACHED_LOGIN;
	}

	params.account_name		= user;
	params.level			= WBC_AUTH_USER_LEVEL_PLAIN;
	params.old_password.plaintext	= oldpass;
	params.new_password.plaintext	= newpass;
	params.flags			= flags;

	wbc_status = wbcChangeUserPasswordEx(&params, &error, &reject_reason, &policy);
	ret = wbc_auth_error_to_pam_error(ctx, error, wbc_status,
					  user, "wbcChangeUserPasswordEx");

	if (WBC_ERROR_IS_OK(wbc_status)) {
		_pam_log(ctx, LOG_NOTICE,
			 "user '%s' password changed", user);
		return PAM_SUCCESS;
	}

	if (!error) {
		wbcFreeMemory(policy);
		return ret;
	}

	for (i=0; i<ARRAY_SIZE(codes); i++) {
		int _ret = ret;
		if (_pam_check_remark_auth_err(ctx, error, codes[i], &_ret)) {
			ret = _ret;
			goto done;
		}
	}

	if (!strcasecmp(error->nt_string,
			"NT_STATUS_PASSWORD_RESTRICTION")) {

		char *pwd_restriction_string = NULL;
		SMB_TIME_T min_pwd_age = 0;

		if (policy) {
			min_pwd_age	= policy->min_passwordage;
		}

		/* FIXME: avoid to send multiple PAM messages after another */
		switch (reject_reason) {
			case -1:
				break;
			case WBC_PWD_CHANGE_NO_ERROR:
				if ((min_pwd_age > 0) &&
				    (pwd_last_set + min_pwd_age > time(NULL))) {
					PAM_WB_REMARK_DIRECT(ctx,
					     "NT_STATUS_PWD_TOO_RECENT");
				}
				break;
			case WBC_PWD_CHANGE_PASSWORD_TOO_SHORT:
				PAM_WB_REMARK_DIRECT(ctx,
					"NT_STATUS_PWD_TOO_SHORT");
				break;
			case WBC_PWD_CHANGE_PWD_IN_HISTORY:
				PAM_WB_REMARK_DIRECT(ctx,
					"NT_STATUS_PWD_HISTORY_CONFLICT");
				break;
			case WBC_PWD_CHANGE_NOT_COMPLEX:
				_make_remark(ctx, PAM_ERROR_MSG,
					     _("Password does not meet "
					       "complexity requirements"));
				break;
			default:
				_pam_log_debug(ctx, LOG_DEBUG,
					       "unknown password change "
					       "reject reason: %d",
					       reject_reason);
				break;
		}

		pwd_restriction_string =
			_pam_compose_pwd_restriction_string(ctx, policy);
		if (pwd_restriction_string) {
			_make_remark(ctx, PAM_ERROR_MSG,
				     pwd_restriction_string);
			TALLOC_FREE(pwd_restriction_string);
		}
	}
 done:
	wbcFreeMemory(error);
	wbcFreeMemory(policy);

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
static int valid_user(struct pwb_context *ctx,
		      const char *user)
{
	/* check not only if the user is available over NSS calls, also make
	 * sure it's really a winbind user, this is important when stacking PAM
	 * modules in the 'account' or 'password' facility. */

	wbcErr wbc_status;
	struct passwd *pwd = NULL;
	struct passwd *wb_pwd = NULL;

	pwd = getpwnam(user);
	if (pwd == NULL) {
		return 1;
	}

	wbc_status = wbcGetpwnam(user, &wb_pwd);
	wbcFreeMemory(wb_pwd);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		_pam_log(ctx, LOG_DEBUG, "valid_user: wbcGetpwnam gave %s\n",
			wbcErrorString(wbc_status));
	}

	switch (wbc_status) {
		case WBC_ERR_UNKNOWN_USER:
		/* match other insane libwbclient return codes */
		case WBC_ERR_WINBIND_NOT_AVAILABLE:
		case WBC_ERR_DOMAIN_NOT_FOUND:
			return 1;
		case WBC_ERR_SUCCESS:
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

static int _winbind_read_password(struct pwb_context *ctx,
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

	_pam_log(ctx, LOG_DEBUG, "getting password (0x%08x)", ctrl);

	/*
	 * make sure nothing inappropriate gets returned
	 */

	*pass = token = NULL;

	/*
	 * which authentication token are we getting?
	 */

	if (on(WINBIND__OLD_PASSWORD, ctrl)) {
		authtok_flag = PAM_OLDAUTHTOK;
	} else {
		authtok_flag = PAM_AUTHTOK;
	}

	/*
	 * should we obtain the password from a PAM item ?
	 */

	if (on(WINBIND_TRY_FIRST_PASS_ARG, ctrl) ||
	    on(WINBIND_USE_FIRST_PASS_ARG, ctrl)) {
		retval = _pam_get_item(ctx->pamh, authtok_flag, &item);
		if (retval != PAM_SUCCESS) {
			/* very strange. */
			_pam_log(ctx, LOG_ALERT,
				 "pam_get_item returned error "
				 "to unix-read-password");
			return retval;
		} else if (item != NULL) {	/* we have a password! */
			*pass = item;
			item = NULL;
			_pam_log(ctx, LOG_DEBUG,
				 "pam_get_item returned a password");
			return PAM_SUCCESS;
		} else if (on(WINBIND_USE_FIRST_PASS_ARG, ctrl)) {
			return PAM_AUTHTOK_RECOVER_ERR;	/* didn't work */
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
			msg[0].msg = discard_const_p(char, comment);
			i = 1;
		} else {
			i = 0;
		}

		pmsg[i] = &msg[i];
		msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
		msg[i++].msg = discard_const_p(char, prompt1);
		replies = 1;

		if (prompt2 != NULL) {
			pmsg[i] = &msg[i];
			msg[i].msg_style = PAM_PROMPT_ECHO_OFF;
			msg[i++].msg = discard_const_p(char, prompt2);
			++replies;
		}
		/* so call the conversation expecting i responses */
		resp = NULL;
		retval = converse(ctx->pamh, i, pmsg, &resp);
		if (resp == NULL) {
			if (retval == PAM_SUCCESS) {
				retval = PAM_AUTHTOK_RECOVER_ERR;
			}
			goto done;
		}
		if (retval != PAM_SUCCESS) {
			_pam_drop_reply(resp, i);
			goto done;
		}

		/* interpret the response */

		token = x_strdup(resp[i - replies].resp);
		if (!token) {
			_pam_log(ctx, LOG_NOTICE,
				 "could not recover "
				 "authentication token");
			retval = PAM_AUTHTOK_RECOVER_ERR;
			goto done;
		}

		if (replies == 2) {
			/* verify that password entered correctly */
			if (!resp[i - 1].resp ||
			    strcmp(token, resp[i - 1].resp)) {
				_pam_delete(token);	/* mistyped */
				retval = PAM_AUTHTOK_RECOVER_ERR;
				_make_remark(ctx, PAM_ERROR_MSG,
					     MISTYPED_PASS);
			}
		}

		/*
		 * tidy up the conversation (resp_retcode) is ignored
		 * -- what is it for anyway? AGM
		 */
		_pam_drop_reply(resp, i);
	}

 done:
	if (retval != PAM_SUCCESS) {
		_pam_log_debug(ctx, LOG_DEBUG,
			       "unable to obtain a password");
		return retval;
	}
	/* 'token' is the entered password */

	/* we store this password as an item */

	retval = pam_set_item(ctx->pamh, authtok_flag, token);
	_pam_delete(token);	/* clean it up */
	if (retval != PAM_SUCCESS ||
	    (retval = _pam_get_item(ctx->pamh, authtok_flag, &item)) != PAM_SUCCESS) {

		_pam_log(ctx, LOG_CRIT, "error manipulating password");
		return retval;

	}

	*pass = item;
	item = NULL;		/* break link to password */

	return PAM_SUCCESS;
}

static const char *get_conf_item_string(struct pwb_context *ctx,
					const char *item,
					int config_flag)
{
	int i = 0;
	const char *parm_opt = NULL;

	if (!(ctx->ctrl & config_flag)) {
		goto out;
	}

	/* let the pam opt take precedence over the pam_winbind.conf option */
	for (i=0; i<ctx->argc; i++) {

		if ((strncmp(ctx->argv[i], item, strlen(item)) == 0)) {
			char *p;

			if ((p = strchr(ctx->argv[i], '=')) == NULL) {
				_pam_log(ctx, LOG_INFO,
					 "no \"=\" delimiter for \"%s\" found\n",
					 item);
				goto out;
			}
			_pam_log_debug(ctx, LOG_INFO,
				       "PAM config: %s '%s'\n", item, p+1);
			return p + 1;
		}
	}

	if (ctx->dict) {
		char *key = NULL;

		key = talloc_asprintf(ctx, "global:%s", item);
		if (!key) {
			goto out;
		}

		parm_opt = iniparser_getstring(ctx->dict, key, NULL);
		TALLOC_FREE(key);

		_pam_log_debug(ctx, LOG_INFO, "CONFIG file: %s '%s'\n",
			       item, parm_opt);
	}
out:
	return parm_opt;
}

static int get_config_item_int(struct pwb_context *ctx,
			       const char *item,
			       int config_flag)
{
	int i, parm_opt = -1;

	if (!(ctx->ctrl & config_flag)) {
		goto out;
	}

	/* let the pam opt take precedence over the pam_winbind.conf option */
	for (i = 0; i < ctx->argc; i++) {

		if ((strncmp(ctx->argv[i], item, strlen(item)) == 0)) {
			char *p;

			if ((p = strchr(ctx->argv[i], '=')) == NULL) {
				_pam_log(ctx, LOG_INFO,
					 "no \"=\" delimiter for \"%s\" found\n",
					 item);
				goto out;
			}
			parm_opt = atoi(p + 1);
			_pam_log_debug(ctx, LOG_INFO,
				       "PAM config: %s '%d'\n",
				       item, parm_opt);
			return parm_opt;
		}
	}

	if (ctx->dict) {
		char *key = NULL;

		key = talloc_asprintf(ctx, "global:%s", item);
		if (!key) {
			goto out;
		}

		parm_opt = iniparser_getint(ctx->dict, key, -1);
		TALLOC_FREE(key);

		_pam_log_debug(ctx, LOG_INFO,
			       "CONFIG file: %s '%d'\n",
			       item, parm_opt);
	}
out:
	return parm_opt;
}

static const char *get_krb5_cc_type_from_config(struct pwb_context *ctx)
{
	return get_conf_item_string(ctx, "krb5_ccache_type",
				    WINBIND_KRB5_CCACHE_TYPE);
}

static const char *get_member_from_config(struct pwb_context *ctx)
{
	const char *ret = NULL;
	ret = get_conf_item_string(ctx, "require_membership_of",
				   WINBIND_REQUIRED_MEMBERSHIP);
	if (ret) {
		return ret;
	}
	return get_conf_item_string(ctx, "require-membership-of",
				    WINBIND_REQUIRED_MEMBERSHIP);
}

static int get_warn_pwd_expire_from_config(struct pwb_context *ctx)
{
	int ret;
	ret = get_config_item_int(ctx, "warn_pwd_expire",
				  WINBIND_WARN_PWD_EXPIRE);
	/* no or broken setting */
	if (ret <= 0) {
		return DEFAULT_DAYS_TO_WARN_BEFORE_PWD_EXPIRES;
	}
	return ret;
}

/**
 * Retrieve the winbind separator.
 *
 * @param ctx PAM winbind context.
 *
 * @return string separator character. NULL on failure.
 */

static char winbind_get_separator(struct pwb_context *ctx)
{
	wbcErr wbc_status;
	static struct wbcInterfaceDetails *details = NULL;

	wbc_status = wbcInterfaceDetails(&details);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		_pam_log(ctx, LOG_ERR,
			 "Could not retrieve winbind interface details: %s",
			 wbcErrorString(wbc_status));
		return '\0';
	}

	if (!details) {
		return '\0';
	}

	return details->winbind_separator;
}


/**
 * Convert a upn to a name.
 *
 * @param ctx PAM winbind context.
 * @param upn  USer UPN to be trabslated.
 *
 * @return converted name. NULL pointer on failure. Caller needs to free.
 */

static char* winbind_upn_to_username(struct pwb_context *ctx,
				     const char *upn)
{
	char sep;
	wbcErr wbc_status = WBC_ERR_UNKNOWN_FAILURE;
	struct wbcDomainSid sid;
	enum wbcSidType type;
	char *domain = NULL;
	char *name;
	char *p;

	/* This cannot work when the winbind separator = @ */

	sep = winbind_get_separator(ctx);
	if (!sep || sep == '@') {
		return NULL;
	}

	name = talloc_strdup(ctx, upn);
	if (!name) {
		return NULL;
	}
	if ((p = strchr(name, '@')) != NULL) {
		*p = 0;
		domain = p + 1;
	}

	/* Convert the UPN to a SID */

	wbc_status = wbcLookupName(domain, name, &sid, &type);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		return NULL;
	}

	/* Convert the the SID back to the sAMAccountName */

	wbc_status = wbcLookupSid(&sid, &domain, &name, &type);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		return NULL;
	}

	return talloc_asprintf(ctx, "%s%c%s", domain, sep, name);
}

static int _pam_delete_cred(pam_handle_t *pamh, int flags,
			 int argc, const char **argv)
{
	int retval = PAM_SUCCESS;
	struct pwb_context *ctx = NULL;
	struct wbcLogoffUserParams logoff;
	struct wbcAuthErrorInfo *error = NULL;
	const char *user;
	wbcErr wbc_status = WBC_ERR_SUCCESS;

	ZERO_STRUCT(logoff);

	retval = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (retval) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("_pam_delete_cred", ctx);

	if (ctx->ctrl & WINBIND_KRB5_AUTH) {

		/* destroy the ccache here */

		uint32_t wbc_flags = 0;
		const char *ccname = NULL;
		struct passwd *pwd = NULL;

		retval = pam_get_user(pamh, &user, _("Username: "));
		if (retval) {
			_pam_log(ctx, LOG_ERR,
				 "could not identify user");
			goto out;
		}

		if (user == NULL) {
			_pam_log(ctx, LOG_ERR,
				 "username was NULL!");
			retval = PAM_USER_UNKNOWN;
			goto out;
		}

		_pam_log_debug(ctx, LOG_DEBUG,
			       "username [%s] obtained", user);

		ccname = pam_getenv(pamh, "KRB5CCNAME");
		if (ccname == NULL) {
			_pam_log_debug(ctx, LOG_DEBUG,
				       "user has no KRB5CCNAME environment");
		}

		pwd = getpwnam(user);
		if (pwd == NULL) {
			retval = PAM_USER_UNKNOWN;
			goto out;
		}

		wbc_flags = WBFLAG_PAM_KRB5 |
			WBFLAG_PAM_CONTACT_TRUSTDOM;

		logoff.username		= user;

		if (ccname) {
			wbc_status = wbcAddNamedBlob(&logoff.num_blobs,
						     &logoff.blobs,
						     "ccfilename",
						     0,
						     (uint8_t *)ccname,
						     strlen(ccname)+1);
			if (!WBC_ERROR_IS_OK(wbc_status)) {
				goto out;
			}
		}

		wbc_status = wbcAddNamedBlob(&logoff.num_blobs,
					     &logoff.blobs,
					     "flags",
					     0,
					     (uint8_t *)&wbc_flags,
					     sizeof(wbc_flags));
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			goto out;
		}

		wbc_status = wbcAddNamedBlob(&logoff.num_blobs,
					     &logoff.blobs,
					     "user_uid",
					     0,
					     (uint8_t *)&pwd->pw_uid,
					     sizeof(pwd->pw_uid));
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			goto out;
		}

		wbc_status = wbcLogoffUserEx(&logoff, &error);
		retval = wbc_auth_error_to_pam_error(ctx, error, wbc_status,
						     user, "wbcLogoffUser");
		wbcFreeMemory(error);
		wbcFreeMemory(logoff.blobs);
		logoff.blobs = NULL;

		if (!WBC_ERROR_IS_OK(wbc_status)) {
			_pam_log(ctx, LOG_INFO,
				 "failed to logoff user %s: %s\n",
					 user, wbcErrorString(wbc_status));
		}
	}

out:
	if (logoff.blobs) {
		wbcFreeMemory(logoff.blobs);
	}

	if (!WBC_ERROR_IS_OK(wbc_status)) {
		retval = wbc_auth_error_to_pam_error(ctx, error, wbc_status,
		     user, "wbcLogoffUser");
	}

	/*
	 * Delete the krb5 ccname variable from the PAM environment
	 * if it was set by winbind.
	 */
	if ((ctx->ctrl & WINBIND_KRB5_AUTH) && pam_getenv(pamh, "KRB5CCNAME")) {
		pam_putenv(pamh, "KRB5CCNAME");
	}

	_PAM_LOG_FUNCTION_LEAVE("_pam_delete_cred", ctx, retval);

	TALLOC_FREE(ctx);

	return retval;
}

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
	const char *username;
	const char *password;
	const char *member = NULL;
	const char *cctype = NULL;
	int warn_pwd_expire;
	int retval = PAM_AUTH_ERR;
	char *username_ret = NULL;
	char *new_authtok_required = NULL;
	char *real_username = NULL;
	struct pwb_context *ctx = NULL;

	retval = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (retval) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_authenticate", ctx);

	/* Get the username */
	retval = pam_get_user(pamh, &username, NULL);
	if ((retval != PAM_SUCCESS) || (!username)) {
		_pam_log_debug(ctx, LOG_DEBUG,
			       "can not get the username");
		retval = PAM_SERVICE_ERR;
		goto out;
	}


#if defined(AIX)
	/* Decode the user name since AIX does not support logn user
	   names by default.  The name is encoded as _#uid.  */

	if (username[0] == '_') {
		uid_t id = atoi(&username[1]);
		struct passwd *pw = NULL;

		if ((id!=0) && ((pw = getpwuid(id)) != NULL)) {
			real_username = strdup(pw->pw_name);
		}
	}
#endif

	if (!real_username) {
		/* Just making a copy of the username we got from PAM */
		if ((real_username = strdup(username)) == NULL) {
			_pam_log_debug(ctx, LOG_DEBUG,
				       "memory allocation failure when copying "
				       "username");
			retval = PAM_SERVICE_ERR;
			goto out;
		}
	}

	/* Maybe this was a UPN */

	if (strchr(real_username, '@') != NULL) {
		char *samaccountname = NULL;

		samaccountname = winbind_upn_to_username(ctx,
							 real_username);
		if (samaccountname) {
			free(real_username);
			real_username = strdup(samaccountname);
		}
	}

	retval = _winbind_read_password(ctx, ctx->ctrl, NULL,
					_("Password: "), NULL,
					&password);

	if (retval != PAM_SUCCESS) {
		_pam_log(ctx, LOG_ERR,
			 "Could not retrieve user's password");
		retval = PAM_AUTHTOK_ERR;
		goto out;
	}

	/* Let's not give too much away in the log file */

#ifdef DEBUG_PASSWORD
	_pam_log_debug(ctx, LOG_INFO,
		       "Verify user '%s' with password '%s'",
		       real_username, password);
#else
	_pam_log_debug(ctx, LOG_INFO,
		       "Verify user '%s'", real_username);
#endif

	member = get_member_from_config(ctx);
	cctype = get_krb5_cc_type_from_config(ctx);
	warn_pwd_expire = get_warn_pwd_expire_from_config(ctx);

	/* Now use the username to look up password */
	retval = winbind_auth_request(ctx, real_username, password,
				      member, cctype, warn_pwd_expire,
				      NULL, NULL, NULL,
				      NULL, &username_ret);

	if (retval == PAM_NEW_AUTHTOK_REQD ||
	    retval == PAM_AUTHTOK_EXPIRED) {

		char *new_authtok_required_during_auth = NULL;

		new_authtok_required = talloc_asprintf(NULL, "%d", retval);
		if (!new_authtok_required) {
			retval = PAM_BUF_ERR;
			goto out;
		}

		pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD,
			     new_authtok_required,
			     _pam_winbind_cleanup_func);

		retval = PAM_SUCCESS;

		new_authtok_required_during_auth = talloc_asprintf(NULL, "%d", true);
		if (!new_authtok_required_during_auth) {
			retval = PAM_BUF_ERR;
			goto out;
		}

		pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH,
			     new_authtok_required_during_auth,
			     _pam_winbind_cleanup_func);

		goto out;
	}

out:
	if (username_ret) {
		pam_set_item (pamh, PAM_USER, username_ret);
		_pam_log_debug(ctx, LOG_INFO,
			       "Returned user was '%s'", username_ret);
		free(username_ret);
	}

	if (real_username) {
		free(real_username);
	}

	if (!new_authtok_required) {
		pam_set_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD, NULL, NULL);
	}

	if (retval != PAM_SUCCESS) {
		_pam_free_data_info3(pamh);
	}

	if (ctx != NULL) {
		_PAM_LOG_FUNCTION_LEAVE("pam_sm_authenticate", ctx, retval);
		TALLOC_FREE(ctx);
	}

	return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh, int flags,
		   int argc, const char **argv)
{
	int ret = PAM_SYSTEM_ERR;
	struct pwb_context *ctx = NULL;

	ret = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (ret) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_setcred", ctx);

	switch (flags & ~PAM_SILENT) {

		case PAM_DELETE_CRED:
			ret = _pam_delete_cred(pamh, flags, argc, argv);
			break;
		case PAM_REFRESH_CRED:
			_pam_log_debug(ctx, LOG_WARNING,
				       "PAM_REFRESH_CRED not implemented");
			ret = PAM_SUCCESS;
			break;
		case PAM_REINITIALIZE_CRED:
			_pam_log_debug(ctx, LOG_WARNING,
				       "PAM_REINITIALIZE_CRED not implemented");
			ret = PAM_SUCCESS;
			break;
		case PAM_ESTABLISH_CRED:
			_pam_log_debug(ctx, LOG_WARNING,
				       "PAM_ESTABLISH_CRED not implemented");
			ret = PAM_SUCCESS;
			break;
		default:
			ret = PAM_SYSTEM_ERR;
			break;
	}

 out:

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_setcred", ctx, ret);

	TALLOC_FREE(ctx);

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
	struct pwb_context *ctx = NULL;

	ret = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (ret) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_acct_mgmt", ctx);


	/* Get the username */
	ret = pam_get_user(pamh, &username, NULL);
	if ((ret != PAM_SUCCESS) || (!username)) {
		_pam_log_debug(ctx, LOG_DEBUG,
			       "can not get the username");
		ret = PAM_SERVICE_ERR;
		goto out;
	}

	/* Verify the username */
	ret = valid_user(ctx, username);
	switch (ret) {
	case -1:
		/* some sort of system error. The log was already printed */
		ret = PAM_SERVICE_ERR;
		goto out;
	case 1:
		/* the user does not exist */
		_pam_log_debug(ctx, LOG_NOTICE, "user '%s' not found",
			       username);
		if (ctx->ctrl & WINBIND_UNKNOWN_OK_ARG) {
			ret = PAM_IGNORE;
			goto out;
		}
		ret = PAM_USER_UNKNOWN;
		goto out;
	case 0:
		pam_get_data(pamh, PAM_WINBIND_NEW_AUTHTOK_REQD,
			     (const void **)&tmp);
		if (tmp != NULL) {
			ret = atoi((const char *)tmp);
			switch (ret) {
			case PAM_AUTHTOK_EXPIRED:
				/* fall through, since new token is required in this case */
			case PAM_NEW_AUTHTOK_REQD:
				_pam_log(ctx, LOG_WARNING,
					 "pam_sm_acct_mgmt success but %s is set",
					 PAM_WINBIND_NEW_AUTHTOK_REQD);
				_pam_log(ctx, LOG_NOTICE,
					 "user '%s' needs new password",
					 username);
				/* PAM_AUTHTOKEN_REQD does not exist, but is documented in the manpage */
				ret = PAM_NEW_AUTHTOK_REQD;
				goto out;
			default:
				_pam_log(ctx, LOG_WARNING,
					 "pam_sm_acct_mgmt success");
				_pam_log(ctx, LOG_NOTICE,
					 "user '%s' granted access", username);
				ret = PAM_SUCCESS;
				goto out;
			}
		}

		/* Otherwise, the authentication looked good */
		_pam_log(ctx, LOG_NOTICE,
			 "user '%s' granted access", username);
		ret = PAM_SUCCESS;
		goto out;
	default:
		/* we don't know anything about this return value */
		_pam_log(ctx, LOG_ERR,
			 "internal module error (ret = %d, user = '%s')",
			 ret, username);
		ret = PAM_SERVICE_ERR;
		goto out;
	}

	/* should not be reached */
	ret = PAM_IGNORE;

 out:

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_acct_mgmt", ctx, ret);

	TALLOC_FREE(ctx);

	return ret;
}

PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
	int ret = PAM_SUCCESS;
	struct pwb_context *ctx = NULL;

	ret = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (ret) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_open_session", ctx);

	if (ctx->ctrl & WINBIND_MKHOMEDIR) {
		/* check and create homedir */
		ret = _pam_mkhomedir(ctx);
	}
 out:
	_PAM_LOG_FUNCTION_LEAVE("pam_sm_open_session", ctx, ret);

	TALLOC_FREE(ctx);

	return ret;
}

PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh, int flags,
			 int argc, const char **argv)
{
	int ret = PAM_SUCCESS;
	struct pwb_context *ctx = NULL;

	ret = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (ret) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_close_session", ctx);

out:
	_PAM_LOG_FUNCTION_LEAVE("pam_sm_close_session", ctx, ret);

	TALLOC_FREE(ctx);

	return ret;
}

/**
 * evaluate whether we need to re-authenticate with kerberos after a
 * password change
 *
 * @param ctx PAM winbind context.
 * @param user The username
 *
 * @return boolean Returns true if required, false if not.
 */

static bool _pam_require_krb5_auth_after_chauthtok(struct pwb_context *ctx,
						   const char *user)
{

	/* Make sure that we only do this if a) the chauthtok got initiated
	 * during a logon attempt (authenticate->acct_mgmt->chauthtok) b) any
	 * later password change via the "passwd" command if done by the user
	 * itself
	 * NB. If we login from gdm or xdm and the password expires,
	 * we change the password, but there is no memory cache.
	 * Thus, even for passthrough login, we should do the
	 * authentication again to update memory cache.
	 * --- BoYang
	 * */

	char *new_authtok_reqd_during_auth = NULL;
	struct passwd *pwd = NULL;

	_pam_get_data(ctx->pamh, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH,
		      &new_authtok_reqd_during_auth);
	pam_set_data(ctx->pamh, PAM_WINBIND_NEW_AUTHTOK_REQD_DURING_AUTH,
		     NULL, NULL);

	if (new_authtok_reqd_during_auth) {
		return true;
	}

	pwd = getpwnam(user);
	if (!pwd) {
		return false;
	}

	if (getuid() == pwd->pw_uid) {
		return true;
	}

	return false;
}


PAM_EXTERN
int pam_sm_chauthtok(pam_handle_t * pamh, int flags,
		     int argc, const char **argv)
{
	unsigned int lctrl;
	int ret;
	bool cached_login = false;

	/* <DO NOT free() THESE> */
	const char *user;
	char *pass_old, *pass_new;
	/* </DO NOT free() THESE> */

	char *Announce;

	int retry = 0;
	char *username_ret = NULL;
	struct wbcAuthErrorInfo *error = NULL;
	struct pwb_context *ctx = NULL;

	ret = _pam_winbind_init_context(pamh, flags, argc, argv, &ctx);
	if (ret) {
		goto out;
	}

	_PAM_LOG_FUNCTION_ENTER("pam_sm_chauthtok", ctx);

	cached_login = (ctx->ctrl & WINBIND_CACHED_LOGIN);

	/* clearing offline bit for auth */
	ctx->ctrl &= ~WINBIND_CACHED_LOGIN;

	/*
	 * First get the name of a user
	 */
	ret = pam_get_user(pamh, &user, _("Username: "));
	if (ret) {
		_pam_log(ctx, LOG_ERR,
			 "password - could not identify user");
		goto out;
	}

	if (user == NULL) {
		_pam_log(ctx, LOG_ERR, "username was NULL!");
		ret = PAM_USER_UNKNOWN;
		goto out;
	}

	_pam_log_debug(ctx, LOG_DEBUG, "username [%s] obtained", user);

	/* check if this is really a user in winbindd, not only in NSS */
	ret = valid_user(ctx, user);
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

#define greeting _("Changing password for")
		Announce = talloc_asprintf(ctx, "%s %s", greeting, user);
		if (!Announce) {
			_pam_log(ctx, LOG_CRIT,
				 "password - out of memory");
			ret = PAM_BUF_ERR;
			goto out;
		}
#undef greeting

		lctrl = ctx->ctrl | WINBIND__OLD_PASSWORD;
		ret = _winbind_read_password(ctx, lctrl,
						Announce,
						_("(current) NT password: "),
						NULL,
						(const char **) &pass_old);
		TALLOC_FREE(Announce);
		if (ret != PAM_SUCCESS) {
			_pam_log(ctx, LOG_NOTICE,
				 "password - (old) token not obtained");
			goto out;
		}

		/* verify that this is the password for this user */

		ret = winbind_auth_request(ctx, user, pass_old,
					   NULL, NULL, 0,
					   &error, NULL, NULL,
					   &pwdlastset_prelim, NULL);

		if (ret != PAM_ACCT_EXPIRED &&
		    ret != PAM_AUTHTOK_EXPIRED &&
		    ret != PAM_NEW_AUTHTOK_REQD &&
		    ret != PAM_SUCCESS) {
			pass_old = NULL;
			goto out;
		}

		pam_set_data(pamh, PAM_WINBIND_PWD_LAST_SET,
			     (void *)pwdlastset_prelim, NULL);

		ret = pam_set_item(pamh, PAM_OLDAUTHTOK,
				   (const void *) pass_old);
		pass_old = NULL;
		if (ret != PAM_SUCCESS) {
			_pam_log(ctx, LOG_CRIT,
				 "failed to set PAM_OLDAUTHTOK");
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
			_pam_log(ctx, LOG_NOTICE,
				 "user not authenticated");
			goto out;
		}

		lctrl = ctx->ctrl & ~WINBIND_TRY_FIRST_PASS_ARG;

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

			ret = _winbind_read_password(ctx, lctrl,
						     NULL,
						     _("Enter new NT password: "),
						     _("Retype new NT password: "),
						     (const char **)&pass_new);

			if (ret != PAM_SUCCESS) {
				_pam_log_debug(ctx, LOG_ALERT,
					       "password - "
					       "new password not obtained");
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
		_pam_get_data(pamh, PAM_WINBIND_PWD_LAST_SET,
			      &pwdlastset_update);

		/*
		 * if cached creds were enabled, make sure to set the
		 * WINBIND_CACHED_LOGIN bit here in order to have winbindd
		 * update the cached creds storage - gd
		 */
		if (cached_login) {
			ctx->ctrl |= WINBIND_CACHED_LOGIN;
		}

		ret = winbind_chauthtok_request(ctx, user, pass_old,
						pass_new, pwdlastset_update);
		if (ret) {
			pass_old = pass_new = NULL;
			goto out;
		}

		if (_pam_require_krb5_auth_after_chauthtok(ctx, user)) {

			const char *member = NULL;
			const char *cctype = NULL;
			int warn_pwd_expire;
			struct wbcLogonUserInfo *info = NULL;
			struct wbcUserPasswordPolicyInfo *policy = NULL;

			member = get_member_from_config(ctx);
			cctype = get_krb5_cc_type_from_config(ctx);
			warn_pwd_expire = get_warn_pwd_expire_from_config(ctx);

			/* Keep WINBIND_CACHED_LOGIN bit for
			 * authentication after changing the password.
			 * This will update the cached credentials in case
			 * that winbindd_dual_pam_chauthtok() fails
			 * to update them.
			 * --- BoYang
			 * */

			ret = winbind_auth_request(ctx, user, pass_new,
						   member, cctype, 0,
						   &error, &info, &policy,
						   NULL, &username_ret);
			pass_old = pass_new = NULL;

			if (ret == PAM_SUCCESS) {

				struct wbcAuthUserInfo *user_info = NULL;

				if (info && info->info) {
					user_info = info->info;
				}

				/* warn a user if the password is about to
				 * expire soon */
				_pam_warn_password_expiry(ctx, user_info, policy,
							  warn_pwd_expire,
							  NULL, NULL);

				/* set some info3 info for other modules in the
				 * stack */
				_pam_set_data_info3(ctx, user_info);

				/* put krb5ccname into env */
				_pam_setup_krb5_env(ctx, info);

				if (username_ret) {
					pam_set_item(pamh, PAM_USER,
						     username_ret);
					_pam_log_debug(ctx, LOG_INFO,
						       "Returned user was '%s'",
						       username_ret);
					free(username_ret);
				}

			}

			if (info && info->blobs) {
				wbcFreeMemory(info->blobs);
			}
			wbcFreeMemory(info);
			wbcFreeMemory(policy);

			goto out;
		}
	} else {
		ret = PAM_SERVICE_ERR;
	}

out:
	{
		/* Deal with offline errors. */
		int i;
		const char *codes[] = {
			"NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND",
			"NT_STATUS_NO_LOGON_SERVERS",
			"NT_STATUS_ACCESS_DENIED"
		};

		for (i=0; i<ARRAY_SIZE(codes); i++) {
			int _ret;
			if (_pam_check_remark_auth_err(ctx, error, codes[i], &_ret)) {
				break;
			}
		}
	}

	wbcFreeMemory(error);

	_PAM_LOG_FUNCTION_LEAVE("pam_sm_chauthtok", ctx, ret);

	TALLOC_FREE(ctx);

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
 * Copyright (c) Guenther Deschner <gd@samba.org>      2005-2008
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
