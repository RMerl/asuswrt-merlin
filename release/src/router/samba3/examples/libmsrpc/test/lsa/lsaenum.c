/*enumerates SIDs*/

#include "libmsrpc.h"
#include "includes.h"

int main(int argc, char **argv) {

   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;

   POLICY_HND *pol      = NULL;

   int i;
   int max_sids;

   mem_ctx = talloc_init("lsaenum");

   hnd = cac_NewServerHandle(True);
   
   printf("Enter server to connect to: ");
   fscanf(stdin, "%s", hnd->server);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
      cac_FreeHandle(hnd);
      exit(-1);
   }

   printf("How many sids do you want to grab at a time? ");
   fscanf(stdin, "%d", &max_sids);

   struct LsaOpenPolicy lop;
   ZERO_STRUCT(lop);

   lop.in.access = SEC_RIGHT_MAXIMUM_ALLOWED;
   lop.in.security_qos = True;


   if(!cac_LsaOpenPolicy(hnd, mem_ctx, &lop)) {
      fprintf(stderr, "Could not open policy handle.\n Error: %s\n", nt_errstr(hnd->status));
      cac_FreeHandle(hnd);
      exit(-1);
   }

   pol = lop.out.pol;


   struct LsaEnumSids esop;
   ZERO_STRUCT(esop);
   esop.in.pol = pol;
   /*grab a couple at a time to demonstrate multiple calls*/
   esop.in.pref_max_sids = max_sids;

   printf("Attempting to fetch SIDs %d at a time\n", esop.in.pref_max_sids);

   while(cac_LsaEnumSids(hnd, mem_ctx, &esop)) {
      
      printf("\nEnumerated %d sids: \n", esop.out.num_sids);
      for(i = 0; i < esop.out.num_sids; i++) {
         printf(" SID: %s\n", sid_string_static(&esop.out.sids[i]));
      }

      printf("Resolving names\n");

      struct LsaGetNamesFromSids gnop;
      ZERO_STRUCT(gnop);

      gnop.in.pol = pol;
      gnop.in.sids = esop.out.sids;
      gnop.in.num_sids = esop.out.num_sids;

      if(!cac_LsaGetNamesFromSids(hnd, mem_ctx, &gnop)) {
         fprintf(stderr, "Could not resolve names.\n Error: %s\n", nt_errstr(hnd->status));
         goto done;
      }

      printf("\nResolved %d names: \n", gnop.out.num_found);
      for(i = 0; i < gnop.out.num_found; i++) {
         printf(" SID: %s\n", sid_string_static(&gnop.out.sids[i].sid));
         printf(" Name: %s\n", gnop.out.sids[i].name);
      }

      /*clean up a little*/
      talloc_free(gnop.out.sids);
   }

done:
   if(!cac_LsaClosePolicy(hnd, mem_ctx, pol)) {
      fprintf(stderr, "Could not close policy handle.\n Error: %s\n", nt_errstr(hnd->status));
   }

   cac_FreeHandle(hnd);
   talloc_destroy(mem_ctx);

   return 0;
}
