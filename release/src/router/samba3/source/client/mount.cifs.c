/* 
   Mount helper utility for Linux CIFS VFS (virtual filesystem) client
   Copyright (C) 2003,2008 Steve French  (sfrench@us.ibm.com)
   Copyright (C) 2008 Jeremy Allison (jra@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <mntent.h>
#include <fcntl.h>
#include <limits.h>
#include "mount.h"

#define MOUNT_CIFS_VERSION_MAJOR "1"
#define MOUNT_CIFS_VERSION_MINOR "12"

#ifndef MOUNT_CIFS_VENDOR_SUFFIX
 #ifdef _SAMBA_BUILD_
  #include "include/version.h"
  #ifdef SAMBA_VERSION_VENDOR_SUFFIX
   #define MOUNT_CIFS_VENDOR_SUFFIX "-"SAMBA_VERSION_OFFICIAL_STRING"-"SAMBA_VERSION_VENDOR_SUFFIX
  #else
   #define MOUNT_CIFS_VENDOR_SUFFIX "-"SAMBA_VERSION_OFFICIAL_STRING
  #endif /* SAMBA_VERSION_OFFICIAL_STRING and SAMBA_VERSION_VENDOR_SUFFIX */
 #else
   #define MOUNT_CIFS_VENDOR_SUFFIX ""
 #endif /* _SAMBA_BUILD_ */
#endif /* MOUNT_CIFS_VENDOR_SUFFIX */

#ifdef _SAMBA_BUILD_
#include "include/config.h"
#endif

#ifndef MS_MOVE 
#define MS_MOVE 8192 
#endif 

#ifndef MS_BIND
#define MS_BIND 4096
#endif

#define MAX_UNC_LEN 1024

#define CONST_DISCARD(type, ptr)      ((type) ((void *) (ptr)))

#ifndef SAFE_FREE
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); x=NULL;} } while(0)
#endif

#define MOUNT_PASSWD_SIZE 64
#define DOMAIN_SIZE 64

/* currently maximum length of IPv6 address string */
#define MAX_ADDRESS_LEN INET6_ADDRSTRLEN

const char *thisprogram;
int verboseflag = 0;
int fakemnt = 0;
static int got_password = 0;
static int got_user = 0;
static int got_domain = 0;
static int got_ip = 0;
static int got_unc = 0;
static int got_uid = 0;
static int got_gid = 0;
static char * user_name = NULL;
static char * mountpassword = NULL;
char * domain_name = NULL;
char * prefixpath = NULL;

/* glibc doesn't have strlcpy, strlcat. Ensure we do. JRA. We
 * don't link to libreplace so need them here. */

/* like strncpy but does not 0 fill the buffer and always null
 *    terminates. bufsize is the size of the destination buffer */

#ifndef HAVE_STRLCPY
static size_t strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len = strlen(s);
	size_t ret = len;
	if (bufsize <= 0) return 0;
	if (len >= bufsize) len = bufsize-1;
	memcpy(d, s, len);
	d[len] = 0;
	return ret;
}
#endif

/* like strncat but does not 0 fill the buffer and always null
 *    terminates. bufsize is the length of the buffer, which should
 *       be one more than the maximum resulting string length */

#ifndef HAVE_STRLCAT
static size_t strlcat(char *d, const char *s, size_t bufsize)
{
	size_t len1 = strlen(d);
	size_t len2 = strlen(s);
	size_t ret = len1 + len2;

	if (len1+len2 >= bufsize) {
		if (bufsize < (len1+1)) {
			return ret;
		}
		len2 = bufsize - (len1+1);
	}
	if (len2 > 0) {
		memcpy(d+len1, s, len2);
		d[len1+len2] = 0;
	}
	return ret;
}
#endif

/* BB finish BB

        cifs_umount
        open nofollow - avoid symlink exposure? 
        get owner of dir see if matches self or if root
        call system(umount argv) etc.
                
BB end finish BB */

static char * check_for_domain(char **);


static void mount_cifs_usage(void)
{
	printf("\nUsage:  %s <remotetarget> <dir> -o <options>\n", thisprogram);
	printf("\nMount the remote target, specified as a UNC name,");
	printf(" to a local directory.\n\nOptions:\n");
	printf("\tuser=<arg>\n\tpass=<arg>\n\tdom=<arg>\n");
	printf("\nLess commonly used options:");
	printf("\n\tcredentials=<filename>,guest,perm,noperm,setuids,nosetuids,rw,ro,");
	printf("\n\tsep=<char>,iocharset=<codepage>,suid,nosuid,exec,noexec,serverino,");
	printf("\n\tmapchars,nomapchars,nolock,servernetbiosname=<SRV_RFC1001NAME>");
	printf("\n\tdirectio,nounix,cifsacl,sec=<authentication mechanism>,sign");
	printf("\n\nOptions not needed for servers supporting CIFS Unix extensions");
	printf("\n\t(e.g. unneeded for mounts to most Samba versions):");
	printf("\n\tuid=<uid>,gid=<gid>,dir_mode=<mode>,file_mode=<mode>,sfu");
	printf("\n\nRarely used options:");
	printf("\n\tport=<tcpport>,rsize=<size>,wsize=<size>,unc=<unc_name>,ip=<ip_address>,");
	printf("\n\tdev,nodev,nouser_xattr,netbiosname=<OUR_RFC1001NAME>,hard,soft,intr,");
	printf("\n\tnointr,ignorecase,noposixpaths,noacl,prefixpath=<path>,nobrl");
	printf("\n\tin6_addr");
	printf("\n\nOptions are described in more detail in the manual page");
	printf("\n\tman 8 mount.cifs\n");
	printf("\nTo display the version number of the mount helper:");
	printf("\n\t%s -V\n",thisprogram);

	SAFE_FREE(mountpassword);
	exit(EX_USAGE);
}

/* caller frees username if necessary */
static char * getusername(void) {
	char *username = NULL;
	struct passwd *password = getpwuid(getuid());

	if (password) {
		username = password->pw_name;
	}
	return username;
}

static int open_cred_file(char * file_name)
{
	char * line_buf;
	char * temp_val;
	FILE * fs;
	int i, length;

	i = access(file_name, R_OK);
	if (i)
		return i;

	fs = fopen(file_name,"r");
	if(fs == NULL)
		return errno;
	line_buf = (char *)malloc(4096);
	if(line_buf == NULL) {
		fclose(fs);
		return ENOMEM;
	}

	while(fgets(line_buf,4096,fs)) {
		/* parse line from credential file */

		/* eat leading white space */
		for(i=0;i<4086;i++) {
			if((line_buf[i] != ' ') && (line_buf[i] != '\t'))
				break;
			/* if whitespace - skip past it */
		}
		if (strncasecmp("username",line_buf+i,8) == 0) {
			temp_val = strchr(line_buf + i,'=');
			if(temp_val) {
				/* go past equals sign */
				temp_val++;
				for(length = 0;length<4087;length++) {
					if ((temp_val[length] == '\n')
					    || (temp_val[length] == '\0')) {
						temp_val[length] = '\0';
						break;
					}
				}
				if(length > 4086) {
					printf("mount.cifs failed due to malformed username in credentials file");
					memset(line_buf,0,4096);
					exit(EX_USAGE);
				} else {
					got_user = 1;
					user_name = (char *)calloc(1 + length,1);
					/* BB adding free of user_name string before exit,
						not really necessary but would be cleaner */
					strlcpy(user_name,temp_val, length+1);
				}
			}
		} else if (strncasecmp("password",line_buf+i,8) == 0) {
			temp_val = strchr(line_buf+i,'=');
			if(temp_val) {
				/* go past equals sign */
				temp_val++;
				for(length = 0;length<MOUNT_PASSWD_SIZE+1;length++) {
					if ((temp_val[length] == '\n')
					    || (temp_val[length] == '\0')) {
						temp_val[length] = '\0';
						break;
					}
				}
				if(length > MOUNT_PASSWD_SIZE) {
					printf("mount.cifs failed: password in credentials file too long\n");
					memset(line_buf,0, 4096);
					exit(EX_USAGE);
				} else {
					if(mountpassword == NULL) {
						mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
					} else
						memset(mountpassword,0,MOUNT_PASSWD_SIZE);
					if(mountpassword) {
						strlcpy(mountpassword,temp_val,MOUNT_PASSWD_SIZE+1);
						got_password = 1;
					}
				}
			}
                } else if (strncasecmp("domain",line_buf+i,6) == 0) {
                        temp_val = strchr(line_buf+i,'=');
                        if(temp_val) {
                                /* go past equals sign */
                                temp_val++;
				if(verboseflag)
					printf("\nDomain %s\n",temp_val);
                                for(length = 0;length<DOMAIN_SIZE+1;length++) {
					if ((temp_val[length] == '\n')
					    || (temp_val[length] == '\0')) {
						temp_val[length] = '\0';
						break;
					}
                                }
                                if(length > DOMAIN_SIZE) {
                                        printf("mount.cifs failed: domain in credentials file too long\n");
                                        exit(EX_USAGE);
                                } else {
                                        if(domain_name == NULL) {
                                                domain_name = (char *)calloc(DOMAIN_SIZE+1,1);
                                        } else
                                                memset(domain_name,0,DOMAIN_SIZE);
                                        if(domain_name) {
                                                strlcpy(domain_name,temp_val,DOMAIN_SIZE+1);
                                                got_domain = 1;
                                        }
                                }
                        }
                }

	}
	fclose(fs);
	SAFE_FREE(line_buf);
	return 0;
}

static int get_password_from_file(int file_descript, char * filename)
{
	int rc = 0;
	int i;
	char c;

	if(mountpassword == NULL)
		mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
	else 
		memset(mountpassword, 0, MOUNT_PASSWD_SIZE);

	if (mountpassword == NULL) {
		printf("malloc failed\n");
		exit(EX_SYSERR);
	}

	if(filename != NULL) {
		rc = access(filename, R_OK);
		if (rc) {
			fprintf(stderr, "mount.cifs failed: access check of %s failed: %s\n",
					filename, strerror(errno));
			exit(EX_SYSERR);
		}
		file_descript = open(filename, O_RDONLY);
		if(file_descript < 0) {
			printf("mount.cifs failed. %s attempting to open password file %s\n",
				   strerror(errno),filename);
			exit(EX_SYSERR);
		}
	}
	/* else file already open and fd provided */

	for(i=0;i<MOUNT_PASSWD_SIZE;i++) {
		rc = read(file_descript,&c,1);
		if(rc < 0) {
			printf("mount.cifs failed. Error %s reading password file\n",strerror(errno));
			if(filename != NULL)
				close(file_descript);
			exit(EX_SYSERR);
		} else if(rc == 0) {
			if(mountpassword[0] == 0) {
				if(verboseflag)
					printf("\nWarning: null password used since cifs password file empty");
			}
			break;
		} else /* read valid character */ {
			if((c == 0) || (c == '\n')) {
				mountpassword[i] = '\0';
				break;
			} else 
				mountpassword[i] = c;
		}
	}
	if((i == MOUNT_PASSWD_SIZE) && (verboseflag)) {
		printf("\nWarning: password longer than %d characters specified in cifs password file",
			MOUNT_PASSWD_SIZE);
	}
	got_password = 1;
	if(filename != NULL) {
		close(file_descript);
	}

	return rc;
}

static int parse_options(char ** optionsp, int * filesys_flags)
{
	const char * data;
	char * percent_char = NULL;
	char * value = NULL;
	char * next_keyword = NULL;
	char * out = NULL;
	int out_len = 0;
	int word_len;
	int rc = 0;
	char user[32];
	char group[32];

	if (!optionsp || !*optionsp)
		return 1;
	data = *optionsp;

	/* BB fixme check for separator override BB */

	if (getuid()) {
		got_uid = 1;
		snprintf(user,sizeof(user),"%u",getuid());
		got_gid = 1;
		snprintf(group,sizeof(group),"%u",getgid());
	}

/* while ((data = strsep(&options, ",")) != NULL) { */
	while(data != NULL) {
		/*  check if ends with trailing comma */
		if(*data == 0)
			break;

		/* format is keyword=value,keyword2=value2,keyword3=value3 etc.) */
		/* data  = next keyword */
		/* value = next value ie stuff after equal sign */

		next_keyword = strchr(data,','); /* BB handle sep= */
	
		/* temporarily null terminate end of keyword=value pair */
		if(next_keyword)
			*next_keyword++ = 0;

		/* temporarily null terminate keyword to make keyword and value distinct */
		if ((value = strchr(data, '=')) != NULL) {
			*value = '\0';
			value++;
		}

		if (strncmp(data, "users",5) == 0) {
			if(!value || !*value) {
				goto nocopy;
			}
		} else if (strncmp(data, "user_xattr",10) == 0) {
		   /* do nothing - need to skip so not parsed as user name */
		} else if (strncmp(data, "user", 4) == 0) {

			if (!value || !*value) {
				if(data[4] == '\0') {
					if(verboseflag)
						printf("\nskipping empty user mount parameter\n");
					/* remove the parm since it would otherwise be confusing
					to the kernel code which would think it was a real username */
					goto nocopy;
				} else {
					printf("username specified with no parameter\n");
					SAFE_FREE(out);
					return 1;	/* needs_arg; */
				}
			} else {
				if (strnlen(value, 260) < 260) {
					got_user=1;
					percent_char = strchr(value,'%');
					if(percent_char) {
						*percent_char = ',';
						if(mountpassword == NULL)
							mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
						if(mountpassword) {
							if(got_password)
								printf("\nmount.cifs warning - password specified twice\n");
							got_password = 1;
							percent_char++;
							strlcpy(mountpassword, percent_char,MOUNT_PASSWD_SIZE+1);
						/*  remove password from username */
							while(*percent_char != 0) {
								*percent_char = ',';
								percent_char++;
							}
						}
					}
					/* this is only case in which the user
					name buf is not malloc - so we have to
					check for domain name embedded within
					the user name here since the later
					call to check_for_domain will not be
					invoked */
					domain_name = check_for_domain(&value);
				} else {
					printf("username too long\n");
					SAFE_FREE(out);
					return 1;
				}
			}
		} else if (strncmp(data, "pass", 4) == 0) {
			if (!value || !*value) {
				if(got_password) {
					fprintf(stderr, "\npassword specified twice, ignoring second\n");
				} else
					got_password = 1;
			} else if (strnlen(value, MOUNT_PASSWD_SIZE) < MOUNT_PASSWD_SIZE) {
				if (got_password) {
					fprintf(stderr, "\nmount.cifs warning - password specified twice\n");
				} else {
					mountpassword = strndup(value, MOUNT_PASSWD_SIZE);
					if (!mountpassword) {
						fprintf(stderr, "mount.cifs error: %s", strerror(ENOMEM));
						SAFE_FREE(out);
						return 1;
					}
					got_password = 1;
				}
			} else {
				fprintf(stderr, "password too long\n");
				SAFE_FREE(out);
				return 1;
			}
			goto nocopy;
		} else if (strncmp(data, "sec", 3) == 0) {
			if (value) {
				if (!strncmp(value, "none", 4) ||
				    !strncmp(value, "krb5", 4))
					got_password = 1;
			}
		} else if (strncmp(data, "ip", 2) == 0) {
			if (!value || !*value) {
				printf("target ip address argument missing");
			} else if (strnlen(value, MAX_ADDRESS_LEN) <= MAX_ADDRESS_LEN) {
				if(verboseflag)
					printf("ip address %s override specified\n",value);
				got_ip = 1;
			} else {
				printf("ip address too long\n");
				SAFE_FREE(out);
				return 1;
			}
		} else if ((strncmp(data, "unc", 3) == 0)
		   || (strncmp(data, "target", 6) == 0)
		   || (strncmp(data, "path", 4) == 0)) {
			if (!value || !*value) {
				printf("invalid path to network resource\n");
				SAFE_FREE(out);
				return 1;  /* needs_arg; */
			} else if(strnlen(value,5) < 5) {
				printf("UNC name too short");
			}

			if (strnlen(value, 300) < 300) {
				got_unc = 1;
				if (strncmp(value, "//", 2) == 0) {
					if(got_unc)
						printf("unc name specified twice, ignoring second\n");
					else
						got_unc = 1;
				} else if (strncmp(value, "\\\\", 2) != 0) {	                   
					printf("UNC Path does not begin with // or \\\\ \n");
					SAFE_FREE(out);
					return 1;
				} else {
					if(got_unc)
						printf("unc name specified twice, ignoring second\n");
					else
						got_unc = 1;
				}
			} else {
				printf("CIFS: UNC name too long\n");
				SAFE_FREE(out);
				return 1;
			}
		} else if ((strncmp(data, "dom" /* domain */, 3) == 0)
			   || (strncmp(data, "workg", 5) == 0)) {
			/* note this allows for synonyms of "domain"
			   such as "DOM" and "dom" and "workgroup"
			   and "WORKGRP" etc. */
			if (!value || !*value) {
				printf("CIFS: invalid domain name\n");
				SAFE_FREE(out);
				return 1;	/* needs_arg; */
			}
			if (strnlen(value, DOMAIN_SIZE+1) < DOMAIN_SIZE+1) {
				got_domain = 1;
			} else {
				printf("domain name too long\n");
				SAFE_FREE(out);
				return 1;
			}
		} else if (strncmp(data, "cred", 4) == 0) {
			if (value && *value) {
				rc = open_cred_file(value);
				if(rc) {
					printf("error %d (%s) opening credential file %s\n",
						rc, strerror(rc), value);
					SAFE_FREE(out);
					return 1;
				}
			} else {
				printf("invalid credential file name specified\n");
				SAFE_FREE(out);
				return 1;
			}
		} else if (strncmp(data, "uid", 3) == 0) {
			if (value && *value) {
				got_uid = 1;
				if (!isdigit(*value)) {
					struct passwd *pw;

					if (!(pw = getpwnam(value))) {
						printf("bad user name \"%s\"\n", value);
						exit(EX_USAGE);
					}
					snprintf(user, sizeof(user), "%u", pw->pw_uid);
				} else {
					strlcpy(user,value,sizeof(user));
				}
			}
			goto nocopy;
		} else if (strncmp(data, "gid", 3) == 0) {
			if (value && *value) {
				got_gid = 1;
				if (!isdigit(*value)) {
					struct group *gr;

					if (!(gr = getgrnam(value))) {
						printf("bad group name \"%s\"\n", value);
						exit(EX_USAGE);
					}
					snprintf(group, sizeof(group), "%u", gr->gr_gid);
				} else {
					strlcpy(group,value,sizeof(group));
				}
			}
			goto nocopy;
       /* fmask and dmask synonyms for people used to smbfs syntax */
		} else if (strcmp(data, "file_mode") == 0 || strcmp(data, "fmask")==0) {
			if (!value || !*value) {
				printf ("Option '%s' requires a numerical argument\n", data);
				SAFE_FREE(out);
				return 1;
			}

			if (value[0] != '0') {
				printf ("WARNING: '%s' not expressed in octal.\n", data);
			}

			if (strcmp (data, "fmask") == 0) {
				printf ("WARNING: CIFS mount option 'fmask' is deprecated. Use 'file_mode' instead.\n");
				data = "file_mode"; /* BB fix this */
			}
		} else if (strcmp(data, "dir_mode") == 0 || strcmp(data, "dmask")==0) {
			if (!value || !*value) {
				printf ("Option '%s' requires a numerical argument\n", data);
				SAFE_FREE(out);
				return 1;
			}

			if (value[0] != '0') {
				printf ("WARNING: '%s' not expressed in octal.\n", data);
			}

			if (strcmp (data, "dmask") == 0) {
				printf ("WARNING: CIFS mount option 'dmask' is deprecated. Use 'dir_mode' instead.\n");
				data = "dir_mode";
			}
			/* the following eight mount options should be
			stripped out from what is passed into the kernel
			since these eight options are best passed as the
			mount flags rather than redundantly to the kernel 
			and could generate spurious warnings depending on the
			level of the corresponding cifs vfs kernel code */
		} else if (strncmp(data, "nosuid", 6) == 0) {
			*filesys_flags |= MS_NOSUID;
		} else if (strncmp(data, "suid", 4) == 0) {
			*filesys_flags &= ~MS_NOSUID;
		} else if (strncmp(data, "nodev", 5) == 0) {
			*filesys_flags |= MS_NODEV;
		} else if ((strncmp(data, "nobrl", 5) == 0) || 
			   (strncmp(data, "nolock", 6) == 0)) {
			*filesys_flags &= ~MS_MANDLOCK;
		} else if (strncmp(data, "dev", 3) == 0) {
			*filesys_flags &= ~MS_NODEV;
		} else if (strncmp(data, "noexec", 6) == 0) {
			*filesys_flags |= MS_NOEXEC;
		} else if (strncmp(data, "exec", 4) == 0) {
			*filesys_flags &= ~MS_NOEXEC;
		} else if (strncmp(data, "guest", 5) == 0) {
			user_name = (char *)calloc(1, 1);
			got_user = 1;
			got_password = 1;
		} else if (strncmp(data, "ro", 2) == 0) {
			*filesys_flags |= MS_RDONLY;
		} else if (strncmp(data, "rw", 2) == 0) {
			*filesys_flags &= ~MS_RDONLY;
                } else if (strncmp(data, "remount", 7) == 0) {
                        *filesys_flags |= MS_REMOUNT;
		} /* else if (strnicmp(data, "port", 4) == 0) {
			if (value && *value) {
				vol->port =
					simple_strtoul(value, &value, 0);
			}
		} else if (strnicmp(data, "rsize", 5) == 0) {
			if (value && *value) {
				vol->rsize =
					simple_strtoul(value, &value, 0);
			}
		} else if (strnicmp(data, "wsize", 5) == 0) {
			if (value && *value) {
				vol->wsize =
					simple_strtoul(value, &value, 0);
			}
		} else if (strnicmp(data, "version", 3) == 0) {
		} else {
			printf("CIFS: Unknown mount option %s\n",data);
		} */ /* nothing to do on those four mount options above.
			Just pass to kernel and ignore them here */

		/* Copy (possibly modified) option to out */
		word_len = strlen(data);
		if (value)
			word_len += 1 + strlen(value);

		out = (char *)realloc(out, out_len + word_len + 2);
		if (out == NULL) {
			perror("malloc");
			exit(EX_SYSERR);
		}

		if (out_len) {
			strlcat(out, ",", out_len + word_len + 2);
			out_len++;
		}

		if (value)
			snprintf(out + out_len, word_len + 1, "%s=%s", data, value);
		else
			snprintf(out + out_len, word_len + 1, "%s", data);
		out_len = strlen(out);

nocopy:
		data = next_keyword;
	}

	/* special-case the uid and gid */
	if (got_uid) {
		word_len = strlen(user);

		out = (char *)realloc(out, out_len + word_len + 6);
		if (out == NULL) {
			perror("malloc");
			exit(EX_SYSERR);
		}

		if (out_len) {
			strlcat(out, ",", out_len + word_len + 6);
			out_len++;
		}
		snprintf(out + out_len, word_len + 5, "uid=%s", user);
		out_len = strlen(out);
	}
	if (got_gid) {
		word_len = strlen(group);

		out = (char *)realloc(out, out_len + 1 + word_len + 6);
		if (out == NULL) {
		perror("malloc");
			exit(EX_SYSERR);
		}

		if (out_len) {
			strlcat(out, ",", out_len + word_len + 6);
			out_len++;
		}
		snprintf(out + out_len, word_len + 5, "gid=%s", group);
		out_len = strlen(out);
	}

	SAFE_FREE(*optionsp);
	*optionsp = out;
	return 0;
}

/* replace all (one or more) commas with double commas */
static void check_for_comma(char ** ppasswrd)
{
	char *new_pass_buf;
	char *pass;
	int i,j;
	int number_of_commas = 0;
	int len;

	if(ppasswrd == NULL)
		return;
	else 
		(pass = *ppasswrd);

	len = strlen(pass);

	for(i=0;i<len;i++)  {
		if(pass[i] == ',')
			number_of_commas++;
	}

	if(number_of_commas == 0)
		return;
	if(number_of_commas > MOUNT_PASSWD_SIZE) {
		/* would otherwise overflow the mount options buffer */
		printf("\nInvalid password. Password contains too many commas.\n");
		return;
	}

	new_pass_buf = (char *)malloc(len+number_of_commas+1);
	if(new_pass_buf == NULL)
		return;

	for(i=0,j=0;i<len;i++,j++) {
		new_pass_buf[j] = pass[i];
		if(pass[i] == ',') {
			j++;
			new_pass_buf[j] = pass[i];
		}
	}
	new_pass_buf[len+number_of_commas] = 0;

	SAFE_FREE(*ppasswrd);
	*ppasswrd = new_pass_buf;
	
	return;
}

/* Usernames can not have backslash in them and we use
   [BB check if usernames can have forward slash in them BB] 
   backslash as domain\user separator character
*/
static char * check_for_domain(char **ppuser)
{
	char * original_string;
	char * usernm;
	char * domainnm;
	int    original_len;
	int    len;
	int    i;

	if(ppuser == NULL)
		return NULL;

	original_string = *ppuser;

	if (original_string == NULL)
		return NULL;
	
	original_len = strlen(original_string);

	usernm = strchr(*ppuser,'/');
	if (usernm == NULL) {
		usernm = strchr(*ppuser,'\\');
		if (usernm == NULL)
			return NULL;
	}

	if(got_domain) {
		printf("Domain name specified twice. Username probably malformed\n");
		return NULL;
	}

	usernm[0] = 0;
	domainnm = *ppuser;
	if (domainnm[0] != 0) {
		got_domain = 1;
	} else {
		printf("null domain\n");
	}
	len = strlen(domainnm);
	/* reset domainm to new buffer, and copy
	domain name into it */
	domainnm = (char *)malloc(len+1);
	if(domainnm == NULL)
		return NULL;

	strlcpy(domainnm,*ppuser,len+1);

/*	move_string(*ppuser, usernm+1) */
	len = strlen(usernm+1);

	if(len >= original_len) {
		/* should not happen */
		return domainnm;
	}

	for(i=0;i<original_len;i++) {
		if(i<len)
			original_string[i] = usernm[i+1];
		else /* stuff with commas to remove last parm */
			original_string[i] = ',';
	}

	/* BB add check for more than one slash? 
	  strchr(*ppuser,'/');
	  strchr(*ppuser,'\\') 
	*/
	
	return domainnm;
}

/* replace all occurances of "from" in a string with "to" */
static void replace_char(char *string, char from, char to, int maxlen)
{
	char *lastchar = string + maxlen;
	while (string) {
		string = strchr(string, from);
		if (string) {
			*string = to;
			if (string >= lastchar)
				return;
		}
	}
}

/* Note that caller frees the returned buffer if necessary */
static struct addrinfo *
parse_server(char ** punc_name)
{
	char * unc_name = *punc_name;
	int length = strnlen(unc_name, MAX_UNC_LEN);
	char * share;
	struct addrinfo *addrlist;
	int rc;

	if(length > (MAX_UNC_LEN - 1)) {
		printf("mount error: UNC name too long");
		return NULL;
	}
	if ((strncasecmp("cifs://", unc_name, 7) == 0) ||
	    (strncasecmp("smb://", unc_name, 6) == 0)) {
		printf("\nMounting cifs URL not implemented yet. Attempt to mount %s\n", unc_name);
		return NULL;
	}

	if(length < 3) {
		/* BB add code to find DFS root here */
		printf("\nMounting the DFS root for domain not implemented yet\n");
		return NULL;
	} else {
		if(strncmp(unc_name,"//",2) && strncmp(unc_name,"\\\\",2)) {
			/* check for nfs syntax ie server:share */
			share = strchr(unc_name,':');
			if(share) {
				*punc_name = (char *)malloc(length+3);
				if(*punc_name == NULL) {
					/* put the original string back  if 
					   no memory left */
					*punc_name = unc_name;
					return NULL;
				}
				*share = '/';
				strlcpy((*punc_name)+2,unc_name,length+1);
				SAFE_FREE(unc_name);
				unc_name = *punc_name;
				unc_name[length+2] = 0;
				goto continue_unc_parsing;
			} else {
				printf("mount error: improperly formatted UNC name.");
				printf(" %s does not begin with \\\\ or //\n",unc_name);
				return NULL;
			}
		} else {
continue_unc_parsing:
			unc_name[0] = '/';
			unc_name[1] = '/';
			unc_name += 2;

			/* allow for either delimiter between host and sharename */
			if ((share = strpbrk(unc_name, "/\\"))) {
				*share = 0;  /* temporarily terminate the string */
				share += 1;
				if(got_ip == 0) {
					rc = getaddrinfo(unc_name, NULL, NULL, &addrlist);
					if (rc != 0) {
						printf("mount error: could not resolve address for %s: %s\n",
							unc_name, gai_strerror(rc));
						addrlist = NULL;
					}
				}
				*(share - 1) = '/'; /* put delimiter back */

				/* we don't convert the prefixpath delimiters since '\\' is a valid char in posix paths */
				if ((prefixpath = strpbrk(share, "/\\"))) {
					*prefixpath = 0;  /* permanently terminate the string */
					if (!strlen(++prefixpath))
						prefixpath = NULL; /* this needs to be done explicitly */
				}
				if(got_ip) {
					if(verboseflag)
						printf("ip address specified explicitly\n");
					return NULL;
				}
				/* BB should we pass an alternate version of the share name as Unicode */

				return addrlist; 
			} else {
				/* BB add code to find DFS root (send null path on get DFS Referral to specified server here */
				printf("Mounting the DFS root for a particular server not implemented yet\n");
				return NULL;
			}
		}
	}
}

static struct option longopts[] = {
	{ "all", 0, NULL, 'a' },
	{ "help",0, NULL, 'h' },
	{ "move",0, NULL, 'm' },
	{ "bind",0, NULL, 'b' },
	{ "read-only", 0, NULL, 'r' },
	{ "ro", 0, NULL, 'r' },
	{ "verbose", 0, NULL, 'v' },
	{ "version", 0, NULL, 'V' },
	{ "read-write", 0, NULL, 'w' },
	{ "rw", 0, NULL, 'w' },
	{ "options", 1, NULL, 'o' },
	{ "type", 1, NULL, 't' },
	{ "rsize",1, NULL, 'R' },
	{ "wsize",1, NULL, 'W' },
	{ "uid", 1, NULL, '1'},
	{ "gid", 1, NULL, '2'},
	{ "user",1,NULL,'u'},
	{ "username",1,NULL,'u'},
	{ "dom",1,NULL,'d'},
	{ "domain",1,NULL,'d'},
	{ "password",1,NULL,'p'},
	{ "pass",1,NULL,'p'},
	{ "credentials",1,NULL,'c'},
	{ "port",1,NULL,'P'},
	/* { "uuid",1,NULL,'U'}, */ /* BB unimplemented */
	{ NULL, 0, NULL, 0 }
};

/* convert a string to uppercase. return false if the string
 * wasn't ASCII. Return success on a NULL ptr */
static int
uppercase_string(char *string)
{
	if (!string)
		return 1;

	while (*string) {
		/* check for unicode */
		if ((unsigned char) string[0] & 0x80)
			return 0;
		*string = toupper((unsigned char) *string);
		string++;
	}

	return 1;
}

int main(int argc, char ** argv)
{
	int c;
	int flags = MS_MANDLOCK; /* no need to set legacy MS_MGC_VAL */
	char * orgoptions = NULL;
	char * share_name = NULL;
	const char * ipaddr = NULL;
	char * uuid = NULL;
	char * mountpoint = NULL;
	char * options = NULL;
	char * optionstail;
	char * resolved_path = NULL;
	char * temp;
	char * dev_name;
	int rc = 0;
	int rsize = 0;
	int wsize = 0;
	int nomtab = 0;
	int uid = 0;
	int gid = 0;
	int optlen = 0;
	int orgoptlen = 0;
	size_t options_size = 0;
	size_t current_len;
	int retry = 0; /* set when we have to retry mount with uppercase */
	struct addrinfo *addrhead = NULL, *addr;
	struct stat statbuf;
	struct utsname sysinfo;
	struct mntent mountent;
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;
	FILE * pmntfile;

	/* setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE); */

	if(argc && argv) {
		thisprogram = argv[0];
	} else {
		mount_cifs_usage();
		exit(EX_USAGE);
	}

	if(thisprogram == NULL)
		thisprogram = "mount.cifs";

	uname(&sysinfo);
	/* BB add workstation name and domain and pass down */

/* #ifdef _GNU_SOURCE
	printf(" node: %s machine: %s sysname %s domain %s\n", sysinfo.nodename,sysinfo.machine,sysinfo.sysname,sysinfo.domainname);
#endif */
	if(argc > 2) {
		dev_name = argv[1];
		share_name = strndup(argv[1], MAX_UNC_LEN);
		if (share_name == NULL) {
			fprintf(stderr, "%s: %s", argv[0], strerror(ENOMEM));
			exit(EX_SYSERR);
		}
		mountpoint = argv[2];
	} else {
		if ((strcmp (argv[1], "--version") == 0) ||
		    ((strcmp (argv[1], "-V") == 0))) {
			printf ("mount.cifs version: %s.%s%s\n",
			MOUNT_CIFS_VERSION_MAJOR,
			MOUNT_CIFS_VERSION_MINOR,
			MOUNT_CIFS_VENDOR_SUFFIX);
			exit (0);
		}
		mount_cifs_usage();
		exit(EX_USAGE);
	}

	/* add sharename in opts string as unc= parm */

	while ((c = getopt_long (argc, argv, "afFhilL:no:O:rsSU:vVwt:",
			 longopts, NULL)) != -1) {
		switch (c) {
/* No code to do the following  options yet */
/*	case 'l':
		list_with_volumelabel = 1;
		break;
	case 'L':
		volumelabel = optarg;
		break; */
/*	case 'a':	       
		++mount_all;
		break; */

		case '?':
		case 'h':	 /* help */
			mount_cifs_usage ();
			exit(EX_USAGE);
		case 'n':
			++nomtab;
			break;
		case 'b':
#ifdef MS_BIND
			flags |= MS_BIND;
#else
			fprintf(stderr,
				"option 'b' (MS_BIND) not supported\n");
#endif
			break;
		case 'm':
#ifdef MS_MOVE		      
			flags |= MS_MOVE;
#else
			fprintf(stderr,
				"option 'm' (MS_MOVE) not supported\n");
#endif
			break;
		case 'o':
			orgoptions = strdup(optarg);
		    break;
		case 'r':  /* mount readonly */
			flags |= MS_RDONLY;
			break;
		case 'U':
			uuid = optarg;
			break;
		case 'v':
			++verboseflag;
			break;
		case 'V':	   
			printf ("mount.cifs version: %s.%s%s\n",
			MOUNT_CIFS_VERSION_MAJOR,
			MOUNT_CIFS_VERSION_MINOR,
			MOUNT_CIFS_VENDOR_SUFFIX);
			exit (0);
		case 'w':
			flags &= ~MS_RDONLY;
			break;
		case 'R':
			rsize = atoi(optarg) ;
			break;
		case 'W':
			wsize = atoi(optarg);
			break;
		case '1':
			if (isdigit(*optarg)) {
				char *ep;

				uid = strtoul(optarg, &ep, 10);
				if (*ep) {
					printf("bad uid value \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
			} else {
				struct passwd *pw;

				if (!(pw = getpwnam(optarg))) {
					printf("bad user name \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
				uid = pw->pw_uid;
				endpwent();
			}
			break;
		case '2':
			if (isdigit(*optarg)) {
				char *ep;

				gid = strtoul(optarg, &ep, 10);
				if (*ep) {
					printf("bad gid value \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
			} else {
				struct group *gr;

				if (!(gr = getgrnam(optarg))) {
					printf("bad user name \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
				gid = gr->gr_gid;
				endpwent();
			}
			break;
		case 'u':
			got_user = 1;
			user_name = optarg;
			break;
		case 'd':
			domain_name = optarg; /* BB fix this - currently ignored */
			got_domain = 1;
			break;
		case 'p':
			if(mountpassword == NULL)
				mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
			if(mountpassword) {
				got_password = 1;
				strlcpy(mountpassword,optarg,MOUNT_PASSWD_SIZE+1);
			}
			break;
		case 'S':
			get_password_from_file(0 /* stdin */,NULL);
			break;
		case 't':
			break;
		case 'f':
			++fakemnt;
			break;
		default:
			printf("unknown mount option %c\n",c);
			mount_cifs_usage();
			exit(EX_USAGE);
		}
	}

	if((argc < 3) || (dev_name == NULL) || (mountpoint == NULL)) {
		mount_cifs_usage();
		exit(EX_USAGE);
	}

	if (getenv("PASSWD")) {
		if(mountpassword == NULL)
			mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
		if(mountpassword) {
			strlcpy(mountpassword,getenv("PASSWD"),MOUNT_PASSWD_SIZE+1);
			got_password = 1;
		}
	} else if (getenv("PASSWD_FD")) {
		get_password_from_file(atoi(getenv("PASSWD_FD")),NULL);
	} else if (getenv("PASSWD_FILE")) {
		get_password_from_file(0, getenv("PASSWD_FILE"));
	}

        if (orgoptions && parse_options(&orgoptions, &flags)) {
                rc = EX_USAGE;
		goto mount_exit;
	}
	addrhead = addr = parse_server(&share_name);
	if((addrhead == NULL) && (got_ip == 0)) {
		printf("No ip address specified and hostname not found\n");
		rc = EX_USAGE;
		goto mount_exit;
	}
	
	/* BB save off path and pop after mount returns? */
	resolved_path = (char *)malloc(PATH_MAX+1);
	if(resolved_path) {
		/* Note that if we can not canonicalize the name, we get
		another chance to see if it is valid when we chdir to it */
		if (realpath(mountpoint, resolved_path)) {
			mountpoint = resolved_path; 
		}
	}
	if(chdir(mountpoint)) {
		printf("mount error: can not change directory into mount target %s\n",mountpoint);
		rc = EX_USAGE;
		goto mount_exit;
	}

	if(stat (".", &statbuf)) {
		printf("mount error: mount point %s does not exist\n",mountpoint);
		rc = EX_USAGE;
		goto mount_exit;
	}

	if (S_ISDIR(statbuf.st_mode) == 0) {
		printf("mount error: mount point %s is not a directory\n",mountpoint);
		rc = EX_USAGE;
		goto mount_exit;
	}

	if((getuid() != 0) && (geteuid() == 0)) {
		if((statbuf.st_uid == getuid()) && (S_IRWXU == (statbuf.st_mode & S_IRWXU))) {
#ifndef CIFS_ALLOW_USR_SUID
			/* Do not allow user mounts to control suid flag
			for mount unless explicitly built that way */
			flags |= MS_NOSUID | MS_NODEV;
#endif						
		} else {
			printf("mount error: permission denied or not superuser and mount.cifs not installed SUID\n"); 
			exit(EX_USAGE);
		}
	}

	if(got_user == 0) {
		/* Note that the password will not be retrieved from the
		   USER env variable (ie user%password form) as there is
		   already a PASSWD environment varaible */
		if (getenv("USER"))
			user_name = strdup(getenv("USER"));
		if (user_name == NULL)
			user_name = getusername();
		got_user = 1;
	}
       
	if(got_password == 0) {
		char *tmp_pass = getpass("Password: "); /* BB obsolete sys call but
							   no good replacement yet. */
		mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
		if (!tmp_pass || !mountpassword) {
			printf("Password not entered, exiting\n");
			exit(EX_USAGE);
		}
		strlcpy(mountpassword, tmp_pass, MOUNT_PASSWD_SIZE+1);
		got_password = 1;
	}
	/* FIXME launch daemon (handles dfs name resolution and credential change) 
	   remember to clear parms and overwrite password field before launching */
	if(orgoptions) {
		optlen = strlen(orgoptions);
		orgoptlen = optlen;
	} else
		optlen = 0;
	if(share_name)
		optlen += strlen(share_name) + 4;
	else {
		printf("No server share name specified\n");
		printf("\nMounting the DFS root for server not implemented yet\n");
                exit(EX_USAGE);
	}
	if(user_name)
		optlen += strlen(user_name) + 6;
	optlen += MAX_ADDRESS_LEN + 4;
	if(mountpassword)
		optlen += strlen(mountpassword) + 6;
mount_retry:
	SAFE_FREE(options);
	options_size = optlen + 10 + DOMAIN_SIZE;
	options = (char *)malloc(options_size /* space for commas in password */ + 8 /* space for domain=  , domain name itself was counted as part of the length username string above */);

	if(options == NULL) {
		printf("Could not allocate memory for mount options\n");
		exit(EX_SYSERR);
	}

	strlcpy(options, "unc=", options_size);
	strlcat(options,share_name,options_size);
	/* scan backwards and reverse direction of slash */
	temp = strrchr(options, '/');
	if(temp > options + 6)
		*temp = '\\';
	if(user_name) {
		/* check for syntax like user=domain\user */
		if(got_domain == 0)
			domain_name = check_for_domain(&user_name);
		strlcat(options,",user=",options_size);
		strlcat(options,user_name,options_size);
	}
	if(retry == 0) {
		if(domain_name) {
			/* extra length accounted for in option string above */
			strlcat(options,",domain=",options_size);
			strlcat(options,domain_name,options_size);
		}
	}

	strlcat(options,",ver=",options_size);
	strlcat(options,MOUNT_CIFS_VERSION_MAJOR,options_size);

	if(orgoptions) {
		strlcat(options,",",options_size);
		strlcat(options,orgoptions,options_size);
	}
	if(prefixpath) {
		strlcat(options,",prefixpath=",options_size);
		strlcat(options,prefixpath,options_size); /* no need to cat the / */
	}

	/* convert all '\\' to '/' in share portion so that /proc/mounts looks pretty */
	replace_char(dev_name, '\\', '/', strlen(share_name));

	if (!got_ip && addr) {
		strlcat(options, ",ip=", options_size);
		current_len = strnlen(options, options_size);
		optionstail = options + current_len;
		switch (addr->ai_addr->sa_family) {
		case AF_INET6:
			addr6 = (struct sockaddr_in6 *) addr->ai_addr;
			ipaddr = inet_ntop(AF_INET6, &addr6->sin6_addr, optionstail,
					   options_size - current_len);
			break;
		case AF_INET:
			addr4 = (struct sockaddr_in *) addr->ai_addr;
			ipaddr = inet_ntop(AF_INET, &addr4->sin_addr, optionstail,
					   options_size - current_len);
			break;
		}

		/* if the address looks bogus, try the next one */
		if (!ipaddr) {
			addr = addr->ai_next;
			if (addr)
				goto mount_retry;
			rc = EX_SYSERR;
			goto mount_exit;
		}
	}

	if(verboseflag)
		fprintf(stderr, "\nmount.cifs kernel mount options: %s", options);

	if (mountpassword) {
		/*
		 * Commas have to be doubled, or else they will
		 * look like the parameter separator
		 */
		if(retry == 0)
			check_for_comma(&mountpassword);
		strlcat(options,",pass=",options_size);
		strlcat(options,mountpassword,options_size);
		if (verboseflag)
			fprintf(stderr, ",pass=********");
	}

	if (verboseflag)
		fprintf(stderr, "\n");

	if (!fakemnt && mount(dev_name, mountpoint, "cifs", flags, options)) {
		switch (errno) {
		case ECONNREFUSED:
		case EHOSTUNREACH:
			if (addr) {
				addr = addr->ai_next;
				if (addr)
					goto mount_retry;
			}
			break;
		case ENODEV:
			printf("mount error: cifs filesystem not supported by the system\n");
			break;
		case ENXIO:
			if(retry == 0) {
				retry = 1;
				if (uppercase_string(dev_name) &&
				    uppercase_string(share_name) &&
				    uppercase_string(prefixpath)) {
					printf("retrying with upper case share name\n");
					goto mount_retry;
				}
			}
		}
		printf("mount error(%d): %s\n", errno, strerror(errno));
		printf("Refer to the mount.cifs(8) manual page (e.g.man mount.cifs)\n");
		rc = EX_FAIL;
		goto mount_exit;
	}

	if (nomtab)
		goto mount_exit;
	atexit(unlock_mtab);
	rc = lock_mtab();
	if (rc) {
		printf("cannot lock mtab");
		goto mount_exit;
	}
	pmntfile = setmntent(MOUNTED, "a+");
	if (!pmntfile) {
		printf("could not update mount table\n");
		unlock_mtab();
		rc = EX_FILEIO;
		goto mount_exit;
	}
	mountent.mnt_fsname = dev_name;
	mountent.mnt_dir = mountpoint;
	mountent.mnt_type = CONST_DISCARD(char *,"cifs");
	mountent.mnt_opts = (char *)malloc(220);
	if(mountent.mnt_opts) {
		char * mount_user = getusername();
		memset(mountent.mnt_opts,0,200);
		if(flags & MS_RDONLY)
			strlcat(mountent.mnt_opts,"ro",220);
		else
			strlcat(mountent.mnt_opts,"rw",220);
		if(flags & MS_MANDLOCK)
			strlcat(mountent.mnt_opts,",mand",220);
		if(flags & MS_NOEXEC)
			strlcat(mountent.mnt_opts,",noexec",220);
		if(flags & MS_NOSUID)
			strlcat(mountent.mnt_opts,",nosuid",220);
		if(flags & MS_NODEV)
			strlcat(mountent.mnt_opts,",nodev",220);
		if(flags & MS_SYNCHRONOUS)
			strlcat(mountent.mnt_opts,",sync",220);
		if(mount_user) {
			if(getuid() != 0) {
				strlcat(mountent.mnt_opts,
					",user=", 220);
				strlcat(mountent.mnt_opts,
					mount_user, 220);
			}
		}
	}
	mountent.mnt_freq = 0;
	mountent.mnt_passno = 0;
	rc = addmntent(pmntfile,&mountent);
	endmntent(pmntfile);
	unlock_mtab();
	SAFE_FREE(mountent.mnt_opts);
	if (rc)
		rc = EX_FILEIO;
mount_exit:
	if(mountpassword) {
		int len = strlen(mountpassword);
		memset(mountpassword,0,len);
		SAFE_FREE(mountpassword);
	}

	if (addrhead)
		freeaddrinfo(addrhead);
	SAFE_FREE(options);
	SAFE_FREE(orgoptions);
	SAFE_FREE(resolved_path);
	SAFE_FREE(share_name);
	exit(rc);
}
