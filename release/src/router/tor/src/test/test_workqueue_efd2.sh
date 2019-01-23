#!/bin/sh

${builddir:-.}/src/test/test_workqueue \
	   --no-eventfd --no-pipe2 --no-pipe --no-socketpair
