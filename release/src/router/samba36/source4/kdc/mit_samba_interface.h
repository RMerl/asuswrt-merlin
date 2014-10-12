/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * plugins/kdb/samba/kdb_samba_interface.h
 *
 * Copyright (c) 2009, Simo Sorce <idra@samba.org>
 * All Rights Reserved.
 *
 *   Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#define MIT_SAMBA_INTERFACE_VERSION 1

#ifndef _SAMBA_BUILD_
typedef struct datablob {
	uint8_t *data;
	size_t length;
} DATA_BLOB;
#endif

struct mit_samba_context;

struct mit_samba_function_table {
    int (*init)(struct mit_samba_context **ctx);
    void (*fini)(struct mit_samba_context *ctx);

    /* db */
    int (*get_principal)(struct mit_samba_context *, char *,
                         unsigned int, hdb_entry_ex **);
    int (*get_firstkey)(struct mit_samba_context *, hdb_entry_ex **);
    int (*get_nextkey)(struct mit_samba_context *, hdb_entry_ex **);

    /* windc */
    int (*get_pac)(struct mit_samba_context *, hdb_entry_ex *, DATA_BLOB *);
    int (*update_pac)(struct mit_samba_context *, hdb_entry_ex *,
                      DATA_BLOB *, DATA_BLOB *);
    int (*client_access)(struct mit_samba_context *,
                         hdb_entry_ex *, const char *,
                         hdb_entry_ex *, const char *,
                         const char *, bool, DATA_BLOB *);
    int (*check_s4u2proxy)(struct mit_samba_context *,
                           hdb_entry_ex *, const char *, bool);
};
