/*
 * $Id: $
 * implement gdbm backend.  This will be an implementation based on the
 * absolute minimum API.  This will be used as a template for the "worst-case"
 * backend.  Writing a full backend might be faster, but this will be guaranteed
 * to work.
 *
 * To use a separate database as authoritative, you'll only need to implement the
 * read-only functions
 */

#ifndef _DB_GDBM_H_
#define _DB_GDBM_H_

#include "mp3-scanner.h"

/* Always required */
extern int db_gdbm_open(char **pe, char *parameters);
extern int db_gdbm_init(int reload);
extern int db_gdbm_deinit(void);

/* Required for read-only support */
extern PACKED_MP3FILE *db_gdbm_fetch_item(char **pe, int id);
extern int db_gdbm_enum_start(char **pe);
extern PACKED_MP3FILE *db_gdbm_enum_fetch(char **pe);
extern int db_gdbm_enum_end(char **pe);

/* Required for read-write (fs scanning) support */
extern int db_gdbm_add(char **pe, MP3FILE *pmp3);
extern int db_gdbm_delete(char **pe, int id);

#endif /* _DB_GDBM_H_ */

