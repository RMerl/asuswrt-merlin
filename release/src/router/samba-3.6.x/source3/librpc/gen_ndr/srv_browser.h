#include "librpc/gen_ndr/ndr_browser.h"
#ifndef __SRV_BROWSER__
#define __SRV_BROWSER__
void _BrowserrServerEnum(struct pipes_struct *p, struct BrowserrServerEnum *r);
void _BrowserrDebugCall(struct pipes_struct *p, struct BrowserrDebugCall *r);
WERROR _BrowserrQueryOtherDomains(struct pipes_struct *p, struct BrowserrQueryOtherDomains *r);
void _BrowserrResetNetlogonState(struct pipes_struct *p, struct BrowserrResetNetlogonState *r);
void _BrowserrDebugTrace(struct pipes_struct *p, struct BrowserrDebugTrace *r);
void _BrowserrQueryStatistics(struct pipes_struct *p, struct BrowserrQueryStatistics *r);
void _BrowserResetStatistics(struct pipes_struct *p, struct BrowserResetStatistics *r);
void _NetrBrowserStatisticsClear(struct pipes_struct *p, struct NetrBrowserStatisticsClear *r);
void _NetrBrowserStatisticsGet(struct pipes_struct *p, struct NetrBrowserStatisticsGet *r);
void _BrowserrSetNetlogonState(struct pipes_struct *p, struct BrowserrSetNetlogonState *r);
void _BrowserrQueryEmulatedDomains(struct pipes_struct *p, struct BrowserrQueryEmulatedDomains *r);
void _BrowserrServerEnumEx(struct pipes_struct *p, struct BrowserrServerEnumEx *r);
void browser_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_browser_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_browser_shutdown(void);
#endif /* __SRV_BROWSER__ */
