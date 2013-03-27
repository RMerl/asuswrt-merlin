/*  This file is part of the program GDB.

    Copyright (C) 1997, Free Software Foundation, Inc.

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


#ifndef _SIM_ASSERT_H_
#define _SIM_ASSERT_H_

#define SIM_FILTER_PATH(FILE, PATH) \
do \
  { \
    /* strip leading path */ \
    const char *p = (PATH); \
    (FILE) = p; \
    while (*p != '\0' && *p != ':') \
      { \
        if (*p == '/') \
          (FILE) = p + 1; \
        p++; \
      } \
  } \
while (0)

/* The subtle difference between SIM_ASSERT and ASSERT is that
   SIM_ASSERT passes `sd' to sim_io_error for the SIM_DESC,
   ASSERT passes NULL.  */

#if !defined (SIM_ASSERT)
#if defined (WITH_ASSERT)
#define SIM_ASSERT(EXPRESSION) \
do \
  { \
    if (WITH_ASSERT) \
      { \
        if (!(EXPRESSION)) \
          { \
            /* report the failure */ \
            const char *file; \
            SIM_FILTER_PATH(file, __FILE__); \
            sim_io_error (sd, "%s:%d: assertion failed - %s", \
                          file, __LINE__, #EXPRESSION); \
          } \
      } \
  } \
while (0)
#else
#define SIM_ASSERT(EXPRESSION) do { /*nothing*/; } while (0)
#endif
#endif

#if !defined (ASSERT)
#if defined (WITH_ASSERT)
#define ASSERT(EXPRESSION) \
do \
  { \
    if (WITH_ASSERT) \
      { \
        if (!(EXPRESSION)) \
          { \
            /* report the failure */ \
            const char *file; \
            SIM_FILTER_PATH(file, __FILE__); \
            sim_io_error (NULL, "%s:%d: assertion failed - %s", \
                          file, __LINE__, #EXPRESSION); \
          } \
      } \
  } \
while (0)
#else
#define ASSERT(EXPRESSION) do { /*nothing*/; } while (0)
#endif
#endif

#endif
