/*gets domain info and prints it out*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;

   mem_ctx = talloc_init("cac_dominfo");

   hnd = cac_NewServerHandle(True);

   cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

   cac_parse_cmd_line(argc, argv, hnd);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   struct SamOpenDomain sod;
   ZERO_STRUCT(sod);

   sod.in.access = MAXIMUM_ALLOWED_ACCESS; 

   if(!cac_SamOpenDomain(hnd, mem_ctx, &sod)) {
      fprintf(stderr, "Could not open domain. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   struct SamGetDomainInfo gdi;
   ZERO_STRUCT(gdi);

   gdi.in.dom_hnd = sod.out.dom_hnd;

   if(!cac_SamGetDomainInfo(hnd, mem_ctx, &gdi)) {
      fprintf(stderr, "Could not get domain info. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   printf("Got domain info:\n");
   print_cac_domain_info(gdi.out.info);

done:
   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);

   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);
   
   return 0;
}

