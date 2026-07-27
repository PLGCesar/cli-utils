# Target compiler and flags
CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99
TARGET = markwarp
HOME_BIN := $(HOME)/markwarp_bin
DIR := $(shell pwd)

all: $(TARGET) install_alias

# Compile the binary
$(TARGET): markwarp.c
	$(CC) $(CFLAGS) markwarp.c -o $(TARGET)

# Copy binary to Home directory and configure shell alias
install_alias: $(TARGET)
	@cp -f $(TARGET) $(HOME_BIN)
	@chmod +x $(HOME_BIN)
	@for rc in ~/.bashrc ~/.zshrc; do \
		if [ -f $$rc ]; then \
			grep -q "alias markwarp=" $$rc 2>/dev/null || ( \
				echo "" >> $$rc && \
				echo "# markwarp CLI configuration" >> $$rc && \
				echo 'alias markwarp="$(HOME_BIN)"' >> $$rc && \
				echo 'source "$(DIR)/markwarp.sh"' >> $$rc \
			); \
		fi \
	done
	@echo "\033[32m[✓] Compiled and configured successfully with alias!\033[0m"
	@echo "Run 'source ~/.bashrc' (or source ~/.zshrc) to start using it!"

clean:
	rm -f $(TARGET) $(HOME_BIN)

.PHONY: all install_alias clean
