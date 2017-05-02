CFLAGS = -std=c99 -g -O0 -Wall -Wextra
LDLIBS = -lm

main: main.c vector.c

clean:
	$(RM) main

.PHONY: clean
