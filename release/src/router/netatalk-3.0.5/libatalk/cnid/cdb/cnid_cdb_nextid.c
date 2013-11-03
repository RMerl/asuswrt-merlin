/*
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB

#ifdef unused
#include "cnid_cdb_private.h"

/* return the next id. we use the fact that ad files are memory
 * mapped. */
cnid_t cnid_cdb_nextid(struct _cnid_db *cdb)
{
    CNID_private *db;
    cnid_t id;

    if (!cdb || !(db = cdb->_private))
        return 0;

    memcpy(&id, ad_entry(&db->rootinfo, ADEID_DID), sizeof(id));
    return id;
}
#endif

#endif /* CNID_BACKEND_CDB */
