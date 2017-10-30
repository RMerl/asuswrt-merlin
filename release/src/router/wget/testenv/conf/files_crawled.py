from misc.colour_terminal import print_red
from conf import hook
from exc.test_failed import TestFailed

""" Post-Test Hook: FilesCrawled
This is a post test hook that is invoked in tests that check wget's behaviour
in recursive mode. It expects an ordered list of the request lines that Wget
must send to the server. If the requests received by the server do not match
the provided list, IN THE GIVEN ORDER, then it raises a TestFailed exception.
Such a test can be used to check the implementation of the recursion algorithm
in Wget too.
"""


@hook()
class FilesCrawled:
    def __init__(self, request_headers):
        self.request_headers = request_headers

    def __call__(self, test_obj):
        for headers, remaining in zip(map(set, self.request_headers),
                                      test_obj.request_remaining()):
            diff = headers.symmetric_difference(remaining)

            if diff:
                print_red(str(diff))
                raise TestFailed('Not all files were crawled correctly.')
