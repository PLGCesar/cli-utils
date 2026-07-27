# Compiler & Flags
CC ?= gcc
CFLAGS ?= -O3 -flto -march=native -Wall -Wextra -std=c99
LDFLAGS ?= -lm

# Target binary name
TARGET = calc

all: $(TARGET)

$(TARGET): calc.c
	$(CC) $(CFLAGS) calc.c -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

# Cross-compilation helpers (e.g. make arm64 / make arm32)
arm64:
	$(MAKE) CC=aarch64-linux-gnu-gcc CFLAGS="-O3 -flto -std=c99"

arm32:
	$(MAKE) CC=arm-linux-gnueabihf-gcc CFLAGS="-O3 -flto -std=c99"
