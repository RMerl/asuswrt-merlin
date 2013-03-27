/* This testcase is part of GDB, the GNU debugger.

   Copyright 1992, 2004, 2007 Free Software Foundation, Inc.

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

#include <string>
#include <iostream>
using namespace std;

template <class T>
class StringTest {
public:
   virtual void runTest();
   void testFunction();
};

template <class T>
void StringTest<T>:: runTest() {
   testFunction ();
}

template <class T>
void StringTest <T>::testFunction() {
   // initialize s with string literal
   cout << "in StringTest" << endl;
   string s("I am a shot string");
   cout << s << endl;

   // insert 'r' to fix "shot"
   s.insert(s.begin()+10,'r' );
   cout << s << endl;

   // concatenate another string
   s += "and now a longer string";
   cout << s << endl;

   // find position where blank needs to be inserted
   string::size_type spos = s.find("and");
   s.insert(spos, " ");
   cout << s << endl;

   // erase the concatenated part
   s.erase(spos);
   cout << s << endl;
}

int main() {
   StringTest<wchar_t> ts;
   ts.runTest();
}

/* output:
I am a shot string
I am a short string
I am a short stringand now a longer string
I am a short string and now a longer string
I am a short string
*/
