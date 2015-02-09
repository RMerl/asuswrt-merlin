/* Copyright (c) 2004-2012 Piotr 'QuakeR' Gasidlo <quaker@barbara.eu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IPT_ACCOUNT_H_
#define _IPT_ACCOUNT_H_

#define IPT_ACCOUNT_NAME_LEN 64

struct t_ipt_account_table;

struct t_ipt_account_info {
  char name[IPT_ACCOUNT_NAME_LEN + 1];
  u_int32_t network, netmask;
  int shortlisting:1;
  /* pointer to the table for fast matching */
  struct t_ipt_account_table *table;
};

#endif /* _IPT_ACCOUNT_H */

