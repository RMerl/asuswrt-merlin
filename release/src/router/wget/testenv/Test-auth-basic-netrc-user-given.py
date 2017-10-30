#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures Wget uses credentials from .netrc for Basic Authorization Negotiation.
    In this case we test that .netrc credentials are used in case only
    user login is given on the command line.
    Also, we ensure that Wget saves the host after a successful auth and
    doesn't wait for a challenge the second time.
"""
############# File Definitions ###############################################
File1 = "I am an invisble man."
File2 = "I too am an invisible man."

User = "Sauron"
Password = "TheEye"

File1_rules = {
    "Authentication"    : {
        "Type"          : "Basic",
        "User"          : User,
        "Pass"          : Password
    }
}
File2_rules = {
    "ExpectHeader"      : {
        "Authorization" : "Basic U2F1cm9uOlRoZUV5ZQ=="
    }
}

Netrc = "machine localhost\n\tlogin {0}\n\tpassword {1}".format(User, Password)

A_File = WgetFile ("File1", File1, rules=File1_rules)
B_File = WgetFile ("File2", File2, rules=File2_rules)
Netrc_File = WgetFile (".netrc", Netrc)

WGET_OPTIONS = "--user={0}".format(User)
WGET_URLS = [["File1", "File2"]]

Files = [[A_File, B_File]]
LocalFiles = [Netrc_File]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [A_File, B_File, Netrc_File]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files,
    "LocalFiles"        : LocalFiles
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
