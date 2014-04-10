
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufsutil.c
 * - program to probe for the existence of a UFS filesystem
 *   and return its name
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */

#include <sys/loadable_fs.h>
#include <servers/netname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/errno.h>


#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 
#include <sys/stat.h>
#include <sys/time.h> 
#include <sys/mount.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <sys/param.h>

#include <ufs/ffs/fs.h>

#include "ufslabel.h"

#define UFS_FS_NAME		"ufs"
#define UFS_FS_NAME_FILE	"UFS"

static void 
usage(const char * progname)
{
    fprintf(stderr, "usage: %s [-m mountflag1 mountflag2 mountflag3 mountflag4] device node\n", progname);
    fprintf(stderr, "       %s [-p mountflag1 mountflag2 mountflag3 mountflag4] device\n", progname);
    fprintf(stderr, "       %s [-ksu] device\n", progname);
    fprintf(stderr, "       %s [-n] device name\n", progname);

    return;
}

union sbunion {
    struct fs	sb;
    char	raw[SBSIZE];
};

boolean_t
read_superblock(int fd, char * dev)
{
    union sbunion	superblock;
    
    if (lseek(fd, SBOFF, SEEK_SET) != SBOFF) {
	fprintf(stderr, "read_superblock: lseek %s failed, %s\n", dev,
		strerror(errno));
	goto fail;
    }
    if (read(fd, &superblock, SBSIZE) != SBSIZE) {
#ifdef DEBUG
		fprintf(stderr, "read_superblock: read %s failed, %s\n", dev,
			strerror(errno));
#endif DEBUG
	goto fail;
    }
    if (ntohl(superblock.sb.fs_magic) == FS_MAGIC) {
#ifdef DEBUG
	fprintf(stderr, "%x (big endian)\n", ntohl(superblock.sb.fs_magic));
#endif DEBUG
    }
    else if (superblock.sb.fs_magic == FS_MAGIC) {
#ifdef DEBUG
	fprintf(stderr, "%x (little endian)\n", superblock.sb.fs_magic);
#endif DEBUG
    }
    else
	goto fail;
    
    return (TRUE);
    
 fail:
    return (FALSE);
}

int 
main(int argc, const char *argv[])
{
    char		dev[512];
    char		opt;
    struct stat		sb;

    if (argc < 3 || argv[1][0] != '-') {
	usage(argv[0]);
	exit(FSUR_INVAL);
    }
    opt = argv[1][1];
    if (opt != FSUC_PROBE && opt != 's' && opt != 'k' && opt != 'n') {
	usage(argv[0]);
	exit(FSUR_INVAL);
    }

    sprintf(dev, "/dev/r%s", argv[2]);
    if (stat(dev, &sb) != 0) {
	fprintf(stderr, "%s: stat %s failed, %s\n", argv[0], dev,
		strerror(errno));
	exit(FSUR_INVAL);
    }
    switch (opt) {
    case FSUC_PROBE: {
	FILE *		f;
	char		filename[MAXPATHLEN + 1];
	int 		fd;
	int		len;
	u_char		name[UFS_MAX_LABEL_NAME + 1];
	struct ufslabel	ul;

	sprintf(filename, "%s/ufs%s/ufs.label", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	unlink(filename);
	sprintf(filename, "%s/ufs%s/ufs.name", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	unlink(filename);

	fd = open(dev, O_RDONLY, 0);
	if (fd <= 0) {
	    fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
		    strerror(errno));
	    exit(FSUR_UNRECOGNIZED);
	}
	if (read_superblock(fd, dev) == FALSE) {
	    exit(FSUR_UNRECOGNIZED);
	}
	len = sizeof(name) - 1;
	if (ufslabel_get(fd, &ul) == FALSE) {
	    exit(FSUR_RECOGNIZED);
	}
	ufslabel_get_name(&ul, (char *)name, &len);
	name[len] = '\0';
	close(fd);

	/* write the ufs.label file */
	sprintf(filename, "%s/ufs%s/ufs" FS_LABEL_SUFFIX, FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	f = fopen(filename, "w");
	if (f != NULL) {
	    fprintf(f, "%s", name);
	    fclose(f);
	}

	/* dump the name to stdout */
	write(1, name, strlen((char *)name));

	/* write the ufs.name file */
	sprintf(filename, "%s/ufs%s/ufs.name", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	f = fopen(filename, "w");
	if (f != NULL) {
	    fprintf(f, UFS_FS_NAME_FILE);
	    fclose(f);
	}
	break;
    }
    case 'n': {
	int		fd;
	char *		name;
	struct ufslabel ul;

	if (argc < 4) {
	    usage(argv[0]);
	    exit(FSUR_INVAL);
	}
	name = (char *)argv[3];
	if (strchr(name, '/') || strchr(name, ':')) {
	    fprintf(stderr, 
		    "%s: '%s' contains invalid characters '/' or ':'\n",
		    argv[0], name);
	    exit(FSUR_INVAL);
	}
	fd = open(dev, O_RDWR, 0);
	if (fd <= 0) {
	    fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
		    strerror(errno));
	    exit(FSUR_UNRECOGNIZED);
	}
	if (read_superblock(fd, dev) == FALSE) {
	    exit(FSUR_UNRECOGNIZED);
	}
	if(ufslabel_get(fd, &ul) == FALSE)
	    ufslabel_init(&ul);
	if (ufslabel_set_name(&ul, (char *)argv[3], strlen(argv[3])) == FALSE) {
	    fprintf(stderr, "%s: couldn't update the name\n",
		    argv[0]);
	    exit(FSUR_IO_FAIL);
	}
	if (ufslabel_set(fd, &ul) == FALSE) {
	    fprintf(stderr, "%s: couldn't update the name\n",
		    argv[0]);
	    exit(FSUR_IO_FAIL);
	}
	break;
    }
    case 's': {
	int		fd;
	struct ufslabel ul;

	if (argc < 3) {
	    usage(argv[0]);
	    exit(FSUR_INVAL);
	}

	fd = open(dev, O_RDWR, 0);
	if (fd <= 0) {
	    fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
		    strerror(errno));
	    exit(FSUR_UNRECOGNIZED);
	}
	if (read_superblock(fd, dev) == FALSE) {
	    exit(FSUR_UNRECOGNIZED);
	}
	if(ufslabel_get(fd, &ul) == FALSE)
	    ufslabel_init(&ul);
	ufslabel_set_uuid(&ul);
	if (ufslabel_set(fd, &ul) == FALSE) {
	    fprintf(stderr, "%s: couldn't update the uuid\n",
		    argv[0]);
	    exit(FSUR_IO_FAIL);
	}

	exit (FSUR_IO_SUCCESS);
	break;
    }
    case 'k': {
	int 		fd;
	char		uuid[UFS_MAX_LABEL_UUID + 1];
	struct ufslabel	ul;

	fd = open(dev, O_RDONLY, 0);
	if (fd <= 0) {
	    fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
		    strerror(errno));
	    exit(FSUR_UNRECOGNIZED);
	}
	if (read_superblock(fd, dev) == FALSE) {
	    exit(FSUR_UNRECOGNIZED);
	}
	if (ufslabel_get(fd, &ul) == FALSE) {
	    fprintf(stderr, "%s: couldn't read the uuid\n",
		    argv[0]);
	    exit(FSUR_IO_FAIL);
	}
	close(fd);
	ufslabel_get_uuid(&ul, uuid);

	/* dump the uuid to stdout */
	write(1, uuid, strlen(uuid));

	exit (FSUR_IO_SUCCESS);
	break;
    }
    default:
	break;
    }
    exit (FSUR_RECOGNIZED);
    return (0);
}
