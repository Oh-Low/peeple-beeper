CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -Ifirmware/include -Ihal -O1

PKG_CONFIG ?= pkg-config
PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig:/opt/local/lib/pkgconfig:$(PKG_CONFIG_PATH)
export PKG_CONFIG_PATH

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
ifeq ($(SDL_CFLAGS),)
  SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
  SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
endif
ifeq ($(SDL_CFLAGS),)
  SDL_PREFIX := $(CURDIR)/build/sdl-prefix
  ifneq ($(wildcard $(SDL_PREFIX)/lib/libSDL2.a),)
    SDL_CFLAGS := -I$(SDL_PREFIX)/include -I$(SDL_PREFIX)/include/SDL2 -D_THREAD_SAFE
    SDL_LIBS := -L$(SDL_PREFIX)/lib -lSDL2 -lm \
      -Wl,-framework,CoreAudio -Wl,-framework,AudioToolbox \
      -Wl,-weak_framework,CoreHaptics -Wl,-weak_framework,GameController \
      -Wl,-framework,ForceFeedback -lobjc -Wl,-framework,CoreVideo \
      -Wl,-framework,Cocoa -Wl,-framework,Carbon -Wl,-framework,IOKit \
      -Wl,-weak_framework,QuartzCore -Wl,-weak_framework,Metal
  endif
endif

FW_SRC := $(wildcard firmware/src/*.c)
ART_SRC := firmware/art/art.c
HOST_SRC := $(wildcard firmware/host/*.c)
HAL_EMU_SRC := hal/emu/emu_hal.c
HAL_BEEPER_SRC := hal/beeper/beeper_hal.c
FW_LINK := $(FW_SRC) $(ART_SRC) $(HOST_SRC) $(HAL_EMU_SRC)
FW_CORE := $(FW_SRC) $(ART_SRC)
TEST_DIR := firmware/tests
BUILD := build

.PHONY: all test emu run nrf-check clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/page_test: $(TEST_DIR)/page_test.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/page_test.c $(FW_LINK)

$(BUILD)/store_test: $(TEST_DIR)/store_test.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/store_test.c $(FW_LINK)

$(BUILD)/mesh_test: $(TEST_DIR)/mesh_test.c emu/seed.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Iemu -Ihal/emu -o $@ $(TEST_DIR)/mesh_test.c emu/seed.c $(FW_LINK)

$(BUILD)/locate_test: $(TEST_DIR)/locate_test.c $(FW_LINK) $(HAL_BEEPER_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -Ihal/beeper -o $@ $(TEST_DIR)/locate_test.c $(FW_LINK) $(HAL_BEEPER_SRC)

test: $(BUILD)/page_test $(BUILD)/store_test $(BUILD)/mesh_test $(BUILD)/locate_test
	$(BUILD)/page_test
	$(BUILD)/store_test
	$(BUILD)/mesh_test
	$(BUILD)/locate_test

$(BUILD)/Roboto-Regular.ttf: emu/third_party/roboto/Roboto-Regular.ttf | $(BUILD)
	cp $< $@

$(BUILD)/peeple-emu: emu/main.c emu/font.c emu/seed.c $(FW_LINK) $(BUILD)/Roboto-Regular.ttf | $(BUILD)
	@if [ -z "$(SDL_CFLAGS)" ]; then \
	  echo "SDL2 is required to build the emulator."; \
	  echo "  macOS:    brew install sdl2 pkg-config"; \
	  echo "  Debian:   sudo apt install libsdl2-dev pkg-config"; \
	  echo "  Fedora:   sudo dnf install SDL2-devel pkgconf-pkg-config"; \
	  echo "  MSYS2:    pacman -S mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-pkgconf"; \
	  exit 1; \
	fi
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Iemu -Ihal/emu -o $@ emu/main.c emu/font.c emu/seed.c $(FW_LINK) $(SDL_LIBS)

emu: $(BUILD)/peeple-emu

run: $(BUILD)/peeple-emu
	./$(BUILD)/peeple-emu

nrf-check: | $(BUILD)
	mkdir -p $(BUILD)/nrf
	for f in $(FW_CORE) $(HAL_BEEPER_SRC); do \
	  $(CC) $(CFLAGS) -Ihal/beeper -c $$f -o $(BUILD)/nrf/$$(basename $$f .c).o; \
	done

clean:
	rm -rf $(BUILD)
