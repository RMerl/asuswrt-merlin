/* simple test code, opens and closes an LSA policy handle using libmsrpc, careful.. there's no password input masking*/

#include "includes.h"
#include "libmsrpc.h"

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

int main() {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx;
   struct LsaOpenPolicy op;

   mem_ctx = talloc_init("lsapol");

   
   hnd = cac_NewServerHandle(False);

   /*this line is unnecesary*/
   cac_SetAuthDataFn(hnd, cac_GetAuthDataFn);

   hnd->debug = 0;

   fill_conn_info(hnd);

   /*connect to the server, its name/ip is already in the handle so just pass NULL*/
   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server. \n Error %s\n errno(%d): %s\n", nt_errstr(hnd->status), errno, strerror(errno));
      cac_FreeHandle(hnd);
      exit(-1);
   }
   else {
      fprintf(stdout, "Connected to server\n");
   }

   op.in.access = GENERIC_EXECUTE_ACCESS;
   op.in.security_qos = True;

   /*open the handle*/
   if(!cac_LsaOpenPolicy(hnd, mem_ctx, &op)) {
      fprintf(stderr, "Could not open policy.\n Error: %s.errno: %d.\n", nt_errstr(hnd->status), errno);
      cac_FreeHandle(hnd);
      exit(-1);
   }
   else {
      fprintf(stdout, "Opened Policy handle\n");
   }

   /*close the handle*/
   if(!cac_LsaClosePolicy(hnd, mem_ctx, op.out.pol)) {
      fprintf(stderr, "Could not close policy. Error: %s\n", nt_errstr(hnd->status));
   }
   else {
      fprintf(stdout, "Closed Policy handle\n");
   }

   /*cleanup*/
   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   fprintf(stdout, "Free'd server handle\n");

   return 0;
}

