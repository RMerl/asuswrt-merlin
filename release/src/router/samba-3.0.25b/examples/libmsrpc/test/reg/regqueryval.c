/*tests cac_RegQueryValue()*/

#include "libmsrpc.h"
#include "test_util.h"

#define MAX_KEYS_PER_ENUM 3

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
    TALLOC_CTX *mem_ctx  = NULL;
 
    fstring key_name;

    fstring val_name;

    mem_ctx = talloc_init("regqueryval");
 
    hnd = cac_NewServerHandle(True);

    cac_SetAuthDataFn(hnd, cactest_GetAuthDataFn);

    cac_parse_cmd_line(argc, argv, hnd);

    printf("Enter key to open: ");
    fscanf(stdin, "%s", key_name);

    printf("Enter value to query: ");
    fscanf(stdin, "%s", val_name);

    if(!cac_Connect(hnd, NULL)) {
       fprintf(stderr, "Could not connect to server.\n Error: %s.\n errno: %s\n", nt_errstr(hnd->status), strerror(errno));
       cac_FreeHandle(hnd);
       exit(-1);
    }

    printf("trying to open key %s...\n", key_name);

    struct RegOpenKey rok;
    ZERO_STRUCT(rok);

    rok.in.parent_key = NULL;
    rok.in.name       = key_name;
    rok.in.access     = REG_KEY_ALL;

    if(!cac_RegOpenKey(hnd, mem_ctx, &rok)) {
       fprintf(stderr, "Could not open key %s\n Error: %s\n", rok.in.name, nt_errstr(hnd->status));
       goto done;
    }

    struct RegQueryValue rqv;
    ZERO_STRUCT(rqv);

    rqv.in.key = rok.out.key;
    rqv.in.val_name = talloc_strdup(mem_ctx, val_name);

    printf("querying value %s...\n", rqv.in.val_name);
    if(!cac_RegQueryValue(hnd, mem_ctx, &rqv)) {
       fprintf(stderr, "Could not query value. Error: %s\n", nt_errstr(hnd->status));
    }
    else {
       printf("Queried value %s\n", rqv.in.val_name);
       print_value(rqv.out.type, rqv.out.data);
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
