/* 
  By Matthew Strait <quadong@users.sf.net>, Dec 2003.
  http://l7-filter.sf.net

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version
  2 of the License, or (at your option) any later version.
  http://www.gnu.org/licenses/gpl.txt
*/

#ifndef _IPT_LAYER7_H
#define _IPT_LAYER7_H

#include <linux/netfilter/xt_layer7.h>

typedef char *(*proc_ipt_search) (char *, char, char *);

#define ipt_layer7_info	xt_layer7_info

#endif /* _IPT_LAYER7_H */
