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


# this script tests the output of the methods. As each is tested we start using
# it.
# So the first test manually implements the entire protocol, the next uses the
# start method and so on.
# it is assumed that we are running from the 'shell' tree root in the source
# of subunit, and that the library sourcing tests have all passed - if they 
# have not, this test script may well fail strangely.

# import the library.
. ${SHELL_SHARE}subunit.sh

echo 'test: subunit_start_test output'
func_output=$(subunit_start_test "foo bar")
func_status=$?
if [ $func_status == 0 -a "x$func_output" = "xtest: foo bar" ]; then
  echo 'success: subunit_start_test output'
else
  echo 'failure: subunit_start_test output ['
  echo 'got an error code or incorrect output:'
  echo "exit: $func_status"
  echo "output: '$func_output'"
  echo ']' ;
fi

subunit_start_test "subunit_pass_test output"
func_output=$(subunit_pass_test "foo bar")
func_status=$?
if [ $func_status == 0 -a "x$func_output" = "xsuccess: foo bar" ]; then
  subunit_pass_test "subunit_pass_test output"
else
  echo 'failure: subunit_pass_test output ['
  echo 'got an error code or incorrect output:'
  echo "exit: $func_status"
  echo "output: '$func_output'"
  echo ']' ;
fi

subunit_start_test "subunit_fail_test output"
func_output=$(subunit_fail_test "foo bar" <<END
something
  wrong
here
END
)
func_status=$?
if [ $func_status == 0 -a "x$func_output" = "xfailure: foo bar [
something
  wrong
here
]" ]; then
  subunit_pass_test "subunit_fail_test output"
else
  echo 'failure: subunit_fail_test output ['
  echo 'got an error code or incorrect output:'
  echo "exit: $func_status"
  echo "output: '$func_output'"
  echo ']' ;
fi

subunit_start_test "subunit_error_test output"
func_output=$(subunit_error_test "foo bar" <<END
something
  died
here
END
)
func_status=$?
if [ $func_status == 0 -a "x$func_output" = "xerror: foo bar [
something
  died
here
]" ]; then
  subunit_pass_test "subunit_error_test output"
else
  subunit_fail_test "subunit_error_test output" <<END
got an error code or incorrect output:
exit: $func_status
output: '$func_output'
END
fi
