CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99
TARGET = markwarp
DIR := $(shell pwd)
RC_FILE := $(shell if [ -f "$$HOME/.zshrc" ]; then echo "$$HOME/.zshrc"; else echo "$$HOME/.bashrc"; fi)

all: $(TARGET) auto_export

$(TARGET): markwarp.c
	$(CC) $(CFLAGS) markwarp.c -o $(TARGET)

auto_export:
	@grep -q "markwarp.sh" $(RC_FILE) 2>/dev/null || ( \
		echo "" >> $(RC_FILE) && \
		echo "# markwarp configuration" >> $(RC_FILE) && \
		echo 'export PATH="$(DIR):$$PATH"' >> $(RC_FILE) && \
		echo 'source "$(DIR)/markwarp.sh"' >> $(RC_FILE) \
	)
	@echo "\033[32m[✓] Compilado e configurado com sucesso em $(RC_FILE)!\033[0m"
	@echo "Agora e so abrir um novo terminal ou rodar: source $(RC_FILE)"

clean:
	rm -f $(TARGET)

.PHONY: all auto_export clean
