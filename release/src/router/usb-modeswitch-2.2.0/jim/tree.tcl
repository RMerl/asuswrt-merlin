# Conceptually compatible with tcllib ::struct::tree
# but uses an object based interface.
# To mimic tcllib, do:
#   rename [tree] mytree

package require oo

# set pt [tree]
#
#   Create a tree
#   This automatically creates a node named "root"
#
# $pt destroy
#
#   Destroy the tree and all it's nodes
#
# $pt set <nodename> <key> <value>
#
#   Set the value for the given key
#
# $pt lappend <nodename> <key> <value> ...
#
#   Append to the (list) value(s) for the given key, or set if not yet set
#
# $pt keyexists <nodename> <key>
#
#   Returns 1 if the given key exists
#
# $pt get <nodename> <key>
#
#   Returns the value associated with the given key
# 
# $pt getall <nodename>
#
#   Returns the entire attribute dictionary associated with the given key
# 
# $pt depth <nodename>
#
#   Returns the depth of the given node. The depth of "root" is 0.
#
# $pt parent <nodename>
#
#   Returns the name of the parent node, or "" for the root node.
# 
# $pt numchildren <nodename>
#
#   Returns the number of child nodes.
# 
# $pt children <nodename>
#
#   Returns a list of the child nodes.
# 
# $pt next <nodename>
#
#   Returns the next sibling node, or "" if none.
# 
# $pt insert <nodename> ?index?
#
#   Add a new child node to the given node.
#   THe default index is "end"
#   Returns the name of the newly added node
#
# $pt walk <nodename> dfs|bfs {actionvar nodevar} <code>
#
#   Walks the tree starting from the given node, either breadth first (bfs)
#   depth first (dfs).
#   The value "enter" or "exit" is stored in variable $actionvar
#   The name of each node is stored in $nodevar.
#   The script $code is evaluated twice for each node, on entry and exit.
#
# $pt dump
#
#   Dumps the tree contents to stdout

#------------------------------------------
# Internal implementation.
# The tree class has 4 instance variables.
# - tree is a dictionary. key=node, value=node value dictionary
# - parent is a dictionary. key=node, value=parent of this node
# - children is a dictionary. key=node, value=list of child nodes for this node
# - nodeid is an integer which increments to give each node a unique id

# Construct a tree with a single root node with no parent and no children
class tree {
	tree {root {}}
	parents {root {}}
	children {root {}}
	nodeid 0
}

# Simply walk up the tree to get the depth
tree method depth {node} {
	set depth 0
	while {$node ne "root"} {
		incr depth
		set node [dict get $parents $node]
	}
	return $depth
}

tree method parent {node} {
	dict get $parents $node
}

tree method children {node} {
	dict get $children $node
}

tree method numchildren {node} {
	llength [dict get $children $node]
}

tree method next {node} {
	# My siblings are my parents children
	set siblings [dict get $children [dict get $parents $node]]
	# Find me
	set i [lsearch $siblings $node]
	incr i
	lindex $siblings $i
}

tree method set {node key value} {
	dict set tree $node $key $value
	return $value
}

tree method get {node key} {
	dict get $tree $node $key
}

tree method keyexists {node key} {
	dict exists $tree $node $key
}

tree method getall {node} {
	dict get $tree $node
}

tree method insert {node {index end}} {

	# Make a new node and add it to the tree
	set childname node[incr nodeid]
	dict set tree $childname {}

	# The new node has no children
	dict set children $childname {}

	# Set the parent
	dict set parents $childname $node

	# And add it as a child
	set nodes [dict get $children $node]
	dict set children $node [linsert $nodes $index $childname]

	return $childname
}

tree method lappend {node key args} {
	if {[dict exists $tree $node $key]} {
		set result [dict get $tree $node $key]
	}
	lappend result {*}$args
	dict set tree $node $key $result
	return $result
}

# $tree walk node bfs|dfs {action loopvar} <code>
#
tree method walk {node type vars code} {
	# set up vars
	lassign $vars actionvar namevar

	set n $node

	if {$type ne "child"} {
		upvar 2 $namevar name $actionvar action

		# Enter this node
		set name $node
		set action enter

		uplevel 2 $code
	}

	if {$type eq "dfs"} {
		# Depth-first so do the children
		foreach child [$self children $n] {
			uplevel 2 [list $self walk $child $type $vars $code]
		}
	} elseif {$type ne "none"} {
		# Breadth-first so do the children to one level only
		foreach child [$self children $n] {
			uplevel 2 [list $self walk $child none $vars $code]
		}

		# Now our grandchildren
		foreach child [$self children $n] {
			uplevel 2 [list $self walk $child child $vars $code]
		}
	}

	if {$type ne "child"} {
		# Exit this node
		set name $node
		set action exit

		uplevel 2 $code
	}
}

tree method dump {} {
	$self walk root dfs {action n} {
		set indent [string repeat "  " [$self depth $n]]
		if {$action eq "enter"} {
			puts "$indent$n ([$self getall $n])"
		}
	}
	puts ""
}
