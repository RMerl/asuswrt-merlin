#include "librpc/gen_ndr/orpc.h"
#include "librpc/gen_ndr/ndr_wmi.h"

struct IWbemClassObject;
struct IWbemServices;
struct IEnumWbemClassObject;
struct IWbemContext;
struct IWbemLevel1Login;
struct IWbemWCOSmartEnum;
struct IWbemFetchSmartEnum;
struct IWbemCallResult;
struct IWbemObjectSink;
#define CLSID_WBEMLEVEL1LOGIN "8BC3F05E-D86B-11d0-A075-00C04FB68820"


#ifndef _IWbemClassObject_
#define _IWbemClassObject_


/* IWbemClassObject */
#define COM_IWBEMCLASSOBJECT_UUID "dc12a681-737f-11cf-884d-00aa004b2e24"

struct IWbemClassObject_vtable;

struct IWbemClassObject {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemClassObject_vtable *vtable;
	void *object_data;
};

#define IWBEMCLASSOBJECT_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*Delete) (struct IWbemClassObject *d, TALLOC_CTX *mem_ctx, uint16_t *wszName);\

struct IWbemClassObject_vtable {
	struct GUID iid;
	IWBEMCLASSOBJECT_METHODS
};

#define IWbemClassObject_Delete(interface, mem_ctx, wszName) ((interface)->vtable->Delete(interface, mem_ctx, wszName))
#endif
#define CLSID_WBEMCLASSOBJECT 9A653086-174F-11d2-B5F9-00104B703EFD


#ifndef _IWbemServices_
#define _IWbemServices_


/* IWbemServices */
#define COM_IWBEMSERVICES_UUID "9556dc99-828c-11cf-a37e-00aa003240c7"

struct IWbemServices_vtable;

struct IWbemServices {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemServices_vtable *vtable;
	void *object_data;
};

#define IWBEMSERVICES_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*OpenNamespace) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strNamespace, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemServices **ppWorkingNamespace, struct IWbemCallResult **ppResult);\
	WERROR (*CancelAsyncCall) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct IWbemObjectSink *pSink);\
	WERROR (*QueryObjectSink) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, int32_t lFlags, struct IWbemObjectSink **ppResponseHandler);\
	WERROR (*GetObject) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strObjectPath, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemClassObject **ppObject, struct IWbemCallResult **ppCallResult);\
	WERROR (*GetObjectAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strObjectPath, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*PutClass) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject *pObject, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemCallResult **ppCallResult);\
	WERROR (*PutClassAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject *pObject, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*DeleteClass) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strClass, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemCallResult **ppCallResult);\
	WERROR (*DeleteClassAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strClass, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*CreateClassEnum) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strSuperclass, int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);\
	WERROR (*CreateClassEnumAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strSuperclass, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*PutInstance) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject *pInst, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemCallResult **ppCallResult);\
	WERROR (*PutInstanceAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject *pInst, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*DeleteInstance) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strObjectPath, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemCallResult **ppCallResult);\
	WERROR (*DeleteInstanceAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strObjectPath, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*CreateInstanceEnum) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strFilter, int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);\
	WERROR (*CreateInstanceEnumAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strSuperClass, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*ExecQuery) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strQueryLanguage, struct BSTR strQuery, int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);\
	WERROR (*ExecQueryAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strQueryLanguage, struct BSTR strQuery, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*ExecNotificationQuery) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strQueryLanguage, struct BSTR strQuery, int32_t lFlags, struct IWbemContext *pCtx, struct IEnumWbemClassObject **ppEnum);\
	WERROR (*ExecNotificationQueryAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strQueryLanguage, struct BSTR strQuery, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemObjectSink *pResponseHandler);\
	WERROR (*ExecMethod) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strObjectPath, struct BSTR strMethodName, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemClassObject *pInParams, struct IWbemClassObject **ppOutParams, struct IWbemCallResult **ppCallResult);\
	WERROR (*ExecMethodAsync) (struct IWbemServices *d, TALLOC_CTX *mem_ctx, struct BSTR strObjectPath, struct BSTR strMethodName, uint32_t lFlags, struct IWbemContext *pCtx, struct IWbemClassObject *pInParams, struct IWbemObjectSink *pResponseHandler);\

struct IWbemServices_vtable {
	struct GUID iid;
	IWBEMSERVICES_METHODS
};

#define IWbemServices_OpenNamespace(interface, mem_ctx, strNamespace, lFlags, pCtx, ppWorkingNamespace, ppResult) ((interface)->vtable->OpenNamespace(interface, mem_ctx, strNamespace, lFlags, pCtx, ppWorkingNamespace, ppResult))
#define IWbemServices_CancelAsyncCall(interface, mem_ctx, pSink) ((interface)->vtable->CancelAsyncCall(interface, mem_ctx, pSink))
#define IWbemServices_QueryObjectSink(interface, mem_ctx, lFlags, ppResponseHandler) ((interface)->vtable->QueryObjectSink(interface, mem_ctx, lFlags, ppResponseHandler))
#define IWbemServices_GetObject(interface, mem_ctx, strObjectPath, lFlags, pCtx, ppObject, ppCallResult) ((interface)->vtable->GetObject(interface, mem_ctx, strObjectPath, lFlags, pCtx, ppObject, ppCallResult))
#define IWbemServices_GetObjectAsync(interface, mem_ctx, strObjectPath, lFlags, pCtx, pResponseHandler) ((interface)->vtable->GetObjectAsync(interface, mem_ctx, strObjectPath, lFlags, pCtx, pResponseHandler))
#define IWbemServices_PutClass(interface, mem_ctx, pObject, lFlags, pCtx, ppCallResult) ((interface)->vtable->PutClass(interface, mem_ctx, pObject, lFlags, pCtx, ppCallResult))
#define IWbemServices_PutClassAsync(interface, mem_ctx, pObject, lFlags, pCtx, pResponseHandler) ((interface)->vtable->PutClassAsync(interface, mem_ctx, pObject, lFlags, pCtx, pResponseHandler))
#define IWbemServices_DeleteClass(interface, mem_ctx, strClass, lFlags, pCtx, ppCallResult) ((interface)->vtable->DeleteClass(interface, mem_ctx, strClass, lFlags, pCtx, ppCallResult))
#define IWbemServices_DeleteClassAsync(interface, mem_ctx, strClass, lFlags, pCtx, pResponseHandler) ((interface)->vtable->DeleteClassAsync(interface, mem_ctx, strClass, lFlags, pCtx, pResponseHandler))
#define IWbemServices_CreateClassEnum(interface, mem_ctx, strSuperclass, lFlags, pCtx, ppEnum) ((interface)->vtable->CreateClassEnum(interface, mem_ctx, strSuperclass, lFlags, pCtx, ppEnum))
#define IWbemServices_CreateClassEnumAsync(interface, mem_ctx, strSuperclass, lFlags, pCtx, pResponseHandler) ((interface)->vtable->CreateClassEnumAsync(interface, mem_ctx, strSuperclass, lFlags, pCtx, pResponseHandler))
#define IWbemServices_PutInstance(interface, mem_ctx, pInst, lFlags, pCtx, ppCallResult) ((interface)->vtable->PutInstance(interface, mem_ctx, pInst, lFlags, pCtx, ppCallResult))
#define IWbemServices_PutInstanceAsync(interface, mem_ctx, pInst, lFlags, pCtx, pResponseHandler) ((interface)->vtable->PutInstanceAsync(interface, mem_ctx, pInst, lFlags, pCtx, pResponseHandler))
#define IWbemServices_DeleteInstance(interface, mem_ctx, strObjectPath, lFlags, pCtx, ppCallResult) ((interface)->vtable->DeleteInstance(interface, mem_ctx, strObjectPath, lFlags, pCtx, ppCallResult))
#define IWbemServices_DeleteInstanceAsync(interface, mem_ctx, strObjectPath, lFlags, pCtx, pResponseHandler) ((interface)->vtable->DeleteInstanceAsync(interface, mem_ctx, strObjectPath, lFlags, pCtx, pResponseHandler))
#define IWbemServices_CreateInstanceEnum(interface, mem_ctx, strFilter, lFlags, pCtx, ppEnum) ((interface)->vtable->CreateInstanceEnum(interface, mem_ctx, strFilter, lFlags, pCtx, ppEnum))
#define IWbemServices_CreateInstanceEnumAsync(interface, mem_ctx, strSuperClass, lFlags, pCtx, pResponseHandler) ((interface)->vtable->CreateInstanceEnumAsync(interface, mem_ctx, strSuperClass, lFlags, pCtx, pResponseHandler))
#define IWbemServices_ExecQuery(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum) ((interface)->vtable->ExecQuery(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum))
#define IWbemServices_ExecQueryAsync(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, pResponseHandler) ((interface)->vtable->ExecQueryAsync(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, pResponseHandler))
#define IWbemServices_ExecNotificationQuery(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum) ((interface)->vtable->ExecNotificationQuery(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum))
#define IWbemServices_ExecNotificationQueryAsync(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, pResponseHandler) ((interface)->vtable->ExecNotificationQueryAsync(interface, mem_ctx, strQueryLanguage, strQuery, lFlags, pCtx, pResponseHandler))
#define IWbemServices_ExecMethod(interface, mem_ctx, strObjectPath, strMethodName, lFlags, pCtx, pInParams, ppOutParams, ppCallResult) ((interface)->vtable->ExecMethod(interface, mem_ctx, strObjectPath, strMethodName, lFlags, pCtx, pInParams, ppOutParams, ppCallResult))
#define IWbemServices_ExecMethodAsync(interface, mem_ctx, strObjectPath, strMethodName, lFlags, pCtx, pInParams, pResponseHandler) ((interface)->vtable->ExecMethodAsync(interface, mem_ctx, strObjectPath, strMethodName, lFlags, pCtx, pInParams, pResponseHandler))
#endif

#ifndef _IEnumWbemClassObject_
#define _IEnumWbemClassObject_


/* IEnumWbemClassObject */
#define COM_IENUMWBEMCLASSOBJECT_UUID 027947e1-d731-11ce-a357-000000000001

struct IEnumWbemClassObject_vtable;

struct IEnumWbemClassObject {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IEnumWbemClassObject_vtable *vtable;
	void *object_data;
};

#define IENUMWBEMCLASSOBJECT_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*Reset) (struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx);\
	WERROR (*IEnumWbemClassObject_Next) (struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, uint32_t uCount, struct IWbemClassObject **apObjects, uint32_t *puReturned);\
	WERROR (*NextAsync) (struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, uint32_t uCount, struct IWbemObjectSink *pSink);\
	WERROR (*IEnumWbemClassObject_Clone) (struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, struct IEnumWbemClassObject **ppEnum);\
	WERROR (*Skip) (struct IEnumWbemClassObject *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, uint32_t nCount);\

struct IEnumWbemClassObject_vtable {
	struct GUID iid;
	IENUMWBEMCLASSOBJECT_METHODS
};

#define IEnumWbemClassObject_Reset(interface, mem_ctx) ((interface)->vtable->Reset(interface, mem_ctx))
#define IEnumWbemClassObject_IEnumWbemClassObject_Next(interface, mem_ctx, lTimeout, uCount, apObjects, puReturned) ((interface)->vtable->IEnumWbemClassObject_Next(interface, mem_ctx, lTimeout, uCount, apObjects, puReturned))
#define IEnumWbemClassObject_NextAsync(interface, mem_ctx, uCount, pSink) ((interface)->vtable->NextAsync(interface, mem_ctx, uCount, pSink))
#define IEnumWbemClassObject_IEnumWbemClassObject_Clone(interface, mem_ctx, ppEnum) ((interface)->vtable->IEnumWbemClassObject_Clone(interface, mem_ctx, ppEnum))
#define IEnumWbemClassObject_Skip(interface, mem_ctx, lTimeout, nCount) ((interface)->vtable->Skip(interface, mem_ctx, lTimeout, nCount))
#endif

#ifndef _IWbemContext_
#define _IWbemContext_


/* IWbemContext */
#define COM_IWBEMCONTEXT_UUID "44aca674-e8fc-11d0-a07c-00c04fb68820"

struct IWbemContext_vtable;

struct IWbemContext {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemContext_vtable *vtable;
	void *object_data;
};

#define IWBEMCONTEXT_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*Clone) (struct IWbemContext *d, TALLOC_CTX *mem_ctx, struct IWbemContext **ppNewCopy);\
	WERROR (*GetNames) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\
	WERROR (*BeginEnumeration) (struct IWbemContext *d, TALLOC_CTX *mem_ctx, int32_t lFlags);\
	WERROR (*Next) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\
	WERROR (*EndEnumeration) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\
	WERROR (*SetValue) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\
	WERROR (*GetValue) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\
	WERROR (*DeleteValue) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\
	WERROR (*DeleteAll) (struct IWbemContext *d, TALLOC_CTX *mem_ctx);\

struct IWbemContext_vtable {
	struct GUID iid;
	IWBEMCONTEXT_METHODS
};

#define IWbemContext_Clone(interface, mem_ctx, ppNewCopy) ((interface)->vtable->Clone(interface, mem_ctx, ppNewCopy))
#define IWbemContext_GetNames(interface, mem_ctx) ((interface)->vtable->GetNames(interface, mem_ctx))
#define IWbemContext_BeginEnumeration(interface, mem_ctx, lFlags) ((interface)->vtable->BeginEnumeration(interface, mem_ctx, lFlags))
#define IWbemContext_Next(interface, mem_ctx) ((interface)->vtable->Next(interface, mem_ctx))
#define IWbemContext_EndEnumeration(interface, mem_ctx) ((interface)->vtable->EndEnumeration(interface, mem_ctx))
#define IWbemContext_SetValue(interface, mem_ctx) ((interface)->vtable->SetValue(interface, mem_ctx))
#define IWbemContext_GetValue(interface, mem_ctx) ((interface)->vtable->GetValue(interface, mem_ctx))
#define IWbemContext_DeleteValue(interface, mem_ctx) ((interface)->vtable->DeleteValue(interface, mem_ctx))
#define IWbemContext_DeleteAll(interface, mem_ctx) ((interface)->vtable->DeleteAll(interface, mem_ctx))
#endif

#ifndef _IWbemLevel1Login_
#define _IWbemLevel1Login_


/* IWbemLevel1Login */
#define COM_IWBEMLEVEL1LOGIN_UUID "F309AD18-D86A-11d0-A075-00C04FB68820"

struct IWbemLevel1Login_vtable;

struct IWbemLevel1Login {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemLevel1Login_vtable *vtable;
	void *object_data;
};

#define IWBEMLEVEL1LOGIN_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*EstablishPosition) (struct IWbemLevel1Login *d, TALLOC_CTX *mem_ctx, uint16_t *wszLocaleList, uint32_t dwNumLocales, uint32_t *reserved);\
	WERROR (*RequestChallenge) (struct IWbemLevel1Login *d, TALLOC_CTX *mem_ctx, uint16_t *wszNetworkResource, uint16_t *wszUser, uint8_t *Nonce);\
	WERROR (*WBEMLogin) (struct IWbemLevel1Login *d, TALLOC_CTX *mem_ctx, uint16_t *wszPreferredLocale, uint8_t *AccessToken, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemServices **ppNamespace);\
	WERROR (*NTLMLogin) (struct IWbemLevel1Login *d, TALLOC_CTX *mem_ctx, uint16_t *wszNetworkResource, uint16_t *wszPreferredLocale, int32_t lFlags, struct IWbemContext *pCtx, struct IWbemServices **ppNamespace);\

struct IWbemLevel1Login_vtable {
	struct GUID iid;
	IWBEMLEVEL1LOGIN_METHODS
};

#define IWbemLevel1Login_EstablishPosition(interface, mem_ctx, wszLocaleList, dwNumLocales, reserved) ((interface)->vtable->EstablishPosition(interface, mem_ctx, wszLocaleList, dwNumLocales, reserved))
#define IWbemLevel1Login_RequestChallenge(interface, mem_ctx, wszNetworkResource, wszUser, Nonce) ((interface)->vtable->RequestChallenge(interface, mem_ctx, wszNetworkResource, wszUser, Nonce))
#define IWbemLevel1Login_WBEMLogin(interface, mem_ctx, wszPreferredLocale, AccessToken, lFlags, pCtx, ppNamespace) ((interface)->vtable->WBEMLogin(interface, mem_ctx, wszPreferredLocale, AccessToken, lFlags, pCtx, ppNamespace))
#define IWbemLevel1Login_NTLMLogin(interface, mem_ctx, wszNetworkResource, wszPreferredLocale, lFlags, pCtx, ppNamespace) ((interface)->vtable->NTLMLogin(interface, mem_ctx, wszNetworkResource, wszPreferredLocale, lFlags, pCtx, ppNamespace))
#endif

#ifndef _IWbemWCOSmartEnum_
#define _IWbemWCOSmartEnum_


/* IWbemWCOSmartEnum */
#define COM_IWBEMWCOSMARTENUM_UUID "423ec01e-2e35-11d2-b604-00104b703efd"

struct IWbemWCOSmartEnum_vtable;

struct IWbemWCOSmartEnum {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemWCOSmartEnum_vtable *vtable;
	void *object_data;
};

#define IWBEMWCOSMARTENUM_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*IWbemWCOSmartEnum_Next) (struct IWbemWCOSmartEnum *d, TALLOC_CTX *mem_ctx, struct GUID *gEWCO, uint32_t lTimeOut, uint32_t uCount, uint32_t unknown, struct GUID *gWCO, uint32_t *puReturned, uint32_t *pSize, uint8_t **pData);\

struct IWbemWCOSmartEnum_vtable {
	struct GUID iid;
	IWBEMWCOSMARTENUM_METHODS
};

#define IWbemWCOSmartEnum_IWbemWCOSmartEnum_Next(interface, mem_ctx, gEWCO, lTimeOut, uCount, unknown, gWCO, puReturned, pSize, pData) ((interface)->vtable->IWbemWCOSmartEnum_Next(interface, mem_ctx, gEWCO, lTimeOut, uCount, unknown, gWCO, puReturned, pSize, pData))
#endif

#ifndef _IWbemFetchSmartEnum_
#define _IWbemFetchSmartEnum_


/* IWbemFetchSmartEnum */
#define COM_IWBEMFETCHSMARTENUM_UUID "1c1c45ee-4395-11d2-b60b-00104b703efd"

struct IWbemFetchSmartEnum_vtable;

struct IWbemFetchSmartEnum {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemFetchSmartEnum_vtable *vtable;
	void *object_data;
};

#define IWBEMFETCHSMARTENUM_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*Fetch) (struct IWbemFetchSmartEnum *d, TALLOC_CTX *mem_ctx, struct IWbemWCOSmartEnum **ppEnum);\
	WERROR (*Test) (struct IWbemFetchSmartEnum *d, TALLOC_CTX *mem_ctx, struct IWbemClassObject **ppEnum);\

struct IWbemFetchSmartEnum_vtable {
	struct GUID iid;
	IWBEMFETCHSMARTENUM_METHODS
};

#define IWbemFetchSmartEnum_Fetch(interface, mem_ctx, ppEnum) ((interface)->vtable->Fetch(interface, mem_ctx, ppEnum))
#define IWbemFetchSmartEnum_Test(interface, mem_ctx, ppEnum) ((interface)->vtable->Test(interface, mem_ctx, ppEnum))
#endif

#ifndef _IWbemCallResult_
#define _IWbemCallResult_


/* IWbemCallResult */
#define COM_IWBEMCALLRESULT_UUID 44aca675-e8fc-11d0-a07c-00c04fb68820

struct IWbemCallResult_vtable;

struct IWbemCallResult {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemCallResult_vtable *vtable;
	void *object_data;
};

#define IWBEMCALLRESULT_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*GetResultObject) (struct IWbemCallResult *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, struct IWbemClassObject **ppResultObject);\
	WERROR (*GetResultString) (struct IWbemCallResult *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, struct BSTR *pstrResultString);\
	WERROR (*GetResultServices) (struct IWbemCallResult *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, struct IWbemServices **ppServices);\
	WERROR (*GetCallStatus) (struct IWbemCallResult *d, TALLOC_CTX *mem_ctx, int32_t lTimeout, int32_t *plStatus);\

struct IWbemCallResult_vtable {
	struct GUID iid;
	IWBEMCALLRESULT_METHODS
};

#define IWbemCallResult_GetResultObject(interface, mem_ctx, lTimeout, ppResultObject) ((interface)->vtable->GetResultObject(interface, mem_ctx, lTimeout, ppResultObject))
#define IWbemCallResult_GetResultString(interface, mem_ctx, lTimeout, pstrResultString) ((interface)->vtable->GetResultString(interface, mem_ctx, lTimeout, pstrResultString))
#define IWbemCallResult_GetResultServices(interface, mem_ctx, lTimeout, ppServices) ((interface)->vtable->GetResultServices(interface, mem_ctx, lTimeout, ppServices))
#define IWbemCallResult_GetCallStatus(interface, mem_ctx, lTimeout, plStatus) ((interface)->vtable->GetCallStatus(interface, mem_ctx, lTimeout, plStatus))
#endif

#ifndef _IWbemObjectSink_
#define _IWbemObjectSink_


/* IWbemObjectSink */
#define COM_IWBEMOBJECTSINK_UUID 7c857801-7381-11cf-884d-00aa004b2e24

struct IWbemObjectSink_vtable;

struct IWbemObjectSink {
	struct OBJREF obj;
	struct com_context *ctx;
	struct IWbemObjectSink_vtable *vtable;
	void *object_data;
};

#define IWBEMOBJECTSINK_METHODS \
	IUNKNOWN_METHODS\
	WERROR (*SetStatus) (struct IWbemObjectSink *d, TALLOC_CTX *mem_ctx, int32_t lFlags, WERROR hResult, struct BSTR strParam, struct IWbemClassObject *pObjParam);\
	WERROR (*Indicate) (struct IWbemObjectSink *d, TALLOC_CTX *mem_ctx, int32_t lObjectCount, struct IWbemClassObject **apObjArray);\

struct IWbemObjectSink_vtable {
	struct GUID iid;
	IWBEMOBJECTSINK_METHODS
};

#define IWbemObjectSink_SetStatus(interface, mem_ctx, lFlags, hResult, strParam, pObjParam) ((interface)->vtable->SetStatus(interface, mem_ctx, lFlags, hResult, strParam, pObjParam))
#define IWbemObjectSink_Indicate(interface, mem_ctx, lObjectCount, apObjArray) ((interface)->vtable->Indicate(interface, mem_ctx, lObjectCount, apObjArray))
#endif
