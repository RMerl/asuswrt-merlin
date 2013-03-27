/* 
   Microsoft SSPI based authentication routines
   Copyright (C) 2004-2005, Vladimir Berezniker @ http://public.xdi.org/=vmpn
   Copyright (C) 2007, Yves Martin  <ymartin59@free.fr>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#include "ne_utils.h"
#include "ne_string.h"
#include "ne_socket.h"
#include "ne_sspi.h"

#ifdef HAVE_SSPI

#define SEC_SUCCESS(Status) ((Status) >= 0)

#ifndef SECURITY_ENTRYPOINT   /* Missing in MingW 3.7 */
#define SECURITY_ENTRYPOINT "InitSecurityInterfaceA"
#endif

struct SSPIContextStruct {
    CtxtHandle context;
    char *serverName;
    CredHandle credentials;
    int continueNeeded;
    int authfinished;
    char *mechanism;
    int ntlm;
    ULONG maxTokenSize;
};

typedef struct SSPIContextStruct SSPIContext;

static ULONG negotiateMaxTokenSize = 0;
static ULONG ntlmMaxTokenSize = 0;
static HINSTANCE hSecDll = NULL;
static PSecurityFunctionTable pSFT = NULL;
static int initialized = 0;

/*
 * Query specified package for it's maximum token size.
 */
static int getMaxTokenSize(char *package, ULONG * maxTokenSize)
{
    SECURITY_STATUS status;
    SecPkgInfo *packageSecurityInfo = NULL;

    status = pSFT->QuerySecurityPackageInfo(package, &packageSecurityInfo);
    if (status == SEC_E_OK) {
        *maxTokenSize = packageSecurityInfo->cbMaxToken;
        if (pSFT->FreeContextBuffer(packageSecurityInfo) != SEC_E_OK) {
            NE_DEBUG(NE_DBG_HTTPAUTH,
                     "sspi: Unable to free security package info.");
        }
    } else {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: QuerySecurityPackageInfo [failed] [%x].", status);
        return -1;
    }

    return 0;
}

/*
 * Initialize all the SSPI data
 */
static void initDll(HINSTANCE hSecDll)
{
    INIT_SECURITY_INTERFACE initSecurityInterface = NULL;

    initSecurityInterface =
        (INIT_SECURITY_INTERFACE) GetProcAddress(hSecDll,
                                                 SECURITY_ENTRYPOINT);

    if (initSecurityInterface == NULL) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: Obtaining security interface [fail].\n");
        initialized = -1;
        return;
    } else {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: Obtaining security interface [ok].\n");
    }

    pSFT = (initSecurityInterface) ();

    if (pSFT == NULL) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Security Function Table [fail].\n");
        initialized = -2;
        return;
    } else {
        NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Security Function Table [ok].\n");
    }

    if (getMaxTokenSize("Negotiate", &negotiateMaxTokenSize)) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: Unable to get negotiate maximum packet size");
        initialized = -3;
    }

    if (getMaxTokenSize("NTLM", &ntlmMaxTokenSize)) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: Unable to get negotiate maximum packet size");
        initialized = -3;
    }
}

/*
 * This function needs to be called at least once before using any other.
 */
int ne_sspi_init(void)
{
    if (initialized) {
        return 0;
    }

    NE_DEBUG(NE_DBG_SOCKET, "sspiInit\n");
    NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Loading security dll.\n");
    hSecDll = LoadLibrary("security.dll");

    if (hSecDll == NULL) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Loading of security dll [fail].\n");
    } else {
        NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Loading of security dll [ok].\n");
        initDll(hSecDll);
        if (initialized == 0) {
            initialized = 1;
        }
    }

    NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: sspiInit [%d].\n", initialized);
    if (initialized < 0) {
        return initialized;
    } else {
        return 0;
    }
}

/*
 * This function can be called to free resources used by SSPI.
 */
int ne_sspi_deinit(void)
{
    NE_DEBUG(NE_DBG_SOCKET, "sspi: DeInit\n");
    if (initialized <= 0) {
        return initialized;
    }

    pSFT = NULL;

    if (hSecDll != NULL) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Unloading security dll.\n");
        if (FreeLibrary(hSecDll)) {
            NE_DEBUG(NE_DBG_HTTPAUTH,
                     "sspi: Unloading of security dll [ok].\n");
        } else {
            NE_DEBUG(NE_DBG_HTTPAUTH,
                     "sspi: Unloading of security dll [fail].\n");
            return -1;
        }
        hSecDll = NULL;
    }

    initialized = 0;
    return 0;
}

/*
 * Simplification wrapper arround AcquireCredentialsHandle as most of
 * the parameters do not change.
 */
static int acquireCredentialsHandle(CredHandle * credentials, char *package)
{
    SECURITY_STATUS status;
    TimeStamp timestamp;

    status =
        pSFT->AcquireCredentialsHandle(NULL, package, SECPKG_CRED_OUTBOUND,
                                       NULL, NULL, NULL, NULL, credentials,
                                       &timestamp);

    if (status != SEC_E_OK) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: AcquireCredentialsHandle [fail] [%x].\n", status);
        return -1;
    }

    return 0;
}

/*
 * Wrapper arround initializeSecurityContext.  Supplies several
 * default parameters as well as logging in case of errors.
 */
static SECURITY_STATUS
initializeSecurityContext(CredHandle * credentials, CtxtHandle * context,
                          char *spn, ULONG contextReq,
                          SecBufferDesc * inBuffer, CtxtHandle * newContext,
                          SecBufferDesc * outBuffer)
{
    ULONG contextAttributes;
    SECURITY_STATUS status;

    status =
        pSFT->InitializeSecurityContext(credentials, context, spn, contextReq,
                                        0, SECURITY_NETWORK_DREP, inBuffer, 0,
                                        newContext, outBuffer,
                                        &contextAttributes, NULL);

    if (!SEC_SUCCESS(status)) {
        if (status == SEC_E_INVALID_TOKEN) {
            NE_DEBUG(NE_DBG_HTTPAUTH,
                     "InitializeSecurityContext [fail] SEC_E_INVALID_TOKEN.\n");
        } else if (status == SEC_E_UNSUPPORTED_FUNCTION) {
            NE_DEBUG(NE_DBG_HTTPAUTH,
                     "InitializeSecurityContext [fail] SEC_E_UNSUPPORTED_FUNCTION.\n");
        } else {
            NE_DEBUG(NE_DBG_HTTPAUTH,
                     "InitializeSecurityContext [fail] [%x].\n", status);
        }
    }

    return status;
}

/*
 * Validates that the pointer is not NULL and converts it to its real type.
 */
static int getContext(void *context, SSPIContext **sspiContext)
{
    if (!context) {
        return -1;
    }

    *sspiContext = context;
    return 0;
}

/*
 * Verifies that the buffer descriptor point only to one buffer and
 * returns the pointer to it.
 */
static int getSingleBufferDescriptor(SecBufferDesc *secBufferDesc,
                                     SecBuffer **secBuffer)
{
    if (secBufferDesc->cBuffers != 1) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: fillBufferDescriptor "
                 "[fail] numbers of descriptor buffers. 1 != [%d].\n",
                 secBufferDesc->cBuffers);
        return -1;
    }

    *secBuffer = secBufferDesc->pBuffers;
    return 0;
}

/*
 * Decodes BASE64 string into SSPI SecBuffer
 */
static int base64ToBuffer(const char *token, SecBufferDesc * secBufferDesc)
{
    SecBuffer *buffer;
    if (getSingleBufferDescriptor(secBufferDesc, &buffer)) {
        return -1;
    }

    buffer->BufferType = SECBUFFER_TOKEN;
    buffer->cbBuffer =
        ne_unbase64(token, (unsigned char **) &buffer->pvBuffer);

    if (buffer->cbBuffer == 0) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: Unable to decode BASE64 SSPI token.\n");
        return -1;
    }

    return 0;
}

/*
 * Creates a SecBuffer of a specified size.
 */
static int makeBuffer(SecBufferDesc * secBufferDesc, ULONG size)
{
    SecBuffer *buffer;
    if (getSingleBufferDescriptor(secBufferDesc, &buffer)) {
        return -1;
    }

    buffer->BufferType = SECBUFFER_TOKEN;
    buffer->cbBuffer = size;
    buffer->pvBuffer = ne_calloc(size);

    return 0;
}

/*
 * Frees data allocated in the buffer.
 */
static int freeBuffer(SecBufferDesc * secBufferDesc)
{
    SecBuffer *buffer;
    if (getSingleBufferDescriptor(secBufferDesc, &buffer)) {
        return -1;
    }

    if (buffer->cbBuffer > 0 && buffer->pvBuffer) {
        ne_free(buffer->pvBuffer);
        buffer->cbBuffer = 0;
        buffer->pvBuffer = NULL;
    }

    return 0;
}

/*
 * Canonicalize a server host name if possible.
 * The returned pointer must be freed after usage.
 */
static char *canonical_hostname(const char *serverName)
{
    char *hostname;
    ne_sock_addr *addresses;
    
    /* DNS resolution.  It would be useful to be able to use the
     * AI_CANONNAME flag where getaddrinfo() is available, but the
     * reverse-lookup is sufficient and simpler. */
    addresses = ne_addr_resolve(serverName, 0);
    if (ne_addr_result(addresses)) {
        /* Lookup failed */
        char buf[256];
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: Could not resolve IP address for `%s': %s\n",
                 serverName, ne_addr_error(addresses, buf, sizeof buf));
        hostname = ne_strdup(serverName);
    } else {
        char hostbuffer[256];
        const ne_inet_addr *address = ne_addr_first(addresses);

        if (ne_iaddr_reverse(address, hostbuffer, sizeof hostbuffer) == 0) {
            hostname = ne_strdup(hostbuffer);
        } else {
            NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Could not resolve host name"
                     "from IP address for `%s'\n", serverName);
            hostname = ne_strdup(serverName);
        }
    }

    ne_addr_destroy(addresses);
    return hostname;
}

/*
 * Create a context to authenticate to specified server, using either
 * ntlm or negotiate.
 */
int ne_sspi_create_context(void **context, char *serverName, int ntlm)
{
    SSPIContext *sspiContext;
    char *canonicalName;

    if (initialized <= 0) {
        return -1;
    }

    sspiContext = ne_calloc(sizeof(SSPIContext));
    sspiContext->continueNeeded = 0;

    if (ntlm) {
        sspiContext->mechanism = "NTLM";
        sspiContext->serverName = ne_strdup(serverName);
        sspiContext->maxTokenSize = ntlmMaxTokenSize;
    } else {
        sspiContext->mechanism = "Negotiate";
        /* Canonicalize to conform to GSSAPI behavior */
        canonicalName = canonical_hostname(serverName);
        sspiContext->serverName = ne_concat("HTTP/", canonicalName, NULL);
        ne_free(canonicalName);
        NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Created context with SPN '%s'\n",
                 sspiContext->serverName);
        sspiContext->maxTokenSize = negotiateMaxTokenSize;
    }

    sspiContext->ntlm = ntlm;
    sspiContext->authfinished = 0;
    *context = sspiContext;
    return 0;
}

/*
 * Resets the context
 */
static void resetContext(SSPIContext * sspiContext)
{
    pSFT->DeleteSecurityContext(&(sspiContext->context));
#if defined(_MSC_VER) && _MSC_VER <= 1200
    pSFT->FreeCredentialHandle(&(sspiContext->credentials));
#else
    pSFT->FreeCredentialsHandle(&(sspiContext->credentials));
#endif
    sspiContext->continueNeeded = 0;
}

/*
 * Initializes supplied SecBufferDesc to point to supplied SecBuffer
 * that is also initialized;
 */
static void
initSingleEmptyBuffer(SecBufferDesc * bufferDesc, SecBuffer * buffer)
{
    buffer->BufferType = SECBUFFER_EMPTY;
    buffer->cbBuffer = 0;
    buffer->pvBuffer = NULL;

    bufferDesc->cBuffers = 1;
    bufferDesc->ulVersion = SECBUFFER_VERSION;
    bufferDesc->pBuffers = buffer;

}

/*
 * Destroyes the supplied context.
 */
int ne_sspi_destroy_context(void *context)
{

    int status;
    SSPIContext *sspiContext;

    if (initialized <= 0) {
        return -1;
    }

    status = getContext(context, &sspiContext);
    if (status) {
        return status;
    }

    resetContext(sspiContext);
    if (sspiContext->serverName) {
        ne_free(sspiContext->serverName);
        sspiContext->serverName = NULL;
    }

    ne_free(sspiContext);
    return 0;
}
int ne_sspi_clear_context(void *context)
{
    int status;
    SSPIContext *sspiContext;

    if (initialized <= 0) {
        return -1;
    }

    status = getContext(context, &sspiContext);
    if (status) {
        return status;
    }
    sspiContext->authfinished = 0;
    return 0;
}
/*
 * Processes received authentication tokens as well as supplies the
 * response token.
 */
int ne_sspi_authenticate(void *context, const char *base64Token, char **responseToken)
{
    SecBufferDesc outBufferDesc;
    SecBuffer outBuffer;
    int status;
    SECURITY_STATUS securityStatus;
    ULONG contextFlags;

    SSPIContext *sspiContext;
    if (initialized <= 0) {
        return -1;
    }

    status = getContext(context, &sspiContext);
    if (status) {
        return status;
    }

    /* TODO: Not sure what flags should be set. joe: this needs to be
     * driven by the ne_auth interface; the GSSAPI code needs similar
     * flags. */
    contextFlags = ISC_REQ_CONFIDENTIALITY | ISC_REQ_MUTUAL_AUTH;

    initSingleEmptyBuffer(&outBufferDesc, &outBuffer);
    status = makeBuffer(&outBufferDesc, sspiContext->maxTokenSize);
    if (status) {
        return status;
    }

    if (base64Token) {
        SecBufferDesc inBufferDesc;
        SecBuffer inBuffer;

        if (!sspiContext->continueNeeded) {
            freeBuffer(&outBufferDesc);
            NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Got an unexpected token.\n");
            return -1;
        }

        initSingleEmptyBuffer(&inBufferDesc, &inBuffer);

        status = base64ToBuffer(base64Token, &inBufferDesc);
        if (status) {
            freeBuffer(&outBufferDesc);
            return status;
        }

        securityStatus =
            initializeSecurityContext(&sspiContext->credentials,
                                      &(sspiContext->context),
                                      sspiContext->serverName, contextFlags,
                                      &inBufferDesc, &(sspiContext->context),
                                      &outBufferDesc);
        if (securityStatus == SEC_E_OK)
        {
            sspiContext->authfinished = 1;
        }
        freeBuffer(&inBufferDesc);
    } else {
        if (sspiContext->continueNeeded) {
            freeBuffer(&outBufferDesc);
            NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: Expected a token from server.\n");
            return -1;
        }
        if (sspiContext->authfinished && (sspiContext->credentials.dwLower || sspiContext->credentials.dwUpper)) {
            if (sspiContext->authfinished)
            {
                freeBuffer(&outBufferDesc);
                sspiContext->authfinished = 0;
                NE_DEBUG(NE_DBG_HTTPAUTH,"sspi: failing because starting over from failed try.\n");
                return -1;
            }
            sspiContext->authfinished = 0;
        }

        /* Reset any existing context since we are starting over */
        resetContext(sspiContext);

        if (acquireCredentialsHandle
            (&sspiContext->credentials, sspiContext->mechanism) != SEC_E_OK) {
                freeBuffer(&outBufferDesc);
                NE_DEBUG(NE_DBG_HTTPAUTH,
                    "sspi: acquireCredentialsHandle failed.\n");
                return -1;
        }

        securityStatus =
            initializeSecurityContext(&sspiContext->credentials, NULL,
                                      sspiContext->serverName, contextFlags,
                                      NULL, &(sspiContext->context),
                                      &outBufferDesc);
    }

    if (securityStatus == SEC_I_COMPLETE_AND_CONTINUE
        || securityStatus == SEC_I_COMPLETE_NEEDED) {
        SECURITY_STATUS compleStatus =
            pSFT->CompleteAuthToken(&(sspiContext->context), &outBufferDesc);

        if (compleStatus != SEC_E_OK) {
            freeBuffer(&outBufferDesc);
            NE_DEBUG(NE_DBG_HTTPAUTH, "sspi: CompleteAuthToken failed.\n");
            return -1;
        }
    }

    if (securityStatus == SEC_I_COMPLETE_AND_CONTINUE
        || securityStatus == SEC_I_CONTINUE_NEEDED) {
        sspiContext->continueNeeded = 1;
    } else {
        sspiContext->continueNeeded = 0;
    }

    if (!(securityStatus == SEC_I_COMPLETE_AND_CONTINUE
          || securityStatus == SEC_I_COMPLETE_NEEDED
          || securityStatus == SEC_I_CONTINUE_NEEDED
          || securityStatus == SEC_E_OK)) {
        NE_DEBUG(NE_DBG_HTTPAUTH,
                 "sspi: initializeSecurityContext [failed] [%x].\n",
                 securityStatus);
        freeBuffer(&outBufferDesc);
        return -1;
    }

    *responseToken = ne_base64(outBufferDesc.pBuffers->pvBuffer,
                               outBufferDesc.pBuffers->cbBuffer);
    freeBuffer(&outBufferDesc);

    return 0;
}
#endif /* HAVE_SSPI */
