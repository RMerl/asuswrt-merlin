/*
 * Unix SMB/Netbios implementation. Version 1.9. smbpasswd module. Copyright
 * (C) Jeremy Allison 1995-1998
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

extern pstring global_myname;
extern int DEBUGLEVEL;

/*
 * Next two lines needed for SunOS and don't
 * hurt anything else...
 */
extern char *optarg;
extern int optind;

/*********************************************************
a strdup with exit
**********************************************************/
static char *xstrdup(char *s)
{
	s = strdup(s);
	if (!s) {
		fprintf(stderr,"out of memory\n");
		exit(1);
	}
	return s;
}


/*********************************************************
 Print command usage on stderr and die.
**********************************************************/
static void usage(void)
{
	if (getuid() == 0) {
		printf("smbpasswd [options] [username] [password]\n");
	} else {
		printf("smbpasswd [options] [password]\n");
	}
	printf("options:\n");
	printf("  -s                   use stdin for password prompt\n");
	printf("  -D LEVEL             debug level\n");
	printf("  -U USER              remote username\n");
	printf("  -r MACHINE           remote machine\n");

	if (getuid() == 0) {
		printf("  -R ORDER             name resolve order\n");
		printf("  -j DOMAIN            join domain name\n");
		printf("  -a                   add user\n");
		printf("  -x                   delete user\n");
		printf("  -d                   disable user\n");
		printf("  -e                   enable user\n");
		printf("  -n                   set no password\n");
		printf("  -m                   machine trust account\n");
	}
	exit(1);
}
#ifdef RPCCLIENT
/*********************************************************
Join a domain.
**********************************************************/
static int join_domain(char *domain, char *remote)
{
	pstring remote_machine;
	fstring trust_passwd;
	unsigned char orig_trust_passwd_hash[16];
	BOOL ret;

	pstrcpy(remote_machine, remote ? remote : "");
	fstrcpy(trust_passwd, global_myname);
	strlower(trust_passwd);
	E_md4hash( (uchar *)trust_passwd, orig_trust_passwd_hash);

	/* Ensure that we are not trying to join a
	   domain if we are locally set up as a domain
	   controller. */

	if(strequal(remote, global_myname)) {
		fprintf(stderr, "Cannot join domain %s as the domain controller name is our own. We cannot be a domain controller for a domain and also be a domain member.\n", domain);
		return 1;
	}

	/*
	 * Create the machine account password file.
	 */
	if(!trust_password_lock( domain, global_myname, True)) {
		fprintf(stderr, "Unable to open the machine account password file for \
machine %s in domain %s.\n", global_myname, domain); 
		return 1;
	}

	/*
	 * Write the old machine account password.
	 */
	
	if(!set_trust_account_password( orig_trust_passwd_hash)) {              
		fprintf(stderr, "Unable to write the machine account password for \
machine %s in domain %s.\n", global_myname, domain);
		trust_password_unlock();
		return 1;
	}
	
	/*
	 * If we are given a remote machine assume this is the PDC.
	 */
	
	if(remote == NULL) {
		pstrcpy(remote_machine, lp_passwordserver());
	}

	if(!*remote_machine) {
		fprintf(stderr, "No password server list given in smb.conf - \
unable to join domain.\n");
		trust_password_unlock();
		return 1;
	}

	ret = change_trust_account_password( domain, remote_machine);
	trust_password_unlock();
	
	if(!ret) {
		trust_password_delete( domain, global_myname);
		fprintf(stderr,"Unable to join domain %s.\n",domain);
	} else {
		printf("Joined domain %s.\n",domain);
	}
	
	return (int)ret;
}
#endif

static void set_line_buffering(FILE *f)
{
	setvbuf(f, NULL, _IOLBF, 0);
}

/*************************************************************
 Utility function to prompt for passwords from stdin. Each
 password entered must end with a newline.
*************************************************************/
static char *stdin_new_passwd(void)
{
	static fstring new_passwd;
	size_t len;

	ZERO_ARRAY(new_passwd);

	/*
	 * if no error is reported from fgets() and string at least contains
	 * the newline that ends the password, then replace the newline with
	 * a null terminator.
	 */
	if ( fgets(new_passwd, sizeof(new_passwd), stdin) != NULL) {
		if ((len = strlen(new_passwd)) > 0) {
			if(new_passwd[len-1] == '\n')
				new_passwd[len - 1] = 0; 
		}
	}
	return(new_passwd);
}


/*************************************************************
 Utility function to get passwords via tty or stdin
 Used if the '-s' option is set to silently get passwords
 to enable scripting.
*************************************************************/
static char *get_pass( char *prompt, BOOL stdin_get)
{
	char *p;
	if (stdin_get) {
		p = stdin_new_passwd();
	} else {
		p = getpass(prompt);
	}
	return xstrdup(p);
}

/*************************************************************
 Utility function to prompt for new password.
*************************************************************/
static char *prompt_for_new_password(BOOL stdin_get)
{
	char *p;
	fstring new_passwd;

	ZERO_ARRAY(new_passwd);
 
	p = get_pass("New SMB password:", stdin_get);

	fstrcpy(new_passwd, p);

	p = get_pass("Retype new SMB password:", stdin_get);

	if (strcmp(p, new_passwd)) {
		fprintf(stderr, "Mismatch - password unchanged.\n");
		ZERO_ARRAY(new_passwd);
		return NULL;
	}

	return xstrdup(p);
}


/*************************************************************
 Change a password either locally or remotely.
*************************************************************/

static BOOL password_change(const char *remote_machine, char *user_name, 
			    char *old_passwd, char *new_passwd, int local_flags)
{
	BOOL ret;
	pstring err_str;
	pstring msg_str;

	if (remote_machine != NULL) {
		if (local_flags & (LOCAL_ADD_USER|LOCAL_DELETE_USER|LOCAL_DISABLE_USER|LOCAL_ENABLE_USER|
							LOCAL_TRUST_ACCOUNT|LOCAL_SET_NO_PASSWORD)) {
			/* these things can't be done remotely yet */
			return False;
		}
		ret = remote_password_change(remote_machine, user_name, 
									 old_passwd, new_passwd, err_str, sizeof(err_str));
		if(*err_str)
			fprintf(stderr, err_str);
		return ret;
	}
	
	ret = local_password_change(user_name, local_flags, new_passwd, 
				     err_str, sizeof(err_str), msg_str, sizeof(msg_str));

	if(*msg_str)
		printf(msg_str);
	if(*err_str)
		fprintf(stderr, err_str);

	return ret;
}


/*************************************************************
 Handle password changing for root.
*************************************************************/

static int process_root(int argc, char *argv[])
{
	struct passwd  *pwd;
	int ch;
	BOOL joining_domain = False;
	int local_flags = 0;
	BOOL stdin_passwd_get = False;
	char *user_name = NULL;
	char *new_domain = NULL;
	char *new_passwd = NULL;
	char *old_passwd = NULL;
	char *remote_machine = NULL;

	while ((ch = getopt(argc, argv, "ax:d:e:mnj:r:sR:D:U:")) != EOF) {
		switch(ch) {
		case 'a':
			local_flags |= LOCAL_ADD_USER;
			break;
		case 'x':
			local_flags |= LOCAL_DELETE_USER;
			user_name = optarg;
			new_passwd = "XXXXXX";
			break;
		case 'd':
			local_flags |= LOCAL_DISABLE_USER;
			user_name = optarg;
			new_passwd = "XXXXXX";
			break;
		case 'e':
			local_flags |= LOCAL_ENABLE_USER;
			user_name = optarg;
			break;
		case 'm':
			local_flags |= LOCAL_TRUST_ACCOUNT;
			break;
		case 'n':
			local_flags |= LOCAL_SET_NO_PASSWORD;
			new_passwd = "NO PASSWORD";
			break;
		case 'j':
			new_domain = optarg;
			strupper(new_domain);
			joining_domain = True;
			break;
		case 'r':
			remote_machine = optarg;
			break;
		case 's':
			set_line_buffering(stdin);
			set_line_buffering(stdout);
			set_line_buffering(stderr);
			stdin_passwd_get = True;
			break;
		case 'R':
			lp_set_name_resolve_order(optarg);
			break;
		case 'D':
			DEBUGLEVEL = atoi(optarg);
			break;
		case 'U':
			user_name = optarg;
			break;
		default:
			usage();
		}
	}
	
	argc -= optind;
	argv += optind;

	/*
	 * Ensure add/delete user and either remote machine or join domain are
	 * not both set.
	 */	
	if((local_flags & (LOCAL_ADD_USER|LOCAL_DELETE_USER)) && ((remote_machine != NULL) || joining_domain)) {
		usage();
	}
#ifdef RPCCLIENT	
	if(joining_domain) {
		if (argc != 0)
			usage();
		return join_domain(new_domain, remote_machine);
	}
#endif
	/*
	 * Deal with root - can add a user, but only locally.
	 */

	switch(argc) {
	case 0:
		break;
	case 1:
		user_name = argv[0];
		break;
	case 2:
		user_name = argv[0];
		new_passwd = argv[1];
		break;
	default:
		usage();
	}

	if (!user_name && (pwd = sys_getpwuid(0))) {
		user_name = xstrdup(pwd->pw_name);
	} 

	if (!user_name) {
		fprintf(stderr,"You must specify a username\n");
		exit(1);
	}

	if (local_flags & LOCAL_TRUST_ACCOUNT) {
		/* add the $ automatically */
		static fstring buf;

		/*
		 * Remove any trailing '$' before we
		 * generate the initial machine password.
		 */

		if (user_name[strlen(user_name)-1] == '$') {
			user_name[strlen(user_name)-1] = 0;
		}

		if (local_flags & LOCAL_ADD_USER) {
			new_passwd = xstrdup(user_name);
			strlower(new_passwd);
		}

		/*
		 * Now ensure the username ends in '$' for
		 * the machine add.
		 */

		slprintf(buf, sizeof(buf)-1, "%s$", user_name);
		user_name = buf;
	}

	if (remote_machine != NULL) {
		old_passwd = get_pass("Old SMB password:",stdin_passwd_get);
	}
	
	if (!new_passwd) {

		/*
		 * If we are trying to enable a user, first we need to find out
		 * if they are using a modern version of the smbpasswd file that
		 * disables a user by just writing a flag into the file. If so
		 * then we can re-enable a user without prompting for a new
		 * password. If not (ie. they have a no stored password in the
		 * smbpasswd file) then we need to prompt for a new password.
		 */

		if(local_flags & LOCAL_ENABLE_USER) {
			struct smb_passwd *smb_pass = getsmbpwnam(user_name);
			if((smb_pass != NULL) && (smb_pass->smb_passwd != NULL)) {
				new_passwd = "XXXX"; /* Don't care. */
			}
		}

		if(!new_passwd)
			new_passwd = prompt_for_new_password(stdin_passwd_get);

		if(!new_passwd) {
			fprintf(stderr, "Unable to get new password.\n");
			exit(1);
		}
	}
	
	if (!password_change(remote_machine, user_name, old_passwd, new_passwd, local_flags)) {
		fprintf(stderr,"Failed to modify password entry for user %s\n", user_name);
		return 1;
	} 

	if(!(local_flags & (LOCAL_ADD_USER|LOCAL_DISABLE_USER|LOCAL_ENABLE_USER|LOCAL_DELETE_USER|LOCAL_SET_NO_PASSWORD))) {
		struct smb_passwd *smb_pass = getsmbpwnam(user_name);
		printf("Password changed for user %s.", user_name );
		if((smb_pass != NULL) && (smb_pass->acct_ctrl & ACB_DISABLED ))
			printf(" User has disabled flag set.");
		if((smb_pass != NULL) && (smb_pass->acct_ctrl & ACB_PWNOTREQ))
			printf(" User has no password flag set.");
		printf("\n");
	}
	return 0;
}


/*************************************************************
handle password changing for non-root
*************************************************************/
static int process_nonroot(int argc, char *argv[])
{
	struct passwd  *pwd = NULL;
	int ch;
	BOOL stdin_passwd_get = False;
	char *old_passwd = NULL;
	char *remote_machine = NULL;
	char *user_name = NULL;
	char *new_passwd = NULL;

	while ((ch = getopt(argc, argv, "hD:r:sU:")) != EOF) {
		switch(ch) {
		case 'D':
			DEBUGLEVEL = atoi(optarg);
			break;
		case 'r':
			remote_machine = optarg;
			break;
		case 's':
			set_line_buffering(stdin);
			set_line_buffering(stdout);
			set_line_buffering(stderr);
			stdin_passwd_get = True;
			break;
		case 'U':
			user_name = optarg;
			break;
		default:
			usage();
		}
	}
	
	argc -= optind;
	argv += optind;

	if(argc > 1) {
		usage();
	}
	
	if (argc == 1) {
		new_passwd = argv[0];
	}
	
	if (!user_name) {
		pwd = sys_getpwuid(getuid());
		if (pwd) {
			user_name = xstrdup(pwd->pw_name);
		} else {
			fprintf(stderr,"you don't exist - go away\n");
			exit(1);
		}
	}
	
	/*
	 * A non-root user is always setting a password
	 * via a remote machine (even if that machine is
	 * localhost).
	 */	
	if (remote_machine == NULL) {
		remote_machine = "127.0.0.1";
	}


	if (remote_machine != NULL) {
		old_passwd = get_pass("Old SMB password:",stdin_passwd_get);
	}
	
	if (!new_passwd) {
		new_passwd = prompt_for_new_password(stdin_passwd_get);
	}
	
	if (!new_passwd) {
		fprintf(stderr, "Unable to get new password.\n");
		exit(1);
	}

	if (!password_change(remote_machine, user_name, old_passwd, new_passwd, 0)) {
		fprintf(stderr,"Failed to change password for %s\n", user_name);
		return 1;
	}

	printf("Password changed for user %s\n", user_name);
	return 0;
}



/*********************************************************
 Start here.
**********************************************************/
int main(int argc, char **argv)
{	
	static pstring servicesf = CONFIGFILE;

#if defined(HAVE_SET_AUTH_PARAMETERS)
	set_auth_parameters(argc, argv);
#endif /* HAVE_SET_AUTH_PARAMETERS */

	TimeInit();
	
	setup_logging("smbpasswd", True);
	
	charset_initialise();
	
	if(!initialize_password_db()) {
		fprintf(stderr, "Can't setup password database vectors.\n");
		exit(1);
	}

	if (!lp_load(servicesf,True,False,False)) {
		fprintf(stderr, "Can't load %s - run testparm to debug it\n", 
			servicesf);
		exit(1);
	}

	/*
	 * Set the machine NETBIOS name if not already
	 * set from the config file. 
	 */ 
    
	if (!*global_myname) {   
		char *p;
		fstrcpy(global_myname, myhostname());
		p = strchr(global_myname, '.' );
		if (p) *p = 0;
	}           
	strupper(global_myname);

	codepage_initialise(lp_client_code_page());

	load_interfaces();

	/* Check the effective uid - make sure we are not setuid */
	if ((geteuid() == (uid_t)0) && (getuid() != (uid_t)0)) {
		fprintf(stderr, "smbpasswd must *NOT* be setuid root.\n");
		exit(1);
	}

	if (getuid() == 0) {
		return process_root(argc, argv);
	} 

	return process_nonroot(argc, argv);
}
