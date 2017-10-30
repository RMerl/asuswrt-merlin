# Makefile for `wget' utility
# Copyright (C) 2013, 2015 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Wget.  If not, see <http://www.gnu.org/licenses/>.

# Additional permission under GNU GPL version 3 section 7

# If you modify this program, or any covered work, by linking or
# combining it with the OpenSSL project's OpenSSL library (or a
# modified version of that library), containing parts covered by the
# terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
# grants you additional permission to convey the resulting work.
# Corresponding Source for a non-source form of such a combination
# shall include the source code for the parts of OpenSSL used as well
# as that of the covered work.


if METALINK_IS_ENABLED
  METALINK_TESTS = Test-metalink-http.py            \
    Test-metalink-http-quoted.py                    \
    Test-metalink-http-baddigest.py                 \
    Test-metalink-http-xml.py                       \
    Test-metalink-http-xml-trust.py                 \
    Test-metalink-http-xml-trust-name.py            \
    Test-metalink-http-xml-type.py                  \
    Test-metalink-http-xml-type-trust.py            \
    Test-metalink-http-xml-type-content.py          \
    Test-metalink-http-xml-type-trust-content.py    \
    Test-metalink-xml.py                            \
    Test-metalink-xml-continue.py                   \
    Test-metalink-xml-relpath.py                    \
    Test-metalink-xml-abspath.py                    \
    Test-metalink-xml-homepath.py                   \
    Test-metalink-xml-trust.py                      \
    Test-metalink-xml-relpath-trust.py              \
    Test-metalink-xml-abspath-trust.py              \
    Test-metalink-xml-homepath-trust.py             \
    Test-metalink-xml-prefix.py                     \
    Test-metalink-xml-relprefix.py                  \
    Test-metalink-xml-absprefix.py                  \
    Test-metalink-xml-homeprefix.py                 \
    Test-metalink-xml-prefix-trust.py               \
    Test-metalink-xml-relprefix-trust.py            \
    Test-metalink-xml-absprefix-trust.py            \
    Test-metalink-xml-homeprefix-trust.py           \
    Test-metalink-xml-emptyprefix-trust.py          \
    Test-metalink-xml-size.py                       \
    Test-metalink-xml-nohash.py                     \
    Test-metalink-xml-nourls.py                     \
    Test-metalink-xml-urlbreak.py
else
  METALINK_TESTS =
endif

AUTOMAKE_OPTIONS = parallel-tests
AM_TESTS_ENVIRONMENT = export WGETRC=/dev/null; MAKE_CHECK=True; export MAKE_CHECK;\
 export PYTHONPATH=$$PYTHONPATH:$(srcdir); export VALGRIND_TESTS="@VALGRIND_TESTS@";

if WITH_SSL
  AM_TESTS_ENVIRONMENT += export SSL_TESTS=1;
endif

if HAVE_PYTHON3
  TESTS = Test-504.py                               \
    Test-auth-basic-fail.py                         \
    Test-auth-basic.py                              \
    Test-auth-basic-netrc.py                        \
    Test-auth-basic-netrc-user-given.py             \
    Test-auth-basic-netrc-pass-given.py             \
    Test-auth-basic-no-netrc-fail.py                \
    Test-auth-both.py                               \
    Test-auth-digest.py                             \
    Test-auth-no-challenge.py                       \
    Test-auth-no-challenge-url.py                   \
    Test-auth-retcode.py                            \
    Test-auth-with-content-disposition.py           \
    Test-c-full.py                                  \
    Test-condget.py                                 \
    Test-Content-disposition-2.py                   \
    Test-Content-disposition.py                     \
    Test--convert-links--content-on-error.py        \
    Test-cookie-401.py                              \
    Test-cookie-domain-mismatch.py                  \
    Test-cookie-expires.py                          \
    Test-cookie.py                                  \
    Test-Head.py                                    \
    Test-hsts.py                                    \
    Test--https.py                                  \
    Test--https-crl.py                              \
    Test-missing-scheme-retval.py                   \
    Test-O.py                                       \
    Test-pinnedpubkey-der-https.py                  \
    Test-pinnedpubkey-der-no-check-https.py         \
    Test-pinnedpubkey-hash-https.py                 \
    Test-pinnedpubkey-hash-no-check-fail-https.py   \
    Test-pinnedpubkey-pem-fail-https.py             \
    Test-pinnedpubkey-pem-https.py                  \
    Test-Post.py                                    \
    Test-recursive-basic.py                         \
    Test-recursive-include.py                       \
    Test-recursive-redirect.py                      \
    Test-redirect.py                                \
    Test-redirect-crash.py                          \
    Test--rejected-log.py                           \
    Test-reserved-chars.py                          \
    Test--spider-r.py                               \
    $(METALINK_TESTS)

endif

EXTRA_DIST = certs conf exc misc server test README $(TESTS)

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = python3
AM_PY_LOG_FLAGS = -O
