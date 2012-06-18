all: randchars randprintable test_speed-kernel test_speed-userspace match_kernel

randchars: randchars.c
	gcc -O2 -o randchars randchars.c

randprintable: randprintable.c
	gcc -O2 -o randprintable randprintable.c

test_speed-kernel: test_speed-kernel.c
	gcc -o test_speed-kernel test_speed-kernel.c

test_speed-userspace: test_speed-userspace.cpp l7-parse-patterns.cpp l7-parse-patterns.h
	g++ -Wall -o test_speed-userspace test_speed-userspace.cpp l7-parse-patterns.cpp

match_kernel: match-kernel.c
	gcc -O2 -o match_kernel match-kernel.c

clean:
	rm -f randprintable randchars test_speed-kernel test_speed-userspace match_kernel
