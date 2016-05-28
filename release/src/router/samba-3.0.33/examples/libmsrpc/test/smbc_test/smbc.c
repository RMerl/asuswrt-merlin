/*simple test for libsmbclient compatibility. initialize a smbc context, open sessions on a couple pipes and quit*/

#include "libmsrpc.h"
#include "libsmbclient.h"
#include "test_util.h"

int main(int argc, char **argv) {
   SMBCCTX *ctx = NULL;
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;

   struct LsaOpenPolicy lop;
   struct RegConnect    rc;
   struct SamOpenDomain sod;

   ZERO_STRUCT(lop);
   ZERO_STRUCT(rc);
   ZERO_STRUCT(sod);

   mem_ctx = talloc_init("cac_smbc");
   if(!mem_ctx) {
      printf("Could not initialize talloc context\n");
      exit(-1);
   }
   
   hnd = cac_NewServerHandle(True);

   cac_parse_cmd_line(argc, argv, hnd);

   /*initialize smbc context*/
   if( (ctx = smbc_new_context()) == NULL) {
      exit(1);
   }

   /*this probably isn't what someone would want to do, but it initializes the values we need*/
   ctx->debug = hnd->debug;
   ctx->callbacks.auth_fn = cac_GetAuthDataFn;
   

   if(smbc_init_context(ctx) == NULL)
      exit(1);

   cac_SetSmbcContext(hnd, ctx);

   /*still have to call cac_Connect()*/
   if(!cac_Connect(hnd, NULL)) {
      printf("Could not connect to server\n");
      exit(1);
   }

   lop.in.access = MAXIMUM_ALLOWED_ACCESS;
   if(!cac_LsaOpenPolicy(hnd, mem_ctx, &lop))
      printf("Could not open LSA policy. Error: %s\n", nt_errstr(hnd->status));

   printf("Opened LSA policy.\n");

   rc.in.access = MAXIMUM_ALLOWED_ACCESS;
   rc.in.root   = HKEY_LOCAL_MACHINE;
   if(!cac_RegConnect(hnd, mem_ctx, &rc))
      printf("Could not connect to registry. Error: %s\n", nt_errstr(hnd->status));

   printf("Connceted to Registry.\n");

   sod.in.access = MAXIMUM_ALLOWED_ACCESS;

   if(!cac_SamOpenDomain(hnd, mem_ctx, &sod))
      printf("Could not open domain SAM. Error: %s\n", nt_errstr(hnd->status));

   printf("Opened domain.\n");

   if(lop.out.pol)
      cac_LsaClosePolicy(hnd, mem_ctx, lop.out.pol);

   if(rc.out.key)
      cac_RegClose(hnd, mem_ctx, rc.out.key);

   if(sod.out.sam)
      cac_SamClose(hnd, mem_ctx, sod.out.sam);

   if(sod.out.dom_hnd)
      cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);

   cac_FreeHandle(hnd);
   talloc_destroy(mem_ctx);

   return 0;
}
