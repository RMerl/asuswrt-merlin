# Setlist: Create base set foo
0 ipset -N foo ipmap --from 2.0.0.1 --to 2.1.0.0
# Setlist: Create base set bar
0 ipset -N bar iphash
# Setlist: Create setlist kind of set
0 ipset -N test setlist
# Setlist: Add foo set to setlist
0 ipset -A test foo
# Setlist: Test foo set in setlist
0 ipset -T test foo
# Setlist: Test nonexistent set in setlist
1 ipset -T test nonexistent
# Setlist: Try to delete foo set
1 ipset -X foo
# Setlist: Add bar set to setlist, after foo
0 ipset -A test bar
# Setlist: Test bar,after,foo
0 ipset -T test bar,after,foo
# Setlist: Test foo,before,bar
0 ipset -T test foo,before,bar
# Setlist: Test bar,before,foo
1 ipset -T test bar,before,foo
# Setlist: Test foo,after,bar
1 ipset -T test foo,after,bar
# Setlist: Save sets
0 ipset -S > setlist.t.restore
# Setlist: Delete bar,before,foo
1 ipset -D test bar,before,foo
# Setlist: Delete foo,after,bar
1 ipset -D test foo,after,bar
# Setlist: Delete bar,after,foo
0 ipset -D test bar,after,foo
# Setlist: Flush test set
0 ipset -F test
# Setlist: Delete test set
0 ipset -X test
# Setlist: Delete all sets
0 ipset -X
# Setlist: Restore saved sets
0 ipset -R < setlist.t.restore
# Setlist: List set
0 ipset -L test > .foo
# Setlist: Check listing
0 diff .foo setlist.t.list0 && rm .foo
# Setlist: Flush all sets
0 ipset -F
# Setlist: Delete all sets
0 ipset -X && rm setlist.t.restore
# eof
