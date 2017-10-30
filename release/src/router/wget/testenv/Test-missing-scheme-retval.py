#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from test.base_test import HTTP
from misc.wget_file import WgetFile
import os

"""
    This test ensures that Wget complains about missing scheme
"""
############# File Definitions ###############################################
A_File = WgetFile ("bar", 'Content')

# put the URL into 'options' to avoid prepending scheme/localhost/port
WGET_OPTIONS = "/foo/bar"
WGET_URLS = [[]]

Files = [[A_File]]

ExpectedReturnCode = 1
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
                post_hook=post_test,
).begin ()

exit (err)
