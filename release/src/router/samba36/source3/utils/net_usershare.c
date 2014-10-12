/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility

   Copyright (C) Jeremy Allison (jra@samba.org) 2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/passwd.h"
#include "system/filesys.h"
#include "utils/net.h"
#include "../libcli/security/security.h"

struct {
	const char *us_errstr;
	enum usershare_err us_err;
} us_errs [] = {
	{"",USERSHARE_OK},
	{N_("Malformed usershare file"), USERSHARE_MALFORMED_FILE},
	{N_("Bad version number"), USERSHARE_BAD_VERSION},
	{N_("Malformed path entry"), USERSHARE_MALFORMED_PATH},
	{N_("Malformed comment entryfile"), USERSHARE_MALFORMED_COMMENT_DEF},
	{N_("Malformed acl definition"), USERSHARE_MALFORMED_ACL_DEF},
	{N_("Acl parse error"), USERSHARE_ACL_ERR},
	{N_("Path not absolute"), USERSHARE_PATH_NOT_ABSOLUTE},
	{N_("Path is denied"), USERSHARE_PATH_IS_DENIED},
	{N_("Path not allowed"), USERSHARE_PATH_NOT_ALLOWED},
	{N_("Path is not a directory"), USERSHARE_PATH_NOT_DIRECTORY},
	{N_("System error"), USERSHARE_POSIX_ERR},
	{N_("Malformed sharename definition"), USERSHARE_MALFORMED_SHARENAME_DEF},
	{N_("Bad sharename (doesn't match filename)"), USERSHARE_BAD_SHARENAME},
	{NULL,(enum usershare_err)-1}
};

static const char *get_us_error_code(enum usershare_err us_err)
{
	char *result;
	int idx = 0;

	while (us_errs[idx].us_errstr != NULL) {
		if (us_errs[idx].us_err == us_err) {
			return us_errs[idx].us_errstr;
		}
		idx++;
	}

	result = talloc_asprintf(talloc_tos(), _("Usershare error code (0x%x)"),
				 (unsigned int)us_err);
	SMB_ASSERT(result != NULL);
	return result;
}

/* The help subsystem for the USERSHARE subcommand */

static int net_usershare_add_usage(struct net_context *c, int argc, const char **argv)
{
	char chr = *lp_winbind_separator();
	d_printf(_(
		"net usershare add [-l|--long] <sharename> <path> [<comment>] [<acl>] [<guest_ok=[y|n]>]\n"
		"\tAdds the specified share name for this user.\n"
		"\t<sharename> is the new share name.\n"
		"\t<path> is the path on the filesystem to export.\n"
		"\t<comment> is the optional comment for the new share.\n"
		"\t<acl> is an optional share acl in the format \"DOMAIN%cname:X,DOMAIN%cname:X,....\"\n"
		"\t<guest_ok=y> if present sets \"guest ok = yes\" on this usershare.\n"
		"\t\t\"X\" represents a permission and can be any one of the characters f, r or d\n"
		"\t\twhere \"f\" means full control, \"r\" means read-only, \"d\" means deny access.\n"
		"\t\tname may be a domain user or group. For local users use the local server name "
		"instead of \"DOMAIN\"\n"
		"\t\tThe default acl is \"Everyone:r\" which allows everyone read-only access.\n"
		"\tAdd -l or --long to print the info on the newly added share.\n"),
		chr, chr );
	return -1;
}

static int net_usershare_delete_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
		"net usershare delete <sharename>\n"
		"\tdeletes the specified share name for this user.\n"));
	return -1;
}

static int net_usershare_info_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
		"net usershare info [-l|--long] [wildcard sharename]\n"
		"\tPrints out the path, comment and acl elements of shares that match the wildcard.\n"
		"\tBy default only gives info on shares owned by the current user\n"
		"\tAdd -l or --long to apply this to all shares\n"
		"\tOmit the sharename or use a wildcard of '*' to see all shares\n"));
	return -1;
}

static int net_usershare_list_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
		"net usershare list [-l|--long] [wildcard sharename]\n"
		"\tLists the names of all shares that match the wildcard.\n"
		"\tBy default only lists shares owned by the current user\n"
		"\tAdd -l or --long to apply this to all shares\n"
		"\tOmit the sharename or use a wildcard of '*' to see all shares\n"));
	return -1;
}

int net_usershare_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net usershare add <sharename> <path> [<comment>] [<acl>] [<guest_ok=[y|n]>] to "
				"add or change a user defined share.\n"
		"net usershare delete <sharename> to delete a user defined share.\n"
		"net usershare info [-l|--long] [wildcard sharename] to print info about a user defined share.\n"
		"net usershare list [-l|--long] [wildcard sharename] to list user defined shares.\n"
		"net usershare help\n"
		"\nType \"net usershare help <option>\" to get more information on that option\n\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

/***************************************************************************
***************************************************************************/

static char *get_basepath(TALLOC_CTX *ctx)
{
	char *basepath = talloc_strdup(ctx, lp_usershare_path());

	if (!basepath) {
		return NULL;
	}
	if ((basepath[0] != '\0') && (basepath[strlen(basepath)-1] == '/')) {
		basepath[strlen(basepath)-1] = '\0';
	}
	return basepath;
}

/***************************************************************************
 Delete a single userlevel share.
***************************************************************************/

static int net_usershare_delete(struct net_context *c, int argc, const char **argv)
{
	char *us_path;
	char *sharename;

	if (argc != 1 || c->display_usage) {
		return net_usershare_delete_usage(c, argc, argv);
	}

	if ((sharename = strlower_talloc(talloc_tos(), argv[0])) == NULL) {
		d_fprintf(stderr, _("strlower_talloc failed\n"));
		return -1;
	}

	if (!validate_net_name(sharename, INVALID_SHARENAME_CHARS, strlen(sharename))) {
		d_fprintf(stderr, _("net usershare delete: share name %s contains "
                        "invalid characters (any of %s)\n"),
                        sharename, INVALID_SHARENAME_CHARS);
		TALLOC_FREE(sharename);
		return -1;
	}

	us_path = talloc_asprintf(talloc_tos(),
				"%s/%s",
				lp_usershare_path(),
				sharename);
	if (!us_path) {
		TALLOC_FREE(sharename);
		return -1;
	}

	if (unlink(us_path) != 0) {
		d_fprintf(stderr, _("net usershare delete: unable to remove usershare %s. "
			"Error was %s\n"),
                        us_path, strerror(errno));
		TALLOC_FREE(sharename);
		return -1;
	}
	TALLOC_FREE(sharename);
	return 0;
}

/***************************************************************************
 Data structures to handle a list of usershare files.
***************************************************************************/

struct file_list {
	struct file_list *next, *prev;
	const char *pathname;
};

static struct file_list *flist;

/***************************************************************************
***************************************************************************/

static int get_share_list(TALLOC_CTX *ctx, const char *wcard, bool only_ours)
{
	SMB_STRUCT_DIR *dp;
	SMB_STRUCT_DIRENT *de;
	uid_t myuid = geteuid();
	struct file_list *fl = NULL;
	char *basepath = get_basepath(ctx);

	if (!basepath) {
		return -1;
	}
	dp = sys_opendir(basepath);
	if (!dp) {
		d_fprintf(stderr,
			_("get_share_list: cannot open usershare directory %s. "
			  "Error %s\n"),
			basepath, strerror(errno) );
		return -1;
	}

	while((de = sys_readdir(dp)) != 0) {
		SMB_STRUCT_STAT sbuf;
		char *path;
		const char *n = de->d_name;

		/* Ignore . and .. */
		if (*n == '.') {
			if ((n[1] == '\0') || (n[1] == '.' && n[2] == '\0')) {
				continue;
			}
		}

		if (!validate_net_name(n, INVALID_SHARENAME_CHARS, strlen(n))) {
			d_fprintf(stderr,
				  _("get_share_list: ignoring bad share "
				    "name %s\n"), n);
			continue;
		}
		path = talloc_asprintf(ctx,
					"%s/%s",
					basepath,
					n);
		if (!path) {
			sys_closedir(dp);
			return -1;
		}

		if (sys_lstat(path, &sbuf, false) != 0) {
			d_fprintf(stderr,
				_("get_share_list: can't lstat file %s. Error "
				  "was %s\n"),
				path, strerror(errno) );
			continue;
		}

		if (!S_ISREG(sbuf.st_ex_mode)) {
			d_fprintf(stderr,
				  _("get_share_list: file %s is not a regular "
				    "file. Ignoring.\n"),
				path );
			continue;
		}

		if (only_ours && sbuf.st_ex_uid != myuid) {
			continue;
		}

		if (!unix_wild_match(wcard, n)) {
			continue;
		}

		/* (Finally) - add to list. */
		fl = TALLOC_P(ctx, struct file_list);
		if (!fl) {
			sys_closedir(dp);
			return -1;
		}
		fl->pathname = talloc_strdup(ctx, n);
		if (!fl->pathname) {
			sys_closedir(dp);
			return -1;
		}

		DLIST_ADD(flist, fl);
	}

	sys_closedir(dp);
	return 0;
}

enum us_priv_op { US_LIST_OP, US_INFO_OP};

struct us_priv_info {
	TALLOC_CTX *ctx;
	enum us_priv_op op;
	struct net_context *c;
};

/***************************************************************************
 Call a function for every share on the list.
***************************************************************************/

static int process_share_list(int (*fn)(struct file_list *, void *), void *priv)
{
	struct file_list *fl;
	int ret = 0;

	for (fl = flist; fl; fl = fl->next) {
		ret = (*fn)(fl, priv);
	}

	return ret;
}

/***************************************************************************
 Info function.
***************************************************************************/

static int info_fn(struct file_list *fl, void *priv)
{
	SMB_STRUCT_STAT sbuf;
	char **lines = NULL;
	struct us_priv_info *pi = (struct us_priv_info *)priv;
	TALLOC_CTX *ctx = pi->ctx;
	struct net_context *c = pi->c;
	int fd = -1;
	int numlines = 0;
	struct security_descriptor *psd = NULL;
	char *basepath;
	char *sharepath = NULL;
	char *comment = NULL;
	char *cp_sharename = NULL;
	char *acl_str;
	int num_aces;
	char sep_str[2];
	enum usershare_err us_err;
	bool guest_ok = false;

	sep_str[0] = *lp_winbind_separator();
	sep_str[1] = '\0';

	basepath = get_basepath(ctx);
	if (!basepath) {
		return -1;
	}
	basepath = talloc_asprintf_append(basepath,
			"/%s",
			fl->pathname);
	if (!basepath) {
		return -1;
	}

#ifdef O_NOFOLLOW
	fd = sys_open(basepath, O_RDONLY|O_NOFOLLOW, 0);
#else
	fd = sys_open(basepath, O_RDONLY, 0);
#endif

	if (fd == -1) {
		d_fprintf(stderr, _("info_fn: unable to open %s. %s\n"),
                        basepath, strerror(errno) );
                return -1;
        }

	/* Paranoia... */
	if (sys_fstat(fd, &sbuf, false) != 0) {
		d_fprintf(stderr,
			_("info_fn: can't fstat file %s. Error was %s\n"),
			basepath, strerror(errno) );
		close(fd);
		return -1;
	}

	if (!S_ISREG(sbuf.st_ex_mode)) {
		d_fprintf(stderr,
			_("info_fn: file %s is not a regular file. Ignoring.\n"),
			basepath );
		close(fd);
		return -1;
	}

	lines = fd_lines_load(fd, &numlines, 10240, NULL);
	close(fd);

	if (lines == NULL) {
		return -1;
	}

	/* Ensure it's well formed. */
	us_err = parse_usershare_file(ctx, &sbuf, fl->pathname, -1, lines, numlines,
				&sharepath,
				&comment,
				&cp_sharename,
				&psd,
				&guest_ok);

	TALLOC_FREE(lines);

	if (us_err != USERSHARE_OK) {
		d_fprintf(stderr,
			_("info_fn: file %s is not a well formed usershare "
			  "file.\n"),
			basepath );
		d_fprintf(stderr, _("info_fn: Error was %s.\n"),
			get_us_error_code(us_err) );
		return -1;
	}

	acl_str = talloc_strdup(ctx, "usershare_acl=");
	if (!acl_str) {
		return -1;
	}

	for (num_aces = 0; num_aces < psd->dacl->num_aces; num_aces++) {
		const char *domain;
		const char *name;
		NTSTATUS ntstatus;

		ntstatus = net_lookup_name_from_sid(c, ctx,
				                    &psd->dacl->aces[num_aces].trustee,
						    &domain, &name);

		if (NT_STATUS_IS_OK(ntstatus)) {
			if (domain && *domain) {
				acl_str = talloc_asprintf_append(acl_str,
						"%s%s",
						domain,
						sep_str);
				if (!acl_str) {
					return -1;
				}
			}
			acl_str = talloc_asprintf_append(acl_str,
						"%s",
						name);
			if (!acl_str) {
				return -1;
			}

		} else {
			fstring sidstr;
			sid_to_fstring(sidstr,
				       &psd->dacl->aces[num_aces].trustee);
			acl_str = talloc_asprintf_append(acl_str,
						"%s",
						sidstr);
			if (!acl_str) {
				return -1;
			}
		}
		acl_str = talloc_asprintf_append(acl_str, ":");
		if (!acl_str) {
			return -1;
		}

		if (psd->dacl->aces[num_aces].type == SEC_ACE_TYPE_ACCESS_DENIED) {
			acl_str = talloc_asprintf_append(acl_str, "D,");
			if (!acl_str) {
				return -1;
			}
		} else {
			if (psd->dacl->aces[num_aces].access_mask & GENERIC_ALL_ACCESS) {
				acl_str = talloc_asprintf_append(acl_str, "F,");
			} else {
				acl_str = talloc_asprintf_append(acl_str, "R,");
			}
			if (!acl_str) {
				return -1;
			}
		}
	}

	/* NOTE: This is smb.conf-like output. Do not translate. */
	if (pi->op == US_INFO_OP) {
		d_printf("[%s]\n", cp_sharename );
		d_printf("path=%s\n", sharepath );
		d_printf("comment=%s\n", comment);
		d_printf("%s\n", acl_str);
		d_printf("guest_ok=%c\n\n", guest_ok ? 'y' : 'n');
	} else if (pi->op == US_LIST_OP) {
		d_printf("%s\n", cp_sharename);
	}

	return 0;
}

/***************************************************************************
 Print out info (internal detail) on userlevel shares.
***************************************************************************/

static int net_usershare_info(struct net_context *c, int argc, const char **argv)
{
	fstring wcard;
	bool only_ours = true;
	int ret = -1;
	struct us_priv_info pi;
	TALLOC_CTX *ctx;

	fstrcpy(wcard, "*");

	if (c->display_usage)
		return net_usershare_info_usage(c, argc, argv);

	if (c->opt_long_list_entries) {
		only_ours = false;
	}

	switch (argc) {
		case 0:
			break;
		case 1:
			fstrcpy(wcard, argv[0]);
			break;
		default:
			return net_usershare_info_usage(c, argc, argv);
	}

	strlower_m(wcard);

	ctx = talloc_init("share_info");
	ret = get_share_list(ctx, wcard, only_ours);
	if (ret) {
		return ret;
	}

	pi.ctx = ctx;
	pi.op = US_INFO_OP;
	pi.c = c;

	ret = process_share_list(info_fn, &pi);
	talloc_destroy(ctx);
	return ret;
}

/***************************************************************************
 Count the current total number of usershares.
***************************************************************************/

static int count_num_usershares(void)
{
	SMB_STRUCT_DIR *dp;
	SMB_STRUCT_DIRENT *de;
	int num_usershares = 0;
	TALLOC_CTX *ctx = talloc_tos();
	char *basepath = get_basepath(ctx);

	if (!basepath) {
		return -1;
	}

	dp = sys_opendir(basepath);
	if (!dp) {
		d_fprintf(stderr,
			_("count_num_usershares: cannot open usershare "
			  "directory %s. Error %s\n"),
			basepath, strerror(errno) );
		return -1;
	}

	while((de = sys_readdir(dp)) != 0) {
		SMB_STRUCT_STAT sbuf;
		char *path;
		const char *n = de->d_name;

		/* Ignore . and .. */
		if (*n == '.') {
			if ((n[1] == '\0') || (n[1] == '.' && n[2] == '\0')) {
				continue;
			}
		}

		if (!validate_net_name(n, INVALID_SHARENAME_CHARS, strlen(n))) {
			d_fprintf(stderr,
				  _("count_num_usershares: ignoring bad share "
				    "name %s\n"), n);
			continue;
		}
		path = talloc_asprintf(ctx,
				"%s/%s",
				basepath,
				n);
		if (!path) {
			sys_closedir(dp);
			return -1;
		}

		if (sys_lstat(path, &sbuf, false) != 0) {
			d_fprintf(stderr,
				_("count_num_usershares: can't lstat file %s. "
				  "Error was %s\n"),
				path, strerror(errno) );
			continue;
		}

		if (!S_ISREG(sbuf.st_ex_mode)) {
			d_fprintf(stderr,
				_("count_num_usershares: file %s is not a "
				  "regular file. Ignoring.\n"),
				path );
			continue;
		}
		num_usershares++;
	}

	sys_closedir(dp);
	return num_usershares;
}

/***************************************************************************
 Add a single userlevel share.
***************************************************************************/

static int net_usershare_add(struct net_context *c, int argc, const char **argv)
{
	TALLOC_CTX *ctx = talloc_stackframe();
	SMB_STRUCT_STAT sbuf;
	SMB_STRUCT_STAT lsbuf;
	char *sharename;
	const char *cp_sharename;
	char *full_path;
	char *full_path_tmp;
	const char *us_path;
	const char *us_comment;
	const char *arg_acl;
	char *us_acl;
	char *file_img;
	int num_aces = 0;
	int i;
	int tmpfd;
	const char *pacl;
	size_t to_write;
	uid_t myeuid = geteuid();
	bool guest_ok = false;
	int num_usershares;

	us_comment = "";
	arg_acl = "S-1-1-0:R";

	if (c->display_usage)
		return net_usershare_add_usage(c, argc, argv);

	switch (argc) {
		case 0:
		case 1:
		default:
			return net_usershare_add_usage(c, argc, argv);
		case 2:
			cp_sharename = argv[0];
			sharename = strlower_talloc(ctx, argv[0]);
			us_path = argv[1];
			break;
		case 3:
			cp_sharename = argv[0];
			sharename = strlower_talloc(ctx, argv[0]);
			us_path = argv[1];
			us_comment = argv[2];
			break;
		case 4:
			cp_sharename = argv[0];
			sharename = strlower_talloc(ctx, argv[0]);
			us_path = argv[1];
			us_comment = argv[2];
			arg_acl = argv[3];
			break;
		case 5:
			cp_sharename = argv[0];
			sharename = strlower_talloc(ctx, argv[0]);
			us_path = argv[1];
			us_comment = argv[2];
			arg_acl = argv[3];
			if (strlen(arg_acl) == 0) {
				arg_acl = "S-1-1-0:R";
			}
			if (!strnequal(argv[4], "guest_ok=", 9)) {
				TALLOC_FREE(ctx);
				return net_usershare_add_usage(c, argc, argv);
			}
			switch (argv[4][9]) {
				case 'y':
				case 'Y':
					guest_ok = true;
					break;
				case 'n':
				case 'N':
					guest_ok = false;
					break;
				default:
					TALLOC_FREE(ctx);
					return net_usershare_add_usage(c, argc, argv);
			}
			break;
	}

	/* Ensure we're under the "usershare max shares" number. Advisory only. */
	num_usershares = count_num_usershares();
	if (num_usershares >= lp_usershare_max_shares()) {
		d_fprintf(stderr,
			_("net usershare add: maximum number of allowed "
			  "usershares (%d) reached\n"),
			lp_usershare_max_shares() );
		TALLOC_FREE(ctx);
		return -1;
	}

	if (!validate_net_name(sharename, INVALID_SHARENAME_CHARS, strlen(sharename))) {
		d_fprintf(stderr, _("net usershare add: share name %s contains "
                        "invalid characters (any of %s)\n"),
                        sharename, INVALID_SHARENAME_CHARS);
		TALLOC_FREE(ctx);
		return -1;
	}

	/* Disallow shares the same as users. */
	if (getpwnam(sharename)) {
		d_fprintf(stderr,
			_("net usershare add: share name %s is already a valid "
			  "system user name\n"),
			sharename );
		TALLOC_FREE(ctx);
		return -1;
	}

	/* Construct the full path for the usershare file. */
	full_path = get_basepath(ctx);
	if (!full_path) {
		TALLOC_FREE(ctx);
		return -1;
	}
	full_path_tmp = talloc_asprintf(ctx,
			"%s/:tmpXXXXXX",
			full_path);
	if (!full_path_tmp) {
		TALLOC_FREE(ctx);
		return -1;
	}

	full_path = talloc_asprintf_append(full_path,
					"/%s",
					sharename);
	if (!full_path) {
		TALLOC_FREE(ctx);
		return -1;
	}

	/* The path *must* be absolute. */
	if (us_path[0] != '/') {
		d_fprintf(stderr,
			_("net usershare add: path %s is not an absolute "
			  "path.\n"),
			us_path);
		TALLOC_FREE(ctx);
		return -1;
	}

	/* Check the directory to be shared exists. */
	if (sys_stat(us_path, &sbuf, false) != 0) {
		d_fprintf(stderr,
			_("net usershare add: cannot stat path %s to ensure "
			  "this is a directory. Error was %s\n"),
			us_path, strerror(errno) );
		TALLOC_FREE(ctx);
		return -1;
	}

	if (!S_ISDIR(sbuf.st_ex_mode)) {
		d_fprintf(stderr,
			_("net usershare add: path %s is not a directory.\n"),
			us_path );
		TALLOC_FREE(ctx);
		return -1;
	}

	/* If we're not root, check if we're restricted to sharing out directories
	   that we own only. */

	if ((myeuid != 0) && lp_usershare_owner_only() && (myeuid != sbuf.st_ex_uid)) {
		d_fprintf(stderr, _("net usershare add: cannot share path %s as "
			"we are restricted to only sharing directories we own.\n"
			"\tAsk the administrator to add the line \"usershare owner only = false\" \n"
			"\tto the [global] section of the smb.conf to allow this.\n"),
			us_path );
		TALLOC_FREE(ctx);
		return -1;
	}

	/* No validation needed on comment. Now go through and validate the
	   acl string. Convert names to SID's as needed. Then run it through
	   parse_usershare_acl to ensure it's valid. */

	/* Start off the string we'll append to. */
	us_acl = talloc_strdup(ctx, "");
	if (!us_acl) {
		TALLOC_FREE(ctx);
		return -1;
	}

	pacl = arg_acl;
	num_aces = 1;

	/* Add the number of ',' characters to get the number of aces. */
	num_aces += count_chars(pacl,',');

	for (i = 0; i < num_aces; i++) {
		struct dom_sid sid;
		const char *pcolon = strchr_m(pacl, ':');
		const char *name;

		if (pcolon == NULL) {
			d_fprintf(stderr,
				_("net usershare add: malformed acl %s "
				  "(missing ':').\n"),
				pacl );
			TALLOC_FREE(ctx);
			return -1;
		}

		switch(pcolon[1]) {
			case 'f':
			case 'F':
			case 'd':
			case 'r':
			case 'R':
				break;
			default:
				d_fprintf(stderr,
					_("net usershare add: malformed acl %s "
					  "(access control must be 'r', 'f', "
					  "or 'd')\n"),
					pacl );
				TALLOC_FREE(ctx);
				return -1;
		}

		if (pcolon[2] != ',' && pcolon[2] != '\0') {
			d_fprintf(stderr,
				_("net usershare add: malformed terminating "
				  "character for acl %s\n"),
				pacl );
			TALLOC_FREE(ctx);
			return -1;
		}

		/* Get the name */
		if ((name = talloc_strndup(ctx, pacl, pcolon - pacl)) == NULL) {
			d_fprintf(stderr, _("talloc_strndup failed\n"));
			TALLOC_FREE(ctx);
			return -1;
		}
		if (!string_to_sid(&sid, name)) {
			/* Convert to a SID */
			NTSTATUS ntstatus = net_lookup_sid_from_name(c, ctx, name, &sid);
			if (!NT_STATUS_IS_OK(ntstatus)) {
				d_fprintf(stderr,
					_("net usershare add: cannot convert "
					  "name \"%s\" to a SID. %s."),
					name, get_friendly_nt_error_msg(ntstatus) );
				if (NT_STATUS_EQUAL(ntstatus, NT_STATUS_CONNECTION_REFUSED)) {
					d_fprintf(stderr,
					    _(" Maybe smbd is not running.\n"));
				} else {
					d_fprintf(stderr, "\n");
				}
				TALLOC_FREE(ctx);
				return -1;
			}
		}
		us_acl = talloc_asprintf_append(
			us_acl, "%s:%c,", sid_string_tos(&sid), pcolon[1]);

		/* Move to the next ACL entry. */
		if (pcolon[2] == ',') {
			pacl = &pcolon[3];
		}
	}

	/* Remove the last ',' */
	us_acl[strlen(us_acl)-1] = '\0';

	if (guest_ok && !lp_usershare_allow_guests()) {
		d_fprintf(stderr, _("net usershare add: guest_ok=y requested "
			"but the \"usershare allow guests\" parameter is not "
			"enabled by this server.\n"));
		TALLOC_FREE(ctx);
		return -1;
	}

	/* Create a temporary filename for this share. */
	tmpfd = mkstemp(full_path_tmp);

	if (tmpfd == -1) {
		d_fprintf(stderr,
			  _("net usershare add: cannot create tmp file %s\n"),
			  full_path_tmp );
		TALLOC_FREE(ctx);
		return -1;
	}

	/* Ensure we opened the file we thought we did. */
	if (sys_lstat(full_path_tmp, &lsbuf, false) != 0) {
		d_fprintf(stderr,
			  _("net usershare add: cannot lstat tmp file %s\n"),
			  full_path_tmp );
		TALLOC_FREE(ctx);
		close(tmpfd);
		return -1;
	}

	/* Check this is the same as the file we opened. */
	if (sys_fstat(tmpfd, &sbuf, false) != 0) {
		d_fprintf(stderr,
			  _("net usershare add: cannot fstat tmp file %s\n"),
			  full_path_tmp );
		TALLOC_FREE(ctx);
		close(tmpfd);
		return -1;
	}

	if (!S_ISREG(sbuf.st_ex_mode) || sbuf.st_ex_dev != lsbuf.st_ex_dev || sbuf.st_ex_ino != lsbuf.st_ex_ino) {
		d_fprintf(stderr,
			  _("net usershare add: tmp file %s is not a regular "
			    "file ?\n"),
			  full_path_tmp );
		TALLOC_FREE(ctx);
		close(tmpfd);
		return -1;
	}

	if (fchmod(tmpfd, 0644) == -1) {
		d_fprintf(stderr,
			  _("net usershare add: failed to fchmod tmp file %s "
			    "to 0644n"),
			  full_path_tmp );
		TALLOC_FREE(ctx);
		close(tmpfd);
		return -1;
	}

	/* Create the in-memory image of the file. */
	file_img = talloc_strdup(ctx, "#VERSION 2\npath=");
	file_img = talloc_asprintf_append(file_img,
			"%s\ncomment=%s\nusershare_acl=%s\n"
			"guest_ok=%c\nsharename=%s\n",
			us_path,
			us_comment,
			us_acl,
			guest_ok ? 'y' : 'n',
			cp_sharename);

	to_write = strlen(file_img);

	if (write(tmpfd, file_img, to_write) != to_write) {
		d_fprintf(stderr,
			_("net usershare add: failed to write %u bytes to "
			  "file %s. Error was %s\n"),
			(unsigned int)to_write, full_path_tmp, strerror(errno));
		unlink(full_path_tmp);
		TALLOC_FREE(ctx);
		close(tmpfd);
		return -1;
	}

	/* Attempt to replace any existing share by this name. */
	if (rename(full_path_tmp, full_path) != 0) {
		unlink(full_path_tmp);
		d_fprintf(stderr,
			_("net usershare add: failed to add share %s. Error "
			  "was %s\n"),
			sharename, strerror(errno));
		TALLOC_FREE(ctx);
		close(tmpfd);
		return -1;
	}

	close(tmpfd);

	if (c->opt_long_list_entries) {
		const char *my_argv[2];
		my_argv[0] = sharename;
		my_argv[1] = NULL;
		net_usershare_info(c, 1, my_argv);
	}

	TALLOC_FREE(ctx);
	return 0;
}

#if 0
/***************************************************************************
 List function.
***************************************************************************/

static int list_fn(struct file_list *fl, void *priv)
{
	d_printf("%s\n", fl->pathname);
	return 0;
}
#endif

/***************************************************************************
 List userlevel shares.
***************************************************************************/

static int net_usershare_list(struct net_context *c, int argc,
			      const char **argv)
{
	fstring wcard;
	bool only_ours = true;
	int ret = -1;
	struct us_priv_info pi;
	TALLOC_CTX *ctx;

	fstrcpy(wcard, "*");

	if (c->display_usage)
		return net_usershare_list_usage(c, argc, argv);

	if (c->opt_long_list_entries) {
		only_ours = false;
	}

	switch (argc) {
		case 0:
			break;
		case 1:
			fstrcpy(wcard, argv[0]);
			break;
		default:
			return net_usershare_list_usage(c, argc, argv);
	}

	strlower_m(wcard);

	ctx = talloc_init("share_list");
	ret = get_share_list(ctx, wcard, only_ours);
	if (ret) {
		return ret;
	}

	pi.ctx = ctx;
	pi.op = US_LIST_OP;
	pi.c = c;

	ret = process_share_list(info_fn, &pi);
	talloc_destroy(ctx);
	return ret;
}

/***************************************************************************
 Entry-point for all the USERSHARE functions.
***************************************************************************/

int net_usershare(struct net_context *c, int argc, const char **argv)
{
	SMB_STRUCT_DIR *dp;

	struct functable func[] = {
		{
			"add",
			net_usershare_add,
			NET_TRANSPORT_LOCAL,
			N_("Add/modify user defined share"),
			N_("net usershare add\n"
			   "    Add/modify user defined share")
		},
		{
			"delete",
			net_usershare_delete,
			NET_TRANSPORT_LOCAL,
			N_("Delete user defined share"),
			N_("net usershare delete\n"
			   "    Delete user defined share")
		},
		{
			"info",
			net_usershare_info,
			NET_TRANSPORT_LOCAL,
			N_("Display information about a user defined share"),
			N_("net usershare info\n"
			   "    Display information about a user defined share")
		},
		{
			"list",
			net_usershare_list,
			NET_TRANSPORT_LOCAL,
			N_("List user defined shares"),
			N_("net usershare list\n"
			   "    List user defined shares")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (lp_usershare_max_shares() == 0) {
		d_fprintf(stderr,
			  _("net usershare: usershares are currently "
			    "disabled\n"));
		return -1;
	}

	dp = sys_opendir(lp_usershare_path());
	if (!dp) {
		int err = errno;
		d_fprintf(stderr,
			_("net usershare: cannot open usershare directory %s. "
			  "Error %s\n"),
			lp_usershare_path(), strerror(err) );
		if (err == EACCES) {
			d_fprintf(stderr,
				_("You do not have permission to create a "
				"usershare. Ask your administrator to grant "
				"you permissions to create a share.\n"));
		} else if (err == ENOENT) {
			d_fprintf(stderr,
				_("Please ask your system administrator to "
				  "enable user sharing.\n"));
		}
		return -1;
	}
	sys_closedir(dp);

	return net_run_function(c, argc, argv, "net usershare", func);
}
