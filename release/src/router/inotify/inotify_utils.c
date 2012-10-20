#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
//#include "data.h"
#include "event_queue.h"
#include "inotify_utils.h"
//#include "function.h"
//#include "api.h"

//#define DEBUG 1


extern int keep_running;
//extern int seq_number;
static int watched_items;
Folders allfolderlist;
extern Folders pathlist;
//char *username ="nasrouter1";
extern int need_restart_inotify;
extern char inotify_path[256];
extern char moved_from_file[256];
extern char moved_to_file[256];
int have_from_file;
int have_from_file_ex;
int is_modify;
int is_create_file;
int is_create_file_ex;
int is_add_folder;




struct my_inotify_event
{
    int wd;		/* Watch descriptor.  */
    uint32_t mask;	/* Watch mask.  */
    uint32_t cookie;	/* Cookie to synchronize two events.  */
    uint32_t len;		/* Length (including NULs) of name.  */
    char name[512] ;	/* Name.  */
};


int write_inotify(queue_entry_t event,const char *filename)
{
    struct my_inotify_event temp_event;
    int fd;
    int len;
    strcpy(temp_event.name,event->inot_ev.name);
    temp_event.cookie = event->inot_ev.cookie;
    temp_event.len = event->inot_ev.len;
    temp_event.mask = event->inot_ev.mask;
    temp_event.wd = event->inot_ev.wd;

    if((fd = open(filename,O_RDWR|O_CREAT|O_TRUNC, 0777))<0)
    {
        perror("Open inotify for write error\n");
        return -1;
    }
    else
    {
        len = write(fd, &temp_event, sizeof(struct my_inotify_event));
        close(fd);
    }

    return len;
}

int read_inotify(struct my_inotify_event *temp_event,const char *filename)
{
    //printf("enter read function \n");
    int fd;
    int len;

    if((fd = open(filename,O_RDONLY))<0)
    {
        printf("Open inotify for read error\n");
        return -1;
    }
    else
    {
        //printf("READY read \n");
        if(NULL == temp_event)
        {
            close(fd);
            return 1;
        }
        len = read(fd, temp_event, sizeof(struct my_inotify_event));
        close(fd);
    }

    //printf("read ok \n");
    return len;
}

/* Create an inotify instance and open a file descriptor
   to access it */
int open_inotify_fd ()
{
    int fd;

    watched_items = 0;
    fd = inotify_init ();

    if (fd < 0)
    {
        perror ("inotify_init () = ");
    }
    return fd;
}


/* Close the open file descriptor that was opened with inotify_init() */
int close_inotify_fd (int fd)
{
    int r;

    if ((r = close (fd)) < 0)
    {
        perror ("close (fd) = ");
    }

    watched_items = 0;
    return r;
}

int send_action(int type,char *content)
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char str[1024];
    int port;

    switch (type)
    {
    case 0:
        port = ASUSWEBSTORAGE_PORT;
        break;
    case 1:
        port = BOOKSNET_PORT;
        break;
    case 2:
        port = WEBDAV_PORT;
        break;
    case 3:
        port = SKYDRIVER_PORT;
        break;
    default:
        break;
    }

    //if(type == ASUSWEBSTORAGE)
    //port = ASUSWEBSTORAGE_PORT;
    /*
    switch (type)
    {
    case ASUSWEBSTORAGE :
         port = ASUSWEBSTORAGE_PORT;
         break;
    default:
         printf("error,no matching programe !\n");
         return -1;

    }
    */

    //struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */

    /*
    if ((he=gethostbyname(hostname)) == NULL) {
        herror("gethostbyname");
        exit(1);
    }
    */

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
        //exit(1);
    }
    //port = atoi(argv[2]);

    bzero(&(their_addr), sizeof(their_addr)); /* zero the rest of the struct */
    their_addr.sin_family = AF_INET; /* host byte order */
    their_addr.sin_port = htons(port); /* short, network byte order */
    their_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //their_addr.sin_addr.s_addr = ((struct in_addr *)(he->h_addr))->s_addr;
    bzero(&(their_addr.sin_zero), sizeof(their_addr.sin_zero)); /* zero the rest of the struct */
    if (connect(sockfd, (struct sockaddr *)&their_addr,sizeof(struct
                                                              sockaddr)) == -1) {
        perror("connect");
        //exit(1);
        return -1;
    }

    //strcpy(str, "add@1@./a01cb8f9f95acfad6539e30e75e196ed7715c672.torrent");
    strcpy(str,content);
    //strcpy(str, argv[3]);
    //printf("####send_action str :%s\n",str);
    if (send(sockfd, str, strlen(str), 0) == -1) {
        perror("send");
        //exit(1);
        return -1;
    }
    //printf("Start Receive\n");

    if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv");
        //exit(1);
        return -1;
    }

    //printf("Stop Receive\n");
    buf[numbytes] = '\0';
    //printf("Received: %s",buf);
    close(sockfd);
    return 0;
}

int test_if_dir(const char *dir){
    DIR *dp = opendir(dir);

    if(dp == NULL)
        return 0;

    closedir(dp);
    return 1;
}

int get_category(char *path)
{
    int i;
    for( i= 0; i < pathlist.number; i++)
    {
        char *p = NULL;
        p = strstr(path,pathlist.folderlist[i].name);
        if(p != NULL)
            return pathlist.folderlist[i].type;
    }

    return -1;
}

int test_if_download_temp_file(char *filename)
{
    char file_suffix[9];
    char *temp_suffix = ".asus.td";
    memset(file_suffix,0,sizeof(file_suffix));
    char *p = filename;

    if(strstr(filename,temp_suffix))
    {
        strcpy(file_suffix,p+(strlen(filename)-strlen(temp_suffix)));

        //printf(" %s file_suffix is %s\n",filename,file_suffix);

        if(!strcmp(file_suffix,temp_suffix))
            return 1;
    }

    return 0;

}

/* This method does the work of determining what happened,
   then allows us to act appropriately
*/
void handle_event (queue_entry_t event)
{
    //printf("********handle_event********\n");
    /* If the event was associated with a filename, we will store it here */
    char *cur_event_filename = NULL;
    char *cur_event_file_or_dir = NULL;
    /* This is the watch descriptor the event occurred on */
    int cur_event_wd = event->inot_ev.wd;
    int cur_event_cookie = event->inot_ev.cookie;
    unsigned long flags;

    //add by gauss
    struct my_inotify_event temp_event;
    int len;
    char fullname[512];
    //ItemID itemID;
    int isfolder;
    //Createfolder createfolder;
    char info[512];
    int type;
    //char type[64];
    char path[512];
    char old_path[512];
    int category = -1;

    memset(&temp_event,0,sizeof(struct my_inotify_event));
    memset(fullname,0,sizeof(fullname));
    memset(info,0,sizeof(info));
    //memset(type,0,sizeof(type));
    memset(path,0,sizeof(path));
    memset(old_path,0,sizeof(path));

    sprintf(path,"%s",allfolderlist.folderlist[cur_event_wd-1].name);

    //category = allfolderlist.folderlist[cur_event_wd-1].type;

    category = get_category(path);

    if(category == -1)
    {
        printf("inotify can't find path type\n");
        return ;
    }

    switch (category)
    {
    case 0:
        type = ASUSWEBSTORAGE;
        break;
    case 1:
        type = BOOKSNET;
        break;
    case 2:
        type = WEBDAV;
        break;
    case 3:
        type = SKYDRIVER;
        break;
    default:
        break;
    }

    //if(strstr(path,ASUSWEBSTORAGE_SYNCFOLDER))
    //type = ASUSWEBSTORAGE;
    //strcpy(type,"asuswebstorage");
    //memset(info)
    //memset(&itemID,0,sizeof(ItemID));
    //memset(&createfolder,0,sizeof(Createfolder));

    if (event->inot_ev.len)
    {
        cur_event_filename = event->inot_ev.name;
    }
    if ( event->inot_ev.mask & IN_ISDIR )
    {
        cur_event_file_or_dir = "Dir";
    }
    else
    {
        cur_event_file_or_dir = "File";
    }
    flags = event->inot_ev.mask &
            ~(IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED );

    /* Perform event dependent handler routines */
    /* The mask is the magic that tells us what file operation occurred */
    switch (event->inot_ev.mask &
            (IN_ALL_EVENTS | IN_UNMOUNT | IN_Q_OVERFLOW | IN_IGNORED))
    {
        /* File was accessed */
    case IN_ACCESS:

#ifdef DEBUG
        printf ("ACCESS: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif
        break;

        /* File was modified */
    case IN_MODIFY:
#ifdef DEBUG
        printf ("MODIFY: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif
        if( strstr(cur_event_filename,".goutputstream") ||
            strstr(cur_event_filename,"~gvf"))
        {
            if(is_modify != 1)
                is_modify = 1;
        }
        break;

        /* File changed attributes */
    case IN_ATTRIB:
#ifdef DEBUG
        printf ("ATTRIB: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif

        break;

        /* File open for writing was closed */
    case IN_CLOSE_WRITE:
#ifdef DEBUG
        printf ("CLOSE_WRITE: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif


#if 0
        if( is_modify == 1 )
        {
            /*
          if(is_create_file_ex == 1)
          {
              printf("###### %s has replace #######\n",temp_event.name);
              is_create_file_ex = 0;
          }
          else
          {
          */
            printf("###### %s has modify #######\n",temp_event.name);

            sprintf(info,"modify@%s@%s",path,cur_event_filename);
            send_action(type,info);

            ///}

            is_modify = 0 ;

        }
#endif
        if( is_create_file == 1 )
        {
            printf("###### %s create file has copy ending #######\n",temp_event.name);

            if(!test_if_download_temp_file(cur_event_filename))
            {
                //if(strrstr())
                //sprintf(fullname,"%s/%s",allfolderlist.folderlist[cur_event_wd-1].name,cur_event_filename);
                sprintf(info,"createfile@%s@%s",path,cur_event_filename);
                send_action(type,info);
            }
            //else
               // printf("ignore download temp file create socket\n");

            is_create_file = 0;
        }


        break;

        /* File open read-only was closed */
    case IN_CLOSE_NOWRITE:
#ifdef DEBUG
        printf ("CLOSE_NOWRITE: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif
        break;

        /* File was opened */
    case IN_OPEN:
#ifdef DEBUG
        printf ("OPEN: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif

        //printf("$$$$ before read $$$$\n");
        //printf("read len is %d \n",len);

        //isfolder = ( temp_event.mask & IN_ISDIR ) ? 1 : 0;

        if(have_from_file == 1)
        {
            len = read_inotify(&temp_event,moved_from_file);

            printf("read len is %d \n",len);

            if( len > 0 && (NULL == strstr(temp_event.name,".goutputstream") &&
                            NULL == strstr(temp_event.name,"~gvf")))
            {

                sprintf(fullname,"%s",allfolderlist.folderlist[temp_event.wd-1].name);
                printf("###### %s has put to recycle #######\n",fullname);

                if(!test_if_download_temp_file(fullname))
                {
                    sprintf(info,"remove@%s@%s",fullname,temp_event.name);
                    send_action(type,info);
                }

                remove(moved_from_file);
                have_from_file = 0;
            }
        }
        break;

        /* File was moved from X */
    case IN_MOVED_FROM:
#ifdef DEBUG
        printf ("MOVED_FROM: %s \"%s\" on WD #%i. Cookie=%d\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                cur_event_cookie);
#endif

        /*add by gauss case MOVED_FROM write to file*/

        if( NULL == strstr(cur_event_filename,".goutputstream") &&
            NULL == strstr(cur_event_filename,"~gvf"))
            //!test_if_download_temp_file(cur_event_filename)) //reject modify and replace
        {
            write_inotify(event,moved_from_file);
            have_from_file = 1;
        }

        //printf("write inotify ok\n");
        break;

        /* File was moved to X */
    case IN_MOVED_TO:
#ifdef DEBUG
        printf ("MOVED_TO: %s \"%s\" on WD #%i. Cookie=%d\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd,
                cur_event_cookie);;
#endif

        //

        if( is_modify == 1 )
        {
            printf("###### %s has modify #######\n",cur_event_filename);

            if(!test_if_download_temp_file(cur_event_filename))
            {
                sprintf(info,"modify@%s@%s",path,cur_event_filename);
                send_action(type,info);
            }

            is_modify = 0 ;
        }
        else
        {
            len = read_inotify(&temp_event,moved_from_file);

            //isfolder = ( temp_event.mask & IN_ISDIR ) ? 1 : 0;

            if(len > 0  )
            {    if( NULL == strstr(temp_event.name,".goutputstream") &&
                     NULL == strstr(temp_event.name,"~gvf") )
                {
                    if(temp_event.cookie == event->inot_ev.cookie)
                    {
                        /* rename  modify replace*/
                        if(temp_event.wd == event->inot_ev.wd)
                        {
                            printf("#########from %s RENAME to %s ##########\n",temp_event.name,cur_event_filename);
                            if(!test_if_download_temp_file(temp_event.name))
                            {
                                sprintf(info,"rename@%s@%s@%s",path,temp_event.name,cur_event_filename);

                                remove("../inotify");
                                send_action(type,info);

                                keep_running = 0;
                                need_restart_inotify = 1;
                            }
                            //else
                                //printf("ignore rename download temp file socket\n");
                        }
                        else
                        {
                            printf("#########move %s to %s ##########\n",temp_event.name,path);
                            if(!test_if_download_temp_file(temp_event.name))
                            {
                                sprintf(old_path,"%s",allfolderlist.folderlist[temp_event.wd-1].name);
                                sprintf(info,"move@%s@%s@%s",path,old_path,temp_event.name);
                                send_action(type,info);

                                keep_running = 0;
                                need_restart_inotify = 1;
                            }
                        }

                    }
                    remove(moved_from_file);
                    have_from_file = 0;
                }
            }
            else
            {   //if( NULL == strstr(cur_event_filename,".goutputstream") )
                if(have_from_file_ex != 1)
                    printf("#########drag %s to %s ##########\n",cur_event_filename,path);

                sprintf(fullname,"%s/%s",path,cur_event_filename);

                isfolder = test_if_dir(fullname);

                if(isfolder)
                {
                    sprintf(info,"dragfolder@%s@%s",path,cur_event_filename);
                    send_action(type,info);

                    keep_running = 0;
                    need_restart_inotify = 1;
                }
                else
                {
                    sprintf(info,"dragfile@%s@%s",path,cur_event_filename);
                    send_action(type,info);
                }
            }
        }

        break;

        /* Subdir or file was deleted */
    case IN_DELETE:
#ifdef DEBUG
        printf ("DELETE: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);

#endif

        /*
      if( is_modify == 1 )
      {
          printf("###### %s has modify #######\n",cur_event_filename);

          sprintf(info,"modify@%s@%s",path,cur_event_filename);
          send_action(type,info);
          is_modify = 0 ;

      }
      else
      */
        if(is_modify != 1)
        {
            printf("#########delete %s  ##########\n",cur_event_filename);

            if(!test_if_download_temp_file(cur_event_filename))
            {
                sprintf(info,"delete@%s@%s",path,cur_event_filename);
                send_action(type,info);
            }

        }

        break;

        /* Subdir or file was created */
    case IN_CREATE:
#ifdef DEBUG
        printf ("CREATE: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif


        //printf("listen dir is %s\n",allfolderlist.folderlist[cur_event_wd-1].name);

        if( NULL == strstr(cur_event_filename,".goutputstream") &&
            NULL == strstr(cur_event_filename,"~gvf") ) //reject modify and replace
        {
            printf("#########create %s ##########\n",cur_event_filename);

            //getID(cur_event_wd,1,"",&itemID);

            if( !strcmp(cur_event_file_or_dir,"Dir"))
            {
                printf("***********it is folder*************\n");
                sprintf(fullname,"%s/%s",path,cur_event_filename);
                sprintf(info,"createfolder@%s@%s",path,cur_event_filename);
                send_action(type,info);

                keep_running = 0;
                need_restart_inotify = 1;
            }
            else
            {
                if(!test_if_download_temp_file(cur_event_filename))
                {
                    sprintf(fullname,"%s/%s",path,cur_event_filename);
                    sprintf(info,"copyfile@%s@%s",path,cur_event_filename);
                    send_action(type,info);
                    is_create_file = 1;
                }
                //else
                    //printf("ignore download temp file copy socket\n");

            }
        }
        else
        {
            is_create_file_ex = 1;
        }
        break;

        /* Watched entry was deleted */
    case IN_DELETE_SELF:
#ifdef DEBUG
        printf ("DELETE_SELF: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif
        break;

        /* Watched entry was moved */
    case IN_MOVE_SELF:
#ifdef DEBUG
        printf ("MOVE_SELF: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif
        break;

        /* Backing FS was unmounted */
    case IN_UNMOUNT:
#ifdef DEBUG
        printf ("UNMOUNT: %s \"%s\" on WD #%i\n",
                cur_event_file_or_dir, cur_event_filename, cur_event_wd);
#endif
        break;

        /* Too many FS events were received without reading them
         some event notifications were potentially lost.  */
    case IN_Q_OVERFLOW:
#ifdef DEBUG
        printf ("Warning: AN OVERFLOW EVENT OCCURRED: \n");
#endif
        break;

        /* Watch was removed explicitly by inotify_rm_watch or automatically
         because file was deleted, or file system was unmounted.  */
    case IN_IGNORED:
        watched_items--;
        printf ("IGNORED: WD #%d\n", cur_event_wd);
        printf("Watching = %d items\n",watched_items);
        break;

        /* Some unknown message received */
    default:
#ifdef DEBUG
        printf ("UNKNOWN EVENT \"%X\" OCCURRED for file \"%s\" on WD #%i\n",
                event->inot_ev.mask, cur_event_filename, cur_event_wd);
#endif
        break;
    }
    /* If any flags were set other than IN_ISDIR, report the flags */
    if (flags & (~IN_ISDIR))
    {
        flags = event->inot_ev.mask;
        printf ("Flags=%lX\n", flags);
    }
}

void handle_events (queue_t q)
{
    queue_entry_t event;
    while (!queue_empty (q))
    {
        event = queue_dequeue (q);
        handle_event (event);
        free (event);
    }
}

int read_events (queue_t q, int fd)
{
    char buffer[16384];
    size_t buffer_i;
    struct inotify_event *pevent;
    queue_entry_t event;
    ssize_t r;
    size_t event_size, q_event_size;
    int count = 0;

    r = read (fd, buffer, 16384);
    if (r <= 0)
        return r;
    buffer_i = 0;
    while (buffer_i < r)
    {
        /* Parse events and queue them. */
        pevent = (struct inotify_event *) &buffer[buffer_i];
        event_size =  offsetof (struct inotify_event, name) + pevent->len;
        q_event_size = offsetof (struct queue_entry, inot_ev.name) + pevent->len;
        event = malloc (q_event_size);
        memmove (&(event->inot_ev), pevent, event_size);
        queue_enqueue (event, q);
        buffer_i += event_size;
        count++;
    }
    //printf ("\n%d events queued\n", count);
    return count;
}

int event_check (int fd)
{
    fd_set rfds;
    FD_ZERO (&rfds);
    FD_SET (fd, &rfds);
    /* Wait until an event happens or we get interrupted
     by a signal that we catch */
    return select (FD_SETSIZE, &rfds, NULL, NULL, NULL);
}

int process_inotify_events (queue_t q, int fd)
{
    while (keep_running && (watched_items > 0))
    {
        if (event_check (fd) > 0)
	{
            int r;
            r = read_events (q, fd);
            if (r < 0)
	    {
                break;
	    }
            else
	    {
                handle_events (q);
	    }
	}
    }
    return 0;
}

int watch_dir (int fd, const char *dirname, unsigned long mask)
{
    int wd;
    wd = inotify_add_watch (fd, dirname, mask);
    if (wd < 0)
    {
        printf ("Cannot add watch for \"%s\" with event mask %lX", dirname,
                mask);
        fflush (stdout);
        perror (" ");
    }
    else
    {
        watched_items++;
        printf ("Watching %s WD=%d\n", dirname, wd);
        printf ("Watching = %d items\n", watched_items);
    }
    return wd;
}

int ignore_wd (int fd, int wd)
{
    int r;
    r = inotify_rm_watch (fd, wd);
    if (r < 0)
    {
        perror ("inotify_rm_watch(fd, wd) = ");
    }
    else
    {
        watched_items--;
    }
    return r;
}
