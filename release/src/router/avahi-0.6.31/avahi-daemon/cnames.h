#ifndef foocnameshfoo
#define foocnameshfoo

/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

void cnames_register(char **aliases);
void cnames_free_all(void);
void cnames_add_to_server(void);
void cnames_remove_from_server(void);

void llmnr_cnames_register(char **aliases_llmnr);
void llmnr_cnames_free_all(void);
void llmnr_cnames_add_to_server(void);
void llmnr_cnames_remove_from_server(void);


#endif
