from misc.colour_terminal import print_green
from server.http.http_server import HTTPd, HTTPSd
from test.base_test import BaseTest, HTTP, HTTPS


class HTTPTest(BaseTest):

    """ Class for HTTP Tests. """

    # Temp Notes: It is expected that when pre-hook functions are executed,
    # only an empty test-dir exists. pre-hook functions are executed just prior
    # to the call to Wget is made. post-hook functions will be executed
    # immediately after the call to Wget returns.

    def __init__(self,
                 pre_hook=None,
                 test_params=None,
                 post_hook=None,
                 protocols=(HTTP,),
                 req_protocols=None):
        super(HTTPTest, self).__init__(pre_hook,
                                       test_params,
                                       post_hook,
                                       protocols,
                                       req_protocols)

    def setup(self):
        self.server_setup()
        self.ready = True

    def begin(self):
        if not self.ready:
            # this is to maintain compatibility with scripts that
            # don't call setup()
            self.setup()
        with self:
            # If any exception occurs, self.__exit__ will be immediately called.
            # We must call the parent method in the end in order to verify
            # whether the tests succeeded or not.
            if self.ready:
                self.do_test()
                print_green("Test Passed.")
            else:
                self.tests_passed = False
            super(HTTPTest, self).begin()

    def instantiate_server_by(self, protocol):
        server = {HTTP: HTTPd,
                  HTTPS: HTTPSd}[protocol]()
        server.start()

        return server

    def request_remaining(self):
        return [s.server_inst.get_req_headers()
                for s in self.servers]

    def stop_server(self):
        for server in self.servers:
            server.server_inst.shutdown()
# vim: set ts=4 sts=4 sw=4 tw=80 et :
