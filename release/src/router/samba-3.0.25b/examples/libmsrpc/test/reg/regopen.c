/*opens and closes a registry handle*/

#include "libmsrpc.h"

int main() {
   CacServerHandle *hnd = NULL;
    TALLOC_CTX *mem_ctx  = NULL;
 
    POLICY_HND **keys      = NULL;

    char roots[4][50] = { {CAC_HKCR}, {CAC_HKLM}, {CAC_HKU}, {CAC_HKPD} };

    int i;
 

    mem_ctx = talloc_init("regopen");
 
    hnd = cac_NewServerHandle(True);

    keys = TALLOC_ARRAY(mem_ctx, POLICY_HND *, 4);
    
    printf("Enter server to connect to: ");
    fscanf(stdin, "%s", hnd->server);

    if(!cac_Connect(hnd, NULL)) {
       fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
       cac_FreeHandle(hnd);
       exit(-1);
    }

    struct RegConnect rc;
    ZERO_STRUCT(rc);

    rc.in.access = SEC_RIGHT_MAXIMUM_ALLOWED;

    for(i = 0; i < 4; i++) {
       printf("opening: %s\n", roots[i]);

       rc.in.root = roots[i];

       if(!cac_RegConnect(hnd, mem_ctx, &rc)) {
          fprintf(stderr, " Could not connect to registry. %s\n", nt_errstr(hnd->status));
          continue;
       }

       keys[i] = rc.out.key;
    }

    for(i = 3; i >= 0; i--) {
       if(keys[i] == NULL)
          continue;

       printf("closing: %s\n", roots[i]);

       if(!cac_RegClose(hnd, mem_ctx, keys[i])) {
          fprintf(stderr, " Could not close handle. %s\n", nt_errstr(hnd->status));
       }
    }

    cac_FreeHandle(hnd);

    talloc_destroy(mem_ctx);

    return 0;

}
