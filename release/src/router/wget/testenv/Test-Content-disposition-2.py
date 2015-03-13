#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget parses the Content-Disposition header
    correctly and creates the appropriate file when the said filename exists.
"""
TEST_NAME = "Content Disposition Clobber"
############# File Definitions ###############################################
File1 = "Teapot"
File2 = "The Teapot Protocol"

File2_rules = {
    "SendHeader"        : {
        "Content-Disposition" : "Attachment; filename=HTTP.Teapot"
    }
}
A_File = WgetFile ("HTTP.Teapot", File1)
B_File = WgetFile ("File2", File2, rules=File2_rules)

WGET_OPTIONS = "--content-disposition"
WGET_URLS = [["File2"]]

Files = [[B_File]]
Existing_Files = [A_File]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [WgetFile ("HTTP.Teapot.1", File2), A_File]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files,
    "LocalFiles"        : Existing_Files
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
