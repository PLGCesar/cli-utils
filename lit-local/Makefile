# Compiler & Flags
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c99 -Wno-format-truncation
TARGET  = lit
ALIAS   = lit
SRC     = lit.c

# OS Detection
ifeq ($(OS),Windows_NT)
    RM = del /Q /F
    EXE = .exe
else
    RM = rm -f
    EXE =
endif

BIN = $(TARGET)$(EXE)

# Build binary and hook shell alias in a single command
all: $(BIN)
	@SHELL_RC="$${HOME}/.bashrc"; \
	if [ -f "$${HOME}/.zshrc" ]; then SHELL_RC="$${HOME}/.zshrc"; fi; \
	touch "$$SHELL_RC" 2>/dev/null || true; \
	EXEC_PATH="$$(pwd)/$(BIN)"; \
	ALIAS_CMD="alias $(ALIAS)='$$EXEC_PATH'"; \
	if grep -q "alias $(ALIAS)=" "$$SHELL_RC" 2>/dev/null; then \
		echo "[+] Successfully built! (Alias '$(ALIAS)' is already active in $$SHELL_RC)"; \
	else \
		echo "$$ALIAS_CMD" >> "$$SHELL_RC" 2>/dev/null || true; \
		echo "[+] Successfully built AND hooked alias '$(ALIAS)' to $$SHELL_RC!"; \
		echo "[!] Run 'source $$SHELL_RC' or restart your terminal to use '$(ALIAS)'."; \
	fi

# Compile target binary
$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

# Clean build artifacts
clean:
	$(RM) $(BIN)

.PHONY: all clean
