#include "base.h"

extern Config ftp_config,ftp_config_stop;
extern sync_list **g_pSyncList;
extern int browse_sev;
extern int my_index;

void replace_char_in_str(char *str,char newchar,char oldchar){

    int i;
    int len;
    len = strlen(str);
    for(i=0;i<len;i++)
    {
        if(str[i] == oldchar)
        {
            str[i] = newchar;
        }
    }

}

void my_local_mkdir(char *path)//看是否有此(path)路径，没有就创建
{
    //char error_message[256];
    DIR *dir;
    if(NULL == (dir = opendir(path)))
    {
        if(-1 == mkdir(path,0777)) //mkdir(path,0777)定义一个以path命名的目录，
            //0777是默认模式，表示文件所有者，文件所有组以及其他用户都具有读写执行的权限
        {
            DEBUG("please check disk can write or dir has exist???");
            DEBUG("mkdir %s fail\n",path);
            return;
        }
    }
    else
        closedir(dir);
}

int is_server_have_localpath(char *path,Server_TreeNode *treenode,int index)
{
    if(treenode == NULL)
        return 0;

    //char *hreftmp;
    char *localpath;
    int ret = 0;
    int cmp = 1;

    localpath = serverpath_to_localpath(treenode->parenthref,index);

    if((cmp = strcmp(localpath,path)) == 0)
    {
        ret = 1;
        free(localpath);
        return ret;
    }
    else
    {
        free(localpath);
    }

    if(treenode->Child != NULL)
    {
        ret = is_server_have_localpath(path,treenode->Child,index);
        if(ret == 1)
        {
            return ret;
        }
    }
    if(treenode->NextBrother != NULL)
    {
        ret = is_server_have_localpath(path,treenode->NextBrother,index);
        if(ret == 1)
        {
            return ret;
        }
    }

    return ret;
}

Server_TreeNode * getoldnode(Server_TreeNode *tempoldnode,char *href)
{
    //Server_TreeNode *tempoldnode;
    Server_TreeNode *retnode;
    //tempoldnode = NULL;
    retnode = NULL;

    //DEBUG("getoldnode tempoldnode->parenthref -> %s   %d\n",tempoldnode->parenthref,strlen(tempoldnode->parenthref));
    //DEBUG("                              href -> %s   %d\n",href,strlen(href));

    if(strcmp(href,tempoldnode->parenthref) == 0)
    {
        //DEBUG("*****get the retnode\n");
        retnode = tempoldnode;
        return retnode;
    }

    if(tempoldnode->Child != NULL)
    {
        retnode = getoldnode(tempoldnode->Child,href);
        if(retnode != NULL)
        {
            return retnode;
        }
    }
    if(tempoldnode->NextBrother != NULL)
    {
        retnode = getoldnode(tempoldnode->NextBrother,href);
        if(retnode != NULL)
        {
            return retnode;
        }
    }
    return retnode;
}

int get_path_to_index(char *path)
{
    DEBUG("%s\n",path);
    int i;
    char *root_path = NULL;
    char *temp = NULL;
    root_path = my_str_malloc(512);

    temp = my_nstrchr('/',path,5);
    if(temp == NULL)
    {
        sprintf(root_path,"%s",path);
    }
    else
    {
        snprintf(root_path,strlen(path)-strlen(temp)+1,"%s",path);
    }

    for(i=0;i<ftp_config.dir_num;i++)
    {
        if(!strcmp(root_path,ftp_config.multrule[i]->base_path))
            break;
    }
    DEBUG("get_path_to_index root_path = %s\t%d\n",root_path,i);

    free(root_path);

    return i;
}

char *change_server_same_name(char *fullname,int index)
{
    DEBUG("###################change server same name...##################\n");
    int i = 1;
    int exist;
    char *filename = NULL;
    char *temp_name = NULL;
    int len = 0;
    char *path;
    char newfilename[512];
    int exit = 1;
    int lens = 0;
    char tail[512];

    char *fullname_tmp = NULL;
    fullname_tmp = my_str_malloc(strlen(fullname)+1);
    sprintf(fullname_tmp,"%s",fullname);

    DEBUG("%s\n",fullname);
    char *temp = get_prepath(fullname_tmp,strlen(fullname_tmp));
    filename = (char *)malloc(sizeof(char)*(strlen(fullname_tmp) - strlen(temp) + 1));
    sprintf(filename,"%s",fullname_tmp + strlen(temp));
    //filename = fullname_tmp + strlen(temp);
    free(temp);
    //len = strlen(fullname_tmp);//2014.11.11 by sherry
    path = get_prepath(fullname,strlen(fullname));

    free(fullname_tmp);

    while(exit)
    {
        int n = i;
        int j = 0;
        while((n=(n/10)))
        {
            j++;
        }
        memset(newfilename,'\0',sizeof(newfilename));
        snprintf(newfilename,252-j,"%s",filename);
        lens = get_prefix(newfilename,strlen(newfilename));
        sprintf(tail,"(%d)",i);

        string newname(newfilename),temp(tail);//拷贝构造函数
        newname = newname.insert(lens,temp);
        //sprintf(newfilename,"%s(%d)",newfilename,i);
        memset(newfilename,'\0',sizeof(newfilename));
        //strcpy(newfilename,newname.c_str());
        sprintf(newfilename,"%s",newname.c_str());

        //newname.~string();
        //temp.~string();
        DEBUG("newfilename = %s\n",newfilename);
        i++;

        temp_name = (char *)malloc(sizeof(char)*(strlen(path) + strlen(newfilename) + 1));
        sprintf(temp_name,"%s%s",path,newfilename);
        DEBUG("temp_name = %s\n",temp_name);

        exist = is_server_exist(path,temp_name,index);

        if(exist)
        {
            //free(path);
            free(temp_name);
        }
        else
        {
            exit = 0;
            //return temp_name;
        }
    }
    free(path);
    char *tmp = get_prepath(temp_name,strlen(temp_name));
    //2014.11.11 by sherry  free的内存空间和申请的不一致，程序挂掉
    //temp_name = temp_name + strlen(tmp) + 1;
    char *temp_name1=NULL;
    temp_name1=my_str_malloc(strlen(temp_name)-strlen(tmp)+1);
    sprintf(temp_name1,"%s",temp_name + strlen(tmp) + 1);

    free(tmp);
    free(filename);
    free(temp_name);//2014.11.11 by sherry

//    temp_name = temp_name + strlen(get_prepath(temp_name,strlen(temp_name))) + 1;
    return temp_name1;//2014.11.11 by sherry
}

char *change_local_same_name(char *fullname)
{
    int i = 1;
    char *temp_name = NULL;
    int len = 0;
    char *path;
    char newfilename[256];
    int lens = 0;
    char tail[512];

    char *fullname_tmp = NULL;
    fullname_tmp = my_str_malloc(strlen(fullname)+1);
    sprintf(fullname_tmp,"%s",fullname);

    char *temp = get_prepath(fullname_tmp,strlen(fullname_tmp));
    char *filename = fullname_tmp + strlen(temp);
    free(temp);
    len = strlen(filename);
    DEBUG("filename len is %d\n",len);
    path = my_str_malloc((size_t)(strlen(fullname)-len+1));
    DEBUG("fullname = %s\n",fullname);
    snprintf(path,strlen(fullname)-len+1,"%s",fullname);
    DEBUG("path = %s\n",path);
    //free(fullname_tmp);

    while(1)
    {
        int n = i;
        int j = 0;
        while((n=(n/10)))
        {
            j++;
        }
        memset(newfilename,'\0',sizeof(newfilename));
        snprintf(newfilename,252-j,"%s",filename);
        lens = get_prefix(newfilename,strlen(newfilename));
        sprintf(tail,"(%d)",i);
        string newname(newfilename),temp(tail);
        newname = newname.insert(lens,temp);
        //sprintf(newfilename,"%s(%d)",newfilename,i);
        memset(newfilename,'\0',sizeof(newfilename));
        sprintf(newfilename,"%s",newname.c_str());

        DEBUG("newfilename = %s\n",newfilename);
        i++;

        temp_name = my_str_malloc((size_t)(strlen(path)+strlen(newfilename)+1));
        sprintf(temp_name,"%s%s",path,newfilename);

        if(access(temp_name,F_OK) != 0)
        {
            //free(path);
            //return temp_name;
            break;
        }
        else
            free(temp_name);
    }
    free(path);
    free(fullname_tmp);
    //free(filename);
    return temp_name;
}

/*
 *judge is server changed
 *0:server changed
 *1:server is not changed
*/
int isServerChanged(Server_TreeNode *newNode,Server_TreeNode *oldNode)
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
                if ((cmp = strcmp(newfoldertmp->href,oldfoldertmp->href)) != 0){
                    DEBUG("########Server changed4\n");
                    return 0;
                }
                newfoldertmp = newfoldertmp->next;
                oldfoldertmp = oldfoldertmp->next;
            }
            while (newfiletmp != NULL || oldfiletmp != NULL)
            {
                if ((cmp = strcmp(newfiletmp->href,oldfiletmp->href)) != 0)
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

/*
 *if a = 0x1,find in folderlist
 *if a = 0x2,find in filelist
 *if a = 0x3,find in folderlist and filelist
*/
CloudFile *get_CloudFile_node(Server_TreeNode* treeRoot,const char *dofile_href,int a)
{
    DEBUG("****get_CloudFile_node****dofile_href = %s\n",dofile_href);
    int href_len = strlen(dofile_href);
    CloudFile *finded_file = NULL;
    if(treeRoot == NULL)
    {
        return NULL;
    }

    if((treeRoot->Child!=NULL))
    {
        finded_file = get_CloudFile_node(treeRoot->Child,dofile_href,a);
        if(finded_file != NULL)
        {
            return finded_file;
        }
    }

    if(treeRoot->NextBrother != NULL)
    {
        finded_file = get_CloudFile_node(treeRoot->NextBrother,dofile_href,a);
        if(finded_file != NULL)
        {
            return finded_file;
        }
    }

    if(treeRoot->browse != NULL)
    {
        int int_folder = 0x1;
        int int_file = 0x2;
        CloudFile *de_foldercurrent = NULL;
        CloudFile *de_filecurrent = NULL;
        DEBUG("111111folder = %d,file = %d\n",treeRoot->browse->foldernumber,treeRoot->browse->filenumber);
        if(treeRoot->browse->foldernumber > 0)
            de_foldercurrent = treeRoot->browse->folderlist->next;
        if(treeRoot->browse->filenumber > 0)
            de_filecurrent = treeRoot->browse->filelist->next;
        if((a&int_folder) && de_foldercurrent != NULL)
        {
            while(de_foldercurrent != NULL)
            {
                if(de_foldercurrent->href != NULL)
                {
                    DEBUG("de_foldercurrent->href = %s\n",de_foldercurrent->href);
                    if(!(strncmp(de_foldercurrent->href,dofile_href,href_len)))
                    {
                        return de_foldercurrent;
                    }
                }
                de_foldercurrent = de_foldercurrent->next;
            }
        }
        if((a&int_file) && de_filecurrent != NULL)
        {
            while(de_filecurrent != NULL)
            {
                if(de_filecurrent->href != NULL)
                {
                    DEBUG("de_filecurrent->href = %s\n",de_filecurrent->href);
                    if(!(strncmp(de_filecurrent->href,dofile_href,href_len)))
                    {
                        DEBUG("get it\n");
                        return de_filecurrent;
                    }
                }
                de_filecurrent = de_filecurrent->next;
            }
        }
    }
    return finded_file;
}

void del_all_items(char *dir,int index)
{
    struct dirent* ent = NULL;
    DIR *pDir;
    pDir=opendir(dir);

    if(pDir != NULL )
    {
        while (NULL != (ent=readdir(pDir)))
        {
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;

            char *fullname;
            size_t len;
            len = strlen(dir)+strlen(ent->d_name)+2;
            fullname = my_str_malloc(len);
            sprintf(fullname,"%s/%s",dir,ent->d_name);

            if(test_if_dir(fullname) == 1)
            {
                wait_handle_socket(index);
                del_all_items(fullname,index);
            }
            else
            {
                wait_handle_socket(index);
                add_action_item("remove",fullname,g_pSyncList[index]->server_action_list);
                remove(fullname);
            }

            free(fullname);
        }
        closedir(pDir);

        add_action_item("remove",dir,g_pSyncList[index]->server_action_list);
        remove(dir);
    }
    else
        DEBUG("open %s fail \n",dir);
}

char *get_prepath(char *temp,int len)
{
    //2014.09.26 by sherry
    //野指针，内存越界
//    char *p = temp;
//    char *q = temp;
//    int count = 0,count_1 = 0;
//    while(strlen(p) != 0) //p 得到字符串的总长度
//    {
//        if(p[0] == '/')
//        {
//            count++;
//        }
//        p++;
//    }
//    while(strlen(q) != 0) //q 定位到最后一个'/'
//    {
//        if(q[0] == '/')
//        {
//            count_1++;
//            if(count_1 == count)
//            {
//                break;
//            }
//        }
//        q++;
//    }
    char *q;
    q=strrchr(temp,'/');
    if( q == NULL)
    {
        return NULL;
    }

    char *ret = (char *)malloc(sizeof(char)*(len - strlen(q) + 1));
    memset(ret,'\0',sizeof(char)*(len - strlen(q) + 1));
    strncpy(ret,temp,len-strlen(q));
    return ret;
}

int get_prefix(char *temp,int len)
{
//    char *p = temp;
//    char *q = temp;
//    int count = 0,count_1 = 0;
//    while(strlen(p) != 0)
//    {
//        if(p[0] == '.')
//        {
//            count++;
//        }
//        p++;
//    }
//    while(strlen(q) != 0)
//    {
//        if(q[0] == '.')
//        {
//            count_1++;
//            if(count_1 == count)
//            {
//                break;
//            }
//        }
//        q++;
//    }
    //2014.11.06 by sherry 程序挂掉
    char *q;
    q=strrchr(temp,'.');
    if(q==NULL)
        return len;
    return len-strlen(q);
}

/*free保存单层文件夹信息所用的空间*/
void free_localfloor_node(Local *local)
{
    free_LocalFile_item(local->filelist);
    free_LocalFolder_item(local->folderlist);
    free(local);
}

void free_LocalFolder_item(LocalFolder *head)
{
    LocalFolder *p = head;
    while(p != NULL)
    {
        head = head->next;
        if(p->path != NULL)
        {
            //DEBUG("free LocalFolder %s\n",point->path);
            free(p->path);
        }
        free(p);
        p = head;
    }
}

void free_LocalFile_item(LocalFile *head)
{
    LocalFile *p = head;
    while(p != NULL)
    {
        head = head->next;
        if(p->path != NULL)
        {
            //DEBUG("free LocalFile %s\n",point->path);
            free(p->path);
        }
        free(p);
        p = head;
    }
}

void free_server_tree(Server_TreeNode *node)
{
    //DEBUG("free_server_tree\n");
    if(node != NULL)
    {
        //DEBUG("free tree node\n");

        //free_server_list(node->browse);

        if(node->NextBrother != NULL)
            free_server_tree(node->NextBrother);
        if(node->Child != NULL)
            free_server_tree(node->Child);
        if(node->parenthref != NULL)
        {
            free(node->parenthref);
        }
        if(node->browse != NULL)
        {
            free_CloudFile_item(node->browse->filelist);
            free_CloudFile_item(node->browse->folderlist);
            free(node->browse);
        }
        free(node);
    }
}

void free_CloudFile_item(CloudFile *head)
{
    //DEBUG("***************free_CloudFile_item*********************\n");

    CloudFile *p = head;
    while(p != NULL)
    {
        head = head->next;
        if(p->href != NULL)
        {
            //DEBUG("free CloudFile %s\n",p->href);
            free(p->href);
        }
        free(p);
        p = head;
    }
    //head=NULL;
}

int is_folder(char *p){
    return (p[0]=='<') ? 0 : 1;
}

int is_file(char *p){
    return (p[0]=='d')?0:1;
}

Server_TreeNode *create_server_treeroot()
{
    Server_TreeNode *TreeRoot = NULL;
    TreeRoot = (Server_TreeNode *)malloc(sizeof (Server_TreeNode));
    memset(TreeRoot,0,sizeof(Server_TreeNode));
    if(TreeRoot == NULL)
    {
        DEBUG("create memory error!\n");
        exit(-1);
    }
    TreeRoot->level=0;
    TreeRoot->NextBrother = NULL;
    //TreeRoot->browse = NULL;
    //sprintf(TreeRoot->parenthref,"%s%s/",HOST,ROOTFOLDER);
    TreeRoot->parenthref = NULL;
    TreeRoot->browse = NULL;
    TreeRoot->Child = NULL;

    return TreeRoot;
}


int getCloudInfo_forsize(char *URL,void (* cmd_data)(char *),int index)
{
    char *command = (char *)malloc(sizeof(char)*(strlen(URL) + 7));
    memset(command,'\0',sizeof(command));
    sprintf(command,"LIST %s",URL);
    char *temp = utf8_to(command,index);
    free(command);
    CURL *curl;
    CURLcode res;
    FILE *fp = fopen(LIST_SIZE_DIR,"w");
    //fp=fopen("/tmp/ftpclient/list_size.txt","w");
    curl=curl_easy_init();
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,temp);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res=curl_easy_perform(curl);
        //DEBUG("%d\n",res);

        if(res != CURLE_OK && res != CURLE_FTP_COULDNT_RETR_FILE){
            curl_easy_cleanup(curl);
            fclose(fp);
            //free(command);
            free(temp);
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
            return -1;
        }
        curl_easy_cleanup(curl);
        fclose(fp);
        //free(command);
        free(temp);
    }
    else
    {
        curl_easy_cleanup(curl);
        fclose(fp);
        //free(command);
        free(temp);
    }
    cmd_data(URL);
    return 0;
}



int is_ftp_file_copying(char *serverhref,int index)
{
    //DEBUG("########check copying#######\n");
    //DEBUG("\rFtp file copying...");
    long long int old_length;
    long long int new_length;
    long long int d_value;

    old_length = get_file_size(serverhref,index);
    usleep(1000*1000);
    new_length = get_file_size(serverhref,index);
    d_value = new_length - old_length;
    if(d_value == 0)
        return 0;
    else
        return 1;
}

int ChangeFile_modtime(char *filepath,time_t servermodtime,int index)
{
    DEBUG("**************ChangeFile_modtime**********\n");
    char *newpath_re = search_newpath(filepath,index);

    struct utimbuf *ub;
    ub = (struct utimbuf *)malloc(sizeof(struct utimbuf));
    if(newpath_re != NULL)

    if(servermodtime == -1)
    {
        return -1;
    }

    ub->actime = servermodtime;
    ub->modtime = servermodtime;

    if(newpath_re != NULL)
    {
        DEBUG("newpath_re = %s\n",newpath_re);
        utime(newpath_re,ub);//修改参数filename文件所属的inode存取时间
    }
    else
    {
        DEBUG("filepath = %s\n",filepath);
        utime(filepath,ub);
    }

    free(newpath_re);
    free(ub);
    return 0;
}

char *localpath_to_serverpath(char *from_localpath,int index)
{
    //2014.10.27 by sherry
    //未考虑在根目录下操作的情况
//    char *p = from_localpath;
//    p = p + strlen(ftp_config.multrule[index]->base_path);
//    char *serverpath = (char*)malloc(sizeof(char)*(strlen(ftp_config.multrule[index]->rooturl) + strlen(p) + 1));
//    memset(serverpath,'\0',sizeof(char)*(strlen(ftp_config.multrule[index]->rooturl) + strlen(p) + 1));
//    sprintf(serverpath,"%s%s",ftp_config.multrule[index]->rooturl,p);
//    return serverpath;

    char *p = from_localpath;
    char *serverpath;

    if(!strcmp(from_localpath,ftp_config.multrule[index]->base_path))
    {
        serverpath = (char*)malloc(sizeof(char)*(strlen(ftp_config.multrule[index]->rooturl) + 1));
        memset(serverpath,'\0',sizeof(char)*(strlen(ftp_config.multrule[index]->rooturl) + 1));
        sprintf(serverpath,"%s",ftp_config.multrule[index]->rooturl);
        return serverpath;
    }
    else
    {
        p = p + strlen(ftp_config.multrule[index]->base_path);
        serverpath = (char*)malloc(sizeof(char)*(strlen(ftp_config.multrule[index]->rooturl) + strlen(p) + 1));
        memset(serverpath,'\0',sizeof(char)*(strlen(ftp_config.multrule[index]->rooturl) + strlen(p) + 1));
        sprintf(serverpath,"%s%s",ftp_config.multrule[index]->rooturl,p);
        return serverpath;
    }
}

char *serverpath_to_localpath(char *from_serverpath,int index)
{
    //2014.10.28 by sherry
    //未考虑在根目录下操作的情况
//    char *p = from_serverpath;
//    p = p + strlen(ftp_config.multrule[index]->rooturl);
//    char *localpath = (char*)malloc(sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(p) + 1));
//    memset(localpath,'\0',sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(p) + 1));
//    sprintf(localpath,"%s%s",ftp_config.multrule[index]->base_path,p);
//    return localpath;

    char *p = from_serverpath;
    char *localpath;
    if(!strcmp(from_serverpath,ftp_config.multrule[index]->rooturl))
    {
        localpath=(char*)malloc(sizeof(char)*(ftp_config.multrule[index]->base_path_len + 1));
        memset(localpath,'\0',sizeof(char)*(ftp_config.multrule[index]->base_path_len + 1));
        sprintf(localpath,"%s",ftp_config.multrule[index]->base_path);
        return localpath;
    }
    else
    {
        p = p + strlen(ftp_config.multrule[index]->rooturl);
        localpath = (char*)malloc(sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(p) + 1));
        memset(localpath,'\0',sizeof(char)*(ftp_config.multrule[index]->base_path_len + strlen(p) + 1));
        sprintf(localpath,"%s%s",ftp_config.multrule[index]->base_path,p);
        return localpath;
    }

}

int test_if_dir_empty(char *dir)
{
    struct dirent* ent = NULL;
    DIR *pDir;
    int i = 0;
    pDir=opendir(dir);
    if(pDir != NULL )
    {
        while (NULL != (ent=readdir(pDir)))
        {
            if(ent->d_name[0] == '.')
                continue;
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            i++;
        }
        closedir(pDir);
    }
    return  (i == 0) ? 1 : 0;
}

Node *queue_dequeue (Node *q)
{
    Node *first = q->next;
    Node *newfirst = q->next->next;
    q->next = newfirst;
    first->next = NULL;
    return first;
}

//void SearchServerTree(Server_TreeNode* treeRoot)
//{
//    FILE *fp = NULL;
//
//    if(treeRoot->level == 0)
//    {
//        fp = fopen(SERVERLIST_0,"a");
//        //fprintf(fp,"%d\n",treeRoot->level);
//    }
//
//    if(treeRoot->level == 1)
//    {
//        fp = fopen(SERVERLIST_1,"a");
//        //fprintf(fp,"%d\n",treeRoot->level);
//    }
//
//    if(treeRoot->level == 2)
//    {
//        fp = fopen(SERVERLIST_2,"a");
//        //fprintf(fp,"%d\n",treeRoot->level);
//    }
//
//    if(treeRoot->browse != NULL)
//    {
//
//        CloudFile *de_foldercurrent;
//        de_foldercurrent = treeRoot->browse->folderlist->next;
//        while(de_foldercurrent != NULL){
//            //DEBUG("serverfolder->href = %s\n",de_foldercurrent->href);
//            fprintf(fp,"%s\n",de_foldercurrent->href);
//            de_foldercurrent = de_foldercurrent->next;
//        }
//    }
//    fclose(fp);
//    if((treeRoot->Child != NULL))
//        SearchServerTree(treeRoot->Child);
//
//    if(treeRoot->NextBrother != NULL)
//        SearchServerTree(treeRoot->NextBrother);
//}

void free_action_item(action_item *head)
{
    action_item *point;
    point = head->next;

    while(point != NULL)
    {
        head->next = point->next;
        free(point->action);
        free(point->path);
        free(point);
        point = head->next;
    }
    free(head);
}

void del_download_only_action_item(const char *action,const char *path,action_item *head)
{
    //DEBUG("del_sync_item action=%s,path=%s\n",action,path);
    action_item *p1, *p2;
    char *cmp_name;
    char *p1_cmp_name;
    p1 = head->next;
    p2 = head;

    cmp_name = my_str_malloc((size_t)(strlen(path)+2));
    sprintf(cmp_name,"%s/",path);    //add for delete folder and subfolder in download only socket list

    while(p1 != NULL)
    {
        p1_cmp_name = my_str_malloc((size_t)(strlen(p1->path)+2));
        sprintf(p1_cmp_name,"%s/",p1->path);      //add for delete folder and subfolder in download only socket list
        //DEBUG("del_download_only_sync_item  p1->name = %s\n",p1->name);
        //DEBUG("del_download_only_sync_item  cmp_name = %s\n",cmp_name);
        if(strstr(p1_cmp_name,cmp_name) != NULL)
        {
            p2->next = p1->next;
            free(p1->action);
            free(p1->path);
            free(p1);
            //DEBUG("del sync item ok\n");
            //break;
            p1 = p2->next;
        }
        else
        {
            p2 = p1;
            p1 = p1->next;
        }
        free(p1_cmp_name);
    }

    free(cmp_name);
    //DEBUG("del sync item fail\n");
}

int add_all_download_only_socket_list(char *cmd,const char *dir,int index)
{
    struct dirent* ent = NULL;
    char *fullname;
    int fail_flag = 0;
    //char error_message[256];

    DIR *dp = opendir(dir);

    if(dp == NULL)
    {
        DEBUG("opendir %s fail",dir);
        fail_flag = 1;
        return -1;
    }

    add_action_item(cmd,dir,g_pSyncList[index]->download_only_socket_head);

    while (NULL != (ent=readdir(dp)))
    {

        if(ent->d_name[0] == '.')
            continue;
        if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
            continue;

        fullname = my_str_malloc((size_t)(strlen(dir)+strlen(ent->d_name)+2));

        sprintf(fullname,"%s/%s",dir,ent->d_name);

        if( test_if_dir(fullname) == 1)
        {
            add_all_download_only_socket_list("createfolder",fullname,index);
        }
        else
        {
            add_action_item("createfile",fullname,g_pSyncList[index]->download_only_socket_head);
        }
        free(fullname);
    }

    closedir(dp);

    return (fail_flag == 1) ? -1 : 0;
}

int test_if_download_temp_file(char *filename)
{
    char file_suffix[9];
    //char *temp_suffix = ".asus.td";
    char *temp_suffix = (char*)malloc(sizeof(char)*9);
    memset(temp_suffix,'\0',sizeof(char)*9);
    strcpy(temp_suffix,".asus.td");

    memset(file_suffix,0,sizeof(file_suffix));
    char *p = filename;
    if(strstr(filename,temp_suffix))
    {
        strcpy(file_suffix,p+(strlen(filename)-strlen(temp_suffix)));
        if(!strcmp(file_suffix,temp_suffix))
        {
            free(temp_suffix);
            return 1;
        }
    }
    free(temp_suffix);
    return 0;
}

int test_if_dir(const char *dir)
{
    DIR *dp = opendir(dir);
    if(dp == NULL)
    {
        //DEBUG("file\n");
        return 0;
    }
    closedir(dp);
    //DEBUG("dir\n");
    return 1;
}

void show(Node* head)
{
    DEBUG("##################Sockets from inotify...###################\n");
    Node *pTemp = head->next;
    while(pTemp!=NULL)
    {
        DEBUG(">>>>%s\n",pTemp->cmdName);
        pTemp=pTemp->next;
    }
}

void show_item(action_item* head)
{
    action_item *pTemp = head->next;
    //DEBUG(">>>>Chain:%s\n",head->cmdName);
    while(pTemp!=NULL)
    {
        DEBUG("*****%s:%s\n",pTemp->action,pTemp->path);
        pTemp=pTemp->next;
    }
}

int del_action_item(const char *action,const char *path,action_item *head)
{
    action_item *p1,*p2;
    p1 = head->next;
    p2 = head;
    while(p1 != NULL)
    {
        if( !strcmp(p1->action,action) && !strcmp(p1->path,path))
        {
            p2->next = p1->next;
            free(p1->action);
            free(p1->path);
            free(p1);
            return 0;
        }
        p2 = p1;
        p1 = p1->next;
    }
    DEBUG("can not find action item\n");

    return 1;
}

int is_local_space_enough(CloudFile *do_file,int index)
{
    long long int freespace;
    freespace = get_local_freespace(index);
    DEBUG("freespace = %lld,do_file->size = %lld\n",freespace,do_file->size);
    if(freespace <= do_file->size){
        DEBUG("local freespace is not enough!\n");
        return 0;
    }
    else
    {
        //DEBUG("local freespace is enough!\n");
        return 1;
    }
}

action_item *create_action_item_head()
{
    action_item *head;

    head = (action_item *)malloc(sizeof(action_item));
    if(head == NULL)
    {
        DEBUG("create memory error!\n");
        exit(-1);
    }
    memset(head,'\0',sizeof(action_item));
    head->next = NULL;

    return head;
}

int add_action_item(const char *action,const char *path,action_item *head)
{
    DEBUG("add_action_item,action = %s,path = %s\n",action,path);

    action_item *p1,*p2;
    p1 = head;
    p2 = (action_item *)malloc(sizeof(action_item));
    memset(p2,'\0',sizeof(action_item));
    p2->action = (char *)malloc(sizeof(char)*(strlen(action)+1));
    p2->path = (char *)malloc(sizeof(char)*(strlen(path)+1));
    memset(p2->action,'\0',sizeof(p2->action));
    memset(p2->path,'\0',sizeof(p2->path));
    sprintf(p2->action,"%s",action);
    sprintf(p2->path,"%s",path);
    while(p1->next != NULL)
        p1 = p1->next;
    p1->next = p2;
    p2->next = NULL;
    DEBUG("add action item OK!\n");
    return 0;
}

action_item *get_action_item(const char *action,const char *path,action_item *head,int index)
{
    action_item *p;
    p = head->next;
    while(p != NULL)
    {
        if(ftp_config.multrule[index]->rules == 1)
        {
            if(!strcmp(p->path,path))
            {
                return p;
            }
        }
        else
        {
            if(!strcmp(p->action,action) && !strcmp(p->path,path))
            {
                return p;
            }
        }
        p = p->next;
    }
    DEBUG("can not find action item\n");
    return NULL;
}

char *my_str_malloc(size_t len)
{
    char *s;
    s = (char *)malloc(sizeof(char)*len);
    if(s == NULL)
    {
        DEBUG("Out of memory.\n");
        exit(1);
    }

    //memset(s,'\0',sizeof(s));//2014.10.28 by sherry sizeof(指针)为固定值4
    memset(s,'\0',sizeof(char)*len);
    return s;
}

long long int get_local_freespace(int index)
{
    DEBUG("***********get %s freespace!***********\n",ftp_config.multrule[index]->base_path);
    long long int freespace = 0;
    struct statvfs diskdata;
    if(!statvfs(ftp_config.multrule[index]->base_path,&diskdata))
    {
        freespace = (long long)diskdata.f_bsize * (long long)diskdata.f_bavail;
        return freespace;
    }
    else
    {
        return 0;
    }
}

char *get_socket_base_path(char *cmd)
{

    DEBUG("get_socket_base_path cmd : %s\n",cmd);

    char *temp = NULL;
    char *temp1 = NULL;
    char path[1024];
    char *root_path = NULL;

    if(!strncmp(cmd,"rmroot",6))
    {
        temp = strchr(cmd,'/');
        root_path = my_str_malloc(512);
        sprintf(root_path,"%s",temp);
    }
    else
    {
        temp = strchr(cmd,'/');
        temp1 = strchr(temp,'\n');
        memset(path,0,sizeof(path));
        strncpy(path,temp,strlen(temp)-strlen(temp1));
        root_path = my_str_malloc(512);
        temp = my_nstrchr('/',path,5);
        if(temp == NULL)
        {
            sprintf(root_path,"%s",path);
        }
        else
        {
            snprintf(root_path,strlen(path)-strlen(temp) + 1,"%s",path);
        }
    }
    //DEBUG("get_socket_base_path root_path = %s\n",root_path);
    return root_path;
}

mod_time *get_mtime_1(FILE *fp)
{
    if(fp != NULL)
    {
        mod_time *modtime_1;
        modtime_1=(mod_time *)malloc(sizeof(mod_time));
        memset(modtime_1,0,sizeof(modtime_1));
        char buff[512]={0};
        int res=0;
        const char *split=" ";
        char *p;
        rewind(fp);
        while(fgets(buff,sizeof(buff),fp) != NULL)
        {
            p=strtok(buff,split);
            int i=0;
            while(p!=NULL)
            {
                switch(i)
                {
                case 0:
                    res=atoi(p);
                    break;
                case 1:
                    if(res == 213)
                    {
                        strcpy(modtime_1->mtime, p);
                        DEBUG("%s", modtime_1->mtime);
                        modtime_1->modtime = change_time_to_sec(modtime_1->mtime);
                    }
                    if(res == 500)
                    {
                        modtime_1->modtime = (time_t)-1;
                    }
                    break;
                default:
                    break;
                }
                i++;
                p=strtok(NULL,split);
            }
            res=0;
        }
        fclose(fp);
        DEBUG1("@@@@@@@@@@@@modtime_1=%s\n",modtime_1);
        return modtime_1;
    }
    else
        DEBUG("fp is NULL \n");
}

int getCloudInfo_one(char *URL,int (* cmd_data)(char *,int),int index)
{
    DEBUG("%s\n",URL);
    int status;
    char *command = (char *)malloc(sizeof(char)*(strlen(URL) + 7));
    //memset(command,'\0',sizeof(command));//2014.10.28 by sherry sizeof(指针)=4
    memset(command,'\0',sizeof(char)*(strlen(URL) + 7));
    sprintf(command,"LIST %s",URL);
    DEBUG("command = %s\n",command);
    char *temp = utf8_to(command,index);
    free(command);
    CURL *curl;
    CURLcode res;
    FILE *fp = fopen(LIST_ONE_DIR,"w");
    //fp=fopen("/tmp/ftpclient/list_one.txt","w");
    curl=curl_easy_init();
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, ftp_config.multrule[index]->server_ip);
        if(strlen(ftp_config.multrule[index]->user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, ftp_config.multrule[index]->user_pwd);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,temp);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl);
        DEBUG("getCloudInfo_one() - res = %d\n",res);
        if(res != CURLE_OK)
        {
            curl_easy_cleanup(curl);
            fclose(fp);
            free(temp);
            return res;
        }
        curl_easy_cleanup(curl);
        fclose(fp);
        free(temp);//free(command);
    }
    else
    {
        curl_easy_cleanup(curl);
        fclose(fp);
        free(temp);//free(command);
    }
    status = cmd_data(URL,index);
    if(status)
    {
        return UNSUPPORT_ENCODING;
    }
    return 0;
}

time_t change_time_to_sec(char *time)
{
    char *p = time;
    char buf[12];
    memset(buf,0,sizeof(buf));
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv,&tz);
    int zone_time = tz.tz_minuteswest / 60;
    time_t sec;
    struct tm *timeptr = (struct tm *)malloc(sizeof(struct tm));

    memset(timeptr,'\0',sizeof(timeptr));
    strncpy(buf,p,4);
    timeptr->tm_year = atoi(buf) - 1900;

    p = p + 4;
    memset(buf,0,sizeof(buf));
    strncpy(buf,p,2);
    timeptr->tm_mon = atoi(buf) - 1;

    p = p + 2;
    memset(buf,0,sizeof(buf));
    strncpy(buf,p,2);
    timeptr->tm_mday = atoi(buf);

    p = p + 2;
    memset(buf,0,sizeof(buf));
    strncpy(buf,p,2);
    timeptr->tm_hour = atoi(buf) - zone_time;

    p = p + 2;
    memset(buf,0,sizeof(buf));
    strncpy(buf,p,2);
    timeptr->tm_min = atoi(buf);

    p = p + 2;
    memset(buf,0,sizeof(buf));
    strncpy(buf,p,2);
    timeptr->tm_sec = atoi(buf);

    sec = mktime(timeptr);
    p = NULL;
    free(timeptr);
    return sec;
}

mod_time *get_mtime(FILE *fp)
{
    //DEBUG("*********************get_mtime***********************\n");
    if(fp != NULL){
        mod_time *modtime;
        modtime=(mod_time *)malloc(sizeof(mod_time));
        memset(modtime,0,sizeof(modtime));
        char buff[512] = {0};
        int res = 0;
        const char *split = " ";
        char *p;
        rewind(fp);
        while(fgets(buff,sizeof(buff),fp) != NULL)
        {
            p=strtok(buff,split);
            int i = 0;
            while(p != NULL)
            {
                switch(i)
                {
                case 0:
                    res = atoi(p);
                    break;
                case 1:
                    if(res == 213)
                    {
                        strcpy(modtime->mtime,p);
                        modtime->modtime = change_time_to_sec(modtime->mtime);
                    }
                    break;
                default:
                    break;
                }
                i++;
                p = strtok(NULL,split);
            }
            res = 0;
        }
        fclose(fp);
        return modtime;
    }
    else
    {
        DEBUG("fp is NULL \n");
    }
}

void my_mkdir_r(char *path,int index)
{
    int i,len;
    char str[512];
    char fullname[512];
    char *temp;
    DEBUG("****************my_mkdir_r*******************\n");
    DEBUG("%s\n",path);
    memset(str,0,sizeof(str));

    temp = strstr(path,ftp_config.multrule[index]->mount_path);

    len = strlen(ftp_config.multrule[index]->mount_path);
    strcpy(str,temp + len);

    //strncpy(str,path,512);
    len = strlen(str);
    for(i=0; i < len ; i++)
    {
        if(str[i] == '/' && i != 0)
        {
            str[i] = '\0';
            memset(fullname,0,sizeof(fullname));
            sprintf(fullname,"%s%s",ftp_config.multrule[index]->mount_path,str);
            if(access(fullname,F_OK) != 0)
            {
                DEBUG("%s\n",fullname);
                my_local_mkdir(fullname);
            }
            str[i] = '/';
        }
    }


    memset(fullname,0,sizeof(fullname));
    sprintf(fullname,"%s%s",ftp_config.multrule[index]->mount_path,str);

    if(len > 0 && access(fullname,F_OK) != 0)
    {
        DEBUG("%s\n",fullname);
        my_local_mkdir(fullname);
    }
}

int parse_config(const char *path,Config *config)
{
    DEBUG("#####parse_config####\n");
    FILE *fp;
    //DIR *dir;
    int status;
    char buffer[256];
    char *p;

    int i = 0;
    int k = 0;
    int len = 0;

    memset(buffer, '\0', sizeof(buffer));

    if (access(path,0) == 0)
    {
        if(( fp = fopen(path,"rb"))==NULL)
        {
            fprintf(stderr,"read Cloud error");
            exit(-1);
        }
        config->multrule = (MultRule **)malloc(sizeof(MultRule*)*config->dir_num);
        while(fgets(buffer,256,fp) != NULL)
        {
            if( buffer[ strlen(buffer)-1 ] == '\n' )
                buffer[ strlen(buffer)-1 ] = '\0';
            p = buffer;
            DEBUG("p:%s\n",p);
            if(i == (k*8))
            {
                config->id = atoi(p);
                DEBUG("config->id = %d\n",config->id);
            }
            else if(i == (k*8 + 1))
            {
                config->multrule[k] = (MultRule*)malloc(sizeof(MultRule));
                memset(config->multrule[k],0,sizeof(MultRule));
                config->multrule[k]->encoding = atoi(p);
                DEBUG("config->multrule[%d]->encoding = %d\n",k,config->multrule[k]->encoding);
            }
            else if(i == (k*8 + 2))
            {
                strcpy(config->multrule[k]->f_usr,p);
            }
            else if(i == (k*8 + 3))
            {
                strcpy(config->multrule[k]->f_pwd,p);
                sprintf(config->multrule[k]->user_pwd,"%s:%s",config->multrule[k]->f_usr,config->multrule[k]->f_pwd);
                DEBUG("config->multrule[%d]->user_pwd = %s len:%d\n",k,config->multrule[k]->user_pwd,strlen(config->multrule[k]->user_pwd));
            }
            else if(i == (k*8 + 4))
            {
                strcpy(config->multrule[k]->server_ip,p);
                len = strlen(config->multrule[k]->server_ip);
                config->multrule[k]->server_ip_len = len;
                DEBUG("config->multrule[%d]->server_ip = %s\n",k,config->multrule[k]->server_ip);
            }
            else if(i == (k*8 + 5))
            {
                strcpy(config->multrule[k]->rooturl,p);
                char *p1 = config->multrule[k]->rooturl + strlen(config->multrule[k]->rooturl);
                while(p1[0] != '/')
                    p1--;
                DEBUG("config->multrule[%d]->rooturl = %s\n",k,config->multrule[k]->rooturl);
                snprintf(config->multrule[k]->hardware,strlen(config->multrule[k]->rooturl) - strlen(p1) + 1,"%s",config->multrule[k]->rooturl);
                DEBUG("config->multrule[%d]->hardware:%s\n",k,config->multrule[k]->hardware);
            }
            else if(i == (k*8 + 6))
            {
                config->multrule[k]->rules = atoi(p);
                DEBUG("config->multrule[%d]->rules = %d\n",k,config->multrule[k]->rules);
            }
            else if(i == (k*8 + 7))
            {
                len = strlen(p);
                if(p[len-1] == '\n')
                    p[len-1] = '\0';
                strcpy(config->multrule[k]->base_path,p);
                len = strlen(config->multrule[k]->base_path);
                config->multrule[k]->base_path_len = len;
                if(config->multrule[k]->base_path[len-1] == '/')
                    config->multrule[k]->base_path[len-1] = '\0';

                char *p2 = config->multrule[k]->base_path + config->multrule[k]->base_path_len;
                while(p2[0] != '/')
                    p2--;
                snprintf(config->multrule[k]->mount_path,config->multrule[k]->base_path_len - strlen(p2) + 1,"%s",config->multrule[k]->base_path);
                sprintf(config->multrule[k]->base_folder,"%s",p2);
                //sprintf(config->multrule[k]->base_path_parentref,"%s",config->multrule[k]->mount_path);
                sprintf(config->multrule[k]->fullrooturl,"%s%s",config->multrule[k]->server_ip,config->multrule[k]->rooturl);

                DEBUG("config->multrule[%d]->mount_path = %s\n",k,config->multrule[k]->mount_path);
                DEBUG("config->multrule[%d]->base_path = %s\n",k,config->multrule[k]->base_path);
                //DEBUG("config->multrule[%d]->base_path_parentref = %s\n",k,config->multrule[k]->base_path_parentref);
                DEBUG("config->multrule[%d]->base_folder = %s\n",k,config->multrule[k]->base_folder);
                DEBUG("config->multrule[%d]->rooturl = %s\n",k,config->multrule[k]->rooturl);
                DEBUG("config->multrule[%d]->fullrooturl = %s\n",k,config->multrule[k]->fullrooturl);
                k++;
            }
            i++;
        }

        //DEBUG("%s\n",buffer);
        //int x;
        //x = config->dir_num;
        //for(x=0;)

        fclose(fp);
    }
    int m;
    for(m = 0; m < config->dir_num; m++)
    {
        DEBUG("m = %d\n",m);
        my_mkdir_r(config->multrule[m]->base_path, m);  //have mountpath
    }
    return 0;
}

int usr_auth(char *ip,char *user_pwd)
{
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, ip);
        if(strlen(user_pwd) != 1)
            curl_easy_setopt(curl, CURLOPT_USERPWD, user_pwd);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,10);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        res = curl_easy_perform(curl);// 28:a timeout was reached  7:couldn't connect to server
        curl_easy_cleanup(curl);
        return res;
    }
}

//void create_start_file()
//{
//    my_local_mkdir("/tmp/smartsync_app");
//    FILE *fp;
//    fp = fopen("/tmp/smartsync_app/ftpclient_start","w");
//    fclose(fp);
//}
//
///*
// *0,no file
// *1,have file
//*/
//int detect_process_file()
//{
//    struct dirent *ent = NULL;
//    DIR *pdir;
//    int num = 0;
//    pdir = opendir("/tmp/smartsync_app");
//
//    if(pdir != NULL)
//    {
//        while (NULL != (ent=readdir(pdir)))
//        {
//            //printf("%s is ent->d_name\n",ent->d_name);
//            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
//                continue;
//            num++;
//        }
//        closedir(pdir);
//    }
//    else
//        return 0;
//
//    if(num)
//        return 1;
//
//    return 0;
//}
