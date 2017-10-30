#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget handles a 504 Gateway Timeout response
    correctly.
    Since, we do not have a direct mechanism for conditionally sending responses
    via the HTTP Server, I've used a workaround.
    The server will always respond to a request for File1 with a 504 Gateway
    Timeout. Using the --tries=2 option, we ensure that Wget attempts the file
    only twice and then move on to the next file. Finally, check the exact
    requests that the Server received and compare them, in order, to the
    expected sequence of requests.

    In this case, we expect Wget to attempt File1 twice and File2 once. If Wget
    considered 504 as a general Server Error, it would be a fatal failure and
    Wget would request File1 only once.
"""
############# File Definitions ###############################################
File1 = """All happy families are alike;
Each unhappy family is unhappy in its own way"""
File2 = "Anyone for chocochip cookies?"

File1_rules = {
    "Response"          : 504
}

A_File = WgetFile ("File1", File1, rules=File1_rules)
B_File = WgetFile ("File2", File2)

Request_List = [
    [
        "GET /File1",
        "GET /File1",
        "GET /File2",
    ]
]


WGET_OPTIONS = "--tries=2"
WGET_URLS = [["File1", "File2"]]

Files = [[A_File, B_File]]

ExpectedReturnCode = 4
ExpectedDownloadedFiles = [B_File]

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
    "ExpectedRetcode"   : ExpectedReturnCode,
    "FilesCrawled"      : Request_List
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
