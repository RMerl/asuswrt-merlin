/*

	web (experimental)
	HTTP client match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#ifndef _IPT_WEB_H
#define _IPT_WEB_H

#define IPT_WEB_MAXTEXT	512

typedef enum {
	IPT_WEB_HTTP,
	IPT_WEB_RURI,
	IPT_WEB_PATH,
	IPT_WEB_QUERY,
	IPT_WEB_HOST,
	IPT_WEB_HORE
} ipt_web_mode_t;

struct ipt_web_info {
	ipt_web_mode_t mode;
	int invert;
	char text[IPT_WEB_MAXTEXT];
};

#endif
