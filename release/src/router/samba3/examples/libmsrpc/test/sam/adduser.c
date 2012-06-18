/*add's a user to a domain*/
#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;

   fstring tmp;

   struct SamOpenUser ou;

   POLICY_HND *user_hnd = NULL;

   mem_ctx = talloc_init("cac_adduser");

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

   struct SamCreateUser cdu;
   ZERO_STRUCT(cdu);

   printf("Enter account name: ");
   cactest_readline(stdin, tmp);

   cdu.in.dom_hnd = sod.out.dom_hnd;
   cdu.in.name = talloc_strdup(mem_ctx, tmp);
   cdu.in.acb_mask  = ACB_NORMAL;

   if(!cac_SamCreateUser(hnd, mem_ctx, &cdu)) {
      fprintf(stderr, "Could not create user %s. Error: %s\n", cdu.in.name, nt_errstr(hnd->status));
   }

   printf("would you like to delete this user? [y/n]: ");
   cactest_readline(stdin, tmp);

   if(tmp[0] == 'y') {

      if(!cdu.out.user_hnd) {
         ZERO_STRUCT(ou);
         ou.in.dom_hnd = sod.out.dom_hnd;
         ou.in.access  = MAXIMUM_ALLOWED_ACCESS;
         ou.in.name    = talloc_strdup(mem_ctx, cdu.in.name);

         if(!cac_SamOpenUser(hnd, mem_ctx, &ou)) {
            fprintf(stderr, "Could not open user for deletion. Error: %s\n", nt_errstr(hnd->status));
         }

         user_hnd = ou.out.user_hnd;
      }

      else {
         user_hnd = cdu.out.user_hnd;
      }

      if(!cac_SamDeleteUser(hnd, mem_ctx, user_hnd))
         fprintf(stderr, "Could not delete user. Error: %s\n", nt_errstr(hnd->status));
   }
   else {
      printf("Nope..ok\n");
   }

   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);
   cac_SamClose(hnd, mem_ctx, sod.out.sam);

done:
   talloc_destroy(mem_ctx);

   cac_FreeHandle(hnd);
   
   return 0;
}

/*TODO: add a function that will create a user and set userinfo and set the password*/
