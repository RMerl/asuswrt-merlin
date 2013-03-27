--  Copyright 2005, 2007 Free Software Foundation, Inc.
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

procedure P is
   type Index is (One, Two, Three);

   type Table is array (Integer range 1 .. 3) of Integer;
   type ETable is array (Index) of Integer;
   type RTable is array (Index range Two .. Three) of Integer;
   type UTable is array (Positive range <>) of Integer;

   type PTable is array (Index) of Boolean;
   pragma Pack (PTable);

   function Get_UTable (I : Integer) return UTable is
   begin
      return Utable'(1 => I, 2 => 2, 3 => 3);
   end Get_UTable;

   One_Two_Three : Table := (1, 2, 3);
   E_One_Two_Three : ETable := (1, 2, 3);
   R_Two_Three : RTable := (2, 3);
   U_One_Two_Three : UTable := Get_UTable (1);
   P_One_Two_Three : PTable := (False, True, True);

   Few_Reps : UTable := (1, 2, 3, 3, 3, 3, 3, 4, 5);
   Many_Reps : UTable := (1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5);

   Empty : array (1 .. 0) of Integer := (others => 0);

begin
   One_Two_Three (1) := 4;  --  START
   E_One_Two_Three (One) := 4;
   R_Two_Three (Two) := 4;
   U_One_Two_Three (U_One_Two_Three'First) := 4;
   P_One_Two_Three (One) := True;

   Few_Reps (Few_Reps'First) := 2;
   Many_Reps (Many_Reps'First) := 2;

   Empty := (others => 1);
end P;
