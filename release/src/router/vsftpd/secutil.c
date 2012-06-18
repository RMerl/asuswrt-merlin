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
 * secutil.c
 */

#include "secutil.h"
#include "str.h"
#include "sysutil.h"
#include "sysstr.h"
#include "utility.h"
#include "sysdeputil.h"

void
vsf_secutil_change_credentials(const struct mystr* p_user_str,
                               const struct mystr* p_dir_str,
                               const struct mystr* p_ext_dir_str,
                               unsigned int caps, unsigned int options)
{
  struct vsf_sysutil_user* p_user;
  if (!vsf_sysutil_running_as_root())
  {
    bug("vsf_secutil_change_credentials: not running as root");
  }

  p_user = str_getpwnam(p_user_str);

  if (p_user == 0)
  {
    die2("cannot locate user entry:", str_getbuf(p_user_str));
  }
  {

    struct mystr dir_str = INIT_MYSTR;
    /* Work out where the chroot() jail is */
    if (p_dir_str == 0 || str_isempty(p_dir_str))
    {
      str_alloc_text(&dir_str, vsf_sysutil_user_get_homedir(p_user));
    }
    else
    {
      str_copy(&dir_str, p_dir_str);
    }

    /* Sort out supplementary groups before the chroot(). We need to access
     * /etc/groups
     */
    if (options & VSF_SECUTIL_OPTION_USE_GROUPS)
    {
      vsf_sysutil_initgroups(p_user);
    }
    else
    {
      vsf_sysutil_clear_supp_groups();
    }

    /* Always do the chdir() regardless of whether we are chroot()'ing */
    {
      /* Do chdir() with the target effective IDs to cater for NFS mounted
       * home directories.
       */
      int saved_euid = 0;
      int saved_egid = 0;
      int retval;
      if (options & VSF_SECUTIL_OPTION_CHANGE_EUID)
      {
        saved_euid = vsf_sysutil_geteuid();
        saved_egid = vsf_sysutil_getegid();
        vsf_sysutil_setegid(p_user);
        vsf_sysutil_seteuid(p_user);
      }

      retval = str_chdir(&dir_str);

      if (retval != 0)
      {
        die2("cannot change directory:", str_getbuf(&dir_str));
      }
      if (p_ext_dir_str && !str_isempty(p_ext_dir_str))
      {
		retval = str_chdir(p_ext_dir_str);
        
        /* Failure on the extra directory is OK as long as we're not in
         * chroot() mode
         */
        if (retval != 0 && !(options & VSF_SECUTIL_OPTION_CHROOT))
        {
          retval = 0;
        }
      }

      if (retval != 0)
      {
        die2("cannot change directory:", str_getbuf(p_ext_dir_str));
      }
      if (options & VSF_SECUTIL_OPTION_CHANGE_EUID)
      {
        vsf_sysutil_seteuid_numeric(saved_euid);
        vsf_sysutil_setegid_numeric(saved_egid);
      }
      
// 2007.05 James {
      /* Do the chroot() if required */
      //if (options & VSF_SECUTIL_OPTION_CHROOT)
      //{ 
      //  vsf_sysutil_chroot(".");
      //}
// 2007.05 James }
    }
    str_free(&dir_str);
  }
  /* Handle capabilities */
  if (caps)
  {
    if (!vsf_sysdep_has_capabilities())
    {
      /* Need privilege but OS has no capabilities - have to keep root */
      return;
    }
    if (!vsf_sysdep_has_capabilities_as_non_root())
    {
      vsf_sysdep_adopt_capabilities(caps);
      return;
    }
    vsf_sysdep_keep_capabilities();
  }

  /* Set group id */
  vsf_sysutil_setgid(p_user);
  /* Finally set user id */
  vsf_sysutil_setuid(p_user);

  if (caps)
  {
    vsf_sysdep_adopt_capabilities(caps);
  }
}

