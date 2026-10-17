CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -g
CPPFLAGS ?= -Iinclude
LDFLAGS ?=

BUILD_DIR := build
BIN := $(BUILD_DIR)/meridiandtl
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))

.PHONY: all clean run test-c

all: $(BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

run: $(BIN)
	./$(BIN) routed

test-c: $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/c/risk_test.c $(filter-out src/main.c src/runtime.c,$(SRC)) $(LDFLAGS) -o $(BUILD_DIR)/risk_test
	./$(BUILD_DIR)/risk_test

clean:
	rm -rf $(BUILD_DIR)
