#!/bin/sh
find -name "*.asp" | xargs enca -L none > en_result.txt
find -name "*.htm" | xargs enca -L none >> en_result.txt
find -name "*.js" | xargs enca -L none >> en_result.txt
find -name "*.dict" | xargs enca -L none >> en_result.txt

