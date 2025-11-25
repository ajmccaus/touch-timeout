# Makefile for touch-timeout unit tests (modular version 2.0)
#
# Builds separate test executables for each module
# Compiles source modules with coverage instrumentation
# Supports code coverage with gcov/lcov

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I../src -D_POSIX_C_SOURCE=200809L
COVERAGE_CFLAGS = --coverage -fprofile-arcs -ftest-coverage -g
LDFLAGS = --coverage

# Source directories
SRC_DIR = ../src
TEST_DIR = .

# Source files (from src/)
CONFIG_SRC = $(SRC_DIR)/config.c
STATE_SRC = $(SRC_DIR)/state.c
DISPLAY_SRC = $(SRC_DIR)/display.c
INPUT_SRC = $(SRC_DIR)/input.c
TIMER_SRC = $(SRC_DIR)/timer.c

# Test object files (built in test directory with coverage)
CONFIG_OBJ = config_test.o
STATE_OBJ = state_test.o
DISPLAY_OBJ = display_test.o
INPUT_OBJ = input_test.o
TIMER_OBJ = timer_test.o

# Test executables
TEST_TARGETS = test_config test_state

.PHONY: all test clean coverage help

all: $(TEST_TARGETS)

# Build module objects with coverage instrumentation
config_test.o: $(CONFIG_SRC)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -c -o $@ $<

state_test.o: $(STATE_SRC)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -c -o $@ $<

display_test.o: $(DISPLAY_SRC)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -c -o $@ $<

input_test.o: $(INPUT_SRC)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -c -o $@ $<

timer_test.o: $(TIMER_SRC)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -c -o $@ $<

# Link test executables
test_config: test_config.c $(CONFIG_OBJ)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -o $@ $^ $(LDFLAGS)

test_state: test_state.c $(STATE_OBJ)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) -o $@ $^ $(LDFLAGS)

# Run all tests
test: $(TEST_TARGETS)
	@echo "========================================="
	@echo "Running modular unit tests"
	@echo "========================================="
	@for test in $(TEST_TARGETS); do \
		echo ""; \
		echo "--- Running $$test ---"; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "========================================="
	@echo "All tests passed!"
	@echo "========================================="

# Generate coverage report (requires lcov)
coverage: test
	@echo ""
	@echo "========================================="
	@echo "Generating coverage report..."
	@echo "========================================="
	@if command -v lcov >/dev/null 2>&1; then \
		mkdir -p coverage; \
		lcov --capture --directory . --output-file coverage/coverage.info 2>/dev/null || true; \
		lcov --remove coverage/coverage.info '/usr/*' --output-file coverage/coverage.info 2>/dev/null || true; \
		genhtml coverage/coverage.info --output-directory coverage 2>/dev/null || true; \
		echo "Coverage report: coverage/index.html"; \
	else \
		echo "lcov not installed. Using gcov for basic coverage:"; \
		gcov *.gcda 2>/dev/null || true; \
		echo "Coverage files: *.gcov"; \
	fi

# Clean test artifacts
clean:
	rm -f $(TEST_TARGETS)
	rm -f *.o *.gcda *.gcno *.gcov
	rm -f $(SRC_DIR)/*.gcda $(SRC_DIR)/*.gcno
	rm -rf coverage
	rm -f test_touch_timeout
	rm -rf test_touch_timeout.dSYM

# Help target
help:
	@echo "Touch-timeout test suite"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build all tests"
	@echo "  make test     - Run all tests"
	@echo "  make coverage - Generate coverage report"
	@echo "  make clean    - Remove build artifacts"
