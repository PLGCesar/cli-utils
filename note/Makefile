CC ?= gcc
CFLAGS ?= -Wall -Wextra -O3 -std=c99 -Wno-format-truncation
TARGET = note
HOME_BIN := $(HOME)/note

all: $(TARGET) install_alias

# Compile binary
$(TARGET): note.c
	$(CC) $(CFLAGS) note.c -o $(TARGET)

# Move binary to ~/note and add direct alias
install_alias: $(TARGET)
	@cp -f $(TARGET) $(HOME_BIN)
	@chmod +x $(HOME_BIN)
	@for rc in ~/.bashrc ~/.zshrc; do \
		if [ -f $$rc ]; then \
			grep -q "alias note=" $$rc 2>/dev/null || ( \
				echo "" >> $$rc && \
				echo "# note CLI tool alias" >> $$rc && \
				echo 'alias note="$(HOME_BIN)"' >> $$rc \
			); \
		fi \
	done
	@echo "\033[32m[✓] Binary copied to $(HOME_BIN) and alias created successfully!\033[0m"
	@echo "Run 'source ~/.bashrc' (or source ~/.zshrc) and test 'note' from ANY folder!"

clean:
	rm -f $(TARGET) $(HOME_BIN)

.PHONY: all install_alias clean
