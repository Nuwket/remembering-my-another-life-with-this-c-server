CC ?= gcc
CSTD := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
WARN := -Wall -Wextra -Werror -Wconversion -Wshadow -pedantic
OPT := -O2 -g
INCLUDES := -Iinclude
THREADS := -pthread

CFLAGS ?= $(CSTD) $(WARN) $(OPT) $(INCLUDES) $(THREADS)
LDFLAGS ?= $(THREADS)
LDLIBS ?= -lsqlite3

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/server

TEST_SRC := $(wildcard tests/*.c)
# Exclude test files that define their own main from lib objects when linking tests.
# We link server objects (except main.o) + one test main at a time via explicit rules below.
SERVER_OBJ_NO_MAIN := $(filter-out build/main.o,$(OBJ))
TEST_BIN := build/test_server
TEST_API_BIN := build/test_api

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin

RUN_BIND ?= 127.0.0.1
RUN_PORT ?= 8080
RUN_THREADS ?= 8
RUN_DB ?= ./data/app.db

.PHONY: all clean test sanitize format-check install help run

all: $(BIN)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

build:
	mkdir -p build

clean:
	rm -rf build

# Unit + integration tests (C, sqlite linked).
test: $(TEST_BIN) $(TEST_API_BIN)
	./$(TEST_BIN)
	@if [ -x ./$(TEST_API_BIN) ]; then ./$(TEST_API_BIN); fi

$(TEST_BIN): $(SERVER_OBJ_NO_MAIN) tests/test_server.c | build
	$(CC) $(CFLAGS) $(SERVER_OBJ_NO_MAIN) tests/test_server.c -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_API_BIN): $(SERVER_OBJ_NO_MAIN) tests/test_api.c | build
	$(CC) $(CFLAGS) $(SERVER_OBJ_NO_MAIN) tests/test_api.c -o $@ $(LDFLAGS) $(LDLIBS)

# ASan+UBSan build + run. Fails on leak/UB. Threading is exercised here as well.
sanitize:
	$(MAKE) clean
	$(MAKE) $(BIN) $(TEST_BIN) $(TEST_API_BIN) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"
	./$(TEST_BIN)
	@if [ -x ./$(TEST_API_BIN) ]; then ./$(TEST_API_BIN); fi
	@echo "[sanitize] tests passed under ASan+UBSan"

format-check:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format --dry-run --Werror src/*.c include/*.h tests/*.c 2>&1; \
	else \
		echo "[format-check] clang-format not installed, skipping (CI should enforce)"; \
	fi

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/c-echo-server

# Run everything: build + start server with sane defaults.
# Overrides: make run RUN_PORT=8081 RUN_DB=./data/dev.db
run: $(BIN)
	mkdir -p $(dir $(RUN_DB))
	./$(BIN) --bind $(RUN_BIND) --port $(RUN_PORT) --threads $(RUN_THREADS) --db $(RUN_DB)

help:
	@echo "Targets: all | test | sanitize | format-check | clean | install | run"
	@echo "  make run [RUN_BIND=.. RUN_PORT=.. RUN_THREADS=.. RUN_DB=..]"
