#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    Simple test for HTTP POST Requests usiong the --method command
"""
############# File Definitions ###############################################
File1 = """A reader lives a thousand lives before he dies, said Jojen.
The man who never reads lives only one"""

File1_response = """A reader lives a thousand lives before he dies, said Jojen.
The man who never reads lives only one
TestMessage"""

A_File = WgetFile ("File1", File1)

WGET_OPTIONS = "--method=post --body-data=TestMessage"
WGET_URLS = [["File1"]]

Files = [[A_File]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [WgetFile ("File1", File1_response)]

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
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
