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
 * banner.c
 *
 * Calls exposed to handle the junk a typical FTP server has to do upon
 * entering a new directory (messages, etc), as well as general banner
 * writing support.
 */

#include "banner.h"
#include "strlist.h"
#include "str.h"
#include "sysstr.h"
#include "tunables.h"
#include "ftpcmdio.h"
#include "filestr.h"
#include "session.h"
#include "sysutil.h"

/* Definitions */
#define VSFTP_MAX_VISIT_REMEMBER 100
#define VSFTP_MAX_MSGFILE_SIZE 4000

void
vsf_banner_dir_changed(struct vsf_session* p_sess, int ftpcode)
{
  struct mystr dir_str = INIT_MYSTR;
  /* Do nothing if .message support is off */
  if (!tunable_dirmessage_enable)
  {
    return;
  }
  if (p_sess->p_visited_dir_list == 0)
  {
    struct mystr_list the_list = INIT_STRLIST;
    p_sess->p_visited_dir_list = vsf_sysutil_malloc(sizeof(struct mystr_list));
    *p_sess->p_visited_dir_list = the_list;
  }
  str_getcwd(&dir_str);
  /* Do nothing if we already visited this directory */
  if (!str_list_contains_str(p_sess->p_visited_dir_list, &dir_str))
  {
    /* Just in case, cap the max. no of visited directories we'll remember */
    if (str_list_get_length(p_sess->p_visited_dir_list) <
        VSFTP_MAX_VISIT_REMEMBER)
    {
      str_list_add(p_sess->p_visited_dir_list, &dir_str, 0);
    }
    /* If we have a .message file, squirt it out prepended by the ftpcode and
     * the continuation mark '-'
     */
    {
      struct mystr msg_file_str = INIT_MYSTR;
      (void) str_fileread(&msg_file_str, tunable_message_file,
                          VSFTP_MAX_MSGFILE_SIZE);
      vsf_banner_write(p_sess, &msg_file_str, ftpcode);
      str_free(&msg_file_str);
    }
  }
  str_free(&dir_str);
}

void
vsf_banner_write(struct vsf_session* p_sess, struct mystr* p_str, int ftpcode)
{
  struct mystr msg_line_str = INIT_MYSTR;
  unsigned int str_pos = 0;
  while (str_getline(p_str, &msg_line_str, &str_pos))
  {
    vsf_cmdio_write_str_hyphen(p_sess, ftpcode, &msg_line_str);
  }
  str_free(&msg_line_str);
}

