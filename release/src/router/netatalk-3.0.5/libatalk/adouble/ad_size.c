/*
 *
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All rights reserved. See COPYRIGHT.
 *
 * if we could depend upon inline functions, this would be one.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <atalk/logger.h>
#include <atalk/adouble.h>

off_t ad_size(const struct adouble *ad, const uint32_t eid)
{
  if (eid == ADEID_DFORK) {
    struct stat st;

    if (ad->ad_data_fork.adf_syml) 
        return strlen(ad->ad_data_fork.adf_syml);

    if (fstat(ad_data_fileno(ad), &st) < 0)
      return 0;
    return st.st_size;
  }  
#if 0
  return ad_getentrylen(ad, eid);
#endif
  return ad->ad_rlen;  
} 
