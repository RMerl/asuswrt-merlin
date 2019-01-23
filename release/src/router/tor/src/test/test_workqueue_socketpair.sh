#!/bin/sh

${builddir:-.}/src/test/test_workqueue \
	   --no-eventfd2 --no-eventfd --no-pipe2 --no-pipe
