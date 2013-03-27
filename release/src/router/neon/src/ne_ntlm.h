/* 
   Handling of NTLM Authentication
   Copyright (C) 2009, Kai Sommerfeld <kso@openoffice.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/
#ifndef NE_NTLM_H
#define NE_NTLM_H

#include "config.h"

/* PRIVATE TO NEON -- NOT PART OF THE EXTERNAL API. */

#ifdef HAVE_NTLM

#include "ne_defs.h"

typedef struct ne_ntlm_context_s ne_ntlm_context;

NE_PRIVATE ne_ntlm_context *ne__ntlm_create_context(const char *userName, const char *password);

NE_PRIVATE int ne__ntlm_clear_context(ne_ntlm_context *context);

NE_PRIVATE void ne__ntlm_destroy_context(ne_ntlm_context *context);

NE_PRIVATE int ne__ntlm_authenticate(ne_ntlm_context *context, const char *responseToken);

NE_PRIVATE char *ne__ntlm_getRequestToken(ne_ntlm_context *context);

#endif /* HAVE_NTLM */

#endif /* NE_NTLM_H */
