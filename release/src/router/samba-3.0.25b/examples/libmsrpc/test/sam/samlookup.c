/*lookup names or rids*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;
            
   
   struct SamGetNamesFromRids sgn;
   struct SamGetRidsFromNames sgr;

   fstring tmp;
   fstring input;
   
   int i;

   mem_ctx = talloc_init("cac_samenum");

   hnd = cac_NewServerHandle(True);

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
      printf("get [n]ames or get [r]ids or [q]uit: ");
      cactest_readline(stdin, tmp);

      switch(tmp[0]) {
         case 'n':
            ZERO_STRUCT(sgn);

            sgn.in.dom_hnd = sod.out.dom_hnd;

            printf("How many rids will you enter: ");
            scanf("%d", &sgn.in.num_rids);

            sgn.in.rids = talloc_array(mem_ctx, int, sgn.in.num_rids);
            
            for(i = 0; i < sgn.in.num_rids; i++) {
               printf("  Enter RID %d: 0x", i);
               scanf("%x", &sgn.in.rids[i]);
            }

            printf("Getting names...\n");

            if(!cac_SamGetNamesFromRids(hnd, mem_ctx, &sgn)) {
               fprintf(stderr, "could not lookup names. Error: %s\n", nt_errstr(hnd->status));
               talloc_free(sgn.in.rids);
               continue;
            }

            printf("Found %d names:\n", sgn.out.num_names);

            for(i = 0; i < sgn.out.num_names; i++) {
               printf(" RID:  0x%x ", sgn.out.map[i].rid);

               if(sgn.out.map[i].found) {
                  printf("Name: %s\n", sgn.out.map[i].name);
               }
               else {
                  printf("Unknown RID\n");
               }

            }

            break;

         case 'r':
            ZERO_STRUCT(sgr);

            sgr.in.dom_hnd = sod.out.dom_hnd;

            printf("How many names will you enter: ");
            scanf("%d", &sgr.in.num_names);

            sgr.in.names = talloc_array(mem_ctx, char *, sgr.in.num_names);

            for(i = 0; i < sgr.in.num_names; i++) {
               printf(" Enter name %d: ", (i+1));
               cactest_readline(stdin, input);

               sgr.in.names[i] = talloc_strdup(mem_ctx, input);
            }

            if(!cac_SamGetRidsFromNames(hnd, mem_ctx, &sgr)) {
               fprintf(stderr, "Could not lookup names. Error: %s\n", nt_errstr(hnd->status));
               continue;
            }

            printf("Found %d RIDs:\n", sgr.out.num_rids);

            for(i = 0; i < sgr.out.num_rids; i++) {
               printf(" Name: %s ", sgr.out.map[i].name);

               if(sgr.out.map[i].found) {
                  printf("RID: 0x%x\n", sgr.out.map[i].rid);
               }
               else {
                  printf("Unknown name\n");
               }
            }

            break;
         case 'q':
            printf("\n");
            break;
         default:
            printf("Invalid command!\n");
      }
   }


   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);
   cac_SamClose(hnd, mem_ctx, sod.out.sam);

done:
   talloc_destroy(mem_ctx);
   cac_FreeHandle(hnd);

   return 0;

}

