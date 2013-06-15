/*disable a user*/
#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;

   struct SamOpenUser ou;

   fstring tmp;

   mem_ctx = talloc_init("cac_disable");

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

   ZERO_STRUCT(ou);
   printf("Enter username: ");
   cactest_readline(stdin, tmp);

   ou.in.name = talloc_strdup(mem_ctx, tmp);
   ou.in.access = MAXIMUM_ALLOWED_ACCESS;
   ou.in.dom_hnd = sod.out.dom_hnd;

   if(!cac_SamOpenUser(hnd, mem_ctx, &ou)) {
      fprintf(stderr, "Could not open user. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   /*enable the user*/
   if(!cac_SamDisableUser(hnd, mem_ctx, ou.out.user_hnd)) {
      fprintf(stderr, "Could not disable user: %s\n", nt_errstr(hnd->status));
   }

done:
   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);

   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);
   
   return 0;
}

