/* This test script is part of GDB, the GNU debugger.

   Copyright 2002, 2004,
   Free Software Foundation, Inc.

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

#include <exception>
#include <stdexcept>
#include <string>

enum region { oriental, egyptian, greek, etruscan, roman };

// Test one.
class gnu_obj_1
{
public:
  typedef region antiquities;
  const bool 		test;
  const int 		key1;
  long       		key2;

  antiquities 	value;

  gnu_obj_1(antiquities a, long l): test(true), key1(5), key2(l), value(a) {}
};

// Test two.
template<typename T>
class gnu_obj_2: public virtual gnu_obj_1
{
public:
  antiquities	value_derived;
  
  gnu_obj_2(antiquities b): gnu_obj_1(oriental, 7), value_derived(b) { }
}; 

// Test three.
template<typename T>
class gnu_obj_3
{
public:
  typedef region antiquities;
  gnu_obj_2<int>   	data;
      
  gnu_obj_3(antiquities b): data(etruscan) { }
}; 

int main()
{
  bool test = true;
  const int i = 5;
  int j = i;
  gnu_obj_2<long> test2(roman);
  gnu_obj_3<long> test3(greek);

  // 1
  try
    {
      ++j;
      throw gnu_obj_1(egyptian, 4589);	// marker 1-throw
    }
  catch (gnu_obj_1& obj)
    {
      ++j;
      if (obj.value != egyptian)	// marker 1-catch
	test &= false;
      if (obj.key2 != 4589)
	test &= false;     
    }
  catch (...)
    {
      j = 0;
      test &= false;
    }

  // 2
  try
    {
      ++j;				// marker 2-start
      try
	{
	  ++j;				// marker 2-next
	  try
	    {
	      ++j;
	      throw gnu_obj_1(egyptian, 4589); // marker 2-throw
	    }
	  catch (gnu_obj_1& obj)
	    {
	      ++j;
	      if (obj.value != egyptian) // marker 2-catch
		test &= false;
	      if (obj.key2 != 4589)
		test &= false;     
	    }
	}
      catch (gnu_obj_1& obj)
	{
	  ++j;
	  if (obj.value != egyptian)
	    test &= false;
	  if (obj.key2 != 4589)
	    test &= false;     
	}
    }
  catch (...)
    {
      j = 0;
      test &= false;
    }

  // 3 use standard library
  using namespace std;
  try
    {
      if (j < 100)
	throw invalid_argument("gdb.1"); // marker 3-throw
    }
  catch (exception& obj)
    {
      if (obj.what() != "gdb.1")	// marker 3-catch
	test &= false;
    }
  return 0;
}
