#!/bin/sh

/usr/lib/rpm/perl.req $* | grep -E -v '(Net::LDAP|Crypt::SmbHash|CGI|Unicode::MapUTF8)'

