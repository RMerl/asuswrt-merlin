#include "../librpc/gen_ndr/ndr_initshutdown.h"
#ifndef __CLI_INITSHUTDOWN__
#define __CLI_INITSHUTDOWN__
struct tevent_req *rpccli_initshutdown_Init_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct rpc_pipe_client *cli,
						 uint16_t *_hostname /* [in] [unique] */,
						 struct lsa_StringLarge *_message /* [in] [unique] */,
						 uint32_t _timeout /* [in]  */,
						 uint8_t _force_apps /* [in]  */,
						 uint8_t _do_reboot /* [in]  */);
NTSTATUS rpccli_initshutdown_Init_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS rpccli_initshutdown_Init(struct rpc_pipe_client *cli,
				  TALLOC_CTX *mem_ctx,
				  uint16_t *hostname /* [in] [unique] */,
				  struct lsa_StringLarge *message /* [in] [unique] */,
				  uint32_t timeout /* [in]  */,
				  uint8_t force_apps /* [in]  */,
				  uint8_t do_reboot /* [in]  */,
				  WERROR *werror);
struct tevent_req *rpccli_initshutdown_Abort_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct rpc_pipe_client *cli,
						  uint16_t *_server /* [in] [unique] */);
NTSTATUS rpccli_initshutdown_Abort_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS rpccli_initshutdown_Abort(struct rpc_pipe_client *cli,
				   TALLOC_CTX *mem_ctx,
				   uint16_t *server /* [in] [unique] */,
				   WERROR *werror);
struct tevent_req *rpccli_initshutdown_InitEx_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct rpc_pipe_client *cli,
						   uint16_t *_hostname /* [in] [unique] */,
						   struct lsa_StringLarge *_message /* [in] [unique] */,
						   uint32_t _timeout /* [in]  */,
						   uint8_t _force_apps /* [in]  */,
						   uint8_t _do_reboot /* [in]  */,
						   uint32_t _reason /* [in]  */);
NTSTATUS rpccli_initshutdown_InitEx_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS rpccli_initshutdown_InitEx(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx,
				    uint16_t *hostname /* [in] [unique] */,
				    struct lsa_StringLarge *message /* [in] [unique] */,
				    uint32_t timeout /* [in]  */,
				    uint8_t force_apps /* [in]  */,
				    uint8_t do_reboot /* [in]  */,
				    uint32_t reason /* [in]  */,
				    WERROR *werror);
#endif /* __CLI_INITSHUTDOWN__ */
