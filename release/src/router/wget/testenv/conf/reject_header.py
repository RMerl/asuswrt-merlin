from conf import rule

""" Rule: RejectHeader
This is a server side rule which expects a dictionary object of Headers and
their values which should be blacklisted by the server for a particular file's
requests.
"""


@rule()
class RejectHeader:
    def __init__(self, header_obj):
        self.headers = header_obj
