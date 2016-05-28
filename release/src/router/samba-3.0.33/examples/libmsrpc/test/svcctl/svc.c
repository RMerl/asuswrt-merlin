/*Tests all of the svcctl calls (at least at time of writing)*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;
            

   struct SvcOpenScm      sos;
   struct SvcEnumServices es;
   struct SvcOpenService  os;
   struct SvcGetStatus    gs;
   struct SvcStartService start;
   struct SvcStopService  stop;
   struct SvcPauseService pause;
   struct SvcContinueService res;
   struct SvcGetDisplayName gdn;
   struct SvcGetServiceConfig sgc;
  
   POLICY_HND *svc_hnd = NULL;

   fstring tmp;
   fstring input;
   
   int i;

   mem_ctx = talloc_init("cac_samgroup");

   hnd = cac_NewServerHandle(True);

   cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

   cac_parse_cmd_line(argc, argv, hnd);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   /*open a handle to the scm*/
   ZERO_STRUCT(sos);

   sos.in.access = SC_MANAGER_ALL_ACCESS;

   if(!cac_SvcOpenScm(hnd, mem_ctx, &sos)) {
      fprintf(stderr, "Could not open SCM. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   printf("Opened SCM\n");

   tmp[0] = 0x00;
   while(tmp[0] != 'q') {
      printf("\n");
      printf("[e] Enum Services\n");
      printf("[o] Open Service\n");
      printf("[x] Close Service\n");
      printf("[g] Get service status\n");
      printf("[s] Start service\n");
      printf("[t] Stop service\n");
      printf("[p] Pause service\n");
      printf("[r] Resume service\n");
      printf("[c] Get service config\n");

      printf("[d] Get display name\n");

      printf("[q]uit\n\n");
      printf("Enter option: ");
      cactest_readline(stdin, tmp);

      printf("\n");

      switch(tmp[0]) {
         case 'e': /*enum services*/
            ZERO_STRUCT(es);
            es.in.scm_hnd = sos.out.scm_hnd;

            if(!cac_SvcEnumServices(hnd, mem_ctx, &es)) {
               printf("Could not enumerate services. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            for(i = 0; i < es.out.num_services; i++) {
               print_cac_service(es.out.services[i]);
            }
            printf("Enumerated %d services:\n", es.out.num_services);

            break;

         case 'o': /*Open service*/
            ZERO_STRUCT(os);

            printf("Enter service name: ");
            cactest_readline(stdin, tmp);

            os.in.name = talloc_strdup(mem_ctx, tmp);
            os.in.scm_hnd = sos.out.scm_hnd;
            os.in.access = SERVICE_ALL_ACCESS;

            if(!cac_SvcOpenService(hnd, mem_ctx, &os)) {
               printf("Could not open service. Error: %s\n", nt_errstr(hnd->status));
               break;
            }
            
            printf("Opened service.\n");
            svc_hnd = os.out.svc_hnd;

            break;
         case 'x': /*close service*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            cac_SvcClose(hnd, mem_ctx, svc_hnd);
            svc_hnd = NULL;
            break;
         case 'g': /*get svc status*/
            
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(gs);

            gs.in.svc_hnd = svc_hnd;

            if(!cac_SvcGetStatus(hnd, mem_ctx, &gs)) {
               printf("Could not get status. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            print_service_status(gs.out.status);
            break;
         case 's': /*start service*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(start);

            start.in.svc_hnd = svc_hnd;

            printf("Enter number of parameters: ");
            scanf("%d", &start.in.num_parms);

            start.in.parms = talloc_array(mem_ctx, char *, start.in.num_parms);

            for(i = 0; i < start.in.num_parms; i++) {
               printf("Parm %d: ", i);
               cactest_readline(stdin, tmp);
               start.in.parms[i] = talloc_strdup(mem_ctx, tmp);
            }

            printf("Timeout (seconds): ");
            scanf("%d", &start.in.timeout);

            if(!cac_SvcStartService(hnd, mem_ctx, &start)) {
               printf("Could not start service. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Started service.\n");
            }

            break;
         case 't': /*stop service*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(stop);
            stop.in.svc_hnd = svc_hnd;

            printf("Timeout (seconds): ");
            scanf("%d", &stop.in.timeout);

            if(!cac_SvcStopService(hnd, mem_ctx, &stop)) {
               if(CAC_OP_FAILED(hnd->status)) {
                  printf("Error occured: %s\n", nt_errstr(hnd->status));
               }
               else {
                  printf("Service was not stopped within %d seconds.\n", stop.in.timeout);
                  print_service_status(stop.out.status);
               }
            }
            else {
               printf("Done.\n");
               print_service_status(stop.out.status);
            }
            break;
         case 'd': /*get display name*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(gdn);
            gdn.in.svc_hnd = svc_hnd;

            if(!cac_SvcGetDisplayName(hnd, mem_ctx, &gdn)) {
               printf("Could not get display name. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("\tDisplay Name: %s\n", gdn.out.display_name);
            }
            break;

         case 'p': /*pause service*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(pause);
            pause.in.svc_hnd = svc_hnd;

            printf("Timeout (seconds): ");
            scanf("%d", &pause.in.timeout);

            if(!cac_SvcPauseService(hnd, mem_ctx, &pause)) {
               if(CAC_OP_FAILED(hnd->status)) {
                  printf("Error occured: %s\n", nt_errstr(hnd->status));
               }
               else {
                  printf("Service was not paused within %d seconds.\n", pause.in.timeout);
                  print_service_status(pause.out.status);
               }
            }
            else {
               printf("Done.\n");
               print_service_status(pause.out.status);
            }

            break;
            
         case 'r': /*resume service*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(res);
            res.in.svc_hnd = svc_hnd;

            printf("Timeout (seconds): ");
            scanf("%d", &res.in.timeout);

            if(!cac_SvcContinueService(hnd, mem_ctx, &res)) {
               if(CAC_OP_FAILED(hnd->status)) {
                  printf("Error occured: %s\n", nt_errstr(hnd->status));
               }
               else {
                  printf("Service was not resumed within %d seconds.\n", res.in.timeout);
                  print_service_status(res.out.status);
               }
            }
            else {
               printf("Done.\n");
               print_service_status(res.out.status);
            }

            break;

         case 'c': /*get service config*/
            if(!svc_hnd) {
               printf("Must open service first!\n");
               break;
            }

            ZERO_STRUCT(sgc);

            sgc.in.svc_hnd = svc_hnd;

            if(!cac_SvcGetServiceConfig(hnd, mem_ctx, &sgc)) {
               printf("Could not get service config. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               print_service_config(&sgc.out.config);
            }
            break;

         case 'q': /*quit*/
            break;
         default:
            printf("Invalid command\n");
      }
   }

   cac_SvcClose(hnd, mem_ctx, sos.out.scm_hnd);

   done:
   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}

