/* 
   Internationalization of neon
   Copyright (C) 1999-2005, Joe Orton <joe@manyfish.co.uk>

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

#include "config.h"

#include "ne_i18n.h"

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

void ne_i18n_init(const char *encoding)
{
#if defined(NE_HAVE_I18N) && defined(NEON_IS_LIBRARY)
    /* The bindtextdomain call is only enabled if neon is built as a
     * library rather than as a bundled source; it would be possible
     * in the future to allow it for bundled builds too, if the neon
     * message catalogs could be installed alongside the app's own
     * message catalogs. */
    bindtextdomain("neon", LOCALEDIR);

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    if (encoding) {
        bind_textdomain_codeset("neon", encoding);
    }
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */

#endif
}
