/*
 * Unix SMB/CIFS implementation. 
 * cacusermgr user implementation.
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

void print_user_info(CacUserInfo *info) {
   printf("\n");
   printf(" User Name      : %s\n", info->username);
   printf(" Full Name      : %s\n", info->full_name);
   printf(" Home Dir       : %s\n", info->home_dir);
   printf(" Home Drive     : %s\n", info->home_drive);
   printf(" Profile Path   : %s\n", info->profile_path);
   printf(" Logon Script   : %s\n", info->logon_script);
   printf(" Description    : %s\n", info->description);
   printf(" Workstations   : %s\n", info->workstations);
   printf(" Remote Dial    : %s\n", info->dial);

   printf(" Logon Time     : %s\n", http_timestring(info->logon_time));
   printf(" Logoff Time    : %s\n", http_timestring(info->logoff_time));
   printf(" Kickoff Time   : %s\n", http_timestring(info->kickoff_time));
   printf(" Pass last set  : %s\n", http_timestring(info->pass_last_set_time));
   printf(" Pass can set   : %s\n", http_timestring(info->pass_can_change_time));
   printf(" Pass must set  : %s\n", http_timestring(info->pass_must_change_time));

   printf(" User RID       : 0x%x\n", info->rid);
   printf(" Group RID      : 0x%x\n", info->group_rid);
   printf(" User Type      : ");

   if(info->acb_mask & ACB_NORMAL)
      printf("Normal User\n");
   else if(info->acb_mask & ACB_TEMPDUP)
      printf("Temporary Duplicate Account\n");
   else if(info->acb_mask & ACB_DOMTRUST)
      printf("Inter-Domain Trust Account\n");
   else if(info->acb_mask & ACB_WSTRUST)
      printf("Workstation Trust Account\n");
   else if(info->acb_mask & ACB_SVRTRUST)
      printf("Server Trust Account\n");
   else
      printf("\n");

   printf(" Disabled       : %s\n", (info->acb_mask & ACB_DISABLED) ? "Yes" : "No");
   printf(" Locked         : %s\n", (info->acb_mask & ACB_AUTOLOCK) ? "Yes" : "No");
   printf(" Pass Expires   : %s\n", (info->acb_mask & ACB_PWNOEXP) ? "No" : "Yes");
   printf(" Pass Required  : %s\n", (info->acb_mask & ACB_PWNOTREQ) ? "No" : "Yes");

}

CacUserInfo *modify_user_info(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *user_hnd) {
   CacUserInfo *info = NULL;
   fstring tmp;

   struct SamGetUserInfo getinfo;
   struct SamSetUserInfo setinfo;

   ZERO_STRUCT(getinfo);
   ZERO_STRUCT(setinfo);

   getinfo.in.user_hnd = user_hnd;

   if(!cac_SamGetUserInfo(hnd, mem_ctx, &getinfo)) {
      printerr("Could not get user info.", hnd->status);
      return NULL;
   }

   info = getinfo.out.info;

   printf("\n");
   printf(" User Name [%s]: ", info->username);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->username = talloc_strdup(mem_ctx, tmp);

   printf(" Full Name [%s]: ", info->full_name);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->full_name = talloc_strdup(mem_ctx, tmp);
   
   printf(" Description  [%s]: ", info->description);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->description = talloc_strdup(mem_ctx, tmp);
   
   printf(" Home Dir  [%s]: ", info->home_dir);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->home_dir = talloc_strdup(mem_ctx, tmp);

   printf(" Home Drive [%s]: ", info->home_drive);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->home_drive = talloc_strdup(mem_ctx, tmp);
   
   printf(" Profile Path [%s]: ", info->profile_path);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->profile_path = talloc_strdup(mem_ctx, tmp);

   printf(" Logon Script [%s]: ", info->logon_script);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->logon_script = talloc_strdup(mem_ctx, tmp);
   
   printf(" Workstations [%s]: ", info->workstations);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->workstations = talloc_strdup(mem_ctx, tmp);
   
   printf(" Remote Dial [%s]: ", info->dial);
   mgr_getline(tmp);
   if(tmp[0] != '\0')
      info->dial = talloc_strdup(mem_ctx, tmp);

   printf(" Disabled [%s] (y/n): ", (info->acb_mask & ACB_DISABLED) ? "Yes" : "No");
   mgr_getline(tmp);
   if(tmp[0] == 'y' || tmp[0] == 'Y')
      info->acb_mask |= ACB_DISABLED;
   else if(tmp[0] == 'n' || tmp[0] == 'N')
      info->acb_mask ^= (info->acb_mask & ACB_DISABLED) ? ACB_DISABLED : 0x0;
      
   printf(" Pass Expires [%s] (y/n): ", (info->acb_mask & ACB_PWNOEXP) ? "No" : "Yes");
   mgr_getline(tmp);
   if(tmp[0] == 'n' || tmp[0] == 'N')
      info->acb_mask |= ACB_PWNOEXP;
   else if(tmp[0] == 'y' || tmp[0] == 'Y')
      info->acb_mask ^= (info->acb_mask & ACB_PWNOEXP) ? ACB_PWNOEXP : 0x0;

   printf(" Pass Required [%s] (y/n): ", (info->acb_mask & ACB_PWNOTREQ) ? "No" : "Yes");
   mgr_getline(tmp);
   if(tmp[0] == 'n' || tmp[0] == 'N')
      info->acb_mask |= ACB_PWNOTREQ;
   else if(tmp[0] == 'y' || tmp[0] == 'Y')
      info->acb_mask ^= (info->acb_mask & ACB_PWNOTREQ) ? ACB_PWNOTREQ : 0x0;

   setinfo.in.user_hnd = user_hnd;
   setinfo.in.info     = info;

   if(!cac_SamSetUserInfo(hnd, mem_ctx, &setinfo)) {
      printerr("Could not set user info.", hnd->status);
   }

   return info;
}

void add_user_to_group(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, CacUserInfo *info, POLICY_HND *dom_hnd) {
   int rid_type = 0;

   char *tmp = NULL;

   struct SamOpenGroup og;
   struct SamAddGroupMember add;

   ZERO_STRUCT(og);
   ZERO_STRUCT(add);
   
   printf("Group RID or Name:");

   og.in.dom_hnd = dom_hnd;
   og.in.access = MAXIMUM_ALLOWED_ACCESS;
   rid_type = rid_or_name(hnd, mem_ctx, dom_hnd, &og.in.rid, &tmp);

   if(!cac_SamOpenGroup(hnd, mem_ctx, &og)) {
      printerr("Could not open group.", hnd->status);
      return;
   }

   add.in.group_hnd = og.out.group_hnd;
   add.in.rid = info->rid;

   if(!cac_SamAddGroupMember(hnd, mem_ctx, &add)) {
      printerr("Could not add user to group.", hnd->status);
   }

   cac_SamClose(hnd, mem_ctx, og.out.group_hnd);
}

void remove_user_from_group(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, CacUserInfo *info, POLICY_HND *dom_hnd) {
   int rid_type = 0;

   char *tmp = NULL;

   struct SamOpenGroup og;
   struct SamRemoveGroupMember del;

   ZERO_STRUCT(og);
   ZERO_STRUCT(del);
   
   printf("Group RID or Name:");

   og.in.dom_hnd = dom_hnd;
   og.in.access = MAXIMUM_ALLOWED_ACCESS;
   rid_type = rid_or_name(hnd, mem_ctx, dom_hnd, &og.in.rid, &tmp);

   if(!cac_SamOpenGroup(hnd, mem_ctx, &og)) {
      printerr("Could not open group.", hnd->status);
      return;
   }

   del.in.group_hnd = og.out.group_hnd;
   del.in.rid = info->rid;

   if(!cac_SamRemoveGroupMember(hnd, mem_ctx, &del)) {
      printerr("Could not add user to group.", hnd->status);
   }

   cac_SamClose(hnd, mem_ctx, og.out.group_hnd);
}

void user_menu(CacServerHandle *hnd, TALLOC_CTX *mem_ctx, POLICY_HND *dom_hnd, POLICY_HND *user_hnd) {
   fstring in;

   struct SamGetUserInfo      getinfo;
   struct SamSetPassword      setpass;
   struct SamGetGroupsForUser groups;
   struct SamGetNamesFromRids gnfr;

   CacUserInfo *info = NULL;

   if(!hnd || !mem_ctx || !user_hnd) {
      printf("Must open user.\n");
      return;
   }

   /*get the userinfo and print it out*/
   ZERO_STRUCT(getinfo);
   getinfo.in.user_hnd = user_hnd;

   if(!cac_SamGetUserInfo(hnd, mem_ctx, &getinfo)) {
      printerr("Could not get info.", hnd->status);
      info = NULL;
   }
   else {
      info = getinfo.out.info;
      print_user_info(info);
   }

   /*now deal with the menu*/
   in[0] = '\0';
   while(in[0] != 'b' && in[0] != 'B' && in[0] != 'q' && in[0] != 'Q') {
      printf("\n");
      printf("[s] Set Password\n");

      if(info && (info->acb_mask & ACB_DISABLED))
         printf("[e] Enable User\n");
      else if(info)
         printf("[d] Disable User\n");

      printf("[v] View User Info\n");
      printf("[m] Modify User Info\n");
      printf("[x] Delete User\n\n");

      printf("[g] List Group Membership\n");
      printf("[a] Add User To Group\n");
      printf("[l] List Domain Groups\n");
      printf("[r] Remove User From Group\n\n");

      printf("[b] Back\n\n");

      printf("Command: ");
      mgr_getline(in);

      printf("\n");

      switch(in[0]) {
         case 'g': /*list group membership*/
         case 'G': 
            ZERO_STRUCT(groups);
            groups.in.user_hnd = user_hnd;

            if(!cac_SamGetGroupsForUser(hnd, mem_ctx, &groups)) {
               printerr("Could not get groups.", hnd->status);
               break;
            }

            ZERO_STRUCT(gnfr);
            gnfr.in.dom_hnd = dom_hnd;
            gnfr.in.rids = groups.out.rids;
            gnfr.in.num_rids = groups.out.num_groups;

            if(!cac_SamGetNamesFromRids(hnd, mem_ctx, &gnfr)) {
               printerr("Could not map RIDs to names.", hnd->status);
               break;
            }

            print_lookup_records(gnfr.out.map, gnfr.out.num_names);

            break;
         case 's': /*reset password*/
         case 'S':
            ZERO_STRUCT(setpass);
            setpass.in.user_hnd = user_hnd;
            setpass.in.password = get_new_password(mem_ctx);
            
            if(!setpass.in.password) {
               printf("Out of memory.\n");
               break;
            }

            if(!cac_SamSetPassword(hnd, mem_ctx, &setpass)) {
               printerr("Could not set password.", hnd->status);
            }
            else {
               printf("Reset password.\n");
            }
            break;

         case 'e': /*enable user*/
         case 'E': 
            if(info && !(info->acb_mask & ACB_DISABLED))
               break;

            if(!cac_SamEnableUser(hnd, mem_ctx, user_hnd)) {
               printerr("Could not enable user.", hnd->status);
            }
            else {
               printf("Enabled User.\n");
               /*toggle the disabled ACB bit in our local copy of the info*/
               info->acb_mask ^= ACB_DISABLED;
            }
            break;

         case 'd': /*disable user*/
         case 'D':
            if(info && (info->acb_mask & ACB_DISABLED))
               break;

            if(!cac_SamDisableUser(hnd, mem_ctx, user_hnd)) {
               printerr("Could not disable user.", hnd->status);
            }
            else {
               printf("Disabled User.\n");
               /*toggle the disabled ACB bit in our local copy of the info*/
               info->acb_mask ^= ACB_DISABLED;
            }
            break;

         case 'v': /*view user info*/
         case 'V':
            ZERO_STRUCT(getinfo);
            getinfo.in.user_hnd = user_hnd;

            if(!cac_SamGetUserInfo(hnd, mem_ctx, &getinfo)) {
               printerr("Could not get info.", hnd->status);
               info = NULL;
            }
            else {
               info = getinfo.out.info;
               print_user_info(info);
            }

            break;

         case 'm': /*modify user info*/
         case 'M':
            info = modify_user_info(hnd, mem_ctx, user_hnd);

            if(info)
               printf("Updated user info.\n");
            break;

         case 'l': /*list domain groups*/
         case 'L':
            list_groups(hnd, mem_ctx, dom_hnd);
            break;

         case 'a': /*add user to group*/
         case 'A':
            add_user_to_group(hnd, mem_ctx, info, dom_hnd);
            break;

         case 'r': /*remove user from group*/
         case 'R':
            remove_user_from_group(hnd, mem_ctx, info, dom_hnd);
            break;
            
         case 'x': /*delete user*/
         case 'X':
            if(!cac_SamDeleteUser(hnd, mem_ctx, user_hnd))
               printerr("Could not delete user.", hnd->status);

            /*we want to go back to the main menu*/
            in[0] = 'b';
            break;

         case 'b': /*back*/
         case 'B':
         case 'q':
         case 'Q':
            /*do nothing*/
            break;
            
         default:
            printf("Invalid command.\n");
      }
   }

   /*close the user before returning*/
   cac_SamClose(hnd, mem_ctx, user_hnd);
}
