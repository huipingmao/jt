build/jt: jt.c
	cc jt.c -o build/jt -Wall -Wextra -std=c99
PHONY.: run
run: jt
	./build/jt
