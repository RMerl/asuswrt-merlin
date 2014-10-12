#include "librpc/gen_ndr/orpc.h"
#include "librpc/gen_ndr/ndr_dcom.h"

struct IUnknown;
struct IClassFactory;
struct IRemUnknown;
struct IClassActivator;
struct ISCMLocalActivator;
struct ISystemActivator;
struct IRemUnknown2;
struct IDispatch;
struct IMarshal;
struct ICoffeeMachine;
struct IStream;

#ifndef _IUnknown_
#define _IUnknown_


/* IUnknown */
#define COM_IUNKNOWN_UUID "00000000-0000-0000-C000-000000000046"

struct IUnknown_vtable;

struct IUnknown {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IUnknown_vtable *vtable;
	void *object_data;
};

#define IUNKNOWN_METHODS \
	WERROR (*QueryInterface) (struct IUnknown *d, TALLOC_CTX *mem_ctx, struct GUID *iid, struct IUnknown **data);\
	uint32_t (*AddRef) (struct IUnknown *d, TALLOC_CTX *mem_ctx);\
	uint32_t (*Release) (struct IUnknown *d, TALLOC_CTX *mem_ctx);\

struct IUnknown_vtable {
	struct GUID iid;
	IUNKNOWN_METHODS
};

#define IUnknown_QueryInterface(interface, mem_ctx, iid, data) ((interface)->vtable->QueryInterface(interface, mem_ctx, iid, data))
#define IUnknown_AddRef(interface, mem_ctx) ((interface)->vtable->AddRef(interface, mem_ctx))
#define IUnknown_Release(interface, mem_ctx) ((interface)->vtable->Release(interface, mem_ctx))
#endif

#ifndef _IClassFactory_
#define _IClassFactory_


/* IClassFactory */
#define COM_ICLASSFACTORY_UUID "00000001-0000-0000-C000-000000000046"

struct IClassFactory_vtable;

struct IClassFactory {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IClassFactory_vtable *vtable;
	void *object_data;
};

#define ICLASSFACTORY_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*CreateInstance) (struct IClassFactory *d, TALLOC_CTX *mem_ctx, struct MInterfacePointer *pUnknown, struct GUID *iid, struct MInterfacePointer *ppv);\
	WERROR (*RemoteCreateInstance) (struct IClassFactory *d, TALLOC_CTX *mem_ctx);\
	WERROR (*LockServer) (struct IClassFactory *d, TALLOC_CTX *mem_ctx, uint8_t lock);\
	WERROR (*RemoteLockServer) (struct IClassFactory *d, TALLOC_CTX *mem_ctx);\

struct IClassFactory_vtable {
	struct GUID iid;
	ICLASSFACTORY_METHODS
};

#define IClassFactory_CreateInstance(interface, mem_ctx, pUnknown, iid, ppv) ((interface)->vtable->CreateInstance(interface, mem_ctx, pUnknown, iid, ppv))
#define IClassFactory_RemoteCreateInstance(interface, mem_ctx) ((interface)->vtable->RemoteCreateInstance(interface, mem_ctx))
#define IClassFactory_LockServer(interface, mem_ctx, lock) ((interface)->vtable->LockServer(interface, mem_ctx, lock))
#define IClassFactory_RemoteLockServer(interface, mem_ctx) ((interface)->vtable->RemoteLockServer(interface, mem_ctx))
#endif

#ifndef _IRemUnknown_
#define _IRemUnknown_


/* IRemUnknown */
#define COM_IREMUNKNOWN_UUID "00000131-0000-0000-C000-000000000046"

struct IRemUnknown_vtable;

struct IRemUnknown {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IRemUnknown_vtable *vtable;
	void *object_data;
};

#define IREMUNKNOWN_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*RemQueryInterface) (struct IRemUnknown *d, TALLOC_CTX *mem_ctx, struct GUID *ripid, uint32_t cRefs, uint16_t cIids, struct GUID *iids, struct MInterfacePointer *ip);\
	WERROR (*RemAddRef) (struct IRemUnknown *d, TALLOC_CTX *mem_ctx, uint16_t cInterfaceRefs, struct REMINTERFACEREF *InterfaceRefs, WERROR *pResults);\
	WERROR (*RemRelease) (struct IRemUnknown *d, TALLOC_CTX *mem_ctx, uint16_t cInterfaceRefs, struct REMINTERFACEREF *InterfaceRefs);\

struct IRemUnknown_vtable {
	struct GUID iid;
	IREMUNKNOWN_METHODS
};

#define IRemUnknown_RemQueryInterface(interface, mem_ctx, ripid, cRefs, cIids, iids, ip) ((interface)->vtable->RemQueryInterface(interface, mem_ctx, ripid, cRefs, cIids, iids, ip))
#define IRemUnknown_RemAddRef(interface, mem_ctx, cInterfaceRefs, InterfaceRefs, pResults) ((interface)->vtable->RemAddRef(interface, mem_ctx, cInterfaceRefs, InterfaceRefs, pResults))
#define IRemUnknown_RemRelease(interface, mem_ctx, cInterfaceRefs, InterfaceRefs) ((interface)->vtable->RemRelease(interface, mem_ctx, cInterfaceRefs, InterfaceRefs))
#endif

#ifndef _IClassActivator_
#define _IClassActivator_


/* IClassActivator */
#define COM_ICLASSACTIVATOR_UUID "00000140-0000-0000-c000-000000000046"

struct IClassActivator_vtable;

struct IClassActivator {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IClassActivator_vtable *vtable;
	void *object_data;
};

#define ICLASSACTIVATOR_METHODS \
	IUNKNOWN_METHODS\
	void (*GetClassObject) (struct IClassActivator *d, TALLOC_CTX *mem_ctx, struct GUID clsid, uint32_t context, uint32_t locale, struct GUID iid, struct MInterfacePointer *data);\

struct IClassActivator_vtable {
	struct GUID iid;
	ICLASSACTIVATOR_METHODS
};

#define IClassActivator_GetClassObject(interface, mem_ctx, clsid, context, locale, iid, data) ((interface)->vtable->GetClassObject(interface, mem_ctx, clsid, context, locale, iid, data))
#endif

#ifndef _ISCMLocalActivator_
#define _ISCMLocalActivator_


/* ISCMLocalActivator */
#define COM_ISCMLOCALACTIVATOR_UUID "00000136-0000-0000-c000-000000000046"

struct ISCMLocalActivator_vtable;

struct ISCMLocalActivator {
	struct OBJREF obj;
	struct com_context *ctx;
	struct ISCMLocalActivator_vtable *vtable;
	void *object_data;
};

#define ISCMLOCALACTIVATOR_METHODS \
	ICLASSACTIVATOR_METHODS\
	WERROR (*ISCMLocalActivator_CreateInstance) (struct ISCMLocalActivator *d, TALLOC_CTX *mem_ctx);\

struct ISCMLocalActivator_vtable {
	struct GUID iid;
	ISCMLOCALACTIVATOR_METHODS
};

#define ISCMLocalActivator_ISCMLocalActivator_CreateInstance(interface, mem_ctx) ((interface)->vtable->ISCMLocalActivator_CreateInstance(interface, mem_ctx))
#endif

#ifndef _ISystemActivator_
#define _ISystemActivator_


/* ISystemActivator */
#define COM_ISYSTEMACTIVATOR_UUID "000001a0-0000-0000-c000-000000000046"

struct ISystemActivator_vtable;

struct ISystemActivator {
	struct OBJREF obj;
	struct com_context *ctx;
	struct ISystemActivator_vtable *vtable;
	void *object_data;
};

#define ISYSTEMACTIVATOR_METHODS \
	ICLASSACTIVATOR_METHODS\
	WERROR (*ISystemActivatorRemoteCreateInstance) (struct ISystemActivator *d, TALLOC_CTX *mem_ctx, uint64_t unknown1, struct MInterfacePointer iface1, uint64_t unknown2, uint32_t *unknown3, struct MInterfacePointer *iface2);\

struct ISystemActivator_vtable {
	struct GUID iid;
	ISYSTEMACTIVATOR_METHODS
};

#define ISystemActivator_ISystemActivatorRemoteCreateInstance(interface, mem_ctx, unknown1, iface1, unknown2, unknown3, iface2) ((interface)->vtable->ISystemActivatorRemoteCreateInstance(interface, mem_ctx, unknown1, iface1, unknown2, unknown3, iface2))
#endif

#ifndef _IRemUnknown2_
#define _IRemUnknown2_


/* IRemUnknown2 */
#define COM_IREMUNKNOWN2_UUID "00000143-0000-0000-C000-000000000046"

struct IRemUnknown2_vtable;

struct IRemUnknown2 {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IRemUnknown2_vtable *vtable;
	void *object_data;
};

#define IREMUNKNOWN2_METHODS \
	IREMUNKNOWN_METHODS\
	WERROR (*RemQueryInterface2) (struct IRemUnknown2 *d, TALLOC_CTX *mem_ctx, struct GUID *ripid, uint16_t cIids, struct GUID *iids, WERROR *phr, struct MInterfacePointer *ppMIF);\

struct IRemUnknown2_vtable {
	struct GUID iid;
	IREMUNKNOWN2_METHODS
};

#define IRemUnknown2_RemQueryInterface2(interface, mem_ctx, ripid, cIids, iids, phr, ppMIF) ((interface)->vtable->RemQueryInterface2(interface, mem_ctx, ripid, cIids, iids, phr, ppMIF))
#endif

#ifndef _IDispatch_
#define _IDispatch_


/* IDispatch */
#define COM_IDISPATCH_UUID "00020400-0000-0000-C000-000000000046"

struct IDispatch_vtable;

struct IDispatch {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IDispatch_vtable *vtable;
	void *object_data;
};

#define IDISPATCH_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*GetTypeInfoCount) (struct IDispatch *d, TALLOC_CTX *mem_ctx, uint16_t *pctinfo);\
	WERROR (*GetTypeInfo) (struct IDispatch *d, TALLOC_CTX *mem_ctx, uint16_t iTInfo, uint32_t lcid, struct REF_ITypeInfo *ppTInfo);\
	WERROR (*GetIDsOfNames) (struct IDispatch *d, TALLOC_CTX *mem_ctx, struct GUID *riid, uint16_t cNames, uint32_t lcid, uint32_t *rgDispId);\
	WERROR (*Invoke) (struct IDispatch *d, TALLOC_CTX *mem_ctx, uint32_t dispIdMember, struct GUID *riid, uint32_t lcid, uint16_t wFlags, struct DISPPARAMS *pDispParams, struct VARIANT *pVarResult, struct EXCEPINFO *pExcepInfo, uint16_t *puArgErr);\

struct IDispatch_vtable {
	struct GUID iid;
	IDISPATCH_METHODS
};

#define IDispatch_GetTypeInfoCount(interface, mem_ctx, pctinfo) ((interface)->vtable->GetTypeInfoCount(interface, mem_ctx, pctinfo))
#define IDispatch_GetTypeInfo(interface, mem_ctx, iTInfo, lcid, ppTInfo) ((interface)->vtable->GetTypeInfo(interface, mem_ctx, iTInfo, lcid, ppTInfo))
#define IDispatch_GetIDsOfNames(interface, mem_ctx, riid, cNames, lcid, rgDispId) ((interface)->vtable->GetIDsOfNames(interface, mem_ctx, riid, cNames, lcid, rgDispId))
#define IDispatch_Invoke(interface, mem_ctx, dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr) ((interface)->vtable->Invoke(interface, mem_ctx, dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr))
#endif

#ifndef _IMarshal_
#define _IMarshal_


/* IMarshal */
#define COM_IMARSHAL_UUID "00000003-0000-0000-C000-000000000046"

struct IMarshal_vtable;

struct IMarshal {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IMarshal_vtable *vtable;
	void *object_data;
};

#define IMARSHAL_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*MarshalInterface) (struct IMarshal *d, TALLOC_CTX *mem_ctx);\
	WERROR (*UnMarshalInterface) (struct IMarshal *d, TALLOC_CTX *mem_ctx);\

struct IMarshal_vtable {
	struct GUID iid;
	IMARSHAL_METHODS
};

#define IMarshal_MarshalInterface(interface, mem_ctx) ((interface)->vtable->MarshalInterface(interface, mem_ctx))
#define IMarshal_UnMarshalInterface(interface, mem_ctx) ((interface)->vtable->UnMarshalInterface(interface, mem_ctx))
#endif

#ifndef _ICoffeeMachine_
#define _ICoffeeMachine_


/* ICoffeeMachine */
#define COM_ICOFFEEMACHINE_UUID DA23F6DB-6F45-466C-9EED-0B65286F2D78

struct ICoffeeMachine_vtable;

struct ICoffeeMachine {
	struct OBJREF obj;
	struct com_context *ctx;
	struct ICoffeeMachine_vtable *vtable;
	void *object_data;
};

#define ICOFFEEMACHINE_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*MakeCoffee) (struct ICoffeeMachine *d, TALLOC_CTX *mem_ctx, uint16_t *flavor);\

struct ICoffeeMachine_vtable {
	struct GUID iid;
	ICOFFEEMACHINE_METHODS
};

#define ICoffeeMachine_MakeCoffee(interface, mem_ctx, flavor) ((interface)->vtable->MakeCoffee(interface, mem_ctx, flavor))
#endif
#define CLSID_COFFEEMACHINE "db7c21f8-fe33-4c11-aea5-ceb56f076fbb"


#ifndef _IStream_
#define _IStream_


/* IStream */
#define COM_ISTREAM_UUID "0000000C-0000-0000-C000-000000000046"

struct IStream_vtable;

struct IStream {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IStream_vtable *vtable;
	void *object_data;
};

#define ISTREAM_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*Read) (struct IStream *d, TALLOC_CTX *mem_ctx, uint8_t *pv, uint32_t num_requested, uint32_t *num_readx, uint32_t *num_read);\
	WERROR (*Write) (struct IStream *d, TALLOC_CTX *mem_ctx, uint8_t *data, uint32_t num_requested, uint32_t *num_written);\

struct IStream_vtable {
	struct GUID iid;
	ISTREAM_METHODS
};

#define IStream_Read(interface, mem_ctx, pv, num_requested, num_readx, num_read) ((interface)->vtable->Read(interface, mem_ctx, pv, num_requested, num_readx, num_read))
#define IStream_Write(interface, mem_ctx, data, num_requested, num_written) ((interface)->vtable->Write(interface, mem_ctx, data, num_requested, num_written))
#endif
#define CLSID_SIMPLE "5e9ddec7-5767-11cf-beab-00aa006c3606"
#define PROGID_SIMPLE "Samba.Simple"

