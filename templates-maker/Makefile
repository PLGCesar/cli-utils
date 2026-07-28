# Compiler & Flags
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c99
TARGET  = template
SRC     = template.c

# OS Detection
ifeq ($(OS),Windows_NT)
    RM = del /Q /F
    EXE = .exe
else
    RM = rm -f
    EXE =
endif

BIN = $(TARGET)$(EXE)

# Compila e cria alias em 1 comando
all: $(BIN)
	@SHELL_RC="$${HOME}/.bashrc"; \
	if [ -f "$${HOME}/.zshrc" ]; then SHELL_RC="$${HOME}/.zshrc"; fi; \
	EXEC_PATH="$$(pwd)/$(BIN)"; \
	ALIAS_CMD="alias $(TARGET)='$$EXEC_PATH'"; \
	if grep -q "alias $(TARGET)=" "$$SHELL_RC" 2>/dev/null; then \
		echo "[+] Compilado! (Alias '$(TARGET)' ja ativo em $$SHELL_RC)"; \
	else \
		echo "$$ALIAS_CMD" >> "$$SHELL_RC"; \
		echo "[+] Compilado E alias '$(TARGET)' adicionado em $$SHELL_RC!"; \
		echo "[!] Digite 'source $$SHELL_RC' para usar o comando 'template'."; \
	fi

# Compilação
$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	$(RM) $(BIN)

.PHONY: all clean
