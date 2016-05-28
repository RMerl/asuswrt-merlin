/*
 * Unix SMB/CIFS implementation. 
 * cacusermgr utility functions.
 *
 * Copyright (C) Chris Nicholls     2005
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.  */

#include "cacusermgr.h"

/*prints usage and quits*/
void usage() {
   printf("Usage:\n");
   printf("    cacusermgr [options] server\n\n");
   printf("options:\n");
   printf("   -u USERNAME        Username to login with\n");
   printf("   -d/-w DOMAIN       Domain name\n");
   printf("   -D LEVEL           Debug level\n");
   printf("   -h                 Print this message\n");

   exit(1);
}

/*initializes values in the server handle from the command line returns 0 if there is a problem, non-zero if everything is ok*/
int process_cmd_line(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, int argc, char **argv) {
   char op;

   if(!hnd || !mem_ctx || !argc)
      return 0;

   while( (op = getopt(argc, argv, "u:U:d:w:W:D:h")) != -1) {
      switch(op) {
         case 'u': /*username*/
         case 'U': 
            if(optarg)
               strncpy(hnd->username, optarg, sizeof(fstring));
            else
               usage();
            break;

         case 'd': /*domain name*/
         case 'w':
         case 'W':
            if(optarg)
               strncpy(hnd->domain, optarg, sizeof(fstring));
            else
               usage();
            break;

         case 'D': /*debug level*/
            if(optarg)
               hnd->debug = atoi(optarg);
            else
               usage();
            break;

         case 'h': /*help*/
            usage();
            break;

         case '?':
         default:
            printf("Unknown option -%c\n", op);
            usage();
      }
   }

   if(optind >= argc)
      usage();

   /*whatever is less should be the server*/
   strncpy(hnd->server, argv[optind], sizeof(fstring));

   return 1;
}

void mgr_getline(fstring line) {

   fgets(line, sizeof(fstring), stdin);

   if(line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

}

/*this is pretty similar to the other get_auth_data_fn's*/
void mgr_GetAuthDataFn(const char * pServer,
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
         mgr_getline(pWorkgroup);

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
         mgr_getline(pUsername);

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

void mgr_page(uint32 line_count) {

   if( (line_count % DEFAULT_SCREEN_LINES) != 0)
      return;

   printf("--Press enter to continue--\n");
   getchar();
}

/*reads a line from stdin, figures out if it is a RID or name, gets a CacLookupRidsRecord and then returns the type*/
uint32 rid_or_name(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd, uint32 *rid, char **name) {
   fstring line;

   BOOL is_rid = False;
   uint32 rid_type = 0;

   struct SamGetNamesFromRids getnames;
   struct SamGetRidsFromNames getrids;

   mgr_getline(line);

   if(strncmp(line, "0x", 2) == 0) {
      /*then this is a RID*/
      sscanf( (line + 2), "%x", rid);
      is_rid = True;
   }
   else {
      /*then this is a name*/
      *name = talloc_strdup(mem_ctx, line);
   }

   if(is_rid) {
      ZERO_STRUCT(getnames);

      getnames.in.dom_hnd  = dom_hnd;
      getnames.in.rids     = rid;
      getnames.in.num_rids = 1;

      cac_SamGetNamesFromRids(hnd, mem_ctx, &getnames);

      if(getnames.out.num_names > 0)
         rid_type = getnames.out.map[0].type;
         
   }
   else {
      ZERO_STRUCT(getrids);

      getrids.in.dom_hnd   = dom_hnd;
      getrids.in.names     = name;
      getrids.in.num_names = 1;

      cac_SamGetRidsFromNames(hnd, mem_ctx, &getrids);

      if(getrids.out.num_rids > 0) {
         rid_type = getrids.out.map[0].type;

         /*send back the RID so cac_SamOpenXX() doesn't have to look it up*/
         *rid = getrids.out.map[0].rid;
      }
   }

   return rid_type;
}

/*print's out some common error messages*/
void printerr(const char *msg, NTSTATUS status) {
   if(NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED))
      printf("%s You do not have sufficient rights.\n", msg);

   else if(NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER))
      printf("%s No such user.\n", msg);
   
   else if(NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_GROUP))
      printf("%s No such group.\n", msg);

   else if(NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS))
      printf("%s User already exists.\n", msg);

   else if(NT_STATUS_EQUAL(status, NT_STATUS_GROUP_EXISTS))
      printf("%s Group already exists.\n", msg);

   else
      printf("%s %s.\n", msg, nt_errstr(status));
}

char *get_new_password(TALLOC_CTX *mem_ctx) {
   char *pass1 = NULL;

   pass1 = getpass("Enter new password: ");

   return talloc_strdup(mem_ctx, pass1);
}

void print_rid_list(uint32 *rids, char **names, uint32 num_rids) {
   uint32 i = 0;

   if(!names || !rids)
      return;

   printf(" RID     Name\n");

   while(i < num_rids) {
      printf("[0x%x] [%s]\n", rids[i], names[i]);

      i++;

      mgr_page(i);
   }
}

void print_lookup_records(CacLookupRidsRecord *map, uint32 num_rids) {
   uint32 i = 0;

   if(!map)
      return;

   printf("RID     Name\n");

   while(i < num_rids) {
      if(map[i].found) {
         printf("[0x%x] [%s]\n", map[i].rid, map[i].name);
      }

      i++;

      mgr_page(i);
   }
}

int list_groups(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd) {
   struct SamEnumGroups eg;
   
   if(!hnd || !mem_ctx || !dom_hnd)
      return 0;

   ZERO_STRUCT(eg);
   eg.in.dom_hnd = dom_hnd;

   while(cac_SamEnumGroups(hnd, mem_ctx, &eg))
      print_rid_list(eg.out.rids, eg.out.names, eg.out.num_groups);

   if(CAC_OP_FAILED(hnd->status)) {
      printerr("Could not enumerate groups.", hnd->status);
      return 0;
   }

   return 1;
}

void list_users(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd) {
   struct SamEnumUsers eu;

   if(!hnd || !mem_ctx || !dom_hnd)
      return;

   ZERO_STRUCT(eu);
   eu.in.dom_hnd = dom_hnd;

   while(cac_SamEnumUsers(hnd, mem_ctx, &eu))
      print_rid_list(eu.out.rids, eu.out.names, eu.out.num_users);

   if(CAC_OP_FAILED(hnd->status))
      printerr("Could not enumerate users.", hnd->status);
}
