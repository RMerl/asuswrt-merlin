#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget returns the correct exit code when Basic
    authentcation failes due to a username/password error.
"""
TEST_NAME = "Basic Authentication Failure"
############# File Definitions ###############################################
File1 = "I am an invisble man."

File1_rules = {
    "Authentication"    : {
        "Type"          : "Basic",
        "User"          : "Sauron",
        "Pass"          : "TheEye"
    }
}
A_File = WgetFile ("File1", File1, rules=File1_rules)

WGET_OPTIONS = "--user=Sauron --password=Eye"
WGET_URLS = [["File1"]]

Files = [[A_File]]

ExpectedReturnCode = 6
ExpectedDownloadedFiles = []

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
