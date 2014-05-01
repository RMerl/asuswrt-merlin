# functions for parsing and generating json

_json_get_var() {
	local ___dest="$1"
	local ___var="$2"
	eval "$___dest=\"\$${JSON_PREFIX}$___var\""
}

_json_set_var() {
	local ___var="$1"
	local ___val="$2"
	eval "${JSON_PREFIX}$___var=\"\$___val\""
}

_jshn_append() {
	local __var="$1"
	local __value="$2"
	local __sep="${3:- }"
	local __old_val

	_json_get_var __old_val "$__var"
	__value="${__old_val:+$__old_val$__sep}$__value"
	_json_set_var "$__var" "$__value"
}

_json_export() {
	local __var="${JSON_PREFIX}$1"
	local __val="$2"

	export -- "$__var=$__val"
	_jshn_append "JSON_UNSET" "$__var"
}

_json_add_key() {
	local table="$1"
	local var="$2"
	_jshn_append "KEYS_${table}" "$var"
}

_get_var() {
	local __dest="$1"
	local __var="$2"
	eval "$__dest=\"\$$__var\""
}

_set_var() {
	local __var="$1"
	local __val="$2"
	eval "$__var=\"\$__val\""
}

_json_inc() {
	local _var="$1"
	local _dest="$2"
	local _seq

	_json_get_var _seq "$_var"
	_seq="$((${_seq:-0} + 1))"
	_json_set_var "$_var" "$_seq"
	[ -n "$_dest" ] && _set_var "$_dest" "$_seq"
}

_json_stack_push() {
	local new_cur="$1"
	local cur

	_json_get_var cur JSON_CUR
	_jshn_append JSON_STACK "$cur"
	_json_set_var JSON_CUR "$new_cur"
}

_json_add_generic() {
	local type="$1"
	local var="$2"
	local val="$3"
	local cur="$4"

	[ -n "$cur" ] || _json_get_var cur JSON_CUR

	if [ "${cur%%[0-9]*}" = "JSON_ARRAY" ]; then
		_json_inc "SEQ_$cur" var
	else
		local name="${var//[^a-zA-Z0-9_]/_}"
		[[ "$name" == "$var" ]] || _json_export "NAME_${cur}_${name}" "$var"
		var="$name"
	fi

	_json_export "${cur}_$var" "$val"
	_json_export "TYPE_${cur}_$var" "$type"
	_json_add_key "$cur" "$var"
}

_json_add_table() {
	local name="$1"
	local type="$2"
	local itype="$3"
	local cur new_cur
	local seq

	_json_get_var cur JSON_CUR
	_json_inc JSON_SEQ seq

	local table="JSON_$itype$seq"
	_json_export "UP_$table" "$cur"
	_json_export "KEYS_$table" ""
	[ "$itype" = "ARRAY" ] && _json_export "SEQ_$table" ""
	_json_stack_push "$table"

	_json_get_var new_cur JSON_CUR
	_json_add_generic "$type" "$1" "$new_cur" "$cur"
}

_json_close_table() {
	local stack new_stack

	_json_get_var stack JSON_STACK
	_json_set_var JSON_CUR "${stack##* }"
	new_stack="${stack% *}"
	[[ "$stack" == "$new_stack" ]] && new_stack=
	_json_set_var JSON_STACK "$new_stack"
}

json_set_namespace() {
	local _new="$1"
	local _old="$2"

	[ -n "$_old" ] && _set_var "$_old" "$JSON_PREFIX"
	JSON_PREFIX="$_new"
}

json_cleanup() {
	local unset

	_json_get_var unset JSON_UNSET
	[ -n "$unset" ] && eval "unset $unset"

	unset \
		${JSON_PREFIX}JSON_SEQ \
		${JSON_PREFIX}JSON_STACK \
		${JSON_PREFIX}JSON_CUR \
		${JSON_PREFIX}JSON_UNSET \
		${JSON_PREFIX}KEYS_JSON_VAR \
		${JSON_PREFIX}TYPE_JSON_VAR
}

json_init() {
	json_cleanup
	export -- \
		${JSON_PREFIX}JSON_SEQ=0 \
		${JSON_PREFIX}JSON_STACK= \
		${JSON_PREFIX}JSON_CUR="JSON_VAR" \
		${JSON_PREFIX}JSON_UNSET="" \
		${JSON_PREFIX}KEYS_JSON_VAR= \
		${JSON_PREFIX}TYPE_JSON_VAR=
}

json_add_object() {
	_json_add_table "$1" object TABLE
}

json_close_object() {
	_json_close_table
}

json_add_array() {
	_json_add_table "$1" array ARRAY 
}

json_close_array() {
	_json_close_table
}

json_add_string() {
	_json_add_generic string "$1" "$2"
}

json_add_int() {
	_json_add_generic int "$1" "$2"
}

json_add_boolean() {
	_json_add_generic boolean "$1" "$2"
}

json_add_double() {
	_json_add_generic double "$1" "$2"
}

# functions read access to json variables

json_load() {
	eval `jshn -r "$1"`
}

json_dump() {
	jshn "$@" ${JSON_PREFIX:+-p "$JSON_PREFIX"} -w 
}

json_get_type() {
	local __dest="$1"
	local __cur

	_json_get_var __cur JSON_CUR
	local __var="${JSON_PREFIX}TYPE_${__cur}_${2//[^a-zA-Z0-9_]/_}"
	eval "export -- \"$__dest=\${$__var}\"; [ -n \"\${$__var+x}\" ]"
}

json_get_keys() {
	local __dest="$1"
	local _tbl_cur

	json_get_var _tbl_cur "$2"
	local __var="${JSON_PREFIX}KEYS_${_tbl_cur}"
	eval "export -- \"$__dest=\${$__var}\"; [ -n \"\${$__var+x}\" ]"
}

json_get_var() {
	local __dest="$1"
	local __cur

	_json_get_var __cur JSON_CUR
	local __var="${JSON_PREFIX}${__cur}_${2//[^a-zA-Z0-9_]/_}"
	eval "export -- \"$__dest=\${$__var}\"; [ -n \"\${$__var+x}\" ]"
}

json_get_vars() {
	while [ "$#" -gt 0 ]; do
		local _var="$1"; shift
		json_get_var "$_var" "$_var"
	done
}

json_select() {
	local target="$1"
	local type
	local cur

	[ -z "$1" ] && {
		_json_set_var JSON_CUR "JSON_VAR"
		return 0
	}
	[[ "$1" == ".." ]] && {
		_json_get_var cur JSON_CUR
		_json_get_var cur "UP_$cur"
		_json_set_var JSON_CUR "$cur"
		return 0
	}
	json_get_type type "$target"
	case "$type" in
		object|array)
			json_get_var cur "$target"
			_json_set_var JSON_CUR "$cur"
		;;
		*)
			echo "WARNING: Variable '$target' does not exist or is not an array/object"
			return 1
		;;
	esac
}

json_is_a() {
	local type

	json_get_type type "$1"
	[ "$type" = "$2" ]
}
