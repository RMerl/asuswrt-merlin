#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget correctly handles responses to HEAD requests
    and does not actually download any data
"""
############# File Definitions ###############################################
File1 = "You shall not pass!"

A_File = WgetFile ("File1", File1)

WGET_OPTIONS = "--method=HEAD"
WGET_URLS = [["File1"]]

Files = [[A_File]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = []

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files,
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
