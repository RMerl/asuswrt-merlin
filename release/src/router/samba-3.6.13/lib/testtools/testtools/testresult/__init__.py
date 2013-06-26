# Copyright (c) 2009 Jonathan M. Lange. See LICENSE for details.

"""Test result objects."""

__all__ = [
    'ExtendedToOriginalDecorator',
    'MultiTestResult',
    'TestResult',
    'TextTestResult',
    'ThreadsafeForwardingResult',
    ]

from testtools.testresult.real import (
    ExtendedToOriginalDecorator,
    MultiTestResult,
    TestResult,
    TextTestResult,
    ThreadsafeForwardingResult,
    )
