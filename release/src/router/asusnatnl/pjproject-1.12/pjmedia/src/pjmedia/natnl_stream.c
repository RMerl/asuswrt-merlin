#include <pjmedia/natnl_stream.h>

#define THIS_FILE "natnl_stream.c"

PJ_DEF(pj_status_t) pjmedia_natnl_stream_create(pj_pool_t *pool,
                                        pjsua_call *call,
                                        pjmedia_stream_info *si,
                                        natnl_stream **stream)
{
    pj_status_t status = PJ_SUCCESS;
    unsigned strm_idx = 0;
    strm_idx = call->index;

        /* TODO:
         *   - Create and start your media stream based on the parameters
         *     in si
         */

    PJ_ASSERT_RETURN(pool, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE,"natnl audio channel update..strm_idx=%d", strm_idx));

    /* Check if no media is active */
    if (si->dir != PJMEDIA_DIR_NONE) {
        /* Create session based on session info. */
#if 0
        pool = pj_pool_create(strm_pool->factory, "strm%p", 
                              NATNL_STREAM_SIZE, NATNL_STREAM_INC, NULL);
        PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);
#endif

        pj_mutex_lock(call->tnl_stream_lock);
		pj_mutex_lock(call->tnl_stream_lock2);
		pj_mutex_lock(call->tnl_stream_lock3);
		pj_mutex_lock(call->tnl_stream_lock4);
        // DEAN don't re-create natnl stream
        if (call->tnl_stream) {
			*stream = call->tnl_stream;
			pj_mutex_unlock(call->tnl_stream_lock4);
			pj_mutex_unlock(call->tnl_stream_lock3);
            pj_mutex_unlock(call->tnl_stream_lock2);
            pj_mutex_unlock(call->tnl_stream_lock);
            return PJ_SUCCESS;
		}

        call->tnl_stream = *stream = PJ_POOL_ZALLOC_T(pool, natnl_stream);
        PJ_ASSERT_RETURN(*stream != NULL, PJ_ENOMEM);
        (*stream)->call = call;
        (*stream)->own_pool = pool;
		(*stream)->med_tp = call->med_tp;
		pj_memcpy(&(*stream)->rem_addr, &si->rem_addr, sizeof(pj_sockaddr));
		pj_list_init(&(*stream)->rbuff);
		pj_list_init(&(*stream)->no_ctl_rbuff);
		pj_list_init(&(*stream)->gcbuff);
		pj_get_timestamp(&(*stream)->last_data_or_ka);
		pj_get_timestamp(&(*stream)->last_data);
		(*stream)->rbuff_cnt = 0;
		(*stream)->no_ctl_rbuff_cnt = 0;

		(*stream)->rx_band = (pj_band_t *)malloc(sizeof(pj_band_t));
		(*stream)->tx_band = (pj_band_t *)malloc(sizeof(pj_band_t));
		pj_memset((*stream)->rx_band, 0, sizeof(pj_band_t));
		pj_memset((*stream)->tx_band, 0, sizeof(pj_band_t));
		pj_bandwidthSetLimited((*stream)->rx_band, PJ_FALSE);
		pj_bandwidthSetLimited((*stream)->tx_band, PJ_FALSE);

        /* Create mutex to protect jitter buffer: */
        status = pj_mutex_create_simple(pool, NULL, &(*stream)->rbuff_mutex);
        if (status != PJ_SUCCESS) {
            //pj_pool_t *tmp_pool = (*stream)->own_pool;
            (*stream)->own_pool = NULL;
            //pj_pool_release(tmp_pool);
            goto on_return;
		}

		/* Create mutex to protect jitter buffer: */
		status = pj_mutex_create_simple(pool, NULL, &(*stream)->no_ctl_rbuff_mutex);
		if (status != PJ_SUCCESS) {
			//pj_pool_t *tmp_pool = (*stream)->own_pool;
			(*stream)->own_pool = NULL;
			//pj_pool_release(tmp_pool);
			goto on_return;
		}

        status = pj_mutex_create_simple(pool, NULL, &(*stream)->gcbuff_mutex);
        if (status != PJ_SUCCESS) {
            (*stream)->own_pool = NULL;
            goto on_return;
        }

        /* Create semaphore */
        status = pj_sem_create(pool, "client", 0, 65535, &(*stream)->rbuff_sem);
        if (status != PJ_SUCCESS) {
            (*stream)->own_pool = NULL;
            goto on_return;
        }

		/* Create semaphore */
		status = pj_sem_create(pool, "client2", 0, 65535, &(*stream)->no_ctl_rbuff_sem);
		if (status != PJ_SUCCESS) {
			(*stream)->own_pool = NULL;
			goto on_return;
		}

        // +Roger - Create Send buffer Mutex
        status = pj_mutex_create_simple(pool, NULL, &(*stream)->sbuff_mutex);
        if (status != PJ_SUCCESS) {
            //pj_pool_t *tmp_pool = (*stream)->own_pool;
            (*stream)->own_pool = NULL;
            //pj_pool_release(tmp_pool);
            goto on_return;
        }
        //------------------------------------//

#if 0
        /* Attach our RTP and RTCP callbacks to the media transport */
        status = pjmedia_transport_attach(call_med->tp, stream, //call_med,
                                          &si->rem_addr, &si->rem_rtcp,
                                          pj_sockaddr_get_len(&si->rem_addr),
                                          &aud_rtp_cb, &aud_rtcp_cb);
#endif

    }

on_return:
	pj_mutex_unlock(call->tnl_stream_lock4);
	pj_mutex_unlock(call->tnl_stream_lock3);
    pj_mutex_unlock(call->tnl_stream_lock2);
    pj_mutex_unlock(call->tnl_stream_lock);

    return status;
}

/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_natnl_stream_destroy( natnl_stream *stream )
{
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    /**
     * don't need to destroy mutex here, 
     * after stream destroy, the mutex will also be freed 
     */
#if 0

    /* Free mutex */
    if (stream->gcbuff_mutex) {
        pj_mutex_destroy(stream->gcbuff_mutex);
        stream->gcbuff_mutex = NULL;
	}

	if (stream->rbuff_mutex) {
		pj_mutex_destroy(stream->rbuff_mutex);
		stream->rbuff_mutex = NULL;
	}

	if (stream->no_ctl_rbuff_mutex) {
		pj_mutex_destroy(stream->no_ctl_rbuff_mutex);
		stream->no_ctl_rbuff_mutex = NULL;
	}

    if (stream->sbuff_mutex) {
        pj_mutex_destroy(stream->sbuff_mutex);
        stream->sbuff_mutex = NULL;
    }
#endif
#if 1 // DEAN don't release pool hear, let pjsip to release it.
    if (stream->own_pool) {
        //pj_pool_release(stream->own_pool);
        stream->own_pool = NULL;
    }
#endif

	if (stream->rx_band) {
		free(stream->rx_band);
		stream->rx_band = NULL;
	}

	if (stream->tx_band) {
		free(stream->tx_band);
		stream->tx_band = NULL;
	}

    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pjmedia_natnl_disabled_flow_control(const void *data, int data_len)
{
	pj_uint8_t *pkt = (pj_uint8_t *)data;
	return (data_len >= NO_FLOW_CTL_TOTAL_HEADER_SIZE && 
		((pj_uint32_t*)pkt)[0] == NO_FLOW_CTL_SESS_MGR_HEADER_MAGIC &&
		pkt[NO_FLOW_CTL_SESS_MSG_DISABLE_FLOW_CTL_INDEX] == 1);
}

PJ_DEF(pj_bool_t) pjmedia_natnl_sess_mgr_packet_is_tnl_data(const void *data, int data_len)
{
	pj_uint8_t *pkt = (pj_uint8_t *)data;
	return (data_len >= SESS_MGR_HEADER_SIZE && 
		(pkt[SESS_MGR_MSG_TYPE_INDEX] == 5 || pkt[SESS_MGR_MSG_TYPE_INDEX] == 6));
}

PJ_DEF(pj_bool_t) pjmedia_natnl_no_flow_ctl_packet_is_tnl_data(const void *data, int data_len)
{
	pj_uint8_t *pkt = (pj_uint8_t *)data;
	return (data_len >= NO_FLOW_CTL_TOTAL_HEADER_SIZE && 
		(pkt[NO_FLOW_CTL_SESS_MSG_TYPE_INDEX] == 5 || pkt[NO_FLOW_CTL_SESS_MSG_TYPE_INDEX] == 6));
}

PJ_DEF(pj_bool_t) pjmedia_natnl_sctp_packet_is_tnl_data(const void *data, int data_len)
{
	pj_uint8_t *pkt = (pj_uint8_t *)data;
	return (data_len >= SCTP_DATA_CHUNK_TOTAL_HEADER_SIZE && 
		pkt[SCTP_DATA_CHUNK_TYPE_INDEX] == 0 && 
		(pkt[SCTP_SESS_MSG_TYPE_INDEX] == 5 || pkt[SCTP_SESS_MSG_TYPE_INDEX] == 6));
}

PJ_DEF(pj_bool_t) pjmedia_natnl_udt_packet_is_tnl_data(const void *data, int data_len)
{
	pj_uint8_t *pkt = (pj_uint8_t *)data;
	return (data_len >= UDT_DATA_CHUNK_TOTAL_HEADER_SIZE && 
		((pj_uint16_t *)pkt)[UDT_DATA_CHUNK_TYPE_INDEX] == 0x00FF &&  // In https://tools.ietf.org/html/draft-gg-udt-00, the first bit should be 0 for data chunk. But in asusnatnl we add 0xFF00 to distinguish with stun packet.
		(pkt[UDT_SESS_MSG_TYPE_INDEX] == 5 || pkt[UDT_SESS_MSG_TYPE_INDEX] == 6));
}