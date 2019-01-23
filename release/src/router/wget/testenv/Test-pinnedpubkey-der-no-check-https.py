#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from test.base_test import HTTP, HTTPS
from misc.wget_file import WgetFile
import os

"""
    This test ensures that Wget can download files from HTTPS Servers
"""
if os.getenv('SSL_TESTS') is None:
    exit (77)

############# File Definitions ###############################################
File1 = "Would you like some Tea?"
File2 = "With lemon or cream?"
File3 = "Sure you're joking Mr. Feynman"

A_File = WgetFile ("File1", File1)
B_File = WgetFile ("File2", File2)
C_File = WgetFile ("File3", File3)

PINNEDPUBKEY = os.path.abspath(os.path.join(os.getenv('srcdir', '.'), 'certs', 'server-pubkey.der'))
WGET_OPTIONS = "--no-check-certificate --pinnedpubkey=" + PINNEDPUBKEY
WGET_URLS = [["File1", "File2"]]

Files = [[A_File, B_File]]
Existing_Files = [C_File]

Servers = [HTTPS]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [A_File, B_File, C_File]

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
