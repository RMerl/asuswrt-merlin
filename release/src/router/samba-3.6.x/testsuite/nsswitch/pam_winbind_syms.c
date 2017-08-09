/*
 * Test required functions are exported from the pam_winbind.so library
 */

#include <stdio.h>
#include <dlfcn.h>

/* Symbol list to check */

static char *symlist[] = {
    "pam_sm_acct_mgmt",
    "pam_sm_authenticate",
    "pam_sm_setcred",
    NULL
};

/* Main function */

int main(int argc, char **argv)
{
    void *handle, *sym;
    int i, y;

    /* Open library */

    if (argc != 2) {
        printf("FAIL: usage '%s sharedlibname'\n", argv[0]);
        return 1;
    }

    handle = dlopen(argv[1], RTLD_NOW);

    if (handle == NULL) {
        printf("FAIL: could not dlopen library: %s\n", dlerror());
        return 1;
    }

    /* Read symbols */

    for (i = 0; symlist[i] != NULL; i++) {
        sym = dlsym(handle, symlist[i]);
        if (sym == NULL) {
            printf("FAIL: could not resolve symbol '%s': %s\n",
                   symlist[i], dlerror());
            return 1;
        } else {
            printf("loaded symbol '%s' ok\n", symlist[i]);
        }
    }

    /* Clean up */

    dlclose(handle);
    return 0;
}
