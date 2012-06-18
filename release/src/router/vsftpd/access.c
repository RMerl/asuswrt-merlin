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
 * access.c
 *
 * Routines to do very very simple access control based on filenames.
 */

#include "access.h"
#include "ls.h"
#include "tunables.h"
#include "str.h"
#include "sysutil.h"

int
vsf_access_check_file(const struct mystr* p_filename_str)
{
  static struct mystr s_access_str;

  if (!tunable_deny_file)
  {
    return 1;
  }
  if (str_isempty(&s_access_str))
  {
    str_alloc_text(&s_access_str, tunable_deny_file);
  }
  if (vsf_filename_passes_filter(p_filename_str, &s_access_str))
  {
    return 0;
  }
  else
  {
    struct str_locate_result loc_res =
      str_locate_str(p_filename_str, &s_access_str);
    if (loc_res.found)
    {
      return 0;
    }
  }
  return 1;
}

int
vsf_access_check_file_visible(const struct mystr* p_filename_str)
{
  static struct mystr s_access_str;

  if (!tunable_hide_file)
  {
    return 1;
  }
  if (str_isempty(&s_access_str))
  {
    str_alloc_text(&s_access_str, tunable_hide_file);
  }
  if (vsf_filename_passes_filter(p_filename_str, &s_access_str))
  {
    return 0;
  }
  else
  {
    struct str_locate_result loc_res =
      str_locate_str(p_filename_str, &s_access_str);
    if (loc_res.found)
    {
      return 0;
    }
  }
  return 1;
}
