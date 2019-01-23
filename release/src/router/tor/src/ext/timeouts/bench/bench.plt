set terminal postscript color

set key top left
set xlabel "Number of timeouts"
set ylabel "Time\n(microseconds)"
#set logscale x

set title "Time spent installing timeouts" font ",20"
plot 'heap-add.dat' using 1:($2*1000000) title "min-heap" with lines ls 1 lw 3 lc "red", \
     'wheel-add.dat' using 1:($2*1000000) title "hierarchical wheel" with lines ls 1 lw 3 lc "forest-green"

set title "Time spent deleting timeouts" font ",20"
plot 'heap-del.dat' using 1:($2*1000000) title "min-heap" with lines ls 1 lw 3 lc "red", \
     'wheel-del.dat' using 1:($2*1000000) title "hierarchical wheel" with lines ls 1 lw 3 lc "forest-green"

set title "Time spent expiring timeouts\n(by iteratively updating clock ~1000 times)" font ",20"
plot 'heap-expire.dat' using 1:($2*1000000) title "min-heap" with lines ls 1 lw 3 lc "red", \
     'wheel-expire.dat' using 1:($2*1000000) title "hierarchical wheel" with lines ls 1 lw 3 lc "forest-green"

