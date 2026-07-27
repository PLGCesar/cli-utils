# Compiler & Flags
CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99
TARGET = genpass
HOME_BIN := $(HOME)/$(TARGET)
RC_FILE := $(shell if [ -f "$$HOME/.zshrc" ]; then echo "$$HOME/.zshrc"; else echo "$$HOME/.bashrc"; fi)

all: $(TARGET) install_alias

# Compile target binary
$(TARGET): genpass.c
	$(CC) $(CFLAGS) genpass.c -o $(TARGET)

# Move binary to Home and add alias configuration
install_alias: $(TARGET)
	@cp -f $(TARGET) $(HOME_BIN)
	@chmod +x $(HOME_BIN)
	@for rc in ~/.bashrc ~/.zshrc; do \
		if [ -f $$rc ]; then \
			grep -q "alias genpass=" $$rc 2>/dev/null || ( \
				echo "" >> $$rc && \
				echo "# genpass CLI alias" >> $$rc && \
				echo 'alias genpass="$(HOME_BIN)"' >> $$rc \
			); \
		fi \
	done
	@echo "\033[32m[✓] Compiled and alias created successfully in $(RC_FILE)!\033[0m"
	@echo "Run 'source $(RC_FILE)' and test 'genpass' from ANY folder!"

clean:
	rm -f $(TARGET) $(HOME_BIN)

.PHONY: all install_alias clean
