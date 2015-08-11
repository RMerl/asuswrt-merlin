#include "base.h"

extern string ftp_url;
extern Config ftp_config,ftp_config_stop;
extern Server_TreeNode *ServerNode_del;
extern sync_list **g_pSyncList;
extern _info LOCAL_FILE,SERVER_FILE;
extern int upload_break;
extern int upload_file_lost;
extern int del_acount;

int code_convert(char *from_charset,char *to_charset,char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
    iconv_t cd;
    char **pin = &inbuf;
    char **pout = &outbuf;

    cd = iconv_open(to_charset,from_charset);

    if(cd == (iconv_t)-1)
        return -1;
    memset(outbuf,0,outlen);

    if(iconv(cd,pin,&inlen,pout,&outlen) == -1)
        return -1;

    iconv_close(cd);
    return 0;
}

char *to_utf8(char *buf,int index)
{
    int status = 0;
    char *out = my_str_malloc(256);
    if(ftp_config.multrule[index]->encoding == 0)
    {
        sprintf(out,"%s",buf);
        return out;
    }
    else if(ftp_config.multrule[index]->encoding == 1)
    {
        DEBUG("converting from GB18030 to utf-8...%s\n",buf);
        status = code_convert("GB18030","utf-8",buf,strlen(buf),out,OUTLEN);
        if(status == 0)
        {
            DEBUG("convert ok\n");
            return out;
        }else{
            DEBUG("convert failed\n");
            sprintf(out,"%s",buf);
            return out;
        }
    }
    else if(ftp_config.multrule[index]->encoding == 2)
    {
        DEBUG("converting from BIG5 to utf-8...%s\n",buf);
        status = code_convert("BIG5","utf-8",buf,strlen(buf),out,OUTLEN);
        if(status == 0)
        {
            DEBUG("convert ok\n");
            return out;
        }else{
            DEBUG("convert failed\n");
            sprintf(out,"%s",buf);
            return out;
        }
    }
}

char *utf8_to(char *buf,int index)
{
    int status = 0;
    char *out = my_str_malloc(256);
    if(ftp_config.multrule[index]->encoding == 0)
    {
        sprintf(out,"%s",buf);
        return out;
    }
    else if(ftp_config.multrule[index]->encoding == 1)
    {
        DEBUG("converting from utf-8 to GB18030...%s\n",buf);
        status = code_convert("utf-8","GB18030",buf,strlen(buf),out,OUTLEN);
        if(status == 0)
        {
            DEBUG("convert ok\n");
            return out;
        }else{
            DEBUG("convert failed\n");
            sprintf(out,"%s",buf);
            return out;
        }
    }
    else if(ftp_config.multrule[index]->encoding == 2)
    {
        DEBUG("converting from utf-8 to BIG5...%s\n",buf);
        status = code_convert("utf-8","BIG5",buf,strlen(buf),out,OUTLEN);
        if(status == 0)
        {
            DEBUG("convert ok\n");
            return out;
        }else{
            DEBUG("convert failed\n");
            sprintf(out,"%s",buf);
            return out;
        }
    }
}

int download_(char *serverpath,int index)
{
    DEBUG("%s\n",serverpath);
    char *temp = serverpath_to_localpath(serverpath,index);
    char *where_it_put = (char*)malloc(sizeof(char)*(strlen(temp) + 9));
    memset(where_it_put,'\0',sizeof(char)*(strlen(temp) + 9));
    sprintf(where_it_put,"%s%s",temp,".asus.td");

    char *chr_coding = utf8_to(serverpath,index);
    char *url_coding = oauth_url_escape(chr_coding);
    char *where_it_from = (char*)malloc(sizeof(char)*(ftp_config.multrule[index]->server_ip_len + strlen(url_coding) + 2));
    memset(where_it_from,'\0',sizeof(char)*(ftp_config.multrule[index]->server_ip_len + strlen(url_coding) + 2));
    sprintf(where_it_from,"%s/%s",ftp_config.multrule[index]->server_ip,url_coding);
    DEBUG("from:%s\n",where_it_from);
    DEBUG("put:%s\n",where_it_put);
    free(url_coding);

    write_log(S_DOWNLOAD,"",serverpath,index);

#if PROGRESSBAR
    char *progress_data = "*";
#endif
    CURL *curl;
    CURLcode res;

    FILE *fp;
    curl_off_t local_file_len = -1;
    struct stat info;
    int use_resume = 0;
    if(stat(where_it_put,&info) == 0)
    {
        local_file_len = info.st_size;
        use_resume = 1;
    }
    fp = fopen(where_it_put,"ab+");
    if(fp == NULL)
    {
        return -1;
    }

    if(SERVER_FILE.path != NULL)
        free(SERVER_FILE.path);
    SERVER_FILE.path = (char*)malloc(sizeof(char)*(strlen(temp) + 1));
    sprintf(SERVER_FILE.path,"%s",temp);
    SERVER_FILE.index = index;

    curl = curl_easy_init();
    if(curl)
    {
#if PROGRESSBAR
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,my_progress_func);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, progress_data);
#endif
        curl_easy_setopt(curl, CURLOPT_URL, where_it_from);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, use_resume?local_file_len:0);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_write_func);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl);
        DEBUG("\nres:%d\n",res);
        if(res != CURLE_OK && res != CURLE_PARTIAL_FILE && res != CURLE_OPERATION_TIMEDOUT)
        {
            fclose(fp);
            free(temp);
            free(where_it_from);
            write_log(S_ERROR,"Download failed","",index);
            curl_easy_cleanup(curl);
            if(del_acount == index)
                unlink(where_it_put);
            free(where_it_put);
            return -1;
        }
        else
        {
            fclose(fp);
            DEBUG("rename .asus.td to filename.\n");
            rename(where_it_put,temp);
            free(temp);
            free(where_it_put);
            free(where_it_from);
            curl_easy_cleanup(curl);
            return 0;
        }
    }
    else
    {
        fclose(fp);
        free(temp);
        free(where_it_put);
        free(where_it_from);
        return 0;
    }
}

int upload(char *localpath1,int index)
{
#if PROGRESSBAR
    char *progress_data = "*";
#endif
    if(access(localpath1,F_OK) != 0)
    {
        DEBUG("Local has no %s\n",localpath1);
        return LOCAL_FILE_LOST;//902
    }
    write_log(S_UPLOAD,"",localpath1,index);

    char *serverpath = localpath_to_serverpath(localpath1,index);
    char *chr_coding = utf8_to(serverpath,index);
    free(serverpath);
    char *url_coding = oauth_url_escape(chr_coding);

    char *fullserverpath = (char*)malloc(sizeof(char)*(ftp_config.multrule[index]->server_ip_len + strlen(url_coding) + 2));
    memset(fullserverpath,'\0',sizeof(char)*(ftp_config.multrule[index]->server_ip_len + strlen(url_coding) + 2));
    sprintf(fullserverpath,"%s/%s",ftp_config.multrule[index]->server_ip,url_coding);

    free(url_coding);
    CURL *curl;
    CURLcode res;
    struct stat info;
    if(stat(localpath1,&info) == -1)
        return LOCAL_FILE_LOST;//902

    FILE *g_stream,*fp;
    g_stream = fopen(localpath1,"rb");
    if(g_stream == NULL)
        return LOCAL_FILE_LOST;//902

    fp = fopen(CURL_HEAD,"wb");

    if(LOCAL_FILE.path != NULL)
        free(LOCAL_FILE.path);
    LOCAL_FILE.path = (char*)malloc(sizeof(char)*(strlen(localpath1) + 1));
     //2014.10.29 by sherry 未初始化
    memset(LOCAL_FILE.path,'\0',sizeof(char)*(strlen(localpath1) + 1));
    sprintf(LOCAL_FILE.path,"%s",localpath1);
    LOCAL_FILE.index = index;

    curl = curl_easy_init();
    if(curl)
    {
#if PROGRESSBAR
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,my_progress_func);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, progress_data);
#endif
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)info.st_size);
        curl_easy_setopt(curl, CURLOPT_URL, fullserverpath);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, my_read_func);
        curl_easy_setopt(curl, CURLOPT_READDATA, g_stream);
        curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fp);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
        //curl_easy_setopt(curl, CURLOPT_FTPPORT, "-");
        //curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);//设置一个长整形数，作为最大延续多少秒
        //curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debugFun);
        //curl_easy_setopt(curl, CURLOPT_DEBUGDATA, debugFile);
        res = curl_easy_perform(curl);
        DEBUG("\nres = %d\n",res);
        free(fullserverpath);
        //2014.11.06 by sherry
        //if(res != CURLE_OK && res != CURLE_OPERATION_TIMEDOUT)
        if(res != CURLE_OK)
        {
            curl_easy_cleanup(curl);
            fclose(fp);
            fclose(g_stream);
            if(upload_break == 1)//程序退出
            {
                FILE *f_stream = fopen(g_pSyncList[index]->up_item_file, "w");
                if(f_stream == NULL)
                {
                    DEBUG("open %s error\n", g_pSyncList[index]->up_item_file);
                    return res;
                }
                fprintf(f_stream,"%s",localpath1);
                fclose(f_stream);
                upload_break = 0;
                return res;
            }
            if(upload_file_lost == 1)//上传的文件丢失
            {
                upload_file_lost = 0;
                return 0;
            }
            //2014.11.07 by sherry
            if(res==55)//CURLE_SEND_ERROR
            {
               Delete(localpath1,index);
               action_item *item;
               item = get_action_item("upload",localpath1,g_pSyncList[index]->unfinished_list,index);
               if(item == NULL)
               {
                   add_action_item("upload",localpath1,g_pSyncList[index]->unfinished_list);
               }
               write_log(S_ERROR,"failed sending network data","",index);
               return CURLE_SEND_ERROR;
            }
            if(res==28)//CURLE_OPERATION_TIMEDOUT
            {
               Delete(localpath1,index);
               action_item *item;
               item = get_action_item("upload",localpath1,g_pSyncList[index]->unfinished_list,index);
               if(item == NULL)
               {
                   add_action_item("upload",localpath1,g_pSyncList[index]->unfinished_list);
               }
               write_log(S_ERROR,"the timeout time was reached","",index);
               return CURLE_OPERATION_TIMEDOUT;
            }

            FILE *fp1 = fopen(CURL_HEAD,"rb");
            char buff[512]={0};
            char *p;
            while(fgets(buff,sizeof(buff),fp1) != NULL)
            {
                p = strstr(buff,"Permission denied");
                if(p != NULL)
                {
                    write_log(S_ERROR,"Permission Denied!","",index);
                    fclose(fp1);
                    return PERMISSION_DENIED;
                }
            }
            fclose(fp1);
            return -1;
        }
        else
        {
            DEBUG("uploading ok!\n");
            del_action_item("upload",localpath1,g_pSyncList[index]->unfinished_list);
            curl_easy_cleanup(curl);
            fclose(fp);
            fclose(g_stream);
            return 0;
        }
    }
    else
    {
        fclose(fp);
        free(fullserverpath);
        return 0;
    }
}

int my_delete_DELE(char *localpath,int index)
{
    int status;
    char *tmp = get_prepath(localpath,strlen(localpath));
    status = is_server_exist(tmp,localpath,index);
    free(tmp);
    if(status)
    {
        char *serverpath = localpath_to_serverpath(localpath,index);
        char *command = (char *)malloc(sizeof(char)*(strlen(serverpath) + 6));
        memset(command,'\0',sizeof(command));
        sprintf(command,"DELE %s",serverpath);
        DEBUG("%s\n",command);
        char *temp = utf8_to(command,index);
        free(command);
        FILE *fp = fopen(CURL_HEAD,"w");
        CURL *curl;
        CURLcode res;
        curl = curl_easy_init();
        if(curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
            if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
                curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,temp);
            curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fp);
            curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
            curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
            res = curl_easy_perform(curl);
            DEBUG("\nres = %d\n",res);
            if(res != CURLE_OK)
            {
                curl_easy_cleanup(curl);
                free(temp);
                fclose(fp);
                free(serverpath);
                FILE *fp1 = fopen(CURL_HEAD,"r");
                char buff[512]={0};
                const char *split=" ";
                char *p;
                while(fgets(buff,sizeof(buff),fp1) != NULL)
                {
                    p=strtok(buff,split);
                    if(atoi(p) == 250)
                    {
                        DEBUG("delete file ok!\n");
                        fclose(fp1);
                        return 0;
                    }
                }
                fclose(fp1);
                return -1;
            }
            else
            {
                curl_easy_cleanup(curl);
                free(temp);//free(command);
                fclose(fp);
                free(serverpath);
                return 0;
            }
        }
        else
        {
            free(temp);//free(command);
            free(serverpath);
            fclose(fp);
            return 0;
        }
    }
    else{
        return 0;
    }
}
int my_delete_RMD(char *localpath,int index)
{
    int status;
    char *tmp = get_prepath(localpath,strlen(localpath));
    status = is_server_exist(tmp,localpath,index);
    free(tmp);
    if(status)
    {
        char *serverpath = localpath_to_serverpath(localpath,index);
        char *command = (char *)malloc(sizeof(char)*(strlen(serverpath) + 5));
        memset(command,'\0',sizeof(command));
        sprintf(command,"RMD %s",serverpath);
        DEBUG("%s\n",command);
        char *temp = utf8_to(command,index);
        free(command);
        FILE *fp = fopen(CURL_HEAD,"w");
        CURL *curl;
        CURLcode res;
        curl = curl_easy_init();
        if(curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
            if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
                curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,temp);
            curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fp);
            curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
            curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
            res = curl_easy_perform(curl);
            DEBUG("\nres = %d\n",res);
            if(res != CURLE_OK)
            {
                curl_easy_cleanup(curl);
                free(temp);//free(command);
                fclose(fp);
                free(serverpath);
                FILE *fp1 = fopen(CURL_HEAD,"r");
                char buff[512]={0};
                const char *split=" ";
                char *p;
                while(fgets(buff,sizeof(buff),fp1) != NULL)
                {
                    p = strstr(buff,"Permission denied");
                    if(p != NULL)
                    {
                        write_log(S_ERROR,"Permission denied","",index);
                        fclose(fp1);
                        return PERMISSION_DENIED;
                    }
                    p=strtok(buff,split);
                    if(atoi(p) == 250)
                    {
                        DEBUG("delete folder ok!\n");
                        fclose(fp1);
                        return 0;
                    }
                }
                return -1;
            }
            else
            {
                curl_easy_cleanup(curl);
                free(temp);//free(command);
                fclose(fp);
                free(serverpath);
                return 0;
            }
        }
        else
        {
            free(temp);//free(command);
            fclose(fp);
            free(serverpath);
            curl_easy_cleanup(curl);
            return 0;
        }
    }
    else
    {
        return 0;
    }
}
void SearchServerTree_(Server_TreeNode* treeRoot,int index)
{
    DEBUG("#################### SsarchServerTree ###################\n");
    int i;
    //int j;
    for(i=0;i<treeRoot->level;i++)
        DEBUG("-");

    if((treeRoot->Child!=NULL))
        SearchServerTree_(treeRoot->Child,index);

    if(treeRoot->NextBrother != NULL)
        SearchServerTree_(treeRoot->NextBrother,index);

    if(treeRoot->browse != NULL)
    {
        CloudFile *de_foldercurrent,*de_filecurrent;
        de_foldercurrent = treeRoot->browse->folderlist->next;
        de_filecurrent = treeRoot->browse->filelist->next;
        while(de_foldercurrent != NULL)
        {
            //DEBUG("serverfolder->href = %s\n",de_foldercurrent->href);
            char *temp = serverpath_to_localpath(de_foldercurrent->href,index);
            my_delete_RMD(temp,index);
            free(temp);
            de_foldercurrent = de_foldercurrent->next;
        }
        while(de_filecurrent != NULL)
        {
            //DEBUG("serverfile->href = %s\n",de_filecurrent->href);
            char *temp = serverpath_to_localpath(de_filecurrent->href,index);
            my_delete_DELE(temp,index);
            free(temp);
            de_filecurrent = de_filecurrent->next;
        }
    }
}
int Delete(char *localpath,int index)
{
    int flag = my_delete_DELE(localpath,index);
    if(flag != 0)
    {
        char *serverpath = localpath_to_serverpath(localpath,index);
        ServerNode_del = create_server_treeroot();
        browse_to_tree(serverpath,ServerNode_del,index);
        free(serverpath);
        SearchServerTree_(ServerNode_del,index);
        free_server_tree(ServerNode_del);
        flag = my_delete_RMD(localpath,index);
    }
    //my_delete_RMD(localpath,index);
    return flag;
}

int my_rename_(char *localpath,char *newername,int index)
{
    char *prepath = get_prepath(localpath,strlen(localpath));
    char *oldname = strstr(localpath,prepath) + strlen(prepath);
    char *cd_path = localpath_to_serverpath(prepath,index);

    char *str1 = my_str_malloc(strlen(cd_path) + 5);
    char *str2 = my_str_malloc(strlen(oldname) + 6);
    char *str3 = my_str_malloc(strlen(newername) + 6);

    sprintf(str1,"CWD %s",cd_path);
    sprintf(str2,"RNFR %s",oldname + 1);
    sprintf(str3,"RNTO %s",newername);

    char *A = utf8_to(str1,index);
    char *B = utf8_to(str2,index);
    char *C = utf8_to(str3,index);
    free(str1);
    free(str2);
    free(str3);
    struct curl_slist *headerlist = NULL;
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl)//curl初始化成功
    {
        headerlist = curl_slist_append(headerlist,A);
        headerlist = curl_slist_append(headerlist,B);
        headerlist = curl_slist_append(headerlist,C);

        curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res=curl_easy_perform(curl);
        if(res != CURLE_OK)
        {    //2014.10.30 by sherry  处理“rename会挂掉"的bug
//            char *temp = (char*)malloc(sizeof(char)*(strlen(prepath) + strlen(newername) + 1));
//            memset(temp,'\0',sizeof(char)*(strlen(prepath) + strlen(newername) + 1));
//            sprintf(temp,"%s/%s",prepath,newername);
            char *temp = (char*)malloc(sizeof(char)*(strlen(prepath) + strlen(newername) + 2));
            memset(temp,'\0',sizeof(char)*(strlen(prepath) + strlen(newername) + 2));
            sprintf(temp,"%s/%s",prepath,newername);
            int status = 0;
            if(!test_if_dir(temp))//test_if_dir如果是folder则返回1
            {
               //file
                status = upload(temp,index);
            }
            else
            {
                //folder
                status = moveFolder(localpath,temp,index);
            }
            free(A);
            free(B);
            free(C);
            free(temp);
            free(prepath);
            free(cd_path);
            //free(newername);形参，函数开始调用时分配动态存储空间，函数结束时释放
            curl_slist_free_all(headerlist);
            curl_easy_cleanup(curl);
            return status;
        }
        else
        {
            free(A);
            free(B);
            free(C);
            free(prepath);
            free(cd_path);
            curl_slist_free_all(headerlist);
            curl_easy_cleanup(curl);
            return 0;
        } 
    }
    else
    {
        free(A);
        free(B);
        free(C);
        free(prepath);
        free(cd_path);
        return 0;
    }
}

int my_mkdir_(char *localpath,int index)
{
    if(access(localpath,0) != 0)//检查本地端文件是否存在，
        //如果文件存在，返回0，不存在，返回-1
    {
        DEBUG("Local has no %s\n",localpath);
        return LOCAL_FILE_LOST;
    }

//    if(strcmp(localpath,ftp_config.multrule[index]->base_path) == 0)
//    {
//        write_log(S_ERROR,"Server Del Sync Folder!","",index);
//        return SERVER_ROOT_DELETED;
//    }

    char *serverpath = localpath_to_serverpath(localpath,index);
    char *command = (char *)malloc(sizeof(char)*(strlen(serverpath) + 5));
    memset(command,'\0',sizeof(command));
    sprintf(command,"MKD %s",serverpath);
    DEBUG("%s\n",command);
    char *temp = utf8_to(command,index);
    free(command);
    FILE *fp = fopen(CURL_HEAD,"w");
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, temp);
        curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fp);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl);
        DEBUG("res:%d\n",res);
        if(res != CURLE_OK)
        {
            curl_easy_cleanup(curl);
            fclose(fp);
            free(temp);//free(command);
            free(serverpath);
            FILE *fp1 = fopen(CURL_HEAD,"rb");
            char buff[512]={0};
            const char *split=" ";
            char *p = NULL;
            int i = 0;
            while(fgets(buff,sizeof(buff),fp1) != NULL)
            {
                p = strstr(buff,"Permission denied");
                if(p != NULL)
                {
                    write_log(S_ERROR,"Permission denied","",index);
                    fclose(fp1);
                    return PERMISSION_DENIED;
                }
                p=strtok(buff,split);
                if(atoi(p) == 257)
                {
                    i++;
                    if(i > 1){
                        DEBUG("Create Ok.\n");
                        fclose(fp1);
                        return 0;
                    }
                }
            }
            fclose(fp1);
            write_log(S_ERROR,"Make Dir failed.","",index);
            return -1;
        }
        else
        {
            curl_easy_cleanup(curl);
            fclose(fp);
            free(temp);//free(command);
            free(serverpath);
            return 0;
        }
    }
    else
    {
        fclose(fp);
        free(serverpath);
        free(temp);//free(command);
        return 0;
    }
}

int moveFolder(char *old_dir,char *new_dir,int index)
{
    //my_delete(old_dir,index);
    Delete(old_dir,index);
    int status;

    struct dirent *ent = NULL;
    DIR *pDir;
    pDir=opendir(new_dir);

    if(pDir != NULL )
    {
        status = my_mkdir_(new_dir,index);
        if(status != 0)
        {
            DEBUG("Create %s failed\n",new_dir);
            closedir(pDir);
            return status;
        }

        while ((ent=readdir(pDir)) != NULL)
        {
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;

            char *old_fullname = my_str_malloc(strlen(old_dir)+strlen(ent->d_name)+2);
            char *new_fullname = my_str_malloc(strlen(new_dir)+strlen(ent->d_name)+2);

            sprintf(new_fullname,"%s/%s",new_dir,ent->d_name);
            sprintf(old_fullname,"%s/%s",old_dir,ent->d_name);

            if(socket_check(new_dir,ent->d_name,index) == 1)
                continue;
            if(test_if_dir(new_fullname) == 1)
            {
                status = moveFolder(old_fullname,new_fullname,index);
                if(status != 0)
                {
                    DEBUG("CreateFolder %s failed\n",new_fullname);
                    free(new_fullname);
                    free(old_fullname);
                    closedir(pDir);
                    return status;
                }
            }
            else
            {
                status = upload(new_fullname,index);
                if(status != 0)
                {
                    DEBUG("move %s failed\n",new_fullname);
                    free(new_fullname);
                    free(old_fullname);
                    closedir(pDir);
                    return status;
                }
                else
                {
                    char *serverpath = localpath_to_serverpath(new_fullname,index);
                    mod_time *onefiletime = Getmodtime(serverpath,index);
                    free(serverpath);
                    if(ChangeFile_modtime(new_fullname,onefiletime->modtime,index))
                    {
                        DEBUG("ChangeFile_modtime failed!\n");
                    }
                }
            }
            free(new_fullname);
            free(old_fullname);

        }
        closedir(pDir);
        return 0;
    }
    else
    {
        DEBUG("open %s fail \n",new_dir);
        return LOCAL_FILE_LOST;
    }
}

int socket_check(char *dir,char *name,int index)
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

    //char *fullpath = my_str_malloc(strlen(dir) + strlen(name) + 1);//2014.11.5 by sherry
    char *fullpath = my_str_malloc(strlen(dir) + strlen(name) + 2);//内存分配不足
    sprintf(fullpath,"%s/%s",dir,name);

    DEBUG("fullpath:%s\n",fullpath);

    Node *pTemp = g_pSyncList[index]->SocketActionList->next;
    while(pTemp != NULL)
    {
        printf("%s\n",pTemp->cmdName);
        if(!strncmp(pTemp->cmdName,"move",4) || !strncmp(pTemp->cmdName,"rename",6))
        {
            sock_buf = my_str_malloc(strlen(pTemp->cmdName) + 1);
            sprintf(sock_buf,"%s",pTemp->cmdName);
            char *ret = strchr(sock_buf,'\n');
            part_one = my_str_malloc(strlen(sock_buf) - strlen(ret) + 1);
            snprintf(part_one,strlen(sock_buf) - strlen(ret) + 1,"%s",sock_buf);
            char *ret1 = strchr(ret + 1,'\n');
            part_two = my_str_malloc(strlen(ret + 1) - strlen(ret1) + 1);
            snprintf(part_two,strlen(ret + 1) - strlen(ret1) + 1,"%s",ret + 1);
            char *ret2 = strchr(ret1 + 1,'\n');
            part_three = my_str_malloc(strlen(ret1 + 1) - strlen(ret2) + 1);
            snprintf(part_three,strlen(ret1 + 1) - strlen(ret2) + 1,"%s",ret1 + 1);
            part_four = my_str_malloc(strlen(ret2 + 1) + 1);
            snprintf(part_four,strlen(ret2 + 1) + 1,"%s",ret2 + 1);

            printf("1:%s\n",part_one);
            printf("2:%s\n",part_two);
            printf("3:%s\n",part_three);
            printf("4:%s\n",part_four);

            if(!strncmp(part_one,"move",4))
            {
                dir1 = my_str_malloc(strlen(part_two) + strlen(part_four) + 1);
                dir2 = my_str_malloc(strlen(part_three) + strlen(part_four) + 1);
            }
            else if(!strncmp(part_one,"rename",6))
            {
                dir1 = my_str_malloc(strlen(part_two) + strlen(part_three) + 1);
                dir2 = my_str_malloc(strlen(part_two) + strlen(part_four) + 1);
            }
            p = strstr(dir1,fullpath);
            q = strstr(dir2,fullpath);
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
            dir1 = my_str_malloc(strlen(dir) + 3);
            dir2 = my_str_malloc(strlen(dir) + 3);
            sprintf(dir1,"\n%s\n",dir);
            sprintf(dir2,"\n%s/",dir);

            DEBUG("dir1:%s\n",dir1);
            DEBUG("dir2:%s\n",dir2);

            p = strstr(pTemp->cmdName,dir1);
            q = strstr(pTemp->cmdName,dir2);
            if(p != NULL || q != NULL)
            {
                res = 1;
                free(dir1);
                free(dir2);
                break;
            }
        }
        pTemp=pTemp->next;
        free(dir1);
        free(dir2);
    }
    free(fullpath);
    return res;
}
