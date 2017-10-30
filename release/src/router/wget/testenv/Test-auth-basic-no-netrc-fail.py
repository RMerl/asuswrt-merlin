#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget will not use credentials from .netrc
    when --no-netrc option is specified and Basic authentication is required
    and fails.
"""
############# File Definitions ###############################################
File1 = "I am an invisble man."

User = "Sauron"
Password = "TheEye"

File1_rules = {
    "Authentication"    : {
        "Type"          : "Basic",
        "User"          : User,
        "Pass"          : Password
    }
}

Netrc = "machine 127.0.0.1\n\tlogin {0}\n\tpassword {1}".format(User, Password)

A_File = WgetFile ("File1", File1, rules=File1_rules)
Netrc_File = WgetFile (".netrc", Netrc)

WGET_OPTIONS = "--no-netrc"
WGET_URLS = [["File1"]]

Files = [[A_File]]
LocalFiles = [Netrc_File]

ExpectedReturnCode = 6
ExpectedDownloadedFiles = [Netrc_File]

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
