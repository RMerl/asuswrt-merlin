
/* 
 *
 * Copyright (c) 2003 the Netatalk Team
 * Copyright (c) 2003 Rafal Lewczuk <rlewczuk@pronet.pl>
 * 
 * This program is free software; you can redistribute and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation version 2 of the License or later
 * version if explicitly stated by any of above copyright holders.
 *
 */

/*
 * This file contains initialization stuff for CNID backends.
 * Currently it only employs static bindings. 
 * No plans for dynamically loaded CNID backends here (temporary). 
 * Maybe somewhere in the future.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/cnid.h>
#include <atalk/list.h>
#include <atalk/logger.h>
#include <stdlib.h>


#ifdef CNID_BACKEND_DB3
extern struct _cnid_module cnid_db3_module;
#endif

#ifdef CNID_BACKEND_HASH
extern struct _cnid_module cnid_hash_module;
#endif

#ifdef CNID_BACKEND_LAST
extern struct _cnid_module cnid_last_module;
#endif

#ifdef CNID_BACKEND_MTAB
extern struct _cnid_module cnid_mtab_module;
#endif

#ifdef CNID_BACKEND_CDB
extern struct _cnid_module cnid_cdb_module;
#endif

#ifdef CNID_BACKEND_DBD
extern struct _cnid_module cnid_dbd_module;
#endif

#ifdef CNID_BACKEND_TDB
extern struct _cnid_module cnid_tdb_module;
#endif

void cnid_init(void)
{
#ifdef CNID_BACKEND_DB3
    cnid_register(&cnid_db3_module);
#endif

#ifdef CNID_BACKEND_HASH
    cnid_register(&cnid_hash_module);
#endif

#ifdef CNID_BACKEND_LAST
    cnid_register(&cnid_last_module);
#endif

#ifdef CNID_BACKEND_MTAB
    cnid_register(&cnid_mtab_module);
#endif

#ifdef CNID_BACKEND_CDB
    cnid_register(&cnid_cdb_module);
#endif

#ifdef CNID_BACKEND_DBD
    cnid_register(&cnid_dbd_module);
#endif

#ifdef CNID_BACKEND_TDB
    cnid_register(&cnid_tdb_module);
#endif
}
