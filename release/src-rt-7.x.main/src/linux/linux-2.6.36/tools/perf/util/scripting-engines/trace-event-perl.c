/*
 * trace-event-perl.  Feed perf trace events to an embedded Perl interpreter.
 *
 * Copyright (C) 2009 Tom Zanussi <tzanussi@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "../../perf.h"
#include "../util.h"
#include "../trace-event.h"

#include <EXTERN.h>
#include <perl.h>

void boot_Perf__Trace__Context(pTHX_ CV *cv);
void boot_DynaLoader(pTHX_ CV *cv);
typedef PerlInterpreter * INTERP;

void xs_init(pTHX);

void xs_init(pTHX)
{
	const char *file = __FILE__;
	dXSUB_SYS;

	newXS("Perf::Trace::Context::bootstrap", boot_Perf__Trace__Context,
	      file);
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

INTERP my_perl;

#define FTRACE_MAX_EVENT				\
	((1 << (sizeof(unsigned short) * 8)) - 1)

struct event *events[FTRACE_MAX_EVENT];

extern struct scripting_context *scripting_context;

static char *cur_field_name;
static int zero_flag_atom;

static void define_symbolic_value(const char *ev_name,
				  const char *field_name,
				  const char *field_value,
				  const char *field_str)
{
	unsigned long long value;
	dSP;

	value = eval_flag(field_value);

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(ev_name, 0)));
	XPUSHs(sv_2mortal(newSVpv(field_name, 0)));
	XPUSHs(sv_2mortal(newSVuv(value)));
	XPUSHs(sv_2mortal(newSVpv(field_str, 0)));

	PUTBACK;
	if (get_cv("main::define_symbolic_value", 0))
		call_pv("main::define_symbolic_value", G_SCALAR);
	SPAGAIN;
	PUTBACK;
	FREETMPS;
	LEAVE;
}

static void define_symbolic_values(struct print_flag_sym *field,
				   const char *ev_name,
				   const char *field_name)
{
	define_symbolic_value(ev_name, field_name, field->value, field->str);
	if (field->next)
		define_symbolic_values(field->next, ev_name, field_name);
}

static void define_symbolic_field(const char *ev_name,
				  const char *field_name)
{
	dSP;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(ev_name, 0)));
	XPUSHs(sv_2mortal(newSVpv(field_name, 0)));

	PUTBACK;
	if (get_cv("main::define_symbolic_field", 0))
		call_pv("main::define_symbolic_field", G_SCALAR);
	SPAGAIN;
	PUTBACK;
	FREETMPS;
	LEAVE;
}

static void define_flag_value(const char *ev_name,
			      const char *field_name,
			      const char *field_value,
			      const char *field_str)
{
	unsigned long long value;
	dSP;

	value = eval_flag(field_value);

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(ev_name, 0)));
	XPUSHs(sv_2mortal(newSVpv(field_name, 0)));
	XPUSHs(sv_2mortal(newSVuv(value)));
	XPUSHs(sv_2mortal(newSVpv(field_str, 0)));

	PUTBACK;
	if (get_cv("main::define_flag_value", 0))
		call_pv("main::define_flag_value", G_SCALAR);
	SPAGAIN;
	PUTBACK;
	FREETMPS;
	LEAVE;
}

static void define_flag_values(struct print_flag_sym *field,
			       const char *ev_name,
			       const char *field_name)
{
	define_flag_value(ev_name, field_name, field->value, field->str);
	if (field->next)
		define_flag_values(field->next, ev_name, field_name);
}

static void define_flag_field(const char *ev_name,
			      const char *field_name,
			      const char *delim)
{
	dSP;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(ev_name, 0)));
	XPUSHs(sv_2mortal(newSVpv(field_name, 0)));
	XPUSHs(sv_2mortal(newSVpv(delim, 0)));

	PUTBACK;
	if (get_cv("main::define_flag_field", 0))
		call_pv("main::define_flag_field", G_SCALAR);
	SPAGAIN;
	PUTBACK;
	FREETMPS;
	LEAVE;
}

static void define_event_symbols(struct event *event,
				 const char *ev_name,
				 struct print_arg *args)
{
	switch (args->type) {
	case PRINT_NULL:
		break;
	case PRINT_ATOM:
		define_flag_value(ev_name, cur_field_name, "0",
				  args->atom.atom);
		zero_flag_atom = 0;
		break;
	case PRINT_FIELD:
		if (cur_field_name)
			free(cur_field_name);
		cur_field_name = strdup(args->field.name);
		break;
	case PRINT_FLAGS:
		define_event_symbols(event, ev_name, args->flags.field);
		define_flag_field(ev_name, cur_field_name, args->flags.delim);
		define_flag_values(args->flags.flags, ev_name, cur_field_name);
		break;
	case PRINT_SYMBOL:
		define_event_symbols(event, ev_name, args->symbol.field);
		define_symbolic_field(ev_name, cur_field_name);
		define_symbolic_values(args->symbol.symbols, ev_name,
				       cur_field_name);
		break;
	case PRINT_STRING:
		break;
	case PRINT_TYPE:
		define_event_symbols(event, ev_name, args->typecast.item);
		break;
	case PRINT_OP:
		if (strcmp(args->op.op, ":") == 0)
			zero_flag_atom = 1;
		define_event_symbols(event, ev_name, args->op.left);
		define_event_symbols(event, ev_name, args->op.right);
		break;
	default:
		/* we should warn... */
		return;
	}

	if (args->next)
		define_event_symbols(event, ev_name, args->next);
}

static inline struct event *find_cache_event(int type)
{
	static char ev_name[256];
	struct event *event;

	if (events[type])
		return events[type];

	events[type] = event = trace_find_event(type);
	if (!event)
		return NULL;

	sprintf(ev_name, "%s::%s", event->system, event->name);

	define_event_symbols(event, ev_name, event->print_fmt.args);

	return event;
}

static void perl_process_event(int cpu, void *data,
			       int size __unused,
			       unsigned long long nsecs, char *comm)
{
	struct format_field *field;
	static char handler[256];
	unsigned long long val;
	unsigned long s, ns;
	struct event *event;
	int type;
	int pid;

	dSP;

	type = trace_parse_common_type(data);

	event = find_cache_event(type);
	if (!event)
		die("ug! no event found for type %d", type);

	pid = trace_parse_common_pid(data);

	sprintf(handler, "%s::%s", event->system, event->name);

	s = nsecs / NSECS_PER_SEC;
	ns = nsecs - s * NSECS_PER_SEC;

	scripting_context->event_data = data;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(handler, 0)));
	XPUSHs(sv_2mortal(newSViv(PTR2IV(scripting_context))));
	XPUSHs(sv_2mortal(newSVuv(cpu)));
	XPUSHs(sv_2mortal(newSVuv(s)));
	XPUSHs(sv_2mortal(newSVuv(ns)));
	XPUSHs(sv_2mortal(newSViv(pid)));
	XPUSHs(sv_2mortal(newSVpv(comm, 0)));

	/* common fields other than pid can be accessed via xsub fns */

	for (field = event->format.fields; field; field = field->next) {
		if (field->flags & FIELD_IS_STRING) {
			int offset;
			if (field->flags & FIELD_IS_DYNAMIC) {
				offset = *(int *)(data + field->offset);
				offset &= 0xffff;
			} else
				offset = field->offset;
			XPUSHs(sv_2mortal(newSVpv((char *)data + offset, 0)));
		} else { /* FIELD_IS_NUMERIC */
			val = read_size(data + field->offset, field->size);
			if (field->flags & FIELD_IS_SIGNED) {
				XPUSHs(sv_2mortal(newSViv(val)));
			} else {
				XPUSHs(sv_2mortal(newSVuv(val)));
			}
		}
	}

	PUTBACK;

	if (get_cv(handler, 0))
		call_pv(handler, G_SCALAR);
	else if (get_cv("main::trace_unhandled", 0)) {
		XPUSHs(sv_2mortal(newSVpv(handler, 0)));
		XPUSHs(sv_2mortal(newSViv(PTR2IV(scripting_context))));
		XPUSHs(sv_2mortal(newSVuv(cpu)));
		XPUSHs(sv_2mortal(newSVuv(nsecs)));
		XPUSHs(sv_2mortal(newSViv(pid)));
		XPUSHs(sv_2mortal(newSVpv(comm, 0)));
		call_pv("main::trace_unhandled", G_SCALAR);
	}
	SPAGAIN;
	PUTBACK;
	FREETMPS;
	LEAVE;
}

static void run_start_sub(void)
{
	dSP; /* access to Perl stack */
	PUSHMARK(SP);

	if (get_cv("main::trace_begin", 0))
		call_pv("main::trace_begin", G_DISCARD | G_NOARGS);
}

/*
 * Start trace script
 */
static int perl_start_script(const char *script, int argc, const char **argv)
{
	const char **command_line;
	int i, err = 0;

	command_line = malloc((argc + 2) * sizeof(const char *));
	command_line[0] = "";
	command_line[1] = script;
	for (i = 2; i < argc + 2; i++)
		command_line[i] = argv[i - 2];

	my_perl = perl_alloc();
	perl_construct(my_perl);

	if (perl_parse(my_perl, xs_init, argc + 2, (char **)command_line,
		       (char **)NULL)) {
		err = -1;
		goto error;
	}

	if (perl_run(my_perl)) {
		err = -1;
		goto error;
	}

	if (SvTRUE(ERRSV)) {
		err = -1;
		goto error;
	}

	run_start_sub();

	free(command_line);
	return 0;
error:
	perl_free(my_perl);
	free(command_line);

	return err;
}

/*
 * Stop trace script
 */
static int perl_stop_script(void)
{
	dSP; /* access to Perl stack */
	PUSHMARK(SP);

	if (get_cv("main::trace_end", 0))
		call_pv("main::trace_end", G_DISCARD | G_NOARGS);

	perl_destruct(my_perl);
	perl_free(my_perl);

	return 0;
}

static int perl_generate_script(const char *outfile)
{
	struct event *event = NULL;
	struct format_field *f;
	char fname[PATH_MAX];
	int not_first, count;
	FILE *ofp;

	sprintf(fname, "%s.pl", outfile);
	ofp = fopen(fname, "w");
	if (ofp == NULL) {
		fprintf(stderr, "couldn't open %s\n", fname);
		return -1;
	}

	fprintf(ofp, "# perf trace event handlers, "
		"generated by perf trace -g perl\n");

	fprintf(ofp, "# Licensed under the terms of the GNU GPL"
		" License version 2\n\n");

	fprintf(ofp, "# The common_* event handler fields are the most useful "
		"fields common to\n");

	fprintf(ofp, "# all events.  They don't necessarily correspond to "
		"the 'common_*' fields\n");

	fprintf(ofp, "# in the format files.  Those fields not available as "
		"handler params can\n");

	fprintf(ofp, "# be retrieved using Perl functions of the form "
		"common_*($context).\n");

	fprintf(ofp, "# See Context.pm for the list of available "
		"functions.\n\n");

	fprintf(ofp, "use lib \"$ENV{'PERF_EXEC_PATH'}/scripts/perl/"
		"Perf-Trace-Util/lib\";\n");

	fprintf(ofp, "use lib \"./Perf-Trace-Util/lib\";\n");
	fprintf(ofp, "use Perf::Trace::Core;\n");
	fprintf(ofp, "use Perf::Trace::Context;\n");
	fprintf(ofp, "use Perf::Trace::Util;\n\n");

	fprintf(ofp, "sub trace_begin\n{\n\t# optional\n}\n\n");
	fprintf(ofp, "sub trace_end\n{\n\t# optional\n}\n\n");

	while ((event = trace_find_next_event(event))) {
		fprintf(ofp, "sub %s::%s\n{\n", event->system, event->name);
		fprintf(ofp, "\tmy (");

		fprintf(ofp, "$event_name, ");
		fprintf(ofp, "$context, ");
		fprintf(ofp, "$common_cpu, ");
		fprintf(ofp, "$common_secs, ");
		fprintf(ofp, "$common_nsecs,\n");
		fprintf(ofp, "\t    $common_pid, ");
		fprintf(ofp, "$common_comm,\n\t    ");

		not_first = 0;
		count = 0;

		for (f = event->format.fields; f; f = f->next) {
			if (not_first++)
				fprintf(ofp, ", ");
			if (++count % 5 == 0)
				fprintf(ofp, "\n\t    ");

			fprintf(ofp, "$%s", f->name);
		}
		fprintf(ofp, ") = @_;\n\n");

		fprintf(ofp, "\tprint_header($event_name, $common_cpu, "
			"$common_secs, $common_nsecs,\n\t             "
			"$common_pid, $common_comm);\n\n");

		fprintf(ofp, "\tprintf(\"");

		not_first = 0;
		count = 0;

		for (f = event->format.fields; f; f = f->next) {
			if (not_first++)
				fprintf(ofp, ", ");
			if (count && count % 4 == 0) {
				fprintf(ofp, "\".\n\t       \"");
			}
			count++;

			fprintf(ofp, "%s=", f->name);
			if (f->flags & FIELD_IS_STRING ||
			    f->flags & FIELD_IS_FLAG ||
			    f->flags & FIELD_IS_SYMBOLIC)
				fprintf(ofp, "%%s");
			else if (f->flags & FIELD_IS_SIGNED)
				fprintf(ofp, "%%d");
			else
				fprintf(ofp, "%%u");
		}

		fprintf(ofp, "\\n\",\n\t       ");

		not_first = 0;
		count = 0;

		for (f = event->format.fields; f; f = f->next) {
			if (not_first++)
				fprintf(ofp, ", ");

			if (++count % 5 == 0)
				fprintf(ofp, "\n\t       ");

			if (f->flags & FIELD_IS_FLAG) {
				if ((count - 1) % 5 != 0) {
					fprintf(ofp, "\n\t       ");
					count = 4;
				}
				fprintf(ofp, "flag_str(\"");
				fprintf(ofp, "%s::%s\", ", event->system,
					event->name);
				fprintf(ofp, "\"%s\", $%s)", f->name,
					f->name);
			} else if (f->flags & FIELD_IS_SYMBOLIC) {
				if ((count - 1) % 5 != 0) {
					fprintf(ofp, "\n\t       ");
					count = 4;
				}
				fprintf(ofp, "symbol_str(\"");
				fprintf(ofp, "%s::%s\", ", event->system,
					event->name);
				fprintf(ofp, "\"%s\", $%s)", f->name,
					f->name);
			} else
				fprintf(ofp, "$%s", f->name);
		}

		fprintf(ofp, ");\n");
		fprintf(ofp, "}\n\n");
	}

	fprintf(ofp, "sub trace_unhandled\n{\n\tmy ($event_name, $context, "
		"$common_cpu, $common_secs, $common_nsecs,\n\t    "
		"$common_pid, $common_comm) = @_;\n\n");

	fprintf(ofp, "\tprint_header($event_name, $common_cpu, "
		"$common_secs, $common_nsecs,\n\t             $common_pid, "
		"$common_comm);\n}\n\n");

	fprintf(ofp, "sub print_header\n{\n"
		"\tmy ($event_name, $cpu, $secs, $nsecs, $pid, $comm) = @_;\n\n"
		"\tprintf(\"%%-20s %%5u %%05u.%%09u %%8u %%-20s \",\n\t       "
		"$event_name, $cpu, $secs, $nsecs, $pid, $comm);\n}");

	fclose(ofp);

	fprintf(stderr, "generated Perl script: %s\n", fname);

	return 0;
}

struct scripting_ops perl_scripting_ops = {
	.name = "Perl",
	.start_script = perl_start_script,
	.stop_script = perl_stop_script,
	.process_event = perl_process_event,
	.generate_script = perl_generate_script,
};
