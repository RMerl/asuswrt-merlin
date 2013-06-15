/*tries to set privileges for an account*/

#include "libmsrpc.h"
#include "test_util.h"

#define BIGGEST_UINT32 0xffffffff

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;
            
   struct LsaOpenPolicy lop;
   struct LsaEnumPrivileges ep;
   struct LsaEnumAccountRights ar;
   struct LsaAddPrivileges ap;
   
   fstring tmp;

   uint32 i = 0;
   
   mem_ctx = talloc_init("lsapriv");

   hnd = cac_NewServerHandle(True);

   cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

   cac_parse_cmd_line(argc, argv, hnd);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   ZERO_STRUCT(lop);

   lop.in.access = SEC_RIGHT_MAXIMUM_ALLOWED;

   if(!cac_LsaOpenPolicy(hnd, mem_ctx, &lop)) {
      fprintf(stderr, "Could not open LSA policy. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   /*first enumerate possible privileges*/
   ZERO_STRUCT(ep);

   ep.in.pol = lop.out.pol;
   ep.in.pref_max_privs = BIGGEST_UINT32;

   printf("Enumerating supported privileges:\n");
   while(cac_LsaEnumPrivileges(hnd, mem_ctx, &ep)) {
      for(i = 0; i < ep.out.num_privs; i++) {
         printf("\t%s\n", ep.out.priv_names[i]);
      }
   }

   if(CAC_OP_FAILED(hnd->status)) {
      fprintf(stderr, "Could not enumerate privileges. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   printf("Enter account name: ");
   cactest_readline(stdin, tmp);

   ZERO_STRUCT(ar);

   ar.in.pol = lop.out.pol;
   ar.in.name = talloc_strdup(mem_ctx, tmp);
   
   printf("Enumerating privileges for %s:\n", ar.in.name);
   if(!cac_LsaEnumAccountRights(hnd, mem_ctx, &ar)) {
      fprintf(stderr, "Could not enumerate privileges. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   printf("Enumerated %d privileges:\n", ar.out.num_privs);

   for(i = 0; i < ar.out.num_privs; i++) 
      printf("\t%s\n", ar.out.priv_names[i]);

   ZERO_STRUCT(ap);

   ap.in.pol = lop.out.pol;
   ap.in.name = ar.in.name;

   printf("How many privileges will you set: ");
   scanf("%d", &ap.in.num_privs);

   ap.in.priv_names = talloc_array(mem_ctx, char *, ap.in.num_privs);
   if(!ap.in.priv_names) {
      fprintf(stderr, "No memory\n");
      goto done;
   }

   for(i = 0; i < ap.in.num_privs; i++) {
      printf("Enter priv %d: ", i);
      cactest_readline(stdin, tmp);

      ap.in.priv_names[i] = talloc_strdup(mem_ctx, tmp);
   }

   if(!cac_LsaSetPrivileges(hnd, mem_ctx, &ap)) {
      fprintf(stderr, "Could not set privileges. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

done:
   talloc_destroy(mem_ctx);
   cac_FreeHandle(hnd);

   return 0;

}

