/* Declarations for css.lex
   Copyright (C) 2006, 2009, 2010, 2011 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef CSS_TOKENS_H
#define CSS_TOKENS_H

enum {
  CSSEOF,
  S,
  CDO,
  CDC,
  INCLUDES,
  DASHMATCH,
  LBRACE,
  PLUS,
  GREATER,
  COMMA,
  STRING,
  INVALID,
  IDENT,
  HASH,
  IMPORT_SYM,
  PAGE_SYM,
  MEDIA_SYM,
  CHARSET_SYM,
  IMPORTANT_SYM,
  EMS,
  EXS,
  LENGTH,
  ANGLE,
  TIME,
  FREQ,
  DIMENSION,
  PERCENTAGE,
  NUMBER,
  URI,
  FUNCTION
};

#endif /* CSS_TOKENS_H */
