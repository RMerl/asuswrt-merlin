USE net_snmp;
DROP TABLE IF EXISTS notifications;
CREATE TABLE IF NOT EXISTS `notifications` (
  `trap_id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `date_time` datetime NOT NULL,
  `host` varchar(255) NOT NULL,
  `auth` varchar(255) NOT NULL,
  `type` ENUM('get','getnext','response','set','trap','getbulk','inform','trap2','report') NOT NULL,
  `version` ENUM('v1','v2c', 'unsupported(v2u)','v3') NOT NULL,
  `request_id` int(11) unsigned NOT NULL,
  `snmpTrapOID` varchar(1024) NOT NULL,
  `transport` varchar(255) NOT NULL,
  `security_model` ENUM('snmpV1','snmpV2c','USM') NOT NULL,
  `v3msgid` int(11) unsigned,
  `v3security_level` ENUM('noAuthNoPriv','authNoPriv','authPriv'),
  `v3context_name` varchar(32),
  `v3context_engine` varchar(64),
  `v3security_name` varchar(32),
  `v3security_engine` varchar(64),
  PRIMARY KEY  (`trap_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;


DROP TABLE IF EXISTS varbinds;
CREATE TABLE IF NOT EXISTS `varbinds` (
  `trap_id` int(11) unsigned NOT NULL default '0',
  `oid` varchar(1024) NOT NULL,
  `type` ENUM('boolean','integer','bit','octet','null','oid','ipaddress','counter','unsigned','timeticks','opaque','unused1','counter64','unused2') NOT NULL,
  `value` blob NOT NULL,
  KEY `trap_id` (`trap_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
