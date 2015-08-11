#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "SMB.h"

extern Config smb_config, smb_config_stop;
extern struct tokenfile_info_tag *tokenfile_info,*tokenfile_info_start,*tokenfile_info_tmp;
extern sync_list **g_pSyncList;
extern int disk_change;
extern int exit_loop;
extern int sync_disk_removed;

int get_mounts_info(struct mounts_info_tag *info)
{
        int len = 0;
        FILE *fp;
        int i = 0;
        int num = 0;
        //char *mount_path;
        char **tmp_mount_list = NULL;
        char **tmp_mount = NULL;

        char buf[len + 1];
        memset(buf, '\0', sizeof(buf));
        char a[1024];
        //char *temp;
        char *p, *q;
        fp = fopen("/proc/mounts", "r");
        if(fp)
        {
                while(!feof(fp))
                {
                        memset(a, '\0', sizeof(a));
                        fscanf(fp, "%[^\n]%*c", a);
                        if((strlen(a) != 0)&&((p = strstr(a, "/dev/sd")) != NULL))
                        {
                                DEBUG("%s\n", p);
                                if((q = strstr(p, "/tmp")) != NULL)
                                {
                                        if((p = strstr(q, " ")) != NULL)
                                        {
                                                len = strlen(q) - strlen(p) + 1;

                                                tmp_mount = (char **)malloc(sizeof(char *)*(num + 1));
                                                if(tmp_mount == NULL)
                                                {
                                                        fclose(fp);
                                                        return -1;
                                                }

                                                tmp_mount[num] = my_malloc(len + 1);
                                                if(tmp_mount[num] == NULL)
                                                {
                                                        free(tmp_mount);
                                                        fclose(fp);
                                                        return -1;
                                                }
                                                snprintf(tmp_mount[num], len, "%s", q);

                                                if(num != 0)
                                                {
                                                        for(i = 0; i < num; ++i)
                                                                tmp_mount[i] = tmp_mount_list[i];

                                                        free(tmp_mount_list);
                                                        tmp_mount_list = tmp_mount;
                                                }
                                                else
                                                        tmp_mount_list = tmp_mount;

                                                ++num;
                                        }
                                }
                        }
                }
        }
        fclose(fp);
        info->paths = tmp_mount_list;
        info->num = num;
        return 0;
}

int get_tokenfile_info()
{
        //printf("get_tokenfile_info()- start\n");
        int i;
        int j = 0;
        struct mounts_info_tag *info = NULL;
        char *tokenfile;
        FILE *fp;
        char buffer[1024];
        char *p;

        tokenfile_info = tokenfile_info_start;

        info = (struct mounts_info_tag *)malloc(sizeof(struct mounts_info_tag));
        if(info == NULL)
        {
                DEBUG("obtain memory space fail\n");
                return -1;
        }

        memset(info, 0, sizeof(struct mounts_info_tag));
        memset(buffer, 0, 1024);

        if(get_mounts_info(info) == -1)
        {
                DEBUG("get mounts info fail\n");
                return -1;
        }

        DEBUG("%d\n", info->num);
        for(i = 0; i < info->num; i++)
        {
                DEBUG("info->paths[%d] = %s\n", i, info->paths[i]);
                tokenfile = my_malloc(strlen(info->paths[i]) + 24);
                //2014.10.20 by sherry malloc申请内存是否成功
                //if(tokenfile==NULL)
                    //return NULL;
                sprintf(tokenfile, "%s/.usbclient_tokenfile", info->paths[i]);
                DEBUG("tokenfile = %s\n", tokenfile);
                if(!access(tokenfile, F_OK))
                {
                        DEBUG("tokenfile is exist!\n");
                        if((fp = fopen(tokenfile, "r")) == NULL)
                        {
                                fprintf(stderr, "read tokenfile error\n");
                                exit(-1);
                        }
                        while(fgets(buffer, 1024, fp) != NULL)
                        {
                                if( buffer[ strlen(buffer) - 1 ] == '\n' )
                                        buffer[ strlen(buffer) - 1 ] = '\0';
                                p = buffer;
                                DEBUG("p = %s\n", p);
                                if(j == 0)
                                {
                                        if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
                                        {
                                                return -1;
                                        }
                                        tokenfile_info_tmp->url = my_malloc(strlen(p) + 1);
                                        //2014.10.20 by sherry malloc申请内存是否成功
                                        //if(tokenfile_info_tmp->url==NULL)
                                            //return NULL;
                                        sprintf(tokenfile_info_tmp->url, "%s", p);
                                        tokenfile_info_tmp->mountpath = my_malloc(strlen(info->paths[i]) + 1);
                                        //2014.10.20 by sherry malloc申请内存是否成功
                                        //if(tokenfile_info_tmp->mountpath==NULL)
                                            //return NULL;
                                        sprintf(tokenfile_info_tmp->mountpath, "%s", info->paths[i]);
                                        j++;
                                }
                                else
                                {
                                        tokenfile_info_tmp->folder = my_malloc(strlen(p) + 1);
                                        //2014.10.20 by sherry malloc申请内存是否成功
                                        //if(tokenfile_info_tmp->folder==NULL)
                                            //return NULL;
                                        sprintf(tokenfile_info_tmp->folder, "%s", p);
                                        tokenfile_info->next = tokenfile_info_tmp;
                                        tokenfile_info = tokenfile_info_tmp;
                                        j = 0;
                                }
                        }
                        fclose(fp);
                }
                free(tokenfile);
        }

        for(i = 0; i < info->num; ++i)
        {
                free(info->paths[i]);
        }
        if(info->paths != NULL)
                free(info->paths);
        free(info);
        //printf("get_tokenfile_info()- end\n");
        return 0;
}

int check_config_path(int is_read_config)
{
        DEBUG("check_config_path start\n");
        int i;
        int flag;
        char *nv;
        char *nvp;
        char *new_nv;
        int nv_len;
        int is_path_change = 0;

#ifdef NVRAM_
#ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD, "smb_tokenfile", tmp);
        nv = my_malloc(strlen(tmp) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        //if(nv==NULL)
           // return NULL;
        sprintf(nv, "%s", tmp);
#else
        nv = strdup(nvram_safe_get("smb_tokenfile"));
#endif
#else
        FILE *fp;
        fp = fopen(TOKENFILE_RECORD, "r");
        if(fp == NULL)
        {
                nv = my_malloc(2);
                //2014.10.20 by sherry malloc申请内存是否成功
               //if(nv==NULL)
                 //   return NULL;
                sprintf(nv, "");
        }
        else
        {
                fseek( fp, 0, SEEK_END );
                int file_size;
                file_size = ftell( fp );
                fseek(fp, 0, SEEK_SET);
                //nv =  (char *)malloc( file_size * sizeof( char ) );
                nv = my_malloc(file_size + 2);
                //2014.10.20 by sherry malloc申请内存是否成功
                //if(nv==NULL)
                    //return NULL;
                //fread(nv , file_size , sizeof(char) , fp);
                fscanf(fp, "%[^\n]%*c", nv);
                fclose(fp);
        }
#endif
        nv_len = strlen(nv);

        DEBUG("nv_len = %d\n", nv_len);

        for(i = 0; i < smb_config.dir_num; i++)
        {
                flag = 0;
                tokenfile_info_tmp = tokenfile_info_start->next;
                while(tokenfile_info_tmp != NULL)
                {
                        if( !strcmp(tokenfile_info_tmp->url, smb_config.multrule[i]->server_root_path) &&
                            !strcmp(tokenfile_info_tmp->folder, smb_config.multrule[i]->client_root_path))
                        {
                                if(strcmp(tokenfile_info_tmp->mountpath, smb_config.multrule[i]->mount_path))
                                {
                                        memset(smb_config.multrule[i]->mount_path, 0, sizeof(smb_config.multrule[i]->mount_path));
                                        sprintf(smb_config.multrule[i]->mount_path, "%s", tokenfile_info_tmp->mountpath);
                                        memset(smb_config.multrule[i]->client_root_path, 0, sizeof(smb_config.multrule[i]->client_root_path));
                                        sprintf(smb_config.multrule[i]->client_root_path, "%s%s", tokenfile_info_tmp->mountpath,tokenfile_info_tmp->folder);
                                        is_path_change = 1;
                                }
                                if(!is_read_config)
                                {
                                        if(g_pSyncList[i]->sync_disk_exist == 0)
                                                is_path_change = 1;   //plug the disk and the mout_path not change
                                }
                                flag = 1;
                                break;
                        }
                        tokenfile_info_tmp = tokenfile_info_tmp->next;
                }
                //printf("flag = %d\n", flag);
                if(!flag)
                {
                        nvp = my_malloc(strlen(smb_config.multrule[i]->server_root_path) + strlen(smb_config.multrule[i]->client_root_path) + 2);
                        //2014.10.20 by sherry malloc申请内存是否成功
                        //if(nvp==NULL)
                         //   return NULL;
                        sprintf(nvp, "%s>%s", smb_config.multrule[i]->server_root_path, smb_config.multrule[i]->client_root_path);

                        if(!is_read_config)
                        {
                                if(g_pSyncList[i]->sync_disk_exist == 1)
                                        is_path_change = 2;   //remove the disk and the mout_path not change
                        }

                        DEBUG("write nvram and tokenfile if before\n");

                        if(strstr(nv, nvp) == NULL)
                        {
                                DEBUG("write nvram and tokenfile if behind");

                                if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
                                {
                                        return -1;
                                }
                                tokenfile_info_tmp->url = my_malloc(strlen(smb_config.multrule[i]->server_root_path) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                               // if(tokenfile_info_tmp->url==NULL)
                                 //   return NULL;
                                sprintf(tokenfile_info_tmp->url, "%s", smb_config.multrule[i]->server_root_path);

                                tokenfile_info_tmp->mountpath = my_malloc(strlen(smb_config.multrule[i]->mount_path) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                //if(tokenfile_info_tmp->mountpath==NULL)
                                 //   return NULL;
                                sprintf(tokenfile_info_tmp->mountpath, "%s", smb_config.multrule[i]->mount_path);

                                tokenfile_info_tmp->folder = my_malloc(strlen(smb_config.multrule[i]->client_root_path) + 1);
                                //2014.10.20 by sherry malloc申请内存是否成功
                                //if(tokenfile_info_tmp->folder==NULL)
                                //    return NULL;
                                sprintf(tokenfile_info_tmp->folder, "%s", smb_config.multrule[i]->client_root_path);

                                tokenfile_info->next = tokenfile_info_tmp;
                                tokenfile_info = tokenfile_info_tmp;

                                write_to_tokenfile(smb_config.multrule[i]->mount_path);

                                if(nv_len)
                                {
                                        new_nv = my_malloc(strlen(nv) + strlen(nvp) + 2);
                                        //2014.10.20 by sherry malloc申请内存是否成功
                                       // if(new_nv==NULL)
                                       //     return NULL;
                                        sprintf(new_nv, "%s<%s", nv,nvp);

                                }
                                else
                                {
                                        new_nv = my_malloc(strlen(nvp) + 1);
                                        //2014.10.20 by sherry malloc申请内存是否成功
                                        //if(new_nv==NULL)
                                        //    return NULL;
                                        sprintf(new_nv, "%s", nvp);
                                }
#ifdef NVRAM_
                                write_to_nvram(new_nv, "smb_tokenfile");
#else
                                write_to_smb_tokenfile(new_nv);
#endif
                                free(new_nv);
                        }
                        free(nvp);
                }
        }
        free(nv);
        return is_path_change;
}

int free_tokenfile_info(struct tokenfile_info_tag *head)
{
        struct tokenfile_info_tag *p = head->next;
        while(p != NULL)
        {
                head->next = p->next;
                if(p->folder != NULL)
                {
                        free(p->folder);
                }
                if(p->mountpath != NULL)
                {
                        free(p->mountpath);
                }
                if(p->url != NULL)
                {
                        free(p->url);
                }
                free(p);
                p = head->next;
        }
        return 0;
}

int check_sync_disk_removed()
{
        DEBUG("check_sync_disk_removed start! \n");

        int ret;

        free_tokenfile_info(tokenfile_info_start);

        if(get_tokenfile_info() == -1)
        {
                DEBUG("\nget_tokenfile_info failed\n");
                exit(-1);
        }

        ret = check_config_path(0);
        return ret;

}

int check_disk_change()
{
        int status = -1;
        disk_change = 0;
        status = check_sync_disk_removed();

        if(status == 2 || status == 1)
        {
                exit_loop = 1;
                //sync_up = 0;
                //sync_down = 0;
                sync_disk_removed = 1;
        }
        return 0;
}

#ifdef NVRAM_
int write_to_nvram(char *contents,char *nv_name)
{
        char *command;
        command = my_malloc(strlen(contents) + strlen(SHELL_FILE) + strlen(nv_name) + 8);
        //2014.10.20 by sherry malloc申请内存是否成功
        //if(command==NULL)
        //    return NULL;
        sprintf(command, "sh %s \"%s\" %s", SHELL_FILE, contents, nv_name);

        DEBUG("command : [%s]\n", command);

        system(command);
        free(command);

        return 0;
}
#else
int write_to_smb_tokenfile(char *contents)
{
        if(contents == NULL || contents == "")
        {
                unlink(TOKENFILE_RECORD);
                return 0;
        }
        FILE *fp;
        if( ( fp = fopen(TOKENFILE_RECORD, "w") ) == NULL )
        {
                //printf("open wd_tokenfile failed!\n");
                return -1;
        }
        fprintf(fp, "%s", contents);
        fclose(fp);
        //printf("write_to_wd_tokenfile end\n");
        return 0;
}
#endif

int delete_tokenfile_info(char *url, char *folder){

        struct tokenfile_info_tag *tmp;

        tmp = tokenfile_info_start;
        tokenfile_info_tmp = tokenfile_info_start->next;

        while(tokenfile_info_tmp != NULL)
        {
                if( !strcmp(tokenfile_info_tmp->url, url) &&
                    !strcmp(tokenfile_info_tmp->folder, folder))
                {
                        tmp->next = tokenfile_info_tmp->next;
                        free(tokenfile_info_tmp->folder);
                        free(tokenfile_info_tmp->url);
                        free(tokenfile_info_tmp->mountpath);
                        free(tokenfile_info_tmp);
                        tokenfile_info_tmp = tmp->next;
                }
                else
                {
                        tmp = tokenfile_info_tmp;
                        tokenfile_info_tmp = tokenfile_info_tmp->next;
                }
        }

        return 0;
}

char *delete_nvram_contents(char *url,char *folder)
{
        char *nv;
        char *nvp;
        char *p,*b;
        char *new_nv;
        int n;
        int i = 0;
#ifdef NVRAM_
#ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD, "smb_tokenfile", tmp);
        nv = my_malloc(strlen(tmp) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
        //if(nv==NULL)
        //    return NULL;
        sprintf(nv, "%s", tmp);
        p = nv;
#else
        p = nv = strdup(nvram_safe_get("smb_tokenfile"));
#endif
#else
        FILE *fp;
        fp = fopen("/opt/etc/smb_tokenfile", "r");
        if (fp == NULL)
        {
                nv = my_malloc(2);
                //2014.10.20 by sherry malloc申请内存是否成功
//                if(nv==NULL)
//                    return NULL;
                sprintf(nv, "");
        }
        else
        {
                fseek( fp , 0 , SEEK_END );
                int file_size;
                file_size = ftell( fp );
                fseek(fp , 0 , SEEK_SET);
                nv = my_malloc(file_size + 2);
                //2014.10.20 by sherry malloc申请内存是否成功
               // if(nv==NULL)
                //    return NULL;
                fscanf(fp, "%[^\n]%*c", nv);
                p = nv;
                fclose(fp);
        }

#endif

        nvp = my_malloc(strlen(url) + strlen(folder) + 2);
        //2014.10.20 by sherry malloc申请内存是否成功
       //if(nvp==NULL)
        //    return NULL;
        sprintf(nvp, "%s>%s", url, folder);

        if(strstr(nv, nvp) == NULL)
        {
                free(nvp);
                return nv;
        }

        if(!strcmp(nv, nvp))
        {
                free(nvp);
                memset(nv, 0, sizeof(nv));
                sprintf(nv, "");
                return nv;
        }

        if(nv)
        {
                while((b = strsep(&p, "<")) != NULL)
                {
                        if(strcmp(b, nvp))
                        {
                                n = strlen(b);
                                if(i == 0)
                                {
                                        new_nv = my_malloc(n + 1);
                                        //2014.10.20 by sherry malloc申请内存是否成功
                                      //  if(new_nv==NULL)
                                      //      return NULL;
                                        sprintf(new_nv, "%s", b);
                                        ++i;
                                }
                                else
                                {
                                        new_nv = (char*)realloc(new_nv, strlen(new_nv) + n + 2);
                                        sprintf(new_nv, "%s<%s", new_nv, b);
                                }
                        }
                }

                free(nv);
        }
        free(nvp);
        return new_nv;
}

int write_to_tokenfile(char *mpath)
{
        DEBUG("write_to_tokenfile\n");
        FILE *fp = NULL;

        char *filename = NULL;
        filename = my_malloc(strlen(mpath) + 24);
        //2014.10.20 by sherry malloc申请内存是否成功
       // if(filename==NULL)
       //    return NULL;
        sprintf(filename, "%s/.usbclient_tokenfile", mpath);

        int i = 0;
        if( ( fp = fopen(filename, "w") ) == NULL )
        {
                DEBUG("open tokenfile failed!\n");
                return -1;
        }

        tokenfile_info_tmp = tokenfile_info_start->next;
        while(tokenfile_info_tmp != NULL)
        {
                DEBUG("tokenfile_info_tmp->mountpath = %s\n", tokenfile_info_tmp->mountpath);
                if(!strcmp(tokenfile_info_tmp->mountpath,mpath))
                {
                        if(i == 0)
                        {
                                fprintf(fp, "%s\n%s", tokenfile_info_tmp->url, tokenfile_info_tmp->folder);
                                i = 1;
                        }
                        else
                        {
                                fprintf(fp, "\n%s\n%s", tokenfile_info_tmp->url, tokenfile_info_tmp->folder);
                        }
                }

                tokenfile_info_tmp = tokenfile_info_tmp->next;
        }

        fclose(fp);
        if(!i)
                remove(filename);
        free(filename);

        return 0;
}

struct tokenfile_info_tag *initial_tokenfile_info_data(struct tokenfile_info_tag **token)
{
        struct tokenfile_info_tag *follow_token;

        if(token == NULL)
                return *token;

        *token = (struct tokenfile_info_tag *)malloc(sizeof(struct tokenfile_info_tag));
        if(*token == NULL)
                return NULL;

        follow_token = *token;

        follow_token->url = NULL;
        follow_token->folder = NULL;
        follow_token->mountpath = NULL;
        follow_token->next = NULL;

        return follow_token;
}

int add_tokenfile_info(char *url, char *folder, char *mpath)
{
        DEBUG("add_tokenfile_info start\n");
        if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
        {
                return -1;
        }

        tokenfile_info_tmp->url = my_malloc(strlen(url) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
       // if(tokenfile_info_tmp->url==NULL)
       //     return NULL;
        sprintf(tokenfile_info_tmp->url, "%s", url);

        tokenfile_info_tmp->mountpath = my_malloc(strlen(mpath) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
      // if(tokenfile_info_tmp->mountpath==NULL)
       //     return NULL;
        sprintf(tokenfile_info_tmp->mountpath, "%s", mpath);

        tokenfile_info_tmp->folder = my_malloc(strlen(folder) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
//        if(tokenfile_info_tmp->folder==NULL)
//            return NULL;
        sprintf(tokenfile_info_tmp->folder, "%s", folder);

        tokenfile_info->next = tokenfile_info_tmp;
        tokenfile_info = tokenfile_info_tmp;

        DEBUG("add_tokenfile_info end\n");
        return 0;
}

char *add_nvram_contents(char *url,char *folder)
{
        DEBUG("add_nvram_contents start\n");
        char *nv;
        int nv_len;
        char *new_nv;
        char *nvp;

        nvp = my_malloc(strlen(url) + strlen(folder) + 2);
        //2014.10.20 by sherry malloc申请内存是否成功
       // if(nvp==NULL)
        //    return NULL;
        sprintf(nvp, "%s>%s", url, folder);

        DEBUG("add_nvram_contents     nvp = %s\n",nvp);

#ifdef NVRAM_
#ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD, "smb_tokenfile", tmp);
        nv = my_malloc(strlen(tmp) + 1);
        //2014.10.20 by sherry malloc申请内存是否成功
       // if(nv==NULL)
       //     return NULL;
        sprintf(nv, "%s", tmp);
        //p = nv;
#else
        nv = strdup(nvram_safe_get("smb_tokenfile"));
#endif
#else
        FILE *fp;
        fp = fopen("/opt/etc/smb_tokenfile", "r");
        if (fp == NULL)
        {
                nv = my_malloc(2);
                //2014.10.20 by sherry malloc申请内存是否成功
          //      if(nv==NULL)
           //         return NULL;
                sprintf(nv, "");
        }
        else
        {
                fseek( fp , 0 , SEEK_END );
                int file_size;
                file_size = ftell( fp );
                fseek(fp , 0 , SEEK_SET);
                nv = my_malloc(file_size + 2);
                //2014.10.20 by sherry malloc申请内存是否成功
               // if(nv==NULL)
               //     return NULL;
                fscanf(fp, "%[^\n]%*c", nv);
                fclose(fp);
        }
#endif
        DEBUG("add_nvram_contents() - nv = %s\n", nv);
        nv_len = strlen(nv);

        if(nv_len)
        {
                new_nv = my_malloc(strlen(nv) + strlen(nvp) + 2);
                //2014.10.20 by sherry malloc申请内存是否成功
               // if(new_nv==NULL)
               //     return NULL;
                sprintf(new_nv, "%s<%s", nv, nvp);

        }
        else
        {
                new_nv = my_malloc(strlen(nvp) + 1);
                //2014.10.20 by sherry malloc申请内存是否成功
               // if(new_nv==NULL)
               //     return NULL;
                sprintf(new_nv, "%s", nvp);
        }

        free(nvp);
        free(nv);
        DEBUG("add_nvram_contents end\n");
        return new_nv;
}

int rewrite_tokenfile_and_nv(){

        int i, j;
        int exist;
        //printf("rewrite_tokenfile_and_nv start\n");
        if(smb_config.dir_num > smb_config_stop.dir_num)
        {
                for(i = 0; i < smb_config.dir_num; i++)
                {
                        exist = 0;
                        for(j = 0; j < smb_config_stop.dir_num; j++)
                        {
                                if(!strcmp(smb_config_stop.multrule[j]->server_root_path, smb_config.multrule[i]->server_root_path))
                                {
                                        exist = 1;
                                        break;
                                }
                        }
                        if(!exist)
                        {
                                //printf("del form nv\n");
                                char *new_nv;
                                delete_tokenfile_info(smb_config.multrule[i]->server_root_path, smb_config.multrule[i]->client_root_path);
                                new_nv = delete_nvram_contents(smb_config.multrule[i]->server_root_path, smb_config.multrule[i]->client_root_path);
                                //printf("new_nv = %s\n", new_nv);
                                write_to_tokenfile(smb_config.multrule[i]->mount_path);
#ifdef NVRAM_
                                write_to_nvram(new_nv, "smb_tokenfile");
#else
                                write_to_smb_tokenfile(new_nv);
#endif
                                free(new_nv);
                        }
                }
        }
        else
        {
                for(i = 0; i < smb_config_stop.dir_num; i++)
                {
                        exist = 0;
                        for(j = 0; j < smb_config.dir_num; j++)
                        {
                                if(!strcmp(smb_config_stop.multrule[i]->server_root_path, smb_config.multrule[j]->server_root_path))
                                {
                                        exist = 1;
                                        break;
                                }
                        }
                        if(!exist)
                        {
                                char *new_nv;
                                add_tokenfile_info(smb_config_stop.multrule[i]->server_root_path, smb_config_stop.multrule[i]->client_root_path, smb_config_stop.multrule[i]->mount_path);
                                new_nv = add_nvram_contents(smb_config_stop.multrule[i]->server_root_path, smb_config_stop.multrule[i]->client_root_path);

                                write_to_tokenfile(smb_config_stop.multrule[i]->mount_path);
#ifdef NVRAM_
                                write_to_nvram(new_nv, "smb_tokenfile");
#else
                                write_to_smb_tokenfile(new_nv);
#endif
                                free(new_nv);
                        }
                }
        }
        return 0;
}
