CC ?= gcc
CSTD := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
WARN := -Wall -Wextra -Werror -Wconversion -Wshadow -pedantic
OPT := -O2 -g
BUILD_DIR ?= build
INCLUDES := -Iinclude -I$(BUILD_DIR)
THREADS := -pthread

CFLAGS ?= $(CSTD) $(WARN) $(OPT) $(INCLUDES) $(THREADS)
LDFLAGS ?= $(THREADS)
LDLIBS ?= -lsqlite3

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
BIN := $(BUILD_DIR)/server

TEST_SRC := $(wildcard tests/*.c)
# Exclude test files that define their own main from lib objects when linking tests.
# We link server objects (except main.o) + one test main at a time via explicit rules below.
SERVER_OBJ_NO_MAIN := $(filter-out $(BUILD_DIR)/main.o,$(OBJ))
TEST_BIN := $(BUILD_DIR)/test_server
TEST_API_BIN := $(BUILD_DIR)/test_api

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin

RUN_BIND ?= 127.0.0.1
RUN_PORT ?= 8080
RUN_THREADS ?= 8
RUN_DB ?= ./data/app.db
RUN_API_KEY ?=
RUN_FLAGS = $(if $(strip $(RUN_API_KEY)),--api-key $(strip $(RUN_API_KEY)))

.PHONY: all clean test sanitize format-check install help run

all: $(BIN)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Embedded frontend: generated header keeps the binary self-contained
# (no runtime dependency on the web/ directory).
$(BUILD_DIR)/frontend.h: web/index.html scripts/embed.py | $(BUILD_DIR)
	python3 scripts/embed.py web/index.html $@ frontend_html

$(BUILD_DIR)/api.o: $(BUILD_DIR)/frontend.h

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf build build-asan

# Unit + integration tests (C, sqlite linked).
test: $(TEST_BIN) $(TEST_API_BIN)
	./$(TEST_BIN)
	@if [ -x ./$(TEST_API_BIN) ]; then ./$(TEST_API_BIN); fi

$(TEST_BIN): $(SERVER_OBJ_NO_MAIN) tests/test_server.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SERVER_OBJ_NO_MAIN) tests/test_server.c -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_API_BIN): $(SERVER_OBJ_NO_MAIN) tests/test_api.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SERVER_OBJ_NO_MAIN) tests/test_api.c -o $@ $(LDFLAGS) $(LDLIBS)

# ASan+UBSan in an isolated dir so instrumented objects never mix with normal ones.
sanitize:
	$(MAKE) BUILD_DIR=build-asan test CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"
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
# Overrides: make run RUN_PORT=8081 RUN_DB=./data/dev.db RUN_API_KEY=secret
run: $(BIN)
	mkdir -p $(dir $(RUN_DB))
	./$(BIN) --bind $(RUN_BIND) --port $(RUN_PORT) --threads $(RUN_THREADS) --db $(RUN_DB) $(RUN_FLAGS)

help:
	@echo "Targets: all | test | sanitize | format-check | clean | install | run"
	@echo "  make run [RUN_BIND=.. RUN_PORT=.. RUN_THREADS=.. RUN_DB=..]"
