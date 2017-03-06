CC = gcc
CFLAGS = -Wall -ggdb -std=c11 -I. -Iserver
ALL_SOURCES = $(wildcard *.c) $(wildcard server/*.c)
ALL_OBJECTS = $(ALL_SOURCES:%.c=%.o)

# Main application 
EXEC_SOURCES = $(filter-out main-test.c, $(ALL_SOURCES))
EXEC_OBJECTS = $(EXEC_SOURCES:%.c=%.o)
EXEC = $(shell grep "\#define EXEC_NAME" common.h | cut -d " " -f3 | sed 's/"//g')

# Test application
TEST_SOURCES = $(filter-out main.c, $(ALL_SOURCES))
TEST_OBJECTS = $(TEST_SOURCES:%.c=%.o)
TESTS = tests

# Main targets

.PHONY: all
all: $(EXEC) $(TESTS)

$(EXEC): $(EXEC_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(TESTS): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# Clean targets

.PHONY: clean
clean:
	rm -f $(ALL_OBJECTS)
	rm -f *~

.PHONY: veryclean
veryclean: clean
	rm -f $(EXEC)
	rm -f $(TESTS)
