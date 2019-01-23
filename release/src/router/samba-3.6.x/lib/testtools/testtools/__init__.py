# Copyright (c) 2008, 2009, 2010 Jonathan M. Lange. See LICENSE for details.

"""Extensions to the standard Python unittest library."""

__all__ = [
    'clone_test_with_new_id',
    'ConcurrentTestSuite',
    'ErrorHolder',
    'ExtendedToOriginalDecorator',
    'iterate_tests',
    'MultipleExceptions',
    'MultiTestResult',
    'PlaceHolder',
    'run_test_with',
    'TestCase',
    'TestResult',
    'TextTestResult',
    'RunTest',
    'skip',
    'skipIf',
    'skipUnless',
    'ThreadsafeForwardingResult',
    'try_import',
    'try_imports',
    ]

from testtools.helpers import (
    try_import,
    try_imports,
    )
from testtools.matchers import (
    Matcher,
    )
from testtools.runtest import (
    MultipleExceptions,
    RunTest,
    )
from testtools.testcase import (
    ErrorHolder,
    PlaceHolder,
    TestCase,
    clone_test_with_new_id,
    run_test_with,
    skip,
    skipIf,
    skipUnless,
    )
from testtools.testresult import (
    ExtendedToOriginalDecorator,
    MultiTestResult,
    TestResult,
    TextTestResult,
    ThreadsafeForwardingResult,
    )
from testtools.testsuite import (
    ConcurrentTestSuite,
    iterate_tests,
    )

# same format as sys.version_info: "A tuple containing the five components of
# the version number: major, minor, micro, releaselevel, and serial. All
# values except releaselevel are integers; the release level is 'alpha',
# 'beta', 'candidate', or 'final'. The version_info value corresponding to the
# Python version 2.0 is (2, 0, 0, 'final', 0)."  Additionally we use a
# releaselevel of 'dev' for unreleased under-development code.
#
# If the releaselevel is 'alpha' then the major/minor/micro components are not
# established at this point, and setup.py will use a version of next-$(revno).
# If the releaselevel is 'final', then the tarball will be major.minor.micro.
# Otherwise it is major.minor.micro~$(revno).

__version__ = (0, 9, 9, 'dev', 0)
