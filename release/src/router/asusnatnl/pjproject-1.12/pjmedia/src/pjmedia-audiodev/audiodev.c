/* $Id: audiodev.c 3611 2011-07-07 09:26:26Z bennylp $ */
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE "audiodev.c"


#define DEFINE_CAP(name, info)	{name, info}

/* Capability names */
static struct cap_info
{
    const char *name;
    const char *info;
} cap_infos[] = 
{
    DEFINE_CAP("ext-fmt",     "Extended/non-PCM format"),
    DEFINE_CAP("latency-in",  "Input latency/buffer size setting"),
    DEFINE_CAP("latency-out", "Output latency/buffer size setting"),
    DEFINE_CAP("vol-in",      "Input volume setting"),
    DEFINE_CAP("vol-out",     "Output volume setting"),
    DEFINE_CAP("meter-in",    "Input meter"),
    DEFINE_CAP("meter-out",   "Output meter"),
    DEFINE_CAP("route-in",    "Input routing"),
    DEFINE_CAP("route-out",   "Output routing"),
    DEFINE_CAP("aec",	      "Accoustic echo cancellation"),
    DEFINE_CAP("aec-tail",    "Tail length setting for AEC"),
    DEFINE_CAP("vad",	      "Voice activity detection"),
    DEFINE_CAP("cng",	      "Comfort noise generation"),
    DEFINE_CAP("plg",	      "Packet loss concealment")
};


/*
 * The device index seen by application and driver is different. 
 *
 * At application level, device index is index to global list of device.
 * At driver level, device index is index to device list on that particular
 * factory only.
 */
#define MAKE_DEV_ID(f_id, index)   (((f_id & 0xFFFF) << 16) | (index & 0xFFFF))
#define GET_INDEX(dev_id)	   ((dev_id) & 0xFFFF)
#define GET_FID(dev_id)		   ((dev_id) >> 16)
#define DEFAULT_DEV_ID		    0


/* extern functions to create factories */
#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
pjmedia_aud_dev_factory* pjmedia_pa_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_COREAUDIO
pjmedia_aud_dev_factory* pjmedia_coreaudio_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_ALSA
pjmedia_aud_dev_factory* pjmedia_alsa_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_WMME
pjmedia_aud_dev_factory* pjmedia_wmme_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS
pjmedia_aud_dev_factory* pjmedia_symb_vas_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_APS
pjmedia_aud_dev_factory* pjmedia_aps_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA
pjmedia_aud_dev_factory* pjmedia_symb_mda_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO
pjmedia_aud_dev_factory* pjmedia_null_audio_factory(pj_pool_factory *pf);
#endif

#define MAX_DRIVERS	16
#define MAX_DEVS	64


/* driver structure */
struct driver
{
    /* Creation function */
    pjmedia_aud_dev_factory_create_func_ptr create;
    /* Factory instance */
    pjmedia_aud_dev_factory *f;
    char		     name[32];	/* Driver name			    */
    unsigned		     dev_cnt;	/* Number of devices		    */
    unsigned		     start_idx;	/* Start index in global list	    */
    int			     rec_dev_idx;/* Default capture device.	    */
    int			     play_dev_idx;/* Default playback device	    */
    int			     dev_idx;	/* Default device.		    */
};

/* The audio subsystem */
static struct aud_subsys
{
    unsigned	     init_count;	/* How many times init() is called  */
    pj_pool_factory *pf;		/* The pool factory.		    */

    unsigned	     drv_cnt;		/* Number of drivers.		    */
    struct driver    drv[MAX_DRIVERS];	/* Array of drivers.		    */

    unsigned	     dev_cnt;		/* Total number of devices.	    */
    pj_uint32_t	     dev_list[MAX_DEVS];/* Array of device IDs.		    */

} aud_subsys;

/* API: get capability name/info */
PJ_DEF(const char*) pjmedia_aud_dev_cap_name(pjmedia_aud_dev_cap cap,
					     const char **p_desc)
{
    const char *desc;
    unsigned i;

    if (p_desc==NULL) p_desc = &desc;

    for (i=0; i<PJ_ARRAY_SIZE(cap_infos); ++i) {
	if ((1 << i)==cap)
	    break;
    }

    if (i==PJ_ARRAY_SIZE(cap_infos)) {
	*p_desc = "??";
	return "??";
    }

    *p_desc = cap_infos[i].info;
    return cap_infos[i].name;
}

static pj_status_t get_cap_pointer(const pjmedia_aud_param *param,
				   pjmedia_aud_dev_cap cap,
				   void **ptr,
				   unsigned *size)
{
#define FIELD_INFO(name)    *ptr = (void*)&param->name; \
			    *size = sizeof(param->name)

    switch (cap) {
    case PJMEDIA_AUD_DEV_CAP_EXT_FORMAT:
	FIELD_INFO(ext_fmt);
	break;
    case PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY:
	FIELD_INFO(input_latency_ms);
	break;
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY:
	FIELD_INFO(output_latency_ms);
	break;
    case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
	FIELD_INFO(input_vol);
	break;
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
	FIELD_INFO(output_vol);
	break;
    case PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE:
	FIELD_INFO(input_route);
	break;
    case PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE:
	FIELD_INFO(output_route);
	break;
    case PJMEDIA_AUD_DEV_CAP_EC:
	FIELD_INFO(ec_enabled);
	break;
    case PJMEDIA_AUD_DEV_CAP_EC_TAIL:
	FIELD_INFO(ec_tail_ms);
	break;
    case PJMEDIA_AUD_DEV_CAP_VAD:
	FIELD_INFO(ext_fmt.vad);
	break;
    case PJMEDIA_AUD_DEV_CAP_CNG:
	FIELD_INFO(cng_enabled);
	break;
    case PJMEDIA_AUD_DEV_CAP_PLC:
	FIELD_INFO(plc_enabled);
	break;
    default:
	return PJMEDIA_EAUD_INVCAP;
    }

#undef FIELD_INFO

    return PJ_SUCCESS;
}

/* API: set cap value to param */
PJ_DEF(pj_status_t) pjmedia_aud_param_set_cap( pjmedia_aud_param *param,
					       pjmedia_aud_dev_cap cap,
					       const void *pval)
{
    void *cap_ptr;
    unsigned cap_size;
    pj_status_t status;

    status = get_cap_pointer(param, cap, &cap_ptr, &cap_size);
    if (status != PJ_SUCCESS)
	return status;

    pj_memcpy(cap_ptr, pval, cap_size);
    param->flags |= cap;

    return PJ_SUCCESS;
}

/* API: get cap value from param */
PJ_DEF(pj_status_t) pjmedia_aud_param_get_cap( const pjmedia_aud_param *param,
					       pjmedia_aud_dev_cap cap,
					       void *pval)
{
    void *cap_ptr;
    unsigned cap_size;
    pj_status_t status;

    status = get_cap_pointer(param, cap, &cap_ptr, &cap_size);
    if (status != PJ_SUCCESS)
	return status;

    if ((param->flags & cap) == 0) {
	pj_bzero(cap_ptr, cap_size);
	return PJMEDIA_EAUD_INVCAP;
    }

    pj_memcpy(pval, cap_ptr, cap_size);
    return PJ_SUCCESS;
}

/* Internal: init driver */
static pj_status_t init_driver(unsigned drv_idx, pj_bool_t refresh)
{
    struct driver *drv = &aud_subsys.drv[drv_idx];
    pjmedia_aud_dev_factory *f;
    unsigned i, dev_cnt;
    pj_status_t status;

    if (!refresh) {
	/* Create the factory */
	f = (*drv->create)(aud_subsys.pf);
	if (!f)
	    return PJ_EUNKNOWN;

	/* Call factory->init() */
	status = f->op->init(f);
	if (status != PJ_SUCCESS) {
	    f->op->destroy(f);
	    return status;
	}
    } else {
	f = drv->f;
    }

    /* Get number of devices */
    dev_cnt = f->op->get_dev_count(f);
    if (dev_cnt + aud_subsys.dev_cnt > MAX_DEVS) {
	PJ_LOG(4,(THIS_FILE, "%d device(s) cannot be registered because"
			      " there are too many devices",
			      aud_subsys.dev_cnt + dev_cnt - MAX_DEVS));
	dev_cnt = MAX_DEVS - aud_subsys.dev_cnt;
    }

    /* enabling this will cause pjsua-lib initialization to fail when there
     * is no sound device installed in the system, even when pjsua has been
     * run with --null-audio
     *
    if (dev_cnt == 0) {
	f->op->destroy(f);
	return PJMEDIA_EAUD_NODEV;
    }
    */

    /* Fill in default devices */
    drv->play_dev_idx = drv->rec_dev_idx = drv->dev_idx = -1;
    for (i=0; i<dev_cnt; ++i) {
	pjmedia_aud_dev_info info;

	status = f->op->get_dev_info(f, i, &info);
	if (status != PJ_SUCCESS) {
	    f->op->destroy(f);
	    return status;
	}

	if (drv->name[0]=='\0') {
	    /* Set driver name */
	    pj_ansi_strncpy(drv->name, info.driver, sizeof(drv->name));
	    drv->name[sizeof(drv->name)-1] = '\0';
	}

	if (drv->play_dev_idx < 0 && info.output_count) {
	    /* Set default playback device */
	    drv->play_dev_idx = i;
	}
	if (drv->rec_dev_idx < 0 && info.input_count) {
	    /* Set default capture device */
	    drv->rec_dev_idx = i;
	}
	if (drv->dev_idx < 0 && info.input_count &&
	    info.output_count)
	{
	    /* Set default capture and playback device */
	    drv->dev_idx = i;
	}

	if (drv->play_dev_idx >= 0 && drv->rec_dev_idx >= 0 && 
	    drv->dev_idx >= 0) 
	{
	    /* Done. */
	    break;
	}
    }

    /* Register the factory */
    drv->f = f;
    drv->f->sys.drv_idx = drv_idx;
    drv->start_idx = aud_subsys.dev_cnt;
    drv->dev_cnt = dev_cnt;

    /* Register devices to global list */
    for (i=0; i<dev_cnt; ++i) {
	aud_subsys.dev_list[aud_subsys.dev_cnt++] = MAKE_DEV_ID(drv_idx, i);
    }

    return PJ_SUCCESS;
}

/* Internal: deinit driver */
static void deinit_driver(unsigned drv_idx)
{
    struct driver *drv = &aud_subsys.drv[drv_idx];

    if (drv->f) {
	drv->f->op->destroy(drv->f);
	drv->f = NULL;
    }

    drv->dev_cnt = 0;
    drv->play_dev_idx = drv->rec_dev_idx = drv->dev_idx = -1;
}

/* API: Initialize the audio subsystem. */
PJ_DEF(pj_status_t) pjmedia_aud_subsys_init(pj_pool_factory *pf)
{
#if 1
	PJ_UNUSED_ARG(pf);
	return PJ_SUCCESS;
#else
    unsigned i;
    pj_status_t status;

    /* Allow init() to be called multiple times as long as there is matching
     * number of shutdown().
     */
    if (aud_subsys.init_count++ != 0) {
	return PJ_SUCCESS;
    }

    /* Register error subsystem */
    status = pj_register_strerror(PJMEDIA_AUDIODEV_ERRNO_START, 
				  PJ_ERRNO_SPACE_SIZE, 
				  &pjmedia_audiodev_strerror);
    pj_assert(status == PJ_SUCCESS);

    /* Init */
    aud_subsys.pf = pf;
    aud_subsys.drv_cnt = 0;
    aud_subsys.dev_cnt = 0;

    /* Register creation functions */
#if PJMEDIA_AUDIO_DEV_HAS_ALSA
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_alsa_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_COREAUDIO
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_coreaudio_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_pa_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_WMME
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_wmme_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_symb_vas_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_SYMB_APS
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_aps_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_symb_mda_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_null_audio_factory;
#endif

    /* Initialize each factory and build the device ID list */
    for (i=0; i<aud_subsys.drv_cnt; ++i) {
	status = init_driver(i, PJ_FALSE);
	if (status != PJ_SUCCESS) {
	    deinit_driver(i);
	    continue;
	}
    }

    return aud_subsys.dev_cnt ? PJ_SUCCESS : status;
#endif
}

/* API: register an audio device factory to the audio subsystem. */
PJ_DEF(pj_status_t)
pjmedia_aud_register_factory(pjmedia_aud_dev_factory_create_func_ptr adf)
{
    pj_status_t status;

    if (aud_subsys.init_count == 0)
	return PJMEDIA_EAUD_INIT;

    aud_subsys.drv[aud_subsys.drv_cnt].create = adf;
    status = init_driver(aud_subsys.drv_cnt, PJ_FALSE);
    if (status == PJ_SUCCESS) {
	aud_subsys.drv_cnt++;
    } else {
	deinit_driver(aud_subsys.drv_cnt);
    }

    return status;
}

/* API: unregister an audio device factory from the audio subsystem. */
PJ_DEF(pj_status_t)
pjmedia_aud_unregister_factory(pjmedia_aud_dev_factory_create_func_ptr adf)
{
    unsigned i, j;

    if (aud_subsys.init_count == 0)
	return PJMEDIA_EAUD_INIT;

    for (i=0; i<aud_subsys.drv_cnt; ++i) {
	struct driver *drv = &aud_subsys.drv[i];

	if (drv->create == adf) {
	    for (j = drv->start_idx; j < drv->start_idx + drv->dev_cnt; j++)
	    {
		aud_subsys.dev_list[j] = (pj_uint32_t)PJMEDIA_AUD_INVALID_DEV;
	    }

	    deinit_driver(i);
	    pj_bzero(drv, sizeof(*drv));
	    return PJ_SUCCESS;
	}
    }

    return PJMEDIA_EAUD_ERR;
}

/* API: get the pool factory registered to the audio subsystem. */
PJ_DEF(pj_pool_factory*) pjmedia_aud_subsys_get_pool_factory(void)
{
    return aud_subsys.pf;
}

/* API: Shutdown the audio subsystem. */
PJ_DEF(pj_status_t) pjmedia_aud_subsys_shutdown(void)
{
    unsigned i;

    /* Allow shutdown() to be called multiple times as long as there is matching
     * number of init().
     */
    if (aud_subsys.init_count == 0) {
	return PJ_SUCCESS;
    }
    --aud_subsys.init_count;

    if (aud_subsys.init_count == 0) {
	for (i=0; i<aud_subsys.drv_cnt; ++i) {
	    deinit_driver(i);
	}

	aud_subsys.pf = NULL;
    }
    return PJ_SUCCESS;
}

/* API: Refresh the list of sound devices installed in the system. */
PJ_DEF(pj_status_t) pjmedia_aud_dev_refresh(void)
{
    unsigned i;
    
    aud_subsys.dev_cnt = 0;
    for (i=0; i<aud_subsys.drv_cnt; ++i) {
	struct driver *drv = &aud_subsys.drv[i];
	
	if (drv->f && drv->f->op->refresh) {
	    pj_status_t status = drv->f->op->refresh(drv->f);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(4, (THIS_FILE, status, "Unable to refresh device "
						 "list for %s", drv->name));
	    }
	}
	init_driver(i, PJ_TRUE);
    }
    return PJ_SUCCESS;
}

/* API: Get the number of sound devices installed in the system. */
PJ_DEF(unsigned) pjmedia_aud_dev_count(void)
{
    return aud_subsys.dev_cnt;
}

/* Internal: convert local index to global device index */
static pj_status_t make_global_index(unsigned drv_idx, 
				     pjmedia_aud_dev_index *id)
{
    if (*id < 0) {
	return PJ_SUCCESS;
    }

    /* Check that factory still exists */
    PJ_ASSERT_RETURN(aud_subsys.drv[drv_idx].f, PJ_EBUG);

    /* Check that device index is valid */
    PJ_ASSERT_RETURN(*id>=0 && *id<(int)aud_subsys.drv[drv_idx].dev_cnt, 
		     PJ_EBUG);

    *id += aud_subsys.drv[drv_idx].start_idx;
    return PJ_SUCCESS;
}

/* Internal: lookup device id */
static pj_status_t lookup_dev(pjmedia_aud_dev_index id,
			      pjmedia_aud_dev_factory **p_f,
			      unsigned *p_local_index)
{
    int f_id, index;

    if (id < 0) {
	unsigned i;

	if (id == PJMEDIA_AUD_INVALID_DEV)
	    return PJMEDIA_EAUD_INVDEV;

	for (i=0; i<aud_subsys.drv_cnt; ++i) {
	    struct driver *drv = &aud_subsys.drv[i];
	    if (drv->dev_idx >= 0) {
		id = drv->dev_idx;
		make_global_index(i, &id);
		break;
	    } else if (id==PJMEDIA_AUD_DEFAULT_CAPTURE_DEV && 
		drv->rec_dev_idx >= 0) 
	    {
		id = drv->rec_dev_idx;
		make_global_index(i, &id);
		break;
	    } else if (id==PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV && 
		drv->play_dev_idx >= 0) 
	    {
		id = drv->play_dev_idx;
		make_global_index(i, &id);
		break;
	    }
	}

	if (id < 0) {
	    return PJMEDIA_EAUD_NODEFDEV;
	}
    }

    f_id = GET_FID(aud_subsys.dev_list[id]);
    index = GET_INDEX(aud_subsys.dev_list[id]);

    if (f_id < 0 || f_id >= (int)aud_subsys.drv_cnt)
	return PJMEDIA_EAUD_INVDEV;

    if (index < 0 || index >= (int)aud_subsys.drv[f_id].dev_cnt)
	return PJMEDIA_EAUD_INVDEV;

    *p_f = aud_subsys.drv[f_id].f;
    *p_local_index = (unsigned)index;

    return PJ_SUCCESS;

}

/* API: Get device information. */
PJ_DEF(pj_status_t) pjmedia_aud_dev_get_info(pjmedia_aud_dev_index id,
					     pjmedia_aud_dev_info *info)
{
    pjmedia_aud_dev_factory *f;
    unsigned index;
    pj_status_t status;

	PJ_ASSERT_RETURN(info && id!=PJMEDIA_AUD_INVALID_DEV, PJ_EINVAL);
	//PJ_ASSERT_RETURN(aud_subsys.pf, PJMEDIA_EAUD_INIT);
	// DEAN don't assertion
	if (!aud_subsys.pf)
		return PJMEDIA_EAUD_INIT;

    status = lookup_dev(id, &f, &index);
    if (status != PJ_SUCCESS)
	return status;

    return f->op->get_dev_info(f, index, info);
}

/* API: find device */
PJ_DEF(pj_status_t) pjmedia_aud_dev_lookup( const char *drv_name,
					    const char *dev_name,
					    pjmedia_aud_dev_index *id)
{
    pjmedia_aud_dev_factory *f = NULL;
    unsigned drv_idx, dev_idx;

	PJ_ASSERT_RETURN(drv_name && dev_name && id, PJ_EINVAL);
	//PJ_ASSERT_RETURN(aud_subsys.pf, PJMEDIA_EAUD_INIT);
	// DEAN don't assertion
	if (!aud_subsys.pf)
		return PJMEDIA_EAUD_INIT;

    for (drv_idx=0; drv_idx<aud_subsys.drv_cnt; ++drv_idx) {
	if (!pj_ansi_stricmp(drv_name, aud_subsys.drv[drv_idx].name)) {
	    f = aud_subsys.drv[drv_idx].f;
	    break;
	}
    }

    if (!f)
	{
		PJ_LOG(4, (THIS_FILE, "pjmedia_aud_dev_lookup() pjmedia_aud_dev_factory not found."));
		return PJ_ENOTFOUND;
	}

    for (dev_idx=0; dev_idx<aud_subsys.drv[drv_idx].dev_cnt; ++dev_idx) {
	pjmedia_aud_dev_info info;
	pj_status_t status;

	status = f->op->get_dev_info(f, dev_idx, &info);
	if (status != PJ_SUCCESS)
	    return status;

	if (!pj_ansi_stricmp(dev_name, info.name))
	    break;
    }

	if (dev_idx==aud_subsys.drv[drv_idx].dev_cnt)
	{
		PJ_LOG(4, (THIS_FILE, "pjmedia_aud_dev_lookup() aud_subsys.drv not found."));
		return PJ_ENOTFOUND;
	}

    *id = dev_idx;
    make_global_index(drv_idx, id);

    return PJ_SUCCESS;
}

/* API: Initialize the audio device parameters with default values for the
 * specified device.
 */
PJ_DEF(pj_status_t) pjmedia_aud_dev_default_param(pjmedia_aud_dev_index id,
						  pjmedia_aud_param *param)
{
    pjmedia_aud_dev_factory *f;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(param && id!=PJMEDIA_AUD_INVALID_DEV, PJ_EINVAL);
    //PJ_ASSERT_RETURN(aud_subsys.pf, PJMEDIA_EAUD_INIT);
	// DEAN don't assertion
	if (!aud_subsys.pf)
		return PJMEDIA_EAUD_INIT;

    status = lookup_dev(id, &f, &index);
    if (status != PJ_SUCCESS)
	return status;

    status = f->op->default_param(f, index, param);
    if (status != PJ_SUCCESS)
	return status;

    /* Normalize device IDs */
    make_global_index(f->sys.drv_idx, &param->rec_id);
    make_global_index(f->sys.drv_idx, &param->play_id);

    return PJ_SUCCESS;
}

/* API: Open audio stream object using the specified parameters. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_create(const pjmedia_aud_param *prm,
					      pjmedia_aud_rec_cb rec_cb,
					      pjmedia_aud_play_cb play_cb,
					      void *user_data,
					      pjmedia_aud_stream **p_aud_strm)
{
    pjmedia_aud_dev_factory *rec_f=NULL, *play_f=NULL, *f=NULL;
    pjmedia_aud_param param;
    pj_status_t status;

	PJ_ASSERT_RETURN(prm && prm->dir && p_aud_strm, PJ_EINVAL);
	//PJ_ASSERT_RETURN(aud_subsys.pf, PJMEDIA_EAUD_INIT);
	// DEAN don't assertion
	if (!aud_subsys.pf)
		return PJMEDIA_EAUD_INIT;
    PJ_ASSERT_RETURN(prm->dir==PJMEDIA_DIR_CAPTURE ||
		     prm->dir==PJMEDIA_DIR_PLAYBACK ||
		     prm->dir==PJMEDIA_DIR_CAPTURE_PLAYBACK,
		     PJ_EINVAL);

    /* Must make copy of param because we're changing device ID */
    pj_memcpy(&param, prm, sizeof(param));

    /* Normalize rec_id */
    if (param.dir & PJMEDIA_DIR_CAPTURE) {
	unsigned index;

	if (param.rec_id < 0)
	    param.rec_id = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;

	status = lookup_dev(param.rec_id, &rec_f, &index);
	if (status != PJ_SUCCESS)
	    return status;

	param.rec_id = index;
	f = rec_f;
    }

    /* Normalize play_id */
    if (param.dir & PJMEDIA_DIR_PLAYBACK) {
	unsigned index;

	if (param.play_id < 0)
	    param.play_id = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

	status = lookup_dev(param.play_id, &play_f, &index);
	if (status != PJ_SUCCESS)
	    return status;

	param.play_id = index;
	f = play_f;
    }

    PJ_ASSERT_RETURN(f != NULL, PJ_EBUG);

    /* For now, rec_id and play_id must belong to the same factory */
    PJ_ASSERT_RETURN((param.dir != PJMEDIA_DIR_CAPTURE_PLAYBACK) || 
		     (rec_f == play_f),
		     PJMEDIA_EAUD_INVDEV);

    /* Create the stream */
    status = f->op->create_stream(f, &param, rec_cb, play_cb,
				  user_data, p_aud_strm);
    if (status != PJ_SUCCESS)
	return status;

    /* Assign factory id to the stream */
    (*p_aud_strm)->sys.drv_idx = f->sys.drv_idx;
    return PJ_SUCCESS;
}

/* API: Get the running parameters for the specified audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_get_param(pjmedia_aud_stream *strm,
						 pjmedia_aud_param *param)
{
    pj_status_t status;

	PJ_ASSERT_RETURN(strm && param, PJ_EINVAL);
	//PJ_ASSERT_RETURN(aud_subsys.pf, PJMEDIA_EAUD_INIT);
	// DEAN don't assertion
	if (!aud_subsys.pf)
		return PJMEDIA_EAUD_INIT;

    status = strm->op->get_param(strm, param);
    if (status != PJ_SUCCESS)
	return status;

    /* Normalize device id's */
    make_global_index(strm->sys.drv_idx, &param->rec_id);
    make_global_index(strm->sys.drv_idx, &param->play_id);

    return PJ_SUCCESS;
}

/* API: Get the value of a specific capability of the audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_get_cap(pjmedia_aud_stream *strm,
					       pjmedia_aud_dev_cap cap,
					       void *value)
{
    return strm->op->get_cap(strm, cap, value);
}

/* API: Set the value of a specific capability of the audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_set_cap(pjmedia_aud_stream *strm,
					       pjmedia_aud_dev_cap cap,
					       const void *value)
{
    return strm->op->set_cap(strm, cap, value);
}

/* API: Start the stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_start(pjmedia_aud_stream *strm)
{
    return strm->op->start(strm);
}

/* API: Stop the stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_stop(pjmedia_aud_stream *strm)
{
    return strm->op->stop(strm);
}

/* API: Destroy the stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_destroy(pjmedia_aud_stream *strm)
{
    return strm->op->destroy(strm);
}


