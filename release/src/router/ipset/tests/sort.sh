#!/bin/sh

awk '/^[A-Za-z]+:/ { print $0 }' $1 > .foo
awk '!/^[A-Za-z]+:/ && !/inding/ { print $0 }' | sort >> .foo
rm $1
