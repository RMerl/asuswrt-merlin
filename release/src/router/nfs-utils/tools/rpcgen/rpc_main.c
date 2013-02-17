/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if 0
static char sccsid[] = "@(#)rpc_main.c 1.30 89/03/30 (C) 1987 SMI";
#endif

/*
 * rpc_main.c, Top level of the RPC protocol compiler. 
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "rpc_parse.h"
#include "rpc_util.h"
#include "rpc_scan.h"

struct commandline {
	int cflag;		/* xdr C routines */
	int hflag;		/* header file */
	int lflag;		/* client side stubs */
	int mflag;		/* server side stubs */
	int nflag;		/* netid flag */
	int sflag;		/* server stubs for the given transport */
	int tflag;		/* dispatch Table file */
	int Ssflag;		/* produce server sample code */
	int Scflag;		/* produce client sample code */
	char *infile;		/* input module name */
	char *outfile;		/* output module name */
};

static char *	extendfile(char *file, char *ext);
static void	open_output(char *infile, char *outfile);
static void	add_warning(void);
static void	clear_args(void);
static void	open_input(char *infile, char *define);
static int	check_nettype(char *name, char **list_to_check);
static void	c_output(char *infile, char *define, int extend, char *outfile);
static void	c_initialize(void);
static char *	generate_guard(char *pathname);
static void	h_output(char *infile, char *define, int extend, char *outfile);
static void	s_output(int argc, char **argv, char *infile,
			char *define, int extend, char *outfile,
			int nomain, int netflag);
static void	l_output(char *infile, char *define, int extend, char *outfile);
static void	t_output(char *infile, char *define, int extend, char *outfile);
static void	svc_output(char *, char *, int, char *);
static void	clnt_output(char *, char *, int, char *);
static int	do_registers(int argc, char **argv);
static void	addarg(char *cp);
static void	putarg(int where, char *cp);
static void	checkfiles(char *infile, char *outfile);
static int	parseargs(int argc, char **argv, struct commandline *cmd);
static void	usage(void);
static void	options_usage(void);

/*
extern void  write_sample_svc();
int write_sample_clnt();
void write_sample_clnt_main();

static svc_output();
 */

#define EXTEND	1		/* alias for TRUE */
#define DONT_EXTEND	0		/* alias for FALSE */

#define SVR4_CPP "/usr/ccs/lib/cpp"
#define SUNOS_CPP "/lib/cpp"
static int cppDefined = 0;          /* explicit path for C preprocessor */


static char *cmdname;

static char *svcclosetime = "120";
static char *CPP = SVR4_CPP;
static char CPPFLAGS[] = "-C";
static char pathbuf[MAXPATHLEN + 1];
static char *allv[] = {
	"rpcgen", "-s", "udp", "-s", "tcp",
};
static int allc = sizeof(allv)/sizeof(allv[0]);
static char *allnv[] = {
	"rpcgen", "-s", "netpath",
};
static int allnc = sizeof(allnv)/sizeof(allnv[0]);

/*
 * machinations for handling expanding argument list
 */
#if 0
static void addarg();		/* add another argument to the list */
static void putarg();		/* put argument at specified location  */
static void clear_args();	/* clear argument list */
static void checkfiles();	/* check if out file already exists */
#endif



#define ARGLISTLEN	20
#define FIXEDARGS         2

static char *arglist[ARGLISTLEN];
static int argcount = FIXEDARGS;


int nonfatalerrors;	/* errors */
int inetdflag/* = 1*/;	/* Support for inetd */ /* is now the default */
int pmflag;		/* Support for port monitors */
int logflag;		/* Use syslog instead of fprintf for errors */
int tblflag;		/* Support for dispatch table file */

/* length at which to start doing an inline */
#define INLINE 3

int Inline = INLINE;	/* length at which to start doing an inline. 3 = default
			 * if 0, no xdr_inline code */

int indefinitewait;	/* If started by port monitors, hang till it wants */
int exitnow;		/* If started by port monitors, exit after the call */
int timerflag;		/* TRUE if !indefinite && !exitnow */
int newstyle;           /* newstyle of passing arguments (by value) */
int Cflag = 0 ;         /* ANSI C syntax */
static int allfiles;    /* generate all files */
#ifdef linux
int tirpcflag = 0;	/* no tirpc by default */
#else
int tirpcflag = 1;      /* generating code for tirpc, by default */
#endif

int
main(int argc, char **argv)
{
	struct commandline cmd;

	(void) memset((char *) &cmd, 0, sizeof(struct commandline));
	clear_args();
	if (!parseargs(argc, argv, &cmd))
		usage();

	if (cmd.cflag || cmd.hflag || cmd.lflag || cmd.tflag || cmd.sflag ||
		cmd.mflag || cmd.nflag || cmd.Ssflag || cmd.Scflag) {
		checkfiles(cmd.infile, cmd.outfile);
	} else
		checkfiles(cmd.infile, NULL);

	if (cmd.cflag) {
		c_output(cmd.infile, "-DRPC_XDR", DONT_EXTEND, cmd.outfile);
	} else if (cmd.hflag) {
		h_output(cmd.infile, "-DRPC_HDR", DONT_EXTEND, cmd.outfile);
	} else if (cmd.lflag) {
		l_output(cmd.infile, "-DRPC_CLNT", DONT_EXTEND, cmd.outfile);
	} else if (cmd.sflag || cmd.mflag || (cmd.nflag)) {
		s_output(argc, argv, cmd.infile, "-DRPC_SVC", DONT_EXTEND,
			cmd.outfile, cmd.mflag, cmd.nflag);
	} else if (cmd.tflag) {
		t_output(cmd.infile, "-DRPC_TBL", DONT_EXTEND, cmd.outfile);
	} else if (cmd.Ssflag) {
		svc_output(cmd.infile, "-DRPC_SERVER", DONT_EXTEND, cmd.outfile);
	} else if (cmd.Scflag) {
		clnt_output(cmd.infile, "-DRPC_CLIENT", DONT_EXTEND, cmd.outfile);
	} else {
		/* the rescans are required, since cpp may effect input */
		c_output(cmd.infile, "-DRPC_XDR", EXTEND, "_xdr.c");
		reinitialize();
		h_output(cmd.infile, "-DRPC_HDR", EXTEND, ".h");
		reinitialize();
		l_output(cmd.infile, "-DRPC_CLNT", EXTEND, "_clnt.c");
		reinitialize();
		if (inetdflag || !tirpcflag)
			s_output(allc, allv, cmd.infile, "-DRPC_SVC", EXTEND,
				"_svc.c", cmd.mflag, cmd.nflag);
		else
			s_output(allnc, allnv, cmd.infile, "-DRPC_SVC",
				EXTEND, "_svc.c", cmd.mflag, cmd.nflag);
		if (tblflag) {
			reinitialize();
			t_output(cmd.infile, "-DRPC_TBL", EXTEND, "_tbl.i");
		}
		if (allfiles) {
			reinitialize();
			svc_output(cmd.infile, "-DRPC_SERVER", EXTEND, "_server.c");
		}
		if (allfiles) {
			reinitialize();
			clnt_output(cmd.infile, "-DRPC_CLIENT", EXTEND, "_client.c");
		}
	}
	exit(nonfatalerrors);
	/* NOTREACHED */
}

/*
 * add extension to filename 
 */
static char *
extendfile(char *file, char *ext)
{
	char *res;
	char *p;

	res = alloc(strlen(file) + strlen(ext) + 1);
	if (res == NULL) {
		abort();
	}
	p = strrchr(file, '.');
	if (p == NULL) {
		p = file + strlen(file);
	}
	(void) strcpy(res, file);
	(void) strcpy(res + (p - file), ext);
	return (res);
}

/*
 * Open output file with given extension 
 */
static void
open_output(char *infile, char *outfile)
{

	if (outfile == NULL) {
		fout = stdout;
		return;
	}

	if (infile != NULL && streq(outfile, infile)) {
		f_print(stderr, "%s: output would overwrite %s\n", cmdname,
			infile);
		crash();
	}
	fout = fopen(outfile, "w");
	if (fout == NULL) {
		f_print(stderr, "%s: unable to open ", cmdname);
		perror(outfile);
		crash();
	}
	record_open(outfile);

}

static void
add_warning(void)
{
	f_print(fout, "/*\n");
	f_print(fout, " * Please do not edit this file.\n");
	f_print(fout, " * It was generated using rpcgen.\n");
	f_print(fout, " */\n\n");
}

/* clear list of arguments */
static void
clear_args(void)
{
  int i;
  for( i=FIXEDARGS; i<ARGLISTLEN; i++ )
    arglist[i] = NULL;
  argcount = FIXEDARGS;
}

/*
 * Open input file with given define for C-preprocessor 
 */
static void
open_input(char *infile, char *define)
{
	int pd[2];

	infilename = (infile == NULL) ? "<stdin>" : infile;
	(void) pipe(pd);
	switch (fork()) {
	case 0:
		putarg(0, "cpp");
		putarg(1, CPPFLAGS);
		addarg(define);
		addarg(infile);
		addarg((char *)NULL);
		(void) close(1);
		(void) dup2(pd[1], 1);
		(void) close(pd[0]);
		if (cppDefined)
			execv(CPP, arglist);
		else {
			execvp("cpp", arglist);
			if (errno == ENOENT)
				execvp(SVR4_CPP, arglist);
			if (errno == ENOENT)
				execvp(SUNOS_CPP, arglist);
		}
		perror("execv");
		exit(1);
	case -1:
		perror("fork");
		exit(1);
	}
	(void) close(pd[1]);
	fin = fdopen(pd[0], "r");
	if (fin == NULL) {
		f_print(stderr, "%s: ", cmdname);
		perror(infilename);
		crash();
	}
}

/* valid tirpc nettypes */
static char*	valid_ti_nettypes[] =
{
	"netpath",
	"visible",
	"circuit_v",
	"datagram_v",
	"circuit_n",
	"datagram_n",
	"udp",
	"tcp",
	"raw",
	NULL
};

/* valid inetd nettypes */
static char* valid_i_nettypes[] =
{
	"udp",
	"tcp",
	NULL
};

static int
check_nettype(char *name, char **list_to_check)
{
  int i;
  for( i = 0; list_to_check[i] != NULL; i++ ) {
	  if( strcmp( name, list_to_check[i] ) == 0 ) {
	    return 1;
	  }
  }
  f_print( stderr, "illegal nettype :\'%s\'\n", name );
  return 0;
}

/*
 * Compile into an XDR routine output file
 */

static void
c_output(char *infile, char *define, int extend, char *outfile)
{
	definition *def;
	char *include;
	char *outfilename;
	long tell;

	c_initialize();
	open_input(infile, define);	
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
		/* .h file already contains rpc/rpc.h */
	} else
	  f_print(fout, "#include <rpc/rpc.h>\n");
	tell = ftell(fout);
	while ((def = get_definition()) != NULL) {
		emit(def);
	}
	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}


static void
c_initialize(void)
{

  /* add all the starting basic types */

  add_type(1,"int");
  add_type(1,"int32_t");
  add_type(1,"short");
  add_type(1,"bool");

  add_type(1,"u_int");
  add_type(1,"u_int32_t");
  add_type(1,"u_short");

}

char rpcgen_table_dcl[] = "struct rpcgen_table {\n\
	char	*(*proc)();\n\
	xdrproc_t	xdr_arg;\n\
	unsigned	len_arg;\n\
	xdrproc_t	xdr_res;\n\
	unsigned	len_res;\n\
};\n";


static char *
generate_guard(char *pathname)
{
        char* filename, *guard, *tmp;

	filename = strrchr(pathname, '/' );  /* find last component */
	filename = ((filename == 0) ? pathname : filename+1);
	guard = strdup(filename);
	/* convert to upper case */
	tmp = guard;
	while (*tmp) {
		if (islower(*tmp))
			*tmp = toupper(*tmp);
		tmp++;
	}
		
	guard = extendfile(guard, "_H_RPCGEN");
	return( guard );
}

/*
 * Compile into an XDR header file
 */
static void
h_output(char *infile, char *define, int extend, char *outfile)
{
	definition *def;
	char *outfilename;
	long tell;
	char *guard;
	list *l;

	open_input(infile, define);
	outfilename =  extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	guard = generate_guard(  outfilename ? outfilename: infile );

	f_print(fout,"#ifndef _%s\n#define _%s\n\n", guard,
		guard);

	f_print(fout, "#include <rpc/rpc.h>\n\n");

	f_print(fout, "#ifndef IXDR_GET_INT32\n");
	f_print(fout, "#define IXDR_GET_INT32(buf) IXDR_GET_LONG((buf))\n");
	f_print(fout, "#endif\n");
	f_print(fout, "#ifndef IXDR_PUT_INT32\n");
	f_print(fout, "#define IXDR_PUT_INT32(buf, v) IXDR_PUT_LONG((buf), (v))\n");
	f_print(fout, "#endif\n");
	f_print(fout, "#ifndef IXDR_GET_U_INT32\n");
	f_print(fout, "#define IXDR_GET_U_INT32(buf) IXDR_GET_U_LONG((buf))\n");
	f_print(fout, "#endif\n");
	f_print(fout, "#ifndef IXDR_PUT_U_INT32\n");
	f_print(fout, "#define IXDR_PUT_U_INT32(buf, v) IXDR_PUT_U_LONG((buf), (v))\n");
	f_print(fout, "#endif\n");

	tell = ftell(fout);
	/* print data definitions */
	while ((def = get_definition()) != NULL) {
		print_datadef(def);
	}

	/* print function declarations.  
	   Do this after data definitions because they might be used as
	   arguments for functions */
	for (l = defined; l != NULL; l = l->next) {
		print_funcdef(l->val);
	}
	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	} else if (tblflag) {
		f_print(fout, rpcgen_table_dcl);
	}
	f_print(fout, "\n#endif /* !_%s */\n", guard);
}

/*
 * Compile into an RPC service
 */
static void
s_output(int argc, char **argv, char *infile, char *define, int extend,
			char *outfile, int nomain, int netflag)
{
	char *include;
	definition *def;
	int foundprogram = 0;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
	  f_print(fout, "#include <rpc/rpc.h>\n");

	f_print(fout, "#include <stdio.h>\n");
	f_print(fout, "#include <stdlib.h>/* getenv, exit */\n"); 
	if (Cflag) {
		f_print (fout, "#include <rpc/pmap_clnt.h> /* for pmap_unset */\n");
		f_print (fout, "#include <string.h> /* strcmp */ \n"); 
	}
	if (strcmp(svcclosetime, "-1") == 0)
		indefinitewait = 1;
	else if (strcmp(svcclosetime, "0") == 0)
		exitnow = 1;
	else if (inetdflag || pmflag) {
		f_print(fout, "#include <signal.h>\n");
	  timerflag = 1;
	}

#ifndef linux
	if( !tirpcflag && inetdflag )
	  f_print(fout, "#include <sys/ttycom.h>/* TIOCNOTTY */\n");
#else
	if( !tirpcflag )
	  f_print(fout, "#include <sys/ttycom.h>/* TIOCNOTTY */\n");
#endif
	if( Cflag && (inetdflag || pmflag ) ) {
	  f_print(fout, "#ifdef __cplusplus\n");
	  f_print(fout, "#include <sysent.h> /* getdtablesize, open */\n"); 
	  f_print(fout, "#endif /* __cplusplus */\n");
	  
	  if( tirpcflag )
	    f_print(fout, "#include <unistd.h> /* setsid */\n");
	}
	if( tirpcflag )
	  f_print(fout, "#include <sys/types.h>\n");

	f_print(fout, "#include <memory.h>\n");
#ifndef linux
	f_print(fout, "#include <stropts.h>\n");
#endif
	if (inetdflag || !tirpcflag ) {
		f_print(fout, "#include <sys/socket.h>\n");
		f_print(fout, "#include <netinet/in.h>\n");
	} 

	if ( (netflag || pmflag) && tirpcflag ) {
		f_print(fout, "#include <netconfig.h>\n");
	}
	if (/*timerflag &&*/ tirpcflag)
		f_print(fout, "#include <sys/resource.h> /* rlimit */\n");
	if (logflag || inetdflag || pmflag) {
#ifdef linux
		f_print(fout, "#include <syslog.h>\n");
#else
		f_print(fout, "#ifdef SYSLOG\n");
		f_print(fout, "#include <syslog.h>\n");
		f_print(fout, "#else\n");
		f_print(fout, "#define LOG_ERR 1\n");
		f_print(fout, "#define openlog(a, b, c)\n");
		f_print(fout, "#endif\n");
#endif
	}

	/* for ANSI-C */
	f_print(fout, "\n#ifdef __STDC__\n#define SIG_PF void(*)(int)\n#endif\n");

	f_print(fout, "\n#ifdef DEBUG\n#define RPC_SVC_FG\n#endif\n");
	if (timerflag)
		f_print(fout, "\n#define _RPCSVC_CLOSEDOWN %s\n", svcclosetime);
	while ((def = get_definition()) != NULL) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_most(infile, netflag, nomain);
	if (!nomain) {
		if( !do_registers(argc, argv) ) {
		  if (outfilename)
		    (void) unlink(outfilename);
		  usage();
		}
		write_rest();
	}
}

/*
 * generate client side stubs
 */
static void
l_output(char *infile, char *define, int extend, char *outfile)
{
	char *include;
	definition *def;
	int foundprogram = 0;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	if (Cflag)
	  f_print (fout, "#include <memory.h> /* for memset */\n");
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
	  f_print(fout, "#include <rpc/rpc.h>\n");
	while ((def = get_definition()) != NULL) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_stubs();
}

/*
 * generate the dispatch table
 */
static void
t_output(char *infile, char *define, int extend, char *outfile)
{
	definition *def;
	int foundprogram = 0;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	open_output(infile, outfilename);
	add_warning();
	while ((def = get_definition()) != NULL) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		(void) unlink(outfilename);
		return;
	}
	write_tables();
}

/* sample routine for the server template */
static  void
svc_output(char *infile, char *define, int extend, char *outfile)
{
  definition *def;
  char *include;
  char *outfilename;
  long tell;
  
  open_input(infile, define);	
  outfilename = extend ? extendfile(infile, outfile) : outfile;
  checkfiles(infile,outfilename); /*check if outfile already exists.
				  if so, print an error message and exit*/
  open_output(infile, outfilename);
  add_sample_msg();

  if (infile && (include = extendfile(infile, ".h"))) {
    f_print(fout, "#include \"%s\"\n", include);
    free(include);
  } else
    f_print(fout, "#include <rpc/rpc.h>\n");

  tell = ftell(fout);
  while ((def = get_definition()) != NULL) {
	  write_sample_svc(def);
  }
  if (extend && tell == ftell(fout)) {
	  (void) unlink(outfilename);
  }
}


/* sample main routine for client */
static  void
clnt_output(char *infile, char *define, int extend, char *outfile)
{
	definition     *def;
	char           *include;
	char           *outfilename;
	long            tell;
	int             has_program = 0;

	open_input(infile, define);
	outfilename = extend ? extendfile(infile, outfile) : outfile;
	checkfiles(infile, outfilename);	/*check if outfile already exists.
				  if so, print an error message and exit*/

	open_output(infile, outfilename);
	add_sample_msg();
	if (infile && (include = extendfile(infile, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	} else
		f_print(fout, "#include <rpc/rpc.h>\n");
	tell = ftell(fout);
	while ((def = get_definition()) != NULL) {
		has_program += write_sample_clnt(def);
	}

	if (has_program)
		write_sample_clnt_main();

	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}

/*
 * Perform registrations for service output 
 * Return 0 if failed; 1 otherwise.
 */
static int
do_registers(int argc, char **argv)
{
	int             i;

	if (inetdflag || !tirpcflag) {
		for (i = 1; i < argc; i++) {
			if (streq(argv[i], "-s")) {
				if (!check_nettype(argv[i + 1], valid_i_nettypes))
					return 0;
				write_inetd_register(argv[i + 1]);
				i++;
			}
		}
	} else {
		for (i = 1; i < argc; i++)
			if (streq(argv[i], "-s")) {
				if (!check_nettype(argv[i + 1], valid_ti_nettypes))
					return 0;
				write_nettype_register(argv[i + 1]);
				i++;
			} else if (streq(argv[i], "-n")) {
				write_netid_register(argv[i + 1]);
				i++;
			}
	}
	return 1;
}

/*
 * Add another argument to the arg list
 */
static void
addarg(char *cp)
{
	if (argcount >= ARGLISTLEN) {
		f_print(stderr, "rpcgen: too many defines\n");
		crash();
		/*NOTREACHED*/
	}
	arglist[argcount++] = cp;

}

static void
putarg(int where, char *cp)
{
	if (where >= ARGLISTLEN) {
		f_print(stderr, "rpcgen: arglist coding error\n");
		crash();
		/*NOTREACHED*/
	}
	arglist[where] = cp;
	
}

/*
 * if input file is stdin and an output file is specified then complain
 * if the file already exists. Otherwise the file may get overwritten
 * If input file does not exist, exit with an error 
 */

static void
checkfiles(char *infile, char *outfile) 
{

  struct stat buf;

  if(infile)			/* infile ! = NULL */
    if(stat(infile,&buf) < 0)
      {
	perror(infile);
	crash();
      };
  if (outfile) {
    if (stat(outfile, &buf) < 0) 
      return;			/* file does not exist */
    else {
      f_print(stderr, 
	      "file '%s' already exists and may be overwritten\n", outfile);
      crash();
    }
  }
}

/*
 * Parse command line arguments 
 */
static int
parseargs(int argc, char **argv, struct commandline *cmd)
{
	int i;
	int j;
	char c;
	char flag[(1 << 8 * sizeof(char))];
	int nflags;

	cmdname = argv[0];
	cmd->infile = cmd->outfile = NULL;
	if (argc < 2) {
		return (0);
	}
	allfiles = 0;
	flag['c'] = 0;
	flag['h'] = 0;
	flag['l'] = 0;
	flag['m'] = 0;
	flag['o'] = 0;
	flag['s'] = 0;
	flag['n'] = 0;
	flag['t'] = 0;
	flag['S'] = 0;
	flag['C'] = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (cmd->infile) {
			        f_print( stderr, "Cannot specify more than one input file!\n");

				return (0);
			}
			cmd->infile = argv[i];
		} else {
			for (j = 1; argv[i][j] != 0; j++) {
				c = argv[i][j];
				switch (c) {
				case 'a':
					allfiles = 1;
					break;
				case 'c':
				case 'h':
				case 'l':
				case 'm':
				case 't':
					if (flag[(int) c]) {
						return (0);
					}
					flag[(int) c] = 1;
					break;
				case 'S':  
					/* sample flag: Ss or Sc.
					   Ss means set flag['S'];
					   Sc means set flag['C']; */
					c = argv[i][++j];  /* get next char */
					if( c == 's' )
					  c = 'S';
					else if( c == 'c' )
					  c = 'C';
					else
					  return( 0 );

					if (flag[(int) c]) {
						return (0);
					}
					flag[(int) c] = 1;
					break;
				case 'C':  /* ANSI C syntax */
					Cflag = 1;
					break;

				case 'b':  /* turn TIRPC flag off for
					    generating backward compatible
					    */
					tirpcflag = 0;
					break;

				case 'I':
					inetdflag = 1;
					break;
				case 'N':
					newstyle = 1;
					break;
				case 'L':
					logflag = 1;
					break;
				case 'K':
					if (++i == argc) {
						return (0);
					}
					svcclosetime = argv[i];
					goto nextarg;
				case 'T':
					tblflag = 1;
					break;
				case 'i' :
				  	if (++i == argc) {
						return (0);
					}
					Inline = atoi(argv[i]);
					goto nextarg;
				case 'n':
				case 'o':
				case 's':
					if (argv[i][j - 1] != '-' || 
					    argv[i][j + 1] != 0) {
						return (0);
					}
					flag[(int) c] = 1;
					if (++i == argc) {
						return (0);
					}
					if (c == 's') {
						if (!streq(argv[i], "udp") &&
						    !streq(argv[i], "tcp")) {
							return (0);
						}
					} else if (c == 'o') {
						if (cmd->outfile) {
							return (0);
						}
						cmd->outfile = argv[i];
					}
					goto nextarg;
				case 'D':
					if (argv[i][j - 1] != '-') {
						return (0);
					}
					(void) addarg(argv[i]);
					goto nextarg;
				case 'Y':
					if (++i == argc) {
						return (0);
					}
					(void) strcpy(pathbuf, argv[i]);
					(void) strcat(pathbuf, "/cpp");
					CPP = pathbuf;
					cppDefined = 1;
					goto nextarg;



				default:
					return (0);
				}
			}
	nextarg:
			;
		}
	}

	cmd->cflag = flag['c'];
	cmd->hflag = flag['h'];
	cmd->lflag = flag['l'];
	cmd->mflag = flag['m'];
	cmd->nflag = flag['n'];
	cmd->sflag = flag['s'];
	cmd->tflag = flag['t'];
	cmd->Ssflag = flag['S'];
	cmd->Scflag = flag['C'];

	if( tirpcflag ) {
	  pmflag = inetdflag ? 0 : 1;	  /* pmflag or inetdflag is always TRUE */
	  if( (inetdflag && cmd->nflag)) { /* netid not allowed with inetdflag */
	    f_print(stderr, "Cannot use netid flag with inetd flag!\n");
	    return (0);
	  }
	} else {  /* 4.1 mode */
	  pmflag = 0;               /* set pmflag only in tirpcmode */
	  inetdflag = 1;            /* inetdflag is TRUE by default */
	  if( cmd->nflag ) {          /* netid needs TIRPC */
	    f_print( stderr, "Cannot use netid flag without TIRPC!\n");
	    return( 0 );
	  }
	}

	if( newstyle && ( tblflag || cmd->tflag) ) {
	  f_print( stderr, "Cannot use table flags with newstyle!\n");
	  return( 0 );
	}

	/* check no conflicts with file generation flags */
	nflags = cmd->cflag + cmd->hflag + cmd->lflag + cmd->mflag +
		cmd->sflag + cmd->nflag + cmd->tflag + cmd->Ssflag + cmd->Scflag;

	if (nflags == 0) {
		if (cmd->outfile != NULL || cmd->infile == NULL) {
			return (0);
		}
	} else if (nflags > 1) {
	        f_print( stderr, "Cannot have more than one file generation flag!\n");
		return (0);
	}
	return (1);
}

static void
usage(void)
{
	f_print(stderr, "usage:  %s infile\n", cmdname);
	f_print(stderr, "\t%s [-a][-b][-C][-Dname[=value]] -i size  [-I [-K seconds]] [-L][-N][-T] infile\n",
			cmdname);
	f_print(stderr, "\t%s [-c | -h | -l | -m | -t | -Sc | -Ss] [-o outfile] [infile]\n",
			cmdname);
	f_print(stderr, "\t%s [-s nettype]* [-o outfile] [infile]\n", cmdname);
	f_print(stderr, "\t%s [-n netid]* [-o outfile] [infile]\n", cmdname);
	options_usage();
	exit(1);
}

static void
options_usage(void)
{
	f_print(stderr, "options:\n");
	f_print(stderr, "-a\t\tgenerate all files, including samples\n");
	f_print(stderr, "-b\t\tbackward compatibility mode (generates code for SunOS 4.1)\n");
	f_print(stderr, "-c\t\tgenerate XDR routines\n");
	f_print(stderr, "-C\t\tANSI C mode\n");
	f_print(stderr, "-Dname[=value]\tdefine a symbol (same as #define)\n");
	f_print(stderr, "-h\t\tgenerate header file\n");
	f_print(stderr, "-i size\t\tsize at which to start generating inline code\n");
	f_print(stderr, "-I\t\tgenerate code for inetd support in server (for SunOS 4.1)\n");
	f_print(stderr, "-K seconds\tserver exits after K seconds of inactivity\n");
	f_print(stderr, "-l\t\tgenerate client side stubs\n");
	f_print(stderr, "-L\t\tserver errors will be printed to syslog\n");
	f_print(stderr, "-m\t\tgenerate server side stubs\n");
	f_print(stderr, "-n netid\tgenerate server code that supports named netid\n");
	f_print(stderr, "-N\t\tsupports multiple arguments and call-by-value\n");
	f_print(stderr, "-o outfile\tname of the output file\n");
	f_print(stderr, "-s nettype\tgenerate server code that supports named nettype\n");
	f_print(stderr, "-Sc\t\tgenerate sample client code that uses remote procedures\n");
	f_print(stderr, "-Ss\t\tgenerate sample server code that defines remote procedures\n");
	f_print(stderr, "-t\t\tgenerate RPC dispatch table\n");
	f_print(stderr, "-T\t\tgenerate code to support RPC dispatch tables\n");
	f_print(stderr, "-Y path\t\tdirectory name to find C preprocessor (cpp)\n");

	exit(1);
}
