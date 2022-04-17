SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
CFLAGS=-g -Wall #-fsanitize=address,undefined
#export ASAN_OPTIONS=detect_leaks=0

mycc: $(OBJS) inc.h
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean test

clean:
	rm -rf mycc $(OBJS)

test: mycc
	@./test.sh
	@rm temp.*