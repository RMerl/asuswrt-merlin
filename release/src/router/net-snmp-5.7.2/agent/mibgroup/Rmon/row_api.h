/**************************************************************
 * Copyright (C) 2001 Alex Rozin, Optical Access 
 *
 *                     All Rights Reserved
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 * 
 * ALEX ROZIN DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * ALEX ROZIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 ******************************************************************/

#ifndef _row_api_h_included__
#define _row_api_h_included__

/*
 * control tables API section 
 */

typedef enum {
    RMON1_ENTRY_VALID = 1,
    RMON1_ENTRY_CREATE_REQUEST,
    RMON1_ENTRY_UNDER_CREATION,
    RMON1_ENTRY_INVALID
} RMON1_ENTRY_STATUS_T;

#define MAX_OWNERSTRING		128

/*
 * structure for entry of all 'control' tables 
 */
typedef struct tagEntry {
    /*
     * begin of the header 
     */
    struct tagEntry *next;
    void           *table_ptr;  /* do casting to (TABLE_DEFINTION_T*) */
    RMON1_ENTRY_STATUS_T status;
    RMON1_ENTRY_STATUS_T new_status;
    u_long          ctrl_index;
    u_long          timer_id;
    char           *owner;
    char           *new_owner;
    u_char          only_just_created;

    /*
     * end of the header 
     */

    void           *body;
    void           *tmp;
} RMON_ENTRY_T;

typedef int     (ENTRY_CALLBACK_T) (RMON_ENTRY_T *);

typedef struct {
    const char     *name;
    RMON_ENTRY_T   *first;
    u_long          max_number_of_entries;      /* '<0' means without limit */
    u_long          current_number_of_entries;
    ENTRY_CALLBACK_T *ClbkCreate;
    ENTRY_CALLBACK_T *ClbkClone;
    ENTRY_CALLBACK_T *ClbkValidate;
    ENTRY_CALLBACK_T *ClbkActivate;
    ENTRY_CALLBACK_T *ClbkDeactivate;
    ENTRY_CALLBACK_T *ClbkDelete;
    ENTRY_CALLBACK_T *ClbkCopy;
} TABLE_DEFINTION_T;

/*
 * Api prototypes 
 */
void            ROWAPI_init_table(TABLE_DEFINTION_T * table_ptr,
                                  const char *name,
                                  u_long max_number_of_entries,
                                  ENTRY_CALLBACK_T * ClbkCreate,
                                  ENTRY_CALLBACK_T * ClbkClone,
                                  ENTRY_CALLBACK_T * ClbkDelete,
                                  ENTRY_CALLBACK_T * ClbkValidate,
                                  ENTRY_CALLBACK_T * ClbkActivate,
                                  ENTRY_CALLBACK_T * ClbkDeactivate,
                                  ENTRY_CALLBACK_T * ClbkCopy);

int             ROWAPI_new(TABLE_DEFINTION_T * table_ptr,
                           u_long ctrl_index);

RMON_ENTRY_T   *ROWAPI_get_clone(TABLE_DEFINTION_T * table_ptr,
                                 u_long ctrl_index, size_t body_size);

void            ROWAPI_delete_clone(TABLE_DEFINTION_T * table_ptr,
                                    u_long ctrl_index);

RMON_ENTRY_T   *ROWAPI_first(TABLE_DEFINTION_T * table_ptr);

RMON_ENTRY_T   *ROWAPI_next(TABLE_DEFINTION_T * table_ptr,
                            u_long prev_index);

RMON_ENTRY_T   *ROWAPI_find(TABLE_DEFINTION_T * table_ptr,
                            u_long ctrl_index);

int             ROWAPI_action_check(TABLE_DEFINTION_T * table_ptr,
                                    u_long ctrl_index);

int             ROWAPI_commit(TABLE_DEFINTION_T * table_ptr,
                              u_long ctrl_index);

RMON_ENTRY_T   *ROWAPI_header_ControlEntry(struct variable *vp, oid * name,
                                           size_t * length, int exact,
                                           size_t * var_len,
                                           TABLE_DEFINTION_T * table_ptr,
                                           void *entry_ptr,
                                           size_t entry_size);

int             ROWAPI_do_another_action(oid * name,
                                         int tbl_first_index_begin,
                                         int action, int *prev_action,
                                         TABLE_DEFINTION_T * table_ptr,
                                         size_t entry_size);

/*
 * data tables API section 
 */

typedef int     (SCROLLER_ENTRY_DESCRUCTOR_T) (void *);

typedef struct nexted_void_t {
    struct nexted_void_t *next;
    u_long          data_index;
} NEXTED_PTR_T;

typedef struct data_scroller {
    u_long          max_number_of_entries;
    u_long          data_requested;
    u_long          data_granted;
    u_long          data_created;       /* number of allocated data entries */
    u_long          data_stored;        /* number of data, currently stored */
    u_long          data_total_number;  /* number of data entries, stored after validation */

    /*
     * these 3 pointers make casting to private (DATA_ENTRY_T*) 
     */
    void           *first_data_ptr;
    NEXTED_PTR_T   *last_data_ptr;
    void           *current_data_ptr;

    size_t          data_size;
    int             (*data_destructor) (struct data_scroller *, void *);
} SCROLLER_T;

int             ROWDATAAPI_init(SCROLLER_T * scrlr,
                                u_long max_number_of_entries,
                                u_long data_requested,
                                size_t data_size,
                                int (*data_destructor) (struct
                                                        data_scroller *,
                                                        void *));

void
                ROWDATAAPI_set_size(SCROLLER_T * scrlr,
                                    u_long data_requested,
                                    u_char do_allocation);

void            ROWDATAAPI_descructor(SCROLLER_T * scrlr);

void           *ROWDATAAPI_locate_new_data(SCROLLER_T * scrlr);

u_long          ROWDATAAPI_get_total_number(SCROLLER_T * scrlr);

RMON_ENTRY_T   *ROWDATAAPI_header_DataEntry(struct variable *vp,
                                            oid * name, size_t * length,
                                            int exact, size_t * var_len,
                                            TABLE_DEFINTION_T * table_ptr,
                                            SCROLLER_T *
                                            (*extract_scroller) (void
                                                                 *body),
                                            size_t data_size,
                                            void *entry_ptr);

#endif                          /* _row_api_h_included__ */
