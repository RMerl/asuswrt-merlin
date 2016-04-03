ipset_list
==========

ipset set listing wrapper script written for the bash shell.
It allows you to match and display sets, headers and elements in various ways.


Features:
==========

- Calculate sum of set members (and match on that count).
- List only members of a specified set.
- Choose a delimiter character for separating members.
- Show only sets containing a specific (glob matching) header.
- Arithmetic comparison on headers with an integer value.
- Arithmetic comparison on flags of the headers 'Header' field.
- Arithmetic comparison on member options with an integer value.
- Match members using a globbing or regex pattern.
- Suppress listing of (glob matching) sets.
- Suppress listing of (glob matching) headers.
- Suppress listing of members matching a glob or regex pattern.
- Suppress listing of members options.
- Calculate the total size in memory of all matching sets.
- Calculate the amount of matching, excluded and traversed sets.
- Colorize the output.
- Operate on a single, selected, or all sets.
- Programmable completion is included to make usage easier and faster.


Examples:
==========

- `ipset_list`                         - no args, just list set names
- `ipset_list -c`                      - show all set names and their member sum
- `ipset_list -t`                      - show all sets, but headers only
- `ipset_list -c -t setA`              - show headers and member sum of setA
- `ipset_list -i setA`                 - show only members entries of setA
- `ipset_list -c -m setA setB`         - show members and sum of setA & setB
- `ipset_list -a -c -d :`              - show all sets members, sum and use `:' as entry delimiter
- `ipset_list -a -c setA`              - show all info of setA and its members sum
- `ipset_list -c -m -d $'\n' setA`     - show members and sum of setA, delim with newline
- `ipset_list -m -r -s setA`           - show members of setA resolved and sorted
- `ipset_list -Fi References:0`    - show all sets with 0 references
- `ipset_list -Hr 0`               - shortcut for -Fi References:0
- `ipset_list -Ht "!(hash:ip)"`    - show sets which are not of type hash:ip
- `ipset_list -Ht "!(bitmap:*)"`   - show sets wich are not of any bitmap type
- `ipset_list -Cs -Ht "hash:*"`    - find sets of any hash type, count their amount.
- `ipset_list -Ts`                 - show all set names and total count of sets.
- `ipset_list -Tm`                 - calculate total size in memory of all sets.
- `ipset_list -Xs setA -Xs setB`   - show all set names, but exclude setA and setB.
- `ipset_list -Xs "set[AB]"`       - show all set names, but exclude setA and setB.
- `ipset_list -Mc 0`               - show sets with zero members 
- `ipset_list -Hr \>=1 -Hv 0 -Hs \>10000`   - find sets with at least one reference, revision of 0 and size in memory greater than 10000
- `ipset_list -i -Fr "^210\..*" setA` - show only members of setA matching the regex "^210\\..*"
- `ipset_list -Mc \>=100 -Mc \<=150` - show sets with a member count greater or equal to 100 and not greater than 150.
- `ipset_list -a -c -Fh  "Type:hash:ip"  -Fr "^210\..*"` - show all information of sets with type hash:ip, matching the regex "^210\\..*", show match and members sum
- `ipset_list -Fh Type:hash:ip -Fh "Header:family inet *"` - show all set names, which are of type hash:ip and header of ipv4.
- `ipset_list -t -Xh "Revision:*" -Xh "References:*"` - show all sets headers, but exclude Revision and References entries.
- `ipset_list -c -m -Xg "210.*" setA` - show members of setA, but suppress listing of entries matching the glob pattern "210.*", show count of excluded and total members.
- `ipset_list -m -Fg "!(210.*)" setA`  - show members of setA excluding the elements matching the negated glob.
- `ipset_list -a -Xh "@(@(H|R|M)e*):*"`  - show all info of all sets, but suppress Header, References, Revision and Member header entries (headers existing as per ipset 6.x -> tested version).
- `ipset_list -t -Tm -Xh "@(Type|Re*|Header):*"` - show all sets headers, but suppress all but name and memsize entry, calculate the total memory size of all sets.
- `ipset_list -t -Tm -Xh "!(Size*|Type):*" -Ts -Co` List all sets headers, but suppress all but name, type and memsize entry,
 count amount of sets, calculate total memory usage, colorize the output.
- `ipset_list -t -Ht "!(@(bit|port)map):*" -Xh "!(Type):*"`   - show all sets that are neither of type bitmap or portmap, suppress all but the type header.
- `ipset_list -c -t -Cs -Ts -Xh "@(Size*|Re*|Header):*" -Ht "!(bitmap:*)"` - find all sets not of any bitmap type, count their members sum, display only the 'Type' header, count amount of matching and traversed sets.
- `ipset_list -Co -c -Ts -Tm`  - show all set names, count their members, count total amount of sets, show total memory usage of all sets, colorize the output
- `ipset_list -m -r -To 0`     - show members of all sets, try to resolve hosts, set the timeout to 0 (effectivly disabling it).
- `ipset_list -m -Xo setA`     - show members of setA, but suppress displaying of the elements options.
- `ipset_list -m -Oi packets:0`     - show members of all sets which have a packet count of 0.
- `ipset_list -m -Oi "packets:>100" -Oi "bytes:>1024"`     - show members of all sets which have a packet count greater than 100 and a byte count greater than 1024.
- `ipset_list -m -Oi "skbmark:>0x123/0XFF" -Oi skbprio:\>=2:<=3 -Oi skbqueue:\!1` - show members of all sets which have the following member options set: skbmark greater than 0x123/0xFF, skbprio major greater or equal to 2 and minor lower or equal to 3, skbqueue not of value 1.
- `ipset_list -n -Ca "foo*"`    - show only set names matching the glob "foo*" and enable all counters.
- `ipset_list -Hi "markmask:>=0x0000beef" -Hi timeout:\!10000`    - show only sets with the header 'Header' fields containing a markmask greater or equal to 0x0000beef and a timeout which is not 10000.


