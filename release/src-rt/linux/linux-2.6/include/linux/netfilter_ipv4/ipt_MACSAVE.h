/*

	macsave match
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#ifndef _IPT_MACSAVE_MATCH_H
#define _IPT_MACSAVE_MATCH_H

struct ipt_macsave_match_info {
	int invert;
	unsigned char mac[6];
};

#endif
