#!/bin/bash
#  subunit shell bindings.
#  Copyright (C) 2006  Robert Collins <robertc@robertcollins.net>
#
#  Licensed under either the Apache License, Version 2.0 or the BSD 3-clause
#  license at the users choice. A copy of both licenses are available in the
#  project source as Apache-2.0 and BSD. You may not use this file except in
#  compliance with one of these two licences.
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under these licenses is distributed on an "AS IS" BASIS, WITHOUT
#  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
#  license you chose for the specific language governing permissions and
#  limitations under that license.
#


# this script tests that we can source the subunit shell bindings successfully.
# It manually implements the control protocol so that it des not depend on the
# bindings being complete yet.

# we expect to be run from the tree root.

echo 'test: shell bindings can be sourced'
# if any output occurs, this has failed to source cleanly
source_output=$(. ${SHELL_SHARE}subunit.sh 2>&1)
if [ $? == 0 -a "x$source_output" = "x" ]; then
  echo 'success: shell bindings can be sourced'
else
  echo 'failure: shell bindings can be sourced ['
  echo 'got an error code or output during sourcing.:'
  echo $source_output
  echo ']' ;
fi

# now source it for real
. ${SHELL_SHARE}subunit.sh

# we should have a start_test function
echo 'test: subunit_start_test exists'
found_type=$(type -t subunit_start_test)
status=$?
if [ $status == 0 -a "x$found_type" = "xfunction" ]; then
  echo 'success: subunit_start_test exists'
else
  echo 'failure: subunit_start_test exists ['
  echo 'subunit_start_test is not a function:'
  echo "type -t status: $status"
  echo "output: $found_type"
  echo ']' ;
fi

# we should have a pass_test function
echo 'test: subunit_pass_test exists'
found_type=$(type -t subunit_pass_test)
status=$?
if [ $status == 0 -a "x$found_type" = "xfunction" ]; then
  echo 'success: subunit_pass_test exists'
else
  echo 'failure: subunit_pass_test exists ['
  echo 'subunit_pass_test is not a function:'
  echo "type -t status: $status"
  echo "output: $found_type"
  echo ']' ;
fi

# we should have a fail_test function
echo 'test: subunit_fail_test exists'
found_type=$(type -t subunit_fail_test)
status=$?
if [ $status == 0 -a "x$found_type" = "xfunction" ]; then
  echo 'success: subunit_fail_test exists'
else
  echo 'failure: subunit_fail_test exists ['
  echo 'subunit_fail_test is not a function:'
  echo "type -t status: $status"
  echo "output: $found_type"
  echo ']' ;
fi

# we should have a error_test function
echo 'test: subunit_error_test exists'
found_type=$(type -t subunit_error_test)
status=$?
if [ $status == 0 -a "x$found_type" = "xfunction" ]; then
  echo 'success: subunit_error_test exists'
else
  echo 'failure: subunit_error_test exists ['
  echo 'subunit_error_test is not a function:'
  echo "type -t status: $status"
  echo "output: $found_type"
  echo ']' ;
fi

# we should have a skip_test function
echo 'test: subunit_skip_test exists'
found_type=$(type -t subunit_skip_test)
status=$?
if [ $status == 0 -a "x$found_type" = "xfunction" ]; then
  echo 'success: subunit_skip_test exists'
else
  echo 'failure: subunit_skip_test exists ['
  echo 'subunit_skip_test is not a function:'
  echo "type -t status: $status"
  echo "output: $found_type"
  echo ']' ;
fi

