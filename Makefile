.PHONY: all clean

CFLAGS := -Wall -Wextra -Wno-format -std=c11 -pedantic -O

SRC = $(wildcard src/*.c)

all: arrow

arrow:
	$(CC) $(CFLAGS) -o arrowc $(SRC) -lm

clean:
	rm -f arrowc *.o *.s
