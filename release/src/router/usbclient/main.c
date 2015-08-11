#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <utime.h>
#include <sys/types.h>
#include "SMB.h"

char g_workgroup[MAX_BUFF_SIZE];
char g_username[MAX_BUFF_SIZE];
char g_password[MAX_BUFF_SIZE];
char g_server[MAX_BUFF_SIZE];
char g_share[MAX_BUFF_SIZE];

Config smb_config, smb_config_stop;

int no_config;
int exit_loop;
int stop_progress;

int disk_change;
int sighandler_finished;
int sync_disk_removed;
int mountflag;

int local_sync;
int server_sync;

Node *newNode;
CloudFile *FileList_one;
CloudFile *FileTail_one;
CloudFile *FolderList_one;
CloudFile *FolderTail_one;
CloudFile *FolderTmp_one;
CloudFile *FileList_size;
CloudFile *FileTail_size;
CloudFile *FolderTmp_size;
CloudFile *TreeFolderList;
CloudFile *TreeFileList;
CloudFile *TreeFolderTail;
CloudFile *TreeFileTail;
CloudFile *FolderTmp;
Server_TreeNode *ServerNode_del,*ServerNode_List;
sync_list **g_pSyncList;

pthread_t newthid1,newthid2,newthid3,newthid4;
pthread_cond_t cond,cond_socket,cond_log;
pthread_mutex_t mutex,mutex_socket,mutex_socket_1,mutex_receve_socket,mutex_log,mutex_rename,mutex_checkdisk;

struct tokenfile_info_tag *tokenfile_info,*tokenfile_info_start,*tokenfile_info_tmp;

void my_replace(char *str,char newchar,char oldchar){

        int i;
        int len;
        len = strlen(str);
        for(i=0; i<len; i++)
        {
                if(str[i] == oldchar)
                {
                        str[i] = newchar;
                }
        }
}

int create_smbclientconf(Config *config)
{
        char *nvp = NULL;
#ifndef NVRAM_
        FILE *fp1 = fopen(TMP_CONFIG, "r");
        if(fp1 == NULL){
                //printf("open - %s failed\n", TMP_CONFIG);DEBUG
                return -1;
        }
        //printf("open - %s success\n", TMP_CONFIG);
        fseek(fp1, 0L, SEEK_END);
        int file_size = ftell(fp1);
        fseek(fp1, 0L, SEEK_SET);
        nvp =  my_malloc(file_size);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(nvp==NULL)
            return  -1;
        fread(nvp, file_size, sizeof(char), fp1);
        fclose(fp1);
#else
#ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD, "cloud_sync", tmp);
        nvp = my_malloc(strlen(tmp) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(nvp==NULL)
            return -1;
        sprintf(nvp, "%s", tmp);
#else
        nvp = strdup(nvram_safe_get("cloud_sync"));
#endif
#endif

        my_replace(nvp, '\0', '\n');
        //printf("nvp = %s\n", nvp);
        char *b, *buf, *buffer, *p;
        int i = 0;
        FILE *fp = fopen(CONFIG_PATH, "w");
        while ((b = strsep(&nvp, "<")) != NULL)
        {
                //i++;
                buf = buffer = strdup(b);

                //printf("buffer = %s\n",buffer);

                while((p = strsep(&buffer, ">")) != NULL)
                {
                        //printf("i = %d, p = %s\n", i, p);

                        if(i == 0)
                        {
                                if(atoi(p) != NV_PORT)
                                        break;

                                fprintf(fp, "%s", p);
                        }
                        else
                        {
                                if((i % 8) == 0)
                                {
                                        if(atoi(p) != NV_PORT)
                                                break;
                                }
                                else if((i % 8) != 0)
                                        fprintf(fp, "\n%s", p);
                        }
                        i++;
                }
                free(buf);

        }
        free(nvp);
        fclose(fp);
        //printf("i = %d, num = %d\n", i, (i % 7));
        config->dir_num = (i % 7);
        //printf("create_smbclientconf() - end\n");
        return 0;
}

int parse_config(Config *config)
{
        char buffer[MAX_BUFF_SIZE];
        int i = 0;
        int k = 0;

        FILE *fp = NULL;
        if(( fp = fopen(CONFIG_PATH, "rb")) == NULL )
        {
                fprintf(stderr, "read Cloud error");
                exit(-1);
        }
        config->multrule = (MultRule **)malloc(sizeof(MultRule*) * config->dir_num);
        while(fgets(buffer, 256, fp) != NULL)
        {
                if( buffer[ strlen(buffer) - 1 ] == '\n' )
                        buffer[ strlen(buffer) - 1 ] = '\0';
                if(i == (k * 7)){
                        config->id = atoi(buffer);
                        DEBUG("config->id = %d\n", config->id);
                        //printf("config->id = %d\n", config->id);
                }else if(i == (k * 7) + 1){
                        config->multrule[k] = (MultRule*)malloc(sizeof(MultRule));
                        memset(config->multrule[k],0,sizeof(MultRule));
                        sprintf(config->multrule[k]->workgroup, "%s", buffer);
                }else if(i == (k * 7) + 2){
                        sprintf(config->multrule[k]->serverIP, "%s", buffer);
                }else if(i == (k * 7) + 3){
                        sprintf(config->multrule[k]->shareFolder, "%s", buffer);
                        sprintf(config->multrule[k]->server_root_path, "%s/%s", config->multrule[k]->serverIP, config->multrule[k]->shareFolder);
                }else if(i == (k * 7) + 4){
                        sprintf(config->multrule[k]->acount, "%s", buffer);
                }else if(i == (k * 7) + 5){
                        sprintf(config->multrule[k]->password, "%s", buffer);
                }else if(i == (k * 7) + 6){
                        config->multrule[k]->rule = atoi(buffer);
                }else if(i == (k * 7) + 7){
                        sprintf(config->multrule[k]->client_root_path, "%s", buffer);
                        char *p = config->multrule[k]->client_root_path;
                        int x = 0;
                        while(p[0] != '\0'){
                                if(p[0] == '/')
                                        x++;
                                if(x == 4)
                                        break;
                                p++;
                        }
                        //printf("p = %s\n", p);
                        DEBUG("p = %s\n", p);
                        snprintf(config->multrule[k]->mount_path, strlen(config->multrule[k]->client_root_path) - strlen(p) + 1, "%s", config->multrule[k]->client_root_path);

//                        printf("config->multrule[k]->workgroup = %s\n", config->multrule[k]->workgroup);
//                        printf("config->multrule[%d]->server_root_path = %s\n", k, config->multrule[k]->server_root_path);
//                        printf("config->multrule[%d]->acount = %s\n", k, config->multrule[k]->acount);
//                        printf("config->multrule[%d]->password = %s\n", k, config->multrule[k]->password);
//                        printf("config->multrule[%d]->rule = %d\n", k, config->multrule[k]->rule);
//                        printf("config->multrule[%d]->client_root_path = %s\n", k, config->multrule[k]->client_root_path);
//                        printf("config->multrule[%d]->mount_path = %s\n", k, config->multrule[k]->mount_path);
                        DEBUG("config->multrule[k]->workgroup = %s\n", config->multrule[k]->workgroup);
                        DEBUG("config->multrule[%d]->server_root_path = %s\n", k, config->multrule[k]->server_root_path);
                        DEBUG("config->multrule[%d]->acount = %s\n", k, config->multrule[k]->acount);
                        DEBUG("config->multrule[%d]->password = %s\n", k, config->multrule[k]->password);
                        DEBUG("config->multrule[%d]->rule = %d\n", k, config->multrule[k]->rule);
                        DEBUG("config->multrule[%d]->client_root_path = %s\n", k, config->multrule[k]->client_root_path);
                        DEBUG("config->multrule[%d]->mount_path = %s\n", k, config->multrule[k]->mount_path);

                        k++;
                }
                i++;
        }
        fclose(fp);
        int m;
        for(m = 0; m < config->dir_num; m++)
        {
            DEBUG("m = %d\n", m);
            my_mkdir_r(config->multrule[m]->client_root_path, m);  //have mountpath
        }
        return 0;
}

int read_config()
{
        create_smbclientconf(&smb_config);
        parse_config(&smb_config);

        //exit(0);
        if(smb_config.dir_num == 0)
        {
                no_config = 1;
                return;
        }

        if(!no_config)
        {
                exit_loop = 0;
                if(get_tokenfile_info() == -1)
                {
                        DEBUG("\nget_tokenfile_info failed\n");
                        exit(-1);
                }
                check_config_path(1);
        }
        return 0;
}

void init_global_var()
{

        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_init(&mutex_socket, NULL);
        pthread_mutex_init(&mutex_socket_1, NULL);
        pthread_mutex_init(&mutex_receve_socket, NULL);
        pthread_mutex_init(&mutex_rename, NULL);

        int num = smb_config.dir_num;
        printf("num = %d\n", smb_config.dir_num);
        g_pSyncList = (sync_list **)malloc(sizeof(sync_list *) * num);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(g_pSyncList==NULL)
            return;
        memset(g_pSyncList, 0, sizeof(g_pSyncList));

        ServerNode_del = NULL;
        int i;
        for(i = 0; i < num; i++)
        {
                g_pSyncList[i] = (sync_list *)malloc(sizeof(sync_list));
                memset(g_pSyncList[i], 0, sizeof(sync_list));

                g_pSyncList[i]->index = i;
                g_pSyncList[i]->receve_socket = 0;
                g_pSyncList[i]->have_local_socket = 0;
                g_pSyncList[i]->first_sync = 1;
                g_pSyncList[i]->no_local_root = 0;
                g_pSyncList[i]->init_completed = 0;
                g_pSyncList[i]->ServerRootNode = NULL;
                g_pSyncList[i]->OldServerRootNode = NULL;
                g_pSyncList[i]->OrigServerRootNode = NULL;
                g_pSyncList[i]->IsNetWorkUnlink = 0;//2014.11.20 by sherry ip or network不通
                g_pSyncList[i]->sync_disk_exist = 0;
                //create head
                g_pSyncList[i]->SocketActionList = (Node *)malloc(sizeof(Node));
                g_pSyncList[i]->SocketActionList->cmdName = NULL;
                g_pSyncList[i]->SocketActionList->next = NULL;

                sprintf(g_pSyncList[i]->temp_path, "%s/.smartsync/usb_client", smb_config.multrule[i]->mount_path);
                my_mkdir_r(g_pSyncList[i]->temp_path, i);

                char *base_path_tmp = my_malloc(sizeof(smb_config.multrule[i]->client_root_path));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(base_path_tmp==NULL)
                    return;
                sprintf(base_path_tmp, "%s", smb_config.multrule[i]->client_root_path);
                replace_char_in_str(base_path_tmp, '_', '/');
                snprintf(g_pSyncList[i]->up_item_file, 255, "%s/%s_up_item", g_pSyncList[i]->temp_path, base_path_tmp);
                free(base_path_tmp);

                g_pSyncList[i]->SocketActionList_Rename = (Node *)malloc(sizeof(Node));
                g_pSyncList[i]->SocketActionList_Rename->cmdName = NULL;
                g_pSyncList[i]->SocketActionList_Rename->next = NULL;

                g_pSyncList[i]->server_action_list = create_action_item_head();
                g_pSyncList[i]->dragfolder_action_list = create_action_item_head();
                g_pSyncList[i]->unfinished_list = create_action_item_head();
                g_pSyncList[i]->up_space_not_enough_list = create_action_item_head();
                g_pSyncList[i]->copy_file_list = create_action_item_head();
                if(smb_config.multrule[i]->rule == 1)
                {
                        g_pSyncList[i]->download_only_socket_head = create_action_item_head();
                }

                tokenfile_info_tmp = tokenfile_info_start->next;
                while(tokenfile_info_tmp != NULL)
                {
                        //printf("tokenfile_info_tmp->mountpath = %s\n", tokenfile_info_tmp->mountpath);
                        if(!strcmp(tokenfile_info_tmp->mountpath, smb_config.multrule[i]->mount_path))
                        {
                                g_pSyncList[i]->sync_disk_exist = 1;
                                break;
                        }
                        tokenfile_info_tmp = tokenfile_info_tmp->next;
                }
        }
}

int send_action(int type, char *content,int port)
{
        int sockfd;
        char str[1024] = {0};

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {             //创建socket套接字
                perror("socket");
                return -1;
        }

        struct sockaddr_in their_addr; /* connector's address information */
        bzero(&(their_addr), sizeof(their_addr)); /* zero the rest of the struct */
        their_addr.sin_family = AF_INET; /* host byte order */
        their_addr.sin_port = htons(port); /* short, network byte order */
        their_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        bzero(&(their_addr.sin_zero), sizeof(their_addr.sin_zero)); /* zero the rest of the struct */

        if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
                perror("connect");
                return -1;
        }

        sprintf(str,"%d@%s", type, content);
        //printf("ID&path = %s\n", str);

        if (send(sockfd, str, strlen(str), 0) == -1) {
                perror("send");
                return -1;
        }

        close(sockfd);
        return 0;
}

void send_to_inotify()
{
        int i;
        for(i = 0; i < smb_config.dir_num; i++){
                printf("send_action base_path = %s\n", smb_config.multrule[i]->client_root_path);
                send_action(smb_config.id, smb_config.multrule[i]->client_root_path, INOTIFY_PORT);
                usleep(1000*500);
        }
}

void show(Node* head)
{
        printf("##################Sockets from inotify...###################\n");
        Node *pTemp = head->next;
        while(pTemp!=NULL)
        {
                printf(">>>>%s\n", pTemp->cmdName);
                pTemp = pTemp->next;
        }
}

void add_socket_item(char *buf, int i)
{

        pthread_mutex_lock(&mutex_receve_socket);
        g_pSyncList[i]->receve_socket = 1;
        pthread_mutex_unlock(&mutex_receve_socket);
        newNode = (Node *)malloc(sizeof(Node));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newNode==NULL)
            return;
        memset(newNode, 0, sizeof(Node));
        newNode->cmdName = my_malloc(strlen(buf) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newNode->cmdName==NULL)
            return;
        //memset(newNode->cmdName, '\0', sizeof(newNode->cmdName));
        sprintf(newNode->cmdName, "%s", buf);

        Node *pTemp = g_pSyncList[i]->SocketActionList;
        while(pTemp->next != NULL)
                pTemp = pTemp->next;
        pTemp->next = newNode;
        newNode->next = NULL;

        //show(g_pSyncList[i]->SocketActionList);
        //2014.10.11 by sherry
        #ifdef __DEBUG__
        show(g_pSyncList[i]->SocketActionList);
        #endif
        return;
}

int deal_dragfolder_to_socketlist(char *dir, int index)
{

        int status;
        struct dirent *ent = NULL;
        char info[512];
        memset(info,0,sizeof(info));
        DIR *pDir;
        pDir = opendir(dir);
        if(pDir != NULL)
        {
                while((ent=readdir(pDir)) != NULL)
                {
                        if(ent->d_name[0] == '.')
                                continue;
                        if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name, ".."))
                                continue;
                        char *fullname;
                        fullname = my_malloc(strlen(dir) + strlen(ent->d_name) + 2);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(fullname==NULL)
                            return -1;
                        sprintf(fullname, "%s/%s", dir, ent->d_name);
                        if(test_if_dir(fullname) == 1)
                        {
                                sprintf(info, "createfolder%s%s%s%s","\n", dir, "\n", ent->d_name);
                                pthread_mutex_lock(&mutex_socket);
                                add_socket_item(info, index);
                                pthread_mutex_unlock(&mutex_socket);
                                status = deal_dragfolder_to_socketlist(fullname, index);
                        }
                        else
                        {
                                sprintf(info, "createfile%s%s%s%s", "\n", dir, "\n", ent->d_name);
                                pthread_mutex_lock(&mutex_socket);
                                add_socket_item(info, index);
                                pthread_mutex_unlock(&mutex_socket);
                        }
                        free(fullname);
                }
                closedir(pDir);
                return 0;
        }
}

int socket_check(char *dir, char *name, int index)
{
        printf("here is socket check\n");
        int res = 0;
        char *dir1,*dir2;

        char *sock_buf;
        char *part_one;
        char *part_two;
        char *part_three;
        char *part_four;

        char *p = NULL;
        char *q = NULL;

        char *fullpath = my_malloc(strlen(dir) + strlen(name) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(fullpath==NULL)
            return -1;
        sprintf(fullpath, "%s/%s", dir,name);

        printf("fullpath = %s\n", fullpath);

        Node *pTemp = g_pSyncList[index]->SocketActionList->next;
        while(pTemp != NULL)
        {
                printf("%s\n", pTemp->cmdName);
                if(!strncmp(pTemp->cmdName, "move", 4) || !strncmp(pTemp->cmdName, "rename", 6))
                {
                        sock_buf = my_malloc(strlen(pTemp->cmdName) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(sock_buf==NULL)
                            return -1;
                        sprintf(sock_buf, "%s", pTemp->cmdName);
                        char *ret = strchr(sock_buf, '\n');
                        part_one = my_malloc(strlen(sock_buf) - strlen(ret) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(part_one==NULL)
                            return -1;
                        snprintf(part_one, strlen(sock_buf) - strlen(ret) + 1, "%s", sock_buf);
                        char *ret1 = strchr(ret + 1, '\n');
                        part_two = my_malloc(strlen(ret + 1) - strlen(ret1) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(part_two==NULL)
                            return -1;
                        snprintf(part_two, strlen(ret + 1) - strlen(ret1) + 1, "%s", ret + 1);
                        char *ret2 = strchr(ret1 + 1, '\n');
                        part_three = my_malloc(strlen(ret1 + 1) - strlen(ret2) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(part_three==NULL)
                            return -1;
                        snprintf(part_three,strlen(ret1 + 1) - strlen(ret2) + 1, "%s", ret1 + 1);
                        part_four = my_malloc(strlen(ret2 + 1) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(part_four==NULL)
                            return -1;
                        snprintf(part_four,strlen(ret2 + 1) + 1, "%s", ret2 + 1);

                       //printf("1:%s\n", part_one);
                        //printf("2:%s\n", part_two);
                       // printf("3:%s\n", part_three);
                       printf("4:%s\n", part_four);

                        if(!strncmp(part_one, "move", 4))
                        {
                                dir1 = my_malloc(strlen(part_two) + strlen(part_four) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                if(dir1==NULL)
                                    return -1;
                                dir2 = my_malloc(strlen(part_three) + strlen(part_four) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                if(dir2==NULL)
                                    return -1;
                        }
                        else if(!strncmp(part_one, "rename", 6))
                        {
                                dir1 = my_malloc(strlen(part_two) + strlen(part_three) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                if(dir1==NULL)
                                    return -1;
                                dir2 = my_malloc(strlen(part_two) + strlen(part_four) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                if(dir2==NULL)
                                    return -1;
                        }
                        p = strstr(dir1, fullpath);
                        q = strstr(dir2, fullpath);
                        if(p != NULL || q != NULL)
                        {
                                res = 1;
                                free(sock_buf);
                                free(part_one);
                                free(part_two);
                                free(part_three);
                                free(part_four);
                                break;
                        }
                        free(sock_buf);
                        free(part_one);
                        free(part_two);
                        free(part_three);
                        free(part_four);
                }
                else
                {
                        dir1 = my_malloc(strlen(dir) + 3);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(dir1==NULL)
                            return -1;
                        dir2 = my_malloc(strlen(dir) + 3);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(dir2==NULL)
                            return -1;
                        sprintf(dir1, "\n%s\n", dir);
                        sprintf(dir2, "\n%s/", dir);

                        //printf("dir1:%s\n", dir1);
                        //printf("dir2:%s\n", dir2);

                        p = strstr(pTemp->cmdName,dir1);
                        q = strstr(pTemp->cmdName,dir2);
                        if(p != NULL || q != NULL)
                        {
                                res = 1;
                                free(dir1);
                                free(dir2);
                                break;
                        }
                        free(dir1);
                        free(dir2);
                }
                pTemp = pTemp->next;
        }
        free(fullpath);
        return res;
}

int moveFolder(char *old_dir, char *new_dir, int index)
{
        //SMB_rm(old_dir, index);
        USB_rm(old_dir, index);
        int status;

        struct dirent *ent = NULL;
        DIR *pDir;
        pDir = opendir(new_dir);

        if(pDir != NULL )
        {
                //status = SMB_mkdir(new_dir, index);
                status = USB_mkdir(new_dir, index);
                if(status == 0)
                {
                        while ((ent = readdir(pDir)) != NULL)
                        {
                                if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
                                        continue;

                                char *old_fullname = my_malloc(strlen(old_dir) + strlen(ent->d_name) + 2);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                if(old_fullname==NULL)
                                    return -1;
                                char *new_fullname = my_malloc(strlen(new_dir) + strlen(ent->d_name) + 2);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                if(new_fullname==NULL)
                                    return -1;

                                sprintf(new_fullname, "%s/%s", new_dir, ent->d_name);
                                sprintf(old_fullname, "%s/%s", old_dir, ent->d_name);
                                if(socket_check(new_dir, ent->d_name, index) == 1)
                                        continue;

                                if(test_if_dir(new_fullname) == 1)
                                {
                                        status = moveFolder(old_fullname, new_fullname, index);
                                }
                                else
                                {
                                        //status = SMB_upload(new_fullname,index);
                                    status = USB_upload(new_fullname,index);
                                        if(status == 0)
                                        {
                                                char *serverpath = localpath_to_serverpath(new_fullname, index);
                                                time_t modtime = Getmodtime(serverpath, index);
                                                free(serverpath);
                                                if(ChangeFile_modtime(new_fullname, modtime, index))
                                                {
                                                        //printf("ChangeFile_modtime failed!\n");
                                                }
                                        }
                                }
                                free(new_fullname);
                                free(old_fullname);
                        }

                }
                closedir(pDir);
                return status;
        }
        else
        {
                //printf("open %s fail \n", new_dir);
                return LOCAL_FILE_LOST;
        }
}

int cmd_parser(char *cmd, int index)
{

        if( strstr(cmd, "(conflict)") != NULL )
                return 0;

        //printf("socket command is %s \n", cmd);
        if( !strncmp(cmd, "exit", 4))
        {
                //printf("exit socket\n");
                return 0;
        }

        if(!strncmp(cmd, "rmroot", 6))
        {
                g_pSyncList[index]->no_local_root = 1;
                return 0;
        }

        char cmd_name[64];
        char *path;
        char *temp;
        char filename[256];
        char *fullname = NULL;
        char oldname[256],newname[256];
        char *oldpath = NULL;
        char action[64];
        char *cmp_name;
        char *mv_newpath;
        char *mv_oldpath;
        char *ch;
        int status = 0;

        memset(cmd_name, '\0', sizeof(cmd_name));
        memset( oldname, '\0', sizeof( oldname));
        memset( newname, '\0', sizeof( newname));
        memset(  action, '\0', sizeof(  action));
        memset(  filename, '\0', sizeof( filename));

        ch = cmd;
        int i = 0;
        while(*ch != '\n')
        {
                i++;
                ch++;
        }

        memcpy(cmd_name, cmd, i);

        char *p = NULL;
        ch++;
        i++;

        temp = my_malloc((size_t)(strlen(ch) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(temp==NULL)
            return -1;
        strcpy(temp, ch);
        p = strchr(temp, '\n');

        path = my_malloc((size_t)(strlen(temp) - strlen(p) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(path==NULL)
            return -1;

        if(p != NULL)
                snprintf(path, strlen(temp) - strlen(p) + 1, "%s", temp);

        p++;
        if(strncmp(cmd_name, "rename", 6) == 0)
        {
                char *p1 = NULL;

                p1 = strchr(p, '\n');

                if(p1 != NULL)
                        strncpy(oldname, p, strlen(p)- strlen(p1));

                p1++;

                strcpy(newname, p1);
                //printf("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n", cmd_name, path, oldname, newname);
                if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
                {
                        free(temp);
                        free(path);
                        return 0;
                }
        }
        else if(strncmp(cmd_name, "move", 4) == 0)
        {
                char *p1 = NULL;
                p1 = strchr(p, '\n');

                oldpath = my_malloc((size_t)(strlen(p) - strlen(p1) + 1));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(oldpath==NULL)
                    return -1;

                if(p1 != NULL)
                        snprintf(oldpath, strlen(p) - strlen(p1) + 1, "%s", p);

                p1++;

                strcpy(oldname, p1);

                //printf("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n", cmd_name, path, oldpath, oldname);
                if(oldname[0] == '.' || (strstr(path,"/.")) != NULL)
                {
                        free(temp);
                        free(path);
                        free(oldpath);
                        return 0;
                }
        }
        else
        {
                strcpy(filename, p);
                //printf("cmd_name: [%s],path: [%s],filename: [%s]\n", cmd_name, path, filename);
                fullname = my_malloc(strlen(path) + strlen(filename) + 2);
                //2014.10.20 by sherry malloc申请内存是否成功
                if(fullname==NULL)
                    return -1;
                if(filename[0] == '.' || (strstr(path,"/.")) != NULL)
                {
                        free(temp);
                        free(path);
                        free(fullname);
                        return 0;
                }
        }

        free(temp);

        if( !strncmp(cmd_name, "rename", 6) )
        {
                cmp_name = my_malloc((size_t)(strlen(path) + strlen(newname) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(cmp_name==NULL)
                    return -1;
                sprintf(cmp_name, "%s/%s", path, newname);
        }
        else
        {
                cmp_name = my_malloc((size_t)(strlen(path) + strlen(filename) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(cmp_name==NULL)
                    return -1;
                sprintf(cmp_name, "%s/%s", path, filename);
        }

        if( strcmp(cmd_name, "createfile") == 0 )
        {
                strcpy(action, "createfile");
                del_action_item("copyfile", cmp_name, g_pSyncList[index]->copy_file_list);
        }
        else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
        {
                strcpy(action, "remove");
        }
        else if( strcmp(cmd_name, "createfolder") == 0 )
        {
                strcpy(action, "createfolder");
                del_action_item("copyfile", cmp_name, g_pSyncList[index]->copy_file_list);
        }
        else if( strncmp(cmd_name, "cancelcopy", 10) == 0)
        {
                del_action_item("copyfile", cmp_name, g_pSyncList[index]->copy_file_list);
        }
        else if( strncmp(cmd_name, "rename", 6) == 0 )
        {
                strcpy(action, "rename");
        }
        else if( strncmp(cmd_name, "move", 4) == 0 )
        {
                strcpy(action, "move");
        }

        if(g_pSyncList[index]->server_action_list->next != NULL)
        {
                action_item *item;

                item = get_action_item(action, cmp_name, g_pSyncList[index]->server_action_list, index);

                if(item != NULL)
                {

                        //printf("##### %s %s by samba Server self ######\n", action, cmp_name);
                        del_action_item(action, cmp_name, g_pSyncList[index]->server_action_list);
                        //printf("#### del action item success!\n");
                        free(path);
                        free(cmp_name);
                        return 0;
                }
        }

        if(g_pSyncList[index]->dragfolder_action_list->next != NULL)
        {
                action_item *item;

                item = get_action_item(action, cmp_name, g_pSyncList[index]->dragfolder_action_list, index);

                if(item != NULL)
                {
                        //printf("##### %s %s by dragfolder recursion self ######\n", action, cmp_name);
                        del_action_item(action, cmp_name, g_pSyncList[index]->dragfolder_action_list);
                        free(path);
                        free(cmp_name);
                        return 0;
                }
        }
        free(cmp_name);

        //printf("###### %s is start ######\n", cmd_name);

        if( strcmp(cmd_name, "copyfile") != 0 )
        {
                g_pSyncList[index]->have_local_socket = 1;
        }

        if( strcmp(cmd_name, "createfile") == 0 || strcmp(cmd_name, "dragfile") == 0 )
        {
                sprintf(fullname, "%s/%s", path, filename);
                //printf("fullname = %s\n", fullname);
                int exist = is_server_exist(fullname, index);
                if(exist)
                {
                        char *temp = change_same_name(fullname, index, 0);
                        if(temp==NULL)
                            return NULL;
                        //SMB_remo(fullname, temp, index);
                        USB_remo(fullname, temp, index);
                        char *err_msg = write_error_message("server file %s is renamed to %s", fullname, temp);
                        if(err_msg==NULL)
                            return  NULL;
                        write_conflict_log(fullname, 3, err_msg); //conflict
                        free(err_msg);
                        free(temp);
                }
                //status = SMB_upload(fullname, index);
                status = USB_upload(fullname, index);
                if(status == 0){
                        char *serverpath = localpath_to_serverpath(fullname, index);
                        //printf("serverpath = %s\n", serverpath);
                        time_t modtime = Getmodtime(serverpath, index);
                        free(serverpath);
                        if(ChangeFile_modtime(fullname, modtime, index))
                        {
                                //printf("ChangeFile_modtime failed!\n");
                        }
                }
                free(fullname);
        }
        else if( strcmp(cmd_name, "copyfile") == 0 )
        {
                sprintf(fullname, "%s/%s", path, filename);
                add_action_item("copyfile", fullname, g_pSyncList[index]->copy_file_list);
                free(fullname);
        }
        else if( strcmp(cmd_name, "modify") == 0 )
        {
                sprintf(fullname, "%s/%s", path, filename);
                char *serverpath = localpath_to_serverpath(fullname,index);
                int exist;
                exist = is_server_exist(fullname, index);
                if(!exist)       //Server has no the same file
                {
                        //status = SMB_upload(fullname, index);
                        status = USB_upload(fullname, index);
                        if(status != 0)
                        {
                                DEBUG("upload %s failed\n", fullname);
                                free(path);
                                free(fullname);
                                return status;
                        }
                        else
                        {
                                time_t modtime = Getmodtime(serverpath, index);
                                if(ChangeFile_modtime(fullname, modtime, index))
                                {
                                        DEBUG("ChangeFile_modtime failed!\n");
                                }
                                free(fullname);
                                free(serverpath);
                        }
                }
                else
                {
                        CloudFile *cloud_file = NULL;
                        //printf("init_completed=%d\n",g_pSyncList[index]->init_completed);
                        if(g_pSyncList[index]->init_completed)
                                cloud_file = get_CloudFile_node(g_pSyncList[index]->OldServerRootNode, serverpath, 0x2,index);
                        else
                            cloud_file = get_CloudFile_node(g_pSyncList[index]->ServerRootNode, serverpath, 0x2,index);

                        //printf("rule=%d\n",smb_config.multrule[index]->rule);
                        if(smb_config.multrule[index]->rule == 2)
                        {
                                if(cloud_file == NULL)
                                {
                                        char *temp = change_same_name(fullname, index, 0);
                                        if(temp==NULL)
                                            return NULL;
                                        //SMB_remo(fullname, temp, index);
                                        USB_remo(fullname, temp, index);
                                        free(temp);
                                        //status = SMB_upload(fullname, index);
                                        status = USB_upload(fullname, index);
                                        if(status != 0)
                                        {
                                                free(path);
                                                return status;
                                        }
                                        else
                                        {

                                                char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                                                write_conflict_log(fullname,3,err_msg); //conflict
                                                free(err_msg);
                                                time_t modtime = Getmodtime(serverpath, index);
                                                if(ChangeFile_modtime(fullname, modtime, index))
                                                {
                                                        DEBUG("ChangeFile_modtime failed!\n");
                                                }
                                                free(fullname);
                                                free(serverpath);
                                        }
                                }
                                else
                                {
                                        if(cloud_file->ismodify)
                                        {
                                                //printf("cloud_file->ismodify=1\n");
                                                char *temp = change_same_name(fullname, index, 0);
                                                if(temp==NULL)
                                                    return NULL;
                                                //SMB_remo(fullname,temp,index);
                                                USB_remo(fullname,temp,index);
                                                free(temp);
                                                //status = SMB_upload(fullname, index);
                                                status = USB_upload(fullname, index);
                                                cloud_file->ismodify = 0;
                                                if(status != 0)
                                                {
                                                        DEBUG("modify failed status = %d\n",status);
                                                        free(path);
                                                        return status;
                                                }
                                                else
                                                {
                                                        char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                                                        write_conflict_log(fullname, 3, err_msg); //conflict
                                                        free(err_msg);
                                                        DEBUG("serverpath = %s\n", serverpath);
                                                        time_t modtime = Getmodtime(serverpath,index);
                                                        if(ChangeFile_modtime(fullname, modtime, index))
                                                        {
                                                                DEBUG("ChangeFile_modtime failed!\n");
                                                        }
                                                        free(fullname);
                                                        free(serverpath);
                                                }
                                        }
                                        else
                                        {
                                                time_t modtime1;
                                                time_t modtime2;
                                                modtime1 = Getmodtime(serverpath, index);
                                                modtime2 = cloud_file->modtime;
                                                if(modtime1 != modtime2)
                                                {
                                                        char *temp = change_same_name(fullname, index, 0);
                                                        if(temp==NULL)
                                                            return NULL;
                                                        //SMB_remo(fullname, temp, index);
                                                        USB_remo(fullname, temp, index);
                                                        free(temp);
                                                        //status = SMB_upload(fullname, index);
                                                        status = USB_upload(fullname, index);
                                                        if(status != 0)
                                                        {
                                                                DEBUG("modify failed status = %d\n",status);
                                                                free(path);
                                                                return status;
                                                        }
                                                        else
                                                        {
                                                                char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                                                                write_conflict_log(fullname, 3, err_msg); //conflict
                                                                free(err_msg);
                                                                DEBUG("serverpath = %s\n",serverpath);
                                                                time_t modtime = Getmodtime(serverpath, index);
                                                                if(ChangeFile_modtime(fullname, modtime, index))
                                                                {
                                                                        DEBUG("ChangeFile_modtime failed!\n");
                                                                }
                                                                free(fullname);
                                                                free(serverpath);
                                                        }
                                                }
                                                else
                                                {
                                                        //status = SMB_upload(fullname, index);
                                                        status = USB_upload(fullname, index);
                                                        if(status != 0)
                                                        {
                                                                DEBUG("modify failed status = %d\n",status);
                                                                free(path);
                                                                return status;
                                                        }
                                                        else
                                                        {
                                                                DEBUG("serverpath = %s\n", serverpath);
                                                                time_t modtime = Getmodtime(serverpath, index);
                                                                if(ChangeFile_modtime(fullname, modtime, index))
                                                                {
                                                                        DEBUG("ChangeFile_modtime failed!\n");
                                                                }
                                                                free(fullname);
                                                                free(serverpath);
                                                        }
                                                }
                                        }
                                }
                        }
                        else
                        {
                                time_t modtime1;
                                time_t modtime2;
                                modtime1 = Getmodtime(serverpath,index);
                                if(cloud_file == NULL)
                                {
                                        modtime2 = (time_t)-1;
                                }
                                else
                                {
                                        modtime2 = cloud_file->modtime;
                                }
                                if(modtime1 == modtime2)
                                {
                                        //status = SMB_upload(fullname,index);
                                        status = USB_upload(fullname,index);
                                        if(status != 0)
                                        {
                                                DEBUG("modify failed status = %d\n",status);
                                                free(path);
                                                return status;
                                        }
                                        else
                                        {
                                                DEBUG("serverpath = %s\n", serverpath);
                                                time_t modtime = Getmodtime(serverpath, index);
                                                if(ChangeFile_modtime(fullname, modtime, index))
                                                {
                                                        DEBUG("ChangeFile_modtime failed!\n");
                                                }
                                                free(fullname);
                                                free(serverpath);
                                        }
                                }
                                else
                                {
                                        char *temp = change_same_name(fullname, index, 0);
                                        if(temp==NULL)
                                            return NULL;
                                        //SMB_remo(fullname, temp, index);
                                        USB_remo(fullname, temp, index);
                                        free(temp);
                                        //status = SMB_upload(fullname, index);
                                        status = USB_upload(fullname, index);
                                        if(status != 0)
                                        {
                                                DEBUG("modify failed status = %d\n",status);
                                                free(path);
                                                return status;
                                        }
                                        else
                                        {
                                                char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                                                write_conflict_log(fullname, 3, err_msg); //conflict
                                                free(err_msg);
                                                DEBUG("serverpath = %s\n", serverpath);
                                                time_t modtime = Getmodtime(serverpath, index);
                                                if(ChangeFile_modtime(fullname, modtime, index))
                                                {
                                                        DEBUG("ChangeFile_modtime failed!\n");
                                                }
                                                free(fullname);
                                                free(serverpath);
                                        }
                                }
                        }
                }
        }
        else if(strcmp(cmd_name, "delete") == 0  || strcmp(cmd_name, "remove") == 0)
        {
                sprintf(fullname, "%s/%s", path, filename);
                //status = SMB_rm(fullname, index);
                status = USB_rm(fullname, index);
                free(fullname);
        }
        else if(strncmp(cmd_name, "move", 4) == 0 || strncmp(cmd_name, "rename", 6) == 0)
        {
                if(strncmp(cmd_name, "move",4) == 0)
                {
                        mv_newpath = my_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(mv_newpath==NULL)
                            return -1;
                        mv_oldpath = my_malloc((size_t)(strlen(oldpath) + strlen(oldname) + 2));
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(mv_oldpath==NULL)
                            return -1;
                        sprintf(mv_newpath, "%s/%s", path, oldname);
                        sprintf(mv_oldpath, "%s/%s", oldpath, oldname);
                        free(oldpath);
                }
                else
                {
                        mv_newpath = my_malloc((size_t)(strlen(path)+strlen(newname)+2));
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(mv_newpath==NULL)
                            return -1;
                        mv_oldpath = my_malloc((size_t)(strlen(path)+strlen(oldname)+2));
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(mv_oldpath==NULL)
                            return -1;
                        sprintf(mv_newpath, "%s/%s", path, newname);
                        sprintf(mv_oldpath, "%s/%s", path, oldname);
                }
                if(strncmp(cmd_name, "rename", 6) == 0)
                {
                        int exist = is_server_exist(mv_newpath, index);
                        if(!exist)
                        {
                                //status = SMB_remo(mv_oldpath, mv_newpath, index);
                            status = USB_remo(mv_oldpath, mv_newpath, index);
                        }
                        else
                        {
                                char *temp = change_same_name(mv_newpath, index, 0);
                                if(temp==NULL)
                                    return NULL;
                                //SMB_remo(mv_newpath, temp, index);
                                USB_remo(mv_newpath, temp, index);
                                char *err_msg = write_error_message("server file %s is renamed to %s", mv_newpath, temp);
                                write_conflict_log(mv_newpath, 3, err_msg); //conflict
                                free(err_msg);
                                free(temp);
                                //status = SMB_remo(mv_oldpath, mv_newpath, index);
                                status = USB_remo(mv_oldpath, mv_newpath, index);
                        }
                }
                else    //move
                {
                        int exist = 0;
                        int old_index;
                        old_index = get_path_to_index(mv_oldpath);
                        if(index == old_index)
                        {
                                exist = is_server_exist(mv_newpath, index);
                                if(!exist)
                                {
                                        //status = SMB_remo(mv_oldpath, mv_newpath, index);
                                    status = USB_remo(mv_oldpath, mv_newpath, index);
                                }
                                else
                                {
                                        char *temp = change_same_name(mv_newpath, index, 0);
                                        if(temp==NULL)
                                            return NULL;
                                        //SMB_remo(mv_newpath, temp, index);
                                        USB_remo(mv_newpath, temp, index);
                                        char *err_msg = write_error_message("server file %s is renamed to %s", mv_newpath, temp);
                                        write_conflict_log(mv_newpath, 3, err_msg); //conflict
                                        free(err_msg);
                                        free(temp);
                                        //status = SMB_remo(mv_oldpath, mv_newpath, index);
                                        status = USB_remo(mv_oldpath, mv_newpath, index);
                                }

                        }
                       // else if(old_index==NULL)//2014.10.17 by sherry malloc申请内存是否成功
                        //{
                           // return NULL;
                        //}
                        else
                        {
                                if(smb_config.multrule[old_index]->rule == 1)
                                {
                                        del_download_only_action_item("move", mv_oldpath, g_pSyncList[old_index]->download_only_socket_head);
                                }
                                else
                                {
                                        //SMB_rm(mv_oldpath, old_index);
                                        USB_rm(mv_oldpath, old_index);
                                }

                                if(test_if_dir(mv_newpath))
                                        status = moveFolder(mv_oldpath, mv_newpath, index);
                                        //if(status==NULL)
                                            //return NULL;
                                else
                                {
                                        //status = SMB_upload(mv_newpath, index);
                                        status = USB_upload(mv_newpath, index);

                                        if(status == 0)
                                        {
                                                char *serverpath = localpath_to_serverpath(mv_newpath, index);
                                                time_t modtime = Getmodtime(serverpath, index);
                                                free(serverpath);
                                                if(ChangeFile_modtime(mv_newpath, modtime, index))
                                                {
                                                        //printf("ChangeFile_modtime failed!\n");
                                                }
                                        }
                                }
                        }
                }

                free(mv_oldpath);
                free(mv_newpath);

        }
        else if(strcmp(cmd_name, "dragfolder") == 0)
        {
                sprintf(fullname,"%s/%s", path, filename);

                char info[512] = {0};
                sprintf(info, "createfolder%s%s%s%s", "\n", path, "\n", filename);
                pthread_mutex_lock(&mutex_socket);
                add_socket_item(info, index);
                pthread_mutex_unlock(&mutex_socket);
                status = deal_dragfolder_to_socketlist(fullname,index);
                free(fullname);
        }
        else if(strcmp(cmd_name, "createfolder") == 0)
        {
                sprintf(fullname, "%s/%s", path, filename);
                int exist = is_server_exist(fullname, index);
                if(!exist)
                {
                    //status = SMB_mkdir(fullname, index);
                    status = USB_mkdir(fullname, index);
                }
                 free(fullname);
        }

        free(path);
        return status;
}

int add_all_download_only_socket_list(char *cmd, const char *dir, int index)
{
        struct dirent* ent = NULL;
        char *fullname;
        int fail_flag = 0;

        DIR *dp = opendir(dir);

        if(dp == NULL)
        {
                //printf("opendir %s fail", dir);
                fail_flag = 1;
                return -1;
        }

        add_action_item(cmd, dir, g_pSyncList[index]->download_only_socket_head);

        while (NULL != (ent = readdir(dp)))
        {

                if(ent->d_name[0] == '.')
                        continue;
                if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                        continue;

                fullname = my_malloc((size_t)(strlen(dir)+strlen(ent->d_name)+2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(fullname==NULL)
                    return -1;

                sprintf(fullname, "%s/%s", dir, ent->d_name);

                if( test_if_dir(fullname) == 1)
                {
                        add_all_download_only_socket_list("createfolder", fullname, index);
                }
                else
                {
                        add_action_item("createfile", fullname, g_pSyncList[index]->download_only_socket_head);
                }
                free(fullname);
        }

        closedir(dp);

        return (fail_flag == 1) ? -1 : 0;
}

int download_only_add_socket_item(char *cmd, int index)
{
        //printf("running download_only_add_socket_item: %s\n",cmd);

        if( strstr(cmd, "(conflict)") != NULL )
                return 0;

        if( !strncmp(cmd, "exit", 4))
        {
                //printf("exit socket\n");
                return 0;
        }

        if(!strncmp(cmd, "rmroot", 6))
        {
                g_pSyncList[index]->no_local_root = 1;
                return 0;
        }

        char cmd_name[64] = {0};
        char *path = NULL;
        char *temp = NULL;
        char filename[256] = {0};
        char *fullname = NULL;
        char oldname[256] = {0}, newname[256] = {0};
        char *oldpath = NULL;
        char action[64] = {0};
        char *ch = NULL;
        char *old_fullname = NULL;

        //        memset(cmd_name, '\0', sizeof(cmd_name));
        //        memset(oldname, '\0', sizeof(oldname));
        //        memset(newname, '\0', sizeof(newname));
        //        memset(action, '\0', sizeof(action));

        ch = cmd;
        int i = 0;
        while(*ch != '\n')
        {
                i++;
                ch++;
        }

        memcpy(cmd_name, cmd, i);

        char *p = NULL;
        ch++;
        i++;
        temp = my_malloc((size_t)(strlen(ch) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(temp==NULL)
            return -1;

        strcpy(temp, ch);
        p = strchr(temp, '\n');

        path = my_malloc((size_t)(strlen(temp) - strlen(p) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(path==NULL)
            return -1;

        //DEBUG("path = %s\n",path);

        if(p != NULL)
                snprintf(path, strlen(temp) - strlen(p) + 1, "%s", temp);

        p++;
        if(strncmp(cmd_name, "rename", 6) == 0)
        {
                char *p1 = NULL;

                p1 = strchr(p, '\n');

                if(p1 != NULL)
                        strncpy(oldname, p, strlen(p) - strlen(p1));

                p1++;

                strcpy(newname, p1);
                //printf("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n", cmd_name, path, oldname, newname);
        }
        else if(strncmp(cmd_name, "move", 4) == 0)
        {
                char *p1 = NULL;
                p1 = strchr(p, '\n');

                oldpath = my_malloc((size_t)(strlen(p) - strlen(p1) + 1));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(oldpath==NULL)
                    return -1;

                if(p1 != NULL)
                        snprintf(oldpath,strlen(p) - strlen(p1) + 1, "%s", p);

                p1++;

                strcpy(oldname, p1);

                //printf("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n", cmd_name, path, oldpath, oldname);
        }
        else
        {
                strcpy(filename, p);
                //fullname = my_malloc((size_t)(strlen(path)+strlen(filename)+2));
                //printf("cmd_name: [%s],path: [%s],filename: [%s]\n", cmd_name, path, filename);
        }

        free(temp);

        if( !strncmp(cmd_name, "rename", 6) )
        {
                fullname = my_malloc((size_t)(strlen(path) + strlen(newname) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(fullname==NULL)
                    return -1;
                old_fullname = my_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(old_fullname==NULL)
                    return -1;
                sprintf(fullname,"%s/%s",path,newname);
                sprintf(old_fullname,"%s/%s",path,oldname);
                free(path);
        }
        else if( !strncmp(cmd_name, "move", 4) )
        {
                fullname = my_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(fullname==NULL)
                    return -1;
                old_fullname = my_malloc((size_t)(strlen(oldpath) + strlen(oldname) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(old_fullname==NULL)
                    return -1;
                sprintf(fullname, "%s/%s", path, oldname);
                sprintf(old_fullname, "%s/%s", oldpath, oldname);
                free(oldpath);
                free(path);
        }
        else
        {
                fullname = my_malloc((size_t)(strlen(path) + strlen(filename) + 2));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(fullname==NULL)
                    return -1;
                sprintf(fullname, "%s/%s", path, filename);
                free(path);
        }

        if( !strncmp(cmd_name, "copyfile", 8) )
        {
                add_action_item("copyfile", fullname, g_pSyncList[index]->copy_file_list);
                free(fullname);
                return 0;
        }
        //2014-01-28
        if( !strncmp(cmd_name, "cancelcopy", 10))
        {
                del_action_item("copyfile", fullname, g_pSyncList[index]->copy_file_list);
                free(fullname);
                return 0;
        }

        if( strcmp(cmd_name, "createfile") == 0 )
        {
                strcpy(action, "createfile");
                del_action_item("copyfile", fullname, g_pSyncList[index]->copy_file_list);
        }
        else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
        {
                strcpy(action,"remove");
                del_download_only_action_item(action, fullname, g_pSyncList[index]->download_only_socket_head);
        }
        else if( strcmp(cmd_name, "createfolder") == 0 )
        {
                strcpy(action, "createfolder");
        }
        else if( strncmp(cmd_name, "rename", 6) == 0 )
        {
                strcpy(action, "rename");
                del_download_only_action_item(action, old_fullname, g_pSyncList[index]->download_only_socket_head);
                free(old_fullname);
        }
        else if( strncmp(cmd_name, "move", 4) == 0 )
        {
                strcpy(action,"move");
                del_download_only_action_item(action,old_fullname,g_pSyncList[index]->download_only_socket_head);
                //free(old_fullname);
        }

        if(g_pSyncList[index]->server_action_list->next != NULL)
        {
                action_item *item;
                //printf("%s:%s\n", action, fullname);
                item = get_action_item(action, fullname, g_pSyncList[index]->server_action_list, index);

                if(item != NULL)
                {
                        //printf("##### %s %s by SMB Server self ######\n", action, fullname);
                        //pthread_mutex_lock(&mutex);
                        del_action_item(action, fullname, g_pSyncList[index]->server_action_list);
                        free(fullname);
                        return 0;
                }
        }

        if(g_pSyncList[index]->dragfolder_action_list->next != NULL)
        {
                action_item *item;

                item = get_action_item(action, fullname, g_pSyncList[index]->dragfolder_action_list, index);

                if(item != NULL)
                {
                        //printf("##### %s %s by dragfolder recursion self ######\n", action, fullname);
                        del_action_item(action, fullname, g_pSyncList[index]->dragfolder_action_list);
                        free(fullname);
                        return 0;
                }
        }

        if( strcmp(cmd_name, "copyfile") != 0 )
        {
                g_pSyncList[index]->have_local_socket = 1;
        }

        if(strncmp(cmd_name, "rename", 6) == 0)
        {
                if(test_if_dir(fullname))
                {
                        add_all_download_only_socket_list(cmd_name, fullname, index);
                }
                else
                {
                        add_action_item(cmd_name, fullname, g_pSyncList[index]->download_only_socket_head);
                }
        }
        else if(strncmp(cmd_name, "move", 4) == 0)
        {
                int old_index = get_path_to_index(old_fullname);
                if(old_index == index)
                {
                        if(test_if_dir(fullname))
                        {
                                add_all_download_only_socket_list(cmd_name, fullname, index);
                        }
                        else
                        {
                                add_action_item(cmd_name, fullname, g_pSyncList[index]->download_only_socket_head);
                        }
                }
                //else if(old_index==NULL)//2014.10.17 by sherry malloc申请内存是否成功
                //{
                  //  return NULL;
                //}
                else
                {
                    if(smb_config.multrule[old_index]->rule == 1)
                    {
                        del_download_only_action_item("", old_fullname, g_pSyncList[old_index]->download_only_socket_head);
                    }
                    else
                    {
                        //SMB_rm(old_fullname, old_index);
                         USB_rm(old_fullname, old_index);
                    }
                    if(test_if_dir(fullname))
                    {
                        add_all_download_only_socket_list(cmd_name, fullname, index);
                    }
                    else
                    {
                        add_action_item(cmd_name, fullname, g_pSyncList[index]->download_only_socket_head);
                    }
                }

                free(old_fullname);
        }
        else if(strcmp(cmd_name, "createfolder") == 0 || strcmp(cmd_name, "dragfolder") == 0)
        {
                add_action_item(cmd_name, fullname, g_pSyncList[index]->download_only_socket_head);
                add_all_download_only_dragfolder_socket_list(fullname, index);
        }
        else if( strcmp(cmd_name, "createfile") == 0  || strcmp(cmd_name, "dragfile") == 0 || strcmp(cmd_name, "modify") == 0)
        {
                add_action_item(cmd_name, fullname, g_pSyncList[index]->download_only_socket_head);
        }

        free(fullname);
        return 0;
}

int add_socket_item_for_rename(char *buf, int i)
{
        pthread_mutex_lock(&mutex_receve_socket);
        g_pSyncList[i]->receve_socket = 1;
        pthread_mutex_unlock(&mutex_receve_socket);

        newNode = (Node *)malloc(sizeof(Node));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newNode==NULL)
            return -1;
        memset(newNode, 0, sizeof(Node));
        newNode->cmdName = my_malloc(strlen(buf) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newNode->cmdName==NULL)
            return -1;
        sprintf(newNode->cmdName, "%s", buf);

        //newNode->re_cmd = NULL;

        Node *pTemp = g_pSyncList[i]->SocketActionList_Rename;
        //Node *pTemp = ActionList;
        while(pTemp->next != NULL)
                pTemp=pTemp->next;
        pTemp->next = newNode;
        newNode->next = NULL;
        show(g_pSyncList[i]->SocketActionList_Rename);
        return 0;
}

char *str_replace(int start, int len, char *origStr, char *tarStr)
{
        char *newStr = NULL;
        char temp1[256] = {0};
        char temp2[256] = {0};
        char *p = origStr;
        p = p + start;
        snprintf(temp1, strlen(origStr) - strlen(p) + 1, "%s", origStr);
        p = p + len;
        sprintf(temp2, "%s", p);
        newStr = my_malloc(strlen(temp1) + strlen(temp2) + strlen(tarStr) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newStr==NULL)
            return NULL;
        sprintf(newStr, "%s%s%s", temp1, tarStr, temp2);

        return newStr;
}

void set_re_cmd(char *oldpath, char *oldpath_1, char *oldpath_2, char *newpath, char *newpath_1, char *newpath_2, int i)
{
        //printf("************************set_re_cmd***********************\n");
        char *p = NULL;
        char *q = NULL;
        int start, len;
        //string socket_old, socket_new;
        char *socket_old, *socket_new;
        Node *pTemp = g_pSyncList[i]->SocketActionList->next;
        while(pTemp != NULL)
        {
                p = strstr(pTemp->cmdName, oldpath_1);
                q = strstr(pTemp->cmdName, oldpath_2);
                socket_old = pTemp->cmdName;
                if(p != NULL)
                {
                        start = strlen(pTemp->cmdName) - strlen(p);
                        len = strlen(oldpath_1);
                        socket_new = str_replace(start, len, socket_old, newpath_1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(socket_new==NULL)
                            return;
                        free(pTemp->cmdName);
                        pTemp->cmdName = my_malloc(strlen(socket_new) + 1);
                        sprintf(pTemp->cmdName, "%s", socket_new);
                }
                if(q != NULL)
                {
                        start = strlen(pTemp->cmdName) - strlen(q);
                        len = strlen(oldpath_2);
                        socket_new = str_replace(start, len, socket_old, newpath_2);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(socket_new==NULL)
                            return;
                        free(pTemp->cmdName);
                        pTemp->cmdName = my_malloc(strlen(socket_new) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(pTemp->cmdName==NULL)
                            return;
                        sprintf(pTemp->cmdName, "%s", socket_new);
                }

                pTemp=pTemp->next;
        }

        action_item *item = g_pSyncList[i]->copy_file_list->next;
        while(item != NULL)
        {
                int res = strncmp(item->path, oldpath_2, strlen(oldpath_2));
                socket_old = item->path;
                if(0 == res)
                {
                        len = strlen(oldpath_2);
                        socket_new = str_replace(0, len, socket_old, newpath_2);
                        free(item->path);
                        item->path = my_malloc(strlen(socket_new) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(item->path==NULL)
                            return;
                        sprintf(item->path, "%s", socket_new);
                }
                item = item->next;
        }
}

int handle_rename_socket(char *buf, int i)
{
        char *oldpath;
        char *oldpath_1;
        char *oldpath_2;
        char *newpath;
        char *newpath_1;
        char *newpath_2;
        char cmd_name[512] = {0};
        char oldname[512] = {0};
        char newname[512] = {0};
        char *path;
        char *temp;
        char *ch = buf;
        int k = 0;
        while(*ch != '\n')
        {
                k++;
                ch++;
        }
        memcpy(cmd_name, buf, k);
        char *p = NULL;
        ch++;
        k++;
        temp = my_malloc((size_t)(strlen(ch) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(temp==NULL)
            return -1;
        strcpy(temp, ch);
        p = strchr(temp, '\n');
        path = my_malloc((size_t)(strlen(temp) - strlen(p) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(p == NULL)
                return -1;
        snprintf(path, strlen(temp) - strlen(p) + 1, "%s", temp);
        p++;
        char *p1 = NULL;
        p1 = strchr(p, '\n');
        if(p1 != NULL)
                strncpy(oldname, p, strlen(p) - strlen(p1));
        p1++;
        strcpy(newname, p1);

        oldpath = my_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(oldpath==NULL)
            return -1;
        oldpath_1 = my_malloc((size_t)(strlen(path) + strlen(oldname) + 3));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(oldpath_1==NULL)
            return -1;
        oldpath_2 = my_malloc((size_t)(strlen(path) + strlen(oldname) + 3));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(oldpath_2==NULL)
            return -1;
        newpath = my_malloc((size_t)(strlen(path) + strlen(newname) + 2));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newpath==NULL)
            return -1;
        newpath_1 = my_malloc((size_t)(strlen(path) + strlen(newname) + 3));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newpath_1==NULL)
            return -1;
        newpath_2 = my_malloc((size_t)(strlen(path) + strlen(newname) + 3));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(newpath_2==NULL)
            return -1;
        sprintf(oldpath, "%s/%s", path, oldname);
        sprintf(oldpath_1, "%s/%s\n", path, oldname);
        sprintf(oldpath_2, "%s/%s/", path, oldname);
        sprintf(newpath, "%s/%s", path, newname);
        sprintf(newpath_1, "%s/%s\n", path, newname);
        sprintf(newpath_2, "%s/%s/", path, newname);
        free(path);
        free(temp);

        set_re_cmd(oldpath, oldpath_1, oldpath_2, newpath, newpath_1, newpath_2, i);

        free(oldpath);
        free(oldpath_1);
        free(oldpath_2);
        free(newpath);
        free(newpath_1);
        free(newpath_2);
        return 0;
}

void *Save_Socket(void *argc)
{
        DEBUG("Save_Socket:%u\n", pthread_self());
        int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd*/
        int numbytes;
        char buf[1024] = {0};
        int yes;
        int ret;

        fd_set read_fds;
        fd_set master;
        int fdmax;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_ZERO(&master);

        struct sockaddr_in my_addr; /* my address information */
        struct sockaddr_in their_addr; /* connector's address information */
        socklen_t sin_size;
        //int sin_size;

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                perror("socket");
                exit(1);
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
                perror("Server-setsockopt() error lol!");
                exit(1);
        }

        my_addr.sin_family = AF_INET; /* host byte order */
        my_addr.sin_port = htons(MYPORT); /* short, network byte order */
        my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
        bzero(&(my_addr.sin_zero), sizeof(my_addr.sin_zero)); /* zero the rest of the struct */

        if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
                perror("bind");
                exit(1);
        }
        if (listen(sockfd, 100) == -1) {
                perror("listen");
                exit(1);
        }
        sin_size = sizeof(struct sockaddr_in);

        FD_SET(sockfd, &master);
        fdmax = sockfd;

        while(!exit_loop)
        { /* main accept() loop */
                //printf("listening!\n");
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                read_fds = master;

                ret = select(fdmax+1, &read_fds, NULL, NULL, &timeout);

                switch (ret)
                {
                case 0:
                        continue;
                        break;
                case -1:
                        perror("select");
                        continue;
                        break;
                default:
                        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
                                perror("accept");
                                continue;
                        }
                        memset(buf, 0, sizeof(buf));

                        if ((numbytes=recv(new_fd, buf, MAX_BUFF_SIZE, 0)) == -1) {
                                perror("recv");
                                exit(1);
                        }

                        if(buf[strlen(buf)] == '\n')
                        {
                                buf[strlen(buf)] = '\0';
                        }

                        DEBUG("buf:%s\n",buf);

                        int i;
                        char *r_path = get_socket_base_path(buf);
                        for(i = 0; i < smb_config.dir_num; i++)
                        {
                                if(!strcmp(r_path, smb_config.multrule[i]->client_root_path))
                                {
                                        free(r_path);
                                        break;
                                }
                        }



                        pthread_mutex_lock(&mutex_socket);
                        if(!strncmp(buf, "rename0", 7))
                        {
                                DEBUG("it is renamed folder!\n");
                                handle_rename_socket(buf, i);
                                add_socket_item_for_rename(buf, i);
                        }
                        else
                        {
                                add_socket_item(buf, i);
                        }
                        close(new_fd);
                        pthread_mutex_unlock(&mutex_socket);
                }
        }
        close(sockfd);

        DEBUG("stop usbclient local sync\n");

        return NULL;
}

int check_link_internet()
{
        struct timeval now;
        struct timespec outtime;
        int link_flag = 0;
        int i;
        while(!link_flag && !exit_loop)
        {
#if defined NVRAM_
#ifdef USE_TCAPI
                char *link_internet;
                char tmp[MAXLEN_TCAPI_MSG] = {0};
                tcapi_get(WANDUCK, "link_internet", tmp);
                link_internet = my_malloc(strlen(tmp) + 1);
                //2014.10.20 by sherry malloc申请内存是否成功
//                if(link_internet==NULL)
//                    return NULL;
                sprintf(link_internet, "%s", tmp);
                link_flag = atoi(link_internet);
                free(link_internet);
#else
                char *link_internet;
                link_internet = strdup(nvram_safe_get("link_internet"));
                link_flag = atoi(link_internet);
                free(link_internet);
#endif
#else
                char cmd_p[128] = {0};
                sprintf(cmd_p, "sh %s", GET_INTERNET);
                system(cmd_p);
                sleep(2);

                char nv[64] = {0};
                FILE *fp;
                fp = fopen(VAL_INTERNET, "r");
                fgets(nv, sizeof(nv), fp);
                fclose(fp);

                link_flag = atoi(nv);
                DEBUG("link_flag = %d\n",link_flag);
#endif
                if(!link_flag)
                {
                        for(i = 0; i < smb_config.dir_num; i++)
                        {
                                g_pSyncList[i]->IsNetWorkUnlink = 1;//2014.11.20 by sherry ip不对和网络连接有问题
                                write_log(S_ERROR, "Network Connection Failed.", "", i);
                        }
                        pthread_mutex_lock(&mutex);
                        if(!exit_loop)
                        {
                                gettimeofday(&now, NULL);
                                outtime.tv_sec = now.tv_sec + 20;
                                outtime.tv_nsec = now.tv_usec * 1000;
                                pthread_cond_timedwait(&cond, &mutex, &outtime);
                        }
                        pthread_mutex_unlock(&mutex);
                }
                else
                        printf("check_link_internet() - local network is ok\n");
        }
        //2014.11.20 by sherry ip不对和网络连接有问题,不显示error信息
//        for(i = 0; i < smb_config.dir_num; i++)
//        {
//                write_log(S_SYNC, "", "", i);
//        }
        for(i = 0;i < smb_config.dir_num;i++)
        {
            if(g_pSyncList[i]->IsNetWorkUnlink && !exit_loop)
            {
               write_log(S_ERROR,"check your ip or network","",i);
               g_pSyncList[i]->IsNetWorkUnlink = 0;
            }
    //        else if(g_pSyncList[i]->IsPermission && !exit_loop)//2014.11.04 by sherry sever权限不足
    //        {
    //           write_log(S_ERROR,"Permission Denied.","",i);
    //           g_pSyncList[i]->IsPermission = 0;
    //        }
            else
                write_log(S_SYNC,"","",i);

        }
}

//int check_smb_auth(Config config, int index)
//{
//        SMB_init(index);
//        int dh = -1;
//        if((dh = smbc_opendir(config.multrule[index]->server_root_path)) < 0)
//                return -1;
//        else{
//                smbc_close(dh);
//                return 0;
//        }
//}

void *SyncLocal(void *argc)
{
        DEBUG("SyncLocal:%u\n", pthread_self());
        Node *socket_execute;
        int status = 0;
        int has_socket = 0;
        int i;
        struct timeval now;
        struct timespec outtime;
        int fail_flag;

        while(!exit_loop)
        {
                for(i = 0; i < smb_config.dir_num; i++)
                {
                        fail_flag = 0;

                        while (server_sync == 1 && exit_loop ==0)
                        {
                                usleep(1000*10);
                        }
                        local_sync = 1;

                        if(disk_change)
                        {
                                check_disk_change();
                        }

                        if(exit_loop)
                                break;

                        //if(check_smb_auth(smb_config, i) == -1)
                        //{
                        //write_log(S_ERROR, "can not connect to server.", "", smb_config.multrule[i]->rule);
                        //continue;
                        //}
                        //write_log(S_SYNC, "", "", i);

                        if(g_pSyncList[i]->sync_disk_exist == 0)
                                continue;

                        if(smb_config.multrule[i]->rule == 1)          //download only
                        {
                                while(exit_loop == 0)
                                {
                                        while(g_pSyncList[i]->SocketActionList_Rename->next != NULL || g_pSyncList[i]->SocketActionList->next != NULL)
                                        {
                                                while(g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                                {
                                                        has_socket = 1;
                                                        socket_execute = g_pSyncList[i]->SocketActionList_Rename->next;
                                                        status = download_only_add_socket_item(socket_execute->cmdName,i);
                                                        if(status == 0)
                                                        {
                                                                //printf("########will del socket item##########\n");
                                                                pthread_mutex_lock(&mutex_socket);
                                                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList_Rename);
                                                                free(socket_execute);
                                                                pthread_mutex_unlock(&mutex_socket);
                                                        }
                                                        else
                                                        {
                                                                fail_flag = 1;
                                                                //printf("######## socket item fail########\n");
                                                                break;
                                                        }
                                                        usleep(1000*20);
                                                }
                                                while(g_pSyncList[i]->SocketActionList->next != NULL)
                                                {
                                                        has_socket = 1;
                                                        socket_execute = g_pSyncList[i]->SocketActionList->next;
                                                        status = download_only_add_socket_item(socket_execute->cmdName,i);
                                                        if(status == 0)
                                                        {
                                                                //printf("########will del socket item##########\n");
                                                                pthread_mutex_lock(&mutex_socket);
                                                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList);
                                                                free(socket_execute);
                                                                pthread_mutex_unlock(&mutex_socket);
                                                        }
                                                        else
                                                        {
                                                                fail_flag = 1;
                                                                //printf("######## socket item fail########\n");
                                                                break;
                                                        }
                                                        usleep(1000*20);

                                                        if(g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                                                break;
                                                }
                                        }

                                        if(fail_flag)
                                        {
                                                break;
                                        }
                                        if(g_pSyncList[i]->copy_file_list->next == NULL)
                                        {
                                                break;
                                        }
                                        else
                                        {
                                                usleep(1000*100);
                                        }
                                }
                                if(g_pSyncList[i]->dragfolder_action_list->next != NULL && g_pSyncList[i]->SocketActionList->next == NULL)
                                {
                                        free_action_item(g_pSyncList[i]->dragfolder_action_list);
                                        g_pSyncList[i]->dragfolder_action_list = create_action_item_head();
                                }
                                if(g_pSyncList[i]->server_action_list->next != NULL && g_pSyncList[i]->SocketActionList->next == NULL)
                                {
                                        free_action_item(g_pSyncList[i]->server_action_list);
                                        g_pSyncList[i]->server_action_list = create_action_item_head();
                                }
                                pthread_mutex_lock(&mutex_receve_socket);
                                if(g_pSyncList[i]->SocketActionList->next == NULL)
                                        g_pSyncList[i]->receve_socket = 0;
                                pthread_mutex_unlock(&mutex_receve_socket);
                        }
                        else                                                  //Sync
                        {
//                                if(smb_config.multrule[i]->rule == 2)     //upload only规则时，处理未完成的上传动作
//                                {

//                                }

                                while(exit_loop == 0)
                                {
                                        while(g_pSyncList[i]->SocketActionList_Rename->next != NULL || g_pSyncList[i]->SocketActionList->next != NULL)
                                        {
                                                while(g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                                {
                                                        has_socket = 1;
                                                        socket_execute = g_pSyncList[i]->SocketActionList_Rename->next;
                                                        status = cmd_parser(socket_execute->cmdName, i);
                                                        if(status == 0 || status == LOCAL_FILE_LOST)
                                                        {
                                                                pthread_mutex_lock(&mutex_socket);
                                                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList_Rename);
                                                                free(socket_execute);
                                                                pthread_mutex_unlock(&mutex_socket);
                                                        }
                                                        else
                                                        {
                                                                fail_flag = 1;
                                                                DEBUG("######## socket item fail########\n");
                                                                break;
                                                        }
                                                        usleep(1000*20);
                                                }
                                                while(g_pSyncList[i]->SocketActionList->next != NULL && exit_loop == 0)
                                                {
                                                        has_socket = 1;
                                                        socket_execute = g_pSyncList[i]->SocketActionList->next;
                                                        status = cmd_parser(socket_execute->cmdName,i);
                                                        DEBUG("status:%d\n",status);
                                                        if(status == 0 || status == LOCAL_FILE_LOST)
                                                        {
                                                                if(status == LOCAL_FILE_LOST && g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                                                        break;
                                                                DEBUG("########will del socket item##########\n");
                                                                pthread_mutex_lock(&mutex_socket);
                                                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList);
                                                                free(socket_execute);
                                                                pthread_mutex_unlock(&mutex_socket);
                                                        }
                                                        else
                                                        {
                                                                fail_flag = 1;
                                                                DEBUG("######## socket item fail########\n");
                                                                break;
                                                        }
                                                        usleep(1000*20);

                                                        if(g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                                                break;
                                                }
                                        }

                                        if(fail_flag)
                                                break;

                                        if(g_pSyncList[i]->copy_file_list->next == NULL)
                                        {
                                                break;
                                        }
                                        else
                                                usleep(1000*100);
                                }
                                if(g_pSyncList[i]->dragfolder_action_list->next != NULL && g_pSyncList[i]->SocketActionList->next == NULL)
                                {
                                        //printf("################################################ dragfolder_action_list!\n");
                                        free_action_item(g_pSyncList[i]->dragfolder_action_list);
                                        g_pSyncList[i]->dragfolder_action_list = create_action_item_head();
                                }
                                if(g_pSyncList[i]->server_action_list->next != NULL && g_pSyncList[i]->SocketActionList->next == NULL)
                                {
                                        //printf("################################################ clear server_action_list!\n");
                                        free_action_item(g_pSyncList[i]->server_action_list);
                                        g_pSyncList[i]->server_action_list = create_action_item_head();
                                }
                                pthread_mutex_lock(&mutex_receve_socket);

                                if(g_pSyncList[i]->SocketActionList->next == NULL){
                                        g_pSyncList[i]->receve_socket = 0;
                                }
                                pthread_mutex_unlock(&mutex_receve_socket);
                        }
                }
                pthread_mutex_lock(&mutex_socket);
                local_sync = 0;
                if(!exit_loop)
                {
                        gettimeofday(&now, NULL);
                        outtime.tv_sec = now.tv_sec + 2;
                        outtime.tv_nsec = now.tv_usec * 1000;
                        pthread_cond_timedwait(&cond_socket, &mutex_socket, &outtime);
                }
                pthread_mutex_unlock(&mutex_socket);
                //printf("set local_sync %d!\n",local_sync);
        }

        //printf("stop usbclient Socket_Parser\n");
        return NULL;
}

int sync_server_to_local_perform(Browse *perform_br,Local *perform_lo,int index)
{
        if(perform_br == NULL || perform_lo == NULL)
        {
                return 0;
        }

        CloudFile *foldertmp = NULL;
        CloudFile *filetmp = NULL;
        LocalFolder *localfoldertmp;
        LocalFile *localfiletmp;
        int ret = 0;

        if(perform_br->foldernumber > 0)
                foldertmp = perform_br->folderlist->next;
        if(perform_br->filenumber > 0)
                filetmp = perform_br->filelist->next;

        localfoldertmp = perform_lo->folderlist->next;
        localfiletmp = perform_lo->filelist->next;

        /****************handle files****************/
        char *filelongurl = NULL;
        char *folderlongurl = NULL;
        if(perform_br->filenumber == 0 && perform_lo->filenumber != 0)
        {
                DEBUG("serverfileNo:%d\t\tlocalfileNo:%d\n", perform_br->filenumber, perform_lo->filenumber);

                while(localfiletmp != NULL && exit_loop == 0)
                {
                        if(smb_config.multrule[index]->rule == 1)
                        {
                                action_item *item;
                                item = get_action_item("download_only", localfiletmp->path,
                                                       g_pSyncList[index]->download_only_socket_head, index);
                                if(item != NULL)
                                {
                                        localfiletmp = localfiletmp->next;
                                        continue;
                                }
                        }
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        add_action_item("remove", localfiletmp->path, g_pSyncList[index]->server_action_list);

                        action_item *pp;
                        pp = get_action_item("upload",localfiletmp->path,
                                             g_pSyncList[index]->up_space_not_enough_list, index);
                        if(pp == NULL)
                        {
                                unlink(localfiletmp->path);
                        }
                        localfiletmp = localfiletmp->next;
                }
        }
        else if(perform_br->filenumber != 0 && perform_lo->filenumber == 0)
        {
                DEBUG("serverfileNo:%d\t\tlocalfileNo:%d\n",perform_br->filenumber,perform_lo->filenumber);

                while(filetmp != NULL && exit_loop == 0)
                {
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }

                        filelongurl = reduce_memory(filetmp->href, index, 1);

                        char *localpath = serverpath_to_localpath(filelongurl, index);
                        //2014.10.17 by sherry  ，判断malloc是否成功
                        if(localpath==NULL)
                        {
                            return NULL;
                        }
                        action_item *item;
                        item = get_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list, index);

                        int cp = 0;
                        do{
                                if(exit_loop != 0)
                                {
                                        free(filelongurl);
                                        return -1;
                                }
                                cp = is_smb_file_copying(filelongurl, index);
                        }while(cp == 1);

                        usleep(100*100);
                        if(is_local_space_enough(filetmp,index))
                        {
                                add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);

                                //int status = SMB_download(filelongurl, index);
                                int status = USB_download(filelongurl, index);
                                //2014.10.20 by sherry malloc申请内存是否成功
//                                if(status==NULL)
//                                    return NULL;
                                if(status == 0)
                                {
                                        DEBUG("do_cmd ok\n");
                                        time_t modtime = Getmodtime(filelongurl, index);
                                        if(ChangeFile_modtime(localpath, modtime, index))
                                        {
                                                DEBUG("ChangeFile_modtime failed!\n");
                                        }
                                        if(item != NULL)
                                        {
                                                del_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                                        }
                                }
                        }
                        else
                        {
                                write_log(S_ERROR, "local space is not enough!", "", index);
                                if(item == NULL)
                                {
                                        add_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                                }
                        }
                        free(localpath);
                        free(filelongurl);
                        filetmp = filetmp->next;
                }
        }
        else if(perform_br->filenumber != 0 && perform_lo->filenumber != 0)
        {
                DEBUG("serverfileNo:%d\t\tlocalfileNo:%d\n", perform_br->filenumber, perform_lo->filenumber);
                DEBUG("Ergodic smb file while\n");
                while(localfiletmp != NULL && exit_loop == 0)
                {
                        int cmp = 1;
                        char *localpathtmp = strstr(localfiletmp->path, smb_config.multrule[index]->client_root_path)
                                             + strlen(smb_config.multrule[index]->client_root_path);
                        while(filetmp != NULL)
                        {
                                //char *serverpathtmp = filetmp->href;
                                //serverpathtmp = serverpathtmp + strlen(smb_config.multrule[index]->server_root_path);
                                //DEBUG("%s\t%s\n", localpathtmp, serverpathtmp);

                                if ((cmp = strcmp(localpathtmp, filetmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        filetmp = filetmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                //DEBUG("cmp:%d\n",cmp);

                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }
                                if(smb_config.multrule[index]->rule == 1)
                                {
                                        action_item *item;
                                        item = get_action_item("download_only", localfiletmp->path,
                                                               g_pSyncList[index]->download_only_socket_head, index);
                                        if(item != NULL)
                                        {
                                                DEBUG("item != NULL\n");
                                                filetmp = perform_br->filelist->next;
                                                localfiletmp = localfiletmp->next;
                                                continue;
                                        }
                                }
                                action_item *pp;
                                pp = get_action_item("upload",localfiletmp->path,
                                                     g_pSyncList[index]->up_space_not_enough_list, index);
                                if(pp == NULL)
                                {
                                        unlink(localfiletmp->path);
                                        add_action_item("remove", localfiletmp->path, g_pSyncList[index]->server_action_list);
                                }
                        }
                        else
                        {
                                if((ret = the_same_name_compare(localfiletmp, filetmp, index)) != 0)
                                {
                                        return ret;
                                }
                        }
                        filetmp = perform_br->filelist->next;
                        localfiletmp = localfiletmp->next;
                }

                filetmp = perform_br->filelist->next;
                localfiletmp = perform_lo->filelist->next;

                DEBUG("Ergodic local file while\n");
                while(filetmp != NULL && exit_loop == 0)
                {
                        //char *serverpathtmp = filetmp->href;
                        //serverpathtmp = serverpathtmp + strlen(smb_config.multrule[index]->server_root_path);
                        int cmp = 1;
                        while(localfiletmp != NULL)
                        {
                                char *localpathtmp = strstr(localfiletmp->path, smb_config.multrule[index]->client_root_path)
                                                     + strlen(smb_config.multrule[index]->client_root_path);
                                //DEBUG("%s\t%s\n", serverpathtmp, localpathtmp);

                                if ((cmp = strcmp(localpathtmp, filetmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        localfiletmp = localfiletmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                //DEBUG("cmp:%d\n),cmp");

                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }

                                filelongurl = reduce_memory(filetmp->href, index, 1);

                                char *localpath = serverpath_to_localpath(filelongurl, index);
                                //2014.10.17 by sherry  ，判断malloc是否成功
                                if(localpath==NULL)
                                {
                                    return -1;
                                }
                                action_item *item;
                                item = get_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list, index);

                                int cp = 0;
                                do{
                                        if(exit_loop == 1)
                                        {
                                                free(filelongurl);
                                                return -1;
                                        }
                                        cp = is_smb_file_copying(filelongurl, index);
                                }while(cp == 1);

                                if(is_local_space_enough(filetmp,index))
                                {
                                        add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);

                                        //int status = SMB_download(filelongurl, index);
                                        int status = USB_download(filelongurl, index);
                                        if(status == 0)
                                        {
                                                DEBUG("do_cmd ok\n");
                                                time_t modtime = Getmodtime(filelongurl, index);
                                                if(ChangeFile_modtime(localpath, modtime, index))
                                                {
                                                        DEBUG("ChangeFile_modtime failed!\n");
                                                }
                                                if(item != NULL)
                                                {
                                                        del_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                                                }
                                        }
                                }
                                else
                                {
                                        write_log(S_ERROR, "local space is not enough!", "", index);
                                        if(item == NULL)
                                        {
                                                add_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                                        }
                                }
                                free(localpath);
                                free(filelongurl);
                        }
                        filetmp = filetmp->next;
                        localfiletmp = perform_lo->filelist->next;
                }
        }

        /*************handle folders**************/
        if(perform_br->foldernumber == 0 && perform_lo->foldernumber != 0)
        {
                DEBUG("serverfolderNo:%d\t\tlocalfolderNo:%d\n", perform_br->foldernumber, perform_lo->foldernumber);
                while(localfoldertmp != NULL && exit_loop == 0)
                {
                        if(smb_config.multrule[index]->rule == 1)
                        {
                                action_item *item;
                                item = get_action_item("download_only",
                                                       localfoldertmp->path,g_pSyncList[index]->download_only_socket_head,index);
                                if(item != NULL)
                                {
                                        localfoldertmp = localfoldertmp->next;
                                        continue;
                                }
                        }
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        del_all_items(localfoldertmp->path, index);
                        localfoldertmp = localfoldertmp->next;
                }
        }
        else if(perform_br->foldernumber != 0 && perform_lo->foldernumber == 0)
        {
                DEBUG("serverfolderNo:%d\t\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
                while(foldertmp != NULL && exit_loop == 0)
                {
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }

                        folderlongurl = reduce_memory(foldertmp->href, index, 1);

                        char *localpath = my_malloc(strlen(smb_config.multrule[index]->client_root_path) + strlen(folderlongurl) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(localpath==NULL)
                            return -1;
                        char *p = folderlongurl;
                        p = p + strlen(smb_config.multrule[index]->server_root_path);
                        sprintf(localpath,"%s%s",smb_config.multrule[index]->client_root_path, p);
                        DEBUG("%s\n",localpath);

                        int exist = is_server_exist(localpath, index);
                        if(exist)
                        {
                                if(NULL == opendir(localpath))
                                {
                                        add_action_item("createfolder", localpath, g_pSyncList[index]->server_action_list);
                                        mkdir(localpath, 0777);
                                }
                        }
                        free(localpath);
                        free(folderlongurl);
                        foldertmp = foldertmp->next;
                }
        }
        else if(perform_br->foldernumber != 0 && perform_lo->foldernumber != 0)
        {
                DEBUG("serverfolderNo:%d\t\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
                while(localfoldertmp != NULL && exit_loop == 0)
                {
                        int cmp = 1;
                        char *localpathtmp = strstr(localfoldertmp->path, smb_config.multrule[index]->client_root_path)
                                             + strlen(smb_config.multrule[index]->client_root_path);
                        while(foldertmp != NULL)
                        {
                                //char *serverpathtmp = foldertmp->href;
                                //serverpathtmp = serverpathtmp + strlen(smb_config.multrule[index]->server_root_path);
                                if ((cmp = strcmp(localpathtmp, foldertmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        foldertmp = foldertmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                if(smb_config.multrule[index]->rule == 1)
                                {
                                        action_item *item;
                                        item = get_action_item("download_only",
                                                               localfoldertmp->path, g_pSyncList[index]->download_only_socket_head, index);
                                        if(item != NULL)
                                        {
                                                foldertmp = perform_br->folderlist->next;
                                                localfoldertmp = localfoldertmp->next;
                                                continue;
                                        }
                                }
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }
                                del_all_items(localfoldertmp->path,index);
                        }
                        foldertmp = perform_br->folderlist->next;
                        localfoldertmp = localfoldertmp->next;
                }

                foldertmp = perform_br->folderlist->next;
                localfoldertmp = perform_lo->folderlist->next;
                while(foldertmp != NULL && exit_loop == 0)
                {
                        int cmp = 1;
                        //char *serverpathtmp = foldertmp->href;
                        //serverpathtmp = serverpathtmp + strlen(smb_config.multrule[index]->server_root_path);
                        while(localfoldertmp != NULL)
                        {
                                char *localpathtmp = strstr(localfoldertmp->path, smb_config.multrule[index]->client_root_path)
                                                     +strlen(smb_config.multrule[index]->client_root_path);
                                if ((cmp = strcmp(localpathtmp, foldertmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        localfoldertmp = localfoldertmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }

                                folderlongurl = reduce_memory(foldertmp->href, index, 1);
                                char *localpath = serverpath_to_localpath(folderlongurl, index);
                                //2014.10.17 by sherry  ，判断malloc是否成功
                                if(localpath==NULL)
                                {
                                    return -1;
                                }
                                DEBUG("%s\n", localpath);

                                int exist = is_server_exist(localpath, index);
                                if(exist)
                                {
                                        if(NULL == opendir(localpath))
                                        {
                                                add_action_item("createfolder", localpath, g_pSyncList[index]->server_action_list);
                                                mkdir(localpath, 0777);
                                        }
                                }
                                free(localpath);
                                free(folderlongurl);
                        }
                        foldertmp = foldertmp->next;
                        localfoldertmp = perform_lo->folderlist->next;
                }
        }
        return ret;
}

int compareLocalList(int index){
        DEBUG("compareLocalList start!\n");
        int ret = 0;

        if(g_pSyncList[index]->ServerRootNode->Child != NULL)
        {
            //DEBUG("compareLocalList Now!\n");
            ret = sync_server_to_local(g_pSyncList[index]->ServerRootNode->Child, sync_server_to_local_perform, index);
        }
        else
        {
                DEBUG("ServerRootNode->Child == NULL\n");
        }
        //DEBUG("compareLocalList end!\n");
        return ret;
}

int isServerChanged(Server_TreeNode *newNode, Server_TreeNode *oldNode)
{
        int res = 1;
        int serverchanged = 0;
        if(newNode->browse == NULL && oldNode->browse == NULL)
        {
                DEBUG("########Server is not change\n");
                return 1;
        }
        else if(newNode->browse == NULL && oldNode->browse != NULL)
        {
                DEBUG("########Server changed1\n");
                return 0;
        }
        else if(newNode->browse != NULL && oldNode->browse == NULL)
        {
                DEBUG("########Server changed2\n");
                return 0;
        }
        else
        {
                if(newNode->browse->filenumber != oldNode->browse->filenumber || newNode->browse->foldernumber != oldNode->browse->foldernumber)
                {
                        DEBUG("########Server changed3\n");
                        return 0;
                }
                else
                {
                        int cmp;
                        CloudFile *newfoldertmp = NULL;
                        CloudFile *oldfoldertmp = NULL;
                        CloudFile *newfiletmp = NULL;
                        CloudFile *oldfiletmp = NULL;
                        if(newNode->browse != NULL)
                        {
                                if(newNode->browse->foldernumber > 0)
                                        newfoldertmp = newNode->browse->folderlist->next;
                                if(newNode->browse->filenumber > 0)
                                        newfiletmp = newNode->browse->filelist->next;
                        }
                        if(oldNode->browse != NULL)
                        {
                                if(oldNode->browse->foldernumber > 0)
                                        oldfoldertmp = oldNode->browse->folderlist->next;
                                if(oldNode->browse->filenumber > 0)
                                        oldfiletmp = oldNode->browse->filelist->next;
                        }

                        while (newfoldertmp != NULL || oldfoldertmp != NULL)
                        {
                                if ((cmp = strcmp(newfoldertmp->href, oldfoldertmp->href)) != 0){
                                        DEBUG("########Server changed4\n");
                                        return 0;
                                }
                                newfoldertmp = newfoldertmp->next;
                                oldfoldertmp = oldfoldertmp->next;
                        }
                        while (newfiletmp != NULL || oldfiletmp != NULL)
                        {
                                if ((cmp = strcmp(newfiletmp->href, oldfiletmp->href)) != 0)
                                {
                                        DEBUG("########Server changed5\n");
                                        return 0;
                                }
                                if (newfiletmp->modtime != oldfiletmp->modtime)
                                {
                                        DEBUG("########Server changed6\n");
                                        return 0;
                                }
                                newfiletmp = newfiletmp->next;
                                oldfiletmp = oldfiletmp->next;
                        }
                }

                if((newNode->Child == NULL && oldNode->Child != NULL) || (newNode->Child != NULL && oldNode->Child == NULL))
                {
                        DEBUG("########Server changed7\n");
                        return 0;
                }
                if((newNode->NextBrother == NULL && oldNode->NextBrother != NULL) || (newNode->NextBrother!= NULL && oldNode->NextBrother == NULL))
                {
                        DEBUG("########Server changed8\n");
                        return 0;
                }

                if(newNode->Child != NULL && oldNode->Child != NULL)
                {
                        res = isServerChanged(newNode->Child,oldNode->Child);
                        if(res == 0)
                        {
                                serverchanged = 1;
                        }
                }
                if(newNode->NextBrother != NULL && oldNode->NextBrother != NULL)
                {
                        res = isServerChanged(newNode->NextBrother,oldNode->NextBrother);
                        if(res == 0)
                        {
                                serverchanged = 1;
                        }
                }
        }
        if(serverchanged == 1)
        {
                DEBUG("########Server changed9\n");
                return 0;
        }
        else
        {
                return 1;
        }
}

int compareServerList(int index)
{
        int ret;
        DEBUG("#########compareServerList\n");

        if(g_pSyncList[index]->ServerRootNode->Child != NULL && g_pSyncList[index]->OldServerRootNode->Child != NULL)
        {
                ret = isServerChanged(g_pSyncList[index]->ServerRootNode->Child,g_pSyncList[index]->OldServerRootNode->Child);
                return ret;
        }
        else if(g_pSyncList[index]->ServerRootNode->Child == NULL && g_pSyncList[index]->OldServerRootNode->Child == NULL)
        {
                ret = 1;
                return ret;
        }
        else
        {
                ret = 0;
                return ret;
        }
}

int do_unfinished(int index)
{
        if(exit_loop)
        {
                return 0;
        }
        DEBUG("*************do_unfinished*****************\n");
        action_item *p,*p1;
        p = g_pSyncList[index]->unfinished_list->next;
        int ret;

        while(p != NULL)
        {
                if(!strcmp(p->action,"download"))
                {
                        CloudFile *filetmp = NULL;
                        filetmp = get_CloudFile_node(g_pSyncList[index]->ServerRootNode,p->path,0x2,index);
                        if(filetmp == NULL)   //if filetmp == NULL,it means server has deleted filetmp
                        {
                                DEBUG("filetmp is NULL\n");

                                p1 = p->next;
                                del_action_item("download", p->path, g_pSyncList[index]->unfinished_list);
                                p = p1;
                                continue;
                        }
                        char *longurl = reduce_memory(filetmp->href, index, 1);
                        char *localpath = serverpath_to_localpath(longurl, index);
                        //2014.10.17 by sherry  ，判断malloc是否成功
                        if(localpath==NULL)
                        {
                            return -1;
                        }
                        DEBUG("localpath = %s\n", localpath);
                        if(is_local_space_enough(filetmp, index))
                        {
                                add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                                //ret = SMB_download(longurl, index);
                                ret = USB_download(longurl, index);
                                if (ret == 0)
                                {
                                        ChangeFile_modtime(localpath,filetmp->modtime,index);
                                        p1 = p->next;
                                        del_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                        p = p1;
                                }
                                else
                                {
                                        //DEBUG("download %s failed", filetmp->href);
                                        p = p->next;
                                        //return ret;
                                }
                        }
                        else
                        {
                                write_log(S_ERROR,"local space is not enough!","",index);
                                p = p->next;
                        }
                        free(longurl);
                        free(localpath);
                }
                else if(!strcmp(p->action, "upload"))
                {
                        p1 = p->next;
                        char *serverpath;
                        serverpath = localpath_to_serverpath(p->path, index);
                        char *path_temp;
                        path_temp = my_malloc(strlen(p->path) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(path_temp==NULL)
                            return -1;
                        sprintf(path_temp,"%s", p->path);
                        //ret = SMB_upload(p->path, index);
                        ret = USB_upload(p->path, index);
                        DEBUG("********* upload ret = %d\n",ret);

                        if(ret == 0)
                        {
                                time_t modtime = Getmodtime(serverpath, index);
                                if(ChangeFile_modtime(path_temp, modtime, index) < 0)
                                {
                                        DEBUG("ChangeFile_modtime failed!\n");
                                }
                                p = p1;
                        }
                        else if(ret == LOCAL_FILE_LOST)
                        {
                                del_action_item("upload",p->path,g_pSyncList[index]->unfinished_list);
                                p = p1;
                        }
                        else
                        {
                                DEBUG("upload %s failed",p->path);
                                p = p->next;
                        }
                        free(serverpath);
                        free(path_temp);
                }
                else
                {
                        p = p->next;
                }
        }
        return 0;
}

void *SyncServer(void *argc)
{
        DEBUG("SyncServer:%u\n",pthread_self());
        //printf("SyncServer:%u\n",pthread_self());
        struct timeval now;
        struct timespec outtime;
        int status;
        int i;

        while (!exit_loop)
        {
                for(i = 0;i < smb_config.dir_num;i++)
                {
                        status = 0;
                        while (local_sync == 1 && exit_loop == 0)
                        {
                                usleep(1000*10);
                        }
                        server_sync = 1;      //server sync starting
                        DEBUG("server sync starting\n");
                        //if(check_smb_auth(smb_config, i) == -1)
                        //{
                        //write_log(S_ERROR, "can not connect to server.", "", smb_config.multrule[i]->rule);
                        //continue;
                        //}
                        //write_log(S_SYNC, "", "", i);

                        if(disk_change)
                        {
                                check_disk_change();
                        }

                        if(exit_loop)
                                break;

                        if(g_pSyncList[i]->sync_disk_exist == 0)
                        {
                                write_log(S_ERROR,"Sync disk unplug!","",i);
                                continue;
                        }

                        if(g_pSyncList[i]->no_local_root)
                        {
                                local_mkdir(smb_config.multrule[i]->client_root_path);
                                send_action(smb_config.id, smb_config.multrule[i]->client_root_path, INOTIFY_PORT);
                                usleep(1000*10);
                                g_pSyncList[i]->no_local_root = 0;
                                g_pSyncList[i]->init_completed = 0;
                        }

                        //printf("1---i=%d\n",i);
                        status = do_unfinished(i);
                        //2014.10.20 by sherry malloc申请内存是否成功
//                       // if(status==NULL)
//                            //return NULL;
                        //printf("2---i=%d\n",i);
                        //printf("2---status=%d\n",status);

                        if(status != 0 && status != -1)
                        {
                                server_sync = 0;      //server sync finished
                                //sleep(2);
                                usleep(1000*200);
                                break;
                        }

                        if(g_pSyncList[i]->init_completed == 0)
                                status = initialization();

                        //2014.11.20 by sherry add
                        if(status!=0)
                            continue;

                        if(smb_config.multrule[i]->rule == 2)
                        {
                                continue;
                        }
                        if(exit_loop == 0)
                        {
                                DEBUG("get ServerRootNode\n");
                                g_pSyncList[i]->ServerRootNode = create_server_treeroot();//init Server_TreeNode 这种节点
                                status = browse_to_tree(smb_config.multrule[i]->server_root_path, g_pSyncList[i]->ServerRootNode, i);
                                if (status != 0)
                                {
                                        DEBUG("get ServerList ERROR! \n");
                                        g_pSyncList[i]->first_sync = 1;
                                        usleep(1000*20);
                                        continue;
                                }

                                if(g_pSyncList[i]->unfinished_list->next != NULL)
                                {
                                        continue;
                                }

                                if(g_pSyncList[i]->first_sync)
                                {
                                        DEBUG("first sync!\n");
                                        g_pSyncList[i]->first_sync = 0;
                                        g_pSyncList[i]->OrigServerRootNode = g_pSyncList[i]->OldServerRootNode;
                                        //free_server_tree(g_pSyncList[i]->OldServerRootNode);
                                        g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                                        status = compareLocalList(i);
                                        free_server_tree(g_pSyncList[i]->OrigServerRootNode);
                                        if(status == 0)
                                                g_pSyncList[i]->first_sync = 0;
                                        else
                                                g_pSyncList[i]->first_sync = 1;
                                }
                                else
                                {
                                        if(smb_config.multrule[i]->rule == 0)
                                        {
                                                status = compareServerList(i);
                                        }
                                        if(status == 0 || smb_config.multrule[i]->rule == 1)
                                        {
                                                g_pSyncList[i]->OrigServerRootNode = g_pSyncList[i]->OldServerRootNode;
                                                //free_server_tree(g_pSyncList[i]->OldServerRootNode);
                                                g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                                                status = compareLocalList(i);
                                                free_server_tree(g_pSyncList[i]->OrigServerRootNode);
                                                /*g_pSyncList[i]->OrigServerRootNode = NULL;
                                                printf("free_server_tree end\n");*/
                                                if(status == 0)
                                                        g_pSyncList[i]->first_sync = 0;
                                                else
                                                        g_pSyncList[i]->first_sync = 1;
                                        }
                                        else
                                        {
                                                free_server_tree(g_pSyncList[i]->ServerRootNode);
                                                g_pSyncList[i]->ServerRootNode = NULL;
                                        }
                                }
                        }
                        write_log(S_SYNC, "", "", i);
                }
                server_sync = 0;
                //printf("--set server_sync %d!\n", server_sync);
                pthread_mutex_lock(&mutex);
                if(!exit_loop)
                {
                        gettimeofday(&now, NULL);
                        outtime.tv_sec = now.tv_sec + 5;
                        outtime.tv_nsec = now.tv_usec * 1000;
                        pthread_cond_timedwait(&cond, &mutex, &outtime);
                }
                pthread_mutex_unlock(&mutex);
                //printf("set server_sync %d!\n", server_sync);
        }
        DEBUG("stop usbclient server sync\n");
}

char *reduce_memory(char *URL, int index, int flag)
{
        char *ret = NULL;
        if(flag) //for long
        {
                ret = my_malloc(strlen(URL) + strlen(smb_config.multrule[index]->server_root_path) + 1);
                //2014.10.20 by sherry malloc申请内存是否成功
                if(ret==NULL)
                    return NULL;
                sprintf(ret, "%s%s", smb_config.multrule[index]->server_root_path, URL);
        }
        else //for short
        {
            //2014.10.15 by sherry
//            char *p = URL;
//            p = p + strlen(smb_config.multrule[index]->server_root_path);
//            ret = my_malloc(strlen(p) + 1);
//            sprintf(ret, "%s", p);

                char *p = URL;
                if(!strcmp(p,smb_config.multrule[index]->server_root_path))
                {
                    return NULL;

                }else
                {
                    p = p + strlen(smb_config.multrule[index]->server_root_path);
                    ret = my_malloc(strlen(p) + 1);
                    //2014.10.20 by sherry malloc申请内存是否成功
                    if(ret==NULL)
                        return NULL;
                    sprintf(ret, "%s", p);
                }

        }
        return ret;
}

/*int getCloudInfo(char *URL, int index)
{
        //printf("getCloudInfo() - start\n");
        int res = 0;
        struct smbc_dirent *dirptr;
        SMB_init(index);
        int dh = -1;
        if((dh = smbc_opendir(URL)) > 0){
                //printf("open success\n");
                while((dirptr = smbc_readdir(dh)) != NULL){
                        //printf("dirptr->name = %s, dirptr->type = %d\n", dirptr->name, dirptr->smbc_type);

                        if(dirptr->name[0] == '.')
                                continue;
                        if(!strcmp(dirptr->name, ".") || !strcmp(dirptr->name, ".."))
                                continue;

                        FolderTmp = (CloudFile *)malloc(sizeof(struct node));
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(FolderTmp==NULL)
                            return -1;
                        memset(FolderTmp, 0, sizeof(FolderTmp));
                        FolderTmp->filename = my_malloc(strlen(dirptr->name) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(FolderTmp->filename==NULL)
                            return -1;
                        sprintf(FolderTmp->filename, "%s", dirptr->name);

                        char *shorturl = reduce_memory(URL, index, 0);

                        //2014.10.15 by sherry
//                        FolderTmp->href = my_malloc(strlen(shorturl) + strlen(FolderTmp->filename) + 2);
//                        sprintf(FolderTmp->href, "%s/%s", shorturl, FolderTmp->filename);
//                        free(shorturl);
                        if(shorturl==NULL)
                        {
                            //FolderTmp->href = my_malloc(strlen(FolderTmp->filename) + strlen("/")+1);
                            FolderTmp->href = my_malloc(strlen(FolderTmp->filename) + 2);
                            //2014.10.20 by sherry malloc申请内存是否成功
                            if(FolderTmp->href==NULL)
                                return -1;
                            sprintf(FolderTmp->href, "/%s", FolderTmp->filename);
                        }else
                        {
                            //FolderTmp->href = my_malloc(strlen(shorturl) + strlen(FolderTmp->filename) + strlen("/")+1);
                            FolderTmp->href = my_malloc(strlen(shorturl) + strlen(FolderTmp->filename) + 2);
                            //2014.10.20 by sherry malloc申请内存是否成功
                            if(FolderTmp->href==NULL)
                                return -1;
                            sprintf(FolderTmp->href, "%s/%s", shorturl, FolderTmp->filename);
                            free(shorturl);
                        }

                        if(dirptr->smbc_type == SMBC_DIR){
                                FolderTmp->isfile = 0;
                                TreeFolderTail->next = FolderTmp;
                                TreeFolderTail = FolderTmp;
                                TreeFolderTail->next = NULL;
                        }else if(dirptr->smbc_type == SMBC_FILE){
                                struct stat info;
                                char *longurl = reduce_memory(FolderTmp->href, index, 1);
                                smbc_stat(longurl, &info);
                                free(longurl);
                                FolderTmp->modtime = info.st_mtime;
                                FolderTmp->size = info.st_size;
                                FolderTmp->isfile = 1;
                                TreeFileTail->next = FolderTmp;
                                TreeFileTail = FolderTmp;
                                TreeFileTail->next = NULL;
                        }
                }
        }else{
                //printf("getCloudInfo() - failed\n");
                res = COULD_NOT_CONNECNT_TO_SERVER;
        }
        smbc_closedir(dh);
        //printf("getCloudInfo() - end\n");
        return res;
}*/

int getCloudInfo(char *URL, int index)
{
        //printf("getCloudInfo() - start\n");
        int res = 0;
        struct dirent *dirptr;
        DIR *  dh;
        if((dh = opendir(URL))  != NULL){
                //printf("open success\n");
                while((dirptr = readdir(dh)) != NULL){
                        //printf("dirptr->d_name = %s, dirptr->d_type = %d\n", dirptr->d_name, dirptr->d_type);

                        if(dirptr->d_name[0] == '.')
                                continue;
                        if(!strcmp(dirptr->d_name, ".") || !strcmp(dirptr->d_name, ".."))
                                continue;

                        FolderTmp = (CloudFile *)malloc(sizeof(struct node));
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(FolderTmp==NULL)
                            return -1;
                        memset(FolderTmp, 0, sizeof(FolderTmp));
                        FolderTmp->filename = my_malloc(strlen(dirptr->d_name) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(FolderTmp->filename==NULL)
                            return -1;
                        sprintf(FolderTmp->filename, "%s", dirptr->d_name);

                        char *shorturl = reduce_memory(URL, index, 0);

                        //2014.10.15 by sherry
//                        FolderTmp->href = my_malloc(strlen(shorturl) + strlen(FolderTmp->filename) + 2);
//                        sprintf(FolderTmp->href, "%s/%s", shorturl, FolderTmp->filename);
//                        free(shorturl);
                        if(shorturl==NULL)
                        {
                            //FolderTmp->href = my_malloc(strlen(FolderTmp->filename) + strlen("/")+1);
                            FolderTmp->href = my_malloc(strlen(FolderTmp->filename) + 2);
                            //2014.10.20 by sherry malloc申请内存是否成功
                            if(FolderTmp->href==NULL)
                                return -1;
                            sprintf(FolderTmp->href, "/%s", FolderTmp->filename);
                        }else
                        {
                            //FolderTmp->href = my_malloc(strlen(shorturl) + strlen(FolderTmp->filename) + strlen("/")+1);
                            FolderTmp->href = my_malloc(strlen(shorturl) + strlen(FolderTmp->filename) + 2);
                            //2014.10.20 by sherry malloc申请内存是否成功
                            if(FolderTmp->href==NULL)
                                return -1;
                            sprintf(FolderTmp->href, "%s/%s", shorturl, FolderTmp->filename);
                            free(shorturl);
                        }

                        if(dirptr->d_type == DT_DIR){
                                FolderTmp->isfile = 0;
                                TreeFolderTail->next = FolderTmp;
                                TreeFolderTail = FolderTmp;
                                TreeFolderTail->next = NULL;
                        }else if(dirptr->d_type == DT_REG){
                                struct stat info;
                                char *longurl = reduce_memory(FolderTmp->href, index, 1);
                                stat(longurl, &info);
                                free(longurl);
                                FolderTmp->modtime = info.st_mtime;
                                FolderTmp->size = info.st_size;
                                FolderTmp->isfile = 1;
                                TreeFileTail->next = FolderTmp;
                                TreeFileTail = FolderTmp;
                                TreeFileTail->next = NULL;
                        }
                }
        }else{
                //printf("getCloudInfo() - failed\n");
                res = COULD_NOT_CONNECNT_TO_SERVER;
        }
        closedir(dh);
        //printf("getCloudInfo() - end\n");
        return res;
}

Browse *browseFolder(char *URL, int index)
{
        //printf("@@@@@@browseFolder start@@@@@\n");
        int status;
        int i=0;

        Browse *browse = (Browse *)malloc(sizeof(Browse));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(  browse== NULL )
        {
                //printf("create memery error\n");
                return NULL;
        }
        memset(browse, 0, sizeof(Browse));

        TreeFolderList = (CloudFile *)malloc(sizeof(CloudFile));
        memset(TreeFolderList,0,sizeof(CloudFile));
        TreeFileList = (CloudFile *)malloc(sizeof(CloudFile));
        memset(TreeFileList,0,sizeof(CloudFile));

        TreeFolderList->href = NULL;
        TreeFileList->href = NULL;

        TreeFolderTail = TreeFolderList;
        TreeFileTail = TreeFileList;
        TreeFolderTail->next = NULL;
        TreeFileTail->next = NULL;

        status = getCloudInfo(URL, index);
        //2014.10.20 by sherry malloc申请内存是否成功
//        //if(status==NULL)
//           // return NULL;
        //printf("getCloudInfo --status=%d\n",status);
        if(status != 0)
        {
                free_CloudFile_item(TreeFolderList);
                free_CloudFile_item(TreeFileList);
                TreeFolderList = NULL;
                TreeFileList = NULL;
                free(browse);
                return NULL;
        }

        browse->filelist = TreeFileList;
        browse->folderlist = TreeFolderList;

        CloudFile *de_foldercurrent, *de_filecurrent;
        de_foldercurrent = TreeFolderList->next;
        de_filecurrent = TreeFileList->next;
        while(de_foldercurrent != NULL){
                ++i;
                de_foldercurrent = de_foldercurrent->next;
        }
        browse->foldernumber = i;
        i = 0;
        while(de_filecurrent != NULL){
                ++i;
                de_filecurrent = de_filecurrent->next;
        }
        browse->filenumber = i;

         //printf("@@@@@@browseFolder end@@@@@\n");
        return browse;
}

int browse_to_tree(char *parenthref, Server_TreeNode *node, int index)
{
        if(exit_loop)
        {
                return -1;
        }

        //printf("browse_to_tree() - start\n");
        //printf("URL = %s\n", parenthref);
        int res = 0;

        Browse *br = NULL;
        int fail_flag = 0;

        Server_TreeNode *tempnode = NULL, *p1 = NULL, *p2 = NULL;
        tempnode = create_server_treeroot();             
        //tempnode->level = node->level + 1;

        tempnode->parenthref = my_malloc((size_t)(strlen(parenthref) + 1));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(tempnode->parenthref==NULL)
            return -1;

        br = browseFolder(parenthref, index);
        sprintf(tempnode->parenthref, "%s", parenthref);
        if(NULL == br)
        {
                free_server_tree(tempnode);
                //printf("browse folder failed\n");
                res = -1;
        }
        else
        {

                tempnode->browse = br;

                if(node->Child == NULL)
                {
                        node->Child = tempnode;
                }
                else
                {
                        //printf("have child\n");
                        p2 = node->Child;
                        p1 = p2->NextBrother;

                        while(p1 != NULL)
                        {
                                //printf("p1 nextbrother have\n");
                                p2 = p1;
                                p1 = p1->NextBrother;
                        }

                        p2->NextBrother = tempnode;
                        tempnode->NextBrother = NULL;
                }
                //printf("browse folder num is %d\n", br->foldernumber);
                CloudFile *de_foldercurrent;
                de_foldercurrent = br->folderlist->next;
                while(de_foldercurrent != NULL)
                {
                        char *longurl = reduce_memory(de_foldercurrent->href, index, 1);
                        if(browse_to_tree(longurl, tempnode, index) == -1)
                        {
                                fail_flag = 1;
                        }
                        free(longurl);
                        de_foldercurrent = de_foldercurrent->next;
                }
                res = (fail_flag == 1) ? -1 : 0 ;
        }
        //printf("browse_to_tree() - end\n");
        return res;
}

int write_log(int status, char *message, char *filename, int index)
{
        if(exit_loop)
        {
                return 0;
        }

        pthread_mutex_lock(&mutex_log);
        struct timeval now;
        struct timespec outtime;

        FILE *fp;
        fp = fopen(LOG_DIR, "w");
        if(fp == NULL)
        {
                printf("open logfile error.\n");
                pthread_mutex_unlock(&mutex_log);
                return -1;
        }

        if(status == S_ERROR)
        {
                printf("******** status is ERROR *******\n");
                //2014.11.20 by sherry
                //fprintf(fp,"STATUS:%d\nMESSAGE:%s\nTOTAL_SPACE:%lld\nUSED_SPACE:%lld\nRULENUM:%d\n",status,message,0,0,index);
                //字串写错 cookie抓不到值
                //fprintf(fp,"STATUS:%d\nMESSAGE:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",status,message,0);
                fprintf(fp,"STATUS:%d\nERR_MSG:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",status,message,0);
                DEBUG("Status:%d\tERR_MSG:%s\tRule:%d\n",status,message,index);
        }
        else if(status == S_DOWNLOAD)
        {
                printf("******** status is DOWNLOAD *******\n");
                fprintf(fp,"STATUS:%d\nMOUNT_PATH:%s\nFILENAME:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n", status, filename, filename, index);
                printf("Status:%d\tDownloading:%s\tRule:%d\n", status, filename, index);
        }
        else if(status == S_UPLOAD)
        {
                printf("******** status is UPLOAD *******\n");
                fprintf(fp,"STATUS:%d\nMOUNT_PATH:%s\nFILENAME:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n", status, filename, filename, index);
                printf("Status:%d\tUploading:%s\tRule:%d\n", status, filename, index);
        }
        else
        {
                if (status == S_INITIAL)
                        printf("******** other status is INIT *******\n");
                else
                        printf("******** other status is SYNC *******\n");
                fprintf(fp,"STATUS:%d\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n", status, index);
        }

        fclose(fp);

        if(!exit_loop)
        {
                gettimeofday(&now, NULL);
                outtime.tv_sec = now.tv_sec + 3;
                outtime.tv_nsec = now.tv_usec * 1000;
                pthread_cond_timedwait(&cond_socket, &mutex_log, &outtime);
        }
        pthread_mutex_unlock(&mutex_log);
        return 0;
}

char *search_newpath(char *href,int i)
{
        char *ret = my_malloc(strlen(href) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        if(ret==NULL)
            return NULL;
        sprintf(ret, "%s", href);
        Node *pTemp = g_pSyncList[i]->SocketActionList_Rename->next;
        while(pTemp != NULL)
        {
                char *oldpath;
                char *newpath;
                char cmd_name[512] = {0};
                char oldname[512] = {0};
                char newname[512] = {0};
                char *path;
                char *temp;
                char *ch = pTemp->cmdName;
                int k = 0;
                while(*ch != '\n')
                {
                        k++;
                        ch++;
                }
                memcpy(cmd_name, pTemp->cmdName, k);
                char *p = NULL;
                ch++;
                k++;
                temp = my_malloc((size_t)(strlen(ch) + 1));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(temp==NULL)
                    return NULL;
                strcpy(temp, ch);
                p = strchr(temp, '\n');
                path = my_malloc((size_t)(strlen(temp)- strlen(p)+1));
                if(p == NULL)
                    return NULL;
                snprintf(path, strlen(temp) - strlen(p) + 1, "%s", temp);
                p++;
                char *p1 = NULL;
                p1 = strchr(p, '\n');
                if(p1 != NULL)
                        strncpy(oldname, p, strlen(p) - strlen(p1));
                p1++;
                strcpy(newname, p1);
                oldpath = my_malloc((size_t)(strlen(path) + strlen(oldname) + 3));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(oldpath==NULL)
                    return NULL;
                newpath = my_malloc((size_t)(strlen(path) + strlen(newname) + 3));
                //2014.10.20 by sherry malloc申请内存是否成功
                if(newpath==NULL)
                    return NULL;
                sprintf(oldpath, "%s/%s/", path, oldname);
                sprintf(newpath, "%s/%s/", path, newname);
                free(path);
                free(temp);

                char *tmp = NULL;
                char *temp1 = ret;
                if(strncmp(ret, oldpath, strlen(oldpath)) == 0)
                {
                        int len = strlen(oldpath);
                        tmp = str_replace(0, len, temp1, newpath);
                        free(ret);
                        ret = my_malloc(strlen(tmp) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        if(ret==NULL)
                            return NULL;
                        sprintf(ret, "%s", tmp);
                }
                pTemp = pTemp->next;
        }

        if(strcmp(ret, href) == 0)
        {
                free(ret);
                return NULL;
        }
        else
        {
                return ret;
        }
}

time_t Getmodtime(char *url, int index)
{
        //SMB_init(index);
        struct stat info;
        //smbc_stat(url, &info);
        stat(url, &info);
        return info.st_mtime;
}

int ChangeFile_modtime(char *localfilepath, time_t modtime, int index)
{
        //printf("**************ChangeFile_modtime**********\n");
        char *newpath_re = search_newpath(localfilepath, index);

        struct utimbuf *ub;
        ub = (struct utimbuf *)malloc(sizeof(struct utimbuf));
        //2014.10.20 by sherry malloc申请内存是否成功
        if(ub==NULL)
            return -1;

        ub->actime = modtime;
        ub->modtime = modtime;

        if(newpath_re != NULL)
        {
                //printf("newpath_re = %s\n", newpath_re);
                utime(newpath_re, ub);
        }
        else
        {
                //printf("localfilepath = %s\n", localfilepath);
                utime(localfilepath, ub);
        }

        free(newpath_re);
        free(ub);
        return 0;
}

/*
 *0,no local socket
 *1,have local socket
*/
int wait_handle_socket(int index){

        //if(receve_socket)
        if(g_pSyncList[index]->receve_socket)
        {
                server_sync = 0;
                //while(receve_socket)
                while(g_pSyncList[index]->receve_socket || local_sync)
                {
                        //printf("lock here\n");
                        usleep(1000*100);
                }
                server_sync = 1;
                if(g_pSyncList[index]->have_local_socket)
                {
                        g_pSyncList[index]->have_local_socket = 0;
                        g_pSyncList[index]->first_sync = 1;
                        return 1;
                }
                else
                {
                        return 0;
                }
        }
        return 0;
}

/* used for initial, local syncfolder is NULL */
int initMyLocalFolder(Server_TreeNode *servertreenode,int index)
{

        int res = 0;
        if(servertreenode->browse != NULL)
        {
                CloudFile *init_foldercurrent = NULL;
                CloudFile *init_filecurrent = NULL;
                if(servertreenode->browse->foldernumber > 0)
                        init_foldercurrent = servertreenode->browse->folderlist->next;
                if(servertreenode->browse->filenumber > 0)
                        init_filecurrent = servertreenode->browse->filelist->next;

                //printf("folder num = %d\n", servertreenode->browse->foldernumber);
                //printf("file num   = %d\n", servertreenode->browse->filenumber);

				char *folderlongurl = NULL;
				char *filelongurl = NULL;
				if(init_foldercurrent != NULL)
                	folderlongurl = reduce_memory(init_foldercurrent->href, index, 1);
				if(init_filecurrent != NULL)
                	filelongurl = reduce_memory(init_filecurrent->href, index, 1);
                while(init_foldercurrent != NULL)
                {
                        char *createpath = serverpath_to_localpath(folderlongurl, index);
                        //2014.10.17 by sherry  ，判断malloc是否成功
                        if(createpath==NULL)
                        {
                            return -1;
                        }
                        if(NULL == opendir(createpath)){
                                if(wait_handle_socket(index))
                                {
                                        free(createpath);
                                        free(filelongurl);
                                        free(folderlongurl);
                                        return HAVE_LOCAL_SOCKET;
                                }
                                if(-1 == mkdir(createpath, 0777)){
                                        //printf("mkdir %s fail", createpath);
                                        exit(-1);
                                }else{
                                        add_action_item("createfolder", createpath, g_pSyncList[index]->server_action_list);
                                }
                        }
                        free(createpath);
                        init_foldercurrent = init_foldercurrent->next;
                }

                while(init_filecurrent != NULL)
                {
                        if(is_local_space_enough(init_filecurrent, index))
                        {
                                char *createpath = serverpath_to_localpath(filelongurl, index);
                                //2014.10.17 by sherry  ，判断malloc是否成功
                                if(createpath==NULL)
                                {
                                    return -1;
                                }
                                if(wait_handle_socket(index))
                                {
                                        free(createpath);
                                        free(filelongurl);
                                        free(folderlongurl);
                                        return HAVE_LOCAL_SOCKET;
                                }
                                add_action_item("createfile", createpath, g_pSyncList[index]->server_action_list);
\
                                //res = SMB_download(filelongurl, index);
                                res = USB_download(filelongurl, index);
                                //printf("initMyLocalFolder() - res = %d\n", res);
                                if(res == 0)
                                {
                                        time_t modtime = Getmodtime(filelongurl, index);
                                        if(ChangeFile_modtime(createpath, modtime, index))
                                                //printf("ChangeFile_modtime failed!\n");
                                        free(createpath);
                                }
                                else
                                {
                                        free(filelongurl);
                                        free(folderlongurl);
                                        free(createpath);
                                        return res;
                                }
                        }
                        else
                        {
                                write_log(S_ERROR,"local space is not enough!","",index);
                                add_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                        }
                        init_filecurrent = init_filecurrent->next;
                }
                free(filelongurl);
                free(folderlongurl);
        }

        if(servertreenode->Child != NULL){
                res = initMyLocalFolder(servertreenode->Child, index);
                if(res != 0)
                {
                        return res;
                }
        }

        if(servertreenode->NextBrother != NULL){
                res = initMyLocalFolder(servertreenode->NextBrother, index);
                if(res != 0)
                {
                        return res;
                }
        }

        return res;
}

int sync_local_to_server_init(Local *perform_lo,int index)
{
        DEBUG("*******sync_local_to_server_init*******\n");
        LocalFolder *localfoldertmp;
        LocalFile *localfiletmp;
        int ret = 0;

        localfoldertmp = perform_lo->folderlist->next;
        localfiletmp = perform_lo->filelist->next;

        //handle files
        while(localfiletmp != NULL)
        {
                if(smb_config.multrule[index]->rule != 1)
                {
                        DEBUG("download only\n");
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);
                        //ret = SMB_upload(localfiletmp->path, index);
                        ret = USB_upload(localfiletmp->path, index);
                        DEBUG("handle files, ret = %d\n", ret);
                        if(ret == 0)
                        {
                                char *serverpath = localpath_to_serverpath(localfiletmp->path, index);
                                time_t modtime = Getmodtime(serverpath, index);
                                if(ChangeFile_modtime(localfiletmp->path, modtime, index))
                                {
                                        printf("ChangeFile_modtime failed!\n");
                                }
                                free(serverpath);
                        }
                        else
                        {
                                return ret;
                        }
                }
                else
                {
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->download_only_socket_head);
                }

                localfiletmp = localfiletmp->next;
        }

        //handle folders
        while(localfoldertmp != NULL)
        {
                if(smb_config.multrule[index]->rule != 1)
                {
                        DEBUG("download only\n");
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        add_action_item("createfolder", localfoldertmp->path, g_pSyncList[index]->server_action_list);
                        //ret = SMB_mkdir(localfoldertmp->path, index);
                        ret = USB_mkdir(localfoldertmp->path, index);
                        DEBUG("handle folders, ret = %d\n", ret);
                        if(ret != 0)
                        {
                                return ret;
                        }
                }
                else
                {
                        DEBUG("sync or upload only");
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        add_action_item("createfolder", localfoldertmp->path, g_pSyncList[index]->download_only_socket_head);
                }

                localfoldertmp = localfoldertmp->next;
        }

        return ret;
}

int sync_local_to_server(char *path,int index)
{
        DEBUG("sync_local_to_server path = %s\n",path);

        Local *localnode;
        int ret = 0;
        int res = 0;
        LocalFolder *localfoldertmp;

        if(exit_loop)
        {
                return -1;
        }

        localnode = Find_Floor_Dir(path);

        if(localnode == NULL)
        {
                return 0;
        }

        ret = is_server_have_localpath(path, g_pSyncList[index]->ServerRootNode->Child, index);

        if(ret == 0)
        {
               DEBUG("ret=0");
                res = sync_local_to_server_init(localnode, index);
                if(res != 0  && res != LOCAL_FILE_LOST)
                {
                        DEBUG("##########res = %d\n",res);
                        free_localfloor_node(localnode);
                        return res;
                }
        }

        localfoldertmp = localnode->folderlist->next;
        while(localfoldertmp != NULL)
        {
                res = sync_local_to_server(localfoldertmp->path, index);
                if(res != 0 && res != LOCAL_FILE_LOST)
                {
                        DEBUG("##########res = %d\n",res);
                        free_localfloor_node(localnode);
                        return res;
                }

                localfoldertmp = localfoldertmp->next;
        }

        free_localfloor_node(localnode);

        return res;
}

/** 1: have same name
 ** 0: have no same name
 ** CONN_ERROR: error
 */
/*int is_server_exist(char *localpath, int index)
{
        //printf("is_server_exsit() - %s start\n", localpath);
        int res = 0;

        char *p ;

        //为了获得,如“/未命名文件夹”
//        p = p + strlen(localpath);//指针从后向前遍历
//        printf("p = %s\n", p);
//        while(p[0] != '/' && strlen(p) < strlen(localpath))
//                p--;

        //2014.10.11 by sherry
        p=strrchr(localpath,'/');
        if(p==NULL)
        {
            return NULL;
        }
        //printf("p = %s\n", p);



        char *temp = my_malloc(strlen(localpath) - strlen(p) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
//        if(temp==NULL)
//            return NULL;
        snprintf(temp, strlen(localpath) - strlen(p) + 1, "%s", localpath);
        //printf("temp = %s\n", temp);

        char *serverpath = localpath_to_serverpath(temp, index);
        free(temp);
        //printf("serverpath = %s\n", serverpath);
        SMB_init(index);
        struct smbc_dirent *dirptr;//smbc_dirent结构体见libsmbclient.h

        int dh = -1;
        if((dh = smbc_opendir(serverpath)) > 0){
                while((dirptr = smbc_readdir(dh)) != NULL){
                   //dirptr = smbc_readdir(dh)读取共享服务器上的文件夹信息
                        if(dirptr->name[0] == '.')//跳过.  .. 文件夹，是linux默认的文件夹
                                continue;
                        if(!strcmp(dirptr->name, ".") || !strcmp(dirptr->name, ".."))
                                continue;

                        //printf("dirptr->name = %s, dirptr->type = %d\n", dirptr->name, dirptr->smbc_type);
                        if(!strcmp(dirptr->name, p + 1))
                        {
                                //printf("get it!\n");
                                res = 1;
                                break;
                        }
                }
                smbc_closedir(dh);
                free(serverpath);
        }
        return res;
}*/

int is_server_exist(char *localpath, int index)
{
        //printf("is_server_exsit() - %s start\n", localpath);
        int res = 0;

        char *p ;

        //为了获得,如“/未命名文件夹”
//        p = p + strlen(localpath);//指针从后向前遍历
//        printf("p = %s\n", p);
//        while(p[0] != '/' && strlen(p) < strlen(localpath))
//                p--;

        //2014.10.11 by sherry
        p=strrchr(localpath,'/');
        if(p==NULL)
        {
            return NULL;
        }
        //printf("p = %s\n", p);



        char *temp = my_malloc(strlen(localpath) - strlen(p) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
//        if(temp==NULL)
//            return NULL;
        snprintf(temp, strlen(localpath) - strlen(p) + 1, "%s", localpath);
        //printf("temp = %s\n", temp);

        char *serverpath = localpath_to_serverpath(temp, index);
        free(temp);
        //printf("serverpath = %s\n", serverpath);

        if (access(serverpath,0) == 0)
        {
            printf("serverpath = %s is exist\n", serverpath);
            free(serverpath);
            res=1;
            return res;
        }
        else
        {
            printf("serverpath = %s is not exist\n", serverpath);
            free(serverpath);
            return res;
        }
}

/** 0:local time != server time
 *  1:|local time - server time| = 1
 *   :local time == server time
 * -1:get server modtime or local modtime failed
**/
int init_newer_file(char *localpath, int index){

        char *serverpath;
        serverpath = localpath_to_serverpath(localpath, index);

        struct stat cli_info;
        if((stat(localpath, &cli_info) == -1))
        {
                perror("stat:");
                return -1;
        }

        time_t  smb_modtime, d_modtime;

        smb_modtime = Getmodtime(serverpath, index);

        //printf("smb_modtime = %lu, cli_modtime = %lu\n", smb_modtime, cli_info.st_mtime);
        d_modtime = smb_modtime - cli_info.st_mtime;

        if(d_modtime == 0)
        {
                return 1;
        }
        else if(d_modtime == 1 || d_modtime == -1)
        {
                ChangeFile_modtime(localpath, smb_modtime, index);
                return 1;
        }
        else
        {
                return 0;
        }
}

/** 0:local time == server time
 *  1:local modify
 *  2:server modify
 *  3:local and server modify
 * -1:get server modtime or local modtime failed
**/
int sync_newer_file(char *localpath, int index, CloudFile *reloldfiletmp)
{
        //printf("sync_newer_file start!\n");
        char *serverpath;
        serverpath = localpath_to_serverpath(localpath, index);
        //printf("serverpath = %s\n",serverpath);
        time_t modtime1,modtime2,modtime3;

        modtime1 = Getmodtime(serverpath, index);
        free(serverpath);

        if(reloldfiletmp == NULL)
                return 0;
        else
                modtime3 = reloldfiletmp->modtime;

        struct stat buf;
        if( stat(localpath,&buf) == -1)
        {
                perror("stat:");
                return -1;
        }
        modtime2 = buf.st_mtime;

        //printf("localtime = %lu,servertime = %lu\n",modtime2,modtime1);

        if(modtime1 == modtime2)     //no modify
        {
                return 0;
        }
        else
        {
                if(modtime1 == modtime3 && modtime2 != modtime3) //local modify
                {
                        return 1;
                }
                else if(modtime1 != modtime3 && modtime2 == modtime3)  //server modify
                {
                        return 2;
                }
                else   //local & server modify
                {
                        return 3;
                }
        }
}

int the_same_name_compare(LocalFile *localfiletmp, CloudFile *filetmp, int index)
{
        int ret = 0;

        //printf("local:%s, server:%s\n", localfiletmp->name, filetmp->filename);
        //printf("local:%ld, server:%ld\n", localfiletmp->modtime, filetmp->modtime);

        char *longurl = reduce_memory(filetmp->href, index, 1);

        if(g_pSyncList[index]->init_completed == 1)//for server
        {
                //printf("###################the same name compare####################\n");
                CloudFile *oldfiletmp = NULL;
                oldfiletmp = get_CloudFile_node(g_pSyncList[index]->OrigServerRootNode, longurl, 0x2,index);

                ret = sync_newer_file(localfiletmp->path, index, oldfiletmp);
                if(ret == 1) //local modify
                {
                        if(smb_config.multrule[index]->rule == 0)
                        {
                                if(wait_handle_socket(index))
                                {
                                        ret = HAVE_LOCAL_SOCKET;
                                }
                                else
                                {
                                        add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);  //??
                                        //ret = SMB_upload(localfiletmp->path,index);
                                        ret = USB_upload(localfiletmp->path,index);
                                        if(ret == 0)
                                        {
                                                time_t modtime = Getmodtime(longurl, index);
                                                ChangeFile_modtime(localfiletmp->path, modtime, index);
                                                oldfiletmp->modtime = modtime;
                                                oldfiletmp->ismodify = 0;
                                        }
                                }
                        }
                        else
                        {
                                char *newname = change_same_name(localfiletmp->path, index, 1);
                                if(newname==NULL)
                                    return NULL;
                                rename(localfiletmp->path, newname);
                                char *err_msg = write_error_message("server file %s is renamed to %s", localfiletmp->path, newname);
                                write_conflict_log(localfiletmp->path, 3, err_msg); //conflict
                                free(err_msg);
                                free(newname);

                                action_item *item;
                                item = get_action_item("download", longurl, g_pSyncList[index]->unfinished_list,index);

                                if(is_local_space_enough(filetmp,index))
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                ret = HAVE_LOCAL_SOCKET;
                                        }
                                        else
                                        {
                                                add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);
                                                //ret = SMB_download(longurl, index);
                                                ret = USB_download(longurl, index);
                                                if (ret == 0)
                                                {
                                                        time_t modtime = Getmodtime(longurl, index);
                                                        ChangeFile_modtime(localfiletmp->path,modtime, index);
                                                        oldfiletmp->modtime = modtime;
                                                        oldfiletmp->ismodify = 0;

                                                        if(item != NULL)
                                                                del_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                                }
                                        }
                                }
                                else
                                {
                                        write_log(S_ERROR, "local space is not enough!", "", index);
                                        if(item == NULL)
                                                add_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                }
                        }
                }
                else if(ret == 2) //server modify
                {
                        action_item *item;
                        item = get_action_item("download", longurl, g_pSyncList[index]->unfinished_list,index);

                        if(is_local_space_enough(filetmp, index))
                        {
                                if(wait_handle_socket(index))
                                {
                                        ret = HAVE_LOCAL_SOCKET;
                                }
                                else
                                {
                                        add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);
                                        //ret = SMB_download(longurl, index);
                                        ret = USB_download(longurl, index);
                                        if (ret == 0)
                                        {
                                                time_t modtime = Getmodtime(longurl, index);
                                                ChangeFile_modtime(localfiletmp->path,modtime, index);
                                                if(item != NULL)
                                                        del_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                        }
                                }
                        }
                        else
                        {
                                write_log(S_ERROR, "local space is not enough!", "", index);
                                if(item == NULL)
                                        add_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                        }
                }
                else if(ret == 3) //need back server up
                {
                        char *newname = change_same_name(localfiletmp->path, index, 1);
                        if(newname==NULL)
                            return NULL;
                        rename(localfiletmp->path,newname);
                        char *err_msg = write_error_message("server file %s is renamed to %s", localfiletmp->path, newname);
                        write_conflict_log(localfiletmp->path, 3, err_msg); //conflict
                        free(newname);

                        action_item *item;
                        item = get_action_item("download", longurl, g_pSyncList[index]->unfinished_list, index);

                        if(is_local_space_enough(filetmp, index))
                        {
                                if(wait_handle_socket(index))
                                {
                                        ret = HAVE_LOCAL_SOCKET;
                                }
                                else
                                {
                                        add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);
                                        //ret = SMB_download(longurl, index);
                                         ret = USB_download(longurl, index);
                                        if (ret == 0)
                                        {
                                                time_t modtime = Getmodtime(longurl, index);
                                                ChangeFile_modtime(localfiletmp->path,modtime, index);
                                                oldfiletmp->modtime = modtime;
                                                oldfiletmp->ismodify = 0;
                                                if(item != NULL)
                                                        del_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                        }
                                }
                        }
                        else
                        {
                                write_log(S_ERROR,"local space is not enough!","",index);
                                if(item == NULL)
                                        add_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                        }
                }
        }
        else//init
        {
                //printf("###################the same name compare...for init...####################\n");
                ret = init_newer_file(localfiletmp->path, index);
                //printf("ret = %d\n", ret);
                if(ret == 0)
                {
                        if(smb_config.multrule[index]->rule != 1)
                        {
                                char *newlocalpath = change_same_name(localfiletmp->path, index, 0);
                                if(newlocalpath==NULL)
                                    return NULL;
                                //SMB_remo(localfiletmp->path, newlocalpath, index);
                                USB_remo(localfiletmp->path, newlocalpath, index);
                                char *err_msg = write_error_message("server file %s is renamed to %s", localfiletmp->path, newlocalpath);
                                write_conflict_log(localfiletmp->path, 3, err_msg); //conflict
                                free(newlocalpath);

                                //ret = SMB_upload(localfiletmp->path, index);
                                ret = USB_upload(localfiletmp->path, index);

                                if(ret == 0)
                                {
                                        time_t modtime = Getmodtime(longurl, index);
                                        if(ChangeFile_modtime(localfiletmp->path, modtime, index))
                                        {
                                                //printf("ChangeFile_modtime failed!\n");
                                                ret = -1;
                                        }
                                }
                        }
                        else
                        {
                                char *newlocalpath = change_same_name(localfiletmp->path, index, 1);
                                if(newlocalpath==NULL)
                                    return NULL;
                                rename(localfiletmp->path, newlocalpath);
                                char *err_msg = write_error_message("server file %s is renamed to %s", localfiletmp->path, newlocalpath);
                                write_conflict_log(localfiletmp->path, 3, err_msg); //conflict
                                free(newlocalpath);

                                action_item *item;
                                item = get_action_item("download", longurl, g_pSyncList[index]->unfinished_list, index);

                                if(is_local_space_enough(filetmp, index))
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                ret = HAVE_LOCAL_SOCKET;
                                        }
                                        else
                                        {
                                                add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);
                                                //ret = SMB_download(longurl, index);
                                                ret = USB_download(longurl, index);
                                                if (ret == 0)
                                                {
                                                        time_t modtime = Getmodtime(longurl, index);
                                                        if(ChangeFile_modtime(localfiletmp->path, modtime, index))
                                                        {
                                                                //printf("ChangeFile_modtime failed!\n");
                                                        }
                                                        if(item != NULL)
                                                        {
                                                                del_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                                        }
                                                }
                                        }
                                }
                                else
                                {
                                        write_log(S_ERROR, "local space is not enough!", "", index);
                                        if(item == NULL)
                                                add_action_item("download", longurl, g_pSyncList[index]->unfinished_list);
                                }
                        }
                }
        }
        free(longurl);
        return 0;
}

int is_smb_file_copying(char *serverpath, int index)
{
        long long int d_value;

        //SMB_init(index);
        struct stat old_info = {0}, new_info ={0};
        //smbc_stat(serverpath, &old_info);
        stat(serverpath, &old_info);
        usleep(1000 * 1000);
        //smbc_stat(serverpath, &new_info);
        stat(serverpath, &new_info);
        d_value = new_info.st_size - old_info.st_size;
        if(d_value)
                return 1;
        else
                return 0;
}

int sync_server_to_local_init(Browse *perform_br, Local *perform_lo, int index)
{
        if(perform_br == NULL || perform_lo == NULL)
        {
                return 0;
        }

        CloudFile *foldertmp = NULL;
        CloudFile *filetmp = NULL;
        LocalFolder *localfoldertmp;
        LocalFile *localfiletmp;
        int ret = 0;

        if(perform_br->foldernumber > 0)
                foldertmp = perform_br->folderlist->next;
        if(perform_br->filenumber > 0)
                filetmp = perform_br->filelist->next;

        localfoldertmp = perform_lo->folderlist->next;
        localfiletmp = perform_lo->filelist->next;

        char *filelongurl = NULL;
        char *folderlongurl = NULL;

        if(perform_br->filenumber == 0 && perform_lo->filenumber != 0)
        {////local端文件不为0，server端为0
                DEBUG("serverfileNo:%d, localfileNo:%d\n", perform_br->filenumber, perform_lo->filenumber);
                while(localfiletmp != NULL && exit_loop == 0)
                {
                        if(smb_config.multrule[index]->rule != 1)
                        {
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }

                                add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);
                                //ret = SMB_upload(localfiletmp->path, index);
                                ret = USB_upload(localfiletmp->path, index);
                                DEBUG("upload--ret=%d\n",ret);
                                if(ret == 0)
                                {
                                        char *serverpath = localpath_to_serverpath(localfiletmp->path, index);
                                        time_t modtime = Getmodtime(serverpath, index);
                                        if(ChangeFile_modtime(localfiletmp->path, modtime,index))
                                        {
                                               DEBUG("ChangeFile_modtime failed!\n");
                                        }
                                        free(serverpath);
                                }
                                else
                                {
                                        DEBUG("upload failed!\n");
                                        return ret;
                                }
                        }
                        else
                        {
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }
                                add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->download_only_socket_head);
                        }
                        localfiletmp = localfiletmp->next;
                }
        }
        else if(perform_br->filenumber != 0 && perform_lo->filenumber == 0)
        {
                DEBUG("serverfileNo:%d, localfileNo:%d\n", perform_br->filenumber, perform_lo->filenumber);
                if(smb_config.multrule[index]->rule != 2)
                {
                        while(filetmp != NULL && exit_loop == 0)
                        {
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }

                                filelongurl = reduce_memory(filetmp->href, index, 1);

                                int cp = 0;
                                do{
                                        if(exit_loop != 0)
                                        {
                                                free(filelongurl);
                                                return -1;
                                        }
                                        cp = is_smb_file_copying(filelongurl, index);
                                }while(cp);

                                if(is_local_space_enough(filetmp, index))
                                {
                                        char *localpath = serverpath_to_localpath(filelongurl, index);
                                        //2014.10.17 by sherry  ，判断malloc是否成功
//                                        if(localpath==NULL)
//                                        {
//                                            return NULL;
//                                        }
                                        add_action_item("createfile", localpath, g_pSyncList[index]->server_action_list);

                                        //ret = SMB_download(filelongurl, index);
                                        ret = USB_download(filelongurl, index);
                                        //printf("ret = %d\n", ret);
                                        if (ret == 0)
                                        {
                                                time_t modtime = Getmodtime(filelongurl, index);
                                                if(ChangeFile_modtime(localpath, modtime, index))
                                                {
                                                        printf("ChangeFile_modtime failed!\n");
                                                }
                                                free(localpath);
                                        }
                                        else
                                        {
                                                free(localpath);
                                                break;
                                        }
                                }
                                else
                                {
                                        write_log(S_ERROR, "local space is not enough!", "", index);
                                        add_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                                }
                                free(filelongurl);
                                filetmp = filetmp->next;
                        }
                }
        }
        else if(perform_br->filenumber != 0 && perform_lo->filenumber != 0)
        {
                DEBUG("serverfileNo:%d\tlocalfileNo:%d\n", perform_br->filenumber, perform_lo->filenumber);
                while(localfiletmp != NULL && exit_loop == 0)
                {
                        //printf("localfiletmp->path = %s\n", localfiletmp->path);
                        int cmp = 1;
                        char *localpathtmp = strstr(localfiletmp->path, smb_config.multrule[index]->client_root_path)
                                             + strlen(smb_config.multrule[index]->client_root_path);
                        while(filetmp != NULL)
                        {
                                //char *serverpathtmp;
                                //serverpathtmp = strstr(filetmp->href, smb_config.multrule[index]->server_root_path)
                                                //+ strlen(smb_config.multrule[index]->server_root_path);
                                //printf("%s, %s\n", localpathtmp, serverpathtmp);
                                if ((cmp = strcmp(localpathtmp, filetmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        filetmp = filetmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                if(smb_config.multrule[index]->rule != 1)
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                return HAVE_LOCAL_SOCKET;
                                        }

                                        add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->server_action_list);
                                        //ret = SMB_upload(localfiletmp->path,index);
                                        ret = USB_upload(localfiletmp->path,index);
                                        if(ret == 0)
                                        {
                                                char *serverpath = localpath_to_serverpath(localfiletmp->path, index);
                                                time_t modtime = Getmodtime(serverpath, index);
                                                if(ChangeFile_modtime(localfiletmp->path, modtime, index))
                                                {
                                                        printf("ChangeFile_modtime failed!\n");
                                                }
                                                free(serverpath);
                                        }
                                        else
                                        {
                                                return ret;
                                        }
                                }
                                else
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                return HAVE_LOCAL_SOCKET;
                                        }
                                        add_action_item("createfile", localfiletmp->path, g_pSyncList[index]->download_only_socket_head);
                                }
                        }
                        else
                        {
                                if((ret = the_same_name_compare(localfiletmp, filetmp, index)) != 0)
                                {
                                        return ret;
                                }
                                //printf("the same name ~~~\n");
                        }
                        filetmp = perform_br->filelist->next;
                        localfiletmp = localfiletmp->next;
                }


                filetmp = perform_br->filelist->next;
                localfiletmp = perform_lo->filelist->next;
                while(filetmp != NULL && exit_loop == 0)
                {
                        int cmp = 1;
                        //char *serverpathtmp;
                        //serverpathtmp = strstr(filetmp->href, smb_config.multrule[index]->server_root_path)
                                        //+ strlen(smb_config.multrule[index]->server_root_path);
                        //serverpathtmp = oauth_url_unescape(serverpathtmp,NULL);
                        while(localfiletmp != NULL)
                        {
                                char *localpathtmp;
                                localpathtmp = strstr(localfiletmp->path, smb_config.multrule[index]->client_root_path)
                                               + strlen(smb_config.multrule[index]->client_root_path);
                                if ((cmp = strcmp(localpathtmp, filetmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        localfiletmp = localfiletmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                filelongurl = reduce_memory(filetmp->href, index, 1);

                                if(smb_config.multrule[index]->rule != 2)
                                {
                                        int cp = 0;
                                        do{
                                                if(exit_loop == 1)
                                                {
                                                        free(filelongurl);
                                                        return -1;
                                                }
                                                cp = is_smb_file_copying(filelongurl, index);
                                        }while(cp == 1);

                                        if(is_local_space_enough(filetmp, index))
                                        {
                                                char *localpath = serverpath_to_localpath(filelongurl, index);
                                                //2014.10.17 by sherry  ，判断malloc是否成功
//                                                if(localpath==NULL)
//                                                {
//                                                    return NULL;
//                                                }

                                                if(wait_handle_socket(index))
                                                {
                                                        free(filelongurl);
                                                        return HAVE_LOCAL_SOCKET;
                                                }

                                                add_action_item("createfile", localpath, g_pSyncList[index]->server_action_list);
                                                //ret = SMB_download(filelongurl, index);
                                                ret = USB_download(filelongurl, index);

                                                if (ret == 0)
                                                {
                                                        time_t modtime = Getmodtime(filelongurl, index);
                                                        if(ChangeFile_modtime(localpath, modtime, index))
                                                        {
                                                                printf("ChangeFile_modtime failed!\n");
                                                        }
                                                        free(localpath);
                                                }
                                                else
                                                {
                                                        free(localpath);
                                                        free(filelongurl);
                                                        return ret;
                                                }
                                        }
                                        else
                                        {
                                                write_log(S_ERROR, "local space is not enough!", "", index);
                                                add_action_item("download", filelongurl, g_pSyncList[index]->unfinished_list);
                                        }
                                }
                                free(filelongurl);
                        }
                        filetmp = filetmp->next;
                        localfiletmp = perform_lo->filelist->next;
                }
        }

        if(perform_br->foldernumber == 0 && perform_lo->foldernumber != 0)
        {//local端文件夹不为0，server端为0
                DEBUG("##serverfolderNo:%d, localfolderNo:%d\n", perform_br->foldernumber, perform_lo->foldernumber);
                while(localfoldertmp != NULL && exit_loop == 0)
                {
                        if(wait_handle_socket(index))
                        {
                                return HAVE_LOCAL_SOCKET;
                        }
                        if(smb_config.multrule[index]->rule != 1)
                        {//不是download only
                                add_action_item("createfolder", localfoldertmp->path, g_pSyncList[index]->server_action_list);
                                //2014.11.19 by sherry 未判断创建文件夹是否成功
                                //SMB_mkdir(localfoldertmp->path, index);
                                int ret=0;
                                //ret=SMB_mkdir(localfoldertmp->path, index);
                                ret=USB_mkdir(localfoldertmp->path, index);
                                if(ret==-1)
                                    return -1;
                        }
                        else
                        {
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }
                                add_action_item("createfolder", localfoldertmp->path, g_pSyncList[index]->download_only_socket_head);
                        }
                        localfoldertmp = localfoldertmp->next;
                }
        }
        else if(perform_br->foldernumber != 0 && perform_lo->foldernumber == 0)
        {
                DEBUG("** serverfolderNo:%d, localfolderNo:%d\n", perform_br->foldernumber, perform_lo->foldernumber);
                if(smb_config.multrule[index]->rule != 2)
                {
                        while(foldertmp != NULL && exit_loop == 0)
                        {
                                if(wait_handle_socket(index))
                                {
                                        return HAVE_LOCAL_SOCKET;
                                }

                                folderlongurl = reduce_memory(foldertmp->href, index, 1);

                                char *localpath = serverpath_to_localpath(folderlongurl, index);
                                //2014.10.17 by sherry  ，判断malloc是否成功
//                                if(localpath==NULL)
//                                {
//                                    return NULL;
//                                }
                                int exist = is_server_exist(localpath, index);
                                if(exist)
                                {
                                        if(NULL == opendir(localpath))
                                        {
                                                add_action_item("createfolder", localpath, g_pSyncList[index]->server_action_list);
                                                mkdir(localpath, 0777);
                                        }
                                }
                                free(localpath);
                                free(folderlongurl);
                                foldertmp = foldertmp->next;
                        }
                }
        }
        else if(perform_br->foldernumber != 0 && perform_lo->foldernumber != 0)
        {
                DEBUG("serverfolderNo:%d, localfolderNo:%d\n", perform_br->foldernumber, perform_lo->foldernumber);
                while(localfoldertmp != NULL && exit_loop == 0)
                {
                        int cmp = 1;
                        char *localpathtmp;
                        localpathtmp = strstr(localfoldertmp->path, smb_config.multrule[index]->client_root_path)
                                       + strlen(smb_config.multrule[index]->client_root_path);
                        while(foldertmp != NULL)
                        {
                                //char *serverpathtmp;
                                //serverpathtmp = strstr(foldertmp->href, smb_config.multrule[index]->server_root_path)
                                                //+ strlen(smb_config.multrule[index]->server_root_path);
                                //printf("%s, %s\n",localpathtmp, serverpathtmp);
                                if ((cmp = strcmp(localpathtmp, foldertmp->href)) == 0)
                                {
                                        //free(serverpathtmp);
                                        break;
                                }
                                else
                                {
                                        foldertmp = foldertmp->next;
                                }
                                //free(serverpathtmp);
                        }
                        if (cmp != 0)
                        {
                                if(smb_config.multrule[index]->rule != 1)
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                return HAVE_LOCAL_SOCKET;
                                        }
                                        add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->server_action_list);
                                        //SMB_mkdir(localfoldertmp->path, index);
                                        USB_mkdir(localfoldertmp->path, index);
                                }
                                else
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                return HAVE_LOCAL_SOCKET;
                                        }
                                        add_action_item("createfolder", localfoldertmp->path, g_pSyncList[index]->download_only_socket_head);
                                }
                        }
                        foldertmp = perform_br->folderlist->next;
                        localfoldertmp = localfoldertmp->next;
                }

                foldertmp = perform_br->folderlist->next;
                localfoldertmp = perform_lo->folderlist->next;
                while(foldertmp != NULL && exit_loop == 0)
                {
                        int cmp = 1;
                        //char *serverpathtmp;
                        //serverpathtmp = strstr(foldertmp->href, smb_config.multrule[index]->server_root_path)
                                        //+ strlen(smb_config.multrule[index]->server_root_path);
                        while(localfoldertmp != NULL)
                        {
                                char *localpathtmp;
                                localpathtmp = strstr(localfoldertmp->path, smb_config.multrule[index]->client_root_path)
                                               + strlen(smb_config.multrule[index]->client_root_path);
                                if ((cmp = strcmp(localpathtmp, foldertmp->href)) == 0)
                                {
                                        break;
                                }
                                else
                                {
                                        localfoldertmp = localfoldertmp->next;
                                }
                        }
                        if (cmp != 0)
                        {
                                if(smb_config.multrule[index]->rule != 2)
                                {
                                        if(wait_handle_socket(index))
                                        {
                                                return HAVE_LOCAL_SOCKET;
                                        }

                                        folderlongurl = reduce_memory(foldertmp->href, index, 1);

                                        char *localpath = serverpath_to_localpath(folderlongurl, index);
                                        //2014.10.17 by sherry  ，判断malloc是否成功
//                                        if(localpath==NULL)
//                                            return NULL;
                                        int exist = is_server_exist(localpath, index);
                                        if(exist)
                                        {
                                                if(NULL == opendir(localpath))
                                                {
                                                        add_action_item("createfolder", localpath, g_pSyncList[index]->server_action_list);
                                                        mkdir(localpath, 0777);
                                                }
                                        }
                                        free(localpath);
                                        free(folderlongurl);
                                }
                        }
                        foldertmp = foldertmp->next;
                        localfoldertmp = perform_lo->folderlist->next;
                }
        }
        return ret;
}

/*获取某一文件夹下的所有文件和文件夹信息*/
Local *Find_Floor_Dir(const char *path)
{
        //DEBUG("Find_Floor_Dir  start\n");
        Local *local;
        int filenum;
        int foldernum;
        LocalFile *localfloorfile;
        LocalFolder *localfloorfolder;
        LocalFile *localfloorfiletmp;
        LocalFolder *localfloorfoldertmp;
        LocalFile *localfloorfiletail;
        LocalFolder *localfloorfoldertail;
        DIR *pDir;
        struct dirent *ent = NULL;

        filenum = 0;
        foldernum = 0;
        local = (Local *)malloc(sizeof(Local));
        //2014.10.20 by sherry malloc申请内存是否成功
//        if(local==NULL)
//            return NULL;
        memset(local,0,sizeof(Local));
        localfloorfile = (LocalFile *)malloc(sizeof(LocalFile));
        //2014.10.20 by sherry malloc申请内存是否成功
//        if(localfloorfile==NULL)
//            return NULL;
        localfloorfolder = (LocalFolder *)malloc(sizeof(LocalFolder));
        //2014.10.20 by sherry malloc申请内存是否成功
//        if(localfloorfolder==NULL)
//            return NULL;
        memset(localfloorfolder,0,sizeof(localfloorfolder));
        memset(localfloorfile,0,sizeof(localfloorfile));

        localfloorfile->path = NULL;
        localfloorfolder->path = NULL;
        localfloorfiletail = localfloorfile;
        localfloorfoldertail = localfloorfolder;
        localfloorfiletail->next = NULL;
        localfloorfoldertail->next = NULL;
        //DEBUG("Find_Floor_Dir  ccc,path=%s\n",path);
        pDir = opendir(path);
        //DEBUG("Find_Floor_Dir  ddd\n");
        if(NULL == pDir)
        {
                return NULL;
        }
        //DEBUG("Find_Floor_Dir  fff\n");
//        if(!readdir(pDir))
//            printf("readdir fail\n");
        while(NULL != (ent = readdir(pDir)))
        {

                if(ent->d_name[0] == '.')
                        continue;
                if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                        continue;
                if(test_if_download_temp_file(ent->d_name)) //2014.10.17 by sherry malloc申请内存是否成功
                        continue;
                        //if((test_if_download_temp_file(ent->d_name)==NULL)||(test_if_download_temp_file(ent->d_name)==1))     //download temp files


                char *fullname;
                size_t len;
                len = strlen(path) + strlen(ent->d_name) + 2;
                fullname = my_malloc(len);
                //2014.10.20 by sherry malloc申请内存是否成功
//                if(fullname==NULL)
//                    return NULL;
                sprintf(fullname, "%s/%s", path, ent->d_name);

                //printf("folder fullname = %s\n",fullname);
                //printf("ent->d_ino = %d\n",ent->d_ino);

                if(test_if_dir(fullname) == 1)//是folder
                {
                        localfloorfoldertmp = (LocalFolder *)malloc(sizeof(LocalFolder));
                        //2014.10.20 by sherry malloc申请内存是否成功
//                        if(localfloorfoldertmp==NULL)
//                            return NULL;
                        memset(localfloorfoldertmp, 0, sizeof(localfloorfoldertmp));
                        localfloorfoldertmp->path = my_malloc(strlen(fullname) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
//                        if(localfloorfoldertmp->path==NULL)
//                            return NULL;
                        localfloorfoldertmp->name = my_malloc(strlen(ent->d_name) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
//                        if(localfloorfoldertmp->name==NULL)
//                            return NULL;
                        sprintf(localfloorfoldertmp->name, "%s", ent->d_name);
                        sprintf(localfloorfoldertmp->path, "%s", fullname);

                        ++foldernum;

                        localfloorfoldertail->next = localfloorfoldertmp;
                        localfloorfoldertail = localfloorfoldertmp;
                        localfloorfoldertail->next = NULL;
                }
                else//是file
                {
                        struct stat buf;

                        if(stat(fullname,&buf) == -1)
                        {
                                perror("stat:");
                                continue;
                        }

                        localfloorfiletmp = (LocalFile *)malloc(sizeof(LocalFile));
                        //2014.10.20 by sherry malloc申请内存是否成功
//                        if(localfloorfiletmp==NULL)
//                            return NULL;
                        memset(localfloorfiletmp, 0, sizeof(localfloorfiletmp));
                        localfloorfiletmp->path = my_malloc(strlen(fullname) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
//                        if(localfloorfiletmp->path==NULL)
//                            return NULL;
                        localfloorfiletmp->name = my_malloc(strlen(ent->d_name) + 1);
                        //2014.10.20 by sherry malloc申请内存是否成功
//                        if(localfloorfiletmp->name==NULL)
//                            return NULL;

                        unsigned long msec = buf.st_mtime;
                        localfloorfiletmp->modtime = msec;
                        sprintf(localfloorfiletmp->name,"%s",ent->d_name);
                        sprintf(localfloorfiletmp->path,"%s",fullname);

                        localfloorfiletmp->size = buf.st_size;
                        ++filenum;

                        localfloorfiletail->next = localfloorfiletmp;
                        localfloorfiletail = localfloorfiletmp;
                        localfloorfiletail->next = NULL;
                }
                free(fullname);
        }
        local->filelist = localfloorfile;
        local->folderlist = localfloorfolder;

        local->filenumber = filenum;
        local->foldernumber = foldernum;

        closedir(pDir);
        return local;

}

int sync_server_to_local(Server_TreeNode *treenode, int (*sync_fuc)(Browse*, Local*, int), int index)
{//sync_fuc这个指针指向sync_server_to_local_perform()函数
        if(exit_loop)
        {
                return -1;
        }
        if(treenode->parenthref == NULL)
        {
                return 0;
        }
        Local *localnode;

        int ret = 0;
        DEBUG("serverpath_to_localpath\n");
        DEBUG("treenode->parenthref=%s\n",treenode->parenthref);
        char *localpath = serverpath_to_localpath(treenode->parenthref, index);
        //2014.10.17 by sherry  ，判断malloc是否成功
//        if(localpath==NULL)
//            return NULL;
        DEBUG("localpath=%s\n",localpath);
        DEBUG("Find_Floor_Dir\n");
        localnode = Find_Floor_Dir(localpath);
        DEBUG("Find_Floor_Dir------\n");

        free(localpath);

        if(NULL != localnode)
        {
                DEBUG("NULL != localnode\n");
                ret = sync_fuc(treenode->browse, localnode, index);
                if(ret == COULD_NOT_CONNECNT_TO_SERVER)
                {
                        DEBUG("###############free localnode\n");
                        free_localfloor_node(localnode);
                        return ret;
                }
                free_localfloor_node(localnode);
        }

        if(treenode->Child != NULL)
        {
                DEBUG("treenode->Child != NULL");
                ret = sync_server_to_local(treenode->Child, sync_fuc, index);
                if(ret != 0 && ret != LOCAL_FILE_LOST && ret != SERVER_FILE_DELETED)
                {
                        DEBUG("ret = %d\n",ret);
                        return ret;
                }
        }
        if(treenode->NextBrother != NULL)
        {
                DEBUG("treenode->NextBrother != NULL");
                ret = sync_server_to_local(treenode->NextBrother, sync_fuc, index);
                if(ret != 0 && ret != LOCAL_FILE_LOST && ret != SERVER_FILE_DELETED)
                {
                        DEBUG("ret = %d\n",ret);
                        return ret;
                }
        }
        DEBUG("sync_server_to_local  end\n");
        return ret;
}

void handle_quit_upload()
{
        DEBUG("###handle_quit_upload###\n");
        //printf("###handle_quit_upload###\n");
        int i;
        for(i = 0; i < smb_config.dir_num; i++)
        {
                if(g_pSyncList[i]->sync_disk_exist == 0)
                        continue;

                if(smb_config.multrule[i]->rule != 1)
                {
                        FILE *fp;
                        long long filesize;
                        char *buf;
                        int status;
                        DEBUG("g_pSyncList[i]->up_item_file = %s\n", g_pSyncList[i]->up_item_file);
                        if(access(g_pSyncList[i]->up_item_file, 0) == 0)
                        {
                            DEBUG("up_item_file is exist\n");
                                filesize = stat_file(g_pSyncList[i]->up_item_file);
                                fp = fopen(g_pSyncList[i]->up_item_file, "r");
                                if(fp == NULL)
                                {
                                        DEBUG("open %s error\n",g_pSyncList[i]->up_item_file);
                                        return;
                                }
                                buf = my_malloc((size_t)(filesize + 1));
                                //2014.10.20 by sherry malloc申请内存是否成功
//                                if(buf==NULL)
//                                    return NULL;
                                fscanf(fp, "%s", buf);
                                fclose(fp);
                                
                                if((strlen(buf)) <= 1)
                                {
                                        free(buf);
                                        return;
                                }
                                DEBUG("buf:%s\n", buf);
                                unlink(g_pSyncList[i]->up_item_file);
                                //SMB_rm(buf, i);
                                   USB_rm(buf,i);
                                //status = SMB_upload(buf, i);
                                  status = USB_upload(buf, i);
                                
                                if(status != 0)
                                {
                                        DEBUG("upload %s failed\n",buf);
                                }
                                else
                                {
                                        char *serverpath = localpath_to_serverpath(buf, i);
                                        DEBUG("serverpath = %s\n", serverpath);
                                        time_t modtime = Getmodtime(serverpath, i);
                                        if(ChangeFile_modtime(buf, modtime, i))
                                        {
                                                DEBUG("ChangeFile_modtime failed!\n");
                                        }
                                        free(serverpath);
                                }
                                
                                free(buf);
                        }
                }
        }
                //printf("###handle_quit_upload end###\n");
}

int initialization()
{
        DEBUG("initialization() - start\n");
        int ret = 0;
        int status;
        int i;
        DEBUG("smb_config.dir_num = %d\n", smb_config.dir_num);
        for(i = 0; i < smb_config.dir_num; i++)
        {
                if(exit_loop)
                        break;

                if(g_pSyncList[i]->init_completed)
                        continue;

                write_log(S_INITIAL, "", "", i);
                ret = 1;
                DEBUG("it is %d init \n", i);

                if(exit_loop == 0)
                {
                        DEBUG("wd_initial ret = %d\n", ret);
                        free_server_tree(g_pSyncList[i]->ServerRootNode);
                        g_pSyncList[i]->ServerRootNode = NULL;

                        g_pSyncList[i]->ServerRootNode = create_server_treeroot();
                        DEBUG("browse_to_tree() - start\n");
                        status = browse_to_tree(smb_config.multrule[i]->server_root_path, g_pSyncList[i]->ServerRootNode, i);
                        DEBUG("status = %d\n", status);
                        DEBUG("browse_to_tree() - end\n");

                        if(status != 0)
                                continue;

                        if(exit_loop == 0)
                        {
                            DEBUG("smb_config.multrule[i]->client_root_path=%s\n",smb_config.multrule[i]->client_root_path);
                                if(test_if_dir_empty(smb_config.multrule[i]->client_root_path))
                                {//local端 没有文件或者文件夹
                                        DEBUG("base_path is blank\n");

                                        if(smb_config.multrule[i]->rule != 2)
                                        {
                                                if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                                                {
                                                        DEBUG("######## init sync folder,please wait......#######\n");

                                                        ret = initMyLocalFolder(g_pSyncList[i]->ServerRootNode->Child, i);
                                                        if(ret != 0)
                                                                continue;
                                                        g_pSyncList[i]->init_completed = 1;
                                                        g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                                                        DEBUG("######## init sync folder end#######\n");
                                                }
                                        }
                                        else
                                        {
                                                g_pSyncList[i]->init_completed = 1;
                                                g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                                                ret = 0;
                                        }
                                }
                                else
                                {
                                        printf("######## init sync folder(have files), please wait......#######\n");
                                        if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                                        {
                                                ret = sync_server_to_local(g_pSyncList[i]->ServerRootNode->Child, sync_server_to_local_init, i);
                                                DEBUG("####### ret1 = %d\n", ret);
                                                if(ret != 0)
                                                        continue;
                                        }
                                        ret = sync_local_to_server(smb_config.multrule[i]->client_root_path, i);
                                        DEBUG("####### ret2 = %d\n", ret);
                                        if(ret != 0)
                                                continue;
                                        DEBUG("#########ret = %d\n", ret);
                                        DEBUG("######## init sync folder end#######\n");
                                        g_pSyncList[i]->init_completed = 1;
                                        g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                                }
                        }
                }
                if(ret == 0)
                {
                        write_log(S_SYNC, "", "", i);
                }

        }

        return ret;
}

void sig_handler (int signum)
{
        switch (signum)
        {
        case SIGTERM:case SIGUSR2:
                {

                        if(signum == SIGUSR2)
                        {
                                DEBUG("signal is SIGUSR2\n");
                                mountflag = 0;
                                FILE *fp;
                                fp = fopen("/proc/mounts","r");
                                if(fp == NULL)
                                {
                                        //printf("open /proc/mounts fail\n");
                                }
                                char a[1024];
                                while(!feof(fp))
                                {
                                        memset(a, '\0', 1024);
                                        fscanf(fp, "%[^\n]%*c", a);
                                        //printf("\nmount = %s\n",a);
                                        if(strstr(a, "/dev/sd"))
                                        {
                                                mountflag = 1;
                                                break;
                                        }
                                }
                                fclose(fp);
                        }
                        //printf("mountflag = %d\n",mountflag);
                        if(signum == SIGTERM || mountflag == 0)
                        {
                                stop_progress = 1;
                                exit_loop = 1;
#ifndef NVRAM_
                                system("sh /tmp/smartsync/usbclient/script/usbclient_get_nvram");
#endif
                                sleep(2);
                                if(create_smbclientconf(&smb_config_stop) == -1)
                                {
                                        //printf("create_webdav_conf_file fail\n");
#ifdef NVRAM_
                                        write_to_nvram("", "smb_tokenfile");
#endif
                                        return;
                                }


                                if(smb_config_stop.dir_num == 0)
                                {
                                        char *filename;
                                        filename = my_malloc(strlen(smb_config.multrule[0]->mount_path) + 24);
                                        //2014.10.20 by sherry malloc申请内存是否成功
//                                        if(filename==NULL)
//                                            return NULL;
                                        sprintf(filename,"%s/.usbclient_tokenfile", smb_config.multrule[0]->mount_path);
                                        remove(filename);
                                        free(filename);
#ifdef NVRAM_
                                        write_to_nvram("","smb_tokenfile");   //the "" maybe cause errors
#else
                                        write_to_smb_tokenfile("");
#endif
                                }
                                else
                                {
                                        if(smb_config_stop.dir_num != smb_config.dir_num)
                                        {
                                                DEBUG("smb_config_stop.dir_num = %d\n", smb_config_stop.dir_num);
                                                DEBUG("smb_config.dir_num = %d\n", smb_config.dir_num);
                                                parse_config(&smb_config_stop);
                                                rewrite_tokenfile_and_nv();
                                        }
                                }
                                sighandler_finished = 1;
                                pthread_cond_signal(&cond);
                                pthread_cond_signal(&cond_socket);
                                pthread_cond_signal(&cond_log);
                        }
                        else
                        {
                                pthread_mutex_lock(&mutex_checkdisk);
                                disk_change = 1;
                                pthread_mutex_unlock(&mutex_checkdisk);
                                sighandler_finished = 1;
                        }
                        break;
                }
        case SIGUSR1:
                DEBUG("signal is SIGUSER1\n");
                break;
        default:
                break;
        }
}

void* sigmgr_thread(void *argc)
{
        DEBUG("sigmgr_thread:%u\n", pthread_self());
        sigset_t   waitset;
        int        sig;
        int        rc;

        pthread_t  ppid = pthread_self();
        pthread_detach(ppid);

        sigemptyset(&waitset);
        sigaddset(&waitset, SIGUSR1);
        sigaddset(&waitset, SIGUSR2);
        sigaddset(&waitset, SIGTERM);

        while(1)
        {
                rc = sigwait(&waitset, &sig);
                if (rc != -1)
                {
                        DEBUG("sigwait() fetch the signal - %d\n", sig);
                        sig_handler(sig);
                }else{
                        printf("sigwaitinfo() returned err: %d; %s\n", errno, strerror(errno));
                }
        }
}

void run()
{
        //init_global_var();
        send_to_inotify();
        handle_quit_upload();

        int create_thid1 = 0;
        if(exit_loop == 0)
        {
                if(pthread_create(&newthid1, NULL, Save_Socket, NULL) != 0)
                {
                        printf("thread creation failed!\n");
                        exit(1);
                }
                usleep(1000 * 500);
                create_thid1 = 1;
        }

        int create_thid2 = 0;
        if(exit_loop == 0)
        {
                if(pthread_create(&newthid2, NULL, SyncLocal, NULL) != 0)
                {
                        printf("thread creation failed!\n");
                        exit(1);
                }
                usleep(1000*500);
                create_thid2 = 1;
        }

        initialization();
        int create_thid3 = 0;
        if(exit_loop == 0)
        {
                if(pthread_create(&newthid3, NULL, SyncServer, NULL) != 0)
                {
                        printf("thread creation failed!\n");
                        exit(1);
                }
                create_thid3 = 1;
        }

        if(create_thid1)
        {
                pthread_join(newthid1,NULL);
                printf("newthid1 has stoped!\n");
        }

        if(create_thid2)
        {
                pthread_join(newthid2,NULL);
                printf("newthid2 has stoped!\n");
        }

        if(create_thid3)
        {
                pthread_join(newthid3,NULL);
                printf("newthid3 has stoped!\n");
        }

        usleep(1000);

}

int main(int argc, char* argv[]){

        no_config = 0;
        exit_loop = 0;

        //创建目录
        local_mkdir("/tmp/smartsync");
        local_mkdir("/tmp/smartsync/.logs");
        local_mkdir("/tmp/smartsync/usbclient");
        local_mkdir("/tmp/smartsync/usbclient/config");
        local_mkdir("/tmp/smartsync/usbclient/script");

        sigset_t bset,oset;
        pthread_t sig_thread;

        sigemptyset(&bset);
        sigaddset(&bset, SIGUSR1);
        sigaddset(&bset, SIGUSR2);
        sigaddset(&bset, SIGTERM);

        if( pthread_sigmask(SIG_BLOCK, &bset, &oset) == -1)
                printf("!! Set pthread mask failed\n");

        if(pthread_create(&sig_thread, NULL, (void *)sigmgr_thread, NULL) != 0)
        {
                printf("thread creation failed\n");
                exit(-1);
        }

        clear_global_var();
#ifndef _PC
        write_notify_file(NOTIFY_PATH, SIGUSR2);

        while(detect_process("nvram"))
                sleep(2);

        create_shell_file();
#ifndef NVRAM_
        local_mkdir("/opt/etc/.smartsync");
        write_get_nvram_script("cloud_sync", GET_NVRAM, TMP_CONFIG);
        system("sh /tmp/smartsync/usbclient/script/usbclient_get_nvram");
        sleep(2);
        write_get_nvram_script("link_internet", GET_INTERNET, VAL_INTERNET);
#endif
#endif

        read_config();
        init_global_var();
/*#ifndef _PC
        check_link_internet();
#endif*/  // 20150617 need add check_server_hdd
        if(!no_config)          
            run();


        pthread_join(sig_thread, NULL);

        stop_process_clean_up();
        return 0;
}

















