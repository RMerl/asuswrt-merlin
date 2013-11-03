/*
 * ad_mmap provides interfaces to memory mapped files. as this is the
 * case, we don't have to deal w/ temporary buffers such as
 * ad_data. the ad_mmap routines are designed to not interact w/ the
 * ad_read/ad_write routines to avoid confusion.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef USE_MMAPPED_HEADERS
#include <stdio.h>

#include <atalk/adouble.h>
#include <string.h>

#include "ad_lock.h"

static void *ad_mmap(const size_t length, const int prot,
				const int flags, const int fd, 
				const off_t offset)
{
  return mmap(0, length, prot, flags, fd, offset);
}

/* this just sets things up for mmap. as mmap can handle offsets,
 * we need to reset the file position before handing it off */
void *ad_mmapread(struct adouble *ad, const u_int32_t eid, 
		  const off_t off, const size_t buflen)
{
    /* data fork */
    if ( eid == ADEID_DFORK ) {
      if ( lseek( ad->ad_df.adf_fd, 0, SEEK_SET ) < 0 ) {
	perror( "df lseek" );
	return (void *) -1;
      }
      ad->ad_df.adf_off = 0;
      return ad_mmap(buflen, PROT_READ | PROT_WRITE, MAP_PRIVATE, 
		     ad->ad_df.adf_fd, off);

    }

    /* resource fork */
    if ( lseek( ad->ad_hf.adf_fd, 0, SEEK_SET ) < 0 ) {
      perror( "hf lseek" );
      return (void *) -1;
    }
    ad->ad_hf.adf_off = 0;
    return ad_mmap(buflen, PROT_READ | PROT_WRITE, MAP_PRIVATE, 
		   ad->ad_hf.adf_fd, ad->ad_eid[eid].ade_off + off);
}


/* to do writeable mmaps correctly, we actually need to make sure that
 * the file to be mapped is large enough. that's what all the initial
 * mess is for. */
void *ad_mmapwrite(struct adouble *ad, const u_int32_t eid,
		   off_t off, const int end, const size_t buflen)
{
    struct stat st;

    /* data fork */
    if ( eid == ADEID_DFORK ) {
        if ( fstat( ad->ad_df.adf_fd, &st ) < 0 ) {
	    return (void *) -1;
        }

	if ( end ) {
	    off = st.st_size - off;
	}

	/* make sure the file is large enough */
	if (st.st_size < buflen + off) 
	  ftruncate(ad->ad_df.adf_fd, buflen + off);

	if ( lseek( ad->ad_df.adf_fd, 0, SEEK_SET ) < 0 ) {
	  return (void *) -1;
	}
	ad->ad_df.adf_off = 0;
	return ad_mmap(buflen, PROT_READ | PROT_WRITE, MAP_SHARED,
		       ad->ad_df.adf_fd, off);
    }

    
    if ( fstat( ad->ad_hf.adf_fd, &st ) < 0 ) {
        return (void *) -1;
    }
    
    if ( end ) {
	off = ad->ad_eid[ eid ].ade_len - off;
    }
    
    off += ad->ad_eid[eid].ade_off;

    /* make sure the file is large enough */
    if (st.st_size < buflen + off) 
      ftruncate(ad->ad_hf.adf_fd, buflen + off);

    if ( lseek( ad->ad_hf.adf_fd, 0, SEEK_SET ) < 0 ) {
      return (void *) -1;
    }
    ad->ad_hf.adf_off = 0;
    return ad_mmap(buflen, PROT_READ | PROT_WRITE, MAP_SHARED,
		   ad->ad_hf.adf_fd, off);
}

#endif
