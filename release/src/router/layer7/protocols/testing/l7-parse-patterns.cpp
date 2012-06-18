/*
  Functions and classes which keep track of and use regexes to classify streams
  of application data.

  By Ethan Sommer <sommere@users.sf.net> and Matthew Strait
  <quadong@users.sf.net>, (C) 2006-2007
  http://l7-filter.sf.net

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version
  2 of the License, or (at your option) any later version.
  http://www.gnu.org/licenses/gpl.txt

  This file is synced between the userspace source code and the test suite
  source code.  I don't think it's worth the effort to make it a proper library.
*/

using namespace std;

#include <iostream>
#include <fstream>
#include <string>
#include "l7-parse-patterns.h"

// Returns true if the line (from a pattern file) is a comment
static int is_comment(string line)
{
        // blank lines are comments
        if(line.size() == 0) return 1;

        // lines starting with # are comments
        if(line[0] == '#') return 1;

        // lines with only whitespace are comments
        for(unsigned int i = 0; i < line.size(); i++)
                if(!isspace(line[i]))
                        return 0;
        return 1;
}

// Extracts the protocol name from a line
// This line should be exactly the name of the file without the .pat extension
// However, we also allow junk after whitespace
static string get_protocol_name(string line)
{
  string name = "";
  for(unsigned int i = 0; i < line.size(); i++)
  {
    if(!isspace(line[i]))
      name += line[i];
    else break;
  }
  return name;
}

// Returns the given file name from the last slash to the next dot
string basename(string filename)
{
        int lastslash = filename.find_last_of('/');
        int nextdot = filename.find_first_of('.', lastslash);

        return filename.substr(lastslash+1, nextdot - (lastslash+1));
}

// Returns, e.g. "userspace pattern" if the line is "userspace pattern=.*foo"
static string attribute(string line)
{
  return line.substr(0, line.find_first_of('='));
}

// Returns, e.g. ".*foo" if the line is "userspace pattern=.*foo"
static string value(string line)
{
  return line.substr(line.find_first_of('=')+1);
}

// parse the regexec and regcomp flags
// Returns 1 on sucess, 0 if any unrecognized flags were encountered
static int parseflags(int & cflags, int & eflags, string line)
{
  string flag = "";
  cflags = 0;
  eflags = 0;
  for(unsigned int i = 0; i < line.size(); i++){
    if(!isspace(line[i]))
      flag += line[i];

    if(isspace(line[i]) || i == line.size()-1){
      if(flag == "REG_EXTENDED")     cflags |= REG_EXTENDED;
      else if(flag == "REG_ICASE")   cflags |= REG_ICASE;
      else if(flag == "REG_NOSUB")   cflags |= REG_NOSUB;
      else if(flag == "REG_NEWLINE") cflags |= REG_NEWLINE;
      else if(flag == "REG_NOTBOL")  eflags |= REG_NOTBOL;
      else if(flag == "REG_NOTEOL")  eflags |= REG_NOTEOL;
      else{
        cerr<<"Error: encountered unknown flag in pattern file " <<flag <<endl;
        return 0;
      }
      flag = "";
    }
  }
  return 1;
}

// Returns 1 on sucess, 0 on failure.
// Takes a filename and "returns" the pattern and flags
int parse_pattern_file(int & cflags, int & eflags, string & pattern,
        string filename)
{
  ifstream the_file(filename.c_str());

  if(!the_file.is_open()){
    cerr << "couldn't read file.\n";
    return 0;
  }

  // What we're looking for. It's either the protocol name, the kernel pattern,
  // which we'll use if no other is present, or any of various (ok, two)
  // userspace config lines.
  enum { protocol, kpattern, userspace } state = protocol;

  string name = "", line;
  cflags = REG_EXTENDED | REG_ICASE | REG_NOSUB;
  eflags = 0;

  while (!the_file.eof()){
    getline(the_file, line);

    if(is_comment(line)) continue;

    if(state == protocol){
      name = get_protocol_name(line);

      if(name != basename(filename)){
        cerr << "Error: Protocol declared in file does not match file name.\n"
          << "File name is " << basename(filename)
          << ", but the file says " << name << endl;
        return 0;
      }
      state = kpattern;
      continue;
    }

    if(state == kpattern){
      pattern = line;
      state = userspace;
      continue;
    }

    if(state == userspace){

      if(line.find_first_of('=') == string::npos){
        cerr<<"Warning: ignored bad line in pattern file:\n\t"<<line<<endl;
        continue;
      }

      if(attribute(line) == "userspace pattern"){
        pattern = value(line);
      }
      else if(attribute(line) == "userspace flags"){
        if(!parseflags(cflags, eflags, value(line)))
          return 0;
      }
      else
        cerr << "Warning: ignored unknown pattern file attribute \""
          << attribute(line) << "\"\n";
    }
  }
  return 1;
}
