.PHONY: all clean

CFLAGS := -Wall -Wextra -Wno-format -std=c99 -pedantic -lm -O

SRC = $(wildcard src/*.c)

all: arrow

arrow:
	$(CC) $(CFLAGS) -o arrowc $(SRC)

clean:
	rm -f arrowc *.o *.s
