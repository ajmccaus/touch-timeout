CC = gcc
CFLAGS = -Wall -Wextra -g -I./mocks
COVERAGE_FLAGS = --coverage -fprofile-arcs -ftest-coverage
TARGET = test_touch_timeout

.PHONY: all test coverage clean report

# Build tests without coverage (fast)
all: $(TARGET)

$(TARGET): test_touch_timeout.c ../touch-timeout.c
	$(CC) $(CFLAGS) -o $(TARGET) test_touch_timeout.c

# Run tests
test: $(TARGET)
	./$(TARGET)

# Build with coverage instrumentation
coverage-build:
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $(TARGET) test_touch_timeout.c

# Run tests with coverage and generate report
# Uses xcrun llvm-cov for macOS, gcov for Linux
coverage: clean coverage-build
	./$(TARGET)
	@echo ""
	@echo "========================================"
	@echo "Coverage Report"
	@echo "========================================"
	@if [ "$(shell uname)" = "Darwin" ]; then \
		xcrun llvm-cov gcov $(TARGET)-$(TARGET).gcda 2>/dev/null; \
	else \
		gcov $(TARGET).c 2>/dev/null; \
	fi
	@echo ""
	@echo "Detailed line coverage saved to: touch-timeout.c.gcov"

# Generate HTML coverage report (requires lcov)
html-coverage: coverage
	@if command -v lcov >/dev/null 2>&1; then \
		lcov --capture --directory . --output-file coverage.info --ignore-errors source 2>/dev/null; \
		genhtml coverage.info --output-directory coverage_html 2>/dev/null; \
		echo "HTML report: coverage_html/index.html"; \
	else \
		echo "lcov not installed. Install with: brew install lcov"; \
	fi

# Clean all generated files
clean:
	rm -f $(TARGET)
	rm -f *.gcno *.gcda *.gcov
	rm -f coverage.info
	rm -rf coverage_html
