/* Copyright 2003, 2004, 2007 Free Software Foundation, Inc.

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



namespace C
{
  class OtherFileClass {
  public:
    int z;
  };

  void ensureOtherRefs () {
    // NOTE (2004-04-23, carlton): This function is here only to make
    // sure that GCC 3.4 outputs debug info for this class.
    static OtherFileClass *c = new OtherFileClass();
  }

  namespace {
    int cXOtherFile = 29;
  };

  int cOtherFile = 316;
}

namespace {
  int XOtherFile = 317;
}
