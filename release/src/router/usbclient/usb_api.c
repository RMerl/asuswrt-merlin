#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "SMB.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int exit_loop;
extern char g_workgroup[MAX_BUFF_SIZE];
extern char g_username[MAX_BUFF_SIZE];
extern char g_password[MAX_BUFF_SIZE];
extern char g_server[MAX_BUFF_SIZE];
extern char g_share[MAX_BUFF_SIZE];
extern Config smb_config;
extern sync_list **g_pSyncList;


/* mkdir (folder) */
int USB_mkdir(char *localpath, int index)
{
    DEBUG("USB_mkdir start\n");
        if(access(localpath, F_OK) != 0)
        {
                printf("Local has no %s\n", localpath);
                return LOCAL_FILE_LOST;
        }

        char *serverpath = localpath_to_serverpath(localpath, index);
        DEBUG("USB_mkdir --serverpath=%s\n",serverpath);
        int res = mkdir(serverpath, DIR_MODE);
        DEBUG("USB_mkdir --res=%d\n",res);
        free(serverpath);
        DEBUG("USB_mkdir end\n");
        return res;
}

/* unlink (file) */
int USB_rm(char *localpath, int index)
{
        printf("USB_rm() - %s start\n", localpath);
        int res = 0;
        int exsit = is_server_exist(localpath, index);
        if(!exsit)
                return res;
        char *serverpath = localpath_to_serverpath(localpath, index);
        res = remove(serverpath);
        free(serverpath);
        return res;
}


/* download file */
int USB_download(char *serverpath, int index)
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

        int fd = -1;
        if((fd = open(serverpath, O_RDONLY, FILE_MODE)) > 0)
        {
                unsigned long long cli_filesize = 0;
                unsigned long long smb_filesize = 0;
                smb_filesize = lseek(fd, 0L, SEEK_END);
                lseek(fd, halfsize, SEEK_SET);
                while((buflen = read(fd, buffer, sizeof(buffer))) > 0 && exit_loop == 0)
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
                        close(fd);
                        close(dst_fd);
                }
                else
                {
                        free(localpath);
                        free(localpath_td);
                        close(fd);
                        close(dst_fd);
                        return -1;
                }
        }
        else
        {
                printf("open() - %s failed\n", serverpath);
                close(dst_fd);
                free(localpath);
                free(localpath_td);
                return COULD_NOT_CONNECNT_TO_SERVER;
        }
        return 0;
}


/* upload file */
int USB_upload(char *localpath, int index)
{
    if(access(localpath, F_OK) != 0)
    {
            printf("Local has no %s\n", localpath);
            return LOCAL_FILE_LOST;
    }

    write_log(S_UPLOAD, "", localpath, index);
    int buflen;
    char buffer[4096] = {0};

    char *serverpath = localpath_to_serverpath(localpath, index);
    FILE *smb_fd = fopen(serverpath, "w+");
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
                    res=fwrite(buffer, buflen, 1, smb_fd);
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
            fclose(smb_fd);
            close(cli_fd);
            DEBUG("upload --end\n");
    }
    free(serverpath);
    return 0;

        return 0;
}

/* rename or move (file&folder) */
int USB_remo(char *localoldpath, char *localnewpath, int index)
{
        char *serveroldpath, *servernewpath;
        serveroldpath = localpath_to_serverpath(localoldpath, index);
        servernewpath = localpath_to_serverpath(localnewpath, index);
        int res = rename(serveroldpath, servernewpath);
        if(res != 0)
        {
                if(!test_if_dir(localnewpath)){
                        res = USB_upload(localnewpath, index);
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
