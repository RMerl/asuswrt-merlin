#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from test.base_test import HTTP, HTTPS
from misc.wget_file import WgetFile

"""
    This is a Prototype Test File.
    Ideally this File should be copied and edited to write new tests.
"""
############# File Definitions ###############################################
File2 = "Would you like some Tea?"

File1_rules = {
    "Response"          : 301,
    "SendHeader"        : {"Location" : "/File2.txt"}
}

# /File1.txt is only a redirect, and so has no file content.
File1_File = WgetFile ("File1.txt", "", rules=File1_rules)
# File1_Retrieved is what will be retrieved for URL /File1.txt.
File1_Retrieved = WgetFile ("File1.txt", File2)
File2_File = WgetFile ("File2.txt", File2)

WGET_OPTIONS = ""
WGET_URLS = [["File1.txt"]]

Servers = [HTTP]

Files = [[File1_File, File2_File]]
Existing_Files = []

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [File1_Retrieved]

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
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test,
                protocols=Servers
).begin ()

exit (err)
