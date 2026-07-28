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

# Build binary and hook shell alias in a single command
all: $(BIN)
	@SHELL_RC="$${HOME}/.bashrc"; \
	if [ -f "$${HOME}/.zshrc" ]; then SHELL_RC="$${HOME}/.zshrc"; fi; \
	EXEC_PATH="$$(pwd)/$(BIN)"; \
	ALIAS_CMD="alias $(TARGET)='$$EXEC_PATH'"; \
	if grep -q "alias $(TARGET)=" "$$SHELL_RC" 2>/dev/null; then \
		echo "[+] Successfully built! (Alias '$(TARGET)' is already active in $$SHELL_RC)"; \
	else \
		echo "$$ALIAS_CMD" >> "$$SHELL_RC"; \
		echo "[+] Successfully built AND hooked alias '$(TARGET)' to $$SHELL_RC!"; \
		echo "[!] Run 'source $$SHELL_RC' or restart your terminal to activate '$(TARGET)'."; \
	fi

# Compile target binary
$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

# Clean compiled build artifacts
clean:
	$(RM) $(BIN)

.PHONY: all clean
