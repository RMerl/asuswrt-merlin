/* Exit status handling.
   Copyright (C) 2009, 2010, 2011, 2012 Free Software Foundation, Inc.

   This file is part of GNU Wget.

   GNU Wget is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   GNU Wget is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Wget.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "wget.h"
#include "exits.h"

static int final_exit_status = WGET_EXIT_SUCCESS;

/* XXX: I don't like that newly-added uerr_t codes will doubtless fall
   through the craccks, or the fact that we seem to have way more
   codes than we know what to do with. Need to go through and sort
   through the truly essential codes, and merge the rest with
   those. Quite a few are never even used!

   Quite a few of the codes below would have no business being
   returned to retrieve_url's caller, but since it's very difficult to
   determine which do and which don't, I grab virtually all of them to
   be safe. */
static int
get_status_for_err (uerr_t err)
{
  switch (err)
    {
    case RETROK:
      return WGET_EXIT_SUCCESS;
    case FOPENERR: case FOPEN_EXCL_ERR: case FWRITEERR: case WRITEFAILED:
    case UNLINKERR: case CLOSEFAILED: case FILEBADFILE:
      return WGET_EXIT_IO_FAIL;
    case NOCONERROR: case HOSTERR: case CONSOCKERR: case CONERROR:
    case CONSSLERR: case CONIMPOSSIBLE: case FTPRERR: case FTPINVPASV:
    case READERR: case TRYLIMEXC:
      return WGET_EXIT_NETWORK_FAIL;
    case VERIFCERTERR:
      return WGET_EXIT_SSL_AUTH_FAIL;
    case FTPLOGINC: case FTPLOGREFUSED: case AUTHFAILED:
      return WGET_EXIT_SERVER_AUTH_FAIL;
    case HEOF: case HERR: case ATTRMISSING:
      return WGET_EXIT_PROTOCOL_ERROR;
    case WRONGCODE: case FTPPORTERR: case FTPSYSERR:
    case FTPNSFOD: case FTPUNKNOWNTYPE: case FTPSRVERR:
    case FTPRETRINT: case FTPRESTFAIL: case FTPNOPASV:
    case CONTNOTSUPPORTED: case RANGEERR: case RETRBADPATTERN:
    case PROXERR:
      return WGET_EXIT_SERVER_ERROR;
    case URLERROR: case QUOTEXC: case SSLINITFAILED: case UNKNOWNATTR:
    default:
      return WGET_EXIT_UNKNOWN;
    }
}

/* inform_exit_status
 *
 * Ensure that Wget's exit status will reflect the problem indicated
 * by ERR, unless the exit status has already been set to reflect a more
 * important problem. */
void
inform_exit_status (uerr_t err)
{
  int new_status = get_status_for_err (err);

  if (new_status != WGET_EXIT_SUCCESS
      && (final_exit_status == WGET_EXIT_SUCCESS
          || new_status < final_exit_status))
    {
      final_exit_status = new_status;
    }
}

int
get_exit_status (void)
{
  return
    (final_exit_status == WGET_EXIT_UNKNOWN)
      ? 1
      : final_exit_status;
}
