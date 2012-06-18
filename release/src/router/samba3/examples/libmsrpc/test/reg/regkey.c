/*opens and closes a key*/

#include "libmsrpc.h"

int main() {
   CacServerHandle *hnd = NULL;
    TALLOC_CTX *mem_ctx  = NULL;
 
    fstring key;

    mem_ctx = talloc_init("regkey");
 
    hnd = cac_NewServerHandle(False);

    /*allocate some memory so get_auth_data_fn can do it's magic*/
    hnd->username = SMB_MALLOC_ARRAY(char, sizeof(fstring));
    hnd->domain   = SMB_MALLOC_ARRAY(char, sizeof(fstring));
    hnd->netbios_name = SMB_MALLOC_ARRAY(char, sizeof(fstring));
    hnd->password = SMB_MALLOC_ARRAY(char, sizeof(fstring));
 
    hnd->server   = SMB_MALLOC_ARRAY(char, sizeof(fstring));
 
    printf("Enter server to connect to: ");
    fscanf(stdin, "%s", hnd->server);

    printf("Enter key to open: ");
    fscanf(stdin, "%s", key);

    if(!cac_Connect(hnd, NULL)) {
       fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
       cac_FreeHandle(hnd);
       exit(-1);
    }

    struct RegConnect rc;
    ZERO_STRUCT(rc);

    rc.in.access = REG_KEY_ALL;
    rc.in.root   = HKEY_LOCAL_MACHINE;

    if(!cac_RegConnect(hnd, mem_ctx, &rc)) {
       fprintf(stderr, " Could not connect to registry. %s\n", nt_errstr(hnd->status));
       goto done;
    }

    printf("trying to open key %s...\n", key);

    
    struct RegOpenKey rok;
    ZERO_STRUCT(rok);

    rok.in.parent_key = rc.out.key;
    rok.in.name   = key;
    rok.in.access = REG_KEY_ALL;

    if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
       fprintf(stderr, "Could not open key %s\n Error: %s\n", rok.in.name, nt_errstr(hnd->status));
       goto done;
    }

    if(!cac_RegClose(hnd, mem_ctx, rok.out.key)) {
       fprintf(stderr, "Could not close handle %s\n", nt_errstr(hnd->status));
    }

    if(!cac_RegClose(hnd, mem_ctx, rc.out.key)) {
       fprintf(stderr, " Could not close handle. %s\n", nt_errstr(hnd->status));
    }

done:
    cac_FreeHandle(hnd);

    talloc_destroy(mem_ctx);

    return 0;

}
