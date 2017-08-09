#include "rpc.h"
#include "rpcndr.h"

void dump_data(const unsigned char *buf1,int len);

#if _WIN32_WINNT < 0x600
#define NdrSendReceive NdrSendReceiveMarshall
void NdrSendReceiveMarshall(PMIDL_STUB_MESSAGE stubmsg, unsigned char *buffer);
#define NdrGetBuffer NdrGetBufferMarshall
void NdrGetBufferMarshall(PMIDL_STUB_MESSAGE stubmsg, unsigned long len, RPC_BINDING_HANDLE hnd);
#define NdrServerInitializeNew NdrServerInitializeNewMarshall
void NdrServerInitializeNewMarshall(PRPC_MESSAGE pRpcMsg,
				    PMIDL_STUB_MESSAGE pStubMsg,
				    PMIDL_STUB_DESC pStubDesc);
#define I_RpcGetBuffer I_RpcGetBufferMarshall
RPC_STATUS WINAPI I_RpcGetBufferMarshall(PRPC_MESSAGE pMsg);

#endif /* _WIN32_WINNT < 0x600 */

