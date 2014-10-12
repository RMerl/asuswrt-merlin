#!/usr/bin/env bash
set -e
for file in ${1:-librpc/gen_ndr/ndr_*.c}; do
	quilt add "$file" || true
	awk '
$0 ~ /^static const struct ndr_interface_call .* = {$/ {
	replace = 1
}

$0 ~ /^}$/ {
	replace = 0;
}

replace == 1 {
	gsub(/.ndr_print_function_t. .*,/, "(ndr_print_function_t) ndr_print_disabled,", $0)
}
{
	print $0
}
	' < "$file" > "$file.new"
	mv "$file.new" "$file"
done
