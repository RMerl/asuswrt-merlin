#!/usr/bin/python
import dbus

bus = dbus.SystemBus()
p = bus.get_object("uk.org.thekelleys.dnsmasq", "/uk/org/thekelleys/dnsmasq")
l = dbus.Interface(p, dbus_interface="uk.org.thekelleys.dnsmasq")

# The new more flexible SetServersEx method
array = dbus.Array()
array.append(["1.2.3.5"])
array.append(["1.2.3.4#664", "foobar.com"])
array.append(["1003:1234:abcd::1%eth0", "eng.mycorp.com", "lab.mycorp.com"])
print l.SetServersEx(array)

# Must create a new object for dnsmasq as the introspection gives the wrong
# signature for SetServers (av) while the code only expects a bunch of arguments
# instead of an array of variants
p = bus.get_object("uk.org.thekelleys.dnsmasq", "/uk/org/thekelleys/dnsmasq", introspect=False)
l = dbus.Interface(p, dbus_interface="uk.org.thekelleys.dnsmasq")

# The previous method; all addresses in machine byte order
print l.SetServers(dbus.UInt32(16909060), # 1.2.3.5
                   dbus.UInt32(16909061), # 1.2.3.4
                   "foobar.com",
                   dbus.Byte(0x10),       # 1003:1234:abcd::1
                   dbus.Byte(0x03),
                   dbus.Byte(0x12),
                   dbus.Byte(0x34),
                   dbus.Byte(0xab),
                   dbus.Byte(0xcd),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x00),
                   dbus.Byte(0x01),
                   "eng.mycorp.com",
                   "lab.mycorp.com")

