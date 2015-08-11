#include <pjnath/natnl_tnl_cache.h>


// global natnl_tnl_cache list
static natnl_tnl_cache tnl_cache = {0};
static pj_mutex_t *cache_lock = NULL;


/* Compare module name, used for searching module based on name. */
static int cmp_device_id(void *device_id, const void *cache)
{
	return pj_stricmp((const pj_str_t*)device_id, &((natnl_tnl_cache*)cache)->device_id);
}

PJ_DEF(void) natnl_tnl_cache_init(pj_pool_t *pool)
{
	pj_status_t status;
	if(!cache_lock)
	{
		status = pj_mutex_create_simple(pool, NULL, &cache_lock);
		pj_list_init(&tnl_cache);
	}
}

PJ_DEF(void) natnl_tnl_cache_deinit()
{
	natnl_tnl_cache *cache;
	pj_mutex_lock(cache_lock);

	cache = tnl_cache.next;
	while (cache != &tnl_cache) 
	{
		natnl_tnl_cache *pre_cache;

		free(cache->check->lcand);
		cache->check->lcand = NULL;

		free(cache->check->rcand);
		cache->check->rcand = NULL;

		free(cache->check);
		cache->check = NULL;

		free(cache->device_id.ptr);
		cache->device_id.ptr = NULL;

		cache = cache->next;

		pre_cache = cache->prev;
		if (pre_cache != &tnl_cache)
		{
			pj_list_erase(cache);
			free(pre_cache);
			pre_cache = NULL;
		}

	}
	pj_mutex_unlock(cache_lock);

	pj_mutex_destroy(cache_lock);
	cache_lock = NULL;
}

PJ_DEF(void) natnl_save_to_cache(pj_str_t *device_id, pj_ice_sess_check *check)
{
	natnl_tnl_cache *temp_cache;
	pj_mutex_lock(cache_lock);

	temp_cache = natnl_get_from_cache(device_id);

	if (temp_cache) 
	{
		pj_list_erase(temp_cache);

		free(temp_cache->check->lcand);
		temp_cache->check->lcand = NULL;

		free(temp_cache->check->rcand);
		temp_cache->check->rcand = NULL;

		free(temp_cache->check);
		temp_cache->check = NULL;

		free(temp_cache->device_id.ptr);
		temp_cache->device_id.ptr = NULL;

		free(temp_cache);
		temp_cache = NULL;
	}

	temp_cache = (natnl_tnl_cache *)malloc(sizeof(natnl_tnl_cache));//PJ_POOL_ZALLOC_T(pool, natnl_tnl_cache);
	//temp_cache->device_id = device_id;
	
	temp_cache->device_id.ptr = (char *)malloc(device_id->slen);//pj_pool_zalloc(pool, device_id->slen);
	pj_strncpy(&temp_cache->device_id, device_id, device_id->slen);
	//temp_cache->check = check;
	
	temp_cache->check = (pj_ice_sess_check *)malloc(sizeof(pj_ice_sess_check));//PJ_POOL_ZALLOC_T(pool, pj_ice_sess_check);
	pj_memcpy(temp_cache->check, check, sizeof(pj_ice_sess_check));

	temp_cache->check->lcand = (pj_ice_sess_cand *)malloc(sizeof(pj_ice_sess_cand));
	pj_memcpy(temp_cache->check->lcand, check->lcand, sizeof(pj_ice_sess_cand));

	temp_cache->check->rcand = (pj_ice_sess_cand *)malloc(sizeof(pj_ice_sess_cand));
	pj_memcpy(temp_cache->check->rcand, check->rcand, sizeof(pj_ice_sess_cand));
	
	pj_list_push_back(&tnl_cache, temp_cache);

	pj_mutex_unlock(cache_lock);

}

PJ_DEF(natnl_tnl_cache *) natnl_get_from_cache(pj_str_t *device_id)
{
	natnl_tnl_cache *temp_cache = (natnl_tnl_cache *)pj_list_search(&tnl_cache, device_id, &cmp_device_id);
	return temp_cache;
}
