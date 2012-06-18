/*queries trusted domain information*/

#include "libmsrpc.h"
#include "includes.h"

#define MAX_STRING_LEN 50;

void print_info(LSA_TRUSTED_DOMAIN_INFO *info) {
   switch(info->info_class) {
      case CAC_INFO_TRUSTED_DOMAIN_FULL_INFO:
      case CAC_INFO_TRUSTED_DOMAIN_INFO_ALL:
         printf("     Domain Name:     %s\n", unistr2_static(&info->info_ex.domain_name.unistring));
         printf("     Netbios Name:    %s\n", unistr2_static(&info->info_ex.netbios_name.unistring));
         printf("     Domain Sid:      %s\n", sid_string_static(&info->info_ex.sid.sid));
         printf("     Trust direction: %d\n", info->info_ex.trust_direction);
         printf("     Trust Type:      %d\n", info->info_ex.trust_type);
         printf("     Trust attr:      %d\n", info->info_ex.trust_attributes); 
         printf("     Posix Offset:    %d\n", info->posix_offset.posix_offset);
         break;
   }
}

int main() {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;
   POLICY_HND *lsa_pol  = NULL;

   int i;

   mem_ctx = talloc_init("lsatrust");

   hnd = cac_NewServerHandle(False);

   /*malloc some memory so get_auth_data_fn can work*/
   hnd->username     = SMB_MALLOC_ARRAY(char, sizeof(fstring));
   hnd->domain       = SMB_MALLOC_ARRAY(char, sizeof(fstring));
   hnd->netbios_name = SMB_MALLOC_ARRAY(char, sizeof(fstring));
   hnd->password     = SMB_MALLOC_ARRAY(char, sizeof(fstring));

   hnd->server       = SMB_MALLOC_ARRAY(char, sizeof(fstring));


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

   printf("Enumerating Trusted Domains\n");

   struct LsaEnumTrustedDomains etd;
   ZERO_STRUCT(etd);

   etd.in.pol = lsa_pol;

   while(cac_LsaEnumTrustedDomains(hnd, mem_ctx, &etd)) {
      printf(" Enumerated %d domains\n", etd.out.num_domains);

      for(i = 0; i < etd.out.num_domains; i++) {
         printf("   Name: %s\n", etd.out.domain_names[i]);
         printf("   SID:  %s\n", sid_string_static(&etd.out.domain_sids[i]));

         printf("\n   Attempting to open domain...\n");

         struct LsaOpenTrustedDomain otd;
         ZERO_STRUCT(otd);

         otd.in.pol = lsa_pol;
         otd.in.domain_sid = &etd.out.domain_sids[i];
         otd.in.access = SEC_RIGHT_MAXIMUM_ALLOWED;

         /*try to query trusted domain info by name*/
         struct LsaQueryTrustedDomainInfo qtd;
         ZERO_STRUCT(qtd);

         qtd.in.pol = lsa_pol;
         qtd.in.domain_name = etd.out.domain_names[i];

         
         int j;
         for(j = 0; j < 100; j++ ) {
            qtd.in.info_class = j;

            printf("    Querying trustdom by name\n");
            if(!cac_LsaQueryTrustedDomainInfo(hnd, mem_ctx, &qtd)) {
               fprintf(stderr, "    could not query trusted domain info.\n    Error %s\n", nt_errstr(hnd->status));
               continue;
            }
            
            printf("    info_class %d succeeded\n", j); 
            printf("    Query result:\n");    
            printf("     size %d\n", sizeof(*qtd.out.info));
         }

         /*try to query trusted domain info by SID*/
         printf("    Querying trustdom by sid\n");
         qtd.in.domain_sid = &etd.out.domain_sids[i];
         if(!cac_LsaQueryTrustedDomainInfo(hnd, mem_ctx, &qtd)) {
            fprintf(stderr, "    could not query trusted domain info.\n    Error %s\n", nt_errstr(hnd->status));
            continue;
         }

         printf("    Query result:\n");    
/*         print_info(qtd.out.info);*/

         if(CAC_OP_FAILED(hnd->status)) {
            fprintf(stderr, "    Could not enum sids.\n    Error: %s\n", nt_errstr(hnd->status));
            continue;
         }
      }

      printf("\n");
   }

   if(CAC_OP_FAILED(hnd->status)) {
      fprintf(stderr, "Error while enumerating trusted domains.\n Error: %s\n", nt_errstr(hnd->status));
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
