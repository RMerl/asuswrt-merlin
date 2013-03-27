/* Code to go along with tests in rtti.exp.
   
   Copyright 2003, 2004, 2007 Free Software Foundation, Inc.

   Contributed by David Carlton <carlton@bactrian.org> and by Kealia,
   Inc.

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

namespace n2 {

  class C2;

  class Base2 {
  public:
    virtual ~Base2() { }
  };


  class C2: public Base2 {
  public:
  };

  class D2 : public C2{
  public:
    D2(C2 *, C2 *);
    
    C2* expr_1_;
    C2* expr_2_;
  };

  extern C2 *create2();

  namespace n3 {
    class C3 : public C2 {
    public:
    };
  }

  extern n3::C3 *create3();
}
