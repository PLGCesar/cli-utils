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

# Build binary and hook shell alias in a single command
all: $(BIN)
	@SHELL_RC="$${HOME}/.bashrc"; \
	if [ -f "$${HOME}/.zshrc" ]; then SHELL_RC="$${HOME}/.zshrc"; fi; \
	EXEC_PATH="$$(pwd)/$(BIN)"; \
	ALIAS_CMD="alias $(ALIAS)='$$EXEC_PATH'"; \
	if grep -q "alias $(ALIAS)=" "$$SHELL_RC" 2>/dev/null; then \
		echo "[+] Successfully built! (Alias '$(ALIAS)' is already active in $$SHELL_RC)"; \
	else \
		echo "$$ALIAS_CMD" >> "$$SHELL_RC"; \
		echo "[+] Successfully built AND hooked alias '$(ALIAS)' to $$SHELL_RC!"; \
		echo "[!] Run 'source $$SHELL_RC' or restart your terminal to use '$(ALIAS)'."; \
	fi

# Compile target binary
$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

# Clean build artifacts
clean:
	$(RM) $(BIN)

.PHONY: all clean
