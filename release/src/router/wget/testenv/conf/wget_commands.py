from conf import hook

""" Pre-Test Hook: WgetCommands
This hook is used to specify the test specific switches that must be passed to
wget on invocation. Default switches are hard coded in the test suite itself.
"""


@hook()
class WgetCommands:
    def __init__(self, commands):
        self.commands = commands

    def __call__(self, test_obj):
        test_obj.wget_options = test_obj._replace_substring(self.commands)
