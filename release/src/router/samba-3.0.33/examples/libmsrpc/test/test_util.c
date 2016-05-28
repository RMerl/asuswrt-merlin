/*some utility functions for the registry tests*/

#include "libmsrpc.h"
#include "test_util.h"


void cactest_print_usage(char **argv) {
   printf("Usage:\n");
   printf("  %s server [-U username] [-W domain] [-P passwprd] [-N netbios_name]\n", argv[0]);
}

/*allocates memory for auth info and parses domain/user/server out of command line*/
void cac_parse_cmd_line(int argc, char **argv, CacServerHandle *hnd) {
   int i = 0;

   ZERO_STRUCTP(hnd->username);
   ZERO_STRUCTP(hnd->domain);
   ZERO_STRUCTP(hnd->netbios_name);
   ZERO_STRUCTP(hnd->password);

   for(i = 1; i < argc; i++) {
      if( strncmp(argv[i], "-U", sizeof(fstring)) == 0) {
         strncpy(hnd->username, argv[i+1], sizeof(fstring));
         i++;
      }

      else if(strncmp(argv[i], "-W", sizeof(fstring)) == 0) {
         strncpy(hnd->domain, argv[i+1], sizeof(fstring));
         i++;

      }

      else if(strncmp(argv[i], "-P", sizeof(fstring)) == 0) {
         strncpy(hnd->password, argv[i+1], sizeof(fstring));
         i++;

      }

      else if(strncmp(argv[i], "-N", sizeof(fstring)) == 0) {
         strncpy(hnd->netbios_name, argv[i+1], sizeof(fstring));
         i++;
      }

      else if(strncmp(argv[i], "-d", sizeof(fstring)) == 0) {
         sscanf(argv[i+1], "%d", &hnd->debug);
         i++;
      }

      else { /*assume this is the server name*/
         strncpy(hnd->server, argv[i], sizeof(fstring));
      }
   }

   if(!hnd->server) {
      cactest_print_usage(argv);
      cac_FreeHandle(hnd);
      exit(-1);
   }

}

void print_value(uint32 type, REG_VALUE_DATA *data) {
   int i = 0;

   switch(type) {
      case REG_SZ:
         printf(" Type: REG_SZ\n");
         printf(" Value: %s\n", data->reg_sz);
         break;
      case REG_EXPAND_SZ:
         printf(" Type: REG_EXPAND_SZ\n");
         printf(" Value: %s\n", data->reg_expand_sz);
         break;
      case REG_MULTI_SZ:
         printf(" Type: REG_MULTI_SZ\n");
         printf(" Values: ");

         for(i = 0; i < data->reg_multi_sz.num_strings; i++) {
            printf("     %d: %s\n", i, data->reg_multi_sz.strings[i]);
         }
         break;
      case REG_DWORD:
         printf(" Type: REG_DWORD\n");
         printf(" Value: %d\n", data->reg_dword);
         break;
      case REG_DWORD_BE:
         printf(" Type: REG_DWORD_BE\n");
         printf(" Value: 0x%x\n", data->reg_dword_be);
         break;
      case REG_BINARY:
         printf(" Type: REG_BINARY\n");
         break;
      default:
         printf(" Invalid type: %d\n", type);
         
   }

   printf("\n");
   
}

void cactest_readline(FILE *in, fstring line) {

   int c;
   
   c = fgetc(in);
   if(c != '\n')
      ungetc(c, in);

   fgets(line, sizeof(fstring), in);

   if(line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

}

void cactest_GetAuthDataFn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword)
    
{
   char temp[sizeof(fstring)];

   static char authUsername[sizeof(fstring)];
   static char authWorkgroup[sizeof(fstring)];
   static char authPassword[sizeof(fstring)];
   static char authSet = 0;

   char *pass = NULL;

   if (authSet)
   {
      strncpy(pWorkgroup, authWorkgroup, maxLenWorkgroup - 1);
      strncpy(pUsername, authUsername, maxLenUsername - 1);
      strncpy(pPassword, authPassword, maxLenPassword - 1);
   }
   else
   {
      if(pWorkgroup[0] != '\0') {
         strncpy(authWorkgroup, pWorkgroup, maxLenWorkgroup - 1);
      }
      else {
         d_printf("Domain: [%s] ", pWorkgroup);
         fscanf(stdin, "%s", temp);

         if (temp[0] != '\0')
         {
            strncpy(pWorkgroup, temp, maxLenWorkgroup - 1);
            strncpy(authWorkgroup, temp, maxLenWorkgroup - 1);
         }
      }


      if(pUsername[0] != '\0') {
         strncpy(authUsername, pUsername, maxLenUsername - 1);
      }
      else {
         d_printf("Username: [%s] ", pUsername);
         fscanf(stdin, "%s", temp);

         if (temp[strlen(temp) - 1] == '\n') /* A new line? */
         {
            temp[strlen(temp) - 1] = '\0';
         }

         if (temp[0] != '\0')
         {
            strncpy(pUsername, temp, maxLenUsername - 1);
            strncpy(authUsername, pUsername, maxLenUsername - 1);
         }
      }
      if(pPassword[0] != '\0') {
         strncpy(authPassword, pPassword, maxLenPassword - 1);
      }
      else {
         pass = getpass("Password: ");
         if (pass)
            fstrcpy(temp, pass);
         if (temp[strlen(temp) - 1] == '\n') /* A new line? */
         {
            temp[strlen(temp) - 1] = '\0';
         }        
         if (temp[0] != '\0')
         {
            strncpy(pPassword, temp, maxLenPassword - 1);
            strncpy(authPassword, pPassword, maxLenPassword - 1);
         }    
      }
      authSet = 1;
   }
}

void cactest_reg_input_val(TALLOC_CTX *mem_ctx, int *type, char **name, REG_VALUE_DATA *data) {
   fstring tmp;
   int i;

   printf("Enter value name: \n");
   cactest_readline(stdin, tmp);
   *name = talloc_strdup(mem_ctx, tmp);

   do {
      printf("Enter type. %d = REG_SZ, %d = REG_DWORD, %d = REG_MULTI_SZ: ", REG_SZ, REG_DWORD, REG_MULTI_SZ);
      scanf("%d", type);
   } while(*type != REG_SZ && *type != REG_DWORD && *type != REG_MULTI_SZ);

   switch(*type) {
      case REG_SZ:
         printf("Enter string:\n");
         cactest_readline(stdin, tmp);

         data->reg_sz = talloc_strdup(mem_ctx, tmp);
         break;

      case REG_DWORD:
         printf("Enter dword: ");
         scanf("%d", &data->reg_dword);
         break;

      case REG_MULTI_SZ:
         printf("Enter number of strings: ");
         scanf("%d", &data->reg_multi_sz.num_strings);
         
         data->reg_multi_sz.strings = talloc_array(mem_ctx, char *, data->reg_multi_sz.num_strings);

         for(i = 0; i < data->reg_multi_sz.num_strings; i++) {
            printf("String %d: ", i+1);
            cactest_readline(stdin, tmp);

            data->reg_multi_sz.strings[i] = talloc_strdup(mem_ctx, tmp);
         }
         break;
   }
}

void print_cac_user_info(CacUserInfo *info) {
   printf("	User Name    : %s\n", info->username);
   printf("	Full Name    : %s\n", info->full_name);
   printf("	Home Dir     : %s\n", info->home_dir);
   printf("	Home Drive   : %s\n", info->home_drive);
   printf("	Profile Path : %s\n", info->profile_path);
   printf("	Logon Script : %s\n", info->logon_script);
   printf("	Description  : %s\n", info->description);
   printf("	Workstations : %s\n", info->workstations);
   printf("	Remote Dial  : %s\n", info->dial);

   printf("	Logon Time   : %s\n", http_timestring(info->logon_time));
   printf("	Logoff Time  : %s\n", http_timestring(info->logoff_time));
   printf("	Kickoff Time : %s\n", http_timestring(info->kickoff_time));
   printf("	Pass last set: %s\n", http_timestring(info->pass_last_set_time));
   printf("	Pass can set : %s\n", http_timestring(info->pass_can_change_time));
   printf("	Pass must set: %s\n", http_timestring(info->pass_must_change_time));

   printf("	User RID     : 0x%x\n", info->rid);
   printf("	Group RID    : 0x%x\n", info->group_rid);
   printf("	ACB Mask     : 0x%x\n", info->acb_mask);

   printf("	Bad pwd count: %d\n", info->bad_passwd_count);
   printf("	Logon Cuont  : %d\n", info->logon_count);

   printf(" NT Password  : %s\n", info->nt_password);
   printf(" LM Password  : %s\n", info->lm_password);

}

void edit_readline(fstring line) {
   fgets(line, sizeof(fstring), stdin);

   if(line[strlen(line)-1] == '\n')
      line[strlen(line)-1] = '\0';
}
void edit_cac_user_info(TALLOC_CTX *mem_ctx, CacUserInfo *info) {
   fstring tmp;

   printf(" User Name [%s]: ", info->username);
   edit_readline(tmp);

   if(tmp[0] != '\0')
      info->username = talloc_strdup(mem_ctx, tmp);

   printf(" Full Name [%s]: ", info->full_name);

   edit_readline(tmp);
   if(tmp[0] != '\0')
      info->full_name = talloc_strdup(mem_ctx, tmp);

   printf(" Description [%s]: ", info->description);
   edit_readline(tmp);
   if(tmp[0] != '\0')
      info->description = talloc_strdup(mem_ctx, tmp);

   printf(" Remote Dial [%s]: ", info->dial);
   edit_readline(tmp);
   if(tmp[0] != '\0')
      info->dial = talloc_strdup(mem_ctx, tmp);

   printf(" ACB Mask [0x%x]: ", info->acb_mask);
   edit_readline(tmp);
   if(tmp[0] != '\0') 
      sscanf(tmp, "%x", &info->acb_mask);

   printf(" Must change pass at next logon? [y/N]: ");
   edit_readline(tmp);

   if(tmp[0] == 'y' || tmp[0] == 'Y')
      info->pass_must_change= True;

}
   
void print_cac_group_info(CacGroupInfo *info) {
   printf(" Group Name  : %s\n", info->name);
   printf(" Description : %s\n", info->description);
   printf(" Num Members : %d\n", info->num_members);
}

void edit_cac_group_info(TALLOC_CTX *mem_ctx, CacGroupInfo *info) {
   fstring tmp;

   printf("Group Name [%s]: ", info->name);
   edit_readline(tmp);
   if(tmp[0] != '\0')
      info->name = talloc_strdup(mem_ctx, tmp);

   printf("Description [%s]: ", info->description);
   edit_readline(tmp);
   if(tmp[0] != '\0')
      info->description = talloc_strdup(mem_ctx, tmp);
}

char *srv_role_str(uint32 role) {
   switch(role) {
      case ROLE_STANDALONE:
         return "STANDALONE";
         break;
      case ROLE_DOMAIN_MEMBER:
         return "DOMAIN_MEMBER";
         break;
      case ROLE_DOMAIN_BDC:
         return "DOMAIN_BDC";
         break;
      case ROLE_DOMAIN_PDC:
         return "DOMAIN_PDC";
         break;
   }

   return "Invalid role!\n";
}

char *cactime_str(CacTime ctime, fstring tmp) {

   snprintf(tmp, sizeof(fstring), "%u Days, %u Hours, %u Minutes, %u Seconds", ctime.days, ctime.hours, ctime.minutes, ctime.seconds);

   return tmp;
}

void print_cac_domain_info(CacDomainInfo *info) {
   fstring tmp;
   
   printf("  Server Role      : %s\n", srv_role_str(info->server_role));
   printf("  Num Users        : %d\n", info->num_users);
   printf("  Num Domain Groups: %d\n", info->num_domain_groups);
   printf("  Num Local Groups : %d\n", info->num_local_groups);
   printf("  Comment          : %s\n", info->comment);
   printf("  Domain Name      : %s\n", info->domain_name);
   printf("  Server Name      : %s\n", info->server_name);
   printf("  Min. Pass. Length: %d\n", info->min_pass_length);
   printf("  Password History : %d\n", info->pass_history);
   printf("\n");
   printf("  Passwords Expire In    : %s\n", cactime_str(info->expire, tmp));
   printf("  Passwords Can Change in: %s\n", cactime_str(info->min_pass_age, tmp));
   printf("  Lockouts last          : %s\n", cactime_str(info->lockout_duration, tmp));
   printf("  Allowed Bad Attempts   : %d\n", info->num_bad_attempts);
}

void print_cac_service(CacService svc) {
   printf("\tService Name: %s\n", svc.service_name);
   printf("\tDisplay Name: %s\n", svc.display_name);
   print_service_status(svc.status);
}

void print_service_status(SERVICE_STATUS status) {
   printf("\tStatus:\n");
   printf("\t Type:          0x%x\n", status.type); 
   printf("\t State:         0x%x\n", status.state);
   printf("\t Controls:      0x%x\n", status.controls_accepted);
   printf("\t W32 Exit Code: 0x%x\n", status.win32_exit_code);
   printf("\t SVC Exit Code: 0x%x\n", status.service_exit_code);
   printf("\t Checkpoint:    0x%x\n", status.check_point);
   printf("\t Wait Hint:     0x%x\n", status.wait_hint);
   printf("\n");
}

void print_service_config(CacServiceConfig *config) {
   printf("\tConfig:\n");
   printf("\tType:             0x%x\n", config->type);
   printf("\tStart Type:       0x%x\n", config->start_type);
   printf("\tError config:     0x%x\n", config->error_control);
   printf("\tExecutable Path:  %s\n", config->exe_path);
   printf("\tLoad Order Group: %s\n", config->load_order_group);
   printf("\tTag ID:           0x%x\n", config->tag_id);
   printf("\tDependencies:     %s\n", config->dependencies);
   printf("\tStart Name:       %s\n", config->start_name);
   printf("\tDisplay Name:     %s\n", config->display_name);
}
