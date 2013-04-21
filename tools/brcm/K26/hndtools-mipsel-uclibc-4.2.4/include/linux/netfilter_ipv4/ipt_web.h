/*

	web (experimental)
	HTTP client match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#ifndef _IPT_WEB_H
#define _IPT_WEB_H

#include <linux/netfilter/xt_web.h>

#define IPT_WEB_MAXTEXT	XT_WEB_MAXTEXT

#define IPT_WEB_HTTP	XT_WEB_HTTP
#define IPT_WEB_RURI	XT_WEB_RURI
#define IPT_WEB_PATH	XT_WEB_PATH
#define IPT_WEB_QUERY	XT_WEB_QUERY
#define IPT_WEB_HOST	XT_WEB_HOST
#define IPT_WEB_HORE	XT_WEB_HORE

#define ipt_web_info xt_web_info

#endif
