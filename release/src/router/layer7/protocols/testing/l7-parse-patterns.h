/*
  By Ethan Sommer <sommere@users.sf.net> and Matthew Strait 
  <quadong@users.sf.net>, (C) Nov 2006-2007
  http://l7-filter.sf.net 

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version
  2 of the License, or (at your option) any later version.
  http://www.gnu.org/licenses/gpl.txt

  This file is synced between the userspace source code and the test suite
  source code.  I don't think it's worth the effort to make it a proper library.
*/


#ifndef L7_PARSE_PATTERNS_H
#define L7_PARSE_PATTERNS_H

using namespace std;
#include <regex.h>

int parse_pattern_file(int & cflags, int & eflags, string & pattern,
        string filename);
string basename(string filename);

#endif          
