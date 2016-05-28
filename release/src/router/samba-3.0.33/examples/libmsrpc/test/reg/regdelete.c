/*tests deleting a key or value*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;

   fstring tmp;
   char input = 'v';
   
   mem_ctx = talloc_init("regdelete");

   hnd = cac_NewServerHandle(True);

   cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

   cac_parse_cmd_line(argc, argv, hnd);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   printf("enter key to open: \n");
   cactest_readline(stdin, tmp);

   struct RegOpenKey rok;
   ZERO_STRUCT(rok);

   rok.in.name = talloc_strdup(mem_ctx, tmp);
   rok.in.access = REG_KEY_ALL;

   if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
      fprintf(stderr, "Could not open key %s. Error %s\n", rok.in.name, nt_errstr(hnd->status));
      exit(-1);
   }

   printf("getting version (just for testing\n");

   struct RegGetVersion rgv;
   ZERO_STRUCT(rgv);

   rgv.in.key = rok.out.key;

   if(!cac_RegGetVersion(hnd, mem_ctx, &rgv))
      fprintf(stderr, "Could not get version. Error: %s\n", nt_errstr(hnd->status));
   else
      printf("Version: %d\n", rgv.out.version);


   while(input == 'v' || input == 'k') {
      printf("Delete [v]alue [k]ey or [q]uit: ");
      scanf("%c", &input);

      switch(input) {
         case 'v':
            printf("Value to delete: ");
            cactest_readline(stdin, tmp);
            
            struct RegDeleteValue rdv;
            ZERO_STRUCT(rdv);
            
            rdv.in.parent_key = rok.out.key;
            rdv.in.name   = talloc_strdup(mem_ctx, tmp);

            if(!cac_RegDeleteValue(hnd, mem_ctx, &rdv))
               fprintf(stderr, "Could not delete value %s. Error: %s\n", rdv.in.name, nt_errstr(hnd->status));

            break;
         case 'k':
            printf("Key to delete: ");
            cactest_readline(stdin, tmp);
            
            struct RegDeleteKey rdk;
            ZERO_STRUCT(rdk);

            rdk.in.parent_key = rok.out.key;
            rdk.in.name   = talloc_strdup(mem_ctx, tmp);

            printf("delete recursively? [y/n]: ");
            cactest_readline(stdin, tmp);

            rdk.in.recursive = (tmp[0] == 'y') ? True : False;

            if(!cac_RegDeleteKey(hnd, mem_ctx, &rdk))
               fprintf(stderr, "Could not delete key %s. Error %s\n", rdk.in.name, nt_errstr(hnd->status));

            break;
      }
   }
   cac_RegClose(hnd, mem_ctx, rok.out.key);

   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}


