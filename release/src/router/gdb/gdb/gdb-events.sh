#!/bin/sh

# User Interface Events.
#
# Copyright (C) 1999, 2000, 2001, 2002, 2004, 2005, 2007
# Free Software Foundation, Inc.
#
# Contributed by Cygnus Solutions.
#
# This file is part of GDB.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

IFS=:

read="class returntype function formal actual attrib"

function_list ()
{
  # category:
  #        # -> disable
  #        * -> compatibility - pointer variable that is initialized
  #             by set_gdb_events().
  #        ? -> Predicate and function proper.
  #        f -> always call (must have a void returntype)
  # return-type
  # name
  # formal argument list
  # actual argument list
  # attributes
  # description
  cat <<EOF |
f:void:breakpoint_create:int b:b
f:void:breakpoint_delete:int b:b
f:void:breakpoint_modify:int b:b
f:void:tracepoint_create:int number:number
f:void:tracepoint_delete:int number:number
f:void:tracepoint_modify:int number:number
f:void:architecture_changed:void
EOF
  grep -v '^#'
}

copyright ()
{
  cat <<EOF
/* User Interface Events.

   Copyright (C) 1999, 2001, 2002, 2004, 2005, 2007
   Free Software Foundation, Inc.

   Contributed by Cygnus Solutions.

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

/* Work in progress */

/* This file was created with the aid of \`\`gdb-events.sh''.

   The bourn shell script \`\`gdb-events.sh'' creates the files
   \`\`new-gdb-events.c'' and \`\`new-gdb-events.h and then compares
   them against the existing \`\`gdb-events.[hc]''.  Any differences
   found being reported.

   If editing this file, please also run gdb-events.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdb-events.sh and using its output may
   prove easier.  */

EOF
}

#
# The .h file
#

exec > new-gdb-events.h
copyright
cat <<EOF

#ifndef GDB_EVENTS_H
#define GDB_EVENTS_H
EOF

# pointer declarations
echo ""
echo ""
cat <<EOF
/* COMPAT: pointer variables for old, unconverted events.
   A call to set_gdb_events() will automatically update these. */
EOF
echo ""
function_list | while eval read $read
do
  case "${class}" in
    "*" )
	echo "extern ${returntype} (*${function}_event) (${formal})${attrib};"
	;;
  esac
done

# function typedef's
echo ""
echo ""
cat <<EOF
/* Type definition of all hook functions.  Recommended pratice is to
   first declare each hook function using the below ftype and then
   define it.  */
EOF
echo ""
function_list | while eval read $read
do
  echo "typedef ${returntype} (gdb_events_${function}_ftype) (${formal});"
done

# gdb_events object
echo ""
echo ""
cat <<EOF
/* gdb-events: object. */
EOF
echo ""
echo "struct gdb_events"
echo "  {"
function_list | while eval read $read
do
  echo "    gdb_events_${function}_ftype *${function}${attrib};"
done
echo "  };"

# function declarations
echo ""
echo ""
cat <<EOF
/* Interface into events functions.
   Where a *_p() predicate is present, it must be called before
   calling the hook proper.  */
EOF
function_list | while eval read $read
do
  case "${class}" in
    "*" ) continue ;;
    "?" )
	echo "extern int ${function}_p (void);"
        echo "extern ${returntype} ${function}_event (${formal})${attrib};"
	;;
    "f" )
	echo "extern ${returntype} ${function}_event (${formal})${attrib};"
	;;
  esac
done

# our set function
cat <<EOF

/* Install custom gdb-events hooks.  */
extern struct gdb_events *deprecated_set_gdb_event_hooks (struct gdb_events *vector);

/* Deliver any pending events.  */
extern void gdb_events_deliver (struct gdb_events *vector);

/* Clear event handlers.  */
extern void clear_gdb_event_hooks (void);
EOF

# close it off
echo ""
echo "#endif"
exec 1>&2
#../move-if-change new-gdb-events.h gdb-events.h
if test -r gdb-events.h
then
  diff -c gdb-events.h new-gdb-events.h
  if [ $? = 1 ]
  then
    echo "gdb-events.h changed? cp new-gdb-events.h gdb-events.h" 1>&2
  fi
else
  echo "File missing? mv new-gdb-events.h gdb-events.h" 1>&2
fi



#
# C file
#

exec > new-gdb-events.c
copyright
cat <<EOF

#include "defs.h"
#include "gdb-events.h"
#include "gdbcmd.h"

static struct gdb_events null_event_hooks;
static struct gdb_events queue_event_hooks;
static struct gdb_events *current_event_hooks = &null_event_hooks;

int gdb_events_debug;
static void
show_gdb_events_debug (struct ui_file *file, int from_tty,
                       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Event debugging is %s.\\n"), value);
}

EOF

# function bodies
function_list | while eval read $read
do
  case "${class}" in
    "*" ) continue ;;
    "?" )
cat <<EOF

int
${function}_event_p (${formal})
{
  return current_event_hooks->${function};
}

${returntype}
${function}_event (${formal})
{
  return current_events->${function} (${actual});
}
EOF
	;;
     "f" )
cat <<EOF

void
${function}_event (${formal})
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "${function}_event\n");
  if (!current_event_hooks->${function})
    return;
  current_event_hooks->${function} (${actual});
}
EOF
	;;
  esac
done

# Set hooks function
echo ""
cat <<EOF
struct gdb_events *
deprecated_set_gdb_event_hooks (struct gdb_events *vector)
{
  struct gdb_events *old_events = current_event_hooks;
  if (vector == NULL)
    current_event_hooks = &queue_event_hooks;
  else
    current_event_hooks = vector;
  return old_events;
EOF
function_list | while eval read $read
do
  case "${class}" in
    "*" )
      echo "  ${function}_event = hooks->${function};"
      ;;
  esac
done
cat <<EOF
}
EOF

# Clear hooks function
echo ""
cat <<EOF
void
clear_gdb_event_hooks (void)
{
  deprecated_set_gdb_event_hooks (&null_event_hooks);
}
EOF

# event type
echo ""
cat <<EOF
enum gdb_event
{
EOF
function_list | while eval read $read
do
  case "${class}" in
    "f" )
      echo "  ${function},"
      ;;
  esac
done
cat <<EOF
  nr_gdb_events
};
EOF

# event data
echo ""
function_list | while eval read $read
do
  case "${class}" in
    "f" )
      if test ${actual}
      then
        echo "struct ${function}"
        echo "  {"
        echo "    `echo ${formal} | tr '[,]' '[;]'`;"
        echo "  };"
        echo ""
      fi
      ;;
  esac
done

# event queue
cat <<EOF
struct event
  {
    enum gdb_event type;
    struct event *next;
    union
      {
EOF
function_list | while eval read $read
do
  case "${class}" in
    "f" )
      if test ${actual}
      then
        echo "        struct ${function} ${function};"
      fi
      ;;
  esac
done
cat <<EOF
      }
    data;
  };
struct event *pending_events;
struct event *delivering_events;
EOF

# append
echo ""
cat <<EOF
static void
append (struct event *new_event)
{
  struct event **event = &pending_events;
  while ((*event) != NULL)
    event = &((*event)->next);
  (*event) = new_event;
  (*event)->next = NULL;
}
EOF

# schedule a given event
function_list | while eval read $read
do
  case "${class}" in
    "f" )
      echo ""
      echo "static void"
      echo "queue_${function} (${formal})"
      echo "{"
      echo "  struct event *event = XMALLOC (struct event);"
      echo "  event->type = ${function};"
      for arg in `echo ${actual} | tr '[,]' '[:]' | tr -d '[ ]'`; do
        echo "  event->data.${function}.${arg} = ${arg};"
      done
      echo "  append (event);"
      echo "}"
      ;;
  esac
done

# deliver
echo ""
cat <<EOF
void
gdb_events_deliver (struct gdb_events *vector)
{
  /* Just zap any events left around from last time. */
  while (delivering_events != NULL)
    {
      struct event *event = delivering_events;
      delivering_events = event->next;
      xfree (event);
    }
  /* Process any pending events.  Because one of the deliveries could
     bail out we move everything off of the pending queue onto an
     in-progress queue where it can, later, be cleaned up if
     necessary. */
  delivering_events = pending_events;
  pending_events = NULL;
  while (delivering_events != NULL)
    {
      struct event *event = delivering_events;
      switch (event->type)
        {
EOF
function_list | while eval read $read
do
  case "${class}" in
    "f" )
      echo "        case ${function}:"
      if test ${actual}
      then
        echo "          vector->${function}"
        sep="            ("
        ass=""
        for arg in `echo ${actual} | tr '[,]' '[:]' | tr -d '[ ]'`; do
          ass="${ass}${sep}event->data.${function}.${arg}"
	  sep=",
               "
        done
        echo "${ass});"
      else
        echo "          vector->${function} ();"
      fi
      echo "          break;"
      ;;
  esac
done
cat <<EOF
        }
      delivering_events = event->next;
      xfree (event);
    }
}
EOF

# Finally the initialization
echo ""
cat <<EOF
void _initialize_gdb_events (void);
void
_initialize_gdb_events (void)
{
  struct cmd_list_element *c;
EOF
function_list | while eval read $read
do
  case "${class}" in
    "f" )
      echo "  queue_event_hooks.${function} = queue_${function};"
      ;;
  esac
done
cat <<EOF

  add_setshow_zinteger_cmd ("event", class_maintenance,
                            &gdb_events_debug, _("\\
Set event debugging."), _("\\
Show event debugging."), _("\\
When non-zero, event/notify debugging is enabled."),
                            NULL,
                            show_gdb_events_debug,
                            &setdebuglist, &showdebuglist);
}
EOF

# close things off
exec 1>&2
#../move-if-change new-gdb-events.c gdb-events.c
# Replace any leading spaces with tabs
sed < new-gdb-events.c > tmp-gdb-events.c \
    -e 's/\(	\)*        /\1	/g'
mv tmp-gdb-events.c new-gdb-events.c
# Move if changed?
if test -r gdb-events.c
then
  diff -c gdb-events.c new-gdb-events.c
  if [ $? = 1 ]
  then
    echo "gdb-events.c changed? cp new-gdb-events.c gdb-events.c" 1>&2
  fi
else
  echo "File missing? mv new-gdb-events.c gdb-events.c" 1>&2
fi
