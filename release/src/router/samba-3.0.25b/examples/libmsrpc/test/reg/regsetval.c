/*tests cac_RegSetVal()*/

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

   printf("enter key to open: \n");
   scanf("%s", tmp);

   struct RegOpenKey rok;
   ZERO_STRUCT(rok);

   rok.in.name = talloc_strdup(mem_ctx, tmp);
   rok.in.access = REG_KEY_ALL;

   if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
      fprintf(stderr, "Could not open key %s. Error %s\n", rok.in.name, nt_errstr(hnd->status));
      exit(-1);
   }

   struct RegSetValue rsv;
   ZERO_STRUCT(rsv);

   rsv.in.key = rok.out.key;

   cactest_reg_input_val(mem_ctx, &rsv.in.type, &rsv.in.val_name, &rsv.in.value);

   if(!cac_RegSetValue(hnd, mem_ctx, &rsv)) {
      fprintf(stderr, "Could not set value. Error: %s\n", nt_errstr(hnd->status));
   }

   cac_RegClose(hnd, mem_ctx, rok.out.key);

   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}


