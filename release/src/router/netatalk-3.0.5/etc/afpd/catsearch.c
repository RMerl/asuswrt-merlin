/* 
 * Netatalk 2002 (c)
 * Copyright (C) 1990, 1993 Regents of The University of Michigan
 * Copyright (C) 2010 Frank Lahm
 * All Rights Reserved. See COPYRIGHT
 */


/*
 * This file contains FPCatSearch implementation. FPCatSearch performs
 * file/directory search based on specified criteria. It is used by client
 * to perform fast searches on (propably) big volumes. So, it has to be
 * pretty fast.
 *
 * This implementation bypasses most of adouble/libatalk stuff as long as
 * possible and does a standard filesystem search. It calls higher-level
 * libatalk/afpd functions only when it is really needed, mainly while
 * returning some non-UNIX information or filtering by non-UNIX criteria.
 *
 * Initial version written by Rafal Lewczuk <rlewczuk@pronet.pl>
 *
 * Starting with Netatalk 2.2 searching by name criteria utilizes the
 * CNID database in conjunction with an enhanced cnid_dbd. This requires
 * the use of cnidscheme:dbd for the searched volume, the new functionality
 * is not built into cnidscheme:cdb.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include <sys/file.h>
#include <netinet/in.h>

#include <atalk/afp.h>
#include <atalk/adouble.h>
#include <atalk/logger.h>
#include <atalk/cnid.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/util.h>
#include <atalk/bstradd.h>
#include <atalk/unicode.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>

#include "desktop.h"
#include "directory.h"
#include "dircache.h"
#include "file.h"
#include "volume.h"
#include "filedir.h"
#include "fork.h"


struct finderinfo {
	uint32_t f_type;
	uint32_t creator;
	uint16_t attrs;    /* File attributes (high 8 bits)*/
	uint16_t label;    /* Label (low 8 bits )*/
	char reserved[22]; /* Unknown (at least for now...) */
};

typedef char packed_finder[ADEDLEN_FINDERI];

/* Known attributes:
 * 0x04 - has a custom icon
 * 0x20 - name/icon is locked
 * 0x40 - is invisible
 * 0x80 - is alias
 *
 * Known labels:
 * 0x02 - project 2
 * 0x04 - project 1
 * 0x06 - personal
 * 0x08 - cool
 * 0x0a - in progress
 * 0x0c - hot
 * 0x0e - essential
 */

/* This is our search-criteria structure. */
struct scrit {
	uint32_t rbitmap;          /* Request bitmap - which values should we check ? */
	uint16_t fbitmap, dbitmap; /* file & directory bitmap - which values should we return ? */
	uint16_t attr;             /* File attributes */
	time_t cdate;               /* Creation date */
	time_t mdate;               /* Last modification date */
	time_t bdate;               /* Last backup date */
	uint32_t pdid;             /* Parent DID */
    uint16_t offcnt;           /* Offspring count */
	struct finderinfo finfo;    /* Finder info */
	char lname[64];             /* Long name */ 
	char utf8name[514];         /* UTF8 or UCS2 name */ /* for convert_charset dest_len parameter +2 */
};

/*
 * Directory tree search is recursive by its nature. But AFP specification
 * requires FPCatSearch to pause after returning n results and be able to
 * resume the search later. So we have to do recursive search using flat
 * (iterative) algorithm and remember all directories to look into in an
 * stack-like structure. The structure below is one item of directory stack.
 *
 */
struct dsitem {
    cnid_t ds_did;         /* CNID of this directory           */
    int    ds_checked;     /* Have we checked this directory ? */
};
 

#define DS_BSIZE 128
static int save_cidx = -1; /* Saved index of currently scanned directory. */
static struct dsitem *dstack = NULL; /* Directory stack data... */
static int dssize = 0;  	     /* Directory stack (allocated) size... */
static int dsidx = 0;   	     /* First free item index... */
static struct scrit c1, c2;          /* search criteria */

/* Clears directory stack. */
static void clearstack(void) 
{
	save_cidx = -1;
	while (dsidx > 0) {
		dsidx--;
	}
}

/* Puts new item onto directory stack. */
static int addstack(char *uname, struct dir *dir, int pidx)
{
	struct dsitem *ds;
    struct dsitem *tmpds = NULL;

	/* check if we have some space on stack... */
	if (dsidx >= dssize) {
		dssize += DS_BSIZE;
		tmpds = realloc(dstack, dssize * sizeof(struct dsitem));	
		if (tmpds == NULL) {
            clearstack();
            free(dstack);
			return -1;
        }
        dstack = tmpds;
	}

	/* Put new element. Allocate and copy lname and path. */
	ds = dstack + dsidx++;
	ds->ds_did = dir->d_did;
	ds->ds_checked = 0;
	return 0;
}

/* Removes checked items from top of directory stack. Returns index of the first unchecked elements or -1. */
static int reducestack(void)
{
	int r;
	if (save_cidx != -1) {
		r = save_cidx;
		save_cidx = -1;
		return r;
	}

	while (dsidx > 0) {
		if (dstack[dsidx-1].ds_checked) {
			dsidx--;
//			free(dstack[dsidx].path);
		} else
			return dsidx - 1;
	} 
	return -1;
} 

/* Looks up for an opened adouble structure, opens resource fork of selected file. 
 * FIXME What about noadouble?
*/
static struct adouble *adl_lkup(struct vol *vol, struct path *path, struct adouble *adp)
{
	static struct adouble ad;
	
	struct ofork *of;
	int isdir;
	
	if (adp)
	    return adp;
	    
	isdir  = S_ISDIR(path->st.st_mode);

	if (!isdir && (of = of_findname(vol, path))) {
		adp = of->of_ad;
	} else {
		ad_init(&ad, vol);
		adp = &ad;
	} 

    if ( ad_metadata( path->u_name, ((isdir) ? ADFLAGS_DIR : 0), adp) < 0 ) {
        adp = NULL; /* FIXME without resource fork adl_lkup will be call again */
    }
    
	return adp;	
}

/* -------------------- */
static struct finderinfo *unpack_buffer(struct finderinfo *finfo, char *buffer)
{
	memcpy(&finfo->f_type,  buffer +FINDERINFO_FRTYPEOFF, sizeof(finfo->f_type));
	memcpy(&finfo->creator, buffer +FINDERINFO_FRCREATOFF, sizeof(finfo->creator));
	memcpy(&finfo->attrs,   buffer +FINDERINFO_FRFLAGOFF, sizeof(finfo->attrs));
	memcpy(&finfo->label,   buffer +FINDERINFO_FRFLAGOFF, sizeof(finfo->label));
	finfo->attrs &= 0xff00; /* high 8 bits */
	finfo->label &= 0xff;   /* low 8 bits */

	return finfo;
}

/* -------------------- */
static struct finderinfo *
unpack_finderinfo(struct vol *vol, struct path *path, struct adouble **adp, struct finderinfo *finfo, int islnk)
{
	packed_finder  buf;
	void           *ptr;
	
    *adp = adl_lkup(vol, path, *adp);
	ptr = get_finderinfo(vol, path->u_name, *adp, &buf,islnk);
	return unpack_buffer(finfo, ptr);
}

/* -------------------- */
#define CATPBIT_PARTIAL 31
/* Criteria checker. This function returns a 2-bit value. */
/* bit 0 means if checked file meets given criteria. */
/* bit 1 means if it is a directory and we should descent into it. */
/* uname - UNIX name 
 * fname - our fname (translated to UNIX)
 * cidx - index in directory stack
 */
static int crit_check(struct vol *vol, struct path *path) {
	int result = 0;
	uint16_t attr, flags = CONV_PRECOMPOSE;
	struct finderinfo *finfo = NULL, finderinfo;
	struct adouble *adp = NULL;
	time_t c_date, b_date;
	uint32_t ac_date, ab_date;
	static char convbuf[514]; /* for convert_charset dest_len parameter +2 */
	size_t len;
    int islnk;
    islnk=S_ISLNK(path->st.st_mode);

	if (S_ISDIR(path->st.st_mode)) {
		if (!c1.dbitmap)
			return 0;
	}
	else {
		if (!c1.fbitmap)
			return 0;

		/* compute the Mac name 
		 * first try without id (it's slow to find it)
		 * An other option would be to call get_id in utompath but 
		 * we need to pass parent dir
		*/
        if (!(path->m_name = utompath(vol, path->u_name, 0 , utf8_encoding(vol->v_obj)) )) {
        	/*retry with the right id */
       
        	cnid_t id;
        	
        	adp = adl_lkup(vol, path, adp);
        	id = get_id(vol, adp, &path->st, path->d_dir->d_did, path->u_name, strlen(path->u_name));
        	if (!id) {
        		/* FIXME */
        		return 0;
        	}
        	/* save the id for getfilparm */
        	path->id = id;
        	if (!(path->m_name = utompath(vol, path->u_name, id , utf8_encoding(vol->v_obj)))) {
        		return 0;
        	}
        }
	}
		
	/* Kind of optimization: 
	 * -- first check things we've already have - filename
	 * -- last check things we get from ad_open()
	 * FIXME strmcp strstr (icase)
	 */

	/* Check for filename */
	if ((c1.rbitmap & (1<<DIRPBIT_LNAME))) { 
		if ( (size_t)(-1) == (len = convert_string(vol->v_maccharset, CH_UCS2, path->m_name, -1, convbuf, 512)) )
			goto crit_check_ret;

		if ((c1.rbitmap & (1<<CATPBIT_PARTIAL))) {
			if (strcasestr_w( (ucs2_t*) convbuf, (ucs2_t*) c1.lname) == NULL)
				goto crit_check_ret;
		} else
			if (strcasecmp_w((ucs2_t*) convbuf, (ucs2_t*) c1.lname) != 0)
				goto crit_check_ret;
	} 
	
	if ((c1.rbitmap & (1<<FILPBIT_PDINFO))) { 
		if ( (size_t)(-1) == (len = convert_charset( CH_UTF8_MAC, CH_UCS2, CH_UTF8, path->m_name, strlen(path->m_name), convbuf, 512, &flags))) {
			goto crit_check_ret;
		}

		if (c1.rbitmap & (1<<CATPBIT_PARTIAL)) {
			if (strcasestr_w((ucs2_t *) convbuf, (ucs2_t*)c1.utf8name) == NULL)
				goto crit_check_ret;
		} else
			if (strcasecmp_w((ucs2_t *)convbuf, (ucs2_t*)c1.utf8name) != 0)
				goto crit_check_ret;
	} 


	/* FIXME */
	if ((unsigned)c2.mdate > 0x7fffffff)
		c2.mdate = 0x7fffffff;
	if ((unsigned)c2.cdate > 0x7fffffff)
		c2.cdate = 0x7fffffff;
	if ((unsigned)c2.bdate > 0x7fffffff)
		c2.bdate = 0x7fffffff;

	/* Check for modification date */
	if ((c1.rbitmap & (1<<DIRPBIT_MDATE))) {
		if (path->st.st_mtime < c1.mdate || path->st.st_mtime > c2.mdate)
			goto crit_check_ret;
	}
	
	/* Check for creation date... */
	if ((c1.rbitmap & (1<<DIRPBIT_CDATE))) {
		c_date = path->st.st_mtime;
		adp = adl_lkup(vol, path, adp);
		if (adp && ad_getdate(adp, AD_DATE_CREATE, &ac_date) >= 0)
		    c_date = AD_DATE_TO_UNIX(ac_date);

		if (c_date < c1.cdate || c_date > c2.cdate)
			goto crit_check_ret;
	}

	/* Check for backup date... */
	if ((c1.rbitmap & (1<<DIRPBIT_BDATE))) {
		b_date = path->st.st_mtime;
		adp = adl_lkup(vol, path, adp);
		if (adp && ad_getdate(adp, AD_DATE_BACKUP, &ab_date) >= 0)
			b_date = AD_DATE_TO_UNIX(ab_date);

		if (b_date < c1.bdate || b_date > c2.bdate)
			goto crit_check_ret;
	}
				
	/* Check attributes */
	if ((c1.rbitmap & (1<<DIRPBIT_ATTR)) && c2.attr != 0) {
		if ((adp = adl_lkup(vol, path, adp))) {
			ad_getattr(adp, &attr);
			if ((attr & c2.attr) != c1.attr)
				goto crit_check_ret;
		} else 
			goto crit_check_ret;
	}		

        /* Check file type ID */
	if ((c1.rbitmap & (1<<DIRPBIT_FINFO)) && c2.finfo.f_type != 0) {
	    finfo = unpack_finderinfo(vol, path, &adp, &finderinfo,islnk);
		if (finfo->f_type != c1.finfo.f_type)
			goto crit_check_ret;
	}
	
	/* Check creator ID */
	if ((c1.rbitmap & (1<<DIRPBIT_FINFO)) && c2.finfo.creator != 0) {
		if (!finfo) {
			finfo = unpack_finderinfo(vol, path, &adp, &finderinfo,islnk);
		}
		if (finfo->creator != c1.finfo.creator)
			goto crit_check_ret;
	}
		
	/* Check finder info attributes */
	if ((c1.rbitmap & (1<<DIRPBIT_FINFO)) && c2.finfo.attrs != 0) {
		if (!finfo) {
			finfo = unpack_finderinfo(vol, path, &adp, &finderinfo,islnk);
		}

		if ((finfo->attrs & c2.finfo.attrs) != c1.finfo.attrs)
			goto crit_check_ret;
	}
	
	/* Check label */
	if ((c1.rbitmap & (1<<DIRPBIT_FINFO)) && c2.finfo.label != 0) {
		if (!finfo) {
			finfo = unpack_finderinfo(vol, path, &adp, &finderinfo,islnk);
		}
		if ((finfo->label & c2.finfo.label) != c1.finfo.label)
			goto crit_check_ret;
	}	
	/* FIXME: Attributes check ! */
	
	/* All criteria are met. */
	result |= 1;
crit_check_ret:
	if (adp != NULL)
		ad_close(adp, ADFLAGS_HF);
	return result;
}  

/* ------------------------------ */
static int rslt_add (const AFPObj *obj, struct vol *vol, struct path *path, char **buf, int ext)
{

	char 		*p = *buf;
	int 		ret;
	size_t		tbuf =0;
	uint16_t	resultsize;
	int 		isdir = S_ISDIR(path->st.st_mode); 

	/* Skip resultsize */
	if (ext) {
		p += sizeof(resultsize); 
	}
	else {
		p++;
	}
	*p++ = isdir ? FILDIRBIT_ISDIR : FILDIRBIT_ISFILE;    /* IsDir ? */

	if (ext) {
		*p++ = 0;                  /* Pad */
	}
	
	if ( isdir ) {
        ret = getdirparams(obj, vol, c1.dbitmap, path, path->d_dir, p , &tbuf ); 
	}
	else {
	    /* FIXME slow if we need the file ID, we already know it, done ? */
		ret = getfilparams (obj, vol, c1.fbitmap, path, path->d_dir, p, &tbuf, 0);
	}

	if ( ret != AFP_OK )
		return 0;

	/* Make sure entry length is even */
	if ((tbuf & 1)) {
	   *p++ = 0;
	   tbuf++;
	}

	if (ext) {
		resultsize = htons(tbuf);
		memcpy ( *buf, &resultsize, sizeof(resultsize) );
		*buf += tbuf + 4;
	}
	else {
		**buf = tbuf;
		*buf += tbuf + 2;
	}

	return 1;
} 
	
#define VETO_STR \
        "./../.AppleDouble/.AppleDB/Network Trash Folder/TheVolumeSettingsFolder/TheFindByContentFolder/.AppleDesktop/.Parent/"

/*!
 * This function performs a filesystem search
 *
 * Uses globals c1, c2, the search criteria
 *
 * @param vol       (r)  volume we are searching on ...
 * @param dir       (rw) directory we are starting from ...
 * @param rmatches  (r)  maximum number of matches we can return
 * @param pos       (r)  position we've stopped recently
 * @param rbuf      (w)  output buffer
 * @param nrecs     (w)  number of matches
 * @param rsize     (w)  length of data written to output buffer
 * @param ext       (r)  extended search flag
 */
#define NUM_ROUNDS 200
static int catsearch(const AFPObj *obj,
                     struct vol *vol,
                     struct dir *dir,  
                     int rmatches,
                     uint32_t *pos,
                     char *rbuf,
                     uint32_t *nrecs,
                     int *rsize,
                     int ext)
{
    static uint32_t cur_pos;    /* Saved position index (ID) - used to remember "position" across FPCatSearch calls */
    static DIR *dirpos; 		 /* UNIX structure describing currently opened directory. */
    struct dir *currentdir;      /* struct dir of current directory */
	int cidx, r;
	struct dirent *entry;
	int result = AFP_OK;
	int ccr;
    struct path path;
	char *vpath = vol->v_path;
	char *rrbuf = rbuf;
    time_t start_time;
    int num_rounds = NUM_ROUNDS;
    int cwd = -1;
    int error;
    int unlen;

	if (*pos != 0 && *pos != cur_pos) {
		result = AFPERR_CATCHNG;
		goto catsearch_end;
	}

	/* FIXME: Category "offspring count ! */


	/* We need to initialize all mandatory structures/variables and change working directory appropriate... */
	if (*pos == 0) {
		clearstack();
		if (dirpos != NULL) {
			closedir(dirpos);
			dirpos = NULL;
		} 
		
		if (addstack(vpath, dir, -1) == -1) {
			result = AFPERR_MISC;
			goto catsearch_end;
		}
		/* FIXME: Sometimes DID is given by client ! (correct this one above !) */
	}

	/* Save current path */
    if ((cwd = open(".", O_RDONLY)) < 0) {
        result = AFPERR_MISC;
        goto catsearch_end;
    }
	
	/* So we are beginning... */
    start_time = time(NULL);

	while ((cidx = reducestack()) != -1) {
        if ((currentdir = dirlookup(vol, dstack[cidx].ds_did)) == NULL) {
            result = AFPERR_MISC;
            goto catsearch_end;
        }
        LOG(log_debug, logtype_afpd, "catsearch: current struct dir: \"%s\"", cfrombstr(currentdir->d_fullpath));

		error = movecwd(vol, currentdir);

		if (!error && dirpos == NULL)
			dirpos = opendir(".");

		if (dirpos == NULL)
			dirpos = opendir(cfrombstr(currentdir->d_fullpath));

		if (error || dirpos == NULL) {
			switch (errno) {
			case EACCES:
				dstack[cidx].ds_checked = 1;
				continue;
			case EMFILE:
			case ENFILE:
			case ENOENT:
				result = AFPERR_NFILE;
				break;
			case ENOMEM:
			case ENOTDIR:
			default:
				result = AFPERR_MISC;
			} /* switch (errno) */
			goto catsearch_end;
		}

		
		while ((entry = readdir(dirpos)) != NULL) {
			(*pos)++;

			if (!check_dirent(vol, entry->d_name))
			   continue;

            LOG(log_debug, logtype_afpd, "catsearch(\"%s\"): dirent: \"%s\"",
                cfrombstr(currentdir->d_fullpath), entry->d_name);

			memset(&path, 0, sizeof(path));
			path.u_name = entry->d_name;
			if (of_stat(vol, &path) != 0) {
				switch (errno) {
				case EACCES:
				case ELOOP:
				case ENOENT:
					continue;
				case ENOTDIR:
				case EFAULT:
				case ENOMEM:
				case ENAMETOOLONG:
				default:
					result = AFPERR_MISC;
					goto catsearch_end;
				} 
			}
            switch (S_IFMT & path.st.st_mode) {
            case S_IFDIR:
				/* here we can short cut 
				   ie if in the same loop the parent dir wasn't in the cache
				   ALL dirsearch_byname will fail.
				*/
                unlen = strlen(path.u_name);
                path.d_dir = dircache_search_by_name(vol,
                                                     currentdir,
                                                     path.u_name,
                                                     unlen);
            	if (path.d_dir == NULL) {
                	/* path.m_name is set by adddir */
            	    if ((path.d_dir = dir_add(vol,
                                              currentdir,
                                              &path,
                                              unlen)) == NULL) {
						result = AFPERR_MISC;
						goto catsearch_end;
					}
                }
                path.m_name = cfrombstr(path.d_dir->d_m_name);
                	
				if (addstack(path.u_name, path.d_dir, cidx) == -1) {
					result = AFPERR_MISC;
					goto catsearch_end;
				} 
                break;
            case S_IFREG:
            	path.d_dir = currentdir;
                break;
            default:
                continue;
            }

			ccr = crit_check(vol, &path);

			/* bit 0 means that criteria has been met */
			if ((ccr & 1)) {
				r = rslt_add (obj, vol, &path, &rrbuf, ext);
				
				if (r == 0) {
					result = AFPERR_MISC;
					goto catsearch_end;
				} 
				*nrecs += r;
				/* Number of matches limit */
				if (--rmatches == 0) 
					goto catsearch_pause;
				/* Block size limit */
				if (rrbuf - rbuf >= 448)
					goto catsearch_pause;
			}
			/* MacOS 9 doesn't like servers executing commands longer than few seconds */
			if (--num_rounds <= 0) {
			    if (start_time != time(NULL)) {
					result=AFP_OK;
					goto catsearch_pause;
			    }
			    num_rounds = NUM_ROUNDS;
			}
		} /* while ((entry=readdir(dirpos)) != NULL) */
		closedir(dirpos);
		dirpos = NULL;
		dstack[cidx].ds_checked = 1;
	} /* while (current_idx = reducestack()) != -1) */

	/* We have finished traversing our tree. Return EOF here. */
	result = AFPERR_EOF;
	goto catsearch_end;

catsearch_pause:
	cur_pos = *pos; 
	save_cidx = cidx;

catsearch_end: /* Exiting catsearch: error condition */
	*rsize = rrbuf - rbuf;
    if (cwd != -1) {
        if ((fchdir(cwd)) != 0) {
            LOG(log_debug, logtype_afpd, "error chdiring back: %s", strerror(errno));        
        }
        close(cwd);
    }
	return result;
} /* catsearch() */

/*!
 * This function performs a CNID db search
 *
 * Uses globals c1, c2, the search criteria
 *
 * @param vol       (r)  volume we are searching on ...
 * @param dir       (rw) directory we are starting from ...
 * @param uname     (r)  UNIX name of object to search
 * @param rmatches  (r)  maximum number of matches we can return
 * @param pos       (r)  position we've stopped recently
 * @param rbuf      (w)  output buffer
 * @param nrecs     (w)  number of matches
 * @param rsize     (w)  length of data written to output buffer
 * @param ext       (r)  extended search flag
 */
static int catsearch_db(const AFPObj *obj,
                        struct vol *vol,
                        struct dir *dir,  
                        const char *uname,
                        int rmatches,
                        uint32_t *pos,
                        char *rbuf,
                        uint32_t *nrecs,
                        int *rsize,
                        int ext)
{
    static char resbuf[DBD_MAX_SRCH_RSLTS * sizeof(cnid_t)];
    static uint32_t cur_pos;
    static int num_matches;
    int ccr ,r;
	int result = AFP_OK;
    struct path path;
	char *rrbuf = rbuf;
    char buffer[MAXPATHLEN +2];
    uint16_t flags = CONV_TOLOWER;

    LOG(log_debug, logtype_afpd, "catsearch_db(req pos: %u): {pos: %u, name: %s}",
        *pos, cur_pos, uname);
        
	if (*pos != 0 && *pos != cur_pos) {
		result = AFPERR_CATCHNG;
		goto catsearch_end;
	}

    if (cur_pos == 0 || *pos == 0) {
        if (convert_charset(vol->v_volcharset,
                            vol->v_volcharset,
                            vol->v_maccharset,
                            uname,
                            strlen(uname),
                            buffer,
                            MAXPATHLEN,
                            &flags) == (size_t)-1) {
            LOG(log_error, logtype_afpd, "catsearch_db: conversion error");
            result = AFPERR_MISC;
            goto catsearch_end;
        }

        LOG(log_debug, logtype_afpd, "catsearch_db: %s", buffer);

        AFP_CNID_START("cnid_find");
        num_matches = cnid_find(vol->v_cdb,
                                buffer,
                                strlen(uname),
                                resbuf,
                                sizeof(resbuf));
        AFP_CNID_DONE();
        if (num_matches == -1) {
            result = AFPERR_MISC;
            goto catsearch_end;
        }
    }
	
	while (cur_pos < num_matches) {
        char *name;
        cnid_t cnid, did;
        char resolvebuf[12 + MAXPATHLEN + 1];
        struct dir *dir;

        /* Next CNID to process from buffer */
        memcpy(&cnid, resbuf + cur_pos * sizeof(cnid_t), sizeof(cnid_t));
        did = cnid;

        AFP_CNID_START("cnid_resolve");
        name = cnid_resolve(vol->v_cdb, &did, resolvebuf, 12 + MAXPATHLEN + 1);
        AFP_CNID_DONE();
        if (name == NULL)
            goto next;

        LOG(log_debug, logtype_afpd, "catsearch_db: {pos: %u, name:%s, cnid: %u}",
            cur_pos, name, ntohl(cnid));
        if ((dir = dirlookup(vol, did)) == NULL)
            goto next;
        if (movecwd(vol, dir) < 0 )
            goto next;

        memset(&path, 0, sizeof(path));
        path.u_name = name;
        path.m_name = utompath(vol, name, cnid, utf8_encoding(vol->v_obj));

        if (of_stat(vol, &path) != 0) {
            switch (errno) {
            case EACCES:
            case ELOOP:
                goto next;
            case ENOENT:
                
            default:
                result = AFPERR_MISC;
                goto catsearch_end;
            } 
        }
        /* For files path.d_dir is the parent dir, for dirs its the dir itself */
        if (S_ISDIR(path.st.st_mode))
            if ((dir = dirlookup(vol, cnid)) == NULL)
                goto next;
        path.d_dir = dir;

        LOG(log_maxdebug, logtype_afpd,"catsearch_db: dir: %s, cwd: %s, name: %s", 
            cfrombstr(dir->d_fullpath), getcwdpath(), path.u_name);

        /* At last we can check the search criteria */
        ccr = crit_check(vol, &path);
        if ((ccr & 1)) {
            LOG(log_debug, logtype_afpd,"catsearch_db: match: %s/%s",
                getcwdpath(), path.u_name);
            /* bit 1 means that criteria has been met */
            r = rslt_add(obj, vol, &path, &rrbuf, ext);
            if (r == 0) {
                result = AFPERR_MISC;
                goto catsearch_end;
            } 
            *nrecs += r;
            /* Number of matches limit */
            if (--rmatches == 0) 
                goto catsearch_pause;
            /* Block size limit */
            if (rrbuf - rbuf >= 448)
                goto catsearch_pause;
        }
    next:
        cur_pos++;
    } /* while */

	/* finished */
	result = AFPERR_EOF;
    cur_pos = 0;
	goto catsearch_end;

catsearch_pause:
    *pos = cur_pos;

catsearch_end: /* Exiting catsearch: error condition */
	*rsize = rrbuf - rbuf;
    LOG(log_debug, logtype_afpd, "catsearch_db(req pos: %u): {pos: %u}", *pos, cur_pos);
	return result;
}

/* -------------------------- */
static int catsearch_afp(AFPObj *obj _U_, char *ibuf, size_t ibuflen,
                  char *rbuf, size_t *rbuflen, int ext)
{
    struct vol *vol;
    uint16_t   vid;
    uint16_t   spec_len;
    uint32_t   rmatches, reserved;
    uint32_t	catpos[4];
    uint32_t   pdid = 0;
    int ret, rsize;
    uint32_t nrecs = 0;
    unsigned char *spec1, *spec2, *bspec1, *bspec2;
    size_t	len;
    uint16_t	namelen;
    uint16_t	flags;
    char  	    tmppath[256];
    char        *uname;

    *rbuflen = 0;

    /* min header size */
    if (ibuflen < 32) {
        return AFPERR_PARAM;
    }

    memset(&c1, 0, sizeof(c1));
    memset(&c2, 0, sizeof(c2));

    ibuf += 2;
    memcpy(&vid, ibuf, sizeof(vid));
    ibuf += sizeof(vid);

    if ((vol = getvolbyvid(vid)) == NULL) {
        return AFPERR_PARAM;
    }
    
    memcpy(&rmatches, ibuf, sizeof(rmatches));
    rmatches = ntohl(rmatches);
    ibuf += sizeof(rmatches); 

    /* FIXME: (rl) should we check if reserved == 0 ? */
    ibuf += sizeof(reserved);

    memcpy(catpos, ibuf, sizeof(catpos));
    ibuf += sizeof(catpos);

    memcpy(&c1.fbitmap, ibuf, sizeof(c1.fbitmap));
    c1.fbitmap = c2.fbitmap = ntohs(c1.fbitmap);
    ibuf += sizeof(c1.fbitmap);

    memcpy(&c1.dbitmap, ibuf, sizeof(c1.dbitmap));
    c1.dbitmap = c2.dbitmap = ntohs(c1.dbitmap);
    ibuf += sizeof(c1.dbitmap);

    memcpy(&c1.rbitmap, ibuf, sizeof(c1.rbitmap));
    c1.rbitmap = c2.rbitmap = ntohl(c1.rbitmap);
    ibuf += sizeof(c1.rbitmap);

    if (! (c1.fbitmap || c1.dbitmap)) {
	    return AFPERR_BITMAP;
    }

    if ( ext) {
        memcpy(&spec_len, ibuf, sizeof(spec_len));
        spec_len = ntohs(spec_len);
    }
    else {
        /* with catsearch only name and parent id are allowed */
    	c1.fbitmap &= (1<<FILPBIT_LNAME) | (1<<FILPBIT_PDID);
    	c1.dbitmap &= (1<<DIRPBIT_LNAME) | (1<<DIRPBIT_PDID);
        spec_len = *(unsigned char*)ibuf;
    }

    /* Parse file specifications */
    spec1 = (unsigned char*)ibuf;
    spec2 = (unsigned char*)ibuf + spec_len + 2;

    spec1 += 2; 
    spec2 += 2; 

    bspec1 = spec1;
    bspec2 = spec2;
    /* File attribute bits... */
    if (c1.rbitmap & (1 << FILPBIT_ATTR)) {
	    memcpy(&c1.attr, spec1, sizeof(c1.attr));
	    spec1 += sizeof(c1.attr);
	    memcpy(&c2.attr, spec2, sizeof(c2.attr));
	    spec2 += sizeof(c1.attr);
    }

    /* Parent DID */
    if (c1.rbitmap & (1 << FILPBIT_PDID)) {
            memcpy(&c1.pdid, spec1, sizeof(pdid));
	    spec1 += sizeof(c1.pdid);
	    memcpy(&c2.pdid, spec2, sizeof(pdid));
	    spec2 += sizeof(c2.pdid);
    } /* FIXME: PDID - do we demarshall this argument ? */

    /* Creation date */
    if (c1.rbitmap & (1 << FILPBIT_CDATE)) {
	    memcpy(&c1.cdate, spec1, sizeof(c1.cdate));
	    spec1 += sizeof(c1.cdate);
	    c1.cdate = AD_DATE_TO_UNIX(c1.cdate);
	    memcpy(&c2.cdate, spec2, sizeof(c2.cdate));
	    spec2 += sizeof(c1.cdate);
	    ibuf += sizeof(c1.cdate);;
	    c2.cdate = AD_DATE_TO_UNIX(c2.cdate);
    }

    /* Modification date */
    if (c1.rbitmap & (1 << FILPBIT_MDATE)) {
	    memcpy(&c1.mdate, spec1, sizeof(c1.mdate));
	    c1.mdate = AD_DATE_TO_UNIX(c1.mdate);
	    spec1 += sizeof(c1.mdate);
	    memcpy(&c2.mdate, spec2, sizeof(c2.mdate));
	    c2.mdate = AD_DATE_TO_UNIX(c2.mdate);
	    spec2 += sizeof(c1.mdate);
    }
    
    /* Backup date */
    if (c1.rbitmap & (1 << FILPBIT_BDATE)) {
	    memcpy(&c1.bdate, spec1, sizeof(c1.bdate));
	    spec1 += sizeof(c1.bdate);
	    c1.bdate = AD_DATE_TO_UNIX(c1.bdate);
	    memcpy(&c2.bdate, spec2, sizeof(c2.bdate));
	    spec2 += sizeof(c2.bdate);
	    c1.bdate = AD_DATE_TO_UNIX(c2.bdate);
    }

    /* Finder info */
    if (c1.rbitmap & (1 << FILPBIT_FINFO)) {
    	packed_finder buf;
    	
	    memcpy(buf, spec1, sizeof(buf));
	    unpack_buffer(&c1.finfo, buf);    	
	    spec1 += sizeof(buf);

	    memcpy(buf, spec2, sizeof(buf));
	    unpack_buffer(&c2.finfo, buf);
	    spec2 += sizeof(buf);
    } /* Finder info */

    if ((c1.rbitmap & (1 << DIRPBIT_OFFCNT)) != 0) {
        /* Offspring count - only directories */
		if (c1.fbitmap == 0) {
	    	memcpy(&c1.offcnt, spec1, sizeof(c1.offcnt));
	    	spec1 += sizeof(c1.offcnt);
	    	c1.offcnt = ntohs(c1.offcnt);
	    	memcpy(&c2.offcnt, spec2, sizeof(c2.offcnt));
	    	spec2 += sizeof(c2.offcnt);
	    	c2.offcnt = ntohs(c2.offcnt);
		}
		else if (c1.dbitmap == 0) {
			/* ressource fork length */
		}
		else {
	    	return AFPERR_BITMAP;  /* error */
		}
    } /* Offspring count/ressource fork length */

    /* Long name */
    if (c1.rbitmap & (1 << FILPBIT_LNAME)) {
        /* Get the long filename */	
		memcpy(tmppath, bspec1 + spec1[1] + 1, (bspec1 + spec1[1])[0]);
		tmppath[(bspec1 + spec1[1])[0]]= 0;
		len = convert_string ( vol->v_maccharset, CH_UCS2, tmppath, -1, c1.lname, sizeof(c1.lname));
        if (len == (size_t)(-1))
            return AFPERR_PARAM;

#if 0	
		/* FIXME: do we need it ? It's always null ! */
		memcpy(c2.lname, bspec2 + spec2[1] + 1, (bspec2 + spec2[1])[0]);
		c2.lname[(bspec2 + spec2[1])[0]]= 0;
#endif
    }
        /* UTF8 Name */
    if (c1.rbitmap & (1 << FILPBIT_PDINFO)) {

		/* offset */
		memcpy(&namelen, spec1, sizeof(namelen));
		namelen = ntohs (namelen);

		spec1 = bspec1+namelen+4; /* Skip Unicode Hint */

		/* length */
		memcpy(&namelen, spec1, sizeof(namelen));
		namelen = ntohs (namelen);
		if (namelen > UTF8FILELEN_EARLY)  /* Safeguard */
			namelen = UTF8FILELEN_EARLY;

		memcpy (c1.utf8name, spec1+2, namelen);
		c1.utf8name[namelen] = 0;
        if ((uname = mtoupath(vol, c1.utf8name, 0, utf8_encoding(obj))) == NULL)
            return AFPERR_PARAM;

 		/* convert charset */
		flags = CONV_PRECOMPOSE;
 		len = convert_charset(CH_UTF8_MAC, CH_UCS2, CH_UTF8, c1.utf8name, namelen, c1.utf8name, 512, &flags);
        if (len == (size_t)(-1))
            return AFPERR_PARAM;
    }
    
    /* Call search */
    *rbuflen = 24;
    if ((c1.rbitmap & (1 << FILPBIT_PDINFO))
        && !(c1.rbitmap & (1<<CATPBIT_PARTIAL))
        && (strcmp(vol->v_cnidscheme, "dbd") == 0)
        && (vol->v_flags & AFPVOL_SEARCHDB))
        /* we've got a name and it's a dbd volume, so search CNID database */
        ret = catsearch_db(obj, vol, vol->v_root, uname, rmatches, &catpos[0], rbuf+24, &nrecs, &rsize, ext);
    else
        /* perform a slow filesystem tree search */
        ret = catsearch(obj, vol, vol->v_root, rmatches, &catpos[0], rbuf+24, &nrecs, &rsize, ext);

    memcpy(rbuf, catpos, sizeof(catpos));
    rbuf += sizeof(catpos);

    c1.fbitmap = htons(c1.fbitmap);
    memcpy(rbuf, &c1.fbitmap, sizeof(c1.fbitmap));
    rbuf += sizeof(c1.fbitmap);

    c1.dbitmap = htons(c1.dbitmap);
    memcpy(rbuf, &c1.dbitmap, sizeof(c1.dbitmap));
    rbuf += sizeof(c1.dbitmap);

    nrecs = htonl(nrecs);
    memcpy(rbuf, &nrecs, sizeof(nrecs));
    rbuf += sizeof(nrecs);
    *rbuflen += rsize;

    return ret;
} /* catsearch_afp */

/* -------------------------- */
int afp_catsearch (AFPObj *obj, char *ibuf, size_t ibuflen,
                  char *rbuf, size_t *rbuflen)
{
	return catsearch_afp( obj, ibuf, ibuflen, rbuf, rbuflen, 0);
}


int afp_catsearch_ext (AFPObj *obj, char *ibuf, size_t ibuflen,
                  char *rbuf, size_t *rbuflen)
{
	return catsearch_afp( obj, ibuf, ibuflen, rbuf, rbuflen, 1);
}

/* FIXME: we need a clean separation between afp stubs and 'real' implementation */
/* (so, all buffer packing/unpacking should be done in stub, everything else 
   should be done in other functions) */
