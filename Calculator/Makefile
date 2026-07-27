# Compiler & Flags
CC ?= gcc
CFLAGS ?= -O3 -flto -march=native -Wall -Wextra -std=c99
LDFLAGS ?= -lm

# Target binary name & Home destination
TARGET = calc
HOME_BIN := $(HOME)/$(TARGET)
RC_FILE := $(shell if [ -f "$$HOME/.zshrc" ]; then echo "$$HOME/.zshrc"; else echo "$$HOME/.bashrc"; fi)

all: $(TARGET) install_alias

$(TARGET): calc.c
	$(CC) $(CFLAGS) calc.c -o $(TARGET) $(LDFLAGS)

# Copy binary to Home directory and configure shell alias
install_alias: $(TARGET)
	@cp -f $(TARGET) $(HOME_BIN)
	@chmod +x $(HOME_BIN)
	@for rc in ~/.bashrc ~/.zshrc; do \
		if [ -f $$rc ]; then \
			grep -q "alias calc=" $$rc 2>/dev/null || ( \
				echo "" >> $$rc && \
				echo "# calc CLI alias" >> $$rc && \
				echo 'alias calc="$(HOME_BIN)"' >> $$rc \
			); \
		fi \
	done
	@echo "\033[32m[✓] Compiled and alias created successfully in $(RC_FILE)!\033[0m"
	@echo "Run 'source $(RC_FILE)' and test 'calc' from ANY folder!"

clean:
	rm -f $(TARGET) $(HOME_BIN) *.o

# Cross-compilation helpers (e.g. make arm64 / make arm32)
arm64:
	$(MAKE) CC=aarch64-linux-gnu-gcc CFLAGS="-O3 -flto -std=c99"

arm32:
	$(MAKE) CC=arm-linux-gnueabihf-gcc CFLAGS="-O3 -flto -std=c99"

.PHONY: all install_alias clean arm64 arm32
