/* 
 * accounting match (ipt_account.c)
 * (C) 2003,2004 by Piotr Gasidlo (quaker@barbara.eu.org)
 *
 * Version: 0.1.7
 *
 * This software is distributed under the terms of GNU GPL
 */

#ifndef _IPT_ACCOUNT_H_
#define _IPT_ACCOUNT_H_

#define IPT_ACCOUNT_NAME_LEN 64

#define IPT_ACCOUNT_NAME "ipt_account"
#define IPT_ACCOUNT_VERSION  "0.1.7"

struct t_ipt_account_info {
	char name[IPT_ACCOUNT_NAME_LEN];
	u_int32_t network;
	u_int32_t netmask;
	int shortlisting:1;
};

#endif

