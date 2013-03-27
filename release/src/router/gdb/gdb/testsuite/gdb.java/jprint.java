// jprint.java test program.
//
// Copyright 2004
// Free Software Foundation, Inc.
//
// Written by Jeff Johnston <jjohnstn@redhat.com> 
// Contributed by Red Hat
//
// This file is part of GDB.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//   
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//   
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

class jvclass {
  public static int k;
  static {
    k = 77;
  }
  public static void addprint (int x, int y, int z) {
    int sum = x + y + z;
    System.out.println ("sum is " + sum);
  }

  public int addk (int x) {
    int sum = x + k;
    System.out.println ("adding k gives " + sum);
    return sum;
  }
}
    
public class jprint extends jvclass {
  public int dothat (int x) {
    int y = x + 3;
    System.out.println ("new value is " + y);
    return y + 4;
  }
  public static void print (int x) {
    System.out.println("x is " + x);
  }
  public static void print (int x, int y) {
    System.out.println("y is " + y);
  }
  public static void main(String[] args) {
    jprint x = new jprint ();
    x.dothat (44);
    print (k, 33);
  }
}


