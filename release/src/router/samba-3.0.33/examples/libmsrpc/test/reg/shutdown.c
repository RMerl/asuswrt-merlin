/*tries to shut down a remote pc*/

#include "libmsrpc.h"
#include "test_util.h"


int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;

   fstring tmp;
   
   mem_ctx = talloc_init("cac_shutdown");

   hnd = cac_NewServerHandle(True);

   cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

   cac_parse_cmd_line(argc, argv, hnd);

   hnd->_internal.srv_level = SRV_WIN_NT4;

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   struct Shutdown s;
   ZERO_STRUCT(s);

   printf("Message: ");
   cactest_readline(stdin, tmp);

   s.in.message = talloc_strdup(mem_ctx, tmp);

   printf("timeout: ");
   scanf("%d", &s.in.timeout);

   printf("Reboot? [y/n]: ");
   cactest_readline(stdin, tmp);

   s.in.reboot = ( tmp[0] == 'y') ? True : False;

   printf("Force? [y/n]: ");
   cactest_readline(stdin, tmp);

   s.in.force = (tmp[0] == 'y') ? True : False;

   if(!cac_Shutdown(hnd, mem_ctx, &s)) {
      fprintf(stderr, "could not shut down server: error %s\n", nt_errstr(hnd->status));
      goto done;
   }

   printf("Server %s is shutting down. Would you like to try to abort? [y/n]: ", hnd->server);
   fscanf(stdin, "%s", tmp);

   if(tmp[0] == 'y') {
      if(!cac_AbortShutdown(hnd, mem_ctx)) {
         fprintf(stderr, "Could not abort shutdown. Error %s\n", nt_errstr(hnd->status));
      }
   }

done:
   cac_FreeHandle(hnd);
   talloc_destroy(mem_ctx);

   return 0;
}
