#include<stdio.h>
#include<stdlib.h>
#include"data.h"



#define MYPORT 3570
#define INOTIFY_PORT 3678
#define BACKLOG 100 /* max listen num*/
int no_config;
int exit_loop = 0;
int stop_progress;
char username[256];
char password[256];

int is_renamed = 1;
int access_token_expired;

char *Clientfp="*";
double start_time;

#define MAXSIZE 512

int write_trans_excep_log(char *fullname,int type,char *msg)
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


    //printf("trans_excep_file=%s\n",trans_excep_file);

    if(access(trans_excep_file,0) == 0)
        fp = fopen(trans_excep_file,"a");
    else
        fp = fopen(trans_excep_file,"w");


    if(fp == NULL)
    {
        printf("open %s fail\n",trans_excep_file);
        return -1;
    }

    //len = strlen(mount_path);
    fprintf(fp,"TYPE:%s\nUSERNAME:%s\nFILENAME:%s\nMESSAGE:%s\n",ctype,username,fullname,msg);
    //fprintf(fp,"ERR_CODE:%d\nMOUNT_PATH:%s\nFILENAME:%s\nRULENUM:%d\n",
                //err_code,mount_path,fullname+len,0);
    fclose(fp);
    return 0;
}

//int write_conflict_log(char *prename, char *conflict_name,int index)
//{
//    //wd_DEBUG("oldname=%s,newname=%s\n",prename,conflict_name);
//    FILE *fp;

//    if(access(g_pSyncList[index]->conflict_file,F_OK))
//    {
//        fp = fopen(g_pSyncList[index]->conflict_file,"w");
//    }
//    else
//    {
//        fp = fopen(g_pSyncList[index]->conflict_file,"a");
//    }

//     if(NULL == fp)
//     {
//         printf("open %s failed\n",g_pSyncList[index]->conflict_file);
//         return -1;
//     }

//     fprintf(fp,"%s is download from server,%s is local file and rename from %s\n",prename,conflict_name,prename);
//     fclose(fp);

//     return 0;
//}

char *write_error_message(char *format,...)
{
    int size=256;
    char *p=(char *)malloc(size);
    memset(p,0,size);
    va_list ap;
    va_start(ap,format);
    vsnprintf(p,size,format,ap);
    va_end(ap);
    return p;
}

#define GMTOFF(t) ((t).tm_gmtoff)

static const char short_months[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* RFC1123: Sun, 06 Nov 1994 08:49:37 GMT */
#define RFC1123_FORMAT "%3s, %02d %3s %4d %02d:%02d:%02d GMT"

time_t ne_rfc1123_parse(const char *date)
{
    struct tm gmt = {0};
    char wkday[4], mon[4];
    int n;
    time_t result;

    /*  it goes: Sun, 06 Nov 1994 08:49:37 GMT */
    n = sscanf(date, RFC1123_FORMAT,
               wkday, &gmt.tm_mday, mon, &gmt.tm_year, &gmt.tm_hour,
               &gmt.tm_min, &gmt.tm_sec);
    /* Is it portable to check n==7 here? */
    gmt.tm_year -= 1900;
    for (n=0; n<12; n++)
        if (strcmp(mon, short_months[n]) == 0)
            break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    gmt.tm_isdst = -1;
    result = mktime(&gmt);
    //printf("%ld\n",GMTOFF(gmt));
    return result + GMTOFF(gmt);
}

void buffer_free(char *b) {
        if (!b) return;

        free(b);
        b = NULL;
}

char *parse_name_from_path(const char *path)
{
    char *name;
    char *p;

    p = strrchr(path,'/');

    if( p == NULL)
    {
        //free(name);
        return NULL;
    }

    p++;

    name = (char *)malloc(sizeof(char)*(strlen(p)+2));
    memset(name,0,strlen(p)+2);

    strcpy(name,p);

    return name;
}
void cjson_change_to_cloudfile(cJSON *q)
{
    //printf("q->string=%s\n",q->string);
    if(strcmp(q->string,"bytes")==0)
    {
        FolderTmp->size=q->valueint;
        //wd_DEBUG("%s:%lld\n",q->string,FolderTmp->size);
    }
    else if(strcmp(q->string,"path")==0)
    {
        FolderTmp->href=(char *)malloc(sizeof(char)*(strlen(q->valuestring)+1));
        strcpy(FolderTmp->href,q->valuestring);
        FolderTmp->name=parse_name_from_path(FolderTmp->href);
        //wd_DEBUG("%s:%s\n",q->string,FolderTmp->href);
        //wd_DEBUG("%s:%s\n",q->string,FolderTmp->name);
    }
    else if(strcmp(q->string,"is_dir")==0)
    {
        FolderTmp->isFolder=q->type;
        //wd_DEBUG("%s:%d\n",q->string,FolderTmp->isFolder);
    }
    else if(strcmp(q->string,"modified")==0)
    {
        strcpy(FolderTmp->tmptime,q->valuestring);
        FolderTmp->mtime=ne_rfc1123_parse(FolderTmp->tmptime);
        //wd_DEBUG("%s:%s\n",q->string,FolderTmp->tmptime);
    }
}
char *cJSON_parse_name(cJSON *json)
{
    char *name;
    cJSON *q;
    q=json->child;
    int i=0;
    while(q!=NULL)
    {
        if(strcmp(q->string,"path")==0)
        {
            wd_DEBUG("path : %s\n",q->valuestring);
            //name=malloc(strlen(q->valuestring)+1);
            //name=parse_name_from_path(q->valuestring);
            //printf("name : %s\n",name);
            return q->valuestring;
        }
        q=q->next;
    }
}
time_t cJSON_printf_expires(cJSON *json)
{
    if(json)
    {
        time_t mtime=(time_t)-1;
        cJSON *q;
        q=json->child;
        while(q!=NULL)
        {
            if(strcmp(q->string,"expires") == 0)
            {
                mtime=ne_rfc1123_parse(q->valuestring);
            }
            q=q->next;
        }
        return mtime;
    }
    else
        return (time_t)-1;
}

int cJSON_printf_dir(cJSON *json)
{
    int flag = 0;

    cJSON *q;
    q=json->child;
    while(q!=NULL)
    {
        if(strcmp(q->string,"is_deleted") == 0)
        {
            flag = q->type;
            return flag;
        }
        q=q->next;
    }
    return flag;

}

time_t cJSON_printf_mtime(cJSON *json)
{
    if(json)
    {
        time_t mtime=(time_t)-1;
        cJSON *q;
        q=json->child;
        while(q!=NULL)
        {
            if(strcmp(q->string,"modified") == 0)
            {
                mtime=ne_rfc1123_parse(q->valuestring);
            }
            else if(strcmp(q->string,"is_deleted") == 0)
            {
                if(q->type)
                    return (time_t)-1;
            }
            q=q->next;
        }
        return mtime;
    }
    else
        return (time_t)-1;
}
/*
 FileTail_one is not only file list ,it is also folder list
*/
int cJSON_printf_one(cJSON *json)
{
    if(json)
    {
        cJSON *p,*q,*m;
        q=json->child;
        int i=0;
        while(q!=NULL)
        {
            //printf("%s:\n",q->string);
            if(strcmp(q->string,"contents")==0)
            {
                if(q->child!=NULL){
                    p=q->child;m=p->child;
                    while(p!=NULL)
                    {
                        FolderTmp = (CloudFile *)malloc(sizeof(CloudFile));
                        memset(FolderTmp,0,sizeof(CloudFile));
                        FolderTmp->href=NULL;

                        m=p->child;i++;
                        while(m!=NULL)
                        {
                            //printf("%s:\n",m->string);
                            cjson_change_to_cloudfile(m);
                            m=m->next;
                        }
                        //if(FolderTmp->isFolder==0)
                        //{
                        FileTail_one->next = FolderTmp;
                        FileTail_one = FolderTmp;
                        FileTail_one->next = NULL;
                        //}
                        p=p->next;
                    }
                    break;
                }
                else
                    wd_DEBUG("this is empty folder\n");
            }
            q=q->next;
        }
    }

}
time_t cJSON_printf(cJSON *json,char *string)
{
    if(json)
    {
        cJSON *p,*q,*m;
        q=json->child;
        int i=0;
        while(q!=NULL)
        {
            //printf("%s:\n",q->string);
            if(strcmp(q->string,"contents")==0)
            {
                if(strcmp(string,"contents")==0)
                {
                    if(q->child!=NULL){
                        p=q->child;m=p->child;
                        while(p!=NULL)
                        {
                            FolderTmp = (CloudFile *)malloc(sizeof(CloudFile));
                            memset(FolderTmp,0,sizeof(CloudFile));
                            FolderTmp->href=NULL;

                            m=p->child;i++;
                            while(m!=NULL)
                            {
                                //printf("%s:\n",m->string);
                                cjson_change_to_cloudfile(m);
                                m=m->next;
                            }
                            if(FolderTmp->isFolder==0)
                            {
                                TreeFileTail->next = FolderTmp;
                                TreeFileTail = FolderTmp;
                                TreeFileTail->next = NULL;
                            }
                            else if(FolderTmp->isFolder==1)
                            {
                                TreeFolderTail->next = FolderTmp;
                                TreeFolderTail = FolderTmp;
                                TreeFolderTail->next = NULL;
                            }
                            p=p->next;
                        }
                        break;
                    }
                    else
                    {
                        //wd_DEBUG("this is empty folder\n");
                    }
                }
            }
            else if(strcmp(q->string,"modified") == 0)
            {
                if(strcmp(string,"modified") == 0)
                {
                    time_t mtime=ne_rfc1123_parse(q->valuestring);
                    return mtime;
                }
            }
            else if(strcmp(q->string,"quota_info") == 0)
            {
                //wd_DEBUG("q->string = %s\n",q->string);
                if(strcmp(string,"quota_info") == 0)
                {
                    if(q->child != NULL)
                    {
                        p=q->child;m=p->child;
                        while(p!=NULL)
                        {
                            //wd_DEBUG("p->string = %s\n",p->string);
                            cjson_to_space(p);
                            p=p->next;
                        }
                    }
                }
            }
            q=q->next;
        }
    }
    else
        return (time_t)-1;

}

void cjson_to_space(cJSON *q)
{
    //printf("q->string = %s,q->valueint = %d,q->valuedouble = %lf,q->valuelong = %lld\n",q->string,q->valueint,q->valuedouble,q->valuelong);
    if(strcmp(q->string,"shared") == 0)
    {
        server_shared=q->valuelong;
    }
    else if(strcmp(q->string,"quota") == 0)
    {
        server_quota=q->valuelong;
    }
    else if(strcmp(q->string,"normal") == 0)
    {
        server_normal=q->valuelong;
    }
}
cJSON *doit(char *text)
{
    char *out;cJSON *json;

    json=cJSON_Parse(text);
    if (!json) {wd_DEBUG("Error before: [%s]\n",cJSON_GetErrorPtr());return NULL;}
    else
    {
        return json;
        //cJSON_printf(json);
        //cJSON_Delete(json);
        //printf("%s\n",out);
        //free(out);
    }
}

/* Read a file, parse, render back, etc. */
cJSON *dofile(char *filename)
{
    cJSON *json;
    FILE *f=fopen(filename,"rb");fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);
    char *data=malloc(len+1);fread(data,1,len,f);fclose(f);
    json=doit(data);
    free(data);
    if(json)
        return json;
    else
        return NULL;
}
#ifdef OAuth1
int open_login_page_first()
{
    char Myurl[MAXSIZE];
    memset(Myurl,0,sizeof(Myurl));

    //const char url[]="https://www.dropbox.com/login?cont=https%3A//www.dropbox.com/1/oauth/authorize%3Foauth_token%3Dss4olgz45siiwdd&signup_tag=oauth&signup_data=177967";
    sprintf(Myurl,"%s","https://www.dropbox.com/login");
    //sprintf(Myurl,"%s%s%s","https://www.dropbox.com/login?cont=https%3A//www.dropbox.com/1/oauth/authorize%3Foauth_token\%3D",auth->tmp_oauth_token,"&signup_tag=oauth&signup_data=177967");
    wd_DEBUG("Myurl:%s\n",Myurl);
    CURL *curl;
    CURLcode res;
    FILE *fp;

    //curl_global_init(CURL_GLOBAL_ALL);
    curl=curl_easy_init();
    if(curl){
        struct curl_slist *headers_l=NULL;
        static const char header1_l[]="Host:www.dropbox.com";
        static const char header2_l[]="Mozilla/5.0 (Windows NT 6.1; rv:17.0) Gecko/20100101 Firefox/17.0";
        static const char header3_l[]="Accept:text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
        static const char header4_l[]="zh-cn,zh;q=0.8,en-us;q=0.5,en;q=0.3";
        //static const char header5_l[]="Accept-Encoding:gzip, deflate";
        static const char header6_l[]="Connection:keep-alive";
        //static const char header7_l[]="Referer:https://www.box.net/api/1.0/auth/a26uln89kmbh0h97e0zxl6jffxnef19f";
        //static const char header8_l[]="Content-Type:application/x-www-form-urlencoded";
        //static const char header9_l[]="Content-Length:237";

        headers_l=curl_slist_append(headers_l,header1_l);
        headers_l=curl_slist_append(headers_l,header2_l);
        headers_l=curl_slist_append(headers_l,header3_l);
        headers_l=curl_slist_append(headers_l,header4_l);
        //headers_l=curl_slist_append(headers_l,header5_l);
        headers_l=curl_slist_append(headers_l,header6_l);
        //headers_l=curl_slist_append(headers_l,header7_l);
        //headers_l=curl_slist_append(headers_l,header8_l);
        //headers_l=curl_slist_append(headers_l,header9_l);
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,Myurl);
        //curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers_l);

        //curl_easy_setopt(curl,CURLOPT_COOKIEJAR,"/tmp/cookie_open.txt");
        curl_easy_setopt(curl,CURLOPT_COOKIEJAR,Con(TMP_R,cookie_open.txt));

        //#ifdef SKIP_PEER_VERIFICATION
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        //#endif

#ifdef SKIP_HOSTNAME_VERFICATION
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
#endif

        fp=fopen(Con(TMP_R,xmldate1.xml),"w");

        //fp1=fopen("file.txt","w");
        if(fp==NULL){
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers_l);
            //curl_global_cleanup();
            return -1;
        }
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,60);
        //curl_easy_setopt(curl,CURLOPT_WRITEHEADER,fp1);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);
        fclose(fp);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers_l);
        if(res != 0){
            //curl_global_cleanup();
            wd_DEBUG("open_login_page [%d] failed!\n",res);
            return -1;
        }
    }
    else
        wd_DEBUG("url is wrong!!!");

    //curl_global_cleanup();


}
int
        login_first(void){

    CURL *curl;
    CURLcode res;
    FILE *fp;
    char Myurl[MAXSIZE]="\0";
    //char url[]="https://www.box.net/api/1.0/auth/";
    //sprintf(Myurl,"%s%s%s","https://www.dropbox.com/login?cont=https%3A//www.dropbox.com/1/oauth/authorize%3Foauth_token\%3D",auth->tmp_oauth_token,"&signup_tag=oauth&signup_data=177967");
    char *data=parse_login_page();
    //sprintf(Myurl,"%s?%s%s","https://www.dropbox.com/1/oauth/authorize","oauth_token=",auth->tmp_oauth_token);
    sprintf(Myurl,"%s","https://www.dropbox.com/login");
    struct curl_slist *headerlist=NULL;
    static const char buf[]="Expect:";
    //printf("data=%s\n",data);
    //char *data=parse_login_page();

    //curl_global_init(CURL_GLOBAL_ALL);
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,buf);

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,Myurl);
        //curl_easy_setopt(curl,CURLOPT_URL,url);
        //curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);

        curl_easy_setopt(curl,CURLOPT_COOKIEFILE,Con(TMP_R,cookie_open.txt));//send first saved cookie
        curl_easy_setopt(curl,CURLOPT_COOKIEJAR,Con(TMP_R,cookie_login.txt));

        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);


//#ifdef SKIP_HOSTNAME_VERFICATION
//        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
//#endif
        fp=fopen(Con(TMP_R,xmldate2.xml),"w");

        if(fp==NULL){
            curl_easy_cleanup(curl);
            curl_slist_free_all(headerlist);
            free(data);
            //curl_global_cleanup();
            return -1;
        }

        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        //curl_global_cleanup();

        if(res != 0){

            free(data);
            wd_DEBUG("login_first [%d] failed!\n",res);
            return -1;
        }

    }
    free(data);
    return 0;
}

char *parse_login_page()
{
    FILE *fp;
    char *p,*m;
    char *s="name=\"t\" value=";
    char buf[1024],buff[100];
    char cont[256]="\0";
    char *data=NULL;
    data=(char *)malloc(sizeof(char *)*1024);
    memset(data,0,1024);
    char *url="https://www.dropbox.com/1/oauth/authorize";
    sprintf(cont,"%s?oauth_token=%s",url,auth->tmp_oauth_token);
    //sprintf(cont,"%s?oauth_token=%s",url,"s1unnz0qytb4j35");
    memset(buf,'\0',sizeof(buf));
    memset(buff,'\0',sizeof(buff));

    fp=fopen(Con(TMP_R,xmldate1.xml),"r");

    while(!feof(fp)){
        fgets(buf,1024,fp);
        p=strstr(buf,s);
        if(p!=NULL){
            //strcat(buff,p);
            m=p;
            p=NULL;
            break;
        }
    }
    m=m+strlen(s)+1;
    p=strchr(m,'"');
    strncpy(buff,m,strlen(m)-strlen(p));
    char *cont_tmp=oauth_url_escape(cont);
    char *usr_tmp=oauth_url_escape(asus_cfg.user);
    char *pwd_tmp=oauth_url_escape(asus_cfg.pwd);
    //sprintf(data,"t=%s&lhs_type=default&cont=%s&signup_tag=oauth&signup_data=177967&login_email=%s&login_password=%s&%s&%s&%s",
    //        buff,oauth_url_escape(cont),oauth_url_escape(info->usr),oauth_url_escape(info->pwd),"login_submit=1","remember_me=on","login_submit_dummy=Sign+in");
#if 1
    sprintf(data,"t=%s&lhs_type=default&cont=%s&signup_tag=oauth&signup_data=177967&login_email=%s&login_password=%s&%s&%s&%s",
            buff,cont_tmp,usr_tmp,pwd_tmp,"login_submit=1","remember_me=on","login_submit_dummy=Sign+in");
#else
    sprintf(data,"t=%s&cont=%s&signup_tag=oauth&signup_data=177967&display=desktop&login_email=%s&login_password=%s&%s&%s&%s",
            buff,cont_tmp,usr_tmp,pwd_tmp,"login_submit=1","remember_me=on","login_submit_dummy=Sign+in");
#endif
    //printf("m=%s\nbuff=%s\ndata=%s\n",m,buff,data);
    fclose(fp);
    free(cont_tmp);
    free(usr_tmp);
    free(pwd_tmp);
    return data;

}
char *parse_cookie()
{
    FILE *fp;

    fp=fopen(Con(TMP_R,/cookie_login.txt),"r");

    char buf[1024]="\0";
    const char *m="	";
    char *p;
    char *s="gvc";

    while(!feof(fp)){
        fgets(buf,1024,fp);
        //printf("buf=%s\n",buf);
        p=strstr(buf,s);
        if(p!=NULL){
            //strcat(buff,p);
            break;
        }
    }
    fgets(buf,1024,fp);
    //printf("buf=%s\n",buf);
    char *cookie;
    cookie=malloc(128);
    memset(cookie,0,128);
    p=strtok(buf,m);

    int i=0;
    while(p!=NULL)
    {
        switch (i)
        {
        case 6:
            strcpy(cookie,p);
            break;
        case 1:
            break;
        default:
            break;
        }
        i++;
        p=strtok(NULL,m);
    }
    //printf("%c\n",cookie[strlen(buf)+1]);
    if(cookie[strlen(buf)+2] == '\n')
    {
        cookie[strlen(buf)+2] = '\0';
    }
    fclose(fp);
    //printf("cookie=%s\n",cookie);
    //free(cookie);
    return cookie;

}
int login_second()
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char Myurl[MAXSIZE]="\0";
    //char url[]="https://www.box.net/api/1.0/auth/";
    //sprintf(Myurl,"%s%s%s","https://www.dropbox.com/login?cont=https%3A//www.dropbox.com/1/oauth/authorize%3Foauth_token\%3D",auth->tmp_oauth_token,"&signup_tag=oauth&signup_data=177967");
    //char *data=parse_login_page();
    char *data=malloc(256);
    memset(data,0,256);
    char *cookie=parse_cookie();
#if 1
    sprintf(data,"t=%s&allow_access=Allow&oauth_token=%s&display&osx_protocol",cookie,auth->tmp_oauth_token);
#else
    sprintf(data,"t=%s&allow_access=Allow&saml_assertion&embedded&osx_protocol&oauth_token=%s&display&oauth_callback&user_id=150377145",cookie,auth->tmp_oauth_token);
#endif
    //sprintf(Myurl,"%s?%s%s","https://www.dropbox.com/1/oauth/authorize","oauth_token=",auth->tmp_oauth_token);
#if 1
    sprintf(Myurl,"%s","https://www.dropbox.com/1/oauth/authorize");
#else
    sprintf(Myurl,"%s","https://www.dropbox.com/1/oauth/authorize_submit");
#endif
    struct curl_slist *headerlist=NULL;
    static const char buf[]="Expect:";
    //printf("data=%s\n",data);
    //char *data=parse_login_page();

    //curl_global_init(CURL_GLOBAL_ALL);
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,buf);

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,Myurl);
        //curl_easy_setopt(curl,CURLOPT_URL,url);
        //curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);


        curl_easy_setopt(curl,CURLOPT_COOKIEFILE,Con(TMP_R,cookie_login.txt));//send first saved cookie
        curl_easy_setopt(curl,CURLOPT_COOKIEJAR,Con(TMP_R,tmp/cookie_login_2.txt));


        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);


#ifdef SKIP_HOSTNAME_VERFICATION
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
#endif

        fp=fopen(Con(TMP_R,xmldate3.xml),"w");


        if(fp==NULL){
            curl_easy_cleanup(curl);
            free(data);
            free(cookie);
            curl_slist_free_all(headerlist);
            //curl_global_cleanup();
            return -1;
        }

        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        //curl_global_cleanup();
        if(res != 0){
            free(data);
            free(cookie);
            wd_DEBUG("login_second [%d] failed!\n",res);
            return -1;
        }

    }

    free(data);
    free(cookie);
    return 0;
}

char *pare_login_second_response()
{
    FILE *f=fopen(Con(TMP_R,xmldate3.xml),"rb");fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);
    char *data=malloc(len+1);fread(data,1,len,f);fclose(f);
    char *s="name=\"user_id\" value=\"";
    char *sb="\"";
    char *p = NULL;
    char *pt = NULL;
    char *user_id = NULL;
    int user_id_length;
    p=strstr(data,s);
    if(p!=NULL){
        //printf("buf=%s\n",buf);
        p+=strlen(s);
        pt = strstr(p,sb);
        user_id_length = strlen(p) - strlen(pt);
        user_id = malloc(user_id_length+1);
        memset(user_id,0,user_id_length+1);
        printf("p=%s\n",p);
        printf("pt=%s\n",pt);
        snprintf(user_id,user_id_length+1,"%s",p);
    }
    my_free(data);


//    FILE *fp = NULL;
//    fp = fopen(Con(TMP_R,xmldate3.xml),"r");
//    char buf[512] ="\0";
//    char *s="name=\"user_id\" value=\"";
//    char *sb="\"";
//    char *p = NULL;
//    char *pt = NULL;
//    char *user_id = NULL;
//    int user_id_length;
//    while(!feof(fp)){
//        fgets(buf,512,fp);
//        printf("buf=%s\n",buf);
//        p=strstr(buf,s);
//        if(p!=NULL){
//            printf("buf=%s\n",buf);
//            p+=strlen(s);
//            pt = strstr(p,sb);
//            user_id_length = strlen(p) - strlen(pt);
//            user_id = malloc(user_id_length+1);
//            memset(user_id,0,user_id_length+1);
//            printf("p=%s\n",p);
//            printf("pt=%s\n",pt);
//            snprintf(user_id,user_id_length+1,"%s",p);
//            break;
//        }
//    }
//    fclose(fp);
    wd_DEBUG("user_id = %s\n",user_id);
    return user_id;
}

int login_second_submit()
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char Myurl[MAXSIZE]="\0";
    //char url[]="https://www.box.net/api/1.0/auth/";
    //sprintf(Myurl,"%s%s%s","https://www.dropbox.com/login?cont=https%3A//www.dropbox.com/1/oauth/authorize%3Foauth_token\%3D",auth->tmp_oauth_token,"&signup_tag=oauth&signup_data=177967");
    //char *data=parse_login_page();
    char *data=malloc(256);
    memset(data,0,256);
    char *cookie=parse_cookie();
    char *user_id = pare_login_second_response();
#if 0
    sprintf(data,"t=%s&allow_access=Allow&oauth_token=%s&display&osx_protocol",cookie,auth->tmp_oauth_token);
#else
    sprintf(data,"t=%s&allow_access=Allow&saml_assertion&embedded&osx_protocol&oauth_token=%s&display&oauth_callback&user_id=%s",cookie,auth->tmp_oauth_token,user_id);
#endif
    //sprintf(Myurl,"%s?%s%s","https://www.dropbox.com/1/oauth/authorize","oauth_token=",auth->tmp_oauth_token);
#if 0
    sprintf(Myurl,"%s","https://www.dropbox.com/1/oauth/authorize");
#else
    sprintf(Myurl,"%s","https://www.dropbox.com/1/oauth/authorize_submit");
#endif
    free(user_id);
    struct curl_slist *headerlist=NULL;
    static const char buf[]="Expect:";
    //printf("data=%s\n",data);
    //char *data=parse_login_page();

    //curl_global_init(CURL_GLOBAL_ALL);
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,buf);

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,Myurl);
        //curl_easy_setopt(curl,CURLOPT_URL,url);
        //curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);


        curl_easy_setopt(curl,CURLOPT_COOKIEFILE,Con(TMP_R,cookie_login.txt));//send first saved cookie
        curl_easy_setopt(curl,CURLOPT_COOKIEJAR,Con(TMP_R,tmp/cookie_login_3.txt));


        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);


#ifdef SKIP_HOSTNAME_VERFICATION
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
#endif

        fp=fopen(Con(TMP_R,xmldate4.xml),"w");


        if(fp==NULL){
            curl_easy_cleanup(curl);
            free(data);
            free(cookie);
            curl_slist_free_all(headerlist);
            //curl_global_cleanup();
            return -1;
        }

        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        //curl_global_cleanup();
        if(res != 0){
            free(data);
            free(cookie);
            wd_DEBUG("login_second_submit [%d] failed!\n",res);
            return -1;
        }

    }

    free(data);
    free(cookie);
    return 0;
}
int get_access_token()
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *header;
    static const char buf[]="Expect:";
    header=makeAuthorize(2);
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/oauth/access_token");
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);

        fp=fopen(Con(TMP_R,data_2.txt),"w");

        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            wd_DEBUG("get_access_token [%d] failed!\n",res);
            return res;
        }
    }
    free(header);
    int status;
    status=parse(Con(TMP_R,data_2.txt),2);
    if(status==-1)
        return -1;
    return 0;
}
#endif
int api_accout_info()
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *header;
    static const char buf[]="Expect:";
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/account/info");
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,data_3.txt),"w");
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            wd_DEBUG("get server space failed , id [%d] !\n",res);
            return -1;
        }
    }
    free(header);
    return 0;
}
int api_metadata_one(char *phref,cJSON *(*cmd_data)(char *filename))
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *myUrl;
    char *phref_tmp=oauth_url_escape(phref);
    myUrl=(char *)malloc(sizeof(char)*(strlen(phref_tmp)+128));
    memset(myUrl,0,strlen(phref_tmp)+128);
    sprintf(myUrl,"%s%s","https://api.dropbox.com/1/metadata/dropbox",phref_tmp);
    free(phref_tmp);
    char *header;
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/oauth_plaintext_example/main.py?list=true&rev=120e60305d");
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/main");
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,data_one.txt),"w");
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            free(myUrl);
            wd_DEBUG("api_metadata_one [%d] failed!\n",res);
            return -1;
        }
    }
    free(myUrl);
    free(header);
    cJSON *json;
    json=cmd_data(Con(TMP_R,data_one.txt));
    if(json){
        cJSON_printf_one(json);
        cJSON_Delete(json);
        return 0;
    }else
        return -1;
}
int api_metadata_test_dir(char *phref,proc_pt cmd_data)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *hd;
    char *myUrl;
    myUrl=(char *)malloc(sizeof(char)*(strlen(phref)+128));
    memset(myUrl,0,strlen(phref)+128);
    sprintf(myUrl,"%s%s%s","https://api.dropbox.com/1/metadata/dropbox",phref,"?list=false&include_deleted=true");
    char *header;
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/oauth_plaintext_example/main.py?list=true&rev=120e60305d");
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/main");
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,data_test_dir.txt),"w");
        hd=fopen(Con(TMP_R,data_test_dir_header.txt),"w+");
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,hd);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            fclose(hd);
            free(header);
            free(myUrl);
            wd_DEBUG("api_metadata_test [%d] failed!\n",res);
            return -1;
        }
        else
        {
            rewind(hd);
            char tmp[256]="\0";
            while(!feof(hd))
            {
                fgets(tmp,sizeof(tmp),hd);
                printf("tmp_ : %s\n",tmp);
                if(strstr(tmp,"404") != NULL)
                {
                    free(myUrl);
                    free(header);
                    fclose(hd);
                    return -1;
                }
                else if(strstr(tmp,"200 OK") != NULL)
                    break;
            }
            fclose(hd);
        }
    }
    free(myUrl);
    free(header);

    int rs = 0;
    cJSON *json;
    json=cmd_data(Con(TMP_R,data_test_dir.txt));
    if(json){
        rs = cJSON_printf_dir(json);
        cJSON_Delete(json);
        return rs;
    }else
        return -1;
}
int api_metadata_test(char *phref)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *myUrl;
    myUrl=(char *)malloc(sizeof(char)*(strlen(phref)+128));
    memset(myUrl,0,strlen(phref)+128);
    sprintf(myUrl,"%s%s%s","https://api.dropbox.com/1/metadata/dropbox",phref,"?list=true&include_deleted=true");
    char *header;
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/oauth_plaintext_example/main.py?list=true&rev=120e60305d");
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/main");
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,data_test.txt),"w");
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            free(myUrl);
            wd_DEBUG("api_metadata_test [%d] failed!\n",res);
            return -1;
        }
    }
    free(myUrl);
    free(header);
    return 0;
}
//int api_metadata(char *phref,cJSON *(*cmd_data)(char *filename))
int api_metadata(char *phref,proc_pt cmd_data)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *hd;
    char *myUrl;
    char *phref_tmp=oauth_url_escape(phref);
    //wd_DEBUG("get %s metadata\n",phref);
    myUrl=(char *)malloc(sizeof(char)*(strlen(phref_tmp)+128));
    memset(myUrl,0,strlen(phref_tmp)+128);

    sprintf(myUrl,"%s%s","https://api.dropbox.com/1/metadata/dropbox",phref_tmp);
    //sprintf(myUrl,"%s","https://sp.yostore.net/member/requestservicegateway/");
    free(phref_tmp);
    char *header;
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    //curl_global_init(CURL_GLOBAL_ALL);
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/oauth_plaintext_example/main.py?list=true&rev=120e60305d");
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/main");
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        //fp=fopen("/tmp/data_5.txt","w");
        fp=fopen(Con(TMP_R,data_5.txt),"w");
        hd=fopen(Con(TMP_R,data_check_access_token.txt),"w");
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,hd);
        res=curl_easy_perform(curl);

        //wd_DEBUG("res = %d\n",res);
        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        //curl_global_cleanup();
        if(res!=0){
            fclose(hd);
            free(header);
            free(myUrl);
            wd_DEBUG("api_metadata %s [%d] failed!\n",phref,res);
            return -1;
        }
        else
        {
            rewind(hd);
            char tmp[256]="\0";
            fgets(tmp,sizeof(tmp),hd);
            wd_DEBUG("tmp:%s\n",tmp);
            if(strstr(tmp,"401")!=NULL)
            {
                write_log(S_ERROR,"Access token has expired,please re-authenticate!","",0);
                exit_loop = 1;
                access_token_expired = 1;
                fclose(hd);
                free(header);
                free(myUrl);
                return -1;
            }
            fclose(hd);

        }
    }
    free(myUrl);
    free(header);
    cJSON *json;
    json=cmd_data(Con(TMP_R,data_5.txt));
    if(json)
    {
        cJSON_printf(json,"contents");
        cJSON_Delete(json);
        return 0;
    }
    else
    {
        /*the file contents is error*/
        return -1;
    }

}
/*
oldname & newname both server path
*/
int api_move(char *oldname,char *newname,int index,int is_changed_time,char *newname_r)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *hd;
    char *header;
    char *data;
    hd=fopen(Con(TMP_R,data_hd.txt),"w+");
    char *oldname_tmp=oauth_url_escape(oldname);
    char *newname_tmp=oauth_url_escape(newname);
    data=(char*)malloc(sizeof(char *)*(64+strlen(oldname_tmp)+strlen(newname_tmp)));
    memset(data,0,64+strlen(oldname_tmp)+strlen(newname_tmp));

    sprintf(data,"root=%s&from_path=%s&to_path=%s","dropbox",oldname_tmp,newname_tmp);
    free(oldname_tmp);
    free(newname_tmp);
    static const char buf[]="Expect:";
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/fileops/move");
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,data_4.txt),"w");
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,hd);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        curl_slist_free_all(headerlist);
        fclose(fp);
        if(res!=0){

            fclose(hd);
            free(header);
            free(data);
            wd_DEBUG("rename %s failed,id [%d]!\n",oldname,res);
            return res;
        }
        else
        {
            rewind(hd);
            char tmp[256]="\0";
            fgets(tmp,sizeof(tmp),hd);
            wd_DEBUG("tmp:%s\n",tmp);
            if(strstr(tmp,"404")!=NULL)
            {
                if(newname_r == NULL)  //newname_r is local new name
                {
                    char *localpath=serverpath_to_localpath(newname,index);
                    if(test_if_dir(localpath))
                    {

                        wd_DEBUG("it is folder\n");

                        res=dragfolder(localpath,index);
                        if(res != 0)
                        {

                            wd_DEBUG("dragfolder %s failed status = %d\n",localpath,res);
                            //write_system_log("error","uploadfile fail");

                            fclose(hd);
                            free(header);
                            free(data);
                            free(localpath);
                            return res;
                        }
                    }
                    else
                    {

                        wd_DEBUG("it is file\n");


                        res=upload_file(localpath,newname,1,index);
                        if(res!=0)
                        {

                            fclose(hd);
                            free(header);
                            free(data);
                            free(localpath);
                            return res;
                        }
                        else
                        {
                            cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                            time_t mtime=cJSON_printf(json,"modified");
                            cJSON_Delete(json);

                            ChangeFile_modtime(localpath,mtime);
                        }
                    }
                    free(localpath);
                }
                else
                {
                    //char *localpath=serverpath_to_localpath(newname_r,index);
                    char *localpath = newname_r;
                    if(test_if_dir(localpath))
                    {

                        wd_DEBUG("it is folder\n");

                        res=dragfolder_old_dir(localpath,index,newname);
                        if(res != 0)
                        {

                            wd_DEBUG("dragfolder %s failed status = %d\n",localpath,res);
                            //write_system_log("error","uploadfile fail");

                            fclose(hd);
                            free(header);
                            free(data);
                            //free(localpath);
                            return res;
                        }
                    }
                    else
                    {
                        wd_DEBUG("it is file\n");

                        res=upload_file(localpath,newname,1,index);
                        if(res!=0)
                        {
                            fclose(hd);
                            free(header);
                            free(data);
                            //free(localpath);
                            return res;
                        }
                        else
                        {
                            cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                            time_t mtime=cJSON_printf(json,"modified");
                            cJSON_Delete(json);

                            ChangeFile_modtime(localpath,mtime);
                        }
                    }
                    //free(localpath);
                }

            }
            else if(strstr(tmp,"200")!=NULL)
            {
                if(is_changed_time)
                {
                    char *localpath=serverpath_to_localpath(newname,index);
                    cJSON *json = dofile(Con(TMP_R,data_4.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);
                    if(test_if_dir(localpath))
                    {
                        dragfolder_rename(localpath,index,mtime);
                    }
                    else
                    {
                        ChangeFile_modtime(localpath,mtime);
                    }
                    free(localpath);
                }
            }
            else if(strstr(tmp,"403")!=NULL)
            {
                //cJSON *json = dofile(Con(TMP_R,data_4.txt));
                char *server_conflcit_name=get_server_exist(newname,index);
                printf("server_conflict_name=%s\n",server_conflcit_name);
                if(server_conflcit_name)
                {
                    deal_big_low_conflcit(server_conflcit_name,oldname,newname,newname_r,index);
                    free(server_conflcit_name);
                }
                else
                {
                    fclose(hd);
                    free(header);
                    free(data);
                    return -1;
                }
                //cJSON_Delete(json);
            }
        }
    }

    fclose(hd);
    wd_DEBUG("rename ok\n");
    free(header);
    free(data);
    return 0;
}
/*
fullname=>local
filename=>server
*/
int api_download(char *fullname,char *filename,int index)
{
    if(access(fullname,F_OK) == 0)
    {

        wd_DEBUG("Local has %s\n",fullname);

        unlink(fullname);
        add_action_item("remove",fullname,g_pSyncList[index]->server_action_list);
    }

    char *temp_suffix = ".asus.td";
    char *Localfilename_tmp=(char *)malloc(sizeof(char)*(strlen(fullname)+strlen(temp_suffix)+2));
    memset(Localfilename_tmp,0,strlen(fullname)+strlen(temp_suffix)+2);
    sprintf(Localfilename_tmp,"%s%s",fullname,temp_suffix);
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *hd;
    hd=fopen(Con(TMP_R,api_download_header.txt),"w+");
    //FILE *hd;
    char *myUrl;
    char *filename_tmp=oauth_url_escape(filename);
    myUrl=(char *)malloc(sizeof(char)*(strlen(filename_tmp)+128));
    memset(myUrl,0,strlen(filename_tmp)+128);

    sprintf(myUrl,"%s%s","https://api-content.dropbox.com/1/files/dropbox",filename_tmp);
    free(filename_tmp);
    char *header;
    static const char buf[]="Expect:";
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    write_log(S_DOWNLOAD,"",fullname,index);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        //curl_easy_setopt(curl,CURLOPT_TIMEOUT,15);
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION,my_progress_func);
        curl_easy_setopt(curl,CURLOPT_PROGRESSDATA,Clientfp);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,my_write_func);

        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);


        fp=fopen(Localfilename_tmp,"w");
        //fp=fopen("/tmp/dropbox_download.txt","w+");
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,hd);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);

        curl_slist_free_all(headerlist);
        if(res!=0){
            fclose(hd);

            free(header);
            free(myUrl);
            wd_DEBUG("download %s failed,id:[%d]!\n",filename,res);
            unlink(Localfilename_tmp);
            free(Localfilename_tmp);
            char *error_message=write_error_message("download %s failed",filename);
            write_log(S_ERROR,error_message,"",index);
            free(error_message);
            if(res == 7)
            {
                //write_log(S_ERROR,"Could not connect to server!","",index);
                return COULD_NOT_CONNECNT_TO_SERVER;
            }
            else
            {
                return res;
            }
        }
        else
        {
            rewind(hd);
            char tmp_name[256]="\0";
            fgets(tmp_name,sizeof(tmp_name),hd);
            wd_DEBUG("tmp:%s\n",tmp_name);
            fclose(hd);
            if(strstr(tmp_name,"404")!=NULL)
            {
                unlink(Localfilename_tmp);
            }
            else
            {
                if(rename(Localfilename_tmp,fullname)!=0)
                {
                    unlink(Localfilename_tmp);
                    return -1;
                }
            }
        }
    }
    free(Localfilename_tmp);
    free(header);
    free(myUrl);
    if(finished_initial)
        write_log(S_SYNC,"","",index);
    else
        write_log(S_INITIAL,"","",index);
    return 0;
}

//int get_file_size(char *filename)
//{
//    int file_len = 0;
//    int fd = 0;

//    fd = open(filename, O_RDONLY);
//    if(fd < 0)
//    {
//        perror("open");
//        /*
//         fix below bug:
//            1.create file a;
//            2.rename a-->b;
//            final open() 'a' failed ,will exit();
//        */
//        //exit(-1);
//        return -1;
//    }

//    file_len = lseek(fd, 0, SEEK_END);
//    //wd_DEBUG("file_len is %d\n",file_len);
//    if(file_len < 0)
//    {
//        perror("lseek");
//        exit(-1);
//    }
//    close(fd);
//    return file_len;
//}

long long int
        get_file_size(const char *file)
{
    struct stat file_info;

    if( !(file || *file) )
        return 0;

    if( file[0] == '-' )
        return 0;

    if( stat(file, &file_info) == -1 )
        return 0;
    else
        return(file_info.st_size);
}

int api_upload_put_test(char *filename,char *serverpath,int flag)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *fp_1;
    FILE *fp_hd;

    struct stat filestat;
    unsigned long int filesize;

    if( stat(filename,&filestat) == -1)
    {
        //perror("stat:");
        wd_DEBUG("servr sapce full stat error:%s file not exist\n",filename);
        return -1;
    }

    filesize = filestat.st_size;

    filesize = filesize / 1024 / 1024;

    fp_1=fopen(filename,"rb");
    long long int size=get_file_size(filename);

    char *header;
    static const char buf[]="Content-Type: test/plain";
#ifdef OAuth1
    header=makeAuthorize(4);
#else
    header=makeAuthorize(2);
#endif
    struct curl_slist *headerlist=NULL;
    char header_l[]="Content-Length: ";
    char header1_l[128]="\0";

    sprintf(header1_l,"%s%d\n",header_l,size);

    header1_l[strlen(header1_l)-1]='\0';
    curl=curl_easy_init();

    char myUrl[1024]="\0";
    wd_DEBUG("serverpath = %s\n",serverpath);
    if(flag)
        sprintf(myUrl,"%s%s?%s&%s","https://api-content.dropbox.com/1/files_put/dropbox",serverpath,header,"overwrite=true");
    else
        sprintf(myUrl,"%s%s?%s&%s","https://api-content.dropbox.com/1/files_put/dropbox",serverpath,header,"overwrite=false");

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_READDATA,fp_1);
        curl_easy_setopt(curl,CURLOPT_UPLOAD,1L);

        curl_easy_setopt(curl, CURLOPT_INFILESIZE,filestat.st_size);


        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION,my_progress_func);
        curl_easy_setopt(curl,CURLOPT_PROGRESSDATA,Clientfp);

        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);

        fp=fopen(Con(TMP_R,upload_chunk_commit.txt),"w");
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        fp_hd=fopen(Con(TMP_R,upload_header.txt),"w+");
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,fp_hd);

        //start_time = time(NULL);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        if(res==0)
        {
            rewind(fp_hd);
            char tmp_[256];
            while(!feof(fp_hd))
            {
                fgets(tmp_,sizeof(tmp_),fp_hd);
                printf("tmp_ : %s\n",tmp_);
                if(strstr(tmp_,"507 Quota Error") != NULL)
                    printf("server space not enough\n");
            }
        }
        fclose(fp_hd);
        fclose(fp);
        fclose(fp_1);
    }
    free(header);
    return 0;
}
int api_upload_put(char *filename,char *serverpath,int flag,int index)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *fp_1;
    FILE *fp_hd;
    struct stat filestat;
    unsigned long int filesize;

    if( stat(filename,&filestat) == -1)
    {
        //perror("stat:");
        wd_DEBUG("servr sapce full stat error:%s file not exist\n",filename);
        return -1;
    }

    filesize = filestat.st_size;

    filesize = filesize / 1024 / 1024;

    fp_1=fopen(filename,"rb");
    if(fp_1 == NULL)
    {
        wd_DEBUG("Local has no %s\n",filename);
        return LOCAL_FILE_LOST;
    }

    long long int size=get_file_size(filename);
    //printf("size=%d,filesize=%lu\n",size,filesize);
    char *header;
    static const char buf[]="Content-Type: test/plain";
#ifdef OAuth1
    header=makeAuthorize(4);
#else
    header=makeAuthorize(2);
#endif
    struct curl_slist *headerlist=NULL;
    char header_l[]="Content-Length: ";
    char header1_l[128]="\0";
    //sprintf(header1_l,"%s%lu\n",header_l,filesize);
    sprintf(header1_l,"%s%d\n",header_l,size);
    //printf("%d\n",strlen(header1_l));
    //printf("%c\n",header1_l[strlen(header1_l)-2]);
    header1_l[strlen(header1_l)-1]='\0';
    curl=curl_easy_init();
    //headerlist=curl_slist_append(headerlist,header1_l);
    char myUrl[1024]="\0";
    wd_DEBUG("serverpath = %s\n",serverpath);
    if(flag)
        sprintf(myUrl,"%s%s?%s&%s","https://api-content.dropbox.com/1/files_put/dropbox",serverpath,header,"overwrite=true");
    else
        sprintf(myUrl,"%s%s?%s&%s","https://api-content.dropbox.com/1/files_put/dropbox",serverpath,header,"overwrite=false");
    //headerlist=curl_slist_append(headerlist,header);
    //headerlist=curl_slist_append(headerlist,buf);
    //headerlist=curl_slist_append(headerlist,buf);

    if(LOCAL_FILE.path != NULL)
        free(LOCAL_FILE.path);
    LOCAL_FILE.path = (char*)malloc(sizeof(char)*(strlen(filename) + 1));
    sprintf(LOCAL_FILE.path,"%s",filename);
    LOCAL_FILE.index = index;

    fp=fopen(Con(TMP_R,upload_chunk_commit.txt),"w");
    fp_hd=fopen(Con(TMP_R,upload_header.txt),"w+");

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        //curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_READDATA,fp_1);
        curl_easy_setopt(curl,CURLOPT_UPLOAD,1L);
        //curl_easy_setopt(curl,CURLOPT_PUT,1L);
        //curl_easy_setopt(curl,CURLOPT_TIMEOUT,15);
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10);//upload time_out
        //curl_easy_setopt(curl,CURLOPT_INFILESIZE_LARGE,(curl_off_t)size);//put Content-Length
        curl_easy_setopt(curl, CURLOPT_INFILESIZE,filestat.st_size);
        //curl_easy_setopt(curl,CURLOPT_INFILESIZE_LARGE,size);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,size);
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);

        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION,my_progress_func);
        curl_easy_setopt(curl,CURLOPT_PROGRESSDATA,Clientfp);
        curl_easy_setopt(curl,CURLOPT_READFUNCTION, my_read_func);


        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);


        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,fp_hd);
        //start_time = time(NULL);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        fclose(fp_1);
        if(res!=0){
            fclose(fp_hd);
            free(header);
            wd_DEBUG("upload %s failed,id is [%d]!\n",filename,res);
            char *error_message=write_error_message("upload %s failed",filename);
            write_log(S_ERROR,error_message,"",index);
            free(error_message);
            if( res == 7 )
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->unfinished_list);
                    }
                    */
                //write_log(S_ERROR,"Could not connect to server!","",index);
                return COULD_NOT_CONNECNT_TO_SERVER;
            }
            else if( res == 28)
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->unfinished_list);
                    }
                    */
                //write_log(S_ERROR,"Could not connect to server!","",index);
                return CONNECNTION_TIMED_OUT;
            }
            else if( res == 35 )
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->unfinished_list);
                    }
                    */
                return INVALID_ARGUMENT;
            }
            else
                return res;
        }
        else
        {
            free(header);
            rewind(fp_hd);
            char tmp_[256];
            while(!feof(fp_hd))
            {
                fgets(tmp_,sizeof(tmp_),fp_hd);
                if(strstr(tmp_,"507 Quota Error") != NULL)
                {
                    write_log(S_ERROR,"server space is not enough!","",index);
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->up_space_not_enough_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->up_space_not_enough_list);
                    }
                    fclose(fp_hd);
                    return SERVER_SPACE_NOT_ENOUGH;
                }
            }
            fclose(fp_hd);
        }
    }
    return 0;
}
int api_upload_post()
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    FILE *fp_1;

    struct stat filestat;
    unsigned long int filesize;

    if( stat("cookie_login.txt",&filestat) == -1)
    {
        //perror("stat:");
        wd_DEBUG("servr sapce full stat error:%s file not exist\n","cookie_login.txt");
        return -1;
    }

    filesize = filestat.st_size;

    filesize = filesize / 1024 / 1024;

    fp_1=fopen("cookie_login.txt","rb+");
    long long int size=get_file_size("cookie_login.txt");
    wd_DEBUG("size=%d,filesize=%lu\n",size,filesize);
    char *header;
    static const char buf[]="Content-Type: test/plain";
#ifdef OAuth1
    header=makeAuthorize(4);
#else
    header=makeAuthorize(2);
#endif
    struct curl_slist *headerlist=NULL;
    char header_l[]="Content-Length: ";
    char header1_l[128]="\0";
    //sprintf(header1_l,"%s%lu\n",header_l,filesize);
    sprintf(header1_l,"%s%d\n",header_l,size);
    wd_DEBUG("%d\n",strlen(header1_l));
    //wd_DEBUG("%c\n",header1_l[strlen(header1_l)-2]);
    header1_l[strlen(header1_l)-1]='\0';
    curl=curl_easy_init();
    //headerlist=curl_slist_append(headerlist,header1_l);
    char myUrl[256]="\0";
    sprintf(myUrl,"%s?%s","https://api-content.dropbox.com/1/files/dropbox/oauth_plaintext_example",header);
    //headerlist=curl_slist_append(headerlist,buf);

    //headerlist=curl_slist_append(headerlist,buf);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        //curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
        //curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_READDATA,fp_1);
        //curl_easy_setopt(curl,CURLOPT_UPLOAD,1L);//put
        curl_easy_setopt(curl,CURLOPT_POST,1L);
        //curl_easy_setopt(curl,CURLOPT_TIMEOUT,15);
        //curl_easy_setopt(curl,CURLOPT_INFILESIZE_LARGE,(curl_off_t)size);//put Content-Length
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,fp_1);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)size);
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen("upload.txt","w");
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        fclose(fp_1);
        if(res!=0){
            wd_DEBUG("delete [%d] failed!\n",res);
            return -1;
        }
    }
    free(header);
}

int my_progress_func(char *clientfp,double t,double d,double ultotal,double ulnow){
    //printf("%g/%g(%g%%)\n",d,t,d*100.0/t);
#if 1
    int sec;
    double  elapsed = 0;
    elapsed = time(NULL) - start_time;
    sec = (int)elapsed;
    if( sec > 0 )
    {
        //double progress = ulnow*100.0/ultoal;
        if(sec % 5 == 0)
            wd_DEBUG("@%s %g / %g (%g %%)\n", clientfp, ulnow, ultotal, ulnow*100.0/ultotal);
    }
#endif

#if 1
    if(t > 1 && d > 10) // download
        wd_DEBUG("@@%s %10.0f / %10.0f (%g %%)\n", clientfp, d, t, d*100.0/t);
    else//upload
    {
        if(exit_loop==1){
            return -1;
        }
        wd_DEBUG("@@@%s %10.0f / %10.0f (%g %%)\n", clientfp, ulnow, ultotal, ulnow*100.0/ultotal);
    }
#endif
    return 0;
}

size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream)

{
    if(exit_loop==1){
        return -1;
    }
    wd_DEBUG("download!\n");
    int len ;
    len = fwrite(ptr, size, nmemb, stream);
    //printf("write len is %d\n",len);
    return len;

}

size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if(exit_loop==1){
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
            return -1;
        }
        else
        {
            if(LOCAL_FILE.path != NULL)
                free(LOCAL_FILE.path);
            LOCAL_FILE.path = (char *)malloc(sizeof(char)*(strlen(ret) + 1));
            sprintf(LOCAL_FILE.path,"%s",ret);
            free(ret);
        }
    }

    int len;
    len = fread(ptr, size, nmemb, stream);
    //DEBUG("\rread len:%d   path:%s",len,LOCAL_FILE.path);
    return len;
}

int api_upload_chunk_put(char *buffer,char *upload_id,unsigned long offset,unsigned long chunk,int index,char *filepath)
{
    wd_DEBUG("upload_id: %s\n",upload_id);
    FILE *fp;
    FILE *fp_2;
    fp_2=fopen(Con(TMP_R,swap),"w+");
    char *header;
#ifdef OAuth1
    header=makeAuthorize(4);
#else
    header=makeAuthorize(2);
#endif

    fwrite(buffer,4000000/10,10,fp_2);
    rewind(fp_2);
    CURL *curl;
    CURLcode res;
    struct curl_slist *headerlist=NULL;

    curl=curl_easy_init();
    //char myUrl[2046]="\0";
    char *myUrl=(char *)malloc(2046);
    memset(myUrl,0,2046);
    //char range[256] = {0};
    if( upload_id ==NULL )
        sprintf(myUrl,"%s?%s","https://api-content.dropbox.com/1/chunked_upload",header);
    else
    {
        sprintf(myUrl,"%s?upload_id=%s&offset=%lu&%s","https://api-content.dropbox.com/1/chunked_upload",upload_id,chunk,header);
    }
    wd_DEBUG("myUrl: %s\n",myUrl);
    //headerlist=curl_slist_append(headerlist,header);

    if(LOCAL_FILE.path != NULL)
        free(LOCAL_FILE.path);
    LOCAL_FILE.path = (char*)malloc(sizeof(char)*(strlen(filepath) + 1));
    sprintf(LOCAL_FILE.path,"%s",filepath);
    LOCAL_FILE.index = index;

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        //curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_READDATA,fp_2);
        curl_easy_setopt(curl,CURLOPT_UPLOAD,1L);
        curl_easy_setopt(curl,CURLOPT_PUT,1L);
        //curl_easy_setopt(curl,CURLOPT_TIMEOUT,10);//download time_out
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10);//upload time_out
        curl_easy_setopt(curl,CURLOPT_INFILESIZE_LARGE,(curl_off_t)offset);//put Content-Length
        //if( offset > 0)
        //sprintf(range,"0-%lu",offset);
        //curl_easy_setopt(curl, CURLOPT_RANGE,range);
        //curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE,(curl_off_t)0);
        fp=fopen(Con(TMP_R,upload_chunk_1.txt),"w+");
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(curl,CURLOPT_PROGRESSFUNCTION,my_progress_func);
        curl_easy_setopt(curl,CURLOPT_PROGRESSDATA,Clientfp);

        curl_easy_setopt(curl,CURLOPT_READFUNCTION, my_read_func);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1);
        curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30);
        //curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,my_write_func);


        res=curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        fclose(fp_2);
        if(res!=CURLE_OK){
            free(header);
            free(myUrl);
            wd_DEBUG("upload fail,id is %d!\n",res);
            char *error_message=write_error_message("upload %s failed",filepath);
            write_log(S_ERROR,error_message,"",index);
            free(error_message);
            if( res == 7 )
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filepath,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filepath,g_pSyncList[index]->unfinished_list);
                    }
                    */
                return COULD_NOT_CONNECNT_TO_SERVER;
            }
            else if( res == 28)
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filepath,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filepath,g_pSyncList[index]->unfinished_list);
                    }
                    */
                return CONNECNTION_TIMED_OUT;
            }
            else if( res == 35 )
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filepath,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filepath,g_pSyncList[index]->unfinished_list);
                    }*/
                return INVALID_ARGUMENT;
            }
            else
                return res;

        }
    }
    free(header);
    free(myUrl);
    return 0;
}
char *get_upload_id()
{
    FILE *fp;
    fp=fopen(Con(TMP_R,upload_chunk_1.txt),"r");
    if(fp==NULL)
    {
        wd_DEBUG("open %s fialed\n","upload_chunk_1.txt");
        return NULL;
    }
    char buff[1024]="\0";
    char *p=NULL,*m=NULL;
    char *upload_id;
    upload_id=(char *)malloc(sizeof(char *)*512);
    memset(upload_id,0,512);
    fgets(buff,sizeof(buff),fp);
    p=strstr(buff,"upload_id");
    p=p+strlen("upload_id")+4;
    m=strchr(p,'"');
    snprintf(upload_id,strlen(p)-strlen(m)+1,"%s",p);
    //strncpy(upload_id,p,strlen(p)-strlen(m));
    wd_DEBUG("upload_id : %s,%d\n",upload_id,strlen(upload_id));
    //wd_DEBUG("upload_id : %c\n",upload_id[21]);
    if(upload_id[strlen(upload_id)] == '\0')
    {
        //wd_DEBUG("111\n");
        upload_id[strlen(upload_id)] = '\0';
    }
    fclose(fp);
    //printf("upload_id : %s,%d\n",upload_id,strlen(upload_id));
    //printf("upload_id : %c\n",upload_id[strlen(upload_id)]);
    return upload_id;
}
int api_upload_chunk_commit(char *upload_id,char *filename,int flag,int index)
{
    FILE *fp;
    FILE *fp_hd;
    char *header;
#ifdef OAuth1
    header=makeAuthorize(4);
#else
    header=makeAuthorize(2);
#endif
    CURL *curl;
    CURLcode res;
    struct curl_slist *headerlist=NULL;

    curl=curl_easy_init();
    char *data=malloc(256);
    memset(data,0,256);
    if(flag)
        sprintf(data,"upload_id=%s",upload_id);
    else
        sprintf(data,"overwrite=false&upload_id=%s",upload_id);
    char myUrl[2046]="\0";
    sprintf(myUrl,"%s%s?upload_id=%s&%s","https://api-content.dropbox.com/1/commit_chunked_upload/dropbox",filename,upload_id,header);
    //headerlist=curl_slist_append(headerlist,header);

    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        //curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_READDATA,fp_2);
        //curl_easy_setopt(curl,CURLOPT_UPLOAD,1L);
        curl_easy_setopt(curl,CURLOPT_POST,1L);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
        //curl_easy_setopt(curl,CURLOPT_TIMEOUT,15);
        //curl_easy_setopt(curl,CURLOPT_INFILESIZE_LARGE,(curl_off_t)offset);//put Content-Length
        //if( offset > 0)
        //sprintf(range,"0-%lu",offset);
        //curl_easy_setopt(curl, CURLOPT_RANGE,range);
        //curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE,(curl_off_t)0);
        fp=fopen(Con(TMP_R,upload_chunk_commit.txt),"w");
        //curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,my_write_func);
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);

        fp_hd=fopen(Con(TMP_R,upload_header.txt),"w+");
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,fp_hd);

        res=curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        if(res!=0){
            fclose(fp_hd);
            free(header);
            free(data);
            wd_DEBUG("upload %s failed,id is [%d]!\n",filename,res);
            char *error_message=write_error_message("upload %s failed",filename);
            write_log(S_ERROR,error_message,"",index);
            free(error_message);
            if( res == 7 )
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->unfinished_list);
                    }*/

                return COULD_NOT_CONNECNT_TO_SERVER;
            }
            else if( res == 28)
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->unfinished_list);
                    }
                    */

                return CONNECNTION_TIMED_OUT;
            }
            else if( res == 35 )
            {
                /*
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->unfinished_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->unfinished_list);
                    }
                    */
                return INVALID_ARGUMENT;
            }
            else
                return res;
        }
        else
        {
            free(header);
            free(data);
            rewind(fp_hd);
            char tmp_[256];
            while(!feof(fp_hd))
            {
                fgets(tmp_,sizeof(tmp_),fp_hd);
                if(strstr(tmp_,"507 Quota Error") != NULL)
                {
                    write_log(S_ERROR,"server space is not enough!","",index);
                    action_item *item;
                    item = get_action_item("upload",filename,g_pSyncList[index]->up_space_not_enough_list,index);
                    if(item == NULL)
                    {
                        add_action_item("upload",filename,g_pSyncList[index]->up_space_not_enough_list);
                    }
                    fclose(fp_hd);
                    return SERVER_SPACE_NOT_ENOUGH;
                }
            }
            fclose(fp_hd);
        }
    }
    return 0;
}
/*
 1=>enough
 0=>not enough
 status=>get accout_info fail
*/
int is_server_enough(char *filename)
{
    long long int size_lo=get_file_size(filename);
    if(size_lo == -1)
        return -1;
    int status;
    status=api_accout_info();
    long long int size_server;
    if(status == 0)
    {
        cJSON *json = dofile(Con(TMP_R,data_3.txt));
        cJSON_printf(json,"quota_info");
        cJSON_Delete(json);
        //printf("%lld %lld %lld\n",server_quota,server_shared,server_normal);
        size_server=server_quota-server_shared;
        size_server-=server_normal;
        //size_server=(server_quota-server_shared-server_normal);
        wd_DEBUG("server free space is %lld\n",size_server);

        if(size_server> size_lo)
        {

            wd_DEBUG("server freespace is enough!\n");

            return 1;
        }
        else
        {

            wd_DEBUG("server freespace is not enough!\n");

            return 0;
        }
    }
    return status;

}
int free_upload_chunk_info()
{
    if(upload_chunk != NULL)
    {
        if(upload_chunk->upload_id != NULL)
            free(upload_chunk->upload_id);
        if(upload_chunk->filename != NULL)
            free(upload_chunk->filename);
        free(upload_chunk);
        upload_chunk = NULL;
    }
    return 1;
}

int add_upload_chunked_info(char *filename,char *upload_id,unsigned long offset,unsigned long chunk,time_t expires)
{
    upload_chunk = (Upload_chunked *)malloc(sizeof(Upload_chunked));
    memset(upload_chunk,0,sizeof(upload_chunk));
    upload_chunk->filename = (char *)malloc(sizeof(char)*(strlen(filename)+1));
    memset(upload_chunk->filename,'\0',strlen(filename)+1);
    sprintf(upload_chunk->filename,"%s",filename);
    upload_chunk->upload_id = (char *)malloc(sizeof(char)*(strlen(upload_id)+1));
    memset(upload_chunk->upload_id,'\0',strlen(upload_id)+1);
    sprintf(upload_chunk->upload_id,"%s",upload_id);
    upload_chunk->chunk = chunk;
    upload_chunk->offset = offset;
    upload_chunk->expires = expires;
    return 1;
}

int api_upload_chunk_continue(char *filename,int index,int flag,char *serverpath,unsigned long size)
{
    wd_DEBUG("api_upload_chunk_continue\n");
    int res=0;
    unsigned long offset=4*1000*1000;//4M chunk
    unsigned long chunk = 0;
    FILE *fp_1;
    char *upload_id = (char *)malloc(sizeof(char)*(strlen(upload_chunk->upload_id)+1));
    time_t expires = upload_chunk->expires;
    memset(upload_id,'\0',strlen(upload_chunk->upload_id)+1);
    sprintf(upload_id,"%s",upload_chunk->upload_id);
    char *buffer = malloc(4000000);
    fp_1=fopen(filename,"r");

    if(fp_1 == NULL)
    {
        wd_DEBUG("open %s failed\n",filename);
        return -1;
    }
    fseek(fp_1,upload_chunk->chunk,SEEK_SET);

    chunk = upload_chunk->chunk;
    while(chunk+offset <= size)
    {
        memset(buffer,0,4000000);
        fread(buffer,sizeof(buffer)/10,10,fp_1);
        res=api_upload_chunk_put(buffer,upload_id,offset,chunk,index,filename);
        if(res != 0)
        {
            free_upload_chunk_info();
            add_upload_chunked_info(filename,upload_id,offset,chunk,expires);

            fclose(fp_1);
            free(upload_id);
            free(buffer);
            return res;
        }
        chunk=chunk+offset;
    }
    offset = size-chunk;
    memset(buffer,0,4000000);
    fread(buffer,sizeof(buffer)/10,10,fp_1);
    res = api_upload_chunk_put(buffer,upload_id,offset,chunk,index,filename);
    if(res != 0)
    {
        free_upload_chunk_info();
        add_upload_chunked_info(filename,upload_id,offset,chunk,expires);
        fclose(fp_1);
        free(buffer);
        free(upload_id);
        return res;
    }
    char *serverpath_encode=oauth_url_escape(serverpath);
    res=api_upload_chunk_commit(upload_id,serverpath_encode,flag,index);
    free(serverpath_encode);
    if(res != 0)
    {
        free_upload_chunk_info();
        fclose(fp_1);
        free(upload_id);
        free(buffer);
        return res;
    }

    free_upload_chunk_info();
    free(buffer);
    free(upload_id);
    fclose(fp_1);
    return 0;
}

int api_upload_chunk(char *filename,int index,int flag,char *serverpath,unsigned long size)
{
    int res=0;
    unsigned long offset=4*1000*1000;//4M chunk
    unsigned long chunk = 0;
    FILE *fp_1;
    char *upload_id;
    fp_1=fopen(filename,"r");

    if(fp_1 == NULL)
    {
        wd_DEBUG("open %s failed\n",filename);
        return -1;
    }
    char *buffer = malloc(4000000);
    memset(buffer,0,4000000);
    fread(buffer,sizeof(buffer)/10,10,fp_1);
    //fwrite(buffer,sizeof(buffer)/10,10,fp_2);
    //rewind(fp_2);
    //printf("buffer : %s\n",buffer);
    start_time = time(NULL);
    res = api_upload_chunk_put(buffer,NULL,offset,0,index,filename);//offset :Content-length;chunk :offset
    if(res != 0)
    {
        fclose(fp_1);
        free(buffer);
        return res;
    }

    /*
    do{
        res=api_upload_chunk_put(buffer,NULL,offset,0);//offset :Content-length;chunk :offset
    }while(res!=0&&res==7||res==35||res==28);
*/
    upload_id = get_upload_id();
    if(upload_id == NULL)
    {
        fclose(fp_1);
        free(buffer);
        return -1;
    }
    cJSON *json = dofile(Con(TMP_R,upload_chunk_1.txt));
    time_t mtime=cJSON_printf_expires(json);
    cJSON_Delete(json);
    wd_DEBUG("filename chunked file expires : %d\n",mtime);
    //exit(1);
    chunk = offset;
    while(chunk+offset <= size)
    {
        memset(buffer,0,4000000);
        fread(buffer,sizeof(buffer)/10,10,fp_1);
        res=api_upload_chunk_put(buffer,upload_id,offset,chunk,index,filename);
        if(res != 0)
        {
            free_upload_chunk_info();
            add_upload_chunked_info(filename,upload_id,offset,chunk,mtime);

            fclose(fp_1);
            free(upload_id);
            free(buffer);
            return res;
        }
        /*
        do{
            res=api_upload_chunk_put(buffer,upload_id,offset,chunk);
        }while(res!=0&&res==7||res==35||res==28);
        */
        chunk=chunk+offset;
    }
    offset=size-chunk;
    memset(buffer,0,4000000);
    fread(buffer,sizeof(buffer)/10,10,fp_1);
    res=api_upload_chunk_put(buffer,upload_id,offset,chunk,index,filename);
    if(res!=0)
    {
        free_upload_chunk_info();
        add_upload_chunked_info(filename,upload_id,offset,chunk,mtime);
        fclose(fp_1);
        free(upload_id);
        free(buffer);
        return res;
    }
    /*
    do{
        res=api_upload_chunk_put(buffer,upload_id,offset,chunk);
    }while(res!=0&&res==7||res==35||res==28);
    */
    char *serverpath_encode=oauth_url_escape(serverpath);
    res=api_upload_chunk_commit(upload_id,serverpath_encode,flag,index);
    free(serverpath_encode);
    if(res!=0)
    {
        fclose(fp_1);
        free(upload_id);
        free(buffer);
        return res;
    }

    free_upload_chunk_info();
    free(upload_id);
    free(buffer);
    fclose(fp_1);
    return 0;
}
/*
 filename=>local full filename
 serverpath=>server full filename
 third para:ture or false
 ture=>server filename will be overwrite
 false=>server filename will not overwrite,but local filename will be rename ,ex xxx.txt=>xxx(1).txt
 */
int upload_file(char *filename,char *serverpath,int flag,int index)
{


    wd_DEBUG("*****************Upload***************\n");

    if(access(filename,F_OK) != 0)
    {

        wd_DEBUG("Local has no %s\n",filename);
        //add_action_item("upload",filename,g_pSyncList[index]->access_failed_list);
        return LOCAL_FILE_LOST;
    }
#if 1
    int server_enough;
    server_enough=is_server_enough(filename);
    if(server_enough != 1)
    {
        if(server_enough == 0)
        {
            write_log(S_ERROR,"server space is not enough!","",index);
            action_item *item;
            item = get_action_item("upload",filename,g_pSyncList[index]->up_space_not_enough_list,index);
            if(item == NULL)
            {
                add_action_item("upload",filename,g_pSyncList[index]->up_space_not_enough_list);
            }
            return SERVER_SPACE_NOT_ENOUGH;
        }
        else
        {
            return server_enough;
        }
    }
#endif
    unsigned long MAX_FILE_SIZE=140*1000*1000;

    int res=0;

    write_log(S_UPLOAD,"",filename,index);

    long long int size = get_file_size(filename);
    if( size == -1)
    {
        if(access(filename,F_OK) != 0)
        {

            wd_DEBUG("Local has no %s\n",filename);
            //add_action_item("upload",filename,g_pSyncList[index]->access_failed_list);
            return LOCAL_FILE_LOST;
        }
    }
    if( size >= MAX_FILE_SIZE )
    {
        //vpOkR7xVrTZKrFl05pPSww
        if(upload_chunk != NULL)
        {
            time_t cur_ts = time(NULL);
            time_t expires = upload_chunk->expires;
            if(strcmp(filename,upload_chunk->filename) == 0 || expires == -1 || cur_ts < expires)
            {
                res = api_upload_chunk_continue(filename,index,flag,serverpath,size);
                if(res != 0)
                {
                    return res;
                }
            }
            else
            {
                free_upload_chunk_info();
                res = api_upload_chunk(filename,index,flag,serverpath,size);
                if(res != 0)
                {
                    return res;
                }
            }
        }
        else
        {
            res = api_upload_chunk(filename,index,flag,serverpath,size);
            if(res != 0)
            {
                return res;
            }
        }
    }
    else
    {
        char *serverpath_encode=oauth_url_escape(serverpath);
        res=api_upload_put(filename,serverpath_encode,flag,index);
        free(serverpath_encode);
        if(res!=0)
        {
            return res;
        }
    }
    if(finished_initial)
        write_log(S_SYNC,"","",index);
    else
        write_log(S_INITIAL,"","",index);
    return 0;
    //fclose(fp_2);

}
int api_delete(char *herf,int index)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *herf_tmp=oauth_url_escape(herf);
    char *data=(char *)malloc(sizeof(char)*(strlen(herf_tmp)+64));
    memset(data,0,strlen(herf_tmp)+64);

    sprintf(data,"%s%s","root=dropbox&path=",herf_tmp);
    free(herf_tmp);
    //char data[]="root=dropbox&path=/main.py";
    char *header;
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/fileops/delete");
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,api_delete.txt),"w");
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            free(data);
            char error_info[100];
            memset(error_info,0,sizeof(error_info));
            sprintf(error_info,"%s%s","delete fail ",herf);
            wd_DEBUG("delete %s failed,id [%d]!\n",herf,res);
            return res;
        }
    }
    free(header);
    free(data);
    return 0;
}
/*
 lcoalpath=>local name
 folderpath=>server path
*/
int api_create_folder(char *localpath,char *foldername)
{

    wd_DEBUG("****************create_folder****************\n");

    if(access(localpath,0) != 0)
    {

        wd_DEBUG("Local has no %s\n",localpath);

        return LOCAL_FILE_LOST;
    }

    CURL *curl;
    CURLcode res;
    FILE *fp;
    fp=fopen(Con(TMP_R,create_folder.txt),"w");

    FILE *fp_hd;
    fp_hd=fopen(Con(TMP_R,create_folder_header.txt),"w");

    //char data[]="root=dropbox&path=/main";
    char *foldername_tmp=oauth_url_escape(foldername);
    char *data=(char *)malloc(sizeof(char)*(strlen(foldername_tmp)+32));
    memset(data,0,strlen(foldername_tmp)+32);
    sprintf(data,"%s%s","root=dropbox&path=",foldername_tmp);
    free(foldername_tmp);
    char *header;
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/fileops/create_folder");
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);

        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,fp_hd);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        fclose(fp_hd);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            free(data);
            wd_DEBUG("create %s failed,is [%d] !\n",foldername,res);
            return res;
        }
        else
        {
            /*deal big or small case sentive problem,will return 403*/
            if(parse_create_folder(Con(TMP_R,create_folder_header.txt)))
            {
#ifndef MULTI_PATH
                char *server_conflcit_name=get_server_exist(foldername,0);
                if(server_conflcit_name)
                {
                    char *local_conflcit_name = serverpath_to_localpath(server_conflcit_name,0);
                    char *g_newname = get_confilicted_name_case(local_conflcit_name,1);
                    my_free(local_conflcit_name);

                    updata_socket_list(localpath,g_newname,0);

                    if(access(localpath,0) == 0)
                    {
                        add_action_item("rename",g_newname,g_pSyncList[0]->server_action_list);
                        rename(localpath,g_newname);
                    }

                    char *s_newname = localpath_to_serverpath(g_newname,0);
                    int status=0;
                    do
                    {
                        status = api_create_folder(g_newname,s_newname);
                    }while(status != 0 && !exit_loop);
                    my_free(g_newname);
                    my_free(s_newname);
                }
#endif
            }
        }
    }
    free(header);
    free(data);
    return 0;
}
int parse_config_mutidir(char *path,struct asus_config *config)
{
    FILE *fp;
    wd_DEBUG("filename:%s\n",path);
    char buf[512];
    char *buffer = buf;
    char *p;
    int i = 0;
    int k = 0;
    int j = 0;

    //memset(username, 0, sizeof(username));
    //memset(password, 0, sizeof(password));
    memset(buf,0,sizeof(buf));

    if (access(path,0) == 0)
    {
        if(( fp = fopen(path,"r"))==NULL)
        {
            fprintf(stderr,"read Cloud error");
            return -1;
        }
        else
        {
            while(fgets(buffer,512,fp)!=NULL)
            {
                if(buffer[strlen(buffer)-1] == '\n')
                {
                    buffer[strlen(buffer)-1] = '\0';
                }
                p=buffer;
                printf("p is %s,outer is %s\n",p,buffer);
                switch (i)
                {
                case 0 :
                    config->type = atoi(p);
                    //printf("config->type:%d\n",config->type);
                    break;
                case 1:
                    config->enable = atoi(p);
                    //printf("config->enable:%d\n",config->enable);
                    break;
                case 2:
                    strncpy(config->user,p,strlen(p));
                    if(config->user[strlen(config->user)-1]=='\n')
                    {
                        //printf("aaaa\n");
                        config->user[strlen(config->user)-1]='\0';
                    }
                    //printf("%d\n",strlen(config->user));
                    //printf("config->user:%c\n",config->user[strlen(config->user)-2]);
                    break;
                    case 3:

                    strncpy(config->pwd,p,strlen(p));
                    if(config->pwd[strlen(config->pwd)-1]=='\n')
                    {
                        //printf("aaaa\n");
                        config->pwd[strlen(config->pwd)-1]='\0';
                    }
                    //printf("%d\n",strlen(config->pwd));
                    //printf("config->user:%c\n",config->pwd[8]);
                    break;
                    case 4:
                    //printf("case is %d,p is %s\n",i,p);
                    config->dir_number = atoi(p);
                    config->prule = (struct asus_rule **)malloc(sizeof(struct asus_rule*)*config->dir_number);
                    if(NULL == config->prule)
                    {
                        return -1;
                    }
                    break;
                    default:
                    k = j / 2 ;
                    if(i % 2 == 1)
                    {
                        config->prule[k] = (struct asus_rule*)malloc(sizeof(struct asus_rule));
                        config->prule[k]->rule = atoi(p);
                    }
                    else
                    {
                        char *dp;
                        strcpy(config->prule[k]->path,p);
                        dp=strrchr(config->prule[k]->path,'/');
                        //strncpy(config->prule[k]->base_path,config->prule[k]->path,strlen(config->prule[k]->path)-strlen(dp));
                        snprintf(config->prule[k]->base_path,strlen(config->prule[k]->path)-strlen(dp)+1,"%s",config->prule[k]->path);
                        config->prule[k]->base_path_len=strlen(config->prule[k]->path)-strlen(dp);
                        //dp++;
                        strcpy(config->prule[k]->rooturl,dp);
                        config->prule[k]->rooturl_len=strlen(dp);
                    }
                    j++;
                    break;
//                    case 4:
//                    config->ismuti = atoi(p);
//                    //printf("config->user:%d\n",config->ismuti);
//                    break;
//                    case 5:
//                    //printf("case is %d,p is %s\n",i,p);
//                    config->dir_number = atoi(p);
//                    config->prule = (struct asus_rule **)malloc(sizeof(struct asus_rule*)*config->dir_number);
//                    if(NULL == config->prule)
//                    {
//                        return -1;
//                    }
//                    break;
//                    default:
//                    k = j / 2 ;
//                    if(i % 2 == 0)
//                    {
//                        config->prule[k] = (struct asus_rule*)malloc(sizeof(struct asus_rule));
//                        config->prule[k]->rule = atoi(p);
//                    }
//                    else
//                    {
//                        char *dp;
//                        strcpy(config->prule[k]->path,p);
//                        dp=strrchr(config->prule[k]->path,'/');
//                        //strncpy(config->prule[k]->base_path,config->prule[k]->path,strlen(config->prule[k]->path)-strlen(dp));
//                        snprintf(config->prule[k]->base_path,strlen(config->prule[k]->path)-strlen(dp)+1,"%s",config->prule[k]->path);
//                        config->prule[k]->base_path_len=strlen(config->prule[k]->path)-strlen(dp);
//                        //dp++;
//                        strcpy(config->prule[k]->rooturl,dp);
//                    }
//                    j++;
//                    break;
                }
                i++;
            }

            fclose(fp);
            return 0;
        }
    }
    else
        return -1;

}
void init_globar_var()
{
#if TOKENFILE
    sync_disk_removed = 0;
    disk_change = 0;
#endif
    upload_chunk = NULL;
    access_token_expired = 0;
    exit_loop = 0;
    stop_progress = 0;
    finished_initial=0;
    sprintf(log_path,"/tmp/smartsync/.logs");
    my_mkdir("/tmp/smartsync");
    my_mkdir("/tmp/smartsync/dropbox");
    my_mkdir("/tmp/smartsync/dropbox/config");
    my_mkdir("/tmp/smartsync/dropbox/script");
    my_mkdir("/tmp/smartsync/dropbox/temp");
#ifdef NVRAM_
    my_mkdir("/tmp/smartsync/script");
#endif
    my_mkdir(TMP_R);
    my_mkdir("/tmp/smartsync/.logs");
    sprintf(general_log,"%s/dropbox",log_path);
    sprintf(trans_excep_file,"%s/dropbox_errlog",log_path);

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex_socket, NULL);
    pthread_mutex_init(&mutex_receve_socket, NULL);
    pthread_mutex_init(&mutex_log, NULL);
    //pthread_mutex_init(&mutex_sync, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_cond_init(&cond_socket, NULL);
    pthread_cond_init(&cond_log, NULL);
    memset(&asus_cfg,'\0',sizeof(struct asus_config));
#if TOKENFILE
    memset(&asus_cfg_stop,0,sizeof(struct asus_config));
    if(initial_tokenfile_info_data(&tokenfile_info_start) == NULL)
    {
        return;
    }
#endif
}
void read_config()
{
#if TOKENFILE
#ifdef NVRAM_
    if(convert_nvram_to_file_mutidir(CONFIG_PATH,&asus_cfg) == -1)
    {
        wd_DEBUG("convert_nvram_to_file fail\n");
        return;
    }
#else
#ifndef WIN_32
    if(create_webdav_conf_file(&asus_cfg) == -1)
    {
        wd_DEBUG("create_dropbox_conf_file fail\n");
        return;
    }
#endif
#endif
#endif
    int i;
    if(parse_config_mutidir(CONFIG_PATH,&asus_cfg) == -1)
    {
        wd_DEBUG("parse_config_mutidir fail\n");
        no_config = 1;
        return;
    }
//    wd_DEBUG("%d,%s,%s,%d,%d,%d\n",asus_cfg.type,asus_cfg.user,asus_cfg.pwd,
//             asus_cfg.enable,asus_cfg.ismuti,asus_cfg.dir_number);
    wd_DEBUG("%d,%s,%s,%d,%d\n",asus_cfg.type,asus_cfg.user,asus_cfg.pwd,
             asus_cfg.enable,asus_cfg.dir_number);

    //conflict_log = (char **)malloc(sizeof(char *)*asus_cfg.dir_number);

    for(i=0;i<asus_cfg.dir_number;i++)
    {
        //int len = strlen(".smartsync")+strlen("dropbox")+asus_cfg.prule[i]->base_path_len+
        //conflict_log[i] = my_str_malloc()
        write_log(S_INITIAL,"","",i);
        wd_DEBUG("rule is %d,path is %s,rooturl is %s\n",asus_cfg.prule[i]->rule,asus_cfg.prule[i]->path,asus_cfg.prule[i]->rooturl);

    //printf("base_path:%s,base_path_len:%d\n",asus_cfg.prule[0]->base_path,asus_cfg.prule[0]->base_path_len);
    }
    strcpy(username,asus_cfg.user);
    strcpy(password,asus_cfg.pwd);
    wd_DEBUG("222\n");
    if( strlen(username) == 0 )
    {

        wd_DEBUG("username is blank ,please input your username and passwrod\n");

        no_config = 1;
    }

//    if(!no_config)
//    {
//        for(i=0;i<asus_cfg.dir_number;i++)
//            my_mkdir_r(asus_cfg.prule[i]->path);
//    }

    no_config = 0 ;
    exit_loop = 0;

}

int sync_initial_again(int index)
{
    int i,status,ret;

    i = index;

    ret = 1;
    if(exit_loop == 0)
    {
#ifdef MULTI_PATH
            if(api_metadata_test_dir(asus_cfg.prule[i]->rooturl,dofile)!=0)
            {
                wd_DEBUG("\nserver sync path is not exist,need create\n");
                api_create_folder(asus_cfg.prule[i]->base_path,asus_cfg.prule[i]->rooturl);
                //exit(0);
            }
#endif
        free_server_tree(g_pSyncList[i]->ServerRootNode);
        g_pSyncList[i]->ServerRootNode = NULL;

        g_pSyncList[i]->ServerRootNode = create_server_treeroot();
#ifdef MULTI_PATH
        status = browse_to_tree(asus_cfg.prule[i]->rooturl,g_pSyncList[i]->ServerRootNode);
#else
        status = browse_to_tree("/",g_pSyncList[i]->ServerRootNode);
#endif

        usleep(1000*200);
        if(status != 0)
            return status;

        if(exit_loop == 0)
        {

            if(test_if_dir_empty(asus_cfg.prule[i]->path))
            {

                g_pSyncList[i]->init_completed=1;
                g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                ret = 0;
            }
            else
            {

                wd_DEBUG("######## init sync folder,please wait......#######\n");

                if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                {
                    ret=sync_local_with_server(g_pSyncList[i]->ServerRootNode->Child,sync_local_with_server_init,i);
                }

                wd_DEBUG("######## init sync folder end#######\n");

                if(ret != 0 )
                    return ret;
                g_pSyncList[i]->init_completed = 1;
                g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
            }
        }
    }
    if(ret == 0)
        write_log(S_SYNC,"","",i);

    return ret;
}

int sync_initial()
{
    int i,status,ret;
    for(i=0;i<asus_cfg.dir_number;i++)
    {


        if(exit_loop)
            break;
#if TOKENFILE
        if(disk_change)
        {
            //disk_change = 0;
            check_disk_change();
        }

        if(g_pSyncList[i]->sync_disk_exist == 0)
        {
            write_log(S_ERROR,"Sync disk unplug!","",i);
            continue;
        }
#endif
        if(g_pSyncList[i]->init_completed)
            continue;

        ret = 1;
        if(exit_loop == 0)
        {
#ifdef MULTI_PATH
            if(api_metadata_test_dir(asus_cfg.prule[i]->rooturl,dofile)!=0)
            {
                wd_DEBUG("\nserver sync path is not exist,need create\n");
                api_create_folder(asus_cfg.prule[i]->base_path,asus_cfg.prule[i]->rooturl);
                //exit(0);
            }
#endif
            free_server_tree(g_pSyncList[i]->ServerRootNode);
            g_pSyncList[i]->ServerRootNode = NULL;

            g_pSyncList[i]->ServerRootNode = create_server_treeroot();
#ifdef MULTI_PATH
            status = browse_to_tree(asus_cfg.prule[i]->rooturl,g_pSyncList[i]->ServerRootNode);
#else
            status = browse_to_tree("/",g_pSyncList[i]->ServerRootNode);
#endif
//            for(i=0;i<asus_cfg.dir_number;i++)
//            {
//                SearchServerTree(g_pSyncList[i]->ServerRootNode);
//                //free_server_tree(g_pSyncList[i]->ServerRootNode);
//            }
            //exit(0);
            usleep(1000*200);
            if(status != 0)
                continue;

            if(exit_loop == 0)
            {

                if(test_if_dir_empty(asus_cfg.prule[i]->path))
                {
                    if(asus_cfg.prule[i]->rule != 2)
                    {

                        wd_DEBUG("######## init sync empty folder,please wait......#######\n");

                        if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                        {
                            ret=initMyLocalFolder(g_pSyncList[i]->ServerRootNode->Child,i);
                            if(ret != 0 )
                                continue;
                            g_pSyncList[i]->init_completed=1;
                            g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                        }

                        wd_DEBUG("######## init sync empty folder end#######\n");

                    }
                    else
                    {
                        g_pSyncList[i]->init_completed=1;
                        g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                        /*
                         fix upload only rule local is empty,UI log is still show "initial"
                        */
                        ret = 0;
                    }
                }
                else
                {

                    wd_DEBUG("######## init sync folder,please wait......#######\n");

                    if(g_pSyncList[i]->ServerRootNode->Child != NULL)
                    {
                        ret=sync_local_with_server(g_pSyncList[i]->ServerRootNode->Child,sync_local_with_server_init,i);
                    }

                    wd_DEBUG("######## init sync folder end#######\n");

                    if(ret != 0 )
                        continue;
                    g_pSyncList[i]->init_completed = 1;
                    g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                }
            }
        }
        if(ret == 0)
            write_log(S_SYNC,"","",i);

    }


    return ret;
}
int sync_local_with_server(Server_TreeNode *treenode,int(* sync_fun)(Server_TreeNode *,Browse*,Local*,int),int index)
{
    if(treenode->parenthref == NULL)
        return 0;
    Local *localnode;
    char *localpath;
    int ret=0;
#ifdef MULTI_PATH
    localpath=serverpath_to_localpath(treenode->parenthref,index);
#else
    if(strcmp(treenode->parenthref,"/") == 0)
    {
        localpath = (char *)malloc(sizeof(char)*(asus_cfg.prule[index]->base_path_len+asus_cfg.prule[index]->rooturl_len+2));
        memset(localpath,'\0',asus_cfg.prule[index]->base_path_len+asus_cfg.prule[index]->rooturl_len+2);
        sprintf(localpath,"%s%s",asus_cfg.prule[index]->base_path,asus_cfg.prule[index]->rooturl);
    }
    else
    {
        localpath=serverpath_to_localpath(treenode->parenthref,index);
    }
#endif

    localnode=Find_Floor_Dir(localpath);
    free(localpath);

    if(localnode != NULL)
    {
        ret=sync_fun(treenode,treenode->browse,localnode,index);
        if(ret != 0 && ret != SERVER_SPACE_NOT_ENOUGH
           && ret != LOCAL_FILE_LOST)
        {
            free_localfloor_node(localnode);
            return ret;
        }
        free_localfloor_node(localnode);
    }
    if(treenode->Child != NULL && !exit_loop)
    {
        ret =sync_local_with_server(treenode->Child,sync_fun,index);
        if(ret != 0 && ret != SERVER_SPACE_NOT_ENOUGH
           && ret != LOCAL_FILE_LOST)
        {
            return ret;
        }
    }
    if(treenode->NextBrother != NULL && !exit_loop)
    {
        ret = sync_local_with_server(treenode->NextBrother,sync_fun,index);
        if(ret != 0 && ret != SERVER_SPACE_NOT_ENOUGH
           && ret != LOCAL_FILE_LOST)
        {
            return ret;
        }
    }
    return ret;
}
int sync_local_with_server_init(Server_TreeNode *treenode,Browse *perform_br,Local *perform_lo,int index)
{
    wd_DEBUG("parentf = %s\n",treenode->parenthref);
    wd_DEBUG("perform_br->foldernumber = %d,perform_br->filenumber = %d,\n",perform_br->foldernumber,perform_br->filenumber);
    wd_DEBUG("perform_lo->foldernumber = %d,perform_lo->filenumber = %d,\n",perform_lo->foldernumber,perform_lo->filenumber);
    if(perform_br == NULL || perform_lo == NULL)
    {
        return 0;
    }
    int ret = 0;

    CloudFile *foldertmp=NULL;
    CloudFile *filetmp=NULL;
    LocalFolder *localfoldertmp;
    LocalFile *localfiletmp;
    if(perform_br->foldernumber > 0)
        foldertmp = perform_br->folderlist->next;
    if(perform_br->filenumber > 0)
        filetmp = perform_br->filelist->next;
    localfoldertmp=perform_lo->folderlist->next;
    localfiletmp=perform_lo->filelist->next;

    /****************handle files****************/
    if(perform_br->filenumber == 0 && perform_lo->filenumber !=0)
    {
        while(localfiletmp != NULL && !exit_loop)
        {
            if(asus_cfg.prule[index]->rule != 1)
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }

                char *serverpath=localpath_to_serverpath(localfiletmp->path,index);
                ret=upload_file(localfiletmp->path,serverpath,1,index);
                if(ret == 0 || ret == SERVER_SPACE_NOT_ENOUGH || ret == LOCAL_FILE_LOST)
                {
                    if(ret == 0)
                    {
                        cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                        time_t mtime=cJSON_printf(json,"modified");
                        cJSON_Delete(json);
                        ChangeFile_modtime(localfiletmp->path,mtime);
                        free(serverpath);
                    }
                }
                else
                {
                    free(serverpath);
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
    else if(perform_br->filenumber != 0 && perform_lo->filenumber ==0)
    {
        while(filetmp != NULL && !exit_loop)
        {
            if(asus_cfg.prule[index]->rule != 2)
            {
                if(is_local_space_enough(filetmp,index))
                {
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    char *localpath= serverpath_to_localpath(filetmp->href,index);
                    add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                    ret=api_download(localpath,filetmp->href,index);
                    if(ret == 0)
                    {
                        ChangeFile_modtime(localpath,filetmp->mtime);
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
            }
            filetmp=filetmp->next;
        }
    }
    else if(perform_br->filenumber != 0 && perform_lo->filenumber !=0)
    {

        while(localfiletmp != NULL && !exit_loop) //is based by localfiletmp
        {
            int cmp=1;
            char *serverpath=localpath_to_serverpath(localfiletmp->path,index);
            while(filetmp != NULL)
            {
                if((cmp=strcmp(serverpath,filetmp->href)) == 0)
                {
                    free(serverpath);
                    break;
                }
                else
                {
                    filetmp=filetmp->next;
                }
            }
            if(cmp != 0)
            {
                if(asus_cfg.prule[index]->rule != 1)
                {
                    if(wait_handle_socket(index))
                    {
                        buffer_free(serverpath);
                        return HAVE_LOCAL_SOCKET;
                    }

                    ret=upload_file(localfiletmp->path,serverpath,1,index);
                    if(ret == 0 || ret == SERVER_SPACE_NOT_ENOUGH || ret == LOCAL_FILE_LOST)
                    {
                        if(ret == 0)
                        {
                            cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                            time_t mtime=cJSON_printf(json,"modified");
                            cJSON_Delete(json);
                            ChangeFile_modtime(localfiletmp->path,mtime);
                            free(serverpath);
                        }
                    }
                    else
                    {
                        free(serverpath);
                        return ret;
                    }
                }
                else
                {
                    buffer_free(serverpath);
                    if(wait_handle_socket(index))
                    {
                        return HAVE_LOCAL_SOCKET;
                    }
                    add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->download_only_socket_head);
                }
            }
            else
            {
                if((ret = the_same_name_compare(localfiletmp,filetmp,index,1)) != 0)
                {
                    return ret;
                }
            }
            filetmp=perform_br->filelist->next;
            localfiletmp=localfiletmp->next;
        }

        filetmp=perform_br->filelist->next;
        localfiletmp=perform_lo->filelist->next;
        while(filetmp != NULL && !exit_loop) //is based by filetmp
        {
            int cmp=1;
            char *localpath=serverpath_to_localpath(filetmp->href,index);
            while(localfiletmp != NULL)
            {
                if((cmp=strcmp(localpath,localfiletmp->path)) == 0)
                {
                    free(localpath);
                    break;
                }
                else
                {
                    localfiletmp=localfiletmp->next;
                }
            }
            if(cmp != 0)
            {
                if(asus_cfg.prule[index]->rule != 2)
                {
                    if(is_local_space_enough(filetmp,index))
                    {
                        if(wait_handle_socket(index))
                        {
                            buffer_free(localpath);
                            return HAVE_LOCAL_SOCKET;
                        }

                        add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                        ret=api_download(localpath,filetmp->href,index);
                        if(ret == 0)
                        {
                            ChangeFile_modtime(localpath,filetmp->mtime);
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
                        buffer_free(localpath);
                        write_log(S_ERROR,"local space is not enough!","",index);
                        add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
            }
            localfiletmp=perform_lo->filelist->next;
            filetmp=filetmp->next;
        }
    }
    /*************handle folders**************/
    if(perform_br->foldernumber !=0 && perform_lo->foldernumber ==0)
    {
        if(asus_cfg.prule[index]->rule != 2)
        {
            while(foldertmp != NULL && !exit_loop)
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                char *localpath;
                localpath = serverpath_to_localpath(foldertmp->href,index);
                if(NULL == opendir(localpath))
                {
                    add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                    int res;
                    res = api_metadata_test_dir(foldertmp->href,dofile);
                    if(res == 0)
                        mkdir(localpath,0777);
                    else if(res == -1)
                    {
                        free(localpath);
                        return res;
                    }
                    else
                    {
                        wd_DEBUG("this %s had been deleted\n",foldertmp->href);
                    }
                }
                free(localpath);
                foldertmp = foldertmp->next;
            }
        }
    }
    else if(perform_br->foldernumber ==0 && perform_lo->foldernumber !=0)
    {
        while(localfoldertmp != NULL && !exit_loop)
        {
            if(asus_cfg.prule[index]->rule != 1)
            {
                char *serverpath;
                serverpath = localpath_to_serverpath(localfoldertmp->path,index);
                ret=api_create_folder(localfoldertmp->path,serverpath);
                if(ret == 0)
                {
                    upload_serverlist(treenode,perform_br,localfoldertmp,index);
                    free(serverpath);
                }
                else
                {
                    free(serverpath);
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
    }
    else if(perform_br->foldernumber !=0 && perform_lo->foldernumber !=0)
    {
        while(foldertmp != NULL && !exit_loop)
        {
            int cmp=1;
            char *localpath;
            localpath = serverpath_to_localpath(foldertmp->href,index);
            while(localfoldertmp != NULL)
            {
                if((cmp = strcmp(localpath,localfoldertmp->path)) == 0)
                {
                    free(localpath);
                    break;
                }
                localfoldertmp=localfoldertmp->next;
            }
            if(cmp != 0)
            {
                if(asus_cfg.prule[index]->rule != 2)
                {
                    if(wait_handle_socket(index))
                    {
                        buffer_free(localpath);
                        return HAVE_LOCAL_SOCKET;
                    }
                    if(NULL == opendir(localpath))
                    {
                        add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                        int res;
                        res = api_metadata_test_dir(foldertmp->href,dofile);
                        if(res == 0)
                            mkdir(localpath,0777);
                        else if(res == -1)
                        {
                            buffer_free(localpath);
                            return res;
                        }
                        else
                        {
                            wd_DEBUG("this %s had been deleted\n",foldertmp->href);
                        }
                    }
                    free(localpath);
                }
                else
                    buffer_free(localpath);
            }
            foldertmp = foldertmp->next;
            localfoldertmp = perform_lo->folderlist->next;
        }


        foldertmp = perform_br->folderlist->next;
        localfoldertmp = perform_lo->folderlist->next;
        while(localfoldertmp != NULL && !exit_loop)
        {
            int cmp=1;
            char *serverpath;
            serverpath = localpath_to_serverpath(localfoldertmp->path,index);
            while(foldertmp != NULL)
            {
                if((cmp = strcmp(serverpath,foldertmp->href)) == 0)
                {
                    free(serverpath);
                    break;
                }
                foldertmp=foldertmp->next;
            }
            if(cmp != 0)
            {
                if(asus_cfg.prule[index]->rule != 1)
                {
                    if(wait_handle_socket(index))
                    {
                        buffer_free(serverpath);
                        return HAVE_LOCAL_SOCKET;
                    }
                    ret=api_create_folder(localfoldertmp->path,serverpath);
                    if(ret == 0)
                    {
                        upload_serverlist(treenode,perform_br,localfoldertmp,index);
                        free(serverpath);
                    }
                    else
                    {
                        free(serverpath);
                        return ret;
                    }
                }
                else
                {
                    buffer_free(serverpath);
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
    }
    return ret;
}

int sync_local_with_server_perform(Server_TreeNode *treenode,Browse *perform_br,Local *perform_lo,int index)
{
    wd_DEBUG("parentf = %s\n",treenode->parenthref);
    wd_DEBUG("perform_br->foldernumber = %d,perform_br->filenumber = %d,\n",perform_br->foldernumber,perform_br->filenumber);
    wd_DEBUG("perform_lo->foldernumber = %d,perform_lo->filenumber = %d,\n",perform_lo->foldernumber,perform_lo->filenumber);
    if(perform_br == NULL || perform_lo == NULL)
    {
        return 0;
    }
    int ret = 0;

    CloudFile *foldertmp=NULL;
    CloudFile *filetmp=NULL;
    LocalFolder *localfoldertmp;
    LocalFile *localfiletmp;
    if(perform_br->foldernumber > 0)
        foldertmp = perform_br->folderlist->next;
    if(perform_br->filenumber > 0)
        filetmp = perform_br->filelist->next;
    localfoldertmp=perform_lo->folderlist->next;
    localfiletmp=perform_lo->filelist->next;

    /****************handle files****************/
    if(perform_br->filenumber == 0 && perform_lo->filenumber !=0)
    {
        while(localfiletmp != NULL && exit_loop == 0)
        {
            if(asus_cfg.prule[index]->rule == 1)
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

            //[bug][fix]:local file size > cloud space,then the local file will be unlink
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
    else if(perform_br->filenumber != 0 && perform_lo->filenumber ==0)
    {
        while(filetmp != NULL && !exit_loop)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            if(is_local_space_enough(filetmp,index))
            {
                action_item *item;
                item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);

                char *localpath= serverpath_to_localpath(filetmp->href,index);

                ret=api_download(localpath,filetmp->href,index);
                if(ret == 0)
                {
                    ChangeFile_modtime(localpath,filetmp->mtime);
                    if(item != NULL)
                    {
                        del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
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
            filetmp=filetmp->next;
        }
    }
    else if(perform_br->filenumber != 0 && perform_lo->filenumber !=0)
    {

        while(localfiletmp != NULL && !exit_loop) //is based by localfiletmp
        {
            int cmp=1;
            char *serverpath=localpath_to_serverpath(localfiletmp->path,index);
            while(filetmp != NULL)
            {
                if((cmp=strcmp(serverpath,filetmp->href)) == 0)
                {
                    free(serverpath);
                    break;
                }
                else
                {
                    filetmp=filetmp->next;
                }
            }
            if(cmp != 0)
            {
                buffer_free(serverpath);
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                if(asus_cfg.prule[index]->rule == 1)
                {
                    action_item *item;
                    item = get_action_item("download_only",localfiletmp->path,
                                           g_pSyncList[index]->download_only_socket_head,index);
                    if(item != NULL)
                    {
                        filetmp = perform_br->filelist->next;
                        localfiletmp = localfiletmp->next;
                        continue;
                    }
                }

                add_action_item("remove",localfiletmp->path,g_pSyncList[index]->server_action_list);

                //[bug][fix]:local file size > cloud space,then the local file will be unlink
                action_item *pp;
                pp = get_action_item("upload",localfiletmp->path,
                                     g_pSyncList[index]->up_space_not_enough_list,index);
                if(pp == NULL)
                {
                    unlink(localfiletmp->path);
                }

            }
            else
            {
                if((ret = the_same_name_compare(localfiletmp,filetmp,index,0)) != 0)
                {
                    return ret;
                }
            }
            filetmp=perform_br->filelist->next;
            localfiletmp=localfiletmp->next;
        }

        filetmp=perform_br->filelist->next;
        localfiletmp=perform_lo->filelist->next;
        while(filetmp != NULL && !exit_loop) //is based by filetmp
        {
            int cmp=1;
            char *localpath=serverpath_to_localpath(filetmp->href,index);
            while(localfiletmp != NULL)
            {
                if((cmp=strcmp(localpath,localfiletmp->path)) == 0)
                {
                    free(localpath);
                    break;
                }
                else
                {
                    localfiletmp=localfiletmp->next;
                }
            }
            if(cmp != 0)
            {
                if(wait_handle_socket(index))
                {
                    buffer_free(localpath);
                    return HAVE_LOCAL_SOCKET;
                }
                if(is_local_space_enough(filetmp,index))
                {
                    action_item *item;
                    item = get_action_item("download",
                                           filetmp->href,g_pSyncList[index]->unfinished_list,index);

                    add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                    ret=api_download(localpath,filetmp->href,index);
                    if(ret == 0)
                    {
                        ChangeFile_modtime(localpath,filetmp->mtime);
                        if(item != NULL)
                        {
                            del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                        }
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
                    buffer_free(localpath);
                    write_log(S_ERROR,"local space is not enough!","",index);
                    add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                }

            }
            localfiletmp=perform_lo->filelist->next;
            filetmp=filetmp->next;
        }
    }
    /*************handle folders**************/
    if(perform_br->foldernumber !=0 && perform_lo->foldernumber ==0)
    {

        while(foldertmp != NULL && !exit_loop)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            char *localpath;
            localpath = serverpath_to_localpath(foldertmp->href,index);
            if(NULL == opendir(localpath))
            {
                int res;
                add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                res = api_metadata_test_dir(foldertmp->href,dofile);
                if(res == 0)
                    mkdir(localpath,0777);
                else if(res == -1)
                {
                    free(localpath);
                    return res;
                }
                else
                {
                    wd_DEBUG("this %s had been deleted\n",foldertmp->href);
                }
            }
            free(localpath);
            foldertmp = foldertmp->next;
        }

    }
    else if(perform_br->foldernumber ==0 && perform_lo->foldernumber !=0)
    {
        while(localfoldertmp != NULL && !exit_loop)
        {
            if(asus_cfg.prule[index]->rule == 1)
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
    else if(perform_br->foldernumber !=0 && perform_lo->foldernumber !=0)
    {
        while(foldertmp != NULL && !exit_loop)
        {
            int cmp=1;
            char *localpath;
            localpath = serverpath_to_localpath(foldertmp->href,index);
            while(localfoldertmp != NULL)
            {
                if((cmp = strcmp(localpath,localfoldertmp->path)) == 0)
                {
                    free(localpath);
                    break;
                }
                localfoldertmp=localfoldertmp->next;
            }
            if(cmp != 0)
            {
                if(wait_handle_socket(index))
                {
                    buffer_free(localpath);
                    return HAVE_LOCAL_SOCKET;
                }
                if(NULL == opendir(localpath))
                {
                    add_action_item("createfolder",localpath,g_pSyncList[index]->server_action_list);
                    int res;
                    res = api_metadata_test_dir(foldertmp->href,dofile);
                    if(res == 0)
                        mkdir(localpath,0777);
                    else if(res == -1)
                    {
                        free(localpath);
                        return res;
                    }
                    else
                    {
                        wd_DEBUG("this %s had been deleted\n",foldertmp->href);
                    }
                }
                free(localpath);
            }
            foldertmp = foldertmp->next;
            localfoldertmp = perform_lo->folderlist->next;
        }


        foldertmp = perform_br->folderlist->next;
        localfoldertmp = perform_lo->folderlist->next;
        while(localfoldertmp != NULL && !exit_loop)
        {
            int cmp=1;
            char *serverpath;
            serverpath = localpath_to_serverpath(localfoldertmp->path,index);
            while(foldertmp != NULL)
            {
                if((cmp = strcmp(serverpath,foldertmp->href)) == 0)
                {
                    free(serverpath);
                    break;
                }
                foldertmp=foldertmp->next;
            }
            if(cmp != 0)
            {
                buffer_free(serverpath);
                if(asus_cfg.prule[index]->rule == 1)
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
    }
    return ret;
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
        wd_DEBUG("open %s fail \n",dir);
}

time_t api_getmtime(char *phref,cJSON *(*cmd_data)(char *filename))
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *myUrl;
    char *phref_tmp=oauth_url_escape(phref);
    myUrl=(char *)malloc(sizeof(char)*(strlen(phref_tmp)+128));
    memset(myUrl,0,strlen(phref_tmp)+128);
    sprintf(myUrl,"%s%s","https://api.dropbox.com/1/metadata/dropbox",phref_tmp);
    free(phref_tmp);
    char *header;
    FILE *hd;
    hd=fopen(Con(TMP_R,api_getmime.txt),"w+");
#ifdef OAuth1
    header=makeAuthorize(3);
#else
    header=makeAuthorize(1);
#endif
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/oauth_plaintext_example/main.py?list=true&rev=120e60305d");
        //curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/metadata/dropbox/main");
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,myUrl);
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,mtime.txt),"w");
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,90);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        curl_easy_setopt(curl,CURLOPT_WRITEHEADER,hd);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            fclose(hd);
            free(header);
            free(myUrl);
            wd_DEBUG("get %s mtime failed!id: [%d] \n",phref,res);
            return -2;
        }
        else
        {
            rewind(hd);
            char tmp[256]="\0";
            fgets(tmp,sizeof(tmp),hd);
            fclose(hd);
            if(strstr(tmp,"404")!=NULL)
            {
                return -1;
            }
        }
    }
    free(myUrl);
    free(header);
    cJSON *json;
    time_t mtime;
    json=cmd_data(Con(TMP_R,mtime.txt));
    mtime=cJSON_printf_mtime(json);
    cJSON_Delete(json);
    if(mtime!=-1)
        return mtime;
    else
        return -1;
}
/*0,local file newer
 *1,server file newer
 *2,local time == server time
 *-1,get server modtime failed
**/
int newer_file(char *localpath,int index,int is_init,int rule){

    char *serverpath;
    serverpath = localpath_to_serverpath(localpath,index);
    //printf("serverpath = %s\n",serverpath);
    time_t old_mtime;
    if(is_init == 0 && rule == 1)
    {
        CloudFile *filetmp;
        filetmp=get_CloudFile_node(g_pSyncList[index]->VeryOldServerRootNode,serverpath,0x2);
        if(filetmp == NULL)
        {
            old_mtime=(time_t)-1;
        }
        else
        {
            old_mtime=filetmp->mtime;
        }
    }

    time_t modtime1,modtime2;
    modtime1 = api_getmtime(serverpath,dofile);
    free(serverpath);
    if(modtime1 == -1 || modtime1 == -2)
    {

        wd_DEBUG("newer_file Getmodtime failed!\n");

        return -1;
    }

    struct stat buf;

    //printf("localpath = %s\n",localpath);

    if( stat(localpath,&buf) == -1)
    {
        perror("stat:");
        return -1;
    }
    modtime2 = buf.st_mtime;
    wd_DEBUG("local>modtime2 = %lu,server>modtime1 = %lu\n",modtime2,modtime1);

    if(is_init)
    {
        if(modtime2 > modtime1)
            return 0;
        else if(modtime2 == modtime1 || modtime2 +1 == modtime1) //FAT32 and umount HD than plugin HD ,it will be local time less then server time 1 sec;
        {
            if(modtime2 +1 == modtime1)
                ChangeFile_modtime(localpath,modtime1);
            return 2;
        }
        else
            return 1;
    }
    else
    {
        if(rule == 1) //download only
        {
            if(modtime2 > modtime1)
                return 0;
            else if(modtime2 == modtime1)
                return 2;
            else
            {
                if(modtime2 == old_mtime)
                    return 1;
                else
                    return 0;
            }
        }
        else
        {
            if(modtime2 > modtime1)
                return 0;
            else if(modtime2 == modtime1)
                return 2;
            else
                return 1;
        }
    }


}
char *change_local_same_name(char *fullname)
{
    int i = 1;
    char *temp_name = NULL;
    //char *temp_suffix = ".asus.td";
    int len = 0;
    char *path;
    char newfilename[256];

    char *fullname_tmp = NULL;
    fullname_tmp = my_str_malloc(strlen(fullname)+1);
    sprintf(fullname_tmp,"%s",fullname);

    char *filename = parse_name_from_path(fullname_tmp);
    len = strlen(filename);

    wd_DEBUG("filename len is %d\n",len);

    path = my_str_malloc((size_t)(strlen(fullname)-len+1));

    wd_DEBUG("fullname = %s\n",fullname);

    snprintf(path,strlen(fullname)-len+1,"%s",fullname);

    wd_DEBUG("path = %s\n",path);

    free(fullname_tmp);

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
        sprintf(newfilename,"%s(%d)",newfilename,i);

        wd_DEBUG("newfilename = %s\n",newfilename);

        i++;

        temp_name = my_str_malloc((size_t)(strlen(path)+strlen(newfilename)+1));
        sprintf(temp_name,"%s%s",path,newfilename);

        if(access(temp_name,F_OK) != 0)
            break;
        else
            free(temp_name);
    }

    free(path);
    free(filename);
    return temp_name;
}
/*
 1=>exit;
 0=>not exit;
 path=>serverpath;
 temp_name=>fullname;
 */
int is_server_exist(char *path,char *temp_name,int index)
{
    int status;
    FileList_one = (CloudFile *)malloc(sizeof(CloudFile));
    memset(FileList_one,0,sizeof(CloudFile));

    FileList_one->href = NULL;
    FileList_one->name = NULL;

    FileTail_one = FileList_one;
    FileTail_one->next = NULL;
    wd_DEBUG("is_server_exist path:%s\n",path);
    status=api_metadata_one(path,dofile);
    CloudFile *de_filecurrent;
    de_filecurrent=FileList_one->next;
    while(de_filecurrent != NULL)
    {
        if(de_filecurrent->href != NULL)
        {
            wd_DEBUG("de_filecurrent->href:%s\n",de_filecurrent->href);
            wd_DEBUG("temp_name           :%s\n",temp_name);
            if((status=strcmp(de_filecurrent->href,temp_name))==0)
            {
                free_CloudFile_item(FileList_one);
                return 1;
            }

        }
        de_filecurrent = de_filecurrent->next;
    }
    free_CloudFile_item(FileList_one);
    return 0;

}

char *get_server_exist(char *temp_name,int index)
{
    char *path;
    char *p = strrchr(temp_name,'/');
    path = my_str_malloc(strlen(temp_name)-strlen(p)+1);
    snprintf(path,strlen(temp_name)-strlen(p)+1,"%s",temp_name);

    int status;
    FileList_one = (CloudFile *)malloc(sizeof(CloudFile));
    memset(FileList_one,0,sizeof(CloudFile));

    FileList_one->href = NULL;
    FileList_one->name = NULL;

    FileTail_one = FileList_one;
    FileTail_one->next = NULL;
    wd_DEBUG("get_server_exist path:%s\n",path);
    status=api_metadata_one(path,dofile);
    if(status == -1)
    {
        free_CloudFile_item(FileList_one);
        free(path);
        return NULL;
    }
    char *temp_name_g = my_str_malloc(strlen(temp_name)+1);
    sprintf(temp_name_g,"%s",temp_name);

    CloudFile *de_filecurrent;
    de_filecurrent=FileList_one->next;
    while(de_filecurrent != NULL)
    {
        if(de_filecurrent->href != NULL)
        {
            wd_DEBUG("de_filecurrent->href:%s\n",de_filecurrent->href);
            wd_DEBUG("temp_name           :%s\n",temp_name);
            char *tmp_href = my_str_malloc(strlen(de_filecurrent->href)+1);
            strcpy(tmp_href,de_filecurrent->href);
            if(strcmp(de_filecurrent->href,temp_name) != 0 && (status=strcmp(strlwr(tmp_href),strlwr(temp_name_g)))==0)
            {
                char *conflict_name = my_str_malloc(strlen(de_filecurrent->href)+1);
                strcpy(conflict_name,de_filecurrent->href);
                free_CloudFile_item(FileList_one);
                free(path);
                free(temp_name_g);
                free(tmp_href);
                return conflict_name;
            }
            else
                free(tmp_href);

        }
        de_filecurrent = de_filecurrent->next;
    }
//    free_CloudFile_item(FileList_one);
//    return 0;

}

#if 0
char *change_server_same_name(char *fullname,int index){

    int i = 1;
    int exist;
    char *filename = NULL;
    char *temp_name = NULL;
    //char *temp_suffix = ".asus.td";
    int len = 0;
    char *path;
    char newfilename[512];
    int exit = 1;

    char *fullname_tmp = NULL;
    fullname_tmp = my_str_malloc(strlen(fullname)+1);
    sprintf(fullname_tmp,"%s",fullname);


    filename = parse_name_from_path(fullname_tmp);
    len = strlen(filename);
    //len = 6;

    wd_DEBUG("filename len is %d\n",len);

    path = my_str_malloc((size_t)(strlen(fullname)-len+1));

    wd_DEBUG("fullname = %s\n",fullname);

    snprintf(path,strlen(fullname)-len+1,"%s",fullname);

    wd_DEBUG("path = %s\n",path);


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
        sprintf(newfilename,"%s(%d)",newfilename,i);

        wd_DEBUG("newfilename = %s\n",newfilename);

        i++;

        temp_name = my_str_malloc((size_t)(strlen(path)+strlen(newfilename)+1));
        sprintf(temp_name,"%s%s",path,newfilename);

        //char *serverpath;
        //serverpath = localpath_to_serverpath(temp_name);

        //do{

        wd_DEBUG("temp_name = %s\n",temp_name);


        exist = is_server_exist(path,temp_name,index);
        //}while(exist == -2);

        if(exist)
        {
            free(temp_name);
        }
        else
        {
            exit = 0;
        }

    }

    free(path);
    free(filename);
    return temp_name;

}
#endif

char *change_server_same_name(char *fullname,int index)
{
    char *newname;
    char *tmp_name = malloc(strlen(fullname)+1);
    memset(tmp_name,0,strlen(fullname)+1);
    sprintf(tmp_name,"%s",fullname);
    int is_folder = test_if_dir(fullname);
    int exist;
    int len;
    char *filename;
    char *path;

    filename = parse_name_from_path(fullname);
    len = strlen(filename);

    path = my_str_malloc((size_t)(strlen(fullname)-len+1));

    wd_DEBUG("fullname = %s\n",fullname);

    snprintf(path,strlen(fullname)-len+1,"%s",fullname);

    while(!exit_loop)
    {
        newname = get_confilicted_name(tmp_name,is_folder);
        //printf("confilicted_name=%s\n",confilicted_name);
        exist = is_server_exist(path,newname,index);
        if(exist == 1)
        {
            my_free(tmp_name);
            tmp_name = malloc(strlen(newname)+1);
            memset(tmp_name,0,strlen(newname)+1);
            sprintf(tmp_name,"%s",newname);
            my_free(newname);
            //have_same = 1;
        }
        else
            break;
    }
    my_free(path);
    my_free(filename);
    my_free(tmp_name);
    return newname;
}

int the_same_name_compare(LocalFile *localfiletmp,CloudFile *filetmp,int index,int is_init)
{

    wd_DEBUG("###########the same name compare################\n");

    int ret=0;
    int newer_file_ret=0;
    if(asus_cfg.prule[index]->rule == 1)  //download only
    {
        newer_file_ret = newer_file(localfiletmp->path,index,is_init,1);
        if(newer_file_ret !=2 && newer_file_ret !=-1)
        {
            if(newer_file_ret == 0) //local file is change
            {
                //char *newname=change_local_same_name(localfiletmp->path);
                char *newname;
                char *tmp_name = malloc(strlen(localfiletmp->path)+1);
                memset(tmp_name,0,strlen(localfiletmp->path)+1);
                sprintf(tmp_name,"%s",localfiletmp->path);

                while(!exit_loop)
                {
                    newname = get_confilicted_name(tmp_name,0);
                    //printf("confilicted_name=%s\n",confilicted_name);
                    if(access(newname,F_OK) == 0)
                    {
                        my_free(tmp_name);
                        tmp_name = malloc(strlen(newname)+1);
                        memset(tmp_name,0,strlen(newname)+1);
                        sprintf(tmp_name,"%s",newname);
                        my_free(newname);
                        //have_same = 1;
                    }
                    else
                        break;
                }
                my_free(tmp_name);

                rename(localfiletmp->path,newname);
               char *err_msg = write_error_message("%s is download from server,%s is local file and rename from %s",localfiletmp->path,newname,localfiletmp->path);
                write_trans_excep_log(localfiletmp->path,3,err_msg);
                free(err_msg);
                //write_conflict_log(localfiletmp->path,newname,index);
                free(newname);
            }

            action_item *item;
            item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);

            if(is_local_space_enough(filetmp,index))
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);
                ret=api_download(localfiletmp->path,filetmp->href,index);
                if(ret == 0)
                {
                    time_t mtime=api_getmtime(filetmp->href,dofile);
                    if(mtime != -1 || mtime != -2)
                        ChangeFile_modtime(localfiletmp->path,mtime);
                    else
                    {

                        wd_DEBUG("ChangeFile_modtime failed!\n");

                    }
                    if(item != NULL)
                    {
                        del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
                else
                {
                    return ret;
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
        }
    }
    else if(asus_cfg.prule[index]->rule == 2)//upload only
    {
        newer_file_ret = newer_file(localfiletmp->path,index,is_init,0);
        if(newer_file_ret !=2 && newer_file_ret !=-1)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            /*
            char *newname;
            newname = change_server_same_name(filetmp->href,index);
            //Move(localfiletmp->path,newname,index);
            api_move(filetmp->href,newname,index);
            wd_DEBUG("newname = %s\n",newname);
            free(newname);
            */
            ret=upload_file(localfiletmp->path,filetmp->href,1,index);
            if(ret == 0 || ret == SERVER_SPACE_NOT_ENOUGH || ret == LOCAL_FILE_LOST)
            {
                if(ret == 0)
                {
                    cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);
                    ChangeFile_modtime(localfiletmp->path,mtime);
                }
            }
            else
                return ret;

        }
    }
    else //sync
    {
        newer_file_ret = newer_file(localfiletmp->path,index,is_init,0);

        if(newer_file_ret == 1)
        {
            if(is_init)
            {
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                char *serverpath=localpath_to_serverpath(localfiletmp->path,index);
                /*third param flase,false=>if server file has exit ,will not be overwrite*/
                ret=upload_file(localfiletmp->path,serverpath,0,index);
                free(serverpath);
                if(ret == 0 || ret == SERVER_SPACE_NOT_ENOUGH || ret == LOCAL_FILE_LOST)
                {
                    if(ret == 0)
                    {
                        char *newlocalname = NULL;
                        cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                        time_t mtime=cJSON_printf(json,"modified");
                        cJSON_Delete(json);

                        newlocalname=change_local_same_file(localfiletmp->path,index);
                        if(newlocalname)
                            ChangeFile_modtime(newlocalname,mtime);
                        free(newlocalname);
                    }

                }
                else
                {
                    return ret;
                }
            }
            if(is_local_space_enough(filetmp,index))
            {
                action_item *item;
                item = get_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list,index);
                if(wait_handle_socket(index))
                {
                    return HAVE_LOCAL_SOCKET;
                }
                add_action_item("createfile",localfiletmp->path,g_pSyncList[index]->server_action_list);
                ret=api_download(localfiletmp->path,filetmp->href,index);
                if(ret == 0)
                {
                    time_t mtime=api_getmtime(filetmp->href,dofile);
                    if(mtime != -1 || mtime != -2)
                        ChangeFile_modtime(localfiletmp->path,mtime);
                    else
                    {

                        wd_DEBUG("ChangeFile_modtime failed!\n");

                    }
                    if(item != NULL)
                    {
                        del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    }
                }
                else
                {
                    return ret;
                }

            }
            else
            {
                write_log(S_ERROR,"local space is not enough!","",index);
                add_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
            }
        }
        else if(newer_file_ret == 0)
        {
            if(wait_handle_socket(index))
            {
                return HAVE_LOCAL_SOCKET;
            }
            ret=upload_file(localfiletmp->path,filetmp->href,1,index);
            if(ret == 0 || ret == SERVER_SPACE_NOT_ENOUGH || ret == LOCAL_FILE_LOST)
            {
                if(ret == 0)
                {
                    cJSON * json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);

                    ChangeFile_modtime(localfiletmp->path,mtime);
                }
            }
            else
                return ret;
        }
    }
    return 0;
}
int send_action(int type, char *content)
{
    if(exit_loop)
    {
        return 0;
    }
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char str[1024];
    int port;

    //if(type == 1)
    port = INOTIFY_PORT;

    struct sockaddr_in their_addr; /* connector's address information */


    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        //exit(1);
        return -1;
    }

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

    sprintf(str,"%d@%s",type,content);

    //printf("send content is %s \n",str);

    if (send(sockfd, str, strlen(str), 0) == -1) {
        perror("send");
        //exit(1);
        return -1;
    }

    if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv");
        //exit(1);
        return -1;
    }

    buf[numbytes] = '\0';
    close(sockfd);

    wd_DEBUG("send_action finished!\n");

    return 0;
}
void send_to_inotify(){

    int i;

    for(i=0;i<asus_cfg.dir_number;i++)
    {

        wd_DEBUG("send_action base_path = %s\n",asus_cfg.prule[i]->path);
        //write_debug_log(asus_cfg.prule[i]->base_path);

        //#ifndef NVRAM_
#if TOKENFILE
        if(g_pSyncList[i]->sync_disk_exist)
        {
            send_action(asus_cfg.type,asus_cfg.prule[i]->path);
            usleep(1000*10);
        }
#else
        send_action(asus_cfg.type,asus_cfg.prule[i]->path);
        usleep(1000*10);
#endif
        //#else
        //send_action(1,asus_cfg.prule[i]->base_path);
        //usleep(1000*10);
        //#endif

    }
}
char *get_path_from_socket(char *cmd,int index)
{

    if(!strncmp(cmd,"rmroot",6))
    {
        return NULL;
    }

    char cmd_name[64]="\0";
    char *path;
    char *temp;
    char filename[256]="\0";
    char *fullname;
    char oldname[256]="\0",newname[256]="\0";
    char *oldpath;
    char action[64]="\0";
    char *cmp_name;
    char *mv_newpath;
    char *mv_oldpath;
    char *ch;
    int status;

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

    p++;
    if(strcmp(cmd_name, "rename") == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));

        p1++;

        strcpy(newname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

        if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return NULL;
        }
    }
    else if(strcmp(cmd_name, "move") == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);


        if(oldname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            free(oldpath);
            return NULL;
        }
    }
    else
    {
        strcpy(filename,p);
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));

        wd_DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);

        if(filename[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return NULL;
        }
    }

    free(temp);

    if( !strcmp(cmd_name,"rename") )
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
    }
    else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
    {
        strcpy(action,"remove");
    }
    else if( strcmp(cmd_name, "createfolder") == 0 )
    {
        strcpy(action,"createfolder");
    }
    else if( strcmp(cmd_name, "rename") == 0 )
    {
        strcpy(action,"rename");
    }
#if 1
    if(g_pSyncList[index]->server_action_list->next != NULL)
    {
        action_item *item;

        item = get_action_item(action,cmp_name,g_pSyncList[index]->server_action_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### %s %s by Dropbox Server self ######\n",action,cmp_name);

            //pthread_mutex_lock(&mutex);
            del_action_item(action,cmp_name,g_pSyncList[index]->server_action_list);

            wd_DEBUG("#### del action item success!\n");

            //pthread_mutex_unlock(&mutex);
            //local_sync = 0;
            free(path);
            if( strcmp(cmd_name, "rename") != 0 )
                free(fullname);
            free(cmp_name);
            return NULL;
        }
    }
#endif
    free(cmp_name);
    return path;

}
void show(queue_entry_t pTemp)
{
    wd_DEBUG(">>>>>>show socketlist>>>>>>>>>>>>>\n");
    while(pTemp!=NULL)
    {
        wd_DEBUG(">>>>>>>>>>>>>>>>>>>>>>>>>\n");
        wd_DEBUG(">>>>%s\n",pTemp->cmd_name);
        wd_DEBUG(">>>>%s\n",pTemp->re_cmd);
        wd_DEBUG("\n");
        pTemp=pTemp->next_ptr;
    }
}

char *search_newpath(char *href,int index)
{
    wd_DEBUG("################search_newpath###########\n");
    wd_DEBUG("href = %s\n",href);
    int flag_r = 0;
    queue_entry_t pTemp = g_pSyncList[index]->SocketActionList->head;
    while(pTemp != NULL)
    {
        if(strncmp(pTemp->cmd_name,"rename0",strlen("rename0")) == 0 ||
           strncmp(pTemp->cmd_name,"move0",strlen("move0")) == 0)
        {
            char cmd_name[64]="\0";
            char *path;
            char *temp;
            char oldname[256]="\0",newname[256]="\0";
            char *oldpath;
            char *mv_newpath;
            char *mv_oldpath;
            char *ch;

            ch = pTemp->cmd_name;
            int i = 0;
            //while(*ch != '@')
            while(*ch != '\n')
            {
                i++;
                ch++;
            }

            memcpy(cmd_name, pTemp->cmd_name, i);

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

            p++;
            if(strcmp(cmd_name, "rename0") == 0)
            {
                char *p1 = NULL;

                //p1 = strchr(p,'@');
                p1 = strchr(p,'\n');

                if(p1 != NULL)
                    strncpy(oldname,p,strlen(p)- strlen(p1));

                p1++;

                strcpy(newname,p1);

                //wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

                if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
                {
                    free(temp);
                    free(path);
                    pTemp=pTemp->next_ptr;
                    continue;
                }
            }
            else if(strcmp(cmd_name, "move0") == 0)
            {
                char *p1 = NULL;

                //p1 = strchr(p,'@');
                p1 = strchr(p,'\n');

                oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

                if(p1 != NULL)
                    snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

                p1++;

                strcpy(oldname,p1);

                //wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);


                if(oldname[0] == '.' || (strstr(path,"/.")) != NULL)
                {
                    free(temp);
                    free(path);
                    pTemp=pTemp->next_ptr;
                    continue;
                }
            }
            wd_DEBUG("111\n");
            free(temp);
            wd_DEBUG("222\n");
            if(strcmp(cmd_name, "move0") == 0)
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
            wd_DEBUG("333\n");
            char *pTemp_t=(char *)malloc(sizeof(char)*(strlen(href)+1+1));
            char *oldpath_t=(char *)malloc(sizeof(char)*(strlen(mv_oldpath)+1+1));
            memset(pTemp_t,'\0',strlen(href)+1+1);
            memset(oldpath_t,'\0',strlen(mv_oldpath)+1+1);
            sprintf(pTemp_t,"%s/",href);
            sprintf(oldpath_t,"%s/",mv_oldpath);

            wd_DEBUG("pTemp_t=%s ,oldpath_t=%s\n",pTemp_t ,oldpath_t);
            char *p_t = NULL;
            char *ret_p = NULL;
            if((p_t=strstr(pTemp_t,oldpath_t)) != NULL)
            {
                wd_DEBUG("666\n");
                p_t=p_t+strlen(oldpath_t);
                wd_DEBUG("p_t=%s\n",p_t);
                ret_p = (char *)malloc(sizeof(char) * (strlen(mv_newpath)+strlen(p_t) + 1 +1));
                memset(ret_p,'\0',strlen(mv_newpath)+strlen(p_t) + 1 +1);
                sprintf(ret_p,"%s/%s",mv_newpath,p_t);
                wd_DEBUG("ret_p = %s\n",ret_p);
                flag_r = 1;
            }
            wd_DEBUG("444\n");
            free(pTemp_t);
            free(oldpath_t);

            free(path);
            free(mv_oldpath);
            free(mv_newpath);
            wd_DEBUG("555\n");
            if(1 == flag_r)
            {
                wd_DEBUG("ret_p = %s\n",ret_p);
                return ret_p;
            }
        }
        pTemp=pTemp->next_ptr;
    }

    if(flag_r == 0)
    {
        return NULL;
    }

}

void set_copyfile_list(char *buf,char *oldpath,char *newpath)
{
    wd_DEBUG("************************set_copyfile_list***********************\n");
    int i;
    char *r_path;
    r_path = get_socket_base_path(buf);
    for(i=0;i<asus_cfg.dir_number;i++)
    {
        //wd_DEBUG("asus_cfg.prule[%d]->base_path:%s\n",i,asus_cfg.prule[i]->path);
        //wd_DEBUG("r_path                       :%s\n",r_path);
        if(!strcmp(r_path,asus_cfg.prule[i]->path))
            break;
    }
    free(r_path);

    check_action_item("copyfile",oldpath,g_pSyncList[i]->copy_file_list,i,newpath);

}

char *get_socket_filename(char *cmd)
{
    char cmd_name[64]="\0";
    char *path;
    char *temp;
    char *ch;
    char *filename;

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

    p++;
    filename = (char *)malloc(sizeof(char)*(strlen(p)+2));
    memset(filename,'\0',strlen(p)+2);
    sprintf(filename,"%s",p);
    return filename;
}

int get_socket_fullname(queue_entry_t pTemp,char *fullname)
{
    char *cmd = pTemp ->cmd_name;
    char cmd_name[64]="\0";
    char *path;
    char *temp;
    char oldname[256]="\0",newname[256]="\0";
    char *oldpath;
    char *mv_newpath;
    char *mv_oldpath;
    char *ch;

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

        //wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

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

        //wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);


        if(oldname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            free(oldpath);
            return 0;
        }
    }

    free(temp);

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

    if(strcmp(fullname,mv_oldpath) == 0 ||strcmp(fullname,mv_newpath) == 0)
    {
        free(path);
        free(mv_oldpath);
        free(mv_newpath);
        return 1;
    }
    if(pTemp->re_cmd != NULL)
    {
        if(strncmp(cmd_name, "move",4) == 0)
        {
            char *p1 = my_str_malloc((size_t)(strlen(pTemp->re_cmd)+strlen(oldname)+2));
            sprintf(p1,"%s%s",pTemp->re_cmd,oldname);
            if(strcmp(fullname,p1) == 0)
            {
                free(path);
                free(mv_oldpath);
                free(mv_newpath);
                free(p1);
                return 1;
            }
            free(p1);
        }
        else
        {
            char *p1 = my_str_malloc((size_t)(strlen(pTemp->re_cmd)+strlen(oldname)+2));
            char *p2 = my_str_malloc((size_t)(strlen(pTemp->re_cmd)+strlen(newname)+2));
            sprintf(p1,"%s%s",pTemp->re_cmd,oldname);
            sprintf(p2,"%s%s",pTemp->re_cmd,newname);
            if(strcmp(fullname,p1) == 0 || strcmp(fullname,p2) == 0 )
            {
                free(path);
                free(mv_oldpath);
                free(mv_newpath);
                free(p1);
                free(p2);
                return 1;
            }
            free(p1);
            free(p2);
        }
    }


    free(path);
    free(mv_oldpath);
    free(mv_newpath);
    return 0;

}

int check_localpath_is_socket(int index,char *ParentHerf,char *Fname,char *fullname)
{
    wd_DEBUG("************************check_localpath_is_socket***********************\n");
    int i = index;

    queue_entry_t pTemp = g_pSyncList[i]->SocketActionList->head;
    int reg = 0;// 0==>is not socket
    while(pTemp!=NULL)
    {
        if(strncmp(pTemp->cmd_name,"rename",6) == 0 ||strncmp(pTemp->cmd_name,"move",4) == 0)
        {
            reg=get_socket_fullname(pTemp,fullname);
            if(reg)
                return reg;
        }
        else
        {

            char *local_p1 = (char *)malloc(sizeof(char)*(strlen(ParentHerf)+1+strlen(Fname)+1+1));

            memset(local_p1,'\0',strlen(ParentHerf)+1+strlen(Fname)+1+1);

            sprintf(local_p1,"\n%s\n%s",ParentHerf,Fname);

            if(strstr(pTemp->cmd_name,local_p1) != NULL )
            {
                reg = 1;
                free(local_p1);
                return reg;
            }
            if(pTemp->re_cmd != NULL)
            {
                char *filename = get_socket_filename(pTemp->cmd_name);
                char *local_p2 = (char *)malloc(sizeof(char)*(strlen(pTemp->re_cmd)+strlen(filename)+2));
                memset(local_p2,'\0',strlen(pTemp->re_cmd)+strlen(filename)+2);
                sprintf(local_p2,"%s%s",pTemp->re_cmd,filename);
                free(filename);

                if(strcmp(local_p2,fullname) == 0)
                {
                    reg = 1;
                    free(local_p1);
                    free(local_p2);
                    return reg;
                }
                free(local_p2);
            }
            free(local_p1);
        }

        pTemp=pTemp->next_ptr;
    }
    return reg;

    //show(g_pSyncList[i]->SocketActionList->head);
}
int check_socket_parser(char *cmd,int index,char *re_cmd,char *str_path,char *so_filename,char *sn_filename)
{
//    if(strstr(cmd,"conflict") != NULL)
//        return 0;
    if( !strncmp(cmd,"exit",4))
    {
        wd_DEBUG("exit socket\n");
        return 0;
    }

    if(!strncmp(cmd,"rmroot",6))
    {
        g_pSyncList[index]->no_local_root = 1;
        return 0;
    }

    char cmd_name[64]="\0";
    char *path;
    char *temp;
    char filename[256]="\0";
    char oldname[256]="\0",newname[256]="\0";
    char *oldpath;
    char *ch;

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

    p++;
    if(strncmp(cmd_name, "rename",strlen("rename")) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));

        p1++;

        strcpy(newname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

        if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }
    else if(strncmp(cmd_name, "move",strlen("move")) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);


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

        wd_DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);

        if(filename[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }

    free(temp);

    if( strcmp(cmd_name, "createfile") == 0 || strcmp(cmd_name, "dragfile") == 0 )
    {}
    else if( strcmp(cmd_name, "modify") == 0 )
    {
        if(re_cmd)
        {
            if(strcmp(re_cmd,str_path) == 0 && strcmp(filename,so_filename)==0)
            {
                pthread_mutex_lock(&mutex_socket);
//                my_free(cmd);
//                cmd = my_str_malloc(512);
                memset(cmd,0,1024);
                sprintf(cmd,"%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,sn_filename);
                pthread_mutex_unlock(&mutex_socket);
            }
        }
        else
        {
            if(strcmp(path,str_path) == 0 && strcmp(filename,so_filename)==0)
            {
                pthread_mutex_lock(&mutex_socket);
//                my_free(cmd);
//                cmd = my_str_malloc(512);
                memset(cmd,0,1024);
                sprintf(cmd,"%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,sn_filename);
                pthread_mutex_unlock(&mutex_socket);
            }
        }
    }
    else if(strcmp(cmd_name, "delete") == 0  || strcmp(cmd_name, "remove") == 0)
    {
        if(strcmp(path,str_path) == 0 && strcmp(filename,so_filename)==0)
        {
            pthread_mutex_lock(&mutex_socket);
//            my_free(cmd);
//            cmd = my_str_malloc(512);
            memset(cmd,0,1024);
            sprintf(cmd,"%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,sn_filename);
            pthread_mutex_unlock(&mutex_socket);
        }
    }
    else if(strncmp(cmd_name, "move",strlen("move")) == 0 || strncmp(cmd_name, "rename",strlen("rename")) == 0)
    {
        if(strncmp(cmd_name, "rename",strlen("rename")) == 0)
        {
            if(re_cmd)
            {
                if(strcmp(re_cmd,str_path) == 0 && strcmp(oldname,so_filename)==0)
                {
                    pthread_mutex_lock(&mutex_socket);
//                    my_free(cmd);
//                    cmd = my_str_malloc(512);
                    memset(cmd,0,1024);
                    sprintf(cmd,"%s%s%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,sn_filename,CMD_SPLIT,newname);
                    pthread_mutex_unlock(&mutex_socket);
                }
            }
            else
            {
                if(strcmp(path,str_path) == 0 && strcmp(oldname,so_filename)==0)
                {
                    pthread_mutex_lock(&mutex_socket);
                    //my_free(cmd);
                    //cmd = my_str_malloc(512);
                    memset(cmd,0,1024);
                    sprintf(cmd,"%s%s%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,sn_filename,CMD_SPLIT,newname);
                    //printf("cmd=%s\n",cmd);
                    pthread_mutex_unlock(&mutex_socket);
                }
            }
        }
        else
        {
            if(re_cmd)
            {
                if(strcmp(re_cmd,str_path) == 0 && strcmp(oldname,so_filename)==0)
                {
                    pthread_mutex_lock(&mutex_socket);
//                    my_free(cmd);
//                    cmd = my_str_malloc(512);
                    memset(cmd,0,1024);
                    sprintf(cmd,"%s%s%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,oldpath,CMD_SPLIT,sn_filename);
                    pthread_mutex_unlock(&mutex_socket);
                }
            }
            else
            {
                if(strcmp(oldpath,str_path) == 0 && strcmp(oldname,so_filename)==0)
                {
                    pthread_mutex_lock(&mutex_socket);
//                    my_free(cmd);
//                    cmd = my_str_malloc(512);
                    memset(cmd,0,1024);
                    sprintf(cmd,"%s%s%s%s%s%s%s",cmd_name,CMD_SPLIT,path,CMD_SPLIT,oldpath,CMD_SPLIT,sn_filename);
                    pthread_mutex_unlock(&mutex_socket);
                }
            }
            my_free(oldpath);
        }
    }
    else if( strcmp(cmd_name,"dragfolder") == 0)
    {}
    else if(strcmp(cmd_name, "createfolder") == 0)
    {}
    free(path);
    return 0;

}
void updata_socket_list(char *temp_name,char *new_name,int i)
{
    char *path;
    char *old_filename;
    char *p = strrchr(temp_name,'/');
    path = my_str_malloc(strlen(temp_name)-strlen(p)+1);
    snprintf(path,strlen(temp_name)-strlen(p)+1,"%s",temp_name);
    p++;
    old_filename = my_str_malloc(strlen(p)+1);
    sprintf(old_filename,"%s",p);

    p = NULL;
    char *new_filename;
    p = strrchr(new_name,'/');
    p++;
    new_filename = my_str_malloc(strlen(p)+1);
    sprintf(new_filename,"%s",p);

    wd_DEBUG("*****************updata_socket_list***************\n");
    queue_entry_t pTemp = g_pSyncList[i]->SocketActionList->head->next_ptr;//head is current socket,updata from next begin
    while(pTemp!=NULL)
    {
        check_socket_parser(pTemp->cmd_name,i,pTemp->re_cmd,path,old_filename,new_filename);
        pTemp = pTemp->next_ptr;
    }
    my_free(path);
    my_free(old_filename);
    my_free(new_filename);

    show(g_pSyncList[i]->SocketActionList->head);
}

void set_re_cmd(char *buf,char *oldpath,char *newpath)
{
    wd_DEBUG("************************set_re_cmd***********************\n");
    int i;
    char *r_path;
    r_path = get_socket_base_path(buf);
    for(i=0;i<asus_cfg.dir_number;i++)
    {
        //wd_DEBUG("asus_cfg.prule[%d]->base_path:%s\n",i,asus_cfg.prule[i]->path);
        //wd_DEBUG("r_path                       :%s\n",r_path);
        if(!strcmp(r_path,asus_cfg.prule[i]->path))
            break;
    }
    free(r_path);

    queue_entry_t pTemp = g_pSyncList[i]->SocketActionList->head;

    while(pTemp!=NULL)
    {
//        if(strncmp(pTemp->cmd_name,"rename0",strlen("rename0")) != 0 &&
//           strncmp(pTemp->cmd_name,"move0",strlen("move0")) != 0)
//        {
            char *socket_path=get_path_from_socket(pTemp->cmd_name,i);
            wd_DEBUG("path:%s\n",socket_path);
            wd_DEBUG("path:%s\n",oldpath);
            char *pTemp_t=(char *)malloc(sizeof(char)*(strlen(socket_path)+1+1));
            char *oldpath_t=(char *)malloc(sizeof(char)*(strlen(oldpath)+1+1));
            memset(pTemp_t,'\0',strlen(socket_path)+1+1);
            memset(oldpath_t,'\0',strlen(oldpath)+1+1);
            sprintf(pTemp_t,"%s/",socket_path);
            sprintf(oldpath_t,"%s/",oldpath);
            char *p_t=NULL;
            if(socket_path != NULL)
            {
                if(pTemp->re_cmd == NULL)
                {
                    //if((p_t=strstr(pTemp->cmd_name,oldpath)) != NULL)
                    if((p_t=strstr(pTemp_t,oldpath_t)) != NULL)
                    {
                        if(strlen(oldpath)<strlen(socket_path))
                        {
                            p_t=p_t+strlen(oldpath);
                            pTemp->re_cmd = (char *)malloc(sizeof(char) * (strlen(newpath)+strlen(p_t) + 1));
                            memset(pTemp->re_cmd,'\0',strlen(newpath)+strlen(p_t) + 1);
                            sprintf(pTemp->re_cmd,"%s%s",newpath,p_t);
                        }
                        else
                        {
                            pTemp->re_cmd = (char *)malloc(sizeof(char) * (strlen(newpath) + 1));
                            sprintf(pTemp->re_cmd,"%s/",newpath);
                        }
                    }
                }
                else
                {
                    //if(strstr(pTemp->re_cmd,oldpath) != NULL)
                    if((p_t=strstr(pTemp->re_cmd,oldpath_t)) != NULL)
                    {
                        if(strlen(oldpath_t)<strlen(pTemp->re_cmd))
                        {
                            p_t+=strlen(oldpath);
                            char *p_tt=(char *)malloc(sizeof(char)*(strlen(p_t)+1));
                            memset(p_tt,'\0',strlen(p_t)+1);
                            sprintf(p_tt,"%s",p_t);
                            free(pTemp->re_cmd);
                            pTemp->re_cmd = (char *)malloc(sizeof(char) * (strlen(newpath)+strlen(p_tt) + 1));
                            memset(pTemp->re_cmd,'\0',strlen(newpath)+strlen(p_tt) + 1);
                            sprintf(pTemp->re_cmd,"%s%s",newpath,p_tt);
                            free(p_tt);
                        }
                        else
                        {
                            free(pTemp->re_cmd);
                            pTemp->re_cmd = (char *)malloc(sizeof(char) * (strlen(newpath) + 1));
                            sprintf(pTemp->re_cmd,"%s/",newpath);
                        }
                    }
                }
                free(socket_path);
            }
            free(pTemp_t);
            free(oldpath_t);
//        }

        pTemp=pTemp->next_ptr;
    }

    show(g_pSyncList[i]->SocketActionList->head);
}

int change_socklist_re_cmd(char *cmd)
{
    char cmd_name[64]="\0";
    char *path;
    char *temp;
    char oldname[256]="\0",newname[256]="\0";
    char *oldpath;
    char *mv_newpath;
    char *mv_oldpath;
    char *ch;

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

    p++;
    if(strcmp(cmd_name, "rename0") == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));

        p1++;

        strcpy(newname,p1);

        //wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

        if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }
    else if(strcmp(cmd_name, "move0") == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        //wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);


        if(oldname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            free(oldpath);
            return 0;
        }
    }

    free(temp);

    if(strcmp(cmd_name, "move0") == 0)
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

    pthread_mutex_lock(&mutex_socket);
    set_re_cmd(cmd,mv_oldpath,mv_newpath);
    set_copyfile_list(cmd,mv_oldpath,mv_newpath);
    pthread_mutex_unlock(&mutex_socket);

    free(path);
    free(mv_oldpath);
    free(mv_newpath);
    return 0;

}

void *SyncLocal()
{
    //printf("it is go to SyncLocal\n");

    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd*/
    int numbytes;
    char buf[MAXDATASIZE];
    int yes = 1;
    int ret;

    fd_set read_fds;
    fd_set master;
    int fdmax;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_ZERO(&master);

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
    sin_size = sizeof(struct sockaddr_in);

    FD_SET(sockfd,&master);
    fdmax = sockfd;

    while(!exit_loop)
    { /* main accept() loop */

        timeout.tv_sec = 0;
        timeout.tv_usec = 100;

        read_fds = master;

        ret = select(fdmax+1,&read_fds,NULL,NULL,&timeout);

        switch (ret)
        {
        case 0:
            //printf("No data in ten seconds\n");
            continue;
            break;
        case -1:
            perror("select");
            continue;
            break;
        default:
            if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, \
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
            wd_DEBUG("socket buf = %s\n",buf);
            close(new_fd);

            //if(sync_down == 1)
            //{
#ifdef RENAME_F
            //rename0 or move0 is the folder not file
            if(strncmp(buf,"rename0",strlen("rename0")) == 0 || strncmp(buf,"move0",strlen("move0")) == 0)
            {
                change_socklist_re_cmd(buf);
            }
#endif
#if 1
            pthread_mutex_lock(&mutex_socket);
            add_socket_item(buf);
            pthread_mutex_unlock(&mutex_socket);
#endif
        }

    }
    close(sockfd);

    wd_DEBUG("stop dropbox local sync\n");

    //stop_down = 1;

}
int add_socket_item(char *buf){

    int i;
    //local_sync = 1;
    char *r_path;
    r_path = get_socket_base_path(buf);
    for(i=0;i<asus_cfg.dir_number;i++)
    {
        //wd_DEBUG("asus_cfg.prule[%d]->base_path:%s\n",i,asus_cfg.prule[i]->path);
        //wd_DEBUG("r_path                       :%s\n",r_path);
        if(!strcmp(r_path,asus_cfg.prule[i]->path))
            break;
    }

    wd_DEBUG("add_socket_item rule:%d\n",i);
    free(r_path);
    pthread_mutex_lock(&mutex_receve_socket);
    //receve_socket = 1;
    g_pSyncList[i]->receve_socket = 1;
    pthread_mutex_unlock(&mutex_receve_socket);

#if MEM_POOL_ENABLE
    SocketActionTmp = mem_alloc(16);
#else
    SocketActionTmp = malloc (sizeof (struct queue_entry));
#endif

    //SocketActionTmp = malloc (sizeof (struct queue_entry));
    memset(SocketActionTmp,0,sizeof(struct queue_entry));
    int len = strlen(buf)+1;
#if MEM_POOL_ENABLE
    SocketActionTmp->cmd_name = mem_alloc(len);
#else
    SocketActionTmp->cmd_name = (char *)calloc(len,sizeof(char));
#endif
    sprintf(SocketActionTmp->cmd_name,"%s",buf);
    SocketActionTmp->re_cmd = NULL;
    SocketActionTmp->is_first = 0;
    queue_enqueue(SocketActionTmp,g_pSyncList[i]->SocketActionList);

    wd_DEBUG("SocketActionTmp->cmd_name = %s\n",SocketActionTmp->cmd_name);
    return 0;
}
char *get_socket_base_path(char *cmd){

    //printf("get_socket_base_path cmd : %s\n",cmd);

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
        //temp1 = strchr(temp,'@');
        temp1 = strchr(temp,'\n');
        memset(path,0,sizeof(path));
        strncpy(path,temp,strlen(temp)-strlen(temp1));

        //printf("get_socket_base_path path = %s\n",path);

        root_path = my_str_malloc(512);
        if(strncmp(path,"/tmp",4) ==0 )
        {
            temp = my_nstrchr('/',path,5);
        }
        else
        {
            temp = my_nstrchr('/',path,4);
        }
        if(temp == NULL)
        {
            sprintf(root_path,"%s",path);
        }
        else
        {
            snprintf(root_path,strlen(path)-strlen(temp)+1,"%s",path);
        }
    }
    //printf("get_socket_base_path root_path = %s\n",root_path);
    return root_path;
}
void run()
{

    int create_thid1 = 0;
    int create_thid2 = 0;
    int create_thid3 = 0;
    int need_server_thid = 0;


    create_sync_list();
    send_to_inotify();

    if(exit_loop == 0)
    {
        if( pthread_create(&newthid2,NULL,(void *)SyncLocal,NULL) != 0)
        {
            wd_DEBUG("thread creation failder\n");
            exit(1);
        }
        create_thid2 = 1;
    }
    //pthread_join(newthid2,NULL);
#if 1
    if(exit_loop == 0)
    {

        wd_DEBUG("create newthid3\n");
        //write_debug_log("create newthid3");

        if( pthread_create(&newthid3,NULL,(void *)Socket_Parser,NULL) != 0)
        {
            wd_DEBUG("thread creation failder\n");
            exit(1);
        }
        create_thid3 = 1;
        usleep(1000*500);

    }

#if 1
//    if(auth_ok)
//    {
        sync_initial();
//    }
#endif

    finished_initial=1;
//    need_server_thid = get_create_threads_state();

//    if(need_server_thid && exit_loop == 0)
//    {
//        if( pthread_create(&newthid1,NULL,(void *)SyncServer,NULL) != 0)
//        {
//            wd_DEBUG("thread creation failder\n");
//            exit(1);
//        }
//        create_thid1 = 1;
//        usleep(1000*500);
//    }
//    else
//    {
//        server_sync = 0;
//    }

    /*
     fix when socket_parse run sync_initial_again ,local send socket,the process will sleep
    */

    if(exit_loop == 0)
    {
        if( pthread_create(&newthid1,NULL,(void *)SyncServer,NULL) != 0)
        {
            wd_DEBUG("thread creation failder\n");
            exit(1);
        }
        create_thid1 = 1;
        usleep(1000*500);
    }

    if(create_thid1)
        pthread_join(newthid1,NULL);
    if(create_thid3)
        pthread_join(newthid3,NULL);
    if(create_thid2)
        pthread_join(newthid2,NULL);

    usleep(1000);
    clean_up();
#if TOKENFILE
    if(stop_progress != 1)
    {
        wd_DEBUG("run again!\n");


        //#ifndef NVRAM_
        while(disk_change)
        {
            //write_debug_log("while disk_change");
            disk_change = 0;
            sync_disk_removed = check_sync_disk_removed();

            if(sync_disk_removed == 2)
            {
                wd_DEBUG("sync path is change\n");
            }
            else if(sync_disk_removed == 1)
            {
                wd_DEBUG("sync disk is unmount\n");
            }
            else if(sync_disk_removed == 0)
            {
                wd_DEBUG("sync disk exists\n");
            }
        }
        //#endif

        exit_loop = 0;
        //read_config();
        run();
    }
#endif
#endif
}

void clean_up()
{

    wd_DEBUG("enter clean up\n");

    int i;


    for(i=0;i<asus_cfg.dir_number;i++)
    {
        queue_destroy(g_pSyncList[i]->SocketActionList);

        //printf("the pointer g_pSyncList[i]->ServerRootNode = %p\n",g_pSyncList[i]->ServerRootNode);
        if(g_pSyncList[i]->ServerRootNode == g_pSyncList[i]->OldServerRootNode)
        {

            wd_DEBUG("the same Pointer!\n");

            if(g_pSyncList[i]->ServerRootNode != NULL)
                free_server_tree(g_pSyncList[i]->ServerRootNode);
        }
        else
        {
            if(g_pSyncList[i]->ServerRootNode != NULL)
                free_server_tree(g_pSyncList[i]->ServerRootNode);

            wd_DEBUG("clean %d ServerRootNode success!\n",i);

            if(g_pSyncList[i]->OldServerRootNode != NULL)
                free_server_tree(g_pSyncList[i]->OldServerRootNode);

            wd_DEBUG("clean %d OldServerRootNode success!\n",i);

        }

        free_action_item(g_pSyncList[i]->server_action_list);
        free_action_item(g_pSyncList[i]->unfinished_list);
        free_action_item(g_pSyncList[i]->copy_file_list);
        free_action_item(g_pSyncList[i]->up_space_not_enough_list);

        if(asus_cfg.prule[i]->rule == 1)
        {
            free_action_item(g_pSyncList[i]->download_only_socket_head);
        }
        free(g_pSyncList[i]);
        //printf("clean %d up_space_not_enough_list success!\n",i);

    }
    free(g_pSyncList);

#if MEM_POOL_ENABLE
    mem_pool_destroy();
#endif

    wd_DEBUG("clean up end !!!\n");

}

void *SyncServer()
{
    struct timeval now;
    struct timespec outtime;

    int status;
    int i;
    while( !exit_loop )
    {

        wd_DEBUG("***************SyncServer start**************\n");

        for(i=0;i<asus_cfg.dir_number;i++)
        {
            status=0;

            //wd_DEBUG("the %d SyncServer\n",i);

            while (local_sync == 1 && exit_loop == 0){
                //printf("local_sync = %d\n",local_sync);
                //sleep(2);
                usleep(1000*10);
                //server_sync = 0;
            }
            server_sync = 1;

            if(exit_loop)
                break;
#if TOKENFILE
            if(disk_change)
            {
                //disk_change = 0;
                check_disk_change();
            }
            if(g_pSyncList[i]->sync_disk_exist == 0)
            {
                write_log(S_ERROR,"Sync disk unplug!","",i);
                continue;
            }

#endif
            if(g_pSyncList[i]->no_local_root)
            {
                my_mkdir_r(asus_cfg.prule[i]->path);   //have mountpath
                send_action(asus_cfg.type,asus_cfg.prule[i]->path);
                usleep(1000*10);
                g_pSyncList[i]->no_local_root = 0;
                g_pSyncList[i]->init_completed = 0;
            }

            status = do_unfinished(i);

            if( !g_pSyncList[i]->init_completed )
                status = sync_initial();

            if(status != 0)
                continue;

            if(asus_cfg.prule[i]->rule == 2)
            {
                //write_log(S_SYNC,"","",i);
                continue;
            }

            if(exit_loop == 0)
            {
                int get_serlist_fail_time = 0;
                do
                {
                    g_pSyncList[i]->ServerRootNode = create_server_treeroot();
#ifdef MULTI_PATH
                    status = browse_to_tree(asus_cfg.prule[i]->rooturl,g_pSyncList[i]->ServerRootNode);
#else
                    status = browse_to_tree("/",g_pSyncList[i]->ServerRootNode);
#endif
#ifdef __DEBUG__
                    SearchServerTree(g_pSyncList[i]->ServerRootNode);
#endif
                    /*
                for(i=0;i<asus_cfg.dir_number;i++)
                {
                    SearchServerTree(g_pSyncList[i]->ServerRootNode);
                    free_server_tree(g_pSyncList[i]->ServerRootNode);
                }
                */
                    if(status != 0)
                    {
                        wd_DEBUG("get ServerList ERROR! \n");
                        get_serlist_fail_time ++;
                        free_server_tree(g_pSyncList[i]->ServerRootNode);
                        g_pSyncList[i]->ServerRootNode = NULL;
                    }


                }while(status!=0 && get_serlist_fail_time < 5 && exit_loop == 0 && g_pSyncList[i]->receve_socket == 0);
                if (status != 0)
                {

                    //wd_DEBUG("get ServerList ERROR! \n");

                    /*auth again:
                        for the token not work!
                    */
#ifdef OAuth1
                    if(g_pSyncList[i]->receve_socket == 0)
                    {
                        if(exit_loop == 0)
                            do_auth();
                    }
#endif
                    /*for get serverlist fail,then mem will updata*/
                    //free_server_tree(g_pSyncList[i]->ServerRootNode);
                    //g_pSyncList[i]->ServerRootNode = NULL;

                    /*first_sync:
                        after the initial finish,the serverlist is change so force to run server sync;
                        when the server sync failed,next time we must force to run server sync;
                    */
                    g_pSyncList[i]->first_sync = 1;
                    //sleep(2);
                    usleep(1000*20);
                    continue;
                    //break;
                }

                if(g_pSyncList[i]->unfinished_list->next != NULL)
                {
                    continue;
                }

                if(g_pSyncList[i]->first_sync)
                {

                    wd_DEBUG("first sync!\n");
                    g_pSyncList[i]->VeryOldServerRootNode=g_pSyncList[i]->OldServerRootNode;
                    //g_pSyncList[i]->first_sync = 0;
                    //free_server_tree(g_pSyncList[i]->OldServerRootNode);
                    g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                    //getLocalList();
                    status=Server_sync(i);
                    free_server_tree(g_pSyncList[i]->VeryOldServerRootNode);
                    if(status == 0)
                        g_pSyncList[i]->first_sync = 0;
                    else
                        g_pSyncList[i]->first_sync = 1;
                }
                else
                {
                    if(asus_cfg.prule[i]->rule == 0)
                    {
                        status=compareServerList(i);
                    }
                    //serverList different or download only
                    if (status == 0 || asus_cfg.prule[i]->rule == 1)
                    {

                        g_pSyncList[i]->VeryOldServerRootNode=g_pSyncList[i]->OldServerRootNode;
                        //g_pSyncList[i]->first_sync = 0;
                        //free_server_tree(g_pSyncList[i]->OldServerRootNode);
                        g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                        //getLocalList();
                        status=Server_sync(i);
                        free_server_tree(g_pSyncList[i]->VeryOldServerRootNode);
                        if(status == 0)
                            g_pSyncList[i]->first_sync = 0;
                        else
                            g_pSyncList[i]->first_sync = 1;
                    }
                    else
                    {//serverList same

                        free_server_tree(g_pSyncList[i]->ServerRootNode);
                        g_pSyncList[i]->ServerRootNode = NULL;
                        status = 0;
                    }
                }

                /*
                if(status == 0 || asus_cfg.prule[i]->rule == 1)
                {
                    status=Server_sync(i);
                }
                free_server_tree(g_pSyncList[i]->OldServerRootNode);
                g_pSyncList[i]->OldServerRootNode = g_pSyncList[i]->ServerRootNode;
                */
            }
//            if(!status)
                write_log(S_SYNC,"","",i);
//            else
//            {
//                write_log(S_ERROR,"Local synchronization is not entirely successful,failure information,please refer errlog","",i);
//            }

        }
        server_sync = 0;      //server sync finished
        pthread_mutex_lock(&mutex);
        if(!exit_loop)
        {
            gettimeofday(&now, NULL);
            outtime.tv_sec = now.tv_sec + 10;
            outtime.tv_nsec = now.tv_usec * 1000;
            pthread_cond_timedwait(&cond, &mutex, &outtime);
        }
        pthread_mutex_unlock(&mutex);
    }


    wd_DEBUG("stop dropbox server sync\n");

}

int Server_sync(int index)
{

    wd_DEBUG("compareLocalList start!\n");

    int ret = 0;

    if(g_pSyncList[index]->ServerRootNode->Child != NULL)
    {

        wd_DEBUG("ServerRootNode->Child != NULL\n");

        ret = sync_local_with_server(g_pSyncList[index]->ServerRootNode->Child,sync_local_with_server_perform,index);
    }
    else
    {

        wd_DEBUG("ServerRootNode->Child == NULL\n");

    }

    return ret;
}

/*
 *judge is server changed
 *0:server changed
 *1:server is not changed
*/
int isServerChanged(Server_TreeNode *newNode,Server_TreeNode *oldNode)
{
    //printf("isServerChanged start!\n");
    int res = 1;
    int serverchanged = 0;
    if(newNode->browse == NULL && oldNode->browse == NULL)
    {

        //wd_DEBUG("########Server is not change\n");

        return 1;
    }
    else if(newNode->browse == NULL && oldNode->browse != NULL)
    {

        //wd_DEBUG("########Server changed1\n");

        return 0;
    }
    else if(newNode->browse != NULL && oldNode->browse == NULL)
    {

        //wd_DEBUG("########Server changed2\n");

        return 0;
    }
    else
    {
        if(newNode->browse->filenumber != oldNode->browse->filenumber || newNode->browse->foldernumber != oldNode->browse->foldernumber)
        {

            //wd_DEBUG("########Server changed3\n");

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

                    //wd_DEBUG("########Server changed4\n");

                    return 0;
                }
                newfoldertmp = newfoldertmp->next;
                oldfoldertmp = oldfoldertmp->next;
            }
            while (newfiletmp != NULL || oldfiletmp != NULL)
            {
                if ((cmp = strcmp(newfiletmp->href,oldfiletmp->href)) != 0)
                {

                    //wd_DEBUG("########Server changed5\n");

                    return 0;
                }
                if (newfiletmp->mtime != oldfiletmp->mtime)
                {
                    //printf("newpath=%s,newtime=%lu\n",newfiletmp->href,newfiletmp->modtime);
                    //printf("oldpath=%s,oldtime=%lu\n",oldfiletmp->href,oldfiletmp->modtime);

                    //wd_DEBUG("########Server changed6\n");

                    return 0;
                }
                newfiletmp = newfiletmp->next;
                oldfiletmp = oldfiletmp->next;
            }
        }

        if((newNode->Child == NULL && oldNode->Child != NULL) || (newNode->Child != NULL && oldNode->Child == NULL))
        {

            //wd_DEBUG("########Server changed7\n");

            return 0;
        }
        if((newNode->NextBrother == NULL && oldNode->NextBrother != NULL) || (newNode->NextBrother!= NULL && oldNode->NextBrother == NULL))
        {

            //wd_DEBUG("########Server changed8\n");

            return 0;
        }

        if(newNode->Child != NULL && oldNode->Child != NULL && !exit_loop)
        {
            res = isServerChanged(newNode->Child,oldNode->Child);
            if(res == 0)
            {
                serverchanged = 1;
            }
        }
        if(newNode->NextBrother != NULL && oldNode->NextBrother != NULL && !exit_loop)
        {
            res = isServerChanged(newNode->NextBrother,oldNode->NextBrother);
            if(res == 0)
            {
                serverchanged = 1;
            }
        }
    }
    //wd_DEBUG("#########compareServerList over\n");
    if(serverchanged == 1)
    {

        wd_DEBUG("########Server changed9\n");

        return 0;
    }
    else
    {
        //printf("########Server is not change\n");
        return 1;
    }
}

/*ret = 0,server changed
 *ret = 1,server is no changed
*/
int compareServerList(int index)
{
    int ret;

    wd_DEBUG("#########compareServerList\n");

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
int get_create_threads_state()
{
    int i;
    for(i=0;i<asus_cfg.dir_number;i++)
    {
        if(asus_cfg.prule[i]->rule != 2)
        {
            return 1;
        }
    }

    return 0;
}
int download_only_add_socket_item(char *cmd,int index)
{

    wd_DEBUG("download_only_add_socket_item receive socket : %s\n",cmd);

    if( strstr(cmd,"(conflict)") != NULL )
        return 0;

    wd_DEBUG("socket command is %s \n",cmd);


    if( !strncmp(cmd,"exit",4))
    {

        wd_DEBUG("exit socket\n");

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

    //printf("temp = %s\n",temp);
    //printf("p = %s\n",p);
    //printf("strlen(temp)- strlen(p) = %d\n",strlen(temp)- strlen(p));

    path = my_str_malloc((size_t)(strlen(temp)- strlen(p)+1));

    //printf("path = %s\n",path);

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

        wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

    }
    else if(strncmp(cmd_name, "move",4) == 0)
    {
        char *p1 = NULL;

        p1 = strchr(p,'\n');
        //p1 = strchr(p,'@');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);

    }
    else
    {
        strcpy(filename,p);
        //fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));

        wd_DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);

    }

    free(temp);

    if( !strncmp(cmd_name,"rename",strlen("rename")) )
    {
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(newname)+2));
        old_fullname = my_str_malloc((size_t)(strlen(path)+strlen(oldname)+2));
        sprintf(fullname,"%s/%s",path,newname);
        sprintf(old_fullname,"%s/%s",path,oldname);
        free(path);
    }
    else if( !strncmp(cmd_name,"move",strlen("move")) )
    {
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(oldname)+2));
        old_fullname = my_str_malloc((size_t)(strlen(oldpath)+strlen(oldname)+2));
        sprintf(fullname,"%s/%s",path,oldname);
        sprintf(old_fullname,"%s/%s",oldpath,oldname);
        free(oldpath);
        free(path);
    }
    else
    {
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
        sprintf(fullname,"%s/%s",path,filename);
        free(path);
    }
    if( !strncmp(cmd_name,"copyfile",strlen("copyfile")) )
    {
        add_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
        free(fullname);
        return 0;
    }


    if( strcmp(cmd_name, "createfile") == 0 )
    {
        strcpy(action,"createfile");
        action_item *item;

        item = get_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### delete copyfile %s ######\n",fullname);

            //pthread_mutex_lock(&mutex);
            del_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
        }
    }
    else if( strcmp(cmd_name, "cancelcopy") == 0 )
    {
        action_item *item;

        item = get_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### delete copyfile %s ######\n",fullname);

            //pthread_mutex_lock(&mutex);
            del_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
        }
        free(fullname);
        return 0;
    }
    else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
    {
        strcpy(action,"remove");
        del_download_only_action_item(action,fullname,g_pSyncList[index]->download_only_socket_head);
    }
    else if( strcmp(cmd_name, "createfolder") == 0 )
    {
        strcpy(action,"createfolder");
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

        item = get_action_item(action,fullname,g_pSyncList[index]->server_action_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### %s %s by dropbox Server self ######\n",action,fullname);

            //pthread_mutex_lock(&mutex);
            del_action_item(action,fullname,g_pSyncList[index]->server_action_list);

            //pthread_mutex_unlock(&mutex);
            //local_sync = 0;
            free(fullname);
            return 0;
        }
    }

    if( strcmp(cmd_name, "copyfile") != 0 )
    {
        g_pSyncList[index]->have_local_socket = 1;
    }


    //}
    //printf("add download_only_socket_head fullname = %s\n",fullname);
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
        if(test_if_dir(fullname))
        {
            add_all_download_only_socket_list(cmd_name,fullname,index);
        }
        else
        {
            add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
        }
    }
    else if(strcmp(cmd_name, "createfolder") == 0 || strcmp(cmd_name, "dragfolder") == 0)
    {
        add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
        if(!strcmp(cmd_name, "dragfolder"))
            add_all_download_only_dragfolder_socket_list(fullname,index);
    }
    else if( strcmp(cmd_name, "createfile") == 0  || strcmp(cmd_name, "dragfile") == 0 || strcmp(cmd_name, "modify") == 0)
    {
        add_action_item(cmd_name,fullname,g_pSyncList[index]->download_only_socket_head);
    }

    free(fullname);
    return 0;


}
void *Socket_Parser()
{

    wd_DEBUG("*******Socket_Parser start********\n");

    queue_entry_t socket_execute;
    int i;
    int status;
    int has_socket = 0;
    int fail_flag;
    struct timeval now;
    struct timespec outtime;
    while(!exit_loop)
    {
        for(i=0;i<asus_cfg.dir_number;i++)
        {
            while(server_sync == 1 && exit_loop ==0)
            {
                usleep(1000*10);
            }
            local_sync=1;

            if(exit_loop)
                break;
#if TOKENFILE
            if(disk_change)
            {
                //disk_change = 0;
                check_disk_change();
            }

            if(g_pSyncList[i]->sync_disk_exist == 0)
                continue;
#endif
            if(asus_cfg.prule[i]->rule == 1) //Download only
            {
                while(exit_loop == 0)
                {
                    while(g_pSyncList[i]->SocketActionList->head != NULL && exit_loop == 0 && server_sync == 0)
                    {
                        has_socket = 1;
                        socket_execute=g_pSyncList[i]->SocketActionList->head;
                        status = download_only_add_socket_item(socket_execute->cmd_name,i);
                        if(status == 0  || status == SERVER_SPACE_NOT_ENOUGH
                           || status == LOCAL_FILE_LOST)
                        {
                            pthread_mutex_lock(&mutex_socket);
                            socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList);

#if MEM_POOL_ENABLE
                    mem_free(socket_execute->cmd_name);
                    mem_free(socket_execute->re_cmd);
                    mem_free(socket_execute);
#else
                    if(socket_execute->re_cmd)
                        free(socket_execute->re_cmd);
                    if(socket_execute->cmd_name)
                        free(socket_execute->cmd_name);
                    free(socket_execute);
#endif
                            //printf("del socket item ok\n");
                            pthread_mutex_unlock(&mutex_socket);
                        }
                        else
                        {
                            fail_flag = 1;

                            wd_DEBUG("######## socket item fail########\n");

                            break;
                        }
                        //sleep(2);
                        usleep(1000*20);
                    }
                    if(fail_flag)
                        break;

                    if(g_pSyncList[i]->copy_file_list->next == NULL)
                    {
                        break;
                    }
                    else
                    {
                        usleep(1000*100);
                        //sleep(1);
                    }
                }
                if(g_pSyncList[i]->server_action_list->next != NULL && g_pSyncList[i]->SocketActionList->head == NULL)
                {
                    free_action_item(g_pSyncList[i]->server_action_list);
                    g_pSyncList[i]->server_action_list = create_action_item_head();
                }
                pthread_mutex_lock(&mutex_receve_socket);
                //receve_socket = 0;
                if(g_pSyncList[i]->SocketActionList->head == NULL)
                    g_pSyncList[i]->receve_socket = 0;
                pthread_mutex_unlock(&mutex_receve_socket);
            }
            else
            {
                if(asus_cfg.prule[i]->rule == 2)
                {
#if 0
                    /*
                     fix upload only rule rm sync dir , can not create new sync dir;
                    */

                    if(g_pSyncList[i]->no_local_root)
                    {
                        my_mkdir_r(asus_cfg.prule[i]->path);   //have mountpath
                        send_action(asus_cfg.type,asus_cfg.prule[i]->path);
                        usleep(1000*10);
                        g_pSyncList[i]->no_local_root = 0;
                        //g_pSyncList[i]->init_completed = 0;
                    }

                    /*
                     fix upload only initial failed,must run intial again();
                    */
                    if(finished_initial)
                    {
                        if( g_pSyncList[i]->init_completed != 1)
                        {
                            sync_initial_again(i);
                        }
                    }

                    /*upload only rule is not server pthread,so unfinished list del is here*/
                    status = do_unfinished(i);
#endif
                }
                while(exit_loop == 0)
                {
                    while(g_pSyncList[i]->SocketActionList->head != NULL && server_sync ==0 && exit_loop ==0)
                    {
                        has_socket = 1;
                        socket_execute=g_pSyncList[i]->SocketActionList->head;

                        wd_DEBUG("######## socket cmd : %s########\n",socket_execute->cmd_name);

                        status=cmd_parser(socket_execute->cmd_name,i,socket_execute->re_cmd);
                        if(status == 0 || status == SERVER_SPACE_NOT_ENOUGH )
                        {
                            wd_DEBUG("del socket item ok?\n");
                            pthread_mutex_lock(&mutex_socket);
                            socket_execute = queue_dequeue(g_pSyncList[i]->SocketActionList);
#if MEM_POOL_ENABLE
                    mem_free(socket_execute->cmd_name);
                    mem_free(socket_execute->re_cmd);
                    mem_free(socket_execute);
#else
                    if(socket_execute->re_cmd)
                        free(socket_execute->re_cmd);
                    if(socket_execute->cmd_name)
                        free(socket_execute->cmd_name);
                    free(socket_execute);
#endif
                            wd_DEBUG("del socket item ok\n");
                            pthread_mutex_unlock(&mutex_socket);
                        }
                        else if(status == LOCAL_FILE_LOST )
                        {
                            wd_DEBUG("del socket item ok?\n");
                            pthread_mutex_lock(&mutex_socket);
                            socket_execute = queue_dequeue_t(g_pSyncList[i]->SocketActionList);
#if MEM_POOL_ENABLE
                    mem_free(socket_execute->cmd_name);
                    mem_free(socket_execute->re_cmd);
                    mem_free(socket_execute);
#else
                    if(socket_execute->re_cmd)
                        free(socket_execute->re_cmd);
                    if(socket_execute->cmd_name)
                        free(socket_execute->cmd_name);
                    free(socket_execute);
#endif
                            wd_DEBUG("del socket item ok\n");
                            pthread_mutex_unlock(&mutex_socket);
                        }
                        else
                        {
                            fail_flag = 1;

                            //wd_DEBUG("######## socket item fail########\n");

                            break;
                        }
                        //sleep(2);
                        usleep(1000*20);
                    }
                    if(fail_flag)
                        break;

                    //wd_DEBUG("######## socket del finished########\n");

                    if(g_pSyncList[i]->copy_file_list->next == NULL)
                    {
                        break;
                    }
                    else
                    {
                        //sleep(1);
                        usleep(1000*100);
                    }
                }
                if(g_pSyncList[i]->server_action_list->next != NULL && g_pSyncList[i]->SocketActionList->head == NULL)
                {
                    free_action_item(g_pSyncList[i]->server_action_list);
                    g_pSyncList[i]->server_action_list = create_action_item_head();
                }
                //wd_DEBUG("#### clear server_action_list success!\n");
                pthread_mutex_lock(&mutex_receve_socket);
                if(g_pSyncList[i]->SocketActionList->head == NULL)
                {
                    g_pSyncList[i]->receve_socket = 0;
                }
                pthread_mutex_unlock(&mutex_receve_socket);
            }

        }
        local_sync = 0;
        pthread_mutex_lock(&mutex_socket);
        if(!exit_loop)
        {
            gettimeofday(&now, NULL);
            outtime.tv_sec = now.tv_sec + 2;
            outtime.tv_nsec = now.tv_usec * 1000;
            pthread_cond_timedwait(&cond_socket, &mutex_socket, &outtime);
        }
        pthread_mutex_unlock(&mutex_socket);
    }
}
int cmd_parser(char *cmd,int index,char *re_cmd)
{
//    if(strstr(cmd,"conflict") != NULL)
//        return 0;
    if( !strncmp(cmd,"exit",4))
    {

        wd_DEBUG("exit socket\n");

        return 0;
    }

    if(!strncmp(cmd,"rmroot",6))
    {
        g_pSyncList[index]->no_local_root = 1;
        return 0;
    }

    char cmd_name[64]="\0";
    char *path;
    char *temp;
    char filename[256]="\0";
    char *fullname;
    char *fullname_t = NULL;
    char oldname[256]="\0",newname[256]="\0";
    char *oldpath;
    char action[64]="\0";
    char *cmp_name;
    char *mv_newpath;
    char *mv_oldpath;
    char *ch;
    int status;

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

    p++;
    if(strncmp(cmd_name, "rename",strlen("rename")) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        if(p1 != NULL)
            strncpy(oldname,p,strlen(p)- strlen(p1));

        p1++;

        strcpy(newname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldname: [%s],newname: [%s]\n",cmd_name,path,oldname,newname);

        if(newname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }
    else if(strncmp(cmd_name, "move",strlen("move")) == 0)
    {
        char *p1 = NULL;

        //p1 = strchr(p,'@');
        p1 = strchr(p,'\n');

        oldpath = my_str_malloc((size_t)(strlen(p)- strlen(p1)+1));

        if(p1 != NULL)
            snprintf(oldpath,strlen(p)- strlen(p1)+1,"%s",p);

        p1++;

        strcpy(oldname,p1);

        wd_DEBUG("cmd_name: [%s],path: [%s],oldpath: [%s],oldname: [%s]\n",cmd_name,path,oldpath,oldname);


        if(oldname[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            free(oldpath);
            return 0;
        }
    }
    else if(strcmp(cmd_name, "delete") == 0 || strcmp(cmd_name, "remove") == 0)
    {
        strcpy(filename,p);
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));

        wd_DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);

        if(filename[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }
    else
    {
        strcpy(filename,p);
        fullname = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
        if(re_cmd != NULL)
            fullname_t = my_str_malloc((size_t)(strlen(re_cmd)+strlen(filename)+2));

        wd_DEBUG("cmd_name: [%s],path: [%s],filename: [%s]\n",cmd_name,path,filename);

        if(filename[0] == '.' || (strstr(path,"/.")) != NULL)
        {
            free(temp);
            free(path);
            return 0;
        }
    }

    free(temp);

    if( !strncmp(cmd_name,"rename",strlen("rename")) )
    {
        cmp_name = my_str_malloc((size_t)(strlen(path)+strlen(newname)+2));
        sprintf(cmp_name,"%s/%s",path,newname);
    }
    else if( !strcmp(cmd_name,"delete") || !strcmp(cmd_name,"remove"))
    {
        cmp_name = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
        sprintf(cmp_name,"%s/%s",path,filename);
    }
    else
    {
        if(re_cmd == NULL)
        {
            cmp_name = my_str_malloc((size_t)(strlen(path)+strlen(filename)+2));
            sprintf(cmp_name,"%s/%s",path,filename);
        }
        else
        {
            cmp_name = my_str_malloc((size_t)(strlen(re_cmd)+strlen(filename)+2));
            sprintf(cmp_name,"%s%s",re_cmd,filename);
        }
    }

    if( strcmp(cmd_name, "createfile") == 0 )
    {
        strcpy(action,"createfile");
        action_item *item;

        item = get_action_item("copyfile",cmp_name,g_pSyncList[index]->copy_file_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### delete copyfile %s ######\n",cmp_name);

            del_action_item("copyfile",cmp_name,g_pSyncList[index]->copy_file_list);
        }
    }
    else if( strcmp(cmd_name, "cancelcopy") == 0 )
    {
        action_item *item;

        item = get_action_item("copyfile",cmp_name,g_pSyncList[index]->copy_file_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### delete copyfile %s ######\n",cmp_name);

            del_action_item("copyfile",cmp_name,g_pSyncList[index]->copy_file_list);
        }
        free(path);
        free(cmp_name);
        free(fullname);
        return 0;
    }
    else if( strcmp(cmd_name, "remove") == 0  || strcmp(cmd_name, "delete") == 0)
    {
        strcpy(action,"remove");
    }
    else if( strcmp(cmd_name, "createfolder") == 0 )
    {
        strcpy(action,"createfolder");
    }
    else if( strncmp(cmd_name, "rename",strlen("rename")) == 0 )
    {
        strcpy(action,"rename");
    }
#if 1
    if(g_pSyncList[index]->server_action_list->next != NULL)
    {
        action_item *item;

        item = get_action_item(action,cmp_name,g_pSyncList[index]->server_action_list,index);

        if(item != NULL)
        {

            wd_DEBUG("##### %s %s by Dropbox Server self ######\n",action,cmp_name);

            //pthread_mutex_lock(&mutex);
            del_action_item(action,cmp_name,g_pSyncList[index]->server_action_list);

            wd_DEBUG("#### del action item success!\n");

            //pthread_mutex_unlock(&mutex);
            //local_sync = 0;
            free(path);
            if( strncmp(cmd_name, "rename",strlen("rename")) != 0 )
                free(fullname);
            free(cmp_name);
            return 0;
        }
    }
#endif
    free(cmp_name);


    wd_DEBUG("###### %s is start ######\n",cmd_name);
    //write_system_log(cmd_name,"start");


    if( strcmp(cmd_name, "copyfile") != 0 )
    {
        g_pSyncList[index]->have_local_socket = 1;
    }

    if( strcmp(cmd_name, "createfile") == 0 || strcmp(cmd_name, "dragfile") == 0 )
    {

        sprintf(fullname,"%s/%s",path,filename);
        char *serverpath=localpath_to_serverpath(fullname,index);
        if(re_cmd != NULL)
        {
            sprintf(fullname_t,"%s%s",re_cmd,filename);
            status=upload_file(fullname_t,serverpath,0,index);
        }
        else
        {
            status=upload_file(fullname,serverpath,0,index);
        }

        /*third param flase,false=>if server file has exit ,will not be overwrite*/
        if(status == 0)
        {
            char *newlocalname = NULL;
            cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
            time_t mtime=cJSON_printf(json,"modified");
            cJSON_Delete(json);
            if(re_cmd == NULL)
                newlocalname=change_local_same_file(fullname,index);
            else
                newlocalname=change_local_same_file(fullname_t,index);
            if(newlocalname)
                ChangeFile_modtime(newlocalname,mtime);
            if(re_cmd)
                free(fullname_t);
            free(fullname);
            free(newlocalname);
            free(serverpath);
        }
        else
        {

            wd_DEBUG("upload %s failed\n",fullname);
            //write_system_log("error","uploadfile fail");
            if(re_cmd)
                free(fullname_t);
            free(path);
            free(fullname);
            free(serverpath);
            return status;
        }
    }
    else if( strcmp(cmd_name, "copyfile") == 0 )
    {
        if(re_cmd == NULL)
        {
            sprintf(fullname,"%s/%s",path,filename);
            add_action_item("copyfile",fullname,g_pSyncList[index]->copy_file_list);
        }
        else
        {
            sprintf(fullname_t,"%s%s",re_cmd,filename);
            add_action_item("copyfile",fullname_t,g_pSyncList[index]->copy_file_list);
        }
        free(fullname);
        if(re_cmd)
            free(fullname_t);
    }
    else if( strcmp(cmd_name, "modify") == 0 )
    {
        time_t mtime_1,mtime_2;

        sprintf(fullname,"%s/%s",path,filename);
        char *serverpath=localpath_to_serverpath(fullname,index);
        if(re_cmd != NULL)
            sprintf(fullname_t,"%s%s",re_cmd,filename);

        CloudFile *filetmp;
        filetmp=get_CloudFile_node(g_pSyncList[index]->OldServerRootNode,serverpath,0x2);
        if(filetmp == NULL)
        {
            mtime_1=(time_t)-1;
        }
        else
        {
            mtime_1=filetmp->mtime;
        }
        mtime_2=api_getmtime(serverpath,dofile);
        if(mtime_2 == -1)
        {
            /*only upload */
            /*third param ture,ture=>if server file has exit ,will be overwrite*/
            if(re_cmd == NULL)
                status=upload_file(fullname,serverpath,1,index);
            else
                status=upload_file(fullname_t,serverpath,1,index);
            if(status == 0)
            {
                cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                time_t mtime=cJSON_printf(json,"modified");
                cJSON_Delete(json);
                if(re_cmd == NULL)
                    ChangeFile_modtime(fullname,mtime);
                else
                    ChangeFile_modtime(fullname_t,mtime);
                if(re_cmd)
                    free(fullname_t);
                free(fullname);
                free(serverpath);
            }
            else
            {

                wd_DEBUG("upload %s failed\n",fullname);
                //write_system_log("error","uploadfile fail");

                if(re_cmd)
                    free(fullname_t);
                free(path);
                free(fullname);
                free(serverpath);
                return status;
            }
        }
        else if(mtime_2 != -1 && mtime_2 != -2)
        {
            if(mtime_1 == mtime_2)
            {
                /*third param ture,ture=>if server file has exit ,will be overwrite*/
                if(re_cmd == NULL)
                    status=upload_file(fullname,serverpath,1,index);
                else
                    status=upload_file(fullname_t,serverpath,1,index);
                if(status == 0)
                {
                    cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);
                    if(re_cmd == NULL)
                        ChangeFile_modtime(fullname,mtime);
                    else
                        ChangeFile_modtime(fullname_t,mtime);
                    if(re_cmd)
                        free(fullname_t);
                    free(fullname);
                    free(serverpath);
                }
                else
                {

                    wd_DEBUG("upload %s failed\n",fullname);
                    //write_system_log("error","uploadfile fail");

                    if(re_cmd)
                        free(fullname_t);
                    free(path);
                    free(fullname);
                    free(serverpath);
                    return status;
                }
            }
            else
            {
                /*rename then upload*/
                /*third param false,false=>if server file has exit ,will not be overwrite*/
                if(re_cmd == NULL)
                    status=upload_file(fullname,serverpath,0,index);
                else
                    status=upload_file(fullname_t,serverpath,0,index);
                if(status == 0)
                {
                    char *newlocalname = NULL;
                    cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);
                    if(re_cmd == NULL)
                        newlocalname=change_local_same_file(fullname,index);
                    else
                        newlocalname=change_local_same_file(fullname_t,index);
                    if(newlocalname)
                        ChangeFile_modtime(newlocalname,mtime);
                    if(re_cmd)
                        free(fullname_t);
                    free(newlocalname);
                    free(fullname);
                    free(serverpath);
                }
                else
                {

                    wd_DEBUG("upload %s failed\n",fullname);

                    if(re_cmd)
                        free(fullname_t);
                    free(path);
                    free(fullname);
                    free(serverpath);
                    return status;
                }
            }
        }
        else
        {
            if(re_cmd)
                free(fullname_t);
            free(path);
            free(fullname);
            free(serverpath);
            return -1;
        }
#if 0
        if(mtime_1 == mtime_2 || mtime_2 == -1 ||asus_cfg.prule[index]->rule == 2)
        {
            /*third param ture,ture=>if server file has exit ,will be overwrite*/
            status=upload_file(fullname,serverpath,1,index);
            if(status == 0)
            {
                time_t mtime=cJSON_printf(dofile(Con(TMP_R,upload_chunk_commit.txt),"modified");
                ChangeFile_modtime(fullname,mtime);
                free(fullname);
                free(serverpath);
            }
            else
            {

                wd_DEBUG("upload %s failed\n",fullname);
                //write_system_log("error","uploadfile fail");

                free(path);
                free(fullname);
                free(serverpath);
                return status;
            }
        }
        else if(mtime_1 !=mtime_2 && mtime_2 != -1 && asus_cfg.prule[index]->rule ==0)
        {
            /*third param false,false=>if server file has exit ,will not be overwrite*/
            status=upload_file(fullname,serverpath,0,index);
            if(status == 0)
            {
                char *newlocalname;
                time_t mtime=cJSON_printf(dofile(Con(TMP_R,upload_chunk_commit.txt),"modified");
                newlocalname=change_local_same_file(fullname,index);
                if(newlocalname)
                    ChangeFile_modtime(newlocalname,mtime);
                free(newlocalname);
                free(fullname);
                free(serverpath);
            }
            else
            {

                wd_DEBUG("upload %s failed\n",fullname);
                //write_system_log("error","uploadfile fail");

                free(path);
                free(fullname);
                free(serverpath);
                return status;
            }
        }
        else if(mtime_2 == -2) //when the network is not conncet ,the curl resporen -2;
        {
            free(path);
            free(fullname);
            free(serverpath);
            return -1;
        }
#endif
    }
    else if(strcmp(cmd_name, "delete") == 0  || strcmp(cmd_name, "remove") == 0)
    {
        action_item *item;
        item = get_action_item_access("upload",fullname,g_pSyncList[index]->access_failed_list,index);
        if(item != NULL)
        {
            del_action_item("upload",item->path,g_pSyncList[index]->access_failed_list);
        }

        sprintf(fullname,"%s/%s",path,filename);
        char *serverpath=localpath_to_serverpath(fullname,index);

        char *serverpath_1=localpath_to_serverpath(path,index);
        if(is_server_exist(serverpath_1,serverpath,index) == 0)
        {
            my_free(serverpath_1);
            my_free(serverpath);
            free(fullname);
            free(path);
            return 0;
        }

        status=api_delete(serverpath,index);

        if(status != 0)
        {

            wd_DEBUG("delete failed\n");

            free(path);
            free(fullname);
            free(serverpath);
            //free(fullname);
            return status;
        }
        free(fullname);
        free(serverpath);
    }
    else if(strncmp(cmd_name, "move",strlen("move")) == 0 || strncmp(cmd_name, "rename",strlen("rename")) == 0)
    {
        if(strncmp(cmd_name, "move",strlen("move")) == 0)
        {
            mv_newpath = my_str_malloc((size_t)(strlen(path)+strlen(oldname)+2));
            mv_oldpath = my_str_malloc((size_t)(strlen(oldpath)+strlen(oldname)+2));
            sprintf(mv_newpath,"%s/%s",path,oldname);
            sprintf(mv_oldpath,"%s/%s",oldpath,oldname);
            free(oldpath);
            if(re_cmd)
            {
                fullname_t = (char *)malloc(sizeof(char)*(strlen(re_cmd)+strlen(oldname)+1));
                memset(fullname_t,'\0',strlen(re_cmd)+strlen(oldname)+1);
                sprintf(fullname_t,"%s%s",re_cmd,oldname);
            }
        }
        else
        {
            mv_newpath = my_str_malloc((size_t)(strlen(path)+strlen(newname)+2));
            mv_oldpath = my_str_malloc((size_t)(strlen(path)+strlen(oldname)+2));
            sprintf(mv_newpath,"%s/%s",path,newname);
            sprintf(mv_oldpath,"%s/%s",path,oldname);
            if(re_cmd)
            {
                fullname_t = (char *)malloc(sizeof(char)*(strlen(re_cmd)+strlen(newname)+1));
                memset(fullname_t,'\0',strlen(re_cmd)+strlen(newname)+1);
                sprintf(fullname_t,"%s%s",re_cmd,newname);
            }
        }
        if(strncmp(cmd_name, "rename",strlen("rename")) == 0)
        {            
//            if(is_renamed)
//            {
                int exist=0;
                char *serverpath=localpath_to_serverpath(mv_newpath,index);
                char *serverpath_old=localpath_to_serverpath(mv_oldpath,index);
                /*if the newer name has exist in server,the server same name will be rename ,than rename the oldname to newer*/
                char *serverpath_1=localpath_to_serverpath(path,index);
                exist=is_server_exist(serverpath_1,serverpath,index);
                wd_DEBUG("exist = %d\n",exist);
                if(exist)
                {
                    char *newname;
                    newname=change_server_same_name(serverpath,index);

                    status = api_move(serverpath,newname,index,0,NULL);


                    if(status == 0)
                    {
                        char *err_msg = write_error_message("server file %s is renamed to %s",serverpath,newname);
                        write_trans_excep_log(serverpath,3,err_msg);
                        free(err_msg);
                        //write_conflict_log(serverpath,newname,index);
                        status = api_move(serverpath_old,serverpath,index,1,fullname_t);
                    }
                    free(newname);
                }
                else
                {
                    status = api_move(serverpath_old,serverpath,index,1,fullname_t);
                }
                free(serverpath);
                free(serverpath_old);
                free(serverpath_1);
                if(re_cmd)
                    free(fullname_t);
                if(status != 0)
                {

                    wd_DEBUG("move/rename failed\n");
                    //write_system_log("error","uploadfile fail");
                    free(mv_oldpath);
                    free(mv_newpath);
                    free(path);
                    return status;
                }
//            }
//            is_renamed = 0;

//            if(test_if_dir(mv_newpath))
//            {
//                action_item *item;
//                item = get_action_item_access("upload",mv_oldpath,g_pSyncList[index]->access_failed_list,index);
//                if(item != NULL)
//                {
//                    char *name_access=parse_name_from_path(item->path);
//                    char *local_access;
//                    local_access=(char *)malloc(sizeof(char)*(strlen(name_access)+strlen(mv_newpath)+2));
//                    memset(local_access,0,sizeof(local_access));
//                    sprintf(local_access,"%s/%s",mv_newpath,name_access);
//                    char *server_access=localpath_to_serverpath(local_access,index);
//                    printf("local_access : %s\n,server_access : %s\n",local_access,server_access);
//                    status=upload_file(local_access,server_access,1,index);
//                    free(local_access);
//                    free(server_access);
//                    if(status == 0 || status == LOCAL_FILE_LOST || status == SERVER_SPACE_NOT_ENOUGH)
//                    {
//                        if(status == 0)
//                        {
//                            time_t mtime=cJSON_printf(dofile(Con(TMP_R,upload_chunk_commit.txt)),"modified");
//                            ChangeFile_modtime(local_access,mtime);
//                        }
//                        del_action_item("upload",item->path,g_pSyncList[index]->access_failed_list);
//                    }
//                    else
//                    {
//                        wd_DEBUG("upload %s failed\n",fullname);
//                        free(path);
//                        free(mv_oldpath);
//                        free(mv_newpath);
//                        return status;
    //                    char info[512];
    //                    sprintf(info,"createfile%s%s%s%s",CMD_SPLIT,mv_newpath,CMD_SPLIT,name_access);
    //                    pthread_mutex_lock(&mutex_socket);
    //                    add_socket_item(info);
    //                    pthread_mutex_unlock(&mutex_socket);
//                    }
//                }
//            }
//            if(access(filename,F_OK) != 0)
//            {

//            }

//            is_renamed = 1;
        }
        else  /*action : move*/
        {
            int exist = 0;
            int old_index;
            old_index=get_path_to_index(mv_oldpath);
            /*the move file is in other rules folder or root path*/
            /*index is 0 or 2
              0=>sync
              2=>upload
            */
            if(asus_cfg.prule[old_index]->rule == 1)
            {
                del_download_only_action_item("move",mv_oldpath,g_pSyncList[old_index]->download_only_socket_head);
            }

            char *serverpath=localpath_to_serverpath(mv_newpath,index);
            char *serverpath_old=localpath_to_serverpath(mv_oldpath,index);
            char *serverpath_1=localpath_to_serverpath(path,index);
            exist=is_server_exist(serverpath_1,serverpath,index);
            if(exist)
            {
                char *newname;
                newname=change_server_same_name(serverpath,index);

                status = api_move(serverpath,newname,index,0,NULL);

                if(status == 0)
                {
                    char *err_msg = write_error_message("server file %s is renamed to %s",serverpath,newname);
                    write_trans_excep_log(serverpath,3,err_msg);
                    free(err_msg);
                    //write_conflict_log(serverpath,newname,index);
                    status = api_move(serverpath_old,serverpath,index,1,fullname_t);
                }

                free(newname);
            }
            else
            {
                status = api_move(serverpath_old,serverpath,index,1,fullname_t);
            }
            free(serverpath);
            free(serverpath_old);
            free(serverpath_1);
            if(re_cmd)
                free(fullname_t);
            if(status != 0)
            {

                wd_DEBUG("move/rename failed\n");
                //write_system_log("error","uploadfile fail");

                free(path);
                free(mv_oldpath);
                free(mv_newpath);
                return status;
            }
        }
        free(mv_oldpath);
        free(mv_newpath);

    }
    else if( strcmp(cmd_name,"dragfolder") == 0)
    {
        char info[512];
        memset(info,0,sizeof(info));
        if(re_cmd == NULL)
        {
            sprintf(fullname,"%s/%s",path,filename);
            sprintf(info,"createfolder%s%s%s%s",CMD_SPLIT,path,CMD_SPLIT,filename);
        }
        else
        {
            sprintf(fullname,"%s%s",re_cmd,filename);
            sprintf(info,"createfolder%s%s%s%s",CMD_SPLIT,re_cmd,CMD_SPLIT,filename);
        }


        pthread_mutex_lock(&mutex_socket);
        add_socket_item(info);
        pthread_mutex_unlock(&mutex_socket);
        deal_dragfolder_to_socketlist(fullname,index);
        if(re_cmd)
            free(fullname_t);
        free(fullname);
//        sprintf(fullname,"%s/%s",path,filename);
//        //printf("fullname is %s\n",fullname);
//        status=dragfolder(fullname,index);
//        free(fullname);
//        if(status != 0)
//        {

//            wd_DEBUG("dragfolder failed status = %d\n",status);

//            free(path);
//            return status;
//        }
    }
    else if(strcmp(cmd_name, "createfolder") == 0)
    {
        int exist;

        sprintf(fullname,"%s/%s",path,filename);
        char *serverpath=localpath_to_serverpath(fullname,index);
        if(re_cmd != NULL)
            sprintf(fullname_t,"%s%s",re_cmd,filename);

        if(re_cmd == NULL)
            status=api_create_folder(fullname,serverpath);
        else
            status=api_create_folder(fullname_t,serverpath);
        if(status==0)
        {
            exist=parse_create_folder(Con(TMP_R,create_folder.txt));
            if(exist)
            {
#if 0
                char *newname;
                newname=change_server_same_name(serverpath,index);

                status = api_move(serverpath,newname,index,0,NULL);

                free(newname);
                if(status ==0)
                {
                    if(re_cmd == NULL)
                        status=api_create_folder(fullname,serverpath);
                    else
                        status=api_create_folder(fullname_t,serverpath);
                }
#endif
            }
            if(re_cmd)
                free(fullname_t);
            free(serverpath);
            free(fullname);
        }
        else
        {

            wd_DEBUG("createFolder failed status = %d\n",status);

            free(path);
            return status;
        }
    }
    free(path);
    return 0;

}
//int first_dragfolder = 0 ;
//int get_local_list(char *dir)
//{
//    Local *localnode;
//    localnode=Find_Floor_Dir(dir);
//    while(localnode->folderlist != NULL)
//        get_local_list(localnode->folderlist->path);
//}
//int dragfolder_test(char *dir,int index)
//{
//    if(first_dragfolder == 0)
//    {
//        get_local_list(dir);
//    }
//}
int deal_dragfolder_to_socketlist(char *dir,int index)
{
    wd_DEBUG("dir = %s\n",dir);

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
            memset(fullname,'\0',strlen(dir)+strlen(ent->d_name)+2);

            sprintf(fullname,"%s/%s",dir,ent->d_name);
            if(test_if_dir(fullname) == 1)
            {
                sprintf(info,"createfolder%s%s%s%s",CMD_SPLIT,dir,CMD_SPLIT,ent->d_name);
                pthread_mutex_lock(&mutex_socket);
                add_socket_item(info);
                pthread_mutex_unlock(&mutex_socket);
                status = deal_dragfolder_to_socketlist(fullname,index);
            }
            else
            {
                sprintf(info,"createfile%s%s%s%s",CMD_SPLIT,dir,CMD_SPLIT,ent->d_name);
                pthread_mutex_lock(&mutex_socket);
                add_socket_item(info);
                pthread_mutex_unlock(&mutex_socket);
            }
            free(fullname);
        }
        closedir(pDir);
        return 0;
    }
}
int dragfolder_rename(char *dir,int index,time_t server_mtime)
{
    wd_DEBUG("change dir = %s mtime\n",dir);

    int status;
    struct dirent *ent = NULL;
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
            memset(fullname,'\0',strlen(dir)+strlen(ent->d_name)+2);

            sprintf(fullname,"%s/%s",dir,ent->d_name);
            if(test_if_dir(fullname) == 1)
            {

                status = dragfolder_rename(fullname,index,server_mtime);

            }
            else
            {
                status=ChangeFile_modtime(fullname,server_mtime);
            }
            free(fullname);
        }
        closedir(pDir);
        return 0;
    }
}
int dragfolder_old_dir(char *dir,int index,char *old_dir)
{

    wd_DEBUG("dir = %s\n",dir);

    int status;
    int exist;
    struct dirent *ent = NULL;
    DIR *pDir;
    pDir=opendir(dir);
    if(pDir != NULL)
    {
        //char *serverpath=localpath_to_serverpath(old_dir,index);
        status=api_create_folder(dir,old_dir);
        if(status != 0)
        {

            wd_DEBUG("Create %s failed\n",old_dir);

            //return -1;
            closedir(pDir);
            return status;
        }
        exist=parse_create_folder(Con(TMP_R,create_folder.txt));
        if(exist)
        {
            char *newname;
            newname=change_server_same_name(old_dir,index);

            status = api_move(old_dir,newname,index,0,NULL);

            free(newname);
            if(status ==0)
            {
                status=api_create_folder(dir,old_dir);
            }
            if(status != 0)
            {
                closedir(pDir);
                return status;
            }
        }
        //free(serverpath);
        while((ent=readdir(pDir)) != NULL)
        {
            if(ent->d_name[0] == '.')
                continue;
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            char *fullname;
            fullname = (char *)malloc(sizeof(char)*(strlen(dir)+strlen(ent->d_name)+2));
            memset(fullname,'\0',strlen(dir)+strlen(ent->d_name)+2);
            sprintf(fullname,"%s/%s",dir,ent->d_name);

            char *fullname_r;
            fullname_r = (char *)malloc(sizeof(char)*(strlen(old_dir)+strlen(ent->d_name)+2));
            memset(fullname_r,'\0',strlen(old_dir)+strlen(ent->d_name)+2);
            sprintf(fullname_r,"%s/%s",old_dir,ent->d_name);

            status = check_localpath_is_socket(index,dir,ent->d_name,fullname);
            if(status == 1)
            {
                wd_DEBUG("the %s is socket ,so do nothing\n",fullname);
                free(fullname);
                free(fullname_r);
                continue;
            }

            if(test_if_dir(fullname) == 1)
            {

                status = dragfolder_old_dir(fullname,index,fullname_r);
                if(status != 0)
                {

                    wd_DEBUG("CreateFolder %s failed\n",fullname);

                    free(fullname);
                    free(fullname_r);
                    closedir(pDir);
                    return status;
                }
            }
            else
            {
                char *serverpath_1=localpath_to_serverpath(fullname_r,index);
                status=upload_file(fullname,serverpath_1,1,index);
                if(status == 0)
                {
                    cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);
                    ChangeFile_modtime(fullname,mtime);

                }
                else if(status == SERVER_SPACE_NOT_ENOUGH)
                {

                    wd_DEBUG("upload %s failed,server space is not enough!\n",fullname);

                }
                else if(status == LOCAL_FILE_LOST)
                {

                    wd_DEBUG("upload %s failed,local file lost!\n",fullname);

                }
                else
                {

                    wd_DEBUG("upload %s failed\n",fullname);

                    free(serverpath_1);
                    free(fullname);
                    free(fullname_r);
                    return status;
                }
            }
            free(fullname);
            free(fullname_r);
        }
        closedir(pDir);
        return 0;
    }
}
int dragfolder(char *dir,int index)
{

    wd_DEBUG("dir = %s\n",dir);

    int status;
    int exist;
    struct dirent *ent = NULL;
    DIR *pDir;
    pDir=opendir(dir);
    if(pDir != NULL)
    {
        char *serverpath=localpath_to_serverpath(dir,index);
        status=api_create_folder(dir,serverpath);
        if(status != 0)
        {

            wd_DEBUG("Create %s failed\n",serverpath);

            //return -1;
            closedir(pDir);
            return status;
        }
        exist=parse_create_folder(Con(TMP_R,create_folder.txt));
        if(exist)
        {
            char *newname;
            newname=change_server_same_name(serverpath,index);

            status = api_move(serverpath,newname,index,0,NULL);

            free(newname);
            if(status ==0)
            {
                status=api_create_folder(dir,serverpath);
            }
            if(status != 0)
            {
                closedir(pDir);
                return status;
            }
        }
        free(serverpath);
        while((ent=readdir(pDir)) != NULL)
        {
            if(ent->d_name[0] == '.')
                continue;
            if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                continue;
            char *fullname;
            fullname = (char *)malloc(sizeof(char)*(strlen(dir)+strlen(ent->d_name)+2));
            memset(fullname,'\0',strlen(dir)+strlen(ent->d_name)+2);

            sprintf(fullname,"%s/%s",dir,ent->d_name);

            /*
             fix :below question
             a.rename local path A,and server is not exist;
             b.create a file 'a' in A/;
             final-->the server will exist 'a' and 'a(1)';
            */
            status = check_localpath_is_socket(index,dir,ent->d_name,fullname);
            if(status == 1)
            {
                wd_DEBUG("the %s is socket ,so do nothing\n",fullname);
                free(fullname);
                continue;
            }
            if(test_if_dir(fullname) == 1)
            {

                status = dragfolder(fullname,index);
                if(status != 0)
                {

                    wd_DEBUG("CreateFolder %s failed\n",fullname);

                    free(fullname);
                    closedir(pDir);
                    return status;
                }
            }
            else
            {
                char *serverpath_1=localpath_to_serverpath(fullname,index);
                status=upload_file(fullname,serverpath_1,1,index);
                if(status == 0)
                {
                    cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
                    time_t mtime=cJSON_printf(json,"modified");
                    cJSON_Delete(json);
                    ChangeFile_modtime(fullname,mtime);

                }
                else if(status == SERVER_SPACE_NOT_ENOUGH)
                {

                    wd_DEBUG("upload %s failed,server space is not enough!\n",fullname);

                }
                else if(status == LOCAL_FILE_LOST)
                {

                    wd_DEBUG("upload %s failed,local file lost!\n",fullname);

                }
                else
                {

                    wd_DEBUG("upload %s failed\n",fullname);

                    free(serverpath_1);
                    free(fullname);
                    return status;
                }
            }
            free(fullname);
        }
        closedir(pDir);
        return 0;
    }
}

int parse_create_folder(char *filename)
{
    FILE *fp;
    fp=fopen(filename,"r");
    if(fp == NULL)
    {
        wd_DEBUG("filename %s is not exit\n",filename);
        return 0;
    }
    char tmp[256]="\0";
    fgets(tmp,256,fp);
    fclose(fp);
#if 0
    if(strstr(tmp,"error") != NULL)
    {
        if(strstr(tmp,"already exists.") != NULL)
        {
            return 1;
        }
    }
#endif
    if(strstr(tmp,"403") != NULL)
    {
        return 1;
    }
    return 0;

}

int get_path_to_index(char *path)
{
    int i;
    char *root_path = NULL;
    char *temp = NULL;
    root_path = my_str_malloc(512);

    temp = my_nstrchr('/',path,4);
    if(temp == NULL)
    {
        sprintf(root_path,"%s",path);
    }
    else
    {
        snprintf(root_path,strlen(path)-strlen(temp)+1,"%s",path);
    }

    for(i=0;i<asus_cfg.dir_number;i++)
    {
        if(!strcmp(root_path,asus_cfg.prule[i]->base_path))
            break;
    }

    wd_DEBUG("get_path_to_index root_path = %s\n",root_path);

    free(root_path);

    return i;
}

void sig_handler (int signum)
{
    //Getmysyncfolder *gf;
    //printf("sig_handler !\n");
    //sleep(5);

    wd_DEBUG("signal is %d\n",signum);
    switch (signum)
    {
#if TOKENFILE
    case SIGTERM:case SIGUSR2:
        {
            int mountflag = 0;
            if(signum == SIGUSR2)
            {

                wd_DEBUG("signal is SIGUSR2\n");

                FILE *fp;
                fp = fopen("/proc/mounts","r");
                char a[10240];
                memset(a,'\0',sizeof(a));
                fread(a,10240,1,fp);
                fclose(fp);
                if(strstr(a,"/dev/sd"))
                {
                    mountflag = 1;
                }

            }
            if(signum == SIGTERM || mountflag == 0)
            {
                stop_progress = 1;
                exit_loop = 1;

                /*updata /tmp/dropbox.conf*/
                /*
                system("sh /tmp/dropbox_get_nvram");
                sleep(2);
                if(create_webdav_conf_file(&asus_cfg_stop) == -1)
                {
                    wd_DEBUG("create_webdav_conf_file fail\n");
                    return;
                }
                */

#ifndef NVRAM_
                char cmd_p[128] = {0};
                sprintf(cmd_p,"sh %s",DropBox_Get_Nvram);
                system(cmd_p);
                //system("sh /tmp/dropbox_get_nvram");
                sleep(2);
                if(create_webdav_conf_file(&asus_cfg_stop) == -1)
                {
                    wd_DEBUG("create_dropbox_conf_file fail\n");
                    return;
                }

#else
                if(convert_nvram_to_file_mutidir(CONFIG_PATH,&asus_cfg_stop) == -1)
                {
                    wd_DEBUG("convert_nvram_to_file fail\n");
                    //nvram_set(NVRAM_USBINFO,"");
                    //nvram_commit();
                    write_to_nvram("","db_tokenfile");
                    return;
                }
#endif
                if(asus_cfg_stop.dir_number == 0)
                {
                    char *filename;
                    filename = my_str_malloc(strlen(asus_cfg.prule[0]->base_path)+20+1);
                    sprintf(filename,"%s/.dropbox_tokenfile",asus_cfg.prule[0]->base_path);
                    remove(filename);
                    free(filename);

                    remove(g_pSyncList[0]->conflict_file);
                    //write_to_wd_tokenfile("");

#ifdef NVRAM_
                    write_to_nvram("","db_tokenfile");
#else
                    write_to_wd_tokenfile("");
#endif

                }
                else
                {
                    if(asus_cfg_stop.dir_number != asus_cfg.dir_number)
                    {
                        parse_config_mutidir(CONFIG_PATH,&asus_cfg_stop);

                        rewrite_tokenfile_and_nv();
                    }
                }
                //pthread_mutex_unlock(&mutex);
                sighandler_finished = 1;
                pthread_cond_signal(&cond);
                pthread_cond_signal(&cond_socket);
                pthread_cond_signal(&cond_log);
            }
            else
            {
                disk_change = 1;
                sighandler_finished = 1;
            }
            break;
        }
#else
    case SIGTERM:
        {
            wd_DEBUG("signal is SIGTERM\n");
            stop_progress = 1;
            exit_loop = 1;

            //sighandler_finished = 1;
            pthread_cond_signal(&cond);
            pthread_cond_signal(&cond_socket);
            pthread_cond_signal(&cond_log);

            break;
        }
#endif
    case SIGPIPE:  // delete user
        wd_DEBUG("signal is SIGPIPE\n");
        signal(SIGPIPE, SIG_IGN);
        break;

    default:
        break;
    }
}

void* sigmgr_thread(){
    sigset_t   waitset;
    //siginfo_t  info;
    int        sig;
    int        rc;
    pthread_t  ppid = pthread_self();

    pthread_detach(ppid);

    sigemptyset(&waitset);
    sigaddset(&waitset,SIGUSR1);
    sigaddset(&waitset,SIGUSR2);
    sigaddset(&waitset,SIGTERM);
    sigaddset(&waitset,SIGPIPE);

    while (1)  {
        rc = sigwait(&waitset, &sig);
        if (rc != -1) {

            wd_DEBUG("sigwait() fetch the signal - %d\n", sig);

            sig_handler(sig);
        } else {
            wd_DEBUG("sigwaitinfo() returned err: %d; %s\n", errno, strerror(errno));
        }
    }
}
void stop_process_clean_up(){
#if TOKENFILE
    unlink("/tmp/notify/usb/dropbox_client");
#endif
    pthread_cond_destroy(&cond);
    pthread_cond_destroy(&cond_socket);
    pthread_cond_destroy(&cond_log);

    //free_disk_struc(&follow_disk_info_start);
    //free_disk_struc(&config_disk_info_start);

    if(!access_token_expired)
        unlink(general_log);
}
char *change_local_same_file(char *oldname,int index)
{
    cJSON *json;
    char *newname;
    if (access(Con(TMP_R,upload_chunk_commit.txt),R_OK) ==0)
    {
        json=dofile(Con(TMP_R,upload_chunk_commit.txt));
        if(json)
        {
            char *localpath;
            newname=cJSON_parse_name(json);
            localpath=serverpath_to_localpath(newname,index);
            if(strcmp(newname,oldname)!=0)
            {
#ifndef TEST
                add_action_item("rename",localpath,g_pSyncList[index]->server_action_list);
                rename(oldname,localpath);

                char *err_msg = write_error_message("server has exist %s file ,rename local %s to %s",oldname,oldname,localpath);
                write_trans_excep_log(oldname,3,err_msg);
                free(err_msg);

                //write_conflict_log(oldname,newname,index);
                //free(localpath);
#endif
                cJSON_Delete(json);
                return localpath;
            }
            else
            {
                cJSON_Delete(json);
                return localpath;
            }
        }
        else
        {
            /*the file contents is error*/
            return NULL;
        }
    }


}

void check_tokenfile()
{
#if TOKENFILE
        if(get_tokenfile_info()==-1)
        {
            wd_DEBUG("\nget_tokenfile_info failed\n");
            exit(-1);
        }
        check_config_path(1);
#endif
}
int check_link_internet()
{
    struct timeval now;
    struct timespec outtime;
    int link_flag = 0;
    int i;
#if defined NVRAM_
    while(!link_flag && !exit_loop)
    {
#ifndef USE_TCAPI
        char *link_internet = strdup(nvram_safe_get("link_internet"));
#else
        char tmp[MAXLEN_TCAPI_MSG] = {0};
        tcapi_get(WANDUCK, "link_internet", tmp);
        char *link_internet = my_str_malloc(strlen(tmp)+1);
        sprintf(link_internet,"%s",tmp);
#endif
        link_flag = atoi(link_internet);
        if(!link_flag)
        {
            for(i=0;i<asus_cfg.dir_number;i++)
            {
                write_log(S_ERROR,"Network Connection Failed","",i);
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
            //sleep(20);
        free(link_internet);
    }
#else

    do{
        char cmd_p[128] ={0};
        sprintf(cmd_p,"sh %s",DropBox_Get_Nvram_Link);
        system(cmd_p);
        sleep(2);

        char nv[64] = {0};
        FILE *fp;
        fp = fopen(TMP_NVRAM_VL,"r");
        fgets(nv,sizeof(nv),fp);
        fclose(fp);

        link_flag = atoi(nv);
        if(!link_flag)
        {
            for(i=0;i<asus_cfg.dir_number;i++)
            {
                write_log(S_ERROR,"Network Connection Failed","",i);
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
            //sleep(20);
        //free(nv);

    }while(!link_flag && !exit_loop);
#endif

    for(i=0;i<asus_cfg.dir_number;i++)
    {
        write_log(S_SYNC,"","",i);
    }
}
#ifdef OAuth1
int do_auth()
{
    int curl_res=0;
    int auth_ok=0;
    int error_time = 0;
    int have_error_log=0;
    int i;
    do
    {
        wd_DEBUG("##########begin auth############\n");
//        if(error_time > 5 && have_error_log != 1)
//        {
//            if(asus_cfg.dir_number > 0)
//                write_log(S_ERROR,"Network Connection Failed","",0);
//            have_error_log = 1;
//        }

        if(error_time > 5)
        {
            check_link_internet();
            error_time = 0;
        }
        if(exit_loop)
            return auth_ok;
        curl_res = get_request_token();

        if(curl_res == -1)
        {
            error_time++;

            //usleep(5000*1000);
            //enter_sleep_time(5);
            continue;
        }
        curl_res = open_login_page_first();
        if(curl_res == -1)
        {
            error_time++;

            //usleep(5000*1000);
            //enter_sleep_time(5);
            continue;
        }
        curl_res = login_first();
        if(curl_res == -1)
        {
            error_time++;

            //usleep(5000*1000);
            //enter_sleep_time(5);
            continue;
        }
        curl_res = login_second();
        if(curl_res == -1)
        {
            error_time++;

            //usleep(5000*1000);
            //enter_sleep_time(5);
            continue;
        }
        curl_res = login_second_submit();
        if(curl_res == -1)
        {
            error_time++;

            //usleep(5000*1000);
            //enter_sleep_time(5);
            continue;
        }

        curl_res=get_access_token();
        if(curl_res == -1 || curl_res > 0)
        {
            if(curl_res == -1)
            {
                for(i=0;i<asus_cfg.dir_number;i++)
                {
                    write_log(S_ERROR,"Authentication Failed","",i);
                }

                return auth_ok;
            }
            else
                error_time++;
            //usleep(5000*1000);
            //enter_sleep_time(5);
            continue;
        }
        auth_ok=1;
        //}while(0);
    }while(auth_ok != 1 && exit_loop != 1);
    wd_DEBUG("##########auth over############\n");

    return auth_ok;
}
#endif
int f_exists(const char *path)	// note: anything but a directory
{
        struct stat st;
        return (stat(path, &st) == 0) && (!S_ISDIR(st.st_mode));
}
void clean_sys_data()
{
    if(f_exists("/tmp/smartsync/.logs/dropbox"))
        unlink("/tmp/smartsync/.logs/dropbox");
    //my_mkdir("/tmp/smartsync_app");
    //system("touch /tmp/smartsync_app/dropbox_client_start");
}

void db_disbale_access_token()
{
    my_free(auth->oauth_token);
    my_free(auth);
}

int main(int args,char *argc[])
{
#ifndef TEST
    clean_sys_data();
    int auth_flag = 0;
    //setenv("MALLOC_TRACE","memlog",1);
    //mtrace();
    sigset_t bset,oset;
    pthread_t sig_thread;
    /*
    int res;
    res = curl_global_init(CURL_GLOBAL_DEFAULT);
*/
    curl_global_init(CURL_GLOBAL_ALL);
    sigemptyset(&bset);
    sigaddset(&bset,SIGUSR1);
    sigaddset(&bset,SIGUSR2);
    sigaddset(&bset,SIGTERM);
    sigaddset(&bset,SIGPIPE);

    if( pthread_sigmask(SIG_BLOCK,&bset,&oset) == -1)
        wd_DEBUG("!! Set pthread mask failed\n");

    if( pthread_create(&sig_thread,NULL,(void *)sigmgr_thread,NULL) != 0)
    {
        wd_DEBUG("thread creation failder\n");
        exit(-1);
    }
    init_globar_var();

#if TOKENFILE
    write_notify_file(NOTIFY_PATH,SIGUSR2);
#ifndef NVRAM_
     my_mkdir("/opt/etc/.smartsync");
#ifndef WIN_32
    write_get_nvram_script_va("link_internet");
    write_get_nvram_script();
    char cmd_p[128] ={0};
    sprintf(cmd_p,"sh %s",DropBox_Get_Nvram);
    //system("sh /tmp/dropbox_get_nvram");
    system(cmd_p);
    sleep(2);
#endif
#else
    create_shell_file();
#endif
#endif
    //signal(SIGPIPE,sig_handler);
    //signal(SIGTERM,sig_handler);

    //while(1)
    //sleep(2);


    auth=(Auth *)malloc(sizeof(Auth));
    memset(auth,0,sizeof(auth));


    read_config();
#ifdef OAuth1
    auth_flag = do_auth();
#else
    auth_flag = 1;
    auth->oauth_token = my_str_malloc(strlen(asus_cfg.pwd)+1);
    sprintf(auth->oauth_token,"%s",asus_cfg.pwd);
#endif

    if(auth_flag)
    {
        check_tokenfile();

        if(no_config == 0)
        {
            run();
        }

        pthread_join(sig_thread,NULL);
        stop_process_clean_up();
    }
    db_disbale_access_token();
    curl_global_cleanup();

    //detect_client();
////#ifdef NVRAM_
//    if(!detect_process("asuswebstorage")&&!detect_process("webdav_client")&&!detect_process("ftpclient"))
//    {
//        system("killall  -9 inotify &");
//        //DEBUG("webdav kill inotify\n");
//    }
//    else
//    {
//        //DEBUG("webdav did not kill inotify\n");
//    }
////#endif
#else
    /*
    //setenv("MALLOC_TRACE","memlog",1);
    //mtrace()
    info=(Info *)malloc(sizeof(Info));
    memset(info,0,sizeof(info));
    auth=(Auth *)malloc(sizeof(Auth));
    memset(auth,0,sizeof(auth));
    strcpy(info->usr,"m15062346679@163.com");
    strcpy(info->pwd,"123abc,./");
    */
    //printf("%s\n",get_confilicted_name_case("/tmp/mnt/ASD/new_inotify/cookie_login(case-conflict).txt",0));
    //printf("%s\n",get_confilicted_name_first("/tmp/mnt/ASD/new_inotify/cookie_login.txt",0));
    //is_server_enough("/111");
    //get_upload_id();
    /*
    get_request_token();
    open_login_page_first();
    login_first();
    login_second();
    get_access_token();
    */
    //cgi_init();
    //api_accout_info();
    //init_globar_var();
    //read_config();
    //create_sync_list();
    //api_metadata_test("/Photo");
    //api_metadata_test("/main");
    //api_move("/11.txt","/fb.txt",0,0,NULL);
    //api_download();
    //api_upload_put("/tmp/mnt/ASD/My410Sync/Music/ssss",oauth_url_escape("/My410Sync/Music/ssss"),0,0);
    //api_upload_post();//failing
    // ChangeFile_modtime("/tmp/mnt/ASD/lighttpd_0.0.0.1_mipsel.ipk",112);
    //api_upload_put_test("/tmp/mnt/ASD/aicloud_1.0.0.4_mipsel.ipk","/aicloud_1.0.0.4_mipsel.ipk",1);
    //is_server_enough("/tmp/mnt/ASD/GPL_aicloud.0.0.2.tar.gz");
    upload_file("/tmp/mnt/ASD/My410Sync/aicloud_1.0.0.10-0-gd8a47cb_mipsel.ipk",oauth_url_escape("/aicloud_1.0.0.10-0-gd8a47cb_mipsel.ipk"),0,0);
    //change_local_same_file("111",1);
    //printf("name=%s\n",oauth_url_escape("111%20aaa"));
    //api_getmtime("/111/51515151",dofile);
    //get_upload_id();
    //api_upload_chunk_commit("IuKV3xLGtsaW3zDc9ax3Og","aicloud_1.0.0.5_mipsel.ipk");
    //api_delete("/fb.txt",0);
    //api_create_folder("/tmp/mnt/ASD/music/","/music");
    //parse_create_folder("/tmp/dropbox/create_folder_header.txt");

    /*
    int i;
    for(i=0;i<asus_cfg.dir_number;i++)
    {
        int status;
        g_pSyncList[i]->ServerRootNode = create_server_treeroot();
        status = browse_to_tree(asus_cfg.prule[i]->rooturl,g_pSyncList[i]->ServerRootNode);

        SearchServerTree(g_pSyncList[i]->ServerRootNode);
        free_server_tree(g_pSyncList[i]->ServerRootNode);
    }
*/
#endif
    /*
    int status;
    ServerRootNode = create_server_treeroot();
    status = browse_to_tree(" ",ServerRootNode);
    SearchServerTree(ServerRootNode);
    free_server_tree(ServerRootNode);
    */
    /*
    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *header;
    static const char buf[]="Expect:";
    header=makeAuthorize();
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
            if(strstr(argc[1],"1")!=NULL)
                curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/oauth/request_token");
            else if((strstr(argc[1],"2")!=NULL))
                curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/oauth/access_token");
            else if((strstr(argc[1],"3")!=NULL))
                curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/account/info");
            curl_easy_setopt(curl,CURLOPT_VERBOSE,1);
            curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
            //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
            //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
            if(strstr(argc[1],"1")!=NULL)
                fp=fopen("delete_1.txt","w");
            else if((strstr(argc[1],"2")!=NULL))
                fp=fopen("delete_2.txt","w");
            else if((strstr(argc[1],"3")!=NULL))
                fp=fopen("delete_3.txt","w");
            curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
            res=curl_easy_perform(curl);

            curl_easy_cleanup(curl);
            fclose(fp);
            if(res!=0){
                printf("delete [%d] failed!\n",res);
                return -1;
            }
    }
    free(header);
    */
}
#ifdef OAuth1
int get_request_token(){

    CURL *curl;
    CURLcode res;
    FILE *fp;
    char *header;
    static const char buf[]="Expect:";
    header=makeAuthorize(1);
    struct curl_slist *headerlist=NULL;
    curl=curl_easy_init();
    headerlist=curl_slist_append(headerlist,header);
    if(curl){
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl,CURLOPT_URL,"https://api.dropbox.com/1/oauth/request_token");
        CURL_DEBUG;
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headerlist);
        //curl_easy_setopt(curl,CURLOPT_POSTFIELDS,"");
        //curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0);
        fp=fopen(Con(TMP_R,data_1.txt),"w");
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
        res=curl_easy_perform(curl);

        curl_easy_cleanup(curl);
        fclose(fp);
        curl_slist_free_all(headerlist);
        if(res!=0){
            free(header);
            wd_DEBUG("get_request_token [%d] failed!\n",res);
            return -1;
        }
    }
    free(header);

    if(parse(Con(TMP_R,data_1.txt),1) == -1)
        return -1;
    return 0;
}

int cgi_init(char *query)
        //int cgi_init()
{
    //char query[]="oauth_token_secret=b1wa7jwkjc4u1ji&oauth_token=34zjc44boelj1zh&uid=150377145";
    int len, nel;
    char *q, *name, *value;
    /* Parse into individualassignments */
    q = query;
    len = strlen(query);
    nel = 1;
    while (strsep(&q, "&"))
        nel++;
    for (q = query; q< (query + len);) {
        value = name = q;
        /* Skip to next assignment */
        for (q += strlen(q); q < (query +len) && !*q; q++);
        /* Assign variable */
        name = strsep(&value,"=");
        if(strcmp(name,"oauth_token_secret")==0)
            strcpy(auth->oauth_token_secret,value);
        else if(strcmp(name,"oauth_token")==0)
            strcpy(auth->oauth_token,value);
        else if(strcmp(name,"uid")==0)
            auth->uid=atoi(value);

    }
    wd_DEBUG("CGI[value] :%s %s %d\n", auth->oauth_token_secret,auth->oauth_token,auth->uid);
    return 0;
}

int parse(char *filename,int flag)
{

    char *p=NULL;
    char p1[56];
    char *p3=NULL;
    char *p4=NULL;
    FILE *fp=fopen(filename,"r");
    char tmp_data[256]="\0";
    fgets(tmp_data,sizeof(tmp_data),fp);
    wd_DEBUG("%s\n",tmp_data);
    if(strstr(tmp_data,"error")==NULL && tmp_data != NULL&&strlen(tmp_data) > 0)
    {
        switch(flag)
        {
        case 1:
            p=strchr(tmp_data,'&');
            memset(p1,'\0',sizeof(p1));
            strncpy(p1,tmp_data,strlen(tmp_data)-strlen(p));
            p3=strchr(p,'=');
            p3++;
            strcpy(auth->tmp_oauth_token,p3);
            p3=NULL;
            p4=strchr(p1,'=');
            p4++;
            snprintf(auth->tmp_oauth_token_secret,strlen(p4)+1,"%s",p4);
            //strncpy(auth->tmp_oauth_token_secret,p4,strlen(p4));
            wd_DEBUG("auth->tmp_oauth_token=%s\nauth->tmp_oauth_token_secret=%s\n",auth->tmp_oauth_token,auth->tmp_oauth_token_secret);
            break;
        case 2:
            cgi_init(tmp_data);
            break;
        default:
            break;
        }
        fclose(fp);
        return 0;
    }
    else
    {
        fclose(fp);
        return -1;
    }


}
#endif
char *makeAuthorize(int flag)
{
    char *header = NULL;
    char http_basic_authentication[] = "Authorization: Bearer ";
    char URI_Query_Parameter[] = "access_token=";
    int header_len ;
    switch(flag)
    {
    case 1:
        header_len = strlen(http_basic_authentication) + strlen(auth->oauth_token) +1;
        header = my_str_malloc(header_len);
        sprintf(header,"%s%s",http_basic_authentication,auth->oauth_token);
        break;
    case 2:
        header_len = strlen(URI_Query_Parameter) + strlen(auth->oauth_token) +1;
        header = my_str_malloc(header_len);
        sprintf(header,"%s%s",URI_Query_Parameter,auth->oauth_token);
        break;
    default:
        break;
    }
    //sprintf(header,"%s%s",http_basic_authentication,auth->oauth_token);
    //sprintf(header,"%s%s","access_token=","XDwYx52HJBYAAAAAAAABcY809z2fKmv_xai8IZXzYmZKcIADF9elZ55nmeUNR_m2");
    wd_DEBUG("makeAuthorize>%s\n",header);
    return header;
}
#ifdef OAuth1
char *makeAuthorize(int flag)
{
    char *header = NULL;
    header = (char *)malloc(sizeof(char*)*1024);
    char header_signature_method[64];
    char header_timestamp[64];
    char header_nonce[64];
    char *header_signature = NULL;
    char prekey[128];
    long long int sec;

    char query_string[1024];
    char *incode_string = NULL;
    char *sha1_string = NULL;

    //snprintf(header_signature_method,64,"%s","HMAC-SHA1");
    snprintf(header_signature_method,64,"%s","PLAINTEXT");
    if(flag==1)
        snprintf(prekey,128,"%s","5vq8jog8wgpx1p0&");
    else if(flag==2)
        //snprintf(prekey,128,"%s%s","5vq8jog8wgpx1p0&","YXG59mgidir11f6b");
        snprintf(prekey,128,"%s%s","5vq8jog8wgpx1p0&",auth->tmp_oauth_token_secret);
    else if(flag==3)
    {
#ifndef TEST
        snprintf(prekey,128,"%s&%s","5vq8jog8wgpx1p0",auth->oauth_token_secret);
#else
        snprintf(prekey,128,"%s","5vq8jog8wgpx1p0&efghhkqrt68qta2");
#endif
    }
    else if(flag==4)
    {
#ifndef TEST
        snprintf(prekey,128,"%s&%s","5vq8jog8wgpx1p0",auth->oauth_token_secret);
#else
        snprintf(prekey,128,"%s","5vq8jog8wgpx1p0&efghhkqrt68qta2");
#endif
    }
    //snprintf(prekey,128,"%s&%s","5vq8jog8wgpx1p0",auth->oauth_token_secret);
    //snprintf(prekey,128,"%s","5vq8jog8wgpx1p0&efghhkqrt68qta2");
    sec = time((time_t *)NULL);
    snprintf(header_timestamp,64,"%lld",sec);
    snprintf(header_nonce,64,"%lld",sec);
    /*
    //hmac_sha1
    snprintf( query_string,1024,"oauth_consumer_key=qah4ku73k3qmigj&oauth_nonce=%s&oauth_signature_method=HMAC-SHA1&oauth_timestamp=%s&oauth_version=1.0",header_nonce,header_timestamp);
    //snprintf( query_string,1024,"POST&https://api.dropbox.com/1/oauth/request_token&oauth_consumer_key",header_nonce,header_signature_method,header_timestamp);
    incode_string = oauth_url_escape(query_string);
    if(NULL == incode_string)
    {
        //handle_error(S_URL_ESCAPE_FAIL,"makeAuthorize");
        return NULL;
    }
    char *httpsUrl="https://api.dropbox.com/1/oauth/request_token";
    char *test_key = NULL;
    test_key=(char *)malloc(sizeof(char *)*(strlen(query_string)+4+4+strlen(httpsUrl)));
    memset(test_key,'\0',sizeof(test_key));
    sprintf(test_key,"POST&%s&%s",httpsUrl,incode_string);
    sha1_string = oauth_sign_hmac_sha1(test_key,prekey);

    if(NULL == sha1_string)
    {
        //handle_error(S_SHA1_FAIL,"makeAuthorize");
        //my_free(incode_string);
        return NULL;
    }

    header_signature = oauth_url_escape(sha1_string);
    if(NULL == header_signature)
    {
       // handle_error(S_URL_ESCAPE_FAIL,"makeAuthorize");
       // my_free(incode_string);
        //my_free(sha1_string);
        return NULL;
    }
*/
    header_signature=oauth_sign_plaintext(NULL,prekey);
    //snprintf(header,1024,"Authorization:signature_method=\"%s\",timestamp=\"%s\",nonce=%s,signature=\"%s\"",header_signature_method,header_timestamp,header_nonce,header_signature);
    if(flag==1)
        snprintf(header,1024,"Authorization:OAuth oauth_consumer_key=\"%s\",oauth_signature_method=\"%s\",oauth_signature=\"%s\",oauth_timestamp=\"%s\",oauth_nonce=\"%s\",oauth_version=\"1.0\""
                 ,"qah4ku73k3qmigj",header_signature_method,header_signature,header_timestamp,header_nonce);
    else if(flag==2)
//        snprintf(header,1024,"Authorization:OAuth oauth_consumer_key=\"%s\",oauth_token=\"%s\",oauth_signature_method=\"%s\",oauth_signature=\"%s\",oauth_timestamp=\"%s\",oauth_nonce=\"%s\",oauth_version=\"1.0\""
 //                ,"qah4ku73k3qmigj","AIZpfQv7qYwuDotM",header_signature_method,header_signature,header_timestamp,header_nonce);
    snprintf(header,1024,"Authorization:OAuth oauth_consumer_key=\"%s\",oauth_token=\"%s\",oauth_signature_method=\"%s\",oauth_signature=\"%s\",oauth_timestamp=\"%s\",oauth_nonce=\"%s\",oauth_version=\"1.0\""
             ,"qah4ku73k3qmigj",auth->tmp_oauth_token,header_signature_method,header_signature,header_timestamp,header_nonce);
    else if(flag==3)
    {
#ifndef TEST
        snprintf(header,1024,"Authorization:OAuth oauth_consumer_key=\"%s\",oauth_token=\"%s\",oauth_signature_method=\"%s\",oauth_signature=\"%s\",oauth_timestamp=\"%s\",oauth_nonce=\"%s\",oauth_version=\"1.0\""
                 ,"qah4ku73k3qmigj",auth->oauth_token,header_signature_method,header_signature,header_timestamp,header_nonce);
#else
        snprintf(header,1024,"Authorization:OAuth oauth_consumer_key=\"%s\",oauth_token=\"%s\",oauth_signature_method=\"%s\",oauth_signature=\"%s\",oauth_timestamp=\"%s\",oauth_nonce=\"%s\",oauth_version=\"1.0\""
                 ,"qah4ku73k3qmigj","8m9knhps2zgaon9",header_signature_method,header_signature,header_timestamp,header_nonce);
#endif
    }
    else if(flag==4)
    {
#ifndef TEST
        snprintf(header,1024,"oauth_consumer_key=%s&oauth_token=%s&oauth_signature_method=%s&oauth_signature=%s&oauth_timestamp=%s&oauth_nonce=%s&oauth_version=1.0"
                 ,"qah4ku73k3qmigj",auth->oauth_token,header_signature_method,header_signature,header_timestamp,header_nonce);
#else
        snprintf(header,1024,"oauth_consumer_key=%s&oauth_token=%s&oauth_signature_method=%s&oauth_signature=%s&oauth_timestamp=%s&oauth_nonce=%s&oauth_version=1.0"
                 ,"qah4ku73k3qmigj","8m9knhps2zgaon9",header_signature_method,header_signature,header_timestamp,header_nonce);
#endif
    }
    //snprintf(header,1024,"oauth_consumer_key=%s&oauth_token=%s&oauth_signature_method=%s&oauth_signature=%s&oauth_timestamp=%s&oauth_nonce=%s&oauth_version=1.0"
    //,"qah4ku73k3qmigj",auth->oauth_token,header_signature_method,header_signature,header_timestamp,header_nonce);
    //snprintf(header,1024,"oauth_consumer_key=%s&oauth_token=%s&oauth_signature_method=%s&oauth_signature=%s&oauth_timestamp=%s&oauth_nonce=%s&oauth_version=1.0"
    //,"qah4ku73k3qmigj","8m9knhps2zgaon9",header_signature_method,header_signature,header_timestamp,header_nonce);

    // my_free(incode_string);
    //my_free(sha1_string);
    free(header_signature);
    //free(test_key);
    return header;
}
#endif
Browse *browseFolder(char *URL){
    //printf("browseFolder URL = %s\n",URL);
    int status;
    int i=0;

    Browse *browse = getb(Browse);
    if( NULL == browse )
    {
        wd_DEBUG("create memery error\n");
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

    status = api_metadata(URL,dofile);
    if(status != 0)
    {
        free_CloudFile_item(TreeFolderList);
        free_CloudFile_item(TreeFileList);
        TreeFolderList = NULL;
        TreeFileList = NULL;
        free(browse);
        //printf("get Cloud Info ERROR! \n");
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

queue_t queue_create ()
{
    queue_t q;
    q = malloc (sizeof (struct queue_struct));
    if (q == NULL)
        exit (-1);

    q->head = q->tail = NULL;
    return q;
}
void queue_enqueue (queue_entry_t d, queue_t q)
{
    d->next_ptr = NULL;
    if (q->tail)
    {
        q->tail->next_ptr = d;
        q->tail = d;
    }
    else
    {
        q->head = q->tail = d;
    }
}

queue_entry_t  queue_dequeue_t (queue_t q)
{
    queue_entry_t first = q->head;

    if (first)
    {
        if(first->re_cmd == NULL)
        {
            q->head = first->next_ptr;
            if (q->head == NULL)
            {
                q->tail = NULL;
            }
            first->next_ptr = NULL;
        }
        else
        {
            if(access(first->re_cmd,0) == 0)
            {
                q->head = first->next_ptr;
                if (q->head == NULL)
                {
                    q->tail = NULL;
                }
                first->next_ptr = NULL;
            }
            else
            {
                if(first->is_first == 0)
                {
                    first->is_first++;
                    return NULL;
                }
                else
                {
                    q->head = first->next_ptr;
                    if (q->head == NULL)
                    {
                        q->tail = NULL;
                    }
                    first->next_ptr = NULL;
                }
            }
        }

    }
    return first;
}
queue_entry_t  queue_dequeue (queue_t q)
{
    queue_entry_t first = q->head;

    if (first)
    {
        q->head = first->next_ptr;
        if (q->head == NULL)
        {
            q->tail = NULL;
        }
        first->next_ptr = NULL;
    }
    return first;
}
void queue_destroy (queue_t q)
{
    if (q != NULL)
    {
        while (q->head != NULL)
        {
            queue_entry_t next = q->head;
            q->head = next->next_ptr;
            next->next_ptr = NULL;

#if MEM_POOL_ENABLE
            mem_free(next->cmd_name);
            mem_free(next->re_cmd);
            mem_free (next);
#else
            if(next->re_cmd)
                free(next->re_cmd);
            free(next->cmd_name);
            free (next);
#endif
        }
        q->head = q->tail = NULL;
        free (q);
    }
}
action_item *get_action_item_access(const char *action,const char *path,action_item *head,int index){

    action_item *p;
    p = head->next;

    while(p != NULL)
    {
        if(asus_cfg.prule[index]->rule == 1)
        {

        }
        else
        {
            //char *path_old=strrchr(path,'/')
            if(!strcmp(p->action,action) && !strncmp(p->path,path,strlen(path)))
            {
                return p;
            }
        }
        p = p->next;
    }
    wd_DEBUG("can not find action item_1\n");
    return NULL;
}
action_item *check_action_item(const char *action,const char *oldpath,action_item *head,int index,const char *newpath)
{
    action_item *p;
    p = head->next;

    while(p != NULL)
    {
        char *pTemp_t=(char *)malloc(sizeof(char)*(strlen(p->path)+1+1));
        char *oldpath_t=(char *)malloc(sizeof(char)*(strlen(oldpath)+1+1));
        memset(pTemp_t,'\0',strlen(p->path)+1+1);
        memset(oldpath_t,'\0',strlen(oldpath)+1+1);
        sprintf(pTemp_t,"%s/",p->path);
        sprintf(oldpath_t,"%s/",oldpath);
        char *p_t = NULL;

        if(!strcmp(p->action,action) && (p_t=strstr(pTemp_t,oldpath_t)) != NULL)
        {
            if(strlen(oldpath_t)<strlen(pTemp_t))
            {
                p_t+=strlen(oldpath);
                char *p_tt=(char *)malloc(sizeof(char)*(strlen(p_t)+1));
                memset(p_tt,'\0',strlen(p_t)+1);
                sprintf(p_tt,"%s",p_t);
                free(p->path);
                p->path = (char *)malloc(sizeof(char) * (strlen(newpath)+strlen(p_tt) + 1));
                memset(p->path,'\0',strlen(newpath)+strlen(p_tt) + 1);
                sprintf(p->path,"%s%s",newpath,p_tt);
                free(p_tt);
            }
            else
            {
                free(p->path);
                p->path = (char *)malloc(sizeof(char) * (strlen(newpath) + 1));
                sprintf(p->path,"%s",newpath);
            }
            //wd_DEBUG("p->action:%s\n p->path:%s\n",p->action,p->path);
        }

        free(pTemp_t);
        free(oldpath_t);
        p = p->next;
    }
    return NULL;
}
action_item *get_action_item(const char *action,const char *path,action_item *head,int index){

    action_item *p;
    p = head->next;

    while(p != NULL)
    {
        if(asus_cfg.prule[index]->rule == 1)
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
    wd_DEBUG("can not find action item_2\n");
    return NULL;
}
int del_action_item(const char *action,const char *path,action_item *head){

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
    wd_DEBUG("can not find action item_3\n");
    return 1;
}
int add_action_item(const char *action,const char *path,action_item *head){

    wd_DEBUG("add_action_item,action = %s,path = %s\n",action,path);

    action_item *p1,*p2;

    p1 = head;

    p2 = (action_item *)malloc(sizeof(action_item));
    memset(p2,'\0',sizeof(action_item));
    p2->action = (char *)malloc(sizeof(char)*(strlen(action)+1));
    p2->path = (char *)malloc(sizeof(char)*(strlen(path)+1));
    memset(p2->action,'\0',strlen(action)+1);
    memset(p2->path,'\0',strlen(path)+1);

    sprintf(p2->action,"%s",action);
    sprintf(p2->path,"%s",path);

    while(p1->next != NULL)
        p1 = p1->next;

    p1->next = p2;
    p2->next = NULL;

    wd_DEBUG("add action item OK!\n");

    return 0;
}
void del_download_only_action_item(const char *action,const char *path,action_item *head)
{
    //printf("del_sync_item action=%s,path=%s\n",action,path);
    if(head == NULL)
    {
        return;
    }
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
        //printf("del_download_only_sync_item  p1->name = %s\n",p1->name);
        //printf("del_download_only_sync_item  cmp_name = %s\n",cmp_name);
        if(strstr(p1_cmp_name,cmp_name) != NULL)
        {
            p2->next = p1->next;
            free(p1->action);
            free(p1->path);
            free(p1);
            //printf("del sync item ok\n");
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
    //printf("del sync item fail\n");
}
void free_action_item(action_item *head){

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
action_item *create_action_item_head(){

    action_item *head;

    head = (action_item *)malloc(sizeof(action_item));
    if(head == NULL)
    {
        wd_DEBUG("create memory error!\n");
        exit(-1);
    }
    memset(head,'\0',sizeof(action_item));
    head->next = NULL;

    return head;
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

        wd_DEBUG("opendir %s fail",dir);

        //sprintf(error_message,"opendir %s fail",dir);
        //handle_error(S_OPENDIR_FAIL,error_message);
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
        //memset(error_message,0,sizeof(error_message));
        //memset(&createfolder,0,sizeof(Createfolder));

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
int add_all_download_only_dragfolder_socket_list(const char *dir,int index)
{
    struct dirent* ent = NULL;
    char *fullname;
    int fail_flag = 0;
    //char error_message[256];

    DIR *dp = opendir(dir);

    if(dp == NULL)
    {

        wd_DEBUG("opendir %s fail",dir);

        //sprintf(error_message,"opendir %s fail",dir);
        //handle_error(S_OPENDIR_FAIL,error_message);
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
        //memset(error_message,0,sizeof(error_message));
        //memset(&createfolder,0,sizeof(Createfolder));

        sprintf(fullname,"%s/%s",dir,ent->d_name);

        if( test_if_dir(fullname) == 1)
        {
            //add_action_item("createfolder",fullname,g_pSyncList[index]->dragfolder_action_list);
            add_action_item("createfolder",fullname,g_pSyncList[index]->download_only_socket_head);
            add_all_download_only_dragfolder_socket_list(fullname,index);
        }
        else
        {
            //add_action_item("createfile",fullname,g_pSyncList[index]->dragfolder_action_list);
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
int do_unfinished(int index){
    if(exit_loop)
    {
        return 0;
    }

    wd_DEBUG("*************do_unfinished*****************\n");

    action_item *p,*p1;
    p = g_pSyncList[index]->unfinished_list->next;
    int ret;

    while(p != NULL)
    {
        //printf("unfinished_list\n");
        //printf("p->path = %s\n",p->path);
        //printf("p->action = %s\n",p->action);
        if(!strcmp(p->action,"download"))
        {
            CloudFile *filetmp = NULL;
            filetmp = get_CloudFile_node(g_pSyncList[index]->ServerRootNode,p->path,0x2);
            //printf("filetmp->href = %s\n",filetmp->href);
            if(filetmp == NULL)   //if filetmp == NULL,it means server has deleted filetmp
            {

                wd_DEBUG("filetmp is NULL\n");

                p1 = p->next;
                del_action_item("download",p->path,g_pSyncList[index]->unfinished_list);
                p = p1;
                continue;
            }
            char *localpath;
            localpath = serverpath_to_localpath(filetmp->href,index);

            wd_DEBUG("localpath = %s\n",localpath);

            if(is_local_space_enough(filetmp,index))
            {
                add_action_item("createfile",localpath,g_pSyncList[index]->server_action_list);
                ret = api_download(localpath,filetmp->href,index);
                if (ret == 0)
                {
                    ChangeFile_modtime(localpath,filetmp->mtime);
                    p1 = p->next;
                    del_action_item("download",filetmp->href,g_pSyncList[index]->unfinished_list);
                    p = p1;
                }
                else
                {

                    wd_DEBUG("download %s failed",filetmp->href);

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
        /*
        else if(!strcmp(p->action,"upload"))
        {
            p1 = p->next;
            char *serverpath;
            serverpath = localpath_to_serverpath(p->path,index);
            char *path_temp;
            path_temp = my_str_malloc(strlen(p->path)+1);
            sprintf(path_temp,"%s",p->path);
            ret = upload_file(p->path,serverpath,1,index);
#ifdef DEBUG
            printf("********* uploadret = %d\n",ret);
#endif
            if(ret == 0)
            {
                //char *serverpath;
                //serverpath = localpath_to_serverpath(p->path,index);
                //printf("serverpath = %s\n",serverpath);
                time_t mtime=cJSON_printf(dofile("/tmp/upload_chunk_commit.txt"),"modified");
                ChangeFile_modtime(p->path,mtime);
                //free(serverpath);
                //p1 = p->next;
                del_action_item("upload",p->path,g_pSyncList[index]->unfinished_list);
                del_action_item("upload",p->path,g_pSyncList[index]->up_space_not_enough_list);
                p = p1;
            }
            else if(ret == LOCAL_FILE_LOST)
            {
                //p1 = p->next;
                del_action_item("upload",p->path,g_pSyncList[index]->unfinished_list);
                p = p1;
            }
            else
            {
#ifdef DEBUG
                printf("upload %s failed",p->path);
#endif
                p = p->next;
            }
            free(serverpath);
            free(path_temp);
        }
        */
        else
        {
            p = p->next;
        }
    }

    p = g_pSyncList[index]->up_space_not_enough_list->next;
    while(p != NULL)
    {
        p1 = p->next;

        wd_DEBUG("up_space_not_enough_list\n");

        char *serverpath;
        serverpath = localpath_to_serverpath(p->path,index);
        char *path_temp;
        path_temp = my_str_malloc(strlen(p->path)+1);
        sprintf(path_temp,"%s",p->path);
        ret = upload_file(p->path,serverpath,1,index);

        wd_DEBUG("########### uploadret = %d\n",ret);

        if(ret == 0)
        {
            //char *serverpath;
            //serverpath = localpath_to_serverpath(p->path,index);
            cJSON *json = dofile(Con(TMP_R,upload_chunk_commit.txt));
            time_t mtime=cJSON_printf(json,"modified");
            cJSON_Delete(json);
            ChangeFile_modtime(p->path,mtime);
            //free(serverpath);
            //p1 = p->next;
            del_action_item("upload",p->path,g_pSyncList[index]->unfinished_list);
            del_action_item("upload",p->path,g_pSyncList[index]->up_space_not_enough_list);
            p = p1;
        }
        else if(ret == LOCAL_FILE_LOST)
        {
            //p1 = p->next;
            del_action_item("upload",p->path,g_pSyncList[index]->up_space_not_enough_list);
            p = p1;
        }
        else
        {

            wd_DEBUG("upload %s failed",p->path);

            p = p->next;
        }
        free(serverpath);
        free(path_temp);
    }
    wd_DEBUG("*************do_unfinished ok*****************\n");
    return 0;
}
int write_log(int status, char *message, char *filename,int index)
{
    if(exit_loop)
    {
        return 0;
    }
    //printf("write log status = %d\n",status);
    pthread_mutex_lock(&mutex_log);
    Log_struc log_s;
    FILE *fp;
    int mount_path_length;
    int ret;
    struct timeval now;
    struct timespec outtime;


    if(status == S_SYNC && exit_loop ==0)
    {
        ret = api_accout_info();
        if(ret==0)
        {
            cJSON *json=dofile(Con(TMP_R,data_3.txt));
            cJSON_printf(json,"quota_info");
            if(json)
                cJSON_Delete(json);
        }

    }
    long long int totalspace = server_quota;


    //mount_path_length = strlen(mount_path);
    mount_path_length = asus_cfg.prule[index]->base_path_len;

    memset(&log_s,0,LOG_SIZE);

    log_s.status = status;

    fp = fopen(general_log,"w");

    if(fp == NULL)
    {

        wd_DEBUG("open %s error\n",general_log);

        pthread_mutex_unlock(&mutex_log);
        return -1;
    }

    if(log_s.status == S_ERROR)
    {

        wd_DEBUG("******** status is ERROR *******\n");

        strcpy(log_s.error,message);
        fprintf(fp,"STATUS:%d\nERR_MSG:%s\nTOTAL_SPACE:%lld\nUSED_SPACE:%lld\nRULENUM:%d\n",
                log_s.status,log_s.error,totalspace,server_normal+server_shared,index);
    }
    else if(log_s.status == S_DOWNLOAD)
    {

        wd_DEBUG("******** status is DOWNLOAD *******\n");

        strcpy(log_s.path,filename);
        fprintf(fp,"STATUS:%d\nMOUNT_PATH:%s\nFILENAME:%s\nTOTAL_SPACE:%lld\nUSED_SPACE:%lld\nRULENUM:%d\n",
                log_s.status,asus_cfg.prule[index]->base_path,log_s.path+mount_path_length,totalspace,server_normal+server_shared,index);
    }
    else if(log_s.status == S_UPLOAD)
    {

        wd_DEBUG("******** status is UPLOAD *******\n");

        strcpy(log_s.path,filename);
        fprintf(fp,"STATUS:%d\nMOUNT_PATH:%s\nFILENAME:%s\nTOTAL_SPACE:%lld\nUSED_SPACE:%lld\nRULENUM:%d\n",
                log_s.status,asus_cfg.prule[index]->base_path,log_s.path+mount_path_length,totalspace,server_normal+server_shared,index);
    }
    else
    {
        //printf("write log status2 = %d\n",status);

        if (log_s.status == S_INITIAL)
            wd_DEBUG("******** other status is INIT *******\n");
        else
            wd_DEBUG("******** other status is SYNC *******\n");

        fprintf(fp,"STATUS:%d\nTOTAL_SPACE:%lld\nUSED_SPACE:%lld\nRULENUM:%d\n",
                log_s.status,totalspace,server_normal+server_shared,index);
        //fprintf(fp,"%d\n",log_s.status);
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
/*SIG*/
#if TOKENFILE
int write_notify_file(char *path,int signal_num)
{
    FILE *fp;
    char fullname[64];
    memset(fullname,0,sizeof(fullname));

    //my_mkdir_r(path);
    my_mkdir("/tmp/notify");
    my_mkdir("/tmp/notify/usb");
    sprintf(fullname,"%s/dropbox_client",path);
    fp = fopen(fullname,"w");
    if(NULL == fp)
    {
        wd_DEBUG("open notify %s file fail\n",fullname);
        return -1;
    }
    fprintf(fp,"%d",signal_num);
    fclose(fp);
    return 0;
}
int get_tokenfile_info()
{
    wd_DEBUG("2222\n");
    int i;
    int j = 0;
    struct mounts_info_tag *info = NULL;
    char *tokenfile;
    FILE *fp;
    char buffer[1024];
    char *p;

    /*if(initial_tokenfile_info_data(&tokenfile_info_start) == NULL)
    {
        return -1;
    }*/
    wd_DEBUG("12222\n");
    tokenfile_info = tokenfile_info_start;
    wd_DEBUG("42222\n");
    info = (struct mounts_info_tag *)malloc(sizeof(struct mounts_info_tag));
    if(info == NULL)
    {
        wd_DEBUG("obtain memory space fail\n");
        return -1;
    }
    wd_DEBUG("32222\n");
    memset(info,0,sizeof(struct mounts_info_tag));
    memset(buffer,0,1024);

    if(get_mounts_info(info) == -1)
    {
        wd_DEBUG("get mounts info fail\n");
        return -1;
    }
    wd_DEBUG("111\n");
    for(i=0;i<info->num;i++)
    {
        //printf("info->paths[%d] = %s\n",i,info->paths[i]);
        tokenfile = my_str_malloc(strlen(info->paths[i])+20+1);
        sprintf(tokenfile,"%s/.dropbox_tokenfile",info->paths[i]);
        //printf("tokenfile = %s\n",tokenfile);
        if(!access(tokenfile,F_OK))
        {
            wd_DEBUG("tokenfile is exist!\n");
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
                if(j == 0)
                {
                    if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
                    {
                        fclose(fp);
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
int get_mounts_info(struct mounts_info_tag *info)
{
    wd_DEBUG("get_mounts_info\n");
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
                //if((q=strstr(p,"/mnt")) != NULL)
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

                        //info->paths =
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
int check_config_path(int is_read_config)
{
    wd_DEBUG("check_config_path start\n");
    int i;
    int flag;
    char *nv;
    char *nvp;
    char *new_nv;
    int nv_len;
    int is_path_change = 0;

#ifdef NVRAM_
#ifndef USE_TCAPI
    nv = strdup(nvram_safe_get("db_tokenfile"));
#else
    char tmp[MAXLEN_TCAPI_MSG] = {0};
    tcapi_get(AICLOUD, "db_tokenfile", tmp);
    nv = my_str_malloc(strlen(tmp)+1);
    sprintf(nv,"%s",tmp);
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
    wd_DEBUG("nv_len = %d\n",nv_len);
    wd_DEBUG("nv = %s\n",nv);

    for(i=0;i<asus_cfg.dir_number;i++)
    {
        flag = 0;
        /*tokenfile exist*/
        tokenfile_info_tmp = tokenfile_info_start->next;
        while(tokenfile_info_tmp != NULL)
        {
            if( !strcmp(tokenfile_info_tmp->url,asus_cfg.user) &&
                !strcmp(tokenfile_info_tmp->folder,asus_cfg.prule[i]->rooturl))
            {
                if(strcmp(tokenfile_info_tmp->mountpath,asus_cfg.prule[i]->base_path))
                {
                    memset(asus_cfg.prule[i]->base_path,0,sizeof(asus_cfg.prule[i]->base_path));
                    sprintf(asus_cfg.prule[i]->base_path,"%s",tokenfile_info_tmp->mountpath);
                    memset(asus_cfg.prule[i]->path,0,sizeof(asus_cfg.prule[i]->path));
                    sprintf(asus_cfg.prule[i]->path,"%s%s",tokenfile_info_tmp->mountpath,tokenfile_info_tmp->folder);
                    asus_cfg.prule[i]->base_path_len = strlen(asus_cfg.prule[i]->base_path);
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
        wd_DEBUG("asus_cfg.prule[%d]->path = %s\n",i,asus_cfg.prule[i]->path);
        if(!flag)
        {
            nvp = my_str_malloc(strlen(asus_cfg.user)+strlen(asus_cfg.prule[i]->rooturl)+2);
            sprintf(nvp,"%s>%s",asus_cfg.user,asus_cfg.prule[i]->rooturl);

            //write_debug_log(nv);
            //write_debug_log(nvp);
            wd_DEBUG("nvp = %s\n",nvp);

            if(!is_read_config)
            {
                if(g_pSyncList[i]->sync_disk_exist == 1)
                    is_path_change = 2;   //remove the disk and the mout_path not change
            }

            //printf("write nvram and tokenfile if before\n");

            /* "name>folder" is not in tokenfile_record */
            if(strstr(nv,nvp) == NULL)
            {
                //printf("write nvram and tokenfile if behind");

                if(initial_tokenfile_info_data(&tokenfile_info_tmp) == NULL)
                {
                    return -1;
                }
                tokenfile_info_tmp->url = my_str_malloc(strlen(asus_cfg.user)+1);
                sprintf(tokenfile_info_tmp->url,"%s",asus_cfg.user);

                tokenfile_info_tmp->mountpath = my_str_malloc(strlen(asus_cfg.prule[i]->base_path)+1);
                sprintf(tokenfile_info_tmp->mountpath,"%s",asus_cfg.prule[i]->base_path);

                tokenfile_info_tmp->folder = my_str_malloc(strlen(asus_cfg.prule[i]->rooturl)+1);
                sprintf(tokenfile_info_tmp->folder,"%s",asus_cfg.prule[i]->rooturl);

                tokenfile_info->next = tokenfile_info_tmp;
                tokenfile_info = tokenfile_info_tmp;

                write_to_tokenfile(asus_cfg.prule[i]->base_path);

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
                wd_DEBUG("new_nv = %s\n",new_nv);
                //write_to_wd_tokenfile(new_nv);

#ifdef NVRAM_
                write_to_nvram(new_nv,"db_tokenfile");
#else
                write_to_wd_tokenfile(new_nv);
#endif

                free(new_nv);
            }
            free(nvp);
        }
    }
    free(nv);
    return is_path_change;
}
int write_to_tokenfile(char *mpath)
{
    //write_debug_log("write_to_tokenfile");
    wd_DEBUG("write_to_tokenfile start\n");
    FILE *fp;
    //int flag=0;

    char *filename;
    filename = my_str_malloc(strlen(mpath)+20+1);
    sprintf(filename,"%s/.dropbox_tokenfile",mpath);
    wd_DEBUG("filename = %s\n",filename);

    int i = 0;
    if( ( fp = fopen(filename,"w") ) == NULL )
    {
        wd_DEBUG("open tokenfile failed!\n");
        return -1;
    }

    tokenfile_info_tmp = tokenfile_info_start->next;
    while(tokenfile_info_tmp != NULL)
    {
        wd_DEBUG("tokenfile_info_tmp->mountpath = %s\n",tokenfile_info_tmp->mountpath);
        if(!strcmp(tokenfile_info_tmp->mountpath,mpath))
        {
            //write_debug_log(tokenfile_info_tmp->folder);
            wd_DEBUG("tokenfile_info_tmp->url = %s\n",tokenfile_info_tmp->url);
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

    wd_DEBUG("write_to_tokenfile end\n");
    return 0;
}
#ifdef NVRAM_
int write_to_nvram(char *contents,char *nv_name)
{
    char *command;
    command = my_str_malloc(strlen(contents)+strlen(SHELL_FILE)+strlen(nv_name)+8);
    sprintf(command,"sh %s \"%s\" %s",SHELL_FILE,contents,nv_name);

    wd_DEBUG("command : [%s]\n",command);

    system(command);
    free(command);

    return 0;
}
#else
int write_to_wd_tokenfile(char *contents)
{
    if(contents == NULL || contents == "")
    {
        unlink(TOKENFILE_RECORD);
        return 0;
    }
    FILE *fp;
    if( ( fp = fopen(TOKENFILE_RECORD,"w") ) == NULL )
    {
        wd_DEBUG("open wd_tokenfile failed!\n");
        return -1;
    }
    fprintf(fp,"%s",contents);
    fclose(fp);
    //printf("write_to_wd_tokenfile end\n");
    return 0;
}
#endif
#ifdef NVRAM_
int convert_nvram_to_file_mutidir(char *file,struct asus_config *config)
{

    wd_DEBUG("enter convert_nvram_to_file_mutidir function\n");

    FILE *fp;
    char *nv, *nvp, *b;
    //struct asus_config config;
    int i;
    //int status;
    char *p;
    char *buffer;
    char *buf;

    fp=fopen(file, "w");

    if (fp==NULL) return -1;
#ifndef USE_TCAPI
    nv = nvp = strdup(nvram_safe_get("cloud_sync"));
#else
    char tmp[MAXLEN_TCAPI_MSG] = {0};
    tcapi_get(AICLOUD, "cloud_sync", tmp);
    nvp = nv = my_str_malloc(strlen(tmp)+1);
    sprintf(nv,"%s",tmp);
#endif

    //printf("otain nvram end\n");

    if(nv)
    {
        while ((b = strsep(&nvp, "<")) != NULL)
        {
            i = 0;
            buf = buffer = strdup(b);

            wd_DEBUG("buffer = %s\n",buffer);

            while((p = strsep(&buffer,">")) != NULL)
            {

                wd_DEBUG("p = %s\n",p);

                if (*p == 0)
                {
                    fprintf(fp,"\n");
                    i++;
                    continue;
                }
                if(i == 0)
                {
                    if(atoi(p) != 3)
                        break;
                    fprintf(fp,"%s",p);
                }
                else
                {
                    fprintf(fp,"\n%s",p);
                }
//                if(i == 5)
//                    config->dir_number = atoi(p);
                if(i == 4)
                    config->dir_number = atoi(p);
                i++;
            }
            free(buf);
        }
        free(nv);
    }

    else
        wd_DEBUG("get nvram fail\n");

    fclose(fp);

    wd_DEBUG("end convert_nvram_to_file_mutidir function\n");

    return 0;
}
#else
int create_webdav_conf_file(struct asus_config *config){


    wd_DEBUG("enter create_dropbox_conf_file function\n");

    FILE *fp;
    char *nv, *nvp, *b;
    int i;
    char *p;
    char *buffer;
    char *buf;

    fp = fopen(TMPCONFIG,"r");
    if (fp==NULL)
    {
        nvp = my_str_malloc(2);
        sprintf(nvp,"");
    }
    else
    {
        fseek( fp , 0 , SEEK_END );
        int file_size;
        file_size = ftell( fp );
        fseek(fp , 0 , SEEK_SET);
        nvp =  (char *)malloc( file_size * sizeof( char ) );
        fread(nvp , file_size , sizeof(char) , fp);
        fclose(fp);
    }

    nv = nvp;

    replace_char_in_str(nvp,'\0','\n');

    wd_DEBUG("**********nv = %s\n",nv);


    fp=fopen(CONFIG_PATH, "w");

    if (fp==NULL) return -1;

    //nv = nvp = strdup(nvram_safe_get("cloud_sync"));

    //printf("otain nvram end\n");
    config->dir_number = 0;
    if(nv)
    {
        while ((b = strsep(&nvp, "<")) != NULL)
        {
            i = 0;
            buf = buffer = strdup(b);

            wd_DEBUG("buffer = %s\n",buffer);

            while((p = strsep(&buffer,">")) != NULL)
            {

                wd_DEBUG("p = %s\n",p);

                if (*p == 0)
                {
                    fprintf(fp,"\n");
                    i++;
                    continue;
                }
                if(i == 0)
                {
                    if(atoi(p) != 3)
                        break;
                    fprintf(fp,"%s",p);
                }
                else
                {
                    fprintf(fp,"\n%s",p);
                }
//                if(i == 5)
//                    config->dir_number = atoi(p);
                if(i == 4)
                    config->dir_number = atoi(p);
                i++;
            }
            //fprintf(fp,"\n");
            free(buf);

        }

        free(nv);

    }
    else
        wd_DEBUG("get nvram fail\n");

    fclose(fp);
    return 0;

}
#endif
int rewrite_tokenfile_and_nv(){

    int i,j;
    int exist;
    //write_debug_log("rewrite_tokenfile_and_nv start");
    wd_DEBUG("rewrite_tokenfile_and_nv start\n");
    if(asus_cfg.dir_number > asus_cfg_stop.dir_number)
    {
        for(i=0;i<asus_cfg.dir_number;i++)
        {
            exist = 0;
            for(j=0;j<asus_cfg_stop.dir_number;j++)
            {
                if(!strcmp(asus_cfg_stop.prule[j]->rooturl,asus_cfg.prule[i]->rooturl))
                {
                    exist = 1;
                    break;
                }
            }
            if(!exist)
            {
                //write_debug_log("del form nv");
                wd_DEBUG("del form nv\n");
                char *new_nv;
                delete_tokenfile_info(asus_cfg.user,asus_cfg.prule[i]->rooturl);
                new_nv = delete_nvram_contents(asus_cfg.user,asus_cfg.prule[i]->rooturl);
                wd_DEBUG("new_nv = %s\n",new_nv);

                remove(g_pSyncList[i]->conflict_file);

                write_to_tokenfile(asus_cfg.prule[i]->base_path);

#ifdef NVRAM_
                write_to_nvram(new_nv,"db_tokenfile");
#else
                write_to_wd_tokenfile(new_nv);
#endif

                //write_to_wd_tokenfile(new_nv);
                free(new_nv);
            }
        }
    }
    else
    {
        for(i=0;i<asus_cfg_stop.dir_number;i++)
        {
            exist = 0;
            for(j=0;j<asus_cfg.dir_number;j++)
            {
                if(!strcmp(asus_cfg_stop.prule[i]->rooturl,asus_cfg.prule[j]->rooturl))
                {
                    exist = 1;
                    break;
                }
            }
            if(!exist)
            {
                //write_debug_log("add to nv");
                wd_DEBUG("add to nv\n");
                char *new_nv;
                add_tokenfile_info(asus_cfg_stop.user,asus_cfg_stop.prule[i]->rooturl,asus_cfg_stop.prule[i]->base_path);
                new_nv = add_nvram_contents(asus_cfg_stop.user,asus_cfg_stop.prule[i]->rooturl);
                wd_DEBUG("new_nv = %s\n",new_nv);
                wd_DEBUG("base_path = %s\n",asus_cfg_stop.prule[i]->base_path);
                write_to_tokenfile(asus_cfg_stop.prule[i]->base_path);

#ifdef NVRAM_
                write_to_nvram(new_nv,"db_tokenfile");
#else
                write_to_wd_tokenfile(new_nv);
#endif

                //write_to_wd_tokenfile(new_nv);
                free(new_nv);
            }
        }
    }
    return 0;
}
#ifndef NVRAM_
int write_get_nvram_script(){

    FILE *fp;
    char contents[512];
    memset(contents,0,512);
    strcpy(contents,"#! /bin/sh\nNV=`nvram get cloud_sync`\nif [ ! -f \"/tmp/smartsync/dropbox/config/dropbox_tmpconfig\" ]; then\n   touch /tmp/smartsync/dropbox/config/dropbox_tmpconfig\nfi\necho \"$NV\" >/tmp/smartsync/dropbox/config/dropbox_tmpconfig");

    if(( fp = fopen(DropBox_Get_Nvram,"w"))==NULL)
    {
        fprintf(stderr,"create dropbox_get_nvram file error\n");
        return -1;
    }

    fprintf(fp,"%s",contents);
    fclose(fp);

    return 0;
}
int write_get_nvram_script_va(char *str)
{
    FILE *fp;
    char contents[512];
    memset(contents,0,512);
    //strcpy(contents,"#! /bin/sh\nNV=`nvram get cloud_sync`\nif [ ! -f \"/tmp/smartsync/dropbox/config/dropbox_tmpconfig\" ]; then\n   touch /tmp/smartsync/dropbox/config/dropbox_tmpconfig\nfi\necho \"$NV\" >/tmp/smartsync/dropbox/config/dropbox_tmpconfig");
    sprintf(contents,"#! /bin/sh\nNV=`nvram get %s`\nif [ ! -f \"%s\" ]; then\n   touch %s\nfi\necho \"$NV\" >%s",str,TMP_NVRAM_VL,TMP_NVRAM_VL,TMP_NVRAM_VL);

    if(( fp = fopen(DropBox_Get_Nvram_Link,"w"))==NULL)
    {
        fprintf(stderr,"create dropbox_get_nvram file error\n");
        return -1;
    }

    fprintf(fp,"%s",contents);
    fclose(fp);

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
            tokenfile_info_tmp = NULL;
            break;
        }
        tmp = tokenfile_info_tmp;
        tokenfile_info_tmp = tokenfile_info_tmp->next;
    }

    //write_debug_log("delete_tokenfile_info stop");
    return 0;
}
char *delete_nvram_contents(char *url,char *folder){
    //write_debug_log("delete_nvram_contents start");

    char *nv;
    char *nvp;
    char *p,*b;

    char *new_nv;
    int n;
    int i=0;



#ifdef NVRAM_
#ifndef USE_TCAPI
    p = nv = strdup(nvram_safe_get("db_tokenfile"));
#else
    char tmp[MAXLEN_TCAPI_MSG] = {0};
    tcapi_get(AICLOUD, "db_tokenfile", tmp);
    p = nv = my_str_malloc(strlen(tmp)+1);
    sprintf(nv,"%s",tmp);
#endif
#else
    FILE *fp;
    fp = fopen(TOKENFILE_RECORD,"r");
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

        nv = my_str_malloc(file_size+2);

        fscanf(fp,"%[^\n]%*c",nv);
        p = nv;
        fclose(fp);
    }

#endif

    nvp = my_str_malloc(strlen(url)+strlen(folder)+2);
    sprintf(nvp,"%s>%s",url,folder);



    if(strstr(nv,nvp) == NULL)
    {
        free(nvp);
        return nv;
    }



    if(!strcmp(nv,nvp))
    {
        free(nvp);
        memset(nv,0,sizeof(nv));
        sprintf(nv,"");
        return nv;
    }


    if(nv)
    {
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
                    new_nv = realloc(new_nv, strlen(new_nv)+n+2);
                    sprintf(new_nv,"%s<%s",new_nv,b);
                }
            }
        }

        free(nv);
    }
    free(nvp);

    return new_nv;
}
int add_tokenfile_info(char *url,char *folder,char *mpath){

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

    return 0;
}
char *add_nvram_contents(char *url,char *folder){

    char *nv;
    int nv_len;
    char *new_nv;
    char *nvp;

    nvp = my_str_malloc(strlen(url)+strlen(folder)+2);
    sprintf(nvp,"%s>%s",url,folder);

    wd_DEBUG("add_nvram_contents     nvp = %s\n",nvp);


#ifdef NVRAM_
#ifndef USE_TCAPI
    nv = strdup(nvram_safe_get("db_tokenfile"));
#else
    char tmp[MAXLEN_TCAPI_MSG] = {0};
    tcapi_get(AICLOUD, "db_tokenfile", tmp);
    nv = my_str_malloc(strlen(tmp)+1);
    sprintf(nv,"%s",tmp);
#endif
#else
    FILE *fp;
    fp = fopen(TOKENFILE_RECORD,"r");
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
        wd_DEBUG("add_nvram_contents     file_size = %d\n",file_size);
        //nv =  (char *)malloc( file_size * sizeof( char ) );
        nv = my_str_malloc(file_size+2);
        //fread(nv , file_size ,1, fp);
        fscanf(fp,"%[^\n]%*c",nv);
        fclose(fp);
    }
#endif

    wd_DEBUG("add_nvram_contents     nv = %s\n",nv);
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
    wd_DEBUG("add_nvram_contents     new_nv = %s\n",new_nv);
    free(nvp);
    free(nv);
    return new_nv;
}
int check_sync_disk_removed()
{

    wd_DEBUG("check_sync_disk_removed start! \n");


    int ret;

    free_tokenfile_info(tokenfile_info_start);

    if(get_tokenfile_info()==-1)
    {
        wd_DEBUG("\nget_tokenfile_info failed\n");
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

        sync_disk_removed = 1;
    }

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
            //printf("free CloudFile %s\n",p->href);
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
int detect_client()
{
    unlink("/tmp/smartsync_app/dropbox_client_start");
    char *dir = "/tmp/smartsync_app";
    int flag = 0;
    if(access(dir,0) != 0)
        flag = 0;
    else
    {
        struct dirent* ent = NULL;
        DIR *pDir;
        pDir=opendir(dir);
        int file_num = 0;
        if(pDir != NULL )
        {
            while (NULL != (ent=readdir(pDir)))
            {
                if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,".."))
                    continue;
                file_num ++ ;
            }
            closedir(pDir);

            if(file_num > 0)
                flag = 1;
            else
                flag = 0;

        }
        else
            flag = 0;
    }
    if(!flag)
    {
        system("killall -9 inotify &");
    }
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
int create_shell_file(){

    FILE *fp;
    char contents[1024];
    memset(contents,0,1024);
#ifndef USE_TCAPI
    strcpy(contents,"#! /bin/sh\nnvram set $2=\"$1\"\nnvram commit");
#else
    strcpy(contents,"#! /bin/sh\ntcapi set AiCloud_Entry $2 \"$1\"\ntcapi commit AiCloud\ntcapi save");
#endif

    if(( fp = fopen(SHELL_FILE,"w"))==NULL)
    {
        fprintf(stderr,"create shell file error");
        return -1;
    }

    fprintf(fp,"%s",contents);
    fclose(fp);
    return 0;

}
/*process running,return 1;else return 0*/

#endif
#endif

void deal_big_low_conflcit(char *server_conflict_name,char *oldname,char *newname,char *newname_r,int index)
{
    /*get conflict name*/

    char *local_conflcit_name = serverpath_to_localpath(server_conflict_name,index);
//    char *tmp_name = malloc(strlen(local_conflcit_name)+1);
//    memset(tmp_name,0,strlen(local_conflcit_name)+1);
//    sprintf(tmp_name,"%s",local_conflcit_name);
    char *g_newname = NULL;
    int is_folder = test_if_dir(local_conflcit_name);

    g_newname = get_confilicted_name_case(local_conflcit_name,is_folder);

    wd_DEBUG("case-conflict=%s\n",g_newname);
    my_free(local_conflcit_name);


//    char *prefix_name = get_prefix_name(tmp_name,is_folder);//a.txt --> a

//    g_newname = get_confilicted_name_first(tmp_name,is_folder);//a.txt-->a(case-conflict).txt

//    if(access(g_newname,F_OK) == 0)//a(case-conflict).txt-->a(case-conflict(n)).txt
//    {
//        my_free(tmp_name);
//        tmp_name = malloc(strlen(g_newname)+1);
//        memset(tmp_name,0,strlen(g_newname)+1);
//        sprintf(tmp_name,"%s",g_newname);
//        my_free(g_newname);
//        while(!exit_loop)
//        {
//            g_newname = get_confilicted_name_second(tmp_name,is_folder,prefix_name);
//            //printf("confilicted_name=%s\n",confilicted_name);
//            if(access(g_newname,F_OK) == 0)
//            {
//                my_free(tmp_name);
//                tmp_name = malloc(strlen(g_newname)+1);
//                memset(tmp_name,0,strlen(g_newname)+1);
//                sprintf(tmp_name,"%s",g_newname);
//                my_free(g_newname);
//                //have_same = 1;
//            }
//            else
//                break;
//        }
//        my_free(tmp_name);
//    }else
//    {
//        my_free(tmp_name);
//    }
//    my_free(prefix_name);

//    while(!exit_loop)
//    {
//        g_newname = get_confilicted_name(tmp_name,is_folder);
//        //printf("confilicted_name=%s\n",confilicted_name);
//        if(access(g_newname,F_OK) == 0)
//        {
//            my_free(tmp_name);
//            tmp_name = malloc(strlen(g_newname)+1);
//            memset(tmp_name,0,strlen(g_newname)+1);
//            sprintf(tmp_name,"%s",g_newname);
//            my_free(g_newname);
//            //have_same = 1;
//        }
//        else
//            break;
//    }
//    my_free(tmp_name);

    char *localname;
    /*rename local conflict name to newname*/
    if(newname_r == NULL)
    {
        localname = serverpath_to_localpath(newname,index);
    }
    else
        localname = newname_r;

    updata_socket_list(localname,g_newname,index);

    if(access(localname,F_OK) == 0)
    {
        add_action_item("rename",g_newname,g_pSyncList[index]->server_action_list);
        rename(localname,g_newname);
    }

    if(newname_r == NULL)
        my_free(localname);
    /*reback to deal rename a--> bb(1)*/
    char *s_newname = localpath_to_serverpath(g_newname,index);
    my_free(g_newname);
    int status = 0;
    do{
        status = api_move(oldname,s_newname,index,1,NULL);
    }while(status != 0 && !exit_loop);
    my_free(s_newname);
}
