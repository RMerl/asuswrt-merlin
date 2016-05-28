/* connects to an LSA, asks for a list of server names,  prints out their sids, then looks up their names from the sids and prints them out again
 *  if you run as lsaq -p, then it will simulate a partial success for cac_GetNamesFromSids. It will try to lookup the server's local and domain sids
 */


#include "libmsrpc.h"
#include "includes.h"

void fill_conn_info(CacServerHandle *hnd) {
   pstring domain;
   pstring username;
   pstring password;
   pstring server;

   fprintf(stdout, "Enter domain name: ");
   fscanf(stdin, "%s", domain);

   fprintf(stdout, "Enter username: ");
   fscanf(stdin, "%s", username);

   fprintf(stdout, "Enter password (no input masking): ");
   fscanf(stdin, "%s", password);

   fprintf(stdout, "Enter server (ip or name): ");
   fscanf(stdin, "%s", server);

   hnd->domain = SMB_STRDUP(domain);
   hnd->username = SMB_STRDUP(username);
   hnd->password = SMB_STRDUP(password);
   hnd->server = SMB_STRDUP(server);
}

void get_server_names(TALLOC_CTX *mem_ctx, int *num_names, char ***names) {
   int i = 0;
   pstring tmp;
   
   fprintf(stdout, "How many names do you want to lookup?: ");
   fscanf(stdin, "%d", num_names);

   *names = TALLOC_ARRAY(mem_ctx, char *, *num_names);
   if(*names == NULL) {
      fprintf(stderr, "No memory for allocation\n");
      exit(-1);
   }

   for(i = 0; i < *num_names; i++) {
      fprintf(stdout, "Enter name: ");
      fscanf(stdin, "%s", tmp);
      (*names)[i] = talloc_strdup(mem_ctx, tmp);
   }
}

int main(int argc, char **argv) {
   int i;
   int result;
   char **names;
   int num_names;
   int num_sids;
   CacServerHandle *hnd = NULL;
   POLICY_HND *lsa_pol  = NULL;
   TALLOC_CTX *mem_ctx  = NULL;

   DOM_SID *sid_buf     = NULL;

   BOOL sim_partial     = False;

   if(argc > 1 && strcmp(argv[1], "-p") == 0)
      sim_partial = True;

   mem_ctx = talloc_init("lsaq");

   hnd = cac_NewServerHandle(False);

   fill_conn_info(hnd);

   get_server_names(mem_ctx, &num_names, &names);

   /*connect to the PDC and open a LSA handle*/
   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server.\n Error %s.\n", nt_errstr(hnd->status));
      cac_FreeHandle(hnd);
      exit(-1);
   }

   fprintf(stdout, "Connected to server: %s\n", hnd->server);

   struct LsaOpenPolicy lop;
   ZERO_STRUCT(lop);

   lop.in.access = SEC_RIGHT_MAXIMUM_ALLOWED;
   lop.in.security_qos = True;

   if(!cac_LsaOpenPolicy(hnd, mem_ctx, &lop)) {
      fprintf(stderr, "Could not get lsa policy handle.\n Error: %s\n", nt_errstr(hnd->status));
      cac_FreeHandle(hnd);
      exit(-1);
   }

   fprintf(stdout, "Opened Policy Handle\n");

   /*just to make things neater*/
   lsa_pol = lop.out.pol;

   /*fetch the local sid and domain sid for the pdc*/

   struct LsaFetchSid fsop;
   ZERO_STRUCT(fsop);

   fsop.in.pol = lsa_pol;
   fsop.in.info_class = (CAC_LOCAL_INFO|CAC_DOMAIN_INFO);

   fprintf(stdout, "fetching SID info for %s\n", hnd->server);

   result = cac_LsaFetchSid(hnd, mem_ctx, &fsop);
   if(!result) {
      fprintf(stderr, "Could not get sid for server: %s\n. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      cac_FreeHandle(hnd);
      talloc_destroy(mem_ctx);
      exit(-1);
   }

   if(result == CAC_PARTIAL_SUCCESS) {
      fprintf(stdout, "could not retrieve both domain and local information\n");
   }
   

   fprintf(stdout, "Fetched SID info for %s\n", hnd->server);
   if(fsop.out.local_sid != NULL)
      fprintf(stdout, " domain: %s. Local SID: %s\n", fsop.out.local_sid->domain, sid_string_static(&fsop.out.local_sid->sid));

   if(fsop.out.domain_sid != NULL)
      fprintf(stdout, " domain: %s, Domain SID: %s\n", fsop.out.domain_sid->domain, sid_string_static(&fsop.out.domain_sid->sid));

   fprintf(stdout, "\nAttempting to query info policy\n");

   struct LsaQueryInfoPolicy qop;
   ZERO_STRUCT(qop);

   qop.in.pol = lsa_pol;

   if(!cac_LsaQueryInfoPolicy(hnd, mem_ctx, &qop)) {
      fprintf(stderr, "Could not query information policy!.\n Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   fprintf(stdout, "Query result: \n");
   fprintf(stdout, " domain name: %s\n", qop.out.domain_name);
   fprintf(stdout, " dns name:    %s\n", qop.out.dns_name);
   fprintf(stdout, " forest name: %s\n", qop.out.forest_name);
   fprintf(stdout, " domain guid: %s\n", smb_uuid_string_static(*qop.out.domain_guid));
   fprintf(stdout, " domain sid:  %s\n", sid_string_static(qop.out.domain_sid));

   fprintf(stdout, "\nLooking up sids\n");
   
   struct LsaGetSidsFromNames gsop;
   ZERO_STRUCT(gsop);
   
   gsop.in.pol       = lsa_pol;
   gsop.in.num_names = num_names;
   gsop.in.names     = names;

   result = cac_LsaGetSidsFromNames(hnd, mem_ctx, &gsop);

   if(!result) {
      fprintf(stderr, "Could not lookup any sids!\n Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   if(result == CAC_PARTIAL_SUCCESS) {
      fprintf(stdout, "Not all names could be looked up.\nThe following names were not found:\n");
      
      for(i = 0; i < (num_names - gsop.out.num_found); i++) {
         fprintf(stdout, " %s\n", gsop.out.unknown[i]);
      }
      
      fprintf(stdout, "\n");
   }

   /*buffer the sids so we can look them up back to names*/
   num_sids = (sim_partial) ? gsop.out.num_found + 2: gsop.out.num_found;
   sid_buf = TALLOC_ARRAY(mem_ctx, DOM_SID, num_sids);

   fprintf(stdout, "%d names were resolved: \n", gsop.out.num_found);


   i = 0;
   while(i < gsop.out.num_found) {
      fprintf(stdout, " Name: %s\n SID: %s\n\n", gsop.out.sids[i].name, sid_string_static(&gsop.out.sids[i].sid));

      sid_buf[i] = gsop.out.sids[i].sid;

      i++;
   }
   
   /*if we want a partial success to occur below, then add the server's SIDs to the end of the array*/
   if(sim_partial) {
      sid_buf[i] = fsop.out.local_sid->sid;
      sid_buf[i+1] = fsop.out.domain_sid->sid;
   }

   fprintf(stdout, "Looking up Names from SIDs\n");

   struct LsaGetNamesFromSids gnop;
   ZERO_STRUCT(gnop);

   gnop.in.pol       = lsa_pol;
   gnop.in.num_sids  = num_sids;
   gnop.in.sids      = sid_buf;

   result = cac_LsaGetNamesFromSids(hnd, mem_ctx, &gnop);

   if(!result) {
      fprintf(stderr, "Could not lookup any names!.\n Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   if(result == CAC_PARTIAL_SUCCESS) {
      fprintf(stdout, "\nNot all SIDs could be looked up.\n. The following SIDs were not found:\n");

      for(i = 0; i < (num_sids - gnop.out.num_found); i++) {
         fprintf(stdout, "SID: %s\n", sid_string_static(&gnop.out.unknown[i]));
      }

      fprintf(stdout, "\n");
   }

   fprintf(stdout, "%d SIDs were resolved: \n", gnop.out.num_found);
   for(i = 0; i < gnop.out.num_found; i++) {
      fprintf(stdout, " SID: %s\n Name: %s\n", sid_string_static(&gnop.out.sids[i].sid), gsop.out.sids[i].name);
   }
   
done:

   if(!cac_LsaClosePolicy(hnd, mem_ctx, lsa_pol)) {
      fprintf(stderr, "Could not close LSA policy handle.\n Error: %s\n", nt_errstr(hnd->status));
   }
   else {
      fprintf(stdout, "Closed Policy handle.\n");
   }

   cac_FreeHandle(hnd);
   talloc_destroy(mem_ctx);

   return 0;
}
