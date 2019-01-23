from exc.test_failed import TestFailed
from conf import hook

""" Hook: SampleHook
This a sample file for how a new hook should be defined.
Any errors should always be reported by raising a TestFailed exception instead
of returning a true or false value.
"""


@hook(alias='SampleHookAlias')
class SampleHook:
    def __init__(self, sample_hook_arg):
        # do conf initialization here
        self.arg = sample_hook_arg

    def __call__(self, test_obj):
        # implement hook here
        # if you need the test case instance, refer to test_obj
        if False:
            raise TestFailed("Reason")
        pass
