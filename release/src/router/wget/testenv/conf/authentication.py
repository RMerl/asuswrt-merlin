from conf import rule

""" Rule: Authentication
This file defines an authentication rule which when applied to any file will
cause the server to prompt the client for the required authentication details
before serving it.
auth_type must be either of: Basic, Digest, Both or Both-inline
When auth_type is Basic or Digest, the server asks for the respective
authentication in its response. When auth_type is Both, the server sends two
Authenticate headers, one requesting Basic and the other requesting Digest
authentication. If auth_type is Both-inline, the server sends only one
Authenticate header, but lists both Basic and Digest as supported mechanisms in
that.
"""


@rule()
class Authentication:
    def __init__(self, auth_obj):
        self.auth_type = auth_obj['Type']
        self.auth_user = auth_obj['User']
        self.auth_pass = auth_obj['Pass']
        self.auth_parm = auth_obj.get('Parm', None)
