from exc.test_failed import TestFailed
from conf import hook

""" Post-Test Hook: ExpectedRetCode
This is a post-test hook which checks if the exit code of the Wget instance
under test is the same as that expected. As a result, this is a very important
post test hook which is checked in all the tests.
Returns a TestFailed exception if the return code does not match the expected
value. Else returns gracefully.
"""


@hook(alias='ExpectedRetcode')
class ExpectedRetCode:
    def __init__(self, expected_ret_code):
        self.expected_ret_code = expected_ret_code

    def __call__(self, test_obj):
        if test_obj.ret_code != self.expected_ret_code:
            if test_obj.ret_code == 45:
                failure = "Memory Leak Found by Valgrind"
            else:
                failure = "Return codes do not match.\n" \
                          "Expected: %s\n" \
                          "Actual: %s" % (self.expected_ret_code,
                                          test_obj.ret_code)
            raise TestFailed(failure)
