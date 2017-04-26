CFLAGS = -std=c99 -g -Wall -Wextra

main: main.c vector.c

clean:
	$(RM) main

.PHONY: clean
