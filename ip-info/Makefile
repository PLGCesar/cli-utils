# Compiler & Flags
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c99
TARGET  = ip-info
ALIAS   = ipinfo
SRC     = ip-info.c

# OS Detection
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32 -liphlpapi
    RM = del /Q /F
    EXE = .exe
else
    LIBS =
    RM = rm -f
    EXE =
endif

BIN = $(TARGET)$(EXE)

# Comando único: Compila e configura o alias
all: $(BIN)
	@SHELL_RC="$${HOME}/.bashrc"; \
	if [ -f "$${HOME}/.zshrc" ]; then SHELL_RC="$${HOME}/.zshrc"; fi; \
	EXEC_PATH="$$(pwd)/$(BIN)"; \
	ALIAS_CMD="alias $(ALIAS)='$$EXEC_PATH'"; \
	if grep -q "alias $(ALIAS)=" "$$SHELL_RC" 2>/dev/null; then \
		echo "[+] Compilado com sucesso! (O alias '$(ALIAS)' ja esta em $$SHELL_RC)"; \
	else \
		echo "$$ALIAS_CMD" >> "$$SHELL_RC"; \
		echo "[+] Compilado com sucesso E alias '$(ALIAS)' adicionado em $$SHELL_RC!"; \
		echo "[!] Digite 'source $$SHELL_RC' para ativar o comando 'ipinfo'."; \
	fi

# Regra de compilação
$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

# Limpar o binário
clean:
	$(RM) $(BIN)

.PHONY: all clean
