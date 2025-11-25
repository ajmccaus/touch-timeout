# Makefile for touch-timeout daemon (modular version 2.0)
#
# Build modular C daemon with:
# - Separate compilation units for each module
# - Optional systemd support
# - Unit tests with mocking
# - Code coverage reporting

CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS =
TARGET = touch-timeout

# Detect OS - add mock includes for non-Linux systems
UNAME_S := $(shell uname -s)
ifneq ($(UNAME_S),Linux)
    CFLAGS += -I./tests/mocks
    $(info Building on $(UNAME_S) with mock headers)
endif

# Source files
SRC_DIR = src
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/config.c \
       $(SRC_DIR)/display.c \
       $(SRC_DIR)/input.c \
       $(SRC_DIR)/state.c \
       $(SRC_DIR)/timer.c

OBJS = $(SRCS:.c=.o)

# Detect systemd availability
SYSTEMD_PKG := $(shell pkg-config --exists libsystemd && echo "yes")

ifeq ($(SYSTEMD_PKG),yes)
    CFLAGS += -DHAVE_SYSTEMD $(shell pkg-config --cflags libsystemd)
    LDFLAGS += $(shell pkg-config --libs libsystemd)
    $(info Building with systemd support)
else
    $(info Building without systemd support)
endif

# Installation paths
PREFIX = /usr
BINDIR = $(PREFIX)/bin
SYSTEMD_UNIT_DIR = /etc/systemd/system
CONFIG_DIR = /etc

.PHONY: all clean install uninstall test coverage

all: $(TARGET)

# Build main binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Dependencies (headers)
$(SRC_DIR)/main.o: $(SRC_DIR)/config.h $(SRC_DIR)/display.h $(SRC_DIR)/input.h $(SRC_DIR)/state.h $(SRC_DIR)/timer.h
$(SRC_DIR)/config.o: $(SRC_DIR)/config.h
$(SRC_DIR)/display.o: $(SRC_DIR)/display.h $(SRC_DIR)/config.h
$(SRC_DIR)/input.o: $(SRC_DIR)/input.h
$(SRC_DIR)/state.o: $(SRC_DIR)/state.h
$(SRC_DIR)/timer.o: $(SRC_DIR)/timer.h

# Clean build artifacts
clean:
	rm -f $(TARGET) $(OBJS)
	$(MAKE) -C tests clean

# Run unit tests
test:
	$(MAKE) -C tests test

# Generate coverage report
coverage:
	$(MAKE) -C tests coverage

# Install to system
install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -D -m 644 systemd/touch-timeout.service $(DESTDIR)$(SYSTEMD_UNIT_DIR)/touch-timeout.service
	install -D -m 644 config/touch-timeout.conf $(DESTDIR)$(CONFIG_DIR)/touch-timeout.conf
	@echo "Installation complete. To enable service:"
	@echo "  sudo systemctl daemon-reload"
	@echo "  sudo systemctl enable touch-timeout.service"
	@echo "  sudo systemctl start touch-timeout.service"

# Uninstall from system
uninstall:
	systemctl stop touch-timeout.service 2>/dev/null || true
	systemctl disable touch-timeout.service 2>/dev/null || true
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(SYSTEMD_UNIT_DIR)/touch-timeout.service
	@echo "Uninstallation complete. Config file preserved at $(CONFIG_DIR)/touch-timeout.conf"
