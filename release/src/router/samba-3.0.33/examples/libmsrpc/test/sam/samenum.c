/*enumerate users/groups/aliases*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;
            
   
   struct SamEnumUsers eu;
   struct SamEnumGroups eg;
   struct SamEnumAliases ea;

   fstring tmp;
   
   int i;

   mem_ctx = talloc_init("cac_samenum");

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
      printf("Enumerate [u]sers, [g]roups or [a]liases or [q]uit: ");
      cactest_readline(stdin, tmp);

      switch(tmp[0]) {
         case 'u':
            ZERO_STRUCT(eu);

            eu.in.dom_hnd = sod.out.dom_hnd;
            
            printf("ACB mask (can be 0): ");
            scanf("%x", &eu.in.acb_mask);

            while(cac_SamEnumUsers(hnd, mem_ctx, &eu)) {
               printf("Enumerated %d users:\n", eu.out.num_users);
               for(i = 0; i < eu.out.num_users; i++) {
                  printf(" Name: %s\n", eu.out.names[i]);
                  printf(" RID:  %d\n", eu.out.rids[i]);
               }
            }

            if(CAC_OP_FAILED(hnd->status)) {
               printf("Could not enumerate users. Error: %s\n", nt_errstr(hnd->status));
            }
            break;
         case 'g':
            ZERO_STRUCT(eg);
            eg.in.dom_hnd = sod.out.dom_hnd;

            printf("Enumerating groups...\n");
            while(cac_SamEnumGroups(hnd, mem_ctx, &eg)) {
               printf("Enumerated %d groups:\n", eg.out.num_groups);
               for(i = 0; i < eg.out.num_groups; i++) {
                  printf("RID:  %d\n", eg.out.rids[i]);
                  printf("Name: %s\n", eg.out.names[i]);
                  printf("Desc: %s\n", eg.out.descriptions[i]);
               }
            }

            if(CAC_OP_FAILED(hnd->status)) {
               printf("Could not enumerate Groups. Error: %s\n", nt_errstr(hnd->status));
            }
            break;
         case 'a':
            ZERO_STRUCT(ea);
            ea.in.dom_hnd = sod.out.dom_hnd;

            printf("Enumerating Aliases...\n");
            while(cac_SamEnumAliases(hnd, mem_ctx, &ea)) {
               printf("Enumerated %d aliases:\n", ea.out.num_aliases);

               for(i = 0; i < ea.out.num_aliases; i++) {
                  printf("RID:  %d\n", ea.out.rids[i]);
                  printf("Name: %s\n", ea.out.names[i]);
                  printf("Desc: %s\n", ea.out.descriptions[i]);
               }
            }
            if(CAC_OP_FAILED(hnd->status)) {
               printf("Could not enumerate Aliases. Error: %s\n", nt_errstr(hnd->status));
            }
            break;
      }
   }

   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);
   cac_SamClose(hnd, mem_ctx, sod.out.sam);

done:
   talloc_destroy(mem_ctx);
   cac_FreeHandle(hnd);

   return 0;

}

