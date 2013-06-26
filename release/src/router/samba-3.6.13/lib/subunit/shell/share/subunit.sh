#
#  subunit.sh: shell functions to report test status via the subunit protocol.
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

subunit_start_test () {
  # emit the current protocol start-marker for test $1
  echo "test: $1"
}


subunit_pass_test () {
  # emit the current protocol test passed marker for test $1
  echo "success: $1"
}


subunit_fail_test () {
  # emit the current protocol fail-marker for test $1, and emit stdin as
  # the error text.
  # we use stdin because the failure message can be arbitrarily long, and this
  # makes it convenient to write in scripts (using <<END syntax.
  echo "failure: $1 ["
  cat -
  echo "]"
}


subunit_error_test () {
  # emit the current protocol error-marker for test $1, and emit stdin as
  # the error text.
  # we use stdin because the failure message can be arbitrarily long, and this
  # makes it convenient to write in scripts (using <<END syntax.
  echo "error: $1 ["
  cat -
  echo "]"
}


subunit_skip_test () {
  # emit the current protocol test skipped marker for test $1
  echo "skip: $1"
}


