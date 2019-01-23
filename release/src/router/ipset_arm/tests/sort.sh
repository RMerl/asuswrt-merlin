#!/bin/sh

head -n 7 $1 > .foo
tail -n +8 $1 | grep  '[[:alnum:]]' | sort >> .foo
rm $1
