CC := cc
AR := ar
ARFLAGS := rcs

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

CFLAGS := -std=c99 -Wall -Wextra -Werror $(SDL_CFLAGS)

BUILD_DIR := build
BIN_DIR := bin
PLATFORM_OBJ := $(BUILD_DIR)/platform_sdl2.o
GAME_OBJ := $(BUILD_DIR)/game.o
MAIN_OBJ := $(BUILD_DIR)/main.o
PLATFORM_LIB := $(BUILD_DIR)/libplatform.a
TARGET := $(BIN_DIR)/asteroids

.PHONY: all clean run

all: $(TARGET)

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

$(PLATFORM_OBJ): src/platform/platform_sdl2.c src/platform/platform.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(GAME_OBJ): src/game.c src/game.h src/platform/platform.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(MAIN_OBJ): src/main.c src/game.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLATFORM_LIB): $(PLATFORM_OBJ) | $(BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $<

$(TARGET): $(MAIN_OBJ) $(GAME_OBJ) $(PLATFORM_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(MAIN_OBJ) $(GAME_OBJ) -L$(BUILD_DIR) -lplatform $(SDL_LIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
