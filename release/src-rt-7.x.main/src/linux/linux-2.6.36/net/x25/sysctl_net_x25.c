/* -*- linux-c -*-
 * sysctl_net_x25.c: sysctl interface to net X.25 subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/x25 directory entry (empty =) ). [MS]
 */

#include <linux/sysctl.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <net/x25.h>

static int min_timer[] = {   1 * HZ };
static int max_timer[] = { 300 * HZ };

static struct ctl_table_header *x25_table_header;

static struct ctl_table x25_table[] = {
	{
		.procname =	"restart_request_timeout",
		.data =		&sysctl_x25_restart_request_timeout,
		.maxlen =	sizeof(int),
		.mode =		0644,
		.proc_handler =	proc_dointvec_minmax,
		.extra1 =	&min_timer,
		.extra2 =	&max_timer,
	},
	{
		.procname =	"call_request_timeout",
		.data =		&sysctl_x25_call_request_timeout,
		.maxlen =	sizeof(int),
		.mode =		0644,
		.proc_handler =	proc_dointvec_minmax,
		.extra1 =	&min_timer,
		.extra2 =	&max_timer,
	},
	{
		.procname =	"reset_request_timeout",
		.data =		&sysctl_x25_reset_request_timeout,
		.maxlen =	sizeof(int),
		.mode =		0644,
		.proc_handler =	proc_dointvec_minmax,
		.extra1 =	&min_timer,
		.extra2 =	&max_timer,
	},
	{
		.procname =	"clear_request_timeout",
		.data =		&sysctl_x25_clear_request_timeout,
		.maxlen =	sizeof(int),
		.mode =		0644,
		.proc_handler =	proc_dointvec_minmax,
		.extra1 =	&min_timer,
		.extra2 =	&max_timer,
	},
	{
		.procname =	"acknowledgement_hold_back_timeout",
		.data =		&sysctl_x25_ack_holdback_timeout,
		.maxlen =	sizeof(int),
		.mode =		0644,
		.proc_handler =	proc_dointvec_minmax,
		.extra1 =	&min_timer,
		.extra2 =	&max_timer,
	},
	{
		.procname =	"x25_forward",
		.data = 	&sysctl_x25_forward,
		.maxlen = 	sizeof(int),
		.mode = 	0644,
		.proc_handler = proc_dointvec,
	},
	{ 0, },
};

static struct ctl_path x25_path[] = {
	{ .procname = "net", },
	{ .procname = "x25", },
	{ }
};

void __init x25_register_sysctl(void)
{
	x25_table_header = register_sysctl_paths(x25_path, x25_table);
}

void x25_unregister_sysctl(void)
{
	unregister_sysctl_table(x25_table_header);
}
