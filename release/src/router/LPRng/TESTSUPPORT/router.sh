#!/bin/sh
echo ROUTER $0 "$@" 1>&2
# echo STDIN 1>&2
# cat 1>&2
# echo STDIN END 1>&2
cat <<'EOF'
dest t1
end
dest t1
end
EOF
exit 0
