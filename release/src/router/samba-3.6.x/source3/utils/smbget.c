/*
   smbget: a wget-like utility with support for recursive downloading and 
   	smb:// urls
   Copyright (C) 2003-2004 Jelmer Vernooij <jelmer@samba.org>

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

#include "includes.h"
#include "system/filesys.h"
#include "popt_common.h"
#include "libsmbclient.h"

#if _FILE_OFFSET_BITS==64
#define OFF_T_FORMAT "%lld"
#define OFF_T_FORMAT_CAST long long
#else
#define OFF_T_FORMAT "%ld"
#define OFF_T_FORMAT_CAST long
#endif

static int columns = 0;

static int debuglevel, update;
static char *outputfile;


static time_t total_start_time = 0;
static off_t total_bytes = 0;

#define SMB_MAXPATHLEN MAXPATHLEN

/* Number of bytes to read when checking whether local and remote file are really the same file */
#define RESUME_CHECK_SIZE 				512
#define RESUME_DOWNLOAD_OFFSET			1024
#define RESUME_CHECK_OFFSET				RESUME_DOWNLOAD_OFFSET+RESUME_CHECK_SIZE
/* Number of bytes to read at once */
#define SMB_DEFAULT_BLOCKSIZE 					64000

static const char *username = NULL, *password = NULL, *workgroup = NULL;
static int nonprompt = 0, quiet = 0, dots = 0, keep_permissions = 0, verbose = 0, send_stdout = 0;
static int blocksize = SMB_DEFAULT_BLOCKSIZE;

static int smb_download_file(const char *base, const char *name, int recursive,
			     int resume, int toplevel, char *outfile);

static int get_num_cols(void)
{
#ifdef TIOCGWINSZ
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
		return 0;
	}
	return ws.ws_col;
#else
#warning No support for TIOCGWINSZ
	char *cols = getenv("COLUMNS");
	if(!cols) return 0;
	return atoi(cols);
#endif
}

static void change_columns(int sig)
{
	columns = get_num_cols();
}

static void human_readable(off_t s, char *buffer, int l)
{
	if (s > 1024 * 1024 * 1024) {
		snprintf(buffer, l, "%.2fGB", 1.0 * s / (1024 * 1024 * 1024));
	} else if (s > 1024 * 1024) {
		snprintf(buffer, l, "%.2fMB", 1.0 * s / (1024 * 1024));
	} else if (s > 1024) {
		snprintf(buffer, l, "%.2fkB", 1.0 * s / 1024);
	} else {
		snprintf(buffer, l, OFF_T_FORMAT"b", (OFF_T_FORMAT_CAST)s);
	}
}

static void get_auth_data(const char *srv, const char *shr, char *wg, int wglen, char *un, int unlen, char *pw, int pwlen)
{
	static char hasasked = 0;
	char *wgtmp, *usertmp;
	char tmp[128];

	if(hasasked) return;
	hasasked = 1;

	if(!nonprompt && !username) {
		printf("Username for %s at %s [guest] ", shr, srv);
		if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
			return;
		}
		if ((strlen(tmp) > 0) && (tmp[strlen(tmp)-1] == '\n')) {
			tmp[strlen(tmp)-1] = '\0';
		}
		strncpy(un, tmp, unlen-1);
	} else if(username) strncpy(un, username, unlen-1);

	if(!nonprompt && !password) {
		char *prompt, *pass;
		if (asprintf(&prompt, "Password for %s at %s: ", shr, srv) == -1) {
			return;
		}
		pass = getpass(prompt);
		free(prompt);
		strncpy(pw, pass, pwlen-1);
	} else if(password) strncpy(pw, password, pwlen-1);

	if(workgroup)strncpy(wg, workgroup, wglen-1);

	wgtmp = SMB_STRNDUP(wg, wglen); 
	usertmp = SMB_STRNDUP(un, unlen);
	if(!quiet)printf("Using workgroup %s, %s%s\n", wgtmp, *usertmp?"user ":"guest user", usertmp);
	free(wgtmp); free(usertmp);
}

/* Return 1 on error, 0 on success. */

static int smb_download_dir(const char *base, const char *name, int resume)
{
	char path[SMB_MAXPATHLEN];
	int dirhandle;
	struct smbc_dirent *dirent;
	const char *relname = name;
	char *tmpname;
	struct stat remotestat;
	int ret = 0;

	snprintf(path, SMB_MAXPATHLEN-1, "%s%s%s", base, (base[0] && name[0] && name[0] != '/' && base[strlen(base)-1] != '/')?"/":"", name);

	/* List files in directory and call smb_download_file on them */
	dirhandle = smbc_opendir(path);
	if(dirhandle < 1) {
		if (errno == ENOTDIR) {
			return smb_download_file(base, name, 1, resume,
						 0, NULL);
		}
		fprintf(stderr, "Can't open directory %s: %s\n", path, strerror(errno));
		return 1;
	}

	while(*relname == '/')relname++;
	mkdir(relname, 0755);

	tmpname = SMB_STRDUP(name);

	while((dirent = smbc_readdir(dirhandle))) {
		char *newname;
		if(!strcmp(dirent->name, ".") || !strcmp(dirent->name, ".."))continue;
		if (asprintf(&newname, "%s/%s", tmpname, dirent->name) == -1) {
			return 1;
		}
		switch(dirent->smbc_type) {
		case SMBC_DIR:
			ret = smb_download_dir(base, newname, resume);
			break;

		case SMBC_WORKGROUP:
			ret = smb_download_dir("smb://", dirent->name, resume);
			break;

		case SMBC_SERVER:
			ret = smb_download_dir("smb://", dirent->name, resume);
			break;

		case SMBC_FILE:
			ret = smb_download_file(base, newname, 1, resume, 0,
						NULL);
			break;

		case SMBC_FILE_SHARE:
			ret = smb_download_dir(base, newname, resume);
			break;

		case SMBC_PRINTER_SHARE:
			if(!quiet)printf("Ignoring printer share %s\n", dirent->name);
			break;

		case SMBC_COMMS_SHARE:
			if(!quiet)printf("Ignoring comms share %s\n", dirent->name);
			break;

		case SMBC_IPC_SHARE:
			if(!quiet)printf("Ignoring ipc$ share %s\n", dirent->name);
			break;

		default:
			fprintf(stderr, "Ignoring file '%s' of type '%d'\n", newname, dirent->smbc_type);
			break;
		}
		free(newname);
	}
	free(tmpname);

	if(keep_permissions) {
		if(smbc_fstat(dirhandle, &remotestat) < 0) {
			fprintf(stderr, "Unable to get stats on %s on remote server\n", path);
			smbc_closedir(dirhandle);
			return 1;
		}

		if(chmod(relname, remotestat.st_mode) < 0) {
			fprintf(stderr, "Unable to change mode of local dir %s to %o\n", relname,
				(unsigned int)remotestat.st_mode);
			smbc_closedir(dirhandle);
			return 1;
		}
	}

	smbc_closedir(dirhandle);
	return ret;
}

static char *print_time(long t)
{
	static char buffer[100];
	int secs, mins, hours;
	if(t < -1) {
		strncpy(buffer, "Unknown", sizeof(buffer));
		return buffer;
	}

	secs = (int)t % 60;
	mins = (int)t / 60 % 60;
	hours = (int)t / (60 * 60);
	snprintf(buffer, sizeof(buffer)-1, "%02d:%02d:%02d", hours, mins, secs);
	return buffer;
}

static void print_progress(const char *name, time_t start, time_t now, off_t start_pos, off_t pos, off_t total)
{
	double avg = 0.0;
	long  eta = -1; 
	double prcnt = 0.0;
	char hpos[20], htotal[20], havg[20];
	char *status, *filename;
	int len;
	if(now - start)avg = 1.0 * (pos - start_pos) / (now - start);
	eta = (total - pos) / avg;
	if(total)prcnt = 100.0 * pos / total;

	human_readable(pos, hpos, sizeof(hpos));
	human_readable(total, htotal, sizeof(htotal));
	human_readable(avg, havg, sizeof(havg));

	len = asprintf(&status, "%s of %s (%.2f%%) at %s/s ETA: %s", hpos, htotal, prcnt, havg, print_time(eta));
	if (len == -1) {
		return;
	}

	if(columns) {
		int required = strlen(name), available = columns - len - strlen("[] ");
		if(required > available) {
			if (asprintf(&filename, "...%s", name + required - available + 3) == -1) {
				return;
			}
		} else {
			filename = SMB_STRNDUP(name, available);
		}
	} else filename = SMB_STRDUP(name);

	fprintf(stderr, "\r[%s] %s", filename, status);

	free(filename); free(status);
}

/* Return 1 on error, 0 on success. */

static int smb_download_file(const char *base, const char *name, int recursive,
			     int resume, int toplevel, char *outfile)
{
	int remotehandle, localhandle;
	time_t start_time = time_mono(NULL);
	const char *newpath;
	char path[SMB_MAXPATHLEN];
	char checkbuf[2][RESUME_CHECK_SIZE];
	char *readbuf = NULL;
	off_t offset_download = 0, offset_check = 0, curpos = 0, start_offset = 0;
	struct stat localstat, remotestat;

	snprintf(path, SMB_MAXPATHLEN-1, "%s%s%s", base, (*base && *name && name[0] != '/' && base[strlen(base)-1] != '/')?"/":"", name);

	remotehandle = smbc_open(path, O_RDONLY, 0755);

	if(remotehandle < 0) {
		switch(errno) {
		case EISDIR: 
			if(!recursive) {
				fprintf(stderr, "%s is a directory. Specify -R to download recursively\n", path);
				return 1;
			}
			return smb_download_dir(base, name, resume);

		case ENOENT:
			fprintf(stderr, "%s can't be found on the remote server\n", path);
			return 1;

		case ENOMEM:
			fprintf(stderr, "Not enough memory\n");
			return 1;

		case ENODEV:
			fprintf(stderr, "The share name used in %s does not exist\n", path);
			return 1;

		case EACCES:
			fprintf(stderr, "You don't have enough permissions to access %s\n", path);
			return 1;

		default:
			perror("smbc_open");
			return 1;
		}
	} 

	if(smbc_fstat(remotehandle, &remotestat) < 0) {
		fprintf(stderr, "Can't stat %s: %s\n", path, strerror(errno));
		return 1;
	}

	if(outfile) newpath = outfile;
	else if(!name[0]) {
		newpath = strrchr(base, '/');
		if(newpath)newpath++; else newpath = base;
	} else newpath = name;

	if (!toplevel && (newpath[0] == '/')) {
		newpath++;
	}

	/* Open local file according to the mode */
	if(update) {
		/* if it is up-to-date, skip */
		if(stat(newpath, &localstat) == 0 &&
				localstat.st_mtime >= remotestat.st_mtime) {
			if(verbose)
				printf("%s is up-to-date, skipping\n", newpath);
			smbc_close(remotehandle);
			return 0;
		}
		/* else open it for writing and truncate if it exists */
		localhandle = open(newpath, O_CREAT | O_NONBLOCK | O_RDWR | O_TRUNC, 0775);
		if(localhandle < 0) {
			fprintf(stderr, "Can't open %s : %s\n", newpath,
					strerror(errno));
			smbc_close(remotehandle);
			return 1;
		}
		/* no offset */
	} else if(!send_stdout) {
		localhandle = open(newpath, O_CREAT | O_NONBLOCK | O_RDWR | (!resume?O_EXCL:0), 0755);
		if(localhandle < 0) {
			fprintf(stderr, "Can't open %s: %s\n", newpath, strerror(errno));
			smbc_close(remotehandle);
			return 1;
		}

		if (fstat(localhandle, &localstat) != 0) {
			fprintf(stderr, "Can't fstat %s: %s\n", newpath, strerror(errno));
			smbc_close(remotehandle);
			close(localhandle);
			return 1;
		}

		start_offset = localstat.st_size;

		if(localstat.st_size && localstat.st_size == remotestat.st_size) {
			if(verbose)fprintf(stderr, "%s is already downloaded completely.\n", path);
			else if(!quiet)fprintf(stderr, "%s\n", path);
			smbc_close(remotehandle);
			close(localhandle);
			return 0;
		}

		if(localstat.st_size > RESUME_CHECK_OFFSET && remotestat.st_size > RESUME_CHECK_OFFSET) {
			offset_download = localstat.st_size - RESUME_DOWNLOAD_OFFSET;
			offset_check = localstat.st_size - RESUME_CHECK_OFFSET;
			if(verbose)printf("Trying to start resume of %s at "OFF_T_FORMAT"\n"
				   "At the moment "OFF_T_FORMAT" of "OFF_T_FORMAT" bytes have been retrieved\n",
				newpath, (OFF_T_FORMAT_CAST)offset_check, 
				(OFF_T_FORMAT_CAST)localstat.st_size,
				(OFF_T_FORMAT_CAST)remotestat.st_size);
		}

		if(offset_check) { 
			off_t off1, off2;
			/* First, check all bytes from offset_check to offset_download */
			off1 = lseek(localhandle, offset_check, SEEK_SET);
			if(off1 < 0) {
				fprintf(stderr, "Can't seek to "OFF_T_FORMAT" in local file %s\n",
					(OFF_T_FORMAT_CAST)offset_check, newpath);
				smbc_close(remotehandle); close(localhandle);
				return 1;
			}

			off2 = smbc_lseek(remotehandle, offset_check, SEEK_SET); 
			if(off2 < 0) {
				fprintf(stderr, "Can't seek to "OFF_T_FORMAT" in remote file %s\n",
					(OFF_T_FORMAT_CAST)offset_check, newpath);
				smbc_close(remotehandle); close(localhandle);
				return 1;
			}

			if(off1 != off2) {
				fprintf(stderr, "Offset in local and remote files is different (local: "OFF_T_FORMAT", remote: "OFF_T_FORMAT")\n",
					(OFF_T_FORMAT_CAST)off1,
					(OFF_T_FORMAT_CAST)off2);
				smbc_close(remotehandle); close(localhandle);
				return 1;
			}

			if(smbc_read(remotehandle, checkbuf[0], RESUME_CHECK_SIZE) != RESUME_CHECK_SIZE) {
				fprintf(stderr, "Can't read %d bytes from remote file %s\n", RESUME_CHECK_SIZE, path);
				smbc_close(remotehandle); close(localhandle);
				return 1;
			}

			if(read(localhandle, checkbuf[1], RESUME_CHECK_SIZE) != RESUME_CHECK_SIZE) {
				fprintf(stderr, "Can't read %d bytes from local file %s\n", RESUME_CHECK_SIZE, name);
				smbc_close(remotehandle); close(localhandle);
				return 1;
			}

			if(memcmp(checkbuf[0], checkbuf[1], RESUME_CHECK_SIZE) == 0) {
				if(verbose)printf("Current local and remote file appear to be the same. Starting download from offset "OFF_T_FORMAT"\n", (OFF_T_FORMAT_CAST)offset_download);
			} else {
				fprintf(stderr, "Local and remote file appear to be different, not doing resume for %s\n", path);
				smbc_close(remotehandle); close(localhandle);
				return 1;
			}
		}
	} else {
		localhandle = STDOUT_FILENO;
		start_offset = 0;
		offset_download = 0;
		offset_check = 0;
	}

	readbuf = (char *)SMB_MALLOC(blocksize);
	if (!readbuf) {
		return 1;
	}

	/* Now, download all bytes from offset_download to the end */
	for(curpos = offset_download; curpos < remotestat.st_size; curpos+=blocksize) {
		ssize_t bytesread = smbc_read(remotehandle, readbuf, blocksize);
		if(bytesread < 0) {
			fprintf(stderr, "Can't read %u bytes at offset "OFF_T_FORMAT", file %s\n", (unsigned int)blocksize, (OFF_T_FORMAT_CAST)curpos, path);
			smbc_close(remotehandle);
			if (localhandle != STDOUT_FILENO) close(localhandle);
			free(readbuf);
			return 1;
		}

		total_bytes += bytesread;

		if(write(localhandle, readbuf, bytesread) < 0) {
			fprintf(stderr, "Can't write %u bytes to local file %s at offset "OFF_T_FORMAT"\n", (unsigned int)bytesread, path, (OFF_T_FORMAT_CAST)curpos);
			free(readbuf);
			smbc_close(remotehandle);
			if (localhandle != STDOUT_FILENO) close(localhandle);
			return 1;
		}

		if(dots)fputc('.', stderr);
		else if(!quiet) {
			print_progress(newpath, start_time, time_mono(NULL),
					start_offset, curpos, remotestat.st_size);
		}
	}

	free(readbuf);

	if(dots){
		fputc('\n', stderr);
		printf("%s downloaded\n", path);
	} else if(!quiet) {
		int i;
		fprintf(stderr, "\r%s", path);
		if(columns) {
			for(i = strlen(path); i < columns; i++) {
				fputc(' ', stderr);
			}
		}
		fputc('\n', stderr);
	}

	if(keep_permissions && !send_stdout) {
		if(fchmod(localhandle, remotestat.st_mode) < 0) {
			fprintf(stderr, "Unable to change mode of local file %s to %o\n", path,
				(unsigned int)remotestat.st_mode);
			smbc_close(remotehandle);
			close(localhandle);
			return 1;
		}
	}

	smbc_close(remotehandle);
	if (localhandle != STDOUT_FILENO) close(localhandle);
	return 0;
}

static void clean_exit(void)
{
	char bs[100];
	human_readable(total_bytes, bs, sizeof(bs));
	if(!quiet)fprintf(stderr, "Downloaded %s in %lu seconds\n", bs,
		(unsigned long)(time_mono(NULL) - total_start_time));
	exit(0);
}

static void signal_quit(int v)
{
	clean_exit();
}

static int readrcfile(const char *name, const struct poptOption long_options[])
{
	FILE *fd = fopen(name, "r");
	int lineno = 0, i;
	char var[101], val[101];
	char found;
	int *intdata; char **stringdata;
	if(!fd) {
		fprintf(stderr, "Can't open RC file %s\n", name);
		return 1;
	}

	while(!feof(fd)) {
		lineno++;
		if(fscanf(fd, "%100s %100s\n", var, val) < 2) {
			fprintf(stderr, "Can't parse line %d of %s, ignoring.\n", lineno, name);
			continue;
		}

		found = 0;

		for(i = 0; long_options[i].shortName; i++) {
			if(!long_options[i].longName)continue;
			if(strcmp(long_options[i].longName, var)) continue;
			if(!long_options[i].arg)continue;

			switch(long_options[i].argInfo) {
			case POPT_ARG_NONE:
				intdata = (int *)long_options[i].arg;
				if(!strcmp(val, "on")) *intdata = 1;
				else if(!strcmp(val, "off")) *intdata = 0;
				else fprintf(stderr, "Illegal value %s for %s at line %d in %s\n", val, var, lineno, name);
				break;
			case POPT_ARG_INT:
				intdata = (int *)long_options[i].arg;
				*intdata = atoi(val);
				break;
			case POPT_ARG_STRING:
				stringdata = (char **)long_options[i].arg;
				*stringdata = SMB_STRDUP(val);
				break;
			default:
				fprintf(stderr, "Invalid variable %s at line %d in %s\n", var, lineno, name);
				break;
			}

			found = 1;
		}
		if(!found) {
			fprintf(stderr, "Invalid variable %s at line %d in %s\n", var, lineno, name);
		}
	}

	fclose(fd);
	return 0;
}

int main(int argc, const char **argv)
{
	int c = 0;
	const char *file = NULL;
	char *rcfile = NULL;
	bool smb_encrypt = false;
	int resume = 0, recursive = 0;
	TALLOC_CTX *frame = talloc_stackframe();
	int ret = 0;
	struct poptOption long_options[] = {
		{"guest", 'a', POPT_ARG_NONE, NULL, 'a', "Work as user guest" },	
		{"encrypt", 'e', POPT_ARG_NONE, NULL, 'e', "Encrypt SMB transport (UNIX extended servers only)" },	
		{"resume", 'r', POPT_ARG_NONE, &resume, 0, "Automatically resume aborted files" },
		{"update", 'U',  POPT_ARG_NONE, &update, 0, "Download only when remote file is newer than local file or local file is missing"},
		{"recursive", 'R',  POPT_ARG_NONE, &recursive, 0, "Recursively download files" },
		{"username", 'u', POPT_ARG_STRING, &username, 'u', "Username to use" },
		{"password", 'p', POPT_ARG_STRING, &password, 'p', "Password to use" },
		{"workgroup", 'w', POPT_ARG_STRING, &workgroup, 'w', "Workgroup to use (optional)" },
		{"nonprompt", 'n', POPT_ARG_NONE, &nonprompt, 'n', "Don't ask anything (non-interactive)" },
		{"debuglevel", 'd', POPT_ARG_INT, &debuglevel, 'd', "Debuglevel to use" },
		{"outputfile", 'o', POPT_ARG_STRING, &outputfile, 'o', "Write downloaded data to specified file" },
		{"stdout", 'O', POPT_ARG_NONE, &send_stdout, 'O', "Write data to stdout" },
		{"dots", 'D', POPT_ARG_NONE, &dots, 'D', "Show dots as progress indication" },
		{"quiet", 'q', POPT_ARG_NONE, &quiet, 'q', "Be quiet" },
		{"verbose", 'v', POPT_ARG_NONE, &verbose, 'v', "Be verbose" },
		{"keep-permissions", 'P', POPT_ARG_NONE, &keep_permissions, 'P', "Keep permissions" },
		{"blocksize", 'b', POPT_ARG_INT, &blocksize, 'b', "Change number of bytes in a block"},
		{"rcfile", 'f', POPT_ARG_STRING, NULL, 'f', "Use specified rc file"},
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	poptContext pc;

	load_case_tables();

	/* only read rcfile if it exists */
	if (asprintf(&rcfile, "%s/.smbgetrc", getenv("HOME")) == -1) {
		return 1;
	}
	if(access(rcfile, F_OK) == 0) 
		readrcfile(rcfile, long_options);
	free(rcfile);

#ifdef SIGWINCH
	signal(SIGWINCH, change_columns);
#endif
	signal(SIGINT, signal_quit);
	signal(SIGTERM, signal_quit);

	pc = poptGetContext(argv[0], argc, argv, long_options, 0);

	while((c = poptGetNextOpt(pc)) >= 0) {
		switch(c) {
		case 'f':
			readrcfile(poptGetOptArg(pc), long_options);
			break;
		case 'a':
			username = ""; password = "";
			break;
		case 'e':
			smb_encrypt = true;
			break;
		}
	}

	if((send_stdout || resume || outputfile) && update) {
		fprintf(stderr, "The -o, -R or -O and -U options can not be used together.\n");
		return 1;
	}
	if((send_stdout || outputfile) && recursive) {
		fprintf(stderr, "The -o or -O and -R options can not be used together.\n");
		return 1;
	}

	if(outputfile && send_stdout) {
		fprintf(stderr, "The -o and -O options cannot be used together.\n");
		return 1;
	}

	if(smbc_init(get_auth_data, debuglevel) < 0) {
		fprintf(stderr, "Unable to initialize libsmbclient\n");
		return 1;
	}

	if (smb_encrypt) {
		SMBCCTX *smb_ctx = smbc_set_context(NULL);
		smbc_option_set(smb_ctx,
			CONST_DISCARD(char *, "smb_encrypt_level"),
			"require");
	}

	columns = get_num_cols();

	total_start_time = time_mono(NULL);

	while ( (file = poptGetArg(pc)) ) {
		if (!recursive) 
			ret = smb_download_file(file, "", recursive, resume,
						1, outputfile);
		else 
			ret = smb_download_dir(file, "", resume);
	}

	TALLOC_FREE(frame);
	if ( ret == 0){
		clean_exit();
	}
	return ret;
}
