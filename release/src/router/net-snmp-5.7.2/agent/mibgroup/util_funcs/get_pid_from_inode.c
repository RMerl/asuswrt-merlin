#include <net-snmp/net-snmp-config.h>

#include "get_pid_from_inode.h"

#include <net-snmp/output_api.h>

#include <ctype.h>
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

# define PROC_PATH          "/proc"
# define SOCKET_TYPE_1      "socket:["
# define SOCKET_TYPE_2      "[0000]:"

/* Definition of a simple open addressing hash table.*/
/* When inode == 0 then the entry is empty.*/
typedef struct {
    ino64_t inode;
    pid_t   pid;
} inode_pid_ent_t;

#define INODE_PID_TABLE_MAX_COLLISIONS 1000
#define INODE_PID_TABLE_LENGTH 20000
#define INODE_PID_TABLE_SIZE (INODE_PID_TABLE_LENGTH * sizeof (inode_pid_ent_t))
static inode_pid_ent_t  inode_pid_table[INODE_PID_TABLE_LENGTH];

static uint32_t
_hash(uint64_t key)
{
    key = (~key) + (key << 18);
    key = key ^ (key >> 31);
    key = key * 21;
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return key;
}

static void
_clear(void)
{
    /* Clear the inode/pid hash table.*/
    memset(inode_pid_table, 0, INODE_PID_TABLE_SIZE);
}

static void
_set(ino64_t inode, pid_t pid)
{
    uint32_t        hash = _hash(inode);
    uint32_t        i;
    inode_pid_ent_t *entry;

    /* We will try for a maximum number of collisions.*/
    for (i = 0; i < INODE_PID_TABLE_MAX_COLLISIONS; i++) {
        entry = &inode_pid_table[(hash + i) % INODE_PID_TABLE_LENGTH];

        /* Check if this entry is empty, or the actual inode we were looking for.*/
        /* The second part should never happen, but it is here for completeness.*/
        if (entry->inode == 0 || entry->inode == inode) {
            entry->inode = inode;
            entry->pid = pid;
            return;
        }
    }

    /* We will silently fail to insert the inode if we get too many collisions.*/
    /* the _get function will return a zero pid.*/
}

static pid_t _get(ino64_t inode)
{
    uint32_t        hash = _hash(inode);
    uint32_t        i;
    inode_pid_ent_t *entry;

    /* We will try for a maximum number of collisions.*/
    for (i = 0; i < INODE_PID_TABLE_MAX_COLLISIONS; i++) {
        entry = &inode_pid_table[(hash + i) % INODE_PID_TABLE_LENGTH];

        /* Check if this entry is empty, or the actual inode we were looking for.*/
        /* If the entry is empty it means the inode is not in the table and we*/
        /* should return 0, the entry will also have a zero pid.*/
        if (entry->inode == 0 || entry->inode == inode) {
            return entry->pid;
        }
    }

    /* We could not find the pid.*/
    return 0;
}

void
netsnmp_get_pid_from_inode_init(void)
{
    DIR            *procdirs = NULL, *piddirs = NULL;
    char            path_name[PATH_MAX + 1];
    char            socket_lnk[NAME_MAX + 1];
    int             filelen = 0, readlen = 0;
    struct dirent  *procinfo, *pidinfo;
    pid_t           pid = 0;
    ino64_t         temp_inode;

    _clear();

    /* walk over all directories in /proc*/
    if (!(procdirs = opendir(PROC_PATH))) {
        NETSNMP_LOGONCE((LOG_ERR, "snmpd: cannot open /proc\n"));
        return;
    }

    while ((procinfo = readdir(procdirs)) != NULL) {
        const char* name = procinfo->d_name;

        /* A pid directory only contains digits, check for those.*/
        for (; *name; name++) {
            if (!isdigit(*name))
                break;
        }
        if(*name)
            continue;

        /* Create the /proc/<pid>/fd/ path name.*/
        memset(path_name, '\0', PATH_MAX + 1);
        filelen = snprintf(path_name, PATH_MAX,
                           PROC_PATH "/%s/fd/", procinfo->d_name);
        if (filelen <= 0 || PATH_MAX < filelen)
            continue;

        /* walk over all the files in /proc/<pid>/fd/*/
        if (!(piddirs = opendir(path_name)))
        continue;

        while ((pidinfo = readdir(piddirs)) != NULL) {
            if (filelen + strlen(pidinfo->d_name) > PATH_MAX)
                continue;

            strcpy(path_name + filelen, pidinfo->d_name);

            /* The file discriptor is a symbolic link to a socket or a file.*/
            /* Thus read the symbolic link.*/
            memset(socket_lnk, '\0', NAME_MAX + 1);
            readlen = readlink(path_name, socket_lnk, NAME_MAX);
            if (readlen < 0)
                continue;

            socket_lnk[readlen] = '\0';

            /* Check if to see if the file descriptor is a socket by comparing*/
            /* the start to a string. Also extract the inode number from this*/
            /* symbolic link.*/
            if (!strncmp(socket_lnk, SOCKET_TYPE_1, 8)) {
                temp_inode = strtoull(socket_lnk + 8, NULL, 0);
            } else if (!strncmp(socket_lnk, SOCKET_TYPE_2, 7)) {
                temp_inode = strtoull(socket_lnk + 7, NULL, 0);
            } else {
                temp_inode = 0;
            }

            /* Add the inode/pid combination to our hash table.*/
            if (temp_inode != 0) {
                pid = strtoul(procinfo->d_name, NULL, 0);
                _set(temp_inode, pid);
            }
        }
        closedir(piddirs);
    }
    if (procdirs)
        closedir(procdirs);
}

pid_t
netsnmp_get_pid_from_inode(ino64_t inode)
{
    return _get(inode);
}

