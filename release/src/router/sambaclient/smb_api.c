#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "libsmbclient.h"
#include "SMB.h"

extern int exit_loop;
extern char g_workgroup[MAX_BUFF_SIZE];
extern char g_username[MAX_BUFF_SIZE];
extern char g_password[MAX_BUFF_SIZE];
extern char g_server[MAX_BUFF_SIZE];
extern char g_share[MAX_BUFF_SIZE];
extern Config smb_config;
extern sync_list **g_pSyncList;

extern SMBCCTX *context;
extern SMBCSRV *srv;

extern CloudFile *TreeFolderTail;
extern CloudFile *TreeFileTail;
extern CloudFile *FolderTmp;

void search_smb(char *url)
{
        struct smbc_dirent* dirptr;
        int dh = 0;
        dh = smbc_opendir(url);
        while((dirptr = smbc_readdir(dh)) != NULL){
                if(!strcmp(dirptr->name, ".") || !strcmp(dirptr->name, ".."))
                        continue;

                char buf[MAX_BUFF_SIZE] = {0};
                sprintf(buf, "%s%s", url, dirptr->name);
                struct stat info;
                smbc_stat(buf, &info);

                if(dirptr->smbc_type == SMBC_FILE) {
                        printf("FILE-------%s%s\n", url, dirptr->name);
                        printf("-----------mtime:%lu\n", info.st_mtime);
                        printf("-----------size :%lld\n", info.st_size);
                        printf("-----------mode :%d S_IDDIR :%d\n", info.st_mode, S_ISDIR(info.st_mode));
                }
                else if(dirptr->smbc_type == SMBC_DIR) {
                        printf("FOLDER-----%s%s\n", url, dirptr->name);
                        printf("-----------mode :%d S_IDDIR :%d\n", info.st_mode, S_ISDIR(info.st_mode));
                        char new_url[255] = {0};
                        sprintf(new_url, "%s%s/", url, dirptr->name);
                        search_smb(new_url);
                }
        }
        smbc_closedir(dh);
}

/* download file */
int SMB_download(char *serverpath, int index)
{
        char buffer[4096] = {0};
        int buflen;

        char *localpath = serverpath_to_localpath(serverpath, index);
        //char *localpath_td = my_malloc(strlen(localpath) + strlen(".asus.td") + 1);
        char *localpath_td = my_malloc(strlen(localpath) + 9);
        //2014.10.20 by sherry malloc申请内存是否成功
        //if(localpath_td==NULL)
          //  return NULL;
        sprintf(localpath_td, "%s%s", localpath, ".asus.td");

        write_log(S_DOWNLOAD, "", serverpath, index);

        unsigned long long halfsize = 0;
        int dst_fd;
        if(access(localpath_td, F_OK) != 0)
        {
                dst_fd = open(localpath_td, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if(dst_fd < 0){
                        printf("open() - %s failed\n", localpath_td);
                        return -1;
                }
        }
        else
        {
                dst_fd = open(localpath_td, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if(dst_fd < 0){
                        printf("open() - %s failed\n", localpath_td);
                        return -1;
                }
                halfsize = lseek(dst_fd, 0L, SEEK_END);
                lseek(dst_fd, halfsize, SEEK_SET);
        }

        SMB_init(index);
        int fd = -1;
        if((fd = smbc_open(serverpath, O_RDONLY, FILE_MODE)) > 0)
        {
                unsigned long long cli_filesize = 0;
                unsigned long long smb_filesize = 0;
                smb_filesize = smbc_lseek(fd, 0L, SEEK_END);
                smbc_lseek(fd, halfsize, SEEK_SET);
                while((buflen = smbc_read(fd, buffer, sizeof(buffer))) > 0 && exit_loop == 0)
                {
                    //2014.11.20 by sherry 判断是否write成功
                        write(dst_fd, buffer, buflen);
                        cli_filesize += buflen;
                        printf("\rDownload [%s] percent - %f ", serverpath, (float)cli_filesize/(float)smb_filesize);
                }
                if(cli_filesize == smb_filesize)
                {
                        rename(localpath_td, localpath);
                        free(localpath);
                        free(localpath_td);
                        smbc_close(fd);
                        close(dst_fd);
                }
                else
                {
                        free(localpath);
                        free(localpath_td);
                        smbc_close(fd);
                        close(dst_fd);
                        return -1;
                }
        }
        else
        {
                printf("smbc_open() - %s failed\n", serverpath);
                close(dst_fd);
                free(localpath);
                free(localpath_td);
                return COULD_NOT_CONNECNT_TO_SERVER;
        }
        return 0;
}

/* upload file */
int SMB_upload(char *localpath, int index)
{
        if(access(localpath, F_OK) != 0)
        {
                printf("Local has no %s\n", localpath);
                return LOCAL_FILE_LOST;
        }

        write_log(S_UPLOAD, "", localpath, index);

        SMB_init(index);

        int buflen;
        char buffer[4096] = {0};

        char *serverpath = localpath_to_serverpath(localpath, index);
        int smb_fd = smbc_open(serverpath, O_RDWR|O_CREAT, FILE_MODE);
        int cli_fd;
        if((cli_fd = open(localpath, O_RDONLY, FILE_MODE)) > 0)
        {//以只读的方式打开，文件
            DEBUG("open localpath sucess\n");
                unsigned long long cli_filesize = 0;
                unsigned long long smb_filesize = 0;
                cli_filesize = lseek(cli_fd, 0L, SEEK_END);
                lseek(cli_fd, 0L, SEEK_SET);//read or write 文件的偏移量
                //2014.11.19 by sherry 判断上传是否成功

                //buflen = read(cli_fd, buffer, sizeof(buffer));
                //DEBUG("buflen=%d\n",buflen);
                //while(buflen > 0 && exit_loop == 0)
                while((buflen = read(cli_fd, buffer, sizeof(buffer))) > 0 && exit_loop == 0)
                {
                        smb_filesize += buflen;
                        //2014.11.19 by sherry 判断上传是否成功
                        //smbc_write(smb_fd, buffer, buflen);//smb_fd为server端的文件
                        int res=0;
                        res=smbc_write(smb_fd, buffer, buflen);
                        if(res==-1)
                            return -1;

                        printf("\rUpload [%s] percent - %f ", localpath, (float)smb_filesize/(float)cli_filesize);
                }
                if(smb_filesize != cli_filesize && exit_loop != 0)
                {
                        DEBUG("smb_filesize != cli_filesize && exit_loop != 0");
                        FILE *f_stream = fopen(g_pSyncList[index]->up_item_file, "w");
                        fprintf(f_stream, "%s", localpath);
                        fclose(f_stream);
                }
                smbc_close(smb_fd);
                close(cli_fd);
                DEBUG("upload --end\n");
        }
        free(serverpath);
        return 0;
}

/* rename or move (file&folder) */
int SMB_remo(char *localoldpath, char *localnewpath, int index)
{
        SMB_init(index);
        char *serveroldpath, *servernewpath;
        serveroldpath = localpath_to_serverpath(localoldpath, index);
        servernewpath = localpath_to_serverpath(localnewpath, index);
        int res = smbc_rename(serveroldpath, servernewpath);
        if(res != 0)
        {
                if(!test_if_dir(localnewpath)){
                        res = SMB_upload(localnewpath, index);
                        if(res == 0)
                        {
                                time_t modtime = Getmodtime(servernewpath, index);
                                if(ChangeFile_modtime(localnewpath, modtime, index))
                                {
                                        printf("ChangeFile_modtime failed!\n");
                                }
                        }
                }else{
                        res = moveFolder(localoldpath, localnewpath, index);
                }
        }
        free(serveroldpath);
        free(servernewpath);
        return res;
}

int SMB_del(char *url, int index)
{
        SMB_init(index);
        int res = 0;
        int dh = 0;
        dh = smbc_opendir(url);
        printf("SMB_del() - dh = %d\n", dh);
        if(dh < 0){
                res = smbc_unlink(url);
        }
        else{
                struct smbc_dirent* dirptr;
                if(url[strlen(url) - 1] != '/')
                        strcat(url, "/");
                while((dirptr = smbc_readdir(dh)) != NULL){
                        if(!strcmp(dirptr->name, ".") || !strcmp(dirptr->name, ".."))
                                continue;

                        char new_url[255] = {0};
                        sprintf(new_url, "%s%s", url, dirptr->name);

                        printf("-------%s\n", new_url);

                        if(dirptr->smbc_type == SMBC_DIR) {
                                if(new_url[strlen(new_url) - 1] != '/')
                                        strcat(new_url, "/");
                                SMB_del(new_url, index);
                        }
                        else if(dirptr->smbc_type == SMBC_FILE) {
                                res = smbc_unlink(new_url);
                        }
                }
                res = smbc_rmdir(url);//删除成功返回0，失败返回-1
        }
        smbc_closedir(dh);
        return res;
}

/* unlink (file) */
int SMB_rm(char *localpath, int index)
{
        printf("SMB_rm() - %s start\n", localpath);
        int res = 0;
        int exsit = is_server_exist(localpath, index);
        if(!exsit)
                return res;
        char *serverpath = localpath_to_serverpath(localpath, index);
        res = SMB_del(serverpath, index);
        free(serverpath);
        return res;
}

/* mkdir (folder) */
int SMB_mkdir(char *localpath, int index)
{
    DEBUG("SMB_mkdir start\n");
        if(access(localpath, F_OK) != 0)
        {
                printf("Local has no %s\n", localpath);
                return LOCAL_FILE_LOST;
        }

        SMB_init(index);

        char *serverpath = localpath_to_serverpath(localpath, index);
        DEBUG("SMB_mkdir --serverpath=%s\n",serverpath);
        int res = smbc_mkdir(serverpath, DIR_MODE);
        DEBUG("SMB_mkdir --res=%d\n",res);
        free(serverpath);
        DEBUG("SMB_mkdir end\n");
        return res;
}

static void auth_fn(const char *server, const char *share, char *workgroup, int wgmaxlen,
                    char *username, int unmaxlen, char *password, int pwmaxlen)
{
        strncpy(workgroup, g_workgroup, wgmaxlen - 1);
        strncpy(username, g_username, unmaxlen - 1);
        strncpy(password, g_password, pwmaxlen - 1);
        strcpy(g_server, server);
        strcpy(g_share, share);
}

int SMB_init(int index)
{
        strcpy(g_workgroup, smb_config.multrule[index]->workgroup);
        strcpy(g_username, smb_config.multrule[index]->acount);
        strcpy(g_password, smb_config.multrule[index]->password);
        smbc_init(auth_fn, 0);
        return 0;
}
