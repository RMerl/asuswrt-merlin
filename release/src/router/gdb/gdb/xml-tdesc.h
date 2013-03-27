/* XML target description support for GDB.

   Copyright (C) 2006
   Free Software Foundation, Inc.

   Contributed by CodeSourcery.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

struct target_ops;
struct target_desc;

/* Read an XML target description from FILENAME.  Parse it, and return
   the parsed description.  */

const struct target_desc *file_read_description_xml (const char *filename);

/* Read an XML target description using OPS.  Parse it, and return the
   parsed description.  */

const struct target_desc *target_read_description_xml (struct target_ops *);
