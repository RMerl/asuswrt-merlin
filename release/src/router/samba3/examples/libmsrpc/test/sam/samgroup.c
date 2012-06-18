/*Some group management stuff*/

#include "libmsrpc.h"
#include "test_util.h"

int main(int argc, char **argv) {
   CacServerHandle *hnd = NULL;
   TALLOC_CTX *mem_ctx = NULL;
            
   
   struct SamEnumGroups eg;
   struct SamEnumUsers eu;
   struct SamCreateGroup cg;
   struct SamOpenGroup og;
   struct SamGetGroupMembers ggm;
   struct SamGetNamesFromRids gn;
   struct SamAddGroupMember add;
   struct SamRemoveGroupMember del;
   struct SamSetGroupMembers set;
   struct SamGetGroupsForUser gg;
   struct SamOpenUser         ou;
   struct SamGetGroupInfo     gi;
   struct SamSetGroupInfo     si;
   struct SamRenameGroup      rg;
   struct SamGetSecurityObject gso;

   POLICY_HND *group_hnd = NULL;

   fstring tmp;
   fstring input;
   
   int i;

   mem_ctx = talloc_init("cac_samgroup");

   hnd = cac_NewServerHandle(True);

   cac_parse_cmd_line(argc, argv, hnd);

   if(!cac_Connect(hnd, NULL)) {
      fprintf(stderr, "Could not connect to server %s. Error: %s\n", hnd->server, nt_errstr(hnd->status));
      exit(-1);
   }

   struct SamOpenDomain sod;
   ZERO_STRUCT(sod);

   sod.in.access = MAXIMUM_ALLOWED_ACCESS; 

   if(!cac_SamOpenDomain(hnd, mem_ctx, &sod)) {
      fprintf(stderr, "Could not open domain. Error: %s\n", nt_errstr(hnd->status));
      goto done;
   }

   tmp[0] = 0x00;
   while(tmp[0] != 'q') {
      printf("\n");
      printf("[l]ist groups\n");
      printf("[c]reate group\n");
      printf("[o]pen group\n");
      printf("[d]elete group\n");
      printf("list [m]embers\n");
      printf("list [u]sers\n");
      printf("list [g]roup for users\n");
      printf("[a]dd member\n");
      printf("[r]emove member\n");
      printf("[x] clear members\n");
      printf("get group [i]nfo\n");
      printf("[e]dit group info\n");
      printf("[s]et members\n");
      printf("re[n]ame group\n");
      printf("[z] close group\n");
      printf("[t] get security info\n");

      printf("[q]uit\n\n");
      printf("Enter option: ");
      cactest_readline(stdin, tmp);

      printf("\n");

      switch(tmp[0]) {
         case 'c': /*create group*/
            if(group_hnd != NULL) {
               /*then we have an open handle.. close it*/
               cac_SamClose(hnd, mem_ctx, group_hnd);
               group_hnd = NULL;
            }

            printf("Enter group name: ");
            cactest_readline(stdin, input);

            ZERO_STRUCT(cg);

            cg.in.name      = talloc_strdup(mem_ctx, input);
            cg.in.access    = MAXIMUM_ALLOWED_ACCESS;
            cg.in.dom_hnd   = sod.out.dom_hnd;

            if(!cac_SamCreateGroup(hnd, mem_ctx, &cg)) {
               fprintf(stderr, "Could not create group. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Created group %s\n", cg.in.name);

               group_hnd = cg.out.group_hnd;
            }
            break;

         case 'o': /*open group*/
            if(group_hnd != NULL) {
               /*then we have an open handle.. close it*/
               cac_SamClose(hnd, mem_ctx, group_hnd);
               group_hnd = NULL;
            }

            ZERO_STRUCT(og);

            og.in.dom_hnd = sod.out.dom_hnd;
            og.in.access = MAXIMUM_ALLOWED_ACCESS;

            printf("Enter RID: 0x");
            scanf("%x", &og.in.rid);

            if(!cac_SamOpenGroup(hnd, mem_ctx, &og)) {
               fprintf(stderr, "Could not open group. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Opened group\n");
               group_hnd = og.out.group_hnd;
            }

            break;

         case 'l': /*list groups*/
            ZERO_STRUCT(eg);
            eg.in.dom_hnd = sod.out.dom_hnd;

            while(cac_SamEnumGroups(hnd, mem_ctx, &eg)) {
               for(i = 0; i < eg.out.num_groups; i++) {
                  printf("RID: 0x%x Name: %s\n", eg.out.rids[i], eg.out.names[i]);
               }
            }

            if(CAC_OP_FAILED(hnd->status)) {
               printf("Could not enumerate Groups. Error: %s\n", nt_errstr(hnd->status));
            }

            break;
            
         case 'm': /*list group members*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(ggm);
            ggm.in.group_hnd = group_hnd;

            if(!cac_SamGetGroupMembers(hnd, mem_ctx, &ggm)) {
               fprintf(stderr, "Could not get group members. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            printf("Group has %d members:\n", ggm.out.num_members);

            if(ggm.out.num_members == 0) /*just skip the rest of this case*/
               break;

            /**get the user names*/
            gn.in.dom_hnd = sod.out.dom_hnd;
            gn.in.num_rids = ggm.out.num_members;
            gn.in.rids = ggm.out.rids;

            if(!cac_SamGetNamesFromRids(hnd, mem_ctx, &gn)) {
               fprintf(stderr, "Could not lookup names. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            for(i = 0; i < gn.out.num_names; i++) {
               printf("RID: 0x%x Name: %s\n", gn.out.map[i].rid, gn.out.map[i].name);
            }

            break;

         case 'd': /*delete group*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            if(!cac_SamDeleteGroup(hnd, mem_ctx, group_hnd)) {
               fprintf(stderr, "Could not delete group. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Deleted group.\n");
               group_hnd = NULL;
            }
            break;

         case 'u': /*list users*/
            ZERO_STRUCT(eu);

            eu.in.dom_hnd = sod.out.dom_hnd;
            
            while(cac_SamEnumUsers(hnd, mem_ctx, &eu)) {
               for(i = 0; i < eu.out.num_users; i++) {
                  printf(" RID: 0x%x Name: %s\n", eu.out.rids[i], eu.out.names[i]);
               }
            }

            if(CAC_OP_FAILED(hnd->status)) {
               printf("Could not enumerate users. Error: %s\n", nt_errstr(hnd->status));
            }

            break;

         case 'a': /*add member to group*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(add);

            add.in.group_hnd = group_hnd;

            printf("Enter user RID: 0x");
            scanf("%x", &add.in.rid);

            if(!cac_SamAddGroupMember(hnd, mem_ctx, &add)) {
               fprintf(stderr, "Could not add user to group. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Successfully added user to group\n");
            }
            break;

         case 'r': /*remove user from group*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(del);
            del.in.group_hnd = group_hnd;

            printf("Enter RID: 0x");
            scanf("%x", &del.in.rid);

            if(!cac_SamRemoveGroupMember(hnd, mem_ctx, &del)) {
               fprintf(stderr, "Could not remove user from group. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Removed user from group.\n");
            }

            break;

         case 'x': /*clear group members*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            if(!cac_SamClearGroupMembers(hnd, mem_ctx, group_hnd)) {
               fprintf(stderr, "Could not clear group members. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Cleared group members\n");
            }

            break;
            
         case 's': /*set members*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(set);

            set.in.group_hnd = group_hnd;

            printf("Enter the number of members: ");
            scanf("%d", &set.in.num_members);

            set.in.rids = TALLOC_ARRAY(mem_ctx, uint32, set.in.num_members);

            for(i = 0; i < set.in.num_members; i++) {
               printf("Enter RID #%d: 0x", (i+1));
               scanf("%x", (set.in.rids + i));
            }

            if(!cac_SamSetGroupMembers(hnd, mem_ctx, &set)) {
               printf("could not set members. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Set users\n");
            }

            break;
            
         case 'g': /*list groups for user*/
            ZERO_STRUCT(ou);
            ZERO_STRUCT(gg);

            printf("Enter username: ");
            cactest_readline(stdin, input);

            if(input[0] != '\0') {
               ou.in.name = talloc_strdup(mem_ctx, input);
            }
            else {
               printf("Enter RID: 0x");
               scanf("%x", &ou.in.rid);
            }
            
            ou.in.access   = MAXIMUM_ALLOWED_ACCESS;
            ou.in.dom_hnd  = sod.out.dom_hnd;

            if(!cac_SamOpenUser(hnd, mem_ctx, &ou)) {
               fprintf(stderr, "Could not open user %s. Error: %s\n", ou.in.name, nt_errstr(hnd->status));
               break;
            }

            /*now find the groups*/
            gg.in.user_hnd = ou.out.user_hnd;

            if(!cac_SamGetGroupsForUser(hnd, mem_ctx, &gg)) {
               fprintf(stderr, "Could not get groups for user. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            cac_SamClose(hnd, mem_ctx, ou.out.user_hnd);

            ZERO_STRUCT(gn);

            gn.in.dom_hnd = sod.out.dom_hnd;
            gn.in.num_rids = gg.out.num_groups;
            gn.in.rids  = gg.out.rids;

            if(!cac_SamGetNamesFromRids(hnd, mem_ctx, &gn)) {
               fprintf(stderr, "Could not get names from RIDs. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            printf("%d groups: \n", gn.out.num_names);

            for(i = 0; i < gn.out.num_names; i++) {
               printf("RID: 0x%x ", gn.out.map[i].rid);

               if(gn.out.map[i].found)
                  printf("Name: %s\n", gn.out.map[i].name);
               else
                  printf("Unknown RID\n");
            }

            break;

         case 'z': /*close group*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            if(!cac_SamClose(hnd, mem_ctx, group_hnd)) {
               printf("Could not close group\n");
               break;
            }

            group_hnd = NULL;
            break;

         case 'i': /*get group info*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(gi);
            gi.in.group_hnd = group_hnd;

            if(!cac_SamGetGroupInfo(hnd, mem_ctx, &gi)) {
               printf("Could not get group info. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Retrieved Group info\n");
               print_cac_group_info(gi.out.info);
            }
            
            break;

         case 'e': /*edit group info*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(gi);
            ZERO_STRUCT(si);

            gi.in.group_hnd = group_hnd;
            
            if(!cac_SamGetGroupInfo(hnd, mem_ctx, &gi)) {
               printf("Could not get group info. Error: %s\n", nt_errstr(hnd->status));
               break;
            }

            edit_cac_group_info(mem_ctx, gi.out.info);

            si.in.group_hnd = group_hnd;
            si.in.info      = gi.out.info;

            if(!cac_SamSetGroupInfo(hnd, mem_ctx, &si)) {
               printf("Could not set group info. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf(" Done.\n");
            }

            break;

         case 'n': /*rename group*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(rg);

            printf("Enter new group name: ");
            cactest_readline(stdin, tmp);

            rg.in.group_hnd = group_hnd;
            rg.in.new_name = talloc_strdup(mem_ctx, tmp);

            if(!cac_SamRenameGroup(hnd, mem_ctx, &rg)) 
               printf("Could not rename group. Error: %s\n", nt_errstr(hnd->status));
            else
               printf("Done.\n");

            break;
         case 't': /*get security info*/
            if(!group_hnd) {
               printf("Must open group first!\n");
               break;
            }

            ZERO_STRUCT(gso);

            gso.in.pol = group_hnd;

            if(!cac_SamGetSecurityObject(hnd, mem_ctx, &gso)) {
               printf("Could not get security descriptor info. Error: %s\n", nt_errstr(hnd->status));
            }
            else {
               printf("Got it.\n");
            }
            break;
            
         case 'q':
            break;

         default:
            printf("Invalid command\n");
      }
   }

   cac_SamClose(hnd, mem_ctx, sod.out.dom_hnd);

   if(group_hnd)
      cac_SamClose(hnd, mem_ctx, group_hnd);

done:
   cac_FreeHandle(hnd);

   talloc_destroy(mem_ctx);

   return 0;
}

