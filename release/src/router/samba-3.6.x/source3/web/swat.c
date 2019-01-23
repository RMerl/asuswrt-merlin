/* 
   Unix SMB/CIFS implementation.
   Samba Web Administration Tool
   Version 3.0.0
   Copyright (C) Andrew Tridgell 1997-2002
   Copyright (C) John H Terpstra 2002

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

/**
 * @defgroup swat SWAT - Samba Web Administration Tool
 * @{ 
 * @file swat.c
 *
 * @brief Samba Web Administration Tool.
 **/

#include "includes.h"
#include "system/filesys.h"
#include "popt_common.h"
#include "web/swat_proto.h"
#include "printing/pcap.h"
#include "printing/load.h"
#include "passdb.h"
#include "intl/lang_tdb.h"
#include "../lib/crypto/md5.h"

static int demo_mode = False;
static int passwd_only = False;
static bool have_write_access = False;
static bool have_read_access = False;
static int iNumNonAutoPrintServices = 0;

/*
 * Password Management Globals
 */
#define SWAT_USER "username"
#define OLD_PSWD "old_passwd"
#define NEW_PSWD "new_passwd"
#define NEW2_PSWD "new2_passwd"
#define CHG_S_PASSWD_FLAG "chg_s_passwd_flag"
#define CHG_R_PASSWD_FLAG "chg_r_passwd_flag"
#define ADD_USER_FLAG "add_user_flag"
#define DELETE_USER_FLAG "delete_user_flag"
#define DISABLE_USER_FLAG "disable_user_flag"
#define ENABLE_USER_FLAG "enable_user_flag"
#define RHOST "remote_host"
#define XSRF_TOKEN "xsrf"
#define XSRF_TIME "xsrf_time"
#define XSRF_TIMEOUT 300

#define _(x) lang_msg_rotate(talloc_tos(),x)

/****************************************************************************
****************************************************************************/
static int enum_index(int value, const struct enum_list *enumlist)
{
	int i;
	for (i=0;enumlist[i].name;i++)
		if (value == enumlist[i].value) break;
	return(i);
}

static char *fix_backslash(const char *str)
{
	static char newstring[1024];
	char *p = newstring;

        while (*str) {
                if (*str == '\\') {*p++ = '\\';*p++ = '\\';}
                else *p++ = *str;
                ++str;
        }
	*p = '\0';
	return newstring;
}

static const char *fix_quotes(TALLOC_CTX *ctx, const char *str)
{
	char *newstring = NULL;
	char *p = NULL;
	size_t newstring_len;
	int quote_len = strlen("&quot;");

	/* Count the number of quotes. */
	newstring_len = 1;
	p = (char *) str;
	while (*p) {
		if ( *p == '\"') {
			newstring_len += quote_len;
		} else {
			newstring_len++;
		}
		++p;
	}
	newstring = TALLOC_ARRAY(ctx, char, newstring_len);
	if (!newstring) {
		return "";
	}
	for (p = newstring; *str; str++) {
		if ( *str == '\"') {
			strncpy( p, "&quot;", quote_len);
			p += quote_len;
		} else {
			*p++ = *str;
		}
	}
	*p = '\0';
	return newstring;
}

static char *stripspaceupper(const char *str)
{
	static char newstring[1024];
	char *p = newstring;

	while (*str) {
		if (*str != ' ') *p++ = toupper_m(*str);
		++str;
	}
	*p = '\0';
	return newstring;
}

static char *make_parm_name(const char *label)
{
	static char parmname[1024];
	char *p = parmname;

	while (*label) {
		if (*label == ' ') *p++ = '_';
		else *p++ = *label;
		++label;
	}
	*p = '\0';
	return parmname;
}

void get_xsrf_token(const char *username, const char *pass,
		    const char *formname, time_t xsrf_time, char token_str[33])
{
	MD5_CTX md5_ctx;
	uint8_t token[16];
	int i;
	char *nonce = cgi_nonce();

	token_str[0] = '\0';
	ZERO_STRUCT(md5_ctx);
	MD5Init(&md5_ctx);

	MD5Update(&md5_ctx, (uint8_t *)formname, strlen(formname));
	MD5Update(&md5_ctx, (uint8_t *)&xsrf_time, sizeof(time_t));
	if (username != NULL) {
		MD5Update(&md5_ctx, (uint8_t *)username, strlen(username));
	}
	if (pass != NULL) {
		MD5Update(&md5_ctx, (uint8_t *)pass, strlen(pass));
	}
	MD5Update(&md5_ctx, (uint8_t *)nonce, strlen(nonce));

	MD5Final(token, &md5_ctx);

	for(i = 0; i < sizeof(token); i++) {
		char tmp[3];

		snprintf(tmp, sizeof(tmp), "%02x", token[i]);
		strlcat(token_str, tmp, sizeof(tmp));
	}
}

void print_xsrf_token(const char *username, const char *pass,
		      const char *formname)
{
	char token[33];
	time_t xsrf_time = time(NULL);

	get_xsrf_token(username, pass, formname, xsrf_time, token);
	printf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n",
	       XSRF_TOKEN, token);
	printf("<input type=\"hidden\" name=\"%s\" value=\"%lld\">\n",
	       XSRF_TIME, (long long int)xsrf_time);
}

bool verify_xsrf_token(const char *formname)
{
	char expected[33];
	const char *username = cgi_user_name();
	const char *pass = cgi_user_pass();
	const char *token = cgi_variable_nonull(XSRF_TOKEN);
	const char *time_str = cgi_variable_nonull(XSRF_TIME);
	char *p = NULL;
	long long xsrf_time_ll = 0;
	time_t xsrf_time = 0;
	time_t now = time(NULL);

	errno = 0;
	xsrf_time_ll = strtoll(time_str, &p, 10);
	if (errno != 0) {
		return false;
	}
	if (p == NULL) {
		return false;
	}
	if (PTR_DIFF(p, time_str) > strlen(time_str)) {
		return false;
	}
	if (xsrf_time_ll > _TYPE_MAXIMUM(time_t)) {
		return false;
	}
	if (xsrf_time_ll < _TYPE_MINIMUM(time_t)) {
		return false;
	}
	xsrf_time = xsrf_time_ll;

	if (abs(now - xsrf_time) > XSRF_TIMEOUT) {
		return false;
	}

	get_xsrf_token(username, pass, formname, xsrf_time, expected);
	return (strncmp(expected, token, sizeof(expected)) == 0);
}


/****************************************************************************
  include a lump of html in a page 
****************************************************************************/
static int include_html(const char *fname)
{
	int fd;
	char buf[1024];
	int ret;

	fd = web_open(fname, O_RDONLY, 0);

	if (fd == -1) {
		printf(_("ERROR: Can't open %s"), fname);
		printf("\n");
		return 0;
	}

	while ((ret = read(fd, buf, sizeof(buf))) > 0) {
		if (write(1, buf, ret) == -1) {
			break;
		}
	}

	close(fd);
	return 1;
}

/****************************************************************************
  start the page with standard stuff 
****************************************************************************/
static void print_header(void)
{
	if (!cgi_waspost()) {
		printf("Expires: 0\r\n");
	}
	printf("Content-type: text/html\r\n");
	printf("X-Frame-Options: DENY\r\n\r\n");

	if (!include_html("include/header.html")) {
		printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n");
		printf("<HTML>\n<HEAD>\n<TITLE>Samba Web Administration Tool</TITLE>\n</HEAD>\n<BODY background=\"/swat/images/background.jpg\">\n\n");
	}
}

/* *******************************************************************
   show parameter label with translated name in the following form
   because showing original and translated label in one line looks
   too long, and showing translated label only is unusable for
   heavy users.
   -------------------------------
   HELP       security   [combo box][button]
   SECURITY
   -------------------------------
   (capital words are translated by gettext.)
   if no translation is available, then same form as original is
   used.
   "i18n_translated_parm" class is used to change the color of the
   translated parameter with CSS.
   **************************************************************** */
static const char *get_parm_translated(TALLOC_CTX *ctx,
	const char* pAnchor, const char* pHelp, const char* pLabel)
{
	const char *pTranslated = _(pLabel);
	char *output;
	if(strcmp(pLabel, pTranslated) != 0) {
		output = talloc_asprintf(ctx,
		  "<A HREF=\"/swat/help/manpages/smb.conf.5.html#%s\" target=\"docs\"> %s</A>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; %s <br><span class=\"i18n_translated_parm\">%s</span>",
		   pAnchor, pHelp, pLabel, pTranslated);
		return output;
	}
	output = talloc_asprintf(ctx,
	  "<A HREF=\"/swat/help/manpages/smb.conf.5.html#%s\" target=\"docs\"> %s</A>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; %s",
	  pAnchor, pHelp, pLabel);
	return output;
}
/****************************************************************************
 finish off the page
****************************************************************************/
static void print_footer(void)
{
	if (!include_html("include/footer.html")) {
		printf("\n</BODY>\n</HTML>\n");
	}
}

/****************************************************************************
  display one editable parameter in a form
****************************************************************************/
static void show_parameter(int snum, struct parm_struct *parm)
{
	int i;
	void *ptr = parm->ptr;
	char *utf8_s1, *utf8_s2;
	size_t converted_size;
	TALLOC_CTX *ctx = talloc_stackframe();

	if (parm->p_class == P_LOCAL && snum >= 0) {
		ptr = lp_local_ptr_by_snum(snum, ptr);
	}

	printf("<tr><td>%s</td><td>", get_parm_translated(ctx,
				stripspaceupper(parm->label), _("Help"), parm->label));
	switch (parm->type) {
	case P_CHAR:
		printf("<input type=text size=2 name=\"parm_%s\" value=\"%c\">",
		       make_parm_name(parm->label), *(char *)ptr);
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.value=\'%c\'\">",
			_("Set Default"), make_parm_name(parm->label),(char)(parm->def.cvalue));
		break;

	case P_LIST:
		printf("<input type=text size=40 name=\"parm_%s\" value=\"",
			make_parm_name(parm->label));
		if ((char ***)ptr && *(char ***)ptr && **(char ***)ptr) {
			char **list = *(char ***)ptr;
			for (;*list;list++) {
				/* enclose in HTML encoded quotes if the string contains a space */
				if ( strchr_m(*list, ' ') ) {
					push_utf8_talloc(talloc_tos(), &utf8_s1, *list, &converted_size);
					push_utf8_talloc(talloc_tos(), &utf8_s2, ((*(list+1))?", ":""), &converted_size);
					printf("&quot;%s&quot;%s", utf8_s1, utf8_s2);
				} else {
					push_utf8_talloc(talloc_tos(), &utf8_s1, *list, &converted_size);
					push_utf8_talloc(talloc_tos(), &utf8_s2, ((*(list+1))?", ":""), &converted_size);
					printf("%s%s", utf8_s1, utf8_s2);
				}
				TALLOC_FREE(utf8_s1);
				TALLOC_FREE(utf8_s2);
			}
		}
		printf("\">");
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.value=\'",
			_("Set Default"), make_parm_name(parm->label));
		if (parm->def.lvalue) {
			char **list = (char **)(parm->def.lvalue);
			for (; *list; list++) {
				/* enclose in HTML encoded quotes if the string contains a space */
				if ( strchr_m(*list, ' ') )
					printf("&quot;%s&quot;%s", *list, ((*(list+1))?", ":""));
				else
					printf("%s%s", *list, ((*(list+1))?", ":""));
			}
		}
		printf("\'\">");
		break;

	case P_STRING:
	case P_USTRING:
		push_utf8_talloc(talloc_tos(), &utf8_s1, *(char **)ptr, &converted_size);
		printf("<input type=text size=40 name=\"parm_%s\" value=\"%s\">",
		       make_parm_name(parm->label), fix_quotes(ctx, utf8_s1));
		TALLOC_FREE(utf8_s1);
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.value=\'%s\'\">",
			_("Set Default"), make_parm_name(parm->label),fix_backslash((char *)(parm->def.svalue)));
		break;

	case P_BOOL:
		printf("<select name=\"parm_%s\">",make_parm_name(parm->label)); 
		printf("<option %s>Yes", (*(bool *)ptr)?"selected":"");
		printf("<option %s>No", (*(bool *)ptr)?"":"selected");
		printf("</select>");
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.selectedIndex=\'%d\'\">",
			_("Set Default"), make_parm_name(parm->label),(bool)(parm->def.bvalue)?0:1);
		break;

	case P_BOOLREV:
		printf("<select name=\"parm_%s\">",make_parm_name(parm->label)); 
		printf("<option %s>Yes", (*(bool *)ptr)?"":"selected");
		printf("<option %s>No", (*(bool *)ptr)?"selected":"");
		printf("</select>");
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.selectedIndex=\'%d\'\">",
			_("Set Default"), make_parm_name(parm->label),(bool)(parm->def.bvalue)?1:0);
		break;

	case P_INTEGER:
		printf("<input type=text size=8 name=\"parm_%s\" value=\"%d\">", make_parm_name(parm->label), *(int *)ptr);
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.value=\'%d\'\">",
			_("Set Default"), make_parm_name(parm->label),(int)(parm->def.ivalue));
		break;

	case P_OCTAL: {
		char *o;
		o = octal_string(*(int *)ptr);
		printf("<input type=text size=8 name=\"parm_%s\" value=%s>",
		       make_parm_name(parm->label), o);
		TALLOC_FREE(o);
		o = octal_string((int)(parm->def.ivalue));
		printf("<input type=button value=\"%s\" "
		       "onClick=\"swatform.parm_%s.value=\'%s\'\">",
		       _("Set Default"), make_parm_name(parm->label), o);
		TALLOC_FREE(o);
		break;
	}

	case P_ENUM:
		printf("<select name=\"parm_%s\">",make_parm_name(parm->label)); 
		for (i=0;parm->enum_list[i].name;i++) {
			if (i == 0 || parm->enum_list[i].value != parm->enum_list[i-1].value) {
				printf("<option %s>%s",(*(int *)ptr)==parm->enum_list[i].value?"selected":"",parm->enum_list[i].name);
			}
		}
		printf("</select>");
		printf("<input type=button value=\"%s\" onClick=\"swatform.parm_%s.selectedIndex=\'%d\'\">",
			_("Set Default"), make_parm_name(parm->label),enum_index((int)(parm->def.ivalue),parm->enum_list));
		break;
	case P_SEP:
		break;
	}
	printf("</td></tr>\n");
	TALLOC_FREE(ctx);
}

/****************************************************************************
  display a set of parameters for a service 
****************************************************************************/
static void show_parameters(int snum, int allparameters, unsigned int parm_filter, int printers)
{
	int i = 0;
	struct parm_struct *parm;
	const char *heading = NULL;
	const char *last_heading = NULL;

	while ((parm = lp_next_parameter(snum, &i, allparameters))) {
		if (snum < 0 && parm->p_class == P_LOCAL && !(parm->flags & FLAG_GLOBAL))
			continue;
		if (parm->p_class == P_SEPARATOR) {
			heading = parm->label;
			continue;
		}
		if (parm->flags & FLAG_HIDE) continue;
		if (snum >= 0) {
			if (printers & !(parm->flags & FLAG_PRINT)) continue;
			if (!printers & !(parm->flags & FLAG_SHARE)) continue;
		}

		if (!( parm_filter & FLAG_ADVANCED )) {
			if (!(parm->flags & FLAG_BASIC)) {
					void *ptr = parm->ptr;

				if (parm->p_class == P_LOCAL && snum >= 0) {
					ptr = lp_local_ptr_by_snum(snum, ptr);
				}

				switch (parm->type) {
				case P_CHAR:
					if (*(char *)ptr == (char)(parm->def.cvalue)) continue;
					break;

				case P_LIST:
					if (!str_list_equal(*(const char ***)ptr, 
							    (const char **)(parm->def.lvalue))) continue;
					break;

				case P_STRING:
				case P_USTRING:
					if (!strcmp(*(char **)ptr,(char *)(parm->def.svalue))) continue;
					break;

				case P_BOOL:
				case P_BOOLREV:
					if (*(bool *)ptr == (bool)(parm->def.bvalue)) continue;
					break;

				case P_INTEGER:
				case P_OCTAL:
					if (*(int *)ptr == (int)(parm->def.ivalue)) continue;
					break;


				case P_ENUM:
					if (*(int *)ptr == (int)(parm->def.ivalue)) continue;
					break;
				case P_SEP:
					continue;
					}
			}
			if (printers && !(parm->flags & FLAG_PRINT)) continue;
		}

		if ((parm_filter & FLAG_WIZARD) && !(parm->flags & FLAG_WIZARD)) continue;

		if ((parm_filter & FLAG_ADVANCED) && !(parm->flags & FLAG_ADVANCED)) continue;

		if (heading && heading != last_heading) {
			printf("<tr><td></td></tr><tr><td><b><u>%s</u></b></td></tr>\n", _(heading));
			last_heading = heading;
		}
		show_parameter(snum, parm);
	}
}

/****************************************************************************
  load the smb.conf file into loadparm.
****************************************************************************/
static bool load_config(bool save_def)
{
	return lp_load(get_dyn_CONFIGFILE(),False,save_def,False,True);
}

/****************************************************************************
  write a config file 
****************************************************************************/
static void write_config(FILE *f, bool show_defaults)
{
	TALLOC_CTX *ctx = talloc_stackframe();

	fprintf(f, "# Samba config file created using SWAT\n");
	fprintf(f, "# from %s (%s)\n", cgi_remote_host(), cgi_remote_addr());
	fprintf(f, "# Date: %s\n\n", current_timestring(ctx, False));

	lp_dump(f, show_defaults, iNumNonAutoPrintServices);

	TALLOC_FREE(ctx);
}

/****************************************************************************
  save and reload the smb.conf config file 
****************************************************************************/
static int save_reload(int snum)
{
	FILE *f;
	struct stat st;

	f = sys_fopen(get_dyn_CONFIGFILE(),"w");
	if (!f) {
		printf(_("failed to open %s for writing"), get_dyn_CONFIGFILE());
		printf("\n");
		return 0;
	}

	/* just in case they have used the buggy xinetd to create the file */
	if (fstat(fileno(f), &st) == 0 &&
	    (st.st_mode & S_IWOTH)) {
#if defined HAVE_FCHMOD
		fchmod(fileno(f), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
#else
		chmod(get_dyn_CONFIGFILE(), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
#endif
	}

	write_config(f, False);
	if (snum >= 0)
		lp_dump_one(f, False, snum);
	fclose(f);

	lp_kill_all_services();

	if (!load_config(False)) {
                printf(_("Can't reload %s"), get_dyn_CONFIGFILE());
		printf("\n");
                return 0;
        }
	iNumNonAutoPrintServices = lp_numservices();
	if (pcap_cache_loaded()) {
		load_printers(server_event_context(),
			      server_messaging_context());
	}

	return 1;
}

/****************************************************************************
  commit one parameter 
****************************************************************************/
static void commit_parameter(int snum, struct parm_struct *parm, const char *v)
{
	int i;
	char *s;

	if (snum < 0 && parm->p_class == P_LOCAL) {
		/* this handles the case where we are changing a local
		   variable globally. We need to change the parameter in 
		   all shares where it is currently set to the default */
		for (i=0;i<lp_numservices();i++) {
			s = lp_servicename(i);
			if (s && (*s) && lp_is_default(i, parm)) {
				lp_do_parameter(i, parm->label, v);
			}
		}
	}

	lp_do_parameter(snum, parm->label, v);
}

/****************************************************************************
  commit a set of parameters for a service 
****************************************************************************/
static void commit_parameters(int snum)
{
	int i = 0;
	struct parm_struct *parm;
	char *label;
	const char *v;

	while ((parm = lp_next_parameter(snum, &i, 1))) {
		if (asprintf(&label, "parm_%s", make_parm_name(parm->label)) > 0) {
			if ((v = cgi_variable(label)) != NULL) {
				if (parm->flags & FLAG_HIDE)
					continue;
				commit_parameter(snum, parm, v);
			}
			SAFE_FREE(label);
		}
	}
}

/****************************************************************************
  spit out the html for a link with an image 
****************************************************************************/
static void image_link(const char *name, const char *hlink, const char *src)
{
	printf("<A HREF=\"%s/%s\"><img border=\"0\" src=\"/swat/%s\" alt=\"%s\"></A>\n", 
	       cgi_baseurl(), hlink, src, name);
}

/****************************************************************************
  display the main navigation controls at the top of each page along
  with a title 
****************************************************************************/
static void show_main_buttons(void)
{
	char *p;

	if ((p = cgi_user_name()) && strcmp(p, "root")) {
		printf(_("Logged in as <b>%s</b>"), p);
		printf("<p>\n");
	}

	image_link(_("Home"), "", "images/home.gif");
	if (have_write_access) {
		image_link(_("Globals"), "globals", "images/globals.gif");
		image_link(_("Shares"), "shares", "images/shares.gif");
		image_link(_("Printers"), "printers", "images/printers.gif");
		image_link(_("Wizard"), "wizard", "images/wizard.gif");
	}
   /* root always gets all buttons, otherwise look for -P */
	if ( have_write_access || (!passwd_only && have_read_access) ) {
		image_link(_("Status"), "status", "images/status.gif");
		image_link(_("View Config"), "viewconfig", "images/viewconfig.gif");
	}
	image_link(_("Password Management"), "passwd", "images/passwd.gif");

	printf("<HR>\n");
}

/****************************************************************************
 * Handle Display/Edit Mode CGI
 ****************************************************************************/
static void ViewModeBoxes(int mode)
{
	printf("<p>%s:&nbsp;\n", _("Current View Is"));
	printf("<input type=radio name=\"ViewMode\" value=0 %s>%s\n", ((mode == 0) ? "checked" : ""), _("Basic"));
	printf("<input type=radio name=\"ViewMode\" value=1 %s>%s\n", ((mode == 1) ? "checked" : ""), _("Advanced"));
	printf("<br>%s:&nbsp;\n", _("Change View To"));
	printf("<input type=submit name=\"BasicMode\" value=\"%s\">\n", _("Basic"));
	printf("<input type=submit name=\"AdvMode\" value=\"%s\">\n", _("Advanced"));
	printf("</p><br>\n");
}

/****************************************************************************
  display a welcome page  
****************************************************************************/
static void welcome_page(void)
{
	if (file_exist("help/welcome.html")) {
		include_html("help/welcome.html");
	} else {
		include_html("help/welcome-no-samba-doc.html");
	}
}

/****************************************************************************
  display the current smb.conf  
****************************************************************************/
static void viewconfig_page(void)
{
	int full_view=0;
	const char form_name[] = "viewconfig";

	if (!verify_xsrf_token(form_name)) {
		goto output_page;
	}

	if (cgi_variable("full_view")) {
		full_view = 1;
	}

output_page:
	printf("<H2>%s</H2>\n", _("Current Config"));
	printf("<form method=post>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), form_name);

	if (full_view) {
		printf("<input type=submit name=\"normal_view\" value=\"%s\">\n", _("Normal View"));
	} else {
		printf("<input type=submit name=\"full_view\" value=\"%s\">\n", _("Full View"));
	}

	printf("<p><pre>");
	write_config(stdout, full_view);
	printf("</pre>");
	printf("</form>\n");
}

/****************************************************************************
  second screen of the wizard ... Fetch Configuration Parameters
****************************************************************************/
static void wizard_params_page(void)
{
	unsigned int parm_filter = FLAG_WIZARD;
	const char form_name[] = "wizard_params";

	/* Here we first set and commit all the parameters that were selected
 	   in the previous screen. */

	printf("<H2>%s</H2>\n", _("Wizard Parameter Edit Page"));

	if (!verify_xsrf_token(form_name)) {
		goto output_page;
	}

	if (cgi_variable("Commit")) {
		commit_parameters(GLOBAL_SECTION_SNUM);
		save_reload(-1);
	}

output_page:
	printf("<form name=\"swatform\" method=post action=wizard_params>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), form_name);

	if (have_write_access) {
		printf("<input type=submit name=\"Commit\" value=\"Commit Changes\">\n");
	}

	printf("<input type=reset name=\"Reset Values\" value=\"Reset\">\n");
	printf("<p>\n");

	printf("<table>\n");
	show_parameters(GLOBAL_SECTION_SNUM, 1, parm_filter, 0);
	printf("</table>\n");
	printf("</form>\n");
}

/****************************************************************************
  Utility to just rewrite the smb.conf file - effectively just cleans it up
****************************************************************************/
static void rewritecfg_file(void)
{
	commit_parameters(GLOBAL_SECTION_SNUM);
	save_reload(-1);
	printf("<H2>%s</H2>\n", _("Note: smb.conf file has been read and rewritten"));
}

/****************************************************************************
  wizard to create/modify the smb.conf file
****************************************************************************/
static void wizard_page(void)
{
	/* Set some variables to collect data from smb.conf */
	int role = 0;
	int winstype = 0;
	int have_home = -1;
	int HomeExpo = 0;
	int SerType = 0;
	const char form_name[] = "wizard";

	if (!verify_xsrf_token(form_name)) {
		goto output_page;
	}

	if (cgi_variable("Rewrite")) {
		(void) rewritecfg_file();
		return;
	}

	if (cgi_variable("GetWizardParams")){
		(void) wizard_params_page();
		return;
	}

	if (cgi_variable("Commit")){
		SerType = atoi(cgi_variable_nonull("ServerType"));
		winstype = atoi(cgi_variable_nonull("WINSType"));
		have_home = lp_servicenumber(HOMES_NAME);
		HomeExpo = atoi(cgi_variable_nonull("HomeExpo"));

		/* Plain text passwords are too badly broken - use encrypted passwords only */
		lp_do_parameter( GLOBAL_SECTION_SNUM, "encrypt passwords", "Yes");

		switch ( SerType ){
			case 0:
				/* Stand-alone Server */
				lp_do_parameter( GLOBAL_SECTION_SNUM, "security", "USER" );
				lp_do_parameter( GLOBAL_SECTION_SNUM, "domain logons", "No" );
				break;
			case 1:
				/* Domain Member */
				lp_do_parameter( GLOBAL_SECTION_SNUM, "security", "DOMAIN" );
				lp_do_parameter( GLOBAL_SECTION_SNUM, "domain logons", "No" );
				break;
			case 2:
				/* Domain Controller */
				lp_do_parameter( GLOBAL_SECTION_SNUM, "security", "USER" );
				lp_do_parameter( GLOBAL_SECTION_SNUM, "domain logons", "Yes" );
				break;
		}
		switch ( winstype ) {
			case 0:
				lp_do_parameter( GLOBAL_SECTION_SNUM, "wins support", "No" );
				lp_do_parameter( GLOBAL_SECTION_SNUM, "wins server", "" );
				break;
			case 1:
				lp_do_parameter( GLOBAL_SECTION_SNUM, "wins support", "Yes" );
				lp_do_parameter( GLOBAL_SECTION_SNUM, "wins server", "" );
				break;
			case 2:
				lp_do_parameter( GLOBAL_SECTION_SNUM, "wins support", "No" );
				lp_do_parameter( GLOBAL_SECTION_SNUM, "wins server", cgi_variable_nonull("WINSAddr"));
				break;
		}

		/* Have to create Homes share? */
		if ((HomeExpo == 1) && (have_home == -1)) {
			const char *unix_share = HOMES_NAME;

			load_config(False);
			lp_copy_service(GLOBAL_SECTION_SNUM, unix_share);
			have_home = lp_servicenumber(HOMES_NAME);
			lp_do_parameter( have_home, "read only", "No");
			lp_do_parameter( have_home, "valid users", "%S");
			lp_do_parameter( have_home, "browseable", "No");
			commit_parameters(have_home);
			save_reload(have_home);
		}

		/* Need to Delete Homes share? */
		if ((HomeExpo == 0) && (have_home != -1)) {
			lp_remove_service(have_home);
			have_home = -1;
		}

		commit_parameters(GLOBAL_SECTION_SNUM);
		save_reload(-1);
	}
	else
	{
		/* Now determine smb.conf WINS settings */
		if (lp_wins_support())
			winstype = 1;
		if (lp_wins_server_list() && strlen(*lp_wins_server_list()))
 		        winstype = 2;

		/* Do we have a homes share? */
		have_home = lp_servicenumber(HOMES_NAME);
	}
	if ((winstype == 2) && lp_wins_support())
		winstype = 3;

	role = lp_server_role();

output_page:
	/* Here we go ... */
	printf("<H2>%s</H2>\n", _("Samba Configuration Wizard"));
	printf("<form method=post action=wizard>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), form_name);

	if (have_write_access) {
		printf("%s\n", _("The \"Rewrite smb.conf file\" button will clear the smb.conf file of all default values and of comments."));
		printf("%s", _("The same will happen if you press the commit button."));
		printf("<br><br>\n");
		printf("<center>");
		printf("<input type=submit name=\"Rewrite\" value=\"%s\"> &nbsp;&nbsp;",_("Rewrite smb.conf file"));
		printf("<input type=submit name=\"Commit\" value=\"%s\"> &nbsp;&nbsp;",_("Commit"));
		printf("<input type=submit name=\"GetWizardParams\" value=\"%s\">", _("Edit Parameter Values"));
		printf("</center>\n");
	}

	printf("<hr>");
	printf("<center><table border=0>");
	printf("<tr><td><b>%s:&nbsp;</b></td>\n", _("Server Type"));
	printf("<td><input type=radio name=\"ServerType\" value=\"0\" %s> %s&nbsp;</td>", ((role == ROLE_STANDALONE) ? "checked" : ""), _("Stand Alone"));
	printf("<td><input type=radio name=\"ServerType\" value=\"1\" %s> %s&nbsp;</td>", ((role == ROLE_DOMAIN_MEMBER) ? "checked" : ""), _("Domain Member")); 
	printf("<td><input type=radio name=\"ServerType\" value=\"2\" %s> %s&nbsp;</td>", ((role == ROLE_DOMAIN_PDC) ? "checked" : ""), _("Domain Controller"));
	printf("</tr>\n");
	if (role == ROLE_DOMAIN_BDC) {
		printf("<tr><td></td><td colspan=3><font color=\"#ff0000\">%s</font></td></tr>\n", _("Unusual Type in smb.conf - Please Select New Mode"));
	}
	printf("<tr><td><b>%s:&nbsp;</b></td>\n", _("Configure WINS As"));
	printf("<td><input type=radio name=\"WINSType\" value=\"0\" %s> %s&nbsp;</td>", ((winstype == 0) ? "checked" : ""), _("Not Used"));
	printf("<td><input type=radio name=\"WINSType\" value=\"1\" %s> %s&nbsp;</td>", ((winstype == 1) ? "checked" : ""), _("Server for client use"));
	printf("<td><input type=radio name=\"WINSType\" value=\"2\" %s> %s&nbsp;</td>", ((winstype == 2) ? "checked" : ""), _("Client of another WINS server"));
	printf("</tr>\n");
	printf("<tr><td></td><td></td><td></td><td>%s&nbsp;<input type=text size=\"16\" name=\"WINSAddr\" value=\"", _("Remote WINS Server"));

	/* Print out the list of wins servers */
	if(lp_wins_server_list()) {
		int i;
		const char **wins_servers = lp_wins_server_list();
		for(i = 0; wins_servers[i]; i++) printf("%s ", wins_servers[i]);
	}

	printf("\"></td></tr>\n");
	if (winstype == 3) {
		printf("<tr><td></td><td colspan=3><font color=\"#ff0000\">%s</font></td></tr>\n", _("Error: WINS Server Mode and WINS Support both set in smb.conf"));
		printf("<tr><td></td><td colspan=3><font color=\"#ff0000\">%s</font></td></tr>\n", _("Please Select desired WINS mode above."));
	}
	printf("<tr><td><b>%s:&nbsp;</b></td>\n", _("Expose Home Directories"));
	printf("<td><input type=radio name=\"HomeExpo\" value=\"1\" %s> Yes</td>", (have_home == -1) ? "" : "checked ");
	printf("<td><input type=radio name=\"HomeExpo\" value=\"0\" %s> No</td>", (have_home == -1 ) ? "checked" : "");
	printf("<td></td></tr>\n");

	/* Enable this when we are ready ....
	 * printf("<tr><td><b>%s:&nbsp;</b></td>\n", _("Is Print Server"));
	 * printf("<td><input type=radio name=\"PtrSvr\" value=\"1\" %s> Yes</td>");
	 * printf("<td><input type=radio name=\"PtrSvr\" value=\"0\" %s> No</td>");
	 * printf("<td></td></tr>\n");
	 */

	printf("</table></center>");
	printf("<hr>");

	printf("%s\n", _("The above configuration options will set multiple parameters and will generally assist with rapid Samba deployment."));
	printf("</form>\n");
}


/****************************************************************************
  display a globals editing page  
****************************************************************************/
static void globals_page(void)
{
	unsigned int parm_filter = FLAG_BASIC;
	int mode = 0;
	const char form_name[] = "globals";

	printf("<H2>%s</H2>\n", _("Global Parameters"));

	if (!verify_xsrf_token(form_name)) {
		goto output_page;
	}

	if (cgi_variable("Commit")) {
		commit_parameters(GLOBAL_SECTION_SNUM);
		save_reload(-1);
	}

	if ( cgi_variable("ViewMode") )
		mode = atoi(cgi_variable_nonull("ViewMode"));
	if ( cgi_variable("BasicMode"))
		mode = 0;
	if ( cgi_variable("AdvMode"))
		mode = 1;

output_page:
	printf("<form name=\"swatform\" method=post action=globals>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), form_name);

	ViewModeBoxes( mode );
	switch ( mode ) {
		case 0:
			parm_filter = FLAG_BASIC;
			break;
		case 1:
			parm_filter = FLAG_ADVANCED;
			break;
	}
	printf("<br>\n");
	if (have_write_access) {
		printf("<input type=submit name=\"Commit\" value=\"%s\">\n",
			_("Commit Changes"));
	}

	printf("<input type=reset name=\"Reset Values\" value=\"%s\">\n", 
		 _("Reset Values"));

	printf("<p>\n");
	printf("<table>\n");
	show_parameters(GLOBAL_SECTION_SNUM, 1, parm_filter, 0);
	printf("</table>\n");
	printf("</form>\n");
}

/****************************************************************************
  display a shares editing page. share is in unix codepage, 
****************************************************************************/
static void shares_page(void)
{
	const char *share = cgi_variable("share");
	char *s;
	char *utf8_s;
	int snum = -1;
	int i;
	int mode = 0;
	unsigned int parm_filter = FLAG_BASIC;
	size_t converted_size;
	const char form_name[] = "shares";

	printf("<H2>%s</H2>\n", _("Share Parameters"));

	if (!verify_xsrf_token(form_name)) {
		goto output_page;
	}

	if (share)
		snum = lp_servicenumber(share);


	if (cgi_variable("Commit") && snum >= 0) {
		commit_parameters(snum);
		save_reload(-1);
		snum = lp_servicenumber(share);
	}

	if (cgi_variable("Delete") && snum >= 0) {
		lp_remove_service(snum);
		save_reload(-1);
		share = NULL;
		snum = -1;
	}

	if (cgi_variable("createshare") && (share=cgi_variable("newshare"))) {
		snum = lp_servicenumber(share);
		if (snum < 0) {
			load_config(False);
			lp_copy_service(GLOBAL_SECTION_SNUM, share);
			snum = lp_servicenumber(share);
			save_reload(snum);
			snum = lp_servicenumber(share);
		}
	}

	if ( cgi_variable("ViewMode") )
		mode = atoi(cgi_variable_nonull("ViewMode"));
	if ( cgi_variable("BasicMode"))
		mode = 0;
	if ( cgi_variable("AdvMode"))
		mode = 1;

output_page:
	printf("<FORM name=\"swatform\" method=post>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), form_name);

	printf("<table>\n");

	ViewModeBoxes( mode );
	switch ( mode ) {
		case 0:
			parm_filter = FLAG_BASIC;
			break;
		case 1:
			parm_filter = FLAG_ADVANCED;
			break;
	}
	printf("<br><tr>\n");
	printf("<td><input type=submit name=selectshare value=\"%s\"></td>\n", _("Choose Share"));
	printf("<td><select name=share>\n");
	if (snum < 0)
		printf("<option value=\" \"> \n");
	for (i=0;i<lp_numservices();i++) {
		s = lp_servicename(i);
		if (s && (*s) && strcmp(s,"IPC$") && !lp_print_ok(i)) {
			push_utf8_talloc(talloc_tos(), &utf8_s, s, &converted_size);
			printf("<option %s value=\"%s\">%s\n", 
			       (share && strcmp(share,s)==0)?"SELECTED":"",
			       utf8_s, utf8_s);
			TALLOC_FREE(utf8_s);
		}
	}
	printf("</select></td>\n");
	if (have_write_access) {
		printf("<td><input type=submit name=\"Delete\" value=\"%s\"></td>\n", _("Delete Share"));
	}
	printf("</tr>\n");
	printf("</table>");
	printf("<table>");
	if (have_write_access) {
		printf("<tr>\n");
		printf("<td><input type=submit name=createshare value=\"%s\"></td>\n", _("Create Share"));
		printf("<td><input type=text size=30 name=newshare></td></tr>\n");
	}
	printf("</table>");


	if (snum >= 0) {
		if (have_write_access) {
			printf("<input type=submit name=\"Commit\" value=\"%s\">\n", _("Commit Changes"));
		}

		printf("<input type=reset name=\"Reset Values\" value=\"%s\">\n", _("Reset Values"));
		printf("<p>\n");
	}

	if (snum >= 0) {
		printf("<table>\n");
		show_parameters(snum, 1, parm_filter, 0);
		printf("</table>\n");
	}

	printf("</FORM>\n");
}

/*************************************************************
change a password either locally or remotely
*************************************************************/
static bool change_password(const char *remote_machine, const char *user_name, 
			    const char *old_passwd, const char *new_passwd, 
				int local_flags)
{
	NTSTATUS ret;
	char *err_str = NULL;
	char *msg_str = NULL;

	if (demo_mode) {
		printf("%s\n<p>", _("password change in demo mode rejected"));
		return False;
	}

	if (remote_machine != NULL) {
		ret = remote_password_change(remote_machine, user_name,
					     old_passwd, new_passwd, &err_str);
		if (err_str != NULL)
			printf("%s\n<p>", err_str);
		SAFE_FREE(err_str);
		return NT_STATUS_IS_OK(ret);
	}

	if(!initialize_password_db(True, NULL)) {
		printf("%s\n<p>", _("Can't setup password database vectors."));
		return False;
	}

	ret = local_password_change(user_name, local_flags, new_passwd,
					&err_str, &msg_str);

	if(msg_str)
		printf("%s\n<p>", msg_str);
	if(err_str)
		printf("%s\n<p>", err_str);

	SAFE_FREE(msg_str);
	SAFE_FREE(err_str);
	return NT_STATUS_IS_OK(ret);
}

/****************************************************************************
  do the stuff required to add or change a password 
****************************************************************************/
static void chg_passwd(void)
{
	const char *host;
	bool rslt;
	int local_flags = 0;

	/* Make sure users name has been specified */
	if (strlen(cgi_variable_nonull(SWAT_USER)) == 0) {
		printf("<p>%s\n", _(" Must specify \"User Name\" "));
		return;
	}

	/*
	 * smbpasswd doesn't require anything but the users name to delete, disable or enable the user,
	 * so if that's what we're doing, skip the rest of the checks
	 */
	if (!cgi_variable(DISABLE_USER_FLAG) && !cgi_variable(ENABLE_USER_FLAG) && !cgi_variable(DELETE_USER_FLAG)) {

		/*
		 * If current user is not root, make sure old password has been specified 
		 * If REMOTE change, even root must provide old password 
		 */
		if (((!am_root()) && (strlen( cgi_variable_nonull(OLD_PSWD)) <= 0)) ||
		    ((cgi_variable(CHG_R_PASSWD_FLAG)) &&  (strlen( cgi_variable_nonull(OLD_PSWD)) <= 0))) {
			printf("<p>%s\n", _(" Must specify \"Old Password\" "));
			return;
		}

		/* If changing a users password on a remote hosts we have to know what host */
		if ((cgi_variable(CHG_R_PASSWD_FLAG)) && (strlen( cgi_variable_nonull(RHOST)) <= 0)) {
			printf("<p>%s\n", _(" Must specify \"Remote Machine\" "));
			return;
		}

		/* Make sure new passwords have been specified */
		if ((strlen( cgi_variable_nonull(NEW_PSWD)) <= 0) ||
		    (strlen( cgi_variable_nonull(NEW2_PSWD)) <= 0)) {
			printf("<p>%s\n", _(" Must specify \"New, and Re-typed Passwords\" "));
			return;
		}

		/* Make sure new passwords was typed correctly twice */
		if (strcmp(cgi_variable_nonull(NEW_PSWD), cgi_variable_nonull(NEW2_PSWD)) != 0) {
			printf("<p>%s\n", _(" Re-typed password didn't match new password "));
			return;
		}
	}

	if (cgi_variable(CHG_R_PASSWD_FLAG)) {
		host = cgi_variable(RHOST);
	} else if (am_root()) {
		host = NULL;
	} else {
		host = "127.0.0.1";
	}

	/*
	 * Set up the local flags.
	 */

	local_flags |= (cgi_variable(ADD_USER_FLAG) ? LOCAL_ADD_USER : 0);
	local_flags |= (cgi_variable(ADD_USER_FLAG) ?  LOCAL_SET_PASSWORD : 0);
	local_flags |= (cgi_variable(CHG_S_PASSWD_FLAG) ? LOCAL_SET_PASSWORD : 0);
	local_flags |= (cgi_variable(DELETE_USER_FLAG) ? LOCAL_DELETE_USER : 0);
	local_flags |= (cgi_variable(ENABLE_USER_FLAG) ? LOCAL_ENABLE_USER : 0);
	local_flags |= (cgi_variable(DISABLE_USER_FLAG) ? LOCAL_DISABLE_USER : 0);

	rslt = change_password(host,
			       cgi_variable_nonull(SWAT_USER),
			       cgi_variable_nonull(OLD_PSWD), cgi_variable_nonull(NEW_PSWD),
				   local_flags);

	if(cgi_variable(CHG_S_PASSWD_FLAG)) {
		printf("<p>");
		if (rslt == True) {
			printf("%s\n", _(" The passwd has been changed."));
		} else {
			printf("%s\n", _(" The passwd has NOT been changed."));
		}
	}

	return;
}

/****************************************************************************
  display a password editing page  
****************************************************************************/
static void passwd_page(void)
{
	const char *new_name = cgi_user_name();
	const char passwd_form[] = "passwd";
	const char rpasswd_form[] = "rpasswd";

	if (!new_name) new_name = "";

	printf("<H2>%s</H2>\n", _("Server Password Management"));

	printf("<FORM name=\"swatform\" method=post>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), passwd_form);

	printf("<table>\n");

	/* 
	 * Create all the dialog boxes for data collection
	 */
	printf("<tr><td> %s : </td>\n", _("User Name"));
	printf("<td><input type=text size=30 name=%s value=%s></td></tr> \n", SWAT_USER, new_name);
	if (!am_root()) {
		printf("<tr><td> %s : </td>\n", _("Old Password"));
		printf("<td><input type=password size=30 name=%s></td></tr> \n",OLD_PSWD);
	}
	printf("<tr><td> %s : </td>\n", _("New Password"));
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW_PSWD);
	printf("<tr><td> %s : </td>\n", _("Re-type New Password"));
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW2_PSWD);
	printf("</table>\n");

	/*
	 * Create all the control buttons for requesting action
	 */
	printf("<input type=submit name=%s value=\"%s\">\n", 
	       CHG_S_PASSWD_FLAG, _("Change Password"));
	if (demo_mode || am_root()) {
		printf("<input type=submit name=%s value=\"%s\">\n",
		       ADD_USER_FLAG, _("Add New User"));
		printf("<input type=submit name=%s value=\"%s\">\n",
		       DELETE_USER_FLAG, _("Delete User"));
		printf("<input type=submit name=%s value=\"%s\">\n", 
		       DISABLE_USER_FLAG, _("Disable User"));
		printf("<input type=submit name=%s value=\"%s\">\n", 
		       ENABLE_USER_FLAG, _("Enable User"));
	}
	printf("<p></FORM>\n");

	/*
	 * Do some work if change, add, disable or enable was
	 * requested. It could be this is the first time through this
	 * code, so there isn't anything to do.  */
	if (verify_xsrf_token(passwd_form) &&
	   ((cgi_variable(CHG_S_PASSWD_FLAG)) || (cgi_variable(ADD_USER_FLAG)) || (cgi_variable(DELETE_USER_FLAG)) ||
	    (cgi_variable(DISABLE_USER_FLAG)) || (cgi_variable(ENABLE_USER_FLAG)))) {
		chg_passwd();		
	}

	printf("<H2>%s</H2>\n", _("Client/Server Password Management"));

	printf("<FORM name=\"swatform\" method=post>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), rpasswd_form);

	printf("<table>\n");

	/* 
	 * Create all the dialog boxes for data collection
	 */
	printf("<tr><td> %s : </td>\n", _("User Name"));
	printf("<td><input type=text size=30 name=%s value=%s></td></tr>\n",SWAT_USER, new_name);
	printf("<tr><td> %s : </td>\n", _("Old Password"));
	printf("<td><input type=password size=30 name=%s></td></tr>\n",OLD_PSWD);
	printf("<tr><td> %s : </td>\n", _("New Password"));
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW_PSWD);
	printf("<tr><td> %s : </td>\n", _("Re-type New Password"));
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW2_PSWD);
	printf("<tr><td> %s : </td>\n", _("Remote Machine"));
	printf("<td><input type=text size=30 name=%s></td></tr>\n",RHOST);

	printf("</table>");

	/*
	 * Create all the control buttons for requesting action
	 */
	printf("<input type=submit name=%s value=\"%s\">", 
	       CHG_R_PASSWD_FLAG, _("Change Password"));

	printf("<p></FORM>\n");

	/*
	 * Do some work if a request has been made to change the
	 * password somewhere other than the server. It could be this
	 * is the first time through this code, so there isn't
	 * anything to do.  */
	if (verify_xsrf_token(passwd_form) && cgi_variable(CHG_R_PASSWD_FLAG)) {
		chg_passwd();		
	}

}

/****************************************************************************
  display a printers editing page  
****************************************************************************/
static void printers_page(void)
{
	const char *share = cgi_variable("share");
	char *s;
	int snum=-1;
	int i;
	int mode = 0;
	unsigned int parm_filter = FLAG_BASIC;
	const char form_name[] = "printers";

	if (!verify_xsrf_token(form_name)) {
		goto output_page;
	}

	if (share)
		snum = lp_servicenumber(share);

	if (cgi_variable("Commit") && snum >= 0) {
		commit_parameters(snum);
		if (snum >= iNumNonAutoPrintServices)
		    save_reload(snum);
		else
		    save_reload(-1);
		snum = lp_servicenumber(share);
	}

	if (cgi_variable("Delete") && snum >= 0) {
		lp_remove_service(snum);
		save_reload(-1);
		share = NULL;
		snum = -1;
	}

	if (cgi_variable("createshare") && (share=cgi_variable("newshare"))) {
		snum = lp_servicenumber(share);
		if (snum < 0 || snum >= iNumNonAutoPrintServices) {
			load_config(False);
			lp_copy_service(GLOBAL_SECTION_SNUM, share);
			snum = lp_servicenumber(share);
			lp_do_parameter(snum, "print ok", "Yes");
			save_reload(snum);
			snum = lp_servicenumber(share);
		}
	}

	if ( cgi_variable("ViewMode") )
		mode = atoi(cgi_variable_nonull("ViewMode"));
        if ( cgi_variable("BasicMode"))
                mode = 0;
        if ( cgi_variable("AdvMode"))
                mode = 1;

output_page:
        printf("<H2>%s</H2>\n", _("Printer Parameters"));

        printf("<H3>%s</H3>\n", _("Important Note:"));
        printf("%s",_("Printer names marked with [*] in the Choose Printer drop-down box "));
        printf("%s",_("are autoloaded printers from "));
        printf("<A HREF=\"/swat/help/smb.conf.5.html#printcapname\" target=\"docs\">%s</A>\n", _("Printcap Name"));
        printf("%s\n", _("Attempting to delete these printers from SWAT will have no effect."));


	printf("<FORM name=\"swatform\" method=post>\n");
	print_xsrf_token(cgi_user_name(), cgi_user_pass(), form_name);

	ViewModeBoxes( mode );
	switch ( mode ) {
		case 0:
			parm_filter = FLAG_BASIC;
			break;
		case 1:
			parm_filter = FLAG_ADVANCED;
			break;
	}
	printf("<table>\n");
	printf("<tr><td><input type=submit name=\"selectshare\" value=\"%s\"></td>\n", _("Choose Printer"));
	printf("<td><select name=\"share\">\n");
	if (snum < 0 || !lp_print_ok(snum))
		printf("<option value=\" \"> \n");
	for (i=0;i<lp_numservices();i++) {
		s = lp_servicename(i);
		if (s && (*s) && strcmp(s,"IPC$") && lp_print_ok(i)) {
                    if (i >= iNumNonAutoPrintServices)
                        printf("<option %s value=\"%s\">[*]%s\n",
                               (share && strcmp(share,s)==0)?"SELECTED":"",
                               s, s);
                    else
			printf("<option %s value=\"%s\">%s\n", 
			       (share && strcmp(share,s)==0)?"SELECTED":"",
			       s, s);
		}
	}
	printf("</select></td>");
	if (have_write_access) {
		printf("<td><input type=submit name=\"Delete\" value=\"%s\"></td>\n", _("Delete Printer"));
	}
	printf("</tr>");
	printf("</table>\n");

	if (have_write_access) {
		printf("<table>\n");
		printf("<tr><td><input type=submit name=\"createshare\" value=\"%s\"></td>\n", _("Create Printer"));
		printf("<td><input type=text size=30 name=\"newshare\"></td></tr>\n");
		printf("</table>");
	}


	if (snum >= 0) {
		if (have_write_access) {
			printf("<input type=submit name=\"Commit\" value=\"%s\">\n", _("Commit Changes"));
		}
		printf("<input type=reset name=\"Reset Values\" value=\"%s\">\n", _("Reset Values"));
		printf("<p>\n");
	}

	if (snum >= 0) {
		printf("<table>\n");
		show_parameters(snum, 1, parm_filter, 1);
		printf("</table>\n");
	}
	printf("</FORM>\n");
}

/*
  when the _() translation macro is used there is no obvious place to free
  the resulting string and there is no easy way to give a static pointer.
  All we can do is rotate between some static buffers and hope a single d_printf()
  doesn't have more calls to _() than the number of buffers
*/

const char *lang_msg_rotate(TALLOC_CTX *ctx, const char *msgid)
{
	const char *msgstr;
	const char *ret;

	msgstr = lang_msg(msgid);
	if (!msgstr) {
		return msgid;
	}

	ret = talloc_strdup(ctx, msgstr);

	lang_msg_free(msgstr);
	if (!ret) {
		return msgid;
	}

	return ret;
}

/**
 * main function for SWAT.
 **/
 int main(int argc, char *argv[])
{
	const char *page;
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "disable-authentication", 'a', POPT_ARG_VAL, &demo_mode, True, "Disable authentication (demo mode)" },
        	{ "password-menu-only", 'P', POPT_ARG_VAL, &passwd_only, True, "Show only change password menu" }, 
		POPT_COMMON_SAMBA
		POPT_TABLEEND
	};
	TALLOC_CTX *frame = talloc_stackframe();

	fault_setup(NULL);
	umask(S_IWGRP | S_IWOTH);

#if defined(HAVE_SET_AUTH_PARAMETERS)
	set_auth_parameters(argc, argv);
#endif /* HAVE_SET_AUTH_PARAMETERS */

	/* just in case it goes wild ... */
	alarm(300);

	setlinebuf(stdout);

	/* we don't want any SIGPIPE messages */
	BlockSignals(True,SIGPIPE);

	debug_set_logfile("/dev/null");

	/* we don't want stderr screwing us up */
	close(2);
	open("/dev/null", O_WRONLY);
	setup_logging("swat", DEBUG_FILE);

	load_case_tables();
	
	pc = poptGetContext("swat", argc, (const char **) argv, long_options, 0);

	/* Parse command line options */

	while(poptGetNextOpt(pc) != -1) { }

	poptFreeContext(pc);

	/* This should set a more apporiate log file */
	load_config(True);
	reopen_logs();
	load_interfaces();
	iNumNonAutoPrintServices = lp_numservices();
	if (pcap_cache_loaded()) {
		load_printers(server_event_context(),
			      server_messaging_context());
	}

	cgi_setup(get_dyn_SWATDIR(), !demo_mode);

	print_header();

	cgi_load_variables();

	if (!file_exist(get_dyn_CONFIGFILE())) {
		have_read_access = True;
		have_write_access = True;
	} else {
		/* check if the authenticated user has write access - if not then
		   don't show write options */
		have_write_access = (access(get_dyn_CONFIGFILE(),W_OK) == 0);

		/* if the user doesn't have read access to smb.conf then
		   don't let them view it */
		have_read_access = (access(get_dyn_CONFIGFILE(),R_OK) == 0);
	}

	show_main_buttons();

	page = cgi_pathinfo();

	/* Root gets full functionality */
	if (have_read_access && strcmp(page, "globals")==0) {
		globals_page();
	} else if (have_read_access && strcmp(page,"shares")==0) {
		shares_page();
	} else if (have_read_access && strcmp(page,"printers")==0) {
		printers_page();
	} else if (have_read_access && strcmp(page,"status")==0) {
		status_page();
	} else if (have_read_access && strcmp(page,"viewconfig")==0) {
		viewconfig_page();
	} else if (strcmp(page,"passwd")==0) {
		passwd_page();
	} else if (have_read_access && strcmp(page,"wizard")==0) {
		wizard_page();
	} else if (have_read_access && strcmp(page,"wizard_params")==0) {
		wizard_params_page();
	} else if (have_read_access && strcmp(page,"rewritecfg")==0) {
		rewritecfg_file();
	} else {
		welcome_page();
	}

	print_footer();

	TALLOC_FREE(frame);
	return 0;
}

/** @} **/
