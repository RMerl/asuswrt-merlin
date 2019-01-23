#!/bin/sh

${builddir:-.}/src/test/test_workqueue \
	   --no-eventfd2 --no-pipe2 --no-pipe --no-socketpair
