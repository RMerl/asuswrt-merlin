#include "base.h"

int no_config;
int exit_loop;
int stop_down;
int stop_up;


int stop_progress;

int my_index;
int click_apply;

int upload_break;
int upload_file_lost;

int del_acount;

int disk_change;
int sync_disk_removed;
int sighandler_finished;
int mountflag;

int local_sync;
int server_sync;
int first_sync;

char *HW;
string ftp_url;

_info LOCAL_FILE,SERVER_FILE;
char log_base_path[MAX_LENGTH];
char log_path[MAX_LENGTH];

pthread_t newthid1,newthid2,newthid3,newthid4;
pthread_cond_t cond,cond_socket,cond_log;
pthread_mutex_t mutex,mutex_socket,mutex_socket_1,mutex_receve_socket,mutex_log,mutex_rename;

Config ftp_config,ftp_config_stop;
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
struct tokenfile_info_tag *tokenfile_info,*tokenfile_info_start,*tokenfile_info_tmp;

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
        tcapi_get(WANDUCK,"link_internet",tmp);
        link_internet = my_str_malloc(strlen(tmp) + 1);
        sprintf(link_internet,"%s",tmp);
        link_flag = atoi(link_internet);//字符串转化为值
        free(link_internet);
    #else
        char *link_internet;
        link_internet = strdup(nvram_safe_get("link_internet"));
        link_flag = atoi(link_internet);
        free(link_internet);
    #endif
#else
        char cmd_p[128] = {0};

#ifdef _PC
        //sprintf(cmd_p,"sh %s",GET_INTERNET);
#else
        sprintf(cmd_p,"sh %s",GET_INTERNET);
#endif
        system(cmd_p);
        sleep(2);

        char nv[64] = {0};
        FILE *fp;

#ifdef _PC
       // fp = fopen(VAL_INTERNET,"r");
#else
        fp = fopen(VAL_INTERNET,"r");
#endif

        fgets(nv,sizeof(nv),fp);
        fclose(fp);

        link_flag = atoi(nv);
        DEBUG("link_flag = %d\n",link_flag);
#endif
        if(!link_flag)
        {
            for(i = 0; i < ftp_config.dir_num; i++)
            {
                g_pSyncList[i]->IsNetWorkUnlink = 1;//2014.11.03 by sherry ip不对和网络连接有问题
                write_log(S_ERROR,"Network Connection Failed.","",i);
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
    }
 //2014.11.03 by sherry ip不对和网络连接有问题,不显示error信息

    for(i = 0;i < ftp_config.dir_num;i++)
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

//    for(i = 0;i < ftp_config.dir_num;i++)//原始的代码
//    {
//        write_log(S_SYNC,"","",i);
//    }
//    if(!exit_loop && IsNetWorkUnlink)//2014.10.31修改后
//    {
//        for(i = 0;i < ftp_config.dir_num;i++)
//        {
//            write_log(S_SYNC,"","",i);
//            IsNetWorkUnlink=0;
//        }
//    }
}

char *parse_nvram(char *nvram)
{
    char *nv = nvram;
    DEBUG("nv = %s\n",nv);
    char *buf,*b;
    char *new_nv = (char*)malloc(sizeof(char));
    //2014.10.30 by sherry sizeof(指针)
    //memset(new_nv,'\0',sizeof(new_nv));
    memset(new_nv,'\0',sizeof(char));//对nvram进行清零操作
    while((b = strsep(&nv, "<")) != NULL)//strsep(&nv, "<")分解字符串为一组字符串
    {
        buf = strdup(b);//strdup对串拷贝到新建的位置
        buf = (char *)realloc(buf,strlen(buf) + 2);//realloc给已经分配了空间的地址重新分配空间
        strcat(buf,">");//连接两个字符数组中的字符串
        char *p = buf;
        DEBUG("p = %s\n",p);
        int count = 0;
        while(strlen(p) != 0)
        {
            if(p[0] == '>')
            {
                count++;
                //DEBUG("count = %d\n",count);
            }
            if(count == 6)
                break;
            p++;
        }

        char *same_part = (char*)malloc(sizeof(char)*(strlen(buf) - strlen(p) + 2));
        memset(same_part,'\0',strlen(buf) - strlen(p) + 2);
        strncpy(same_part,buf,strlen(buf) - strlen(p) + 1);
        //DEBUG("same_part = %s\n",same_part);

        p = p + 3;
        //DEBUG("p = %s\n",p);
        char *q = p;
        //DEBUG("q = %s\n",q);
        int _count = 0;
        while(strlen(q) != 0)
        {
            if(q[0] == '>')
                _count++;
            if(_count != 0 && _count%2 == 0)
            {
                char *temp = (char*)malloc(sizeof(char)*(strlen(p) - strlen(q) + 1));
                memset(temp,'\0',strlen(p) - strlen(q) + 1);
                strncpy(temp,p,strlen(p) - strlen(q));
                //DEBUG("temp = %s\n",temp);
                new_nv = (char *)realloc(new_nv,strlen(new_nv) + strlen(same_part) + strlen(temp) + 1 + 3);
                strcat(new_nv,same_part);
                strcat(new_nv,temp);
                strcat(new_nv,"<");
                free(temp);
                q = q + 1;
                p = q;
                _count = 0;
            }
            q++;
        }
        free(same_part);
        free(buf);
    }
    new_nv[strlen(new_nv) - 1] = '\0';
    DEBUG("new_nv = %s\n",new_nv);
    return new_nv;
}

#ifdef NVRAM_
/*Type>Desc>URL>Rule>capacha>LocalFolder*/
int convert_nvram_to_file_mutidir(char *file,Config *config)
{
    DEBUG("enter convert_nvram_to_file_mutidir function\n");
    FILE *fp;
    char *nv, *nvp, *b;
    char *new_nv;
    int i;
    int j = 0;
    //int status;
    char *p;
    char *buffer;
    char *buf;

    fp = fopen(file, "w");

    if (fp==NULL) return -1;
    #ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD,"cloud_sync",tmp);
        nvp = my_str_malloc(strlen(tmp) + 1);
        sprintf(nvp,"%s",tmp);
    #else
        nvp = strdup(nvram_safe_get("cloud_sync"));
    #endif
    new_nv = parse_nvram(nvp);
    free(nvp);
    nv = new_nv;

    DEBUG("********nv = %s\n",nv);

    if(nv)
    {
        while ((b = strsep(&new_nv, "<")) != NULL)
        {
            i = 0;
            buf = buffer = strdup(b);
            DEBUG("buffer = %s\n",buffer);
            while((p = strsep(&buffer,">")) != NULL)
            {
                DEBUG("p = %s\n",p);
                if (*p == 0)
                {
                    fprintf(fp,"\n");
                    i++;
                    continue;
                }
                if(i == 0)
                {
                    if(atoi(p) != 2)
                        break;
                    if(j > 0)
                    {
                        fprintf(fp,"\n%s",p);
                    }
                    else
                    {
                        fprintf(fp,"%s",p);
                    }
                    j++;
                }
                else
                {
                    fprintf(fp,"\n%s",p);
                }
                i++;
            }
            free(buf);
        }
        DEBUG("j = %d\n",j);
        free(nv);
        config->dir_num = j;
    }
    else
        DEBUG("get nvram fail\n");
    fclose(fp);
    DEBUG("end convert_nvram_to_file_mutidir function\n");
    return 0;
}
#endif
int create_ftp_conf_file(Config *config)
{
    DEBUG("enter create_ftp_conf_file function\n");
    FILE *fp;
    char *nv, *nvp, *b;
    char *new_nv;
    int i;
    int j = 0;
    char *p;
    char *buffer;
    char *buf;

#ifdef _PC
    //fp = fopen(TMP_CONFIG,"r");
#else
    fp = fopen(TMP_CONFIG,"r");
#endif
    if (fp == NULL)
    {
        nvp = my_str_malloc(2);
        sprintf(nvp,"");
    }
    else
    {
        fseek( fp , 0 , SEEK_END );
        int file_size;
        file_size = ftell( fp );//文件所含字节数
        fseek(fp , 0 , SEEK_SET);
        nvp =  (char *)malloc( file_size * sizeof( char ) );
        fread(nvp , file_size , sizeof(char) , fp);
        fclose(fp);
    }
    new_nv = parse_nvram(nvp);
    free(nvp);
    nv = new_nv;
    replace_char_in_str(new_nv,'\0','\n');
    DEBUG("**********nv = %s\n",nv);
    fp = fopen(CONFIG_PATH, "w");
    if (fp==NULL) return -1;
    if(nv)
    {
        while ((b = strsep(&new_nv, "<")) != NULL)//分解字符串为一组字符串
        {
            i = 0;
            buf = buffer = strdup(b);
            DEBUG("buffer = %s\n",buffer);
            while((p = strsep(&buffer,">")) != NULL)
            {
                DEBUG("p = %s\n",p);
                if (*p == 0)
                {
                    fprintf(fp,"\n");
                    i++;
                    continue;
                }
                if(i == 0)
                {
                    if(atoi(p) != 2)
                        break;
                    if(j > 0)
                    {
                        fprintf(fp,"\n%s",p);
                    }
                    else
                    {
                        fprintf(fp,"%s",p);
                    }
                    j++;
                }
                else
                {
                    fprintf(fp,"\n%s",p);
                }
                i++;
            }
            free(buf);

        }
        free(nv);
        DEBUG("j:%d\n",j);
        config->dir_num = j;
    }
    fclose(fp);
    return 0;
}

#ifndef NVRAM_
int write_get_nvram_script(char *name,char *shell_dir,char *val_dir)
{
    FILE *fp;
    char contents[512];
    memset(contents,0,512);
    sprintf(contents,"#! /bin/sh\n" \
                    "NV=`nvram get %s`\n" \
                    "if [ ! -f \"%s\" ]; then\n" \
                        "touch %s\n" \
                    "fi\n" \
                    "echo \"$NV\" >%s",name,val_dir,val_dir,val_dir);

    if(( fp = fopen(shell_dir,"w"))==NULL)
    {
        fprintf(stderr,"create ftpclient_get_nvram file error\n");
        return -1;
    }

    fprintf(fp,"%s",contents);
    fclose(fp);

    return 0;
}
#endif

int create_shell_file()
{
    DEBUG("create shell file\n");
    FILE *fp;
    char contents[256];
    memset(contents,0,256);
#ifndef USE_TCAPI
    strcpy(contents,"#! /bin/sh\n" \
                    "nvram set $2=\"$1\"\n" \
                    "nvram commit");
#else
    strcpy(contents,"#! /bin/sh\n" \
                    "tcapi set AiCloud_Entry $2 \"$1\"\n" \
                    "tcapi commit AiCloud\n" \
                    "tcapi save");
#endif
#ifdef _PC
//    if(( fp = fopen(SHELL_FILE,"w"))==NULL)
//    {
//        fprintf(stderr,"create shell file error");
//        return -1;
//    }

//    fprintf(fp,"%s",contents);
//    fclose(fp);
//    return 0;
#else
        if(( fp = fopen(SHELL_FILE,"w"))==NULL)
        {
            fprintf(stderr,"create shell file error");
            return -1;
        }

        fprintf(fp,"%s",contents);
        fclose(fp);
        return 0;
#endif
}

int detect_process(char * process_name)
{
    FILE *ptr;
    char buff[512];
    char ps[128];
    sprintf(ps,"ps | grep -c %s",process_name);
    strcpy(buff,"ABNORMAL");
    if((ptr=popen(ps, "r")) != NULL)
    {
        while (fgets(buff, 512, ptr) != NULL)
        {
            if(atoi(buff)>2)
            {
                pclose(ptr);
                return 1;
            }
        }
    }
    if(strcmp(buff,"ABNORMAL")==0)  /*ps command error*/
    {
        return 0;
    }
    pclose(ptr);
    return 0;
}

#ifdef NVRAM_
int write_to_nvram(char *contents,char *nv_name)
{
    char *command;
    command = my_str_malloc(strlen(contents)+strlen(SHELL_FILE)+strlen(nv_name)+8);
    sprintf(command,"sh %s \"%s\" %s",SHELL_FILE,contents,nv_name);//8是：字符sh，3个空格，两个转义字符\" 以及\0
    DEBUG("command : [%s]\n",command);
    system(command);
    free(command);

    return 0;
}
#else
int write_to_ftp_tokenfile(char *contents)
{
    if(contents == NULL || contents == "")
    {
        unlink(TOKENFILE_RECORD);
        return 0;
    }
    FILE *fp;
    if( ( fp = fopen(TOKENFILE_RECORD,"w") ) == NULL )
    {
        DEBUG("open ftp_tokenfile failed!\n");
        return -1;
    }
    fprintf(fp,"%s",contents);
    fclose(fp);
    //DEBUG("write_to_ftp_tokenfile end\n");
    return 0;
}
#endif

int delete_tokenfile_info(char *url,char *folder){

    struct tokenfile_info_tag *tmp;

    tmp = tokenfile_info_start;
    tokenfile_info_tmp = tokenfile_info_start->next;

    while(tokenfile_info_tmp != NULL)
    {
        if( !strcmp(tokenfile_info_tmp->url,url) &&
            !strcmp(tokenfile_info_tmp->folder,folder))
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
    //write_debug_log("delete_nvram_contents start");

    char *nv;
    char *nvp;
    char *p,*b;
    //char buffer;
    //const char *split = "<";
    char *new_nv;
    int n;
    int i=0;
#ifdef NVRAM_
    #ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD,"ftp_tokenfile",tmp);
        nv = my_str_malloc(strlen(tmp) + 1);
        sprintf(nv,"%s",tmp);
	p = nv;
    #else
        p = nv = strdup(nvram_safe_get("ftp_tokenfile"));
    #endif
#else
    FILE *fp;
    fp = fopen("/opt/etc/ftp_tokenfile","r");
    if (fp==NULL)
    {
        nv = my_str_malloc(2);
        sprintf(nv,"");
    }
    else
    {
        fseek( fp , 0 , SEEK_END );
        int file_size;
        file_size = ftell( fp );
        fseek(fp , 0 , SEEK_SET);
        //nv =  (char *)malloc( file_size * sizeof( char ) );
        nv = my_str_malloc(file_size+2);
        //fread(nv , file_size , sizeof(char) , fp);
        fscanf(fp,"%[^\n]%*c",nv);//[^\n]字符串的分隔符是 "\n",中括号里可以写分隔符表
        p = nv;
        fclose(fp);
    }

#endif

    nvp = my_str_malloc(strlen(url)+strlen(folder)+2);
    sprintf(nvp,"%s>%s",url,folder);

    //write_debug_log(nv);

    if(strstr(nv,nvp) == NULL)
    {
        free(nvp);
        return nv;
    }

    //write_debug_log("first if");

    if(!strcmp(nv,nvp))
    {
        free(nvp);
        memset(nv,0,sizeof(nv));
        sprintf(nv,"");
        return nv;
    }

    //write_debug_log("second if");

    if(nv)
    {
        //write_debug_log("if nv");
        while((b = strsep(&p, "<")) != NULL)
        {
            //write_debug_log(b);
            if(strcmp(b,nvp))
            {
                n = strlen(b);
                if(i == 0)
                {
                    new_nv = my_str_malloc(n+1);
                    sprintf(new_nv,"%s",b);
                    ++i;
                }
                else
                {
                    new_nv = (char*)realloc(new_nv, strlen(new_nv)+n+2);
                    sprintf(new_nv,"%s<%s",new_nv,b);
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
    FILE *fp;
    //int flag=0;

    char *filename;

    filename = my_str_malloc(strlen(mpath) + 21 + 1);
    //filename = my_str_malloc(strlen(mpath) + strlen("/.ftpclient_tokenfile") + 1);
    sprintf(filename,"%s/.ftpclient_tokenfile",mpath);

    int i = 0;
    if( ( fp = fopen(filename,"w") ) == NULL )
    {
        DEBUG("open tokenfile failed!\n");
        return -1;
    }

    tokenfile_info_tmp = tokenfile_info_start->next;
    while(tokenfile_info_tmp != NULL)
    {
        DEBUG("tokenfile_info_tmp->mountpath = %s\n",tokenfile_info_tmp->mountpath);
        if(!strcmp(tokenfile_info_tmp->mountpath,mpath))
        {
            //write_debug_log(tokenfile_info_tmp->folder);
            if(i == 0)
            {
                fprintf(fp,"%s\n%s",tokenfile_info_tmp->url,tokenfile_info_tmp->folder);
                i=1;
            }
            else
            {
                fprintf(fp,"\n%s\n%s",tokenfile_info_tmp->url,tokenfile_info_tmp->folder);
            }
            //flag = 1;
        }

        tokenfile_info_tmp = tokenfile_info_tmp->next;
    }

    fclose(fp);
    if(!i)
        remove(filename);
    free(filename);

    return 0;
}

int add_tokenfile_info(char *url,char *folder,char *mpath)
{
    DEBUG("add_tokenfile_info start\n");
    if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
    {
        return -1;
    }

    tokenfile_info_tmp->url = my_str_malloc(strlen(url)+1);
    sprintf(tokenfile_info_tmp->url,"%s",url);

    tokenfile_info_tmp->mountpath = my_str_malloc(strlen(mpath)+1);
    sprintf(tokenfile_info_tmp->mountpath,"%s",mpath);

    tokenfile_info_tmp->folder = my_str_malloc(strlen(folder)+1);
    sprintf(tokenfile_info_tmp->folder,"%s",folder);

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

    nvp = my_str_malloc(strlen(url)+strlen(folder)+2);
    //nvp = my_str_malloc(strlen(url)+strlen(folder)+strlen(">")+1);
    sprintf(nvp,"%s>%s",url,folder);

    DEBUG("add_nvram_contents     nvp = %s\n",nvp);

#ifdef NVRAM_
    #ifdef USE_TCAPI
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(AICLOUD,"ftp_tokenfile",tmp);
        nv = my_str_malloc(strlen(tmp) + 1);
        sprintf(nv,"%s",tmp);
	p = nv;
    #else
    	nv = strdup(nvram_safe_get("ftp_tokenfile"));
    #endif
#else
    FILE *fp;
    fp = fopen("/opt/etc/ftp_tokenfile","r");
    if (fp==NULL)
    {
        nv = my_str_malloc(2);
        sprintf(nv,"");
    }
    else
    {
        fseek( fp , 0 , SEEK_END );
        int file_size;
        file_size = ftell( fp );
        fseek(fp , 0 , SEEK_SET);
        DEBUG("add_nvram_contents     file_size = %d\n",file_size);
        //nv =  (char *)malloc( file_size * sizeof( char ) );
        nv = my_str_malloc(file_size+2);
        //fread(nv , file_size ,1, fp);
        fscanf(fp,"%[^\n]%*c",nv);
        fclose(fp);
    }
#endif
    DEBUG("add_nvram_contents     nv = %s\n",nv);
    nv_len = strlen(nv);

    if(nv_len)
    {
        new_nv = my_str_malloc(strlen(nv)+strlen(nvp)+2);
        sprintf(new_nv,"%s<%s",nv,nvp);

    }
    else
    {
        new_nv = my_str_malloc(strlen(nvp)+1);
        sprintf(new_nv,"%s",nvp);
    }

    DEBUG("add_nvram_contents     new_nv = %s\n",new_nv);
    free(nvp);
    free(nv);
    DEBUG("add_nvram_contents end\n");
    return new_nv;
}

int rewrite_tokenfile_and_nv()
{
    int i,j;
    int exist;
    DEBUG("rewrite_tokenfile_and_nv start\n");
    if(ftp_config.dir_num > ftp_config_stop.dir_num)
    {
        DEBUG("ftp_config.dir_num > ftp_config_stop.dir_num\n");
        for(i=0;i<ftp_config.dir_num;i++)
        {
            exist = 0;
            for(j=0;j<ftp_config_stop.dir_num;j++)
            {
                if(!strcmp(ftp_config_stop.multrule[j]->fullrooturl,ftp_config.multrule[i]->fullrooturl))
                {
                    exist = 1;
                    break;
                }
            }
            if(!exist)
            {
                //write_debug_log("del form nv");
                char *new_nv;
                del_acount = i;
                delete_tokenfile_info(ftp_config.multrule[i]->fullrooturl,ftp_config.multrule[i]->base_folder);
                DEBUG("%s\n",ftp_config.multrule[i]->fullrooturl);
                DEBUG("%s\n",ftp_config.multrule[i]->base_folder);
                new_nv = delete_nvram_contents(ftp_config.multrule[i]->fullrooturl,ftp_config.multrule[i]->base_folder);
                write_to_tokenfile(ftp_config.multrule[i]->mount_path);
#ifdef NVRAM_
                write_to_nvram(new_nv,"ftp_tokenfile");
#else
                write_to_ftp_tokenfile(new_nv);
#endif
                free(new_nv);
            }
        }
    }
    else
    {
        for(i = 0;i < ftp_config_stop.dir_num;i++)
        {
            exist = 0;
            for(j = 0;j < ftp_config.dir_num;j++)
            {
                if(!strcmp(ftp_config_stop.multrule[i]->fullrooturl,ftp_config.multrule[j]->fullrooturl))
                {
                    exist = 1;
                    break;
                }
            }
            if(!exist)
            {
                DEBUG("add to nv\n");
                char *new_nv;
                add_tokenfile_info(ftp_config_stop.multrule[i]->fullrooturl,ftp_config_stop.multrule[i]->base_folder,ftp_config_stop.multrule[i]->mount_path);
                new_nv = add_nvram_contents(ftp_config_stop.multrule[i]->fullrooturl,ftp_config_stop.multrule[i]->base_folder);
                write_to_tokenfile(ftp_config_stop.multrule[i]->mount_path);
#ifdef NVRAM_
                write_to_nvram(new_nv,"ftp_tokenfile");
#else
                write_to_ftp_tokenfile(new_nv);
#endif
                free(new_nv);
            }
        }
    }
    return 0;
}

int write_notify_file(char *path,int signal_num)
{
    FILE *fp;
    char fullname[64];
    memset(fullname,0,sizeof(fullname));

    my_local_mkdir("/tmp/notify");
    my_local_mkdir("/tmp/notify/usb");
    sprintf(fullname,"%s/ftpclient",path);
    fp = fopen(fullname,"w");
    if(NULL == fp)
    {
        DEBUG("open notify %s file fail\n",fullname);
        return -1;
    }
    fprintf(fp,"%d",signal_num);
    fclose(fp);
    return 0;
}

int free_tokenfile_info(struct tokenfile_info_tag *head)
{
    struct tokenfile_info_tag *p = head->next;
    while(p != NULL)
    {
        head->next = p->next;
        if(p->folder != NULL)
        {
            //DEBUG("free CloudFile %s\n",p->href);
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

/*检查同步目录所在的硬碟还是否存在*/
int check_sync_disk_removed()
{
    DEBUG("check_sync_disk_removed start! \n");

    int ret;

    free_tokenfile_info(tokenfile_info_start);

    if(get_tokenfile_info()==-1)
    {
        DEBUG("\nget_tokenfile_info failed\n");
        exit(-1);
    }

    ret = check_config_path(0);
    return ret;

}

int get_mounts_info(struct mounts_info_tag *info)
{
    int len = 0;
    FILE *fp;
    int i = 0;
    int num = 0;
    //char *mount_path;
    char **tmp_mount_list=NULL;
    char **tmp_mount=NULL;

    char buf[len+1];
    memset(buf,'\0',sizeof(buf));
    char a[1024];
    //char *temp;
    char *p,*q;
    fp = fopen("/proc/mounts","r");
    if(fp)
    {
        while(!feof(fp))
        {
            memset(a,'\0',sizeof(a));
            fscanf(fp,"%[^\n]%*c",a);
            if((strlen(a) != 0)&&((p=strstr(a,"/dev/sd")) != NULL))
            {
                DEBUG("%s\n",p);
                if((q=strstr(p,"/tmp")) != NULL)
                {
                    if((p=strstr(q," ")) != NULL)
                    {
                        len = strlen(q) - strlen(p)+1;

                        tmp_mount = (char **)malloc(sizeof(char *)*(num+1));
                        if(tmp_mount == NULL)
                        {
                            fclose(fp);
                            return -1;
                        }

                        tmp_mount[num] = my_str_malloc(len+1);
                        if(tmp_mount[num] == NULL)
                        {
                            free(tmp_mount);
                            fclose(fp);
                            return -1;
                        }
                        snprintf(tmp_mount[num],len,"%s",q);

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

    memset(info,0,sizeof(struct mounts_info_tag));
    memset(buffer,0,1024);

    if(get_mounts_info(info) == -1)
    {
        DEBUG("get mounts info fail\n");
        return -1;
    }

    DEBUG("%d\n",info->num);
    for(i=0;i<info->num;i++)
    {
        DEBUG("info->paths[%d] = %s\n",i,info->paths[i]);

        tokenfile = my_str_malloc(strlen(info->paths[i]) + 21 + 1);
        //tokenfile = my_str_malloc(strlen(info->paths[i]) + strlen("/.ftpclient_tokenfile") + 1);
        sprintf(tokenfile,"%s/.ftpclient_tokenfile",info->paths[i]);
        DEBUG("tokenfile = %s\n",tokenfile);
        if(!access(tokenfile,F_OK))
        {
            DEBUG("tokenfile is exist!\n");
            if((fp = fopen(tokenfile,"r"))==NULL)
            {
                fprintf(stderr,"read tokenfile error\n");
                exit(-1);
            }
            while(fgets(buffer,1024,fp)!=NULL)
            {
                if( buffer[ strlen(buffer)-1 ] == '\n' )
                    buffer[ strlen(buffer)-1 ] = '\0';
                p = buffer;
                DEBUG("p = %s\n",p);
                if(j == 0)
                {
                    if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
                    {
                        return -1;
                    }
                    tokenfile_info_tmp->url = my_str_malloc(strlen(p)+1);
                    sprintf(tokenfile_info_tmp->url,"%s",p);
                    tokenfile_info_tmp->mountpath = my_str_malloc(strlen(info->paths[i])+1);
                    sprintf(tokenfile_info_tmp->mountpath,"%s",info->paths[i]);
                    j++;
                }
                else
                {
                    tokenfile_info_tmp->folder = my_str_malloc(strlen(p)+1);
                    sprintf(tokenfile_info_tmp->folder,"%s",p);
                    tokenfile_info->next = tokenfile_info_tmp;
                    tokenfile_info = tokenfile_info_tmp;
                    j = 0;
                }
            }
            fclose(fp);
        }
        free(tokenfile);
    }

    for(i=0;i<info->num;++i)
    {
        free(info->paths[i]);
    }
    if(info->paths != NULL)
        free(info->paths);
    free(info);
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
        tcapi_get(AICLOUD,"ftp_tokenfile",tmp);
        nv = my_str_malloc(strlen(tmp) + 1);
        sprintf(nv,"%s",tmp);
	p = nv;
    #else
    	nv = strdup(nvram_safe_get("ftp_tokenfile"));
    #endif
#else
    FILE *fp;
    fp = fopen(TOKENFILE_RECORD,"r");
    if(fp==NULL)
    {
        nv = my_str_malloc(2);
        sprintf(nv,"");
    }
    else
    {
        fseek( fp , 0 , SEEK_END );
        int file_size;
        file_size = ftell( fp );
        fseek(fp , 0 , SEEK_SET);
        //nv =  (char *)malloc( file_size * sizeof( char ) );
        nv = my_str_malloc(file_size+2);
        //fread(nv , file_size , sizeof(char) , fp);
        fscanf(fp,"%[^\n]%*c",nv);
        fclose(fp);
    }
#endif
    nv_len = strlen(nv);

    //write_debug_log("check_config_path");
    DEBUG("nv_len = %d\n",nv_len);

    for(i=0;i<ftp_config.dir_num;i++)
    {
        flag = 0;
        tokenfile_info_tmp = tokenfile_info_start->next;
        while(tokenfile_info_tmp != NULL)
        {
            if( !strcmp(tokenfile_info_tmp->url,ftp_config.multrule[i]->fullrooturl) &&
                !strcmp(tokenfile_info_tmp->folder,ftp_config.multrule[i]->base_folder))
            {
                if(strcmp(tokenfile_info_tmp->mountpath,ftp_config.multrule[i]->mount_path))
                {
                    memset(ftp_config.multrule[i]->mount_path,0,sizeof(ftp_config.multrule[i]->mount_path));
                    sprintf(ftp_config.multrule[i]->mount_path,"%s",tokenfile_info_tmp->mountpath);
                    memset(ftp_config.multrule[i]->base_path,0,sizeof(ftp_config.multrule[i]->base_path));
                    sprintf(ftp_config.multrule[i]->base_path,"%s%s",tokenfile_info_tmp->mountpath,tokenfile_info_tmp->folder);
                    ftp_config.multrule[i]->base_path_len = strlen(ftp_config.multrule[i]->base_path);
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
        if(!flag)
        {

            nvp = my_str_malloc(strlen(ftp_config.multrule[i]->fullrooturl)+strlen(ftp_config.multrule[i]->base_folder)+2);
            //nvp = my_str_malloc(strlen(ftp_config.multrule[i]->fullrooturl)+strlen(ftp_config.multrule[i]->base_folder)+strlen(">")+1);
            sprintf(nvp,"%s>%s",ftp_config.multrule[i]->fullrooturl,ftp_config.multrule[i]->base_folder);

            //write_debug_log(nv);
            //write_debug_log(nvp);

            if(!is_read_config)
            {
                if(g_pSyncList[i]->sync_disk_exist == 1)
                    is_path_change = 2;   //remove the disk and the mout_path not change
            }

            DEBUG("write nvram and tokenfile if before\n");

            if(strstr(nv,nvp) == NULL)
            {
                DEBUG("write nvram and tokenfile if behind");

                if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
                {
                    return -1;
                }
                tokenfile_info_tmp->url = my_str_malloc(strlen(ftp_config.multrule[i]->fullrooturl) + 1);
                sprintf(tokenfile_info_tmp->url,"%s",ftp_config.multrule[i]->fullrooturl);

                tokenfile_info_tmp->mountpath = my_str_malloc(strlen(ftp_config.multrule[i]->mount_path) + 1);
                sprintf(tokenfile_info_tmp->mountpath,"%s",ftp_config.multrule[i]->mount_path);

                tokenfile_info_tmp->folder = my_str_malloc(strlen(ftp_config.multrule[i]->base_folder) + 1);
                sprintf(tokenfile_info_tmp->folder,"%s",ftp_config.multrule[i]->base_folder);

                tokenfile_info->next = tokenfile_info_tmp;
                tokenfile_info = tokenfile_info_tmp;

                write_to_tokenfile(ftp_config.multrule[i]->mount_path);

                if(nv_len)
                {

                    new_nv = my_str_malloc(strlen(nv)+strlen(nvp) + 2);
                    //new_nv = my_str_malloc(strlen(nv)+strlen(nvp) + strlen("<")+1);
                    sprintf(new_nv,"%s<%s",nv,nvp);

                }
                else
                {
                    new_nv = my_str_malloc(strlen(nvp)+1);
                    sprintf(new_nv,"%s",nvp);
                }
#ifdef NVRAM_
                write_to_nvram(new_nv,"ftp_tokenfile");
#else
                write_to_ftp_tokenfile(new_nv);
#endif
                free(new_nv);
            }
            free(nvp);
        }
    }
    free(nv);
    return is_path_change;
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
char *my_nstrchr(const char chr,char *str,int n){

    if(n<1)
    {
        DEBUG("my_nstrchr need n>=1\n");
        return NULL;
    }

    char *p1,*p2;
    int i = 1;
    p1 = str;

    do{
        p2 = strchr(p1,chr);
        p1 = p2;
        p1++;
        i++;
    }while(p2!=NULL && i<=n);

    if(i<n)
    {
        return NULL;
    }

    return p2;
}

static void *xmalloc_fatal(size_t size) {
    if (size==0) return NULL;
    fprintf(stderr, "Out of memory.");
    exit(1);
}

void *xmalloc (size_t size) {
    void *ptr = malloc (size);
    if (ptr == NULL) return xmalloc_fatal(size);
    return ptr;
}

void *xrealloc (void *ptr, size_t size) {
    void *p = realloc (ptr, size);
    if (p == NULL) return xmalloc_fatal(size);
    return p;
}

char *xstrdup (const char *s) {
    void *ptr = xmalloc(strlen(s)+1);
    strcpy ((char*)ptr, s);
    return (char*) ptr;
}

/**
 * Escape 'string' according to RFC3986 and
 * http://oauth.net/core/1.0/#encoding_parameters.
 *
 * @param string The data to be encoded
 * @return encoded string otherwise NULL
 * The caller must free the returned string.
 */
char *oauth_url_escape(const char *string) {
    size_t alloc, newlen;
    char *ns = NULL, *testing_ptr = NULL;
    unsigned char in;
    size_t strindex=0;
    size_t length;

    if (!string) return xstrdup("");

    alloc = strlen(string)+1;
    newlen = alloc;

    ns = (char*) xmalloc(alloc);

    length = alloc-1;
    while(length--) {
        in = *string;

        switch(in){
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '_': case '~': case '.': case '-':
            ns[strindex++]=in;
            break;
        default:
            newlen += 2; /* this'll become a %XX */
            if(newlen > alloc) {
                alloc *= 2;
                testing_ptr = (char*) xrealloc(ns, alloc);
                ns = testing_ptr;
            }
            //snprintf(&ns[strindex], 4, "%%%02X", in);
            snprintf(&ns[strindex], 4, "%%%02x", in);
            strindex+=3;
            break;
        }
        string++;
    }
    ns[strindex]=0;
    return ns;
}

#ifndef ISXDIGIT
# define ISXDIGIT(x) (isxdigit((int) ((unsigned char)x)))
#endif

/**
 * Parse RFC3986 encoded 'string' back to  unescaped version.
 *
 * @param string The data to be unescaped
 * @param olen unless NULL the length of the returned string is stored there.
 * @return decoded string or NULL
 * The caller must free the returned string.
 */
char *oauth_url_unescape(const char *string, size_t *olen) {
    size_t alloc, strindex=0;
    char *ns = NULL;
    unsigned char in;
    long hex;

    if (!string) return NULL;
    alloc = strlen(string)+1;
    ns = (char*) xmalloc(alloc);

    while(--alloc > 0) {
        in = *string;
        if(('%' == in) && ISXDIGIT(string[1]) && ISXDIGIT(string[2])) {
            char hexstr[3]; // '%XX'
            hexstr[0] = string[1];
            hexstr[1] = string[2];
            hexstr[2] = 0;
            hex = strtol(hexstr, NULL, 16);
            in = (unsigned char)hex; /* hex is always < 256 */
            string+=2;
            alloc-=2;
        }
        ns[strindex++] = in;
        string++;
    }
    ns[strindex]=0;
    if(olen) *olen = strindex;
    return ns;
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
            filetmp = get_CloudFile_node(g_pSyncList[index]->ServerRootNode,p->path,0x2);
            if(filetmp == NULL)   //if filetmp == NULL,it means server has deleted filetmp
            {
                DEBUG("filetmp is NULL\n");

                p1 = p->next;
                del_action_item("download",p->path,g_pSyncList[index]->unfinished_list);
                p = p1;
                continue;
            }
            char *localpath = serverpath_to_localpath(filetmp->href,index);
            DEBUG("localpath = %s\n",localpath);
            if(is_local_space_enough(filetmp,index))
            {
                add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                ret = download_(filetmp->href,index);
                if (ret == 0)
                {
                    ChangeFile_modtime(localpath,filetmp->modtime,index);
                    p1 = p->next;
                    del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    p = p1;
                }
                else
                {
                    DEBUG("download %s failed",filetmp->href);
                    p = p->next;
                    //return ret;
                }
            }
            else
            {
                write_log(S_ERROR,"local space is not enough!","",index);
                p = p->next;
            }
            free(localpath);
        }
        else if(!strcmp(p->action,"upload"))
        {
            p1 = p->next;
            char *serverpath;
            serverpath = localpath_to_serverpath(p->path,index);
            char *path_temp;
            path_temp = my_str_malloc(strlen(p->path)+1);
            sprintf(path_temp,"%s",p->path);
            ret = upload(p->path,index);
            DEBUG("********* upload ret = %d\n",ret);

            if(ret == 0)
            {
                mod_time *modt = Getmodtime(serverpath,index);
                if(modt->modtime != -1)
                {
                    ChangeFile_modtime(path_temp,modt->modtime,index);
                    free(modt);
                }
                else
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

char *get_currtm()
{
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    //DEBUG("%s",ctime(&rawtime));
    char *ret = my_str_malloc(22);
    sprintf(ret,"%d-%d-%d %d:%d:%d",1900+timeinfo->tm_year,1+timeinfo->tm_mon,timeinfo->tm_mday,
           timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
    return ret;
}

char *write_error_message(char *format,...)
{
    int size=256;
    char *p=(char *)malloc(size);
    memset(p,0,size);//2014.11.11 by sherry
    //memset(p,0,sizeof(p));
    va_list ap;
    va_start(ap,format);
    vsnprintf(p,size,format,ap);
    va_end(ap);
    return p;
}

int write_conflict_log(char *fullname, int type,char *msg)
{
    FILE *fp = 0;
    //int len;
    char ctype[16] = {0};

    if(type == 1)
        strcpy(ctype,"Error");
    else if(type == 2)
        strcpy(ctype,"Info");
    else if(type == 3)
        strcpy(ctype,"Warning");


    if(access(CONFLICT_DIR,0) == 0)
        fp = fopen(CONFLICT_DIR,"a");
    else
        fp = fopen(CONFLICT_DIR,"w");


    if(fp == NULL)
    {
        printf("open %s fail\n",CONFLICT_DIR);
        return -1;
    }

    //len = strlen(mount_path);
    fprintf(fp,"TYPE:%s\nUSERNAME:%s\nFILENAME:%s\nMESSAGE:%s\n",ctype,"NULL",fullname,msg);
    //fprintf(fp,"ERR_CODE:%d\nMOUNT_PATH:%s\nFILENAME:%s\nRULENUM:%d\n",
                //err_code,mount_path,fullname+len,0);
    fclose(fp);
    return 0;
}

int write_log(int status, char *message, char *filename,int index)
{
    if(exit_loop)
    {
        return 0;
    }
    DEBUG("write log status = %d\n",status);
    pthread_mutex_lock(&mutex_log);
    struct timeval now;
    struct timespec outtime;
    FILE *fp;
    //int ret;
    Log_struc log_s;
    memset(&log_s,0,LOG_SIZE);

    log_s.status = status;

    fp = fopen(LOG_DIR,"w");
    if(fp == NULL)
    {
        DEBUG("open logfile error.\n");
        pthread_mutex_unlock(&mutex_log);
        return -1;
    }

    if(log_s.status == S_ERROR)
    {
        DEBUG("******** status is ERROR *******\n");
        //2014.10.31 by sherry 写入log的RuleNum的值出错为4527456
        //fprintf(fp,"STATUS:%d\nMESSAGE:%s\nTOTAL_SPACE:%lld\nUSED_SPACE:%lld\nRULENUM:%d\n",log_s.status,message,0,0,index);
        //字串写错 cookie抓不到值
        //fprintf(fp,"STATUS:%d\nMESSAGE:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",log_s.status,message,0);
        fprintf(fp,"STATUS:%d\nERR_MSG:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",log_s.status,message,0);
        DEBUG("Status:%d\tERR_MSG:%s\tRule:%d\n",log_s.status,message,index);
    }
    else if(log_s.status == S_DOWNLOAD)
    {
        DEBUG("******** status is DOWNLOAD *******\n");
        strcpy(log_s.path,filename);
        fprintf(fp,"STATUS:%d\nMOUNT_PATH:%s\nFILENAME:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",log_s.status,log_s.path,filename,index);
        DEBUG("Status:%d\tDownloading:%s\tRule:%d\n",log_s.status,log_s.path,index);
    }
    else if(log_s.status == S_UPLOAD)
    {
        DEBUG("******** status is UPLOAD *******\n");
        strcpy(log_s.path,filename);
        fprintf(fp,"STATUS:%d\nMOUNT_PATH:%s\nFILENAME:%s\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",log_s.status,log_s.path,filename,index);
        DEBUG("Status:%d\tUploading:%s\tRule:%d\n",log_s.status,log_s.path,index);
    }
    else
    {
        if (log_s.status == S_INITIAL)
            DEBUG("******** other status is INIT *******\n");
        else
            DEBUG("******** other status is SYNC *******\n");
        fprintf(fp,"STATUS:%d\nTOTAL_SPACE:0\nUSED_SPACE:0\nRULENUM:%d\n",log_s.status,index);
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

int my_stat(_info *FILE_INFO)
{
    char *prepath = get_prepath(FILE_INFO->path,strlen(FILE_INFO->path));
    int exist = is_server_exist(prepath,FILE_INFO->path,FILE_INFO->index);
    if(exist == 1)
    {
        free(prepath);
        return 0;
    }
    else
    {
        free(prepath);
        return -1;
    }
}

int debugFun(CURL *curl,curl_infotype type,char *str,size_t len,void *stream)
{
    //if(type == CURLINFO_TEXT)
        fwrite(str,1,len,(FILE*)stream);
}

size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if(exit_loop)
    {
        DEBUG("set download_break 1\n");
        upload_break = 1;
        return -1;
    }

    int len ;
    char *temp = (char*)malloc(sizeof(char)*(strlen(SERVER_FILE.path) + 9));
    //char *temp = (char*)malloc(sizeof(char)*(strlen(SERVER_FILE.path) + strlen(".asus.td")+1));
    sprintf(temp, "%s.asus.td", SERVER_FILE.path);
    struct stat info;
    if(stat(temp,&info) == -1)
    {
        DEBUG("set download_file_lost 1\n");
        upload_file_lost = 1;
        free(temp);//valgrind
        return -1;
    }

//    if(my_stat(&SERVER_FILE) == -1)
//    {
//        DEBUG("break!\n");
//        return size*nmemb - 1;
//        //return -1;
//    }
    free(temp);//valgrind
    len = fwrite(ptr, size, nmemb, stream);
    return len;

}

size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if(exit_loop)
    {
        DEBUG("set upload_break 1\n");
        upload_break = 1;
        return -1;
    }

    struct stat info;
    char *ret = NULL;
    int status = stat(LOCAL_FILE.path,&info);
    if(status == -1)
    {
        usleep(1000*100);
        ret = search_newpath(LOCAL_FILE.path,LOCAL_FILE.index);
        if(ret == NULL)
        {
            DEBUG("set upload_file_lost 1\n");
            upload_file_lost = 1;
            return -1;
        }
        else
        {
            free(LOCAL_FILE.path);
            LOCAL_FILE.path = (char *)malloc(sizeof(char)*(strlen(ret) + 1));
            sprintf(LOCAL_FILE.path,"%s",ret);
            free(ret);
        }
    }

    int len;
    len = fread(ptr, size, nmemb, stream);
    return len;
}

int my_progress_func(char *progress_data,double t, double d,double ultotal,double ulnow)
{
    if(exit_loop)
        return -1;

    if(t > 1 && d > 10) // download
    {
        DEBUG("1\n");
        DEBUG("\r%s %10.0f / %10.0f (%g %%)", progress_data, d, t, d*100.0/t);
    }else{
        DEBUG("2\n");
        //DEBUG("\r%s %10.0f / %10.0f (%g %%)", progress_data, ulnow, ultotal, ulnow*100.0/ultotal);
        DEBUG("\r%s %10.0f", progress_data,ulnow);
    }
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

int deal_dragfolder_to_socketlist(char *dir,int index)
{
    DEBUG("dir = %s\n",dir);

    int status;
    struct dirent *ent = NULL;
    char info[512];
    memset(info,0,sizeof(info));
    DIR *pDir;
    pDir=opendir(dir);
    if(pDir != NULL)
    {
        while((ent=readdir(pDir)) != NULL)
        {
            if(ent->d_name[0] == '.')
                continue;
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            char *fullname;

            fullname = (char *)malloc(sizeof(char)*(strlen(dir)+strlen(ent->d_name)+2));
            //fullname = (char *)malloc(sizeof(char)*(strlen(dir)+strlen(ent->d_name)+strlen("/")+1));
            //2014.10.30 by sherry  sizeof(指针)=4
            //memset(fullname,'\0',sizeof(fullname));
            //memset(fullname,'\0',sizeof(char)*(strlen(dir)+strlen(ent->d_name)+strlen("/")+1));
            memset(fullname,'\0',sizeof(char)*(strlen(dir)+strlen(ent->d_name)+2));
            sprintf(fullname,"%s/%s",dir,ent->d_name);
            if(test_if_dir(fullname) == 1)
            {
                sprintf(info,"createfolder%s%s%s%s","\n",dir,"\n",ent->d_name);
                pthread_mutex_lock(&mutex_socket);
                add_socket_item(info,index);
                pthread_mutex_unlock(&mutex_socket);
                status = deal_dragfolder_to_socketlist(fullname,index);
            }
            else
            {
                sprintf(info,"createfile%s%s%s%s","\n",dir,"\n",ent->d_name);
                pthread_mutex_lock(&mutex_socket);
                add_socket_item(info,index);
                pthread_mutex_unlock(&mutex_socket);
            }
            free(fullname);
        }
        closedir(pDir);
        return 0;
    }
}

int add_all_download_only_dragfolder_socket_list(const char *dir,int index)
{
    struct dirent* ent = NULL;
    char *fullname;
    int fail_flag = 0;
    DIR *dp = opendir(dir);

    if(dp == NULL)
    {
        DEBUG("opendir %s fail",dir);
        fail_flag = 1;
        return -1;
    }

    while (NULL != (ent=readdir(dp)))
    {

        if(ent->d_name[0] == '.')
            continue;
        if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
            continue;


        fullname = my_str_malloc((size_t)(strlen(dir)+strlen(ent->d_name)+2));
        //fullname = my_str_malloc((size_t)(strlen(dir)+strlen(ent->d_name)+strlen("/")+1));

        sprintf(fullname,"%s/%s",dir,ent->d_name);

        if( test_if_dir(fullname) == 1)
        {
            add_action_item("createfolder",fullname,g_pSyncList[index]->dragfolder_action_list);
            add_action_item("createfolder",fullname,g_pSyncList[index]->download_only_socket_head);
            add_all_download_only_dragfolder_socket_list(fullname,index);
        }
        else
        {
            add_action_item("createfile",fullname,g_pSyncList[index]->dragfolder_action_list);
            add_action_item("createfile",fullname,g_pSyncList[index]->download_only_socket_head);
        }
        free(fullname);
    }

    closedir(dp);
    return (fail_flag == 1) ? -1 : 0;
}

mod_time *Getmodtime(char *href,int index)
{
    DEBUG("\n********************get one file's modtime********************\n");
    mod_time *mtime_1;

    char *command = (char *)malloc(sizeof(char)*(strlen(href) + 6));
    //char *command = (char *)malloc(sizeof(char)*(strlen(href) + strlen("MDTM ")+1));
    //2014.10.29 by sherry   sizeof(指针)=4
    //memset(command,'\0',sizeof(command));
    //memset(command,'\0',sizeof(char)*(strlen(href) + strlen("MDTM ")+1));
    memset(command,'\0',sizeof(char)*(strlen(href) + 6));
    sprintf(command,"MDTM %s",href + 1);
    DEBUG("command:%s\n",command);
    char *temp = utf8_to(command,index);
    free(command);
    DEBUG("%s\n",temp);
    CURL *curl_handle;
    CURLcode res;
    curl_handle = curl_easy_init();
    FILE *fp = fopen(TIME_ONE_DIR,"w+");;
    //fp=fopen("/tmp/ftpclient/time_one.txt","w+");
    if(curl_handle)
    {
        curl_easy_setopt(curl_handle, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl_handle, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        //curl_easy_setopt(curl_handle, CURLOPT_FTP_USE_EPSV, 0);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER,fp);
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST,temp);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME,30);
        curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl_handle);
        DEBUG("%d\n",res);
        if(res != CURLE_OK && res != CURLE_FTP_COULDNT_RETR_FILE)
        {
            curl_easy_cleanup(curl_handle);
            fclose(fp);
            free(temp);
            //DEBUG("get %s mtime failed res = %d\n",href,res);
            if(res == CURLE_COULDNT_CONNECT)
            {
                write_log(S_ERROR,"Could not connect.","",index);
            }
            else if(res == CURLE_OPERATION_TIMEDOUT)
            {
                write_log(S_ERROR,"Connect timeout.","",index);
            }
            else if(res == CURLE_REMOTE_ACCESS_DENIED)
            {
                write_log(S_ERROR,"Connect refused.","",index);
            }
            mtime_1 = (mod_time *)malloc(sizeof(mod_time));
            memset(mtime_1, 0, sizeof(mtime_1));
            mtime_1->modtime = (time_t)-1;
            return mtime_1;
        }
        else
        {
            free(temp);
            curl_easy_cleanup(curl_handle);
            mtime_1 = get_mtime_1(fp);
            return mtime_1;
        }
    }
}

//int add_socket_item_for_refresh(char *buf,int i)
//{
//    DEBUG("comeing add_socket_item_for_refresh\n");
//
//    pthread_mutex_lock(&mutex_receve_socket);
//    g_pSyncList[i]->receve_socket = 1;
//    pthread_mutex_unlock(&mutex_receve_socket);
//
//    newNode = (Node *)malloc(sizeof(Node));
//    memset(newNode,0,sizeof(Node));
//    newNode->cmdName = (char *)malloc(sizeof(char)*(strlen(buf)+1));
//    memset(newNode->cmdName,'\0',sizeof(newNode->cmdName));
//    sprintf(newNode->cmdName,"%s",buf);
//
//    //newNode->re_cmd = NULL;
//
//    Node *pTemp = g_pSyncList[i]->SocketActionList_Refresh;
//    //Node *pTemp = ActionList;
//    while(pTemp->next!=NULL)
//        pTemp=pTemp->next;
//    pTemp->next=newNode;
//    newNode->next=NULL;
//    show(g_pSyncList[i]->SocketActionList_Refresh);
//    DEBUG("end add_socket_item_for_rename\n");
//    return 0;
//}

int add_socket_item_for_rename(char *buf,int i)
{
    DEBUG("comeing add_socket_item_for_rename\n");

    pthread_mutex_lock(&mutex_receve_socket);
    g_pSyncList[i]->receve_socket = 1;
    pthread_mutex_unlock(&mutex_receve_socket);

    newNode = (Node *)malloc(sizeof(Node));
    memset(newNode,0,sizeof(Node));
    newNode->cmdName = (char *)malloc(sizeof(char)*(strlen(buf)+1));
    memset(newNode->cmdName,'\0',sizeof(newNode->cmdName));
    sprintf(newNode->cmdName,"%s",buf);

    //newNode->re_cmd = NULL;

    Node *pTemp = g_pSyncList[i]->SocketActionList_Rename;
    //Node *pTemp = ActionList;
    while(pTemp->next!=NULL)
        pTemp=pTemp->next;
    pTemp->next=newNode;
    newNode->next=NULL;
    show(g_pSyncList[i]->SocketActionList_Rename);
    DEBUG("end add_socket_item_for_rename\n");
    return 0;
}

char *search_newpath(char *href,int i)//传入文件路径
{
    char *ret = (char*)malloc(sizeof(char)*(strlen(href) + 1));
    //2014.10.29 by sherry 未初始化ret
    memset(ret,0,sizeof(char)*(strlen(href) + 1));
    sprintf(ret,"%s",href);
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
        temp = my_str_malloc((size_t)(strlen(ch)+1));
        strcpy(temp,ch);
        p = strchr(temp,'\n');
        path = my_str_malloc((size_t)(strlen(temp)- strlen(p)+1));
        if(p!=NULL)
            snprintf(path,strlen(temp)- strlen(p)+1,"%s",temp);
        p++;
        char *p1 = NULL;
        p1 = strchr(p,'\n');
        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));
        p1++;
        strcpy(newname,p1);

        oldpath = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + 3));
        newpath = my_str_malloc((size_t)(strlen(path) + strlen(newname) + 3));
        //oldpath = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + strlen("/")+strlen("/")+1));
        //newpath = my_str_malloc((size_t)(strlen(path) + strlen(newname) + strlen("/")+strlen("/")+1));
        sprintf(oldpath,"%s/%s/",path,oldname);
        sprintf(newpath,"%s/%s/",path,newname);
        free(path);
        free(temp);

        string temp1(ret),tmp;
        int m = strncmp(ret,oldpath,strlen(oldpath));
        //char *m = strstr(ret,oldpath);
        int start,end;
        if(m == 0)
        {
            //start = strlen(ret) - strlen(m);
            end = strlen(oldpath);
            tmp = temp1.replace(0,end,newpath);
            free(ret);
            ret = (char*)malloc(sizeof(char)*(tmp.length() + 1));
            sprintf(ret,"%s",tmp.c_str());
        }
        pTemp = pTemp->next;
    }

    if(strcmp(ret,href) == 0)
    {
        free(ret);
        return NULL;
    }
    else
    {
        return ret;
    }
}

void set_re_cmd_(char *oldpath,char *oldpath_1,char *oldpath_2,char *newpath,char *newpath_1,char *newpath_2,int index)
{
    int i;
    char *change_start;
    char *change_stop;
    char *old_buf;
    char new_buf[1024] = {0};
    char *p = NULL;

    Node *pTemp = g_pSyncList[index]->SocketActionList->next;
    while(pTemp != NULL)
    {
        change_start = NULL;
        change_stop = NULL;
        if((change_start = strstr(pTemp->cmdName,oldpath_1)) != NULL)
        {
            i = 0;
            old_buf = (char *)malloc(sizeof(char)*(strlen(pTemp->cmdName) + 1));
            strcpy(old_buf,pTemp->cmdName);
            p = strtok(old_buf,ENTER);

            while(p != NULL)
            {
                if(strcmp(p,oldpath))   //have no oldpath_full
                {
                    if(i == 0)
                    {
                        i++;
                        sprintf(new_buf,"%s",p);
                    }
                    else
                    {
                        sprintf(new_buf,"%s\n%s",new_buf,p);
                    }
                }
                else    //have oldpath_full
                {
                    if(i == 0)
                    {
                        i++;
                        sprintf(new_buf,"%s",newpath);
                    }
                    else
                    {
                        sprintf(new_buf,"%s\n%s",new_buf,newpath);
                    }
                }

                p=strtok(NULL,ENTER);
            }
            free(pTemp->cmdName);
            pTemp->cmdName = (char*)malloc(sizeof(char)*(strlen(new_buf) + 1));
            sprintf(pTemp->cmdName,"%s",new_buf);
        }

        else if((change_start = strstr(pTemp->cmdName,oldpath_2)) != NULL)
        {
            i = 0;
            old_buf = (char *)malloc(sizeof(char)*(strlen(pTemp->cmdName) + 1));
            strcpy(old_buf,pTemp->cmdName);
            p = strtok(old_buf,ENTER);
            while(p != NULL)
            {
                if(strncmp(p,oldpath_2,strlen(oldpath_2)))   //have no oldpath_part
                {
                    if(i == 0)
                    {
                        i++;
                        sprintf(new_buf,"%s",p);
                    }
                    else
                    {
                        sprintf(new_buf,"%s\n%s",new_buf,p);
                    }
                }
                else    //have oldpath_part
                {
                    change_stop = p + strlen(oldpath_2);
                    if(i == 0)
                    {
                        i++;
                        sprintf(new_buf,"%s%s",newpath_2,change_stop);
                    }
                    else
                    {
                        sprintf(new_buf,"%s\n%s%s",new_buf,newpath_2,change_stop);
                    }
                }
                p=strtok(NULL,ENTER);
            }
            free(pTemp->cmdName);
            pTemp->cmdName = (char*)malloc(sizeof(char)*(strlen(new_buf) + 1));
        }
        pTemp = pTemp->next;
    }

    int start,end;
    string socket_old,socket_new;
    action_item *item = g_pSyncList[index]->copy_file_list->next;
    while(item != NULL)
    {
        int res = strncmp(item->path,oldpath_2,strlen(oldpath_2));
        socket_old = item->path;
        if(0 == res)
        {
            end = strlen(oldpath_2);
            socket_new = socket_old.replace(0,end,newpath_2);
            free(item->path);
            item->path = (char *)malloc(sizeof(char)*(socket_new.length() + 1));
            sprintf(item->path,"%s",socket_new.c_str());
        }
        item = item->next;
    }
}

void set_re_cmd(char *oldpath,char *oldpath_1,char *oldpath_2,char *newpath,char *newpath_1,char *newpath_2,int i)
{
    DEBUG("************************set_re_cmd***********************\n");
    char *p = NULL;
    char *q = NULL;
    int start,end;
    string socket_old,socket_new;
    Node *pTemp = g_pSyncList[i]->SocketActionList->next;
    while(pTemp != NULL)
    {
        p = strstr(pTemp->cmdName,oldpath_1);
        q = strstr(pTemp->cmdName,oldpath_2);
        socket_old = pTemp->cmdName;
        if(p != NULL)
        {
            start = strlen(pTemp->cmdName) - strlen(p);
            end = strlen(oldpath_1);
            socket_new = socket_old.replace(start,end,newpath_1);
            free(pTemp->cmdName);
            pTemp->cmdName = (char *)malloc(sizeof(char)*(socket_new.length() + 1));
            sprintf(pTemp->cmdName,"%s",socket_new.c_str());
        }
        if(q != NULL)
        {
            start = strlen(pTemp->cmdName) - strlen(q);
            end = strlen(oldpath_2);
            socket_new = socket_old.replace(start,end,newpath_2);
            free(pTemp->cmdName);
            pTemp->cmdName = (char *)malloc(sizeof(char)*(socket_new.length() + 1));
            sprintf(pTemp->cmdName,"%s",socket_new.c_str());
        }

        pTemp=pTemp->next;
    }

    action_item *item = g_pSyncList[i]->copy_file_list->next;
    while(item != NULL)
    {
        int res = strncmp(item->path,oldpath_2,strlen(oldpath_2));
        socket_old = item->path;
        if(0 == res)
        {
            end = strlen(oldpath_2);
            socket_new = socket_old.replace(0,end,newpath_2);
            free(item->path);
            item->path = (char *)malloc(sizeof(char)*(socket_new.length() + 1));
            sprintf(item->path,"%s",socket_new.c_str());
        }
        item = item->next;
    }

    //show(g_pSyncList[i]->SocketActionList);
}

int handle_rename_socket(char *buf,int i)
{
    DEBUG("handle_rename_socket\n");
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
    temp = my_str_malloc((size_t)(strlen(ch)+1));
    strcpy(temp,ch);
    //cout << temp << endl;
    p = strchr(temp,'\n');
    path = my_str_malloc((size_t)(strlen(temp)- strlen(p) + 1));
    //cout << path << endl;
    if(p!=NULL)
        snprintf(path,strlen(temp)- strlen(p)+1,"%s",temp);
    p++;
    char *p1 = NULL;
    p1 = strchr(p,'\n');
    if(p1 != NULL)
        strncpy(oldname,p,strlen(p)- strlen(p1));
    p1++;
    strcpy(newname,p1);

    oldpath = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
    oldpath_1 = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + 3));
    oldpath_2 = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + 3));
    newpath = my_str_malloc((size_t)(strlen(path) + strlen(newname) + 2));
    newpath_1 = my_str_malloc((size_t)(strlen(path) + strlen(newname) + 3));
    newpath_2 = my_str_malloc((size_t)(strlen(path) + strlen(newname) + 3));
    //oldpath = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + strlen("/")+1));
   // oldpath_1 = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + strlen("/")+strlen("\n")+1));
    //oldpath_2 = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + strlen("/")+strlen("/")+1));
    //newpath = my_str_malloc((size_t)(strlen(path) + strlen(newname) + strlen("/")+1));
    //newpath_1 = my_str_malloc((size_t)(strlen(path) + strlen(newname) + strlen("/")+strlen("\n")+1));
    //newpath_2 = my_str_malloc((size_t)(strlen(path) + strlen(newname) + strlen("/")+strlen("/")+1));
    sprintf(oldpath,"%s/%s",path,oldname);
    sprintf(oldpath_1,"%s/%s\n",path,oldname);
    sprintf(oldpath_2,"%s/%s/",path,oldname);
    sprintf(newpath,"%s/%s",path,newname);
    sprintf(newpath_1,"%s/%s\n",path,newname);
    sprintf(newpath_2,"%s/%s/",path,newname);
    free(path);
    free(temp);

    set_re_cmd(oldpath,oldpath_1,oldpath_2,newpath,newpath_1,newpath_2,i);

    free(oldpath);
    free(oldpath_1);
    free(oldpath_2);
    free(newpath);
    free(newpath_1);
    free(newpath_2);
    return 0;
}

int add_socket_item(char *buf,int i)
{
    DEBUG("comeing add_socket_item，i=%d\n",i);

    pthread_mutex_lock(&mutex_receve_socket);
    g_pSyncList[i]->receve_socket = 1;
    pthread_mutex_unlock(&mutex_receve_socket);
    newNode = (Node *)malloc(sizeof(Node));
    memset(newNode,0,sizeof(Node));
    newNode->cmdName = (char *)malloc(sizeof(char)*(strlen(buf)+1));
    //2014.10.29 by sherry sizeof(指针)=4
    //memset(newNode->cmdName,'\0',sizeof(newNode->cmdName));
    memset(newNode->cmdName,'\0',sizeof(char)*(strlen(buf)+1));
    sprintf(newNode->cmdName,"%s",buf);
    //newNode->re_cmd = NULL;
    Node *pTemp = g_pSyncList[i]->SocketActionList;
    while(pTemp->next != NULL)
        pTemp = pTemp->next;
    pTemp->next = newNode;
    newNode->next = NULL;
    show(g_pSyncList[i]->SocketActionList);
    DEBUG("end add_socket_item\n");
    return 0;
}

//char *search_xml_node(xmlNodePtr p)
//{
//    int count = 0;
//    xmlChar *szAttr;
//    char fold0[1024];
//    char *temp;
//    char *res;
//    res = (char*)malloc(sizeof(char));
//    memset(res,'\0',sizeof(res));
//    while(p != NULL)
//    {
//        szAttr = xmlGetProp(p,BAD_CAST p->properties->name);
//        sprintf(fold0,"%s",szAttr);
//        xmlNodePtr q = p->children;
//        int _count = 0;
//        while(q != NULL)
//        {
//            szAttr = xmlGetProp(q,BAD_CAST q->properties->name);
//            q = q->next;
//            _count++;
//        }
//        if(!xmlStrcmp(p->name,(const xmlChar*)("fold0")))
//        {
//            temp = (char *)malloc(sizeof(char)*(strlen(fold0) + 12));
//            memset(temp,'\0',sizeof(char)*(strlen(fold0) + 12));
//            sprintf(temp,",\"%s#%d#%d\"",fold0,count,_count);
//            int len = strlen(res) + strlen(temp) + 1;
//            res = (char*)realloc(res,len);
//            strncat(res,temp,strlen(temp));
//            free(temp);
//        }
//        p = p->next;
//        count++;
//    }
//    if(count)
//    {
//        res = (char*)realloc(res,strlen(res) + 3);
//        sprintf(res,"[%s]",res + 1);
//        return res;
//    }
//    else
//    {
//        free(res);
//        return NULL;
//    }
//}

void *Save_Socket(void *argc)
{
    //写死socket
    //    char buf1[256] ={0};
    //    char buf2[256] = {0};
    //    sprintf(buf1,"%s","createfolder\n/tmp/mnt/SNK/sync\n未命名文件夹");
    //    sprintf(buf2,"%s","rename0\n/tmp/mnt/SNK/sync\n未命名文件夹\na");

    //    int i=0;
    //    pthread_mutex_lock(&mutex_socket);
    //    handle_rename_socket(buf2,i);
    //    add_socket_item_for_rename(buf2,i);
    //    add_socket_item(buf1,i);
    //    pthread_mutex_unlock(&mutex_socket);

    DEBUG("Save_Socket:%u\n",pthread_self());
    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd*/
    int numbytes;
    char buf[MAXDATASIZE];
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

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct
                                                         sockaddr))== -1) {
        perror("bind");
        exit(1);
    }
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    sin_size = sizeof(struct sockaddr_in);

    FD_SET(sockfd,&master);
    fdmax = sockfd;

    while(!exit_loop)
    { /* main accept() loop */
        //DEBUG("listening!\n");
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        read_fds = master;

        ret = select(fdmax+1,&read_fds,NULL,NULL,&timeout);

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
            if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
                                 &sin_size)) == -1) {
                perror("accept");
                continue;
            }
            memset(buf, 0, sizeof(buf));

            if ((numbytes=recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv");
                exit(1);
            }

            if(buf[strlen(buf)] == '\n')
            {
                buf[strlen(buf)] = '\0';
            }


            int i=0;
#ifdef _PC
//            if(strncmp(buf,"refresh",7))
//            {
//                char *r_path = get_socket_base_path(buf);
//                DEBUG("r_path:%s\n",r_path);
//                for(i=0;i<ftp_config.dir_num;i++)
//                {
//                    DEBUG("r_path:%s,base_path=%s\n",r_path,ftp_config.multrule[i]->base_path);
//                    if(!strcmp(r_path,ftp_config.multrule[i]->base_path))
//                    {
//                        free(r_path);
//                        break;
//                    }
//                }

//            }
#else
            if(strncmp(buf,"refresh",7))
            {
                char *r_path = get_socket_base_path(buf);
                DEBUG("r_path:%s\n",r_path);
                for(i=0;i<ftp_config.dir_num;i++)
                {
                    if(!strcmp(r_path,ftp_config.multrule[i]->base_path))
                    {
                        free(r_path);
                        break;
                    }
                }

            }
#endif
            pthread_mutex_lock(&mutex_socket);
            if(!strncmp(buf,"rename0",7))
            {
                DEBUG("it is renamed folder!\n");
                handle_rename_socket(buf,i);
                add_socket_item_for_rename(buf,i);
            }
            else
            {
                add_socket_item(buf,i);
            }
            close(new_fd);
            pthread_mutex_unlock(&mutex_socket);
        }
    }
    close(sockfd);

    DEBUG("stop FtpClient local sync\n");

    stop_down = 1;
    return NULL;
}

void parseCloudInfo_forsize(char *parentherf)
{
    FILE *fp;
    fp = fopen(LIST_SIZE_DIR,"r");
    char tmp[512] = {0};
    char buf[512] = {0};
    const char *split = " ";
    const char *split_2 = "\n";
    char *p;
    while(fgets(buf,sizeof(buf),fp)!=NULL)
    {
        int fail = 0;
        FolderTmp_size=(CloudFile *)malloc(sizeof(struct node));
        memset(FolderTmp_size,0,sizeof(FolderTmp_size));
        p = strtok(buf,split);
        int i=0;
        while(p != NULL && i <= 8)
        {
            switch(i)
            {
            case 0:
                strcpy(tmp,p);
                //strcpy(FolderTmp_size->auth,p);
                FolderTmp_size->isfile=is_file(tmp);
                break;
            case 4:
                FolderTmp_size->size=atoll(p);
                break;
            case 5:
                //strcpy(FolderTmp_size->month,p);
                break;
            case 6:
                //strcpy(FolderTmp_size->day,p);
                break;
            case 7:
                //strcpy(FolderTmp_size->lastmodifytime,p);
                break;
            case 8:
                if(!strcmp(p,".") || !strcmp(p,".."))
                    fail = 1;
                strcpy(FolderTmp_size->filename,p);
                break;
            default:
                break;
            }
            i++;
            if(i<=7)
                p=strtok(NULL,split);
            else
                p=strtok(NULL,split_2);
        }

        FolderTmp_size->href = (char *)malloc(sizeof(char)*(strlen(parentherf)+1+1+strlen(FolderTmp_size->filename)));
        //FolderTmp_size->href = (char *)malloc(sizeof(char)*(strlen(parentherf)+strlen("/")+1+strlen(FolderTmp_size->filename)));
        memset(FolderTmp_size->href,'\0',sizeof(FolderTmp_size->href));
        if(strcmp(parentherf," ")==0)
            sprintf(FolderTmp_size->href,"%s",FolderTmp_size->filename);
        else
            sprintf(FolderTmp_size->href,"%s/%s",parentherf,FolderTmp_size->filename);
        if(!fail)
        {
            if(FolderTmp_size->isfile==0)                   //文件夹链表
            {
                //FolderTail_size->next = FolderTmp_size;
                //FolderTail_size = FolderTmp_size;
                //FolderTail_size->next = NULL;
            }
            else if(FolderTmp_size->isfile==1)               //文件链表
            {
                FileTail_size->next = FolderTmp_size;
                FileTail_size = FolderTmp_size;
                FileTail_size->next = NULL;
            }
        }
        else
        {
            free_CloudFile_item(FolderTmp_size);
        }
    }
    fclose(fp);
}

long long int get_file_size(char *serverpath,int index)
{
    char *url;
    long long int ret = 0;
    FileList_size = (CloudFile *)malloc(sizeof(CloudFile));
    memset(FileList_size,0,sizeof(CloudFile));

    FileList_size->href = NULL;
    FileTail_size = FileList_size;
    FileTail_size->next = NULL;
    url = get_prepath(serverpath,strlen(serverpath));

    getCloudInfo_forsize(url,parseCloudInfo_forsize,index);
    CloudFile *de_filecurrent;
    de_filecurrent = FileList_size->next;

    while(de_filecurrent != NULL)
    {
        if(de_filecurrent->href != NULL)
        {
            if(!(strcmp(de_filecurrent->href,serverpath)))
            {
                ret  = de_filecurrent->size;
                free_CloudFile_item(FileList_size);
                free(url);
                return ret;
            }
        }
        de_filecurrent = de_filecurrent->next;
    }
    free(url);
    free_CloudFile_item(FileList_size);
}



mod_time *ftp_MDTM(char *href,int index)
{
    mod_time *mtime;

    char *command = (char *)malloc(sizeof(char)*(strlen(href) + 6));
    //char *command = (char *)malloc(sizeof(char)*(strlen(href) + strlen("MDTM ")+1));
    memset(command,'\0',sizeof(command));
    sprintf(command,"MDTM %s",href+1);
    DEBUG("%s\n",command);
    char *temp = utf8_to(command,index);
    free(command);
    CURL *curl_t;
    CURLcode res;
    curl_t = curl_easy_init();
    FILE *fp = fopen(TIME_DIR,"w+");
    //fp=fopen("/tmp/ftpclient/time.txt","w+");
    if(curl_t){
        curl_easy_setopt(curl_t, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl_t, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        //curl_easy_setopt(curl_t, CURLOPT_FTP_USE_EPSV, 0);
        curl_easy_setopt(curl_t, CURLOPT_WRITEHEADER,fp);
        curl_easy_setopt(curl_t, CURLOPT_CUSTOMREQUEST,temp);
        curl_easy_setopt(curl_t, CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl_t, CURLOPT_LOW_SPEED_TIME,30);
        curl_easy_setopt(curl_t, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl_t);
        DEBUG("res = %d\n",res);
        if(res != CURLE_OK && res != CURLE_FTP_COULDNT_RETR_FILE)
        {
            curl_easy_cleanup(curl_t);
            fclose(fp);
            free(temp);
            //DEBUG("get %s mtime failed res =%d\n",href,res);
            if(res == CURLE_COULDNT_CONNECT)
            {
                write_log(S_ERROR,"Could not connect.","",index);
            }
            else if(res == CURLE_OPERATION_TIMEDOUT)
            {
                write_log(S_ERROR,"Connect timeout.","",index);
            }
            else if(res == CURLE_REMOTE_ACCESS_DENIED)
            {
                write_log(S_ERROR,"Connect refused.","",index);
            }
            mtime = (mod_time *)malloc(sizeof(mod_time));
            memset(mtime,0,sizeof(mtime));
            mtime->modtime = (time_t)-1;
            return mtime;
        }
        else
        {
            free(temp);
            curl_easy_cleanup(curl_t);
            mtime = get_mtime(fp);
            return mtime;
        }
    }
}

int parseCloudInfo_tree(char *parentherf,int index)
{
    FILE *fp;
    fp = fopen(LIST_DIR,"r");
    char tmp[512] = {0};
    char buf[512] = {0};
    const char *split = " ";
    const char *split_1 = "         \n";
    const char *split_2 = "\n";
    char *str,*temp;
    char *p;
    while(fgets(buf,sizeof(buf),fp)!=NULL)
    {
        int fail = 0;
        DEBUG("BUF:%s\n",buf);
        FolderTmp=(CloudFile *)malloc(sizeof(struct node));
        memset(FolderTmp,0,sizeof(FolderTmp));
        p=strtok(buf,split);
        int i=0;
        //DEBUG("p:%s\n",p);
        //string tmpStr;
        if(strlen(p) == 8)
        {
            int flag = 0;
            while(i <= 3)//p != NULL &&
            {
                switch(i)
                {
                case 1:
                    //strncpy(FolderTmp->lastmodifytime,p,5);
                    break;
                case 2:
                    strcpy(tmp,p);
                    flag = is_folder(tmp);
                    if(flag == 0)
                    {
                        FolderTmp->isfile = 0;
                    }
                    else
                    {
                        FolderTmp->isfile = 1;
                        FolderTmp->size=atoll(p);
                    }
                    break;
                case 3:
                    if(p == NULL)
                    {
                        free_CloudFile_item(FolderTmp);
                        //fclose(fp);
                        //return UNSUPPORT_ENCODING;
                        break;
                    }
                    str = (char*)malloc(sizeof(char)*(strlen(p) + 1));
                    strcpy(str,p);
                    temp = to_utf8(str,index);
                    strcpy(FolderTmp->filename,temp);
                    break;
                default:
                    break;
                }
                i++;
                if(i<3)
                {
                    p=strtok(NULL,split);
                }
                else
                {
                    if(FolderTmp->isfile == 0)
                        p=strtok(NULL,split_1);
                    else
                        p=strtok(NULL,split_2);
                }
            }
        }
        else
        {
            while(i <= 8)//p != NULL &&
            {
                //DEBUG("p:%s %d\n",p,i);
                switch(i)
                {
                case 0:
                    strcpy(tmp,p);
                    //strcpy(FolderTmp->auth,p);
                    FolderTmp->isfile=is_file(tmp);
                    break;
                case 4:
                    FolderTmp->size=atoll(p);
                    break;
                case 5:
                    //strcpy(FolderTmp->month,p);
                    break;
                case 6:
                    //strcpy(FolderTmp->day,p);
                    break;
                case 7:
                    //strcpy(FolderTmp->lastmodifytime,p);
                    break;
                case 8:
                    if(p == NULL || !strcmp(p,".") || !strcmp(p,".."))
                    {
                        DEBUG("free item\n");
                        fail = 1;
                    }
                    str = (char*)malloc(sizeof(char)*(strlen(p) + 1));
                    strcpy(str,p);
                    temp = to_utf8(str,index);
                    free(str);
                    strcpy(FolderTmp->filename,temp);
                    break;
                default:
                    break;
                }
                i++;
                if(i<=7)
                {
                    p=strtok(NULL,split);
                    //DEBUG("p:%s\n",p);
                }
                else
                {
                    //DEBUG("p:%s\n",p);
                    p=strtok(NULL,split_2);
                }
            }
        }
        free(temp);
        FolderTmp->href = (char *)malloc(sizeof(char)*(strlen(parentherf)+1+1+strlen(FolderTmp->filename)));//"1">strlen("/")

        memset(FolderTmp->href,'\0',sizeof(FolderTmp->href));
        if(strcmp(parentherf," ")==0)
            sprintf(FolderTmp->href,"%s",FolderTmp->filename);
        else
            sprintf(FolderTmp->href,"%s/%s",parentherf,FolderTmp->filename);
        if(!fail)
        {
            if(FolderTmp->isfile==0)                   //文件夹链表
            {
                TreeFolderTail->next=FolderTmp;
                TreeFolderTail=FolderTmp;
                TreeFolderTail->next=NULL;
            }
            else if(FolderTmp->isfile==1)               //文件链表
            {
                mod_time *mtime;
                mtime = ftp_MDTM(FolderTmp->href,index);
                if(mtime->modtime == -1)
                {
                    fclose(fp);
                    return -1;
                }
                //DEBUG("mtime->modtime = %lu,mtime->mtime = %s\n",mtime->modtime,mtime->mtime);
                FolderTmp->modtime = mtime->modtime;
                //strcpy(FolderTmp->mtime,mtime->mtime);
                //DEBUG("FolderTmp->modtime = %lu,FolderTmp->mtime = %s\n",FolderTmp->modtime,FolderTmp->mtime);
                free(mtime);
                TreeFileTail->next=FolderTmp;
                TreeFileTail=FolderTmp;
                TreeFileTail->next=NULL;
            }
        }
        else
        {
            free_CloudFile_item(FolderTmp);
            fclose(fp);
            return 912;
        }
    }
    fclose(fp);
    return 0;
}

int getCloudInfo(char *URL,int (* cmd_data)(char *,int),int index)
{
    int status;
    char *command = (char *)malloc(sizeof(char)*(strlen(URL) + 7));
    memset(command,'\0',sizeof(command));
    sprintf(command,"LIST %s",URL);
    DEBUG("command = %s\n",command);
    char *temp = utf8_to(command,index);
    free(command);
    DEBUG("temp = %s\n",temp);
    CURL *curl;
    CURLcode res;
    FILE *fp = fopen(LIST_DIR,"wb");
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,temp);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl);
        DEBUG("getCloudInfo() - res = %d\n",res);
        curl_easy_cleanup(curl);
        fclose(fp);
        free(temp);
        if(res != CURLE_OK)
        {
                if(res == CURLE_COULDNT_CONNECT)
                {

                    write_log(S_ERROR,"Could not connect.","",index);

                }
                else if(res == CURLE_OPERATION_TIMEDOUT)
                { 
                    write_log(S_ERROR,"Connect timeout.","",index);
                }
                else if(res == CURLE_REMOTE_ACCESS_DENIED)
                {
                    write_log(S_ERROR,"Connect refused.","",index);
                }
                else if(res == CURLE_FTP_COULDNT_RETR_FILE)
                {
                    write_log(S_ERROR, "Permission denied.", "", index);
                }

                return -1;
        }
        else
        {
            status = cmd_data(URL,index);
            if(status == 912)
            {
                write_log(S_ERROR,"Unsuported encoding:BIG5 Contains Simplified.","",index);
                return UNSUPPORT_ENCODING;
            }
            return status;
        }
    }
    else
    {
        fclose(fp);
        free(temp);
        return 0;
    }
}

Browse *browseFolder(char *URL,int index)
{
    DEBUG("browseFolder URL = %s\n",URL);
    int status;
    int i=0;

    Browse *browse = (Browse *)malloc(sizeof(Browse));
    if( NULL == browse )
    {
        DEBUG("create memery error\n");
        exit(-1);
    }
    memset(browse,0,sizeof(Browse));

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

    status = getCloudInfo(URL,parseCloudInfo_tree,index);
    DEBUG("end getCloudInfo,status = %d\n",status);
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

    CloudFile *de_foldercurrent,*de_filecurrent;
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
    return browse;
}

int browse_to_tree(char *parenthref,Server_TreeNode *node,int index)
{
    if(exit_loop)
    {
        return -1;
    }
    if(index == -1)
    {
        return -1;
    }
    Browse *br = NULL;
    int fail_flag = 0;

    Server_TreeNode *tempnode = NULL, *p1 = NULL,*p2 = NULL;
    tempnode = create_server_treeroot();
    tempnode->level = node->level + 1;

    tempnode->parenthref = my_str_malloc((size_t)(strlen(parenthref)+1));
    memset(tempnode->parenthref,'\0',sizeof(tempnode->parenthref));

    DEBUG("parenthref:%s\n",parenthref);
    br = browseFolder(parenthref,index);
    sprintf(tempnode->parenthref,"%s",parenthref);
    if(NULL == br)
    {
        free_server_tree(tempnode);
        DEBUG("browse folder failed\n");
        return -1;
    }

    tempnode->browse = br;

    if(node->Child == NULL)
    {
        node->Child = tempnode;
    }
    else
    {
        DEBUG("have child\n");
        p2 = node->Child;
        p1 = p2->NextBrother;

        while(p1 != NULL)
        {
            DEBUG("p1 nextbrother have\n");
            p2 = p1;
            p1 = p1->NextBrother;
        }

        p2->NextBrother = tempnode;
        tempnode->NextBrother = NULL;
    }
    DEBUG("browse folder num is %d\n",br->foldernumber);
    CloudFile *de_foldercurrent;
    de_foldercurrent = br->folderlist->next;
    while(de_foldercurrent != NULL)
    {
        if(browse_to_tree(de_foldercurrent->href,tempnode,index) == -1)
        {
            fail_flag = 1;
        }
        de_foldercurrent = de_foldercurrent->next;
    }
    return (fail_flag == 1) ? -1 : 0 ;

}

/*获取某一文件夹下的所有文件和文件夹信息*/
Local *Find_Floor_Dir(const char *path)
{
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
    memset(local,0,sizeof(Local));
    localfloorfile = (LocalFile *)malloc(sizeof(LocalFile));
    localfloorfolder = (LocalFolder *)malloc(sizeof(LocalFolder));
    memset(localfloorfolder,0,sizeof(localfloorfolder));
    memset(localfloorfile,0,sizeof(localfloorfile));

    localfloorfile->path = NULL;
    localfloorfolder->path = NULL;
    localfloorfiletail = localfloorfile;
    localfloorfoldertail = localfloorfolder;
    localfloorfiletail->next = NULL;
    localfloorfoldertail->next = NULL;

    pDir = opendir(path);
    if(NULL == pDir)
    {
        return NULL;
    }

    while(NULL != (ent = readdir(pDir)))
    {
        if(ent->d_name[0] == '.')
            continue;
        if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
            continue;
        if(test_if_download_temp_file(ent->d_name))     //download temp files
            continue;

        char *fullname;
        size_t len;
        len = strlen(path)+strlen(ent->d_name)+2;
        fullname = my_str_malloc(len);
        sprintf(fullname,"%s/%s",path,ent->d_name);

        //DEBUG("folder fullname = %s\n",fullname);
        //DEBUG("ent->d_ino = %d\n",ent->d_ino);

        if(test_if_dir(fullname) == 1)
        {
            localfloorfoldertmp = (LocalFolder *)malloc(sizeof(LocalFolder));
            memset(localfloorfoldertmp,0,sizeof(localfloorfoldertmp));
            localfloorfoldertmp->path = my_str_malloc((size_t)(strlen(fullname)+1));

            sprintf(localfloorfoldertmp->name,"%s",ent->d_name);
            sprintf(localfloorfoldertmp->path,"%s",fullname);

            ++foldernum;

            localfloorfoldertail->next = localfloorfoldertmp;
            localfloorfoldertail = localfloorfoldertmp;
            localfloorfoldertail->next = NULL;
        }
        else
        {
            struct stat buf;

            if(stat(fullname,&buf) == -1)
            {
                perror("stat:");
                continue;
            }

            localfloorfiletmp = (LocalFile *)malloc(sizeof(LocalFile));
            memset(localfloorfiletmp,0,sizeof(localfloorfiletmp));
            localfloorfiletmp->path = my_str_malloc((size_t)(strlen(fullname)+1));

            //unsigned long asec = buf.st_atime;
            unsigned long msec = buf.st_mtime;
            //unsigned long csec = buf.st_ctime;

            //sprintf(localfloorfiletmp->creationtime,"%lu",csec);
            //sprintf(localfloorfiletmp->lastaccesstime,"%lu",asec);
            //sprintf(localfloorfiletmp->lastwritetime,"%lu",msec);
            localfloorfiletmp->modtime = msec;

            sprintf(localfloorfiletmp->name,"%s",ent->d_name);
            sprintf(localfloorfiletmp->path,"%s",fullname);

            //localfloorfiletmp->size = buf.st_size;
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

/*0,local file newer
 *1,server file newer
 *2,local time == server time
 *-1,get server modtime failed
**/
int newer_file(char *localpath,int index,int flag)//比较server和loacal端的文件谁最新
{
    char *serverpath = localpath_to_serverpath(localpath,index);

    mod_time *modtime1;
    time_t modtime2,oldtime;
    modtime1 = Getmodtime(serverpath,index);
    if(modtime1->modtime == -1)
    {
        DEBUG("newer_file Getmodtime failed!\n");
        free(modtime1);
        return -1;
    }

    struct stat buf;
    if( stat(localpath,&buf) == -1)
    {
        perror("stat:");
        free(modtime1);
        return -1;
    }
    modtime2 = buf.st_mtime;
    DEBUG("new  local time:%ld\n",modtime2);
    DEBUG("new server time:%ld\n",modtime1->modtime);
    if(flag)
    {
        CloudFile *cloud_file = get_CloudFile_node(g_pSyncList[index]->OrigServerRootNode,serverpath,0x2);
        if(cloud_file != NULL)
        {
            DEBUG("old server time:%ld\n",cloud_file->modtime);
            oldtime = cloud_file->modtime;
        }
        else
        {
            oldtime = (time_t)-1;
        }
        free(serverpath);
        if(ftp_config.multrule[index]->rules == 1)//download only
        {
            if(modtime2 > modtime1->modtime)
            {
                free(modtime1);
                return 0;
            }
            else if(modtime2 == modtime1->modtime)
            {
                free(modtime1);
                return 2;
            }
            else
            {
                if(modtime2 == oldtime)
                {
                    free(modtime1);
                    return 1;
                }
                else
                {
                    free(modtime1);
                    return 0;
                }
            }
        }
        else
        {
            if(modtime2 > modtime1->modtime)
            {
                free(modtime1);
                return 0;
            }
            else if(modtime2 == modtime1->modtime)
            {
                free(modtime1);
                return 2;
            }
            else
            {
                free(modtime1);
                return 1;
            }
        }
    }
    free(serverpath);
    if(!flag)//for init
    {
        if(modtime1->modtime - modtime2 == 1 || modtime2 - modtime1->modtime == 1)
        {
            if(ChangeFile_modtime(localpath,modtime1->modtime,index))
            {
                DEBUG("ChangeFile_modtime failed!\n");
            }
            free(modtime1);
            return 2;
        }
        else if(modtime2 > modtime1->modtime)
        {
            free(modtime1);
            return 0;
        }
        else if(modtime2 == modtime1->modtime)
        {
            free(modtime1);
            return 2;
        }
        else
        {
            free(modtime1);
            return 1;
        }
    }
}

int the_same_name_compare(LocalFile *localfiletmp,CloudFile *filetmp,int index,int flag)
{
    int status = 0;
    int newer_file_ret = 0;

    if(!flag)
        DEBUG("###################the same name compare...for init...####################\n");
    else
        DEBUG("###################the same name compare...####################\n");
    DEBUG("local:%s\t\tserver:%s\n",localfiletmp->name,filetmp->filename);
    DEBUG("local:%ld\t\tserver:%ld\n",localfiletmp->modtime,filetmp->modtime);

    if(ftp_config.multrule[index]->rules == 1)
    {
        newer_file_ret = newer_file(localfiletmp->path,index,flag);
        DEBUG("newer_file_ret:%d\n",newer_file_ret);
        if(newer_file_ret != 2 && newer_file_ret != -1)
        {
            action_item *item1;
            item1 = get_action_item("download_only",localfiletmp->path,
                                    g_pSyncList[index]->download_only_socket_head,index);
            if(item1 != NULL)
            {
                char *mynewname = change_local_same_name(localfiletmp->path);
                rename(localfiletmp->path,mynewname);
                char *err_msg = write_error_message("%s is download from server,%s is local file and rename from %s",localfiletmp->path,mynewname,localfiletmp->path);
                write_conflict_log(localfiletmp->name,3,err_msg); //conflict
                free(mynewname);
                free(err_msg);
                add_action_item("rename",localfiletmp->path,g_pSyncList[index]->server_action_list);
            }
            char *localpath = serverpath_to_localpath(filetmp->href,index);

            action_item *item;
            item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);

            if(is_local_space_enough(filetmp,index))
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }

                add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);

                status = download_(filetmp->href,index);
                if(status == 0)
                {
                    DEBUG("do_cmd ok\n");
                    mod_time *onefiletime = Getmodtime(filetmp->href,index);
                    if(ChangeFile_modtime(localpath,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                    free(onefiletime);
                    if(item != NULL)
                    {
                        del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
            }
            else
            {
                write_log(S_ERROR,"local space is not enough!","",index);
                if(item == NULL)
                {
                    add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                }
            }
            free(localpath);
        }
    }
    else if(ftp_config.multrule[index]->rules == 2)
    {
        newer_file_ret = newer_file(localfiletmp->path,index,flag);
        DEBUG("newer_file_ret:%d\n",newer_file_ret);
        if(newer_file_ret != 2 && newer_file_ret != -1)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            char *temp = change_server_same_name(localfiletmp->path,index);
            my_rename_(localfiletmp->path,temp,index);
            //free(temp);
            status = upload(localfiletmp->path,index);
            if(status == 0)
            {
                char *err_msg = write_error_message("server file %s is renamed to %s",localfiletmp->path,temp);
                write_conflict_log(localfiletmp->path,3,err_msg); //conflict
                free(err_msg);
                char *serverpath = localpath_to_serverpath(localfiletmp->path,index);
                mod_time *onefiletime = Getmodtime(serverpath,index);
                if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                {
                    DEBUG("ChangeFile_modtime failed!\n");
                }
                free(onefiletime);
                free(serverpath);
            }
            else
            {
                return status;
            }
        }
    }
    else
    {
        newer_file_ret = newer_file(localfiletmp->path,index,flag);
        DEBUG("newer_file_ret:%d\n",newer_file_ret);
        if(!flag)
        {
            if(newer_file_ret != 2 && newer_file_ret != -1)
            {
                char *temp = change_server_same_name(localfiletmp->path,index);
                my_rename_(localfiletmp->path,temp,index);
                //free(temp);
                status = upload(localfiletmp->path,index);
                if(status == 0)
                {
                    char *err_msg = write_error_message("server file %s is renamed to %s",localfiletmp->path,temp);
                    write_conflict_log(localfiletmp->path,3,err_msg); //conflict
                    free(err_msg);
                    char *serverpath = localpath_to_serverpath(localfiletmp->path,index);
                    mod_time *onefiletime = Getmodtime(serverpath,index);
                    if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                    free(onefiletime);
                    free(serverpath);
                }
                else
                {
                    return status;
                }
            }
        }
        else
        {
            if(newer_file_ret == 1)
            {
                char *localpath = serverpath_to_localpath(filetmp->href,index);
                add_action_item("remove",localfiletmp->path,g_pSyncList[index]->server_action_list);
                unlink(localfiletmp->path);
                action_item *item;
                item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);

                if(is_local_space_enough(filetmp,index))
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                    status = download_(filetmp->href,index);
                    if (status == 0)
                    {
                        mod_time *onefiletime = Getmodtime(filetmp->href,index);
                        if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                        if(item != NULL)
                        {
                            del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                        }
                    }
                    else
                    {
                        free(localpath);
                        return status;
                    }
                }
                else
                {
                    write_log(S_ERROR,"local space is not enough!","",index);
                    if(item == NULL)
                    {
                        add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }

                }
                free(localpath);
            }
            else if(newer_file_ret == 0 && ftp_config.multrule[index]->rules != 1)
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                if(!flag)
                {
                    char *temp = change_server_same_name(localfiletmp->path,index);
                    my_rename_(localfiletmp->path,temp,index);
                    char *err_msg = write_error_message("server file %s is renamed to %s",localfiletmp->path,temp);
                    write_conflict_log(localfiletmp->path,3,err_msg); //conflict
                    free(err_msg);
                    //free(temp);
                }
                add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);  //??
                status = upload(localfiletmp->path,index);
                if(status == 0)
                {
                    char *serverpath = localpath_to_serverpath(localfiletmp->path,index);
                    mod_time *onefiletime = Getmodtime(serverpath,index);
                    if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                    free(onefiletime);
                    free(serverpath);
                }
                else
                {
                    return status;
                }
            }
        }
    }
    return status;
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
    //int wait_ret = 0;

    if(perform_br->foldernumber > 0)
        foldertmp = perform_br->folderlist->next;
    if(perform_br->filenumber > 0)
        filetmp = perform_br->filelist->next;

    localfoldertmp = perform_lo->folderlist->next;
    localfiletmp = perform_lo->filelist->next;

    /****************handle files****************/
    //DEBUG("##########handle files\n");
    if(perform_br->filenumber == 0 && perform_lo->filenumber != 0)
    {
        DEBUG("serverfileNo:%d\t\tlocalfileNo:%d\n",perform_br->filenumber,perform_lo->filenumber);

        while(localfiletmp != NULL && exit_loop == 0)
        {
            if(ftp_config.multrule[index]->rules == 1)
            {
                action_item *item;
                item = get_action_item("download_only",localfiletmp->path,
                                       g_pSyncList[index]->download_only_socket_head,index);
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
            add_action_item("remove",localfiletmp->path,g_pSyncList[index]->server_action_list);

            action_item *pp;
            pp = get_action_item("upload",localfiletmp->path,
                                 g_pSyncList[index]->up_space_not_enough_list,index);
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
            char *localpath = serverpath_to_localpath(filetmp->href,index);

            action_item *item;
            item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);

            int cp = 0;
            do{
                if(exit_loop == 1)
                    return -1;
                cp = is_ftp_file_copying(filetmp->href,index);
            }while(cp == 1);

            usleep(100*100);
            if(is_local_space_enough(filetmp,index))
            {
                add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);

                int status = download_(filetmp->href,index);
//                if(status == 0)
//                {
//                    DEBUG("do_cmd ok\n");
//                    mod_time *onefiletime = Getmodtime(filetmp->href,index);
//                    if(ChangeFile_modtime(localpath,onefiletime->modtime,index))
//                    {
//                        DEBUG("ChangeFile_modtime failed!\n");
//                    }
//                    free(onefiletime);
//                }
//                else
//                {
                    //DEBUG("do_cmd failed,try again~~\n");
                    //status = download_(filetmp->href,index);
                if(status == 0)
                {
                    DEBUG("do_cmd ok\n");
                    mod_time *onefiletime = Getmodtime(filetmp->href,index);
                    if(ChangeFile_modtime(localpath,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                    free(onefiletime);
                    if(item != NULL)
                    {
                        del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
                //}
            }
            else
            {
                write_log(S_ERROR,"local space is not enough!","",index);
                if(item == NULL)
                {
                    add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                }
            }
            free(localpath);
            filetmp = filetmp->next;
        }
    }
    else if(perform_br->filenumber != 0 && perform_lo->filenumber != 0)
    {
        DEBUG("serverfileNo:%d\t\tlocalfileNo:%d\n",perform_br->filenumber,perform_lo->filenumber);
        DEBUG("Ergodic ftp file while\n");
        while(localfiletmp != NULL && exit_loop == 0)
        {
            int cmp = 1;
            char *localpathtmp = strstr(localfiletmp->path,ftp_config.multrule[index]->base_path) + ftp_config.multrule[index]->base_path_len;
            while(filetmp != NULL)
            {
                char *serverpathtmp = filetmp->href;
                serverpathtmp = serverpathtmp + strlen(ftp_config.multrule[index]->rooturl);
                DEBUG("%s\t%s\n",localpathtmp,serverpathtmp);

                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                DEBUG("cmp:%d\n",cmp);

                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                if(ftp_config.multrule[index]->rules == 1)
                {
                    action_item *item;
                    item = get_action_item("download_only",localfiletmp->path,
                                           g_pSyncList[index]->download_only_socket_head,index);
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
                                     g_pSyncList[index]->up_space_not_enough_list,index);
                if(pp == NULL)
                {
                    unlink(localfiletmp->path);
                    add_action_item("remove",localfiletmp->path,g_pSyncList[index]->server_action_list);
                }
            }
            else
            {
                if((ret = the_same_name_compare(localfiletmp,filetmp,index,1)) != 0)
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
            char *serverpathtmp = filetmp->href;
            serverpathtmp = serverpathtmp + strlen(ftp_config.multrule[index]->rooturl);
            int cmp = 1;
            while(localfiletmp != NULL)
            {
                char *localpathtmp = strstr(localfiletmp->path,ftp_config.multrule[index]->base_path) + ftp_config.multrule[index]->base_path_len;
                DEBUG("%s\t%s\n",serverpathtmp,localpathtmp);

                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                DEBUG("cmp:%d\n),cmp");

                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                char *localpath = serverpath_to_localpath(filetmp->href,index);
                action_item *item;
                item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);

                int cp = 0;
                do{
                    if(exit_loop == 1)
                        return -1;
                    cp = is_ftp_file_copying(filetmp->href,index);
                }while(cp == 1);

                if(is_local_space_enough(filetmp,index))
                {
                    add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);

                    int status = download_(filetmp->href,index);
                    if(status == 0)
                    {
                        DEBUG("do_cmd ok\n");
                        mod_time *onefiletime = Getmodtime(filetmp->href,index);
                        if(ChangeFile_modtime(localpath,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                        if(item != NULL)
                        {
                            del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                        }
                    }
                }
                else
                {
                    write_log(S_ERROR,"local space is not enough!","",index);
                    if(item == NULL)
                    {
                        add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
                free(localpath);
            }
            filetmp = filetmp->next;
            localfiletmp = perform_lo->filelist->next;
        }
    }

    /*************handle folders**************/
    if(perform_br->foldernumber == 0 && perform_lo->foldernumber != 0)
    {
        DEBUG("serverfolderNo:%d\t\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
        while(localfoldertmp != NULL && exit_loop == 0)
        {
            if(ftp_config.multrule[index]->rules == 1)
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
            del_all_items(localfoldertmp->path,index);
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
            char *localpath = (char *)malloc(sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(foldertmp->href) + 1));
            memset(localpath,'\0',sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(foldertmp->href) + 1));
            char *p = foldertmp->href;
            p = p + strlen(ftp_config.multrule[index]->rooturl);
            sprintf(localpath,"%s%s",ftp_config.multrule[index]->base_path,p);
            DEBUG("%s\n",localpath);

            char *prePath = get_prepath(localpath,strlen(localpath));
            int exist = is_server_exist(prePath,localpath,index);
            if(exist)
            {
                if(NULL == opendir(localpath))
                {
                    add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                    mkdir(localpath,0777);
                }
            }
            free(prePath);
            free(localpath);
            foldertmp = foldertmp->next;
        }
    }
    else if(perform_br->foldernumber != 0 && perform_lo->foldernumber != 0)
    {
        DEBUG("serverfolderNo:%d\t\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
        while(localfoldertmp != NULL && exit_loop == 0)
        {
            int cmp = 1;
            char *localpathtmp = strstr(localfoldertmp->path,ftp_config.multrule[index]->base_path)
                           + ftp_config.multrule[index]->base_path_len;
            while(foldertmp != NULL)
            {
                char *serverpathtmp = foldertmp->href;
                serverpathtmp = serverpathtmp + strlen(ftp_config.multrule[index]->rooturl);
                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                if(ftp_config.multrule[index]->rules == 1)
                {
                    action_item *item;
                    item = get_action_item("download_only",
                                           localfoldertmp->path,g_pSyncList[index]->download_only_socket_head,index);
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
            char *serverpathtmp = foldertmp->href;
            serverpathtmp = serverpathtmp + strlen(ftp_config.multrule[index]->rooturl);
            while(localfoldertmp != NULL)
            {
                char *localpathtmp = strstr(localfoldertmp->path,ftp_config.multrule[index]->base_path)
                               + ftp_config.multrule[index]->base_path_len;
                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                char *localpath = (char *)malloc(sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(foldertmp->href) + 1));
                memset(localpath,'\0',sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(foldertmp->href) + 1));
                sprintf(localpath,"%s%s",ftp_config.multrule[index]->base_path,serverpathtmp);
                DEBUG("%s\n",localpath);

                char *prePath = get_prepath(localpath,strlen(localpath));
                int exist = is_server_exist(prePath,localpath,index);
                if(exist)
                {
                    if(NULL == opendir(localpath))
                    {
                        add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                        mkdir(localpath,0777);
                    }
                }
                free(prePath);
                free(localpath);
            }
            foldertmp = foldertmp->next;
            localfoldertmp = perform_lo->folderlist->next;
        }
    }
    return ret;
}

int sync_server_to_local(Server_TreeNode *treenode,int (*sync_fuc)(Browse*,Local*,int),int index)
{
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
    char *localpath = serverpath_to_localpath(treenode->parenthref,index);
    localnode = Find_Floor_Dir(localpath);

    free(localpath);

    if(NULL != localnode)
    {
        ret = sync_fuc(treenode->browse,localnode,index);
        if(ret == COULD_NOT_CONNECNT_TO_SERVER || ret == PERMISSION_DENIED || ret == CONNECNTION_TIMED_OUT
           || ret == COULD_NOT_READ_RESPONSE_BODY || ret == HAVE_LOCAL_SOCKET)
        {
            free_localfloor_node(localnode);
            return ret;
        }
        free_localfloor_node(localnode);
    }

    if(treenode->Child != NULL)
    {
        ret = sync_server_to_local(treenode->Child,sync_fuc,index);
        if(ret != 0 && ret != SERVER_SPACE_NOT_ENOUGH
           && ret != LOCAL_FILE_LOST && ret != SERVER_FILE_DELETED)
        {
            return ret;
        }
    }
    if(treenode->NextBrother != NULL)
    {
        ret = sync_server_to_local(treenode->NextBrother,sync_fuc,index);
        if(ret != 0 && ret != SERVER_SPACE_NOT_ENOUGH
           && ret != LOCAL_FILE_LOST && ret != SERVER_FILE_DELETED)
        {
            return ret;
        }
    }

    return ret;
}

int compareLocalList(int index){
    DEBUG("compareLocalList start!\n");
    int ret = 0;

    if(g_pSyncList[index]->ServerRootNode->Child != NULL)
    {
        ret = sync_server_to_local(g_pSyncList[index]->ServerRootNode->Child,sync_server_to_local_perform,index);
    }
    else
    {
        DEBUG("ServerRootNode->Child == NULL\n");
    }

    return ret;
}


/*ret = 0,server changed
 *ret = 1,server is no changed
*/
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

void *SyncServer(void *argc)
{
    DEBUG("SyncServer:%u\n",pthread_self());
    struct timeval now;
    struct timespec outtime;
    int status;
    int i;

    while (!exit_loop)
    {
#ifdef _PC
        //check_link_internet();
#else
        check_link_internet();
#endif
        for(i = 0; i < ftp_config.dir_num; i++)
        {
            //2014.10.30 by sherry  Sockect_praser和Sync_server线程都卡着（处于在等待状态）
            //server_sync 和local_sync同时为1
//            status = 0;
//            server_sync = 1;      //server sync starting

//            while (local_sync == 1 && exit_loop == 0)
//            {
//                usleep(1000*10);
//            }


                status = 0;

                while (local_sync == 1 && exit_loop == 0)
                {
                    usleep(1000*10);
                }

                server_sync = 1;      //server sync starting


                if(exit_loop)
                    break;

                if(disk_change)
                {
                    //disk_change = 0;
                    check_disk_change();
                }

                status = usr_auth(ftp_config.multrule[i]->server_ip, ftp_config.multrule[i]->user_pwd);
                if(status == 7)
                {
                        write_log(S_ERROR,"check your ip.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                else if(status == 67)
                {
                        write_log(S_ERROR,"check your usrname or password.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                else if(status == 28)
                {
                        write_log(S_ERROR,"connect timeout,check your ip or network.","",ftp_config.multrule[i]->rules); 
                        continue;
                }
                int flag = is_server_exist(ftp_config.multrule[i]->hardware, ftp_config.multrule[i]->rooturl, i);
                if(flag == CURLE_FTP_COULDNT_RETR_FILE)
                {
                        write_log(S_ERROR,"Permission Denied.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                else if(flag == 0)
                {
                        //write_log(S_ERROR,"remote root path lost OR write only.","",ftp_config.multrule[i]->rules);
                        //continue;
                }

            if(g_pSyncList[i]->sync_disk_exist == 0)
            {
                write_log(S_ERROR,"Sync disk unplug!","",i);
                continue;
            }

            if(g_pSyncList[i]->no_local_root)
            {
                my_local_mkdir(ftp_config.multrule[i]->base_path);
                send_action(ftp_config.id,ftp_config.multrule[i]->base_path,INOTIFY_PORT);
                usleep(1000*10);
                g_pSyncList[i]->no_local_root = 0;
                g_pSyncList[i]->init_completed = 0;
            }

            status = do_unfinished(i);

            if(status != 0 && status != -1)
            {
                server_sync = 0;      //server sync finished
                //sleep(2);
                usleep(1000*200);
                break;
            }

            if(g_pSyncList[i]->init_completed == 0)
            {
                status = initialization();
                if(status)
                        continue;
            }

            if(ftp_config.multrule[i]->rules == 2)
            {
                continue;
            }
            if(exit_loop == 0)
            {
                DEBUG("get ServerRootNode\n");
                g_pSyncList[i]->ServerRootNode = create_server_treeroot();
                status = browse_to_tree(ftp_config.multrule[i]->rooturl,g_pSyncList[i]->ServerRootNode,i);
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
                    if(ftp_config.multrule[i]->rules == 0)
                    {
                        status = compareServerList(i);
                    }
                    if(status == 0 || ftp_config.multrule[i]->rules == 1)
                    {
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
                        free_server_tree(g_pSyncList[i]->ServerRootNode);
                        g_pSyncList[i]->ServerRootNode = NULL;
                    }
                }
            }
            write_log(S_SYNC,"","",i);
        }
        server_sync = 0;  //server_sync finished!
        pthread_mutex_lock(&mutex);
        if(!exit_loop)
        {
            gettimeofday(&now, NULL);
            outtime.tv_sec = now.tv_sec + 5;
            outtime.tv_nsec = now.tv_usec * 1000;
            pthread_cond_timedwait(&cond, &mutex, &outtime);
        }
        //DEBUG("server loop\n");
        pthread_mutex_unlock(&mutex);
    }
    DEBUG("stop FtpClient server sync\n");

    stop_up = 1;

}

int cmd_parser(char *cmd,int index)
{
    //对path，name，actionname进行解析
    if( strstr(cmd,"(conflict)") != NULL )
        return 0;

    DEBUG("socket command is %s \n",cmd);
    if( !strncmp(cmd,"exit",4))
    {
        DEBUG("exit socket\n");
        return 0;
    }

    if(!strncmp(cmd,"rmroot",6))
    {
        g_pSyncList[index]->no_local_root = 1;
        return 0;
    }

    char cmd_name[64];
    //char cmd_param[512];
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
    int status;

    //Asusconfig asusconfig;

    memset(cmd_name, 0, sizeof(cmd_name));
    memset(oldname,'\0',sizeof(oldname));
    memset(newname,'\0',sizeof(newname));
    memset(action,0,sizeof(action));

    ch = cmd;
    int i = 0;
    //while(*ch != '@')
    while(*ch != '\n')
    {
        i++;
        ch++;
    }

    memcpy(cmd_name, cmd, i);

    char *p = NULL;
    ch++;
    i++;

    temp = my_str_malloc((size_t)(strlen(ch)+1));

    strcpy(temp,ch);
    //p = strchr(temp,'@');
    p = strchr(temp,'\n');

    path = my_str_malloc((size_t)(strlen(temp)- strlen(p)+1));

    if(p!=NULL)
        snprintf(path,strlen(temp)- strlen(p)+1,"%s",temp);

    //free(temp);

    p++;
    if(strncmp(cmd_name, "rename",6) == 0)//是否是rename，是rename，strncmp(cmd_name, "rename",6)返回0，是走里面的
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));

        p1++;

        strcpy(newname,p1);

        DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);
        if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }
    else if(strncmp(cmd_name, "move",4) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);
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
        strcpy(filename,p);
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
        DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);
        if(filename[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }

    free(temp);

    if( !strncmp(cmd_name,"rename",6) )//是rename返回1，即：是rename走里面的内容
    {
        cmp_name = my_str_malloc((size_t)(strlen(path)+strlen(newname)+2));
        sprintf(cmp_name,"%s/%s",path,newname);
    }
    else
    {
        cmp_name = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
        sprintf(cmp_name,"%s/%s",path,filename);
    }

    if( strcmp(cmd_name, "createfile") == 0 )
    {
        strcpy(action,"createfile");
            del_action_item("copyfile",cmp_name,g_pSyncList[index]->copy_file_list);
    }
    else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
    {
        strcpy(action,"remove");
    }
    else if( strcmp(cmd_name, "createfolder") == 0 )
    {
        strcpy(action,"createfolder");
        del_action_item("copyfile",cmp_name,g_pSyncList[index]->copy_file_list);
    }
    else if( strncmp(cmd_name, "rename",6) == 0 )
    {
        strcpy(action,"rename");
    }
    else if( strncmp(cmd_name, "move",4) == 0 )
    {
        strcpy(action,"move");
    }

    if(g_pSyncList[index]->server_action_list->next != NULL)
    {
        action_item *item;

        item = get_action_item(action,cmp_name,g_pSyncList[index]->server_action_list,index);

        if(item != NULL)
        {
            DEBUG("##### %s %s by FTP Server self ######\n",action,cmp_name);
            del_action_item(action,cmp_name,g_pSyncList[index]->server_action_list);
            DEBUG("#### del action item success!\n");
            free(path);
            free(fullname);
            free(cmp_name);
            return 0;
        }
    }

    if(g_pSyncList[index]->dragfolder_action_list->next != NULL)
    {
        action_item *item;

        item = get_action_item(action,cmp_name,g_pSyncList[index]->dragfolder_action_list,index);

        if(item != NULL)
        {
            DEBUG("##### %s %s by dragfolder recursion self ######\n",action,cmp_name);
            del_action_item(action,cmp_name,g_pSyncList[index]->dragfolder_action_list);
            free(path);
            free(fullname);
            free(cmp_name);
            return 0;
        }
    }
    free(cmp_name);
//针对不同的操作进行，作出具体的反应
    DEBUG("###### %s is start ######\n",cmd_name);

    if( strcmp(cmd_name, "copyfile") != 0 )
    {
        g_pSyncList[index]->have_local_socket = 1;
    }

    if( strcmp(cmd_name, "createfile") == 0 || strcmp(cmd_name, "dragfile") == 0 )
    {
        sprintf(fullname,"%s/%s",path,filename);
        int exist = is_server_exist(path,fullname,index);
        //2014.11.7 by sherry
        //if(exist)
//        if(exist==1)
//        {
//            char *temp = change_server_same_name(fullname,index);//名字要改成什么
//            my_rename_(fullname,temp,index);//改名字
//            free(temp);
//            char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
//            write_conflict_log(fullname,3,err_msg); //conflict
//            free(err_msg);
//        }
        if(exist==1)
        {
//           if((newer_file(fullname,index,1)) != 2 && (newer_file(fullname,index,1)) != -1)
//           {
               char *temp = change_server_same_name(fullname,index);//名字要改成什么
               my_rename_(fullname,temp,index);//改名字
               char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
               write_conflict_log(fullname,3,err_msg); //conflict
               free(err_msg);
              free(temp);
//           }

        }
        status = upload(fullname,index);
        if(status != 0)
        {
            DEBUG("upload %s failed\n",fullname);
            free(path);
            free(fullname);
            return status;
        }
        else
        {
            char *serverpath = localpath_to_serverpath(fullname,index);
            mod_time *onefiletime = Getmodtime(serverpath,index);
            if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
            {
                DEBUG("ChangeFile_modtime failed!\n");
            }
            free(onefiletime);
            free(fullname);
            free(serverpath);
        }
    }
    else if( strcmp(cmd_name, "copyfile") == 0 )
    {
        sprintf(fullname,"%s/%s",path,filename);
        add_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
        free(fullname);
    }
    else if( strncmp(cmd_name, "cancelcopy", 10) == 0)
    {
            sprintf(fullname,"%s/%s",path,filename);
            del_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
            free(fullname);
    }
    else if( strcmp(cmd_name, "modify") == 0 )
    {
        sprintf(fullname,"%s/%s",path,filename);
        char *serverpath = localpath_to_serverpath(fullname,index);
        int exist;
        DEBUG("%s %s\n",path,fullname);
        exist = is_server_exist(path,fullname,index);
        if(!exist)       //Server has no the same file
        {
            status = upload(fullname,index);
            if(status != 0)
            {
                DEBUG("upload %s failed\n",fullname);
                free(path);
                free(fullname);
                return status;
            }
            else
            {
                mod_time *onefiletime;
                onefiletime = Getmodtime(serverpath,index);
                if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                {
                    DEBUG("ChangeFile_modtime failed!\n");
                }
                free(onefiletime);
                free(fullname);
                free(serverpath);
            }
        }
        else
        {
            //mod_time *onefiletime;
            //onefiletime = Getmodtime(serverpath,index);
            CloudFile *cloud_file = NULL;
            if(g_pSyncList[index]->init_completed)
                cloud_file = get_CloudFile_node(g_pSyncList[index]->OldServerRootNode,serverpath,0x2);
            else
                cloud_file = get_CloudFile_node(g_pSyncList[index]->ServerRootNode,serverpath,0x2);
            if(ftp_config.multrule[index]->rules == 2)
            {
                if(cloud_file == NULL)
                {
                    char *temp = change_server_same_name(fullname,index);
                    my_rename_(fullname,temp,index);
                    //free(temp);
                    status = upload(fullname,index);
                    if(status != 0)
                    {
                        DEBUG("modify failed status = %d\n",status);
                        free(path);
                        return status;
                    }
                    else
                    {

                        char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                        write_conflict_log(fullname,3,err_msg); //conflict
                        free(err_msg);
                        DEBUG("serverpath = %s\n",serverpath);
                        mod_time *onefiletime = Getmodtime(serverpath,index);
                        if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                        free(fullname);
                        free(serverpath);
                    }
                }
                else
                {
                    if(cloud_file->ismodify)
                    {
                        char *temp = change_server_same_name(fullname,index);
                        my_rename_(fullname,temp,index);
                        free(temp);
                        status = upload(fullname,index);
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
                            write_conflict_log(fullname,3,err_msg); //conflict
                            free(err_msg);
                            DEBUG("serverpath = %s\n",serverpath);
                            mod_time *onefiletime = Getmodtime(serverpath,index);
                            if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                            {
                                DEBUG("ChangeFile_modtime failed!\n");
                            }
                            free(onefiletime);
                            free(fullname);
                            free(serverpath);
                        }
                    }
                    else
                    {
                        mod_time *modtime1 = NULL;
                        time_t modtime2;
                        modtime1 = Getmodtime(serverpath,index);
                        modtime2 = cloud_file->modtime;
                        if(modtime1->modtime != modtime2)
                        {
                            char *temp = change_server_same_name(fullname,index);
                            my_rename_(fullname,temp,index);
                            free(temp);
                            status = upload(fullname,index);
                            free(modtime1);
                            if(status != 0)
                            {
                                DEBUG("modify failed status = %d\n",status);
                                free(path);
                                return status;
                            }
                            else
                            {
                                char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                                write_conflict_log(fullname,3,err_msg); //conflict
                                free(err_msg);
                                DEBUG("serverpath = %s\n",serverpath);
                                mod_time *onefiletime = Getmodtime(serverpath,index);
                                if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                                {
                                    DEBUG("ChangeFile_modtime failed!\n");
                                }
                                free(onefiletime);
                                free(fullname);
                                free(serverpath);
                            }
                        }
                        else
                        {
                            status = upload(fullname,index);
                            if(status != 0)
                            {
                                DEBUG("modify failed status = %d\n",status);
                                free(path);
                                return status;
                            }
                            else
                            {
                                DEBUG("serverpath = %s\n",serverpath);
                                mod_time *onefiletime;
                                onefiletime = Getmodtime(serverpath,index);
                                if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                                {
                                    DEBUG("ChangeFile_modtime failed!\n");
                                }
                                free(onefiletime);
                                free(fullname);
                                free(serverpath);
                            }
                        }
                    }
                }
            }
            else
            {
                mod_time *modtime1;
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
                if(modtime1->modtime == modtime2)
                {
                    status = upload(fullname,index);
                    free(modtime1);
                    if(status != 0)
                    {
                        DEBUG("modify failed status = %d\n",status);
                        free(path);
                        return status;
                    }
                    else
                    {
                        DEBUG("serverpath = %s\n",serverpath);
                        mod_time *onefiletime = Getmodtime(serverpath,index);
                        if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                        free(fullname);
                        free(serverpath);
                    }
                }
                else
                {
                    char *temp = change_server_same_name(fullname,index);
                    my_rename_(fullname,temp,index);
                    //free(temp);
                    status = upload(fullname,index);
                    if(status != 0)
                    {
                        DEBUG("modify failed status = %d\n",status);
                        free(path);
                        return status;
                    }
                    else
                    {
                        char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
                        write_conflict_log(fullname,3,err_msg); //conflict
                        free(err_msg);
                        DEBUG("serverpath = %s\n",serverpath);
                        mod_time *onefiletime = Getmodtime(serverpath,index);
                        if(ChangeFile_modtime(fullname,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                        free(fullname);
                        free(serverpath);
                    }
                }
            }
        }
    }
    else if(strcmp(cmd_name, "delete") == 0  || strcmp(cmd_name, "remove") == 0)
    {
        sprintf(fullname,"%s/%s",path,filename);
        status = Delete(fullname,index);
        free(fullname);
        free(path);
        return status;
    }
    else if(strncmp(cmd_name, "move",4) == 0 || strncmp(cmd_name, "rename",6) == 0)
    {
        if(strncmp(cmd_name, "move",4) == 0)
        {

            mv_newpath = my_str_malloc((size_t)(strlen(path)+strlen(oldname)+2));
            mv_oldpath = my_str_malloc((size_t)(strlen(oldpath)+strlen(oldname)+2));
            sprintf(mv_newpath,"%s/%s",path,oldname);
            sprintf(mv_oldpath,"%s/%s",oldpath,oldname);
            free(oldpath);
        }
        else
        {
            mv_newpath = my_str_malloc((size_t)(strlen(path)+strlen(newname)+2));
            mv_oldpath = my_str_malloc((size_t)(strlen(path)+strlen(oldname)+2));
            sprintf(mv_newpath,"%s/%s",path,newname);
            sprintf(mv_oldpath,"%s/%s",path,oldname);
        }
        if(strncmp(cmd_name,"rename",6) == 0)
        {

            int exist = is_server_exist(path,mv_newpath,index);
            if(!exist)
            {
                //status = 0;
                status = my_rename_(mv_oldpath,newname,index);
            }
//            else
//            {
//                char *temp = change_server_same_name(mv_newpath,index);
//                my_rename_(mv_newpath,temp,index);
//                //free(temp);
//                char *err_msg = write_error_message("server file %s is renamed to %s",fullname,temp);
//                write_conflict_log(fullname,3,err_msg); //conflict
//                free(err_msg);
//                status = my_rename_(mv_oldpath,newname,index);
//            }
        }
        else    //move
        {
            int exist = 0;
            int old_index;
            old_index = get_path_to_index(mv_oldpath);
//            if(ftp_config.multrule[old_index]->rules == 1)
//            {
//                del_download_only_action_item("move",mv_oldpath,g_pSyncList[old_index]->download_only_socket_head);
//            }
//
//            if(test_if_dir(mv_newpath))
//            {
//                exist = is_server_exist(path,mv_newpath,index);
//                if(exist)
//                {
//                    char *newname = change_server_same_name(mv_newpath,index);
//                    status = my_rename_(mv_newpath,newname,index);
//                    //free(newname);
//                    status = moveFolder(mv_oldpath,mv_newpath,index);
//                }
//                else
//                {
//                    status = moveFolder(mv_oldpath,mv_newpath,index);
//                }
//            }
//            else
//            {
//                Delete(mv_oldpath,index);
//                exist = is_server_exist(path,mv_newpath,index);
//                if(exist)
//                {
//                    char *newname = change_server_same_name(mv_newpath,index);
//                    status = my_rename_(mv_newpath,newname,index);
//                }
//                status = upload(mv_newpath,index);
//                if(status == 0)
//                {
//                    char *serverpath = localpath_to_serverpath(mv_newpath,index);
//                    mod_time *onefiletime = Getmodtime(serverpath,index);
//                    free(serverpath);
//                    if(ChangeFile_modtime(mv_newpath,onefiletime->modtime,index))
//                    {
//                        DEBUG("ChangeFile_modtime failed!\n");
//                    }
//                    free(onefiletime);
//                }
//            }
            if(index == old_index)
            {
                if(test_if_dir(mv_newpath))
                {
                    exist = is_server_exist(path,mv_newpath,index);

                    if(exist)
                    {
                        char *newname;
                        newname = change_server_same_name(mv_newpath,index);
                        status = my_rename_(mv_newpath,newname,index);
                        char *err_msg = write_error_message("server file %s is renamed to %s",mv_newpath,newname);
                        write_conflict_log(mv_newpath,3,err_msg); //conflict
                        free(err_msg);

                        free(newname);
                        if(status == 0)
                        {
                            status = moveFolder(mv_oldpath,mv_newpath,index);
                        }
                    }
                    else
                    {
                        status = moveFolder(mv_oldpath,mv_newpath,index);
                    }
                }
                else
                {

                    Delete(mv_oldpath,index);
                    exist = is_server_exist(path,mv_newpath,index);
                    if(exist)
                    {
                        char *newname;
                        newname = change_server_same_name(mv_newpath,index);
                        status = my_rename_(mv_newpath,newname,index);
                        char *err_msg = write_error_message("server file %s is renamed to %s",mv_newpath,newname);
                        write_conflict_log(mv_newpath,3,err_msg); //conflict
                        free(err_msg);

                        free(newname);
                        if(status == 0)
                        {
                            status = upload(mv_newpath,index);
                            if(status != 0)
                            {
                                DEBUG("move %S to %s failed\n",mv_oldpath,mv_newpath);
                                free(path);
                                free(mv_oldpath);
                                free(mv_newpath);
                                return status;
                            }
                            else
                            {
                                char *serverpath = localpath_to_serverpath(mv_newpath,index);
                                mod_time *onefiletime = Getmodtime(serverpath,index);
                                free(serverpath);
                                if(ChangeFile_modtime(mv_newpath,onefiletime->modtime,index))
                                {
                                    DEBUG("ChangeFile_modtime failed!\n");
                                }
                                free(onefiletime);
                            }
                        }

                    }
                    else
                    {
                        status = upload(mv_newpath,index);
                        if(status != 0)
                        {
                            DEBUG("move %S to %s failed\n",mv_oldpath,mv_newpath);
                            free(path);
                            free(mv_oldpath);
                            free(mv_newpath);
                            return status;
                        }
                        else
                        {
                            char *serverpath = localpath_to_serverpath(mv_newpath,index);
                            mod_time *onefiletime = Getmodtime(serverpath,index);
                            free(serverpath);
                            if(ChangeFile_modtime(mv_newpath,onefiletime->modtime,index))
                            {
                                DEBUG("ChangeFile_modtime failed!\n");
                            }
                            free(onefiletime);
                        }
                    }

                }
            }
            else
            {
                if(ftp_config.multrule[old_index]->rules == 1)
                {
                    del_download_only_action_item("move",mv_oldpath,g_pSyncList[old_index]->download_only_socket_head);
                }
                else
                {
                    Delete(mv_oldpath,old_index);
                }

                if(test_if_dir(mv_newpath))
                {
                    status = moveFolder(mv_oldpath,mv_newpath,index);
                    if(status != 0)
                    {
                        DEBUG("createFolder failed status = %d\n",status);
                        free(path);
                        free(mv_oldpath);
                        free(mv_newpath);
                        return status;
                    }

                }
                else
                {
                    status = upload(mv_newpath,index);

                    if(status != 0)
                    {
                        DEBUG("move %S to %s failed\n",mv_oldpath,mv_newpath);
                        free(path);
                        free(mv_oldpath);
                        free(mv_newpath);
                        return status;
                    }
                    else
                    {
                        char *serverpath = localpath_to_serverpath(mv_newpath,index);
                        mod_time *onefiletime = Getmodtime(serverpath,index);
                        free(serverpath);
                        if(ChangeFile_modtime(mv_newpath,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                    }
                }
            }
        }

        free(mv_oldpath);
        free(mv_newpath);

        if(status != 0)
        {
            DEBUG("move/rename failed\n");
            free(path);
            return status;
        }
    }

    else if(strcmp(cmd_name, "dragfolder") == 0)
    {
        sprintf(fullname,"%s/%s",path,filename);

        char info[512];
        memset(info,0,sizeof(info));
        sprintf(info,"createfolder%s%s%s%s","\n",path,"\n",filename);
        pthread_mutex_lock(&mutex_socket);
        add_socket_item(info,index);
        pthread_mutex_unlock(&mutex_socket);
        status = deal_dragfolder_to_socketlist(fullname,index);
        free(fullname);
//        int exist = is_server_exist(path,fullname,index);
//        if(exist)
//        {
//            my_rename_(fullname,change_server_same_name(fullname,index),index);
//        }
//        status = createFolder(fullname,index);
//        free(fullname);
//        if(status != 0)
//        {
//            DEBUG("createFolder failed status = %d\n",status);
//            free(path);
//            return status;
//        }
    }

    else if(strcmp(cmd_name, "createfolder") == 0)
    {
        sprintf(fullname,"%s/%s",path,filename);
        int exist = is_server_exist(path,fullname,index);
        if(exist)
        {
            return 0;
            //char *temp = change_server_same_name(fullname,index);
            //my_rename_(fullname,temp,index);
        }
        status = my_mkdir_(fullname,index);
        free(fullname);
        if(status != 0)
        {
            free(path);
            return status;
        }
    }

    free(path);
    return 0;
}
int download_only_add_socket_item(char *cmd,int index)
{
    DEBUG("running download_only_add_socket_item: %s\n",cmd);

    if( strstr(cmd,"(conflict)") != NULL )
        return 0;

    if( !strncmp(cmd,"exit",4))
    {
        DEBUG("exit socket\n");
        return 0;
    }

    if(!strncmp(cmd,"rmroot",6))
    {
        g_pSyncList[index]->no_local_root = 1;
        return 0;
    }

    char cmd_name[64];
    char *path = NULL;
    char *temp = NULL;
    char filename[256];
    char *fullname = NULL;
    char oldname[256],newname[256];
    char *oldpath = NULL;
    char action[64];
    //char *cmp_name = NULL;
    //char *mv_newpath;
    //char *mv_oldpath;
    char *ch = NULL;
    char *old_fullname = NULL;
    //int status;

    memset(cmd_name,'\0',sizeof(cmd_name));
    memset(oldname,'\0',sizeof(oldname));
    memset(newname,'\0',sizeof(newname));
    memset(action,'\0',sizeof(action));

    ch = cmd;
    int i = 0;
    //while(*ch != '@')
    while(*ch != '\n')
    {
        i++;
        ch++;
    }

    memcpy(cmd_name, cmd, i);

    char *p = NULL;
    ch++;
    i++;
    temp = my_str_malloc((size_t)(strlen(ch)+1));

    strcpy(temp,ch);
    //p = strchr(temp,'@');
    p = strchr(temp,'\n');

    //DEBUG("temp = %s\n",temp);
    //DEBUG("p = %s\n",p);
    //DEBUG("strlen(temp)- strlen(p) = %d\n",strlen(temp)- strlen(p));

    path = my_str_malloc((size_t)(strlen(temp)- strlen(p)+1));

    //DEBUG("path = %s\n",path);

    if(p!=NULL)
        snprintf(path,strlen(temp)- strlen(p)+1,"%s",temp);

    //free(temp);

    p++;
    if(strncmp(cmd_name, "rename",6) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));

        p1++;

        strcpy(newname,p1);
    }
    else if(strncmp(cmd_name, "move",4) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);
    }
    else
    {
        strcpy(filename,p);
        //fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
        DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);
    }

    free(temp);

    if( !strncmp(cmd_name,"rename",strlen("rename")) )
    {
        fullname = my_str_malloc((size_t)(strlen(path) + strlen(newname) + 2));
        old_fullname = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
        sprintf(fullname,"%s/%s",path,newname);
        sprintf(old_fullname,"%s/%s",path,oldname);
        free(path);
    }
    else if( !strncmp(cmd_name,"move",strlen("move")) )
    {
        fullname = my_str_malloc((size_t)(strlen(path) + strlen(oldname) + 2));
        old_fullname = my_str_malloc((size_t)(strlen(oldpath) + strlen(oldname) + 2));
        sprintf(fullname,"%s/%s",path,oldname);
        sprintf(old_fullname,"%s/%s",oldpath,oldname);
        free(oldpath);
        free(path);
    }
    else
    {
        fullname = my_str_malloc((size_t)(strlen(path) + strlen(filename) + 2));
        sprintf(fullname,"%s/%s",path,filename);
        free(path);
    }

    if( !strncmp(cmd_name,"copyfile",strlen("copyfile")) )
    {
        add_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
        return 0;
    }
    if( !strncmp(cmd_name, "cancelcopy", 10))
    {
            //sprintf(fullname,"%s/%s",path,filename);
            del_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
            free(fullname);
            return 0;
    }
    if( strcmp(cmd_name, "createfile") == 0 )
    {
                strcpy(action,"createfile");
            del_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
    }
    else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
    {
        strcpy(action,"remove");
        del_download_only_action_item(action,fullname,g_pSyncList[index]->download_only_socket_head);
    }
    else if( strcmp(cmd_name, "createfolder") == 0 )
    {
        strcpy(action,"createfolder");
        del_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
    }
    else if( strncmp(cmd_name, "rename",6) == 0 )
    {
        strcpy(action,"rename");
        del_download_only_action_item(action,old_fullname,g_pSyncList[index]->download_only_socket_head);
        free(old_fullname);
    }
    else if( strncmp(cmd_name, "move",4) == 0 )
    {
        strcpy(action,"move");
        del_download_only_action_item(action,old_fullname,g_pSyncList[index]->download_only_socket_head);
        //free(old_fullname);
    }

    if(g_pSyncList[index]->server_action_list->next != NULL)
    {
        action_item *item;
        DEBUG("%s:%s\n",action,fullname);
        item = get_action_item(action,fullname,g_pSyncList[index]->server_action_list,index);

        if(item != NULL)
        {
            DEBUG("##### %s %s by FTP Server self ######\n",action,fullname);
            //pthread_mutex_lock(&mutex);
            del_action_item(action,fullname,g_pSyncList[index]->server_action_list);
            free(fullname);
            return 0;
        }
    }

    if(g_pSyncList[index]->dragfolder_action_list->next != NULL)
    {
        action_item *item;

        item = get_action_item(action,fullname,g_pSyncList[index]->dragfolder_action_list,index);

        if(item != NULL)
        {
            DEBUG("##### %s %s by dragfolder recursion self ######\n",action,fullname);
            del_action_item(action,fullname,g_pSyncList[index]->dragfolder_action_list);
            free(fullname);
            return 0;
        }
    }

    if( strcmp(cmd_name, "copyfile") != 0 )
    {
        g_pSyncList[index]->have_local_socket = 1;
    }

    if(strncmp(cmd_name, "rename",6) == 0)
    {
        if(test_if_dir(fullname))
        {
            add_all_download_only_socket_list(cmd_name,fullname,index);
        }
        else
        {
            add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
        }
    }
    else if(strncmp(cmd_name, "move",4) == 0)
    {
        int old_index = get_path_to_index(old_fullname);
        if(old_index == index)
        {
            if(test_if_dir(fullname))
            {
                add_all_download_only_socket_list(cmd_name,fullname,index);
            }
            else
            {
                add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
            }
        }
        else
        {
            if(ftp_config.multrule[old_index]->rules == 1)
            {
                del_download_only_action_item("",old_fullname,g_pSyncList[old_index]->download_only_socket_head);
            }
            else
            {
                Delete(old_fullname,old_index);
            }
            if(test_if_dir(fullname))
            {
                add_all_download_only_socket_list(cmd_name,fullname,index);
            }
            else
            {
                add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
            }
        }

        free(old_fullname);
    }
    else if(strcmp(cmd_name, "createfolder") == 0 || strcmp(cmd_name, "dragfolder") == 0)
    {
        add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
        add_all_download_only_dragfolder_socket_list(fullname,index);
    }
    else if( strcmp(cmd_name, "createfile") == 0  || strcmp(cmd_name, "dragfile") == 0 || strcmp(cmd_name, "modify") == 0)
    {
        add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
    }

    free(fullname);
    return 0;
}

int is_server_modify(Server_TreeNode *newNode,Server_TreeNode *oldNode)
{

    //DEBUG("########is_server_modify\n");
    if(newNode->browse == NULL && oldNode->browse == NULL)
    {
        DEBUG("########Server is no modify1\n");
        return 1;
    }
    else if(newNode->browse == NULL && oldNode->browse != NULL)
    {
        DEBUG("########Server is no modify2\n");
        return 1;
    }
    else if(newNode->browse != NULL && oldNode->browse == NULL)
    {
        DEBUG("########Server is no modify3\n");
        return 1;
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
            {
                newfiletmp = newNode->browse->filelist->next;
            }
            else
            {
                DEBUG("########Server is no modify4\n");
                return 1;
            }
        }
        if(oldNode->browse != NULL)
        {
            if(oldNode->browse->foldernumber > 0)
                oldfoldertmp = oldNode->browse->folderlist->next;
            if(oldNode->browse->filenumber <= 0)
            {
                DEBUG("########Server is no modify5\n");
                return 1;
            }
        }

        while(newfiletmp != NULL)
        {
            oldfiletmp = oldNode->browse->filelist->next;
            while (oldfiletmp != NULL)
            {
                if ((cmp = strcmp(newfiletmp->href,oldfiletmp->href)) == 0)
                {
                    if((newfiletmp->modtime != oldfiletmp->modtime) || (oldfiletmp->ismodify == 1))
                    {
                        DEBUG("########Server %s is modified\n",newfiletmp->href);
                        newfiletmp->ismodify = 1;
                    }
                    break;
                }

                oldfiletmp = oldfiletmp->next;
            }

            newfiletmp = newfiletmp->next;
        }

    }
    //DEBUG("########is_server_modify over\n");
    return 1;
}

int check_serverlist_modify(int index,Server_TreeNode *newnode)
{
    int ret;
    //DEBUG("############check_serverlist_modify %s\n",newnode->parenthref);
    Server_TreeNode *oldnode;
    oldnode = getoldnode(g_pSyncList[index]->OldServerRootNode->Child,newnode->parenthref);

//    if(newnode == NULL)
//    {
//        DEBUG("newnode is NULL\n");
//    }

//    if(oldnode == NULL)
//    {
//        DEBUG("oldnode is NULL\n");
//    }

    if(newnode != NULL && oldnode != NULL)
    {

        ret = is_server_modify(newnode,oldnode);
        //return ret;
    }
    else if(newnode == NULL)
    {
        return 0;
    }

    if(newnode->Child != NULL)
    {
        ret = check_serverlist_modify(index,newnode->Child);
    }

    if(newnode->NextBrother != NULL)
    {
        ret = check_serverlist_modify(index,newnode->NextBrother);
    }

    return ret;
}

void *Socket_Parser(void *argc)
{
    DEBUG("Socket_Parser:%u\n",pthread_self());//获取线程号
    Node *socket_execute;
    int status = 0;
    //int mysync = 1;
    int has_socket = 0;
    int i;
    struct timeval now;
    struct timespec outtime;
    int fail_flag;

    while(!exit_loop)
    {
#ifdef _PC
        //check_link_internet();
#else
        check_link_internet();
#endif
        for(i = 0;i<ftp_config.dir_num;i++)
        {
                fail_flag = 0;
                while (server_sync == 1 && exit_loop ==0)
                {
                    usleep(1000*10);//10毫秒
                }
                local_sync = 1;

                if(exit_loop)
                    break;

                if(disk_change)
                {
                    check_disk_change();
                }
                status = usr_auth(ftp_config.multrule[i]->server_ip,ftp_config.multrule[i]->user_pwd);
                if(status == 7)
                {
                        write_log(S_ERROR,"check your ip.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                else if(status == 67)
                {
                        write_log(S_ERROR,"check your usrname or password.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                else if(status == 28)
                {
                        write_log(S_ERROR,"connect timeout,check your ip or network.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                int flag = is_server_exist(ftp_config.multrule[i]->hardware, ftp_config.multrule[i]->rooturl, i);
                //is_server_exist返回1，硬碟存在，flag=1
                if(flag == CURLE_FTP_COULDNT_RETR_FILE)//判断flag=19? RETR' 命令收到了不正常的回复,或完成的传输尺寸为零字节
                {
                        write_log(S_ERROR,"Permission Denied.","",ftp_config.multrule[i]->rules);
                        continue;
                }
                else if(flag == 0)//curl_ok (All fine. Proceed as usual)
                {
                    DEBUG("curl_OK\n");
                        //write_log(S_ERROR,"remote root path lost OR write only.","",ftp_config.multrule[i]->rules);
                        //continue;
                }


            if(g_pSyncList[i]->sync_disk_exist == 0)
                continue;

            if(ftp_config.multrule[i]->rules == 1)          //download only
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
                                DEBUG("########will del socket item##########\n");
                                pthread_mutex_lock(&mutex_socket);
                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList_Rename);
                                free(socket_execute);
                                DEBUG("del socket item ok\n");
                                pthread_mutex_unlock(&mutex_socket);
                            }
                            else
                            {
                                fail_flag = 1;
                                DEBUG("######## socket item fail########\n");
                                break;
                            }
                            //sleep(2);
                            usleep(1000*20);
                        }
                        while(g_pSyncList[i]->SocketActionList->next != NULL)
                        {
                            has_socket = 1;
                            socket_execute = g_pSyncList[i]->SocketActionList->next;
                            status = download_only_add_socket_item(socket_execute->cmdName,i);
                            if(status == 0)
                            {
                                DEBUG("########will del socket item##########\n");
                                pthread_mutex_lock(&mutex_socket);
                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList);
                                free(socket_execute);
                                DEBUG("del socket item ok\n");
                                pthread_mutex_unlock(&mutex_socket);
                            }
                            else
                            {
                                fail_flag = 1;
                                DEBUG("######## socket item fail########\n");
                                break;
                            }
                            //sleep(2);
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
                if(ftp_config.multrule[i]->rules == 2)     //upload only规则时，处理未完成的上传动作
                {
                }

                while(exit_loop ==0)
                {
                    while(g_pSyncList[i]->SocketActionList_Rename->next != NULL || g_pSyncList[i]->SocketActionList->next != NULL)
                    {
                        while(g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                        {
                            has_socket = 1;
                            socket_execute = g_pSyncList[i]->SocketActionList_Rename->next;
                            //socket_execute->cmdName=rename0
                            //                   /tmp/mnt/SNK/sync
                            //                   未命名文件夹
                            //                    aa
                            status = cmd_parser(socket_execute->cmdName,i);
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
                            //pthread_mutex_unlock(&mutex_socket);
                            //sleep(2);
                            usleep(1000*20);
                        }
                        while(g_pSyncList[i]->SocketActionList->next != NULL && exit_loop == 0)
                        {
                            has_socket = 1;
                            socket_execute = g_pSyncList[i]->SocketActionList->next;
                            //pthread_mutex_lock(&mutex_socket);
                            status = cmd_parser(socket_execute->cmdName,i);
                            DEBUG("status:%d\n",status);
                            if(status == 0 || status == LOCAL_FILE_LOST || status==SERVER_SPACE_NOT_ENOUGH || status == CURLE_READ_ERROR || status == CURLE_ABORTED_BY_CALLBACK)
                            {//CURLE_ABORTED_BY_CALLBACK由回调中止,  CURLE_READ_ERROR:couldn't open/read from file
                                if(status == LOCAL_FILE_LOST && g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                    break;
                                DEBUG("########will del socket item##########\n");
                                pthread_mutex_lock(&mutex_socket);
                                socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList);
                                free(socket_execute);
                                DEBUG("del socket item ok\n");
                                pthread_mutex_unlock(&mutex_socket);
                            }
                            else
                            {
                                fail_flag = 1;
                                DEBUG("######## socket item fail########\n");
                                break;
                            }
                            //pthread_mutex_unlock(&mutex_socket);
                            //sleep(2);
                            usleep(1000*20);

                            if(g_pSyncList[i]->SocketActionList_Rename->next != NULL)
                                break;
                        }
                    }

                    if(fail_flag)
                        break;

                    //show_item(g_pSyncList[i]->copy_file_list);
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
                    //DEBUG("################################################ dragfolder_action_list!\n");
                    free_action_item(g_pSyncList[i]->dragfolder_action_list);
                    g_pSyncList[i]->dragfolder_action_list = create_action_item_head();
                }
                if(g_pSyncList[i]->server_action_list->next != NULL && g_pSyncList[i]->SocketActionList->next == NULL)
                {
                    //DEBUG("################################################ clear server_action_list!\n");
                    free_action_item(g_pSyncList[i]->server_action_list);
                    g_pSyncList[i]->server_action_list = create_action_item_head();
                }
                //DEBUG("#### clear server_action_list success!\n");
                pthread_mutex_lock(&mutex_receve_socket);
                if(g_pSyncList[i]->SocketActionList->next == NULL)
                    g_pSyncList[i]->receve_socket = 0;
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
        //DEBUG("set local_sync %d!\n",local_sync);
    }

    //DEBUG("stop FTP Socket_Parser\n");
    stop_down = 1;
    return NULL;
}

int send_action(int type, char *content,int port)
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char str[1024];
    //int port;
    //if(type == 1)
    //port = INOTIFY_PORT;//3678

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {             //创建socket套接字
        perror("socket");
        return -1;
    }

    struct sockaddr_in their_addr; /* connector's address information */
    bzero(&(their_addr), sizeof(their_addr)); /* zero the rest of the struct */
    their_addr.sin_family = AF_INET; /* host byte order */
    their_addr.sin_port = htons(port); /* short, network byte order */
    their_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //their_addr.sin_addr.s_addr = ((struct in_addr *)(he->h_addr))->s_addr;
    bzero(&(their_addr.sin_zero), sizeof(their_addr.sin_zero)); /* zero the rest of the struct */

    if (connect(sockfd, (struct sockaddr *)&their_addr,sizeof(struct sockaddr)) == -1) {
        perror("connect");
        return -1;
    }

    if(port == 80)
    {
        sprintf(str,"%s",content);
        DEBUG("to 80 port:%s\n",str);
    }
    else
    {
        sprintf(str,"%d@%s",type,content);
    }

    if (send(sockfd, str, strlen(str), 0) == -1) {
        perror("send");
        //exit(1);
        return -1;
    }

//    if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
//        perror("recv");
//        //exit(1);
//        return -1;
//    }

    //buf[numbytes] = '\0';
    close(sockfd);
    DEBUG("send_action finished!\n");
    return 0;
}

void send_to_inotify()
{
#if PLAN_B
    if(browse_sev != -1)
    {
        DEBUG("send to inotify:selecting serverlist!\n");
        return;
    }
#endif
    int i;

    for(i=0;i<ftp_config.dir_num;i++)
    {
        DEBUG("send_action base_path = %s\n",ftp_config.multrule[i]->base_path);
        send_action(ftp_config.id,ftp_config.multrule[i]->base_path,INOTIFY_PORT);
        usleep(1000*500);
    }
}

void read_config()
{
#ifdef NVRAM_
    if(convert_nvram_to_file_mutidir(CONFIG_PATH,&ftp_config) == -1)
    {
        DEBUG("convert_nvram_to_file fail\n");
        exit(-1);
    }
#else
#ifdef _PC
//    if(create_ftp_conf_file(&ftp_config) == -1)
//    {
//        DEBUG("create_ftp_conf_file fail\n");
//        exit(-1);
//    }
#else
    if(create_ftp_conf_file(&ftp_config) == -1)
    {
        DEBUG("create_ftp_conf_file fail\n");
        exit(-1);
    }
#endif
#endif
    DEBUG("#####read_config#####\n");
    parse_config(CONFIG_PATH,&ftp_config);
#ifdef _PC
    ftp_config.dir_num = 1;
#else
    //exit(-1);
    if(ftp_config.dir_num == 0)
    {
        no_config = 1;
        return;
    }
#endif
    while(no_config == 1 && exit_loop != 1 )
    {
        sleep(1);
    }

    //if(!no_config && browse_sev == -1)
    if(!no_config)
    {
        exit_loop = 0;
        if(get_tokenfile_info()==-1)
        {
            DEBUG("\nget_tokenfile_info failed\n");
            exit(-1);
        }
        DEBUG("get_tokenfile_info over\n");
        check_config_path(1);
    }
}

void init_global_var()
{
#if PLAN_B
    if(browse_sev != -1)
    {
        DEBUG("init global var:selecting serverlist!\n");
        return;
    }
#endif
    upload_break = 0;
    upload_file_lost = 0;
    del_acount = -1;

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex_socket, NULL);
    pthread_mutex_init(&mutex_socket_1, NULL);
    pthread_mutex_init(&mutex_receve_socket, NULL);
    pthread_mutex_init(&mutex_rename, NULL);

    int num = ftp_config.dir_num;
    g_pSyncList = (sync_list **)malloc(sizeof(sync_list *)*num);
    memset(g_pSyncList,0,sizeof(g_pSyncList));

    ServerNode_del = NULL;

    for(int i=0;i<num;i++)
    {
        g_pSyncList[i] = (sync_list *)malloc(sizeof(sync_list));
        memset(g_pSyncList[i],0,sizeof(sync_list));

        g_pSyncList[i]->index = i;
        g_pSyncList[i]->receve_socket = 0;
        g_pSyncList[i]->have_local_socket = 0;
        g_pSyncList[i]->first_sync = 1;
        g_pSyncList[i]->no_local_root = 0;
        g_pSyncList[i]->init_completed = 0;
        g_pSyncList[i]->IsNetWorkUnlink = 0;//2014.11.03 by sherry
        g_pSyncList[i]->IsPermission = 0;//2014.11.04 by sherry
        g_pSyncList[i]->ServerRootNode = NULL;
        g_pSyncList[i]->OldServerRootNode = NULL;
        g_pSyncList[i]->OrigServerRootNode = NULL;

        g_pSyncList[i]->sync_disk_exist = 0;
        //create head
        g_pSyncList[i]->SocketActionList = new Node;
        g_pSyncList[i]->SocketActionList->cmdName = NULL;
        g_pSyncList[i]->SocketActionList->next = NULL;

        sprintf(g_pSyncList[i]->temp_path,"%s/.smartsync/ftpclient",ftp_config.multrule[i]->mount_path);
        my_mkdir_r(g_pSyncList[i]->temp_path,i);

        char *base_path_tmp = my_str_malloc(sizeof(ftp_config.multrule[i]->base_path));
        sprintf(base_path_tmp,"%s",ftp_config.multrule[i]->base_path);
        replace_char_in_str(base_path_tmp,'_','/');
        snprintf(g_pSyncList[i]->up_item_file,255,"%s/%s_up_item",g_pSyncList[i]->temp_path,base_path_tmp);
        //snprintf(g_pSyncList[i]->conflict_item,255,"%s/conflict_%s",
                 //g_pSyncList[i]->temp_path,ftp_config.multrule[i]->base_folder + 1);
        free(base_path_tmp);

        g_pSyncList[i]->SocketActionList_Rename = new Node;
        g_pSyncList[i]->SocketActionList_Rename->cmdName = NULL;
        g_pSyncList[i]->SocketActionList_Rename->next = NULL;

        g_pSyncList[i]->server_action_list = create_action_item_head();
        g_pSyncList[i]->dragfolder_action_list = create_action_item_head();
        g_pSyncList[i]->unfinished_list = create_action_item_head();
        g_pSyncList[i]->up_space_not_enough_list = create_action_item_head();
        g_pSyncList[i]->copy_file_list = create_action_item_head();
        if(ftp_config.multrule[i]->rules == 1)
        {
            g_pSyncList[i]->download_only_socket_head = create_action_item_head();
        }

        tokenfile_info_tmp = tokenfile_info_start->next;
        while(tokenfile_info_tmp != NULL)
        {
            DEBUG("tokenfile_info_tmp->mountpath = %s\n",tokenfile_info_tmp->mountpath);
            DEBUG("ftp_config.multrule[i]->mount_path = %s\n",ftp_config.multrule[i]->mount_path);
            //if(!strcmp(tokenfile_info_tmp->mountpath,ftp_config.multrule[i]->mount_path) && browse_sev == -1)
            if(!strcmp(tokenfile_info_tmp->mountpath,ftp_config.multrule[i]->mount_path))
            {
                g_pSyncList[i]->sync_disk_exist = 1;
                break;
            }
            tokenfile_info_tmp = tokenfile_info_tmp->next;
        }
    }
}

/*used for initial,local syncfolder is NULL*/
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

        while(init_foldercurrent != NULL)
        {
            char *createpath = serverpath_to_localpath(init_foldercurrent->href,index);
            if(NULL == opendir(createpath))
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                if(-1 == mkdir(createpath,0777))
                {
                    DEBUG("mkdir %s fail",createpath);
                    exit(-1);
                }
                else
                {
                    add_action_item("createfolder",createpath,g_pSyncList[index]->server_action_list);
                }
            }
            free(createpath);
            init_foldercurrent = init_foldercurrent->next;
        }

        while(init_filecurrent != NULL)
        {
            if(is_local_space_enough(init_filecurrent,index))
            {
                char *createpath = serverpath_to_localpath(init_filecurrent->href,index);
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                add_action_item("createfile",createpath,g_pSyncList[index]->server_action_list);

                int status = download_(init_filecurrent->href,index);
                if(status == 0)
                {
                    DEBUG("do_cmd ok\n");
                    mod_time *onefiletime = Getmodtime(init_filecurrent->href,index);
                    if(ChangeFile_modtime(createpath,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                        return status;
                    }
                    free(onefiletime);
                }
                free(createpath);
            }
            else
            {
                write_log(S_ERROR,"local space is not enough!","",index);
                add_action_item("download",init_filecurrent->href,g_pSyncList[index]->unfinished_list);
            }
            init_filecurrent = init_filecurrent->next;
        }
    }

    if(servertreenode->Child != NULL)
    {
        res = initMyLocalFolder(servertreenode->Child,index);
        if(res != 0)
        {
            return res;
        }
    }

    if(servertreenode->NextBrother != NULL)
    {
        res = initMyLocalFolder(servertreenode->NextBrother,index);
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
        if(ftp_config.multrule[index]->rules != 1)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);
            ret = upload(localfiletmp->path,index);
            DEBUG("handle files,ret = %d\n",ret);
            if(ret == 0)
            {
                char *serverpath = localpath_to_serverpath(localfiletmp->path,index);
                //DEBUG("serverpath = %s\n",serverpath);
                mod_time *onefiletime = Getmodtime(serverpath,index);
                if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                {
                    DEBUG("ChangeFile_modtime failed!\n");
                }
                free(onefiletime);
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
            add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->download_only_socket_head);
        }

        localfiletmp = localfiletmp->next;
    }

    //handle folders
    while(localfoldertmp != NULL)
    {
        if(ftp_config.multrule[index]->rules != 1)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->server_action_list);
            ret = my_mkdir_(localfoldertmp->path,index);
            DEBUG("handle folders,ret = %d\n",ret);
            if(ret != 0)
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
            add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->download_only_socket_head);
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

    ret = is_server_have_localpath(path,g_pSyncList[index]->ServerRootNode->Child,index);

    if(ret == 0)
    {
        res = sync_local_to_server_init(localnode,index);
        if(res != 0  && res != LOCAL_FILE_LOST)//&& res != SERVER_SPACE_NOT_ENOUGH
        {
            DEBUG("##########res = %d\n",res);
            free_localfloor_node(localnode);
            return res;
        }
    }

    localfoldertmp = localnode->folderlist->next;
    while(localfoldertmp != NULL)
    {
        res = sync_local_to_server(localfoldertmp->path,index);
        if(res != 0 && res != LOCAL_FILE_LOST)// && res != SERVER_SPACE_NOT_ENOUGH
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

int sync_server_to_local_init(Browse *perform_br,Local *perform_lo,int index)
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

    if(perform_br->filenumber == 0 && perform_lo->filenumber != 0)
    {
        DEBUG("serverfileNo:%d\tlocalfileNo:%d\n",perform_br->filenumber,perform_lo->filenumber);
        while(localfiletmp != NULL && exit_loop == 0)
        {
            if(ftp_config.multrule[index]->rules != 1)
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }

                add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);
                ret = upload(localfiletmp->path,index);
                if(ret == 0)
                {
                    char *serverpath = localpath_to_serverpath(localfiletmp->path,index);
                    //DEBUG("serverpath = %s\n",serverpath);
                    mod_time *onefiletime = Getmodtime(serverpath,index);
                    free(serverpath);
                    if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                    free(onefiletime);
                }
                else
                {
                    //DEBUG("upload failed!\n");
                    return ret;
                }
            }
            else
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->download_only_socket_head);
            }
            localfiletmp = localfiletmp->next;
        }
    }
    else if(perform_br->filenumber != 0 && perform_lo->filenumber == 0)
    {
        DEBUG("serverfileNo:%d\tlocalfileNo:%d\n",perform_br->filenumber,perform_lo->filenumber);
        if(ftp_config.multrule[index]->rules != 2)
        {
            while(filetmp != NULL && exit_loop == 0)
            {
                int cp = 0;
                do{
                    if(exit_loop == 1)
                        return -1;
                    cp = is_ftp_file_copying(filetmp->href,index);
                }while(cp == 1);

                if(is_local_space_enough(filetmp,index))
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    char *localpath = serverpath_to_localpath(filetmp->href,index);
                    add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);

                    //wait_handle_socket(index);

                    ret = download_(filetmp->href,index);
                    if (ret == 0)
                    {
                        //ChangeFile_modtime(localpath,filetmp->modtime);
                        mod_time *onefiletime = Getmodtime(filetmp->href,index);
                        if(ChangeFile_modtime(localpath,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
                        free(localpath);
                    }
                    else
                    {
                        free(localpath);
                        return ret;
                    }
                }
                else
                {
                    write_log(S_ERROR,"local space is not enough!","",index);
                    add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                }
                filetmp = filetmp->next;
            }
        }
    }
    else if(perform_br->filenumber != 0 && perform_lo->filenumber != 0)
    {
        DEBUG("serverfileNo:%d\tlocalfileNo:%d\n",perform_br->filenumber,perform_lo->filenumber);
        while(localfiletmp != NULL && exit_loop == 0)
        {
            DEBUG("localfiletmp->path = %s\n",localfiletmp->path);
            int cmp = 1;
            char *localpathtmp = strstr(localfiletmp->path,ftp_config.multrule[index]->base_path) + ftp_config.multrule[index]->base_path_len;
            while(filetmp != NULL)
            {
                char *serverpathtmp;
                serverpathtmp = strstr(filetmp->href,ftp_config.multrule[index]->rooturl) + strlen(ftp_config.multrule[index]->rooturl);
                DEBUG("%s\t%s\n",localpathtmp,serverpathtmp);
                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                if(ftp_config.multrule[index]->rules != 1)
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }

                    add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);
                    ret = upload(localfiletmp->path,index);
                    if(ret == 0)
                    {
                        char *serverpath = localpath_to_serverpath(localfiletmp->path,index);
                        mod_time *onefiletime = Getmodtime(serverpath,index);
                        if(ChangeFile_modtime(localfiletmp->path,onefiletime->modtime,index))
                        {
                            DEBUG("ChangeFile_modtime failed!\n");
                        }
                        free(onefiletime);
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
                    add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->download_only_socket_head);
                }
            }
            else
            {
                if((ret = the_same_name_compare(localfiletmp,filetmp,index,0)) != 0)
                {
                    return ret;
                }
            }
            filetmp = perform_br->filelist->next;
            localfiletmp = localfiletmp->next;
        }


        filetmp = perform_br->filelist->next;
        localfiletmp = perform_lo->filelist->next;
        while(filetmp != NULL && exit_loop == 0)
        {
            int cmp = 1;
            char *serverpathtmp;
            serverpathtmp = strstr(filetmp->href,ftp_config.multrule[index]->rooturl) + strlen(ftp_config.multrule[index]->rooturl);
            //serverpathtmp = oauth_url_unescape(serverpathtmp,NULL);
            while(localfiletmp != NULL)
            {
                char *localpathtmp;
                localpathtmp = strstr(localfiletmp->path,ftp_config.multrule[index]->base_path) + ftp_config.multrule[index]->base_path_len;
                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                if(ftp_config.multrule[index]->rules != 2)
                {
                    int cp = 0;
                    do{
                        if(exit_loop == 1)
                            return -1;
                        cp = is_ftp_file_copying(filetmp->href,index);
                    }while(cp == 1);

                    if(is_local_space_enough(filetmp,index))
                    {
                        char *localpath = serverpath_to_localpath(filetmp->href,index);

                        if(wait_handle_socket(index))
                        {
                            return HAVE_LOCAL_SOCKET;
                        }

                        add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                        ret = download_(filetmp->href,index);

                        if (ret == 0)
                        {
                            mod_time *onefiletime = Getmodtime(filetmp->href,index);
                            if(ChangeFile_modtime(localpath,onefiletime->modtime,index))
                            {
                                DEBUG("ChangeFile_modtime failed!\n");
                            }
                            free(onefiletime);
                            free(localpath);
                        }
                        else
                        {
                            free(localpath);
                            //free(serverpathtmp);
                            return ret;
                        }
                    }
                    else
                    {
                        write_log(S_ERROR,"local space is not enough!","",index);
                        add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
            }
            //free(serverpathtmp);
            filetmp = filetmp->next;
            localfiletmp = perform_lo->filelist->next;
        }
    }

    if(perform_br->foldernumber == 0 && perform_lo->foldernumber != 0)
    {
        DEBUG("serverfolderNo:%d\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
        while(localfoldertmp != NULL && exit_loop == 0)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            if(ftp_config.multrule[index]->rules != 1)
            {
                add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->server_action_list);
                my_mkdir_(localfoldertmp->path,index);
            }
            else
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->download_only_socket_head);
            }
            localfoldertmp = localfoldertmp->next;
        }
    }
    else if(perform_br->foldernumber != 0 && perform_lo->foldernumber == 0)
    {
        DEBUG("serverfolderNo:%d\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
        if(ftp_config.multrule[index]->rules != 2)
        {
            while(foldertmp != NULL && exit_loop == 0)
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                char *localpath = serverpath_to_localpath(foldertmp->href,index);
                char *prePath = get_prepath(localpath,strlen(localpath));
                int exist = is_server_exist(prePath,localpath,index);
                if(exist)
                {
                    if(NULL == opendir(localpath))
                    {
                        add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                        mkdir(localpath,0777);
                    }
                }
                free(localpath);
                free(prePath);
                foldertmp = foldertmp->next;
            }
        }
    }
    else if(perform_br->foldernumber != 0 && perform_lo->foldernumber != 0)
    {
        DEBUG("serverfolderNo:%d\tlocalfolderNo:%d\n",perform_br->foldernumber,perform_lo->foldernumber);
        while(localfoldertmp != NULL && exit_loop == 0)
        {
            int cmp = 1;
            char *localpathtmp;
            localpathtmp = strstr(localfoldertmp->path,ftp_config.multrule[index]->base_path) + ftp_config.multrule[index]->base_path_len;
            while(foldertmp != NULL)
            {
                char *serverpathtmp;
                serverpathtmp = strstr(foldertmp->href,ftp_config.multrule[index]->rooturl) + strlen(ftp_config.multrule[index]->rooturl);
                DEBUG("%s\t%s\n",localpathtmp,serverpathtmp);
                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                if(ftp_config.multrule[index]->rules != 1)
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->server_action_list);
                    my_mkdir_(localfoldertmp->path,index);
                }
                else
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    add_action_item("createfolder",localfoldertmp->path,g_pSyncList[index]->download_only_socket_head);
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
            char *serverpathtmp;
            serverpathtmp = strstr(foldertmp->href,ftp_config.multrule[index]->rooturl) + strlen(ftp_config.multrule[index]->rooturl);
            while(localfoldertmp != NULL)
            {
                char *localpathtmp;
                localpathtmp = strstr(localfoldertmp->path,ftp_config.multrule[index]->base_path) + ftp_config.multrule[index]->base_path_len;
                if ((cmp = strcmp(localpathtmp,serverpathtmp)) == 0)
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
                if(ftp_config.multrule[index]->rules != 2)
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    char *localpath = serverpath_to_localpath(foldertmp->href,index);
                    char *prePath = get_prepath(localpath,strlen(localpath));
                    int exist = is_server_exist(prePath,localpath,index);
                    if(exist)
                    {
                        if(NULL == opendir(localpath))
                        {
                            add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                            mkdir(localpath,0777);
                        }
                    }
                    free(prePath);
                    free(localpath);
                }
            }
            //free(serverpathtmp);
            foldertmp = foldertmp->next;
            localfoldertmp = perform_lo->folderlist->next;
        }
    }

    return ret;
}

int parseCloudInfo_one(char *parentherf,int index)
{
    FILE *fp;
    fp = fopen(LIST_ONE_DIR,"r");
    char tmp[512]={0};
    char buf[512]={0};
    const char *split = " ";
    const char *split_2 = "\n";
    char *str,*temp;
    char *p;
    while(fgets(buf,sizeof(buf),fp)!=NULL)
    {
        int fail = 0;
        DEBUG("%s\n",buf);
        FolderTmp_one=(CloudFile *)malloc(sizeof(struct node));
        //memset(FolderTmp_one,0,sizeof(FolderTmp_one));
        memset(FolderTmp_one,0,sizeof(struct node));//2014.10.28 by sherry sizeof(指针)=4
        p=strtok(buf,split);
        int i=0;
        while(p != NULL && i <= 8)
        {
            switch(i)
            {
            case 0:
                strcpy(tmp,p);
                //strcpy(FolderTmp_one->auth,p);
                FolderTmp_one->isfile=is_file(tmp);
                break;
            case 4:
                FolderTmp_one->size=atoll(p);
                break;
            case 5:
                //strcpy(FolderTmp_one->month,p);
                break;
            case 6:
                //strcpy(FolderTmp_one->day,p);
                break;
            case 7:
                //strcpy(FolderTmp_one->lastmodifytime,p);
                break;
            case 8:
//                if(p == NULL || !strcmp(p,".") || !strcmp(p,".."))
//                {
//                    DEBUG("free iterm\n");
//                    fail = 1;
//                }
                str = (char*)malloc(sizeof(char)*(strlen(p) + 1));
                strcpy(str,p);
                temp = to_utf8(str,index);
                free(str);
                strcpy(FolderTmp_one->filename,temp);
                break;
            default:
                break;
            }
            i++;
            if(i<=7)
                p=strtok(NULL,split);
            else
                p=strtok(NULL,split_2);
        }
        free(temp);
        FolderTmp_one->href = (char *)malloc(sizeof(char)*(strlen(parentherf)+1+1+strlen(FolderTmp_one->filename)));
        //memset(FolderTmp_one->href,'\0',sizeof(FolderTmp_one->href));//2014.10.28 by sherry sizeof(指针)=4
        memset(FolderTmp_one->href,'\0',sizeof(char)*(strlen(parentherf)+1+1+strlen(FolderTmp_one->filename)));
        if(strcmp(parentherf," ")==0){
            sprintf(FolderTmp_one->href,"%s",FolderTmp_one->filename);
        }
        else{
            sprintf(FolderTmp_one->href,"%s/%s",parentherf,FolderTmp_one->filename);
        }
        //if(!fail)
        //{
            FileTail_one->next = FolderTmp_one;
            FileTail_one = FolderTmp_one;
            FileTail_one->next = NULL;
        //}
        //else
        //{
            //free_CloudFile_item(FolderTmp_one);
        //}
    }
    fclose(fp);
}

/*
 *1,server has the same file
 *0,server has no the same file
*/
int is_server_exist(char *parentpath,char *filepath,int index)
{
    DEBUG("####################is_server_exist...###############\n");
    char *url = NULL;
    char *file_url = NULL;
    int status;
    FileList_one = (CloudFile *)malloc(sizeof(CloudFile));
    memset(FileList_one,0,sizeof(CloudFile));

    FileList_one->href = NULL;
    FileTail_one = FileList_one;
    FileTail_one->next = NULL;

    //2014.09.18 by sherry
    //未考虑深层目录的情况，出现指针乱指的现象
//    if(!strcmp(parentpath,ftp_config.multrule[index]->hardware))
//    {// 如果传进的值就是hardware，那么走这里面的。

//        url = (char *)malloc(sizeof(char)*((strlen(parentpath) + 1)));
//        sprintf(url,"%s",parentpath);
//    }
//    else
//    {
//        char *p = parentpath;
//        p = p + ftp_config.multrule[index]->base_path_len;
//        url = (char *)malloc(sizeof(char)*((strlen(ftp_config.multrule[index]->rooturl)+strlen(p) + 1)));
//        sprintf(url,"%s%s",ftp_config.multrule[index]->rooturl,p);

//    }

    if(!strcmp(parentpath,ftp_config.multrule[index]->hardware))
    {// 如果传进的值就是hardware，那么走这里面的。

        url = (char *)malloc(sizeof(char)*((strlen(parentpath) + 1)));
        sprintf(url,"%s",parentpath);
    }
    else
    {
        if(!strcmp(parentpath,ftp_config.multrule[index]->base_path))
        {
            url = (char *)malloc(sizeof(char)*((strlen(ftp_config.multrule[index]->rooturl) + 1)));
            memset(url,0,sizeof(char)*((strlen(ftp_config.multrule[index]->rooturl) + 1)));
            sprintf(url,"%s",ftp_config.multrule[index]->rooturl);

        }
        else
        {
            char *p = parentpath;
            p = p + ftp_config.multrule[index]->base_path_len;
            url = (char *)malloc(sizeof(char)*((strlen(ftp_config.multrule[index]->rooturl)+strlen(p) + 1)));
            memset(url,0,sizeof(char)*((strlen(ftp_config.multrule[index]->rooturl)+strlen(p) + 1)));//2014.10.28,by sherry 未初始化
            sprintf(url,"%s%s",ftp_config.multrule[index]->rooturl,p);

        }
    }

    do
    {
        status = getCloudInfo_one(url,parseCloudInfo_one,index);

    }while(status == CURLE_COULDNT_CONNECT||status==CURLE_OPERATION_TIMEDOUT && exit_loop == 0);
    //while(status == CURLE_COULDNT_CONNECT && exit_loop == 0); //2014.11.07 by sherry

    if(status != 0)
    {
        free(url);
        free_CloudFile_item(FileList_one);
        FileList_one = NULL;
        DEBUG("get Cloud Info One ERROR! \n");
        //2014.11.07 by sherry
        //return status;
        return 0;
    }

    CloudFile *de_filecurrent;
    de_filecurrent = FileList_one->next;

    char *q = filepath;
    q = q + strlen(parentpath);
    file_url = (char*)malloc(sizeof(char)*(strlen(url) + strlen(q) + 1));
    memset(file_url,'\0',sizeof(char)*(strlen(url) + strlen(q) + 1));
    sprintf(file_url,"%s%s",url,q);
    DEBUG("file_url:%s\n",file_url);
    //DEBUG("filepath:%s\n",filepath);
    while(de_filecurrent != NULL)
    {
        if(de_filecurrent->href != NULL)
        {
            if(!(strcmp(de_filecurrent->href,file_url)))
            {
                DEBUG("get it.\n");
                free(url);
                free(file_url);
                free_CloudFile_item(FileList_one);
                return 1;
            }
        }
        de_filecurrent = de_filecurrent->next;
    }
    DEBUG("not exist.\n");
    free(url);
    free(file_url);
    free_CloudFile_item(FileList_one);
    return 0;
}

void clear_global_var()
{
    if(initial_tokenfile_info_data(&tokenfile_info_start) == NULL)
    {
        return;
    }
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

void sig_handler (int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGUSR2:
        if(signum == SIGUSR2)
        {
            DEBUG("signal is SIGUSR2\n");
            mountflag = 0;
            FILE *fp;
            fp = fopen("/proc/mounts","r");
            if(fp == NULL)
            {
                DEBUG("open /proc/mounts fail\n");
            }
            char a[1024];
            while(!feof(fp))//检测流上的文件结束符，如果文件结束，则返回非0值，否则返回0
            {//当fp没有结束时
                memset(a,'\0',1024);//对a数组进行清零操作
                fscanf(fp,"%[^\n]%*c",a);//将fp读入到a数组，
                DEBUG("\nmount = %s\n",a);
                if(strstr(a,"/dev/sd"))
                {
                    mountflag = 1;
                    break;
                }
            }
            fclose(fp);
        }
        DEBUG("mountflag = %d\n",mountflag);
        if(signum == SIGTERM || mountflag == 0)
        {
            //sync_up = 0;
            //sync_down = 0;
            stop_progress = 1;
            //write_to_lock(stop_progress);
            exit_loop = 1;

#ifndef NVRAM_
            system("sh /tmp/smartsync/ftpclient/script/ftpclient_get_nvram");
            sleep(2);
            if(create_ftp_conf_file(&ftp_config_stop) == -1)
            {
                DEBUG("create_webdav_conf_file fail\n");
                return;
            }
#else
            if(convert_nvram_to_file_mutidir(CONFIG_PATH,&ftp_config_stop) == -1)
            {
                DEBUG("convert_nvram_to_file fail\n");
                write_to_nvram("","ftp_tokenfile");   //the "" maybe cause errors
                return;
            }
#endif
            if(ftp_config_stop.dir_num == 0)
            {
                char *filename;
                del_acount = 0;
                filename = my_str_malloc(strlen(ftp_config.multrule[0]->mount_path) + 21 + 1);
                sprintf(filename,"%s/.ftpclient_tokenfile",ftp_config.multrule[0]->mount_path);
                remove(filename);
                free(filename);
#ifdef NVRAM_
                write_to_nvram("","ftp_tokenfile");   //the "" maybe cause errors
#else
                write_to_ftp_tokenfile("");
#endif
            }
            else
            {
                if(ftp_config_stop.dir_num != ftp_config.dir_num)
                {
                    DEBUG("ftp_config_stop.dir_num = %d\n",ftp_config_stop.dir_num);
                    DEBUG("ftp_config.dir_num = %d\n",ftp_config.dir_num);
                    parse_config(CONFIG_PATH,&ftp_config_stop);
                    //verify(ftp_config_stop);
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
            //pthread_mutex_lock(&mutex_checkdisk);
            disk_change = 1;
            //pthread_mutex_unlock(&mutex_checkdisk);
            sighandler_finished = 1;
        }
        break;
    case SIGUSR1:  // add user
        DEBUG("signal is SIGUSER1\n");
        click_apply = 1;
        break;
    default:
        break;
    }
}

void* sigmgr_thread(void *argc)
{
    DEBUG("\nsigmgr_thread:%u\n",pthread_self());
    sigset_t   waitset;
    int        sig;
    int        rc;

    pthread_t  ppid = pthread_self();
    pthread_detach(ppid);

    sigemptyset(&waitset);
    sigaddset(&waitset,SIGUSR1);
    sigaddset(&waitset,SIGUSR2);
    sigaddset(&waitset,SIGTERM);

    while(1)
    {
        rc = sigwait(&waitset, &sig);
        if (rc != -1)
        {
            DEBUG("sigwait() fetch the signal - %d\n", sig);
            sig_handler(sig);
        }else{
            DEBUG("sigwaitinfo() returned err: %d; %s\n", errno, strerror(errno));
        }
    }
}

long long stat_file(const char *filename)
{
    //unsigned long size;
    struct stat filestat;
    if( stat(filename,&filestat) == -1)
    {
        perror("stat:");
        return -1;
        //exit(1);
    }
    return  filestat.st_size;

    //return size;

}

void handle_quit_upload()//同一帐号，继续上传未上传完的文件
{

    DEBUG("###handle_quit_upload###\n");
    int status;
    int i;
    for(i = 0; i<ftp_config.dir_num; i++)
    {
            status = usr_auth(ftp_config.multrule[i]->server_ip,ftp_config.multrule[i]->user_pwd);
            if(status == 7)//status=7表示couldn't connect to server
            {
                    write_log(S_ERROR,"check your ip.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            else if(status == 67)
            {
                    write_log(S_ERROR,"check your usrname or password.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            else if(status == 28)
            {
                    write_log(S_ERROR,"connect timeout,check your ip or network.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            int flag = is_server_exist(ftp_config.multrule[i]->hardware, ftp_config.multrule[i]->rooturl, i);
            if(flag == CURLE_FTP_COULDNT_RETR_FILE)
            {
                    write_log(S_ERROR,"Permission Denied.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            else if(flag == 0)
            {
                    //write_log(S_ERROR,"remote root path lost OR write only.","",ftp_config.multrule[i]->rules);
                    //continue;
            }
//#ifndef NVRAM_
        if(g_pSyncList[i]->sync_disk_exist == 0)
            continue;
//#endif
        if(ftp_config.multrule[i]->rules != 1)//不是两条规则时
        {
            FILE *fp;
            long long filesize;
            char *buf;
            char *filepath;
            DEBUG("g_pSyncList[i]->up_item_file = %s\n",g_pSyncList[i]->up_item_file);
            if(access(g_pSyncList[i]->up_item_file,0) == 0)
            {
                filesize = stat_file(g_pSyncList[i]->up_item_file);
                fp = fopen(g_pSyncList[i]->up_item_file,"r");
                if(fp == NULL)
                {
                    DEBUG("open %s error\n",g_pSyncList[i]->up_item_file);
                    return;
                }
                buf = my_str_malloc((size_t)(filesize + 1));
                fscanf(fp,"%s",buf);
                fclose(fp);

                if((strlen(buf)) <= 1)
                {
                    return ;
                }
                DEBUG("buf:%s\n",buf);
                unlink(g_pSyncList[i]->up_item_file);

                //filepath = my_str_malloc((size_t)(filesize + ftp_config.multrule[i]->base_path_len + strlen(buf) + 1));
                //sprintf(filepath,"%s%s",ftp_config.multrule[i]->base_path,buf);
                //free(buf);
                //DEBUG("buf:%s\n",buf);
                Delete(buf,i);
                status = upload(buf,i);

                if(status != 0)
                {
                    DEBUG("upload %s failed\n",buf);
                }
                else
                {
                    char *serverpath = localpath_to_serverpath(buf,i);
                    DEBUG("serverpath = %s\n",serverpath);
                    mod_time *onefiletime = Getmodtime(serverpath,i);
                    if(ChangeFile_modtime(buf,onefiletime->modtime,i))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                    free(serverpath);
                }

                free(buf);
            }
        }
    }

}

int initialization()
{
    int ret = 0;
    int status;
    int i;
    DEBUG("############################ftp_config.dir_num = %d\n",ftp_config.dir_num);
    for(i = 0; i < ftp_config.dir_num; i++)
    {
            if(exit_loop)
                break;
            if(disk_change)
            {
                //disk_change = 0;
                check_disk_change();
            }

            status = usr_auth(ftp_config.multrule[i]->server_ip,ftp_config.multrule[i]->user_pwd);
            if(status == 7)
            {
                    write_log(S_ERROR,"check your ip.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            else if(status == 67)
            {
                    write_log(S_ERROR,"check your usrname or password.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            else if(status == 28)
            {
                    write_log(S_ERROR,"connect timeout,check your ip or network.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            int flag = is_server_exist(ftp_config.multrule[i]->hardware, ftp_config.multrule[i]->rooturl, i);
            if(flag == CURLE_FTP_COULDNT_RETR_FILE)
            {
                    write_log(S_ERROR,"Permission Denied.","",ftp_config.multrule[i]->rules);
                    continue;
            }
            else if(flag == 0)
            {
                    //write_log(S_ERROR,"remote root path lost OR write only.","",ftp_config.multrule[i]->rules);
                    //continue;
            }

        if(g_pSyncList[i]->sync_disk_exist == 0)
        {
            write_log(S_ERROR,"Sync disk unplug!","",i);
            continue;
        }

        if(g_pSyncList[i]->init_completed)
            continue;

        //2104.11.04 by sherry server权限不足,error,finish,initial三种状态交替
        write_log(S_INITIAL,"","",i);
//        if(!g_pSyncList[i]->IsPermission && !exit_loop)//有权限
//        {
//            write_log(S_INITIAL,"","",i);
//        }
        ret = 1;
        DEBUG("it is %d init \n",i);

        if(exit_loop == 0)
        {
            DEBUG("wd_initial ret = %d\n",ret);
            free_server_tree(g_pSyncList[i]->ServerRootNode);
            g_pSyncList[i]->ServerRootNode = NULL;

            g_pSyncList[i]->ServerRootNode = create_server_treeroot();
            //DEBUG("%s\n",ftp_config.multrule[i]->rooturl);
            status = browse_to_tree(ftp_config.multrule[i]->rooturl,g_pSyncList[i]->ServerRootNode,i);
            DEBUG("%d\n",status);

            if(status != 0)
                continue;

            if(exit_loop == 0)
            {
                if(test_if_dir_empty(ftp_config.multrule[i]->base_path))
                {
                    DEBUG("base_path is blank\n");

                    if(ftp_config.multrule[i]->rules != 2)
                    {
                        if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                        {
                            DEBUG("######## init sync folder,please wait......#######\n");

                            ret = initMyLocalFolder(g_pSyncList[i]->ServerRootNode->Child,i);
                            printf("ret1 = %d\n", ret);
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
                    DEBUG("######## init sync folder(have files),please wait......#######\n");
                    if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                    {
                        DEBUG("########sync_server_to_local\n");
                        ret = sync_server_to_local(g_pSyncList[i]->ServerRootNode->Child,sync_server_to_local_init,i);
                        printf("ret2 = %d\n", ret);
                        if(ret != 0)
                                continue;
                    }
                    ret = sync_local_to_server(ftp_config.multrule[i]->base_path,i);
                    printf("ret3 = %d\n", ret);
                    if(ret != 0)
                            continue;
                    DEBUG("#########ret = %d\n",ret);
                    DEBUG("######## init sync folder end#######\n");
                    g_pSyncList[i]->init_completed = 1;
                    g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                }
            }
        }
        if(ret == 0)
        {
            write_log(S_SYNC,"","",i);
        }
    }
    return ret;
}

void run()
{
    //init_global_var();
    send_to_inotify();
    handle_quit_upload();//处理上次上传中断的动作（估计是U盘拔掉啥的,程序挂掉）

    int create_thid1 = 0;
    if(exit_loop == 0)
    {
        if(pthread_create(&newthid1,NULL,Save_Socket,NULL) != 0)
        {
            DEBUG("thread creation failed!\n");
            exit(1);
        }
        usleep(1000*500);
        create_thid1 = 1;
    }

    int create_thid2 = 0;
    //if(exit_loop == 0 && browse_sev == -1)
    if(exit_loop == 0)
    {
        if(pthread_create(&newthid2,NULL,Socket_Parser,NULL) != 0)
        {
            DEBUG("thread creation failed!\n");
            exit(1);
        }
        usleep(1000*500);
        create_thid2 = 1;
    }
    //DEBUG("!!!!!!!!!!!!! start initialization()\n");
   //by
    initialization();
   // DEBUG("!!!!!!!!!!!!! end initialization()\n");
    int create_thid3 = 0;
    //if(exit_loop == 0 && browse_sev == -1)

//by
    if(exit_loop == 0)
    {
        if(pthread_create(&newthid3,NULL,SyncServer,NULL) != 0)
        {
            DEBUG("thread creation failed!\n");
            exit(1);
        }
        //usleep(1000*500);
        create_thid3 = 1;
    }

    if(create_thid1)
    {
        pthread_join(newthid1,NULL);
        DEBUG("newthid1 has stoped!\n");
    }

    if(create_thid2)
    {
        pthread_join(newthid2,NULL);
        DEBUG("newthid2 has stoped!\n");
    }

    if(create_thid3)
    {
        pthread_join(newthid3,NULL);
        DEBUG("newthid3 has stoped!\n");
    }

    usleep(1000);
    //clean_up();

    if(stop_progress != 1)
    {
        DEBUG("run again!\n");
        DEBUG("disk_change:%d\n",disk_change);
        while(disk_change)
        {
            disk_change = 0;
            sync_disk_removed = check_sync_disk_removed();

            if(sync_disk_removed == 2)
            {
                DEBUG("sync path is change\n");
            }
            else if(sync_disk_removed == 1)
            {
                DEBUG("sync disk is unmount\n");
            }
            else if(sync_disk_removed == 0)
            {
                DEBUG("sync disk exists\n");
            }
        }

        exit_loop = 0;
        init_global_var();
        run();
    }

}


void stop_process_clean_up()
{
    //unlink("/tmp/smartsync_app/ftpclient_start");

    unlink("/tmp/notify/usb/ftp_client");
    pthread_cond_destroy(&cond);
    pthread_cond_destroy(&cond_socket);
    pthread_cond_destroy(&cond_log);
    unlink(LOG_DIR);
}

//int create_xml(int status)
//{
//    char buf1[512] = {0};
//    char buf2[512] = {0};
//    char LOG[512] = {0};
//
//    FILE *stream = fopen(SERVERLIST_TD,"a");
//    if(!access(SERVERLIST_LOG,F_OK))
//    {
//        FILE *my_fp = fopen(SERVERLIST_LOG,"r");
//        if(fgets(LOG,sizeof(LOG),my_fp) != NULL)
//        {
//            if(LOG[ strlen(LOG)-1 ] == '\n')
//                LOG[ strlen(LOG)-1 ] = '\0';
//        }
//        fprintf(stream,"<root stat=\"%s\"></root>",LOG);
//        fclose(stream);
//        rename(SERVERLIST_TD,SERVERLIST_XML);
//        return 0;
//    }
////    if(status == -1)
////    {
////        fprintf(stream,"<root stat=\"Error\"></root>");
////        fclose(stream);
////        rename(SERVERLIST_TD,SERVERLIST_XML);
////        return 0;
////    }
//    FILE *fp1 = fopen(SERVERLIST_1,"r");
//    fprintf(stream,"<root stat=\"OK\">");
//    while(fgets(buf1,sizeof(buf1),fp1) != NULL)
//    {
//        if(buf1[ strlen(buf1)-1 ] == '\n')
//            buf1[ strlen(buf1)-1 ] = '\0';
//        char *temp1 = (char*)malloc(sizeof(char)*(strlen(buf1) + 1));
//        memset(temp1,'\0',sizeof(char)*(strlen(buf1) + 1));
//        sprintf(temp1,"%s",buf1);
//        char *p = temp1 + strlen(temp1) - 1;
//        int i = 0;
//        while(p[0] != '/' && i < (strlen(temp1)-1))
//        {
//            i++;
//            p--;
//        }
//        p = (p[0] == '/') ? (p + 1) : p;
//        fprintf(stream,"<fold0 name=\"%s\">",p);
//        FILE *fp2 = fopen(SERVERLIST_2,"r");
//        while(fgets(buf2,sizeof(buf2),fp2) != NULL)
//        {
//            if(buf2[ strlen(buf2)-1 ] == '\n')
//                buf2[ strlen(buf2)-1 ] = '\0';
//            char *temp2 = (char*)malloc(sizeof(char)*(strlen(buf2) + 1));
//            memset(temp2,'\0',sizeof(char)*(strlen(buf2) + 1));
//            sprintf(temp2,"%s",buf2);
//            if(strncmp(buf2,buf1,strlen(buf1)) == 0)
//            {
//                fprintf(stream,"<fold1 name=\"%s\">",temp2 + strlen(buf1) + 1);
//                fprintf(stream,"</fold1>");
//            }
//            free(temp2);
//        }
//        fclose(fp2);
//        fprintf(stream,"</fold0>");
//        free(temp1);
//    }
//    fprintf(stream,"</root>");
//    fclose(fp1);
//    fclose(stream);
//    rename(SERVERLIST_TD,SERVERLIST_XML);
//    return 0;
//}

char *program_name;
char *login;

int main(int argc,char **argv)
{
    program_name=argv[0];

    //create_start_file();
    no_config = 0;
    exit_loop = 0;
    stop_progress = 0;

    my_local_mkdir("/tmp/smartsync");
    my_local_mkdir("/tmp/smartsync/.logs");
    my_local_mkdir("/tmp/smartsync/ftpclient");
    my_local_mkdir("/tmp/smartsync/ftpclient/config");
    my_local_mkdir("/tmp/smartsync/ftpclient/script");
    my_local_mkdir("/tmp/smartsync/ftpclient/temp");

    sigset_t bset,oset;
    pthread_t sig_thread;

    sigemptyset(&bset);//用来将参数bset信号集初始化并清空
    sigaddset(&bset,SIGUSR1);//用来将参数"SIGUSR1(此为信号种类)"信号加入至参数bset信号集里
    sigaddset(&bset,SIGUSR2);
    sigaddset(&bset,SIGTERM);

    if( pthread_sigmask(SIG_BLOCK,&bset,&oset) == -1)//在多线程的程序里，希望只在主线程中处理信号，
                                                //用作在主调线程里控制信号掩码
        DEBUG("!! Set pthread mask failed\n");

    if( pthread_create(&sig_thread,NULL,sigmgr_thread,NULL) != 0)//创建信号的线程
    {
        DEBUG("thread creation failder\n");
        exit(-1);
    }
    clear_global_var();

#ifndef _PC
    write_notify_file(NOTIFY_PATH,SIGUSR2);

    while(detect_process("nvram"))
    {
        sleep(2);
    }
    create_shell_file();
#ifndef NVRAM_
    my_local_mkdir("/opt/etc/.smartsync");
    write_get_nvram_script("cloud_sync",GET_NVRAM,TMP_CONFIG);//
    system("sh /tmp/smartsync/ftpclient/script/ftpclient_get_nvram");
    sleep(2);
    write_get_nvram_script("link_internet",GET_INTERNET,VAL_INTERNET);//
#endif
#endif

    read_config();
    //DEBUG("browse_sev:%d\n",browse_sev);
    init_global_var();

#ifdef _PC

#else
    check_link_internet();
#endif
    if(no_config == 0)
        run();

    pthread_join(sig_thread,NULL);
    stop_process_clean_up();


//    if(detect_process_file())
//    {
//        DEBUG("ftpclient did not kill inotify\n");
//    }
//    else
//    {
//        system("killall  -9 inotify &");
//        DEBUG("ftpclient kill inotify\n");
//    }
//    if(!detect_process("asuswebstorage") && !detect_process("webdav_client") && !detect_process("dropbox_client"))
//    {
//        system("killall  -9 inotify &");
//        DEBUG("ftpclient kill inotify\n");
//    }
//    else
//    {
//        DEBUG("ftpclient did not kill inotify\n");
//    }
    DEBUG("FtpClient stop.\n");
    return 0;
}
