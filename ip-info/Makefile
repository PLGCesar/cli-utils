CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c99
TARGET  = ip-info
ALIAS   = ipinfo
SRC     = ip-info.c

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

# Compila e cria alias em 1 comando
all: $(BIN)
	@SHELL_RC="$${HOME}/.bashrc"; \
	if [ -f "$${HOME}/.zshrc" ]; then SHELL_RC="$${HOME}/.zshrc"; fi; \
	EXEC_PATH="$$(pwd)/$(BIN)"; \
	ALIAS_CMD="alias $(ALIAS)='$$EXEC_PATH'"; \
	if grep -q "alias $(ALIAS)=" "$$SHELL_RC" 2>/dev/null; then \
		echo "[+] Compilado com sucesso! (Alias '$(ALIAS)' ativo em $$SHELL_RC)"; \
	else \
		echo "$$ALIAS_CMD" >> "$$SHELL_RC"; \
		echo "[+] Compilado com sucesso E alias '$(ALIAS)' adicionado em $$SHELL_RC!"; \
		echo "[!] Roda 'source $$SHELL_RC' para usar o comando 'ipinfo'."; \
	fi

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

clean:
	$(RM) $(BIN)

.PHONY: all clean
