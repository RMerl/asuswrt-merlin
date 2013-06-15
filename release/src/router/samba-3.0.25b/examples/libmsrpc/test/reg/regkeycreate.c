/*tests creating a registry key*/

#include "libmsrpc.h"

#define MAX_KEYS_PER_ENUM 3

int main() {
   CacServerHandle *hnd = NULL;
    TALLOC_CTX *mem_ctx  = NULL;
 
    fstring key_name;

    fstring key_to_create;

    mem_ctx = talloc_init("regcreatekey");
 
    hnd = cac_NewServerHandle(True);

    printf("Enter server to connect to: ");
    fscanf(stdin, "%s", hnd->server);

    printf("Enter key to open: ");
    fscanf(stdin, "%s", key_name);

    printf("Enter key to create: ");
    fscanf(stdin, "%s", key_to_create);

    if(!cac_Connect(hnd, NULL)) {
       fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
       cac_FreeHandle(hnd);
       exit(-1);
    }

    printf("trying to open key %s...\n", key_name);

    struct RegOpenKey rok;
    ZERO_STRUCT(rok);

    rok.in.parent_key = NULL;
    rok.in.name   = key_name;
    rok.in.access = REG_KEY_ALL;

    if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
       fprintf(stderr, "Could not open key %s\n Error: %s\n", rok.in.name, nt_errstr(hnd->status));
       goto done;
    }

    printf("Creating key %s...\n", key_to_create);

    struct RegCreateKey rck;
    ZERO_STRUCT(rck);

    rck.in.parent_key = rok.out.key;
    rck.in.key_name = talloc_strdup(mem_ctx, key_to_create);
    rck.in.class_name = talloc_strdup(mem_ctx, "");
    rck.in.access = REG_KEY_ALL;

    if(!cac_RegCreateKey(hnd, mem_ctx, &rck)) {
       fprintf(stderr, "Could not create key. Error %s\n", nt_errstr(hnd->status));
       goto done;
    }

    if(!cac_RegClose(hnd, mem_ctx, rck.out.key)) {
       fprintf(stderr, "Could not close key.  Error %s\n", nt_errstr(hnd->status));
       goto done;
    }

    /**enumerate all the subkeys*/
    printf("Enumerating all subkeys:\n");

    struct RegEnumKeys ek;
    ZERO_STRUCT(ek);

    ek.in.key = rok.out.key;
    ek.in.max_keys = 50;

    while(cac_RegEnumKeys(hnd, mem_ctx, &ek)) {
       int j;

       for(j = 0; j < ek.out.num_keys; j++) {
          printf(" Key name: %s\n", ek.out.key_names[j]);
       }
    }

    if(CAC_OP_FAILED(hnd->status)) {
       fprintf(stderr, "Could not enumerate keys: %s\n", nt_errstr(hnd->status));
       goto done;
    }

    printf("deleting key %s\n", key_to_create);

    struct RegDeleteKey rdk;
    ZERO_STRUCT(rdk);

    rdk.in.parent_key = rok.out.key;
    rdk.in.name   = key_to_create;

    if(!cac_RegDeleteKey(hnd, mem_ctx, &rdk)) {
       fprintf(stderr, "Could not delete key.  Error %s\n", nt_errstr(hnd->status));
    }

    printf("closing key %s...\n", key_name);

    if(!cac_RegClose(hnd, mem_ctx, rok.out.key)) {
       fprintf(stderr, "Could not close handle %s\n", nt_errstr(hnd->status));
    }

done:
    cac_FreeHandle(hnd);

    talloc_destroy(mem_ctx);

    return 0;

}
