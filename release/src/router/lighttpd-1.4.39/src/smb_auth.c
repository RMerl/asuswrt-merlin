#include "base.h"

#ifdef HAVE_LIBSMBCLIENT

//#include "smb.h"
#include "libsmbclient.h"
#include "log.h"
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlinklist.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#ifdef USE_OPENSSL
#include <openssl/md5.h>
#else
#include "md5.h"
typedef li_MD5_CTX MD5_CTX;
#define MD5_Init li_MD5_Init
#define MD5_Update li_MD5_Update
#define MD5_Final li_MD5_Final
#endif

#if EMBEDDED_EANBLE
#ifndef APP_IPKG
#include "disk_share.h"
#endif
#endif

#if defined(HAVE_LIBXML_H) && defined(HAVE_SQLITE3_H)
#include <sqlite3.h>
#endif

#define DBE 0
#define LIGHTTPD_ARPPING_PID_FILE_PATH	"/tmp/lighttpd/lighttpd-arpping.pid"

int file_exist(const char *filepath)
{
	struct stat stat_buf;
    if (!stat(filepath, &stat_buf))
    	return S_ISREG(stat_buf.st_mode);
    else
        return 0;
}

int dir_exist(const char *dirpath){
	struct stat stat_buf;

    if (!stat(dirpath, &stat_buf))
    	return S_ISDIR(stat_buf.st_mode);
    else
    	return 0;
}

unsigned long file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return st.st_size;
    return (unsigned long)-1;
}

char *read_whole_file(const char *target){
	FILE *fp;
    char *buffer, *new_str;
    int i;
    unsigned int read_bytes = 0;
    unsigned int each_size = 1024;

	if((fp = fopen(target, "r")) == NULL)
    	return NULL;

    buffer = (char *)malloc(sizeof(char)*each_size);
    if(buffer == NULL){
    	//_dprintf("No memory \"buffer\".\n");
        fclose(fp);
        return NULL;
    }
    memset(buffer, 0, each_size);

    while ((i = fread(buffer+read_bytes, each_size * sizeof(char), 1, fp)) == 1){
    	read_bytes += each_size;
        new_str = (char *)malloc(sizeof(char)*(each_size+read_bytes));
        if(new_str == NULL){
        	//_dprintf("No memory \"new_str\".\n");
            free(buffer);
            fclose(fp);
            return NULL;
        }
        memset(new_str, 0, sizeof(char)*(each_size+read_bytes));
        memcpy(new_str, buffer, read_bytes);

        free(buffer);
        buffer = new_str;
    }

    fclose(fp);
    return buffer;
}

char* get_aicloud_db_file_path(){
#ifdef EMBEDDED_EANBLE
	return "/jffs/aicloud.db";
#else
	return "/tmp/aicloud.db";
#endif
}

#ifdef APP_IPKG
/*???start?*/
#include <dirent.h>
#define PROTOCOL_CIFS "cifs"
#define PROTOCOL_FTP "ftp"
#define PROTOCOL_MEDIASERVER "dms"
#ifdef RTCONFIG_WEBDAV_OLD
#define PROTOCOL_WEBDAV "webdav"
#define MAX_PROTOCOL_NUM 4
#else
#define MAX_PROTOCOL_NUM 3
#endif

#define PROTOCOL_CIFS_BIT 0
#define PROTOCOL_FTP_BIT 1
#define PROTOCOL_MEDIASERVER_BIT 2
#ifdef RTCONFIG_WEBDAV_OLD
#define PROTOCOL_WEBDAV_BIT 3
#endif

#define DEFAULT_SAMBA_RIGHT 3
#define DEFAULT_FTP_RIGHT 3
#define DEFAULT_DMS_RIGHT 1
#ifdef RTCONFIG_WEBDAV_OLD
#define DEFAULT_WEBDAV_RIGHT 3
#endif

#define DEBUG_USB

#ifdef DEBUG_USB
#define usb_dbg(fmt, args...) do{ \
                FILE *fp = fopen("/tmp/usb.log", "a+"); \
                if(fp){ \
                        fprintf(fp, "[usb_dbg: %s] ", __FUNCTION__); \
                        fprintf(fp, fmt, ## args); \
                        fclose(fp); \
                } \
        }while(0)
#else
#define usb_dbg printf
#endif

/* Transfer Char to ASCII */
int char_to_ascii_safe(const char *output, const char *input, int outsize)
{
	char *src = (char *)input;
    char *dst = (char *)output;
    char *end = (char *)output + outsize - 1;
    char *escape = "[]"; // shouldn't be more?

    if (src == NULL || dst == NULL || outsize <= 0)
    	return 0;

	for ( ; *src && dst < end; src++) {
		if ((*src >='0' && *src <='9') ||
            (*src >='A' && *src <='Z') ||
            (*src >='a' && *src <='z')) {
        	*dst++ = *src;
        } else if (strchr(escape, *src)) {
        	if (dst + 2 > end)
            	break;
            *dst++ = '\\';
            *dst++ = *src;
        } else {
        	if (dst + 3 > end)
            	break;
            dst += sprintf(dst, "%%%.02X", *src);
        }
	}
    if (dst <= end)
    	*dst = '\0';
	
    return dst - output;
}

#ifdef APP_IPKG
int
check_if_file_exist(const char *filepath)
{
	struct stat stat_buf;

    if (!stat(filepath, &stat_buf))
    	return S_ISREG(stat_buf.st_mode);
    else
        return 0;
}
#endif
#if 0
unsigned long f_size(const char *path)	// 4GB-1	-1 = error
{
    struct stat st;
    if (stat(path, &st) == 0) return st.st_size;
    return (unsigned long)-1;
}
#endif

extern int check_file_integrity(const char *const file_name){
    unsigned long fsize;
    char test_file[PATH_MAX];

    if((fsize = file_size(file_name)) == -1){
        usb_dbg("Fail to get the size of the file.\n");
        return 0;
    }

    memset(test_file, 0, PATH_MAX);
    sprintf(test_file, "%s.%lu", file_name, fsize);
    if(!file_exist(test_file)){
        usb_dbg("Fail to check the folder list.\n");
        return 0;
    }

    return 1;
}

#if 0
int
check_if_dir_exist(const char *dirpath){
	struct stat stat_buf;

    if (!stat(dirpath, &stat_buf))
    	return S_ISDIR(stat_buf.st_mode);
    else
    	return 0;
}
#endif

extern int delete_file_or_dir(char *target){
	int ret;

    if(dir_exist(target))
    	ret = rmdir(target);
    else
        ret = unlink(target);

    return ret;
}

extern char *get_upper_str(const char *const str, char **target){
	int len, i;
    char *ptr;

    len = strlen(str);
    *target = (char *)malloc(sizeof(char)*(len+1));
    if(*target == NULL){
    	printf("No memory \"*target\".\n");
        return NULL;
    }
    ptr = *target;
    for(i = 0; i < len; ++i)
    	ptr[i] = toupper(str[i]);
   	ptr[len] = 0;

    return ptr;
}

extern int upper_strcmp(const char *const str1, const char *const str2){
    char *upper_str1, *upper_str2;
    int ret;

    if(str1 == NULL || str2 == NULL)
    	return -1;

    if(get_upper_str(str1, &upper_str1) == NULL)
        return -1;

    if(get_upper_str(str2, &upper_str2) == NULL){
    	free(upper_str1);
        return -1;
    }

    ret = strcmp(upper_str1, upper_str2);
    free(upper_str1);
    free(upper_str2);

    return ret;
}

extern int test_if_System_folder(const char *const dirname){
	const char *const MS_System_folder[] = {"SYSTEM VOLUME INFORMATION", "RECYCLER", "RECYCLED", "$RECYCLE.BIN", NULL};
    const char *const Linux_System_folder[] = {"lost+found", NULL};
    int i;

    for(i = 0; MS_System_folder[i] != NULL; ++i){
    	if(!upper_strcmp(dirname, MS_System_folder[i]))
        	return 1;
    }

    for(i = 0; Linux_System_folder[i] != NULL; ++i){
    	if(!upper_strcmp(dirname, Linux_System_folder[i]))
        	return 1;
    }

    return 0;
}

extern int get_var_file_name(const char *const account, const char *const path, char **file_name){
    int len;
    char *var_file;
    char ascii_user[64];

    if(path == NULL)
        return -1;

    len = strlen(path)+strlen("/.___var.txt");
    if(account != NULL){
        memset(ascii_user, 0, 64);
        char_to_ascii_safe(ascii_user, account, 64);

        len += strlen(ascii_user);
    }
    *file_name = (char *)malloc(sizeof(char)*(len+1));
    if(*file_name == NULL)
        return -1;

    var_file = *file_name;
    if(account != NULL)
        sprintf(var_file, "%s/.__%s_var.txt", path, ascii_user);
    else
        sprintf(var_file, "%s/.___var.txt", path);
    var_file[len] = 0;

    return 0;
}


extern char *upper_strstr(const char *const str, const char *const target){
    char *upper_str, *upper_target;
    char *ret;
    int len;

    if(str == NULL || target == NULL)
    	return NULL;

    if(get_upper_str(str, &upper_str) == NULL)
    	return NULL;

    if(get_upper_str(target, &upper_target) == NULL){
    	free(upper_str);
        return NULL;
    }

    ret = strstr(upper_str, upper_target);
    if(ret == NULL){
    	free(upper_str);
        free(upper_target);
        return NULL;
    }

    if((len = upper_str-ret) < 0)
    	len = ret-upper_str;

    free(upper_str);
    free(upper_target);

    return (char *)(str+len);
}

extern void free_2_dimension_list(int *num, char ***list) {
    int i;
    char **target = *list;

    if (*num <= 0 || target == NULL){
        *num = 0;
        return;
    }

    for (i = 0; i < *num; ++i)
        if (target[i] != NULL)
            free(target[i]);

    if (target != NULL)
        free(target);

    *num = 0;
}

extern int get_all_folder(const char *const mount_path, int *sh_num, char ***folder_list) {
    DIR *pool_to_open;
    struct dirent *dp;
    char *testdir;
    char **tmp_folder_list, **tmp_folder;
    int len, i;

    pool_to_open = opendir(mount_path);
    if (pool_to_open == NULL) {
        usb_dbg("Can't opendir \"%s\".\n", mount_path);
        return -1;
    }

    *sh_num = 0;
    while ((dp = readdir(pool_to_open)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;

        if (test_if_System_folder(dp->d_name) == 1)
            continue;

        len = strlen(mount_path)+strlen("/")+strlen(dp->d_name);
        testdir = (char *)malloc(sizeof(char)*(len+1));
        if (testdir == NULL) {
            closedir(pool_to_open);
            return -1;
        }
        sprintf(testdir, "%s/%s", mount_path, dp->d_name);
        testdir[len] = 0;
        if (!dir_exist(testdir)) {
            free(testdir);
            continue;
        }
        free(testdir);

        tmp_folder = (char **)malloc(sizeof(char *)*(*sh_num+1));
        if (tmp_folder == NULL) {
            usb_dbg("Can't malloc \"tmp_folder\".\n");
            return -1;
        }

        len = strlen(dp->d_name);
        tmp_folder[*sh_num] = (char *)malloc(sizeof(char)*(len+1));
        if (tmp_folder[*sh_num] == NULL) {
            usb_dbg("Can't malloc \"tmp_folder[%d]\".\n", *sh_num);
            free(tmp_folder);
            return -1;
        }
        strcpy(tmp_folder[*sh_num], dp->d_name);
        if (*sh_num != 0) {
            for (i = 0; i < *sh_num; ++i)
                tmp_folder[i] = tmp_folder_list[i];

            free(tmp_folder_list);
            tmp_folder_list = tmp_folder;
        }
        else
            tmp_folder_list = tmp_folder;

        ++(*sh_num);
    }
    closedir(pool_to_open);

    *folder_list = tmp_folder_list;

    return 0;
}

extern void set_file_integrity(const char *const file_name){
    unsigned long fsize;
    char test_file[PATH_MAX], test_file_name[PATH_MAX];
    FILE *fp;
    char target_dir[PATH_MAX], *ptr;
    int len;
    DIR *opened_dir;
    struct dirent *dp;

    if((ptr = strrchr(file_name, '/')) == NULL){
        usb_dbg("Fail to get the target_dir of the file.\n");
        return;
    }
    len = strlen(file_name)-strlen(ptr);
    memset(target_dir, 0, PATH_MAX);
    strncpy(target_dir, file_name, len);

    if((fsize = file_size(file_name)) == -1){
        usb_dbg("Fail to get the size of the file.\n");
        return;
    }

    memset(test_file, 0, PATH_MAX);
    sprintf(test_file, "%s.%lu", file_name, fsize);
    if((fp = fopen(test_file, "w")) != NULL)
        fclose(fp);

    memset(test_file_name, 0, PATH_MAX);
    ++ptr;
    sprintf(test_file_name, "%s.%lu", ptr, fsize);

    if((opened_dir = opendir(target_dir)) == NULL){
        usb_dbg("Can't opendir \"%s\".\n", target_dir);
        return;
    }

    len = strlen(ptr);
    while((dp = readdir(opened_dir)) != NULL){
        char test_path[PATH_MAX];

        if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;

        if(strncmp(dp->d_name, ptr, len) || !strcmp(dp->d_name, ptr) || !strcmp(dp->d_name, test_file_name))
            continue;

        memset(test_path, 0, PATH_MAX);
        sprintf(test_path, "%s/%s", target_dir, dp->d_name);
        usb_dbg("delete %s.\n", test_path);
        delete_file_or_dir(test_path);
    }
    closedir(opened_dir);
}

extern int initial_var_file(const char *const account, const char *const mount_path) {
    FILE *fp;
    char *var_file;
    int result, i;
    int sh_num;
    char **folder_list;
    int samba_right, ftp_right, dms_right;
#ifdef RTCONFIG_WEBDAV_OLD
    int webdav_right;
#endif

    if (mount_path == NULL || strlen(mount_path) <= 0) {
        usb_dbg("No input, mount_path\n");
        return -1;
    }

    // 1. get the folder number and folder_list
    //result = get_folder_list(mount_path, &sh_num, &folder_list);
    result = get_all_folder(mount_path, &sh_num, &folder_list);

    // 2. get the var file
    if(get_var_file_name(account, mount_path, &var_file)){
        usb_dbg("Can't malloc \"var_file\".\n");
        free_2_dimension_list(&sh_num, &folder_list);
        return -1;
    }

    // 3. get the default permission of all protocol.
    char *aa=nvram_safe_get("http_username"); //add by zero

    if(account == NULL // share mode.
       || !strcmp(account, aa)){
//       || !strcmp(account, nvram_safe_get("http_username"))){
        samba_right = DEFAULT_SAMBA_RIGHT;
        ftp_right = DEFAULT_FTP_RIGHT;
        dms_right = DEFAULT_DMS_RIGHT;
#ifdef RTCONFIG_WEBDAV_OLD
        webdav_right = DEFAULT_WEBDAV_RIGHT;
#endif
    }
    else{
        samba_right = 0;
        ftp_right = 0;
        dms_right = 0;
#ifdef RTCONFIG_WEBDAV_OLD
        webdav_right = 0;
#endif
    }
    free(aa);
    // 4. write the default content in the var file
    if ((fp = fopen(var_file, "w")) == NULL) {
        usb_dbg("Can't create the var file, \"%s\".\n", var_file);
        free_2_dimension_list(&sh_num, &folder_list);
        free(var_file);
        return -1;
    }

    for (i = -1; i < sh_num; ++i) {
        fprintf(fp, "*");

        if(i != -1)
            fprintf(fp, "%s", folder_list[i]);
#ifdef RTCONFIG_WEBDAV_OLD
        fprintf(fp, "=%d%d%d%d\n", samba_right, ftp_right, dms_right, webdav_right);
#else
        fprintf(fp, "=%d%d%d\n", samba_right, ftp_right, dms_right);
#endif
    }

    fclose(fp);
    free_2_dimension_list(&sh_num, &folder_list);

    // 5. set the check target of file.
    set_file_integrity(var_file);
    free(var_file);

    return 0;
}

extern int get_permission(const char *const account,
                              const char *const mount_path,
                              const char *const folder,
                              const char *const protocol) {
    char *var_file, *var_info;
    char *target, *follow_info;
    int len, result;

    // 1. get the var file
    if(get_var_file_name(account, mount_path, &var_file)){
    	usb_dbg("Can't malloc \"var_file\".\n");
        return -1;
    }

    // 2. check the file integrity.
    if(!check_file_integrity(var_file)){
        usb_dbg("Fail to check the file: %s.\n", var_file);
    	if(initial_var_file(account, mount_path) != 0){
        	usb_dbg("Can't initial \"%s\"'s file in %s.\n", account, mount_path);
                free(var_file);
                return -1;
        }
    }

    // 3. get the content of the var_file of the account
    var_info = read_whole_file(var_file);
    if (var_info == NULL) {
    	usb_dbg("get_permission: \"%s\" isn't existed or there's no content.\n", var_file);
        free(var_file);
    	return -1;
    }
    free(var_file);

    // 4. get the target in the content
    if(folder == NULL)
    	len = strlen("*=");
    else
        len = strlen("*")+strlen(folder)+strlen("=");
    target = (char *)malloc(sizeof(char)*(len+1));
    if (target == NULL) {
    	usb_dbg("Can't allocate \"target\".\n");
        free(var_info);
        return -1;
    }
    if(folder == NULL)
    	strcpy(target, "*=");
    else
    	sprintf(target, "*%s=", folder);
    target[len] = 0;

    follow_info = upper_strstr(var_info, target);
    free(target);
    if (follow_info == NULL) {
    	if(account == NULL)
        	usb_dbg("No right about \"%s\" with the share mode.\n", (folder == NULL?"Pool":folder));
        else
        	usb_dbg("No right about \"%s\" with \"%s\".\n", (folder == NULL?"Pool":folder), account);
        free(var_info);
        return -1;
	}

    follow_info += len;

    if (follow_info[MAX_PROTOCOL_NUM] != '\n') {
    	if(account == NULL)
        	usb_dbg("The var info is incorrect.\nPlease reset the var file of the share mode.\n");
        else
            usb_dbg("The var info is incorrect.\nPlease reset the var file of \"%s\".\n", account);

        free(var_info);
        return -1;
    }

    // 5. get the right of folder
    if (!strcmp(protocol, PROTOCOL_CIFS))
    	result = follow_info[0]-'0';
    else if (!strcmp(protocol, PROTOCOL_FTP))
        result = follow_info[1]-'0';
    else if (!strcmp(protocol, PROTOCOL_MEDIASERVER))
        result = follow_info[2]-'0';
#ifdef RTCONFIG_WEBDAV_OLD
    else if (!strcmp(protocol, PROTOCOL_WEBDAV))
        result = follow_info[3]-'0';
#endif
    else{
        usb_dbg("The protocol, \"%s\", is incorrect.\n", protocol);
        free(var_info);
    	return -1;
    }
    free(var_info);

    if (result < 0 || result > 3) {
    	if(account == NULL)
        	usb_dbg("The var info is incorrect.\nPlease reset the var file of the share mode.\n");
        else
        	usb_dbg("The var info is incorrect.\nPlease reset the var file of \"%s\".\n", account);
    	return -1;
    }

    return result;
}

/*???end?*/

/*pids*/
#include <errno.h>
#include<sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <stddef.h>
enum {
	PSSCAN_PID      = 1 << 0,
    PSSCAN_PPID     = 1 << 1,
    PSSCAN_PGID     = 1 << 2,
    PSSCAN_SID      = 1 << 3,
    PSSCAN_UIDGID   = 1 << 4,
    PSSCAN_COMM     = 1 << 5,
    /* PSSCAN_CMD      = 1 << 6, - use read_cmdline instead */
    PSSCAN_ARGV0    = 1 << 7,
    /* PSSCAN_EXE      = 1 << 8, - not implemented */
    PSSCAN_STATE    = 1 << 9,
    PSSCAN_VSZ      = 1 << 10,
    PSSCAN_RSS      = 1 << 11,
    PSSCAN_STIME    = 1 << 12,
    PSSCAN_UTIME    = 1 << 13,
    PSSCAN_TTY      = 1 << 14,
    PSSCAN_SMAPS    = (1 << 15) * 0,
    PSSCAN_ARGVN    = (1 << 16) * 1,
    PSSCAN_START_TIME = 1 << 18,
    /* These are all retrieved from proc/NN/stat in one go: */
    PSSCAN_STAT     = PSSCAN_PPID | PSSCAN_PGID | PSSCAN_SID
                      | PSSCAN_COMM | PSSCAN_STATE
                      | PSSCAN_VSZ | PSSCAN_RSS
                      | PSSCAN_STIME | PSSCAN_UTIME | PSSCAN_START_TIME
                      | PSSCAN_TTY,
};

#define PROCPS_BUFSIZE 1024

static int read_to_buf(const char *filename, void *buf)
{
    int fd;
    /* open_read_close() would do two reads, checking for EOF.
         * When you have 10000 /proc/$NUM/stat to read, it isn't desirable */
	int ret = -1;
    fd = open(filename, O_RDONLY);
   	if (fd >= 0) {
    	ret = read(fd, buf, PROCPS_BUFSIZE-1);
        close(fd);
    }
    ((char *)buf)[ret > 0 ? ret : 0] = '\0';
    return ret;
}

void* xzalloc(size_t size)
{
    void *ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void* xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL && size != 0)
    	perror("no memory");
    return ptr;
}

void* xrealloc_vector_helper(void *vector, unsigned sizeof_and_shift, int idx)
{
    int mask = 1 << (unsigned char)sizeof_and_shift;

    if (!(idx & (mask - 1))) {
    	sizeof_and_shift >>= 8; /* sizeof(vector[0]) */
        vector = xrealloc(vector, sizeof_and_shift * (idx + mask + 1));
        memset((char*)vector + (sizeof_and_shift * idx), 0, sizeof_and_shift * (mask + 1));
    }
    return vector;
}

#define xrealloc_vector(vector, shift, idx) \
        xrealloc_vector_helper((vector), (sizeof((vector)[0]) << 8) + (shift), (idx))

typedef struct procps_status_t {
    DIR *dir;
    unsigned char shift_pages_to_bytes;
    unsigned char shift_pages_to_kb;
	/* Fields are set to 0/NULL if failed to determine (or not requested) */
    unsigned int argv_len;
    char *argv0;
    /* Everything below must contain no ptrs to malloc'ed data:
         * it is memset(0) for each process in procps_scan() */
    unsigned long vsz, rss; /* we round it to kbytes */
    unsigned long stime, utime;
    unsigned long start_time;
    unsigned pid;
    unsigned ppid;
    unsigned pgid;
    unsigned sid;
    unsigned uid;
    unsigned gid;
    unsigned tty_major,tty_minor;
    char state[4];
    /* basename of executable in exec(2), read from /proc/N/stat
         * (if executable is symlink or script, it is NOT replaced
         * by link target or interpreter name) */
    char comm[16];
    /* user/group? - use passwd/group parsing functions */
} procps_status_t;

static procps_status_t* alloc_procps_scan(void)
{
	unsigned n = getpagesize();
    procps_status_t* sp = xzalloc(sizeof(procps_status_t));
    sp->dir = opendir("/proc");
    while (1) {
    	n >>= 1;
        if (!n) break;
        sp->shift_pages_to_bytes++;
    }
    sp->shift_pages_to_kb = sp->shift_pages_to_bytes - 10;
    return sp;
}

void BUG_comm_size(void)
{
}
#define ULLONG_MAX     (~0ULL)
#define UINT_MAX       (~0U)

static unsigned long long ret_ERANGE(void)
{
    errno = ERANGE; /* this ain't as small as it looks (on glibc) */
	return ULLONG_MAX;
}
static unsigned long long handle_errors(unsigned long long v, char **endp, char *endptr)
{
    if (endp) *endp = endptr;

    /* errno is already set to ERANGE by strtoXXX if value overflowed */
    if (endptr[0]) {
        /* "1234abcg" or out-of-range? */
        if (isalnum(endptr[0]) || errno)
            return ret_ERANGE();
        /* good number, just suspicious terminator */
        errno = EINVAL;
    }
    return v;
}
unsigned bb_strtou(const char *arg, char **endp, int base)
{
    unsigned long v;
    char *endptr;

    if (!isalnum(arg[0])) return ret_ERANGE();
    errno = 0;
    v = strtoul(arg, &endptr, base);
    if (v > UINT_MAX) return ret_ERANGE();
    return handle_errors(v, endp, endptr);
}

const char* bb_basename(const char *name)
{
    const char *cp = strrchr(name, '/');
    if (cp)
    	return cp + 1;
    return name;
}

static int comm_match(procps_status_t *p, const char *procName)
{
    int argv1idx;

    /* comm does not match */
    if (strncmp(p->comm, procName, 15) != 0)
    	return 0;

    /* in Linux, if comm is 15 chars, it may be a truncated */
    if (p->comm[14] == '\0') /* comm is not truncated - match */
         return 1;

    /* comm is truncated, but first 15 chars match.
         * This can be crazily_long_script_name.sh!
         * The telltale sign is basename(argv[1]) == procName. */

    if (!p->argv0)
    	return 0;

    argv1idx = strlen(p->argv0) + 1;
    if (argv1idx >= p->argv_len)
    	return 0;

    if (strcmp(bb_basename(p->argv0 + argv1idx), procName) != 0)
    	return 0;

    return 1;
}

void free_procps_scan(procps_status_t* sp)
{
    closedir(sp->dir);
    free(sp->argv0);
    free(sp);
}

procps_status_t* procps_scan(procps_status_t* sp, int flags)
{
	struct dirent *entry;
    char buf[PROCPS_BUFSIZE];
    char filename[sizeof("/proc//cmdline") + sizeof(int)*3];
    char *filename_tail;
    long tasknice;
    unsigned pid;
    int n;
    struct stat sb;

    if (!sp)
    	sp = alloc_procps_scan();

    for (;;) {
    	entry = readdir(sp->dir);
        if (entry == NULL) {
        	free_procps_scan(sp);
            return NULL;
        }
        pid = bb_strtou(entry->d_name, NULL, 10);
        if (errno)
        	continue;

        /* After this point we have to break, not continue
                 * ("continue" would mean that current /proc/NNN
                 * is not a valid process info) */

        memset(&sp->vsz, 0, sizeof(*sp) - offsetof(procps_status_t, vsz));

        sp->pid = pid;
        if (!(flags & ~PSSCAN_PID)) break;

        filename_tail = filename + sprintf(filename, "/proc/%d", pid);

        if (flags & PSSCAN_UIDGID) {
        	if (stat(filename, &sb))
            	break;
        	/* Need comment - is this effective or real UID/GID? */
            sp->uid = sb.st_uid;
            sp->gid = sb.st_gid;
        }

        if (flags & PSSCAN_STAT) {
        	char *cp, *comm1;
            int tty;
            unsigned long vsz, rss;

            /* see proc(5) for some details on this */
            strcpy(filename_tail, "/stat");
            n = read_to_buf(filename, buf);
            if (n < 0)
            	break;
            cp = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
            /*if (!cp || cp[1] != ' ')
                    	break;*/
            cp[0] = '\0';
            if (sizeof(sp->comm) < 16)
            	BUG_comm_size();
            comm1 = strchr(buf, '(');
            /*if (comm1)*/
            strncpy(sp->comm, comm1 + 1, sizeof(sp->comm));

            n = sscanf( cp+2,
                        "%c %u "               /* state, ppid */
                        "%u %u %d %*s "        /* pgid, sid, tty, tpgid */
                        "%*s %*s %*s %*s %*s " /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
                        "%lu %lu "             /* utime, stime */
                        "%*s %*s %*s "         /* cutime, cstime, priority */
                        "%ld "                 /* nice */
                        "%*s %*s "             /* timeout, it_real_value */
                        "%lu "                 /* start_time */
                        "%lu "                 /* vsize */
                        "%lu "                 /* rss */
            			/*	"%lu %lu %lu %lu %lu %lu " rss_rlim, start_code, end_code, start_stack, kstk_esp, kstk_eip */
            			/*	"%u %u %u %u "         signal, blocked, sigignore, sigcatch */
            			/*	"%lu %lu %lu"          wchan, nswap, cnswap */
                        ,
                        sp->state, &sp->ppid,
                        &sp->pgid, &sp->sid, &tty,
                        &sp->utime, &sp->stime,
                        &tasknice,
                        &sp->start_time,
                        &vsz,
                        &rss);
			if (n != 11)
            	break;
            /* vsz is in bytes and we want kb */
            sp->vsz = vsz >> 10;
            /* vsz is in bytes but rss is in *PAGES*! Can you believe that? */
            sp->rss = rss << sp->shift_pages_to_kb;
            sp->tty_major = (tty >> 8) & 0xfff;
            sp->tty_minor = (tty & 0xff) | ((tty >> 12) & 0xfff00);

            if (sp->vsz == 0 && sp->state[0] != 'Z')
            	sp->state[1] = 'W';
            else
            	sp->state[1] = ' ';
            if (tasknice < 0)
            	sp->state[2] = '<';
            else if (tasknice) /* > 0 */
            	sp->state[2] = 'N';
            else
            	sp->state[2] = ' ';

		}

        if (flags & (PSSCAN_ARGV0|PSSCAN_ARGVN)) {
        	free(sp->argv0);
            sp->argv0 = NULL;
            strcpy(filename_tail, "/cmdline");
            n = read_to_buf(filename, buf);
            if (n <= 0)
            	break;
            if (flags & PSSCAN_ARGVN) {
            	sp->argv_len = n;
                sp->argv0 = malloc(n + 1);
                memcpy(sp->argv0, buf, n + 1);
                /* sp->argv0[n] = '\0'; - buf has it */
            } else {
            	sp->argv_len = 0;
                sp->argv0 = strdup(buf);
            }
        }
        break;
	}
    return sp;
}

pid_t* find_pid_by_name(const char *procName)
{
    pid_t* pidList;
    int i = 0;
    procps_status_t* p = NULL;

    pidList = xzalloc(sizeof(*pidList));
    while ((p = procps_scan(p, PSSCAN_PID|PSSCAN_COMM|PSSCAN_ARGVN))) {
    	if (comm_match(p, procName)
        	/* or we require argv0 to match (essential for matching reexeced /proc/self/exe)*/
            || (p->argv0 && strcmp(bb_basename(p->argv0), procName) == 0)
            /* TOOD: we can also try /proc/NUM/exe link, do we want that? */
            ) {
            	if (p->state[0] != 'Z')
                {
                	pidList = xrealloc_vector(pidList, 2, i);
                    pidList[i++] = p->pid;
                }
    	}
    }

    pidList[i] = 0;
    return pidList;
}

int pids(char *appname)
{
    pid_t *pidList;
    pid_t *pl;
    int count = 0;

    pidList = find_pid_by_name(appname);
    for (pl = pidList; *pl; pl++) {
    	count++;
    }
    free(pidList);

	if (count)
    	return 1;
   	else
    	return 0;
}
/*pids end*/

/*end #ifdef APP_IPKG*/
#endif 

const char base64_pad = '=';

/* "A-Z a-z 0-9 + /" maps to 0-63 */
const short base64_reverse_table[256] = {
/*	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x00 - 0x0F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x10 - 0x1F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, /* 0x20 - 0x2F */
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 0x30 - 0x3F */
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 0x40 - 0x4F */
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, /* 0x50 - 0x5F */
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 0x60 - 0x6F */
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, /* 0x70 - 0x7F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x80 - 0x8F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x90 - 0x9F */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xA0 - 0xAF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xB0 - 0xBF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xC0 - 0xCF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xD0 - 0xDF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xE0 - 0xEF */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0xF0 - 0xFF */
};

unsigned char * base64_decode(buffer *out, const char *in) {
        unsigned char *result;
        unsigned int j = 0; /* current output character (position) that is decoded. can contain partial result */
        unsigned int group = 0; /* how many base64 digits in the current group were decoded already. each group has up to 4 digits */
        size_t i;

        size_t in_len = strlen(in);

        buffer_string_prepare_copy(out, in_len);

        result = (unsigned char *)out->ptr;

        /* run through the whole string, converting as we go */
        for (i = 0; i < in_len; i++) {
                unsigned char c = (unsigned char) in[i];
                short ch;

                if (c == '\0') break;

                if (c == base64_pad) {
                        /* pad character can only come after 2 base64 digits in a group */
                        if (group < 2) return NULL;
                        break;
                }

                ch = base64_reverse_table[c];
                if (ch < 0) continue; /* skip invalid characters */

                switch(group) {
                case 0:
                        result[j] = ch << 2;
                        group = 1;
                        break;
                case 1:
                        result[j++] |= ch >> 4;
                        result[j] = (ch & 0x0f) << 4;
                        group = 2;
                        break;
                case 2:
                        result[j++] |= ch >>2;
                        result[j] = (ch & 0x03) << 6;
                        group = 3;
                        break;
                case 3:
                        result[j++] |= ch;
                        group = 0;
                        break;
                }
        }

        switch(group) {
        case 0:
                /* ended on boundary */
                break;
        case 1:
                /* need at least 2 base64 digits per group */
                return NULL;
        case 2:
                /* have 2 base64 digits in last group => one real octect, two zeroes padded */
        case 3:
                /* have 3 base64 digits in last group => two real octects, one zero padded */

                /* for both cases the current index already is on the first zero padded octet
                 * - check it really is zero (overlapping bits) */
                if (0 != result[j]) return NULL;
                break;
        }

        result[j] = '\0';
        out->used = j;

        return result;
}
//unsigned char * base64_decode(buffer *out, const char *in) {
//	unsigned char *result;
//	int ch, j = 0, k;
//	size_t i;

//	size_t in_len = strlen(in);
	
//	buffer_prepare_copy(out, in_len);

//	result = (unsigned char *)out->ptr;

//	ch = in[0];
//	/* run through the whole string, converting as we go */
//	for (i = 0; i < in_len; i++) {
//		ch = in[i];

//		if (ch == '\0') break;

//		if (ch == base64_pad) break;

//		ch = base64_reverse_table[ch];
//		if (ch < 0) continue;

//		switch(i % 4) {
//		case 0:
//			result[j] = ch << 2;
//			break;
//		case 1:
//			result[j++] |= ch >> 4;
//			result[j] = (ch & 0x0f) << 4;
//			break;
//		case 2:
//			result[j++] |= ch >>2;
//			result[j] = (ch & 0x03) << 6;
//			break;
//		case 3:
//			result[j++] |= ch;
//			break;
//		}
//	}
//	k = j;
//	/* mop things up if we ended on a boundary */
//	if (ch == base64_pad) {
//		switch(i % 4) {
//		case 0:
//		case 1:
//			return NULL;
//		case 2:
//			k++;
//		case 3:
//			result[k++] = 0;
//		}
//	}
//	result[k] = '\0';

//	out->used = k;

//	return result;
//}

char *ldb_base64_encode(const char *buf, int len)
{
	const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int bit_offset, byte_offset, idx, i;
	const uint8_t *d = (const uint8_t *)buf;
	int bytes = (len*8 + 5)/6, pad_bytes = (bytes % 4) ? 4 - (bytes % 4) : 0;
	char *out;

	out = (char *)calloc(1, bytes+pad_bytes+1);
	if (!out) return NULL;

	for (i=0;i<bytes;i++) {
		byte_offset = (i*6)/8;
		bit_offset = (i*6)%8;
		if (bit_offset < 3) {
			idx = (d[byte_offset] >> (2-bit_offset)) & 0x3F;
		} else {
			idx = (d[byte_offset] << (bit_offset-2)) & 0x3F;
			if (byte_offset+1 < len) {
				idx |= (d[byte_offset+1] >> (8-(bit_offset-2)));
			}
		}
		out[i] = b64[idx];
	}

	for (;i<bytes+pad_bytes;i++)
		out[i] = '=';
	out[i] = 0;

	return out;
}

char* get_mac_address(const char* iface, char* mac){
	int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;	
    strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);		
    ioctl(fd, SIOCGIFHWADDR, &ifr);
	close(fd);
    sprintf(mac,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
       		(unsigned char)ifr.ifr_hwaddr.sa_data[0],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[1],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[2],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[3],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[4],
         	(unsigned char)ifr.ifr_hwaddr.sa_data[5]);
	return mac;
}

char *replace_str(char *st, char *orig, char *repl, char* buff) {  
	char *ch;
	if (!(ch = strstr(st, orig)))
		return st;
	strncpy(buff, st, ch-st);  
	buff[ch-st] = 0;
	sprintf(buff+(ch-st), "%s%s", repl, ch+strlen(orig));  

	return buff;
}

//- 耞秨﹍竚 
int startposizition( char *str, int start )  
{  
	int i=0;            //-ノ璸计
    int posizition=0;   //- 竚
    int tempposi=start;    
    while(str[tempposi]<0)  
    {  
    	i++;  
        tempposi--;  
    }  
  
    if(i%2==0 && i!=0)  
    	posizition=start+1;   
    else   
    	posizition=start;     
    return posizition;  
} 

//- 耞ソ狠竚
int endposizition( char *str, int end )  
{  
	int i=0;            //-ノ璸计
	int posizition=0;   //- 竚
	int tempposi=end; 
	while(str[tempposi]<0)  
	{ 
		i++;  
	    tempposi--;  
	}  
 
	if(i%2==0 && i!=0)  
		posizition=end;   
	else  
	    posizition=end-1;     

 	return posizition;  
} 

void getStr( char *str, char *substr, int substrlen, int start, int end )  
{  
	int  i=0;	
	char* temp;
	temp = (char*)malloc(substrlen);
	memset(temp,0,sizeof(temp));  

	for( start; start<=end; start++ )  
    {  
       	temp[i]=str[start];  
        i++;  
    }  
    temp[i]='\0';  
	strcpy(substr,temp);  

	free(temp); 
} 

void  getSubStr( char *str, char *substr, int start, int end )  
{  
	int substrlen = end - start + 1;
    start = startposizition( str, start );  
    end   = endposizition( str, end );  
	getStr( str, substr, substrlen, start, end );     
}  
 
const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int extract_filename(const char* src, char** out){
	const char *splash = strrchr(src, '/');
    if(!splash || splash == src) return 0;
	int len = strlen(src) - (splash - src) + 1;
	*out = (char*)malloc(len+1);
	memset(*out, '\0', len+1);
	strcpy(*out, splash+1);	
    return 1;
}

int extract_filepath(const char* src, char** out){
	const char *splash = strrchr(src, '/');		
    if(!splash || splash == src) return 0;
	int len = splash - src + 1;	
	*out = (char*)malloc(len+1);
	memset(*out, '\0', len+1);
	strncpy(*out, src, len);	
	return 1;
}

int createDirectory(const char * path) {

	char* p;
	struct stat st;
  	for( p = strchr(path+1, '/'); p; p = strchr(p+1, '/') ) {
    	*p='\0';
		
		if( stat(path, &st) == -1 ){
		    if( mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO)==-1) {				
				return 0;
		    }
		}
    	*p='/';
  	}

	return 1;
	
	/*
  	char * buffer = malloc((strlen(path) + 10) * sizeof(char));
  	sprintf(buffer, "mkdir -p %s", path);
  	int result = system(buffer);
  	free(buffer);
  	return result;
  	*/
}

int smbc_wrapper_opendir(connection* con, const char *durl)
{
	int ret = -1;
	
	if(con->mode== SMB_BASIC){		
		ret = (int)smbc_opendir(durl);
	}
	else if(con->mode == SMB_NTLM){
		ret = (int)smbc_cli_opendir(con->smb_info->cli, durl);
	}
	
	return ret;
}

int smbc_wrapper_opensharedir(connection* con, const char *durl)
{
	int ret = -1;
	
	if(con->mode== SMB_BASIC){
		ret = (int)smbc_opendir(durl);
	}
	else if(con->mode == SMB_NTLM){
		char turi[512];
		sprintf(turi, "%s\\IPC$", durl);
		ret = (int)smbc_cli_open_share(con->smb_info->cli, turi);
	}
		
	return ret;
}

struct smbc_dirent* smbc_wrapper_readdir(connection* con, unsigned int dh)
{
	if(con->mode== SMB_BASIC)
		return smbc_readdir(dh);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_readdir(dh);

	return NULL;
}

int smbc_wrapper_closedir(connection* con, int dh)
{
	if(con->mode== SMB_BASIC)
		return smbc_closedir(dh);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_closedir(dh);

	return -1;
}

int smbc_wrapper_stat(connection* con, const char *url, struct stat *st)
{
	if(con->mode== SMB_BASIC){
		return smbc_stat(url, st);
	}
	else if(con->mode == SMB_NTLM){
		return smbc_cli_stat(con->smb_info->cli, url, st);
	}
	
	return -1;
	
}

int smbc_wrapper_unlink(connection* con, const char *furl)
{
	if(con->mode== SMB_BASIC)
		return smbc_unlink(furl);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_unlink(con->smb_info->cli, furl, 0);

	return -1;
}

uint32_t smbc_wrapper_rmdir(connection* con, const char *dname)
{
	if(con->mode== SMB_BASIC)
		return smbc_rmdir(dname);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_rmdir(con->smb_info->cli, dname);

	return 0;
}

uint32_t smbc_wrapper_mkdir(connection* con, const char *fname, mode_t mode)
{
	if(con->mode== SMB_BASIC)
		return smbc_mkdir(fname, mode);
	else if(con->mode == SMB_NTLM){
		smb_file_t *fp;
		
		if((fp = smbc_cli_ntcreate(con->smb_info->cli, fname,
						FILE_READ_DATA | FILE_WRITE_DATA, FILE_CREATE, FILE_DIRECTORY_FILE)) == NULL)
		{
			return -1;
		}
		else{
			smbc_cli_close(con->smb_info->cli, fp);
		}
	}
	
	return 0;
}

uint32_t smbc_wrapper_rename(connection* con, char *src, char *dst)
{
	if(con->mode== SMB_BASIC)
		return smbc_rename(src, dst);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_rename(con->smb_info->cli, src, dst);

	return 0;
}

int smbc_wrapper_open(connection* con, const char *furl, int flags, mode_t mode)
{
	if(con->mode== SMB_BASIC)
		return smbc_open(furl, flags, mode);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_open(con->smb_info->cli, furl, flags);

	return -1;
}

int smbc_wrapper_create(connection* con, const char *furl, int flags, mode_t mode)
{
	if(con->mode== SMB_BASIC)
		return smbc_creat(furl, mode);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_ntcreate(con->smb_info->cli, furl, flags, FILE_CREATE, mode);

	return -1;
}

int smbc_wrapper_close(connection* con, int fd)
{
	if(con->mode== SMB_BASIC)
		return smbc_close(fd);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_close(con->smb_info->cli, fd);
	
	return -1;
}

size_t smbc_wrapper_read(connection* con, int fd, void *buf, size_t bufsize)
{
	if(con->mode== SMB_BASIC)
		return smbc_read(fd, buf, bufsize);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_read(con->smb_info->cli, fd, buf, bufsize);
	
	return -1;
}

size_t smbc_wrapper_write(connection* con, int fd, const void *buf, size_t bufsize, uint16_t write_mode )
{
	if(con->mode== SMB_BASIC)
		return smbc_write(fd, buf, bufsize);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_write(con->smb_info->cli, fd, write_mode, buf, bufsize);
	
	return -1;
}

off_t
smbc_wrapper_lseek(connection* con, int fd, off_t offset, int whence)
{
   if(con->mode== SMB_BASIC)
		return smbc_lseek(fd, offset,whence);
	else if(con->mode == SMB_NTLM)
		return smbc_cli_lseek(con->smb_info->cli, fd, offset,whence);
	
	return -1;
}
int smbc_wrapper_parse_path(connection* con, char *pWorkgroup, char *pServer, char *pShare, char *pPath){
	if(con->mode== SMB_BASIC||con->mode== SMB_NTLM){ 

		smbc_parse_path(con->physical.path->ptr, pWorkgroup, pServer, pShare, pPath);
		
		//- Jerry add: replace '\\' to '/'
		do{
			char buff[4096];
			strcpy( pPath, replace_str(&pPath[0],"\\","/", (char *)&buff[0]) );
		}while(strstr(pPath,"\\")!=NULL);
	}
	
	return 1;
}

int smbc_wrapper_parse_path2(connection* con, char *pWorkgroup, char *pServer, char *pShare, char *pPath){	 
	if(con->mode== SMB_BASIC||con->mode== SMB_NTLM){

		smbc_parse_path(con->physical_auth_url->ptr, pWorkgroup, pServer, pShare, pPath);

		int len = strlen(pPath)+1;
		
		char* buff = (char*)malloc(len);
		memset(buff, '\0', len);
		
		do{	
			strcpy( pPath, replace_str(&pPath[0],"\\","/", buff) );
		}while(strstr(pPath,"\\")!=NULL);
		
		free(buff);
	}

	return 1;
}

int smbc_parse_mnt_path(
	const char* physical_path,
	const char* mnt_path,
    int mnt_path_len,
    char** usbdisk_path,
    char** usbdisk_share_folder){

	char* aa = physical_path + mnt_path_len;
	//Cdbg(1, "physical_path = %s, aa=%s", physical_path ,aa);
	char* bb = strstr(aa, "/");
	char* cc = NULL;
	//Cdbg(1, "bb=%s", bb);
	int len = 0;
	int len2 = 0;
	if(bb) {
		len=bb - aa;
		//Cdbg(1, "bb=%s", bb);
		cc = strstr(bb+1, "/");
		//Cdbg(1, "cc=%s", cc);
		if(cc) len2 = cc - bb;
		else len2 = strlen(bb);
	}
	else 
		len = ( physical_path + strlen(physical_path) ) - aa;
	//Cdbg(1, "strlen(physical_path)=%d", strlen(physical_path));
	*usbdisk_path = (char*)malloc(len+mnt_path_len+1);	
	memset( *usbdisk_path, '\0', len+mnt_path_len+1);	
	strncpy( *usbdisk_path, physical_path, len+mnt_path_len);
	//Cdbg(1, "usbdisk_path=%s, len=%d, mnt_path_len=%d", *usbdisk_path, len, mnt_path_len);
	if(bb){
		*usbdisk_share_folder= (char*)malloc(len2+1);
		memset( *usbdisk_share_folder, '\0', len2+1);
		strncpy(*usbdisk_share_folder, bb+1, len2-1);
	}
	return 1;
}

int isBrowser(connection *con){
	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");
	return ( ds && (strstr( ds->value->ptr, "Mozilla" ) || strstr( ds->value->ptr, "Opera" ))) ? 1 : 0;
}

void smbc_wrapper_response_401(server *srv, connection *con)
{
	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");
		
	//- Browser response
	if( ds && (strstr( ds->value->ptr, "Mozilla" )||strstr( ds->value->ptr, "Opera" )) ){
		if(con->mode == SMB_BASIC||con->mode == DIRECT){
			Cdbg(DBE, "con->mode == SMB_BASIC -> return 401");
			con->http_status = 401;
			return;
		}
	}
	
	Cdbg(DBE, "smbc_wrapper_response_401 -> return 401");
	
	char str[50];

	UNUSED(srv);

	buffer* tmp_buf = buffer_init();	
	if(con->mode == SMB_BASIC){
		//sprintf(str, "Basic realm=\"%s\"", "smbdav");
		
		if(con->smb_info&&con->smb_info->server->used)
			sprintf(str, "Basic realm=\"smb://%s\"", con->smb_info->server->ptr);
		else
			sprintf(str, "Basic realm=\"%s\"", "webdav");
	}
	else if(con->mode == SMB_NTLM)
		sprintf(str, "NTLM");
	else
		sprintf(str, "Basic realm=\"%s\"", "webdav");
	
	buffer_copy_string(tmp_buf, str);
	
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(tmp_buf));
	con->http_status = 401;
		
	buffer_free(tmp_buf);
}

void smbc_wrapper_response_realm_401(server *srv, connection *con)
{
/*
	if(con->mode == SMB_BASIC){
		if(con->smb_info&&con->smb_info->server->used){
			Cdbg(DBE, "sssssssss");
			con->http_status = 401;
		}
		return;
	}
	*/
	char str[50];

	UNUSED(srv);

	buffer* tmp_buf = buffer_init();	
	if(con->mode == SMB_BASIC){
		//sprintf(str, "Basic realm=\"%s\"", "smbdav");
		
		if(con->smb_info&&con->smb_info->server->used)
			sprintf(str, "Basic realm=\"smb://%s\"", con->smb_info->server->ptr);
		else
			sprintf(str, "Basic realm=\"%s\"", "webdav");
		
	}
	else if(con->mode == SMB_NTLM)
		sprintf(str, "NTLM");
	else
		sprintf(str, "Basic realm=\"%s\"", "webdav");
	
	buffer_copy_string(tmp_buf, str);
	
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(tmp_buf));
	con->http_status = 401;
		
	buffer_free(tmp_buf);
}

void smbc_wrapper_response_404(server *srv, connection *con)
{
	char str[50];

	UNUSED(srv);
	/*
	buffer* tmp_buf = buffer_init();

	if(con->mode == SMB_BASIC)
		sprintf(str, "Basic realm=\"%s\"", "smbdav");
	else if(con->mode == SMB_NTLM)
		sprintf(str, "NTLM");
		
	buffer_copy_string(tmp_buf, str);
	response_header_insert(srv, con, CONST_STR_LEN("WWW-Authenticate"), CONST_BUF_LEN(tmp_buf));
	*/
	con->http_status = 404;
	//buffer_free(tmp_buf);
}

buffer* smbc_wrapper_physical_url_path(server *srv, connection *con)
{	
	if(con->mode==SMB_BASIC||con->mode==SMB_NTLM){
		return con->url.path;
	}
	return con->physical.path;	
}

int is_acc_account_existed(const char *username){
	
	char *nvram_acc_list;
	int account_existed = 0;
	int len = 0;
	
#if EMBEDDED_EANBLE
	char *acc_list = nvram_get_acc_list();
	if(acc_list==NULL)
		return 0;
	
	len = strlen(acc_list);
	nvram_acc_list = (char *)malloc(sizeof(char)*(len+1));
	if(nvram_acc_list==NULL)
		return 0;
	
	memset(nvram_acc_list, 0, len+1);
	strncpy(nvram_acc_list, acc_list, len);

	#ifdef APP_IPKG
	free(acc_list);
	#endif
#else
	nvram_acc_list = (char*)malloc(100);
	if(nvram_acc_list==NULL)
		return 0;
	
	strcpy(nvram_acc_list, "admin>admin<jerry>jerry");
#endif
	
	if(nvram_acc_list!=NULL){

		buffer* buffer_name = buffer_init();
		char * pch = strtok(nvram_acc_list, "<>");
		
		while(pch!=NULL){
			//- User Name
			len = strlen(pch);			
			buffer_copy_string_len(buffer_name, pch, len);
			buffer_urldecode_path(buffer_name);
			
			if(buffer_is_equal_string(buffer_name, username, strlen(username))){
				account_existed = 1;
				break;
			}
			
			//- User Password
			pch = strtok(NULL,"<>");

			//- next
			pch = strtok(NULL,"<>");
		}
		
		free(nvram_acc_list);
		buffer_free(buffer_name);
	}

	return account_existed;
}

int is_aicloud_account_existed(const char *username){
	aicloud_acc_info_t* c;
	for (c = aicloud_acc_info_list; c; c = c->next) {

		if(buffer_is_equal_string(c->username, username, strlen(username))){
			return 1;
		}
	}

	return 0;
}

int is_account_existed(const char *username){
	if(is_acc_account_existed(username)==1){
		return 1;
	}

	if(is_aicloud_account_existed(username)==1){
		return 1;
	}

	return 0;
}

account_type smbc_get_account_type(const char* user_name){

	account_type var_type;
	
	if(is_acc_account_existed(user_name)==1 || strncmp(user_name, "guest", 5)==0){
		var_type = T_ACCOUNT_SAMBA;
	}
	else if(is_aicloud_account_existed(user_name)==1){
		var_type = T_ACCOUNT_AICLOUD;
	}
	else
		return -1;
	
	return var_type;
	
}

account_permission_type smbc_get_account_permission(const char* user_name){

#if EMBEDDED_EANBLE
	if(strcmp( user_name, nvram_get_http_username())==0){
		return T_ADMIN;
	}
#else
	if(strcmp( user_name, "admin")==0){
		return T_ADMIN;
	}
#endif

	return T_USER;
}

int smbc_get_usbdisk_permission(const char* user_name, const char* usbdisk_rel_sub_path, const char* usbdisk_sub_share_folder)
{
	int permission = -1;
	account_type var_type = smbc_get_account_type(user_name);
	
	if(var_type == T_ACCOUNT_SAMBA){
#if EMBEDDED_EANBLE
		int st_samba_mode = nvram_get_st_samba_mode();
		
		if( (st_samba_mode==1) ||
		    (user_name!=NULL && strncmp(user_name, "guest", 5)==0))
			permission = 3;
		else{
			permission = get_permission( user_name,
									     usbdisk_rel_sub_path,
									     usbdisk_sub_share_folder,
									     "cifs");
		}
		
		Cdbg(DBE, "usbdisk_rel_sub_path=%s, usbdisk_sub_share_folder=%s, permission=%d, user_name=%s", 
					usbdisk_rel_sub_path, usbdisk_sub_share_folder, permission, user_name);				
	
#endif
	}
	else if(var_type == T_ACCOUNT_AICLOUD){

#if EMBEDDED_EANBLE
	
		permission = get_aicloud_permission( user_name,
											 usbdisk_rel_sub_path,
											 usbdisk_sub_share_folder);
#endif
	}
	
	return permission;
}

int hex2ten(const char* in_str )
{	
	int ret;

	if( in_str == (char*)'0' )
		ret = 0;
	else if( in_str == (char*)'1' )
		ret = 1;
	else if( in_str == (char*)'2' )
		ret = 2;
	else if( in_str == (char*)'3' )
		ret = 3;
	else if( in_str == (char*)'4' )
		ret = 4;
	else if( in_str == (char*)'5' )
		ret = 5;
	else if( in_str == (char*)'6' )
		ret = 6;
	else if( in_str == (char*)'7' )
		ret = 7;
	else if( in_str == (char*)'8' )
		ret = 8;
	else if( in_str == (char*)'9' )
		ret = 9;
	else if( in_str == (char*)'A' || in_str==(char*)'a' )
		ret = 10;
	else if( in_str==(char*)'B' || in_str==(char*)'b' )
		ret = 11;
	else if( in_str==(char*)'C' || in_str==(char*)'c' )
		ret = 12;
	else if( in_str==(char*)'D' || in_str==(char*)'d' )
		ret = 13;
	else if( in_str==(char*)'E' || in_str==(char*)'e' )
		ret = 14;
	else if( in_str==(char*)'F' || in_str==(char*)'f' )
		ret = 15;
	else
		ret = 0;
	
	//Cdbg(1, "12121212, in_str=%c, ret=%d", in_str, ret);
	return ret;
}

void ten2bin(int ten, char** out)
{
	char _array[5]="\0";
	_array[0] = '0';    	
	_array[1] = '0';    	
	_array[2] = '0';    	
	_array[3] = '0'; 
	int _count = 3;

	while( ten > 0 ){
		//Cdbg(1, "_count=%d", _count);
		
		if( ( ten%2 ) == 0 )
			_array[_count] = '0';
		else
			_array[_count] = '1';
		_count--;	    
		ten = ten/2;
	}

	*out = (char*)malloc(5);
	memset(*out, '\0', 5);
	strcpy(*out, _array);
}

void hexstr2binstr(const char* in_str, char** out_str)
{
	char *tmp;
	buffer* ret = buffer_init();
	for (tmp = in_str; *tmp; ++tmp){
		
		int ten = hex2ten( (char*)(*tmp) );
				
		char* binStr;
		ten2bin(ten, &binStr);
		//Cdbg(1, "444, binStr=%s", binStr);
		
		buffer_append_string_len(ret, binStr, 4);

		free(binStr);
	}

	*out_str = (char*)malloc(ret->size+1);
	strcpy(*out_str, ret->ptr);
	buffer_free(ret);
	
}

void ten2hex(int  _v, char** out )
{	
	*out = (char*)malloc(5);
	memset(*out, '\0', 5);
	sprintf(*out, "%x", _v);
}

void binstr2hexstr( const char* in_str, char** out_str )
{	
	buffer* ret = buffer_init();	
	if( 0 != strlen(in_str) % 4 )	{
		buffer_free(ret);
		return;	
	}

	buffer_copy_string(ret, "");
	
	int ic = strlen(in_str) / 4;

	int i = 0;
	int offset = 0;
	for(i = 0; i < ic; i++)
	{		
		char t[5] = "\0";
		offset = i * 4;

		strncpy(t, in_str + offset, 4);
	
		int uc = bin2ten( t );
		
		//- ten to hex
		char* hex;
		ten2hex(uc, &hex);
		buffer_append_string(ret, hex);
		free(hex);
	}		

	*out_str = (char*)malloc(ret->size+1);
	strcpy(*out_str, ret->ptr);

	buffer_free(ret);
}

void deinterleave( char* src, int charcnt, char** out )
{	
	int src_len = strlen(src);
	int ic = src_len / charcnt;	
	buffer* ret = buffer_init();

	for( int i = 0; i < charcnt; i++ )	
	{	
		for( int j = 0; j < ic; j++ )		
		{			
			int x = (j*charcnt) + i;
			
			char tmps[8]="\0";
			sprintf(tmps, "%c", *(src+x));

			//Cdbg(1, "tmps =  %s", tmps);
			buffer_append_string(ret, tmps);
		}	
		
	}

	*out = (char*)malloc(ret->size+1);
	strcpy(*out, ret->ptr);

	buffer_free(ret);
}

void interleave( char* src, int charcnt, char** out )
{	
	int src_len = strlen(src);
	int ic = src_len / charcnt;	
	buffer* ret = buffer_init();

	for( int i = 0; i < ic; i++ )	{		
		for( int j = 0; j < charcnt; j++ ) {			
		
			int x = (j * ic) + i;
			
			char tmps[8]="\0";
			sprintf(tmps, "%c", *(src+x));

			//Cdbg(DBE, "tmps =  %s", tmps);
			buffer_append_string(ret, tmps);
		}	
	}
	
	*out = (char*)malloc(ret->size+1);
	strcpy(*out, ret->ptr);

	buffer_free(ret);
}

void str2hexstr(char* str, char* hex)
{
	const char* cHex = "0123456789ABCDEF";
	int i=0;
	for(int j =0; j < strlen(str); j++)
	{
		unsigned int a = (unsigned int) str[j];
		hex[i++] = cHex[(a & 0xf0) >> 4];
		hex[i++] = cHex[(a & 0x0f)];
	}
	hex = '\0';
}

int sumhexstr( const char* str )
{		
	//- e.g:  str = 4D44, ret = 4*16*16*16 + D*16*16 + 4*16*16 + 4*1
	
	int ret = 0;		
	int ic = strlen(str) / 4.0;

	int i = 0;	
	int offset = 0;
	for(i = 0; i < ic; i++ )	
	{		
		char t[5] = "\0";
		offset = i * 4;

		strncpy(t, str + offset, 4);

		int j=0;
		int y = 0;
		
		char *tmp;	
		for (tmp = t; *tmp; ++tmp){
			int pow = 1;
			for( int k = 0; k < strlen(t) - j - 1; k++ )
				pow *=16;
			
			int x = hex2ten( *tmp ) * pow;
			y += x;
			j++;
		}
	
		ret += y;	
	}			
	
	//Cdbg(DBE, "ret=%d", ret);
	
	return ret;
}

int getshiftamount( const char* seed_str, const char* orignaldostr )
{	
	//- e.g {M,D,A,6,M,G,M,6,M,j,k,6,M,m,M,6,M,T,A,6,M,j,k,=}
	//- => {4D,44,41,36,4D,47,4D,36,4D,6A,6B,36,4D,6D,4D,36,4D,54,41,36,4D,6A,6B,3D}
	char seed_hex_str[100]="\0";
	str2hexstr( seed_str, &seed_hex_str );
	//Cdbg(DBE, "getshiftamount: seed_hex_str=%s", seed_hex_str);
	
	//- sum hexstring, every 4digit
	//- e.g {0x4D44 + 0x4136 + ...} = 246635;	
	int sumhex = sumhexstr( seed_hex_str );		
	//Cdbg(DBE, "getshiftamount: sumhex=%d", sumhex);
	
	//- limit shift value to lenght of seed	
	int shiftamount = sumhex % strlen(orignaldostr);
	//Cdbg(1, "sumhex=%d, getshiftamount=%d", sumhex, strlen(orignaldostr));

	//- ensure there is a shiftamount;	
	if( shiftamount == 0 )		
		shiftamount = 15;	
	
	return shiftamount;
}

void strleftshift( const char* str, int shiftamount, char** out )
{	
	// e.g strleftshift("abcdef", 2) => "cdefab"
	
	int str_len = strlen(str);

	int right_buffer_size = str_len-shiftamount+1;
	char* right_buffer = (char*)malloc(right_buffer_size);
	memset(right_buffer, 0, right_buffer_size);
	strncpy(right_buffer, str+shiftamount, right_buffer_size-1);

	int left_buffer_size = shiftamount+1;
	char *left_buffer = (char*)malloc(left_buffer_size);
	memset(left_buffer, 0, left_buffer_size);
	strncpy(left_buffer, str, left_buffer_size-1);
	
	int size = strlen(right_buffer)+strlen(left_buffer)+1;
	*out = (char*)malloc(size);
	memset(*out, 0, size);
	strncpy(*out , right_buffer, strlen(right_buffer));
	strncat(*out, left_buffer, strlen(left_buffer));

	free(right_buffer);
	free(left_buffer);
}

void strrightshift( const char* str, int shiftamount, char** out )
{	
	// e.g strrightshift("abcdef", 2) => "efabcd"
		
	int str_len = strlen(str);
		
	int right_buffer_size = shiftamount+1;
	char* right_buffer = (char*)malloc(right_buffer_size);
	memset(right_buffer, 0, right_buffer_size);
	strncpy(right_buffer, str+(str_len-shiftamount), right_buffer_size-1);
	
	int left_buffer_size = str_len-shiftamount+1;
	char *left_buffer = (char*)malloc(left_buffer_size);
	memset(left_buffer, 0, left_buffer_size);
	strncpy(left_buffer, str, left_buffer_size-1);
	
	int size = strlen(right_buffer)+strlen(left_buffer)+1;
	*out = (char*)malloc(size);
	memset(*out, 0, size);
	strncpy(*out , right_buffer, strlen(right_buffer));
	strncat(*out, left_buffer, strlen(left_buffer));
	
	free(right_buffer);
	free(left_buffer);
}

int bin2ten( const char* _v )
{	
	int ret = 0;
	for( int j = 0; j < strlen(_v); j++ )	{	
		int pow = 1;
		for( int k = 0; k < strlen(_v) - j - 1; k++ )
			pow *=2;

		char tmp[8]="\0";
		strncpy(tmp, _v+j, 1);

		ret += atoi(tmp)*pow;
	}	
	return ret;
}

void binstr2str( const char* inbinstr, char** out )
{	
	//玡璶干0	
	if( 0 != strlen(inbinstr) % 8 )
	{		
		return;
	}
	
	int ic = strlen(inbinstr) / 8;	
	int k = 0;	
	int offset = 0;

	*out = (char*)malloc(ic+1);
	memset(*out , '\0', ic);
	
	for( int i = 0; i < strlen(inbinstr); i+=8 )	
	{				
		//玡じ	
		char substrupper[5] = "\0";
		offset = i;
		strncpy(substrupper, inbinstr + offset, 4);
		int uc = bin2ten( substrupper );
		Cdbg(DBE, "substrupper=%s, uc=%d", substrupper, uc);
		
		//じ
		char substrlowwer[5] = "\0";
		offset = i+4;
		strncpy(substrlowwer, inbinstr + offset, 4);		
		int lc = bin2ten( substrlowwer );
		Cdbg(DBE, "substrlowwer=%s, lc=%d", substrlowwer, lc);
		
		int v = (uc << 4) | lc;	
		
		char out_char[8];
		sprintf(out_char, "%c", v);
		
		strcat(*out, out_char);											
		k++;
	}

}

void dec2Bin(int dec, char** out) {
	
	char _array[8] = "00000000";
	int _count = 7;

	while( dec > 0 ){
		//Cdbg(1, "_count=%d", _count);
		
		if( ( dec%2 ) == 0 )
			_array[_count] = '0';
		else
			_array[_count] = '1';
		_count--;	    
		dec = dec/2;
	}

	*out = (char*)malloc(8);
	memset(*out, '\0', 8);
	strncpy(*out, _array, 8);
}

void str2binstr(const char* instr, char** out)
{	
	buffer* b = buffer_init();
	int last = strlen(instr);	
	for (int i = 0; i < last; i++) {

		char tmp[8]="\0";
		strncpy(tmp, instr + i, 1);
		
		int acsii_code = ((int)*tmp) & 0x000000ff;
		if (acsii_code < 128){
			char* binStr;
			dec2Bin(acsii_code, &binStr);

			//Cdbg(1, "%s -> %d %d, binStr=%s", tmp, *tmp, acsii_code, binStr);
			
			buffer_append_string_len(b, binStr, 8);
			//Cdbg(1, "b=%s", b->ptr);

			free(binStr);
		}
		
		
	}		

	//Cdbg(1, "11111111111111 %d %d %d", b->used, last*8, b->size+1);

	int len = last*8 + 1;
	*out = (char*)malloc(len);
	memset(*out, '\0', len);
	strcpy(*out, b->ptr);
	buffer_free(b);
}

char* x123_decode(const char* in_str, const char* key, char* out_str)
{
	Cdbg(DBE, "in_str=%s, key=%s", in_str, key);

	char* ibinstr;
	hexstr2binstr( in_str, &ibinstr );
	Cdbg(DBE, "ibinstr=%s", ibinstr);

	char* binstr;
	deinterleave( ibinstr, 8, &binstr );
	Cdbg(DBE, "deinterleave: binstr=%s", binstr);

	int shiftamount = getshiftamount( key, binstr );
	Cdbg(DBE, "getshiftamount: shiftamount=%d, key=%s", shiftamount, key);

	char* unshiftbinstr;
	strleftshift( binstr, shiftamount, &unshiftbinstr );
	Cdbg(DBE, "strleftshift: unshiftbinstr=%s", unshiftbinstr);

	binstr2str(unshiftbinstr, &out_str);
	Cdbg(DBE, "out_str=%s", out_str);

	free(ibinstr);
	free(binstr);
	free(unshiftbinstr);
	
	return out_str;
}

char* x123_encode(const char* in_str, const char* key, char* out_str)
{
	//Cdbg(DBE, "in_str=%s, key=%s", in_str, key);

	char* ibinstr;
	str2binstr( in_str, &ibinstr );
	//Cdbg(DBE, "ibinstr = %s", ibinstr);

	int shiftamount = getshiftamount( key, ibinstr );
	//Cdbg(DBE, "shiftamount = %d", shiftamount);

	char* unshiftbinstr = NULL;
	strrightshift( ibinstr, shiftamount, &unshiftbinstr );
	//Cdbg(DBE, "unshiftbinstr = %s", unshiftbinstr);

	char* binstr;
	interleave( unshiftbinstr, 8, &binstr );
	//Cdbg(DBE, "binstr = %s", binstr);

	char* hexstr;
	binstr2hexstr(binstr, &hexstr);
	//Cdbg(DBE, "hexstr = %s", hexstr);

	int out_len = strlen(hexstr)+9;
	//Cdbg(DBE, "out_len = %d, %d", out_len, strlen(hexstr));
	
	out_str = (char*)malloc(out_len);
	memset(out_str , '\0', out_len);
	strcpy(out_str, "ASUSHARE");
	strcat(out_str, hexstr);

	free(unshiftbinstr);
	free(ibinstr);
	free(binstr);
	free(hexstr);
	
	return out_str;
}

void md5String(const char* input1, const char* input2, char** out)
{
	int i;

	buffer* b = buffer_init();
	unsigned char signature[16];
	MD5_CTX ctx;
	MD5_Init(&ctx);

	if(input1){
		//Cdbg(1, "input1 %s", input1);
		MD5_Update(&ctx, input1, strlen(input1));	
	}
	
	if(input2){
		//Cdbg(1, "input2 %s", input2);
		MD5_Update(&ctx, input2, strlen(input2));	
	}
	
	MD5_Final(signature, &ctx);
	char tempstring[2];

	for(i=0; i<16; i++){
		int x = signature[i]/16;
		sprintf(tempstring, "%x", x);
		buffer_append_string(b, tempstring);
		
		int y = signature[i]%16;
		sprintf(tempstring, "%x", y);
		buffer_append_string(b, tempstring);		
	}

	//Cdbg(1, "result %s", b->ptr);
	
	int len = b->used + 1;
	*out = (char*)malloc(len);
	memset(*out, '\0', len);
	strcpy(*out, b->ptr);
	buffer_free(b);

}

int smbc_parser_basic_authentication(server *srv, connection* con, char** username,  char** password){
	
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return 0;

	data_string* ds = (data_string *)array_get_element(con->request.headers, "Authorization");

	if(con->share_link_basic_auth->used){
		ds = data_string_init();
		buffer_copy_buffer( ds->value, con->share_link_basic_auth );
		buffer_reset(con->share_link_basic_auth);
	}
	
	if (ds != NULL) {
		char *http_authorization = NULL;
		http_authorization = ds->value->ptr;
		
		if(strncmp(http_authorization, "Basic ", 6) == 0) {
			buffer *basic_msg = buffer_init();
			buffer *user = buffer_init();
			buffer *pass = buffer_init();
					
			if (!base64_decode(basic_msg, &http_authorization[6])) {
				log_error_write(srv, __FILE__, __LINE__, "sb", "decodeing base64-string failed", basic_msg);
				buffer_free(basic_msg);
				free(user);
				free(pass);							
				return 0;
			}
			char *s, bmsg[1024] = {0};

			//fetech the username and password from credential
			memcpy(bmsg, basic_msg->ptr, basic_msg->used);
			s = strchr(bmsg, ':');
			bmsg[s-bmsg] = '\0';

			buffer_copy_string(user, bmsg);
			buffer_copy_string(pass, s+1);
			
			*username = (char*)malloc(user->used);
			strcpy(*username, user->ptr);

			*password = (char*)malloc(pass->used);
			strcpy(*password, pass->ptr);
			
			buffer_free(basic_msg);
			free(user);
			free(pass);

			Cdbg(DBE, "base64_decode=[%s][%s]", *username, *password);
			
			return 1;
		}
	}
	else
		Cdbg(DBE, "smbc_parser_basic_authentication: ds == NULL, %s", con->url.path->ptr);
	
	return 0;
}

const char* g_temp_sharelink_file = "/tmp/sharelink";

int generate_sharelink(server* srv,
			               connection *con,
			               const char* filename, 
			               const char* url, 
			               const char* base64_auth, 
			               int expire, 
			               int toShare,
			               buffer** out){

	if(filename==NULL||url==NULL||base64_auth==NULL)
		return 0;

#if 1
	if(toShare==1){
		char * pch;
		pch = strtok(filename, ";");
		
		*out = buffer_init();
				
		while(pch!=NULL){
					
			char share_link[1024];
			struct timeval tv;
			unsigned long long now_utime;
			gettimeofday(&tv,NULL);
			now_utime = tv.tv_sec * 1000000 + tv.tv_usec;  
			sprintf( share_link, "AICLOUD%d", abs(now_utime));
		
			share_link_info_t *share_link_info;
			share_link_info = (share_link_info_t *)calloc(1, sizeof(share_link_info_t));
									
			share_link_info->shortpath = buffer_init();
			buffer_copy_string(share_link_info->shortpath, share_link);
									
			share_link_info->realpath = buffer_init();
			buffer_copy_string(share_link_info->realpath, url);
			buffer_urldecode_path(share_link_info->realpath);
				
			share_link_info->filename = buffer_init();
			buffer_copy_string(share_link_info->filename, pch);
			buffer_urldecode_path(share_link_info->filename);
			
			share_link_info->auth = buffer_init();
			buffer_copy_string(share_link_info->auth, base64_auth);
		
			share_link_info->createtime = time(NULL);
			share_link_info->expiretime = (expire==0 ? 0 : share_link_info->createtime + expire);
			share_link_info->toshare = toShare;
					
			DLIST_ADD(share_link_info_list, share_link_info);
					
			buffer_append_string(*out, share_link);
			buffer_append_string_len(*out, CONST_STR_LEN("/"));
			buffer_append_string(*out, pch);
					
			//- Next
			pch = strtok(NULL,";");
		
			if(pch)
				buffer_append_string_len(*out, CONST_STR_LEN(";"));
		
			log_sys_write(srv, "sbsbss", "Create share link", share_link_info->realpath , "/", share_link_info->filename, "from ip", con->dst_addr_buf->ptr);
		
		}
	}
	else{
		char * pch;
		pch = strtok(filename, ";");
		
		*out = buffer_init();
		
		char strTime[20]="\0";
		char mac[20]="\0";
		
	#if EMBEDDED_EANBLE
		#ifdef APP_IPKG
        char *router_mac=nvram_get_router_mac();
        sprintf(mac,"%s",router_mac);
        free(router_mac);
		#else
		char *router_mac=nvram_get_router_mac();
		strcpy(mac, (router_mac==NULL) ? "00:12:34:56:78:90" : router_mac);
		#endif
	#else
		get_mac_address("eth0", &mac);
	#endif
				
		char* base64_key = ldb_base64_encode(mac, strlen(mac));
		
		expire = 86400; //- 24hr
		unsigned long expiretime = (expire==0 ? 0 : time(NULL) + expire);		
		sprintf(strTime, "%lu", expiretime);
		
		char* share_link;
		char* urlstr = (char*)malloc(strlen(url) +  strlen(base64_auth) + strlen(strTime) + 15);
		sprintf(urlstr, "%s?auth=%s&expire=%s", url, base64_auth, strTime);
		Cdbg(DBE, "urlstr=%s", urlstr);
		share_link = x123_encode(urlstr, base64_key, &share_link);
		Cdbg(DBE, "share_link=%s", share_link);
		
		while(pch!=NULL){
			buffer_append_string(*out, share_link);
			buffer_append_string_len(*out, CONST_STR_LEN("/"));
			buffer_append_string(*out, pch);
			
			//- Next
			pch = strtok(NULL,";");

			if(pch)
				buffer_append_string_len(*out, CONST_STR_LEN(";"));
		}

		//Cdbg(1, "out=%s", (*out)->ptr);
		
		if(base64_key){
			free(base64_key);
			base64_key=NULL;
		}

		if(urlstr){
			free(urlstr);
			urlstr=NULL;
		}

		if(share_link){
			free(share_link);
			share_link=NULL;
		}
		
		//share_link = x123_encode("sambapc/sharefolder", "abcdefg", &share_link);
		//char* decode_str;
		//decode_str = x123_decode(encode_str, ldb_base64_encode(mac, strlen(mac)), &decode_str);
		//Cdbg(DBE, "do HTTP_METHOD_GSL decode_str=%s", decode_str);
	}
#else

#if 1	
	char * pch;
	pch = strtok(filename, ";");
	
	*out = buffer_init();
			
	while(pch!=NULL){
				
		char share_link[1024];
		struct timeval tv;
		unsigned long long now_utime;
		gettimeofday(&tv,NULL);
		now_utime = tv.tv_sec * 1000000 + tv.tv_usec;  
		sprintf( share_link, "AICLOUD%d", abs(now_utime));
	
		share_link_info_t *share_link_info;
		share_link_info = (share_link_info_t *)calloc(1, sizeof(share_link_info_t));
								
		share_link_info->shortpath = buffer_init();
		buffer_copy_string(share_link_info->shortpath, share_link);
								
		share_link_info->realpath = buffer_init();
		buffer_copy_string(share_link_info->realpath, url);
		buffer_urldecode_path(share_link_info->realpath);
			
		share_link_info->filename = buffer_init();
		buffer_copy_string(share_link_info->filename, pch);
		buffer_urldecode_path(share_link_info->filename);
		
		share_link_info->auth = buffer_init();
		buffer_copy_string(share_link_info->auth, base64_auth);
	
		share_link_info->createtime = time(NULL);
		share_link_info->expiretime = (expire==0 ? 0 : share_link_info->createtime + expire);
		share_link_info->toshare = toShare;
				
		DLIST_ADD(share_link_info_list, share_link_info);
				
		buffer_append_string(*out, share_link);
		buffer_append_string_len(*out, CONST_STR_LEN("/"));
		buffer_append_string(*out, pch);
				
		//- Next
		pch = strtok(NULL,";");
	
		if(pch)
			buffer_append_string_len(*out, CONST_STR_LEN(";"));
	
		if(toShare==1)
			log_sys_write(srv, "sbsbss", "Create share link", share_link_info->realpath , "/", share_link_info->filename, "from ip", con->dst_addr_buf->ptr);
	
	}
#else
	char mac[20]="\0";
	get_mac_address("eth0", &mac);
			
	char* base64_auth = ldb_base64_encode(auth, strlen(auth));
	char* base64_key = ldb_base64_encode(mac, strlen(mac));
	char* urlstr = (char*)malloc(buffer_url->size +  strlen(base64_auth) + strlen(strTime) + 15);
	sprintf(urlstr, "%s?auth=%s&expire=%s", buffer_url->ptr, base64_auth, strTime);
	
	share_link = x123_encode(urlstr, base64_key, &share_link);
	//share_link = x123_encode("sambapc/sharefolder", "abcdefg", &share_link);
	free(base64_auth);
	free(base64_key);
	free(urlstr);
	
	Cdbg(DBE, "do HTTP_METHOD_GSL urlstr=%s, share_link=%s", urlstr, share_link);
	
	//char* decode_str;
	//decode_str = x123_decode(encode_str, ldb_base64_encode(mac, strlen(mac)), &decode_str);
	//Cdbg(DBE, "do HTTP_METHOD_GSL decode_str=%s", decode_str);
#endif
#endif

	return 1;

}

int get_sharelink_save_count(){
	int count = 0;
	share_link_info_t* c;
	time_t cur_time = time(NULL);
	for (c = share_link_info_list; c; c = c->next) {			
		if(c->toshare != 0){
			double offset = difftime(c->expiretime, cur_time);
			if( c->expiretime == 0 || offset >= 0.0 ){
				count++;
			}
		}
	}
	Cdbg(DBE, "get_sharelink_save_count=%d", count);
	return count;
}

void free_share_link_info(share_link_info_t *smb_sharelink_info){
	buffer_free(smb_sharelink_info->shortpath);
	buffer_free(smb_sharelink_info->realpath);
	buffer_free(smb_sharelink_info->filename);
	buffer_free(smb_sharelink_info->auth);
}

void save_sharelink_list(){
	share_link_info_t* c;
	time_t cur_time = time(NULL);
	
	buffer* sharelink_list = buffer_init();
	buffer_copy_string(sharelink_list, "");
	
	for (c = share_link_info_list; c; c = c->next) {

		double offset = difftime(c->expiretime, cur_time);					
		if( c->expiretime !=0 && offset < 0.0 ){				
			share_link_info_t* c_prev = c->prev;
			free_share_link_info(c);
			DLIST_REMOVE(share_link_info_list, c);
			free(c);
			c = c_prev;				
			continue;
		}
			
		if(c->toshare != 0){
				
			buffer* temp = buffer_init();
			
			buffer_copy_buffer(temp, c->shortpath);
			buffer_append_string(temp, ">");
			buffer_append_string_buffer(temp, c->realpath);
			buffer_append_string(temp, ">");
			buffer_append_string_buffer(temp, c->filename);
			//buffer_append_string_encoded(temp, CONST_BUF_LEN(c->filename), ENCODING_REL_URI);
			buffer_append_string(temp, ">");
			buffer_append_string_buffer(temp, c->auth);
			buffer_append_string(temp, ">");
			
			char strTime[25] = {0};
			sprintf(strTime, "%lu", c->expiretime);		
			buffer_append_string(temp, strTime);
			
			buffer_append_string(temp, ">");		
			char strTime2[25] = {0};
			sprintf(strTime2, "%lu", c->createtime);		
			buffer_append_string(temp, strTime2);
			
			buffer_append_string(temp, ">");		
			char toshare[5] = {0};
			sprintf(toshare, "%d", c->toshare);		
			buffer_append_string(temp, toshare);
			
			buffer_append_string(temp, "<");
				
			buffer_append_string_buffer(sharelink_list, temp);
			
			buffer_free(temp);
		}
				
	}

#if EMBEDDED_EANBLE
	nvram_set_sharelink_str(sharelink_list->ptr);
#else
	unlink(g_temp_sharelink_file);
	FILE* fp = fopen(g_temp_sharelink_file, "w");
	if(fp!=NULL){
		fprintf(fp, "%s", sharelink_list->ptr);
		fclose(fp);
	}
#endif

	buffer_free(sharelink_list);

	//Cdbg(1, "save sharelink array length = %d", get_sharelink_save_count());
}



void read_sharelink_list(){

	char *nvram_sharelink = NULL;
	
#if EMBEDDED_EANBLE	
	nvram_sharelink = nvram_get_sharelink_str();
#else	
	nvram_sharelink = read_whole_file(g_temp_sharelink_file);
#endif

	if(nvram_sharelink==NULL){
		return;
	}

	char* str_sharelink_list = (char*)malloc(strlen(nvram_sharelink)+1);
	strcpy(str_sharelink_list, nvram_sharelink);

#ifdef APP_IPKG
	free(nvram_sharelink);
#endif

	if(str_sharelink_list==NULL)
		return;
	
	char *pch, *pch2, *saveptr=NULL, *saveptr2=NULL;
	pch = strtok_r(str_sharelink_list, "<", &saveptr);
	Cdbg(DBE, "pch=%s", pch);

	while(pch!=NULL){
		char* tmp = strdup(pch);

		pch2 = strtok_r(tmp, ">", &saveptr2);
		Cdbg(DBE, "pch2=%s", pch2);
		
		while(pch2!=NULL){
			
			//pch2 = strtok_r(NULL, ">", &saveptr2);

			int b_addto_list = 1;
			
			share_link_info_t *smb_sharelink_info;
			smb_sharelink_info = (share_link_info_t *)calloc(1, sizeof(share_link_info_t));
						
			//- Share Path
			if(pch2){
				Cdbg(DBE, "share path=%s", pch2);
				smb_sharelink_info->shortpath = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->shortpath, pch2, strlen(pch2));
			}
			
			//- Real Path
			pch2 = strtok_r(NULL, ">", &saveptr2);
			if(pch2){
				Cdbg(DBE, "real path=%s", pch2);
				smb_sharelink_info->realpath = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->realpath, pch2, strlen(pch2));
			}
			
			//- File Name
			pch2 = strtok_r(NULL, ">", &saveptr2);
			if(pch2){
				Cdbg(DBE, "file name=%s", pch2);
				smb_sharelink_info->filename = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->filename, pch2, strlen(pch2));
			}
			
			//- Auth
			pch2 = strtok_r(NULL, ">", &saveptr2);
			if(pch2){
				Cdbg(DBE, "auth=%s", pch2);
				smb_sharelink_info->auth = buffer_init();
				buffer_copy_string_len(smb_sharelink_info->auth, pch2, strlen(pch2));
			}

			//- Expire Time
			pch2 = strtok_r(NULL, ">", &saveptr2);
			if(pch2){				
				smb_sharelink_info->expiretime = atoi(pch2);	
				time_t cur_time = time(NULL);				
				double offset = difftime(smb_sharelink_info->expiretime, cur_time);					
				if( smb_sharelink_info->expiretime !=0 && offset < 0.0 ){
					free_share_link_info(smb_sharelink_info);
					free(smb_sharelink_info);
					b_addto_list = 0;
				}
			}
			
			//- Create Time
			pch2 = strtok_r(NULL, ">", &saveptr2);
			if(pch2){		
				Cdbg(DBE, "createtime=%s", pch2);
				smb_sharelink_info->createtime = atoi(pch2);
			}
			
			//- toShare
			pch2 = strtok_r(NULL, ">", &saveptr2);
			if(pch2){	
				Cdbg(DBE, "toshare=%s", pch2);
				smb_sharelink_info->toshare = atoi(pch2);
			}
			
			if(b_addto_list==1)
				DLIST_ADD(share_link_info_list, smb_sharelink_info);
			
			//- Next
			pch2 = strtok_r(NULL, ">", &saveptr2);
		}

		if(tmp!=NULL)
			free(tmp);
		
		pch = strtok_r(NULL, "<", &saveptr);
	}

}

void start_arpping_process(const char* scan_interface)
{	
#if EMBEDDED_EANBLE
	if(pids("lighttpd-arpping"))
		return;
#else
	if(system("pidof lighttpd-arpping") == 0)
		return;
#endif

	char cmd[BUFSIZ]="\0";
	/*
	char mybuffer[BUFSIZ]="\0";
	FILE* fp = popen("pwd", "r");
	if(fp){
		int len = fread(mybuffer, sizeof(char), BUFSIZ, fp);
		mybuffer[len-1]="\0";
		pclose(fp);

		sprintf(cmd, ".%s/_inst/sbin/lighttpd-arpping -f %s &", mybuffer, scan_interface);
		Cdbg(DBE, "%s, len=%d", cmd, len);
		system(cmd);
	}
	*/	
#ifndef APP_IPKG
#if EMBEDDED_EANBLE
	sprintf(cmd, "/usr/sbin/lighttpd-arpping -f %s &", scan_interface);
#else
	sprintf(cmd, "./_inst/sbin/lighttpd-arpping -f %s &", scan_interface);
#endif
#else
#if EMBEDDED_EANBLE
    sprintf(cmd, "/opt/bin/lighttpd-arpping -f %s &", scan_interface);
#else
    sprintf(cmd, "./_inst/sbin/lighttpd-arpping -f %s &", scan_interface);
#endif
#endif
	system(cmd);

}

void stop_arpping_process()
{
	FILE *fp;
    char buf[256];
    pid_t pid = 0;
    int n;
	
	if ((fp = fopen(LIGHTTPD_ARPPING_PID_FILE_PATH, "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp) != NULL)
	    	pid = strtoul(buf, NULL, 0);
	    fclose(fp);
	}

	if (pid > 1 && kill(pid, SIGTERM) == 0) {
		n = 10;	       
	    while ((kill(pid, SIGTERM) == 0) && (n-- > 0)) {
	    	Cdbg(DBE,"Mod_smbdav: %s: waiting pid=%d n=%d\n", __FUNCTION__, pid, n);
	        usleep(100 * 1000);
	    }
	}

	unlink(LIGHTTPD_ARPPING_PID_FILE_PATH);
}

int in_the_same_folder(buffer *src, buffer *dst) 
{
	char *sp;

	char *smem = (char *)malloc(src->used);
	memcpy(smem, src->ptr, src->used);
		
	char *dmem = (char *)malloc(dst->used);	
	memcpy(dmem, dst->ptr, dst->used);
		
	sp = strrchr(smem, '/');
	int slen = sp - smem;
	sp = strrchr(dmem, '/');
	int dlen = sp - dmem;

	smem[slen] = '\0';
	dmem[dlen] = '\0';
	
	int res = memcmp(smem, dmem, (slen>dlen) ? slen : dlen);		
	Cdbg(DBE, "smem=%s,dmem=%s, slen=%d, dlen=%d", smem, dmem, slen, dlen);
	free(smem);
	free(dmem);

	return (res) ? 0 : 1;
}

int is_dms_ipk_enabled(void)
{
    FILE* fp2;
    char line[128];
    char ms_enable[20]="\0";
    int result = 0;
    if((fp2 = fopen("/opt/lib/ipkg/info/mediaserver.control", "r")) != NULL){
            memset(line, 0, sizeof(line));
            while(fgets(line, 128, fp2) != NULL){
                    if(strncmp(line, "Enabled:", 8)==0){
                            strncpy(ms_enable, line + 9, strlen(line)-8);
                    }
            }
            fclose(fp2);
    }
    Cdbg(DBE, "ms_enable=%s\n", ms_enable);
    if(strncmp(ms_enable,"yes",strlen("yes")) == 0)
        result = 1;
    //fprintf(stderr,"ms_enable=%s,result=%d\n", ms_enable,result);
    return result;
}


int is_dms_enabled(void){
	int result = 0;
	
#if EMBEDDED_EANBLE
	char *dms_enable = nvram_get_dms_enable();
	char *ms_dlna = NULL;

	if(NULL == dms_enable || strcmp(dms_enable, "") == 0)                
	{                    
		ms_dlna = nvram_get_ms_enable();      
	}
#else
	char *dms_enable = "1";
	char *ms_dlna = NULL;
#endif

	if(NULL != dms_enable && strcmp( dms_enable, "1" ) == 0) 
		result = 1;
        else if(NULL != ms_dlna && strcmp( ms_dlna, "1" ) == 0)
        {
            if(0 == access("/opt/lib/ipkg/info/mediaserver.control",F_OK))
            {
                result = is_dms_ipk_enabled();
            }
            else
            {
                result = 0;
            }
         }

#if EMBEDDED_EANBLE
#ifdef APP_IPKG
	if(dms_enable)							  
		free(dms_enable); 					   
	if(ms_dlna)							  
		free(ms_dlna);
#endif
#endif	

	return result;
}

#if 0
int smbc_host_account_authentication(connection* con, const char *username, const char *password){

	char *nvram_acc_list, *nvram_acc_webdavproxy;	
	int st_samba_mode = 1;
	if (con->mode != SMB_BASIC&&con->mode != DIRECT) return -1;
#if EMBEDDED_EANBLE
	char *a = nvram_get_acc_list();
	if(a==NULL) return -1;
	int l = strlen(a);
	nvram_acc_list = (char*)malloc(l+1);
	strncpy(nvram_acc_list, a, l);
	nvram_acc_list[l] = '\0';
#ifdef APP_IPKG
	free(a);
#endif
	char *b = nvram_get_acc_webdavproxy();
	if(b==NULL) return -1;
	l = strlen(b);
	nvram_acc_webdavproxy = (char*)malloc(l+1);
	strncpy(nvram_acc_webdavproxy, b, l);
	nvram_acc_webdavproxy[l] = '\0';
#ifdef APP_IPKG
	free(b);
#endif
	st_samba_mode = nvram_get_st_samba_mode();
#else
	int i = 100;
	nvram_acc_list = (char*)malloc(100);
	strcpy(nvram_acc_list, "admin<admin>jerry<jerry");

	nvram_acc_webdavproxy = (char*)malloc(100);
	strcpy(nvram_acc_webdavproxy, "admin<1>jerry<0");
#endif

	int found_account = 0, account_right = -1;

	//- Share All, use guest account
	if(st_samba_mode==0){
		buffer_copy_string(con->router_user, "guest");
		account_right = 1;
		return account_right;
	}
		

	char * pch;
	pch = strtok(nvram_acc_list, "<>");	

	while(pch!=NULL){
		char *name;
		char *pass;
		int len;
		
		//- User Name
		len = strlen(pch);
		name = (char*)malloc(len+1);
		strncpy(name, pch, len);
		name[len] = '\0';
				
		//- User Password
		pch = strtok(NULL,"<>");
		len = strlen(pch);
		pass = (char*)malloc(len+1);
		strncpy(pass, pch, len);
		pass[len] = '\0';
		
		if( strcmp(username, name) == 0 &&
		    strcmp(password, pass) == 0 ){
			
			//Cdbg(1, "22222222222 name = %s, pass=%s, right=%d", username, password, right);
			
			int len2;
			char  *pch2, *name2;
			pch2 = strtok(nvram_acc_webdavproxy, "<>");	

			while(pch2!=NULL){
				//- User Name
				len2 = strlen(pch2);
				name2 = (char*)malloc(len2+1);
				strncpy(name2, pch2, len2);
				name2[len2] = '\0';

				//- User Right
				pch2 = strtok(NULL,"<>");
				account_right = atoi(pch2);
		
				if( strcmp(username, name2) == 0 ){
					free(name2);
					
					if(con->smb_info)
						con->smb_info->auth_right = account_right;
					
					//- Copy user name to router_user
					buffer_copy_string(con->router_user, username);

					found_account = 1;
					
					break;
				}

				free(name2);
				pch2 = strtok(NULL,"<>");
			}

			free(name);
			free(pass);
			
			if(found_account==1)
				break;
		}
		
		free(name);
		free(pass);

		pch = strtok(NULL,"<>");
	}
	
	free(nvram_acc_list);
	free(nvram_acc_webdavproxy);
	
	return account_right;
}
#endif

int is_jffs_supported(void){

#ifdef EMBEDDED_EANBLE
	if(dir_exist("/jffs/"))
		return 1;
#else
	if(dir_exist("/tmp/"))
		return 1;
#endif

	return 0;
}

//- 20121108 JerryLin add for router to router sync use.
void process_share_link_for_router_sync_use(){

	int expire = 0;
	int toshare = 2;
	
#if EMBEDDED_EANBLE
	char* username = nvram_get_http_username();
	char* password = nvram_get_http_passwd();
	char* a = nvram_get_share_link_param();

	if(a==NULL)
		return;
	
	int len = strlen(a) + 1;	
	char* str_sharelink_param = (char*)malloc(len);
	memset(str_sharelink_param, '\0', len);
	memcpy(str_sharelink_param, a, len);
	#ifdef APP_IPKG
	free(a);
	#endif
	Cdbg(1, "str_sharelink_param=%s", str_sharelink_param);
	
#else
	char username[20] = "admin";
	char password[20] = "admin";
	//char str_sharelink_param[100] = "/usbdisk/a1/>folder1</usbdisk/a2/>folder2";
	char str_sharelink_param[100] = "delete>AICLOUD306106790";	
#endif

	char auth[100]="\0";		
	sprintf(auth, "%s:%s", username, password);
	char* base64_auth = ldb_base64_encode(auth, strlen(auth));
#ifdef APP_IPKG
	#if EMBEDDED_EANBLE
	free(username);
	free(password);
	#endif
#endif
	buffer* return_sharelink = buffer_init();	
	buffer_reset(return_sharelink);	

	char *pch, *pch2, *saveptr=NULL, *saveptr2=NULL;
	pch = strtok_r(str_sharelink_param, "<", &saveptr);

	while(pch!=NULL){
		char* tmp = strdup(pch);
		pch2 = strtok_r(tmp, ">", &saveptr2);

		while(pch2!=NULL){
			
			if(strcmp(pch2, "delete") == 0 ){
				
				int b_save_arpping_list = 0;
				char* short_link = strtok_r(NULL, ">", &saveptr2);

				if(short_link!=NULL){
					share_link_info_t* c;
					for (c = share_link_info_list; c; c = c->next) {

						if(c->toshare != 2)
							continue;
						
						if(buffer_is_equal_string(c->shortpath, short_link, strlen(short_link))){
							DLIST_REMOVE(share_link_info_list, c);
							b_save_arpping_list = 1;
							break;
						}
					}

					if(b_save_arpping_list)
						save_sharelink_list();
				}
			}
			else{
				char* filename = strtok_r(NULL, ">", &saveptr2);			
				if(filename!=NULL){
					share_link_info_t *share_link_info;
					share_link_info = (share_link_info_t *)calloc(1, sizeof(share_link_info_t));
					
					char share_link[1024];			
					struct timeval tv;
					unsigned long long now_utime;
					gettimeofday(&tv,NULL);
					now_utime = tv.tv_sec * 1000000 + tv.tv_usec;  
					sprintf( share_link, "AICLOUD%d", abs(now_utime));

					buffer_append_string(return_sharelink, share_link);
					buffer_append_string(return_sharelink, ">");
						
					share_link_info->shortpath = buffer_init();
					buffer_copy_string(share_link_info->shortpath, share_link);
					
					share_link_info->realpath = buffer_init();
					buffer_copy_string(share_link_info->realpath, pch2);

					share_link_info->filename = buffer_init();
					buffer_copy_string(share_link_info->filename, filename);
								
					share_link_info->auth = buffer_init();
					buffer_copy_string(share_link_info->auth, base64_auth);
					
					share_link_info->createtime = time(NULL);
					share_link_info->expiretime = 0;
					share_link_info->toshare = toshare;
					
					DLIST_ADD(share_link_info_list, share_link_info);
				}
			}
			
			//- Next
			pch2 = strtok_r(NULL, ">", &saveptr2);
		}

		if(tmp!=NULL)
			free(tmp);
		
		//- Next
		pch = strtok_r(NULL, "<", &saveptr);
	}

	save_sharelink_list();

#if EMBEDDED_EANBLE
	nvram_set_share_link_result(return_sharelink->ptr);
	free(str_sharelink_param);
#endif

	Cdbg(DBE,"process_share_link_for_router_sync_use...return_sharelink=%s", return_sharelink->ptr);

	buffer_free(return_sharelink);
}

static const char base64_xlat[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int base64_decode2(const char *in, unsigned char *out, int inlen)
{
    char *p;
    int n;
    unsigned long x;
    unsigned char *o;
    char c;
    o = out;
    n = 0;
    x = 0;
    while (inlen-- > 0) {
        if (*in == '=') break;
        if ((p = strchr(base64_xlat, c = *in++)) == NULL) {
//          printf("ignored - %x %c\n", c, c);
            continue;   // note: invalid characters are just ignored
        }
        x = (x << 6) | (p - base64_xlat);
        if (++n == 4) {
            *out++ = x >> 16;
            *out++ = (x >> 8) & 0xFF;
            *out++ = x & 0xFF;
            n = 0;
            x = 0;
        }
    }
    if (n == 3) {
        *out++ = x >> 10;
        *out++ = (x >> 2) & 0xFF;
    }
    else if (n == 2) {
        *out++ = x >> 4;
    }
    return out - o;
}

/* md5sum:
 * Concatenates a series of strings together then generates an md5
 * message digest. Writes the result directly into 'output', which
 * MUST be large enough to accept the result. (Size ==
 * 128 + null terminator.)
 */
char secret[100] = "804b51d14d840d6e";

/* sprint_hex:
 * print a long hex value into a string buffer. This function will
 * write 'len' bytes of data from 'char_buf' into 2*len bytes of space
 * in 'output'. (Each hex value is 4 bytes.) Space in output must be
 * pre-allocated. */
void sprint_hex(char* output, unsigned char* char_buf, int len)
{
	int i;
	for (i=0; i < len; i++) {
		sprintf(output + i*2, "%02x", char_buf[i]);
	}
}

void md5sum(char* output, int counter, ...)
{
	va_list argp;
	unsigned char temp[MD5_DIGEST_LENGTH];
	char* md5_string;
	int full_len;
	int i, str_len;
	int buffer_size = 10;

	md5_string = (char*)malloc(buffer_size);
	md5_string[0] = '\0';

	full_len = 0;
	va_start(argp, secret);
	for (i = 0; i < counter; i++) {
		char* string = va_arg(argp, char*);
		int len = strlen(string);

		/* If the buffer is not large enough, expand until it is. */
		while (len + full_len > buffer_size - 1) {
			buffer_size += buffer_size;
			md5_string = realloc(md5_string, buffer_size);
		}

		strncat(md5_string, string, len);

		full_len += len;
		md5_string[full_len] = '\0';
	}
	va_end(argp);

	str_len = strlen(md5_string);
	MD5((const unsigned char*)md5_string, (unsigned int)str_len, temp);

	free(md5_string);

	sprint_hex(output, temp, MD5_DIGEST_LENGTH);	
	output[129] = '\0';
}

int check_skip_folder_name(char* foldername){	
	if ( strcmp(foldername, "minidlna")==0 ||		 
		 strcmp(foldername, "asusware")==0 ||		 
		 strcmp(foldername, "asusware.big")==0 ||		 
		 strcmp(foldername, "asusware.mipsbig")==0 ||
		 strcmp(foldername, "asusware.arm")==0 ||		 
		 strcmp(foldername, "$RECYCLE.BIN")==0 ||
		 strcmp(foldername, "System Volume Information")==0 ) {		
		 return 1;	
	}	

	if ( prefix_is(foldername, "ntfsck")==1 )
		return 1;
	
	return 0;
}

int prefix_is(char* source, char* prefix){
	if(source==NULL||prefix==NULL)
		return -1;
	
	int index = strstr(source, prefix) - source;
	
	return (index==0) ? 1 : -1;
}
#if 0
int is_utf8_file(const char* file){
	FILE* fp;
	unsigned char header[5];
	int ret = 0;
	if((fp = fopen(file, "rb")) != NULL){
		fgets(header, 5, fp);
		if (header[0] == 0xEF && header[1] == 0xBB && header[2] == 0xBF){
			ret = 1;
		}
						
		fclose(fp);
	}

	return ret;
}

//- Unicode -> ANSI
char* w2m(const wchar_t* wcs)
{
      int len;
      char* buf;
      len =wcstombs(NULL,wcs,0);
      if (len == 0)
          return NULL;
      buf = (char *)malloc(sizeof(char)*(len+1));
      memset(buf, 0, sizeof(char) *(len+1));
      len =wcstombs(buf,wcs,len+1);
      return buf;
}

int ansiToWCHAR(wchar_t* out, int outsize, const char* in, int insize, const char* codepage)
{
    if(outsize > 0 && insize > 0 && out && in && setlocale(LC_ALL, codepage))
    {
    	Cdbg(1, "ansiToWCHAR, in=%s", in);
        int size = mbstowcs(out,in,outsize);
        if(size < outsize - 1){out[size]=0;out[size+1]=0;}
        return size;
    }
    return 0;
}

//- ANSI -> Unicode
wchar_t* m2w(const char* mbs)
{
    int len;
    wchar_t* buf;
	setlocale(LC_ALL, "en_ZW.utf8");
	
    len = mbstowcs(0,mbs,0);
    if (len <= 0){
		return NULL;
 	}
	Cdbg(1, "mbstowcs len=%d", len);
	
#if 0
	buf = (wchar_t *)malloc(sizeof(wchar_t)*(len/2 + 1));
	memset(buf, 0, sizeof(wchar_t)*(len/2 + 1));
    len = mbstowcs(buf, mbs, len/2+1);
#else		
	buf = (wchar_t *)malloc(sizeof(wchar_t)*(len+1));
    memset(buf, 0, sizeof(wchar_t) *(len+1));
    len = mbstowcs(buf,mbs,len+1);
#endif
	Cdbg(1, "len=%d, buf=%s, mbs=%s", len, buf, mbs);
    return buf;
}

int enc_unicode_to_utf8_one(unsigned long unic, unsigned char *pOutput,
        int outSize)
{
    //assert(pOutput != NULL);
    //assert(outSize >= 6);

    if ( unic <= 0x0000007F )
    {
        // * U-00000000 - U-0000007F:  0xxxxxxx
        *pOutput     = (unic & 0x7F);
        return 1;
    }
    else if ( unic >= 0x00000080 && unic <= 0x000007FF )
    {
        // * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
        *(pOutput+1) = (unic & 0x3F) | 0x80;
        *pOutput     = ((unic >> 6) & 0x1F) | 0xC0;
        return 2;
    }
    else if ( unic >= 0x00000800 && unic <= 0x0000FFFF )
    {
        // * U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
        *(pOutput+2) = (unic & 0x3F) | 0x80;
        *(pOutput+1) = ((unic >> 6) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 12) & 0x0F) | 0xE0;
        return 3;
    }
    else if ( unic >= 0x00010000 && unic <= 0x001FFFFF )
    {
        // * U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput+3) = (unic & 0x3F) | 0x80;
        *(pOutput+2) = ((unic >> 6) & 0x3F) | 0x80;
        *(pOutput+1) = ((unic >> 12) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 18) & 0x07) | 0xF0;
        return 4;
    }
    else if ( unic >= 0x00200000 && unic <= 0x03FFFFFF )
    {
        // * U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput+4) = (unic & 0x3F) | 0x80;
        *(pOutput+3) = ((unic >> 6) & 0x3F) | 0x80;
        *(pOutput+2) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput+1) = ((unic >> 18) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 24) & 0x03) | 0xF8;
        return 5;
    }
    else if ( unic >= 0x04000000 && unic <= 0x7FFFFFFF )
    {
        // * U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput+5) = (unic & 0x3F) | 0x80;
        *(pOutput+4) = ((unic >> 6) & 0x3F) | 0x80;
        *(pOutput+3) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput+2) = ((unic >> 18) & 0x3F) | 0x80;
        *(pOutput+1) = ((unic >> 24) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 30) & 0x01) | 0xFC;
        return 6;
    }

    return 0;
}


int do_iconv(const char* to_ces, const char* from_ces,
	 char *inbuf,  size_t inbytesleft,
	 char *outbuf_orig, size_t outbytesleft_orig)
{
#ifdef HAVE_ICONV_H
	size_t rc;
	int ret = 1;

	size_t outbytesleft = outbytesleft_orig - 1;
	char* outbuf = outbuf_orig;

	iconv_t cd  = iconv_open(to_ces, from_ces);

	if(cd == (iconv_t)-1)
	{
		return 0;
	}
	rc = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	//Cdbg(1, "outbuf=%s", outbuf);
	if(rc == (size_t)-1)
	{
		Cdbg(1, "fail");
		ret = 0;
		/*
		if(errno == E2BIG)
		{
			ret = 0;
		}
		else
		{
			ret = 0;
			memset(outbuf_orig, '\0', outbytesleft_orig);
		}
		*/
	}
	iconv_close(cd);

	return ret;
#else // HAVE_ICONV_H
	return 0;
#endif // HAVE_ICONV_H
}

int enc_get_utf8_size(unsigned char c)
{
    unsigned char t = 0x80;
    unsigned char r = c;
    int count = 0;

    while ( r & t )
    {
        r = r << 1;
        count++;
    }

    return count;
}

int enc_utf8_to_unicode_one(const unsigned char* pInput, unsigned long *Unic)
{
    assert(pInput != NULL && Unic != NULL);

    char b1, b2, b3, b4, b5, b6;

    *Unic = 0x0;
    int utfbytes = enc_get_utf8_size(*pInput);
    unsigned char *pOutput = (unsigned char *) Unic;

    switch ( utfbytes )
    {
        case 0:
            *pOutput     = *pInput;
            utfbytes    += 1;
            break;
        case 2:
            b1 = *pInput;
            b2 = *(pInput + 1);
            if ( (b2 & 0xE0) != 0x80 )
                return 0;
            *pOutput     = (b1 << 6) + (b2 & 0x3F);
            *(pOutput+1) = (b1 >> 2) & 0x07;
            break;
        case 3:
            b1 = *pInput;
            b2 = *(pInput + 1);
            b3 = *(pInput + 2);
            if ( ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80) )
                return 0;
            *pOutput     = (b2 << 6) + (b3 & 0x3F);
            *(pOutput+1) = (b1 << 4) + ((b2 >> 2) & 0x0F);
            break;
        case 4:
            b1 = *pInput;
            b2 = *(pInput + 1);
            b3 = *(pInput + 2);
            b4 = *(pInput + 3);
            if ( ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)
                    || ((b4 & 0xC0) != 0x80) )
                return 0;
            *pOutput     = (b3 << 6) + (b4 & 0x3F);
            *(pOutput+1) = (b2 << 4) + ((b3 >> 2) & 0x0F);
            *(pOutput+2) = ((b1 << 2) & 0x1C)  + ((b2 >> 4) & 0x03);
            break;
        case 5:
            b1 = *pInput;
            b2 = *(pInput + 1);
            b3 = *(pInput + 2);
            b4 = *(pInput + 3);
            b5 = *(pInput + 4);
            if ( ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)
                    || ((b4 & 0xC0) != 0x80) || ((b5 & 0xC0) != 0x80) )
                return 0;
            *pOutput     = (b4 << 6) + (b5 & 0x3F);
            *(pOutput+1) = (b3 << 4) + ((b4 >> 2) & 0x0F);
            *(pOutput+2) = (b2 << 2) + ((b3 >> 4) & 0x03);
            *(pOutput+3) = (b1 << 6);
            break;
        case 6:
            b1 = *pInput;
            b2 = *(pInput + 1);
            b3 = *(pInput + 2);
            b4 = *(pInput + 3);
            b5 = *(pInput + 4);
            b6 = *(pInput + 5);
            if ( ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)
                    || ((b4 & 0xC0) != 0x80) || ((b5 & 0xC0) != 0x80)
                    || ((b6 & 0xC0) != 0x80) )
                return 0;
            *pOutput     = (b5 << 6) + (b6 & 0x3F);
            *(pOutput+1) = (b5 << 4) + ((b6 >> 2) & 0x0F);
            *(pOutput+2) = (b3 << 2) + ((b4 >> 4) & 0x03);
            *(pOutput+3) = ((b1 << 6) & 0x40) + (b2 & 0x3F);
            break;
        default:
            return 0;
            break;
    }

    return utfbytes;
}

int enc_utf8_to_unicode(const unsigned char* pInput, int nMembIn,unsigned long* pOutput, int* nMembOut)
{
    assert(pInput != NULL && pOutput != NULL);
    assert(*nMembOut >= 1);

    int i, ret, outSize;
    const unsigned char *pIn     = pInput;
    unsigned long       *pOutCur = pOutput;

    outSize = *nMembOut;
    for ( i = 0; i < nMembIn; )
    {
        if ( pOutCur - pOutput >= outSize )
        {
            *nMembOut = pOutCur - pOutput;
            return 2; // ??ぃì
        }

        ret = enc_utf8_to_unicode_one(pIn, pOutCur);
        if ( ret == 0 )
        {
            *nMembOut = pOutCur - pOutput;
            return 0; // ?才??ぃ琌UTF8
        }

        i    += ret;
        pIn  += ret;
        pOutCur += 1;
    }

    *nMembOut = pOutCur - pOutput;
    return 1;
}

int enc_utf8_to_unicode_str(const unsigned char *pInput,unsigned long *pOutput, int *nMembOut)
{
    assert(pInput != NULL && pOutput != NULL);
    assert(*nMembOut >= 1);

    return enc_utf8_to_unicode(pInput, strlen((const char *)pInput), 
            pOutput, nMembOut);
}

int enc_unicode_to_utf8(const unsigned long *pInput, int nMembIn, unsigned char *pOutput, int *nMembOut)
{
    assert(pInput != NULL && pOutput != NULL );
    assert(*nMembOut >= 6);

    int i, ret, outSize, resOutSize;
    const unsigned long *pIn     = pInput;
    unsigned char       *pOutCur = pOutput;

    outSize = *nMembOut;
    for ( i = 0; i < nMembIn; )
    {
        resOutSize = outSize - (pOutCur - pOutput);
        if ( resOutSize < 6 )
        {
            *nMembOut = pOutCur - pOutput;
            return 2;
        }

        ret = enc_unicode_to_utf8_one(*pIn, pOutCur, resOutSize);
        if ( ret == 0 )
        {
            *nMembOut = pOutCur - pOutput;
            return 0;
        }

        i   += 1;
        pIn += 1;
        pOutCur += ret;
    }

    *nMembOut = pOutCur - pOutput;
    return 1;
}

int enc_unicode_to_utf8_str(const unsigned long *pInput, unsigned char *pOutput, int *nMembOut)
{
    if(pInput == NULL || pOutput == NULL )
		return 0;
	
    if(*nMembOut < 6) 
		return 0;

    int i, ret, outSize, resOutSize;
    const unsigned long *pIn     = pInput;
    unsigned char       *pOutCur = pOutput;

    outSize = *nMembOut;
    for ( ; *pIn != 0; )
    {
        resOutSize = outSize - (pOutCur - pOutput);
        if ( resOutSize < 6 )
        {
            *nMembOut = pOutCur - pOutput;
            return 2;
        }

        ret = enc_unicode_to_utf8_one(*pIn, pOutCur, resOutSize);
        if ( ret == 0 )
        {
            *nMembOut = pOutCur - pOutput;
            return 0;
        }

        i   += 1;
        pIn += 1;
        pOutCur += ret;
    }

    *nMembOut = pOutCur - pOutput;
    return 1;
}
#endif

#if EMBEDDED_EANBLE

int get_aicloud_var_file_name(const char *const account, const char *const path, char **file_name){
	int len;
	char *var_file;
	char ascii_user[64];

	if(path == NULL)
		return -1;

	len = strlen(path)+strlen("/.__aicloud__var.txt");
	if(account != NULL){
		memset(ascii_user, 0, 64);
		char_to_ascii_safe(ascii_user, account, 64);

		len += strlen(ascii_user);
	}
	*file_name = (char *)malloc(sizeof(char)*(len+1));
	if(*file_name == NULL)
		return -1;

	var_file = *file_name;
	if(account != NULL)
		sprintf(var_file, "%s/.__aicloud_%s_var.txt", path, ascii_user);
	else
		sprintf(var_file, "%s/.__aicloud_var.txt", path);
	var_file[len] = 0;

	return 0;
}

int initial_aicloud_var_file(const char *const account, const char *const mount_path) {
	FILE *fp;
	char *var_file;
	int result, i;
	int sh_num;
	char **folder_list;
	int aicloud_right = 0;

	if (mount_path == NULL || strlen(mount_path) <= 0) {
		Cdbg(DBE, "No input, mount_path");
		return -1;
	}

	// 1. get the folder number and folder_list
	//result = get_folder_list(mount_path, &sh_num, &folder_list);
	result = get_all_folder(mount_path, &sh_num, &folder_list);

	// 2. get the var file
	if(get_aicloud_var_file_name(account, mount_path, &var_file)){
		Cdbg(DBE, "Can't malloc \"var_file\".");
		free_2_dimension_list(&sh_num, &folder_list);
		return -1;
	}
	
	// 4. write the default content in the var file
	if ((fp = fopen(var_file, "w")) == NULL) {
		Cdbg(DBE, "Can't create the var file, \"%s\".", var_file);
		free_2_dimension_list(&sh_num, &folder_list);
		free(var_file);
		return -1;
	}

	for (i = -1; i < sh_num; ++i) {
		fprintf(fp, "*");
		
		if(i != -1)
			fprintf(fp, "%s", folder_list[i]);

		fprintf(fp, "=%d\n", aicloud_right);
	}
	
	fclose(fp);
	free_2_dimension_list(&sh_num, &folder_list);

	// 5. set the check target of file.
	set_file_integrity(var_file);
	free(var_file);

	return 0;
}

int get_aicloud_permission(const char *const account,
								const char *const mount_path,
								const char *const folder) {
	char *var_file, *var_info;
	char *target, *follow_info;
	int len, result;
	char *f = (char*) folder;

	Cdbg(DBE, "get_aicloud_permission: %s, %s, %s.", account, mount_path, folder);
	
	// 1. get the aicloud var file
	if(get_aicloud_var_file_name(account, mount_path, &var_file)){
		Cdbg(DBE, "Can't malloc \"var_file\".");
		return -1;
	}

	Cdbg(DBE, "var_file: %s.", var_file);
	
	// 2. check the file integrity.
	if(!check_file_integrity(var_file)){
		Cdbg(DBE, "Fail to check the file: %s.", var_file);
		if(initial_var_file(account, mount_path) != 0){
			Cdbg(DBE, "Can't initial \"%s\"'s file in %s.", account, mount_path);
			free(var_file);
			return -1;
		}
	}
	
	// 3. get the content of the var_file of the account
	var_info = read_whole_file(var_file);
	if (var_info == NULL) {
		Cdbg(DBE, "get_permission: \"%s\" isn't existed or there's no content.", var_file);
		free(var_file);
		return -1;
	}
	free(var_file);
	
	// 4. get the target in the content
retry_get_permission:
	if(f == NULL)
		len = strlen("*=");
	else
		len = strlen("*")+strlen(f)+strlen("=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (target == NULL) {
		Cdbg(DBE, "Can't allocate \"target\".");
		free(var_info);
		return -1;
	}
	if(f == NULL)
		strcpy(target, "*=");
	else
		sprintf(target, "*%s=", f);
	target[len] = 0;
	
	follow_info = upper_strstr(var_info, target);
	free(target);
	if (follow_info == NULL) {
		if(account == NULL)
			Cdbg(DBE, "No right about \"%s\" with the share mode.", f? f:"Pool");
		else
			Cdbg(DBE, "No right about \"%s\" with \"%s\".", f? f:"Pool", account);

		if (f == NULL) {
			free(var_info);
			return -1;
		} else {
			f = NULL;
			goto retry_get_permission;
		}
	}
	
	follow_info += len;

	if (follow_info[1] != '\n') {
		if(account == NULL)
			Cdbg(DBE, "The var info is incorrect.\nPlease reset the var file of the share mode.");
		else
			Cdbg(DBE, "The var info is incorrect.\nPlease reset the var file of \"%s\".", account);

		free(var_info);
		return -1;
	}
	
	// 5. get the right of folder
	result = follow_info[0]-'0';
	
	free(var_info);
	
	if (result < 0 || result > 3) {
		if(account == NULL)
			Cdbg(DBE, "The var info is incorrect.\nPlease reset the var file of the share mode.");
		else
			Cdbg(DBE, "The var info is incorrect.\nPlease reset the var file of \"%s\".", account);
		return -1;
	}
	
	return result;
}

int set_aicloud_permission(const char *const account,
						  const char *const mount_path,
						  const char *const folder,
						  const int flag) {
	FILE *fp;
	char *var_file, *var_info;
	char *target, *follow_info;
	int len;
	
	if (flag < 0 || flag > 3) {
		Cdbg(DBE, "correct Rights is 0, 1, 2, 3.");
		return -1;
	}
	
	// 1. get the var file
	if(get_aicloud_var_file_name(account, mount_path, &var_file)){
		Cdbg(DBE, "Can't malloc \"var_file\".");
		return -1;
	}

	if(!check_if_file_exist(var_file)){
		if(initial_aicloud_var_file(account, mount_path)!=0){
			Cdbg(DBE, "Fail to check the folder list.");
			return -1;
		}
	}
	
	
	// 2. check the file integrity.
	if(!check_file_integrity(var_file)){
		Cdbg(DBE, "Fail to check the file: %s.", var_file);
		if(initial_var_file(account, mount_path) != 0){
			Cdbg(DBE, "Can't initial \"%s\"'s file in %s.", account, mount_path);
			free(var_file);
			return -1;
		}
	}
	
	// 3. get the content of the var_file of the account
	var_info = read_whole_file(var_file);
	if (var_info == NULL) {
		initial_aicloud_var_file(account, mount_path);
		sleep(1);
		var_info = read_whole_file(var_file);
		if (var_info == NULL) {
			Cdbg(DBE, "set_permission: \"%s\" isn't existed or there's no content.", var_file);
			free(var_file);
			return -1;
		}
	}
		
	// 4. get the target in the content
	if(folder == NULL)
		len = strlen("*=");
	else
		len = strlen("*")+strlen(folder)+strlen("=");
	target = (char *)malloc(sizeof(char)*(len+1));
	if (target == NULL) {
		Cdbg(DBE, "Can't allocate \"target\".");
		free(var_file);
		free(var_info);
		
		return -1;
	}
	if(folder == NULL)
		strcpy(target, "*=");
	else
		sprintf(target, "*%s=", folder);
	target[len] = 0;
	
	// 5. judge if the target is in the var file.
	follow_info = upper_strstr(var_info, target);
	if (follow_info == NULL) {
		if(account == NULL)
			Cdbg(DBE, "No right about \"%s\" with the share mode.", (folder == NULL?"Pool":folder));
		else
			Cdbg(DBE, "No right about \"%s\" with \"%s\".", (folder == NULL?"Pool":folder), account);
		free(var_info);
		
		fp = fopen(var_file, "a+");
		if (fp == NULL) {
			Cdbg(DBE, "1. Can't rewrite the file, \"%s\".", var_file);
			free(var_file);
			return -1;
		}
		free(var_file);
		
		fprintf(fp, "%s", target);
		free(target);

		fprintf(fp, "%d\n", 0, 0, flag);
				
		fclose(fp);
		return 0;
	}
	free(target);
	
	follow_info += len;
	if (follow_info[1] != '\n') {
		if(account == NULL)
			Cdbg(DBE, "The var info is incorrect.\nPlease reset the var file of the share mode.");
		else
			Cdbg(DBE, "The var info is incorrect.\nPlease reset the var file of \"%s\".", account);
		free(var_file);
		free(var_info);
		return -1;
	}
		
	if(follow_info[0] == '0'+flag){
		Cdbg(DBE, "The aicloud right of \"%s\" is the same.", folder);
		free(var_file);
		free(var_info);
		return 0;
	}
	
	follow_info[0] = '0'+flag;
	
	// 7. rewrite the var file.
	fp = fopen(var_file, "w");
	if (fp == NULL) {
		Cdbg(DBE, "2. Can't rewrite the file, \"%s\".", var_file);
		free(var_file);
		free(var_info);
		return -1;
	}
	fprintf(fp, "%s", var_info);
	fclose(fp);
	free(var_info);
	
	return 0;
}

#endif

#if 0
int del_aicloud_account(const char *account){
	if(account==NULL)
		return -1;
	
	char *err;
	char sql_query[2048] = "\0";
	sqlite3 *sql_aicloud;
	char **result;
	int rows;
	int column_count;
	char* aicloud_db_file_path = get_aicloud_db_file_path();
			
	if (SQLITE_OK != sqlite3_open(aicloud_db_file_path, &(sql_aicloud))) {
		return -1;
	}
					
	sprintf(sql_query, "DELETE FROM account_info WHERE username='%s'", account );
							
	if (SQLITE_OK != sqlite3_exec(sql_aicloud, sql_query , NULL, NULL, &err)) {
		sqlite3_free(err);
		sqlite3_close(sql_aicloud);
		return -1;
	}
				
	if (sql_aicloud) {
		sqlite3_close(sql_aicloud);
	}

#if EMBEDDED_EANBLE

	disk_info_t *disk_list, *follow_disk;
	partition_info_t *follow_partition;
	char *var_file, *ptr;
	DIR *opened_dir;
	struct dirent *dp;
	int len;
	
	disk_list = read_disk_data();
	if(disk_list != NULL){
		
		for(follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next){			
			for(follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
				if(follow_partition->mount_point == NULL)
					continue;

				if(get_aicloud_var_file_name(account, follow_partition->mount_point, &var_file)){
					//free_disk_data(&disk_list);
					continue;
				}

				if((ptr = strrchr(var_file, '/')) == NULL){
					//free_disk_data(&disk_list);
					free(var_file);
					continue;
				}

				if((opened_dir = opendir(follow_partition->mount_point)) == NULL){
					//free_disk_data(&disk_list);
					free(var_file);
					continue;
				}

				++ptr;
				
				len = strlen(ptr);
				while((dp = readdir(opened_dir)) != NULL){
					char test_path[PATH_MAX];

					if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
						continue;

					if(strncmp(dp->d_name, ptr, len))
						continue;

					memset(test_path, 0, PATH_MAX);
					sprintf(test_path, "%s/%s", follow_partition->mount_point, dp->d_name);
					Cdbg(1, "delete %s.", test_path);
					
					delete_file_or_dir(test_path);
				}
				closedir(opened_dir);

				free(var_file);
			}
		}

		free_disk_data(&disk_list);
	}	
#endif

	return 0;
}
#endif


int is_connection_admin_permission(server* srv, connection* con){
	if(con==NULL || srv==NULL)
		return 0;
	
	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");
	if(ds!=NULL){
		smb_info_t *c;
		for (c = srv->smb_srv_info_list; c; c = c->next) {
	
			if( buffer_is_equal(c->user_agent, ds->value) && 
				buffer_is_equal(c->src_ip, con->dst_addr_buf) &&
				buffer_is_empty(c->server) ){
	
				if( smbc_get_account_permission(c->username->ptr) == T_ADMIN ){
					return 1;
				}
						
				break;
			}
			
		}
	}

	return 0;
}

int parse_postdata_from_chunkqueue(server *srv, connection *con, chunkqueue *cq, char **ret_data) {
	
	int res = -1;
	
	chunk *c;
	char* result_data = NULL;
	
	UNUSED(con);
	
	for (c = cq->first; cq->bytes_out != cq->bytes_in; c = cq->first) {
		size_t weWant = cq->bytes_out - cq->bytes_in;
		size_t weHave;

		switch(c->type) {
			case MEM_CHUNK:
				weHave = c->mem->used - 1 - c->offset;

				if (weHave > weWant) weHave = weWant;
				
				result_data = (char*)malloc(sizeof(char)*(weHave+1));
				memset(result_data, 0, sizeof(char)*(weHave+1));
				strcpy(result_data, c->mem->ptr + c->offset);
				
				c->offset += weHave;
				cq->bytes_out += weHave;

				res = 0;
				
				break;
				
			default:
				break;
		}
		
		chunkqueue_remove_finished_chunks(cq);
	}
	
	*ret_data = result_data;

	return res;
}

void read_aicloud_acc_list(){

	char *nvram_aicloud_acc_list = NULL;
	
#if EMBEDDED_EANBLE	
	nvram_aicloud_acc_list = nvram_get_aicloud_acc_list();
#else	
	nvram_aicloud_acc_list = read_whole_file("/tmp/aicloud_acc_list");
#endif
	
	if(nvram_aicloud_acc_list==NULL){
		return;
	}

	int len = strlen(nvram_aicloud_acc_list);
	char* aicloud_acc_list = malloc(len);
	
	if(aicloud_acc_list==NULL){
		return;
	}
	
	memset(aicloud_acc_list, '\0', len);
	strncpy(aicloud_acc_list, nvram_aicloud_acc_list, len);

#if EMBEDDED_EANBLE	
	#ifdef APP_IPKG
	if(nvram_aicloud_acc_list!=NULL)
		free(nvram_aicloud_acc_list);
	#endif
#else
	if(nvram_aicloud_acc_list!=NULL)
		free(nvram_aicloud_acc_list);
#endif

	char * pch = strtok(aicloud_acc_list, "<>");
	while(pch!=NULL){

		aicloud_acc_info_t *aicloud_acc_info;
		aicloud_acc_info = (aicloud_acc_info_t *)calloc(1, sizeof(aicloud_acc_info_t));
			
		//- User Name
		aicloud_acc_info->username = buffer_init();
		buffer_copy_string_len(aicloud_acc_info->username, pch, strlen(pch));
		buffer_urldecode_path(aicloud_acc_info->username);
		
		//- User Password
		pch = strtok(NULL,"<>");
		aicloud_acc_info->password = buffer_init();
		buffer_copy_string_len(aicloud_acc_info->password, pch, strlen(pch));
		buffer_urldecode_path(aicloud_acc_info->password);
		
		//- next
		pch = strtok(NULL,"<>");

		DLIST_ADD(aicloud_acc_info_list, aicloud_acc_info);
	}

	free(aicloud_acc_list);

}

void save_aicloud_acc_list(){
	aicloud_acc_info_t* c;
	
	buffer* aicloud_acc_list = buffer_init();
	buffer_copy_string(aicloud_acc_list, "");
	
	for (c = aicloud_acc_info_list; c; c = c->next) {

		if(!buffer_is_equal_string(aicloud_acc_list, "", 0))
			buffer_append_string(aicloud_acc_list, "<");
		
		buffer_append_string_encoded(aicloud_acc_list, CONST_BUF_LEN(c->username), ENCODING_REL_URI);
		buffer_append_string(aicloud_acc_list, ">");
		buffer_append_string_encoded(aicloud_acc_list, CONST_BUF_LEN(c->password), ENCODING_REL_URI);
	}

#if EMBEDDED_EANBLE
	nvram_set_aicloud_acc_list(aicloud_acc_list->ptr);
#else
	unlink("/tmp/aicloud_acc_list");
	FILE* fp = fopen("/tmp/aicloud_acc_list", "w");
	if(fp!=NULL){
		fprintf(fp, "%s", aicloud_acc_list->ptr);
		fclose(fp);
	}
#endif

	buffer_free(aicloud_acc_list);

}

void free_aicloud_acc_info(aicloud_acc_info_t *aicloud_acc_info){
	buffer_free(aicloud_acc_info->username);
	buffer_free(aicloud_acc_info->password);
}

void read_aicloud_acc_invite_list(){

	char *nvram_aicloud_acc_invite_list = NULL;
	
#if EMBEDDED_EANBLE	
	nvram_aicloud_acc_invite_list = nvram_get_aicloud_acc_invite_list();
#else	
	nvram_aicloud_acc_invite_list = read_whole_file("/tmp/aicloud_acc_invite_list");
#endif
	
	if(nvram_aicloud_acc_invite_list==NULL){
		return;
	}
	
	int len = strlen(nvram_aicloud_acc_invite_list);
	char* aicloud_acc_invite_list = malloc(len);
	
	if(aicloud_acc_invite_list==NULL){
		return;
	}
	
	memset(aicloud_acc_invite_list, '\0', len);
	strncpy(aicloud_acc_invite_list, nvram_aicloud_acc_invite_list, len);

#if EMBEDDED_EANBLE		
	#ifdef APP_IPKG
	if(nvram_aicloud_acc_invite_list!=NULL)
		free(nvram_aicloud_acc_invite_list);
	#endif
#else
	if(nvram_aicloud_acc_invite_list!=NULL)
		free(nvram_aicloud_acc_invite_list);
#endif

	char * pch = strtok(aicloud_acc_invite_list, "<>");
	while(pch!=NULL){
	
		aicloud_acc_invite_info_t *aicloud_acc_invite_info;
		aicloud_acc_invite_info = (aicloud_acc_invite_info_t *)calloc(1, sizeof(aicloud_acc_invite_info_t));
				
		//- productid
		aicloud_acc_invite_info->productid = buffer_init();
		if(pch!=NULL && strcmp(pch, "none")!=0){
			buffer_copy_string_len(aicloud_acc_invite_info->productid, pch, strlen(pch));
			buffer_urldecode_path(aicloud_acc_invite_info->productid);
		}
		
		//- deviceid
		pch = strtok(NULL,"<>");
		aicloud_acc_invite_info->deviceid = buffer_init();
		if(pch!=NULL && strcmp(pch, "none")!=0){
			buffer_copy_string_len(aicloud_acc_invite_info->deviceid, pch, strlen(pch));
			buffer_urldecode_path(aicloud_acc_invite_info->deviceid);
		}
		
		//- token
		pch = strtok(NULL,"<>");
		aicloud_acc_invite_info->token = buffer_init();
		if(pch!=NULL && strcmp(pch, "none")!=0){
			buffer_copy_string_len(aicloud_acc_invite_info->token, pch, strlen(pch));
			buffer_urldecode_path(aicloud_acc_invite_info->token);
		}
		
		//- permission
		pch = strtok(NULL,"<>");
		aicloud_acc_invite_info->permission = buffer_init();
		if(pch!=NULL && strcmp(pch, "none")!=0){
			buffer_copy_string_len(aicloud_acc_invite_info->permission, pch, strlen(pch));
			buffer_urldecode_path(aicloud_acc_invite_info->permission);
		}

		//- bytes_in_avail
		pch = strtok(NULL,"<>");
		if(pch!=NULL)
			aicloud_acc_invite_info->bytes_in_avail = atoi(pch);

		//- smart_access
		pch = strtok(NULL,"<>");
		if(pch!=NULL)
			aicloud_acc_invite_info->smart_access = atoi(pch);

		//- security_code
		pch = strtok(NULL,"<>");
		aicloud_acc_invite_info->security_code = buffer_init();
		if(pch!=NULL && strcmp(pch, "none")!=0){
			buffer_copy_string_len(aicloud_acc_invite_info->security_code, pch, strlen(pch));
			buffer_urldecode_path(aicloud_acc_invite_info->security_code);
		}
		
		//- status
		pch = strtok(NULL,"<>");
		if(pch!=NULL)
			aicloud_acc_invite_info->status = atoi(pch);

		//- auth_type
		pch = strtok(NULL,"<>");
		if(pch!=NULL)
			aicloud_acc_invite_info->auth_type = atoi(pch);

		//- createtime
		pch = strtok(NULL,"<>");
		if(pch!=NULL)
			aicloud_acc_invite_info->createtime = atoi(pch);
			
		//- next
		pch = strtok(NULL,"<>");
	
		DLIST_ADD(aicloud_acc_invite_info_list, aicloud_acc_invite_info);
	}
	
	free(aicloud_acc_invite_list);

}

void append_text_to_list(buffer* buf, const char* text){
	
	if(text==NULL || strcmp(text, "")==0){
		buffer_append_string(buf, "none");
		return;
	}

	buffer_append_string_encoded(buf, text, strlen(text), ENCODING_REL_URI);
}

void save_aicloud_acc_invite_list(){
	aicloud_acc_invite_info_t* c;
		
	buffer* aicloud_acc_invite_list = buffer_init();
	buffer_copy_string(aicloud_acc_invite_list, "");	
	
	for (c = aicloud_acc_invite_info_list; c; c = c->next) {
	
		if(!buffer_is_equal_string(aicloud_acc_invite_list, "", 0))
			buffer_append_string(aicloud_acc_invite_list, "<");
		
		append_text_to_list(aicloud_acc_invite_list, c->productid->ptr);

		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, c->deviceid->ptr);
		
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, c->token->ptr);
		
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, c->permission->ptr);
		
		char strByteInAvail[25] = {0};
		sprintf(strByteInAvail, "%lu", c->bytes_in_avail);	
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, strByteInAvail);
		
		char strSmartAccess[2] = {0};
		sprintf(strSmartAccess, "%d", c->smart_access);	
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, strSmartAccess);

		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, c->security_code->ptr);

		char strStatus[2] = {0};
		sprintf(strStatus, "%d", c->status);	
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, strStatus);

		char strAuthType[2] = {0};
		sprintf(strAuthType, "%d", c->auth_type);	
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, strAuthType);

		char strCreateTime[25] = {0};
		sprintf(strCreateTime, "%lu", c->createtime);	
		buffer_append_string(aicloud_acc_invite_list, ">");
		append_text_to_list(aicloud_acc_invite_list, strCreateTime);
		
	}

#if EMBEDDED_EANBLE
	nvram_set_aicloud_acc_invite_list(aicloud_acc_invite_list->ptr);
#else
	unlink("/tmp/aicloud_acc_invite_list");
	FILE* fp = fopen("/tmp/aicloud_acc_invite_list", "w");
	if(fp!=NULL){
		fprintf(fp, "%s", aicloud_acc_invite_list->ptr);
		fclose(fp);
	}
#endif

	buffer_free(aicloud_acc_invite_list);
}
	
void free_aicloud_acc_invite_info(aicloud_acc_invite_info_t *aicloud_acc_invite_info){
	buffer_free(aicloud_acc_invite_info->productid);
	buffer_free(aicloud_acc_invite_info->deviceid);
	buffer_free(aicloud_acc_invite_info->token);
	buffer_free(aicloud_acc_invite_info->permission);
	buffer_free(aicloud_acc_invite_info->security_code);
}
#endif
