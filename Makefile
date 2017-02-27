ifeq ($(WITH_CORE),)
	EXTRA_DEFS = -D CORE=0
else
	EXTRA_DEFS = -D CORE=1
endif

CC = gcc
CFLAGS = -Wall -ggdb -std=c11 $(EXTRA_DEFS)
ALL_SOURCES = $(wildcard *.c)
ALL_OBJECTS = $(ALL_SOURCES:%.c=%.o)

# Main application 
EXEC_SOURCES = $(filter-out $(wildcard main-*.c), $(ALL_SOURCES))
EXEC_OBJECTS = $(EXEC_SOURCES:%.c=%.o)
EXEC = $(shell grep "\#define EXEC_NAME" common.h | cut -d " " -f3 | sed 's/"//g')

# Test application
TEST_SOURCES = $(filter-out main.c main-core_server.c, $(ALL_SOURCES))
TEST_OBJECTS = $(TEST_SOURCES:%.c=%.o)
TESTS = tests

# Core server application
CORE_SOURCES = $(filter-out main.c main-test.c, $(ALL_SOURCES))
CORE_OBJECTS = $(CORE_SOURCES:%.c=%.o)
CORE = core-server

# Main targets

.PHONY: all
all: $(EXEC) $(TESTS) $(CORE)

$(EXEC): $(EXEC_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(TESTS): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(CORE): $(CORE_OBJECTS)
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
	rm -f $(CORE)
