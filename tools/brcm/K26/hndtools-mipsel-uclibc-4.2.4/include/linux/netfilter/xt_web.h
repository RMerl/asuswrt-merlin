/*

	web (experimental)
	HTTP client match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#ifndef _XT_WEB_H
#define _XT_WEB_H

#define XT_WEB_MAXTEXT	512

typedef enum {
	XT_WEB_HTTP,
	XT_WEB_RURI,
	XT_WEB_PATH,
	XT_WEB_QUERY,
	XT_WEB_HOST,
	XT_WEB_HORE
} xt_web_mode_t;

struct xt_web_info {
	xt_web_mode_t mode;
	int invert;
	char text[XT_WEB_MAXTEXT];
};

#endif
