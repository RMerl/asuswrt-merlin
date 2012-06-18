/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * ls.c
 *
 * Would you believe, code to handle directory listing.
 */

#include "sysstr.h"
#include "ls.h"
#include "access.h"
#include "str.h"
#include "strlist.h"
#include "sysutil.h"
#include "tunables.h"
#include "utility.h"	// Jiahao
#include <stdlib.h>
#include <string.h>
#include <disk_io_tools.h>

static void build_dir_line(struct mystr* p_str,
                           const struct mystr* p_filename_str,
                           const struct vsf_sysutil_statbuf* p_stat);

void
vsf_ls_populate_dir_list(const char* session_user,	// James
                         struct mystr_list* p_list,
                         struct mystr_list* p_subdir_list,
                         struct vsf_sysutil_dir* p_dir,
                         const struct mystr* p_base_dir_str,
                         const struct mystr* p_option_str,
                         const struct mystr* p_filter_str,
                         int is_verbose)
{
  struct mystr dirline_str = INIT_MYSTR;
  struct mystr normalised_base_dir_str = INIT_MYSTR;
  struct str_locate_result loc_result;
  int a_option;
  int r_option;
  int t_option;
  int F_option;
  int do_stat = 0;
  loc_result = str_locate_char(p_option_str, 'a');
  a_option = loc_result.found;
  loc_result = str_locate_char(p_option_str, 'r');
  r_option = loc_result.found;
  loc_result = str_locate_char(p_option_str, 't');
  t_option = loc_result.found;
  loc_result = str_locate_char(p_option_str, 'F');
  F_option = loc_result.found;
  loc_result = str_locate_char(p_option_str, 'l');
  if (loc_result.found)
  {
    is_verbose = 1;
  }
  /* Invert "reverse" arg for "-t", the time sorting */
  if (t_option)
  {
    r_option = !r_option;
  }
  if (is_verbose || t_option || F_option || p_subdir_list != 0)
  {
    do_stat = 1;
  }
  /* If the filter starts with a . then implicitly enable -a */
  if (!str_isempty(p_filter_str) && str_get_char_at(p_filter_str, 0) == '.')
  {
    a_option = 1;
  }
  /* "Normalise" the incoming base directory string by making sure it
   * ends in a '/' if it is nonempty
   */
  if (!str_equal_text(p_base_dir_str, "."))
  {
    str_copy(&normalised_base_dir_str, p_base_dir_str);
  }
  if (!str_isempty(&normalised_base_dir_str))
  {
    unsigned int len = str_getlen(&normalised_base_dir_str);
    if (str_get_char_at(&normalised_base_dir_str, len - 1) != '/')
    {
      str_append_char(&normalised_base_dir_str, '/');
    }
  }
  /* If we're going to need to do time comparisions, cache the local time */
  if (is_verbose)
  {
    vsf_sysutil_update_cached_time();
  }
	
	while (1){
		int len;
		static struct mystr s_next_filename_str;
		static struct mystr s_next_path_and_filename_str;
		static struct vsf_sysutil_statbuf* s_p_statbuf;
// 2007.05 James {
		str_next_dirent(session_user, str_getbuf(p_base_dir_str), &s_next_filename_str, p_dir);
		if(!strcmp(str_getbuf(&s_next_filename_str), DENIED_DIR))
			continue;
// 2007.05 James }

		if (str_isempty(&s_next_filename_str)){
			break;
		}
		len = str_getlen(&s_next_filename_str);
		if (len > 0 && str_get_char_at(&s_next_filename_str, 0) == '.'){
			if (!a_option && !tunable_force_dot_files){
				continue;
			}
			if (!a_option &&
					((len == 2 && str_get_char_at(&s_next_filename_str, 1) == '.') ||
					len == 1)){
				continue;
			}
		}

		/* Don't show hidden directory entries */
		if (!vsf_access_check_file_visible(&s_next_filename_str)){
			continue;
		}
#if 0
		/* If we have an ls option which is a filter, apply it */
		if (!str_isempty(p_filter_str)){
			if (!vsf_filename_passes_filter(&s_next_filename_str, p_filter_str)){
				continue;
			}
		}
#endif
		/* Calculate the full path (relative to CWD) for lstat() and
		 * output purposes
		 */
		str_copy(&s_next_path_and_filename_str, &normalised_base_dir_str);
		str_append_str(&s_next_path_and_filename_str, &s_next_filename_str);
		if (do_stat){
			/* lstat() the file. Of course there's a race condition - the
			 * directory entry may have gone away whilst we read it, so
		 	* ignore failure to stat
		 	*/
			int retval = str_lstat(&s_next_path_and_filename_str, &s_p_statbuf);
			if (vsf_sysutil_retval_is_error(retval)){
				continue;
			}
		}
		
		if (is_verbose){
			static struct mystr s_final_file_str;
			/* If it's a damn symlink, we need to append the target */
			str_copy(&s_final_file_str, &s_next_filename_str);
			if (vsf_sysutil_statbuf_is_symlink(s_p_statbuf)){
				static struct mystr s_temp_str;
				int retval = str_readlink(&s_temp_str, &s_next_path_and_filename_str);
				if (retval == 0 && !str_isempty(&s_temp_str)){
					str_append_text(&s_final_file_str, " -> ");
					str_append_str(&s_final_file_str, &s_temp_str);
				}
			}
			if (F_option && vsf_sysutil_statbuf_is_dir(s_p_statbuf)){
				str_append_char(&s_final_file_str, '/');
			}
			
			build_dir_line(&dirline_str, &s_final_file_str, s_p_statbuf);
		}
		else{
			/* Just emit the filenames - note, we prepend the directory for NLST
			 * but not for LIST
			 */
			str_copy(&dirline_str, &s_next_path_and_filename_str);
			if (F_option){
				if (vsf_sysutil_statbuf_is_dir(s_p_statbuf)){
					str_append_char(&dirline_str, '/');
				}
				else if (vsf_sysutil_statbuf_is_symlink(s_p_statbuf)){
					str_append_char(&dirline_str, '@');
				}
			}
			str_append_text(&dirline_str, "\r\n");
			
			char *ptr = strstr(str_getbuf(&dirline_str), POOL_MOUNT_ROOT);
			if(ptr != NULL)
				str_alloc_text(&dirline_str, ptr+strlen(POOL_MOUNT_ROOT));
		}
		
		/* Add filename into our sorted list - sorting by filename or time. Also,
		 * if we are required to, maintain a distinct list of direct
		 * subdirectories.
		 */
		static struct mystr s_temp_str;
		const struct mystr* p_sort_str = 0;
		const struct mystr* p_sort_subdir_str = 0;
		if (!t_option){
			p_sort_str = &s_next_filename_str;
		}
		else{
			str_alloc_text(&s_temp_str, vsf_sysutil_statbuf_get_sortkey_mtime(s_p_statbuf));
			p_sort_str = &s_temp_str;
			p_sort_subdir_str = &s_temp_str;
		}
		
		str_list_add(p_list, &dirline_str, p_sort_str);
		if (p_subdir_list != 0 && vsf_sysutil_statbuf_is_dir(s_p_statbuf)){
			str_list_add(p_subdir_list, &s_next_filename_str, p_sort_subdir_str);
		}
	} /* END: while(1) */
	
	str_list_sort(p_list, r_option);
	if (p_subdir_list != 0){
		str_list_sort(p_subdir_list, r_option);
	}
	
	str_free(&dirline_str);
	str_free(&normalised_base_dir_str);
}

int
vsf_filename_passes_filter(const struct mystr* p_filename_str,
                           const struct mystr* p_filter_str)
{
  /* A simple routine to match a filename against a pattern.
   * This routine is used instead of e.g. fnmatch(3), because we should be
   * reluctant to trust the latter. fnmatch(3) involves _lots_ of string
   * parsing and handling. There is broad potential for any given fnmatch(3)
   * implementation to be buggy.
   *
   * Currently supported pattern(s):
   * - any number of wildcards, "*" or "?"
   * - {,} syntax (not nested)
   *
   * Note that pattern matching is only supported within the last path
   * component. For example, searching for /a/b/? will work, but searching
   * for /a/?/c will not.
   */
  struct mystr filter_remain_str = INIT_MYSTR;
  struct mystr name_remain_str = INIT_MYSTR;
  struct mystr temp_str = INIT_MYSTR;
  struct mystr brace_list_str = INIT_MYSTR;
  struct mystr new_filter_str = INIT_MYSTR;
  int ret = 0;
  char last_token = 0;
  int must_match_at_current_pos = 1;
  str_copy(&filter_remain_str, p_filter_str);
  str_copy(&name_remain_str, p_filename_str);

  while (!str_isempty(&filter_remain_str))
  {
    static struct mystr s_match_needed_str;
    /* Locate next special token */
    struct str_locate_result locate_result =
      str_locate_chars(&filter_remain_str, "*?{");
    /* Isolate text leading up to token (if any) - needs to be matched */
    if (locate_result.found)
    {
      unsigned int indexx = locate_result.index;
      str_left(&filter_remain_str, &s_match_needed_str, indexx);
      str_mid_to_end(&filter_remain_str, &temp_str, indexx + 1);
      str_copy(&filter_remain_str, &temp_str);
      last_token = locate_result.char_found;
    }
    else
    {
      /* No more tokens. Must match remaining filter string exactly. */
      str_copy(&s_match_needed_str, &filter_remain_str);
      str_empty(&filter_remain_str);
      last_token = 0;
    }
    if (!str_isempty(&s_match_needed_str))
    {
      /* Need to match something.. could be a match which has to start at
       * current position, or we could allow it to start anywhere
       */
      unsigned int indexx;
      locate_result = str_locate_str(&name_remain_str, &s_match_needed_str);
      if (!locate_result.found)
      {
        /* Fail */
        goto out;
      }
      indexx = locate_result.index;
      if (must_match_at_current_pos && indexx > 0)
      {
        goto out;
      }
      /* Chop matched string out of remainder */
      str_mid_to_end(&name_remain_str, &temp_str,
                     indexx + str_getlen(&s_match_needed_str));
      str_copy(&name_remain_str, &temp_str);
    }
    if (last_token == '?')
    {
      if (str_isempty(&name_remain_str))
      {
        goto out;
      }
      str_right(&name_remain_str, &temp_str, str_getlen(&name_remain_str) - 1);
      str_copy(&name_remain_str, &temp_str);
      must_match_at_current_pos = 1;
    }
    else if (last_token == '{')
    {
      struct str_locate_result end_brace =
        str_locate_char(&filter_remain_str, '}');
      must_match_at_current_pos = 1;
      if (end_brace.found)
      {
        str_split_char(&filter_remain_str, &temp_str, '}');
        str_copy(&brace_list_str, &filter_remain_str);
        str_copy(&filter_remain_str, &temp_str);
        str_split_char(&brace_list_str, &temp_str, ',');
        while (!str_isempty(&brace_list_str))
        {
          str_copy(&new_filter_str, &brace_list_str);
          str_append_str(&new_filter_str, &filter_remain_str);
          if (vsf_filename_passes_filter(&name_remain_str, &new_filter_str))
          {
            ret = 1;
            goto out;
          }
          str_copy(&brace_list_str, &temp_str);
          str_split_char(&brace_list_str, &temp_str, ',');
        }
        goto out;
      }
      else if (str_isempty(&name_remain_str) ||
               str_get_char_at(&name_remain_str, 0) != '{')
      {
        goto out;
      }
      else
      {
        str_right(&name_remain_str, &temp_str,
                  str_getlen(&name_remain_str) - 1);
        str_copy(&name_remain_str, &temp_str);
      }
    }
    else
    {
      must_match_at_current_pos = 0;
    }
  }
  /* Any incoming string left means no match unless we ended on the correct
   * type of wildcard.
   */
  if (str_getlen(&name_remain_str) > 0 && last_token != '*')
  {
    goto out;
  }
  /* OK, a match */
  ret = 1;
out:
  str_free(&filter_remain_str);
  str_free(&name_remain_str);
  str_free(&temp_str);
  str_free(&brace_list_str);
  str_free(&new_filter_str);
  return ret;
}

static void
build_dir_line(struct mystr* p_str, const struct mystr* p_filename_str,
               const struct vsf_sysutil_statbuf* p_stat)
{
  static struct mystr s_tmp_str;
  char *tmp_filename=NULL;	// Jiahao
  filesize_t size = vsf_sysutil_statbuf_get_size(p_stat);
  /* Permissions */
  str_alloc_text(p_str, vsf_sysutil_statbuf_get_perms(p_stat));
  str_append_char(p_str, ' ');
  /* Hard link count */
  str_alloc_ulong(&s_tmp_str, vsf_sysutil_statbuf_get_links(p_stat));
  str_lpad(&s_tmp_str, 4);
  str_append_str(p_str, &s_tmp_str);
  str_append_char(p_str, ' ');
  /* User */
  if (tunable_hide_ids)
  {
    str_alloc_text(&s_tmp_str, "ftp");
  }
  else
  {
    int uid = vsf_sysutil_statbuf_get_uid(p_stat);
    struct vsf_sysutil_user* p_user = 0;
    if (tunable_text_userdb_names)
    {
      p_user = vsf_sysutil_getpwuid(uid);
    }
    if (p_user == 0)
    {
      str_alloc_ulong(&s_tmp_str, (unsigned long) uid);
    }
    else
    {
      str_alloc_text(&s_tmp_str, vsf_sysutil_user_getname(p_user));
    }
  }
  str_rpad(&s_tmp_str, 8);
  str_append_str(p_str, &s_tmp_str);
  str_append_char(p_str, ' ');
  /* Group */
  if (tunable_hide_ids)
  {
    str_alloc_text(&s_tmp_str, "ftp");
  }
  else
  {
    int gid = vsf_sysutil_statbuf_get_gid(p_stat);
    struct vsf_sysutil_group* p_group = 0;
    if (tunable_text_userdb_names)
    {
      p_group = vsf_sysutil_getgrgid(gid);
    }
    if (p_group == 0)
    {
      str_alloc_ulong(&s_tmp_str, (unsigned long) gid);
    }
    else
    {
      str_alloc_text(&s_tmp_str, vsf_sysutil_group_getname(p_group));
    }
  }
  str_rpad(&s_tmp_str, 8);
  str_append_str(p_str, &s_tmp_str);
  str_append_char(p_str, ' ');
  /* Size in bytes */
  str_alloc_filesize_t(&s_tmp_str, size);
  str_lpad(&s_tmp_str, 8);
  str_append_str(p_str, &s_tmp_str);
  str_append_char(p_str, ' ');
  /* Date stamp */
  str_append_text(p_str, vsf_sysutil_statbuf_get_date(p_stat,
                                                      tunable_use_localtime));
  str_append_char(p_str, ' ');
  /* Filename */
//  str_append_str(p_str, p_filename_str);	// Jiahao
	//wlog("handle ls");
  tmp_filename = local2remote(str_getbuf(p_filename_str));
  if (tmp_filename == NULL)
    str_append_str(p_str, p_filename_str);
  else {
    str_append_text(p_str, tmp_filename);
    vsf_sysutil_free(tmp_filename);
  }
  str_append_text(p_str, "\r\n");
}

