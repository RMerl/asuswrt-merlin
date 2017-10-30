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

File1_rules = {
    "Response"          : 301,
    "SendHeader"        : {"Location" : "/b/File1.html"}
}

File1_File = WgetFile ("a/File1.html", "", rules=File1_rules)
File1_Redirected = WgetFile ("b/File1.html", File1)
File1_Retrieved = WgetFile ("a/File1.html", File1)
File2_File = WgetFile ("a/File2.html", File2)
File3_File = WgetFile ("b/File3.html", File3)

WGET_OPTIONS = "--recursive --no-host-directories --include-directories=a"
WGET_URLS = [["a/File1.html"]]

Servers = [HTTP]

Files = [[File1_Redirected, File1_File, File2_File, File3_File]]
Existing_Files = []

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [File1_Retrieved, File2_File]
Request_List = [["GET /a/File1.html",
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
    "ExpectedRetcode"   : ExpectedReturnCode
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test,
                protocols=Servers
).begin ()

exit (err)
