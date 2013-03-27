--  Copyright 2004, 2007 Free Software Foundation, Inc.
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 3 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program.  If not, see <http://www.gnu.org/licenses/>.

with System;

procedure Fixed_Points is

   type Base_Fixed_Point_Type is
     delta 1.0 / 16.0
       range (System.Min_Int / 2) * 1.0 / 16.0 ..
       (System.Max_Int / 2) * 1.0 / 16.0;

     subtype Fixed_Point_Subtype is
       Base_Fixed_Point_Type range -50.0 .. 50.0;

     type New_Fixed_Point_Type is
       new Base_Fixed_Point_Type range -50.0 .. 50.0;

     Base_Object            : Base_Fixed_Point_Type := -50.0;
     Subtype_Object         : Fixed_Point_Subtype := -50.0;
     New_Type_Object        : New_Fixed_Point_Type := -50.0;
begin
   Base_Object := 1.0/16.0;   -- Set breakpoint here
   Subtype_Object := 1.0/16.0;
   New_Type_Object := 1.0/16.0;
end Fixed_Points;
