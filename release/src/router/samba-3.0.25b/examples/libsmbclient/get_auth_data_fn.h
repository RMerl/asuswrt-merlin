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
    char temp[128];
    
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
}
