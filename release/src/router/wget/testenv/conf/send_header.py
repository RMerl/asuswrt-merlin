from conf import rule

""" Rule: SendHeader
Have the server send custom headers when responding to a request for the file
this rule is applied to. The header_obj object is expected to be dictionary
mapping headers to their contents. """


@rule()
class SendHeader:
    def __init__(self, header_obj):
        self.headers = header_obj
