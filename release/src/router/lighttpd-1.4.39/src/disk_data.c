#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <setjmp.h>

//#include <bcmnvram.h>
//#include <volume_id_internal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/statfs.h>
#include <stdint.h>
//#include <shared.h>
#include "disk_data.h"


#define PARTITION_FILE "/proc/partitions"
#define USB_DISK_MAJOR 8
#define DEFAULT_USB_TAG "USB disk"
#define PARTITION_TYPE_UNKNOWN "unknown"
#define MOUNT_FILE "/proc/mounts"
#define SYS_BLOCK "/sys/block"

/* Copy each token in wordlist delimited by space into word */
#define foreach(word, wordlist, next) \
        for (next = &wordlist[strspn(wordlist, " ")], \
                strncpy(word, next, sizeof(word)), \
                word[strcspn(word, " ")] = '\0', \
                word[sizeof(word) - 1] = '\0', \
                next = strchr(next, ' '); \
                strlen(word); \
                next = next ? &next[strspn(next, " ")] : "", \
                strncpy(word, next, sizeof(word)), \
                word[strcspn(word, " ")] = '\0', \
                word[sizeof(word) - 1] = '\0', \
                next = strchr(next, ' '))

//#ifdef RTCONFIG_USB

char xhci_string[32];
char ehci_string[32];
char ohci_string[32];
extern char *nvram_safe_get(char *name);
char *get_usb_xhci_port(int port)
{
        char word[100], *next;
        int i=0;

        strcpy(xhci_string, "xxxxxxxx");

        foreach(word, nvram_safe_get("xhci_ports"), next) {
                if(i==port) {
                        strcpy(xhci_string, word);
                        break;
                }
                i++;
        }
        return xhci_string;
}

char *get_usb_ehci_port(int port)
{
    char word[100], *next;
    int i=0;

    strcpy(ehci_string, "xxxxxxxx");

    foreach(word, nvram_safe_get("ehci_ports"), next) {
        if(i==port) {
            strcpy(ehci_string, word);
            break;
        }
        i++;
    }
    return ehci_string;
}

char *get_usb_ohci_port(int port)
{
    char word[100], *next;
    int i=0;

    strcpy(ohci_string, "xxxxxxxx");

    foreach(word, nvram_safe_get("ohci_ports"), next) {
        if(i==port) {
            strcpy(ohci_string, word);
            break;
        }
        i++;
    }
    return ohci_string;
}
//#endif

#define USB_XHCI_PORT_1 get_usb_xhci_port(0)
#define USB_XHCI_PORT_2 get_usb_xhci_port(1)
#define USB_EHCI_PORT_1 get_usb_ehci_port(0)
#define USB_EHCI_PORT_2 get_usb_ehci_port(1)
#define USB_OHCI_PORT_1 get_usb_ohci_port(0)
#define USB_OHCI_PORT_2 get_usb_ohci_port(1)
#define USB_EHCI_PORT_3 get_usb_ehci_port(2)
#define USB_OHCI_PORT_3 get_usb_ohci_port(2)

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

#define VOLUME_ID_LABEL_SIZE		64
#define VOLUME_ID_UUID_SIZE		36

struct volume_id {
    int		fd;
//	int		fd_close:1;
    int		error;
    size_t		sbbuf_len;
    size_t		seekbuf_len;
    uint8_t		*sbbuf;
    uint8_t		*seekbuf;
    uint64_t	seekbuf_off;
#ifdef UNUSED_PARTITION_CODE
    struct volume_id_partition *partitions;
    size_t		partition_count;
#endif
//	uint8_t		label_raw[VOLUME_ID_LABEL_SIZE];
//	size_t		label_raw_len;
    char		label[VOLUME_ID_LABEL_SIZE+1];
//	uint8_t		uuid_raw[VOLUME_ID_UUID_SIZE];
//	size_t		uuid_raw_len;
    /* uuid is stored in ASCII (not binary) form here: */
    char		uuid[VOLUME_ID_UUID_SIZE+1];
//	char		type_version[VOLUME_ID_FORMAT_SIZE];
//	smallint	usage_id;
//	const char	*usage;
//	const char	*type;
};

enum {
    DEVICE_TYPE_UNKNOWN=0,
    DEVICE_TYPE_DISK,
    DEVICE_TYPE_PRINTER,
    DEVICE_TYPE_SG,
    DEVICE_TYPE_CD,
    DEVICE_TYPE_MODEM,
    DEVICE_TYPE_BECEEM
};

extern char *get_usb_port_by_device(const char *device_name, char *buf, const int buf_size);
char *get_line_from_buffer(const char *buf, char *line, const int line_size){
    int buf_len, len;
    char *ptr;

    if(buf == NULL || (buf_len = strlen(buf)) <= 0)
        return NULL;

    if((ptr = strchr(buf, '\n')) == NULL)
        ptr = (char *)(buf+buf_len);

    if((len = ptr-buf) < 0)
        len = buf-ptr;
    ++len; // include '\n'.

    memset(line, 0, line_size);
    strncpy(line, buf, len);

    return line;
}

int isStorageDevice(const char *device_name){
    if(!strncmp(device_name, "sd", 2))
        return 1;

    return 0;
}

int get_device_type_by_device(const char *device_name)
{
    if(device_name == NULL || strlen(device_name) <= 0){
        usb_dbg("(%s): The device name is not correct.\n", device_name);
        return DEVICE_TYPE_UNKNOWN;
    }

    if(isStorageDevice(device_name)
#ifdef BCM_MMC
            || isMMCDevice(device_name) // SD card.
#endif
            ){
        return DEVICE_TYPE_DISK;
    }
#ifdef RTCONFIG_USB_PRINTER
    if(!strncmp(device_name, "lp", 2)){
        return DEVICE_TYPE_PRINTER;
    }
#endif
#ifdef RTCONFIG_USB_MODEM
    if(!strncmp(device_name, "sg", 2)){
        return DEVICE_TYPE_SG;
    }
    if(!strncmp(device_name, "sr", 2)){
        return DEVICE_TYPE_CD;
    }
    if(isTTYNode(device_name)){
        return DEVICE_TYPE_MODEM;
    }
#endif
#ifdef RTCONFIG_USB_BECEEM
    if(isBeceemNode(device_name)){
        return DEVICE_TYPE_BECEEM;
    }
#endif

    return DEVICE_TYPE_UNKNOWN;
}

int is_disk_name(const char *device_name){
    if(get_device_type_by_device(device_name) != DEVICE_TYPE_DISK)
        return 0;

#ifdef BCM_MMC
    if(isMMCDevice(device_name)){
        if(strchr(device_name, 'p'))
            return 0;
    }
    else
#endif
    if(isdigit(device_name[strlen(device_name)-1]))
        return 0;

    return 1;
}

int get_disk_partitionnumber(const char *string, u32 *partition_number, u32 *mounted_number){
    char disk_name[16];
    char target_path[128];
    DIR *dp;
    struct dirent *file;
    int len;
    char *mount_info = NULL, target[8];

    if(string == NULL)
        return 0;

    if(partition_number == NULL)
        return 0;

    *partition_number = 0; // initial value.
    if(mounted_number != NULL){
        *mounted_number = 0; // initial value.
        mount_info = read_whole_file(MOUNT_FILE);
    }

    len = strlen(string);
    if(!is_disk_name(string)){
        while(isdigit(string[len-1]))
            --len;

#ifdef BCM_MMC
        if(string[len-1] == 'p')
            --len;
#endif
    }
    else if(mounted_number != NULL && mount_info != NULL){
        memset(target, 0, 8);
        sprintf(target, "%s ", string);
        if(strstr(mount_info, target) != NULL)
            ++(*mounted_number);
    }
    memset(disk_name, 0, 16);
    strncpy(disk_name, string, len);

    memset(target_path, 0, 128);
    sprintf(target_path, "%s/%s", SYS_BLOCK, disk_name);
    if((dp = opendir(target_path)) == NULL){
        if(mount_info != NULL)
            free(mount_info);
        return 0;
    }

    len = strlen(disk_name);
    while((file = readdir(dp)) != NULL){
        if(file->d_name[0] == '.')
            continue;

        if(!strncmp(file->d_name, disk_name, len)){
            ++(*partition_number);

            if(mounted_number == NULL || mount_info == NULL)
                continue;

            memset(target, 0, 8);
            sprintf(target, "%s ", file->d_name);
            if(strstr(mount_info, target) != NULL)
                ++(*mounted_number);
        }
    }
    closedir(dp);
    if(mount_info != NULL)
        free(mount_info);

    return 1;
}

int is_partition_name(const char *device_name, u32 *partition_order){
    int order;
    u32 partition_number;

    if(partition_order != NULL)
        *partition_order = 0;

    if(get_device_type_by_device(device_name) != DEVICE_TYPE_DISK)
        return 0;

    // get the partition number in the device_name
#ifdef BCM_MMC
    if(isMMCDevice(device_name)) // SD card: mmcblk0p1.
        order = (u32)strtol(device_name+8, NULL, 10);
    else // sda1.
#endif
        order = (u32)strtol(device_name+3, NULL, 10);
    if(order <= 0 || order == LONG_MIN || order == LONG_MAX)
        return 0;

    if(!get_disk_partitionnumber(device_name, &partition_number, NULL))
        return 0;

    if(partition_order != NULL)
        *partition_order = order;

    return 1;
}

void free_partition_data(partition_info_t **partition_info_list){
    partition_info_t *follow_partition, *old_partition;

    if(partition_info_list == NULL)
        return;

    follow_partition = *partition_info_list;
    while(follow_partition != NULL){
        if(follow_partition->device != NULL)
            free(follow_partition->device);
        if(follow_partition->label != NULL)
            free(follow_partition->label);
        if(follow_partition->mount_point != NULL)
            free(follow_partition->mount_point);
        if(follow_partition->file_system != NULL)
            free(follow_partition->file_system);
        if(follow_partition->permission != NULL)
            free(follow_partition->permission);

        follow_partition->disk = NULL;

        old_partition = follow_partition;
        follow_partition = follow_partition->next;
        free(old_partition);
    }
}

extern partition_info_t *initial_part_data(partition_info_t **part_info_list){
    partition_info_t *follow_part;

    if(part_info_list == NULL)
        return NULL;

    *part_info_list = (partition_info_t *)malloc(sizeof(partition_info_t));
    if(*part_info_list == NULL)
        return NULL;

    follow_part = *part_info_list;

    follow_part->device = NULL;
    follow_part->label = NULL;
    follow_part->partition_order = (u32)0;
    follow_part->mount_point = NULL;
    follow_part->file_system = NULL;
    follow_part->permission = NULL;
    follow_part->size_in_kilobytes = (u64)0;
    follow_part->used_kilobytes = (u64)0;
    follow_part->disk = NULL;
    follow_part->next = NULL;

    return follow_part;
}
#define SB_BUFFER_SIZE				0x11000
void volume_id_free_buffer(struct volume_id *id)
{
    free(id->sbbuf);
    id->sbbuf = NULL;
    id->sbbuf_len = 0;
    free(id->seekbuf);
    id->seekbuf = NULL;
    id->seekbuf_len = 0;
    id->seekbuf_off = 0; /* paranoia */
}

#define LARGEST_PAGESIZE			0x4000
#if __GNUC_PREREQ(3,0) && defined(i386) /* || defined(__x86_64__)? */
/* stdcall makes callee to pop arguments from stack, not caller */
# define FAST_FUNC __attribute__((regparm(3),stdcall))
/* #elif ... - add your favorite arch today! */
#else
# define FAST_FUNC
#endif
#define ENABLE_FEATURE_SYSLOG 0
enum {
    LOGMODE_NONE = 0,
    LOGMODE_STDIO = (1 << 0),
    LOGMODE_SYSLOG = (1 << 1) * ENABLE_FEATURE_SYSLOG,
    LOGMODE_BOTH = LOGMODE_SYSLOG + LOGMODE_STDIO,
};
/* Size-saving "small" ints (arch-dependent) */
#if defined(i386) || defined(__x86_64__) || defined(__mips__) || defined(__cris__)
/* add other arches which benefit from this... */
typedef signed char smallint;
typedef unsigned char smalluint;
#else
/* for arches where byte accesses generate larger code: */
typedef int smallint;
typedef unsigned smalluint;
#endif

smallint logmode = LOGMODE_STDIO;
const char *msg_eol = "\n";
const char *applet_name;

int FAST_FUNC fflush_all(void)
{
    return fflush(NULL);
}
ssize_t FAST_FUNC safe_write(int fd, const void *buf, size_t count)
{
    ssize_t n;

    do {
        n = write(fd, buf, count);
    } while (n < 0 && errno == EINTR);

    return n;
}

/*
 * Write all of the supplied buffer out to a file.
 * This does multiple writes as necessary.
 * Returns the amount written, or -1 on an error.
 */
ssize_t FAST_FUNC full_write(int fd, const void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len) {
        cc = safe_write(fd, buf, len);

        if (cc < 0) {
            if (total) {
                /* we already wrote some! */
                /* user can do another write to know the error code */
                return total;
            }
            return cc;	/* write() returns -1 on failure. */
        }

        total += cc;
        buf = ((const char *)buf) + cc;
        len -= cc;
    }

    return total;
}

void FAST_FUNC bb_verror_msg(const char *s, va_list p, const char* strerr)
{
    char *msg, *msg1;
    int applet_len, strerr_len, msgeol_len, used;

    if (!logmode)
        return;

    if (!s) /* nomsg[_and_die] uses NULL fmt */
        s = ""; /* some libc don't like printf(NULL) */

    used = vasprintf(&msg, s, p);
    if (used < 0)
        return;

    /* This is ugly and costs +60 bytes compared to multiple
     * fprintf's, but is guaranteed to do a single write.
     * This is needed for e.g. httpd logging, when multiple
     * children can produce log messages simultaneously. */

    applet_len = strlen(applet_name) + 2; /* "applet: " */
    strerr_len = strerr ? strlen(strerr) : 0;
    msgeol_len = strlen(msg_eol);
    /* can't use xrealloc: it calls error_msg on failure,
     * that may result in a recursion */
    /* +3 is for ": " before strerr and for terminating NUL */
    msg1 = realloc(msg, applet_len + used + strerr_len + msgeol_len + 3);
    if (!msg1) {
        msg[used++] = '\n'; /* overwrites NUL */
        applet_len = 0;
    } else {
        msg = msg1;
        /* TODO: maybe use writev instead of memmoving? Need full_writev? */
        memmove(msg + applet_len, msg, used);
        used += applet_len;
        strcpy(msg, applet_name);
        msg[applet_len - 2] = ':';
        msg[applet_len - 1] = ' ';
        if (strerr) {
            if (s[0]) { /* not perror_nomsg? */
                msg[used++] = ':';
                msg[used++] = ' ';
            }
            strcpy(&msg[used], strerr);
            used += strerr_len;
        }
        strcpy(&msg[used], msg_eol);
        used += msgeol_len;
    }

    if (logmode & LOGMODE_STDIO) {
        fflush_all();
        full_write(STDERR_FILENO, msg, used);
    }
#if ENABLE_FEATURE_SYSLOG
    if (logmode & LOGMODE_SYSLOG) {
        syslog(LOG_ERR, "%s", msg + applet_len);
    }
#endif
    free(msg);
}

#define ENABLE_FEATURE_PREFER_APPLETS 0
#define ENABLE_HUSH 0

int die_sleep;
jmp_buf die_jmp;
int xfunc_error_retval;

#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_HUSH
jmp_buf die_jmp;
#endif

void FAST_FUNC xfunc_die(void)
{
    if (die_sleep) {
        if ((ENABLE_FEATURE_PREFER_APPLETS || ENABLE_HUSH)
         && die_sleep < 0
        ) {
            /* Special case. We arrive here if NOFORK applet
             * calls xfunc, which then decides to die.
             * We don't die, but jump instead back to caller.
             * NOFORK applets still cannot carelessly call xfuncs:
             * p = xmalloc(10);
             * q = xmalloc(10); // BUG! if this dies, we leak p!
             */
            /* -2222 means "zero" (longjmp can't pass 0)
             * run_nofork_applet() catches -2222. */
            longjmp(die_jmp, xfunc_error_retval ? xfunc_error_retval : -2222);
        }
        sleep(die_sleep);
    }
    exit(xfunc_error_retval);
}

void FAST_FUNC bb_error_msg_and_die(const char *s, ...)
{
    va_list p;

    va_start(p, s);
    bb_verror_msg(s, p, NULL);
    va_end(p);
    xfunc_die();
}
#if 1 /* if needed: !defined(arch1) && !defined(arch2) */
# define ALIGN1 __attribute__((aligned(1)))
# define ALIGN2 __attribute__((aligned(2)))
# define ALIGN4 __attribute__((aligned(4)))
#else
/* Arches which MUST have 2 or 4 byte alignment for everything are here */
# define ALIGN1
# define ALIGN2
# define ALIGN4
#endif
const char bb_msg_memory_exhausted[] ALIGN1 = "memory exhausted";
// Die if we can't allocate size bytes of memory.
void* FAST_FUNC xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0)
        bb_error_msg_and_die(bb_msg_memory_exhausted);
    return ptr;
}

#if defined(__GLIBC__)
/* glibc uses __errno_location() to get a ptr to errno */
/* We can just memorize it once - no multithreading in busybox :) */
//extern int *const bb_errno;
int *bb_errno;
#undef errno
#define errno (*bb_errno)
#endif
// Die if we can't resize previously allocated memory.  (This returns a pointer
// to the new memory, which may or may not be the same as the old memory.
// It'll copy the contents to a new chunk and free the old one if necessary.)
void* FAST_FUNC xrealloc2(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL && size != 0)
        bb_error_msg_and_die(bb_msg_memory_exhausted);
    return ptr;
}
ssize_t FAST_FUNC safe_read(int fd, void *buf, size_t count)
{
    ssize_t n;

    do {
        n = read(fd, buf, count);
    } while (n < 0 && errno == EINTR);

    return n;
}
/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
ssize_t FAST_FUNC full_read(int fd, void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len) {
        cc = safe_read(fd, buf, len);

        if (cc < 0) {
            if (total) {
                /* we already have some! */
                /* user can do another read to know the error code */
                return total;
            }
            return cc; /* read() returns -1 on failure. */
        }
        if (cc == 0)
            break;
        buf = ((char *)buf) + cc;
        total += cc;
        len -= cc;
    }

    return total;
}

#if !defined(BCMARM) && !defined(QCA)
#define XMALLOC		xmalloc
#define XREALLOC 	xrealloc2
#define FULL_READ	full_read
#else
#define XMALLOC		malloc
#define XREALLOC	 realloc
#define FULL_READ	read
#endif

#define SEEK_BUFFER_SIZE			0x10000
#define dbg(...) ((void)0)
enum uuid_format {
    UUID_DCE_STRING,
    UUID_DCE,
    UUID_DOS,
    UUID_NTFS,
    UUID_HFS,
};
enum endian {
    LE = 0,
    BE = 1
};
//#ifdef UTIL2
void volume_id_set_unicode16(char *str, size_t len, const uint8_t *buf, enum endian endianess, size_t count)
{
    unsigned i, j;
    unsigned c;

    j = 0;
    for (i = 0; i + 2 <= count; i += 2) {
        if (endianess == LE)
            c = (buf[i+1] << 8) | buf[i];
        else
            c = (buf[i] << 8) | buf[i+1];
        if (c == 0) {
            str[j] = '\0';
            break;
        } else if (c < 0x80) {
            if (j+1 >= len)
                break;
            str[j++] = (uint8_t) c;
        } else if (c < 0x800) {
            if (j+2 >= len)
                break;
            str[j++] = (uint8_t) (0xc0 | (c >> 6));
            str[j++] = (uint8_t) (0x80 | (c & 0x3f));
        } else {
            if (j+3 >= len)
                break;
            str[j++] = (uint8_t) (0xe0 | (c >> 12));
            str[j++] = (uint8_t) (0x80 | ((c >> 6) & 0x3f));
            str[j++] = (uint8_t) (0x80 | (c & 0x3f));
        }
    }
    str[j] = '\0';
}
//#endif

void volume_id_set_label_string(struct volume_id *id, const uint8_t *buf, size_t count)
{
    unsigned i;

    memcpy(id->label, buf, count);

    /* remove trailing whitespace */
    i = strnlen(id->label, count);
    while (i--) {
        if (!isspace(id->label[i]))
            break;
    }
    id->label[i+1] = '\0';
}
void volume_id_set_label_unicode16(struct volume_id *id, const uint8_t *buf, enum endian endianess, size_t count)
{
     volume_id_set_unicode16(id->label, sizeof(id->label), buf, endianess, count);
}
void volume_id_set_uuid(struct volume_id *id, const uint8_t *buf, enum uuid_format format)
{
    unsigned i;
    unsigned count = 0;

    switch (format) {
    case UUID_DOS:
        count = 4;
        break;
    case UUID_NTFS:
    case UUID_HFS:
        count = 8;
        break;
    case UUID_DCE:
        count = 16;
        break;
    case UUID_DCE_STRING:
        /* 36 is ok, id->uuid has one extra byte for NUL */
        count = VOLUME_ID_UUID_SIZE;
        break;
    }
//	memcpy(id->uuid_raw, buf, count);
//	id->uuid_raw_len = count;

    /* if set, create string in the same format, the native platform uses */
    for (i = 0; i < count; i++)
        if (buf[i] != 0)
            goto set;
    return; /* all bytes are zero, leave it empty ("") */

set:
    switch (format) {
    case UUID_DOS:
        sprintf(id->uuid, "%02X%02X-%02X%02X",
            buf[3], buf[2], buf[1], buf[0]);
        break;
    case UUID_NTFS:
        sprintf(id->uuid, "%02X%02X%02X%02X%02X%02X%02X%02X",
            buf[7], buf[6], buf[5], buf[4],
            buf[3], buf[2], buf[1], buf[0]);
        break;
    case UUID_HFS:
        sprintf(id->uuid, "%02X%02X%02X%02X%02X%02X%02X%02X",
            buf[0], buf[1], buf[2], buf[3],
            buf[4], buf[5], buf[6], buf[7]);
        break;
    case UUID_DCE:
        sprintf(id->uuid,
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            buf[0], buf[1], buf[2], buf[3],
            buf[4], buf[5],
            buf[6], buf[7],
            buf[8], buf[9],
            buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
        break;
    case UUID_DCE_STRING:
        memcpy(id->uuid, buf, count);
        id->uuid[count] = '\0';
        break;
    }
}
/* Do not use xlseek here. With it, single corrupted filesystem
 * may result in attempt to seek past device -> exit.
 * It's better to ignore such fs and continue.  */
void *volume_id_get_buffer(struct volume_id *id, uint64_t off, size_t len)
{
    uint8_t *dst;
    unsigned small_off;
    ssize_t read_len;

    dbg("get buffer off 0x%llx(%llu), len 0x%zx",
        (unsigned long long) off, (unsigned long long) off, len);

    /* check if requested area fits in superblock buffer */
    if (off + len <= SB_BUFFER_SIZE
     /* && off <= SB_BUFFER_SIZE - want this paranoid overflow check? */
    ) {
        if (id->sbbuf == NULL) {
            id->sbbuf = XMALLOC(SB_BUFFER_SIZE);
        }
        small_off = off;
        dst = id->sbbuf;

        /* check if we need to read */
        len += off;
        if (len <= id->sbbuf_len)
            goto ret; /* we already have it */

        dbg("read sbbuf len:0x%x", (unsigned) len);
        id->sbbuf_len = len;
        off = 0;
        goto do_read;
    }

    if (len > SEEK_BUFFER_SIZE) {
        dbg("seek buffer too small %d", SEEK_BUFFER_SIZE);
        return NULL;
    }
    dst = id->seekbuf;

    /* check if we need to read */
    if ((off >= id->seekbuf_off)
     && ((off + len) <= (id->seekbuf_off + id->seekbuf_len))
    ) {
        small_off = off - id->seekbuf_off; /* can't overflow */
        goto ret; /* we already have it */
    }

    id->seekbuf_off = off;
    id->seekbuf_len = len;
    id->seekbuf = XREALLOC(id->seekbuf, len);
    small_off = 0;
    dst = id->seekbuf;
    dbg("read seekbuf off:0x%llx len:0x%zx",
                (unsigned long long) off, len);
 do_read:
    if (lseek(id->fd, off, SEEK_SET) != off) {
        dbg("seek(0x%llx) failed", (unsigned long long) off);
        goto err;
    }
    read_len = FULL_READ(id->fd, dst, len);
    if (read_len != len) {
        dbg("requested 0x%x bytes, got 0x%x bytes",
                (unsigned) len, (unsigned) read_len);
 err:
        /* No filesystem can be this tiny. It's most likely
         * non-associated loop device, empty drive and so on.
         * Flag it, making it possible to short circuit future
         * accesses. Rationale:
         * users complained of slow blkid due to empty floppy drives.
         */
        if (off < 64*1024)
            id->error = 1;
        /* id->seekbuf_len or id->sbbuf_len is wrong now! Fixing. */
        volume_id_free_buffer(id);
        return NULL;
    }
 ret:
    return dst + small_off;
}

#define PACKED __attribute__ ((__packed__))

struct swap_header_v1_2 {
    uint8_t		bootbits[1024];
    uint32_t	version;
    uint32_t	last_page;
    uint32_t	nr_badpages;
    uint8_t		uuid[16];
    uint8_t		volume_name[16];
} PACKED;


int FAST_FUNC volume_id_probe_linux_swap(struct volume_id *id /*,uint64_t off*/)
{
#define off ((uint64_t)0)
    struct swap_header_v1_2 *sw;
    const uint8_t *buf;
    unsigned page;

    dbg("probing at offset 0x%llx", (unsigned long long) off);

    /* the swap signature is at the end of the PAGE_SIZE */
    for (page = 0x1000; page <= LARGEST_PAGESIZE; page <<= 1) {
            buf = volume_id_get_buffer(id, off + page-10, 10);
            if (buf == NULL)
                return -1;

            if (memcmp(buf, "SWAP-SPACE", 10) == 0) {
//				id->type_version[0] = '1';
//				id->type_version[1] = '\0';
                goto found;
            }

            if (memcmp(buf, "SWAPSPACE2", 10) == 0
             || memcmp(buf, "S1SUSPEND", 9) == 0
             || memcmp(buf, "S2SUSPEND", 9) == 0
             || memcmp(buf, "ULSUSPEND", 9) == 0
            ) {
                sw = volume_id_get_buffer(id, off, sizeof(struct swap_header_v1_2));
                if (sw == NULL)
                    return -1;
//				id->type_version[0] = '2';
//				id->type_version[1] = '\0';
//				volume_id_set_label_raw(id, sw->volume_name, 16);
                volume_id_set_label_string(id, sw->volume_name, 16);
                volume_id_set_uuid(id, sw->uuid, UUID_DCE);
                goto found;
            }
    }
    return -1;

found:
//	volume_id_set_usage(id, VOLUME_ID_OTHER);
//	id->type = "swap";

    return 0;
}

struct ext2_super_block {
    uint32_t	inodes_count;
    uint32_t	blocks_count;
    uint32_t	r_blocks_count;
    uint32_t	free_blocks_count;
    uint32_t	free_inodes_count;
    uint32_t	first_data_block;
    uint32_t	log_block_size;
    uint32_t	dummy3[7];
    uint8_t	magic[2];
    uint16_t	state;
    uint32_t	dummy5[8];
    uint32_t	feature_compat;
    uint32_t	feature_incompat;
    uint32_t	feature_ro_compat;
    uint8_t	uuid[16];
    uint8_t	volume_name[16];
} PACKED;

#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x00000004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x00000008
#define EXT_SUPERBLOCK_OFFSET			0x400

int FAST_FUNC volume_id_probe_ext(struct volume_id *id /*,uint64_t off*/)
{
#define off ((uint64_t)0)
    struct ext2_super_block *es;

    dbg("ext: probing at offset 0x%llx", (unsigned long long) off);

    es = volume_id_get_buffer(id, off + EXT_SUPERBLOCK_OFFSET, 0x200);
    if (es == NULL)
        return -1;

    if (es->magic[0] != 0123 || es->magic[1] != 0357) {
        dbg("ext: no magic found");
        return -1;
    }

//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	volume_id_set_label_raw(id, es->volume_name, 16);
    volume_id_set_label_string(id, es->volume_name, 16);
    volume_id_set_uuid(id, es->uuid, UUID_DCE);
    dbg("ext: label '%s' uuid '%s'", id->label, id->uuid);

//	if ((le32_to_cpu(es->feature_compat) & EXT3_FEATURE_COMPAT_HAS_JOURNAL) != 0)
//		id->type = "ext3";
//	else
//		id->type = "ext2";

    return 0;
}

struct vfat_super_block {
    uint8_t		boot_jump[3];
    uint8_t		sysid[8];
    uint16_t	sector_size_bytes;
    uint8_t		sectors_per_cluster;
    uint16_t	reserved_sct;
    uint8_t		fats;
    uint16_t	dir_entries;
    uint16_t	sectors;
    uint8_t		media;
    uint16_t	fat_length;
    uint16_t	secs_track;
    uint16_t	heads;
    uint32_t	hidden;
    uint32_t	total_sect;
    union {
        struct fat_super_block {
            uint8_t		unknown[3];
            uint8_t		serno[4];
            uint8_t		label[11];
            uint8_t		magic[8];
            uint8_t		dummy2[192];
            uint8_t		pmagic[2];
        } PACKED fat;
        struct fat32_super_block {
            uint32_t	fat32_length;
            uint16_t	flags;
            uint8_t		version[2];
            uint32_t	root_cluster;
            uint16_t	insfo_sector;
            uint16_t	backup_boot;
            uint16_t	reserved2[6];
            uint8_t		unknown[3];
            uint8_t		serno[4];
            uint8_t		label[11];
            uint8_t		magic[8];
            uint8_t		dummy2[164];
            uint8_t		pmagic[2];
        } PACKED fat32;
    } PACKED type;
} PACKED;
struct vfat_dir_entry {
    uint8_t		name[11];
    uint8_t		attr;
    uint16_t	time_creat;
    uint16_t	date_creat;
    uint16_t	time_acc;
    uint16_t	date_acc;
    uint16_t	cluster_high;
    uint16_t	time_write;
    uint16_t	date_write;
    uint16_t	cluster_low;
    uint32_t	size;
} PACKED;
#define bswap16(x) (uint16_t)	( \
                (((uint16_t)(x) & 0x00ffu) << 8) | \
                (((uint16_t)(x) & 0xff00u) >> 8))

#define bswap32(x) (uint32_t)	( \
                (((uint32_t)(x) & 0xff000000u) >> 24) | \
                (((uint32_t)(x) & 0x00ff0000u) >>  8) | \
                (((uint32_t)(x) & 0x0000ff00u) <<  8) | \
                (((uint32_t)(x) & 0x000000ffu) << 24))

#define bswap64(x) (uint64_t)	( \
                (((uint64_t)(x) & 0xff00000000000000ull) >> 56) | \
                (((uint64_t)(x) & 0x00ff000000000000ull) >> 40) | \
                (((uint64_t)(x) & 0x0000ff0000000000ull) >> 24) | \
                (((uint64_t)(x) & 0x000000ff00000000ull) >>  8) | \
                (((uint64_t)(x) & 0x00000000ff000000ull) <<  8) | \
                (((uint64_t)(x) & 0x0000000000ff0000ull) << 24) | \
                (((uint64_t)(x) & 0x000000000000ff00ull) << 40) | \
                (((uint64_t)(x) & 0x00000000000000ffull) << 56))
#define FAT16_MAX			0xfff4
#define FAT32_MAX			0x0ffffff6
#define FAT_ATTR_VOLUME_ID		0x08
#define FAT_ATTR_DIR			0x10
#define FAT_ATTR_LONG_NAME		0x0f
#define FAT_ATTR_MASK			0x3f
#define FAT_ENTRY_FREE			0xe5

#if defined(__BIG_ENDIAN__) && __BIG_ENDIAN__
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) || defined(__386__)
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#else
# error "Can't determine endianness"
#endif

#if BB_LITTLE_ENDIAN
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)
#define be16_to_cpu(x) bswap16(x)
#define be32_to_cpu(x) bswap32(x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_be32(x) bswap32(x)
#else
#define le16_to_cpu(x) bswap16(x)
#define le32_to_cpu(x) bswap32(x)
#define le64_to_cpu(x) bswap64(x)
#define be16_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define cpu_to_le16(x) bswap16(x)
#define cpu_to_le32(x) bswap32(x)
#define cpu_to_be32(x) (x)
#endif

static uint8_t *get_attr_volume_id(struct vfat_dir_entry *dir, int count)
{
    for (;--count >= 0; dir++) {
        /* end marker */
        if (dir->name[0] == 0x00) {
            dbg("end of dir");
            break;
        }

        /* empty entry */
        if (dir->name[0] == FAT_ENTRY_FREE)
            continue;

        /* long name */
        if ((dir->attr & FAT_ATTR_MASK) == FAT_ATTR_LONG_NAME)
            continue;

        if ((dir->attr & (FAT_ATTR_VOLUME_ID | FAT_ATTR_DIR)) == FAT_ATTR_VOLUME_ID) {
            /* labels do not have file data */
            if (dir->cluster_high != 0 || dir->cluster_low != 0)
                continue;

            dbg("found ATTR_VOLUME_ID id in root dir");
            return dir->name;
        }

        dbg("skip dir entry");
    }

    return NULL;
}
int FAST_FUNC volume_id_probe_vfat(struct volume_id *id /*,uint64_t fat_partition_off*/)
{
#define fat_partition_off ((uint64_t)0)
    struct vfat_super_block *vs;
    struct vfat_dir_entry *dir;
    uint16_t sector_size_bytes;
    uint16_t dir_entries;
    uint32_t sect_count;
    uint16_t reserved_sct;
    uint32_t fat_size_sct;
    uint32_t root_cluster;
    uint32_t dir_size_sct;
    uint32_t cluster_count;
    uint64_t root_start_off;
    uint32_t start_data_sct;
    uint8_t *buf;
    uint32_t buf_size;
    uint8_t *label = NULL;
    uint32_t next_cluster;
    int maxloop;

    dbg("probing at offset 0x%llx", (unsigned long long) fat_partition_off);

    vs = volume_id_get_buffer(id, fat_partition_off, 0x200);
    if (vs == NULL)
        return -1;

    /* believe only that's fat, don't trust the version
     * the cluster_count will tell us
     */
    if (memcmp(vs->sysid, "NTFS", 4) == 0)
        return -1;

    if (memcmp(vs->type.fat32.magic, "MSWIN", 5) == 0)
        goto valid;

    if (memcmp(vs->type.fat32.magic, "FAT32   ", 8) == 0)
        goto valid;

    if (memcmp(vs->type.fat.magic, "FAT16   ", 8) == 0)
        goto valid;

    if (memcmp(vs->type.fat.magic, "MSDOS", 5) == 0)
        goto valid;

    if (memcmp(vs->type.fat.magic, "FAT12   ", 8) == 0)
        goto valid;

    /*
     * There are old floppies out there without a magic, so we check
     * for well known values and guess if it's a fat volume
     */

    /* boot jump address check */
    if ((vs->boot_jump[0] != 0xeb || vs->boot_jump[2] != 0x90)
     && vs->boot_jump[0] != 0xe9
    ) {
        return -1;
    }

    /* heads check */
    if (vs->heads == 0)
        return -1;

    /* cluster size check */
    if (vs->sectors_per_cluster == 0
     || (vs->sectors_per_cluster & (vs->sectors_per_cluster-1))
    ) {
        return -1;
    }

    /* media check */
    if (vs->media < 0xf8 && vs->media != 0xf0)
        return -1;

    /* fat count*/
    if (vs->fats != 2)
        return -1;

 valid:
    /* sector size check */
    sector_size_bytes = le16_to_cpu(vs->sector_size_bytes);
    if (sector_size_bytes != 0x200 && sector_size_bytes != 0x400
     && sector_size_bytes != 0x800 && sector_size_bytes != 0x1000
    ) {
        return -1;
    }

    dbg("sector_size_bytes 0x%x", sector_size_bytes);
    dbg("sectors_per_cluster 0x%x", vs->sectors_per_cluster);

    reserved_sct = le16_to_cpu(vs->reserved_sct);
    dbg("reserved_sct 0x%x", reserved_sct);

    sect_count = le16_to_cpu(vs->sectors);
    if (sect_count == 0)
        sect_count = le32_to_cpu(vs->total_sect);
    dbg("sect_count 0x%x", sect_count);

    fat_size_sct = le16_to_cpu(vs->fat_length);
    if (fat_size_sct == 0)
        fat_size_sct = le32_to_cpu(vs->type.fat32.fat32_length);
    fat_size_sct *= vs->fats;
    dbg("fat_size_sct 0x%x", fat_size_sct);

    dir_entries = le16_to_cpu(vs->dir_entries);
    dir_size_sct = ((dir_entries * sizeof(struct vfat_dir_entry)) +
            (sector_size_bytes-1)) / sector_size_bytes;
    dbg("dir_size_sct 0x%x", dir_size_sct);

    cluster_count = sect_count - (reserved_sct + fat_size_sct + dir_size_sct);
    cluster_count /= vs->sectors_per_cluster;
    dbg("cluster_count 0x%x", cluster_count);

//	if (cluster_count < FAT12_MAX) {
//		strcpy(id->type_version, "FAT12");
//	} else if (cluster_count < FAT16_MAX) {
//		strcpy(id->type_version, "FAT16");
//	} else {
//		strcpy(id->type_version, "FAT32");
//		goto fat32;
//	}
    if (cluster_count >= FAT16_MAX)
        goto fat32;

    /* the label may be an attribute in the root directory */
    root_start_off = (reserved_sct + fat_size_sct) * sector_size_bytes;
    dbg("root dir start 0x%llx", (unsigned long long) root_start_off);
    dbg("expected entries 0x%x", dir_entries);

    buf_size = dir_entries * sizeof(struct vfat_dir_entry);
    buf = volume_id_get_buffer(id, fat_partition_off + root_start_off, buf_size);
    if (buf == NULL)
        goto ret;

    label = get_attr_volume_id((struct vfat_dir_entry*) buf, dir_entries);

    vs = volume_id_get_buffer(id, fat_partition_off, 0x200);
    if (vs == NULL)
        return -1;

    if (label != NULL && memcmp(label, "NO NAME    ", 11) != 0) {
//		volume_id_set_label_raw(id, label, 11);
        volume_id_set_label_string(id, label, 11);
    } else if (memcmp(vs->type.fat.label, "NO NAME    ", 11) != 0) {
//		volume_id_set_label_raw(id, vs->type.fat.label, 11);
        volume_id_set_label_string(id, vs->type.fat.label, 11);
    }
    volume_id_set_uuid(id, vs->type.fat.serno, UUID_DOS);
    goto ret;

 fat32:
    /* FAT32 root dir is a cluster chain like any other directory */
    buf_size = vs->sectors_per_cluster * sector_size_bytes;
    root_cluster = le32_to_cpu(vs->type.fat32.root_cluster);
    start_data_sct = reserved_sct + fat_size_sct;

    next_cluster = root_cluster;
    maxloop = 100;
    while (--maxloop) {
        uint64_t next_off_sct;
        uint64_t next_off;
        uint64_t fat_entry_off;
        int count;

        dbg("next_cluster 0x%x", (unsigned)next_cluster);
        next_off_sct = (uint64_t)(next_cluster - 2) * vs->sectors_per_cluster;
        next_off = (start_data_sct + next_off_sct) * sector_size_bytes;
        dbg("cluster offset 0x%llx", (unsigned long long) next_off);

        /* get cluster */
        buf = volume_id_get_buffer(id, fat_partition_off + next_off, buf_size);
        if (buf == NULL)
            goto ret;

        dir = (struct vfat_dir_entry*) buf;
        count = buf_size / sizeof(struct vfat_dir_entry);
        dbg("expected entries 0x%x", count);

        label = get_attr_volume_id(dir, count);
        if (label)
            break;

        /* get FAT entry */
        fat_entry_off = (reserved_sct * sector_size_bytes) + (next_cluster * sizeof(uint32_t));
        dbg("fat_entry_off 0x%llx", (unsigned long long)fat_entry_off);
        buf = volume_id_get_buffer(id, fat_partition_off + fat_entry_off, buf_size);
        if (buf == NULL)
            goto ret;

        /* set next cluster */
        next_cluster = le32_to_cpu(*(uint32_t*)buf) & 0x0fffffff;
        if (next_cluster < 2 || next_cluster > FAT32_MAX)
            break;
    }
    if (maxloop == 0)
        dbg("reached maximum follow count of root cluster chain, give up");

    vs = volume_id_get_buffer(id, fat_partition_off, 0x200);
    if (vs == NULL)
        return -1;

    if (label != NULL && memcmp(label, "NO NAME    ", 11) != 0) {
//		volume_id_set_label_raw(id, label, 11);
        volume_id_set_label_string(id, label, 11);
    } else if (memcmp(vs->type.fat32.label, "NO NAME    ", 11) != 0) {
//		volume_id_set_label_raw(id, vs->type.fat32.label, 11);
        volume_id_set_label_string(id, vs->type.fat32.label, 11);
    }
    volume_id_set_uuid(id, vs->type.fat32.serno, UUID_DOS);

 ret:
//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	id->type = "vfat";

    return 0;
}

#define MFT_RECORD_ATTR_END			0xffffffffu
#define MFT_RECORD_VOLUME			3
#define MFT_RECORD_ATTR_VOLUME_NAME		0x60
struct master_file_table_record {
    uint8_t		magic[4];
    uint16_t	usa_ofs;
    uint16_t	usa_count;
    uint64_t	lsn;
    uint16_t	sequence_number;
    uint16_t	link_count;
    uint16_t	attrs_offset;
    uint16_t	flags;
    uint32_t	bytes_in_use;
    uint32_t	bytes_allocated;
} PACKED;
struct ntfs_super_block {
    uint8_t		jump[3];
    uint8_t		oem_id[8];
    uint16_t	bytes_per_sector;
    uint8_t		sectors_per_cluster;
    uint16_t	reserved_sectors;
    uint8_t		fats;
    uint16_t	root_entries;
    uint16_t	sectors;
    uint8_t		media_type;
    uint16_t	sectors_per_fat;
    uint16_t	sectors_per_track;
    uint16_t	heads;
    uint32_t	hidden_sectors;
    uint32_t	large_sectors;
    uint16_t	unused[2];
    uint64_t	number_of_sectors;
    uint64_t	mft_cluster_location;
    uint64_t	mft_mirror_cluster_location;
    int8_t		cluster_per_mft_record;
    uint8_t		reserved1[3];
    int8_t		cluster_per_index_record;
    uint8_t		reserved2[3];
    uint8_t		volume_serial[8];
    uint16_t	checksum;
} PACKED;
struct file_attribute {
    uint32_t	type;
    uint32_t	len;
    uint8_t		non_resident;
    uint8_t		name_len;
    uint16_t	name_offset;
    uint16_t	flags;
    uint16_t	instance;
    uint32_t	value_len;
    uint16_t	value_offset;
} PACKED;
int FAST_FUNC volume_id_probe_ntfs(struct volume_id *id /*,uint64_t off*/)
{
#define off ((uint64_t)0)
    unsigned sector_size;
    unsigned cluster_size;
    uint64_t mft_cluster;
    uint64_t mft_off;
    unsigned mft_record_size;
    unsigned attr_type;
    unsigned attr_off;
    unsigned attr_len;
    unsigned val_off;
    unsigned val_len;
    struct master_file_table_record *mftr;
    struct ntfs_super_block *ns;
    const uint8_t *buf;
    const uint8_t *val;

    dbg("probing at offset 0x%llx", (unsigned long long) off);

    ns = volume_id_get_buffer(id, off, 0x200);
    if (ns == NULL)
        return -1;

    if (memcmp(ns->oem_id, "NTFS", 4) != 0)
        return -1;

    volume_id_set_uuid(id, ns->volume_serial, UUID_NTFS);

    sector_size = le16_to_cpu(ns->bytes_per_sector);
    cluster_size = ns->sectors_per_cluster * sector_size;
    mft_cluster = le64_to_cpu(ns->mft_cluster_location);
    mft_off = mft_cluster * cluster_size;

    if (ns->cluster_per_mft_record < 0)
        /* size = -log2(mft_record_size); normally 1024 Bytes */
        mft_record_size = 1 << -ns->cluster_per_mft_record;
    else
        mft_record_size = ns->cluster_per_mft_record * cluster_size;

    dbg("sectorsize  0x%x", sector_size);
    dbg("clustersize 0x%x", cluster_size);
    dbg("mftcluster  %llu", (unsigned long long) mft_cluster);
    dbg("mftoffset  0x%llx", (unsigned long long) mft_off);
    dbg("cluster per mft_record  %i", ns->cluster_per_mft_record);
    dbg("mft record size  %i", mft_record_size);

    buf = volume_id_get_buffer(id, off + mft_off + (MFT_RECORD_VOLUME * mft_record_size),
             mft_record_size);
    if (buf == NULL)
        goto found;

    mftr = (struct master_file_table_record*) buf;

    dbg("mftr->magic '%c%c%c%c'", mftr->magic[0], mftr->magic[1], mftr->magic[2], mftr->magic[3]);
    if (memcmp(mftr->magic, "FILE", 4) != 0)
        goto found;

    attr_off = le16_to_cpu(mftr->attrs_offset);
    dbg("file $Volume's attributes are at offset %i", attr_off);

    while (1) {
        struct file_attribute *attr;

        attr = (struct file_attribute*) &buf[attr_off];
        attr_type = le32_to_cpu(attr->type);
        attr_len = le32_to_cpu(attr->len);
        val_off = le16_to_cpu(attr->value_offset);
        val_len = le32_to_cpu(attr->value_len);
        attr_off += attr_len;

        if (attr_len == 0)
            break;

        if (attr_off >= mft_record_size)
            break;

        if (attr_type == MFT_RECORD_ATTR_END)
            break;

        dbg("found attribute type 0x%x, len %i, at offset %i",
            attr_type, attr_len, attr_off);

//		if (attr_type == MFT_RECORD_ATTR_VOLUME_INFO) {
//			struct volume_info *info;
//			dbg("found info, len %i", val_len);
//			info = (struct volume_info*) (((uint8_t *) attr) + val_off);
//			snprintf(id->type_version, sizeof(id->type_version)-1,
//				 "%u.%u", info->major_ver, info->minor_ver);
//		}

        if (attr_type == MFT_RECORD_ATTR_VOLUME_NAME) {
            dbg("found label, len %i", val_len);
            if (val_len > VOLUME_ID_LABEL_SIZE)
                val_len = VOLUME_ID_LABEL_SIZE;

            val = ((uint8_t *) attr) + val_off;
//			volume_id_set_label_raw(id, val, val_len);
            volume_id_set_label_unicode16(id, val, LE, val_len);
        }
    }

 found:
//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	id->type = "ntfs";

    return 0;
}

struct hfs_finder_info{
    uint32_t	boot_folder;
    uint32_t	start_app;
    uint32_t	open_folder;
    uint32_t	os9_folder;
    uint32_t	reserved;
    uint32_t	osx_folder;
    uint8_t		id[8];
} PACKED;

struct hfs_mdb {
    uint8_t		signature[2];
    uint32_t	cr_date;
    uint32_t	ls_Mod;
    uint16_t	atrb;
    uint16_t	nm_fls;
    uint16_t	vbm_st;
    uint16_t	alloc_ptr;
    uint16_t	nm_al_blks;
    uint32_t	al_blk_size;
    uint32_t	clp_size;
    uint16_t	al_bl_st;
    uint32_t	nxt_cnid;
    uint16_t	free_bks;
    uint8_t		label_len;
    uint8_t		label[27];
    uint32_t	vol_bkup;
    uint16_t	vol_seq_num;
    uint32_t	wr_cnt;
    uint32_t	xt_clump_size;
    uint32_t	ct_clump_size;
    uint16_t	num_root_dirs;
    uint32_t	file_count;
    uint32_t	dir_count;
    struct hfs_finder_info finder_info;
    uint8_t		embed_sig[2];
    uint16_t	embed_startblock;
    uint16_t	embed_blockcount;
} PACKED;

struct hfsplus_bnode_descriptor {
    uint32_t	next;
    uint32_t	prev;
    uint8_t		type;
    uint8_t		height;
    uint16_t	num_recs;
    uint16_t	reserved;
} PACKED;

struct hfsplus_bheader_record {
    uint16_t	depth;
    uint32_t	root;
    uint32_t	leaf_count;
    uint32_t	leaf_head;
    uint32_t	leaf_tail;
    uint16_t	node_size;
} PACKED;

struct hfsplus_catalog_key {
    uint16_t	key_len;
    uint32_t	parent_id;
    uint16_t	unicode_len;
    uint8_t		unicode[255 * 2];
} PACKED;

struct hfsplus_extent {
    uint32_t	start_block;
    uint32_t	block_count;
} PACKED;

#define HFSPLUS_EXTENT_COUNT		8
struct hfsplus_fork {
    uint64_t	total_size;
    uint32_t	clump_size;
    uint32_t	total_blocks;
    struct hfsplus_extent extents[HFSPLUS_EXTENT_COUNT];
} PACKED;

struct hfsplus_vol_header {
    uint8_t		signature[2];
    uint16_t	version;
    uint32_t	attributes;
    uint32_t	last_mount_vers;
    uint32_t	reserved;
    uint32_t	create_date;
    uint32_t	modify_date;
    uint32_t	backup_date;
    uint32_t	checked_date;
    uint32_t	file_count;
    uint32_t	folder_count;
    uint32_t	blocksize;
    uint32_t	total_blocks;
    uint32_t	free_blocks;
    uint32_t	next_alloc;
    uint32_t	rsrc_clump_sz;
    uint32_t	data_clump_sz;
    uint32_t	next_cnid;
    uint32_t	write_count;
    uint64_t	encodings_bmp;
    struct hfs_finder_info finder_info;
    struct hfsplus_fork alloc_file;
    struct hfsplus_fork ext_file;
    struct hfsplus_fork cat_file;
    struct hfsplus_fork attr_file;
    struct hfsplus_fork start_file;
} PACKED;

#define HFS_SUPERBLOCK_OFFSET		0x400
#define HFS_NODE_LEAF			0xff
#define HFSPLUS_POR_CNID		1





int FAST_FUNC volume_id_probe_hfs_hfsplus(struct volume_id *id /*,uint64_t off*/)
{
    //uint64_t off = 0;
    uint64_t off2 = 0;
    unsigned blocksize;
    unsigned cat_block;
    unsigned ext_block_start;
    unsigned ext_block_count;
    int ext;
    unsigned leaf_node_head;
    unsigned leaf_node_count;
    unsigned leaf_node_size;
    unsigned leaf_block;
    uint64_t leaf_off;
    unsigned alloc_block_size;
    unsigned alloc_first_block;
    unsigned embed_first_block;
    unsigned record_count;
    struct hfsplus_vol_header *hfsplus;
    struct hfsplus_bnode_descriptor *descr;
    struct hfsplus_bheader_record *bnode;
    struct hfsplus_catalog_key *key;
    unsigned label_len;
    struct hfsplus_extent extents[HFSPLUS_EXTENT_COUNT];
    struct hfs_mdb *hfs;
    const uint8_t *buf;

    //dbg("probing at offset 0x%llx", (unsigned long long) off);
    dbg("probing at offset 0x%llx", (unsigned long long) off2);

    //buf = volume_id_get_buffer(id, off + HFS_SUPERBLOCK_OFFSET, 0x200);
    buf = volume_id_get_buffer(id, off2 + HFS_SUPERBLOCK_OFFSET, 0x200);
    if (buf == NULL)
        return -1;

    hfs = (struct hfs_mdb *) buf;
    if (hfs->signature[0] != 'B' || hfs->signature[1] != 'D')
        goto checkplus;

    /* it may be just a hfs wrapper for hfs+ */
    if (hfs->embed_sig[0] == 'H' && hfs->embed_sig[1] == '+') {
        alloc_block_size = be32_to_cpu(hfs->al_blk_size);
        dbg("alloc_block_size 0x%x", alloc_block_size);

        alloc_first_block = be16_to_cpu(hfs->al_bl_st);
        dbg("alloc_first_block 0x%x", alloc_first_block);

        embed_first_block = be16_to_cpu(hfs->embed_startblock);
        dbg("embed_first_block 0x%x", embed_first_block);

        //off += (alloc_first_block * 512) +
               //(embed_first_block * alloc_block_size);
        //dbg("hfs wrapped hfs+ found at offset 0x%llx", (unsigned long long) off);
        off2 += (alloc_first_block * 512) +
               (embed_first_block * alloc_block_size);
        dbg("hfs wrapped hfs+ found at offset 0x%llx", (unsigned long long) off2);
        //buf = volume_id_get_buffer(id, off + HFS_SUPERBLOCK_OFFSET, 0x200);
        buf = volume_id_get_buffer(id, off2 + HFS_SUPERBLOCK_OFFSET, 0x200);
        if (buf == NULL)
            return -1;
        goto checkplus;
    }

    if (hfs->label_len > 0 && hfs->label_len < 28) {
//		volume_id_set_label_raw(id, hfs->label, hfs->label_len);
        volume_id_set_label_string(id, hfs->label, hfs->label_len) ;
    }

    volume_id_set_uuid(id, hfs->finder_info.id, UUID_HFS);
//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	id->type = "hfs";

    return 0;

 checkplus:
    hfsplus = (struct hfsplus_vol_header *) buf;
    if (hfs->signature[0] == 'H')
        if (hfs->signature[1] == '+' || hfs->signature[1] == 'X')
            goto hfsplus;
    return -1;

 hfsplus:
    volume_id_set_uuid(id, hfsplus->finder_info.id, UUID_HFS);

    blocksize = be32_to_cpu(hfsplus->blocksize);
    dbg("blocksize %u", blocksize);

    memcpy(extents, hfsplus->cat_file.extents, sizeof(extents));
    cat_block = be32_to_cpu(extents[0].start_block);
    dbg("catalog start block 0x%x", cat_block);

    //buf = volume_id_get_buffer(id, off + (cat_block * blocksize), 0x2000);
    buf = volume_id_get_buffer(id, off2 + (cat_block * blocksize), 0x2000);
    if (buf == NULL)
        goto found;

    bnode = (struct hfsplus_bheader_record *)
        &buf[sizeof(struct hfsplus_bnode_descriptor)];

    leaf_node_head = be32_to_cpu(bnode->leaf_head);
    dbg("catalog leaf node 0x%x", leaf_node_head);

    leaf_node_size = be16_to_cpu(bnode->node_size);
    dbg("leaf node size 0x%x", leaf_node_size);

    leaf_node_count = be32_to_cpu(bnode->leaf_count);
    dbg("leaf node count 0x%x", leaf_node_count);
    if (leaf_node_count == 0)
        goto found;

    leaf_block = (leaf_node_head * leaf_node_size) / blocksize;

    /* get physical location */
    for (ext = 0; ext < HFSPLUS_EXTENT_COUNT; ext++) {
        ext_block_start = be32_to_cpu(extents[ext].start_block);
        ext_block_count = be32_to_cpu(extents[ext].block_count);
        dbg("extent start block 0x%x, count 0x%x", ext_block_start, ext_block_count);

        if (ext_block_count == 0)
            goto found;

        /* this is our extent */
        if (leaf_block < ext_block_count)
            break;

        leaf_block -= ext_block_count;
    }
    if (ext == HFSPLUS_EXTENT_COUNT)
        goto found;
    dbg("found block in extent %i", ext);

    leaf_off = (ext_block_start + leaf_block) * blocksize;

    //buf = volume_id_get_buffer(id, off + leaf_off, leaf_node_size);
    buf = volume_id_get_buffer(id, off2 + leaf_off, leaf_node_size);
    if (buf == NULL)
        goto found;

    descr = (struct hfsplus_bnode_descriptor *) buf;
    dbg("descriptor type 0x%x", descr->type);

    record_count = be16_to_cpu(descr->num_recs);
    dbg("number of records %u", record_count);
    if (record_count == 0)
        goto found;

    if (descr->type != HFS_NODE_LEAF)
        goto found;

    key = (struct hfsplus_catalog_key *)
        &buf[sizeof(struct hfsplus_bnode_descriptor)];

    dbg("parent id 0x%x", be32_to_cpu(key->parent_id));
    if (key->parent_id != cpu_to_be32(HFSPLUS_POR_CNID))
        goto found;

    label_len = be16_to_cpu(key->unicode_len) * 2;
    dbg("label unicode16 len %i", label_len);
//	volume_id_set_label_raw(id, key->unicode, label_len);
    volume_id_set_label_unicode16(id, key->unicode, BE, label_len);

 found:
//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	id->type = "hfsplus";

    return 0;
}

int find_partition_label(const char *dev_name, char *label){
    struct volume_id id;
    char dev_path[128];
    char usb_port[32];
    char nvram_label[32], nvram_value[512];

    if(label) *label = 0;

    memset(usb_port, 0, 32);
    if(get_usb_port_by_device(dev_name, usb_port, 32) == NULL)
        return 0;

    memset(nvram_label, 0, 32);
    sprintf(nvram_label, "usb_path_%s_label", dev_name);

    memset(nvram_value, 0, 512);
    strncpy(nvram_value, nvram_safe_get(nvram_label), 512);
    if(strlen(nvram_value) > 0){
        strcpy(label, nvram_value);

        return (label && *label != 0);
    }

    memset(dev_path, 0, 128);
    sprintf(dev_path, "/dev/%s", dev_name);

    memset(&id, 0x00, sizeof(id));
    if((id.fd = open(dev_path, O_RDONLY)) < 0)
        return 0;

    volume_id_get_buffer(&id, 0, SB_BUFFER_SIZE);

    if(volume_id_probe_linux_swap(&id) == 0 || id.error)
        goto ret;
    if(volume_id_probe_ext(&id) == 0 || id.error)
        goto ret;
    if(volume_id_probe_vfat(&id) == 0 || id.error)
        goto ret;
    if(volume_id_probe_ntfs(&id) == 0 || id.error)
        goto ret;
    if(volume_id_probe_hfs_hfsplus(&id) == 0 || id.error)
        goto ret;

ret:
    volume_id_free_buffer(&id);
    if(label && (*id.label != 0))
        strcpy(label, id.label);
    else
        strcpy(label, " ");
    nvram_set(nvram_label, label);
    close(id.fd);
    return (label && *label != 0);
}

void strntrim(char *str){
    register char *start, *end;
    int len;

    if(str == NULL)
        return;

    len = strlen(str);
    start = str;
    end = start+len-1;

    while(start < end && isspace(*start))
        ++start;
    while(start <= end && isspace(*end))
        --end;

    end++;

    if((int)(end-start) < len){
        memcpy(str, start, (end-start));
        str[end-start] = 0;
    }

    return;
}

/* Serialize using fcntl() calls
 */

int check_magic(char *buf, char *magic){
    if(!strncmp(magic, "ext3_chk", 8)){
        if(!((*buf)&4))
            return 0;
        if(*(buf+4) >= 0x40)
            return 0;
        if(*(buf+8) >= 8)
            return 0;
        return 1;
    }

    if(!strncmp(magic, "ext4_chk", 8)){
        if(!((*buf)&4))
            return 0;
        if(*(buf+4) > 0x3F)
            return 1;
        if(*(buf+4) >= 0x40)
            return 0;
        if(*(buf+8) <= 7)
            return 0;
        return 1;
    }

    return 0;
}

char *detect_fs_type(char *device)
{
    int fd;
    unsigned char buf[4096];

    if ((fd = open(device, O_RDONLY)) < 0)
        return NULL;

    if (read(fd, buf, sizeof(buf)) != sizeof(buf))
    {
        close(fd);
        return NULL;
    }

    close(fd);

    /* first check for mbr */
    if (*device && device[strlen(device) - 1] > '9' &&
        buf[510] == 0x55 && buf[511] == 0xAA && /* signature */
        ((buf[0x1be] | buf[0x1ce] | buf[0x1de] | buf[0x1ee]) & 0x7f) == 0) /* boot flags */
    {
        return "mbr";
    }
    /* detect swap */
    else if (memcmp(buf + 4086, "SWAPSPACE2", 10) == 0 ||
         memcmp(buf + 4086, "SWAP-SPACE", 10) == 0 ||
         memcmp(buf + 4086, "S1SUSPEND", 9) == 0 ||
         memcmp(buf + 4086, "S2SUSPEND", 9) == 0 ||
         memcmp(buf + 4086, "ULSUSPEND", 9) == 0)
    {
        return "swap";
    }
    /* detect ext2/3/4 */
    else if (buf[0x438] == 0x53 && buf[0x439] == 0xEF)
    {
        if(check_magic((char *) &buf[0x45c], "ext3_chk"))
            return "ext3";
        else if(check_magic((char *) &buf[0x45c], "ext4_chk"))
            return "ext4";
        else
            return "ext2";
    }
    /* detect hfs */
    else if(buf[1024] == 0x48){
        if(!memcmp(buf+1032, "HFSJ", 4)){
            if(buf[1025] == 0x58) // with case-sensitive
                return "hfs+jx";
            else
                return "hfs+j";
        }
        else
            return "hfs";
    }
    /* detect ntfs */
    else if (buf[510] == 0x55 && buf[511] == 0xAA && /* signature */
        memcmp(buf + 3, "NTFS    ", 8) == 0)
    {
        return "ntfs";
    }
    /* detect vfat */
    else if (buf[510] == 0x55 && buf[511] == 0xAA && /* signature */
        buf[11] == 0 && buf[12] >= 1 && buf[12] <= 8 /* sector size 512 - 4096 */ &&
        buf[13] != 0 && (buf[13] & (buf[13] - 1)) == 0) /* sectors per cluster */
    {
        if(buf[6] == 0x20 && buf[7] == 0x20 && !memcmp(buf+71, "EFI        ", 11))
            return "apple_efi";
        else
            return "vfat";
    }

    return "unknown";
}

int read_mount_data(const char *device_name
        , char *mount_point, int mount_len
        , char *type, int type_len
        , char *right, int right_len
        ){
    char *mount_info = read_whole_file(MOUNT_FILE);
    char *start, line[PATH_MAX];
    char target[8];

    if(mount_point == NULL || mount_len <= 0
            || type == NULL || type_len <= 0
            || right == NULL || right_len <= 0
            ){
        usb_dbg("Bad input!!\n");
        return 0;
    }

    if(mount_info == NULL){
        usb_dbg("Failed to open \"%s\"!!\n", MOUNT_FILE);
        return 0;
    }

    memset(target, 0, 8);
    sprintf(target, "%s ", device_name);

    if((start = strstr(mount_info, target)) == NULL){
        //usb_dbg("disk_initial:: %s: Failed to execute strstr()!\n", device_name);
        free(mount_info);
        return 0;
    }

    start += strlen(target);

    if(get_line_from_buffer(start, line, PATH_MAX) == NULL){
        usb_dbg("%s: Failed to execute get_line_from_buffer()!\n", device_name);
        free(mount_info);
        return 0;
    }

    memset(mount_point, 0, mount_len);
    memset(type, 0, type_len);
    memset(right, 0, right_len);

    if(sscanf(line, "%s %s %[^\n ]", mount_point, type, right) != 3){
        usb_dbg("%s: Failed to execute sscanf()!\n", device_name);
        free(mount_info);
        return 0;
    }

    if(!strcmp(type, "ufsd")){
        char full_dev[16];

        memset(full_dev, 0, 16);
        sprintf(full_dev, "/dev/%s", device_name);

        memset(type, 0, type_len);
        strcpy(type, detect_fs_type(full_dev));
    }

    right[2] = 0;

    free(mount_info);
    return 1;
}

int get_mount_size(const char *mount_point, u64 *total_kilobytes, u64 *used_kilobytes){
    u64 total_size, free_size, used_size;
    struct statfs fsbuf;

    if(total_kilobytes == NULL || used_kilobytes == NULL)
        return 0;

    *total_kilobytes = 0;
    *used_kilobytes = 0;

    if(statfs(mount_point, &fsbuf))
        return 0;

    total_size = (u64)((u64)fsbuf.f_blocks*(u64)fsbuf.f_bsize);
    free_size = (u64)((u64)fsbuf.f_bfree*(u64)fsbuf.f_bsize);
    used_size = total_size-free_size;

    *total_kilobytes = total_size/1024;
    *used_kilobytes = used_size/1024;

    return 1;
}

extern char *get_disk_name(const char *string, char *buf, const int buf_size){
    int len;

    if(string == NULL || buf_size <= 0)
        return NULL;
    memset(buf, 0, buf_size);

    if(!is_disk_name(string) && !is_partition_name(string, NULL))
        return NULL;

    len = strlen(string);
    if(!is_disk_name(string)){
        while(isdigit(string[len-1]))
            --len;

#ifdef BCM_MMC
        if(string[len-1] == 'p')
            --len;
#endif
    }

    if(len > buf_size)
        return NULL;

    strncpy(buf, string, len);

    return buf;
}

int get_partition_size(const char *partition_name, u64 *size_in_kilobytes){
    FILE *fp;
    char disk_name[16];
    char target_file[128], buf[16], *ptr;

    if(size_in_kilobytes == NULL)
        return 0;

    *size_in_kilobytes = 0; // initial value.

    if(!is_partition_name(partition_name, NULL))
        return 0;

    get_disk_name(partition_name, disk_name, 16);

    memset(target_file, 0, 128);
    sprintf(target_file, "%s/%s/%s/size", SYS_BLOCK, disk_name, partition_name);
    if((fp = fopen(target_file, "r")) == NULL)
        return 0;

    memset(buf, 0, 16);
    ptr = fgets(buf, 16, fp);
    fclose(fp);
    if(ptr == NULL)
        return 0;

    *size_in_kilobytes = ((u64)strtoll(buf, NULL, 10))/2;

    return 1;
}


partition_info_t *create_partition(const char *device_name, partition_info_t **new_part_info){
    partition_info_t *follow_part_info;
    char label[128];
    u32 partition_order = 0;
    u64 size_in_kilobytes = 0, total_kilobytes = 0, used_kilobytes = 0;
    char buf1[PATH_MAX], buf2[64], buf3[PATH_MAX]; // options of mount info needs more buffer size.
    int len;

    if(new_part_info == NULL){
        usb_dbg("Bad input!!\n");
        return NULL;
    }

    *new_part_info = NULL; // initial value.

    if(device_name == NULL || get_device_type_by_device(device_name) != DEVICE_TYPE_DISK)
        return NULL;

    if(!is_disk_name(device_name) && !is_partition_name(device_name, &partition_order))
        return NULL;

    if(initial_part_data(&follow_part_info) == NULL){
        usb_dbg("No memory!!(follow_part_info)\n");
        return NULL;
    }

    len = strlen(device_name);
    follow_part_info->device = (char *)malloc(len+1);
    if(follow_part_info->device == NULL){
        usb_dbg("No memory!!(follow_part_info->device)\n");
        free_partition_data(&follow_part_info);
        return NULL;
    }
    strncpy(follow_part_info->device, device_name, len);
    follow_part_info->device[len] = 0;

    if(find_partition_label(device_name, label)){
        strntrim(label);
        len = strlen(label);
        follow_part_info->label = (char *)malloc(len+1);
        if(follow_part_info->label == NULL){
            usb_dbg("No memory!!(follow_part_info->label)\n");
            free_partition_data(&follow_part_info);
            return NULL;
        }
        strncpy(follow_part_info->label, label, len);
        follow_part_info->label[len] = 0;
    }

    follow_part_info->partition_order = partition_order;

    if(read_mount_data(device_name, buf1, PATH_MAX, buf2, 64, buf3, PATH_MAX)){
        len = strlen(buf1);
        follow_part_info->mount_point = (char *)malloc(len+1);
        if(follow_part_info->mount_point == NULL){
            usb_dbg("No memory!!(follow_part_info->mount_point)\n");
            free_partition_data(&follow_part_info);
            return NULL;
        }
        strncpy(follow_part_info->mount_point, buf1, len);
        follow_part_info->mount_point[len] = 0;

        len = strlen(buf2);
        follow_part_info->file_system = (char *)malloc(len+1);
        if(follow_part_info->file_system == NULL){
            usb_dbg("No memory!!(follow_part_info->file_system)\n");
            free_partition_data(&follow_part_info);
            return NULL;
        }
        strncpy(follow_part_info->file_system, buf2, len);
        follow_part_info->file_system[len] = 0;

        len = strlen(buf3);
        follow_part_info->permission = (char *)malloc(len+1);
        if(follow_part_info->permission == NULL){
            usb_dbg("No memory!!(follow_part_info->permission)\n");
            free_partition_data(&follow_part_info);
            return NULL;
        }
        strncpy(follow_part_info->permission, buf3, len);
        follow_part_info->permission[len] = 0;

        if(get_mount_size(follow_part_info->mount_point, &total_kilobytes, &used_kilobytes)){
            follow_part_info->size_in_kilobytes = total_kilobytes;
            follow_part_info->used_kilobytes = used_kilobytes;
        }
    }
    else{
        /*if(is_disk_name(device_name)){	// Disk
            free_partition_data(&follow_part_info);
            return NULL;
        }
        else{//*/
            len = strlen(PARTITION_TYPE_UNKNOWN);
            follow_part_info->file_system = (char *)malloc(len+1);
            if(follow_part_info->file_system == NULL){
                usb_dbg("No memory!!(follow_part_info->file_system)\n");
                free_partition_data(&follow_part_info);
                return NULL;
            }
            strncpy(follow_part_info->file_system, PARTITION_TYPE_UNKNOWN, len);
            follow_part_info->file_system[len] = 0;

            get_partition_size(device_name, &size_in_kilobytes);
            follow_part_info->size_in_kilobytes = size_in_kilobytes;
        //}
    }

    *new_part_info = follow_part_info;

    return *new_part_info;
}

extern void free_disk_data(disk_info_t **disk_info_list){
    printf("#####free_disk_data start#####\n");
    fprintf(stderr,"@@@@@free_disk_data start@@@@@\n");
    disk_info_t *follow_disk, *old_disk;

    if(disk_info_list == NULL)
        return;

    follow_disk = *disk_info_list;
    while(follow_disk != NULL){
        if(follow_disk->tag != NULL)
            free(follow_disk->tag);
        if(follow_disk->vendor != NULL)
            free(follow_disk->vendor);
        if(follow_disk->model != NULL)
            free(follow_disk->model);
        if(follow_disk->device != NULL)
            free(follow_disk->device);
        if(follow_disk->port != NULL)
            free(follow_disk->port);

        free_partition_data(&(follow_disk->partitions));

        old_disk = follow_disk;
        follow_disk = follow_disk->next;
        free(old_disk);
    }
    fprintf(stderr,"@@@@@free_disk_data end@@@@@\n");
}

disk_info_t *initial_disk_data(disk_info_t **disk_info_list){
    disk_info_t *follow_disk;

    if(disk_info_list == NULL)
        return NULL;

    *disk_info_list = (disk_info_t *)malloc(sizeof(disk_info_t));
    if(*disk_info_list == NULL)
        return NULL;

    follow_disk = *disk_info_list;

    follow_disk->tag = NULL;
    follow_disk->vendor = NULL;
    follow_disk->model = NULL;
    follow_disk->device = NULL;
    follow_disk->major = (u32)0;
    follow_disk->minor = (u32)0;
    follow_disk->port = NULL;
    follow_disk->partition_number = (u32)0;
    follow_disk->mounted_number = (u32)0;
    follow_disk->size_in_kilobytes = (u64)0;
    follow_disk->partitions = NULL;
    follow_disk->next = NULL;

    return follow_disk;
}

int get_disk_major_minor(const char *disk_name, u32 *major, u32 *minor){
    FILE *fp;
    char target_file[128], buf[8], *ptr;

    if(major == NULL || minor == NULL)
        return 0;

    *major = 0; // initial value.
    *minor = 0; // initial value.

    if(disk_name == NULL || !is_disk_name(disk_name))
        return 0;

    memset(target_file, 0, 128);
    sprintf(target_file, "%s/%s/dev", SYS_BLOCK, disk_name);
    if((fp = fopen(target_file, "r")) == NULL)
        return 0;

    memset(buf, 0, 8);
    ptr = fgets(buf, 8, fp);
    fclose(fp);
    if(ptr == NULL)
        return 0;

    if((ptr = strchr(buf, ':')) == NULL)
        return 0;

    ptr[0] = '\0';
    *major = (u32)strtol(buf, NULL, 10);
    *minor = (u32)strtol(ptr+1, NULL, 10);

    return 1;
}

int get_disk_size(const char *disk_name, u64 *size_in_kilobytes){
    FILE *fp;
    char target_file[128], buf[16], *ptr;

    if(size_in_kilobytes == NULL)
        return 0;

    *size_in_kilobytes = 0; // initial value.

    if(disk_name == NULL || !is_disk_name(disk_name))
        return 0;

    memset(target_file, 0, 128);
    sprintf(target_file, "%s/%s/size", SYS_BLOCK, disk_name);
    if((fp = fopen(target_file, "r")) == NULL)
        return 0;

    memset(buf, 0, 16);
    ptr = fgets(buf, 16, fp);
    fclose(fp);
    if(ptr == NULL)
        return 0;

    *size_in_kilobytes = ((u64)strtoll(buf, NULL, 10))/2;

    return 1;
}

//#ifdef PC
char *get_usb_port_by_device(const char *device_name, char *buf, const int buf_size){
    int device_type = get_device_type_by_device(device_name);

    if(device_type == DEVICE_TYPE_UNKNOWN)
        return NULL;

    memset(buf, 0, buf_size);
    strcpy(buf, "1-1");

    return buf;
}
//#else
char *get_usb_port_by_string(const char *target_string, char *buf, const int buf_size)
{
    fprintf(stderr, "target_string=%s\n",target_string);
    memset(buf, 0, buf_size);

    if(strstr(target_string, USB_XHCI_PORT_1))
        strcpy(buf, USB_XHCI_PORT_1);
    else if(strstr(target_string, USB_XHCI_PORT_2))
        strcpy(buf, USB_XHCI_PORT_2);
    else if(strstr(target_string, USB_EHCI_PORT_1))
        strcpy(buf, USB_EHCI_PORT_1);
    else if(strstr(target_string, USB_EHCI_PORT_2))
        strcpy(buf, USB_EHCI_PORT_2);
    else if(strstr(target_string, USB_OHCI_PORT_1))
        strcpy(buf, USB_OHCI_PORT_1);
    else if(strstr(target_string, USB_OHCI_PORT_2))
        strcpy(buf, USB_OHCI_PORT_2);
    else if(strstr(target_string, USB_EHCI_PORT_3))
        strcpy(buf, USB_EHCI_PORT_3);
    else if(strstr(target_string, USB_OHCI_PORT_3))
        strcpy(buf, USB_OHCI_PORT_3);
    else
        return NULL;

    return buf;
}
//#endif

char *get_usb_node_by_string(const char *target_string, char *ret, const int ret_size)
{
    char usb_port[32], buf[16];
    char *ptr, *ptr2, *ptr3;
    int len;

    memset(usb_port, 0, sizeof(usb_port));
    if(get_usb_port_by_string(target_string, usb_port, sizeof(usb_port)) == NULL)
        return NULL;
    if((ptr = strstr(target_string, usb_port)) == NULL)
        return NULL;
    if(ptr != target_string)
        ptr += strlen(usb_port)+1;

    if((ptr2 = strchr(ptr, ':')) == NULL)
        return NULL;
    ptr3 = ptr2;
    *ptr3 = 0;

    if((ptr2 = strrchr(ptr, '/')) == NULL)
        ptr2 = ptr;
    else
        ptr = ptr2+1;

    len = strlen(ptr);
    if(len > 16)
        len = 16;

    memset(buf, 0, sizeof(buf));
    strncpy(buf, ptr, len);

    len = strlen(buf);
    if(len > ret_size)
        len = ret_size;

    memset(ret, 0, ret_size);
    strncpy(ret, buf, len);

    *ptr3 = ':';

    return ret;
}

char *get_usb_node_by_device(const char *device_name, char *buf, const int buf_size)
{
    int device_type = get_device_type_by_device(device_name);
    char device_path[128], usb_path[PATH_MAX];
    char disk_name[16];

    if(device_type == DEVICE_TYPE_UNKNOWN)
        return NULL;

    memset(device_path, 0, 128);
    memset(usb_path, 0, PATH_MAX);

    if(device_type == DEVICE_TYPE_DISK){
        get_disk_name(device_name, disk_name, 16);
        sprintf(device_path, "%s/%s/device", SYS_BLOCK, disk_name);
        if(realpath(device_path, usb_path) == NULL){
            usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
            return NULL;
        }
    }
    else
#ifdef RTCONFIG_USB_PRINTER
    if(device_type == DEVICE_TYPE_PRINTER){
        sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
        if(realpath(device_path, usb_path) == NULL){
            usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
            return NULL;
        }
    }
    else
#endif
#ifdef RTCONFIG_USB_MODEM
    if(device_type == DEVICE_TYPE_SG){
        sprintf(device_path, "%s/%s/device", SYS_SG, device_name);
        if(realpath(device_path, usb_path) == NULL){
            usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
            return NULL;
        }
    }
    else
    if(device_type == DEVICE_TYPE_CD){
        sprintf(device_path, "%s/%s/device", SYS_BLOCK, device_name);
        if(realpath(device_path, usb_path) == NULL){
            usb_dbg("(%s): Fail to get link: %s.\n", device_name, device_path);
            return NULL;
        }
    }
    else
    if(device_type == DEVICE_TYPE_MODEM){
        sprintf(device_path, "%s/%s/device", SYS_TTY, device_name);
        if(realpath(device_path, usb_path) == NULL){
            sleep(1); // Sometimes link would be built slowly, so try again.

            if(realpath(device_path, usb_path) == NULL){
                usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
                return NULL;
            }
        }
    }
    else
#endif
#ifdef RTCONFIG_USB_BECEEM
    if(device_type == DEVICE_TYPE_BECEEM){
        sprintf(device_path, "%s/%s/device", SYS_USB, device_name);
        if(realpath(device_path, usb_path) == NULL){
            if(realpath(device_path, usb_path) == NULL){
                usb_dbg("(%s)(2/2): Fail to get link: %s.\n", device_name, device_path);
                return NULL;
            }
        }
    }
    else
#endif
        return NULL;

#ifdef BCM_MMC
    if(isMMCDevice(device_name)){ // SD card.
        snprintf(buf, buf_size, "%s", SDCARD_PORT);
    }
    else
#endif
    if(get_usb_node_by_string(usb_path, buf, buf_size) == NULL){
        usb_dbg("(%s): Fail to get usb node: %s.\n", device_name, usb_path);
        return NULL;
    }
    fprintf(stderr, "usb_node=%s\n", buf);
    return buf;
}

int get_usb_port_number(const char *usb_port) // shared/rtstate.c
{
    char word[100], *next;
    int port_num, i;

    port_num = 0;
    i = 0;
    foreach(word, nvram_safe_get("xhci_ports"), next){
        ++i;
        if(!strcmp(usb_port, word)){
            port_num = i;
            break;
        }
    }

    i = 0;
    if(port_num == 0){
        foreach(word, nvram_safe_get("ehci_ports"), next){
            ++i;
            if(!strcmp(usb_port, word)){
                port_num = i;
                break;
            }
        }
    }

    i = 0;
    if(port_num == 0){
        foreach(word, nvram_safe_get("ohci_ports"), next){
            ++i;
            if(!strcmp(usb_port, word)){
                port_num = i;
                break;
            }
        }
    }
    return port_num;
}
char *get_path_by_node(const char *usb_node, char *buf, const int buf_size){
    char usb_port[32], *hub_path;
    int port_num = 0, len;

    if(usb_node == NULL || buf == NULL || buf_size <= 0)
        return NULL;

    // Get USB port.
    if(get_usb_port_by_string(usb_node, usb_port, sizeof(usb_port)) == NULL)
        return NULL;

    port_num = get_usb_port_number(usb_port);
    if(port_num == 0)
        return NULL;

    if(strlen(usb_node) > (len = strlen(usb_port))){
        hub_path = (char *)usb_node+len;
        snprintf(buf, buf_size, "%d%s", port_num, hub_path);
    }
    else
        snprintf(buf, buf_size, "%d", port_num);

    return buf;
}

char *get_disk_vendor(const char *disk_name, char *buf, const int buf_size){
    FILE *fp;
    char target_file[128], *ptr;
    int len;

    if(buf_size <= 0)
        return NULL;
    memset(buf, 0, buf_size);

    if(disk_name == NULL || !is_disk_name(disk_name))
        return NULL;

    memset(target_file, 0, 128);
    sprintf(target_file, "%s/%s/device/vendor", SYS_BLOCK, disk_name);
    if((fp = fopen(target_file, "r")) == NULL)
        return NULL;

    ptr = fgets(buf, buf_size, fp);
    fclose(fp);
    if(ptr == NULL)
        return NULL;

    len = strlen(buf);
    buf[len-1] = 0;
    fprintf(stderr,"get_disk_vendor,buf=%s\n",buf);
    return buf;
}

char *get_disk_model(const char *disk_name, char *buf, const int buf_size){
    FILE *fp;
    char target_file[128], *ptr;
    int len;

    if(buf_size <= 0)
        return NULL;
    memset(buf, 0, buf_size);

    if(disk_name == NULL || !is_disk_name(disk_name))
        return NULL;

    memset(target_file, 0, 128);
    sprintf(target_file, "%s/%s/device/model", SYS_BLOCK, disk_name);
    if((fp = fopen(target_file, "r")) == NULL)
        return NULL;

    ptr = fgets(buf, buf_size, fp);
    fclose(fp);
    if(ptr == NULL)
        return NULL;

    len = strlen(buf);
    buf[len-1] = 0;
    fprintf(stderr,"get_disk_model,buf=%s\n",buf);
    return buf;
}

disk_info_t *create_disk(const char *device_name, disk_info_t **new_disk_info){
    fprintf(stderr,"%s,%d,create_disk start\n",__FILE__,__LINE__,create_disk);
    disk_info_t *follow_disk_info;
    u32 major, minor;
    u64 size_in_kilobytes = 0;
    int len;
    char usb_node[32], port_path[8];
    char buf[64], *port, *vendor = NULL, *model = NULL, *ptr;
    partition_info_t *new_partition_info, **follow_partition_list;

    if(new_disk_info == NULL){
        usb_dbg("Bad input!!\n");
        return NULL;
    }

    *new_disk_info = NULL; // initial value.

    if(device_name == NULL || !is_disk_name(device_name))
        return NULL;

    if(initial_disk_data(&follow_disk_info) == NULL){
        usb_dbg("No memory!!(follow_disk_info)\n");
        return NULL;
    }

    len = strlen(device_name);
    follow_disk_info->device = (char *)malloc(len+1);
    if(follow_disk_info->device == NULL){
        usb_dbg("No memory!!(follow_disk_info->device)\n");
        free_disk_data(&follow_disk_info);
        return NULL;
    }
    strcpy(follow_disk_info->device, device_name);
    follow_disk_info->device[len] = 0;

    if(!get_disk_major_minor(device_name, &major, &minor)){
        usb_dbg("Fail to get disk's major and minor: %s.\n", device_name);
        free_disk_data(&follow_disk_info);
        return NULL;
    }
    follow_disk_info->major = major;
    follow_disk_info->minor = minor;

    if(!get_disk_size(device_name, &size_in_kilobytes)){
        usb_dbg("Fail to get disk's size_in_kilobytes: %s.\n", device_name);
        free_disk_data(&follow_disk_info);
        return NULL;
    }
    follow_disk_info->size_in_kilobytes = size_in_kilobytes;

    if(isStorageDevice(device_name)
#ifdef BCM_MMC
            || isMMCDevice(device_name)
#endif
            ){
        // Get USB node.
        if(get_usb_node_by_device(device_name, usb_node, 32) == NULL){
            usb_dbg("(%s): Fail to get usb node.\n", device_name);
            free_disk_data(&follow_disk_info);
            return NULL;
        }

        if(get_path_by_node(usb_node, port_path, 8) == NULL){
            usb_dbg("(%s): Fail to get usb path.\n", usb_node);
            free_disk_data(&follow_disk_info);
            return NULL;
        }

        // Get USB port.
        if(get_usb_port_by_string(usb_node, buf, 64) == NULL){
            usb_dbg("Fail to get usb port: %s.\n", usb_node);
            free_disk_data(&follow_disk_info);
            return NULL;
        }

        len = strlen(buf);
        if(len > 0){
            port = (char *)malloc(8);
            if(port == NULL){
                usb_dbg("No memory!!(port)\n");
                free_disk_data(&follow_disk_info);
                return NULL;
            }
            memset(port, 0, 8);
            strncpy(port, port_path, 8);

            follow_disk_info->port = port;
        }

        // start get vendor.
        get_disk_vendor(device_name, buf, 64);

        len = strlen(buf);
        if(len > 0){
            vendor = (char *)malloc(len+1);
            if(vendor == NULL){
                usb_dbg("No memory!!(vendor)\n");
                free_disk_data(&follow_disk_info);
                return NULL;
            }
            strncpy(vendor, buf, len);
            vendor[len] = 0;
            strntrim(vendor);

            follow_disk_info->vendor = vendor;
        }

        // start get model.
        get_disk_model(device_name, buf, 64);

        len = strlen(buf);
        if(len > 0){
            model = (char *)malloc(len+1);
            if(model == NULL){
                usb_dbg("No memory!!(model)\n");
                free_disk_data(&follow_disk_info);
                return NULL;
            }
            strncpy(model, buf, len);
            model[len] = 0;
            strntrim(model);

            follow_disk_info->model = model;
        }

        // get USB's tag
        memset(buf, 0, 64);
        len = 0;
        ptr = buf;
        if(vendor != NULL){
            len += strlen(vendor);
            strcpy(ptr, vendor);
            ptr += len;
        }
        if(model != NULL){
            if(len > 0){
                ++len; // Add a space between vendor and model.
                strcpy(ptr, " ");
                ++ptr;
            }
            len += strlen(model);
            strcpy(ptr, model);
            ptr += len;
        }

        if(len > 0){
            follow_disk_info->tag = (char *)malloc(len+1);
            if(follow_disk_info->tag == NULL){
                usb_dbg("No memory!!(follow_disk_info->tag)\n");
                free_disk_data(&follow_disk_info);
                return NULL;
            }
            strcpy(follow_disk_info->tag, buf);
            follow_disk_info->tag[len] = 0;
        }
        else{
#ifdef BCM_MMC
            if(isMMCDevice(device_name))
                len = strlen(DEFAULT_MMC_TAG);
            else
#endif
                len = strlen(DEFAULT_USB_TAG);

            follow_disk_info->tag = (char *)malloc(len+1);
            if(follow_disk_info->tag == NULL){
                usb_dbg("No memory!!(follow_disk_info->tag)\n");
                free_disk_data(&follow_disk_info);
                return NULL;
            }
#ifdef BCM_MMC
            if(isMMCDevice(device_name))
                strcpy(follow_disk_info->tag, DEFAULT_MMC_TAG);
            else
#endif
                strcpy(follow_disk_info->tag, DEFAULT_USB_TAG);
            follow_disk_info->tag[len] = 0;
        }

        follow_partition_list = &(follow_disk_info->partitions);
        while(*follow_partition_list != NULL)
            follow_partition_list = &((*follow_partition_list)->next);

        new_partition_info = create_partition(device_name, follow_partition_list);
        if(new_partition_info != NULL){
            new_partition_info->disk = follow_disk_info;

            ++(follow_disk_info->partition_number);
            ++(follow_disk_info->mounted_number);
            if(!strcmp(new_partition_info->device, follow_disk_info->device))
                new_partition_info->size_in_kilobytes = follow_disk_info->size_in_kilobytes-4;
        }
    }

    if(!strcmp(follow_disk_info->device, follow_disk_info->partitions->device))
        get_disk_partitionnumber(device_name, &(follow_disk_info->partition_number), &(follow_disk_info->mounted_number));

    *new_disk_info = follow_disk_info;

    return *new_disk_info;
}

extern disk_info_t *read_disk_data(){
    printf("#####read_disk_data start#####\n");
    fprintf(stderr,"@@@@@read_disk_data start@@@@@\n");
	disk_info_t *disk_info_list = NULL, *new_disk_info, **follow_disk_info_list;
	char *partition_info = read_whole_file(PARTITION_FILE);
	char *follow_info;
	char line[64], device_name[16];
	u32 major;
	disk_info_t *parent_disk_info;
	partition_info_t *new_partition_info, **follow_partition_list;
	u64 device_size;

	if(partition_info == NULL){
		usb_dbg("Failed to open \"%s\"!!\n", PARTITION_FILE);
		return disk_info_list;
	}
	follow_info = partition_info;

	memset(device_name, 0, 16);
	while(get_line_from_buffer(follow_info, line, 64) != NULL){
		follow_info += strlen(line);

		if(sscanf(line, "%u %*u %llu %[^\n ]", &major, &device_size, device_name) != 3)
			continue;
		if(major != USB_DISK_MAJOR
#ifdef BCM_MMC
				&& major != MMC_DISK_MAJOR
#endif
				)
			continue;
		if(device_size == 1) // extend partition.
			continue;
        fprintf(stderr,"%s,%d,device_name=%s\n",__FILE__,__LINE__,device_name);
		if(is_disk_name(device_name)){ // Disk
			follow_disk_info_list = &disk_info_list;
			while(*follow_disk_info_list != NULL)
				follow_disk_info_list = &((*follow_disk_info_list)->next);

			new_disk_info = create_disk(device_name, follow_disk_info_list);
		}
		else if(is_partition_name(device_name, NULL)){ // Partition
			// Found a partition device.
			// Find the parent disk.
			parent_disk_info = disk_info_list;
			while(1){
				if(parent_disk_info == NULL){
					usb_dbg("Error while parsing %s: found "
									"partition '%s' but haven't seen the disk device "
									"of which it is a part.\n", PARTITION_FILE, device_name);
					free(partition_info);
					return disk_info_list;
				}

				if(!strncmp(device_name, parent_disk_info->device, 3))
					break;

				parent_disk_info = parent_disk_info->next;
			}

			follow_partition_list = &(parent_disk_info->partitions);
			while(*follow_partition_list != NULL){
				if((*follow_partition_list)->partition_order == 0){
					free_partition_data(follow_partition_list);
					parent_disk_info->partitions = NULL;
					follow_partition_list = &(parent_disk_info->partitions);
				}
				else
					follow_partition_list = &((*follow_partition_list)->next);
			}

			new_partition_info = create_partition(device_name, follow_partition_list);
			if(new_partition_info != NULL)
				new_partition_info->disk = parent_disk_info;
		}
	}

	free(partition_info);
    fprintf(stderr,"@@@@@read_disk_data end@@@@@\n");
	return disk_info_list;
}

//--------------------------disk_share.c----------------------------//
#include <syslog.h>
#define MAX_ACCOUNT_NUM 6

int _vstrsep(char *buf, const char *sep, ...)
{
    va_list ap;
    char **p;
    int n;

    n = 0;
    va_start(ap, sep);
    while ((p = va_arg(ap, char **)) != NULL) {
        if ((*p = strsep(&buf, sep)) == NULL) break;
        ++n;
    }
    va_end(ap);
    return n;
}
#define vstrsep(buf, sep, args...) _vstrsep(buf, sep, args, NULL)

int nvram_get_int(const char *key) //shared/misc.c
{
    return atoi(nvram_safe_get(key));
}

/* Transfer ASCII to Char */
int ascii_to_char_safe(const char *output, const char *input, int outsize){ //shared/strings.c
    char *src = (char *)input;
    char *dst = (char *)output;
    char *end = (char *)output+outsize-1;
    char char_array[3];
    unsigned int char_code;

    if(src == NULL || dst == NULL || outsize <= 0)
        return 0;

    for(; *src && dst < end; ++src, ++dst){
        if((*src >= '0' && *src <= '9')
                || (*src >= 'A' && *src <= 'Z')
                || (*src >= 'a' && *src <= 'z')
                ){
            *dst = *src;
        }
        else if(*src == '\\'){
            ++src;
            if(!(*src))
                break;

            *dst = *src;
        }
        else{
            ++src;
            if(!(*src))
                break;
            memset(char_array, 0, 3);
            strncpy(char_array, src, 2);
            ++src;

            char_code = strtol(char_array, NULL, 16);

            *dst = (char)char_code;
        }
    }

    if(dst <= end)
        *dst = '\0';

    return (dst-output);
}

/* Serialize using fcntl() calls
 */

/* when lock file has been re-opened by the same process,
 * it can't be closed, because it release original lock,
 * that have been set earlier. this results in file
 * descriptors leak.
 * one way to avoid it - check if the process has set the
 * lock already via /proc/locks, but it seems overkill
 * with al of related file ops and text searching. there's
 * no kernel API for that.
 * maybe need different lock kind? */
#define LET_FD_LEAK

int file_lock(const char *tag) //shared/files.c
{
    struct flock lock;
    char path[64];
    int lockfd;
    pid_t pid, err;
#ifdef LET_FD_LEAK
    pid_t lockpid;
#else
    struct stat st;
#endif

    snprintf(path, sizeof(path), "/var/lock/%s.lock", tag);

#ifndef LET_FD_LEAK
    pid = getpid();

    /* check if we already hold a lock */
    if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode) && st.st_size > 0) {
        FILE *fp;
        char line[100], *ptr, *value;
        char id[sizeof("XX:XX:4294967295")];

        if ((fp = fopen("/proc/locks", "r")) == NULL)
            goto error;

        snprintf(id, sizeof(id), "%02x:%02x:%ld",
             major(st.st_dev), minor(st.st_dev), st.st_ino);
        while ((value = fgets(line, sizeof(line), fp)) != NULL) {
            strtok_r(line, " ", &ptr);
            if ((value = strtok_r(NULL, " ", &ptr)) && strcmp(value, "POSIX") == 0 &&
                (value = strtok_r(NULL, " ", &ptr)) && /* strcmp(value, "ADVISORY") == 0 && */
                (value = strtok_r(NULL, " ", &ptr)) && strcmp(value, "WRITE") == 0 &&
                (value = strtok_r(NULL, " ", &ptr)) && atoi(value) == pid &&
                (value = strtok_r(NULL, " ", &ptr)) && strcmp(value, id) == 0)
                break;
        }
        fclose(fp);

        if (value != NULL) {
            syslog(LOG_DEBUG, "Error locking %s: %d %s", path, 0, "Already locked");
            return -1;
        }
    }
#endif

    if ((lockfd = open(path, O_CREAT | O_RDWR, 0666)) < 0)
        goto error;

#ifdef LET_FD_LEAK
    pid = getpid();

    /* check if we already hold a lock */
    if (read(lockfd, &lockpid, sizeof(pid_t)) == sizeof(pid_t) &&
        lockpid == pid) {
        /* don't close the file here as that will release all locks */
        syslog(LOG_DEBUG, "Error locking %s: %d %s", path, 0, "Already locked");
        return -1;
    }
#endif

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_pid = pid;
    while (fcntl(lockfd, F_SETLKW, &lock) < 0) {
        if (errno != EINTR)
            goto close;
    }

    if (lseek(lockfd, 0, SEEK_SET) < 0 ||
        write(lockfd, &pid, sizeof(pid_t)) < 0)
        goto close;

    return lockfd;

close:
    err = errno;
    close(lockfd);
    errno = err;
error:
    syslog(LOG_DEBUG, "Error locking %s: %d %s", path, errno, strerror(errno));
    return -1;
}

void file_unlock(int lockfd)
{
    if (lockfd < 0) {
        errno = EBADF;
        syslog(LOG_DEBUG, "Error unlocking %d: %d %s", lockfd, errno, strerror(errno));
        return;
    }

    ftruncate(lockfd, 0);
    close(lockfd);
}

// Success: return value is account number. Fail: return value is -1.
int get_account_list(int *acc_num, char ***account_list)
{
    char **tmp_account_list = NULL, **tmp_account;
    int len, i, j;
    char *nv, *nvp, *b;
    char *tmp_ascii_user, *tmp_ascii_passwd;
    char char_user[64];

    *acc_num = nvram_get_int("acc_num");
    if(*acc_num <= 0)
        return 0;

    nv = nvp = strdup(nvram_safe_get("acc_list"));
    i = 0;
    if(nv && strlen(nv) > 0){
        while((b = strsep(&nvp, "<")) != NULL){
            if(vstrsep(b, ">", &tmp_ascii_user, &tmp_ascii_passwd) != 2)
                continue;

            tmp_account = (char **)malloc(sizeof(char *)*(i+1));
            if(tmp_account == NULL){
                usb_dbg("Can't malloc \"tmp_account\".\n");
                free(nv);
                return -1;
            }

            memset(char_user, 0, 64);
            ascii_to_char_safe(char_user, tmp_ascii_user, 64);

            len = strlen(char_user);
            tmp_account[i] = (char *)malloc(sizeof(char)*(len+1));
            if(tmp_account[i] == NULL){
                usb_dbg("Can't malloc \"tmp_account[%d]\".\n", i);
                free(tmp_account);
                free(nv);
                return -1;
            }
            strcpy(tmp_account[i], char_user);
            tmp_account[i][len] = 0;

            if(i != 0){
                for(j = 0; j < i; ++j)
                    tmp_account[j] = tmp_account_list[j];

                free(tmp_account_list);
                tmp_account_list = tmp_account;
            }
            else
                tmp_account_list = tmp_account;

            if(++i >= *acc_num)
                break;
        }
        free(nv);

        *account_list = tmp_account_list;
        *acc_num = i;

        return *acc_num;
    }

    if(nv)
        free(nv);

    return 0;
}

extern int test_if_exist_account(const char *const account) {
    int acc_num;
    char **account_list;
    int result, i;

    if(account == NULL)
        return 1;

    result = get_account_list(&acc_num, &account_list);
    if (result < 0) {
        usb_dbg("Can't get the account list.\n");
        free_2_dimension_list(&acc_num, &account_list);
        return -1;
    }

    result = 0;
    for (i = 0; i < acc_num; ++i)
        if (!strcmp(account, account_list[i])) {
            result = 1;
            break;
        }
    free_2_dimension_list(&acc_num, &account_list);

    return result;
}

extern int add_account(const char *const account, const char *const password){
    disk_info_t *disk_list, *follow_disk;
    partition_info_t *follow_partition;
    int acc_num;
    int len;
    char nvram_value[PATH_MAX], *ptr;
    char ascii_user[64], ascii_passwd[64];
    int lock;

    if(account == NULL || strlen(account) <= 0){
        usb_dbg("No input, \"account\".\n");
        return -1;
    }
    if(password == NULL || strlen(password) <= 0){
        usb_dbg("No input, \"password\".\n");
        return -1;
    }
    if(test_if_exist_account(account)){
        usb_dbg("\"%s\" is already created.\n", account);
        return -1;
    }

    memset(ascii_user, 0, 64);
    char_to_ascii_safe(ascii_user, account, 64);

    memset(ascii_passwd, 0, 64);
    char_to_ascii_safe(ascii_passwd, password, 64);

    acc_num = nvram_get_int("acc_num");
    if(acc_num < 0)
        acc_num = 0;
    if(acc_num >= MAX_ACCOUNT_NUM){
        usb_dbg("Too many accounts are created.\n");
        return -1;
    }

    memset(nvram_value, 0, PATH_MAX);
    strcpy(nvram_value, nvram_safe_get("acc_list"));
    len = strlen(nvram_value);
    if(len > 0){
        ptr = nvram_value+len;
        sprintf(ptr, "<%s>%s", ascii_user, ascii_passwd);
        nvram_set("acc_list", nvram_value);

        sprintf(nvram_value, "%d", acc_num+1);
        nvram_set("acc_num", nvram_value);
    }
    else{
        sprintf(nvram_value, "%s>%s", ascii_user, ascii_passwd);
        nvram_set("acc_list", nvram_value);

        nvram_set("acc_num", "1");
    }

    lock = file_lock("nvramcommit");
    nvram_commit();
    file_unlock(lock);

    disk_list = read_disk_data();
    if(disk_list == NULL){
        usb_dbg("Couldn't read the disk list out.\n");
        return 0;
    }

    for(follow_disk = disk_list; follow_disk != NULL; follow_disk = follow_disk->next){
        for(follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
            if(follow_partition->mount_point == NULL)
                continue;

            if(initial_var_file(account, follow_partition->mount_point) != 0)
                usb_dbg("Can't initial \"%s\"'s file in %s.\n", account, follow_partition->mount_point);
        }
    }
    free_disk_data(&disk_list);

    return 0;
}
