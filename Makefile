CFLAGS = -std=c99 -g -Wall -Wextra
LDLIBS = -lm

main: main.c vector.c

clean:
	$(RM) main

.PHONY: clean
