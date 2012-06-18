#!/usr/bin/python

import sys
import dcerpc, samr

def test_Connect(pipe):

    handle = samr.Connect(pipe)
    handle = samr.Connect2(pipe)
    handle = samr.Connect3(pipe)
    handle = samr.Connect4(pipe)

    # WIN2K3 only?
    
    try:
        handle = samr.Connect5(pipe)
    except dcerpc.NTSTATUS, arg:
        if arg[0] != 0xc00000d2L:       # NT_STATUS_NET_WRITE_FAULT
            raise

    return handle
    
def test_UserHandle(user_handle):

    # QuerySecurity()/SetSecurity()

    user_handle.SetSecurity(user_handle.QuerySecurity())

    # GetUserPwInfo()

    user_handle.GetUserPwInfo()

    # GetUserInfo()

    for level in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 20,
                  21, 23, 24, 25, 26]:

        try:
            user_handle.QueryUserInfo(level)
            user_handle.QueryUserInfo2(level)
        except dcerpc.NTSTATUS, arg:
            if arg[0] != 0xc0000003L:   # NT_STATUS_INVALID_INFO_CLASS
                raise

    # GetGroupsForUser()

    user_handle.GetGroupsForUser()

    # TestPrivateFunctionsUser()

    try:
        user_handle.TestPrivateFunctionsUser()
    except dcerpc.NTSTATUS, arg:
        if arg[0] != 0xC0000002L:
            raise

def test_GroupHandle(group_handle):

    # QuerySecurity()/SetSecurity()

    group_handle.SetSecurity(group_handle.QuerySecurity())

    # QueryGroupInfo()

    for level in [1, 2, 3, 4, 5]:
        info = group_handle.QueryGroupInfo(level)

    # TODO: SetGroupinfo()

    # QueryGroupMember()

    group_handle.QueryGroupMember()

def test_AliasHandle(alias_handle):

    # QuerySecurity()/SetSecurity()

    alias_handle.SetSecurity(alias_handle.QuerySecurity())

    print alias_handle.GetMembersInAlias()

def test_DomainHandle(name, sid, domain_handle):

    print 'testing %s (%s)' % (name, sid)

    # QuerySecurity()/SetSecurity()

    domain_handle.SetSecurity(domain_handle.QuerySecurity())

    # LookupNames(), none mapped

    try:
        domain_handle.LookupNames(['xxNONAMExx'])
    except dcerpc.NTSTATUS, arg:
        if arg[0] != 0xc0000073L:
            raise dcerpc.NTSTATUS(arg)

    # LookupNames(), some mapped

    if name != 'Builtin':
        domain_handle.LookupNames(['Administrator', 'xxNONAMExx'])

    # QueryDomainInfo()/SetDomainInfo()

    levels = [1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13]
    set_ok = [1, 0, 1, 1, 0, 1, 1, 0, 1,  0,  1,  0]
    
    for i in range(len(levels)):

        info = domain_handle.QueryDomainInfo(level = levels[i])

        try:
            domain_handle.SetDomainInfo(levels[i], info)
        except dcerpc.NTSTATUS, arg:
            if not (arg[0] == 0xc0000003L and not set_ok[i]):
                raise

    # QueryDomainInfo2()

    levels = [1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13]

    for i in range(len(levels)):
        domain_handle.QueryDomainInfo2(level = levels[i])

    # EnumDomainUsers

    print 'testing users'

    users = domain_handle.EnumDomainUsers()
    rids = domain_handle.LookupNames(users)
    
    for i in range(len(users)):
        test_UserHandle(domain_handle.OpenUser(rids[0][i]))
    
    # QueryDisplayInfo

    for i in [1, 2, 3, 4, 5]:
        domain_handle.QueryDisplayInfo(level = i)
        domain_handle.QueryDisplayInfo2(level = i)
        domain_handle.QueryDisplayInfo3(level = i)
            
    # EnumDomainGroups

    print 'testing groups'

    groups = domain_handle.EnumDomainGroups()
    rids = domain_handle.LookupNames(groups)

    for i in range(len(groups)):
        test_GroupHandle(domain_handle.OpenGroup(rids[0][i]))

    # EnumDomainAliases

    print 'testing aliases'

    aliases = domain_handle.EnumDomainAliases()
    rids = domain_handle.LookupNames(aliases)

    for i in range(len(aliases)):
        test_AliasHandle(domain_handle.OpenAlias(rids[0][i]))
    
    # CreateUser
    # CreateUser2
    # CreateDomAlias
    # RidToSid
    # RemoveMemberFromForeignDomain
    # CreateDomainGroup
    # GetAliasMembership

    # GetBootKeyInformation()

    try:
        domain_handle.GetBootKeyInformation()
    except dcerpc.NTSTATUS, arg:
        pass

    # TestPrivateFunctionsDomain()

    try:
        domain_handle.TestPrivateFunctionsDomain()
    except dcerpc.NTSTATUS, arg:
        if arg[0] != 0xC0000002L:
            raise

def test_ConnectHandle(connect_handle):

    print 'testing connect handle'

    # QuerySecurity/SetSecurity

    connect_handle.SetSecurity(connect_handle.QuerySecurity())

    # Lookup bogus domain

    try:
        connect_handle.LookupDomain('xxNODOMAINxx')
    except dcerpc.NTSTATUS, arg:
        if arg[0] != 0xC00000DFL:          # NT_STATUS_NO_SUCH_DOMAIN
            raise

    # Test all domains

    for domain_name in connect_handle.EnumDomains():

        connect_handle.GetDomPwInfo(domain_name)
        sid = connect_handle.LookupDomain(domain_name)
        domain_handle = connect_handle.OpenDomain(sid)

        test_DomainHandle(domain_name, sid, domain_handle)

    # TODO: Test Shutdown() function

def runtests(binding, creds):

    print 'Testing SAMR pipe'

    pipe = dcerpc.pipe_connect(binding,
            dcerpc.DCERPC_SAMR_UUID, int(dcerpc.DCERPC_SAMR_VERSION), creds)

    handle = test_Connect(pipe)
    test_ConnectHandle(handle)
