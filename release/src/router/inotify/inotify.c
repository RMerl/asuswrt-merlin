#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include "event_queue.h"
#include "inotify_utils.h"

#define inotify_folder "../inotify_file"
#define MYPORT 3678
#define BACKLOG 10 /* max listen num*/
#define PC 0
#define CONFIG_GER_PATH "/home/gauss/asuswebstorage/etc/Cloud.conf"
//#define CONFIG_GER_PATH "/opt/etc/Cloud.conf"

int keep_running;
int need_restart_inotify;
int add_new_watch_item;
//int have_from_file;
//int have_from_file_ex;
//int is_modify;
Folders allfolderlist;
Paths pathlist;
//char pathlist[][256];
#if PC
char *base_path = "/home/gauss/optware_bulid";
#else
char base_path[256];
#endif


char inotify_path[256];
char moved_from_file[256];
char moved_to_file[256];
int ready = 0;



int get_all_folders(const char *dirname,Folders *allfolderlist,int offset);
void signal_handler (int signum);
int inotifyStart();
void watch_socket();
void watch_folder();
void cmd_parser(char *path);
void initConfig(char *path);
void initConfig2(char *path);
void my_mkdir_r(char *path);
char  *get_mount_path(char *path , int n);



int main(int argc, char *argv[])
{
   //base_path = argv[1];

    //initConfig2(CONFIG_GER_PATH);


    /*
    if(argc < 2)
    {
        printf("usage: inotify path\n");
        return -1;
    }
    */


   strcpy(base_path,"/tmp/Cloud");

   //printf("inotify base path is %s\n",base_path);

   //if(NULL == opendir(base_path))
       //mkdir(base_path,0777);

   sprintf(inotify_path,"%s/%s",base_path,"inotify_file");

   my_mkdir_r(inotify_path);

   sprintf(moved_from_file,"%s/%s",inotify_path,"moved_from");
   sprintf(moved_to_file,"%s/%s",inotify_path,"moved_to");

  //if(NULL == opendir(BASIC_PATH))
      //mkdir(BASIC_PATH,0777);

  if(NULL == opendir(inotify_path))
      mkdir(inotify_path,0777);

  keep_running = 1;


  /* Set a ctrl-c signal handler */
  if (signal (SIGINT, signal_handler) == SIG_IGN)
  {
      /* Reset to SIG_IGN (ignore) if that was the prior state */
      signal (SIGINT, SIG_IGN);
  }

  pthread_t newthid1,newthid2;

  if( pthread_create(&newthid1,NULL,(void *)watch_socket,NULL) != 0)
  {
      printf("thread creation failder\n");
      exit(1);
  }

  if( pthread_create(&newthid2,NULL,(void *)watch_folder,NULL) != 0)
  {
      printf("thread creation failder\n");
      exit(1);
  }
  //sleep(1);

  pthread_join(newthid2,NULL);

  return 0;
}

int inotifyStart()
{
    int inotify_fd;
    int i;
    //Folders allfolderlist;

    keep_running = 1;
    need_restart_inotify = 0;
    add_new_watch_item = 0;

    /* First we open the inotify dev entry */
    inotify_fd = open_inotify_fd ();
    if (inotify_fd > 0)
    {

        queue_t q;
        q = queue_create (128);

        int wd;

        wd = 0;

        memset(&allfolderlist,0,sizeof(Folders));

        //printf("list num is %d\n",pathlist.number);

        for( i = 0 ; i < pathlist.number; i++)
        {
            //printf("list is %s\n",pathlist.folderlist[i].name);
            get_all_folders(pathlist.folderlist[i].name,&allfolderlist,allfolderlist.number);
        }

       //for( i = 0 ; i < number; i++)uploadFile


        //strcpy(allfolderlist.folderlist[0].name,dirname);
        //allfolderlist.number = 1;



        printf("\n");
        for( i = 0; (i<allfolderlist.number) && (wd >= 0);i++)
        {
            wd = watch_dir (inotify_fd, allfolderlist.folderlist[i].name, IN_ALL_EVENTS);
            //printf("minitor path is %s\n",allfolderlist.folderlist[i].name);
        }

        if (wd > 0)
        {

            process_inotify_events (q, inotify_fd);
        }
        printf ("\nTerminating\n");
        close_inotify_fd (inotify_fd);
        queue_destroy (q);

        if(need_restart_inotify == 1)
            inotifyStart();
    }
    return 0;
}


//int get_all_folders(char *dirname,int *num,char **folder_list)
int get_all_folders(const char *dirname,Folders *allfolderlist,int offset)
{
    struct dirent* ent = NULL;
    DIR *pDir;
    char temp_dir[1024];
    int num ;

    //printf("dis %s\n",dirname);

#if 0
    if(offset == 0)
    {
        strcpy(allfolderlist->folderlist[0].name,dirname);

     }
    else if(offset > 0)
        strcpy(allfolderlist->folderlist[offset].name,dirname);
#endif

    if(offset != -1)
    {
        strcpy(allfolderlist->folderlist[offset].name,dirname);
        allfolderlist->number++;
    }



    //allfolderlist->folderlist[0].name

    //if(offset != -1)
        //allfolderlist->number++;

    //allfolderlist.number = 1;

    pDir=opendir(dirname);
    if(pDir != NULL )
    {
        while (NULL != (ent=readdir(pDir)))
        {
            //printf("ent->d_name is %s\n",ent->d_name);

            if(ent->d_name[0] == '.')
                continue;

            if( !strcmp(dirname,mount_path) && !strcmp(ent->d_name,"smartcloud") )
            {
                continue;
            }

            if(ent->d_type == DT_DIR)
            {
               num = allfolderlist->number;
               memset(temp_dir,0,sizeof(temp_dir));
               sprintf(temp_dir,"%s/%s",dirname,ent->d_name);

               strcpy(allfolderlist->folderlist[num].name,temp_dir);
               //printf("folder name is %s,num is %d\n",temp_dir,num);

               allfolderlist->number++;
               get_all_folders(temp_dir,allfolderlist,-1);
            }

        }
    closedir(pDir);
    }
    else
        printf("open %s fail \n",dirname);

  return 0;
}

/* Signal handler that simply resets a flag to cause termination */
void signal_handler (int signum)
{
  keep_running = 0;
  //pthread_exit(0);
}

void watch_socket()
{
    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd*/
    int numbytes;
    char buf[MAXDATASIZE];
    int yes;

    struct sockaddr_in my_addr; /* my address information */
    struct sockaddr_in their_addr; /* connector's address information */
    int sin_size;
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

    while(1) { /* main accept() loop */

             //if( keep_running == 0 )
                 //pthread_exit(0);
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, \
                             &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        //printf("server: got connection from %s\n", \
               //inet_ntoa(their_addr.sin_addr));

        memset(buf, 0, sizeof(buf));

        if ((numbytes=recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
            perror("recv");
            continue;
            ///exit(1);
        }
        //else
          //printf("recv command is %s \n",buf);

        cmd_parser(buf);


        /*
        memset(buf, 0, sizeof(buf));
        strcpy(buf, "rev cmd");
        if (send(new_fd, buf, strlen(buf), 0) == -1) {
            perror("send");
            exit(1);
        }
        */

        close(new_fd);
        //while(waitpid(-1,NULL,WNOHANG) > 0);
    }
}

void watch_folder()
{
    while(1)
    {

        if( pathlist.number > 0 && add_new_watch_item == 1)
        {
             //printf("restart inotifystart\n");
             keep_running = 0;
             inotifyStart();
        }

        sleep(1);
        //if(keep_running == 0)
            //pthread_exit(0);

    }
}

int check_path_exist(int type,char *path)
{
    int i;
    int category = -1;

    for(i=0;i<pathlist.number;i++)
    {
        category = pathlist.folderlist[i].type;
        if(category == type)
        {
           if( strcmp(pathlist.folderlist[i].name,path) == 0)
               return 1;
           else
           {
               memset(pathlist.folderlist[i].name,0,sizeof(pathlist.folderlist[i].name));
               strcpy(pathlist.folderlist[i].name,path);
               return 2;
           }
        }
    }

    return 0;
}

void cmd_parser(char *content)
{
   //printf("\n path is %s\n",path);

   int type;
   char path[256];
   char temp[512];
   int len;
   int i;
   int num = pathlist.number;
   const char *split = "@";
   char *p;
   int status;
   char *m_path = NULL;

   memset(path,0,sizeof(path));
   memset(temp,0,sizeof(temp));

   p=strtok(content,split);
   int j=0;
   while(p!=NULL)
   {
       switch (j)
       {
       case 0 :
           type = atoi(p);
           break;
       case 1:
            strcpy(path,p);
            break;
       default:
           break;
       }

       j++;
       p=strtok(NULL,split);
   }

   //printf("inotify type is %d,path is %s\n",type,path);

   len = strlen(path);
   if(path[len - 1] == '/')
         strncpy(temp,path, len - 1 );
   else
         strcpy(temp,path);


   status = check_path_exist(type,temp);

   if(type == ASUSWEBSTORAGE && strlen(mount_path) == 0)
   {
       m_path = get_mount_path(temp,4);

      if(NULL == m_path)
      {
          printf("get mount path fail from %s\n",temp);
      }
      else
      {
          memset(mount_path,0,sizeof(mount_path));
          strcpy(mount_path,m_path);
          free(m_path);
      }
   }



   switch (status)
   {
   case 0: // path is new add
       strcpy(pathlist.folderlist[num].name,temp);
       pathlist.folderlist[num].type = type;
       pathlist.number++;
       add_new_watch_item = 1;

       break;
   case 1:    // path has exist;
        break;
   case 2:     //update exist path;
       add_new_watch_item = 1;
   default:
       break;
   }

  /*
  if( check_path_exist(temp) == 0 )
   {
      strcpy(pathlist.folderlist[num].name,temp);
      pathlist.number++;
      add_new_watch_item = 1;
   }
   */

   for( i = 0; i <pathlist.number;i++)
      printf("folder is %s \n",pathlist.folderlist[i].name);
}

void initConfig(char *path)
{
    int fd, len, i=0,name_len;
    name_len=strlen(path);
    //printf("name lenth is %d\n",name_len);
    char ch, tmp[256], name[256], content[256];
    memset(tmp, 0, sizeof(tmp));
    memset(name, 0, sizeof(name));
    memset(content, 0, sizeof(content));

    if((fd = open(path, O_RDONLY | O_NONBLOCK)) < 0)
    {
        printf("\nread log error!\n");
    }
    else
    {

            while((len = read(fd, &ch, 1)) > 0)
            {
                if(ch == ':')
                {
                    strcpy(name, tmp);
                    //printf("name is %s\n",name);
                    memset(tmp, 0, sizeof(tmp));
                    i = 0;
                    continue;
                }
                else if(ch == '\n')
                {
                    strcpy(content, tmp);
                    //printf("content is [%s] \n",content);
                    memset(tmp, 0, sizeof(tmp));
                    i = 0;

                    if(strcmp(name, "BASE_PATH") == 0)
                    {
                        strcpy(base_path, content);
                    }
                    continue;
                }


                memcpy(tmp+i, &ch, 1);
                i++;
            }
            close(fd);
      }

}

void initConfig2(char *path)
{
    FILE *fp;

    char buffer[256];
    const char *split = ",";
    char *p;

    memset(buffer, '\0', sizeof(buffer));

    if (access(path,0) == 0)
    {
        if(( fp = fopen(path,"rb"))==NULL)
        {
            fprintf(stderr,"read Cloud error");
        }
        while(fgets(buffer,256,fp)!=NULL)
        {
            p=strtok(buffer,split);
            int i=0;
            while(p!=NULL)
            {
                switch (i)
                {
                case 0 :
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    break;
                case 6:
                    //strncpy(base_path,p,strlen(p));
                    strcpy(base_path,p);
                    if( base_path[ strlen(base_path)-1 ] == '\n' )
                            base_path[ strlen(base_path)-1 ] = '\0';
                    break;
                default:
                    break;
                }

                i++;
                p=strtok(NULL,split);
            }


        }

        fclose(fp);
    }

    if(NULL == opendir(base_path))
        mkdir(base_path,0777);

    printf("base_path is %s\n",base_path);

}

void my_mkdir(char *path)
{
    //char error_message[256];
    if(NULL == opendir(path))
    {
        if(-1 == mkdir(path,0777))
        {
            printf("mkdir %s fail",path);
            //handle_error(S_MKDIR_FAIL,error_message);
            exit(-1);
        }
    }
}

void my_mkdir_r(char *path)
{
   int i,len;
   char str[512];

   strncpy(str,path,512);
   len = strlen(str);
   for(i=0; i < len ; i++)
   {
       if(str[i] == '/' && i != 0)
       {
           str[i] = '\0';
           if(access(str,F_OK) != 0)
           {
            my_mkdir(str);
           }
           str[i] = '/';
       }
   }

   if(len > 0 && access(str,F_OK) != 0)
   {
       my_mkdir(str);
   }

}

char  *get_mount_path(char *path , int n)
{
    int i;
    char *m_path = NULL;
    m_path = (char *)malloc(sizeof(char)*256);
    memset(m_path,0,256);

    //memset(info_dest_path,'\0',sizeof(info_dest_path));

    char *new_path = NULL;
    new_path = path;

    for(i= 0;i< n ;i++)
    {
        new_path = strchr(new_path,'/');
        if(new_path == NULL)
            break;
        new_path++;
    }

    if( i > 3)
        strncpy(m_path,path,strlen(path)-strlen(new_path)-1);
    else
        strcpy(m_path,path);

    printf("mount path is [%s]\n",m_path);

    //return m_path;
    //strcpy(info_dest_path,new_path);

    return m_path;
}
