/* 
   Dummy ACL tests
   Copyright (C) 2001-2003, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "ne_acl.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

/**** DUMMY TESTS: just makes sure the stuff doesn't dump core. */

static int test_acl(const char *uri, ne_acl_entry *es, int nume)
{
    ne_session *sess;

    CALL(make_session(&sess, single_serve_string, 
		      "HTTP/1.1 200 OK\r\n"
		      "Connection: close\r\n\r\n"));
    
    ON(ne_acl_set(sess, uri, es, nume));
    
    CALL(await_server());
    ne_session_destroy(sess);
    
    return OK;
}

static int grant_all(void)
{
    ne_acl_entry e = {0};

    e.apply = ne_acl_all;
    e.type = ne_acl_grant;

    CALL(test_acl("/foo", &e, 1));

    return OK;
}

static int deny_all(void)
{
    ne_acl_entry e = {0};

    e.apply = ne_acl_all;
    e.type = ne_acl_deny;
    
    CALL(test_acl("/foo", &e, 1));

    return OK;
}

static int deny_one(void)
{
    ne_acl_entry e = {0};

    e.apply = ne_acl_href;
    e.type = ne_acl_deny;
    e.principal = "http://webdav.org/users/joe";

    CALL(test_acl("/foo", &e, 1));

    return OK;
}       

static int deny_byprop(void)
{
    ne_acl_entry e = {0};

    e.apply = ne_acl_property;
    e.type = ne_acl_deny;
    e.principal = "owner";

    CALL(test_acl("/foo", &e, 1));

    return OK;
}

ne_test tests[] = {
    T(grant_all),
    T(deny_all),
    T(deny_one),
    T(deny_byprop),
    T(NULL)
};
