# Target compiler and flags
CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99
TARGET = markwarp
HOME_BIN := $(HOME)/$(TARGET)
DIR := $(shell pwd)
RC_FILE := $(shell if [ -f "$$HOME/.zshrc" ]; then echo "$$HOME/.zshrc"; else echo "$$HOME/.bashrc"; fi)

all: $(TARGET) install_global

# Compile the binary
$(TARGET): markwarp.c
	$(CC) $(CFLAGS) markwarp.c -o $(TARGET)

# Copy binary to Home directory and configure shell environment
install_global: $(TARGET)
	@cp -f $(TARGET) $(HOME_BIN)
	@chmod +x $(HOME_BIN)
	@grep -q "markwarp.sh" $(RC_FILE) 2>/dev/null || ( \
		echo "" >> $(RC_FILE) && \
		echo "# markwarp configuration" >> $(RC_FILE) && \
		echo 'export PATH="$(HOME):$$PATH"' >> $(RC_FILE) && \
		echo 'source "$(DIR)/markwarp.sh"' >> $(RC_FILE) \
	)
	@echo "\033[32m[✓] Compiled and configured successfully in $(RC_FILE)!\033[0m"
	@echo "Run 'source $(RC_FILE)' and test 'mark' and 'warp' anywhere!"

clean:
	rm -f $(TARGET) $(HOME_BIN)

.PHONY: all install_global clean
