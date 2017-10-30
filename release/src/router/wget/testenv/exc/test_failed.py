
class TestFailed(Exception):

    """ A Custom Exception raised by the Test Environment. """

    def __init__(self, error):
        self.error = error
