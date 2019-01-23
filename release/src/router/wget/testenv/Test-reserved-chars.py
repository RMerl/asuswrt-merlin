#!/usr/bin/env python3
from sys import exit
from os import environ # to set LC_ALL
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
This test ensures that Wget keeps reserved characters in URLs in non-UTF-8 charsets.
"""
# This bug only happened with ASCII charset,
# so we need to set LC_ALL="C" in order to reproduce it.
environ["LC_ALL"] = "C"

######### File Definitions #########
RequestList = [
    [
        "HEAD /base.html",
        "GET /base.html",
        "GET /robots.txt",
        "HEAD /a%2Bb.html",
        "GET /a%2Bb.html"
    ]
]
A_File_Name = "base.html"
B_File_Name = "a%2Bb.html"
A_File = WgetFile (A_File_Name, "<a href=\"a%2Bb.html\">")
B_File = WgetFile (B_File_Name, "this is file B")

WGET_OPTIONS = " --spider -r"
WGET_URLS = [[A_File_Name]]

Files = [[A_File, B_File]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = []

######### Pre and Post Test Hooks #########
pre_test = {
    "ServerFiles"   : Files
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode,
    "FilesCrawled"      : RequestList
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
