from conf import rule

""" Rule: Response
When this rule is set against a certain file, the server will unconditionally
respond to any request for the said file with the provided response code. """


@rule()
class Response:
    def __init__(self, ret_code):
        self.response_code = ret_code
