/* $Id: tsx_basic_test.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "tsx_basic_test.c"

static char TARGET_URI[PJSIP_MAX_URL_SIZE];
static char FROM_URI[PJSIP_MAX_URL_SIZE];


/* Test transaction layer. */
static int tsx_layer_test(void)
{
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx, *found;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  transaction layer test"));

    target = pj_str(TARGET_URI);
    from = pj_str(FROM_URI);

    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
					&from, &target, NULL, NULL, -1, NULL,
					&tdata);
    if (status != PJ_SUCCESS) {
	app_perror("  error: unable to create request", status);
	return -110;
    }

    status = pjsip_tsx_create_uac(0, NULL, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create transaction", status);
	return -120;
    }

    pj_strdup(tdata->pool, &tsx_key, &tsx->transaction_key);

    found = pjsip_tsx_layer_find_tsx(0, &tsx_key, PJ_FALSE);
    if (found != tsx) {
	return -130;
    }

    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    flush_events(500);

    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED) {
	return -140;
    }

    return 0;
}

/* Double terminate test. */
static int double_terminate(void)
{
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  double terminate test"));

    target = pj_str(TARGET_URI);
    from = pj_str(FROM_URI);

    /* Create request. */
    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
					&from, &target, NULL, NULL, -1, NULL,
					&tdata);
    if (status != PJ_SUCCESS) {
	app_perror("  error: unable to create request", status);
	return -10;
    }

    /* Create transaction. */
    status = pjsip_tsx_create_uac(0, NULL, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create transaction", status);
	return -20;
    }

    /* Save transaction key for later. */
    pj_strdup_with_null(tdata->pool, &tsx_key, &tsx->transaction_key);

    /* Add reference to transmit buffer (tsx_send_msg() will dec txdata). */
    pjsip_tx_data_add_ref(tdata);

    /* Send message to start timeout timer. */
    status = pjsip_tsx_send_msg(tsx, NULL);

    /* Terminate transaction. */
    status = pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to terminate transaction", status);
	return -30;
    }

    tsx = pjsip_tsx_layer_find_tsx(0, &tsx_key, PJ_TRUE);
    if (tsx) {
	/* Terminate transaction again. */
	pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
	if (status != PJ_SUCCESS) {
	    app_perror("   error: unable to terminate transaction", status);
	    return -40;
	}
	pj_mutex_unlock(tsx->mutex);
    }

    flush_events(500);
    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED) {
	return -50;
    }

    return PJ_SUCCESS;
}

int tsx_basic_test(struct tsx_test_param *param)
{
    int status;

    pj_ansi_sprintf(TARGET_URI, "sip:bob@127.0.0.1:%d;transport=%s", 
		    param->port, param->tp_type);
    pj_ansi_sprintf(FROM_URI, "sip:alice@127.0.0.1:%d;transport=%s", 
		    param->port, param->tp_type);

    status = tsx_layer_test();
    if (status != 0)
	return status;

    status = double_terminate();
    if (status != 0)
	return status;

    return 0;
}
