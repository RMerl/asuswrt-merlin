/*
 * Unix SMB/CIFS implementation. 
 * cacusermgr main implementation.
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

#define DEFAULT_MENU_LINES 15


void create_menu(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd) {
   struct SamCreateUser  cu;
   struct SamCreateGroup cg;

   fstring in;
   fstring tmp;

   if(!hnd || !mem_ctx || !dom_hnd) {
      printf("No Handle to SAM.\n");
      return;
   }

   /*the menu*/
   in[0] = '\0';
   while(in[0] != 'c' && in[0] != 'C' && in[0] != 'q' && in[0] != 'Q') {
      printf("\n");
      printf("[u] Create User\n");
      printf("[g] Create Group\n");
      printf("[m] Create Machine Account\n");
      printf("[c] Cancel\n\n");

      printf("Command: ");
      mgr_getline(in);

      printf("\n");

      switch(in[0]) {
         case 'u': /*create user*/
         case 'U':
            ZERO_STRUCT(cu);
            cu.in.dom_hnd = dom_hnd;
            cu.in.acb_mask = ACB_NORMAL;

            printf("Enter name: ");
            mgr_getline(tmp);
            cu.in.name = talloc_strdup(mem_ctx, tmp);

            if(!cac_SamCreateUser(hnd, mem_ctx, &cu)) {
               printerr("Could not create user.", hnd->status);
            }
            else {
               user_menu(hnd, mem_ctx, dom_hnd, cu.out.user_hnd);
            }

            /*this will break the loop and send us back to the main menu*/
            in[0] = 'c';
            break;

         case 'g': /*create group*/
         case 'G':
            ZERO_STRUCT(cg);
            cg.in.dom_hnd = dom_hnd;
            cg.in.access  = MAXIMUM_ALLOWED_ACCESS;

            printf("Enter name: ");
            mgr_getline(tmp);
            cg.in.name = talloc_strdup(mem_ctx, tmp);

            if(!cac_SamCreateGroup(hnd, mem_ctx, &cg)) {
               printerr("Could not create group.", hnd->status);
            }
            else {
               group_menu(hnd, mem_ctx, dom_hnd, cg.out.group_hnd);
            }

            /*this will break the loop and send us back to the main menu*/
            in[0] = 'c';
            break;

         case 'm': /*create machine account*/
         case 'M':
            ZERO_STRUCT(cu);
            cu.in.dom_hnd  = dom_hnd;
            cu.in.acb_mask = ACB_WSTRUST;

            printf("Enter machine name: ");
            mgr_getline(tmp);

            /*make sure we have a $ on the end*/
            if(tmp[strlen(tmp) - 1] != '$')
               cu.in.name = talloc_asprintf(mem_ctx, "%s$", tmp);
            else
               cu.in.name = talloc_strdup(mem_ctx, tmp);

            strlower_m(cu.in.name);

            printf("Creating account: %s\n", cu.in.name);

            if(!cac_SamCreateUser(hnd, mem_ctx, &cu)) {
               printerr("Could not create account.", hnd->status);
            }
            else {
               user_menu(hnd, mem_ctx, dom_hnd, cu.out.user_hnd);
            }

            /*this will break the loop and send us back to the main menu*/
            in[0] = 'c';
            break;

         case 'c': /*cancel*/
         case 'C':
         case 'q':
         case 'Q':
            /*do nothing*/
            break;

         default:
            printf("Invalid option\n");
      }
   }

   return;
}

void main_menu(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd) {
   fstring in;
   
   uint32 rid_type = 0;

   struct SamOpenUser   openu;
   struct SamOpenGroup  openg;
   struct SamEnumUsers  enumu;
   struct SamEnumGroups enumg;
   struct SamFlush      flush;

   char *name = NULL;
   uint32 rid = 0;

   if(!hnd || !mem_ctx || !dom_hnd) {
      printf("No handle to SAM.\n");
      return;
   }

   /*initialize this here and don't worry about it later*/
   ZERO_STRUCT(flush);
   flush.in.dom_hnd = dom_hnd;

   in[0] = '\0';

   /*handle the menu and commands*/
   while(in[0] != 'q' && in[0] != 'Q') {
      printf("\n");

      printf("[o] Open User or Group\n");
      printf("[c] Create Account or Group\n");
      printf("[u] List Users\n");
      printf("[g] List Groups\n");
      printf("[m] List Machine Accounts\n");
      printf("[q] Quit\n\n");

      printf("Command: ");

      mgr_getline(in);

      printf("\n");

      switch(in[0]) {
         case 'o': /*open user or group*/
         case 'O':
            printf("Enter RID or Name: ");
            rid_type = rid_or_name(hnd, mem_ctx, dom_hnd, &rid, &name);

            if(rid_type == CAC_USER_RID) {
               ZERO_STRUCT(openu);
               openu.in.dom_hnd = dom_hnd;
               openu.in.rid     = rid;
               openu.in.access  = MAXIMUM_ALLOWED_ACCESS;

               if(!cac_SamOpenUser(hnd, mem_ctx, &openu))
                  printerr("Could not open user.", hnd->status);
               else {
                  user_menu(hnd, mem_ctx, dom_hnd, openu.out.user_hnd);

                  if(!cac_SamFlush(hnd, mem_ctx, &flush)) {
                     printerr("Lost handle while flushing SAM.", hnd->status);
                     /*we want to quit*/
                     in[0] = 'q';
                  }
               }
            }
            else if(rid_type == CAC_GROUP_RID) {
               ZERO_STRUCT(openg);
               openg.in.dom_hnd = dom_hnd;
               openg.in.rid     = rid;
               openg.in.access  = MAXIMUM_ALLOWED_ACCESS;

               if(!cac_SamOpenGroup(hnd, mem_ctx, &openg))
                  printerr("Could not open group.", hnd->status);
               else {
                  group_menu(hnd, mem_ctx, dom_hnd, openg.out.group_hnd);

                  if(!cac_SamFlush(hnd, mem_ctx, &flush)) {
                     printerr("Lost handle while flushing SAM.", hnd->status);
                     /*we want to quit*/
                     in[0] = 'q';
                  }
               }
            }
            else {
               printf("Unknown RID/Name.\n");
            }
               
            break;

         case 'c': /*create account/group*/
         case 'C':
            create_menu(hnd, mem_ctx, dom_hnd);
            if(!cac_SamFlush(hnd, mem_ctx, &flush)) {
               printerr("Lost handle while flushing SAM.", hnd->status);
               /*we want to quit*/
               in[0] = 'q';
            }
            break;

         case 'u': /*list users*/
         case 'U':
            ZERO_STRUCT(enumu);
            enumu.in.dom_hnd = dom_hnd;
            enumu.in.acb_mask = ACB_NORMAL;

            printf("Users:\n");
            while(cac_SamEnumUsers(hnd, mem_ctx, &enumu)) {
               print_rid_list(enumu.out.rids, enumu.out.names, enumu.out.num_users);
            }
            if(CAC_OP_FAILED(hnd->status))
               printerr("Error occured while enumerating users.", hnd->status);
            break;

         case 'g': /*list groups*/
         case 'G':
            ZERO_STRUCT(enumg);
            enumg.in.dom_hnd = dom_hnd;

            while(cac_SamEnumGroups(hnd, mem_ctx, &enumg)) {
               print_rid_list( enumg.out.rids, enumg.out.names, enumg.out.num_groups);
            }

            if(CAC_OP_FAILED(hnd->status))
               printerr("Error occured while enumerating groups.", hnd->status);
            break;

         case 'm': /*list machine accounts*/
         case 'M':
            ZERO_STRUCT(enumu);
            enumu.in.dom_hnd = dom_hnd;
            enumu.in.acb_mask = ACB_WSTRUST;

            printf("Users:\n");
            while(cac_SamEnumUsers(hnd, mem_ctx, &enumu)) {
               print_rid_list( enumu.out.rids, enumu.out.names, enumu.out.num_users);
            }
            if(CAC_OP_FAILED(hnd->status))
               printerr("Error occured while enumerating accounts.", hnd->status);
            break;

         case 'q': /*quit*/
         case 'Q':
            /*just do nothing*/
            break;

         default:
            printf("Invalid Command.\n");
      }
   }
}

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx  = NULL;

   struct SamOpenDomain sod;

   mem_ctx = talloc_init("cacusermgr");
   if(!mem_ctx) {
      printf("Could not initialize Talloc Context\n");
      exit(-1);
   }

   /**first initialize the server handle with what we have*/
   hnd = cac_NewServerHandle(True);
   if(!hnd) {
      printf("Could not create server handle\n");
      exit(-1);
   }

   /*fill in the blanks*/
   if(!process_cmd_line(hnd, mem_ctx, argc, argv))
      usage();

   if(!cac_Connect(hnd, NULL)) {
      printf("Could not connect to server %s. %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   /*open the domain sam*/
   ZERO_STRUCT(sod);
   sod.in.access = MAXIMUM_ALLOWED_ACCESS;

   if(!cac_SamOpenDomain(hnd, mem_ctx, &sod)) {
      printf("Could not open handle to domain SAM. %s\n", nt_errstr(hnd->status));
      goto cleanup;
   }

   main_menu(hnd, mem_ctx, sod.out.dom_hnd);

cleanup:

   if(sod.out.dom_hnd)
      cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);

   if(sod.out.sam)
      cac_SamClose(hnd, mem_ctx, sod.out.sam);

   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}
