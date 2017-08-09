#include <stdlib.h>

static void
get_auth_data_fn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword)
{
    char            temp[128];
    char            server[256] = { '\0' };
    char            share[256] = { '\0' };
    char            workgroup[256] = { '\0' };
    char            username[256] = { '\0' };
    char            password[256] = { '\0' };

    static int krb5_set = 1;

    if (strcmp(server, pServer) == 0 &&
        strcmp(share, pShare) == 0 &&
        *workgroup != '\0' &&
        *username != '\0')
    {
        strncpy(pWorkgroup, workgroup, maxLenWorkgroup - 1);
        strncpy(pUsername, username, maxLenUsername - 1);
        strncpy(pPassword, password, maxLenPassword - 1);
        return;
    }

    if (krb5_set && getenv("KRB5CCNAME")) {
      krb5_set = 0;
      return;
    }

    fprintf(stdout, "Workgroup: [%s] ", pWorkgroup);
    fgets(temp, sizeof(temp), stdin);
    
    if (temp[strlen(temp) - 1] == '\n') /* A new line? */
    {
        temp[strlen(temp) - 1] = '\0';
    }
    
    if (temp[0] != '\0')
    {
        strncpy(pWorkgroup, temp, maxLenWorkgroup - 1);
    }
    
    fprintf(stdout, "Username: [%s] ", pUsername);
    fgets(temp, sizeof(temp), stdin);
    
    if (temp[strlen(temp) - 1] == '\n') /* A new line? */
    {
        temp[strlen(temp) - 1] = '\0';
    }
    
    if (temp[0] != '\0')
    {
        strncpy(pUsername, temp, maxLenUsername - 1);
    }
    
    fprintf(stdout, "Password: ");
    fgets(temp, sizeof(temp), stdin);
    
    if (temp[strlen(temp) - 1] == '\n') /* A new line? */
    {
        temp[strlen(temp) - 1] = '\0';
    }
    
    if (temp[0] != '\0')
    {
        strncpy(pPassword, temp, maxLenPassword - 1);
    }

    strncpy(workgroup, pWorkgroup, sizeof(workgroup) - 1);
    strncpy(username, pUsername, sizeof(username) - 1);
    strncpy(password, pPassword, sizeof(password) - 1);

    krb5_set = 1;
}
