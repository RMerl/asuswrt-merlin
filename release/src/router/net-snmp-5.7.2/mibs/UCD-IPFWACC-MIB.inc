-- input include file for smicng.

#condInclude "SNMPv2-SMI.inc"  -- SNMPv2-SMI (SNMPv2 SMI)
#condInclude "SNMPv2-TC.inc"   -- SNMPv2-TC (SNMPv2 TC macro and base TCs)
#condInclude "UCD-SNMP-MIB.inc"

#pushOpt
#addOpt "CM"
#condInclude "UCD-IPFWACC-MIB.txt"
#popOpt

