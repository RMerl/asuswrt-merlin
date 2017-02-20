#ifndef __WRAP_H__
#define __WRAP_H__

#ifdef WIN32
   #ifndef __MINGW__
      // Explicitly define 32-bit and 64-bit numbers
      typedef __int32 int32_t;
      typedef __int64 int64_t;
      //typedef unsigned __int32 uint32_t;
#if !defined(PJ_WIN32_UWP)
	  typedef unsigned long uint32_t;	// +Roger modified
#endif
      #ifndef LEGACY_WIN32
         typedef unsigned __int64 uint64_t;
      #else
         // VC 6.0 does not support unsigned __int64: may cause potential problems.
         typedef __int64 uint64_t;
      #endif

      #ifdef UDT_EXPORTS
         #define UDT_API //__declspec(dllexport)
      #else
         #define UDT_API //__declspec(dllimport)
      #endif
   #else
      #define UDT_API
   #endif
#else
   #define UDT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Functions to export to C namespace */
//UDT_API int MyUDTCore_Init();
UDT_API void udt_startup(int inst_id);
UDT_API void udt_cleanup(void);
UDT_API int udt_socket(void);
UDT_API int udt_bind(void *call);
#if 0
UDT_API int udt_select(int nfds, UDSET* readfds, UDSET* writefds, UDSET* exceptfds, const struct timeval* tv);
#endif
UDT_API int udt_recv(int u, char* buf, int len, int flags);
UDT_API int udt_send(int u, const char* buf, int len, int flags);
UDT_API int udt_connect(void *call);
UDT_API int udt_close(void *call);
//UDT_API int udt_close(int u);

#ifdef __cplusplus
}
#endif


#endif //__WRAP_H_