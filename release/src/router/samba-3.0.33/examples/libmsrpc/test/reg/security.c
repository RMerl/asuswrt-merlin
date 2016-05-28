/*tests cac_RegSetKeySecurity()*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;

   fstring tmp;
   
   mem_ctx = talloc_init("regsetval");

   hnd = cac_NewServerHandle(True);

   cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

   cac_parse_cmd_line(argc, argv, hnd);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   struct RegOpenKey rok;
   ZERO_STRUCT(rok);

   printf("enter key to query: ");
   cactest_readline(stdin, tmp);

   rok.in.name = talloc_strdup(mem_ctx, tmp);
   rok.in.access = REG_KEY_ALL;

   if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
      fprintf(stderr, "Could not open key %s. Error %s\n", rok.in.name, nt_errstr(hnd->status));
      exit(-1);
   }

   struct RegGetKeySecurity rks;
   ZERO_STRUCT(rks);

   rks.in.key = rok.out.key;
   rks.in.info_type = ALL_SECURITY_INFORMATION;

   if(!cac_RegGetKeySecurity(hnd, mem_ctx, &rks)) {
      fprintf(stderr, "Could not query security for %s.  Error: %s\n", rok.in.name, nt_errstr(hnd->status));
      goto done;
   }

   printf("resetting key security...\n");

   struct RegSetKeySecurity rss;
   ZERO_STRUCT(rss);

   rss.in.key = rok.out.key;
   rss.in.info_type = ALL_SECURITY_INFORMATION;
   rss.in.size = rks.out.size;
   rss.in.descriptor = rks.out.descriptor;

   if(!cac_RegSetKeySecurity(hnd, mem_ctx, &rss)) {
      fprintf(stderr, "Could not set security. Error %s\n", nt_errstr(hnd->status));
   }

done:
   cac_RegClose(hnd, mem_ctx, rok.out.key);
   
   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}


