#!/bin/bash
if cvs log $1 >/dev/null 2>/dev/null; then exit 0; else echo "$1 shouldn't be here" ; exit 1; fi


