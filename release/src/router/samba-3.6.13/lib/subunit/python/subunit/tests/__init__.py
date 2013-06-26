#
#  subunit: extensions to python unittest to get test results from subprocesses.
#  Copyright (C) 2005  Robert Collins <robertc@robertcollins.net>
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

from subunit.tests import (
    TestUtil,
    test_chunked,
    test_details,
    test_progress_model,
    test_subunit_filter,
    test_subunit_stats,
    test_subunit_tags,
    test_tap2subunit,
    test_test_protocol,
    test_test_results,
    )

def test_suite():
    result = TestUtil.TestSuite()
    result.addTest(test_chunked.test_suite())
    result.addTest(test_details.test_suite())
    result.addTest(test_progress_model.test_suite())
    result.addTest(test_test_results.test_suite())
    result.addTest(test_test_protocol.test_suite())
    result.addTest(test_tap2subunit.test_suite())
    result.addTest(test_subunit_filter.test_suite())
    result.addTest(test_subunit_tags.test_suite())
    result.addTest(test_subunit_stats.test_suite())
    return result
