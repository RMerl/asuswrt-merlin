/*
   CIFSDD - dd for SMB.
   Main program, argument handling and block copying.

   Copyright (C) James Peach 2005-2006

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
#include "system/filesys.h"
#include "auth/gensec/gensec.h"
#include "lib/cmdline/popt_common.h"
#include "libcli/resolve/resolve.h"
#include "libcli/raw/libcliraw.h"
#include "lib/events/events.h"

#include "cifsdd.h"
#include "param/param.h"

const char * const PROGNAME = "cifsdd";

#define SYNTAX_EXIT_CODE	 1	/* Invokation syntax or logic error. */
#define EOM_EXIT_CODE		 9	/* Out of memory error. */
#define FILESYS_EXIT_CODE	10	/* File manipulation error. */
#define IOERROR_EXIT_CODE	11	/* Error during IO phase. */

struct	dd_stats_record dd_stats;

static int dd_sigint;
static int dd_sigusr1;

static void dd_handle_signal(int sig)
{
	switch (sig)
	{
		case SIGINT:
			++dd_sigint;
			break;
		case SIGUSR1:
			++dd_sigusr1;
			break;
		default:
			break;
	}
}

/* ------------------------------------------------------------------------- */
/* Argument handling.							     */
/* ------------------------------------------------------------------------- */

static const char * argtype_str(enum argtype arg_type)
{
	static const struct {
		enum argtype arg_type;
		const char * arg_name;
	} names [] = 
	{
		{ ARG_NUMERIC, "COUNT" },
		{ ARG_SIZE, "SIZE" },
		{ ARG_PATHNAME, "FILE" },
		{ ARG_BOOL, "BOOLEAN" },
	};

	int i;

	for (i = 0; i < ARRAY_SIZE(names); ++i) {
		if (arg_type == names[i].arg_type) {
			return(names[i].arg_name);
		}
	}

	return("<unknown>");
}

static struct argdef args[] =
{
	{ "bs",	ARG_SIZE,	"force ibs and obs to SIZE bytes" },
	{ "ibs", ARG_SIZE,	"read SIZE bytes at a time" },
	{ "obs", ARG_SIZE,	"write SIZE bytes at a time" },

	{ "count", ARG_NUMERIC,	"copy COUNT input blocks" },
	{ "seek",ARG_NUMERIC,	"skip COUNT blocks at start of output" },
	{ "skip",ARG_NUMERIC,	"skip COUNT blocks at start of input" },

	{ "if",	ARG_PATHNAME,	"read input from FILE" },
	{ "of",	ARG_PATHNAME,	"write output to FILE" },

	{ "direct", ARG_BOOL,	"use direct I/O if non-zero" },
	{ "sync", ARG_BOOL,	"use synchronous writes if non-zero" },
	{ "oplock", ARG_BOOL,	"take oplocks on the input and output files" },

/* FIXME: We should support using iflags and oflags for setting oplock and I/O
 * options. This would make us compatible with GNU dd.
 */
};

static struct argdef * find_named_arg(const char * arg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(args); ++i) {
		if (strwicmp(arg, args[i].arg_name) == 0) {
			return(&args[i]);
		}
	}

	return(NULL);
}

int set_arg_argv(const char * argv)
{
	struct argdef *	arg;

	char *	name;
	char *	val;

	if ((name = strdup(argv)) == NULL) {
		return(0);
	}

	if ((val = strchr(name, '=')) == NULL) {
		fprintf(stderr, "%s: malformed argument \"%s\"\n",
				PROGNAME, argv);
		goto fail;
	}

	*val = '\0';
	val++;

	if ((arg = find_named_arg(name)) == NULL) {
		fprintf(stderr, "%s: ignoring unknown argument \"%s\"\n",
				PROGNAME, name);
		goto fail;
	}

	/* Found a matching name; convert the variable argument. */
	switch (arg->arg_type) {
		case ARG_NUMERIC:
			if (!conv_str_u64(val, &arg->arg_val.nval)) {
				goto fail;
			}
			break;
		case ARG_SIZE:
			if (!conv_str_size(val, &arg->arg_val.nval)) {
				goto fail;
			}
			break;
		case ARG_BOOL:
			if (!conv_str_bool(val, &arg->arg_val.bval)) {
				goto fail;
			}
			break;
		case ARG_PATHNAME:
			if (!(arg->arg_val.pval = strdup(val))) {
				goto fail;
			}
			break;
		default:
			fprintf(stderr, "%s: argument \"%s\" is of "
				"unknown type\n", PROGNAME, name);
			goto fail;
	}

	free(name);
	return(1);

fail:
	free(name);
	return(0);
}

void set_arg_val(const char * name, ...)
{
	va_list		ap;
	struct argdef * arg;

	va_start(ap, name);
	if ((arg = find_named_arg(name)) == NULL) {
		goto fail;
	}

	/* Found a matching name; convert the variable argument. */
	switch (arg->arg_type) {
		case ARG_NUMERIC:
		case ARG_SIZE:
			arg->arg_val.nval = va_arg(ap, uint64_t);
			break;
		case ARG_BOOL:
			arg->arg_val.bval = va_arg(ap, int);
			break;
		case ARG_PATHNAME:
			arg->arg_val.pval = va_arg(ap, char *);
			if (arg->arg_val.pval) {
				arg->arg_val.pval = strdup(arg->arg_val.pval);
			}
			break;
		default:
			fprintf(stderr, "%s: argument \"%s\" is of "
				"unknown type\n", PROGNAME, name);
			goto fail;
	}

	va_end(ap);
	return;

fail:
	fprintf(stderr, "%s: ignoring unknown argument \"%s\"\n",
			PROGNAME, name);
	va_end(ap);
	return;
}

bool check_arg_bool(const char * name)
{
	struct argdef * arg;

	if ((arg = find_named_arg(name)) &&
	    (arg->arg_type == ARG_BOOL)) {
		return(arg->arg_val.bval);
	}

	DEBUG(0, ("invalid argument name: %s", name));
	SMB_ASSERT(0);
	return(false);
}

uint64_t check_arg_numeric(const char * name)
{
	struct argdef * arg;

	if ((arg = find_named_arg(name)) &&
	    (arg->arg_type == ARG_NUMERIC || arg->arg_type == ARG_SIZE)) {
		return(arg->arg_val.nval);
	}

	DEBUG(0, ("invalid argument name: %s", name));
	SMB_ASSERT(0);
	return(-1);
}

const char * check_arg_pathname(const char * name)
{
	struct argdef * arg;

	if ((arg = find_named_arg(name)) &&
	    (arg->arg_type == ARG_PATHNAME)) {
		return(arg->arg_val.pval);
	}

	DEBUG(0, ("invalid argument name: %s", name));
	SMB_ASSERT(0);
	return(NULL);
}

static void dump_args(void)
{
	int i;

	DEBUG(10, ("dumping argument values:\n"));
	for (i = 0; i < ARRAY_SIZE(args); ++i) {
		switch (args[i].arg_type) {
			case ARG_NUMERIC:
			case ARG_SIZE:
				DEBUG(10, ("\t%s=%llu\n", args[i].arg_name,
					(unsigned long long)args[i].arg_val.nval));
				break;
			case ARG_BOOL:
				DEBUG(10, ("\t%s=%s\n", args[i].arg_name,
					args[i].arg_val.bval ? "yes" : "no"));
				break;
			case ARG_PATHNAME:
				DEBUG(10, ("\t%s=%s\n", args[i].arg_name,
					args[i].arg_val.pval ?
						args[i].arg_val.pval :
						"(NULL)"));
				break;
			default:
				SMB_ASSERT(0);
		}
	}
}

static void cifsdd_help_message(poptContext pctx,
		enum poptCallbackReason preason,
		struct poptOption * poption, 
		const char * parg,
		void * pdata)
{
	static const char notes[] = 
"FILE can be a local filename or a UNC path of the form //server/share/path.\n";

	char prefix[24];
	int i;

	if (poption->shortName != '?') {
		poptPrintUsage(pctx, stdout, 0);
		fprintf(stdout, "        [dd options]\n");
		exit(0);
	}

	poptPrintHelp(pctx, stdout, 0);
	fprintf(stdout, "\nCIFS dd options:\n");

	for (i = 0; i < ARRAY_SIZE(args); ++i) {
		if (args[i].arg_name == NULL) {
			break;
		}

		snprintf(prefix, sizeof(prefix), "%s=%-*s",
			args[i].arg_name,
			(int)(sizeof(prefix) - strlen(args[i].arg_name) - 2),
			argtype_str(args[i].arg_type));
		prefix[sizeof(prefix) - 1] = '\0';
		fprintf(stdout, "  %s%s\n", prefix, args[i].arg_help);
	}

	fprintf(stdout, "\n%s\n", notes);
	exit(0);
}

/* ------------------------------------------------------------------------- */
/* Main block copying routine.						     */
/* ------------------------------------------------------------------------- */

static void print_transfer_stats(void)
{
	if (DEBUGLEVEL > 0) {
		printf("%llu+%llu records in (%llu bytes)\n"
			"%llu+%llu records out (%llu bytes)\n",
			(unsigned long long)dd_stats.in.fblocks,
			(unsigned long long)dd_stats.in.pblocks,
			(unsigned long long)dd_stats.in.bytes,
			(unsigned long long)dd_stats.out.fblocks,
			(unsigned long long)dd_stats.out.pblocks,
			(unsigned long long)dd_stats.out.bytes);
	} else {
		printf("%llu+%llu records in\n%llu+%llu records out\n",
				(unsigned long long)dd_stats.in.fblocks,
				(unsigned long long)dd_stats.in.pblocks,
				(unsigned long long)dd_stats.out.fblocks,
				(unsigned long long)dd_stats.out.pblocks);
	}
}

static struct dd_iohandle * open_file(struct resolve_context *resolve_ctx, 
				      struct tevent_context *ev,
				      const char * which, const char **ports,
				      struct smbcli_options *smb_options,
				      const char *socket_options,
				      struct smbcli_session_options *smb_session_options,
				      struct smb_iconv_convenience *iconv_convenience,
				      struct gensec_settings *gensec_settings)
{
	int			options = 0;
	const char *		path = NULL;
	struct dd_iohandle *	handle = NULL;

	if (check_arg_bool("direct")) {
		options |= DD_DIRECT_IO;
	}

	if (check_arg_bool("sync")) {
		options |= DD_SYNC_IO;
	}

	if (check_arg_bool("oplock")) {
		options |= DD_OPLOCK;
	}

	if (strcmp(which, "if") == 0) {
		path = check_arg_pathname("if");
		handle = dd_open_path(resolve_ctx, ev, path, ports,
				      check_arg_numeric("ibs"), options,
				      socket_options,
				      smb_options, smb_session_options,
				      iconv_convenience,
				      gensec_settings);
	} else if (strcmp(which, "of") == 0) {
		options |= DD_WRITE;
		path = check_arg_pathname("of");
		handle = dd_open_path(resolve_ctx, ev, path, ports,
				      check_arg_numeric("obs"), options,
				      socket_options,
				      smb_options, smb_session_options,
				      iconv_convenience,
				      gensec_settings);
	} else {
		SMB_ASSERT(0);
		return(NULL);
	}

	if (!handle) {
		fprintf(stderr, "%s: failed to open %s\n", PROGNAME, path);
	}

	return(handle);
}

static int copy_files(struct tevent_context *ev, struct loadparm_context *lp_ctx)
{
	uint8_t *	iobuf;	/* IO buffer. */
	uint64_t	iomax;	/* Size of the IO buffer. */
	uint64_t	data_size; /* Amount of data in the IO buffer. */

	uint64_t	ibs;
	uint64_t	obs;
	uint64_t	count;

	struct dd_iohandle *	ifile;
	struct dd_iohandle *	ofile;

	struct smbcli_options options;
	struct smbcli_session_options session_options;

	ibs = check_arg_numeric("ibs");
	obs = check_arg_numeric("obs");
	count = check_arg_numeric("count");

	lp_smbcli_options(lp_ctx, &options);
	lp_smbcli_session_options(lp_ctx, &session_options);

	/* Allocate IO buffer. We need more than the max IO size because we
	 * could accumulate a remainder if ibs and obs don't match.
	 */
	iomax = 2 * MAX(ibs, obs);
	if ((iobuf = malloc_array_p(uint8_t, iomax)) == NULL) {
		fprintf(stderr,
			"%s: failed to allocate IO buffer of %llu bytes\n",
			PROGNAME, (unsigned long long)iomax);
		return(EOM_EXIT_CODE);
	}

	options.max_xmit = MAX(ibs, obs);

	DEBUG(4, ("IO buffer size is %llu, max xmit is %d\n",
			(unsigned long long)iomax, options.max_xmit));

	if (!(ifile = open_file(lp_resolve_context(lp_ctx), ev, "if",
				lp_smb_ports(lp_ctx), &options,
				lp_socket_options(lp_ctx),
				&session_options, lp_iconv_convenience(lp_ctx),
				lp_gensec_settings(lp_ctx, lp_ctx)))) {
		return(FILESYS_EXIT_CODE);
	}

	if (!(ofile = open_file(lp_resolve_context(lp_ctx), ev, "of",
				lp_smb_ports(lp_ctx), &options,
				lp_socket_options(lp_ctx),
				&session_options,
				lp_iconv_convenience(lp_ctx),
				lp_gensec_settings(lp_ctx, lp_ctx)))) {
		return(FILESYS_EXIT_CODE);
	}

	/* Seek the files to their respective starting points. */
	ifile->io_seek(ifile, check_arg_numeric("skip") * ibs);
	ofile->io_seek(ofile, check_arg_numeric("seek") * obs);

	DEBUG(4, ("max xmit was negotiated to be %d\n", options.max_xmit));

	for (data_size = 0;;) {

		/* Handle signals. We are somewhat compatible with GNU dd.
		 * SIGINT makes us stop, but still print transfer statistics.
		 * SIGUSR1 makes us print transfer statistics but we continue
		 * copying.
		 */
		if (dd_sigint) {
			break;
		}

		if (dd_sigusr1) {
			print_transfer_stats();
			dd_sigusr1 = 0;
		}

		if (ifile->io_flags & DD_END_OF_FILE) {
			DEBUG(4, ("flushing %llu bytes at EOF\n",
					(unsigned long long)data_size));
			while (data_size > 0) {
				if (!dd_flush_block(ofile, iobuf,
							&data_size, obs)) {
					return(IOERROR_EXIT_CODE);
				}
			}
			goto done;
		}

		/* Try and read enough blocks of ibs bytes to be able write
		 * out one of obs bytes.
		 */
		if (!dd_fill_block(ifile, iobuf, &data_size, obs, ibs)) {
			return(IOERROR_EXIT_CODE);
		}

		if (data_size == 0) {
			/* Done. */
			SMB_ASSERT(ifile->io_flags & DD_END_OF_FILE);
		}

		/* Stop reading when we hit the block count. */
		if (dd_stats.in.bytes >= (ibs * count)) {
			ifile->io_flags |= DD_END_OF_FILE;
		}

		/* If we wanted to be a legitimate dd, we would do character
		 * conversions and other shenanigans here.
		 */

		/* Flush what we read in units of obs bytes. We want to have
		 * at least obs bytes in the IO buffer but might not if the
		 * file is too small.
		 */
		if (data_size && 
		    !dd_flush_block(ofile, iobuf, &data_size, obs)) {
			return(IOERROR_EXIT_CODE);
		}
	}

done:
	print_transfer_stats();
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Main.								     */
/* ------------------------------------------------------------------------- */

struct poptOption cifsddHelpOptions[] = {
  { NULL, '\0', POPT_ARG_CALLBACK, (void *)&cifsdd_help_message, '\0', NULL, NULL },
  { "help", '?', 0, NULL, '?', "Show this help message", NULL },
  { "usage", '\0', 0, NULL, 'u', "Display brief usage message", NULL },
  { NULL }
} ;

int main(int argc, const char ** argv)
{
	int i;
	const char ** dd_args;
	struct tevent_context *ev;

	poptContext pctx;
	struct poptOption poptions[] = {
		/* POPT_AUTOHELP */
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, cifsddHelpOptions,
			0, "Help options:", NULL },
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	/* Block sizes. */
	set_arg_val("bs", (uint64_t)4096);
	set_arg_val("ibs", (uint64_t)4096);
	set_arg_val("obs", (uint64_t)4096);
	/* Block counts. */
	set_arg_val("count", (uint64_t)-1);
	set_arg_val("seek", (uint64_t)0);
	set_arg_val("seek", (uint64_t)0);
	/* Files. */
	set_arg_val("if", NULL);
	set_arg_val("of", NULL);
	/* Options. */
	set_arg_val("direct", false);
	set_arg_val("sync", false);
	set_arg_val("oplock", false);

	pctx = poptGetContext(PROGNAME, argc, argv, poptions, 0);
	while ((i = poptGetNextOpt(pctx)) != -1) {
		;
	}

	for (dd_args = poptGetArgs(pctx); dd_args && *dd_args; ++dd_args) {

		if (!set_arg_argv(*dd_args)) {
			fprintf(stderr, "%s: invalid option: %s\n",
					PROGNAME, *dd_args);
			exit(SYNTAX_EXIT_CODE);
		}

		/* "bs" has the side-effect of setting "ibs" and "obs". */
		if (strncmp(*dd_args, "bs=", 3) == 0) {
			uint64_t bs = check_arg_numeric("bs");
			set_arg_val("ibs", bs);
			set_arg_val("obs", bs);
		}
	}

	ev = s4_event_context_init(talloc_autofree_context());

	gensec_init(cmdline_lp_ctx);
	dump_args();

	if (check_arg_numeric("ibs") == 0 || check_arg_numeric("ibs") == 0) {
		fprintf(stderr, "%s: block sizes must be greater that zero\n",
				PROGNAME);
		exit(SYNTAX_EXIT_CODE);
	}

	if (check_arg_pathname("if") == NULL) {
		fprintf(stderr, "%s: missing input filename\n", PROGNAME);
		exit(SYNTAX_EXIT_CODE);
	}

	if (check_arg_pathname("of") == NULL) {
		fprintf(stderr, "%s: missing output filename\n", PROGNAME);
		exit(SYNTAX_EXIT_CODE);
	}

	CatchSignal(SIGINT, dd_handle_signal);
	CatchSignal(SIGUSR1, dd_handle_signal);
	return(copy_files(ev, cmdline_lp_ctx));
}

/* vim: set sw=8 sts=8 ts=8 tw=79 : */
