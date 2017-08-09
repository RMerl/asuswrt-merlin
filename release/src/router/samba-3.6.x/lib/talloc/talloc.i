/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Don't expose talloc contexts in Python code. Python does reference 
   counting for us, so just create a new top-level talloc context.
 */
%typemap(in, numinputs=0, noblock=1) TALLOC_CTX * {
    $1 = NULL;
}

%define %talloctype(TYPE)
%nodefaultctor TYPE;
%extend TYPE {
    ~TYPE() { talloc_free($self); }
}
%enddef
