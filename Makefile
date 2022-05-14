SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
DEPS=$(OBJS:.o=.d)
CFLAGS=-g -Wall -std=c99 #-fsanitize=address,undefined

mycc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

.PHONY: clean test

clean:
	rm -rf mycc $(OBJS) $(DEPS)

test: mycc
	@./test.sh
	@rm temp.*