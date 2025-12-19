# Makefile for touch-timeout unit tests
#
# Tests the pure state machine module with coverage

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -I../src
COVERAGE_CFLAGS = --coverage -fprofile-arcs -ftest-coverage -g
LDFLAGS = --coverage

SRC_DIR = ../src

.PHONY: all test clean coverage help

all: test_state

# Build state module object with coverage
state_test.o: $(SRC_DIR)/state.c
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -c -o $@ $<

# Link test executable
test_state: test_state.c state_test.o
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -o $@ $^ $(LDFLAGS)

# Run tests
test: test_state
	./test_state

# Generate coverage report
coverage: test
	@echo ""
	@echo "Generating coverage report..."
	@if command -v lcov >/dev/null 2>&1; then \
		mkdir -p coverage; \
		lcov --capture --directory . --output-file coverage/coverage.info 2>/dev/null || true; \
		lcov --remove coverage/coverage.info '/usr/*' --output-file coverage/coverage.info 2>/dev/null || true; \
		genhtml coverage/coverage.info --output-directory coverage 2>/dev/null || true; \
		echo "Coverage report: coverage/index.html"; \
	else \
		echo "lcov not installed. Using gcov:"; \
		gcov *.gcda 2>/dev/null || true; \
	fi

# Clean
clean:
	rm -f test_state *.o *.gcda *.gcno *.gcov
	rm -rf coverage

help:
	@echo "  make       - Build tests"
	@echo "  make test  - Run tests"
	@echo "  make clean - Remove artifacts"
