c Copyright 2005 Free Software Foundation, Inc.

c This program is free software; you can redistribute it and/or modify
c it under the terms of the GNU General Public License as published by
c the Free Software Foundation; either version 3 of the License, or
c (at your option) any later version.
c
c This program is distributed in the hope that it will be useful,
c but WITHOUT ANY WARRANTY; without even the implied warranty of
c MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
c GNU General Public License for more details.
c
c You should have received a copy of the GNU General Public License
c along with this program.  If not, see <http://www.gnu.org/licenses/>.

c Ihis file is the Fortran source file for subarray.exp.  It was written
c by Wu Zhou. (woodzltc@cn.ibm.com)

        PROGRAM subarray

        character *7 str
        integer array(7)

c Initialize character array "str" and integer array "array". 
        str = 'abcdefg'
        do i = 1, 7
          array(i) = i
        end do

        write (*, *) str(2:4)
        write (*, *) str(:3)
        write (*, *) str(5:)
        write (*, *) str(:)

        END PROGRAM
