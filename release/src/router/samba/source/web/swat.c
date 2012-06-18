/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba Web Administration Tool
   Copyright (C) Andrew Tridgell 1997-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"
#include "smb.h"

#define GLOBALS_SNUM -1

static pstring servicesf = CONFIGFILE;
static BOOL demo_mode = False;
static BOOL have_write_access = False;
static BOOL have_read_access = False;
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

/* we need these because we link to locking*.o */
 void become_root(BOOL save_dir) {}
 void unbecome_root(BOOL restore_dir) {}

/****************************************************************************
****************************************************************************/
static int enum_index(int value, struct enum_list *enumlist)
{
int i;
	for (i=0;enumlist[i].name;i++)
		if (value == enumlist[i].value) break;
	return(i);
}

static char *fix_backslash(char *str)
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

static char *stripspace(char *str)
{
static char newstring[1024];
char *p = newstring;

        while (*str) {
                if (*str != ' ') *p++ = *str;
                ++str;
        }
	*p = '\0';
	return newstring;
}

static char *make_parm_name(char *label)
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

/****************************************************************************
  include a lump of html in a page 
****************************************************************************/
static int include_html(char *fname)
{
	FILE *f = sys_fopen(fname,"r");
	char buf[1024];
	int ret;

	if (!f) {
		printf("ERROR: Can't open %s\n", fname);
		return 0;
	}

	while (!feof(f)) {
		ret = fread(buf, 1, sizeof(buf), f);
		if (ret <= 0) break;
		fwrite(buf, 1, ret, stdout);
	}

	fclose(f);
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
	printf("Content-type: text/html\r\n\r\n");

	if (!include_html("include/header.html")) {
		printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n");
		printf("<HTML>\n<HEAD>\n<TITLE>Samba Web Administration Tool</TITLE>\n</HEAD>\n<BODY background=\"/swat/images/background.jpg\">\n\n");
	}
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

	if (parm->class == P_LOCAL && snum >= 0) {
		ptr = lp_local_ptr(snum, ptr);
	}

	printf("<tr><td><A HREF=\"/swat/help/smb.conf.5.html#%s\" target=\"docs\">Help</A>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; %s</td><td>", 
	       stripspace(parm->label), parm->label);

	switch (parm->type) {
	case P_CHAR:
		printf("<input type=text size=2 name=\"parm_%s\" value=\"%c\">",
		       make_parm_name(parm->label), *(char *)ptr);
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.value=\'%c\'\">",
			make_parm_name(parm->label),(char)(parm->def.cvalue));
		break;

	case P_STRING:
	case P_USTRING:
		printf("<input type=text size=40 name=\"parm_%s\" value=\"%s\">",
		       make_parm_name(parm->label), *(char **)ptr);
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.value=\'%s\'\">",
			make_parm_name(parm->label),fix_backslash((char *)(parm->def.svalue)));
		break;

	case P_GSTRING:
	case P_UGSTRING:
		printf("<input type=text size=40 name=\"parm_%s\" value=\"%s\">",
		       make_parm_name(parm->label), (char *)ptr);
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.value=\'%s\'\">",
			make_parm_name(parm->label),fix_backslash((char *)(parm->def.svalue)));
		break;

	case P_BOOL:
		printf("<select name=\"parm_%s\">",make_parm_name(parm->label)); 
		printf("<option %s>Yes", (*(BOOL *)ptr)?"selected":"");
		printf("<option %s>No", (*(BOOL *)ptr)?"":"selected");
		printf("</select>");
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.selectedIndex=\'%d\'\">",
			make_parm_name(parm->label),(BOOL)(parm->def.bvalue)?0:1);
		break;

	case P_BOOLREV:
		printf("<select name=\"parm_%s\">",make_parm_name(parm->label)); 
		printf("<option %s>Yes", (*(BOOL *)ptr)?"":"selected");
		printf("<option %s>No", (*(BOOL *)ptr)?"selected":"");
		printf("</select>");
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.selectedIndex=\'%d\'\">",
			make_parm_name(parm->label),(BOOL)(parm->def.bvalue)?1:0);
		break;

	case P_INTEGER:
		printf("<input type=text size=8 name=\"parm_%s\" value=%d>", make_parm_name(parm->label), *(int *)ptr);
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.value=\'%d\'\">",
			make_parm_name(parm->label),(int)(parm->def.ivalue));
		break;

	case P_OCTAL:
		printf("<input type=text size=8 name=\"parm_%s\" value=%s>", make_parm_name(parm->label), octal_string(*(int *)ptr));
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.value=\'%s\'\">",
		       make_parm_name(parm->label),
		       octal_string((int)(parm->def.ivalue)));
		break;

	case P_ENUM:
		printf("<select name=\"parm_%s\">",make_parm_name(parm->label)); 
		for (i=0;parm->enum_list[i].name;i++)
			printf("<option %s>%s",(*(int *)ptr)==parm->enum_list[i].value?"selected":"",parm->enum_list[i].name);
		printf("</select>");
		printf("<input type=button value=\"Set Default\" onClick=\"swatform.parm_%s.selectedIndex=\'%d\'\">",
			make_parm_name(parm->label),enum_index((int)(parm->def.ivalue),parm->enum_list));
		break;
	case P_SEP:
		break;
	}
	printf("</td></tr>\n");
}

/****************************************************************************
  display a set of parameters for a service 
****************************************************************************/
static void show_parameters(int snum, int allparameters, int advanced, int printers)
{
	int i = 0;
	struct parm_struct *parm;
	char *heading = NULL;
	char *last_heading = NULL;

	while ((parm = lp_next_parameter(snum, &i, allparameters))) {
		if (snum < 0 && parm->class == P_LOCAL && !(parm->flags & FLAG_GLOBAL))
			continue;
		if (parm->class == P_SEPARATOR) {
			heading = parm->label;
			continue;
		}
		if (parm->flags & FLAG_HIDE) continue;
		if (snum >= 0) {
			if (printers & !(parm->flags & FLAG_PRINT)) continue;
			if (!printers & !(parm->flags & FLAG_SHARE)) continue;
		}
		if (!advanced) {
			if (!(parm->flags & FLAG_BASIC)) {
				void *ptr = parm->ptr;

				if (parm->class == P_LOCAL && snum >= 0) {
					ptr = lp_local_ptr(snum, ptr);
				}

				switch (parm->type) {
				case P_CHAR:
					if (*(char *)ptr == (char)(parm->def.cvalue)) continue;
					break;

				case P_STRING:
				case P_USTRING:
					if (!strcmp(*(char **)ptr,(char *)(parm->def.svalue))) continue;
					break;

				case P_GSTRING:
				case P_UGSTRING:
					if (!strcmp((char *)ptr,(char *)(parm->def.svalue))) continue;
					break;

				case P_BOOL:
				case P_BOOLREV:
					if (*(BOOL *)ptr == (BOOL)(parm->def.bvalue)) continue;
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
		if (heading && heading != last_heading) {
			printf("<tr><td></td></tr><tr><td><b><u>%s</u></b></td></tr>\n", heading);
			last_heading = heading;
		}
		show_parameter(snum, parm);
	}
}

/****************************************************************************
  load the smb.conf file into loadparm.
****************************************************************************/
static BOOL load_config(BOOL save_def)
{
	lp_resetnumservices();
	return lp_load(servicesf,False,save_def,False);
}

/****************************************************************************
  write a config file 
****************************************************************************/
static void write_config(FILE *f, BOOL show_defaults)
{
	fprintf(f, "# Samba config file created using SWAT\n");
	fprintf(f, "# from %s (%s)\n", cgi_remote_host(), cgi_remote_addr());
	fprintf(f, "# Date: %s\n\n", timestring(False));
	
	lp_dump(f, show_defaults, iNumNonAutoPrintServices);	
}

/****************************************************************************
  save and reoad the smb.conf config file 
****************************************************************************/
static int save_reload(int snum)
{
	FILE *f;

	f = sys_fopen(servicesf,"w");
	if (!f) {
		printf("failed to open %s for writing\n", servicesf);
		return 0;
	}

	write_config(f, False);
	if (snum)
		lp_dump_one(f, False, snum);
	fclose(f);

	lp_killunused(NULL);

	if (!load_config(False)) {
                printf("Can't reload %s\n", servicesf);
                return 0;
        }
	iNumNonAutoPrintServices = lp_numservices();
#ifdef PRINTING
	load_printers();
#endif
	return 1;
}

/****************************************************************************
  commit one parameter 
****************************************************************************/
static void commit_parameter(int snum, struct parm_struct *parm, char *v)
{
	int i;
	char *s;

	if (snum < 0 && parm->class == P_LOCAL) {
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
	pstring label;
	char *v;

	while ((parm = lp_next_parameter(snum, &i, 1))) {
		slprintf(label, sizeof(label)-1, "parm_%s", make_parm_name(parm->label));
		if ((v = cgi_variable(label))) {
			if (parm->flags & FLAG_HIDE) continue;
			commit_parameter(snum, parm, v); 
		}
	}
}

/****************************************************************************
  spit out the html for a link with an image 
****************************************************************************/
static void image_link(char *name,char *hlink, char *src)
{
	printf("<A HREF=\"%s/%s\"><img src=\"/swat/%s\" alt=\"%s\"></A>\n", 
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
		printf("Logged in as <b>%s</b><p>\n", p);
	}

	image_link("Home", "", "images/home.gif");
	if (have_write_access) {
		image_link("Globals", "globals", "images/globals.gif");
		image_link("Shares", "shares", "images/shares.gif");
		image_link("Printers", "printers", "images/printers.gif");
	}
	if (have_read_access) {
		image_link("Status", "status", "images/status.gif");
		image_link("View Config", "viewconfig","images/viewconfig.gif");
	}
	image_link("Password Management", "passwd", "images/passwd.gif");

	printf("<HR>\n");
}

/****************************************************************************
  display a welcome page  
****************************************************************************/
static void welcome_page(void)
{
	include_html("help/welcome.html");
}

/****************************************************************************
  display the current smb.conf  
****************************************************************************/
static void viewconfig_page(void)
{
	int full_view=0;

	if (cgi_variable("full_view")) {
		full_view = 1;
	}

	printf("<H2>Current Config</H2>\n");
	printf("<form method=post>\n");

	if (full_view) {
		printf("<input type=submit name=\"normal_view\" value=\"Normal View\">\n");
	} else {
		printf("<input type=submit name=\"full_view\" value=\"Full View\">\n");
	}

	printf("<p><pre>");
	write_config(stdout, full_view);
	printf("</pre>");
	printf("</form>\n");
}

/****************************************************************************
  display a globals editing page  
****************************************************************************/
static void globals_page(void)
{
	int advanced = 0;

	printf("<H2>Global Variables</H2>\n");

	if (cgi_variable("Advanced") && !cgi_variable("Basic"))
		advanced = 1;

	if (cgi_variable("Commit")) {
		commit_parameters(GLOBALS_SNUM);
		save_reload(0);
	}

	printf("<FORM name=\"swatform\" method=post>\n");

	if (have_write_access) {
		printf("<input type=submit name=\"Commit\" value=\"Commit Changes\">\n");
	}

	printf("<input type=reset name=\"Reset Values\" value=\"Reset Values\">\n");
	if (advanced == 0) {
		printf("<input type=submit name=\"Advanced\" value=\"Advanced View\">\n");
	} else {
		printf("<input type=submit name=\"Basic\" value=\"Basic View\">\n");
	}
	printf("<p>\n");
	
	printf("<table>\n");
	show_parameters(GLOBALS_SNUM, 1, advanced, 0);
	printf("</table>\n");

	if (advanced) {
		printf("<input type=hidden name=\"Advanced\" value=1>\n");
	}

	printf("</FORM>\n");
}

/****************************************************************************
  display a shares editing page  
****************************************************************************/
static void shares_page(void)
{
	char *share = cgi_variable("share");
	char *s;
	int snum=-1;
	int i;
	int advanced = 0;

	if (share)
		snum = lp_servicenumber(share);

	printf("<H2>Share Parameters</H2>\n");

	if (cgi_variable("Advanced") && !cgi_variable("Basic"))
		advanced = 1;

	if (cgi_variable("Commit") && snum >= 0) {
		commit_parameters(snum);
		save_reload(0);
	}

	if (cgi_variable("Delete") && snum >= 0) {
		lp_remove_service(snum);
		save_reload(0);
		share = NULL;
		snum = -1;
	}

	if (cgi_variable("createshare") && (share=cgi_variable("newshare"))) {
		load_config(False);
		lp_copy_service(GLOBALS_SNUM, share);
		iNumNonAutoPrintServices = lp_numservices();
		save_reload(0);
		snum = lp_servicenumber(share);
	}

	printf("<FORM name=\"swatform\" method=post>\n");

	printf("<table>\n");
	printf("<tr>\n");
	printf("<td><input type=submit name=selectshare value=\"Choose Share\"></td>\n");
	printf("<td><select name=share>\n");
	if (snum < 0)
		printf("<option value=\" \"> \n");
	for (i=0;i<lp_numservices();i++) {
		s = lp_servicename(i);
		if (s && (*s) && strcmp(s,"IPC$") && !lp_print_ok(i)) {
			printf("<option %s value=\"%s\">%s\n", 
			       (share && strcmp(share,s)==0)?"SELECTED":"",
			       s, s);
		}
	}
	printf("</select></td>\n");
	if (have_write_access) {
		printf("<td><input type=submit name=\"Delete\" value=\"Delete Share\"></td>\n");
	}
	printf("</tr>\n");
	printf("</table>");
	printf("<table>");
	if (have_write_access) {
		printf("<tr>\n");
		printf("<td><input type=submit name=createshare value=\"Create Share\"></td>\n");
		printf("<td><input type=text size=30 name=newshare></td></tr>\n");
	}
	printf("</table>");


	if (snum >= 0) {
		if (have_write_access) {
			printf("<input type=submit name=\"Commit\" value=\"Commit Changes\">\n");
		}

		printf("<input type=reset name=\"Reset Values\" value=\"Reset Values\">\n");
		if (advanced == 0) {
			printf("<input type=submit name=\"Advanced\" value=\"Advanced View\">\n");
		} else {
			printf("<input type=submit name=\"Basic\" value=\"Basic View\">\n");
		}
		printf("<p>\n");
	}

	if (snum >= 0) {
		printf("<table>\n");
		show_parameters(snum, 1, advanced, 0);
		printf("</table>\n");
	}

	if (advanced) {
		printf("<input type=hidden name=\"Advanced\" value=1>\n");
	}

	printf("</FORM>\n");
}

/*************************************************************
change a password either locally or remotely
*************************************************************/
static BOOL change_password(const char *remote_machine, char *user_name, 
			    char *old_passwd, char *new_passwd, 
				int local_flags)
{
	BOOL ret = False;
	pstring err_str;
	pstring msg_str;

	if (demo_mode) {
		printf("password change in demo mode rejected\n<p>");
		return False;
	}
	
	if (remote_machine != NULL) {
		ret = remote_password_change(remote_machine, user_name, old_passwd, 
									 new_passwd, err_str, sizeof(err_str));
		if(*err_str)
			printf("%s\n<p>", err_str);
		return ret;
	}

	if(!initialize_password_db()) {
		printf("Can't setup password database vectors.\n<p>");
		return False;
	}
	
	ret = local_password_change(user_name, local_flags, new_passwd, err_str, sizeof(err_str),
					 msg_str, sizeof(msg_str));

	if(*msg_str)
		printf("%s\n<p>", msg_str);
	if(*err_str)
		printf("%s\n<p>", err_str);

	return ret;
}

/****************************************************************************
  do the stuff required to add or change a password 
****************************************************************************/
static void chg_passwd(void)
{
	char *host;
	BOOL rslt;
	int local_flags = 0;

	/* Make sure users name has been specified */
	if (strlen(cgi_variable(SWAT_USER)) == 0) {
		printf("<p> Must specify \"User Name\" \n");
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
		if (((!am_root()) && (strlen( cgi_variable(OLD_PSWD)) <= 0)) ||
		    ((cgi_variable(CHG_R_PASSWD_FLAG)) &&  (strlen( cgi_variable(OLD_PSWD)) <= 0))) {
			printf("<p> Must specify \"Old Password\" \n");
			return;
		}

		/* If changing a users password on a remote hosts we have to know what host */
		if ((cgi_variable(CHG_R_PASSWD_FLAG)) && (strlen( cgi_variable(RHOST)) <= 0)) {
			printf("<p> Must specify \"Remote Machine\" \n");
			return;
		}

		/* Make sure new passwords have been specified */
		if ((strlen( cgi_variable(NEW_PSWD)) <= 0) ||
		    (strlen( cgi_variable(NEW2_PSWD)) <= 0)) {
			printf("<p> Must specify \"New, and Re-typed Passwords\" \n");
			return;
		}

		/* Make sure new passwords was typed correctly twice */
		if (strcmp(cgi_variable(NEW_PSWD), cgi_variable(NEW2_PSWD)) != 0) {
			printf("<p> Re-typed password didn't match new password\n");
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
	local_flags |= (cgi_variable(DELETE_USER_FLAG) ? LOCAL_DELETE_USER : 0);
	local_flags |= (cgi_variable(ENABLE_USER_FLAG) ? LOCAL_ENABLE_USER : 0);
	local_flags |= (cgi_variable(DISABLE_USER_FLAG) ? LOCAL_DISABLE_USER : 0);

	rslt = change_password(host,
			       cgi_variable(SWAT_USER),
			       cgi_variable(OLD_PSWD), cgi_variable(NEW_PSWD),
				   local_flags);

	if(local_flags == 0) {
		if (rslt == True) {
			printf("<p> The passwd for '%s' has been changed. \n", cgi_variable(SWAT_USER));
		} else {
			printf("<p> The passwd for '%s' has NOT been changed. \n",cgi_variable(SWAT_USER));
		}
	}
	
	return;
}

/****************************************************************************
  display a password editing page  
****************************************************************************/
static void passwd_page(void)
{
	char *new_name = cgi_user_name();

	/* 
	 * After the first time through here be nice. If the user
	 * changed the User box text to another users name, remember it.
	 */
	if (cgi_variable(SWAT_USER)) {
		new_name = cgi_variable(SWAT_USER);
	} 

	if (!new_name) new_name = "";

	printf("<H2>Server Password Management</H2>\n");

	printf("<FORM name=\"swatform\" method=post>\n");

	printf("<table>\n");

	/* 
	 * Create all the dialog boxes for data collection
	 */
	printf("<tr><td> User Name : </td>\n");
	printf("<td><input type=text size=30 name=%s value=%s></td></tr> \n", SWAT_USER, new_name);
	if (!am_root()) {
		printf("<tr><td> Old Password : </td>\n");
		printf("<td><input type=password size=30 name=%s></td></tr> \n",OLD_PSWD);
	}
	printf("<tr><td> New Password : </td>\n");
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW_PSWD);
	printf("<tr><td> Re-type New Password : </td>\n");
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW2_PSWD);
	printf("</table>\n");

	/*
	 * Create all the control buttons for requesting action
	 */
	printf("<input type=submit name=%s value=\"Change Password\">\n", 
	       CHG_S_PASSWD_FLAG);
	if (demo_mode || am_root()) {
		printf("<input type=submit name=%s value=\"Add New User\">\n",
		       ADD_USER_FLAG);
		printf("<input type=submit name=%s value=\"Delete User\">\n",
		       DELETE_USER_FLAG);
		printf("<input type=submit name=%s value=\"Disable User\">\n", 
		       DISABLE_USER_FLAG);
		printf("<input type=submit name=%s value=\"Enable User\">\n", 
		       ENABLE_USER_FLAG);
	}
	printf("<p></FORM>\n");

	/*
	 * Do some work if change, add, disable or enable was
	 * requested. It could be this is the first time through this
	 * code, so there isn't anything to do.  */
	if ((cgi_variable(CHG_S_PASSWD_FLAG)) || (cgi_variable(ADD_USER_FLAG)) || (cgi_variable(DELETE_USER_FLAG)) ||
	    (cgi_variable(DISABLE_USER_FLAG)) || (cgi_variable(ENABLE_USER_FLAG))) {
		chg_passwd();		
	}

	printf("<H2>Client/Server Password Management</H2>\n");

	printf("<FORM name=\"swatform\" method=post>\n");

	printf("<table>\n");

	/* 
	 * Create all the dialog boxes for data collection
	 */
	printf("<tr><td> User Name : </td>\n");
	printf("<td><input type=text size=30 name=%s value=%s></td></tr>\n",SWAT_USER, new_name);
	printf("<tr><td> Old Password : </td>\n");
	printf("<td><input type=password size=30 name=%s></td></tr>\n",OLD_PSWD);
	printf("<tr><td> New Password : </td>\n");
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW_PSWD);
	printf("<tr><td> Re-type New Password : </td>\n");
	printf("<td><input type=password size=30 name=%s></td></tr>\n",NEW2_PSWD);
	printf("<tr><td> Remote Machine : </td>\n");
	printf("<td><input type=text size=30 name=%s></td></tr>\n",RHOST);

	printf("</table>");

	/*
	 * Create all the control buttons for requesting action
	 */
	printf("<input type=submit name=%s value=\"Change Password\">", 
	       CHG_R_PASSWD_FLAG);

	printf("<p></FORM>\n");

	/*
	 * Do some work if a request has been made to change the
	 * password somewhere other than the server. It could be this
	 * is the first time through this code, so there isn't
	 * anything to do.  */
	if (cgi_variable(CHG_R_PASSWD_FLAG)) {
		chg_passwd();		
	}

}

/****************************************************************************
  display a printers editing page  
****************************************************************************/
static void printers_page(void)
{
	char *share = cgi_variable("share");
	char *s;
	int snum=-1;
	int i;
	int advanced = 0;

	if (share)
		snum = lp_servicenumber(share);

	printf("<H2>Printer Parameters</H2>\n");

	printf("<H3>Important Note:</H3>\n");
	printf("Printer names marked with [*] in the Choose Printer drop-down box ");
	printf("are autoloaded printers from ");
	printf("<A HREF=\"/swat/help/smb.conf.5.html#printcapname\" target=\"docs\">Printcap Name</A>.\n");
	printf("Attempting to delete these printers from SWAT will have no effect.\n");

	if (cgi_variable("Advanced") && !cgi_variable("Basic"))
		advanced = 1;

	if (cgi_variable("Commit") && snum >= 0) {
		commit_parameters(snum);
		if (snum >= iNumNonAutoPrintServices)
		    save_reload(snum);
		else
		    save_reload(0);
	}

	if (cgi_variable("Delete") && snum >= 0) {
		lp_remove_service(snum);
		save_reload(0);
		share = NULL;
		snum = -1;
	}

	if (cgi_variable("createshare") && (share=cgi_variable("newshare"))) {
		load_config(False);
		lp_copy_service(GLOBALS_SNUM, share);
		iNumNonAutoPrintServices = lp_numservices();
		snum = lp_servicenumber(share);
		lp_do_parameter(snum, "print ok", "Yes");
		save_reload(0);
		snum = lp_servicenumber(share);
	}

	printf("<FORM name=\"swatform\" method=post>\n");

	printf("<table>\n");
	printf("<tr><td><input type=submit name=selectshare value=\"Choose Printer\"></td>\n");
	printf("<td><select name=share>\n");
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
		printf("<td><input type=submit name=\"Delete\" value=\"Delete Printer\"></td>\n");
	}
	printf("</tr>");
	printf("</table>\n");

	if (have_write_access) {
		printf("<table>\n");
		printf("<tr><td><input type=submit name=createshare value=\"Create Printer\"></td>\n");
		printf("<td><input type=text size=30 name=newshare></td></tr>\n");
		printf("</table>");
	}


	if (snum >= 0) {
		if (have_write_access) {
			printf("<input type=submit name=\"Commit\" value=\"Commit Changes\">\n");
		}
		printf("<input type=reset name=\"Reset Values\" value=\"Reset Values\">\n");
		if (advanced == 0) {
			printf("<input type=submit name=\"Advanced\" value=\"Advanced View\">\n");
		} else {
			printf("<input type=submit name=\"Basic\" value=\"Basic View\">\n");
		}
		printf("<p>\n");
	}

	if (snum >= 0) {
		printf("<table>\n");
		show_parameters(snum, 1, advanced, 1);
		printf("</table>\n");
	}

	if (advanced) {
		printf("<input type=hidden name=\"Advanced\" value=1>\n");
	}

	printf("</FORM>\n");
}

/****************************************************************************
  MAIN()
****************************************************************************/
 int main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	extern FILE *dbf;
	int opt;
	char *page;

	fault_setup(NULL);

#if defined(HAVE_SET_AUTH_PARAMETERS)
	set_auth_parameters(argc, argv);
#endif /* HAVE_SET_AUTH_PARAMETERS */

	/* just in case it goes wild ... */
	alarm(300);

	/* we don't want any SIGPIPE messages */
	BlockSignals(True,SIGPIPE);

	dbf = sys_fopen("/dev/null", "w");
	if (!dbf) dbf = stderr;

	/* we don't want stderr screwing us up */
	close(2);
	open("/dev/null", O_WRONLY);

	while ((opt = getopt(argc, argv,"s:a")) != EOF) {
		switch (opt) {
		case 's':
			pstrcpy(servicesf,optarg);
			break;	  
		case 'a':
			demo_mode = True;
			break;	  
		}
	}

	charset_initialise();
	load_config(True);
	iNumNonAutoPrintServices = lp_numservices();
#ifdef PRINTING
	load_printers();
#endif
	cgi_setup(SWATDIR, !demo_mode);

	print_header();
	
	cgi_load_variables(NULL);

	if (!file_exist(servicesf, NULL)) {
		have_read_access = True;
		have_write_access = True;
	} else {
		/* check if the authenticated user has write access - if not then
		   don't show write options */
		have_write_access = (access(servicesf,W_OK) == 0);

		/* if the user doesn't have read access to smb.conf then
		   don't let them view it */
		have_read_access = (access(servicesf,R_OK) == 0);
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
	} else {
		welcome_page();
	}

	print_footer();
	return 0;
}
