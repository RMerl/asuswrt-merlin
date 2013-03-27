--  Copyright 2006, 2007 Free Software Foundation, Inc.
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

package Pck is

   type Data_Small is array (1 .. 2) of Integer;
   type Data_Large is array (1 .. 4) of Integer;

   type Small_Float_Vector is array (1 .. 2) of Float;

   function Create_Small return Data_Small;
   function Create_Large return Data_Large;
   function Create_Small_Float_Vector return Small_Float_Vector;

end Pck;

