/*tests enumerating registry values*/

#include "libmsrpc.h"
#include "test_util.h"

#define MAX_KEYS_PER_ENUM 3


int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
    TALLOC_CTX *mem_ctx  = NULL;
 
    int num_keys;

    int max_enum;

    fstring *key_names;

    int i;
 
    mem_ctx = talloc_init("regvalenum");
 
    hnd = cac_NewServerHandle(True);

    cac_parse_cmd_line(argc, argv, hnd);

    cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);
    
    if(!cac_Connect(hnd, NULL)) {
       fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
       cac_FreeHandle(hnd);
       exit(-1);
    }

    printf("How many keys do you want to open?: ");
    fscanf(stdin, "%d", &num_keys);

    printf("How many values per enum?: ");
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
       printf("Enumerating all values:\n");

       struct RegEnumValues rev;
       ZERO_STRUCT(rev);

       rev.in.key = rok.out.key;
       rev.in.max_values = max_enum;

       while(cac_RegEnumValues(hnd, mem_ctx, &rev)) {
          int j;

          for(j = 0; j < rev.out.num_values; j++) {
             printf(" Value name: %s\n", rev.out.value_names[j]);
             print_value(rev.out.types[j], rev.out.values[j]);
          }
       }

       if(CAC_OP_FAILED(hnd->status)) {
          fprintf(stderr, "Could not enumerate values: %s\n", nt_errstr(hnd->status));
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
