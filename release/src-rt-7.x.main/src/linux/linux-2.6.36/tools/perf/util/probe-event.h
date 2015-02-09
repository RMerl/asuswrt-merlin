#ifndef _PROBE_EVENT_H
#define _PROBE_EVENT_H

#include <stdbool.h>
#include "strlist.h"

extern bool probe_event_dry_run;

/* kprobe-tracer tracing point */
struct probe_trace_point {
	char		*symbol;	/* Base symbol */
	unsigned long	offset;		/* Offset from symbol */
	bool		retprobe;	/* Return probe flag */
};

/* probe-tracer tracing argument referencing offset */
struct probe_trace_arg_ref {
	struct probe_trace_arg_ref	*next;	/* Next reference */
	long				offset;	/* Offset value */
};

/* kprobe-tracer tracing argument */
struct probe_trace_arg {
	char				*name;	/* Argument name */
	char				*value;	/* Base value */
	char				*type;	/* Type name */
	struct probe_trace_arg_ref	*ref;	/* Referencing offset */
};

/* kprobe-tracer tracing event (point + arg) */
struct probe_trace_event {
	char				*event;	/* Event name */
	char				*group;	/* Group name */
	struct probe_trace_point	point;	/* Trace point */
	int				nargs;	/* Number of args */
	struct probe_trace_arg		*args;	/* Arguments */
};

/* Perf probe probing point */
struct perf_probe_point {
	char		*file;		/* File path */
	char		*function;	/* Function name */
	int		line;		/* Line number */
	bool		retprobe;	/* Return probe flag */
	char		*lazy_line;	/* Lazy matching pattern */
	unsigned long	offset;		/* Offset from function entry */
};

/* Perf probe probing argument field chain */
struct perf_probe_arg_field {
	struct perf_probe_arg_field	*next;	/* Next field */
	char				*name;	/* Name of the field */
	long				index;	/* Array index number */
	bool				ref;	/* Referencing flag */
};

/* Perf probe probing argument */
struct perf_probe_arg {
	char				*name;	/* Argument name */
	char				*var;	/* Variable name */
	char				*type;	/* Type name */
	struct perf_probe_arg_field	*field;	/* Structure fields */
};

/* Perf probe probing event (point + arg) */
struct perf_probe_event {
	char			*event;	/* Event name */
	char			*group;	/* Group name */
	struct perf_probe_point	point;	/* Probe point */
	int			nargs;	/* Number of arguments */
	struct perf_probe_arg	*args;	/* Arguments */
};


/* Line number container */
struct line_node {
	struct list_head	list;
	int			line;
};

/* Line range */
struct line_range {
	char			*file;		/* File name */
	char			*function;	/* Function name */
	int			start;		/* Start line number */
	int			end;		/* End line number */
	int			offset;		/* Start line offset */
	char			*path;		/* Real path name */
	char			*comp_dir;	/* Compile directory */
	struct list_head	line_list;	/* Visible lines */
};

/* Command string to events */
extern int parse_perf_probe_command(const char *cmd,
				    struct perf_probe_event *pev);

/* Events to command string */
extern char *synthesize_perf_probe_command(struct perf_probe_event *pev);
extern char *synthesize_probe_trace_command(struct probe_trace_event *tev);
extern int synthesize_perf_probe_arg(struct perf_probe_arg *pa, char *buf,
				     size_t len);

/* Check the perf_probe_event needs debuginfo */
extern bool perf_probe_event_need_dwarf(struct perf_probe_event *pev);

/* Release event contents */
extern void clear_perf_probe_event(struct perf_probe_event *pev);

/* Command string to line-range */
extern int parse_line_range_desc(const char *cmd, struct line_range *lr);


extern int add_perf_probe_events(struct perf_probe_event *pevs, int npevs,
				 bool force_add, int max_probe_points);
extern int del_perf_probe_events(struct strlist *dellist);
extern int show_perf_probe_events(void);
extern int show_line_range(struct line_range *lr);


/* Maximum index number of event-name postfix */
#define MAX_EVENT_INDEX	1024

#endif /*_PROBE_EVENT_H */
