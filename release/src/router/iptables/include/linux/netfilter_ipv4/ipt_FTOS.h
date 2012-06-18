/* Set TOS field in header to any value
 *
 * (C) 2000 by Matthew G. Marsh <mgm@paktronix.com>
 *
 * This software is distributed under GNU GPL v2, 1991
 * 
 * ipt_FTOS.h borrowed heavily from ipt_TOS.h  11/09/2000
*/
#ifndef _IPT_FTOS_H
#define _IPT_FTOS_H

struct ipt_FTOS_info {
	u_int8_t ftos;
};

#endif /*_IPT_FTOS_H*/
