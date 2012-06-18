#!/bin/sh

# -npro	Do no read the '.indent.pro' files.
# -kr	Use K&R formatting rules
# -i8	Set indentation level to 8 spaces.
# -ts8	Set tab size to 8 spaces
# -sob	Swallow optional blank lines.
# -l80	Set the maximum line length at 80 characters.
# -ss	On one-line for and while statments, force a blank before the semicolon
# -ncs	Do not put a space after cast operators.

indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs "$@"
