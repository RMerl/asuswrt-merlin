/*

	bcount match (experimental)
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#ifndef _IPT_BCOUNT_H
#define _IPT_BCOUNT_H

struct ipt_bcount_match {
	u_int32_t min;
	u_int32_t max;
	int invert;
};

#endif
