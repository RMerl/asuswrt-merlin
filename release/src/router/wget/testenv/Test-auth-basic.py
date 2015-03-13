#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures Wget's Basic Authorization Negotiation.
    Also, we ensure that Wget saves the host after a successfull auth and
    doesn't wait for a challenge the second time.
"""
TEST_NAME = "Basic Authorization"
############# File Definitions ###############################################
File1 = "I am an invisble man."
File2 = "I too am an invisible man."

File1_rules = {
    "Authentication"    : {
        "Type"          : "Basic",
        "User"          : "Sauron",
        "Pass"          : "TheEye"
    }
}
File2_rules = {
    "ExpectHeader"      : {
        "Authorization" : "Basic U2F1cm9uOlRoZUV5ZQ=="
    }
}
A_File = WgetFile ("File1", File1, rules=File1_rules)
B_File = WgetFile ("File2", File2, rules=File2_rules)

WGET_OPTIONS = "--user=Sauron --password=TheEye"
WGET_URLS = [["File1", "File2"]]

Files = [[A_File, B_File]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [A_File, B_File]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files
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
                name=TEST_NAME,
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
