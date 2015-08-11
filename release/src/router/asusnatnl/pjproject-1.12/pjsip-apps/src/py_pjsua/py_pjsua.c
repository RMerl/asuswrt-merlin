/* $Id: py_pjsua.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "py_pjsua.h"

#define THIS_FILE    "main.c"
#define POOL_SIZE    4000
#define SND_DEV_NUM  64
#define SND_NAME_LEN  64

/* LIB BASE */

static PyObject* obj_log_cb;
static long thread_id;

#define ENTER_PYTHON()	    PyGILState_STATE state = PyGILState_Ensure()
#define LEAVE_PYTHON()	    PyGILState_Release(state)

/*
 * cb_log_cb
 * declares method for reconfiguring logging process for callback struct
 */
static void cb_log_cb(int level, const char *data, int len)
{
	
    /* Ignore if this callback is called from alien thread context,
     * or otherwise it will crash Python.
     */
    if (pj_thread_local_get(thread_id) == 0)
	return;

    if (PyCallable_Check(obj_log_cb))
    {
	ENTER_PYTHON();

        PyObject_CallFunctionObjArgs(
            obj_log_cb, Py_BuildValue("i",level),
            PyString_FromString(data), Py_BuildValue("i",len), NULL
        );

	LEAVE_PYTHON();
    }
}



/*
 * The global callback object.
 */
static PyObj_pjsua_callback * g_obj_callback;


/*
 * cb_on_call_state
 * declares method on_call_state for callback struct
 */
static void cb_on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    if (PyCallable_Check(g_obj_callback->on_call_state))
    {	
        PyObj_pjsip_event * obj;

	ENTER_PYTHON();

	obj = (PyObj_pjsip_event *)PyType_GenericNew(&PyTyp_pjsip_event,
						      NULL, NULL);
		
	obj->event = e;
		
        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_state,
	    Py_BuildValue("i",call_id),
	    obj,
	    NULL
        );

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
    if (PyCallable_Check(g_obj_callback->on_incoming_call))
    {
	PyObj_pjsip_rx_data * obj;

	ENTER_PYTHON();

	obj = (PyObj_pjsip_rx_data *)PyType_GenericNew(&PyTyp_pjsip_rx_data, 
							NULL, NULL);
	obj->rdata = rdata;

        PyObject_CallFunctionObjArgs(
                g_obj_callback->on_incoming_call,
		Py_BuildValue("i",acc_id),
                Py_BuildValue("i",call_id),
		obj,
		NULL
        );

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_call_media_state
 * declares method on_call_media_state for callback struct
 */
static void cb_on_call_media_state(pjsua_call_id call_id)
{
    if (PyCallable_Check(g_obj_callback->on_call_media_state))
    {
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
    if (PyCallable_Check(g_obj_callback->on_dtmf_digit))
    {
	char digit_str[10];

	ENTER_PYTHON();

	pj_ansi_snprintf(digit_str, sizeof(digit_str), "%c", digit);

        PyObject_CallFunctionObjArgs(
	    g_obj_callback->on_dtmf_digit,
	    Py_BuildValue("i",call_id),
	    PyString_FromString(digit_str),
	    NULL
	);

	LEAVE_PYTHON();
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
    if (PyCallable_Check(g_obj_callback->on_call_transfer_request))
    {
	PyObject * ret;
	int cd;

	ENTER_PYTHON();

        ret = PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_transfer_request,
	    Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(dst->ptr, dst->slen),
            Py_BuildValue("i",*code),
	    NULL
        );
	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_Parse(ret,"i",&cd)) {
		    *code = cd;
		}
	    }
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
    if (PyCallable_Check(g_obj_callback->on_call_transfer_status))
    {
	PyObject * ret;
	int cnt;

	ENTER_PYTHON();

        ret = PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_transfer_status,
	    Py_BuildValue("i",call_id),
	    Py_BuildValue("i",status_code),
            PyString_FromStringAndSize(status_text->ptr, status_text->slen),
	    Py_BuildValue("i",final),
            Py_BuildValue("i",*p_cont),
	    NULL
        );
	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_Parse(ret,"i",&cnt)) {
		    *p_cont = cnt;
		}
	    }
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
    if (PyCallable_Check(g_obj_callback->on_call_replace_request))
    {
	PyObject * ret;
	PyObject * txt;
	int cd;
        PyObj_pjsip_rx_data * obj;

	ENTER_PYTHON();

	obj = (PyObj_pjsip_rx_data *)PyType_GenericNew(&PyTyp_pjsip_rx_data,
							NULL, NULL);
        obj->rdata = rdata;

        ret = PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_replace_request,
	    Py_BuildValue("i",call_id),
	    obj,
	    Py_BuildValue("i",*st_code),
            PyString_FromStringAndSize(st_text->ptr, st_text->slen),
	    NULL
        );
	if (ret != NULL) {
	    if (ret != Py_None) {
		if (PyArg_ParseTuple(ret,"iO",&cd, &txt)) {
		    *st_code = cd;
		    st_text->ptr = PyString_AsString(txt);
		    st_text->slen = strlen(PyString_AsString(txt));
		}
	    }
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
    if (PyCallable_Check(g_obj_callback->on_call_replaced))
    {
	ENTER_PYTHON();

        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_call_replaced,
	    Py_BuildValue("i",old_call_id),
	    Py_BuildValue("i",new_call_id),
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
    if (PyCallable_Check(g_obj_callback->on_reg_state))
    {
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
 * cb_on_buddy_state
 * declares method on_buddy state for callback struct
 */
static void cb_on_buddy_state(pjsua_buddy_id buddy_id)
{
    if (PyCallable_Check(g_obj_callback->on_buddy_state))
    {
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
                        const pj_str_t *mime_type, const pj_str_t *body)
{
    if (PyCallable_Check(g_obj_callback->on_pager))
    {
	ENTER_PYTHON();

        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_pager,Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(from->ptr, from->slen),
            PyString_FromStringAndSize(to->ptr, to->slen),
            PyString_FromStringAndSize(contact->ptr, contact->slen),
            PyString_FromStringAndSize(mime_type->ptr, mime_type->slen),
            PyString_FromStringAndSize(body->ptr, body->slen), 
	    NULL
        );

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
                                const pj_str_t *reason)
{
    if (PyCallable_Check(g_obj_callback->on_pager))
    {
	PyObject * obj_user_data;

	ENTER_PYTHON();

	obj_user_data = Py_BuildValue("i", user_data);

        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_pager_status,
	    Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(to->ptr, to->slen),
            PyString_FromStringAndSize(body->ptr, body->slen), 
	    obj_user_data,
            Py_BuildValue("i",status),
	    PyString_FromStringAndSize(reason->ptr,reason->slen),
	    NULL
        );

	LEAVE_PYTHON();
    }
}


/*
 * cb_on_typing
 * declares method on_typing for callback struct
 */
static void cb_on_typing(pjsua_call_id call_id, const pj_str_t *from,
                            const pj_str_t *to, const pj_str_t *contact,
                            pj_bool_t is_typing)
{
    if (PyCallable_Check(g_obj_callback->on_typing))
    {
	ENTER_PYTHON();

        PyObject_CallFunctionObjArgs(
            g_obj_callback->on_typing,Py_BuildValue("i",call_id),
            PyString_FromStringAndSize(from->ptr, from->slen),
            PyString_FromStringAndSize(to->ptr, to->slen),
            PyString_FromStringAndSize(contact->ptr, contact->slen),
            Py_BuildValue("i",is_typing),
	    NULL
        );

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

        for (i = 0; i < PyList_Size(py_hdr_list); i++) 
	{ 
            pj_str_t hname, hvalue;
	    pjsip_generic_string_hdr * new_hdr;
            PyObject * tuple = PyList_GetItem(py_hdr_list, i);

            if (PyTuple_Check(tuple)) 
	    {
                hname.ptr = PyString_AsString(PyTuple_GetItem(tuple,0));
                hname.slen = strlen(PyString_AsString
					(PyTuple_GetItem(tuple,0)));
                hvalue.ptr = PyString_AsString(PyTuple_GetItem(tuple,1));
                hvalue.slen = strlen(PyString_AsString
					(PyTuple_GetItem(tuple,1)));
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
 * translate_hdr_rev
 * internal function
 * translate from pjsip_generic_string_hdr to hdr_list
 */

void translate_hdr_rev(pjsip_generic_string_hdr *hdr, PyObject *py_hdr_list)
{
    int i;
    int len;
    pjsip_generic_string_hdr * p_hdr;

    len = pj_list_size(hdr);
    
    if (len > 0) 
    {
        p_hdr = hdr;
        Py_XDECREF(py_hdr_list);
        py_hdr_list = PyList_New(len);

        for (i = 0; i < len && p_hdr != NULL; i++) 
	{
            PyObject * tuple;
            PyObject * str;

            tuple = PyTuple_New(2);
	    
            str = PyString_FromStringAndSize(p_hdr->name.ptr, p_hdr->name.slen);
            PyTuple_SetItem(tuple, 0, str);
            str = PyString_FromStringAndSize
		(hdr->hvalue.ptr, p_hdr->hvalue.slen);
            PyTuple_SetItem(tuple, 1, str);
            PyList_SetItem(py_hdr_list, i, tuple);
            p_hdr = p_hdr->next;
	}
    }
    
    
}

/*
 * py_pjsua_thread_register
 * !added @ 061206
 */
static PyObject *py_pjsua_thread_register(PyObject *pSelf, PyObject *pArgs)
{
	
    pj_status_t status;	
    const char *name;
    PyObject *py_desc;
    pj_thread_t *thread;
    void *thread_desc;
#if 0
    int size;
    int i;
    int *td;
#endif

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "sO", &name, &py_desc))
    {
         return NULL;
    }
#if 0
    size = PyList_Size(py_desc);
    td = (int *)malloc(size * sizeof(int));
    for (i = 0; i < size; i++) 
    {
	if (!PyArg_Parse(PyList_GetItem(py_desc,i),"i", td[i])) 
	{
	    return NULL;
	}
    }
    thread_desc = td;
#else
    thread_desc = malloc(sizeof(pj_thread_desc));
#endif
    status = pj_thread_register(name, thread_desc, &thread);

    if (status == PJ_SUCCESS)
	status = pj_thread_local_set(thread_id, (void*)1);
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_logging_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_logging_config_default(PyObject *pSelf,
                                                    PyObject *pArgs)
{
    PyObj_pjsua_logging_config *obj;	
    pjsua_logging_config cfg;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    
    pjsua_logging_config_default(&cfg);
    obj = (PyObj_pjsua_logging_config *) PyObj_pjsua_logging_config_new
		(&PyTyp_pjsua_logging_config,NULL,NULL);
    PyObj_pjsua_logging_config_import(obj, &cfg);
    
    return (PyObject *)obj;
}


/*
 * py_pjsua_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_config_default(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjsua_config *obj;
    pjsua_config cfg;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_config_default(&cfg);
    obj = (PyObj_pjsua_config *) PyObj_pjsua_config_new(&PyTyp_pjsua_config, NULL, NULL);
    PyObj_pjsua_config_import(obj, &cfg);

    return (PyObject *)obj;
}


/*
 * py_pjsua_media_config_default
 * !modified @ 051206
 */
static PyObject * py_pjsua_media_config_default(PyObject *pSelf,
                                                PyObject *pArgs)
{
    PyObj_pjsua_media_config *obj;
    pjsua_media_config cfg;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_media_config_default(&cfg);
    obj = (PyObj_pjsua_media_config *)
	  PyType_GenericNew(&PyTyp_pjsua_media_config, NULL, NULL);
    PyObj_pjsua_media_config_import(obj, &cfg);
    return (PyObject *)obj;
}


/*
 * py_pjsua_msg_data_init
 * !modified @ 051206
 */
static PyObject *py_pjsua_msg_data_init(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjsua_msg_data *obj;
    pjsua_msg_data msg;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    pjsua_msg_data_init(&msg);
    obj = (PyObj_pjsua_msg_data *)PyObj_pjsua_msg_data_new(&PyTyp_pjsua_msg_data, NULL, NULL);
    Py_XDECREF(obj->content_type);
    obj->content_type = PyString_FromStringAndSize(
        msg.content_type.ptr, msg.content_type.slen
    );
    Py_XDECREF(obj->msg_body);
    obj->msg_body = PyString_FromStringAndSize(
        msg.msg_body.ptr, msg.msg_body.slen
    );

    translate_hdr_rev((pjsip_generic_string_hdr *)&msg.hdr_list,obj->hdr_list);
    
    return (PyObject *)obj;
}


/*
 * py_pjsua_reconfigure_logging
 */
static PyObject *py_pjsua_reconfigure_logging(PyObject *pSelf, PyObject *pArgs)
{
    PyObject * logObj;
    PyObj_pjsua_logging_config *log;
    pjsua_logging_config cfg;
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &logObj))
    {
        return NULL;
    }
    if (logObj != Py_None) 
    {
        log = (PyObj_pjsua_logging_config *)logObj;
        cfg.msg_logging = log->msg_logging;
        cfg.level = log->level;
        cfg.console_level = log->console_level;
        cfg.decor = log->decor;
        cfg.log_filename.ptr = PyString_AsString(log->log_filename);
        cfg.log_filename.slen = strlen(cfg.log_filename.ptr);
        Py_XDECREF(obj_log_cb);
        obj_log_cb = log->cb;
        Py_INCREF(obj_log_cb);
        cfg.cb = &cb_log_cb;
        status = pjsua_reconfigure_logging(&cfg);
    } else {
        status = pjsua_reconfigure_logging(NULL);
    }
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_pool_create
 */
static PyObject *py_pjsua_pool_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_size_t init_size;
    pj_size_t increment;
    const char * name;
    pj_pool_t *p;
    PyObj_pj_pool *pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "sII", &name, &init_size, &increment))
    {
        return NULL;
    }
    
    p = pjsua_pool_create(name, init_size, increment);
    pool = (PyObj_pj_pool *)PyType_GenericNew(&PyTyp_pj_pool_t, NULL, NULL);
    pool->pool = p;
    return (PyObject *)pool;

}


/*
 * py_pjsua_get_pjsip_endpt
 */
static PyObject *py_pjsua_get_pjsip_endpt(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjsip_endpoint *endpt;
    pjsip_endpoint *e;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    e = pjsua_get_pjsip_endpt();
    endpt = (PyObj_pjsip_endpoint *)PyType_GenericNew(
        &PyTyp_pjsip_endpoint, NULL, NULL
    );
    endpt->endpt = e;
    return (PyObject *)endpt;
}


/*
 * py_pjsua_get_pjmedia_endpt
 */
static PyObject *py_pjsua_get_pjmedia_endpt(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjmedia_endpt *endpt;
    pjmedia_endpt *e;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    e = pjsua_get_pjmedia_endpt();
    endpt = (PyObj_pjmedia_endpt *)PyType_GenericNew(
        &PyTyp_pjmedia_endpt, NULL, NULL
    );
    endpt->endpt = e;
    return (PyObject *)endpt;
}


/*
 * py_pjsua_get_pool_factory
 */
static PyObject *py_pjsua_get_pool_factory(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pj_pool_factory *pool;
    pj_pool_factory *p;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    p = pjsua_get_pool_factory();
    pool = (PyObj_pj_pool_factory *)PyType_GenericNew(
        &PyTyp_pj_pool_factory, NULL, NULL
    );
    pool->pool_fact = p;
    return (PyObject *)pool;
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

    if (!PyArg_ParseTuple(pArgs, "ssi", &sender, &title, &status))
    {
        return NULL;
    }
	
    pjsua_perror(sender, title, status);
    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * py_pjsua_create
 */
static PyObject *py_pjsua_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    status = pjsua_create();
    
    if (status == PJ_SUCCESS) 
    {
	status = pj_thread_local_alloc(&thread_id);
	if (status == PJ_SUCCESS)
	    status = pj_thread_local_set(thread_id, (void*)1);
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

    if (!PyArg_ParseTuple(pArgs, "OOO", &o_ua_cfg, &o_log_cfg, &o_media_cfg))
    {
        return NULL;
    }
    
    pjsua_config_default(&cfg_ua);
    pjsua_logging_config_default(&cfg_log);
    pjsua_media_config_default(&cfg_media);

    if (o_ua_cfg != Py_None) 
    {
	PyObj_pjsua_config *obj_ua_cfg = (PyObj_pjsua_config*)o_ua_cfg;

	PyObj_pjsua_config_export(&cfg_ua, obj_ua_cfg);

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
    	cfg_ua.cb.on_buddy_state = &cb_on_buddy_state;
    	cfg_ua.cb.on_pager = &cb_on_pager;
    	cfg_ua.cb.on_pager_status = &cb_on_pager_status;
    	cfg_ua.cb.on_typing = &cb_on_typing;

        p_cfg_ua = &cfg_ua;

    } else {
        p_cfg_ua = NULL;
    }

    if (o_log_cfg != Py_None) 
    {
	PyObj_pjsua_logging_config * obj_log;

        obj_log = (PyObj_pjsua_logging_config *)o_log_cfg;
        
        PyObj_pjsua_logging_config_export(&cfg_log, obj_log);

        Py_XDECREF(obj_log_cb);
        obj_log_cb = obj_log->cb;
        Py_INCREF(obj_log_cb);

        cfg_log.cb = &cb_log_cb;
        p_cfg_log = &cfg_log;

    } else {
        p_cfg_log = NULL;
    }

    if (o_media_cfg != Py_None) 
    {
	PyObj_pjsua_media_config_export(&cfg_media, 
				        (PyObj_pjsua_media_config*)o_media_cfg);
	p_cfg_media = &cfg_media;

    } else {
        p_cfg_media = NULL;
    }

    status = pjsua_init(p_cfg_ua, p_cfg_log, p_cfg_media);
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_start
 */
static PyObject *py_pjsua_start(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    status = pjsua_start();
    
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_destroy
 */
static PyObject *py_pjsua_destroy(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    status = pjsua_destroy();
    
    return Py_BuildValue("i",status);
}


/*
 * py_pjsua_handle_events
 */
static PyObject *py_pjsua_handle_events(PyObject *pSelf, PyObject *pArgs)
{
    int ret;
    unsigned msec;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &msec))
    {
        return NULL;
    }

    /* Since handle_events() will block, we must wrap it with ALLOW_THREADS
     * construct, or otherwise many Python blocking functions (such as
     * time.sleep(), readline(), etc.) may hang/block indefinitely.
     * See http://www.python.org/doc/current/api/threads.html for more info.
     */
    Py_BEGIN_ALLOW_THREADS
    ret = pjsua_handle_events(msec);
    Py_END_ALLOW_THREADS
    
    return Py_BuildValue("i",ret);
}


/*
 * py_pjsua_verify_sip_url
 */
static PyObject *py_pjsua_verify_sip_url(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    const char *url;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "s", &url))
    {
        return NULL;
    }
    status = pjsua_verify_sip_url(url);
    
    return Py_BuildValue("i",status);
}


/*
 * function doc
 */

static char pjsua_thread_register_doc[] =
    "int py_pjsua.thread_register(string name, int[] desc)";
static char pjsua_perror_doc[] =
    "void py_pjsua.perror (string sender, string title, int status) "
    "Display error message for the specified error code. Parameters: "
    "sender: The log sender field;  "
    "title: Message title for the error; "
    "status: Status code.";

static char pjsua_create_doc[] =
    "int py_pjsua.create (void) "
    "Instantiate pjsua application. Application "
    "must call this function before calling any other functions, to make sure "
    "that the underlying libraries are properly initialized. Once this "
    "function has returned success, application must call pjsua_destroy() "
    "before quitting.";

static char pjsua_init_doc[] =
    "int py_pjsua.init (py_pjsua.Config obj_ua_cfg, "
        "py_pjsua.Logging_Config log_cfg, py_pjsua.Media_Config media_cfg) "
    "Initialize pjsua with the specified settings. All the settings are "
    "optional, and the default values will be used when the config is not "
    "specified. Parameters: "
    "obj_ua_cfg : User agent configuration;  "
    "log_cfg : Optional logging configuration; "
    "media_cfg : Optional media configuration.";

static char pjsua_start_doc[] =
    "int py_pjsua.start (void) "
    "Application is recommended to call this function after all "
    "initialization is done, so that the library can do additional checking "
    "set up additional";

static char pjsua_destroy_doc[] =
    "int py_pjsua.destroy (void) "
    "Destroy pjsua This function must be called once PJSUA is created. To "
    "make it easier for application, application may call this function "
    "several times with no danger.";

static char pjsua_handle_events_doc[] =
    "int py_pjsua.handle_events (int msec_timeout) "
    "Poll pjsua for events, and if necessary block the caller thread for the "
    "specified maximum interval (in miliseconds) Parameters: "
    "msec_timeout: Maximum time to wait, in miliseconds. "
    "Returns: The number of events that have been handled during the poll. "
    "Negative value indicates error, and application can retrieve the error "
    "as (err = -return_value).";

static char pjsua_verify_sip_url_doc[] =
    "int py_pjsua.verify_sip_url (string c_url) "
    "Verify that valid SIP url is given Parameters: "
    "c_url: The URL, as NULL terminated string.";

static char pjsua_pool_create_doc[] =
    "py_pjsua.Pj_Pool py_pjsua.pool_create (string name, int init_size, "
                                            "int increment) "
    "Create memory pool Parameters: "
    "name: Optional pool name; "
    "init_size: Initial size of the pool;  "
    "increment: Increment size.";

static char pjsua_get_pjsip_endpt_doc[] =
    "py_pjsua.Pjsip_Endpoint py_pjsua.get_pjsip_endpt (void) "
    "Internal function to get SIP endpoint instance of pjsua, which is needed "
    "for example to register module, create transports, etc. Probably is only "
    "valid after pjsua_init() is called.";

static char pjsua_get_pjmedia_endpt_doc[] =
    "py_pjsua.Pjmedia_Endpt py_pjsua.get_pjmedia_endpt (void) "
    "Internal function to get media endpoint instance. Only valid after "
    "pjsua_init() is called.";

static char pjsua_get_pool_factory_doc[] =
    "py_pjsua.Pj_Pool_Factory py_pjsua.get_pool_factory (void) "
    "Internal function to get PJSUA pool factory. Only valid after "
    "pjsua_init() is called.";

static char pjsua_reconfigure_logging_doc[] =
    "int py_pjsua.reconfigure_logging (py_pjsua.Logging_Config c) "
    "Application can call this function at any time (after pjsua_create(), of "
    "course) to change logging settings. Parameters: "
    "c: Logging configuration.";

static char pjsua_logging_config_default_doc[] =
    "py_pjsua.Logging_Config py_pjsua.logging_config_default  ()  "
    "Use this function to initialize logging config.";

static char pjsua_config_default_doc[] =
    "py_pjsua.Config py_pjsua.config_default (). Use this function to "
    "initialize pjsua config. ";

static char pjsua_media_config_default_doc[] =
    "py_pjsua.Media_Config py_pjsua.media_config_default (). "
    "Use this function to initialize media config.";

static char pjsua_msg_data_init_doc[] =
    "py_pjsua.Msg_Data void py_pjsua.msg_data_init () "
    "Initialize message data ";
        

/* END OF LIB BASE */

/* LIB TRANSPORT */

/*
 * py_pjsua_transport_config_default
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_config_default(PyObject *pSelf, 
						   PyObject *pArgs)
{
    PyObj_pjsua_transport_config *obj;
    pjsua_transport_config cfg;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }

    pjsua_transport_config_default(&cfg);
    obj = (PyObj_pjsua_transport_config*)
	  PyObj_pjsua_transport_config_new(&PyTyp_pjsua_transport_config,
					   NULL, NULL);
    PyObj_pjsua_transport_config_import(obj, &cfg);

    return (PyObject *)obj;
}

/*
 * py_pjsua_transport_create
 * !modified @ 051206
 */
static PyObject *py_pjsua_transport_create(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int type;
    PyObject * tmpObj;
    pjsua_transport_config cfg;
    pjsua_transport_id id;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &type, &tmpObj)) {
        return NULL;
    }

    if (tmpObj != Py_None) {
	PyObj_pjsua_transport_config *obj;
        obj = (PyObj_pjsua_transport_config*)tmpObj;
	PyObj_pjsua_transport_config_export(&cfg, obj);
        status = pjsua_transport_create(type, &cfg, &id);
    } else {
        status = pjsua_transport_create(type, NULL, &id);
    }
    
    
    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_enum_transports
 * !modified @ 261206
 */
static PyObject *py_pjsua_enum_transports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_transport_id id[PJSIP_MAX_TRANSPORTS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_transports(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {     
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
        {
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
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
        return Py_BuildValue("O", obj);
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

/*
 * py_pjsua_transport_set_enable
 */
static PyObject *py_pjsua_transport_set_enable
(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    int id;
    int enabled;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &id, &enabled))
    {
        return NULL;
    }	
    status = pjsua_transport_set_enable(id, enabled);	
    
    return Py_BuildValue("i",status);
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

    if (!PyArg_ParseTuple(pArgs, "ii", &id, &force))
    {
        return NULL;
    }	
    status = pjsua_transport_close(id, force);	
    
    return Py_BuildValue("i",status);
}

static char pjsua_transport_config_default_doc[] =
    "py_pjsua.Transport_Config py_pjsua.transport_config_default () "
    "Call this function to initialize UDP config with default values.";
static char pjsua_transport_create_doc[] =
    "int, int py_pjsua.transport_create (int type, "
    "py_pjsua.Transport_Config cfg) "
    "Create SIP transport.";
static char pjsua_enum_transports_doc[] =
    "int[] py_pjsua.enum_transports () "
    "Enumerate all transports currently created in the system.";
static char pjsua_transport_get_info_doc[] =
    "void py_pjsua.transport_get_info "
    "(py_pjsua.Transport_ID id, py_pjsua.Transport_Info info) "
    "Get information about transports.";
static char pjsua_transport_set_enable_doc[] =
    "void py_pjsua.transport_set_enable "
    "(py_pjsua.Transport_ID id, int enabled) "
    "Disable a transport or re-enable it. "
    "By default transport is always enabled after it is created. "
    "Disabling a transport does not necessarily close the socket, "
    "it will only discard incoming messages and prevent the transport "
    "from being used to send outgoing messages.";
static char pjsua_transport_close_doc[] =
    "void py_pjsua.transport_close (py_pjsua.Transport_ID id, int force) "
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
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_config_default(PyObject *pSelf, PyObject *pArgs)
{
    PyObj_pjsua_acc_config *obj;
    pjsua_acc_config cfg;

    PJ_UNUSED_ARG(pSelf);

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

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }

    count = pjsua_acc_get_count();
    return Py_BuildValue("i",count);
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

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }

    id = pjsua_acc_get_default();
	
    return Py_BuildValue("i", id);
}

/*
 * py_pjsua_acc_add
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_add(PyObject *pSelf, PyObject *pArgs)
{    
    int is_default;
    PyObject * acObj;
    PyObj_pjsua_acc_config * ac;
    int acc_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "Oi", &acObj, &is_default)) {
        return NULL;
    }
    
    if (acObj != Py_None) {
	pjsua_acc_config cfg;

	pjsua_acc_config_default(&cfg);
        ac = (PyObj_pjsua_acc_config *)acObj;
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
 * !modified @ 051206
 */
static PyObject *py_pjsua_acc_add_local(PyObject *pSelf, PyObject *pArgs)
{    
    int is_default;
    int tid;
    int p_acc_id;
    int status;
	
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &tid, &is_default)) {
        return NULL;
    }
	
    
    status = pjsua_acc_add_local(tid, is_default, &p_acc_id);
    
    return Py_BuildValue("ii", status, p_acc_id);
}

/*
 * py_pjsua_acc_del
 */
static PyObject *py_pjsua_acc_del(PyObject *pSelf, PyObject *pArgs)
{    
    int acc_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &acc_id))
    {
        return NULL;
    }
	
	
    status = pjsua_acc_del(acc_id);	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_acc_modify
 */
static PyObject *py_pjsua_acc_modify(PyObject *pSelf, PyObject *pArgs)
{    	
    PyObject * acObj;
    PyObj_pjsua_acc_config * ac;
    int acc_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &acc_id, &acObj)) {
        return NULL;
    }

    if (acObj != Py_None) {
	pjsua_acc_config cfg;	

	pjsua_acc_config_default(&cfg);
        ac = (PyObj_pjsua_acc_config *)acObj;
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
    const char *activity_text;
    pjrpid_element rpid;
    pj_status_t status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiis", &acc_id, &is_online,
			  &activity_id, &activity_text)) {
        return NULL;
    }

    pj_bzero(&rpid, sizeof(rpid));
    rpid.activity = activity_id;
    rpid.note = pj_str((char*)activity_text);

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
 * !modified @ 051206
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
	obj = (PyObj_pjsua_acc_info *)
	    PyObj_pjsua_acc_info_new(&PyTyp_pjsua_acc_info,NULL, NULL);
	PyObj_pjsua_acc_info_import(obj, &info);
        return Py_BuildValue("O", obj);
    } else {
	Py_INCREF(Py_None);
	return Py_None;
    }
}

/*
 * py_pjsua_enum_accs
 * !modified @ 241206
 */
static PyObject *py_pjsua_enum_accs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_acc_id id[PJSUA_MAX_ACC];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    c = PJ_ARRAY_SIZE(id);
    
    status = pjsua_enum_accs(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
	{
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_acc_enum_info
 * !modified @ 241206
 */
static PyObject *py_pjsua_acc_enum_info(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    pjsua_acc_info info[PJSUA_MAX_ACC];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_acc_enum_info(info, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) {
        PyObj_pjsua_acc_info *obj;
        obj = (PyObj_pjsua_acc_info *)
	      PyObj_pjsua_acc_info_new(&PyTyp_pjsua_acc_info, NULL, NULL);

	PyObj_pjsua_acc_info_import(obj, &info[i]);

        PyList_SetItem(list, i, (PyObject *)obj);
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_acc_find_for_outgoing
 */
static PyObject *py_pjsua_acc_find_for_outgoing(PyObject *pSelf, 
						PyObject *pArgs)
{    	
    int acc_id;	
    PyObject * url;
    pj_str_t str;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &url))
    {
        return NULL;
    }
    str.ptr = PyString_AsString(url);
    str.slen = strlen(PyString_AsString(url));
	
    acc_id = pjsua_acc_find_for_outgoing(&str);
	
    return Py_BuildValue("i", acc_id);
}

/*
 * py_pjsua_acc_find_for_incoming
 */
static PyObject *py_pjsua_acc_find_for_incoming(PyObject *pSelf, 
						PyObject *pArgs)
{    	
    int acc_id;	
    PyObject * tmpObj;
    PyObj_pjsip_rx_data * obj;
    pjsip_rx_data * rdata;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &tmpObj))
    {
        return NULL;
    }
    if (tmpObj != Py_None)
    {
        obj = (PyObj_pjsip_rx_data *)tmpObj;
        rdata = obj->rdata;
        acc_id = pjsua_acc_find_for_incoming(rdata);
    } else {
        acc_id = pjsua_acc_find_for_incoming(NULL);
    }
    return Py_BuildValue("i", acc_id);
}

/*
 * py_pjsua_acc_create_uac_contact
 * !modified @ 061206
 */
static PyObject *py_pjsua_acc_create_uac_contact(PyObject *pSelf, 
						 PyObject *pArgs)
{    	
    int status;
    int acc_id;
    PyObject * pObj;
    PyObj_pj_pool * p;
    pj_pool_t * pool;
    PyObject * strc;
    pj_str_t contact;
    PyObject * stru;
    pj_str_t uri;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OiO", &pObj, &acc_id, &stru))
    {
        return NULL;
    }
    if (pObj != Py_None)
    {
        p = (PyObj_pj_pool *)pObj;
        pool = p->pool;    
        uri.ptr = PyString_AsString(stru);
        uri.slen = strlen(PyString_AsString(stru));
        status = pjsua_acc_create_uac_contact(pool, &contact, acc_id, &uri);
    } else {
        status = pjsua_acc_create_uac_contact(NULL, &contact, acc_id, &uri);
    }
    strc = PyString_FromStringAndSize(contact.ptr, contact.slen);
	
    return Py_BuildValue("O", strc);
}

/*
 * py_pjsua_acc_create_uas_contact
 * !modified @ 061206
 */
static PyObject *py_pjsua_acc_create_uas_contact(PyObject *pSelf, 
						 PyObject *pArgs)
{    	
    int status;
    int acc_id;	
    PyObject * pObj;
    PyObj_pj_pool * p;
    pj_pool_t * pool;
    PyObject * strc;
    pj_str_t contact;
    PyObject * rObj;
    PyObj_pjsip_rx_data * objr;
    pjsip_rx_data * rdata;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OiO", &pObj, &acc_id, &rObj))
    {
        return NULL;
    }
    if (pObj != Py_None)
    {
        p = (PyObj_pj_pool *)pObj;
        pool = p->pool;
    } else {
		pool = NULL;
    }
    if (rObj != Py_None)
    {
        objr = (PyObj_pjsip_rx_data *)rObj;
        rdata = objr->rdata;
    } else {
        rdata = NULL;
    }
    status = pjsua_acc_create_uas_contact(pool, &contact, acc_id, rdata);
    strc = PyString_FromStringAndSize(contact.ptr, contact.slen);
	
    return Py_BuildValue("O", strc);
}

static char pjsua_acc_config_default_doc[] =
    "py_pjsua.Acc_Config py_pjsua.acc_config_default () "
    "Call this function to initialize account config with default values.";
static char pjsua_acc_get_count_doc[] =
    "int py_pjsua.acc_get_count () "
    "Get number of current accounts.";
static char pjsua_acc_is_valid_doc[] =
    "int py_pjsua.acc_is_valid (int acc_id)  "
    "Check if the specified account ID is valid.";
static char pjsua_acc_set_default_doc[] =
    "int py_pjsua.acc_set_default (int acc_id) "
    "Set default account to be used when incoming "
    "and outgoing requests doesn't match any accounts.";
static char pjsua_acc_get_default_doc[] =
    "int py_pjsua.acc_get_default () "
    "Get default account.";
static char pjsua_acc_add_doc[] =
    "int, int py_pjsua.acc_add (py_pjsua.Acc_Config cfg, "
    "int is_default) "
    "Add a new account to pjsua. PJSUA must have been initialized "
    "(with pjsua_init()) before calling this function.";
static char pjsua_acc_add_local_doc[] =
    "int,int py_pjsua.acc_add_local (int tid, "
    "int is_default) "
    "Add a local account. A local account is used to identify "
    "local endpoint instead of a specific user, and for this reason, "
    "a transport ID is needed to obtain the local address information.";
static char pjsua_acc_del_doc[] =
    "int py_pjsua.acc_del (int acc_id) "
    "Delete account.";
static char pjsua_acc_modify_doc[] =
    "int py_pjsua.acc_modify (int acc_id, py_pjsua.Acc_Config cfg) "
    "Modify account information.";
static char pjsua_acc_set_online_status_doc[] =
    "int py_pjsua.acc_set_online_status2(int acc_id, int is_online) "
    "Modify account's presence status to be advertised "
    "to remote/presence subscribers.";
static char pjsua_acc_set_online_status2_doc[] =
    "int py_pjsua.acc_set_online_status (int acc_id, int is_online, "
                                         "int activity_id, string activity_text) "
    "Modify account's presence status to be advertised "
    "to remote/presence subscribers.";
static char pjsua_acc_set_registration_doc[] =
    "int py_pjsua.acc_set_registration (int acc_id, int renew) "
    "Update registration or perform unregistration.";
static char pjsua_acc_get_info_doc[] =
    "py_pjsua.Acc_Info py_pjsua.acc_get_info (int acc_id) "
    "Get account information.";
static char pjsua_enum_accs_doc[] =
    "int[] py_pjsua.enum_accs () "
    "Enum accounts all account ids.";
static char pjsua_acc_enum_info_doc[] =
    "py_pjsua.Acc_Info[] py_pjsua.acc_enum_info () "
    "Enum accounts info.";
static char pjsua_acc_find_for_outgoing_doc[] =
    "int py_pjsua.acc_find_for_outgoing (string url) "
    "This is an internal function to find the most appropriate account "
    "to used to reach to the specified URL.";
static char pjsua_acc_find_for_incoming_doc[] =
    "int py_pjsua.acc_find_for_incoming (PyObj_pjsip_rx_data rdata) "
    "This is an internal function to find the most appropriate account "
    "to be used to handle incoming calls.";
static char pjsua_acc_create_uac_contact_doc[] =
    "string py_pjsua.acc_create_uac_contact (PyObj_pj_pool pool, "
    "int acc_id, string uri) "
    "Create a suitable URI to be put as Contact based on the specified "
    "target URI for the specified account.";
static char pjsua_acc_create_uas_contact_doc[] =
    "string py_pjsua.acc_create_uas_contact (PyObj_pj_pool pool, "
    "int acc_id, PyObj_pjsip_rx_data rdata) "
    "Create a suitable URI to be put as Contact based on the information "
    "in the incoming request.";

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

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }
    
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
    int ret;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }
    ret = pjsua_get_buddy_count();
	
    return Py_BuildValue("i", ret);
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
 * !modified @ 241206
 */
static PyObject *py_pjsua_enum_buddies(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_buddy_id id[PJSUA_MAX_BUDDIES];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "")) {
        return NULL;
    }	
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_buddies(id, &c);
    list = PyList_New(c);
    for (i = 0; i < c; i++) {
        PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_buddy_get_info
 * !modified @ 071206
 */
static PyObject *py_pjsua_buddy_get_info(PyObject *pSelf, PyObject *pArgs)
{    	
    int buddy_id;
    PyObj_pjsua_buddy_info * obj;
    pjsua_buddy_info info;
    int status;	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id)) {
        return NULL;
    }

    status = pjsua_buddy_get_info(buddy_id, &info);
    if (status == PJ_SUCCESS) {
	obj = (PyObj_pjsua_buddy_info *)
	      PyObj_pjsua_buddy_config_new(&PyTyp_pjsua_buddy_info,NULL,NULL);
	PyObj_pjsua_buddy_info_import(obj, &info);	
        return Py_BuildValue("O", obj);
    } else {
	Py_INCREF(Py_None);
	return Py_None;
    }
}

/*
 * py_pjsua_buddy_add
 * !modified @ 061206
 */
static PyObject *py_pjsua_buddy_add(PyObject *pSelf, PyObject *pArgs)
{   
    PyObject * bcObj;
    int buddy_id;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &bcObj)) {
        return NULL;
    }

    if (bcObj != Py_None) {
	pjsua_buddy_config cfg;
	PyObj_pjsua_buddy_config * bc;

        bc = (PyObj_pjsua_buddy_config *)bcObj;

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

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &buddy_id)) {
        return NULL;
    }
	
	
    status = pjsua_buddy_del(buddy_id);	
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_buddy_subscribe_pres
 */
static PyObject *py_pjsua_buddy_subscribe_pres(PyObject *pSelf, PyObject *pArgs)
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
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_im_send
 * !modified @ 071206
 */
static PyObject *py_pjsua_im_send(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t * mime_type, tmp_mime_type;
    pj_str_t to, content;
    PyObject * st;
    PyObject * smt;
    PyObject * sc;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;
    
    int user_data;
    pj_pool_t *pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOOOOi", &acc_id, 
		&st, &smt, &sc, &omdObj, &user_data))
    {
        return NULL;
    }
    if (smt != Py_None) {
        mime_type = &tmp_mime_type;
	tmp_mime_type = PyString_to_pj_str(smt);
    } else {
        mime_type = NULL;
    }
    to = PyString_to_pj_str(st);
        content = PyString_to_pj_str(sc);

    if (omdObj != Py_None) {
		
        omd = (PyObj_pjsua_msg_data *)omdObj;
	msg_data.content_type = PyString_to_pj_str(omd->content_type);
	msg_data.msg_body = PyString_to_pj_str(omd->msg_body);
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);

        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_im_send(acc_id, &to, mime_type, 
			&content, &msg_data, (void *)user_data);	
        pj_pool_release(pool);
    } else {
		
        status = pjsua_im_send(acc_id, &to, mime_type, 
			&content, NULL, NULL);	
    }
    
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
    PyObject * st;
    int is_typing;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOiO", &acc_id, &st, &is_typing, &omdObj)) {
        return NULL;
    }
	
    to = PyString_to_pj_str(st);

    if (omdObj != Py_None) {
        omd = (PyObj_pjsua_msg_data *)omdObj;
	msg_data.content_type = PyString_to_pj_str(omd->content_type);
	msg_data.msg_body = PyString_to_pj_str(omd->msg_body);
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);

        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_im_typing(acc_id, &to, is_typing, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_im_typing(acc_id, &to, is_typing, NULL);
    }
    return Py_BuildValue("i",status);
}

static char pjsua_buddy_config_default_doc[] =
    "py_pjsua.Buddy_Config py_pjsua.buddy_config_default () "
    "Set default values to the buddy config.";
static char pjsua_get_buddy_count_doc[] =
    "int py_pjsua.get_buddy_count () "
    "Get total number of buddies.";
static char pjsua_buddy_is_valid_doc[] =
    "int py_pjsua.buddy_is_valid (int buddy_id) "
    "Check if buddy ID is valid.";
static char pjsua_enum_buddies_doc[] =
    "int[] py_pjsua.enum_buddies () "
    "Enum buddy IDs.";
static char pjsua_buddy_get_info_doc[] =
    "py_pjsua.Buddy_Info py_pjsua.buddy_get_info (int buddy_id) "
    "Get detailed buddy info.";
static char pjsua_buddy_add_doc[] =
    "int,int py_pjsua.buddy_add (py_pjsua.Buddy_Config cfg) "
    "Add new buddy.";
static char pjsua_buddy_del_doc[] =
    "int py_pjsua.buddy_del (int buddy_id) "
    "Delete buddy.";
static char pjsua_buddy_subscribe_pres_doc[] =
    "int py_pjsua.buddy_subscribe_pres (int buddy_id, int subscribe) "
    "Enable/disable buddy's presence monitoring.";
static char pjsua_pres_dump_doc[] =
    "void py_pjsua.pres_dump (int verbose) "
    "Dump presence subscriptions to log file.";
static char pjsua_im_send_doc[] =
    "int py_pjsua.im_send (int acc_id, string to, string mime_type, "
    "string content, py_pjsua.Msg_Data msg_data, int user_data) "
    "Send instant messaging outside dialog, using the specified account "
    "for route set and authentication.";
static char pjsua_im_typing_doc[] =
    "int py_pjsua.im_typing (int acc_id, string to, int is_typing, "
    "py_pjsua.Msg_Data msg_data) "
    "Send typing indication outside dialog.";

/* END OF LIB BUDDY */

/* LIB MEDIA */



/*
 * PyObj_pjsua_codec_info
 * Codec Info
 * !modified @ 071206
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    PyObject * codec_id;
    pj_uint8_t priority;    
    char buf_[32];
} PyObj_pjsua_codec_info;


/*
 * codec_info_dealloc
 * deletes a codec_info from memory
 * !modified @ 071206
 */
static void codec_info_dealloc(PyObj_pjsua_codec_info* self)
{
    Py_XDECREF(self->codec_id);    
    
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * codec_info_new
 * constructor for codec_info object
 * !modified @ 071206
 */
static PyObject * codec_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    PyObj_pjsua_codec_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_codec_info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->codec_id = PyString_FromString("");
        if (self->codec_id == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	

    }
    return (PyObject *)self;
}

/*
 * codec_info_members
 * !modified @ 071206
 */
static PyMemberDef codec_info_members[] =
{    
    {
        "codec_id", T_OBJECT_EX,
        offsetof(PyObj_pjsua_codec_info, codec_id), 0,
        "Codec unique identification."        
    },
    
    {
        "priority", T_INT, 
        offsetof(PyObj_pjsua_codec_info, priority), 0,
        "Codec priority (integer 0-255)."
    },
    
    
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_codec_info
 */
static PyTypeObject PyTyp_pjsua_codec_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Codec_Info",      /*tp_name*/
    sizeof(PyObj_pjsua_codec_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)codec_info_dealloc,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Codec Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    codec_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    codec_info_new,             /* tp_new */

};

/*
 * PyObj_pjsua_conf_port_info
 * Conf Port Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    int  slot_id;
    PyObject *  name;
    unsigned  clock_rate;
    unsigned  channel_count;
    unsigned  samples_per_frame;
    unsigned  bits_per_sample;
    PyListObject * listeners;

} PyObj_pjsua_conf_port_info;


/*
 * conf_port_info_dealloc
 * deletes a conf_port_info from memory
 */
static void conf_port_info_dealloc(PyObj_pjsua_conf_port_info* self)
{
    Py_XDECREF(self->name);    
    Py_XDECREF(self->listeners);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * conf_port_info_new
 * constructor for conf_port_info object
 */
static PyObject * conf_port_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    PyObj_pjsua_conf_port_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_conf_port_info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->name = PyString_FromString("");
        if (self->name == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	
	self->listeners = (PyListObject *)PyList_New(0);
        if (self->listeners == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }
    return (PyObject *)self;
}

/*
 * conf_port_info_members
 */
static PyMemberDef conf_port_info_members[] =
{   
    {
        "slot_id", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, slot_id), 0,
        "Conference port number."
    },
    {
        "name", T_OBJECT_EX,
        offsetof(PyObj_pjsua_conf_port_info, name), 0,
        "Port name"        
    },
    {
        "clock_rate", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, clock_rate), 0,
        "Clock rate"
    },
    {
        "channel_count", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, channel_count), 0,
        "Number of channels."
    },
    {
        "samples_per_frame", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, samples_per_frame), 0,
        "Samples per frame "
    },
    {
        "bits_per_sample", T_INT, 
        offsetof(PyObj_pjsua_conf_port_info, bits_per_sample), 0,
        "Bits per sample"
    },
    {
        "listeners", T_OBJECT_EX,
        offsetof(PyObj_pjsua_conf_port_info, listeners), 0,
        "Array of listeners (in other words, ports where this port "
	"is transmitting to"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_conf_port_info
 */
static PyTypeObject PyTyp_pjsua_conf_port_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Conf_Port_Info",      /*tp_name*/
    sizeof(PyObj_pjsua_conf_port_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)conf_port_info_dealloc,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Conf Port Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    conf_port_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    conf_port_info_new,             /* tp_new */

};

/*
 * PyObj_pjmedia_port
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */
    pjmedia_port * port;
} PyObj_pjmedia_port;


/*
 * PyTyp_pjmedia_port
 */
static PyTypeObject PyTyp_pjmedia_port =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "py_pjsua.PJMedia_Port",        /*tp_name*/
    sizeof(PyObj_pjmedia_port),    /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "pjmedia_port objects",       /* tp_doc */

};

/*
 * PyObj_pjmedia_snd_dev_info
 * PJMedia Snd Dev Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    
    unsigned  input_count;
    unsigned  output_count;
    unsigned  default_samples_per_sec;    
    PyObject * name;

} PyObj_pjmedia_snd_dev_info;


/*
 * pjmedia_snd_dev_info_dealloc
 * deletes a pjmedia_snd_dev_info from memory
 */
static void pjmedia_snd_dev_info_dealloc(PyObj_pjmedia_snd_dev_info* self)
{
    Py_XDECREF(self->name);        
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * pjmedia_snd_dev_info_new
 * constructor for pjmedia_snd_dev_info object
 */
static PyObject * pjmedia_snd_dev_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    PyObj_pjmedia_snd_dev_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjmedia_snd_dev_info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->name = PyString_FromString("");
        if (self->name == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	
    }
    return (PyObject *)self;
}

/*
 * pjmedia_snd_dev_info_members
 */
static PyMemberDef pjmedia_snd_dev_info_members[] =
{   
    
    {
        "name", T_OBJECT_EX,
        offsetof(PyObj_pjmedia_snd_dev_info, name), 0,
        "Device name"        
    },
    {
        "input_count", T_INT, 
        offsetof(PyObj_pjmedia_snd_dev_info, input_count), 0,
        "Max number of input channels"
    },
    {
        "output_count", T_INT, 
        offsetof(PyObj_pjmedia_snd_dev_info, output_count), 0,
        "Max number of output channels"
    },
    {
        "default_samples_per_sec", T_INT, 
        offsetof(PyObj_pjmedia_snd_dev_info, default_samples_per_sec), 0,
        "Default sampling rate."
    },
    
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjmedia_snd_dev_info
 */
static PyTypeObject PyTyp_pjmedia_snd_dev_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Snd_Dev_Info",      /*tp_name*/
    sizeof(PyObj_pjmedia_snd_dev_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjmedia_snd_dev_info_dealloc,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "PJMedia Snd Dev Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_snd_dev_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjmedia_snd_dev_info_new,             /* tp_new */

};

/*
 * PyObj_pjmedia_codec_param_info
 * PJMedia Codec Param Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    unsigned  clock_rate;
    unsigned  channel_cnt;
    pj_uint32_t avg_bps;
    pj_uint16_t frm_ptime;
    pj_uint8_t  pcm_bits_per_sample;
    pj_uint8_t  pt;	

} PyObj_pjmedia_codec_param_info;



/*
 * pjmedia_codec_param_info_members
 */
static PyMemberDef pjmedia_codec_param_info_members[] =
{   
    
    {
        "clock_rate", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, clock_rate), 0,
        "Sampling rate in Hz"
    },
    {
        "channel_cnt", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, channel_cnt), 0,
        "Channel count"
    },
    {
        "avg_bps", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, avg_bps), 0,
        "Average bandwidth in bits/sec"
    },
    {
        "frm_ptime", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, frm_ptime), 0,
        "Base frame ptime in msec."
    },
    {
        "pcm_bits_per_sample", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, pcm_bits_per_sample), 0,
        "Bits/sample in the PCM side"
    },
    {
        "pt", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_info, pt), 0,
        "Payload type"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjmedia_codec_param_info
 */
static PyTypeObject PyTyp_pjmedia_codec_param_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Codec_Param_Info",      /*tp_name*/
    sizeof(PyObj_pjmedia_codec_param_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "PJMedia Codec Param Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_info_members,         /* tp_members */
    

};

/*
 * PyObj_pjmedia_codec_param_setting
 * PJMedia Codec Param Setting
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    pj_uint8_t  frm_per_pkt; 
    unsigned    vad;
    unsigned    cng;
    unsigned    penh;
    unsigned    plc;
    unsigned    reserved;
    pj_uint8_t  enc_fmtp_mode;
    pj_uint8_t  dec_fmtp_mode; 

} PyObj_pjmedia_codec_param_setting;



/*
 * pjmedia_codec_param_setting_members
 */
static PyMemberDef pjmedia_codec_param_setting_members[] =
{   
    
    {
        "frm_per_pkt", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, frm_per_pkt), 0,
        "Number of frames per packet"
    },
    {
        "vad", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, vad), 0,
        "Voice Activity Detector"
    },
    {
        "penh", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, penh), 0,
        "Perceptual Enhancement"
    },
    {
        "plc", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, plc), 0,
        "Packet loss concealment"
    },
    {
        "reserved", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, reserved), 0,
        "Reserved, must be zero"
    },
    {
        "cng", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, cng), 0,
        "Comfort Noise Generator"
    },
    {
        "enc_fmtp_mode", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, enc_fmtp_mode), 0,
        "Mode param in fmtp (def:0)"
    },
    {
        "dec_fmtp_mode", T_INT, 
        offsetof(PyObj_pjmedia_codec_param_setting, dec_fmtp_mode), 0,
        "Mode param in fmtp (def:0)"
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjmedia_codec_param_setting
 */
static PyTypeObject PyTyp_pjmedia_codec_param_setting =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Codec_Param_Setting",      /*tp_name*/
    sizeof(PyObj_pjmedia_codec_param_setting),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "PJMedia Codec Param Setting objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_setting_members,         /* tp_members */
    

};

/*
 * PyObj_pjmedia_codec_param
 * PJMedia Codec Param
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    PyObj_pjmedia_codec_param_info * info;
    PyObj_pjmedia_codec_param_setting * setting;

} PyObj_pjmedia_codec_param;


/*
 * pjmedia_codec_param_dealloc
 * deletes a pjmedia_codec_param from memory
 */
static void pjmedia_codec_param_dealloc(PyObj_pjmedia_codec_param* self)
{
    Py_XDECREF(self->info);        
    Py_XDECREF(self->setting);        
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * pjmedia_codec_param_new
 * constructor for pjmedia_codec_param object
 */
static PyObject * pjmedia_codec_param_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    PyObj_pjmedia_codec_param *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjmedia_codec_param *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->info = (PyObj_pjmedia_codec_param_info *)
	    PyType_GenericNew(&PyTyp_pjmedia_codec_param_info, NULL, NULL);
        if (self->info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
	self->setting = (PyObj_pjmedia_codec_param_setting *)
	    PyType_GenericNew(&PyTyp_pjmedia_codec_param_setting, NULL, NULL);
        if (self->setting == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }        
    }
    return (PyObject *)self;
}

/*
 * pjmedia_codec_param_members
 */
static PyMemberDef pjmedia_codec_param_members[] =
{   
    
    {
        "info", T_OBJECT_EX,
        offsetof(PyObj_pjmedia_codec_param, info), 0,
        "The 'info' part of codec param describes the capability of the codec,"
        " and the value should NOT be changed by application."        
    },
    {
        "setting", T_OBJECT_EX,
        offsetof(PyObj_pjmedia_codec_param, setting), 0, 
        "The 'setting' part of codec param describes various settings to be "
        "applied to the codec. When the codec param is retrieved from the "
        "codec or codec factory, the values of these will be filled by "
        "the capability of the codec. Any features that are supported by "
        "the codec (e.g. vad or plc) will be turned on, so that application "
        "can query which capabilities are supported by the codec. "
        "Application may change the settings here before instantiating "
        "the codec/stream."        
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjmedia_codec_param
 */
static PyTypeObject PyTyp_pjmedia_codec_param =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJMedia_Codec_Param",      /*tp_name*/
    sizeof(PyObj_pjmedia_codec_param),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)pjmedia_codec_param_dealloc,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "PJMedia Codec Param objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pjmedia_codec_param_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    pjmedia_codec_param_new,             /* tp_new */

};

/*
 * py_pjsua_conf_get_max_ports
 */
static PyObject *py_pjsua_conf_get_max_ports
(PyObject *pSelf, PyObject *pArgs)
{    
    int ret;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    ret = pjsua_conf_get_max_ports();
	
    return Py_BuildValue("i", ret);
}

/*
 * py_pjsua_conf_get_active_ports
 */
static PyObject *py_pjsua_conf_get_active_ports
(PyObject *pSelf, PyObject *pArgs)
{    
    int ret;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }
    ret = pjsua_conf_get_active_ports();
	
    return Py_BuildValue("i", ret);
}

/*
 * py_pjsua_enum_conf_ports
 * !modified @ 241206
 */
static PyObject *py_pjsua_enum_conf_ports(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_conf_port_id id[PJSUA_MAX_CONF_PORTS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_conf_ports(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
	{
            return NULL;
	}
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_conf_get_port_info
 */
static PyObject *py_pjsua_conf_get_port_info
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    PyObj_pjsua_conf_port_info * obj;
    pjsua_conf_port_info info;
    int status;	
    unsigned i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }
	
    
    status = pjsua_conf_get_port_info(id, &info);
    obj = (PyObj_pjsua_conf_port_info *)conf_port_info_new
	    (&PyTyp_pjsua_conf_port_info,NULL,NULL);
    obj->bits_per_sample = info.bits_per_sample;
    obj->channel_count = info.bits_per_sample;
    obj->clock_rate = info.clock_rate;
    obj->name = PyString_FromStringAndSize(info.name.ptr, info.name.slen);
    obj->samples_per_frame = info.samples_per_frame;
    obj->slot_id = info.slot_id;
    
    obj->listeners = (PyListObject *)PyList_New(info.listener_cnt);
    for (i = 0; i < info.listener_cnt; i++) {
	PyObject * item = Py_BuildValue("i",info.listeners[i]);
	PyList_SetItem((PyObject *)obj->listeners, i, item);
    }
    return Py_BuildValue("O", obj);
}

/*
 * py_pjsua_conf_add_port
 */
static PyObject *py_pjsua_conf_add_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    int p_id;
    PyObject * oportObj;
    PyObj_pjmedia_port * oport;
    pjmedia_port * port;
    PyObject * opoolObj;
    PyObj_pj_pool * opool;
    pj_pool_t * pool;
    
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OO", &opoolObj, &oportObj))
    {
        return NULL;
    }
    if (opoolObj != Py_None)
    {
        opool = (PyObj_pj_pool *)opoolObj;
		pool = opool->pool;
    } else {
       opool = NULL;
       pool = NULL;
    }
    if (oportObj != Py_None)
    {
        oport = (PyObj_pjmedia_port *)oportObj;
		port = oport->port;
    } else {
        oport = NULL;
        port = NULL;
    }

    status = pjsua_conf_add_port(pool, port, &p_id);
    
    
    return Py_BuildValue("ii", status, p_id);
}

/*
 * py_pjsua_conf_remove_port
 */
static PyObject *py_pjsua_conf_remove_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    status = pjsua_conf_remove_port(id);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_connect
 */
static PyObject *py_pjsua_conf_connect
(PyObject *pSelf, PyObject *pArgs)
{    	
    int source, sink;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &source, &sink))
    {
        return NULL;
    }	
    
    status = pjsua_conf_connect(source, sink);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_conf_disconnect
 */
static PyObject *py_pjsua_conf_disconnect
(PyObject *pSelf, PyObject *pArgs)
{    	
    int source, sink;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &source, &sink))
    {
        return NULL;
    }	
    
    status = pjsua_conf_disconnect(source, sink);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_player_create
 */
static PyObject *py_pjsua_player_create
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int options;
    PyObject * filename;
    pj_str_t str;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "Oi", &filename, &options))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(filename);
    str.slen = strlen(PyString_AsString(filename));
    status = pjsua_player_create(&str, options, &id);
    
    return Py_BuildValue("ii", status, id);
}

/*
 * py_pjsua_player_get_conf_port
 */
static PyObject *py_pjsua_player_get_conf_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int id, port_id;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    port_id = pjsua_player_get_conf_port(id);
    
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_player_set_pos
 */
static PyObject *py_pjsua_player_set_pos
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    pj_uint32_t samples;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iI", &id, &samples))
    {
        return NULL;
    }	
    
    status = pjsua_player_set_pos(id, samples);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_player_destroy
 */
static PyObject *py_pjsua_player_destroy
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    status = pjsua_player_destroy(id);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_recorder_create
 * !modified @ 261206
 */
static PyObject *py_pjsua_recorder_create
(PyObject *pSelf, PyObject *pArgs)
{    	
    int p_id;
    int options;
    int max_size;
    PyObject * filename;
    pj_str_t str;
    PyObject * enc_param;
    pj_str_t strparam;
    int enc_type;
    
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OiOii", &filename, 
		&enc_type, &enc_param, &max_size, &options))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(filename);
    str.slen = strlen(PyString_AsString(filename));
    if (enc_param != Py_None)
    {
        strparam.ptr = PyString_AsString(enc_param);
        strparam.slen = strlen(PyString_AsString(enc_param));
        status = pjsua_recorder_create
		(&str, enc_type, NULL, max_size, options, &p_id);
    } else {
        status = pjsua_recorder_create
		(&str, enc_type, NULL, max_size, options, &p_id);
    }
    return Py_BuildValue("ii", status, p_id);
}

/*
 * py_pjsua_recorder_get_conf_port
 */
static PyObject *py_pjsua_recorder_get_conf_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int id, port_id;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
        return NULL;
    }	
    
    port_id = pjsua_recorder_get_conf_port(id);
    
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_recorder_destroy
 */
static PyObject *py_pjsua_recorder_destroy
(PyObject *pSelf, PyObject *pArgs)
{    	
    int id;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &id))
    {
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
    PyObject *list;
    
    pjmedia_snd_dev_info info[SND_DEV_NUM];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_enum_snd_devs(info, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret;
        int j;
        char * str;
	
        PyObj_pjmedia_snd_dev_info * obj;
        obj = (PyObj_pjmedia_snd_dev_info *)pjmedia_snd_dev_info_new
	    (&PyTyp_pjmedia_snd_dev_info, NULL, NULL);
        obj->default_samples_per_sec = info[i].default_samples_per_sec;
        obj->input_count = info[i].input_count;
        obj->output_count = info[i].output_count;
        str = (char *)malloc(SND_NAME_LEN * sizeof(char));
        memset(str, 0, SND_NAME_LEN);
        for (j = 0; j < SND_NAME_LEN; j++)
	{
            str[j] = info[i].name[j];
	}
        obj->name = PyString_FromStringAndSize(str, SND_NAME_LEN);
        free(str);
        ret = PyList_SetItem(list, i, (PyObject *)obj);
        if (ret == -1) 
	{
            return NULL;
	}
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_get_snd_dev
 */
static PyObject *py_pjsua_get_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    int capture_dev, playback_dev;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    status = pjsua_get_snd_dev(&capture_dev, &playback_dev);
    
    
    return Py_BuildValue("ii", capture_dev, playback_dev);
}

/*
 * py_pjsua_set_snd_dev
 */
static PyObject *py_pjsua_set_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    int capture_dev, playback_dev;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &capture_dev, &playback_dev))
    {
        return NULL;
    }	
    
    status = pjsua_set_snd_dev(capture_dev, playback_dev);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_set_null_snd_dev
 */
static PyObject *py_pjsua_set_null_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    status = pjsua_set_null_snd_dev();
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_set_no_snd_dev
 */
static PyObject *py_pjsua_set_no_snd_dev
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    PyObj_pjmedia_port * obj;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
     
    obj = (PyObj_pjmedia_port *)PyType_GenericNew
	(&PyTyp_pjmedia_port, NULL, NULL);
    obj->port = pjsua_set_no_snd_dev();
    return Py_BuildValue("O", obj);
}

/*
 * py_pjsua_set_ec
 */
static PyObject *py_pjsua_set_ec
(PyObject *pSelf, PyObject *pArgs)
{    	
    int options;
    int tail_ms;
    int status;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &tail_ms, &options))
    {
        return NULL;
    }	
    
    status = pjsua_set_ec(tail_ms, options);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_get_ec_tail
 */
static PyObject *py_pjsua_get_ec_tail
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    unsigned p_tail_ms;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    status = pjsua_get_ec_tail(&p_tail_ms);
    
    
    return Py_BuildValue("i", p_tail_ms);
}

/*
 * py_pjsua_enum_codecs
 * !modified @ 261206
 */
static PyObject *py_pjsua_enum_codecs(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_codec_info info[PJMEDIA_CODEC_MGR_MAX_CODECS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(info);
    status = pjsua_enum_codecs(info, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {
        int ret;
        int j;
        PyObj_pjsua_codec_info * obj;
        obj = (PyObj_pjsua_codec_info *)codec_info_new
	    (&PyTyp_pjsua_codec_info, NULL, NULL);
        obj->codec_id = PyString_FromStringAndSize
	    (info[i].codec_id.ptr, info[i].codec_id.slen);
        obj->priority = info[i].priority;
        for (j = 0; j < 32; j++)
        {	    
             obj->buf_[j] = info[i].buf_[j];
        }	
        ret = PyList_SetItem(list, i, (PyObject *)obj);
        if (ret == -1) {
            return NULL;
        }	
    }
    

    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_codec_set_priority
 */
static PyObject *py_pjsua_codec_set_priority
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    PyObject * id;
    pj_str_t str;
    pj_uint8_t priority;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OB", &id, &priority))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(id);
    str.slen = strlen(PyString_AsString(id));
    status = pjsua_codec_set_priority(&str, priority);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_codec_get_param
 */
static PyObject *py_pjsua_codec_get_param
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    PyObject * id;
    pj_str_t str;
    pjmedia_codec_param param;
    PyObj_pjmedia_codec_param *obj;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "O", &id))
    {
        return NULL;
    }	
    str.ptr = PyString_AsString(id);
    str.slen = strlen(PyString_AsString(id));
    status = pjsua_codec_get_param(&str, &param);
    obj = (PyObj_pjmedia_codec_param *)pjmedia_codec_param_new
	(&PyTyp_pjmedia_codec_param, NULL, NULL);
    obj->info->avg_bps = param.info.avg_bps;
    obj->info->channel_cnt = param.info.channel_cnt;
    obj->info->clock_rate = param.info.clock_rate;
    obj->info->frm_ptime = param.info.frm_ptime;
    obj->info->pcm_bits_per_sample = param.info.pcm_bits_per_sample;
    obj->info->pt = param.info.pt;
    obj->setting->cng = param.setting.cng;
    //deprecated:
    //obj->setting->dec_fmtp_mode = param.setting.dec_fmtp_mode;
    //obj->setting->enc_fmtp_mode = param.setting.enc_fmtp_mode;
    obj->setting->frm_per_pkt = param.setting.frm_per_pkt;
    obj->setting->penh = param.setting.penh;
    obj->setting->plc = param.setting.plc;
    obj->setting->reserved = param.setting.reserved;
    obj->setting->vad = param.setting.vad;

    return Py_BuildValue("O", obj);
}
/*
 * py_pjsua_codec_set_param
 */
static PyObject *py_pjsua_codec_set_param
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int status;	
    PyObject * id;
    pj_str_t str;
    pjmedia_codec_param param;
    PyObject * tmpObj;
    PyObj_pjmedia_codec_param *obj;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "OO", &id, &tmpObj))
    {
        return NULL;
    }	

    str.ptr = PyString_AsString(id);
    str.slen = strlen(PyString_AsString(id));
    if (tmpObj != Py_None)
    {
        obj = (PyObj_pjmedia_codec_param *)tmpObj;
        param.info.avg_bps = obj->info->avg_bps;
        param.info.channel_cnt = obj->info->channel_cnt;
        param.info.clock_rate = obj->info->clock_rate;
        param.info.frm_ptime = obj->info->frm_ptime;
        param.info.pcm_bits_per_sample = obj->info->pcm_bits_per_sample;
        param.info.pt = obj->info->pt;
        param.setting.cng = obj->setting->cng;
	//Deprecated:
        //param.setting.dec_fmtp_mode = obj->setting->dec_fmtp_mode;
        //param.setting.enc_fmtp_mode = obj->setting->enc_fmtp_mode;
        param.setting.frm_per_pkt = obj->setting->frm_per_pkt;
        param.setting.penh = obj->setting->penh;
        param.setting.plc = obj->setting->plc;
        param.setting.reserved = obj->setting->reserved;
        param.setting.vad = obj->setting->vad;
        status = pjsua_codec_set_param(&str, &param);
    } else {
        status = pjsua_codec_set_param(&str, NULL);
    }
    return Py_BuildValue("i", status);
}

static char pjsua_conf_get_max_ports_doc[] =
    "int py_pjsua.conf_get_max_ports () "
    "Get maxinum number of conference ports.";
static char pjsua_conf_get_active_ports_doc[] =
    "int py_pjsua.conf_get_active_ports () "
    "Get current number of active ports in the bridge.";
static char pjsua_enum_conf_ports_doc[] =
    "int[] py_pjsua.enum_conf_ports () "
    "Enumerate all conference ports.";
static char pjsua_conf_get_port_info_doc[] =
    "py_pjsua.Conf_Port_Info py_pjsua.conf_get_port_info (int id) "
    "Get information about the specified conference port";
static char pjsua_conf_add_port_doc[] =
    "int, int py_pjsua.conf_add_port "
    "(py_pjsua.Pj_Pool pool, py_pjsua.PJMedia_Port port) "
    "Add arbitrary media port to PJSUA's conference bridge. "
    "Application can use this function to add the media port "
    "that it creates. For media ports that are created by PJSUA-LIB "
    "(such as calls, file player, or file recorder), PJSUA-LIB will "
    "automatically add the port to the bridge.";
static char pjsua_conf_remove_port_doc[] =
    "int py_pjsua.conf_remove_port (int id) "
    "Remove arbitrary slot from the conference bridge. "
    "Application should only call this function "
    "if it registered the port manually.";
static char pjsua_conf_connect_doc[] =
    "int py_pjsua.conf_connect (int source, int sink) "
    "Establish unidirectional media flow from souce to sink. "
    "One source may transmit to multiple destinations/sink. "
    "And if multiple sources are transmitting to the same sink, "
    "the media will be mixed together. Source and sink may refer "
    "to the same ID, effectively looping the media. "
    "If bidirectional media flow is desired, application "
    "needs to call this function twice, with the second "
    "one having the arguments reversed.";
static char pjsua_conf_disconnect_doc[] =
    "int py_pjsua.conf_disconnect (int source, int sink) "
    "Disconnect media flow from the source to destination port.";
static char pjsua_player_create_doc[] =
    "int, int py_pjsua.player_create (string filename, int options) "
    "Create a file player, and automatically connect "
    "this player to the conference bridge.";
static char pjsua_player_get_conf_port_doc[] =
    "int py_pjsua.player_get_conf_port (int) "
    "Get conference port ID associated with player.";
static char pjsua_player_set_pos_doc[] =
    "int py_pjsua.player_set_pos (int id, int samples) "
    "Set playback position.";
static char pjsua_player_destroy_doc[] =
    "int py_pjsua.player_destroy (int id) "
    "Close the file, remove the player from the bridge, "
    "and free resources associated with the file player.";
static char pjsua_recorder_create_doc[] =
    "int, int py_pjsua.recorder_create (string filename, "
    "int enc_type, int enc_param, int max_size, int options) "
    "Create a file recorder, and automatically connect this recorder "
    "to the conference bridge. The recorder currently supports recording "
    "WAV file, and on Windows, MP3 file. The type of the recorder to use "
    "is determined by the extension of the file (e.g. '.wav' or '.mp3').";
static char pjsua_recorder_get_conf_port_doc[] =
    "int py_pjsua.recorder_get_conf_port (int id) "
    "Get conference port associated with recorder.";
static char pjsua_recorder_destroy_doc[] =
    "int py_pjsua.recorder_destroy (int id) "
    "Destroy recorder (this will complete recording).";
static char pjsua_enum_snd_devs_doc[] =
    "py_pjsua.PJMedia_Snd_Dev_Info[] py_pjsua.enum_snd_devs (int count) "
    "Enum sound devices.";
static char pjsua_get_snd_dev_doc[] =
    "int, int py_pjsua.get_snd_dev () "
    "Get currently active sound devices. "
    "If sound devices has not been created "
    "(for example when pjsua_start() is not called), "
    "it is possible that the function returns "
    "PJ_SUCCESS with -1 as device IDs.";
static char pjsua_set_snd_dev_doc[] =
    "int py_pjsua.set_snd_dev (int capture_dev, int playback_dev) "
    "Select or change sound device. Application may call this function "
    "at any time to replace current sound device.";
static char pjsua_set_null_snd_dev_doc[] =
    "int py_pjsua.set_null_snd_dev () "
    "Set pjsua to use null sound device. The null sound device only "
    "provides the timing needed by the conference bridge, and will not "
    "interract with any hardware.";
static char pjsua_set_no_snd_dev_doc[] =
    "py_pjsua.PJMedia_Port py_pjsua.set_no_snd_dev () "
    "Disconnect the main conference bridge from any sound devices, "
    "and let application connect the bridge to it's "
    "own sound device/master port.";
static char pjsua_set_ec_doc[] =
    "int py_pjsua.set_ec (int tail_ms, int options) "
    "Configure the echo canceller tail length of the sound port.";
static char pjsua_get_ec_tail_doc[] =
    "int py_pjsua.get_ec_tail () "
    "Get current echo canceller tail length.";
static char pjsua_enum_codecs_doc[] =
    "py_pjsua.Codec_Info[] py_pjsua.enum_codecs () "
    "Enum all supported codecs in the system.";
static char pjsua_codec_set_priority_doc[] =
    "int py_pjsua.codec_set_priority (string id, int priority) "
    "Change codec priority.";
static char pjsua_codec_get_param_doc[] =
    "py_pjsua.PJMedia_Codec_Param py_pjsua.codec_get_param (string id) "
    "Get codec parameters";
static char pjsua_codec_set_param_doc[] =
    "int py_pjsua.codec_set_param (string id, "
    "py_pjsua.PJMedia_Codec_Param param) "
    "Set codec parameters.";

/* END OF LIB MEDIA */

/* LIB CALL */

/*
 * PyObj_pj_time_val
 * PJ Time Val
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    long sec;
    long msec;

} PyObj_pj_time_val;



/*
 * pj_time_val_members
 */
static PyMemberDef pj_time_val_members[] =
{   
    
    {
        "sec", T_INT, 
        offsetof(PyObj_pj_time_val, sec), 0,
        "The seconds part of the time"
    },
    {
        "msec", T_INT, 
        offsetof(PyObj_pj_time_val, sec), 0,
        "The milliseconds fraction of the time"
    },
    
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pj_time_val
 */
static PyTypeObject PyTyp_pj_time_val =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.PJ_Time_Val",      /*tp_name*/
    sizeof(PyObj_pj_time_val),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    0,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "PJ Time Val objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    pj_time_val_members,         /* tp_members */
    

};

/*
 * PyObj_pjsua_call_info
 * Call Info
 */
typedef struct
{
    PyObject_HEAD
    /* Type-specific fields go here. */ 
    
    int id;
    int role;
    int acc_id;
    PyObject * local_info;
    PyObject * local_contact;
    PyObject * remote_info;
    PyObject * remote_contact;
    PyObject * call_id;
    int state;
    PyObject * state_text;
    int last_status;
    PyObject * last_status_text;
    int media_status;
    int media_dir;
    int conf_slot;
    PyObj_pj_time_val * connect_duration;
    PyObj_pj_time_val * total_duration;
    struct {
	char local_info[128];
	char local_contact[128];
	char remote_info[128];
	char remote_contact[128];
	char call_id[128];
	char last_status_text[128];
    } buf_;

} PyObj_pjsua_call_info;


/*
 * call_info_dealloc
 * deletes a call_info from memory
 */
static void call_info_dealloc(PyObj_pjsua_call_info* self)
{
    Py_XDECREF(self->local_info);
    Py_XDECREF(self->local_contact);
    Py_XDECREF(self->remote_info);
    Py_XDECREF(self->remote_contact);
    Py_XDECREF(self->call_id);
    Py_XDECREF(self->state_text);
    Py_XDECREF(self->last_status_text);
    Py_XDECREF(self->connect_duration);
    Py_XDECREF(self->total_duration);
    self->ob_type->tp_free((PyObject*)self);
}


/*
 * call_info_new
 * constructor for call_info object
 */
static PyObject * call_info_new(PyTypeObject *type, PyObject *args,
                                    PyObject *kwds)
{
    PyObj_pjsua_call_info *self;

    PJ_UNUSED_ARG(args);
    PJ_UNUSED_ARG(kwds);

    self = (PyObj_pjsua_call_info *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->local_info = PyString_FromString("");
        if (self->local_info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }       
	self->local_contact = PyString_FromString("");
        if (self->local_contact == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->remote_info = PyString_FromString("");
        if (self->remote_info == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->remote_contact = PyString_FromString("");
        if (self->remote_contact == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->call_id = PyString_FromString("");
        if (self->call_id == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->state_text = PyString_FromString("");
        if (self->state_text == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->last_status_text = PyString_FromString("");
        if (self->last_status_text == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->connect_duration = (PyObj_pj_time_val *)PyType_GenericNew
	    (&PyTyp_pj_time_val,NULL,NULL);
        if (self->connect_duration == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
	self->total_duration = (PyObj_pj_time_val *)PyType_GenericNew
	    (&PyTyp_pj_time_val,NULL,NULL);
        if (self->total_duration == NULL)
    	{
            Py_DECREF(self);
            return NULL;
        }
    }
    return (PyObject *)self;
}

/*
 * call_info_members
 */
static PyMemberDef call_info_members[] =
{   
    {
        "id", T_INT, 
        offsetof(PyObj_pjsua_call_info, id), 0,
        "Call identification"
    },
    {
        "role", T_INT, 
        offsetof(PyObj_pjsua_call_info, role), 0,
        "Initial call role (UAC == caller)"
    },
    {
        "acc_id", T_INT, 
        offsetof(PyObj_pjsua_call_info, acc_id), 0,
        "The account ID where this call belongs."
    },
    {
        "local_info", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, local_info), 0,
        "Local URI"        
    },
    {
        "local_contact", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, local_contact), 0,
        "Local Contact"        
    },
    {
        "remote_info", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, remote_info), 0,
        "Remote URI"        
    },
    {
        "remote_contact", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, remote_contact), 0,
        "Remote Contact"        
    },
    {
        "call_id", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, call_id), 0,
        "Dialog Call-ID string"        
    },
    {
        "state", T_INT, 
        offsetof(PyObj_pjsua_call_info, state), 0,
        "Call state"
    },
    {
        "state_text", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, state_text), 0,
        "Text describing the state "        
    },
    {
        "last_status", T_INT, 
        offsetof(PyObj_pjsua_call_info, last_status), 0,
        "Last status code heard, which can be used as cause code"
    },
    {
        "last_status_text", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, last_status_text), 0,
        "The reason phrase describing the status."        
    },
    {
        "media_status", T_INT, 
        offsetof(PyObj_pjsua_call_info, media_status), 0,
        "Call media status."
    },
    {
        "media_dir", T_INT, 
        offsetof(PyObj_pjsua_call_info, media_dir), 0,
        "Media direction"
    },
    {
        "conf_slot", T_INT, 
        offsetof(PyObj_pjsua_call_info, conf_slot), 0,
        "The conference port number for the call"
    },
    {
        "connect_duration", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, connect_duration), 0,
        "Up-to-date call connected duration(zero when call is not established)"
    },
    {
        "total_duration", T_OBJECT_EX,
        offsetof(PyObj_pjsua_call_info, total_duration), 0,
        "Total call duration, including set-up time"        
    },
    
    {NULL}  /* Sentinel */
};




/*
 * PyTyp_pjsua_call_info
 */
static PyTypeObject PyTyp_pjsua_call_info =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "py_pjsua.Call_Info",      /*tp_name*/
    sizeof(PyObj_pjsua_call_info),  /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)call_info_dealloc,/*tp_dealloc*/
    0,                              /*tp_print*/
    0,                              /*tp_getattr*/
    0,                              /*tp_setattr*/
    0,                              /*tp_compare*/
    0,                              /*tp_repr*/
    0,                              /*tp_as_number*/
    0,                              /*tp_as_sequence*/
    0,                              /*tp_as_mapping*/
    0,                              /*tp_hash */
    0,                              /*tp_call*/
    0,                              /*tp_str*/
    0,                              /*tp_getattro*/
    0,                              /*tp_setattro*/
    0,                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /*tp_flags*/
    "Call Info objects",       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    call_info_members,         /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    call_info_new,             /* tp_new */

};

/*
 * py_pjsua_call_get_max_count
 */
static PyObject *py_pjsua_call_get_max_count
(PyObject *pSelf, PyObject *pArgs)
{    	
    int count;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    count = pjsua_call_get_max_count();
    
    
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_call_get_count
 */
static PyObject *py_pjsua_call_get_count
(PyObject *pSelf, PyObject *pArgs)
{    	
    
    int count;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    count = pjsua_call_get_count();
    
    
    return Py_BuildValue("i", count);
}

/*
 * py_pjsua_enum_calls
 */
static PyObject *py_pjsua_enum_calls(PyObject *pSelf, PyObject *pArgs)
{
    pj_status_t status;
    PyObject *list;
    
    pjsua_transport_id id[PJSUA_MAX_CALLS];
    unsigned c, i;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    c = PJ_ARRAY_SIZE(id);
    status = pjsua_enum_calls(id, &c);
    
    list = PyList_New(c);
    for (i = 0; i < c; i++) 
    {     
        int ret = PyList_SetItem(list, i, Py_BuildValue("i", id[i]));
        if (ret == -1) 
        {
            return NULL;
        }
    }
    
    return Py_BuildValue("O",list);
}

/*
 * py_pjsua_call_make_call
 */
static PyObject *py_pjsua_call_make_call
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int acc_id;
    pj_str_t dst_uri;
    PyObject * sd;
    unsigned options;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;
    int user_data;
    int call_id;
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple
		(pArgs, "iOIiO", &acc_id, &sd, &options, &user_data, &omdObj))
    {
        return NULL;
    }
	
    dst_uri.ptr = PyString_AsString(sd);
    dst_uri.slen = strlen(PyString_AsString(sd));
    if (omdObj != Py_None) 
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_make_call(acc_id, &dst_uri, 
			options, (void*)user_data, &msg_data, &call_id);	
        pj_pool_release(pool);
    } else {
		
        status = pjsua_call_make_call(acc_id, &dst_uri, 
			options, (void*)user_data, NULL, &call_id);	
    }
	
    return Py_BuildValue("ii",status, call_id);
	
}

/*
 * py_pjsua_call_is_active
 */
static PyObject *py_pjsua_call_is_active
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int isActive;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    isActive = pjsua_call_is_active(call_id);
    
    
    return Py_BuildValue("i", isActive);
}

/*
 * py_pjsua_call_has_media
 */
static PyObject *py_pjsua_call_has_media
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int hasMedia;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    hasMedia = pjsua_call_has_media(call_id);
    
    
    return Py_BuildValue("i", hasMedia);
}

/*
 * py_pjsua_call_get_conf_port
 */
static PyObject *py_pjsua_call_get_conf_port
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int port_id;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    port_id = pjsua_call_get_conf_port(call_id);
    
    
    return Py_BuildValue("i", port_id);
}

/*
 * py_pjsua_call_get_info
 */
static PyObject *py_pjsua_call_get_info
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int status;
    PyObj_pjsua_call_info * oi;
    pjsua_call_info info;
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    
    status = pjsua_call_get_info(call_id, &info);
    if (status == PJ_SUCCESS) 
    {
        oi = (PyObj_pjsua_call_info *)call_info_new(&PyTyp_pjsua_call_info, NULL, NULL);
        oi->acc_id = info.acc_id;
        pj_ansi_snprintf(oi->buf_.call_id, sizeof(oi->buf_.call_id),
	    "%.*s", (int)info.call_id.slen, info.call_id.ptr);
        pj_ansi_snprintf(oi->buf_.last_status_text, 
	    sizeof(oi->buf_.last_status_text),
	    "%.*s", (int)info.last_status_text.slen, info.last_status_text.ptr);
        pj_ansi_snprintf(oi->buf_.local_contact, sizeof(oi->buf_.local_contact),
	    "%.*s", (int)info.local_contact.slen, info.local_contact.ptr);
        pj_ansi_snprintf(oi->buf_.local_info, sizeof(oi->buf_.local_info),
	    "%.*s", (int)info.local_info.slen, info.local_info.ptr);
        pj_ansi_snprintf(oi->buf_.remote_contact,
	    sizeof(oi->buf_.remote_contact),
	    "%.*s", (int)info.remote_contact.slen, info.remote_contact.ptr);
        pj_ansi_snprintf(oi->buf_.remote_info, sizeof(oi->buf_.remote_info),
	    "%.*s", (int)info.remote_info.slen, info.remote_info.ptr);

        oi->call_id = PyString_FromStringAndSize(info.call_id.ptr, 
	    info.call_id.slen);
        oi->conf_slot = info.conf_slot;
        oi->connect_duration->sec = info.connect_duration.sec;
        oi->connect_duration->msec = info.connect_duration.msec;
        oi->total_duration->sec = info.total_duration.sec;
        oi->total_duration->msec = info.total_duration.msec;
        oi->id = info.id;
        oi->last_status = info.last_status;
        oi->last_status_text = PyString_FromStringAndSize(
	    info.last_status_text.ptr, info.last_status_text.slen);
        oi->local_contact = PyString_FromStringAndSize(
	    info.local_contact.ptr, info.local_contact.slen);
        oi->local_info = PyString_FromStringAndSize(
   	    info.local_info.ptr, info.local_info.slen);
        oi->remote_contact = PyString_FromStringAndSize(
	    info.remote_contact.ptr, info.remote_contact.slen);
        oi->remote_info = PyString_FromStringAndSize(
	    info.remote_info.ptr, info.remote_info.slen);
        oi->media_dir = info.media_dir;
        oi->media_status = info.media_status;
        oi->role = info.role;
        oi->state = info.state;
        oi->state_text = PyString_FromStringAndSize(
   	    info.state_text.ptr, info.state_text.slen);

	return Py_BuildValue("O", oi);
    } else {
	Py_INCREF(Py_None);
	return Py_None;
    }
}

/*
 * py_pjsua_call_set_user_data
 */
static PyObject *py_pjsua_call_set_user_data
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int user_data;	
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "ii", &call_id, &user_data))
    {
        return NULL;
    }	
    
    status = pjsua_call_set_user_data(call_id, (void*)user_data);
    
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_get_user_data
 */
static PyObject *py_pjsua_call_get_user_data
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    void * user_data;	
    
    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &call_id))
    {
        return NULL;
    }	
    
    user_data = pjsua_call_get_user_data(call_id);
    
    
    return Py_BuildValue("i", (int)user_data);
}

/*
 * py_pjsua_call_answer
 */
static PyObject *py_pjsua_call_answer
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t * reason, tmp_reason;
    PyObject * sr;
    unsigned code;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iIOO", &call_id, &code, &sr, &omdObj))
    {
        return NULL;
    }
    if (sr == Py_None) 
    {
        reason = NULL;
    } else {
	reason = &tmp_reason;
        tmp_reason.ptr = PyString_AsString(sr);
        tmp_reason.slen = strlen(PyString_AsString(sr));
    }
    if (omdObj != Py_None) 
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
	
        status = pjsua_call_answer(call_id, code, reason, &msg_data);	
    
        pj_pool_release(pool);
    } else {
	
        status = pjsua_call_answer(call_id, code, reason, NULL);
	
    }
    
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_hangup
 */
static PyObject *py_pjsua_call_hangup
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t * reason, tmp_reason;
    PyObject * sr;
    unsigned code;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool = NULL;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iIOO", &call_id, &code, &sr, &omdObj))
    {
        return NULL;
    }
    if (sr == Py_None)
    {
        reason = NULL;
    } else {
        reason = &tmp_reason;
        tmp_reason.ptr = PyString_AsString(sr);
        tmp_reason.slen = strlen(PyString_AsString(sr));
    }
    if (omdObj != Py_None) 
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_hangup(call_id, code, reason, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_hangup(call_id, code, reason, NULL);	
    }
    
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_set_hold
 */
static PyObject *py_pjsua_call_set_hold
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &omdObj))
    {
        return NULL;
    }

    if (omdObj != Py_None) 
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_set_hold(call_id, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_set_hold(call_id, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_reinvite
 */
static PyObject *py_pjsua_call_reinvite
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    int unhold;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &unhold, &omdObj))
    {
        return NULL;
    }

    if (omdObj != Py_None)
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_reinvite(call_id, unhold, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_reinvite(call_id, unhold, NULL);
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_xfer
 */
static PyObject *py_pjsua_call_xfer
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t dest;
    PyObject * sd;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iOO", &call_id, &sd, &omdObj))
    {
        return NULL;
    }
	
    dest.ptr = PyString_AsString(sd);
    dest.slen = strlen(PyString_AsString(sd));
    
    if (omdObj != Py_None)
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_xfer(call_id, &dest, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_xfer(call_id, &dest, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_xfer_replaces
 */
static PyObject *py_pjsua_call_xfer_replaces
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    int dest_call_id;
    unsigned options;    
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple
		(pArgs, "iiIO", &call_id, &dest_call_id, &options, &omdObj))
    {
        return NULL;
    }
	
    if (omdObj != Py_None)
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;    
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_xfer_replaces
			(call_id, dest_call_id, options, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_xfer_replaces(call_id, dest_call_id,options, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_dial_dtmf
 */
static PyObject *py_pjsua_call_dial_dtmf
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    PyObject * sd;
    pj_str_t digits;
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iO", &call_id, &sd))
    {
        return NULL;
    }	
    digits.ptr = PyString_AsString(sd);
    digits.slen = strlen(PyString_AsString(sd));
    status = pjsua_call_dial_dtmf(call_id, &digits);
    
    return Py_BuildValue("i", status);
}

/*
 * py_pjsua_call_send_im
 */
static PyObject *py_pjsua_call_send_im
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;
    pj_str_t content;
    pj_str_t * mime_type, tmp_mime_type;
    PyObject * sm;
    PyObject * sc;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    int user_data;
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple
		(pArgs, "iOOOi", &call_id, &sm, &sc, &omdObj, &user_data))
    {
        return NULL;
    }
    if (sm == Py_None)
    {
        mime_type = NULL;
    } else {
        mime_type = &tmp_mime_type;
        tmp_mime_type.ptr = PyString_AsString(sm);
        tmp_mime_type.slen = strlen(PyString_AsString(sm));
    }
    content.ptr = PyString_AsString(sc);
    content.slen = strlen(PyString_AsString(sc));
    
    if (omdObj != Py_None)
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_send_im
		(call_id, mime_type, &content, &msg_data, (void *)user_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_send_im
			(call_id, mime_type, &content, NULL, (void *)user_data);	
    }
    
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_send_typing_ind
 */
static PyObject *py_pjsua_call_send_typing_ind
(PyObject *pSelf, PyObject *pArgs)
{    
    int status;
    int call_id;    
    int is_typing;
    pjsua_msg_data msg_data;
    PyObject * omdObj;
    PyObj_pjsua_msg_data * omd;    
    pj_pool_t * pool;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiO", &call_id, &is_typing, &omdObj))
    {
        return NULL;
    }
	
    if (omdObj != Py_None)
    {
        omd = (PyObj_pjsua_msg_data *)omdObj;
        msg_data.content_type.ptr = PyString_AsString(omd->content_type);
        msg_data.content_type.slen = strlen
			(PyString_AsString(omd->content_type));
        msg_data.msg_body.ptr = PyString_AsString(omd->msg_body);
        msg_data.msg_body.slen = strlen(PyString_AsString(omd->msg_body));
        pool = pjsua_pool_create("pjsua", POOL_SIZE, POOL_SIZE);
        translate_hdr(pool, &msg_data.hdr_list, omd->hdr_list);
        status = pjsua_call_send_typing_ind(call_id, is_typing, &msg_data);	
        pj_pool_release(pool);
    } else {
        status = pjsua_call_send_typing_ind(call_id, is_typing, NULL);	
    }
    return Py_BuildValue("i",status);
}

/*
 * py_pjsua_call_hangup_all
 */
static PyObject *py_pjsua_call_hangup_all
(PyObject *pSelf, PyObject *pArgs)
{    	

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, ""))
    {
        return NULL;
    }	
    
    pjsua_call_hangup_all();
    
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * py_pjsua_call_dump
 */
static PyObject *py_pjsua_call_dump
(PyObject *pSelf, PyObject *pArgs)
{    	
    int call_id;
    int with_media;
    PyObject * sb;
    PyObject * si;
    char * buffer;
    char * indent;
    unsigned maxlen;    
    int status;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "iiIO", &call_id, &with_media, &maxlen, &si))
    {
        return NULL;
    }	
    buffer = (char *) malloc (maxlen * sizeof(char));
    indent = PyString_AsString(si);
    
    status = pjsua_call_dump(call_id, with_media, buffer, maxlen, indent);
    sb = PyString_FromStringAndSize(buffer, maxlen);
    free(buffer);
    return Py_BuildValue("O", sb);
}


/*
 * py_pjsua_dump
 * Dump application states.
 */
static PyObject *py_pjsua_dump(PyObject *pSelf, PyObject *pArgs)
{
    unsigned old_decor;
    char buf[1024];
    int detail;

    PJ_UNUSED_ARG(pSelf);

    if (!PyArg_ParseTuple(pArgs, "i", &detail))
    {
        return NULL;
    }	

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    if (detail)
	pj_dump_config();

    pjsip_endpt_dump(pjsua_get_pjsip_endpt(), detail);
    pjmedia_endpt_dump(pjsua_get_pjmedia_endpt());
    pjsip_tsx_layer_dump(detail);
    pjsip_ua_dump(detail);


    /* Dump all invite sessions: */
    PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

    if (pjsua_call_get_count() == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

    } else {
	unsigned i, max;

	max = pjsua_call_get_max_count();
	for (i=0; i<max; ++i) {
	    if (pjsua_call_is_active(i)) {
		pjsua_call_dump(i, detail, buf, sizeof(buf), "  ");
		PJ_LOG(3,(THIS_FILE, "%s", buf));
	    }
	}
    }

    /* Dump presence status */
    pjsua_pres_dump(detail);

    pj_log_set_decor(old_decor);
    PJ_LOG(3,(THIS_FILE, "Dump complete"));

    Py_INCREF(Py_None);
    return Py_None;
}


static char pjsua_call_get_max_count_doc[] =
    "int py_pjsua.call_get_max_count () "
    "Get maximum number of calls configured in pjsua.";
static char pjsua_call_get_count_doc[] =
    "int py_pjsua.call_get_count () "
    "Get number of currently active calls.";
static char pjsua_enum_calls_doc[] =
    "int[] py_pjsua.enum_calls () "
    "Get maximum number of calls configured in pjsua.";
static char pjsua_call_make_call_doc[] =
    "int,int py_pjsua.call_make_call (int acc_id, string dst_uri, int options,"
    "int user_data, py_pjsua.Msg_Data msg_data) "
    "Make outgoing call to the specified URI using the specified account.";
static char pjsua_call_is_active_doc[] =
    "int py_pjsua.call_is_active (int call_id) "
    "Check if the specified call has active INVITE session and the INVITE "
    "session has not been disconnected.";
static char pjsua_call_has_media_doc[] =
    "int py_pjsua.call_has_media (int call_id) "
    "Check if call has an active media session.";
static char pjsua_call_get_conf_port_doc[] =
    "int py_pjsua.call_get_conf_port (int call_id) "
    "Get the conference port identification associated with the call.";
static char pjsua_call_get_info_doc[] =
    "py_pjsua.Call_Info py_pjsua.call_get_info (int call_id) "
    "Obtain detail information about the specified call.";
static char pjsua_call_set_user_data_doc[] =
    "int py_pjsua.call_set_user_data (int call_id, int user_data) "
    "Attach application specific data to the call.";
static char pjsua_call_get_user_data_doc[] =
    "int py_pjsua.call_get_user_data (int call_id) "
    "Get user data attached to the call.";
static char pjsua_call_answer_doc[] =
    "int py_pjsua.call_answer (int call_id, int code, string reason, "
    "py_pjsua.Msg_Data msg_data) "
    "Send response to incoming INVITE request.";
static char pjsua_call_hangup_doc[] =
    "int py_pjsua.call_hangup (int call_id, int code, string reason, "
    "py_pjsua.Msg_Data msg_data) "
    "Hangup call by using method that is appropriate according "
    "to the call state.";
static char pjsua_call_set_hold_doc[] =
    "int py_pjsua.call_set_hold (int call_id, py_pjsua.Msg_Data msg_data) "
    "Put the specified call on hold.";
static char pjsua_call_reinvite_doc[] =
    "int py_pjsua.call_reinvite (int call_id, int unhold, "
    "py_pjsua.Msg_Data msg_data) "
    "Send re-INVITE (to release hold).";
static char pjsua_call_xfer_doc[] =
    "int py_pjsua.call_xfer (int call_id, string dest, "
    "py_pjsua.Msg_Data msg_data) "
    "Initiate call transfer to the specified address. "
    "This function will send REFER request to instruct remote call party "
    "to initiate a new INVITE session to the specified destination/target.";
static char pjsua_call_xfer_replaces_doc[] =
    "int py_pjsua.call_xfer_replaces (int call_id, int dest_call_id, "
    "int options, py_pjsua.Msg_Data msg_data) "
    "Initiate attended call transfer. This function will send REFER request "
    "to instruct remote call party to initiate new INVITE session to the URL "
    "of dest_call_id. The party at dest_call_id then should 'replace' the call"
    "with us with the new call from the REFER recipient.";
static char pjsua_call_dial_dtmf_doc[] =
    "int py_pjsua.call_dial_dtmf (int call_id, string digits) "
    "Send DTMF digits to remote using RFC 2833 payload formats.";
static char pjsua_call_send_im_doc[] =
    "int py_pjsua.call_send_im (int call_id, string mime_type, string content,"
    "py_pjsua.Msg_Data msg_data, int user_data) "
    "Send instant messaging inside INVITE session.";
static char pjsua_call_send_typing_ind_doc[] =
    "int py_pjsua.call_send_typing_ind (int call_id, int is_typing, "
    "py_pjsua.Msg_Data msg_data) "
    "Send IM typing indication inside INVITE session.";
static char pjsua_call_hangup_all_doc[] =
    "void py_pjsua.call_hangup_all () "
    "Terminate all calls.";
static char pjsua_call_dump_doc[] =
    "int py_pjsua.call_dump (int call_id, int with_media, int maxlen, "
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
    	"pool_create", py_pjsua_pool_create, METH_VARARGS,
    	pjsua_pool_create_doc
    },
    {
    	"get_pjsip_endpt", py_pjsua_get_pjsip_endpt, METH_VARARGS,
    	pjsua_get_pjsip_endpt_doc
    },
    {
    	"get_pjmedia_endpt", py_pjsua_get_pjmedia_endpt, METH_VARARGS,
    	pjsua_get_pjmedia_endpt_doc
    },
    {
    	"get_pool_factory", py_pjsua_get_pool_factory, METH_VARARGS,
    	pjsua_get_pool_factory_doc
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
        "enum_accs", py_pjsua_enum_accs, METH_VARARGS,
        pjsua_enum_accs_doc
    },
    {
        "acc_enum_info", py_pjsua_acc_enum_info, METH_VARARGS,
        pjsua_acc_enum_info_doc
    },
    {
        "acc_find_for_outgoing", py_pjsua_acc_find_for_outgoing, METH_VARARGS,
        pjsua_acc_find_for_outgoing_doc
    },
    {
        "acc_find_for_incoming", py_pjsua_acc_find_for_incoming, METH_VARARGS,
        pjsua_acc_find_for_incoming_doc
    },
    {
        "acc_create_uac_contact", py_pjsua_acc_create_uac_contact, METH_VARARGS,
        pjsua_acc_create_uac_contact_doc
    },
    {
        "acc_create_uas_contact", py_pjsua_acc_create_uas_contact, METH_VARARGS,
        pjsua_acc_create_uas_contact_doc
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
        "conf_add_port", py_pjsua_conf_add_port, METH_VARARGS,
        pjsua_conf_add_port_doc
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
        "player_create", py_pjsua_player_create, METH_VARARGS,
        pjsua_player_create_doc
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
        "set_no_snd_dev", py_pjsua_set_no_snd_dev, METH_VARARGS,
        pjsua_set_no_snd_dev_doc
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
	"dump", py_pjsua_dump, METH_VARARGS, "Dump application state"
    },

    
    {NULL, NULL} /* end of function list */
};



/*
 * Mapping C structs from and to Python objects & initializing object
 */
DL_EXPORT(void)
initpy_pjsua(void)
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
    PyTyp_pjsip_event.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjsip_event) < 0)
        return;
    PyTyp_pjsip_rx_data.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjsip_rx_data) < 0)
        return;
    PyTyp_pj_pool_t.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pj_pool_t) < 0)
        return;
    PyTyp_pjsip_endpoint.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjsip_endpoint) < 0)
        return;
    PyTyp_pjmedia_endpt.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjmedia_endpt) < 0)
        return;
    PyTyp_pj_pool_factory.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pj_pool_factory) < 0)
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

    PyTyp_pjmedia_port.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pjmedia_port) < 0)
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

    PyTyp_pj_time_val.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTyp_pj_time_val) < 0)
        return;

    if (PyType_Ready(&PyTyp_pjsua_call_info) < 0)
        return;

    /* END OF LIB CALL */

    m = Py_InitModule3(
        "py_pjsua", py_pjsua_methods,"PJSUA-lib module for python"
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

    Py_INCREF(&PyTyp_pjsip_event);
    PyModule_AddObject(m, "Pjsip_Event", (PyObject *)&PyTyp_pjsip_event);

    Py_INCREF(&PyTyp_pjsip_rx_data);
    PyModule_AddObject(m, "Pjsip_Rx_Data", (PyObject *)&PyTyp_pjsip_rx_data);

    Py_INCREF(&PyTyp_pj_pool_t);
    PyModule_AddObject(m, "Pj_Pool", (PyObject *)&PyTyp_pj_pool_t);

    Py_INCREF(&PyTyp_pjsip_endpoint);
    PyModule_AddObject(m, "Pjsip_Endpoint", (PyObject *)&PyTyp_pjsip_endpoint);

    Py_INCREF(&PyTyp_pjmedia_endpt);
    PyModule_AddObject(m, "Pjmedia_Endpt", (PyObject *)&PyTyp_pjmedia_endpt);

    Py_INCREF(&PyTyp_pj_pool_factory);
    PyModule_AddObject(
        m, "Pj_Pool_Factory", (PyObject *)&PyTyp_pj_pool_factory
    );

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
    Py_INCREF(&PyTyp_pjmedia_port);
    PyModule_AddObject(m, "PJMedia_Port", (PyObject *)&PyTyp_pjmedia_port);
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

    Py_INCREF(&PyTyp_pj_time_val);
    PyModule_AddObject(m, "PJ_Time_Val", (PyObject *)&PyTyp_pj_time_val);

    Py_INCREF(&PyTyp_pjsua_call_info);
    PyModule_AddObject(m, "Call_Info", (PyObject *)&PyTyp_pjsua_call_info);

    /* END OF LIB CALL */


    /*
     * Add various constants.
     */

    /* Call states */
    ADD_CONSTANT(m, PJSIP_INV_STATE_NULL);
    ADD_CONSTANT(m, PJSIP_INV_STATE_CALLING);
    ADD_CONSTANT(m, PJSIP_INV_STATE_INCOMING);
    ADD_CONSTANT(m, PJSIP_INV_STATE_EARLY);
    ADD_CONSTANT(m, PJSIP_INV_STATE_CONNECTING);
    ADD_CONSTANT(m, PJSIP_INV_STATE_CONFIRMED);
    ADD_CONSTANT(m, PJSIP_INV_STATE_DISCONNECTED);

    /* Call media status (enum pjsua_call_media_status) */
    ADD_CONSTANT(m, PJSUA_CALL_MEDIA_NONE);
    ADD_CONSTANT(m, PJSUA_CALL_MEDIA_ACTIVE);
    ADD_CONSTANT(m, PJSUA_CALL_MEDIA_LOCAL_HOLD);
    ADD_CONSTANT(m, PJSUA_CALL_MEDIA_REMOTE_HOLD);

    /* Buddy status */
    ADD_CONSTANT(m, PJSUA_BUDDY_STATUS_UNKNOWN);
    ADD_CONSTANT(m, PJSUA_BUDDY_STATUS_ONLINE);
    ADD_CONSTANT(m, PJSUA_BUDDY_STATUS_OFFLINE);

    /* PJSIP transport types (enum pjsip_transport_type_e) */
    ADD_CONSTANT(m, PJSIP_TRANSPORT_UNSPECIFIED);
    ADD_CONSTANT(m, PJSIP_TRANSPORT_UDP);
    ADD_CONSTANT(m, PJSIP_TRANSPORT_TCP);
    ADD_CONSTANT(m, PJSIP_TRANSPORT_TLS);
    ADD_CONSTANT(m, PJSIP_TRANSPORT_SCTP);
    ADD_CONSTANT(m, PJSIP_TRANSPORT_LOOP);
    ADD_CONSTANT(m, PJSIP_TRANSPORT_LOOP_DGRAM);


    /* Invalid IDs */
    ADD_CONSTANT(m, PJSUA_INVALID_ID);


    /* Various compile time constants */
    ADD_CONSTANT(m, PJSUA_ACC_MAX_PROXIES);
    ADD_CONSTANT(m, PJSUA_MAX_ACC);
    ADD_CONSTANT(m, PJSUA_REG_INTERVAL);
    ADD_CONSTANT(m, PJSUA_PUBLISH_EXPIRATION);
    ADD_CONSTANT(m, PJSUA_DEFAULT_ACC_PRIORITY);
    ADD_CONSTANT(m, PJSUA_MAX_BUDDIES);
    ADD_CONSTANT(m, PJSUA_MAX_CONF_PORTS);
    ADD_CONSTANT(m, PJSUA_DEFAULT_CLOCK_RATE);
    ADD_CONSTANT(m, PJSUA_DEFAULT_CODEC_QUALITY);
    ADD_CONSTANT(m, PJSUA_DEFAULT_ILBC_MODE);
    ADD_CONSTANT(m, PJSUA_DEFAULT_EC_TAIL_LEN);
    ADD_CONSTANT(m, PJSUA_MAX_CALLS);
    ADD_CONSTANT(m, PJSUA_XFER_NO_REQUIRE_REPLACES);


#undef ADD_CONSTANT
}
