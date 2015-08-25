
#include <pj/list.h>
#include <pj/pool.h>
#include <pjnath/ice_session.h>

PJ_BEGIN_DECL

typedef struct natnl_tnl_cache
{
	PJ_DECL_LIST_MEMBER(struct natnl_tnl_cache);
	pj_str_t            device_id;
	pj_ice_sess_check   *check;
	//pj_sockaddr		    addr;
	//natnl_tunnel_type   tunnel_type;

} natnl_tnl_cache;

PJ_DECL(void) natnl_tnl_cache_init(pj_pool_t *pool);
PJ_DECL(void) natnl_tnl_cache_deinit();
PJ_DECL(void) natnl_save_to_cache(pj_str_t *device_id, pj_ice_sess_check *check);
PJ_DECL(natnl_tnl_cache *) natnl_get_from_cache(pj_str_t *device_id);

PJ_END_DECL