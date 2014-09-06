snmptrapd MySQL Logging
-----------------------

A trap handler for logging traps to a MySQL database was added
in release 5.5.0.

The MySQL database location and password must be configured in
/root/.my.cnf:

	[snmptrapd]
	host=localhost
	password=sql

User may also be configured, if using a MySQL user besides root.

snmptrapd.conf must be configured to for the queue size and
periodic flush interval:

	# maximum number of traps to queue before forced flush
	sqlMaxQueue 140

	# seconds between periodic queue flushes
	sqlSaveInterval 9

A value of 0 for sqlSaveInterval will completely disable MySQL
logging of traps.

The schema must be loaded into MySQL before running snmptrapd.
The schema can be found in dist/schema-snmptrapd.sql
