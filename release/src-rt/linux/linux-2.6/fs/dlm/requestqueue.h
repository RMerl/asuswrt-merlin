/******************************************************************************
*******************************************************************************
**
**  Copyright (C) 2005-2007 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#ifndef __REQUESTQUEUE_DOT_H__
#define __REQUESTQUEUE_DOT_H__

void dlm_add_requestqueue(struct dlm_ls *ls, int nodeid, struct dlm_message *ms);
int dlm_process_requestqueue(struct dlm_ls *ls);
void dlm_wait_requestqueue(struct dlm_ls *ls);
void dlm_purge_requestqueue(struct dlm_ls *ls);

#endif

