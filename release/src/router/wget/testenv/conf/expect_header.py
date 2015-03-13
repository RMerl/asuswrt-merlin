from conf import rule

""" Rule: ExpectHeader
This rule defines a dictionary of headers and their value which the server
should expect in each request for the file to which the rule was applied.
"""


@rule()
class ExpectHeader:
    def __init__(self, header_obj):
        self.headers = header_obj
