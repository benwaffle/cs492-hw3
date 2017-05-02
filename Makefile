CFLAGS = -std=c99 -g -Og -Wall -Wextra
LDLIBS = -lm

main: main.c vector.c

clean:
	$(RM) main

.PHONY: clean
