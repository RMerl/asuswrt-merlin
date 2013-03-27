! Copyright 2006 Free Software Foundation, Inc.
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 3 of the License, or
! (at your option) any later version.
! 
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
! 
! You should have received a copy of the GNU General Public License
! along with this program.  If not, see <http://www.gnu.org/licenses/>.
!
! Ihis file is the Fortran source file for derived-type.exp.  It was written
! by Wu Zhou. (woodzltc@cn.ibm.com)

program main

  type bar
    integer :: c
    real :: d
  end type
  type foo
    real :: a
    type(bar) :: x
    character*7 :: b 
  end type foo
  type(foo) :: q
  type(bar) :: p

  p = bar(1, 2.375)
  q%a = 3.125
  q%b = "abcdefg"
  q%x%c = 1
  q%x%d = 2.375
  print *,p,q 

end program main
