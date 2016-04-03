ipset-bash-completion
=====================

Description
===========

Programmable completion specification (compspec) for the bash shell
to support the ipset program (netfilter.org).


Programmable completion allows the user, while working in an interactive shell,
to retrieve and auto-complete commands, their options, filenames, etc.
Pressing [TAB] will complete on the current word, if only one match is found.
If multiple completions are possible, they will be listed by hitting [TAB] again.


Features
========

This completion specification follows the logic of ipset and
 will show commands and options only when they are available for the current context
(combination of commands and/or options).
Providing some kind of interactive help.

- Show and complete commands and options.
- Show and complete set names.
- Show and complete the set types when using the create and help command.
- Show and complete set elements (members) when using the del command.
- Show and complete services (also named port ranges), protocols,
icmp[6] types and interface names when adding, deleting or testing elements.
- Show and complete hostnames, when adding, deleting or testing elements.
- Show and complete ip and mac addresses (dynamically and from file).
- Complete on filenames if the current option requires it.
- Complete variable names and command substitution.
- Do not complete if an invalid combination of options is used.
- Do not complete if an invalid value of an option argument is detected.
- Environment variables allow to modify completion behaviour.


Installation
============

Put it into ~/.bash_completion or /etc/bash_completion.d/.

Tip:
To make tab completion more handsome put the following into either
/etc/inputrc or ~/.inputrc:

     set show-all-if-ambiguous on

This will allow single tab completion as opposed to requiring a double tab.

     set page-completions off

This turns off the use of the internal pager when returning long completion lists.


Usage
=====

Type -[TAB] to start option completion.
Sets named - (hyphen) or starting with it, are supported,
excluded are option names (i.e. -exist).

Type [TAB] to complete on anything available at the current context.

Depending on the environment variable **_IPSET_COMPL_OPT_FORMAT**,
either the long, short or both forms of options are shown for completion.
By default (empty _IPSET_COMPL_OPT_FORMAT) the long versions of options
are displayed (_IPSET_COMPL_OPT_FORMAT=long also does the same).
Setting it to 'short' will cause completion to show only the short form.
Setting it to any other value, will result in both version being displayed and completed.

---

To complete named port ranges, enter the hypen after the first completed service (port) name,
hit [TAB] again to start completion on the second named port (the brackets [] for service names
containing a - (hyphen) are already surrounding the name in the completion list).

---

The environment variable **HOSTFILE** controls how hostname completion is performed.
Taking the description from the bash man-page:

	Contains the name of a file in the same format as /etc/hosts that 
	should be read when the shell needs to complete a hostname.
	The list of possible hostname completions may be changed while the shell is running
	the next time hostname completion is attempted after the value is changed,
	bash adds the contents of the new file to the existing list.
	If HOSTFILE is set, but has no value, or does not name a readable file, bash
	attempts to read /etc/hosts to obtain the list of possible hostname completions.
	When HOSTFILE is unset, the hostname list is cleared.

As it is impossible to distinguish between IPv4 and IPv6 hostnames without resolving
them, it is considered best practice to seperate IPv4 hosts from IPv6 hosts
in different files.

If the *bash-completion* package is available hostname completion is extended
the following way (description from bash-completion source):

	Helper function for completing _known_hosts.
	This function performs host completion based on ssh config and known_hosts
	files, as well as hostnames reported by avahi-browse if
	COMP_KNOWN_HOSTS_WITH_AVAHI is set to a non-empty value.
	Also hosts from HOSTFILE (compgen -A hostname) are added, unless
	COMP_KNOWN_HOSTS_WITH_HOSTFILE is set to an empty value.


Also the environment variable **_IPSET_SSH_CONFIGS** controls which files are taken
as ssh_config files, in order to retrieve the global and user known_host files,
which will be used for hostname completion.

For all *net* type of sets and the hash:ip,mark set type, if hostname completion is attempted,
if the environment variable **_IPSET_COMP_NETWORKS** is set to a non-empty value,
networks are retrieved from /etc/networks.

Also a list of ip addresses can be supplied using the environment variable
**_IPSET_IPLIST_FILE**. Which should point to a file containing an ip address per line.
They can be ipv4 and/or ipv6. Detection which type should be included
is done automatically based on the set header.

---

When deleting elements from any but list:set set types:
the environment variable **_IPSET_COMPL_DEL_MODE** is queried to decide how to complete.
If it is set to 'members' it will list the members of the set.
If it is set to 'spec' it will follow the format of a port specification ([proto:]port).
If it is set to any other value both methods are used to generate
the list of possible completions (this is the default).

---

When testing elements from any but list:set set types:
the environment variable **_IPSET_COMPL_TEST_MODE** is queried to decide how to complete.
If it is set to 'members' it will list the members of the set.
If it is set to 'spec' it will follow the format of a port specification ([proto:]port).
If it is set to any other value both methods are used to generate
the list of possible completions (this is the default).

---

When adding elements to a **bitmap:ip,mac** type of set,
the environment variable **_IPSET_MACLIST_FILE** will be queried
for a file containing a list of mac addresses.
The file should contain one mac address per line.
Empty lines and comments (also after the address) are supported.
If the variable is unset mac addresses are fetched from arp cache,
/etc/ethers and the output of `ip link show`.

---

When adding elements to one of the following set types:
**hash:ip,port hash:ip,port,ip hash:ip,port,net hash:net,port hash:net,port,net**
and completion is attempted on the port specification,
the list of possible completions may become quite long.
Especially if no characters are given to match on.
This behaviour is because of the many different
values such a port specification can possibly have.


---

At any time completion on variable names (starting with '$' or '${'),
or command substitution (starting with '$(') is available.
Using this with values validated by input validation, will stop further completion.
In that case it is recommended to disable input validation (see below).


---

If the environment variable **_IPSET_VALIDATE_INPUT** is set to a non empty value
validation of users input is disabled.

---

If the environment variable **_DEBUG_NF_COMPLETION** is defined (any value)
debugging information is displayed.



Compatibility
=============

Compatible with ipset versions 6+.

Tested with ipset v6.20.1.

bash v4+ is required.

Compatibility for future ipset versions cannot be promised, as new options may appear, 
which of course are currently unpredictable.

The bash-completion (v2.0+) package is highly recommended, though not mandatory.

http://bash-completion.alioth.debian.org/

Some things might not be that reliable or feature rich without it.
Also the colon (if there) is removed from COMP_WORDBREAKS.
This alteration is globally, which might affect other completions,
if they do not take care of it themselves.

The iproute program (ip) is needed to display information about the local system.



Availability
============

https://github.com/AllKind/ipset-bash-completion

http://sourceforge.net/projects/ipset-bashcompl/



Bugs
============

Please report bugs!


