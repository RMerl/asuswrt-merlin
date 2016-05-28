/*
 * Unix SMB/CIFS implementation. 
 * cacusermgr group implementation.
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

CacGroupInfo *get_group_info(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *group_hnd) {
   struct SamGetGroupInfo getinfo;

   if(!hnd || !mem_ctx ||!group_hnd)
      return NULL;

   ZERO_STRUCT(getinfo);
   getinfo.in.group_hnd = group_hnd;

   if(!cac_SamGetGroupInfo(hnd, mem_ctx, &getinfo)) 
      printerr("Could not get group info.", hnd->status);

   return getinfo.out.info;
}

void print_group_info(CacGroupInfo *info) {
   if(!info)
      return;

   printf(" Group Name        : %s\n", info->name);
   printf(" Description       : %s\n", info->description);
   printf(" Number of Members : %d\n", info->num_members);
}

CacGroupInfo *modify_group_info(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *group_hnd) {
   struct SamSetGroupInfo setinfo;
   CacGroupInfo *info = NULL;
   fstring tmp;

   info = get_group_info(hnd, mem_ctx, group_hnd);

   if(!info)
      return NULL;

   printf("Description [%s]: ", info->description);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->description = talloc_strdup(mem_ctx, tmp);

   ZERO_STRUCT(setinfo);
   setinfo.in.group_hnd = group_hnd;
   setinfo.in.info = info;

   if(!cac_SamSetGroupInfo(hnd, mem_ctx, &setinfo)) {
      printerr("Could not set info.", hnd->status);
      info = NULL;
   }

   return info;
}

void group_menu(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd, POLICY_HND *group_hnd) {
   CacGroupInfo *info = NULL;
   int rid_type = 0;

   fstring in;

   char *buf;

   struct SamGetGroupMembers getmem;
   struct SamGetNamesFromRids getnames;
   struct SamAddGroupMember add;
   struct SamRemoveGroupMember del;
 
   info = get_group_info(hnd, mem_ctx, group_hnd);

   printf("\n");
   print_group_info(info);

   while(in[0] != 'b' && in[0] != 'B' && in[0] != 'q' && in[0] != 'Q') {
      printf("\n");
      printf("[m] List Group Members\n");
      printf("[a] Add User To Group\n");
      printf("[r] Remove User From Group\n");
      printf("[l] List Users\n");
      printf("[v] View Group Info\n");
      printf("[d] Set Group Description\n");
      printf("[x] Delete Group\n");
      printf("[b] Back\n\n");
      
      printf("Command: ");
      mgr_getline(in);
      
      printf("\n");

      switch(in[0]) {
         case 'a': /*add member to group*/
         case 'A':
            ZERO_STRUCT(add);
            add.in.group_hnd = group_hnd;

            printf("Enter RID or Name: ");
            rid_type = rid_or_name(hnd, mem_ctx, dom_hnd, &add.in.rid, &buf);

            if(rid_type != CAC_USER_RID) {
               printf("Invalid User.\n");
               break;
            }

            if(!cac_SamAddGroupMember(hnd, mem_ctx, &add)) {
               printerr("Could not add user to group.", hnd->status);
            }
            break;

         case 'r': /*remove user from group*/
         case 'R':
            ZERO_STRUCT(del);
            del.in.group_hnd = group_hnd;

            printf("Enter RID or Name: ");
            rid_type = rid_or_name(hnd, mem_ctx, dom_hnd, &del.in.rid, &buf);

            if(rid_type != CAC_USER_RID) {
               printf("Invalid User.\n");
               break;
            }

            if(!cac_SamRemoveGroupMember(hnd, mem_ctx, &del)) {
               printerr("Could not remove use from group.", hnd->status);
            }
            break;

         case 'l': /*list users*/
         case 'L':
            list_users(hnd, mem_ctx, dom_hnd);
            break;

         case 'm': /*list members*/
         case 'M':
            ZERO_STRUCT(getmem);
            getmem.in.group_hnd = group_hnd;

            if(!cac_SamGetGroupMembers(hnd, mem_ctx, &getmem)) {
               printerr("Could not get members.", hnd->status);
               break;
            }

            ZERO_STRUCT(getnames);
            getnames.in.dom_hnd = dom_hnd;
            getnames.in.rids = getmem.out.rids;
            getnames.in.num_rids = getmem.out.num_members;

            if(!cac_SamGetNamesFromRids(hnd, mem_ctx, &getnames)) {
               printerr("Could not lookup names.", hnd->status);
               break;
            }

            printf("Group has %d members:\n", getnames.out.num_names);
            print_lookup_records(getnames.out.map, getnames.out.num_names);

            break;

         case 'd': /*set description*/
         case 'D':
            info = modify_group_info(hnd, mem_ctx, group_hnd);

            if(info)
               printf("Set Group Info.\n");
            break;

         case 'v': /*view info*/
         case 'V':
            info = get_group_info(hnd, mem_ctx, group_hnd);
            print_group_info(info);
            break;

         case 'x': /*delete group*/
         case 'X': 
            if(!cac_SamDeleteGroup(hnd, mem_ctx, group_hnd))
               printerr("Could Not Delete Group.", hnd->status);

            /*we want to go back to the main menu*/
            in[0] = 'b';
            break;

         case 'b': /*back*/
         case 'B':
         case 'q':
         case 'Q':
            break;

         default:
            printf("Invalid Command.\n");
      }
   }

   cac_SamClose(hnd, mem_ctx, group_hnd);
}
