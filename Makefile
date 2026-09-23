CC ?= gcc
CSTD := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
WARN := -Wall -Wextra -Werror -Wconversion -Wshadow -pedantic
OPT := -O2 -g
INCLUDES := -Iinclude
THREADS := -pthread

CFLAGS ?= $(CSTD) $(WARN) $(OPT) $(INCLUDES) $(THREADS)
LDFLAGS ?= $(THREADS)

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/server

TEST_SRC := $(wildcard tests/*.c)
# Exclude test files that define their own main from lib objects when linking tests.
# We link server objects (except main.o) + one test main at a time via explicit rules below.
SERVER_OBJ_NO_MAIN := $(filter-out build/main.o,$(OBJ))
TEST_BIN := build/test_server

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin

.PHONY: all clean test sanitize format-check install help

all: $(BIN)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

# Unit + integration tests (C, no external deps).
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(SERVER_OBJ_NO_MAIN) tests/test_server.c | build
	$(CC) $(CFLAGS) $(SERVER_OBJ_NO_MAIN) tests/test_server.c -o $@ $(LDFLAGS)

# ASan+UBSan build + run. Fails on leak/UB. Threading is exercised here as well.
sanitize:
	$(MAKE) clean
	$(MAKE) $(BIN) $(TEST_BIN) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"
	./$(TEST_BIN)
	@echo "[sanitize] test_server passed under ASan+UBSan"

format-check:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format --dry-run --Werror src/*.c include/*.h tests/*.c 2>&1; \
	else \
		echo "[format-check] clang-format not installed, skipping (CI should enforce)"; \
	fi

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/c-echo-server

help:
	@echo "Targets: all | test | sanitize | format-check | clean | install"
