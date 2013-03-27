/* This test script is part of GDB, the GNU debugger.

   Copyright 1999, 2002, 2003, 2004, 2005, 2006, 2007
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

#include <iostream>

using namespace std;

void marker1()
{
  return;
}

class A1 {
  int x;
  int y;

friend ostream& operator<<(ostream& outs, A1 one);

public:

  A1(int a, int b)
  {
   x=a;
   y=b;
  }

A1 operator+=(int value);
A1 operator+(const A1&);
A1 operator-(const A1&);
A1 operator%(const A1&);
int operator==(const A1&);
int operator!=(const A1&);
int operator&&(const A1&);
int operator||(const A1&);
A1 operator<<(int);
A1 operator>>(int);
A1 operator|(const A1&);
A1 operator^(const A1&);
A1 operator&(const A1&);
int operator<(const A1&);
int operator<=(const A1&);
int operator>=(const A1&);
int operator>(const A1&);
A1 operator*(const A1&);
A1 operator/(const A1&);
A1 operator=(const A1&);

A1 operator~();
A1 operator+();
A1 operator-();
int operator!();
A1 operator++();
A1 operator++(int);
A1 operator--(); 
A1 operator--(int);

};


A1 A1::operator+(const A1& second)
{
 A1 sum(0,0);
 sum.x = x + second.x;
 sum.y = y + second.y;
 
 return (sum);
}

A1 A1::operator*(const A1& second)
{
 A1 product(0,0);
 product.x = this->x * second.x;
 product.y = this->y * second.y;
 
 return product;
}

A1 A1::operator-(const A1& second)
{
 A1 diff(0,0);
 diff.x = x - second.x;
 diff.y = y - second.y;
 
 return diff;
}

A1 A1::operator/(const A1& second)
{
 A1 div(0,0);
 div.x = x / second.x;
 div.y = y / second.y;
 
 return div;
}

A1 A1::operator%(const A1& second)
{
 A1 rem(0,0);
 rem.x = x % second.x;
 rem.y = y % second.y;
 
 return rem;
}

int A1::operator==(const A1& second)
{
 int a = (x == second.x);
 int b = (y == second.y);
 
 return (a && b);
}

int A1::operator!=(const A1& second)
{
 int a = (x != second.x);
 int b = (y != second.y);
 
 return (a || b);
}

int A1::operator&&(const A1& second)
{
 return ( x && second.x);
}

int A1::operator||(const A1& second)
{
 return ( x || second.x);
}

A1 A1::operator<<(int value)
{
 A1 lshft(0,0);
 lshft.x = x << value;
 lshft.y = y << value;
 
 return lshft;
}

A1 A1::operator>>(int value)
{
 A1 rshft(0,0);
 rshft.x = x >> value;
 rshft.y = y >> value;
 
 return rshft;
}

A1 A1::operator|(const A1& second)
{
 A1 abitor(0,0);
 abitor.x = x | second.x;
 abitor.y = y | second.y;
 
 return abitor;
}

A1 A1::operator^(const A1& second)
{
 A1 axor(0,0);
 axor.x = x ^ second.x;
 axor.y = y ^ second.y;
 
 return axor;
}

A1 A1::operator&(const A1& second)
{
 A1 abitand(0,0);
 abitand.x = x & second.x;
 abitand.y = y & second.y;
 
 return abitand;
}

int A1::operator<(const A1& second)
{
 A1 b(0,0);
 b.x = 3;
 return (x < second.x);
}

int A1::operator<=(const A1& second)
{
 return (x <= second.x);
}

int A1::operator>=(const A1& second)
{
 return (x >= second.x);
}

int A1::operator>(const A1& second)
{
 return (x > second.x);
}

int A1::operator!(void)
{
 return (!x);
}

A1 A1::operator-(void)
{
 A1 neg(0,0);
 neg.x = -x;
 neg.y = -y;

 return (neg);
}

A1 A1::operator+(void)
{
 A1 pos(0,0);
 pos.x = +x;
 pos.y = +y;

 return (pos);
}

A1 A1::operator~(void)
{
 A1 acompl(0,0);
 acompl.x = ~x;
 acompl.y = ~y;

 return (acompl);
}

A1 A1::operator++() // pre increment
{
 x = x +1;
 
 return (*this);
}

A1 A1::operator++(int) // post increment
{
 y = y +1;
 
 return (*this);
}

A1 A1::operator--() // pre decrement
{
 x = x -1;
 
 return (*this);
}

A1 A1::operator--(int) // post decrement
{
 y = y -1;
 
 return (*this);
}


A1 A1::operator=(const A1& second)
{

 x = second.x;
 y = second.y;

 return (*this);
}

A1 A1::operator+=(int value)
{

 x += value;
 y += value;

 return (*this);
}

ostream& operator<<(ostream& outs, A1 one)
{
 return (outs << endl << "x = " << one.x << endl << "y = " << one.y << endl << "-------" << endl); 
}

class A2 {
  public:
A2 operator+();
};

A2 A2::operator+()
{
  return A2 ();
}

class Member
{
public:
  int z;
};

class Container
{
public:
  Member m;

  Member& operator* ();
};

Member& Container::operator* ()
{
  return this->m;
}

int main (void)
{
 A1 one(2,3);
 A1 two(4,5);
 A1 three(0,0);
 Container c;
 int val;
 
 marker1(); // marker1-returns-here
 cout << one; // marker1-returns-here
 cout << two;
 three = one + two;
 cout << "+ " <<  three;
 three = one - two;
 cout <<  "- " << three;
 three = one * two;
 cout <<"* " <<  three;
 three = one / two;
 cout << "/ " << three;
 three = one % two;
 cout << "% " << three;
 three = one | two;
 cout << "| " <<three;
 three = one ^ two;
 cout << "^ " <<three;
 three = one & two;
 cout << "& "<< three;

 val = one && two;
 cout << "&& " << val << endl << "-----"<<endl;
 val = one || two;
 cout << "|| " << val << endl << "-----"<<endl;
 val = one == two;
 cout << " == " << val << endl << "-----"<<endl;
 val = one != two;
 cout << "!= " << val << endl << "-----"<<endl;
 val = one >= two;
 cout << ">= " << val << endl << "-----"<<endl;
 val = one <= two;
 cout << "<= " << val << endl << "-----"<<endl;
 val = one < two;
 cout << "< " << val << endl << "-----"<<endl;
 val = one > two;
 cout << "> " << val << endl << "-----"<<endl;
 
 three = one << 2;
 cout << "lsh " << three;
 three = one >> 2;
 cout << "rsh " << three;

 three = one;
 cout << " = "<< three;
 three += 5;
 cout << " += "<< three;
 
 val = (!one);
 cout << "! " << val << endl << "-----"<<endl;
 three = (+one);
 cout << "+ " << three;
 three = (-one);
 cout << "- " << three;
 three = (~one);
 cout << " ~" << three;
 three++;
 cout << "postinc " << three;
 three--;
 cout << "postdec " << three;
 
 --three;
 cout << "predec " << three;
 ++three;
 cout << "preinc " << three;

 (*c).z = 1;

 return 0;

}
