CC ?= cc
CFLAGS ?= -O2
WARNINGS := -std=c11 -Wall -Wextra -Wpedantic
BUILD_DIR := build

.PHONY: all clean

all: $(BUILD_DIR)/sdl3-four-motor-test $(BUILD_DIR)/sdl2-wine-path-probe

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/sdl3-four-motor-test: src/sdl3-four-motor-test.c | $(BUILD_DIR)
	$(CC) $(WARNINGS) $(CFLAGS) $< -o $@ $$(pkg-config --cflags --libs sdl3)

$(BUILD_DIR)/sdl2-wine-path-probe: src/sdl2-wine-path-probe.c | $(BUILD_DIR)
	$(CC) $(WARNINGS) $(CFLAGS) $< -o $@ $$(pkg-config --cflags --libs sdl2)

clean:
	rm -rf -- $(BUILD_DIR)
