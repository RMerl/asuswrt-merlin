/*
 *
 * Function graph tracer.
 * Copyright (c) 2008-2009 Frederic Weisbecker <fweisbec@gmail.com>
 * Mostly borrowed from function tracer which
 * is Copyright (c) Steven Rostedt <srostedt@redhat.com>
 *
 */
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "trace.h"
#include "trace_output.h"

struct fgraph_cpu_data {
	pid_t		last_pid;
	int		depth;
	int		ignore;
	unsigned long	enter_funcs[FTRACE_RETFUNC_DEPTH];
};

struct fgraph_data {
	struct fgraph_cpu_data		*cpu_data;

	/* Place to preserve last processed entry. */
	struct ftrace_graph_ent_entry	ent;
	struct ftrace_graph_ret_entry	ret;
	int				failed;
	int				cpu;
};

#define TRACE_GRAPH_INDENT	2

/* Flag options */
#define TRACE_GRAPH_PRINT_OVERRUN	0x1
#define TRACE_GRAPH_PRINT_CPU		0x2
#define TRACE_GRAPH_PRINT_OVERHEAD	0x4
#define TRACE_GRAPH_PRINT_PROC		0x8
#define TRACE_GRAPH_PRINT_DURATION	0x10
#define TRACE_GRAPH_PRINT_ABS_TIME	0x20

static struct tracer_opt trace_opts[] = {
	/* Display overruns? (for self-debug purpose) */
	{ TRACER_OPT(funcgraph-overrun, TRACE_GRAPH_PRINT_OVERRUN) },
	/* Display CPU ? */
	{ TRACER_OPT(funcgraph-cpu, TRACE_GRAPH_PRINT_CPU) },
	/* Display Overhead ? */
	{ TRACER_OPT(funcgraph-overhead, TRACE_GRAPH_PRINT_OVERHEAD) },
	/* Display proc name/pid */
	{ TRACER_OPT(funcgraph-proc, TRACE_GRAPH_PRINT_PROC) },
	/* Display duration of execution */
	{ TRACER_OPT(funcgraph-duration, TRACE_GRAPH_PRINT_DURATION) },
	/* Display absolute time of an entry */
	{ TRACER_OPT(funcgraph-abstime, TRACE_GRAPH_PRINT_ABS_TIME) },
	{ } /* Empty entry */
};

static struct tracer_flags tracer_flags = {
	/* Don't display overruns and proc by default */
	.val = TRACE_GRAPH_PRINT_CPU | TRACE_GRAPH_PRINT_OVERHEAD |
	       TRACE_GRAPH_PRINT_DURATION,
	.opts = trace_opts
};

static struct trace_array *graph_array;


/* Add a function return address to the trace stack on thread info.*/
int
ftrace_push_return_trace(unsigned long ret, unsigned long func, int *depth,
			 unsigned long frame_pointer)
{
	unsigned long long calltime;
	int index;

	if (!current->ret_stack)
		return -EBUSY;

	/*
	 * We must make sure the ret_stack is tested before we read
	 * anything else.
	 */
	smp_rmb();

	/* The return trace stack is full */
	if (current->curr_ret_stack == FTRACE_RETFUNC_DEPTH - 1) {
		atomic_inc(&current->trace_overrun);
		return -EBUSY;
	}

	calltime = trace_clock_local();

	index = ++current->curr_ret_stack;
	barrier();
	current->ret_stack[index].ret = ret;
	current->ret_stack[index].func = func;
	current->ret_stack[index].calltime = calltime;
	current->ret_stack[index].subtime = 0;
	current->ret_stack[index].fp = frame_pointer;
	*depth = index;

	return 0;
}

/* Retrieve a function return address to the trace stack on thread info.*/
static void
ftrace_pop_return_trace(struct ftrace_graph_ret *trace, unsigned long *ret,
			unsigned long frame_pointer)
{
	int index;

	index = current->curr_ret_stack;

	if (unlikely(index < 0)) {
		ftrace_graph_stop();
		WARN_ON(1);
		/* Might as well panic, otherwise we have no where to go */
		*ret = (unsigned long)panic;
		return;
	}

#ifdef CONFIG_HAVE_FUNCTION_GRAPH_FP_TEST
	/*
	 * The arch may choose to record the frame pointer used
	 * and check it here to make sure that it is what we expect it
	 * to be. If gcc does not set the place holder of the return
	 * address in the frame pointer, and does a copy instead, then
	 * the function graph trace will fail. This test detects this
	 * case.
	 *
	 * Currently, x86_32 with optimize for size (-Os) makes the latest
	 * gcc do the above.
	 */
	if (unlikely(current->ret_stack[index].fp != frame_pointer)) {
		ftrace_graph_stop();
		WARN(1, "Bad frame pointer: expected %lx, received %lx\n"
		     "  from func %ps return to %lx\n",
		     current->ret_stack[index].fp,
		     frame_pointer,
		     (void *)current->ret_stack[index].func,
		     current->ret_stack[index].ret);
		*ret = (unsigned long)panic;
		return;
	}
#endif

	*ret = current->ret_stack[index].ret;
	trace->func = current->ret_stack[index].func;
	trace->calltime = current->ret_stack[index].calltime;
	trace->overrun = atomic_read(&current->trace_overrun);
	trace->depth = index;
}

/*
 * Send the trace to the ring-buffer.
 * @return the original return address.
 */
unsigned long ftrace_return_to_handler(unsigned long frame_pointer)
{
	struct ftrace_graph_ret trace;
	unsigned long ret;

	ftrace_pop_return_trace(&trace, &ret, frame_pointer);
	trace.rettime = trace_clock_local();
	ftrace_graph_return(&trace);
	barrier();
	current->curr_ret_stack--;

	if (unlikely(!ret)) {
		ftrace_graph_stop();
		WARN_ON(1);
		/* Might as well panic. What else to do? */
		ret = (unsigned long)panic;
	}

	return ret;
}

int __trace_graph_entry(struct trace_array *tr,
				struct ftrace_graph_ent *trace,
				unsigned long flags,
				int pc)
{
	struct ftrace_event_call *call = &event_funcgraph_entry;
	struct ring_buffer_event *event;
	struct ring_buffer *buffer = tr->buffer;
	struct ftrace_graph_ent_entry *entry;

	if (unlikely(__this_cpu_read(ftrace_cpu_disabled)))
		return 0;

	event = trace_buffer_lock_reserve(buffer, TRACE_GRAPH_ENT,
					  sizeof(*entry), flags, pc);
	if (!event)
		return 0;
	entry	= ring_buffer_event_data(event);
	entry->graph_ent			= *trace;
	if (!filter_current_check_discard(buffer, call, entry, event))
		ring_buffer_unlock_commit(buffer, event);

	return 1;
}

int trace_graph_entry(struct ftrace_graph_ent *trace)
{
	struct trace_array *tr = graph_array;
	struct trace_array_cpu *data;
	unsigned long flags;
	long disabled;
	int ret;
	int cpu;
	int pc;

	if (!ftrace_trace_task(current))
		return 0;

	/* trace it when it is-nested-in or is a function enabled. */
	if (!(trace->depth || ftrace_graph_addr(trace->func)))
		return 0;

	local_irq_save(flags);
	cpu = raw_smp_processor_id();
	data = tr->data[cpu];
	disabled = atomic_inc_return(&data->disabled);
	if (likely(disabled == 1)) {
		pc = preempt_count();
		ret = __trace_graph_entry(tr, trace, flags, pc);
	} else {
		ret = 0;
	}

	atomic_dec(&data->disabled);
	local_irq_restore(flags);

	return ret;
}

int trace_graph_thresh_entry(struct ftrace_graph_ent *trace)
{
	if (tracing_thresh)
		return 1;
	else
		return trace_graph_entry(trace);
}

void __trace_graph_return(struct trace_array *tr,
				struct ftrace_graph_ret *trace,
				unsigned long flags,
				int pc)
{
	struct ftrace_event_call *call = &event_funcgraph_exit;
	struct ring_buffer_event *event;
	struct ring_buffer *buffer = tr->buffer;
	struct ftrace_graph_ret_entry *entry;

	if (unlikely(__this_cpu_read(ftrace_cpu_disabled)))
		return;

	event = trace_buffer_lock_reserve(buffer, TRACE_GRAPH_RET,
					  sizeof(*entry), flags, pc);
	if (!event)
		return;
	entry	= ring_buffer_event_data(event);
	entry->ret				= *trace;
	if (!filter_current_check_discard(buffer, call, entry, event))
		ring_buffer_unlock_commit(buffer, event);
}

void trace_graph_return(struct ftrace_graph_ret *trace)
{
	struct trace_array *tr = graph_array;
	struct trace_array_cpu *data;
	unsigned long flags;
	long disabled;
	int cpu;
	int pc;

	local_irq_save(flags);
	cpu = raw_smp_processor_id();
	data = tr->data[cpu];
	disabled = atomic_inc_return(&data->disabled);
	if (likely(disabled == 1)) {
		pc = preempt_count();
		__trace_graph_return(tr, trace, flags, pc);
	}
	atomic_dec(&data->disabled);
	local_irq_restore(flags);
}

void set_graph_array(struct trace_array *tr)
{
	graph_array = tr;

	/* Make graph_array visible before we start tracing */

	smp_mb();
}

void trace_graph_thresh_return(struct ftrace_graph_ret *trace)
{
	if (tracing_thresh &&
	    (trace->rettime - trace->calltime < tracing_thresh))
		return;
	else
		trace_graph_return(trace);
}

static int graph_trace_init(struct trace_array *tr)
{
	int ret;

	set_graph_array(tr);
	if (tracing_thresh)
		ret = register_ftrace_graph(&trace_graph_thresh_return,
					    &trace_graph_thresh_entry);
	else
		ret = register_ftrace_graph(&trace_graph_return,
					    &trace_graph_entry);
	if (ret)
		return ret;
	tracing_start_cmdline_record();

	return 0;
}

static void graph_trace_reset(struct trace_array *tr)
{
	tracing_stop_cmdline_record();
	unregister_ftrace_graph();
}

static int max_bytes_for_cpu;

static enum print_line_t
print_graph_cpu(struct trace_seq *s, int cpu)
{
	int ret;

	/*
	 * Start with a space character - to make it stand out
	 * to the right a bit when trace output is pasted into
	 * email:
	 */
	ret = trace_seq_printf(s, " %*d) ", max_bytes_for_cpu, cpu);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}

#define TRACE_GRAPH_PROCINFO_LENGTH	14

static enum print_line_t
print_graph_proc(struct trace_seq *s, pid_t pid)
{
	char comm[TASK_COMM_LEN];
	/* sign + log10(MAX_INT) + '\0' */
	char pid_str[11];
	int spaces = 0;
	int ret;
	int len;
	int i;

	trace_find_cmdline(pid, comm);
	comm[7] = '\0';
	sprintf(pid_str, "%d", pid);

	/* 1 stands for the "-" character */
	len = strlen(comm) + strlen(pid_str) + 1;

	if (len < TRACE_GRAPH_PROCINFO_LENGTH)
		spaces = TRACE_GRAPH_PROCINFO_LENGTH - len;

	/* First spaces to align center */
	for (i = 0; i < spaces / 2; i++) {
		ret = trace_seq_printf(s, " ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	ret = trace_seq_printf(s, "%s-%s", comm, pid_str);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* Last spaces to align center */
	for (i = 0; i < spaces - (spaces / 2); i++) {
		ret = trace_seq_printf(s, " ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}
	return TRACE_TYPE_HANDLED;
}


static enum print_line_t
print_graph_lat_fmt(struct trace_seq *s, struct trace_entry *entry)
{
	if (!trace_seq_putc(s, ' '))
		return 0;

	return trace_print_lat_fmt(s, entry);
}

/* If the pid changed since the last trace, output this event */
static enum print_line_t
verif_pid(struct trace_seq *s, pid_t pid, int cpu, struct fgraph_data *data)
{
	pid_t prev_pid;
	pid_t *last_pid;
	int ret;

	if (!data)
		return TRACE_TYPE_HANDLED;

	last_pid = &(per_cpu_ptr(data->cpu_data, cpu)->last_pid);

	if (*last_pid == pid)
		return TRACE_TYPE_HANDLED;

	prev_pid = *last_pid;
	*last_pid = pid;

	if (prev_pid == -1)
		return TRACE_TYPE_HANDLED;
/*
 * Context-switch trace line:

 ------------------------------------------
 | 1)  migration/0--1  =>  sshd-1755
 ------------------------------------------

 */
	ret = trace_seq_printf(s,
		" ------------------------------------------\n");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	ret = print_graph_cpu(s, cpu);
	if (ret == TRACE_TYPE_PARTIAL_LINE)
		return TRACE_TYPE_PARTIAL_LINE;

	ret = print_graph_proc(s, prev_pid);
	if (ret == TRACE_TYPE_PARTIAL_LINE)
		return TRACE_TYPE_PARTIAL_LINE;

	ret = trace_seq_printf(s, " => ");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	ret = print_graph_proc(s, pid);
	if (ret == TRACE_TYPE_PARTIAL_LINE)
		return TRACE_TYPE_PARTIAL_LINE;

	ret = trace_seq_printf(s,
		"\n ------------------------------------------\n\n");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}

static struct ftrace_graph_ret_entry *
get_return_for_leaf(struct trace_iterator *iter,
		struct ftrace_graph_ent_entry *curr)
{
	struct fgraph_data *data = iter->private;
	struct ring_buffer_iter *ring_iter = NULL;
	struct ring_buffer_event *event;
	struct ftrace_graph_ret_entry *next;

	/*
	 * If the previous output failed to write to the seq buffer,
	 * then we just reuse the data from before.
	 */
	if (data && data->failed) {
		curr = &data->ent;
		next = &data->ret;
	} else {

		ring_iter = iter->buffer_iter[iter->cpu];

		/* First peek to compare current entry and the next one */
		if (ring_iter)
			event = ring_buffer_iter_peek(ring_iter, NULL);
		else {
			/*
			 * We need to consume the current entry to see
			 * the next one.
			 */
			ring_buffer_consume(iter->tr->buffer, iter->cpu,
					    NULL, NULL);
			event = ring_buffer_peek(iter->tr->buffer, iter->cpu,
						 NULL, NULL);
		}

		if (!event)
			return NULL;

		next = ring_buffer_event_data(event);

		if (data) {
			/*
			 * Save current and next entries for later reference
			 * if the output fails.
			 */
			data->ent = *curr;
			/*
			 * If the next event is not a return type, then
			 * we only care about what type it is. Otherwise we can
			 * safely copy the entire event.
			 */
			if (next->ent.type == TRACE_GRAPH_RET)
				data->ret = *next;
			else
				data->ret.ent.type = next->ent.type;
		}
	}

	if (next->ent.type != TRACE_GRAPH_RET)
		return NULL;

	if (curr->ent.pid != next->ent.pid ||
			curr->graph_ent.func != next->ret.func)
		return NULL;

	/* this is a leaf, now advance the iterator */
	if (ring_iter)
		ring_buffer_read(ring_iter, NULL);

	return next;
}

/* Signal a overhead of time execution to the output */
static int
print_graph_overhead(unsigned long long duration, struct trace_seq *s,
		     u32 flags)
{
	/* If duration disappear, we don't need anything */
	if (!(flags & TRACE_GRAPH_PRINT_DURATION))
		return 1;

	/* Non nested entry or return */
	if (duration == -1)
		return trace_seq_printf(s, "  ");

	if (flags & TRACE_GRAPH_PRINT_OVERHEAD) {
		/* Duration exceeded 100 msecs */
		if (duration > 100000ULL)
			return trace_seq_printf(s, "! ");

		/* Duration exceeded 10 msecs */
		if (duration > 10000ULL)
			return trace_seq_printf(s, "+ ");
	}

	return trace_seq_printf(s, "  ");
}

static int print_graph_abs_time(u64 t, struct trace_seq *s)
{
	unsigned long usecs_rem;

	usecs_rem = do_div(t, NSEC_PER_SEC);
	usecs_rem /= 1000;

	return trace_seq_printf(s, "%5lu.%06lu |  ",
			(unsigned long)t, usecs_rem);
}

static enum print_line_t
print_graph_irq(struct trace_iterator *iter, unsigned long addr,
		enum trace_type type, int cpu, pid_t pid, u32 flags)
{
	int ret;
	struct trace_seq *s = &iter->seq;

	if (addr < (unsigned long)__irqentry_text_start ||
		addr >= (unsigned long)__irqentry_text_end)
		return TRACE_TYPE_UNHANDLED;

	/* Absolute time */
	if (flags & TRACE_GRAPH_PRINT_ABS_TIME) {
		ret = print_graph_abs_time(iter->ts, s);
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Cpu */
	if (flags & TRACE_GRAPH_PRINT_CPU) {
		ret = print_graph_cpu(s, cpu);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Proc */
	if (flags & TRACE_GRAPH_PRINT_PROC) {
		ret = print_graph_proc(s, pid);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
		ret = trace_seq_printf(s, " | ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* No overhead */
	ret = print_graph_overhead(-1, s, flags);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	if (type == TRACE_GRAPH_ENT)
		ret = trace_seq_printf(s, "==========>");
	else
		ret = trace_seq_printf(s, "<==========");

	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* Don't close the duration column if haven't one */
	if (flags & TRACE_GRAPH_PRINT_DURATION)
		trace_seq_printf(s, " |");
	ret = trace_seq_printf(s, "\n");

	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;
	return TRACE_TYPE_HANDLED;
}

enum print_line_t
trace_print_graph_duration(unsigned long long duration, struct trace_seq *s)
{
	unsigned long nsecs_rem = do_div(duration, 1000);
	/* log10(ULONG_MAX) + '\0' */
	char msecs_str[21];
	char nsecs_str[5];
	int ret, len;
	int i;

	sprintf(msecs_str, "%lu", (unsigned long) duration);

	/* Print msecs */
	ret = trace_seq_printf(s, "%s", msecs_str);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	len = strlen(msecs_str);

	/* Print nsecs (we don't want to exceed 7 numbers) */
	if (len < 7) {
		snprintf(nsecs_str, min(sizeof(nsecs_str), 8UL - len), "%03lu",
			 nsecs_rem);
		ret = trace_seq_printf(s, ".%s", nsecs_str);
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
		len += strlen(nsecs_str);
	}

	ret = trace_seq_printf(s, " us ");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* Print remaining spaces to fit the row's width */
	for (i = len; i < 7; i++) {
		ret = trace_seq_printf(s, " ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}
	return TRACE_TYPE_HANDLED;
}

static enum print_line_t
print_graph_duration(unsigned long long duration, struct trace_seq *s)
{
	int ret;

	ret = trace_print_graph_duration(duration, s);
	if (ret != TRACE_TYPE_HANDLED)
		return ret;

	ret = trace_seq_printf(s, "|  ");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}

/* Case of a leaf function on its call entry */
static enum print_line_t
print_graph_entry_leaf(struct trace_iterator *iter,
		struct ftrace_graph_ent_entry *entry,
		struct ftrace_graph_ret_entry *ret_entry,
		struct trace_seq *s, u32 flags)
{
	struct fgraph_data *data = iter->private;
	struct ftrace_graph_ret *graph_ret;
	struct ftrace_graph_ent *call;
	unsigned long long duration;
	int ret;
	int i;

	graph_ret = &ret_entry->ret;
	call = &entry->graph_ent;
	duration = graph_ret->rettime - graph_ret->calltime;

	if (data) {
		struct fgraph_cpu_data *cpu_data;
		int cpu = iter->cpu;

		cpu_data = per_cpu_ptr(data->cpu_data, cpu);

		/*
		 * Comments display at + 1 to depth. Since
		 * this is a leaf function, keep the comments
		 * equal to this depth.
		 */
		cpu_data->depth = call->depth - 1;

		/* No need to keep this function around for this depth */
		if (call->depth < FTRACE_RETFUNC_DEPTH)
			cpu_data->enter_funcs[call->depth] = 0;
	}

	/* Overhead */
	ret = print_graph_overhead(duration, s, flags);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* Duration */
	if (flags & TRACE_GRAPH_PRINT_DURATION) {
		ret = print_graph_duration(duration, s);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Function */
	for (i = 0; i < call->depth * TRACE_GRAPH_INDENT; i++) {
		ret = trace_seq_printf(s, " ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	ret = trace_seq_printf(s, "%ps();\n", (void *)call->func);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}

static enum print_line_t
print_graph_entry_nested(struct trace_iterator *iter,
			 struct ftrace_graph_ent_entry *entry,
			 struct trace_seq *s, int cpu, u32 flags)
{
	struct ftrace_graph_ent *call = &entry->graph_ent;
	struct fgraph_data *data = iter->private;
	int ret;
	int i;

	if (data) {
		struct fgraph_cpu_data *cpu_data;
		int cpu = iter->cpu;

		cpu_data = per_cpu_ptr(data->cpu_data, cpu);
		cpu_data->depth = call->depth;

		/* Save this function pointer to see if the exit matches */
		if (call->depth < FTRACE_RETFUNC_DEPTH)
			cpu_data->enter_funcs[call->depth] = call->func;
	}

	/* No overhead */
	ret = print_graph_overhead(-1, s, flags);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* No time */
	if (flags & TRACE_GRAPH_PRINT_DURATION) {
		ret = trace_seq_printf(s, "            |  ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Function */
	for (i = 0; i < call->depth * TRACE_GRAPH_INDENT; i++) {
		ret = trace_seq_printf(s, " ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	ret = trace_seq_printf(s, "%ps() {\n", (void *)call->func);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/*
	 * we already consumed the current entry to check the next one
	 * and see if this is a leaf.
	 */
	return TRACE_TYPE_NO_CONSUME;
}

static enum print_line_t
print_graph_prologue(struct trace_iterator *iter, struct trace_seq *s,
		     int type, unsigned long addr, u32 flags)
{
	struct fgraph_data *data = iter->private;
	struct trace_entry *ent = iter->ent;
	int cpu = iter->cpu;
	int ret;

	/* Pid */
	if (verif_pid(s, ent->pid, cpu, data) == TRACE_TYPE_PARTIAL_LINE)
		return TRACE_TYPE_PARTIAL_LINE;

	if (type) {
		/* Interrupt */
		ret = print_graph_irq(iter, addr, type, cpu, ent->pid, flags);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Absolute time */
	if (flags & TRACE_GRAPH_PRINT_ABS_TIME) {
		ret = print_graph_abs_time(iter->ts, s);
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Cpu */
	if (flags & TRACE_GRAPH_PRINT_CPU) {
		ret = print_graph_cpu(s, cpu);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Proc */
	if (flags & TRACE_GRAPH_PRINT_PROC) {
		ret = print_graph_proc(s, ent->pid);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;

		ret = trace_seq_printf(s, " | ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Latency format */
	if (trace_flags & TRACE_ITER_LATENCY_FMT) {
		ret = print_graph_lat_fmt(s, ent);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	return 0;
}

static enum print_line_t
print_graph_entry(struct ftrace_graph_ent_entry *field, struct trace_seq *s,
			struct trace_iterator *iter, u32 flags)
{
	struct fgraph_data *data = iter->private;
	struct ftrace_graph_ent *call = &field->graph_ent;
	struct ftrace_graph_ret_entry *leaf_ret;
	static enum print_line_t ret;
	int cpu = iter->cpu;

	if (print_graph_prologue(iter, s, TRACE_GRAPH_ENT, call->func, flags))
		return TRACE_TYPE_PARTIAL_LINE;

	leaf_ret = get_return_for_leaf(iter, field);
	if (leaf_ret)
		ret = print_graph_entry_leaf(iter, field, leaf_ret, s, flags);
	else
		ret = print_graph_entry_nested(iter, field, s, cpu, flags);

	if (data) {
		/*
		 * If we failed to write our output, then we need to make
		 * note of it. Because we already consumed our entry.
		 */
		if (s->full) {
			data->failed = 1;
			data->cpu = cpu;
		} else
			data->failed = 0;
	}

	return ret;
}

static enum print_line_t
print_graph_return(struct ftrace_graph_ret *trace, struct trace_seq *s,
		   struct trace_entry *ent, struct trace_iterator *iter,
		   u32 flags)
{
	unsigned long long duration = trace->rettime - trace->calltime;
	struct fgraph_data *data = iter->private;
	pid_t pid = ent->pid;
	int cpu = iter->cpu;
	int func_match = 1;
	int ret;
	int i;

	if (data) {
		struct fgraph_cpu_data *cpu_data;
		int cpu = iter->cpu;

		cpu_data = per_cpu_ptr(data->cpu_data, cpu);

		/*
		 * Comments display at + 1 to depth. This is the
		 * return from a function, we now want the comments
		 * to display at the same level of the bracket.
		 */
		cpu_data->depth = trace->depth - 1;

		if (trace->depth < FTRACE_RETFUNC_DEPTH) {
			if (cpu_data->enter_funcs[trace->depth] != trace->func)
				func_match = 0;
			cpu_data->enter_funcs[trace->depth] = 0;
		}
	}

	if (print_graph_prologue(iter, s, 0, 0, flags))
		return TRACE_TYPE_PARTIAL_LINE;

	/* Overhead */
	ret = print_graph_overhead(duration, s, flags);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* Duration */
	if (flags & TRACE_GRAPH_PRINT_DURATION) {
		ret = print_graph_duration(duration, s);
		if (ret == TRACE_TYPE_PARTIAL_LINE)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Closing brace */
	for (i = 0; i < trace->depth * TRACE_GRAPH_INDENT; i++) {
		ret = trace_seq_printf(s, " ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/*
	 * If the return function does not have a matching entry,
	 * then the entry was lost. Instead of just printing
	 * the '}' and letting the user guess what function this
	 * belongs to, write out the function name.
	 */
	if (func_match) {
		ret = trace_seq_printf(s, "}\n");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	} else {
		ret = trace_seq_printf(s, "} /* %ps */\n", (void *)trace->func);
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Overrun */
	if (flags & TRACE_GRAPH_PRINT_OVERRUN) {
		ret = trace_seq_printf(s, " (Overruns: %lu)\n",
					trace->overrun);
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	ret = print_graph_irq(iter, trace->func, TRACE_GRAPH_RET,
			      cpu, pid, flags);
	if (ret == TRACE_TYPE_PARTIAL_LINE)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}

static enum print_line_t
print_graph_comment(struct trace_seq *s, struct trace_entry *ent,
		    struct trace_iterator *iter, u32 flags)
{
	unsigned long sym_flags = (trace_flags & TRACE_ITER_SYM_MASK);
	struct fgraph_data *data = iter->private;
	struct trace_event *event;
	int depth = 0;
	int ret;
	int i;

	if (data)
		depth = per_cpu_ptr(data->cpu_data, iter->cpu)->depth;

	if (print_graph_prologue(iter, s, 0, 0, flags))
		return TRACE_TYPE_PARTIAL_LINE;

	/* No overhead */
	ret = print_graph_overhead(-1, s, flags);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	/* No time */
	if (flags & TRACE_GRAPH_PRINT_DURATION) {
		ret = trace_seq_printf(s, "            |  ");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

	/* Indentation */
	if (depth > 0)
		for (i = 0; i < (depth + 1) * TRACE_GRAPH_INDENT; i++) {
			ret = trace_seq_printf(s, " ");
			if (!ret)
				return TRACE_TYPE_PARTIAL_LINE;
		}

	/* The comment */
	ret = trace_seq_printf(s, "/* ");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	switch (iter->ent->type) {
	case TRACE_BPRINT:
		ret = trace_print_bprintk_msg_only(iter);
		if (ret != TRACE_TYPE_HANDLED)
			return ret;
		break;
	case TRACE_PRINT:
		ret = trace_print_printk_msg_only(iter);
		if (ret != TRACE_TYPE_HANDLED)
			return ret;
		break;
	default:
		event = ftrace_find_event(ent->type);
		if (!event)
			return TRACE_TYPE_UNHANDLED;

		ret = event->funcs->trace(iter, sym_flags, event);
		if (ret != TRACE_TYPE_HANDLED)
			return ret;
	}

	/* Strip ending newline */
	if (s->buffer[s->len - 1] == '\n') {
		s->buffer[s->len - 1] = '\0';
		s->len--;
	}

	ret = trace_seq_printf(s, " */\n");
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}


enum print_line_t
print_graph_function_flags(struct trace_iterator *iter, u32 flags)
{
	struct ftrace_graph_ent_entry *field;
	struct fgraph_data *data = iter->private;
	struct trace_entry *entry = iter->ent;
	struct trace_seq *s = &iter->seq;
	int cpu = iter->cpu;
	int ret;

	if (data && per_cpu_ptr(data->cpu_data, cpu)->ignore) {
		per_cpu_ptr(data->cpu_data, cpu)->ignore = 0;
		return TRACE_TYPE_HANDLED;
	}

	/*
	 * If the last output failed, there's a possibility we need
	 * to print out the missing entry which would never go out.
	 */
	if (data && data->failed) {
		field = &data->ent;
		iter->cpu = data->cpu;
		ret = print_graph_entry(field, s, iter, flags);
		if (ret == TRACE_TYPE_HANDLED && iter->cpu != cpu) {
			per_cpu_ptr(data->cpu_data, iter->cpu)->ignore = 1;
			ret = TRACE_TYPE_NO_CONSUME;
		}
		iter->cpu = cpu;
		return ret;
	}

	switch (entry->type) {
	case TRACE_GRAPH_ENT: {
		/*
		 * print_graph_entry() may consume the current event,
		 * thus @field may become invalid, so we need to save it.
		 * sizeof(struct ftrace_graph_ent_entry) is very small,
		 * it can be safely saved at the stack.
		 */
		struct ftrace_graph_ent_entry saved;
		trace_assign_type(field, entry);
		saved = *field;
		return print_graph_entry(&saved, s, iter, flags);
	}
	case TRACE_GRAPH_RET: {
		struct ftrace_graph_ret_entry *field;
		trace_assign_type(field, entry);
		return print_graph_return(&field->ret, s, entry, iter, flags);
	}
	case TRACE_STACK:
	case TRACE_FN:
		/* dont trace stack and functions as comments */
		return TRACE_TYPE_UNHANDLED;

	default:
		return print_graph_comment(s, entry, iter, flags);
	}

	return TRACE_TYPE_HANDLED;
}

static enum print_line_t
print_graph_function(struct trace_iterator *iter)
{
	return print_graph_function_flags(iter, tracer_flags.val);
}

static enum print_line_t
print_graph_function_event(struct trace_iterator *iter, int flags,
			   struct trace_event *event)
{
	return print_graph_function(iter);
}

static void print_lat_header(struct seq_file *s, u32 flags)
{
	static const char spaces[] = "                "	/* 16 spaces */
		"    "					/* 4 spaces */
		"                 ";			/* 17 spaces */
	int size = 0;

	if (flags & TRACE_GRAPH_PRINT_ABS_TIME)
		size += 16;
	if (flags & TRACE_GRAPH_PRINT_CPU)
		size += 4;
	if (flags & TRACE_GRAPH_PRINT_PROC)
		size += 17;

	seq_printf(s, "#%.*s  _-----=> irqs-off        \n", size, spaces);
	seq_printf(s, "#%.*s / _----=> need-resched    \n", size, spaces);
	seq_printf(s, "#%.*s| / _---=> hardirq/softirq \n", size, spaces);
	seq_printf(s, "#%.*s|| / _--=> preempt-depth   \n", size, spaces);
	seq_printf(s, "#%.*s||| / _-=> lock-depth      \n", size, spaces);
	seq_printf(s, "#%.*s|||| /                     \n", size, spaces);
}

void print_graph_headers_flags(struct seq_file *s, u32 flags)
{
	int lat = trace_flags & TRACE_ITER_LATENCY_FMT;

	if (lat)
		print_lat_header(s, flags);

	/* 1st line */
	seq_printf(s, "#");
	if (flags & TRACE_GRAPH_PRINT_ABS_TIME)
		seq_printf(s, "     TIME       ");
	if (flags & TRACE_GRAPH_PRINT_CPU)
		seq_printf(s, " CPU");
	if (flags & TRACE_GRAPH_PRINT_PROC)
		seq_printf(s, "  TASK/PID       ");
	if (lat)
		seq_printf(s, "|||||");
	if (flags & TRACE_GRAPH_PRINT_DURATION)
		seq_printf(s, "  DURATION   ");
	seq_printf(s, "               FUNCTION CALLS\n");

	/* 2nd line */
	seq_printf(s, "#");
	if (flags & TRACE_GRAPH_PRINT_ABS_TIME)
		seq_printf(s, "      |         ");
	if (flags & TRACE_GRAPH_PRINT_CPU)
		seq_printf(s, " |  ");
	if (flags & TRACE_GRAPH_PRINT_PROC)
		seq_printf(s, "   |    |        ");
	if (lat)
		seq_printf(s, "|||||");
	if (flags & TRACE_GRAPH_PRINT_DURATION)
		seq_printf(s, "   |   |      ");
	seq_printf(s, "               |   |   |   |\n");
}

void print_graph_headers(struct seq_file *s)
{
	print_graph_headers_flags(s, tracer_flags.val);
}

void graph_trace_open(struct trace_iterator *iter)
{
	/* pid and depth on the last trace processed */
	struct fgraph_data *data;
	int cpu;

	iter->private = NULL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		goto out_err;

	data->cpu_data = alloc_percpu(struct fgraph_cpu_data);
	if (!data->cpu_data)
		goto out_err_free;

	for_each_possible_cpu(cpu) {
		pid_t *pid = &(per_cpu_ptr(data->cpu_data, cpu)->last_pid);
		int *depth = &(per_cpu_ptr(data->cpu_data, cpu)->depth);
		int *ignore = &(per_cpu_ptr(data->cpu_data, cpu)->ignore);
		*pid = -1;
		*depth = 0;
		*ignore = 0;
	}

	iter->private = data;

	return;

 out_err_free:
	kfree(data);
 out_err:
	pr_warning("function graph tracer: not enough memory\n");
}

void graph_trace_close(struct trace_iterator *iter)
{
	struct fgraph_data *data = iter->private;

	if (data) {
		free_percpu(data->cpu_data);
		kfree(data);
	}
}

static struct trace_event_functions graph_functions = {
	.trace		= print_graph_function_event,
};

static struct trace_event graph_trace_entry_event = {
	.type		= TRACE_GRAPH_ENT,
	.funcs		= &graph_functions,
};

static struct trace_event graph_trace_ret_event = {
	.type		= TRACE_GRAPH_RET,
	.funcs		= &graph_functions
};

static struct tracer graph_trace __read_mostly = {
	.name		= "function_graph",
	.open		= graph_trace_open,
	.pipe_open	= graph_trace_open,
	.close		= graph_trace_close,
	.pipe_close	= graph_trace_close,
	.wait_pipe	= poll_wait_pipe,
	.init		= graph_trace_init,
	.reset		= graph_trace_reset,
	.print_line	= print_graph_function,
	.print_header	= print_graph_headers,
	.flags		= &tracer_flags,
#ifdef CONFIG_FTRACE_SELFTEST
	.selftest	= trace_selftest_startup_function_graph,
#endif
};

static __init int init_graph_trace(void)
{
	max_bytes_for_cpu = snprintf(NULL, 0, "%d", nr_cpu_ids - 1);

	if (!register_ftrace_event(&graph_trace_entry_event)) {
		pr_warning("Warning: could not register graph trace events\n");
		return 1;
	}

	if (!register_ftrace_event(&graph_trace_ret_event)) {
		pr_warning("Warning: could not register graph trace events\n");
		return 1;
	}

	return register_tracer(&graph_trace);
}

device_initcall(init_graph_trace);
