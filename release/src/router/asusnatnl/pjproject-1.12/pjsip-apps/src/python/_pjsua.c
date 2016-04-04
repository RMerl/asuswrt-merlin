/* $Id: _pjsua.c 3973 2012-03-13 02:01:58Z bennylp $ */
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
#include "_pjsua.h"

#define THIS_FILE    "main.c"
#define POOL_SIZE    512
#define SND_DEV_NUM  64
#define SND_NAME_LEN  64

/* LIB BASE */

static PyObject* g_obj_log_cb;
static long g_thread_id;
static struct py_thread_desc
{
    struct py_thread_desc *next;
    pj_thread_desc	   desc;
} *py_thread_desc;

/*
 * The global callback object.
 */
static PyObj_pjsua_callback * g_obj_callback;

/* Set this to 1 if all threads are created by Python */
#define NO_PJSIP_THREAD 1

#if NO_PJSIP_THREAD
#   define ENTER_PYTHON()
#   define LEAVE_PYTHON()
#else
#   define ENTER_PYTHON()   PyGILState_STATE state = PyGILState_Ensure()
#   define LEAVE_PYTHON()   PyGILState_Release(state)
#endif


static void clear_py_thread_desc(void)
{
    while (py_thread_desc) {
	struct py_thread_desc *next = py_thread_desc->next;
	free(py_thread_desc);
	py_thread_desc = next;
    }
}


/*
 * cb_log_cb
 * declares method for reconfiguring logging process for callback struct
 */
static void cb_log_cb(int level, const char *data, int len)
{
	
    /* Ignore if this callback is called from alien thread context,
     * or otherwise it will crash Python.
     */
    if (pj_thread_local_get(g_thread_id) == 0)
	return;

    if (PyCallable_Check(g_obj_log_cb)) {
	PyObject *param_data;

	ENTER_PYTHON();

	param_data = PyString_FromStringAndSize(data, len);

        PyObject_CallFunction(
            g_obj_log_cb, 
	    "iOi",
	    level,
            param_data, 
	    len, 
	    NULL
        );

	Py_DECREF(param_data);

	LEAVE_PYTHON();
    }
}

/*
 * cb_on_call_state
 * declares method on_call_state for callback struct
 */
static void cb_on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    if (PyCallable_Check(g_obj_callback->on_call_state)) {	
        PyObject * obj;

	ENTER_PYTHON();

	obj = Py_BuildValue("");
		
        PyObject_CallFunction(
            g_obj_callback->on_call_state,
	    "iO",
	    call_id,
	    obj,
	    NULL
        );

	Py_DECREF(obj);

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_incoming_call
 * declares method on_incoming_call for callback struct
 */
static void cb_on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                                pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(rdata);

    if (PyCallable_Check(g_obj_callback->on_incoming_call)) {
	PyObject *obj;

	ENTER_PYTHON();

	obj = Py_BuildValue("");

        PyObject_CallFunction(
                g_obj_callback->on_incoming_call,
		"iiO",
		acc_id,
                call_id,
		obj,
		NULL
        );

	Py_DECREF(obj);

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_call_media_state
 * declares method on_call_media_state for callback struct
 */
static void cb_on_call_media_state(pjsua_call_id call_id)
{
    if (PyCallable_Check(g_obj_callback->on_call_media_state)) {

	ENTER_PYTHON();

        PyObject_CallFunction(
	    g_obj_callback->on_call_media_state,
	    "i",
	    call_id,
	    NULL
	);

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_dtmf_digit()
 * Callback from PJSUA-LIB on receiving DTMF digit
 */
static void cb_on_dtmf_digit(pjsua_call_id call_id, int digit)
{
    if (PyCallable_Check(g_obj_callback->on_dtmf_digit)) {
	char digit_str[10];

	PyGILState_STATE state = PyGILState_Ensure();

	pj_ansi_snprintf(digit_str, sizeof(digit_str), "%c", digit);

        PyObject_CallFunction(
	    g_obj_callback->on_dtmf_digit,
	    "is",
	    call_id,
	    digit_str,
	    NULL
	);

	PyGILState_Release(state);
    }
}


/*
 * Notify application on call being transfered.
 * !modified @061206
 */
static void cb_on_call_transfer_request(pjsua_call_id call_id,
				        const pj_str_t *dst,
				        pjsip_status_code *code)
{
    if (PyCallable_Check(g_obj_callback->on_call_transfer_request)) {
	PyObject *ret, *param_dst;
	int cd;

	ENTER_PYTHON();

	param_dst = PyString_FromPJ(dst);

        ret = PyObject_CallFunction(
		    g_obj_callback->on_call_transfer_request,
		    "iOi",
		    call_id,
		    param_dst,
		    *code,
		    NULL
		);

	Py_DECREF(param_dst);

	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_Parse(ret,"i",&cd)) {
		    *code = cd;
		}
	    }
	    Py_DECREF(ret);
	}

	LEAVE_PYTHON();
    }
}


/*
 * Notify application of the status of previously sent call
 * transfer request. Application can monitor the status of the
 * call transfer request, for example to decide whether to 
 * terminate existing call.
 * !modified @061206
 */
static void cb_on_call_transfer_status( pjsua_call_id call_id,
					int status_code,
					const pj_str_t *status_text,
					pj_bool_t final,
					pj_bool_t *p_cont)
{
    if (PyCallable_Check(g_obj_callback->on_call_transfer_status)) {
	PyObject *ret, *param_reason;

	ENTER_PYTHON();

	param_reason = PyString_FromPJ(status_text);

        ret = PyObject_CallFunction(
		    g_obj_callback->on_call_transfer_status,
		    "iiOii",
		    call_id,
		    status_code,
		    param_reason,
		    final,
		    *p_cont,
		    NULL
		);

	Py_DECREF(param_reason);

	if (ret != NULL) {
	    if (ret != Py_None) {
		int cnt;
		if (PyArg_Parse(ret,"i",&cnt)) {
		    *p_cont = cnt;
		}
	    }
	    Py_DECREF(ret);
	}

	LEAVE_PYTHON();
    }
}


/*
 * Notify application about incoming INVITE with Replaces header.
 * Application may reject the request by setting non-2xx code.
 * !modified @061206
 */
static void cb_on_call_replace_request( pjsua_call_id call_id,
					pjsip_rx_data *rdata,
					int *st_code,
					pj_str_t *st_text)
{
    PJ_UNUSED_ARG(rdata);

    if (PyCallable_Check(g_obj_callback->on_call_replace_request)) {
	PyObject *ret, *param_reason, *param_rdata;
	int cd;

	ENTER_PYTHON();

	param_reason = PyString_FromPJ(st_text);
	param_rdata = Py_BuildValue("");

        ret = PyObject_CallFunction(
		    g_obj_callback->on_call_replace_request,
		    "iOiO",
		    call_id,
		    param_rdata,
		    *st_code,
		    param_reason,
		    NULL
		);

	Py_DECREF(param_rdata);
	Py_DECREF(param_reason);

	if (ret != NULL) {
	    if (ret != Py_None) {
		PyObject * txt;
		if (PyArg_ParseTuple(ret,"iO",&cd, &txt)) {
		    *st_code = cd;
		    *st_text = PyString_ToPJ(txt);
		}
	    }
	    Py_DECREF(ret);
	}

	LEAVE_PYTHON();
    }
}


/*
 * Notify application that an existing call has been replaced with
 * a new call. This happens when PJSUA-API receives incoming INVITE
 * request with Replaces header.
 */
static void cb_on_call_replaced(pjsua_call_id old_call_id,
				pjsua_call_id new_call_id)
{
    if (PyCallable_Check(g_obj_callback->on_call_replaced)) {
	ENTER_PYTHON();

        PyObject_CallFunction(
            g_obj_callback->on_call_replaced,
	    "ii",
	    old_call_id,
	    new_call_id,
	    NULL
        );

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_reg_state
 * declares method on_reg_state for callback struct
 */
static void cb_on_reg_state(pjsua_acc_id acc_id)
{
    if (PyCallable_Check(g_obj_callback->on_reg_state)) {
	ENTER_PYTHON();

        PyObject_CallFunction(
	    g_obj_callback->on_reg_state,
	    "i",
	    acc_id,
	    NULL
	);

	LEAVE_PYTHON();
    }
}

/* 
 * cb_on_incoming_subscribe
 */
static void cb_on_incoming_subscribe( pjsua_acc_id acc_id,
				      pjsua_srv_pres *srv_pres,
				      pjsua_buddy_id buddy_id,
				      const pj_str_t *from,
				      pjsip_rx_data *rdata,
				      pjsip_status_code *code,
				      pj_str_t *reason,
				      pjsua_msg_data *msg_data)
{
    static char reason_buf[64];

    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(msg_data);

    if (PyCallable_Check(g_obj_callback->on_incoming_subscribe)) {
	PyObject *ret, *param_from, *param_contact, *param_srv_pres;
	pjsip_contact_hdr *contact_hdr;
	pj_pool_t *pool = NULL;

	ENTER_PYTHON();

	param_from = PyString_FromPJ(from);
	param_srv_pres = PyLong_FromLong((long)srv_pres);

	contact_hdr = (pjsip_contact_hdr*)
		      pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT,
					 NULL);
	if (contact_hdr) {
	    char *contact;
	    int len;

	    pool = pjsua_pool_create("pytmp", 512, 512);
	    contact = (char*) pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE+1);
	    len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, contact_hdr->uri, 
				  contact, PJSIP_MAX_URL_SIZE);
	    if (len < 1)
		len = 0;
	    contact[len] = '\0';

	    param_contact = PyString_FromStringAndSize(contact, len);
	} else {
	    param_contact = Py_BuildValue("");
	}

        ret = PyObject_CallFunction(
		    g_obj_callback->on_incoming_subscribe,
		    "iiOOO",
		    acc_id,
		    buddy_id,
		    param_from,
		    param_contact,
		    param_srv_pres,
		    NULL
		);

	if (pool)
	    pj_pool_release(pool);

	Py_DECREF(param_from);
	Py_DECREF(param_contact);
	Py_DECREF(param_srv_pres);

	if (ret && PyTuple_Check(ret)) {
	    if (PyTuple_Size(ret) >= 1)
		*code = (int)PyInt_AsLong(PyTuple_GetItem(ret, 0));
	    if (PyTuple_Size(ret) >= 2) {
		if (PyTuple_GetItem(ret, 1) != Py_None) {
		    pj_str_t tmp;
		    tmp = PyString_ToPJ(PyTuple_GetItem(ret, 1));
		    reason->ptr = reason_buf;
		    pj_strncpy(reason, &tmp, sizeof(reason_buf));
		} else {
		    reason->slen = 0;
		}
	    }
	    Py_XDECREF(ret);
	} else if (ret) {
	    Py_XDECREF(ret);
	}

	LEAVE_PYTHON();
    }
}

/*
 * cb_on_buddy_state
 * declares method on_buddy state for callback struct
 */
static void cb_on_buddy_state(pjsua_buddy_id buddy_id)
{
    if (PyCallable_Check(g_obj_callback->on_buddy_state)) {
	ENTER_PYTHON();

        PyObject_CallFunction(
	    g_obj_callback->on_buddy_state,
	    "i",
	    buddy_id,
	    NULL
	);

	LEAVE_PYTHON();
    }
}

/*
 * cb_on_pager
 * declares method on_pager for callback struct
 */
static void cb_on_pager(pjsua_call_id call_id, const pj_str_t *from,
                        const pj_str_t *to, const pj_str_t *contact,
                        const pj_str_t *mime_type, const pj_str_t *body,
			pjsip_rx_data *rdata, pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(rdata);

    if (PyCallable_Check(g_obj_callback->on_pager)) {
	PyObject *param_from, *param_to, *param_contact, *param_mime_type,
		 *param_body;

	ENTER_PYTHON();

	param_from = PyString_FromPJ(from);
	param_to = PyString_FromPJ(to);
	param_contact = PyString_FromPJ(contact);
	param_mime_type = PyString_FromPJ(mime_type);
	param_body = PyString_FromPJ(body);

        PyObject_CallFunction(
		g_obj_callback->on_pager,
		"iOOOOOi",
		call_id,
		param_from,
		param_to,
		param_contact,
		param_mime_type,
		param_body, 
		acc_id,
		NULL
	    );

	Py_DECREF(param_body);
	Py_DECREF(param_mime_type);
	Py_DECREF(param_contact);
	Py_DECREF(param_to);
	Py_DECREF(param_from);

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_pager_status
 * declares method on_pager_status for callback struct
 */
static void cb_on_pager_status(pjsua_call_id call_id, const pj_str_t *to,
                                const pj_str_t *body, void *user_data,
                                pjsip_status_code status,
                                const pj_str_t *reason,
				pjsip_tx_data *tdata,
				pjsip_rx_data *rdata,
				pjsua_acc_id acc_id)
{
    if (PyCallable_Check(g_obj_callback->on_pager)) {
	PyObject *param_call_id, *param_to, *param_body,
		 *param_user_data, *param_status, *param_reason,
		 *param_acc_id;

	ENTER_PYTHON();

	PJ_UNUSED_ARG(tdata);
	PJ_UNUSED_ARG(rdata);

        PyObject_CallFunctionObjArgs(
		g_obj_callback->on_pager_status,
		param_call_id	= Py_BuildValue("i",call_id),
		param_to	= PyString_FromPJ(to),
		param_body	= PyString_FromPJ(body), 
		param_user_data = Py_BuildValue("i", user_data),
		param_status	= Py_BuildValue("i",status),
		param_reason	= PyString_FromPJ(reason),
		param_acc_id	= Py_BuildValue("i",acc_id),
		NULL
	    );

	Py_DECREF(param_call_id);
	Py_DECREF(param_to);
	Py_DECREF(param_body);
	Py_DECREF(param_user_data);
	Py_DECREF(param_status);
	Py_DECREF(param_reason);
	Py_DECREF(param_acc_id);

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_typing
 * declares method on_typing for callback struct
 */
static void cb_on_typing(pjsua_call_id call_id, const pj_str_t *from,
                            const pj_str_t *to, const pj_str_t *contact,
                            pj_bool_t is_typing, pjsip_rx_data *rdata,
			    pjsua_acc_id acc_id)
{
    if (PyCallable_Check(g_obj_callback->on_typing)) {
	PyObject *param_call_id, *param_from, *param_to, *param_contact,
		 *param_is_typing, *param_acc_id;

	ENTER_PYTHON();

	PJ_UNUSED_ARG(rdata);

        PyObject_CallFunctionObjArgs(
		g_obj_callback->on_typing,
		param_call_id	= Py_BuildValue("i",call_id),
		param_from	= PyString_FromPJ(from),
		param_to	= PyString_FromPJ(to),
		param_contact	= PyString_FromPJ(contact),
		param_is_typing = Py_BuildValue("i",is_typing),
		param_acc_id	= Py_BuildValue("i",acc_id),
		NULL
	    );

	Py_DECREF(param_call_id);
	Py_DECREF(param_from);
	Py_DECREF(param_to);
	Py_DECREF(param_contact);
	Py_DECREF(param_is_typing); 
	Py_DECREF(param_acc_id);

	LEAVE_PYTHON();
    }
}


/*
 * on_mwi_info
 */
static void cb_on_mwi_info(pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info)
{
    if (PyCallable_Check(g_obj_callback->on_mwi_info)) {
	PyObject *param_acc_id, *param_body;
	pj_str_t body;

	ENTER_PYTHON();

	body.ptr = mwi_info->rdata->msg_info.msg->body->data;
	body.slen = mwi_info->rdata->msg_info.msg->body->len;

        PyObject_CallFunctionObjArgs(
		g_obj_callback->on_mwi_info,
		param_acc_id	= Py_BuildValue("i",acc_id),
		param_body	= PyString_FromPJ(&body),
		NULL
	    );

	Py_DECREF(param_acc_id);
	Py_DECREF(param_body);

	LEAVE_PYTHON();
    }
}

/* 
 * translate_hdr
 * internal function 
 * translate from hdr_list to pjsip_generic_string_hdr
 */
void translate_hdr(pj_pool_t *pool, pjsip_hdr *hdr, PyObject *py_hdr_list)
{
    pj_list_init(hdr);

    if (PyList_Check(py_hdr_list)) {
	int i;

        for (i=0; i<PyList_Size(py_hdr_list); ++i)  { 
            pj_str_t hname, hvalue;
	    pjsip_generic_string_hdr * new_hdr;
            PyObject * tuple = PyList_GetItem(py_hdr_list, i);

            if (PyTuple_Check(tuple)) {
		if (PyTuple_Size(tuple) >= 1)
		    hname = PyString_ToPJ(PyTuple_GetItem(tuple,0));
		else
		    hname.slen = 0;
		if (PyTuple_Size(tuple) >= 2)
		    hvalue = PyString_ToPJ(PyTuple_GetItem(tuple,1));
		else
		    hvalue.slen = 0;
            } else {
		hname.ptr = "";
		hname.slen = 0;
		hvalue.ptr = "";
		hvalue.slen = 0;
            }  
            new_hdr = pjsip_generic_string_hdr_create(pool, &hname, &hvalue);
            pj_list_push_back((pj_list_type *)hdr, (pj_list_type *)new_hdr);
	}     
    }
}

/*
 * py_pjsua_thread_register
 */
static PyObject *py_pjsua_thread_register(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;	
    const char *name;
    PyObject *py_desc;
    pj_thread_t *thread;
    struct py_thread_desc *thread_desc;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "sO", &name, &py_desc)) {
         return NULL;
    }
    thread_desc = (struct py_thread_desc*)
		  malloc(sizeof(struct py_thread_desc));
    thread_desc->next = py_thread_desc;
    py_thread_desc = thread_desc;

    status = pj_thread_register(name, thread_desc->desc, &thread);

    if (status == PJ_SUCCESS)
	status = pj_thread_local_set(g_thread_id, (void*)1);

    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_logging_config_default
 */
static PyObject *py_pjsua_logging_config_default(PyObject *pSelf,
                                                 PyObject *pArgs)
{
    PyObj_pjsua_logging_config *obj;	
    pjsua_logging_config cfg;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    pjsua_logging_config_default(&cfg);
    obj = (PyObj_pjsua_logging_config*) 
	  PyObj_pjsua_logging_config_new(&PyTyp_pjsua_logging_config, 
					 NULL, NULL);
    PyObj_pjsua_logging_config_import(obj, &cfg);
    
    return (PyObject*)obj;
}


/*
 * py_pjsua_config_default
 */
static PyObject *py_pjsua_config_default(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjsua_config *obj;
    pjsua_config cfg;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    pjsua_config_default(&cfg);
    obj = (PyObj_pjsua_config *) PyObj_pjsua_config_new(&PyTyp_pjsua_config, 
							NULL, NULL);
    PyObj_pjsua_config_import(obj, &cfg);

    return (PyObject*)obj;
}


/*
 * py_pjsua_media_config_default
 */
static PyObject * py_pjsua_media_config_default(PyObject *pSelf,
                                                PyObject *pArgs)
{
    PyObj_pjsua_media_config *obj;
    pjsua_media_config cfg;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    pjsua_media_config_default(&cfg);
    obj = (PyObj_pjsua_media_config *)
	  PyType_GenericNew(&PyTyp_pjsua_media_config, NULL, NULL);
    PyObj_pjsua_media_config_import(obj, &cfg);

    return (PyObject *)obj;
}


/*
 * py_pjsua_msg_data_init
 */
static PyObject *py_pjsua_msg_data_init(PyObject *pSelf, PyObject *pArgs)
{
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    return (PyObject *)PyObj_pjsua_msg_data_new(&PyTyp_pjsua_msg_data, 
						NULL, NULL);
}


/*
 * py_pjsua_reconfigure_logging
 */
static PyObject *py_pjsua_reconfigure_logging(PyObject *pSelf, 
					      PyObject *pArgs)
{
    PyObject *logObj;
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &logObj)) {
        return NULL;
    }

    if (logObj != Py_None) {
	PyObj_pjsua_logging_config *log;
	pjsua_logging_config cfg;

        log = (PyObj_pjsua_logging_config*)logObj;
        cfg.msg_logging = log->msg_logging;
        cfg.level = log->level;
        cfg.console_level = log->console_level;
        cfg.decor = log->decor;
        cfg.log_filename = PyString_ToPJ(log->log_filename);
        Py_XDECREF(g_obj_log_cb);
        g_obj_log_cb = log->cb;
        Py_INCREF(g_obj_log_cb);
        cfg.cb = &cb_log_cb;
        status = pjsua_reconfigure_logging(&cfg);
    } else {
        status = pjsua_reconfigure_logging(NULL);
    }

    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_perror
 */
static PyObject *py_pjsua_perror(PyObject *pSelf, PyObject *pArgs)
{
    const char *sender;
    const char *title;
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ssi", &sender, &title, &status)) {
        return NULL;
    }
	
    pjsua_perror(sender, title, status);

    return Py_BuildValue("");
}


/*
 * py_pjsua_create
 */
static PyObject *py_pjsua_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    status = pjsua_create();
    
    if (status == PJ_SUCCESS)  {
	status = pj_thread_local_alloc(&g_thread_id);
	if (status == PJ_SUCCESS)
	    status = pj_thread_local_set(g_thread_id, (void*)1);

	pj_atexit(0, &clear_py_thread_desc);
    }

    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_init
 */
static PyObject *py_pjsua_init(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *o_ua_cfg, *o_log_cfg, *o_media_cfg;
    pjsua_config cfg_ua, *p_cfg_ua;
    pjsua_logging_config cfg_log, *p_cfg_log;
    pjsua_media_config cfg_media, *p_cfg_media;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OOO", &o_ua_cfg, &o_log_cfg, &o_media_cfg)) {
        return NULL;
    }
    
    pjsua_config_default(&cfg_ua);
    pjsua_logging_config_default(&cfg_log);
    pjsua_media_config_default(&cfg_media);

    if (o_ua_cfg != Py_None) {
	PyObj_pjsua_config *obj_ua_cfg = (PyObj_pjsua_config*)o_ua_cfg;

	PyObj_pjsua_config_export(&cfg_ua, obj_ua_cfg);

	Py_XDECREF(g_obj_callback);
    	g_obj_callback = obj_ua_cfg->cb;
    	Py_INCREF(g_obj_callback);

    	cfg_ua.cb.on_call_state = &cb_on_call_state;
    	cfg_ua.cb.on_incoming_call = &cb_on_incoming_call;
    	cfg_ua.cb.on_call_media_state = &cb_on_call_media_state;
	cfg_ua.cb.on_dtmf_digit = &cb_on_dtmf_digit;
    	cfg_ua.cb.on_call_transfer_request = &cb_on_call_transfer_request;
    	cfg_ua.cb.on_call_transfer_status = &cb_on_call_transfer_status;
    	cfg_ua.cb.on_call_replace_request = &cb_on_call_replace_request;
    	cfg_ua.cb.on_call_replaced = &cb_on_call_replaced;
    	cfg_ua.cb.on_reg_state = &cb_on_reg_state;
	cfg_ua.cb.on_incoming_subscribe = &cb_on_incoming_subscribe;
    	cfg_ua.cb.on_buddy_state = &cb_on_buddy_state;
    	cfg_ua.cb.on_pager2 = &cb_on_pager;
    	cfg_ua.cb.on_pager_status2 = &cb_on_pager_status;
    	cfg_ua.cb.on_typing2 = &cb_on_typing;
	cfg_ua.cb.on_mwi_info = &cb_on_mwi_info;

        p_cfg_ua = &cfg_ua;

    } else {
        p_cfg_ua = NULL;
    }

    if (o_log_cfg != Py_None)  {
	PyObj_pjsua_logging_config * obj_log;

        obj_log = (PyObj_pjsua_logging_config *)o_log_cfg;
        
        PyObj_pjsua_logging_config_export(&cfg_log, obj_log);

        Py_XDECREF(g_obj_log_cb);
        g_obj_log_cb = obj_log->cb;
        Py_INCREF(g_obj_log_cb);

        cfg_log.cb = &cb_log_cb;
        p_cfg_log = &cfg_log;

    } else {
        p_cfg_log = NULL;
    }

    if (o_media_cfg != Py_None) {
	PyObj_pjsua_media_config_export(&cfg_media, 
				        (PyObj_pjsua_media_config*)o_media_cfg);
	p_cfg_media = &cfg_media;

    } else {
        p_cfg_media = NULL;
    }

    status = pjsua_init(p_cfg_ua, p_cfg_log, p_cfg_media);

    return Py_BuildValue("i", status);
}


/*
 * py_pjsua_start
 */
static PyObject *py_pjsua_start(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    status = pjsua_start();
    
    return Py_BuildValue("i", status);
}


/*
 * py_pjsua_destroy
 */
static PyObject *py_pjsua_destroy(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    status = pjsua_destroy();
    
    return Py_BuildValue("i", status);
}


/*
 * py_pjsua_handle_events
 */
static PyObject *py_pjsua_handle_events(PyObject *pSelf, PyObject *pArgs)
{
    int ret;
    int msec;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &msec)) {
        return NULL;
    }

    if (msec < 0)
	msec = 0;

#if !NO_PJSIP_THREAD
    /* Since handle_events() will block, we must wrap it with ALLOW_THREADS
     * construct, or otherwise many Python blocking functions (such as
     * time.sleep(), readline(), etc.) may hang/block indefinitely.
     * See http://www.python.org/doc/current/api/threads.html for more info.
     */
    Py_BEGIN_ALLOW_THREADS
#endif

    ret = pjsua_handle_events(msec);

#if !NO_PJSIP_THREAD
    Py_END_ALLOW_THREADS
#endif
    
    return Py_BuildValue("i", ret);
}


/*
 * py_pjsua_verify_sip_url
 */
static PyObject *py_pjsua_verify_sip_url(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    const char *url;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "s", &url)) {
        return NULL;
    }

    status = pjsua_verify_sip_url(url);
    
    return Py_BuildValue("i", status);
}


/*
 * function doc
 */

static char pjsua_thread_register_doc[] =
    "int _pjsua.thread_register(string name, int[] desc)";
static char pjsua_perror_doc[] =
    "void _pjsua.perror (string sender, string title, int status) "
    "Display error message for the specified error code. Parameters: "
    "sender: The log sender field;  "
    "title: Message title for the error; "
    "status: Status code.";

static char pjsua_create_doc[] =
    "int _pjsua.create (void) "
    "Instantiate pjsua application. Application "
    "must call this function before calling any other functions, to make sure "
    "that the underlying libraries are properly initialized. Once this "
    "function has returned success, application must call pjsua_destroy() "
    "before quitting.";

static char pjsua_init_doc[] =
    "int _pjsua.init (_pjsua.Config obj_ua_cfg, "
        "_pjsua.Logging_Config log_cfg, _pjsua.Media_Config media_cfg) "
    "Initialize pjsua with the specified settings. All the settings are "
    "optional, and the default values will be used when the config is not "
    "specified. Parameters: "
    "obj_ua_cfg : User agent configuration;  "
    "log_cfg : Optional logging configuration; "
    "media_cfg : Optional media configuration.";

static char pjsua_start_doc[] =
    "int _pjsua.start (void) "
    "Application is recommended to call this function after all "
    "initialization is done, so that the library can do additional checking "
    "set up additional";

static char pjsua_destroy_doc[] =
    "int _pjsua.destroy (void) "
    "Destroy pjsua This function must be called once PJSUA is created. To "
    "make it easier for application, application may call this function "
    "several times with no danger.";

static char pjsua_handle_events_doc[] =
    "int _pjsua.handle_events (int msec_timeout) "
    "Poll pjsua for events, and if necessary block the caller thread for the "
    "specified maximum interval (in miliseconds) Parameters: "
    "msec_timeout: Maximum time to wait, in miliseconds. "
    "Returns: The number of events that have been handled during the poll. "
    "Negative value indicates error, and application can retrieve the error "
    "as (err = -return_value).";

static char pjsua_verify_sip_url_doc[] =
    "int _pjsua.verify_sip_url (string c_url) "
    "Verify that valid SIP url is given Parameters: "
    "c_url: The URL, as NULL terminated string.";

static char pjsua_reconfigure_logging_doc[] =
    "int _pjsua.reconfigure_logging (_pjsua.Logging_Config c) "
    "Application can call this function at any time (after pjsua_create(), of "
    "course) to change logging settings. Parameters: "
    "c: Logging configuration.";

static char pjsua_logging_config_default_doc[] =
    "_pjsua.Logging_Config _pjsua.logging_config_default  ()  "
    "Use this function to initialize logging config.";

static char pjsua_config_default_doc[] =
    "_pjsua.Config _pjsua.config_default (). Use this function to "
    "initialize pjsua config. ";

static char pjsua_media_config_default_doc[] =
    "_pjsua.Media_Config _pjsua.media_config_default (). "
    "Use this function to initialize media config.";

static char pjsua_msg_data_init_doc[] =
    "_pjsua.Msg_Data void _pjsua.msg_data_init () "
    "Initialize message data ";
        

/* END OF LIB BASE */

/* LIB TRANSPORT */

/*
 * py_pjsua_transport_config_default
 */
static PyObject *py_pjsua_transport_config_default(PyObject *pSelf, 
						   PyObject *pArgs)
{
    PyObj_pjsua_transport_config *obj;
    pjsua_transport_config cfg;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    pjsua_transport_config_default(&cfg);
    obj = (PyObj_pjsua_transport_config*)
	  PyObj_pjsua_transport_config_new(&PyTyp_pjsua_transport_config,
					   NULL, NULL);
    PyObj_pjsua_transport_config_import(obj, &cfg);

    return (PyObject *)obj;
}

/*
 * py_pjsua_transport_create
 */
static PyObject *py_pjsua_transport_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int type;
    PyObject *pCfg;
    pjsua_transport_config cfg;
    pjsua_transport_id id;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &type, &pCfg)) {
        return NULL;
    }

    if (pCfg != Py_None) {
	PyObj_pjsua_transport_config *obj;

        obj = (PyObj_pjsua_transport_config*)pCfg;
	PyObj_pjsua_transport_config_export(&cfg, obj);
        status = pjsua_transport_create(type, &cfg, &id);
    } else {
        status = pjsua_transport_create(type, NULL, &id);
    }
    
    
    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_enum_transports
 */
static PyObject *py_pjsua_enum_transports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    pjsua_transport_id id[PJSIP_MAX_TRANSPORTS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_transports(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) {     
        PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
    }
    
    return (PyObject*)list;
}

/*
 * py_pjsua_transport_get_info
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_get_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int id;
    pjsua_transport_info info;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }	
    
    status = pjsua_transport_get_info(id, &info);	
    if (status == PJ_SUCCESS) {
	PyObj_pjsua_transport_info *obj;
        obj = (PyObj_pjsua_transport_info *) 
	      PyObj_pjsua_transport_info_new(&PyTyp_pjsua_transport_info, 
					     NULL, NULL);
	PyObj_pjsua_transport_info_import(obj, &info);
        return (PyObject*)obj;
    } else {
        return Py_BuildValue("");
    }
}

/*
 * py_pjsua_transport_set_enable
 */
static PyObject *py_pjsua_transport_set_enable(PyObject *pSelf, 
					       PyObject *pArgs)
{
    pj_status_t status;
    int id;
    int enabled;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &id, &enabled)) {
        return NULL;
    }
    status = pjsua_transport_set_enable(id, enabled);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_transport_close
 */
static PyObject *py_pjsua_transport_close(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int id;
    int force;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &id, &force)) {
        return NULL;
    }	
    status = pjsua_transport_close(id, force);	
    
    return Py_BuildValue("i", status);
}

static char pjsua_transport_config_default_doc[] =
    "_pjsua.Transport_Config _pjsua.transport_config_default () "
    "Call this function to initialize UDP config with default values.";
static char pjsua_transport_create_doc[] =
    "int, int _pjsua.transport_create (int type, "
    "_pjsua.Transport_Config cfg) "
    "Create SIP transport.";
static char pjsua_enum_transports_doc[] =
    "int[] _pjsua.enum_transports () "
    "Enumerate all transports currently created in the system.";
static char pjsua_transport_get_info_doc[] =
    "void _pjsua.transport_get_info "
    "(_pjsua.Transport_ID id, _pjsua.Transport_Info info) "
    "Get information about transports.";
static char pjsua_transport_set_enable_doc[] =
    "void _pjsua.transport_set_enable "
    "(_pjsua.Transport_ID id, int enabled) "
    "Disable a transport or re-enable it. "
    "By default transport is always enabled after it is created. "
    "Disabling a transport does not necessarily close the socket, "
    "it will only discard incoming messages and prevent the transport "
    "from being used to send outgoing messages.";
static char pjsua_transport_close_doc[] =
    "void _pjsua.transport_close (_pjsua.Transport_ID id, int force) "
    "Close the transport. If transport is forcefully closed, "
    "it will be immediately closed, and any pending transactions "
    "that are using the transport may not terminate properly. "
    "Otherwise, the system will wait until all transactions are closed "
    "while preventing new users from using the transport, and will close "
    "the transport when it is safe to do so.";

/* END OF LIB TRANSPORT */

/* LIB ACCOUNT */


/*
 * py_pjsua_acc_config_default
 */
static PyObject *py_pjsua_acc_config_default(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjsua_acc_config *obj;
    pjsua_acc_config cfg;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }

    pjsua_acc_config_default(&cfg);
    obj = (PyObj_pjsua_acc_config *)
	  PyObj_pjsua_acc_config_new(&PyTyp_pjsua_acc_config, 
				     NULL, NULL);
    PyObj_pjsua_acc_config_import(obj, &cfg);
    return (PyObject *)obj;
}

/*
 * py_pjsua_acc_get_count
 */
static PyObject *py_pjsua_acc_get_count(PyObject *pSelf, PyObject *pArgs)
{
    int count;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    count = pjsua_acc_get_count();
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_acc_is_valid
 */
static PyObject *py_pjsua_acc_is_valid(PyObject *pSelf, PyObject *pArgs)
{    
    int id;
    int is_valid;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }

    is_valid = pjsua_acc_is_valid(id);	
    return Py_BuildValue("i", is_valid);
}

/*
 * py_pjsua_acc_set_default
 */
static PyObject *py_pjsua_acc_set_default(PyObject *pSelf, PyObject *pArgs)
{    
    int id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }
    status = pjsua_acc_set_default(id);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_get_default
 */
static PyObject *py_pjsua_acc_get_default(PyObject *pSelf, PyObject *pArgs)
{    
    int id;
	
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    id = pjsua_acc_get_default();
	
    return Py_BuildValue("i", id);
}

/*
 * py_pjsua_acc_add
 */
static PyObject *py_pjsua_acc_add(PyObject *pSelf, PyObject *pArgs)
{    
    int is_default;
    PyObject *pCfg;
    int acc_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "Oi", &pCfg, &is_default)) {
        return NULL;
    }
    
    if (pCfg != Py_None) {
	pjsua_acc_config cfg;
	PyObj_pjsua_acc_config *ac;

	pjsua_acc_config_default(&cfg);
        ac = (PyObj_pjsua_acc_config *)pCfg;
        PyObj_pjsua_acc_config_export(&cfg, ac);
        status = pjsua_acc_add(&cfg, is_default, &acc_id);
    } else {
        status = PJ_EINVAL;
	acc_id = PJSUA_INVALID_ID;
    }
    
    return Py_BuildValue("ii", status, acc_id);
}

/*
 * py_pjsua_acc_add_local
 */
static PyObject *py_pjsua_acc_add_local(PyObject *pSelf, PyObject *pArgs)
{    
    int is_default;
    int tid;
    int acc_id;
    int status;
	
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &tid, &is_default)) {
        return NULL;
    }
	
    status = pjsua_acc_add_local(tid, is_default, &acc_id);
    
    return Py_BuildValue("ii", status, acc_id);
}

/*
 * py_pjsua_acc_set_user_data
 */
static PyObject *py_pjsua_acc_set_user_data(PyObject *pSelf, PyObject *pArgs)
{    
    int acc_id;
    PyObject *pUserData, *old_user_data;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &acc_id, &pUserData)) {
        return NULL;
    }

    old_user_data = (PyObject*) pjsua_acc_get_user_data(acc_id);

    status = pjsua_acc_set_user_data(acc_id, (void*)pUserData);

    if (status == PJ_SUCCESS) {
	Py_XINCREF(pUserData);
	Py_XDECREF(old_user_data);
    }

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_get_user_data
 */
static PyObject *py_pjsua_acc_get_user_data(PyObject *pSelf, PyObject *pArgs)
{    
    int acc_id;
    PyObject *user_data;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &acc_id)) {
        return NULL;
    }

    user_data = (PyObject*) pjsua_acc_get_user_data(acc_id);

    return user_data ? Py_BuildValue("O", user_data) : Py_BuildValue("");
}

/*
 * py_pjsua_acc_del
 */
static PyObject *py_pjsua_acc_del(PyObject *pSelf, PyObject *pArgs)
{    
    int acc_id;
    PyObject *user_data;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &acc_id)) {
        return NULL;
    }

    user_data = (PyObject*) pjsua_acc_get_user_data(acc_id);
    Py_XDECREF(user_data);

    status = pjsua_acc_del(acc_id);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_modify
 */
static PyObject *py_pjsua_acc_modify(PyObject *pSelf, PyObject *pArgs)
{    	
    PyObject *pCfg;
    PyObj_pjsua_acc_config * ac;
    int acc_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &acc_id, &pCfg)) {
        return NULL;
    }

    if (pCfg != Py_None) {
	pjsua_acc_config cfg;	

	pjsua_acc_config_default(&cfg);
        ac = (PyObj_pjsua_acc_config*)pCfg;
        PyObj_pjsua_acc_config_export(&cfg, ac);

        status = pjsua_acc_modify(acc_id, &cfg);
    } else {
        status = PJ_EINVAL;
    }
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_set_online_status
 */
static PyObject *py_pjsua_acc_set_online_status(PyObject *pSelf, 
						PyObject *pArgs)
{    
    int is_online;	
    int acc_id;
    int status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &acc_id, &is_online)) {
        return NULL;
    }
	
    status = pjsua_acc_set_online_status(acc_id, is_online);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_set_online_status2
 */
static PyObject *py_pjsua_acc_set_online_status2(PyObject *pSelf, 
						 PyObject *pArgs)
{    
    int is_online;	
    int acc_id;
    int activity_id;
    const char *activity_text = NULL;
    const char *rpid_id = NULL;
    pjrpid_element rpid;
    pj_status_t status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiiss", &acc_id, &is_online,
			  &activity_id, &activity_text, &rpid_id)) 
    {
        return NULL;
    }

    pj_bzero(&rpid, sizeof(rpid));
    rpid.type = PJRPID_ELEMENT_TYPE_PERSON;
    rpid.activity = activity_id;
    if (activity_text)
	rpid.note = pj_str((char*)activity_text);

    if (rpid_id)
	rpid.id = pj_str((char*)rpid_id);

    status = pjsua_acc_set_online_status2(acc_id, is_online, &rpid);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_set_registration
 */
static PyObject *py_pjsua_acc_set_registration(PyObject *pSelf, 
					       PyObject *pArgs)
{    
    int renew;	
    int acc_id;
    int status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &acc_id, &renew)) {
        return NULL;
    }
	
    status = pjsua_acc_set_registration(acc_id, renew);
	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_get_info
 */
static PyObject *py_pjsua_acc_get_info(PyObject *pSelf, PyObject *pArgs)
{    	
    int acc_id;
    PyObj_pjsua_acc_info * obj;
    pjsua_acc_info info;
    int status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &acc_id)) {
        return NULL;
    }
    
    status = pjsua_acc_get_info(acc_id, &info);
    if (status == PJ_SUCCESS) {
	obj = (PyObj_pjsua_acc_info*)
	      PyObj_pjsua_acc_info_new(&PyTyp_pjsua_acc_info, NULL, NULL);
	PyObj_pjsua_acc_info_import(obj, &info);
        return (PyObject*)obj;
    } else {
	return Py_BuildValue("");
    }
}

/*
 * py_pjsua_enum_accs
 */
static PyObject *py_pjsua_enum_accs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    pjsua_acc_id id[PJSUA_MAX_ACC];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_accs(id, &c);
    if (status != PJ_SUCCESS)
	c = 0;
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) {
        PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
    }
    
    return (PyObject*)list;
}

/*
 * py_pjsua_acc_enum_info
 */
static PyObject *py_pjsua_acc_enum_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    pjsua_acc_info info[PJSUA_MAX_ACC];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_acc_enum_info(info, &c);
    if (status != PJ_SUCCESS)
	c = 0;

    list = PyList_New(c);
    for (i = 0; i < c; i++) {
        PyObj_pjsua_acc_info *obj;
        obj = (PyObj_pjsua_acc_info *)
	      PyObj_pjsua_acc_info_new(&PyTyp_pjsua_acc_info, NULL, NULL);

	PyObj_pjsua_acc_info_import(obj, &info[i]);

        PyList_SetItem(list, i, (PyObject*)obj);
    }
    
    return (PyObject*)list;
}

/*
 * py_pjsua_acc_set_transport
 */
static PyObject *py_pjsua_acc_set_transport(PyObject *pSelf, PyObject *pArgs)
{    	
    int acc_id, transport_id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &acc_id, &transport_id)) {
        return NULL;
    }	
    
    status = pjsua_acc_set_transport(acc_id, transport_id);
    
    
    return Py_BuildValue("i", status);
}


/*
 * py_pjsua_acc_pres_notify
 */
static PyObject *py_pjsua_acc_pres_notify(PyObject *pSelf, 
					  PyObject *pArgs)
{
    int acc_id, state;
    PyObject *arg_pres, *arg_msg_data, *arg_reason;
    void *srv_pres;
    pjsua_msg_data msg_data;
    pj_str_t reason;
    pj_bool_t with_body;
    pj_pool_t *pool = NULL;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOiOO", &acc_id, &arg_pres, 
			  &state, &arg_reason, &arg_msg_data))
    {
        return NULL;
    }	
    
    srv_pres = (void*) PyLong_AsLong(arg_pres);
    with_body = (state != PJSIP_EVSUB_STATE_TERMINATED);

    if (arg_reason && PyString_Check(arg_reason)) {
	reason = PyString_ToPJ(arg_reason);
    } else {
	reason = pj_str("");
    }

    pjsua_msg_data_init(&msg_data);
    if (arg_msg_data && arg_msg_data != Py_None) {
        PyObj_pjsua_msg_data *omd = (PyObj_pjsua_msg_data *)arg_msg_data;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_pres_notify(acc_id, (pjsua_srv_pres*)srv_pres,
			       (pjsip_evsub_state)state, NULL,
			       &reason, with_body, &msg_data);
    
    if (pool) {
	pj_pool_release(pool);
    }

    return Py_BuildValue("i", status);
}

static char pjsua_acc_config_default_doc[] =
    "_pjsua.Acc_Config _pjsua.acc_config_default () "
    "Call this function to initialize account config with default values.";
static char pjsua_acc_get_count_doc[] =
    "int _pjsua.acc_get_count () "
    "Get number of current accounts.";
static char pjsua_acc_is_valid_doc[] =
    "int _pjsua.acc_is_valid (int acc_id)  "
    "Check if the specified account ID is valid.";
static char pjsua_acc_set_default_doc[] =
    "int _pjsua.acc_set_default (int acc_id) "
    "Set default account to be used when incoming "
    "and outgoing requests doesn't match any accounts.";
static char pjsua_acc_get_default_doc[] =
    "int _pjsua.acc_get_default () "
    "Get default account.";
static char pjsua_acc_add_doc[] =
    "int, int _pjsua.acc_add (_pjsua.Acc_Config cfg, "
    "int is_default) "
    "Add a new account to pjsua. PJSUA must have been initialized "
    "(with pjsua_init()) before calling this function.";
static char pjsua_acc_add_local_doc[] =
    "int,int _pjsua.acc_add_local (int tid, "
    "int is_default) "
    "Add a local account. A local account is used to identify "
    "local endpoint instead of a specific user, and for this reason, "
    "a transport ID is needed to obtain the local address information.";
static char pjsua_acc_del_doc[] =
    "int _pjsua.acc_del (int acc_id) "
    "Delete account.";
static char pjsua_acc_modify_doc[] =
    "int _pjsua.acc_modify (int acc_id, _pjsua.Acc_Config cfg) "
    "Modify account information.";
static char pjsua_acc_set_online_status_doc[] =
    "int _pjsua.acc_set_online_status2(int acc_id, int is_online) "
    "Modify account's presence status to be advertised "
    "to remote/presence subscribers.";
static char pjsua_acc_set_online_status2_doc[] =
    "int _pjsua.acc_set_online_status (int acc_id, int is_online, "
                                         "int activity_id, string activity_text) "
    "Modify account's presence status to be advertised "
    "to remote/presence subscribers.";
static char pjsua_acc_set_registration_doc[] =
    "int _pjsua.acc_set_registration (int acc_id, int renew) "
    "Update registration or perform unregistration.";
static char pjsua_acc_get_info_doc[] =
    "_pjsua.Acc_Info _pjsua.acc_get_info (int acc_id) "
    "Get account information.";
static char pjsua_enum_accs_doc[] =
    "int[] _pjsua.enum_accs () "
    "Enum accounts all account ids.";
static char pjsua_acc_enum_info_doc[] =
    "_pjsua.Acc_Info[] _pjsua.acc_enum_info () "
    "Enum accounts info.";

/* END OF LIB ACCOUNT */

/* LIB BUDDY */



/*
 * py_pjsua_buddy_config_default
 */
static PyObject *py_pjsua_buddy_config_default(PyObject *pSelf, 
					       PyObject *pArgs)
{    
    PyObj_pjsua_buddy_config *obj;	
    pjsua_buddy_config cfg;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    pjsua_buddy_config_default(&cfg);
    obj = (PyObj_pjsua_buddy_config *) 
	  PyObj_pjsua_buddy_config_new(&PyTyp_pjsua_buddy_config, NULL, NULL);
    PyObj_pjsua_buddy_config_import(obj, &cfg);
    
    return (PyObject *)obj;
}

/*
 * py_pjsua_get_buddy_count
 */
static PyObject *py_pjsua_get_buddy_count(PyObject *pSelf, PyObject *pArgs)
{    
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    return Py_BuildValue("i", pjsua_get_buddy_count());
}

/*
 * py_pjsua_buddy_is_valid
 */
static PyObject *py_pjsua_buddy_is_valid(PyObject *pSelf, PyObject *pArgs)
{    
    int id;
    int is_valid;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }
    is_valid = pjsua_buddy_is_valid(id);
	
    return Py_BuildValue("i", is_valid);
}

/*
 * py_pjsua_enum_buddies
 */
static PyObject *py_pjsua_enum_buddies(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    pjsua_buddy_id id[PJSUA_MAX_BUDDIES];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_buddies(id, &c);
    if (status != PJ_SUCCESS)
	c = 0;

    list = PyList_New(c);
    for (i = 0; i < c; i++) {
        PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
    }
    
    return (PyObject*)list;
}

/*
 * py_pjsua_buddy_find
 */
static PyObject *py_pjsua_buddy_find(PyObject *pSelf, PyObject *pArgs)
{    
    PyObject *pURI;
    pj_str_t uri;
    pjsua_buddy_id buddy_id;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &pURI)) {
        return NULL;
    }

    if (!PyString_Check(pURI))
	return Py_BuildValue("i", PJSUA_INVALID_ID);

    uri = PyString_ToPJ(pURI);
    buddy_id = pjsua_buddy_find(&uri);

    return Py_BuildValue("i", buddy_id);
}

/*
 * py_pjsua_buddy_get_info
 */
static PyObject *py_pjsua_buddy_get_info(PyObject *pSelf, PyObject *pArgs)
{    	
    int buddy_id;
    pjsua_buddy_info info;
    int status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id)) {
        return NULL;
    }

    status = pjsua_buddy_get_info(buddy_id, &info);
    if (status == PJ_SUCCESS) {
	PyObj_pjsua_buddy_info *obj;

	obj = (PyObj_pjsua_buddy_info *)
	      PyObj_pjsua_buddy_config_new(&PyTyp_pjsua_buddy_info, 
					   NULL, NULL);
	PyObj_pjsua_buddy_info_import(obj, &info);	
        return (PyObject*)obj;
    } else {
	return Py_BuildValue("");
    }
}

/*
 * py_pjsua_buddy_add
 */
static PyObject *py_pjsua_buddy_add(PyObject *pSelf, PyObject *pArgs)
{   
    PyObject *pCfg;
    int buddy_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &pCfg)) {
        return NULL;
    }

    if (pCfg != Py_None) {
	pjsua_buddy_config cfg;
	PyObj_pjsua_buddy_config *bc;

        bc = (PyObj_pjsua_buddy_config *)pCfg;

	pjsua_buddy_config_default(&cfg);
        PyObj_pjsua_buddy_config_export(&cfg, bc);  
    
        status = pjsua_buddy_add(&cfg, &buddy_id);

    } else {
        status = PJ_EINVAL;
	buddy_id = PJSUA_INVALID_ID;
    }
    return Py_BuildValue("ii", status, buddy_id);
}

/*
 * py_pjsua_buddy_del
 */
static PyObject *py_pjsua_buddy_del(PyObject *pSelf, PyObject *pArgs)
{    
    int buddy_id;
    int status;
    PyObject *user_data;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id)) {
        return NULL;
    }

    user_data = (PyObject*) pjsua_buddy_get_user_data(buddy_id);
    Py_XDECREF(user_data);

    status = pjsua_buddy_del(buddy_id);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_buddy_set_user_data
 */
static PyObject *py_pjsua_buddy_set_user_data(PyObject *pSelf, PyObject *pArgs)
{    
    int buddy_id;
    int status;
    PyObject *user_data, *old_user_data;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &buddy_id, &user_data)) {
        return NULL;
    }

    if (!pjsua_buddy_is_valid(buddy_id)) {
	return Py_BuildValue("i", 0);
    }

    old_user_data = (PyObject*) pjsua_buddy_get_user_data(buddy_id);

    status = pjsua_buddy_set_user_data(buddy_id, (void*)user_data);

    if (status == PJ_SUCCESS) {
	Py_XINCREF(user_data);
	Py_XDECREF(old_user_data);
    }

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_buddy_get_user_data
 */
static PyObject *py_pjsua_buddy_get_user_data(PyObject *pSelf, PyObject *pArgs)
{    
    int buddy_id;
    PyObject *user_data;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id)) {
        return NULL;
    }

    user_data = (PyObject*) pjsua_buddy_get_user_data(buddy_id);

    return user_data? Py_BuildValue("O", user_data) : Py_BuildValue("");
}

/*
 * py_pjsua_buddy_subscribe_pres
 */
static PyObject *py_pjsua_buddy_subscribe_pres(PyObject *pSelf, 
					       PyObject *pArgs)
{
    int buddy_id;
    int status;
    int subscribe;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &buddy_id, &subscribe)) {
        return NULL;
    }

    status = pjsua_buddy_subscribe_pres(buddy_id, subscribe);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_pres_dump
 */
static PyObject *py_pjsua_pres_dump(PyObject *pSelf, PyObject *pArgs)
{    
    int verbose;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &verbose)) {
        return NULL;
    }

    pjsua_pres_dump(verbose);	

    return Py_BuildValue("");
}

/*
 * py_pjsua_im_send
 */
static PyObject *py_pjsua_im_send(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t *mime_type, tmp_mime_type;
    pj_str_t to, content;
    PyObject *pTo;
    PyObject *pMimeType;
    PyObject *pContent;
    pjsua_msg_data msg_data;
    PyObject *pMsgData;
    int user_data;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOOOOi", &acc_id, 
		&pTo, &pMimeType, &pContent, &pMsgData, &user_data))
    {
        return NULL;
    }

    if (pMimeType != Py_None) {
        mime_type = &tmp_mime_type;
	tmp_mime_type = PyString_ToPJ(pMimeType);
    } else {
        mime_type = NULL;
    }

    to = PyString_ToPJ(pTo);
    content = PyString_ToPJ(pContent);

    pjsua_msg_data_init(&msg_data);

    if (pMsgData != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)pMsgData;
	msg_data.content_type = PyString_ToPJ(omd->content_type);
	msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_im_send(acc_id, &to, mime_type, &content, 
			   &msg_data, NULL, NULL, (void*)(long)user_data);
    if (pool)
	pj_pool_release(pool);
    
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_im_typing
 */
static PyObject *py_pjsua_im_typing(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t to;
    PyObject *pTo;
    int is_typing;
    pjsua_msg_data msg_data;
    PyObject *pMsgData;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOiO", &acc_id, &pTo, &is_typing, 
			  &pMsgData)) 
    {
        return NULL;
    }
	
    to = PyString_ToPJ(pTo);

    pjsua_msg_data_init(&msg_data);

    if (pMsgData != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)pMsgData;
	msg_data.content_type = PyString_ToPJ(omd->content_type);
	msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);

        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_im_typing(acc_id, &to, is_typing, &msg_data);

    if (pool)
	pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

static char pjsua_buddy_config_default_doc[] =
    "_pjsua.Buddy_Config _pjsua.buddy_config_default () "
    "Set default values to the buddy config.";
static char pjsua_get_buddy_count_doc[] =
    "int _pjsua.get_buddy_count () "
    "Get total number of buddies.";
static char pjsua_buddy_is_valid_doc[] =
    "int _pjsua.buddy_is_valid (int buddy_id) "
    "Check if buddy ID is valid.";
static char pjsua_enum_buddies_doc[] =
    "int[] _pjsua.enum_buddies () "
    "Enum buddy IDs.";
static char pjsua_buddy_get_info_doc[] =
    "_pjsua.Buddy_Info _pjsua.buddy_get_info (int buddy_id) "
    "Get detailed buddy info.";
static char pjsua_buddy_add_doc[] =
    "int,int _pjsua.buddy_add (_pjsua.Buddy_Config cfg) "
    "Add new buddy.";
static char pjsua_buddy_del_doc[] =
    "int _pjsua.buddy_del (int buddy_id) "
    "Delete buddy.";
static char pjsua_buddy_subscribe_pres_doc[] =
    "int _pjsua.buddy_subscribe_pres (int buddy_id, int subscribe) "
    "Enable/disable buddy's presence monitoring.";
static char pjsua_pres_dump_doc[] =
    "void _pjsua.pres_dump (int verbose) "
    "Dump presence subscriptions to log file.";
static char pjsua_im_send_doc[] =
    "int _pjsua.im_send (int acc_id, string to, string mime_type, "
    "string content, _pjsua.Msg_Data msg_data, int user_data) "
    "Send instant messaging outside dialog, using the specified account "
    "for route set and authentication.";
static char pjsua_im_typing_doc[] =
    "int _pjsua.im_typing (int acc_id, string to, int is_typing, "
    "_pjsua.Msg_Data msg_data) "
    "Send typing indication outside dialog.";

/* END OF LIB BUDDY */

/* LIB MEDIA */


/*
 * py_pjsua_conf_get_max_ports
 */
static PyObject *py_pjsua_conf_get_max_ports(PyObject *pSelf, PyObject *pArgs)
{    
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    return Py_BuildValue("i", pjsua_conf_get_max_ports());
}

/*
 * py_pjsua_conf_get_active_ports
 */
static PyObject *py_pjsua_conf_get_active_ports(PyObject *pSelf, 
						PyObject *pArgs)
{    
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    return Py_BuildValue("i", pjsua_conf_get_active_ports());
}

/*
 * py_pjsua_enum_conf_ports
 */
static PyObject *py_pjsua_enum_conf_ports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    pjsua_conf_port_id id[PJSUA_MAX_CONF_PORTS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_conf_ports(id, &c);
    if (status != PJ_SUCCESS)
	c = 0;
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) {
        PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
    }
    
    return (PyObject*)list;
}

/*
 * py_pjsua_conf_get_port_info
 */
static PyObject *py_pjsua_conf_get_port_info(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    PyObj_pjsua_conf_port_info *ret;
    pjsua_conf_port_info info;
    int status;	
    unsigned i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }
    
    status = pjsua_conf_get_port_info(id, &info);
    ret = (PyObj_pjsua_conf_port_info *)
	  conf_port_info_new(&PyTyp_pjsua_conf_port_info, NULL, NULL);
    ret->bits_per_sample = info.bits_per_sample;
    ret->channel_count = info.channel_count;
    ret->clock_rate = info.clock_rate;
    ret->name = PyString_FromPJ(&info.name);
    ret->samples_per_frame = info.samples_per_frame;
    ret->slot_id = info.slot_id;
    Py_XDECREF(ret->listeners);
    ret->listeners = PyList_New(info.listener_cnt);
    for (i = 0; i < info.listener_cnt; i++) {
	PyObject *item = Py_BuildValue("i",info.listeners[i]);
	PyList_SetItem(ret->listeners, i, item);
    }
    return (PyObject*)ret;
}

/*
 * py_pjsua_conf_remove_port
 */
static PyObject *py_pjsua_conf_remove_port(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }	
    
    status = pjsua_conf_remove_port(id);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_connect
 */
static PyObject *py_pjsua_conf_connect(PyObject *pSelf, PyObject *pArgs)
{    	
    int source, sink;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &source, &sink)) {
        return NULL;
    }	
    
    status = pjsua_conf_connect(source, sink);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_disconnect
 */
static PyObject *py_pjsua_conf_disconnect(PyObject *pSelf, PyObject *pArgs)
{    	
    int source, sink;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &source, &sink)) {
        return NULL;
    }	
    
    status = pjsua_conf_disconnect(source, sink);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_set_tx_level
 */
static PyObject *py_pjsua_conf_set_tx_level(PyObject *pSelf, PyObject *pArgs)
{    	
    int slot;
    float level;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "if", &slot, &level)) {
        return NULL;
    }	
    
    status = pjsua_conf_adjust_tx_level(slot, level);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_set_rx_level
 */
static PyObject *py_pjsua_conf_set_rx_level(PyObject *pSelf, PyObject *pArgs)
{    	
    int slot;
    float level;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "if", &slot, &level)) {
        return NULL;
    }	
    
    status = pjsua_conf_adjust_rx_level(slot, level);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_get_signal_level
 */
static PyObject *py_pjsua_conf_get_signal_level(PyObject *pSelf, 
						PyObject *pArgs)
{    	
    int slot;
    unsigned tx_level, rx_level;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &slot)) {
        return NULL;
    }	
    
    status = pjsua_conf_get_signal_level(slot, &tx_level, &rx_level);
    
    return Py_BuildValue("iff", status, (float)(tx_level/255.0), 
			 (float)(rx_level/255.0));
}

/*
 * py_pjsua_player_create
 */
static PyObject *py_pjsua_player_create(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int options;
    PyObject *pFilename;
    pj_str_t filename;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "Oi", &pFilename, &options)) {
        return NULL;
    }

    filename = PyString_ToPJ(pFilename);
    status = pjsua_player_create(&filename, options, &id);
    
    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_playlist_create
 */
static PyObject *py_pjsua_playlist_create(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int options;
    PyObject *pLabel, *pFileList;
    pj_str_t label;
    int count;
    pj_str_t files[64];
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OOi", &pLabel, &pFileList, &options)) {
        return NULL;
    }

    label = PyString_ToPJ(pLabel);
    if (!PyList_Check(pFileList))
	return Py_BuildValue("ii", PJ_EINVAL, PJSUA_INVALID_ID);

    count = 0;
    for (count=0; count<PyList_Size(pFileList) && 
		  count<PJ_ARRAY_SIZE(files); ++count) 
    {
	files[count] = PyString_ToPJ(PyList_GetItem(pFileList, count));
    }

    status = pjsua_playlist_create(files, count, &label, options, &id);
    
    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_player_get_conf_port
 */
static PyObject *py_pjsua_player_get_conf_port(PyObject *pSelf, 
					       PyObject *pArgs)
{    	
    
    int id, port_id;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }	
    
    port_id = pjsua_player_get_conf_port(id);
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_player_set_pos
 */
static PyObject *py_pjsua_player_set_pos(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int samples;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &id, &samples)) {
        return NULL;
    }	
    
    if (samples < 0)
	samples = 0;

    status = pjsua_player_set_pos(id, samples);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_player_destroy
 */
static PyObject *py_pjsua_player_destroy(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }	
    
    status = pjsua_player_destroy(id);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_recorder_create
 */
static PyObject *py_pjsua_recorder_create(PyObject *pSelf, PyObject *pArgs)
{    	
    int id, options;
    int max_size;
    PyObject *pFilename, *pEncParam;
    pj_str_t filename;
    int enc_type;
    
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OiOii", &pFilename, &enc_type, &pEncParam,
			  &max_size, &options))
    {
        return NULL;
    }

    filename = PyString_ToPJ(pFilename);

    status = pjsua_recorder_create(&filename, enc_type, NULL, max_size,
				   options, &id);

    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_recorder_get_conf_port
 */
static PyObject *py_pjsua_recorder_get_conf_port(PyObject *pSelf, 
						 PyObject *pArgs)
{    	
    
    int id, port_id;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }	
    
    port_id = pjsua_recorder_get_conf_port(id);
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_recorder_destroy
 */
static PyObject *py_pjsua_recorder_destroy(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id)) {
        return NULL;
    }	
    
    status = pjsua_recorder_destroy(id);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_enum_snd_devs
 */
static PyObject *py_pjsua_enum_snd_devs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *ret;
    pjmedia_snd_dev_info info[SND_DEV_NUM];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(info);
    status = pjsua_enum_snd_devs(info, &c);
    if (status != PJ_SUCCESS)
	c = 0;
    
    ret = PyList_New(c);
    for (i = 0; i < c; i++)  {
        PyObj_pjmedia_snd_dev_info * obj;

        obj = (PyObj_pjmedia_snd_dev_info *)
	      pjmedia_snd_dev_info_new(&PyTyp_pjmedia_snd_dev_info, 
				       NULL, NULL);
        obj->default_samples_per_sec = info[i].default_samples_per_sec;
        obj->input_count = info[i].input_count;
        obj->output_count = info[i].output_count;
        obj->name = PyString_FromString(info[i].name);

        PyList_SetItem(ret, i, (PyObject *)obj);
    }
    
    return (PyObject*)ret;
}

/*
 * py_pjsua_get_snd_dev
 */
static PyObject *py_pjsua_get_snd_dev(PyObject *pSelf, PyObject *pArgs)
{    	
    int capture_dev, playback_dev;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    status = pjsua_get_snd_dev(&capture_dev, &playback_dev);
    
    return Py_BuildValue("ii", capture_dev, playback_dev);
}

/*
 * py_pjsua_set_snd_dev
 */
static PyObject *py_pjsua_set_snd_dev(PyObject *pSelf, PyObject *pArgs)
{    	
    int capture_dev, playback_dev;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &capture_dev, &playback_dev)) {
        return NULL;
    }	
    
    status = pjsua_set_snd_dev(capture_dev, playback_dev);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_set_null_snd_dev
 */
static PyObject *py_pjsua_set_null_snd_dev(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;	

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    status = pjsua_set_null_snd_dev();

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_set_ec
 */
static PyObject *py_pjsua_set_ec(PyObject *pSelf, PyObject *pArgs)
{    	
    int options;
    int tail_ms;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &tail_ms, &options)) {
        return NULL;
    }	

    status = pjsua_set_ec(tail_ms, options);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_get_ec_tail
 */
static PyObject *py_pjsua_get_ec_tail(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;	
    unsigned tail_ms;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    status = pjsua_get_ec_tail(&tail_ms);
    if (status != PJ_SUCCESS)
	tail_ms = 0;
    
    return Py_BuildValue("i", tail_ms);
}

/*
 * py_pjsua_enum_codecs
 */
static PyObject *py_pjsua_enum_codecs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *ret;
    pjsua_codec_info info[PJMEDIA_CODEC_MGR_MAX_CODECS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(info);
    status = pjsua_enum_codecs(info, &c);
    if (status != PJ_SUCCESS)
	c = 0;
    
    ret = PyList_New(c);
    for (i = 0; i < c; i++)  {
        PyObj_pjsua_codec_info * obj;
        obj = (PyObj_pjsua_codec_info *)
	      codec_info_new(&PyTyp_pjsua_codec_info, NULL, NULL);
        obj->codec_id = PyString_FromPJ(&info[i].codec_id);
        obj->priority = info[i].priority;

        PyList_SetItem(ret, i, (PyObject *)obj);
    }

    return (PyObject*)ret;
}

/*
 * py_pjsua_codec_set_priority
 */
static PyObject *py_pjsua_codec_set_priority(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;	
    PyObject *pCodecId;
    pj_str_t codec_id;
    int priority;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "Oi", &pCodecId, &priority)) {
        return NULL;
    }

    codec_id = PyString_ToPJ(pCodecId);
    if (priority < 0)
	priority = 0;
    if (priority > 255)
	priority = 255;

    status = pjsua_codec_set_priority(&codec_id, (pj_uint8_t)priority);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_codec_get_param
 */
static PyObject *py_pjsua_codec_get_param(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;	
    PyObject *pCodecId;
    pj_str_t codec_id;
    pjmedia_codec_param param;
    PyObj_pjmedia_codec_param *ret;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &pCodecId)) {
        return NULL;
    }	

    codec_id = PyString_ToPJ(pCodecId);

    status = pjsua_codec_get_param(&codec_id, &param);
    if (status != PJ_SUCCESS)
	return Py_BuildValue("");

    ret = (PyObj_pjmedia_codec_param *)
	  pjmedia_codec_param_new(&PyTyp_pjmedia_codec_param, NULL, NULL);

    ret->info->avg_bps = param.info.avg_bps;
    ret->info->channel_cnt = param.info.channel_cnt;
    ret->info->clock_rate = param.info.clock_rate;
    ret->info->frm_ptime = param.info.frm_ptime;
    ret->info->pcm_bits_per_sample = param.info.pcm_bits_per_sample;
    ret->info->pt = param.info.pt;
    ret->setting->cng = param.setting.cng;
    //ret->setting->dec_fmtp_mode = param.setting.dec_fmtp_mode;
    //ret->setting->enc_fmtp_mode = param.setting.enc_fmtp_mode;
    ret->setting->frm_per_pkt = param.setting.frm_per_pkt;
    ret->setting->penh = param.setting.penh;
    ret->setting->plc = param.setting.plc;
    ret->setting->vad = param.setting.vad;

    return (PyObject*)ret;
}


/*
 * py_pjsua_codec_set_param
 */
static PyObject *py_pjsua_codec_set_param(PyObject *pSelf, PyObject *pArgs)
{    	
    int status;	
    PyObject *pCodecId, *pCodecParam;
    pj_str_t codec_id;
    pjmedia_codec_param param;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OO", &pCodecId, &pCodecParam)) {
        return NULL;
    }	

    codec_id = PyString_ToPJ(pCodecId);

    if (pCodecParam != Py_None) {
	PyObj_pjmedia_codec_param *obj;

        obj = (PyObj_pjmedia_codec_param *)pCodecParam;

        param.info.avg_bps = obj->info->avg_bps;
        param.info.channel_cnt = obj->info->channel_cnt;
        param.info.clock_rate = obj->info->clock_rate;
        param.info.frm_ptime = obj->info->frm_ptime;
        param.info.pcm_bits_per_sample = obj->info->pcm_bits_per_sample;
        param.info.pt = obj->info->pt;
        param.setting.cng = obj->setting->cng;
        //param.setting.dec_fmtp_mode = obj->setting->dec_fmtp_mode;
        //param.setting.enc_fmtp_mode = obj->setting->enc_fmtp_mode;
        param.setting.frm_per_pkt = obj->setting->frm_per_pkt;
        param.setting.penh = obj->setting->penh;
        param.setting.plc = obj->setting->plc;
        param.setting.vad = obj->setting->vad;
        status = pjsua_codec_set_param(&codec_id, &param);

    } else {
        status = pjsua_codec_set_param(&codec_id, NULL);
    }

    return Py_BuildValue("i", status);
}

static char pjsua_conf_get_max_ports_doc[] =
    "int _pjsua.conf_get_max_ports () "
    "Get maxinum number of conference ports.";
static char pjsua_conf_get_active_ports_doc[] =
    "int _pjsua.conf_get_active_ports () "
    "Get current number of active ports in the bridge.";
static char pjsua_enum_conf_ports_doc[] =
    "int[] _pjsua.enum_conf_ports () "
    "Enumerate all conference ports.";
static char pjsua_conf_get_port_info_doc[] =
    "_pjsua.Conf_Port_Info _pjsua.conf_get_port_info (int id) "
    "Get information about the specified conference port";
static char pjsua_conf_remove_port_doc[] =
    "int _pjsua.conf_remove_port (int id) "
    "Remove arbitrary slot from the conference bridge. "
    "Application should only call this function "
    "if it registered the port manually.";
static char pjsua_conf_connect_doc[] =
    "int _pjsua.conf_connect (int source, int sink) "
    "Establish unidirectional media flow from souce to sink. "
    "One source may transmit to multiple destinations/sink. "
    "And if multiple sources are transmitting to the same sink, "
    "the media will be mixed together. Source and sink may refer "
    "to the same ID, effectively looping the media. "
    "If bidirectional media flow is desired, application "
    "needs to call this function twice, with the second "
    "one having the arguments reversed.";
static char pjsua_conf_disconnect_doc[] =
    "int _pjsua.conf_disconnect (int source, int sink) "
    "Disconnect media flow from the source to destination port.";
static char pjsua_player_create_doc[] =
    "int, int _pjsua.player_create (string filename, int options) "
    "Create a file player, and automatically connect "
    "this player to the conference bridge.";
static char pjsua_player_get_conf_port_doc[] =
    "int _pjsua.player_get_conf_port (int) "
    "Get conference port ID associated with player.";
static char pjsua_player_set_pos_doc[] =
    "int _pjsua.player_set_pos (int id, int samples) "
    "Set playback position.";
static char pjsua_player_destroy_doc[] =
    "int _pjsua.player_destroy (int id) "
    "Close the file, remove the player from the bridge, "
    "and free resources associated with the file player.";
static char pjsua_recorder_create_doc[] =
    "int, int _pjsua.recorder_create (string filename, "
    "int enc_type, int enc_param, int max_size, int options) "
    "Create a file recorder, and automatically connect this recorder "
    "to the conference bridge. The recorder currently supports recording "
    "WAV file, and on Windows, MP3 file. The type of the recorder to use "
    "is determined by the extension of the file (e.g. '.wav' or '.mp3').";
static char pjsua_recorder_get_conf_port_doc[] =
    "int _pjsua.recorder_get_conf_port (int id) "
    "Get conference port associated with recorder.";
static char pjsua_recorder_destroy_doc[] =
    "int _pjsua.recorder_destroy (int id) "
    "Destroy recorder (this will complete recording).";
static char pjsua_enum_snd_devs_doc[] =
    "_pjsua.PJMedia_Snd_Dev_Info[] _pjsua.enum_snd_devs (int count) "
    "Enum sound devices.";
static char pjsua_get_snd_dev_doc[] =
    "int, int _pjsua.get_snd_dev () "
    "Get currently active sound devices. "
    "If sound devices has not been created "
    "(for example when pjsua_start() is not called), "
    "it is possible that the function returns "
    "PJ_SUCCESS with -1 as device IDs.";
static char pjsua_set_snd_dev_doc[] =
    "int _pjsua.set_snd_dev (int capture_dev, int playback_dev) "
    "Select or change sound device. Application may call this function "
    "at any time to replace current sound device.";
static char pjsua_set_null_snd_dev_doc[] =
    "int _pjsua.set_null_snd_dev () "
    "Set pjsua to use null sound device. The null sound device only "
    "provides the timing needed by the conference bridge, and will not "
    "interract with any hardware.";
static char pjsua_set_ec_doc[] =
    "int _pjsua.set_ec (int tail_ms, int options) "
    "Configure the echo canceller tail length of the sound port.";
static char pjsua_get_ec_tail_doc[] =
    "int _pjsua.get_ec_tail () "
    "Get current echo canceller tail length.";
static char pjsua_enum_codecs_doc[] =
    "_pjsua.Codec_Info[] _pjsua.enum_codecs () "
    "Enum all supported codecs in the system.";
static char pjsua_codec_set_priority_doc[] =
    "int _pjsua.codec_set_priority (string id, int priority) "
    "Change codec priority.";
static char pjsua_codec_get_param_doc[] =
    "_pjsua.PJMedia_Codec_Param _pjsua.codec_get_param (string id) "
    "Get codec parameters";
static char pjsua_codec_set_param_doc[] =
    "int _pjsua.codec_set_param (string id, "
    "_pjsua.PJMedia_Codec_Param param) "
    "Set codec parameters.";

/* END OF LIB MEDIA */

/* LIB CALL */

/*
 * py_pjsua_call_get_max_count
 */
static PyObject *py_pjsua_call_get_max_count(PyObject *pSelf, PyObject *pArgs)
{    	
    int count;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    count = pjsua_call_get_max_count();
    
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_call_get_count
 */
static PyObject *py_pjsua_call_get_count(PyObject *pSelf, PyObject *pArgs)
{    	
    int count;	
    
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    count = pjsua_call_get_count();
    
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_enum_calls
 */
static PyObject *py_pjsua_enum_calls(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *ret;
    pjsua_transport_id id[PJSUA_MAX_CALLS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_calls(id, &c);
    if (status != PJ_SUCCESS)
	c = 0;
    
    ret = PyList_New(c);
    for (i = 0; i < c; i++)  {     
        PyList_SetItem(ret, i, Py_BuildValue("i", id[i]));
    }
    
    return (PyObject*)ret;
}

/*
 * py_pjsua_call_make_call
 */
static PyObject *py_pjsua_call_make_call(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t dst_uri;
    PyObject *pDstUri, *pMsgData, *pUserData;
    unsigned options;
    pjsua_msg_data msg_data;
    int call_id;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOIOO", &acc_id, &pDstUri, &options, 
			  &pUserData, &pMsgData))
    {
        return NULL;
    }
	
    dst_uri = PyString_ToPJ(pDstUri);
    pjsua_msg_data_init(&msg_data);

    if (pMsgData != Py_None) {
	PyObj_pjsua_msg_data * omd;

        omd = (PyObj_pjsua_msg_data *)pMsgData;

        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    Py_XINCREF(pUserData);

    status = pjsua_call_make_call(acc_id, &dst_uri, 
				  options, 0, (void*)pUserData, 
				  &msg_data, &call_id);	
    if (pool != NULL)
	pj_pool_release(pool);
    
    if (status != PJ_SUCCESS) {
    	Py_XDECREF(pUserData);
    }

    return Py_BuildValue("ii", status, call_id);	
}

/*
 * py_pjsua_call_is_active
 */
static PyObject *py_pjsua_call_is_active(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int is_active;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id)) {
        return NULL;
    }	
    
    is_active = pjsua_call_is_active(call_id);
    
    return Py_BuildValue("i", is_active);
}

/*
 * py_pjsua_call_has_media
 */
static PyObject *py_pjsua_call_has_media(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int has_media;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id)) {
        return NULL;
    }	
    
    has_media = pjsua_call_has_media(call_id);

    return Py_BuildValue("i", has_media);
}

/*
 * py_pjsua_call_get_conf_port
 */
static PyObject* py_pjsua_call_get_conf_port(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int port_id;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id)) {
        return NULL;
    }

    port_id = pjsua_call_get_conf_port(call_id);

    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_call_get_info
 */
static PyObject* py_pjsua_call_get_info(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int status;
    PyObj_pjsua_call_info *ret;
    pjsua_call_info info;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id)) {
        return NULL;
    }	
    
    status = pjsua_call_get_info(call_id, &info);
    if (status != PJ_SUCCESS)
	return Py_BuildValue("");

    ret = (PyObj_pjsua_call_info *)call_info_new(&PyTyp_pjsua_call_info,
						 NULL, NULL);
    ret->acc_id = info.acc_id;
    Py_XDECREF(ret->call_id);
    ret->call_id = PyString_FromPJ(&info.call_id);
    ret->conf_slot = info.conf_slot;
    ret->connect_duration = info.connect_duration.sec * 1000 +
			    info.connect_duration.msec;
    ret->total_duration = info.total_duration.sec * 1000 +
			  info.total_duration.msec;
    ret->id = info.id;
    ret->last_status = info.last_status;
    Py_XDECREF(ret->last_status_text);
    ret->last_status_text = PyString_FromPJ(&info.last_status_text);
    Py_XDECREF(ret->local_contact);
    ret->local_contact = PyString_FromPJ(&info.local_contact);
    Py_XDECREF(ret->local_info);
    ret->local_info = PyString_FromPJ(&info.local_info);
    Py_XDECREF(ret->remote_contact);
    ret->remote_contact = PyString_FromPJ(&info.remote_contact);
    Py_XDECREF(ret->remote_info);
    ret->remote_info = PyString_FromPJ(&info.remote_info);
    ret->media_dir = info.media_dir;
    ret->media_status = info.media_status;
    ret->role = info.role;
    ret->state = info.state;
    Py_XDECREF(ret->state_text);
    ret->state_text = PyString_FromPJ(&info.state_text);

    return (PyObject*)ret;
}

/*
 * py_pjsua_call_set_user_data
 */
static PyObject *py_pjsua_call_set_user_data(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    PyObject *pUserData, *old_user_data;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &pUserData)) {
        return NULL;
    }

    old_user_data = (PyObject*) pjsua_call_get_user_data(call_id);

    if (old_user_data == pUserData) {
	return Py_BuildValue("i", PJ_SUCCESS);
    }

    Py_XINCREF(pUserData);
    Py_XDECREF(old_user_data);

    status = pjsua_call_set_user_data(call_id, (void*)pUserData);
    
    if (status != PJ_SUCCESS) {
    	Py_XDECREF(pUserData);
    }

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_get_user_data
 */
static PyObject *py_pjsua_call_get_user_data(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    PyObject *user_data;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id)) {
        return NULL;
    }	
    
    user_data = (PyObject*)pjsua_call_get_user_data(call_id);
    return user_data ? Py_BuildValue("O", user_data) : Py_BuildValue("");
}

/*
 * py_pjsua_call_answer
 */
static PyObject *py_pjsua_call_answer(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t * reason, tmp_reason;
    PyObject *pReason;
    unsigned code;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    pj_pool_t * pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iIOO", &call_id, &code, &pReason, &omdObj)) {
        return NULL;
    }

    if (pReason == Py_None) {
        reason = NULL;
    } else {
	reason = &tmp_reason;
        tmp_reason = PyString_ToPJ(pReason);
    }

    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }
    
    status = pjsua_call_answer(call_id, code, reason, &msg_data);	

    if (pool)
	pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_hangup
 */
static PyObject *py_pjsua_call_hangup(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t *reason, tmp_reason;
    PyObject *pReason;
    unsigned code;
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iIOO", &call_id, &code, &pReason, 
			  &omdObj))
    {
        return NULL;
    }

    if (pReason == Py_None) {
        reason = NULL;
    } else {
        reason = &tmp_reason;
        tmp_reason = PyString_ToPJ(pReason);
    }

    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None)  {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }
    
    status = pjsua_call_hangup(call_id, code, reason, &msg_data);
    if (pool)
	pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_set_hold
 */
static PyObject *py_pjsua_call_set_hold(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &omdObj)) {
        return NULL;
    }

    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;    

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_set_hold(call_id, &msg_data);

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_reinvite
 */
static PyObject *py_pjsua_call_reinvite(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    int unhold;
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &unhold, &omdObj)) {
        return NULL;
    }

    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_reinvite(call_id, unhold, &msg_data);

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_update
 */
static PyObject *py_pjsua_call_update(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    int option;
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &option, &omdObj)) {
        return NULL;
    }

    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_update(call_id, option, &msg_data);	

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_send_request
 */
static PyObject *py_pjsua_call_send_request(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    PyObject *pMethod;
    pj_str_t method;
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOO", &call_id, &pMethod, &omdObj)) {
        return NULL;
    }

    if (!PyString_Check(pMethod)) {
	return NULL;
    }

    method = PyString_ToPJ(pMethod);
    pjsua_msg_data_init(&msg_data);

    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_send_request(call_id, &method, &msg_data);	

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_xfer
 */
static PyObject *py_pjsua_call_xfer(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t dest;
    PyObject *pDstUri;
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOO", &call_id, &pDstUri, &omdObj)) {
        return NULL;
    }

    if (!PyString_Check(pDstUri))
	return NULL;

    dest = PyString_ToPJ(pDstUri);
    pjsua_msg_data_init(&msg_data);

    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_xfer(call_id, &dest, &msg_data);

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_xfer_replaces
 */
static PyObject *py_pjsua_call_xfer_replaces(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    int dest_call_id;
    unsigned options;    
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiIO", &call_id, &dest_call_id, 
			  &options, &omdObj))
    {
        return NULL;
    }

    pjsua_msg_data_init(&msg_data);

    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_xfer_replaces(call_id, dest_call_id, options, 
				      &msg_data);

    if (pool)
	pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_dial_dtmf
 */
static PyObject *py_pjsua_call_dial_dtmf(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    PyObject *pDigits;
    pj_str_t digits;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &pDigits)) {
        return NULL;
    }

    if (!PyString_Check(pDigits))
	return Py_BuildValue("i", PJ_EINVAL);

    digits = PyString_ToPJ(pDigits);
    status = pjsua_call_dial_dtmf(call_id, &digits);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_send_im
 */
static PyObject *py_pjsua_call_send_im(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t content;
    pj_str_t * mime_type, tmp_mime_type;
    PyObject *pMimeType, *pContent, *omdObj;
    pjsua_msg_data msg_data;
    int user_data;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOOOi", &call_id, &pMimeType, &pContent, 
			  &omdObj, &user_data))
    {
        return NULL;
    }

    if (!PyString_Check(pContent))
	return Py_BuildValue("i", PJ_EINVAL);

    content = PyString_ToPJ(pContent);

    if (PyString_Check(pMimeType)) {
        mime_type = &tmp_mime_type;
        tmp_mime_type = PyString_ToPJ(pMimeType);
    } else {
	mime_type = NULL;   
    }

    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data * omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }
    
    status = pjsua_call_send_im(call_id, mime_type, &content, 
				&msg_data, (void*)(long)user_data);

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_send_typing_ind
 */
static PyObject *py_pjsua_call_send_typing_ind(PyObject *pSelf, 
					       PyObject *pArgs)
{    
    int status;
    int call_id;    
    int is_typing;
    pjsua_msg_data msg_data;
    PyObject *omdObj;
    pj_pool_t *pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &is_typing, &omdObj)) {
        return NULL;
    }
	
    pjsua_msg_data_init(&msg_data);
    if (omdObj != Py_None) {
	PyObj_pjsua_msg_data *omd;

        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type = PyString_ToPJ(omd->content_type);
        msg_data.msg_body = PyString_ToPJ(omd->msg_body);
        pool = pjsua_pool_create("pytmp", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
    }

    status = pjsua_call_send_typing_ind(call_id, is_typing, &msg_data);	

    if (pool)
        pj_pool_release(pool);

    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_hangup_all
 */
static PyObject *py_pjsua_call_hangup_all(PyObject *pSelf, PyObject *pArgs)
{    	
    PJ_UNUSED_ARG(pSelf);
    PJ_UNUSED_ARG(pArgs);

    pjsua_call_hangup_all();
    
    return Py_BuildValue("");
}

/*
 * py_pjsua_call_dump
 */
static PyObject *py_pjsua_call_dump(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int with_media;
    PyObject *ret;
    PyObject *pIndent;
    char *buffer;
    char *indent;
    unsigned maxlen;    
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiIO", &call_id, &with_media, 
			  &maxlen, &pIndent))
    {
        return NULL;
    }	

    buffer = (char*) malloc(maxlen * sizeof(char));
    indent = PyString_AsString(pIndent);
    
    status = pjsua_call_dump(call_id, with_media, buffer, maxlen, indent);
    if (status != PJ_SUCCESS) {
	free(buffer);
	return PyString_FromString("");
    }

    ret = PyString_FromString(buffer);
    free(buffer);
    return (PyObject*)ret;
}


/*
 * py_pjsua_dump
 * Dump application states.
 */
static PyObject *py_pjsua_dump(PyObject *pSelf, PyObject *pArgs)
{
    int detail;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &detail)) {
        return NULL;
    }	

    pjsua_dump(detail);

    return Py_BuildValue("");
}


/*
 * py_pj_strerror
 */
static PyObject *py_pj_strerror(PyObject *pSelf, PyObject *pArgs)
{
    int err;
    char err_msg[PJ_ERR_MSG_SIZE];
    pj_str_t ret;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &err)) {
        return NULL;
    }
    
    ret = pj_strerror(err, err_msg, sizeof(err_msg));
    
    return PyString_FromStringAndSize(err_msg, ret.slen);
}


/*
 * py_pj_parse_simple_sip
 */
static PyObject *py_pj_parse_simple_sip(PyObject *pSelf, PyObject *pArgs)
{
    const char *arg_uri;
    pj_pool_t *pool;
    char tmp[PJSIP_MAX_URL_SIZE];
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    PyObject *ret, *item;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "s", &arg_uri)) {
        return NULL;
    }
    
    strncpy(tmp, arg_uri, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';

    pool = pjsua_pool_create("py_pj_parse_simple_sip", 512, 512);
    uri = pjsip_parse_uri(pool, tmp, strlen(tmp), 0);
    
    if (uri == NULL || (!PJSIP_URI_SCHEME_IS_SIP(uri) &&
			!PJSIP_URI_SCHEME_IS_SIPS(uri))) {
	pj_pool_release(pool);
	return Py_BuildValue("");
    }
    
    ret = PyTuple_New(5);
    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);

    /* Scheme */
    item = PyString_FromPJ(pjsip_uri_get_scheme(uri));
    PyTuple_SetItem(ret, 0, item);

    /* Username */
    item = PyString_FromPJ(&sip_uri->user);
    PyTuple_SetItem(ret, 1, item);

    /* Host */
    item = PyString_FromPJ(&sip_uri->host);
    PyTuple_SetItem(ret, 2, item);

    /* Port */
    if (sip_uri->port == 5060) {
	sip_uri->port = 0;
    }
    item = Py_BuildValue("i", sip_uri->port);
    PyTuple_SetItem(ret, 3, item);

    /* Transport */
    if (pj_stricmp2(&sip_uri->transport_param, "udp")) {
	sip_uri->transport_param.ptr = "";
	sip_uri->transport_param.slen = 0;
    }
    item = PyString_FromPJ(&sip_uri->transport_param);
    PyTuple_SetItem(ret, 4, item);

    pj_pool_release(pool);
    return ret;
}


static char pjsua_call_get_max_count_doc[] =
    "int _pjsua.call_get_max_count () "
    "Get maximum number of calls configured in pjsua.";
static char pjsua_call_get_count_doc[] =
    "int _pjsua.call_get_count () "
    "Get number of currently active calls.";
static char pjsua_enum_calls_doc[] =
    "int[] _pjsua.enum_calls () "
    "Get maximum number of calls configured in pjsua.";
static char pjsua_call_make_call_doc[] =
    "int,int _pjsua.call_make_call (int acc_id, string dst_uri, int options,"
    "int user_data, _pjsua.Msg_Data msg_data) "
    "Make outgoing call to the specified URI using the specified account.";
static char pjsua_call_is_active_doc[] =
    "int _pjsua.call_is_active (int call_id) "
    "Check if the specified call has active INVITE session and the INVITE "
    "session has not been disconnected.";
static char pjsua_call_has_media_doc[] =
    "int _pjsua.call_has_media (int call_id) "
    "Check if call has an active media session.";
static char pjsua_call_get_conf_port_doc[] =
    "int _pjsua.call_get_conf_port (int call_id) "
    "Get the conference port identification associated with the call.";
static char pjsua_call_get_info_doc[] =
    "_pjsua.Call_Info _pjsua.call_get_info (int call_id) "
    "Obtain detail information about the specified call.";
static char pjsua_call_set_user_data_doc[] =
    "int _pjsua.call_set_user_data (int call_id, int user_data) "
    "Attach application specific data to the call.";
static char pjsua_call_get_user_data_doc[] =
    "int _pjsua.call_get_user_data (int call_id) "
    "Get user data attached to the call.";
static char pjsua_call_answer_doc[] =
    "int _pjsua.call_answer (int call_id, int code, string reason, "
    "_pjsua.Msg_Data msg_data) "
    "Send response to incoming INVITE request.";
static char pjsua_call_hangup_doc[] =
    "int _pjsua.call_hangup (int call_id, int code, string reason, "
    "_pjsua.Msg_Data msg_data) "
    "Hangup call by using method that is appropriate according "
    "to the call state.";
static char pjsua_call_set_hold_doc[] =
    "int _pjsua.call_set_hold (int call_id, _pjsua.Msg_Data msg_data) "
    "Put the specified call on hold.";
static char pjsua_call_reinvite_doc[] =
    "int _pjsua.call_reinvite (int call_id, int unhold, "
    "_pjsua.Msg_Data msg_data) "
    "Send re-INVITE (to release hold).";
static char pjsua_call_xfer_doc[] =
    "int _pjsua.call_xfer (int call_id, string dest, "
    "_pjsua.Msg_Data msg_data) "
    "Initiate call transfer to the specified address. "
    "This function will send REFER request to instruct remote call party "
    "to initiate a new INVITE session to the specified destination/target.";
static char pjsua_call_xfer_replaces_doc[] =
    "int _pjsua.call_xfer_replaces (int call_id, int dest_call_id, "
    "int options, _pjsua.Msg_Data msg_data) "
    "Initiate attended call transfer. This function will send REFER request "
    "to instruct remote call party to initiate new INVITE session to the URL "
    "of dest_call_id. The party at dest_call_id then should 'replace' the call"
    "with us with the new call from the REFER recipient.";
static char pjsua_call_dial_dtmf_doc[] =
    "int _pjsua.call_dial_dtmf (int call_id, string digits) "
    "Send DTMF digits to remote using RFC 2833 payload formats.";
static char pjsua_call_send_im_doc[] =
    "int _pjsua.call_send_im (int call_id, string mime_type, string content,"
    "_pjsua.Msg_Data msg_data, int user_data) "
    "Send instant messaging inside INVITE session.";
static char pjsua_call_send_typing_ind_doc[] =
    "int _pjsua.call_send_typing_ind (int call_id, int is_typing, "
    "_pjsua.Msg_Data msg_data) "
    "Send IM typing indication inside INVITE session.";
static char pjsua_call_hangup_all_doc[] =
    "void _pjsua.call_hangup_all () "
    "Terminate all calls.";
static char pjsua_call_dump_doc[] =
    "int _pjsua.call_dump (int call_id, int with_media, int maxlen, "
    "string indent) "
    "Dump call and media statistics to string.";

/* END OF LIB CALL */

/*
 * Map of function names to functions
 */
static PyMethodDef py_pjsua_methods[] =
{
    {
        "thread_register", py_pjsua_thread_register, METH_VARARGS, 
         pjsua_thread_register_doc
    },
    {
    	"perror", py_pjsua_perror, METH_VARARGS, pjsua_perror_doc
    },
    {
    	"create", py_pjsua_create, METH_VARARGS, pjsua_create_doc
    },
    {
    	"init", py_pjsua_init, METH_VARARGS, pjsua_init_doc
    },
    {
    	"start", py_pjsua_start, METH_VARARGS, pjsua_start_doc
    },
    {
    	"destroy", py_pjsua_destroy, METH_VARARGS, pjsua_destroy_doc
    },
    {
    	"handle_events", py_pjsua_handle_events, METH_VARARGS,
    	pjsua_handle_events_doc
    },
    {
    	"verify_sip_url", py_pjsua_verify_sip_url, METH_VARARGS,
    	pjsua_verify_sip_url_doc
    },
    {
    	"reconfigure_logging", py_pjsua_reconfigure_logging, METH_VARARGS,
    	pjsua_reconfigure_logging_doc
    },
    {
    	"logging_config_default", py_pjsua_logging_config_default,
    	METH_VARARGS, pjsua_logging_config_default_doc
    },
    {
    	"config_default", py_pjsua_config_default, METH_VARARGS,
    	pjsua_config_default_doc
    },
    {
    	"media_config_default", py_pjsua_media_config_default, METH_VARARGS,
    	pjsua_media_config_default_doc
    },
    
    
    {
    	"msg_data_init", py_pjsua_msg_data_init, METH_VARARGS,
    	pjsua_msg_data_init_doc
    },
    {
        "transport_config_default", py_pjsua_transport_config_default, 
        METH_VARARGS,pjsua_transport_config_default_doc
    },
    {
        "transport_create", py_pjsua_transport_create, METH_VARARGS,
        pjsua_transport_create_doc
    },
    {
        "transport_enum_transports", py_pjsua_enum_transports, METH_VARARGS,
        pjsua_enum_transports_doc
    },
    {
        "transport_get_info", py_pjsua_transport_get_info, METH_VARARGS,
        pjsua_transport_get_info_doc
    },
    {
        "transport_set_enable", py_pjsua_transport_set_enable, METH_VARARGS,
        pjsua_transport_set_enable_doc
    },
    {
       "transport_close", py_pjsua_transport_close, METH_VARARGS,
        pjsua_transport_close_doc
    },
    {
        "acc_config_default", py_pjsua_acc_config_default, METH_VARARGS,
        pjsua_acc_config_default_doc
    },
    {
        "acc_get_count", py_pjsua_acc_get_count, METH_VARARGS,
        pjsua_acc_get_count_doc
    },
    {
        "acc_is_valid", py_pjsua_acc_is_valid, METH_VARARGS,
        pjsua_acc_is_valid_doc
    },
    {
        "acc_set_default", py_pjsua_acc_set_default, METH_VARARGS,
        pjsua_acc_set_default_doc
    },
    {
        "acc_get_default", py_pjsua_acc_get_default, METH_VARARGS,
        pjsua_acc_get_default_doc
    },
    {
        "acc_add", py_pjsua_acc_add, METH_VARARGS,
        pjsua_acc_add_doc
    },
    {
        "acc_add_local", py_pjsua_acc_add_local, METH_VARARGS,
        pjsua_acc_add_local_doc
    },
    {
        "acc_del", py_pjsua_acc_del, METH_VARARGS,
        pjsua_acc_del_doc
    },
    {
        "acc_set_user_data", py_pjsua_acc_set_user_data, METH_VARARGS,
        "Accociate user data with the account"
    },
    {
        "acc_get_user_data", py_pjsua_acc_get_user_data, METH_VARARGS,
        "Get account's user data"
    },
    {
        "acc_modify", py_pjsua_acc_modify, METH_VARARGS,
        pjsua_acc_modify_doc
    },
    {
        "acc_set_online_status", py_pjsua_acc_set_online_status, METH_VARARGS,
        pjsua_acc_set_online_status_doc
    },
    {
        "acc_set_online_status2", py_pjsua_acc_set_online_status2, METH_VARARGS,
        pjsua_acc_set_online_status2_doc
    },
    {
        "acc_set_registration", py_pjsua_acc_set_registration, METH_VARARGS,
        pjsua_acc_set_registration_doc
    },
    {
        "acc_get_info", py_pjsua_acc_get_info, METH_VARARGS,
        pjsua_acc_get_info_doc
    },
    {
	"acc_pres_notify", py_pjsua_acc_pres_notify, METH_VARARGS,
	"Accept or reject subscription request"
    },
    {
        "enum_accs", py_pjsua_enum_accs, METH_VARARGS,
        pjsua_enum_accs_doc
    },
    {
        "acc_enum_info", py_pjsua_acc_enum_info, METH_VARARGS,
        pjsua_acc_enum_info_doc
    },
    {
        "acc_set_transport", py_pjsua_acc_set_transport, METH_VARARGS,
        "Lock transport to use the specified transport"
    },
    {
        "buddy_config_default", py_pjsua_buddy_config_default, METH_VARARGS,
        pjsua_buddy_config_default_doc
    },
    {
        "get_buddy_count", py_pjsua_get_buddy_count, METH_VARARGS,
        pjsua_get_buddy_count_doc
    },
    {
        "buddy_is_valid", py_pjsua_buddy_is_valid, METH_VARARGS,
        pjsua_buddy_is_valid_doc
    },
    {
        "enum_buddies", py_pjsua_enum_buddies, METH_VARARGS,
        pjsua_enum_buddies_doc
    },    
    {
        "buddy_find", py_pjsua_buddy_find, METH_VARARGS,
        "Find buddy with the specified URI"
    },    
    {
        "buddy_get_info", py_pjsua_buddy_get_info, METH_VARARGS,
        pjsua_buddy_get_info_doc
    },
    {
        "buddy_add", py_pjsua_buddy_add, METH_VARARGS,
        pjsua_buddy_add_doc
    },
    {
        "buddy_del", py_pjsua_buddy_del, METH_VARARGS,
        pjsua_buddy_del_doc
    },
    {
        "buddy_set_user_data", py_pjsua_buddy_set_user_data, METH_VARARGS,
        "Associate user data to the buddy object"
    },
    {
        "buddy_get_user_data", py_pjsua_buddy_get_user_data, METH_VARARGS,
        "Get buddy user data"
    },
    {
        "buddy_subscribe_pres", py_pjsua_buddy_subscribe_pres, METH_VARARGS,
        pjsua_buddy_subscribe_pres_doc
    },
    {
        "pres_dump", py_pjsua_pres_dump, METH_VARARGS,
        pjsua_pres_dump_doc
    },
    {
        "im_send", py_pjsua_im_send, METH_VARARGS,
        pjsua_im_send_doc
    },
    {
        "im_typing", py_pjsua_im_typing, METH_VARARGS,
        pjsua_im_typing_doc
    },
        {
        "conf_get_max_ports", py_pjsua_conf_get_max_ports, METH_VARARGS,
        pjsua_conf_get_max_ports_doc
    },
    {
        "conf_get_active_ports", py_pjsua_conf_get_active_ports, METH_VARARGS,
        pjsua_conf_get_active_ports_doc
    },
    {
        "enum_conf_ports", py_pjsua_enum_conf_ports, METH_VARARGS,
        pjsua_enum_conf_ports_doc
    },
    {
        "conf_get_port_info", py_pjsua_conf_get_port_info, METH_VARARGS,
        pjsua_conf_get_port_info_doc
    },
    {
        "conf_remove_port", py_pjsua_conf_remove_port, METH_VARARGS,
        pjsua_conf_remove_port_doc
    },
    {
        "conf_connect", py_pjsua_conf_connect, METH_VARARGS,
        pjsua_conf_connect_doc
    },
    {
        "conf_disconnect", py_pjsua_conf_disconnect, METH_VARARGS,
        pjsua_conf_disconnect_doc
    },
    {
	"conf_set_tx_level", py_pjsua_conf_set_tx_level, METH_VARARGS,
	"Adjust the signal level to be transmitted from the bridge to the" 
	" specified port by making it louder or quieter"
    },
    {
	"conf_set_rx_level", py_pjsua_conf_set_rx_level, METH_VARARGS,
	"Adjust the signal level to be received from the specified port (to"
	" the bridge) by making it louder or quieter"
    },
    {
	"conf_get_signal_level", py_pjsua_conf_get_signal_level, METH_VARARGS,
	"Get last signal level transmitted to or received from the specified port"
    },
    {
        "player_create", py_pjsua_player_create, METH_VARARGS,
        pjsua_player_create_doc
    },
    {
        "playlist_create", py_pjsua_playlist_create, METH_VARARGS,
        "Create WAV playlist"
    },
    {
        "player_get_conf_port", py_pjsua_player_get_conf_port, METH_VARARGS,
        pjsua_player_get_conf_port_doc
    },
    {
        "player_set_pos", py_pjsua_player_set_pos, METH_VARARGS,
        pjsua_player_set_pos_doc
    },
    {
        "player_destroy", py_pjsua_player_destroy, METH_VARARGS,
        pjsua_player_destroy_doc
    },
    {
        "recorder_create", py_pjsua_recorder_create, METH_VARARGS,
        pjsua_recorder_create_doc
    },
    {
        "recorder_get_conf_port", py_pjsua_recorder_get_conf_port, METH_VARARGS,
        pjsua_recorder_get_conf_port_doc
    },
    {
        "recorder_destroy", py_pjsua_recorder_destroy, METH_VARARGS,
        pjsua_recorder_destroy_doc
    },
    {
        "enum_snd_devs", py_pjsua_enum_snd_devs, METH_VARARGS,
        pjsua_enum_snd_devs_doc
    },
    {
        "get_snd_dev", py_pjsua_get_snd_dev, METH_VARARGS,
        pjsua_get_snd_dev_doc
    },
    {
        "set_snd_dev", py_pjsua_set_snd_dev, METH_VARARGS,
        pjsua_set_snd_dev_doc
    },
    {
        "set_null_snd_dev", py_pjsua_set_null_snd_dev, METH_VARARGS,
        pjsua_set_null_snd_dev_doc
    },
    {
        "set_ec", py_pjsua_set_ec, METH_VARARGS,
        pjsua_set_ec_doc
    },
    {
        "get_ec_tail", py_pjsua_get_ec_tail, METH_VARARGS,
        pjsua_get_ec_tail_doc
    },
    {
        "enum_codecs", py_pjsua_enum_codecs, METH_VARARGS,
        pjsua_enum_codecs_doc
    },
    {
        "codec_set_priority", py_pjsua_codec_set_priority, METH_VARARGS,
        pjsua_codec_set_priority_doc
    },
    {
        "codec_get_param", py_pjsua_codec_get_param, METH_VARARGS,
        pjsua_codec_get_param_doc
    },
    {
        "codec_set_param", py_pjsua_codec_set_param, METH_VARARGS,
        pjsua_codec_set_param_doc
    },
    {
        "call_get_max_count", py_pjsua_call_get_max_count, METH_VARARGS,
        pjsua_call_get_max_count_doc
    },
    {
        "call_get_count", py_pjsua_call_get_count, METH_VARARGS,
        pjsua_call_get_count_doc
    },
    {
        "enum_calls", py_pjsua_enum_calls, METH_VARARGS,
        pjsua_enum_calls_doc
    },
    {
        "call_make_call", py_pjsua_call_make_call, METH_VARARGS,
        pjsua_call_make_call_doc
    },
    {
        "call_is_active", py_pjsua_call_is_active, METH_VARARGS,
        pjsua_call_is_active_doc
    },
    {
        "call_has_media", py_pjsua_call_has_media, METH_VARARGS,
        pjsua_call_has_media_doc
    },
    {
        "call_get_conf_port", py_pjsua_call_get_conf_port, METH_VARARGS,
        pjsua_call_get_conf_port_doc
    },
    {
        "call_get_info", py_pjsua_call_get_info, METH_VARARGS,
        pjsua_call_get_info_doc
    },
    {
        "call_set_user_data", py_pjsua_call_set_user_data, METH_VARARGS,
        pjsua_call_set_user_data_doc
    },
    {
        "call_get_user_data", py_pjsua_call_get_user_data, METH_VARARGS,
        pjsua_call_get_user_data_doc
    },
    {
        "call_answer", py_pjsua_call_answer, METH_VARARGS,
        pjsua_call_answer_doc
    },
    {
        "call_hangup", py_pjsua_call_hangup, METH_VARARGS,
        pjsua_call_hangup_doc
    },
    {
        "call_set_hold", py_pjsua_call_set_hold, METH_VARARGS,
        pjsua_call_set_hold_doc
    },
    {
        "call_reinvite", py_pjsua_call_reinvite, METH_VARARGS,
        pjsua_call_reinvite_doc
    },
    {
        "call_update", py_pjsua_call_update, METH_VARARGS,
        "Send UPDATE"
    },
    {
        "call_xfer", py_pjsua_call_xfer, METH_VARARGS,
        pjsua_call_xfer_doc
    },
    {
        "call_xfer_replaces", py_pjsua_call_xfer_replaces, METH_VARARGS,
        pjsua_call_xfer_replaces_doc
    },
    {
        "call_dial_dtmf", py_pjsua_call_dial_dtmf, METH_VARARGS,
        pjsua_call_dial_dtmf_doc
    },
    {
        "call_send_im", py_pjsua_call_send_im, METH_VARARGS,
        pjsua_call_send_im_doc
    },
    {
        "call_send_typing_ind", py_pjsua_call_send_typing_ind, METH_VARARGS,
        pjsua_call_send_typing_ind_doc
    },
    {
        "call_hangup_all", py_pjsua_call_hangup_all, METH_VARARGS,
        pjsua_call_hangup_all_doc
    },
    {
        "call_dump", py_pjsua_call_dump, METH_VARARGS,
        pjsua_call_dump_doc
    },
    {
        "call_send_request", py_pjsua_call_send_request, METH_VARARGS,
        "Send arbitrary request"
    },
    {
	"dump", py_pjsua_dump, METH_VARARGS, "Dump application state"
    },
    {
	"strerror", py_pj_strerror, METH_VARARGS, "Get error message"
    },
    {
	"parse_simple_uri", py_pj_parse_simple_sip, METH_VARARGS, "Parse URI"
    },

    
    {NULL, NULL} /* end of function list */
};



/*
 * Mapping C structs from and to Python objects & initializing object
 */
DL_EXPORT(void)
init_pjsua(void)
{
    PyObject* m = NULL;
#define ADD_CONSTANT(mod,name)	PyModule_AddIntConstant(mod,#name,name)

    
    PyEval_InitThreads();

    if (PyType_Ready(&PyTyp_pjsua_callback) < 0)
        return;
    if (PyType_Ready(&PyTyp_pjsua_config) < 0)
        return;
    if (PyType_Ready(&PyTyp_pjsua_logging_config) < 0)
        return;
    if (PyType_Ready(&PyTyp_pjsua_msg_data) < 0)
        return;
    PyTyp_pjsua_media_config.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjsua_media_config) < 0)
        return;
    PyTyp_pjsip_cred_info.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjsip_cred_info) < 0)
        return;

    /* LIB TRANSPORT */

    if (PyType_Ready(&PyTyp_pjsua_transport_config) < 0)
        return;
    
    if (PyType_Ready(&PyTyp_pjsua_transport_info) < 0)
        return;
    
    /* END OF LIB TRANSPORT */

    /* LIB ACCOUNT */

    
    if (PyType_Ready(&PyTyp_pjsua_acc_config) < 0)
        return;
    if (PyType_Ready(&PyTyp_pjsua_acc_info) < 0)
        return;

    /* END OF LIB ACCOUNT */

    /* LIB BUDDY */

    if (PyType_Ready(&PyTyp_pjsua_buddy_config) < 0)
        return;
    if (PyType_Ready(&PyTyp_pjsua_buddy_info) < 0)
        return;

    /* END OF LIB BUDDY */

    /* LIB MEDIA */
  
    if (PyType_Ready(&PyTyp_pjsua_codec_info) < 0)
        return;

    if (PyType_Ready(&PyTyp_pjsua_conf_port_info) < 0)
        return;

    if (PyType_Ready(&PyTyp_pjmedia_snd_dev_info) < 0)
        return;

    PyTyp_pjmedia_codec_param_info.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjmedia_codec_param_info) < 0)
        return;
    PyTyp_pjmedia_codec_param_setting.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjmedia_codec_param_setting) < 0)
        return;

    if (PyType_Ready(&PyTyp_pjmedia_codec_param) < 0)
        return;

    /* END OF LIB MEDIA */

    /* LIB CALL */

    if (PyType_Ready(&PyTyp_pjsua_call_info) < 0)
        return;

    /* END OF LIB CALL */

    m = Py_InitModule3(
        "_pjsua", py_pjsua_methods, "PJSUA-lib module for python"
    );

    Py_INCREF(&PyTyp_pjsua_callback);
    PyModule_AddObject(m, "Callback", (PyObject *)&PyTyp_pjsua_callback);

    Py_INCREF(&PyTyp_pjsua_config);
    PyModule_AddObject(m, "Config", (PyObject *)&PyTyp_pjsua_config);

    Py_INCREF(&PyTyp_pjsua_media_config);
    PyModule_AddObject(m, "Media_Config", (PyObject *)&PyTyp_pjsua_media_config);

    Py_INCREF(&PyTyp_pjsua_logging_config);
    PyModule_AddObject(m, "Logging_Config", (PyObject *)&PyTyp_pjsua_logging_config);

    Py_INCREF(&PyTyp_pjsua_msg_data);
    PyModule_AddObject(m, "Msg_Data", (PyObject *)&PyTyp_pjsua_msg_data);

    Py_INCREF(&PyTyp_pjsip_cred_info);
    PyModule_AddObject(m, "Pjsip_Cred_Info",
        (PyObject *)&PyTyp_pjsip_cred_info
    );

    /* LIB TRANSPORT */

    Py_INCREF(&PyTyp_pjsua_transport_config);
    PyModule_AddObject
        (m, "Transport_Config", (PyObject *)&PyTyp_pjsua_transport_config);
    
    Py_INCREF(&PyTyp_pjsua_transport_info);
    PyModule_AddObject(m, "Transport_Info", (PyObject *)&PyTyp_pjsua_transport_info);
    

    /* END OF LIB TRANSPORT */

    /* LIB ACCOUNT */

    
    Py_INCREF(&PyTyp_pjsua_acc_config);
    PyModule_AddObject(m, "Acc_Config", (PyObject *)&PyTyp_pjsua_acc_config);
    Py_INCREF(&PyTyp_pjsua_acc_info);
    PyModule_AddObject(m, "Acc_Info", (PyObject *)&PyTyp_pjsua_acc_info);

    /* END OF LIB ACCOUNT */

    /* LIB BUDDY */
    
    Py_INCREF(&PyTyp_pjsua_buddy_config);
    PyModule_AddObject(m, "Buddy_Config", (PyObject *)&PyTyp_pjsua_buddy_config);
    Py_INCREF(&PyTyp_pjsua_buddy_info);
    PyModule_AddObject(m, "Buddy_Info", (PyObject *)&PyTyp_pjsua_buddy_info);

    /* END OF LIB BUDDY */

    /* LIB MEDIA */

    Py_INCREF(&PyTyp_pjsua_codec_info);
    PyModule_AddObject(m, "Codec_Info", (PyObject *)&PyTyp_pjsua_codec_info);
    Py_INCREF(&PyTyp_pjsua_conf_port_info);
    PyModule_AddObject(m, "Conf_Port_Info", (PyObject *)&PyTyp_pjsua_conf_port_info);
    Py_INCREF(&PyTyp_pjmedia_snd_dev_info);
    PyModule_AddObject(m, "PJMedia_Snd_Dev_Info", 
	(PyObject *)&PyTyp_pjmedia_snd_dev_info);
    Py_INCREF(&PyTyp_pjmedia_codec_param_info);
    PyModule_AddObject(m, "PJMedia_Codec_Param_Info", 
	(PyObject *)&PyTyp_pjmedia_codec_param_info);
    Py_INCREF(&PyTyp_pjmedia_codec_param_setting);
    PyModule_AddObject(m, "PJMedia_Codec_Param_Setting", 
	(PyObject *)&PyTyp_pjmedia_codec_param_setting);
    Py_INCREF(&PyTyp_pjmedia_codec_param);
    PyModule_AddObject(m, "PJMedia_Codec_Param", 
	(PyObject *)&PyTyp_pjmedia_codec_param);

    /* END OF LIB MEDIA */

    /* LIB CALL */

    Py_INCREF(&PyTyp_pjsua_call_info);
    PyModule_AddObject(m, "Call_Info", (PyObject *)&PyTyp_pjsua_call_info);

    /* END OF LIB CALL */


    /*
     * Add various constants.
     */
    /* Skip it.. */

#undef ADD_CONSTANT
}
