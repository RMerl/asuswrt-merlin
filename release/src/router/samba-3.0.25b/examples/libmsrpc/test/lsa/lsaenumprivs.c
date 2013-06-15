/*enumerates privileges*/

#include "libmsrpc.h"
#include "includes.h"

#define MAX_STRING_LEN 50;

int main() {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;
   POLICY_HND *lsa_pol  = NULL;

   int i;

   mem_ctx = talloc_init("lsatrust");

   hnd = cac_NewServerHandle(True);

   printf("Server: ");
   fscanf(stdin, "%s", hnd->server);

   printf("Connecting to server....\n");

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server.\n Error: %s\n errno %s\n", nt_errstr(hnd->status), strerror(errno));
      cac_FreeHandle(hnd);
      exit(-1);
   }

   printf("Connected to server\n");

   struct LsaOpenPolicy lop;
   ZERO_STRUCT(lop);

   lop.in.access = SEC_RIGHT_MAXIMUM_ALLOWED;
   lop.in.security_qos = True;


   if(!cac_LsaOpenPolicy(hnd, mem_ctx, &lop)) {
      fprintf(stderr, "Could not open policy handle.\n Error: %s\n", nt_errstr(hnd->status));
      cac_FreeHandle(hnd);
      exit(-1);
   }

   lsa_pol = lop.out.pol;

   printf("Enumerating Privileges\n");

   struct LsaEnumPrivileges ep;
   ZERO_STRUCT(ep);

   ep.in.pol = lsa_pol;
   ep.in.pref_max_privs = 50;

   while(cac_LsaEnumPrivileges(hnd, mem_ctx, &ep)) {
      printf(" Enumerated %d privileges\n", ep.out.num_privs);

      for(i = 0; i < ep.out.num_privs; i++) {
         printf("\"%s\"\n", ep.out.priv_names[i]);
      }

      printf("\n");
   }

   if(CAC_OP_FAILED(hnd->status)) {
      fprintf(stderr, "Error while enumerating privileges.\n Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

done:
   if(!cac_LsaClosePolicy(hnd, mem_ctx, lsa_pol)) {
      fprintf(stderr, "Could not close policy handle.\n Error: %s\n", nt_errstr(hnd->status));
   }

   cac_FreeHandle(hnd);
   talloc_destroy(mem_ctx);

   return 0;
}
