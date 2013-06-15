/*tests enumerating keys or values*/

#include "libmsrpc.h"

#define MAX_KEYS_PER_ENUM 3

int main() {
   CacServerHandle *hnd = NULL;
    TALLOC_CTX *mem_ctx  = NULL;
 
    int num_keys;

    int max_enum;

    int i;

    fstring *key_names;

    mem_ctx = talloc_init("regkeyenum");
 
    hnd = cac_NewServerHandle(True);

    printf("Enter server to connect to: ");
    fscanf(stdin, "%s", hnd->server);

    printf("How many keys do you want to open?: ");
    fscanf(stdin, "%d", &num_keys);

    printf("How many keys per enum?: ");
    fscanf(stdin, "%d", &max_enum);

    key_names = TALLOC_ARRAY(mem_ctx, fstring , num_keys);
    if(!key_names) {
       fprintf(stderr, "No memory\n");
       exit(-1);
    }

    for(i = 0; i < num_keys; i++) {
       printf("Enter key to open: ");
       fscanf(stdin, "%s", key_names[i]);
    }

    if(!cac_Connect(hnd, NULL)) {
       fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
       cac_FreeHandle(hnd);
       exit(-1);
    }

    for(i = 0; i < num_keys; i++) {
       printf("trying to open key %s...\n", key_names[i]);

       struct RegOpenKey rok;
       ZERO_STRUCT(rok);

       rok.in.parent_key = NULL;
       rok.in.name   = key_names[i];
       rok.in.access = REG_KEY_ALL;

       if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
          fprintf(stderr, "Could not open key %s\n Error: %s\n", rok.in.name, nt_errstr(hnd->status));
          continue;
       }

       /**enumerate all the subkeys*/
       printf("Enumerating all subkeys:\n");

       struct RegEnumKeys ek;
       ZERO_STRUCT(ek);

       ek.in.key = rok.out.key;
       ek.in.max_keys = max_enum;

       while(cac_RegEnumKeys(hnd, mem_ctx, &ek)) {
          int j;

          for(j = 0; j < ek.out.num_keys; j++) {
             printf(" Key name: %s\n", ek.out.key_names[j]);
          }
       }

       if(CAC_OP_FAILED(hnd->status)) {
          fprintf(stderr, "Could not enumerate keys: %s\n", nt_errstr(hnd->status));
          continue;
       }

       printf("closing key %s...\n", key_names[i]);

       if(!cac_RegClose(hnd, mem_ctx, rok.out.key)) {
          fprintf(stderr, "Could not close handle %s\n", nt_errstr(hnd->status));
       }
    }

    cac_FreeHandle(hnd);

    talloc_destroy(mem_ctx);

    return 0;

}
