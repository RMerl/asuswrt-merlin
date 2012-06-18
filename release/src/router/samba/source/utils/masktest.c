/* 
   Unix SMB/Netbios implementation.
   Version 2.0
   mask_match tester
   Copyright (C) Andrew Tridgell 1999
   
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

#define NO_SYSLOG

#include "includes.h"

extern int DEBUGLEVEL;
static fstring password;
static fstring username;
static fstring workgroup;
static int got_pass;

static BOOL showall = False;

static char *maskchars = "<>\"?*abc.";
static char *filechars = "abcdefghijklm.";

char *standard_masks[] = {"*", "*.", "*.*", 
			  ".*", "d2.??", "d2\">>", 
			  NULL};
char *standard_files[] = {"abc", "abc.", ".abc", 
			  "abc.def", "abc.de.f", 
			  "d2.x", 
			  NULL};


#include <regex.h>

static char *reg_test(char *pattern, char *file)
{
	static fstring ret;
	pstring rpattern;
	regex_t preg;

	pattern = 1+strrchr(pattern,'\\');
	file = 1+strrchr(file,'\\');

	fstrcpy(ret,"---");

	if (strcmp(file,"..") == 0) file = ".";
	if (strcmp(pattern,".") == 0) return ret;

	if (strcmp(pattern,"") == 0) {
		ret[2] = '+';
		return ret;
	}

	pstrcpy(rpattern,"^");
	pstrcat(rpattern, pattern);

	all_string_sub(rpattern,".", "[.]", 0);
	all_string_sub(rpattern,"?", ".{1}", 0);
	all_string_sub(rpattern,"*", ".*", 0);
	all_string_sub(rpattern+strlen(rpattern)-1,">", "([^.]?|[.]?$)", 0);
	all_string_sub(rpattern,">", "[^.]?", 0);

	all_string_sub(rpattern,"<[.]", ".*[.]", 0);
	all_string_sub(rpattern,"<\"", "(.*[.]|.*$)", 0);
	all_string_sub(rpattern,"<", "([^.]*|[^.]*[.]|[.][^.]*|[.].*[.])", 0);
	if (strlen(pattern)>1) {
		all_string_sub(rpattern+strlen(rpattern)-1,"\"", "[.]?", 0);
	}
	all_string_sub(rpattern,"\"", "([.]|$)", 0);
	pstrcat(rpattern,"$");

	/* printf("pattern=[%s] rpattern=[%s]\n", pattern, rpattern); */

	regcomp(&preg, rpattern, REG_ICASE|REG_NOSUB|REG_EXTENDED);
	if (regexec(&preg, ".", 0, NULL, 0) == 0) {
		ret[0] = '+';
		ret[1] = '+';
	}
	if (regexec(&preg, file, 0, NULL, 0) == 0) {
		ret[2] = '+';
	}
	regfree(&preg);

	return ret;
}


/***************************************************** 
return a connection to a server
*******************************************************/
struct cli_state *connect_one(char *share)
{
	struct cli_state *c;
	struct nmb_name called, calling;
	char *server_n;
	char *server;
	struct in_addr ip;
	extern struct in_addr ipzero;

	server = share+2;
	share = strchr(server,'\\');
	if (!share) return NULL;
	*share = 0;
	share++;

	server_n = server;
	
	ip = ipzero;

	make_nmb_name(&calling, "masktest", 0x0, "");
	make_nmb_name(&called , server, 0x20, "");

 again:
	ip = ipzero;

	/* have to open a new connection */
	if (!(c=cli_initialise(NULL)) || (cli_set_port(c, 139) == 0) ||
	    !cli_connect(c, server_n, &ip)) {
		DEBUG(0,("Connection to %s failed\n", server_n));
		return NULL;
	}

	if (!cli_session_request(c, &calling, &called)) {
		DEBUG(0,("session request to %s failed\n", called.name));
		cli_shutdown(c);
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20, "");
			goto again;
		}
		return NULL;
	}

	DEBUG(4,(" session request ok\n"));

	if (!cli_negprot(c)) {
		DEBUG(0,("protocol negotiation failed\n"));
		cli_shutdown(c);
		return NULL;
	}

	if (!got_pass) {
		char *pass = getpass("Password: ");
		if (pass) {
			pstrcpy(password, pass);
		}
	}

	if (!cli_session_setup(c, username, 
			       password, strlen(password),
			       password, strlen(password),
			       workgroup)) {
		DEBUG(0,("session setup failed: %s\n", cli_errstr(c)));
		return NULL;
	}

	/*
	 * These next two lines are needed to emulate
	 * old client behaviour for people who have
	 * scripts based on client output.
	 * QUESTION ? Do we want to have a 'client compatibility
	 * mode to turn these on/off ? JRA.
	 */

	if (*c->server_domain || *c->server_os || *c->server_type)
		DEBUG(1,("Domain=[%s] OS=[%s] Server=[%s]\n",
			c->server_domain,c->server_os,c->server_type));
	
	DEBUG(4,(" session setup ok\n"));

	if (!cli_send_tconX(c, share, "?????",
			    password, strlen(password)+1)) {
		DEBUG(0,("tree connect failed: %s\n", cli_errstr(c)));
		cli_shutdown(c);
		return NULL;
	}

	DEBUG(4,(" tconx ok\n"));

	return c;
}

static char *resultp;

void listfn(file_info *f, const char *s)
{
	if (strcmp(f->name,".") == 0) {
		resultp[0] = '+';
	} else if (strcmp(f->name,"..") == 0) {
		resultp[1] = '+';		
	} else {
		resultp[2] = '+';
	}
}


static void testpair(struct cli_state *cli1, struct cli_state *cli2,
		     char *mask, char *file)
{
	int fnum;
	fstring res1, res2;
	char *res3;
	static int count;

	count++;

	fstrcpy(res1, "---");
	fstrcpy(res2, "---");

	fnum = cli_open(cli1, file, O_CREAT|O_TRUNC|O_RDWR, 0);
	if (fnum == -1) {
		DEBUG(0,("Can't create %s on cli1\n", file));
		return;
	}
	cli_close(cli1, fnum);

	fnum = cli_open(cli2, file, O_CREAT|O_TRUNC|O_RDWR, 0);
	if (fnum == -1) {
		DEBUG(0,("Can't create %s on cli2\n", file));
		return;
	}
	cli_close(cli2, fnum);

	resultp = res1;
	cli_list(cli1, mask, aHIDDEN | aDIR, listfn);

	res3 = reg_test(mask, file);

	resultp = res2;
	cli_list(cli2, mask, aHIDDEN | aDIR, listfn);

	if (showall || strcmp(res1, res2)) {
		DEBUG(0,("%s %s %s %d mask=[%s] file=[%s]\n",
			 res1, res2, res3, count, mask, file));
	}

	cli_unlink(cli1, file);
	cli_unlink(cli2, file);
}

static void test_mask(int argc, char *argv[], 
		      struct cli_state *cli1, struct cli_state *cli2)
{
	pstring mask, file;
	int l1, l2, i, j, l;
	int mc_len = strlen(maskchars);
	int fc_len = strlen(filechars);

	cli_mkdir(cli1, "masktest");
	cli_mkdir(cli2, "masktest");

	cli_unlink(cli1, "\\masktest\\*");
	cli_unlink(cli2, "\\masktest\\*");

	if (argc >= 2) {
		while (argc >= 2) {
			pstrcpy(mask,"\\masktest\\");
			pstrcpy(file,"\\masktest\\");
			pstrcat(mask, argv[0]);
			pstrcat(file, argv[1]);
			testpair(cli1, cli2, mask, file);
			argv += 2;
			argc -= 2;
		}
		goto finished;
	}

	for (i=0; standard_masks[i]; i++) {
		for (j=0; standard_files[j]; j++) {
			pstrcpy(mask,"\\masktest\\");
			pstrcpy(file,"\\masktest\\");
			pstrcat(mask, standard_masks[i]);
			pstrcat(file, standard_files[j]);
			testpair(cli1, cli2, mask, file);
		}
	}

	while (1) {
		l1 = 1 + random() % 20;
		l2 = 1 + random() % 20;
		pstrcpy(mask,"\\masktest\\");
		pstrcpy(file,"\\masktest\\");
		l = strlen(mask);
		for (i=0;i<l1;i++) {
			mask[i+l] = maskchars[random() % mc_len];
		}
		mask[l+l1] = 0;

		for (i=0;i<l2;i++) {
			file[i+l] = filechars[random() % fc_len];
		}
		file[l+l2] = 0;

		if (strcmp(file+l,".") == 0 || 
		    strcmp(file+l,"..") == 0 ||
		    strcmp(mask+l,"..") == 0) continue;

		testpair(cli1, cli2, mask, file);
	}

 finished:
	cli_rmdir(cli1, "\\masktest");
	cli_rmdir(cli2, "\\masktest");
}


static void usage(void)
{
	printf(
"Usage:\n\
  masktest //server1/share1 //server2/share2 [options..]\n\
  options:\n\
        -U user%%pass\n\
        -s seed\n\
        -f filechars (default %s)\n\
        -m maskchars (default %s)\n\
        -a                             show all tests\n\
\n\
  This program tests wildcard matching between two servers. It generates\n\
  random pairs of filenames/masks and tests that they match in the same\n\
  way on two servers\n\
", 
  filechars, maskchars);
}

/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	char *share1, *share2;
	struct cli_state *cli1, *cli2;	
	extern char *optarg;
	extern int optind;
	extern FILE *dbf;
	int opt;
	char *p;
	int seed;

	setlinebuf(stdout);

	dbf = stderr;

	if (argv[1][0] == '-' || argc < 3) {
		usage();
		exit(1);
	}

	share1 = argv[1];
	share2 = argv[2];

	all_string_sub(share1,"/","\\",0);
	all_string_sub(share2,"/","\\",0);

	setup_logging(argv[0],True);

	argc -= 2;
	argv += 2;

	TimeInit();
	charset_initialise();

	if (getenv("USER")) {
		pstrcpy(username,getenv("USER"));
	}

	seed = time(NULL);

	while ((opt = getopt(argc, argv, "U:s:hm:f:a")) != EOF) {
		switch (opt) {
		case 'U':
			pstrcpy(username,optarg);
			p = strchr(username,'%');
			if (p) {
				*p = 0;
				pstrcpy(password, p+1);
				got_pass = 1;
			}
			break;
		case 's':
			seed = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(1);
		case 'm':
			maskchars = optarg;
			break;
		case 'f':
			filechars = optarg;
			break;
		case 'a':
			showall = 1;
			break;
		default:
			printf("Unknown option %c (%d)\n", (char)opt, opt);
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	DEBUG(0,("seed=%d\n", seed));
	srandom(seed);

	cli1 = connect_one(share1);
	if (!cli1) {
		DEBUG(0,("Failed to connect to %s\n", share1));
		exit(1);
	}

	cli2 = connect_one(share2);
	if (!cli2) {
		DEBUG(0,("Failed to connect to %s\n", share2));
		exit(1);
	}


	test_mask(argc, argv, cli1, cli2);

	return(0);
}
