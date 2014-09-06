""" Runs all unit tests for the netsnmp package.   """
# Copyright (c) 2006 Andy Gross.  See LICENSE.txt for details.

import sys
import unittest
import netsnmp
import time

class BasicTests(unittest.TestCase):
    def testFuncs(self):        
        print ""
        var = netsnmp.Varbind('sysDescr.0')
        var = netsnmp.Varbind('sysDescr','0')
        var = netsnmp.Varbind(
            '.iso.org.dod.internet.mgmt.mib-2.system.sysDescr','0')
        var = netsnmp.Varbind(
            '.iso.org.dod.internet.mgmt.mib-2.system.sysDescr.0')
        var = netsnmp.Varbind('.1.3.6.1.2.1.1.1.0')

        var = netsnmp.Varbind('.1.3.6.1.2.1.1.1','0')

        print "---v1 GET tests -------------------------------------\n"
        res = netsnmp.snmpget(var,
                              Version = 1,
                              DestHost='localhost',
                              Community='public')

        print "v1 snmpget result: ", res, "\n"

        print "v1 get var: ",  var.tag, var.iid, "=", var.val, '(',var.type,')'
        
        print "---v1 GETNEXT tests-------------------------------------\n"
        res = netsnmp.snmpgetnext(var,
                                  Version = 1,
                                  DestHost='localhost',
                                  Community='public')

        print "v1 snmpgetnext result: ", res, "\n"
                
        print "v1 getnext var: ",  var.tag, var.iid, "=", var.val, '(',var.type,')'
        
        print "---v1 SET tests-------------------------------------\n"
        var = netsnmp.Varbind('sysLocation','0', 'my new location')
        res = netsnmp.snmpset(var,
                        Version = 1,
                        DestHost='localhost',
                        Community='public')

        print "v1 snmpset result: ", res, "\n"

        print "v1 set var: ",  var.tag, var.iid, "=", var.val, '(',var.type,')'
        
        print "---v1 walk tests-------------------------------------\n"
        vars = netsnmp.VarList(netsnmp.Varbind('system'))

        print "v1 varlist walk in: "
        for var in vars:
            print "  ",var.tag, var.iid, "=", var.val, '(',var.type,')'

        res = netsnmp.snmpwalk(vars,
                               Version = 1,
                               DestHost='localhost',
                               Community='public')
        print "v1 snmpwalk result: ", res, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
       
        
        print "---v1 walk 2-------------------------------------\n"

        print "v1 varbind walk in: "
        var = netsnmp.Varbind('system')
        res = netsnmp.snmpwalk(var,
                               Version = 1,
                               DestHost='localhost',
                               Community='public')
        print "v1 snmpwalk result (should be = orig): ", res, "\n"

        print var.tag, var.iid, "=", var.val, '(',var.type,')'
        
        print "---v1 multi-varbind test-------------------------------------\n"
        sess = netsnmp.Session(Version=1,
                               DestHost='localhost',
                               Community='public')
        
        vars = netsnmp.VarList(netsnmp.Varbind('sysUpTime', 0),
                               netsnmp.Varbind('sysContact', 0),
                               netsnmp.Varbind('sysLocation', 0))
        vals = sess.get(vars)
        print "v1 sess.get result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
       
        vals = sess.getnext(vars)
        print "v1 sess.getnext result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
       
        vars = netsnmp.VarList(netsnmp.Varbind('sysUpTime'),
                               netsnmp.Varbind('sysORLastChange'),
                               netsnmp.Varbind('sysORID'),
                               netsnmp.Varbind('sysORDescr'),
                               netsnmp.Varbind('sysORUpTime'))

        vals = sess.getbulk(2, 8, vars)
        print "v1 sess.getbulk result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'

        print "---v1 set2-------------------------------------\n"

        vars = netsnmp.VarList(
            netsnmp.Varbind('sysLocation', '0', 'my newer location'))
        res = sess.set(vars)
        print "v1 sess.set result: ", res, "\n"

        print "---v1 walk3-------------------------------------\n"
        vars = netsnmp.VarList(netsnmp.Varbind('system'))
                
        vals = sess.walk(vars)
        print "v1 sess.walk result: ", vals, "\n"
        
        for var in vars:
            print "  ",var.tag, var.iid, "=", var.val, '(',var.type,')'
            
        print "---v2c get-------------------------------------\n"

        sess = netsnmp.Session(Version=2,
                               DestHost='localhost',
                               Community='public')

        sess.UseEnums = 1
        sess.UseLongNames = 1
        
        vars = netsnmp.VarList(netsnmp.Varbind('sysUpTime', 0),
                               netsnmp.Varbind('sysContact', 0),
                               netsnmp.Varbind('sysLocation', 0))
        vals = sess.get(vars)
        print "v2 sess.get result: ", vals, "\n"

        print "---v2c getnext-------------------------------------\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"
       
        vals = sess.getnext(vars)
        print "v2 sess.getnext result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"
       
        print "---v2c getbulk-------------------------------------\n"

        vars = netsnmp.VarList(netsnmp.Varbind('sysUpTime'),
                               netsnmp.Varbind('sysORLastChange'),
                               netsnmp.Varbind('sysORID'),
                               netsnmp.Varbind('sysORDescr'),
                               netsnmp.Varbind('sysORUpTime'))

        vals = sess.getbulk(2, 8, vars)
        print "v2 sess.getbulk result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"

        print "---v2c set-------------------------------------\n"

        vars = netsnmp.VarList(
            netsnmp.Varbind('sysLocation','0','my even newer location'))
        
        res = sess.set(vars)
        print "v2 sess.set result: ", res, "\n"

        print "---v2c walk-------------------------------------\n"
        vars = netsnmp.VarList(netsnmp.Varbind('system'))
                
        vals = sess.walk(vars)
        print "v2 sess.walk result: ", vals, "\n"
        
        for var in vars:
            print "  ",var.tag, var.iid, "=", var.val, '(',var.type,')'
            
        print "---v3 setup-------------------------------------\n"
        sess = netsnmp.Session(Version=3,
                               DestHost='localhost',
                               SecLevel='authPriv',
                               SecName='initial',
                               PrivPass='priv_pass',
                               AuthPass='auth_pass')

        sess.UseSprintValue = 1

        vars = netsnmp.VarList(netsnmp.Varbind('sysUpTime', 0),
                               netsnmp.Varbind('sysContact', 0),
                               netsnmp.Varbind('sysLocation', 0))
        print "---v3 get-------------------------------------\n"
        vals = sess.get(vars)
        print "v3 sess.get result: ", vals, "\n"
        
        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"

        print "---v3 getnext-------------------------------------\n"
       
        vals = sess.getnext(vars)
        print "v3 sess.getnext result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"
       
        vars = netsnmp.VarList(netsnmp.Varbind('sysUpTime'),
                               netsnmp.Varbind('sysORLastChange'),
                               netsnmp.Varbind('sysORID'),
                               netsnmp.Varbind('sysORDescr'),
                               netsnmp.Varbind('sysORUpTime'))

        vals = sess.getbulk(2, 8, vars)
        print "v3 sess.getbulk result: ", vals, "\n"

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"

        print "---v3 set-------------------------------------\n"

        vars = netsnmp.VarList(
            netsnmp.Varbind('sysLocation','0', 'my final destination'))
        res = sess.set(vars)
        print "v3 sess.set result: ", res, "\n"
        
        print "---v3 walk-------------------------------------\n"
        vars = netsnmp.VarList(netsnmp.Varbind('system'))
                
        vals = sess.walk(vars)
        print "v3 sess.walk result: ", vals, "\n"
        
        for var in vars:
            print "  ",var.tag, var.iid, "=", var.val, '(',var.type,')'


class SetTests(unittest.TestCase):
    def testFuncs(self):        
        print "\n-------------- SET Test Start ----------------------------\n"

        var = netsnmp.Varbind('sysUpTime','0')
        res = netsnmp.snmpget(var, Version = 1, DestHost='localhost',
                        Community='public')
        print "uptime = ", res[0]

        
        var = netsnmp.Varbind('versionRestartAgent','0', 1)
        res = netsnmp.snmpset(var, Version = 1, DestHost='localhost',
                        Community='public')

        var = netsnmp.Varbind('sysUpTime','0')
        res = netsnmp.snmpget(var, Version = 1, DestHost='localhost',
                        Community='public')
        print "uptime = ", res[0]

        var = netsnmp.Varbind('nsCacheEntry')
        res = netsnmp.snmpgetnext(var, Version = 1, DestHost='localhost',
                        Community='public')
        print "var = ", var.tag, var.iid, "=", var.val, '(',var.type,')'

        var.val = 65
        res = netsnmp.snmpset(var, Version = 1, DestHost='localhost',
                        Community='public')
        res = netsnmp.snmpget(var, Version = 1, DestHost='localhost',
                        Community='public')
        print "var = ", var.tag, var.iid, "=", var.val, '(',var.type,')'

        sess = netsnmp.Session(Version = 1, DestHost='localhost',
                        Community='public')

        vars = netsnmp.VarList(netsnmp.Varbind('.1.3.6.1.6.3.12.1.2.1.2.116.101.115.116','','.1.3.6.1.6.1.1'),
                              netsnmp.Varbind('.1.3.6.1.6.3.12.1.2.1.3.116.101.115.116','','1234'),
                              netsnmp.Varbind('.1.3.6.1.6.3.12.1.2.1.9.116.101.115.116','', 4))
        res = sess.set(vars)

        print "res = ", res

        vars = netsnmp.VarList(netsnmp.Varbind('snmpTargetAddrTDomain'),
                               netsnmp.Varbind('snmpTargetAddrTAddress'),
                               netsnmp.Varbind('snmpTargetAddrRowStatus'))

        res = sess.getnext(vars)

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"

        vars = netsnmp.VarList(netsnmp.Varbind('.1.3.6.1.6.3.12.1.2.1.9.116.101.115.116','', 6))      

        res = sess.set(vars)

        print "res = ", res

        vars = netsnmp.VarList(netsnmp.Varbind('snmpTargetAddrTDomain'),
                               netsnmp.Varbind('snmpTargetAddrTAddress'),
                               netsnmp.Varbind('snmpTargetAddrRowStatus'))

        res = sess.getnext(vars)

        for var in vars:
            print var.tag, var.iid, "=", var.val, '(',var.type,')'
        print "\n"

        print "\n-------------- SET Test End ----------------------------\n"
        

if __name__=='__main__':
    unittest.main()
