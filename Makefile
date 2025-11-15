CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = touch-timeout

all: $(TARGET)

$(TARGET): touch-timeout.c
	$(CC) $(CFLAGS) -o $(TARGET) touch-timeout.c

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
