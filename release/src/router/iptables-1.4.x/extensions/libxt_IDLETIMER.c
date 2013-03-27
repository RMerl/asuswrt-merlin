/*
 * Shared library add-on for iptables to add IDLETIMER support.
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_IDLETIMER.h>

enum {
	O_TIMEOUT = 0,
	O_LABEL,
};

#define s struct idletimer_tg_info
static const struct xt_option_entry idletimer_tg_opts[] = {
	{.name = "timeout", .id = O_TIMEOUT, .type = XTTYPE_UINT32,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, timeout)},
	{.name = "label", .id = O_LABEL, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, label)},
	XTOPT_TABLEEND,
};
#undef s

static void idletimer_tg_help(void)
{
	printf(
"IDLETIMER target options:\n"
" --timeout time	Timeout until the notification is sent (in seconds)\n"
" --label string	Unique rule identifier\n"
"\n");
}

static void idletimer_tg_print(const void *ip,
			       const struct xt_entry_target *target,
			       int numeric)
{
	struct idletimer_tg_info *info =
		(struct idletimer_tg_info *) target->data;

	printf(" timeout:%u", info->timeout);
	printf(" label:%s", info->label);
}

static void idletimer_tg_save(const void *ip,
			      const struct xt_entry_target *target)
{
	struct idletimer_tg_info *info =
		(struct idletimer_tg_info *) target->data;

	printf(" --timeout %u", info->timeout);
	printf(" --label %s", info->label);
}

static struct xtables_target idletimer_tg_reg = {
	.family	       = NFPROTO_UNSPEC,
	.name	       = "IDLETIMER",
	.version       = XTABLES_VERSION,
	.revision      = 0,
	.size	       = XT_ALIGN(sizeof(struct idletimer_tg_info)),
	.userspacesize = offsetof(struct idletimer_tg_info, timer),
	.help	       = idletimer_tg_help,
	.x6_parse      = xtables_option_parse,
	.print	       = idletimer_tg_print,
	.save	       = idletimer_tg_save,
	.x6_options    = idletimer_tg_opts,
};

void _init(void)
{
	xtables_register_target(&idletimer_tg_reg);
}
