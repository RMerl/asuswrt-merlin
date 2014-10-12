/* Cut down version of getent which only returns passwd and group database
   entries and seems to compile on most systems without too much fuss.
   Original copyright notice below. */

/* Copyright (c) 1998, 1999, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Thorsten Kukuk <kukuk@suse.de>, 1998.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not, 
   see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <pwd.h>
#include <grp.h>

group_keys (int number, char *key[])
{
  int result = 0;
  int i;

  for (i = 0; i < number; ++i)
    {
      struct group *grp;

      if (isdigit (key[i][0]))
        grp = getgrgid (atol (key[i]));
      else
        grp = getgrnam (key[i]);

      if (grp == NULL)
        result = 2;
      else
        print_group (grp);
    }

  return result;
}

passwd_keys (int number, char *key[])
{
  int result = 0;
  int i;

  for (i = 0; i < number; ++i)
    {
      struct passwd *pwd;

      if (isdigit (key[i][0]))
        pwd = getpwuid (atol (key[i]));
      else
        pwd = getpwnam (key[i]);

      if (pwd == NULL)
        result = 2;
      else
        print_passwd (pwd);
    }

  return result;
}

print_group (struct group *grp)
{
  unsigned int i = 0;

  printf ("%s:%s:%ld:", grp->gr_name ? grp->gr_name : "",
          grp->gr_passwd ? grp->gr_passwd : "",
          (unsigned long)grp->gr_gid);

  while (grp->gr_mem[i] != NULL)
    {
      fputs (grp->gr_mem[i], stdout);
      ++i;
      if (grp->gr_mem[i] != NULL)
        fputs (",", stdout);
    }
  fputs ("\n", stdout);
}

print_passwd (struct passwd *pwd)
{
  printf ("%s:%s:%ld:%ld:%s:%s:%s\n",
          pwd->pw_name ? pwd->pw_name : "",
          pwd->pw_passwd ? pwd->pw_passwd : "",
          (unsigned long)pwd->pw_uid,
          (unsigned long)pwd->pw_gid,
          pwd->pw_gecos ? pwd->pw_gecos : "",
          pwd->pw_dir ? pwd->pw_dir : "",
          pwd->pw_shell ? pwd->pw_shell : "");
}

int main(int argc, char **argv)
{
  switch(argv[1][0])
    {
    case 'g': /* group */
      if (strcmp (argv[1], "group") == 0)
        {
          if (argc == 2)
            {
              struct group *grp;

              setgrent ();
              while ((grp = getgrent()) != NULL)
                print_group (grp);
              endgrent ();
            }
          else
            return group_keys (argc - 2, &argv[2]);
        }
      else
        goto error;
      break;

   case 'p': /* passwd, protocols */
      if (strcmp (argv[1], "passwd") == 0)
        {
          if (argc == 2)
            {
              struct passwd *pwd;

              setpwent ();
              while ((pwd = getpwent()) != NULL)
                print_passwd (pwd);
              endpwent ();
            }
          else
            return passwd_keys (argc - 2, &argv[2]);
        }
      else
        goto error;
      break;
    default:
    error:
      fprintf (stderr, "Unknown database: %s\n", argv[1]);
      return 1;
    }
  return 0;
}
