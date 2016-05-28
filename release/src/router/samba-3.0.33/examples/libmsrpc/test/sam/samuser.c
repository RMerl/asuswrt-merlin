/*Some user management stuff*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;
            
   
   struct SamOpenUser    ou;
   struct SamEnumUsers   eu;
   struct SamCreateUser  cu;
   struct SamGetUserInfo gi;
   struct SamSetUserInfo si;
   struct SamRenameUser  ru;
   struct SamSetPassword sp;

   POLICY_HND *user_hnd = NULL;

   fstring tmp;
   fstring input;

   char *pass1 = NULL;
   char *pass2 = NULL;
   
   int i;

   mem_ctx = talloc_init("cac_samgroup");

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

   tmp[0] = 0x00;
   while(tmp[0] != 'q') {
      printf("\n");
      printf("[l]ist users\n");
      printf("[c]reate user\n");
      printf("[o]pen user\n");
      printf("[d]elete user\n");
      printf("[g]et user info\n");
      printf("[e]dit user info\n");
      printf("[r]ename user\n");
      printf("reset [p]assword\n");
      printf("[n] close user\n");

      printf("[q]uit\n\n");
      printf("Enter option: ");
      cactest_readline(stdin, tmp);

      printf("\n");

      switch(tmp[0]) {
         case 'c': /*create user*/
            if(user_hnd != NULL) {
               /*then we have an open handle.. close it*/
               cac_SamClose(hnd, mem_ctx, user_hnd);
               user_hnd = NULL;
            }

            printf("Enter user name: ");
            cactest_readline(stdin, input);

            ZERO_STRUCT(cu);

            cu.in.name      = talloc_strdup(mem_ctx, input);
            cu.in.dom_hnd   = sod.out.dom_hnd;
            cu.in.acb_mask  = ACB_NORMAL;

            if(!cac_SamCreateUser(hnd, mem_ctx, &cu)) {
               printf("Could not create user. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Created user %s with RID 0x%x\n", cu.in.name, cu.out.rid);
               user_hnd = cu.out.user_hnd;
            }

            break;

         case 'o': /*open group*/
            if(user_hnd != NULL) {
               /*then we have an open handle.. close it*/
               cac_SamClose(hnd, mem_ctx, user_hnd);
               user_hnd = NULL;
            }

            ZERO_STRUCT(ou);

            ou.in.dom_hnd = sod.out.dom_hnd;
            ou.in.access = MAXIMUM_ALLOWED_ACCESS;

            printf("Enter RID: 0x");
            scanf("%x", &ou.in.rid);

            if(!cac_SamOpenUser(hnd, mem_ctx, &ou)) {
               fprintf(stderr, "Could not open user. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Opened user\n");
               user_hnd = ou.out.user_hnd;
            }

            break;

         case 'l': /*list users*/
            ZERO_STRUCT(eu);
            eu.in.dom_hnd = sod.out.dom_hnd;

            while(cac_SamEnumUsers(hnd, mem_ctx, &eu)) {
               for(i = 0; i < eu.out.num_users; i++) {
                  printf("RID: 0x%x Name: %s\n", eu.out.rids[i], eu.out.names[i]);
               }
            }

            if(CAC_OP_FAILED(hnd->status)) {
               printf("Could not enumerate Users. Error: %s\n", nt_errstr(hnd->status));
            }

            break;
            
            break;

         case 'd': /*delete group*/
            if(!user_hnd) {
               printf("Must open group first!\n");
               break;
            }

            if(!cac_SamDeleteGroup(hnd, mem_ctx, user_hnd)) {
               fprintf(stderr, "Could not delete group. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Deleted group.\n");
               user_hnd = NULL;
            }
            break;

         
         case 'n':
            if(!user_hnd) {
               printf("Must open user first!\n");
               break;
            }

            if(!cac_SamClose(hnd, mem_ctx, user_hnd)) {
               printf("Could not user group\n");
               break;
            }

            user_hnd = NULL;
            break;

         case 'g': /*get user info*/
            if(!user_hnd) {
               printf("Must open user first!\n");
               break;
            }

            ZERO_STRUCT(gi);
            gi.in.user_hnd = ou.out.user_hnd;

            if(!cac_SamGetUserInfo(hnd, mem_ctx, &gi)) {
               printf("Could not get user info. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Retrieved User information:\n");
               print_cac_user_info(gi.out.info);
            } 

            break;

         case 'e': /*edit user info*/
            if(!user_hnd) {
               printf("Must Open user first!\n");
               break;
            }

            ZERO_STRUCT(gi);
            gi.in.user_hnd = ou.out.user_hnd;
            if(!cac_SamGetUserInfo(hnd, mem_ctx, &gi)) {
               printf("Could not get user info. Error: %s\n", nt_errstr(hnd->status));
               break;
            }
            
            edit_cac_user_info(mem_ctx, gi.out.info);

            printf("setting following info:\n");
            print_cac_user_info(gi.out.info);

            ZERO_STRUCT(si);

            si.in.user_hnd = user_hnd;
            si.in.info     = gi.out.info;

            if(!cac_SamSetUserInfo(hnd, mem_ctx, &si)) {
               printf("Could not set user info. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Done.\n");
            }

            break;

         case 'r': /*rename user*/
            if(!user_hnd) {
               printf("Must open user first!\n");
               break;
            }

            ZERO_STRUCT(ru);

            printf("Enter new username: ");
            cactest_readline(stdin, tmp);

            ru.in.user_hnd = user_hnd;
            ru.in.new_name = talloc_strdup(mem_ctx, tmp);

            if(!cac_SamRenameUser(hnd, mem_ctx, &ru)) {
               printf("Could not rename user. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Renamed user\n");
            }

            break;

         case 'p': /*reset password*/

            if(!user_hnd) {
               printf("Must open user first!\n");
               break;
            }

            do {
               if(pass1 && pass2) {
                  printf("Passwords do not match. Please try again\n");
               }

               pass1 = getpass("Enter new password: ");
               pass2 = getpass("Re-enter new password: ");
            } while(strncmp(pass1, pass2, MAX_PASS_LEN));

            ZERO_STRUCT(sp);
            sp.in.user_hnd = user_hnd;
            sp.in.password = talloc_strdup(mem_ctx, pass1);

            if(!cac_SamSetPassword(hnd, mem_ctx, &sp)) {
               printf("Could not set password. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Done.\n");
            }

            break;

         case 'q':
            break;

         default:
            printf("Invalid command\n");
      }
   }

   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);

   if(user_hnd)
      cac_SamClose(hnd, mem_ctx, user_hnd);

done:
   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}

