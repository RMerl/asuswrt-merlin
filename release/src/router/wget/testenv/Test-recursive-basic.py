#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from test.base_test import HTTP, HTTPS
from misc.wget_file import WgetFile

"""
    Basic test of --recursive.
"""
############# File Definitions ###############################################
File1 = """<html><body>
<a href=\"/a/File2.html\">text</a>
<a href=\"/b/File3.html\">text</a>
</body></html>"""
File2 = "With lemon or cream?"
File3 = "Surely you're joking Mr. Feynman"

File1_File = WgetFile ("a/File1.html", File1)
File2_File = WgetFile ("a/File2.html", File2)
File3_File = WgetFile ("b/File3.html", File3)

WGET_OPTIONS = "--recursive --no-host-directories"
WGET_URLS = [["a/File1.html"]]

Servers = [HTTP]

Files = [[File1_File, File2_File, File3_File]]
Existing_Files = []

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [File1_File, File2_File, File3_File]
Request_List = [["GET /a/File1.html",
                 "GET /robots.txt",
                 "GET /a/File2.html",
                 "GET /b/File3.html"]]

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
    "ExpectedRetcode"   : ExpectedReturnCode,
    "FilesCrawled"      : Request_List
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test,
                protocols=Servers
).begin ()

exit (err)
