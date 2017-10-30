#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget identifies bad servers trying to set cookies
    for a different domain and rejects them.
"""
############# File Definitions ###############################################
File1 = "Would you care for a cup of coffee?"
File2 = "Anyone for chocochip cookies?"

File1_rules = {
    "SendHeader"        : {
        # use upper case 'I' to provoke Wget failure with turkish locale
        "Set-Cookie"    : "sess-id=0213; path=/; DoMAIn=.example.com"
    }
}
File2_rules = {
    "RejectHeader"      : {
        "Cookie"        : "sess-id=0213"
    }
}

A_File = WgetFile ("File1", File1, rules=File1_rules)
B_File = WgetFile ("File2", File2, rules=File2_rules)

WGET_OPTIONS = ""
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
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
