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
ART_SRC := firmware/art/art.c firmware/art/beeple.c
HOST_SRC := $(wildcard firmware/host/*.c)
HAL_EMU_SRC := hal/emu/emu_hal.c
HAL_BEEPER_SRC := hal/beeper/beeper_hal.c
FW_LINK := $(FW_SRC) $(ART_SRC) $(HOST_SRC) $(HAL_EMU_SRC)
FW_CORE := $(FW_SRC) $(ART_SRC)
TEST_DIR := firmware/tests
BUILD := build

ARM_GCC := $(shell command -v arm-none-eabi-gcc 2>/dev/null)
ifeq ($(ARM_GCC),)
  ARM_GCC := $(firstword $(wildcard $(CURDIR)/.toolchain/*/bin/arm-none-eabi-gcc))
endif
ARM_BIN := $(dir $(ARM_GCC))
ARM_OBJCOPY := $(ARM_BIN)arm-none-eabi-objcopy
BRINGUP_CPU := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
BRINGUP_WARN := -std=c11 -Wall -Wextra -Werror -Os -ffreestanding -fno-builtin -nostdlib \
  -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections

BEEPER_CFLAGS := $(BRINGUP_CPU) -std=c11 -Wall -Wextra -Werror -Wno-sign-compare -Wno-format -Os \
  -ffunction-sections -fdata-sections -fno-unwind-tables \
  -Ifirmware/include -Ihal -Ihal/beeper
BEEPER_OBJ := hal/beeper/main.c hal/beeper/board.c hal/beeper/sx1262_setup.c $(HAL_BEEPER_SRC) $(FW_CORE)

.PHONY: all test emu run nrf-check arm-required bringup flash-bringup beeper flash-beeper clean

arm-required:
	@if [ -z "$(ARM_GCC)" ]; then \
	  echo "arm-none-eabi-gcc was not found."; \
	  echo "Install the Arm GNU Toolchain and put its bin directory on PATH."; \
	  echo "  https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"; \
	  echo "A copy unpacked at .toolchain/<version>/bin is also picked up."; \
	  exit 1; \
	fi

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/page_test: $(TEST_DIR)/page_test.c $(TEST_DIR)/page_harness.h $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/page_test.c $(FW_LINK)

$(BUILD)/relay_test: $(TEST_DIR)/relay_test.c $(TEST_DIR)/page_harness.h $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/relay_test.c $(FW_LINK)

$(BUILD)/store_test: $(TEST_DIR)/store_test.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/store_test.c $(FW_LINK)

$(BUILD)/mesh_test: $(TEST_DIR)/mesh_test.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/mesh_test.c $(FW_LINK)

$(BUILD)/seed_test: $(TEST_DIR)/seed_test.c emu/seed.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Iemu -Ihal/emu -o $@ $(TEST_DIR)/seed_test.c emu/seed.c $(FW_LINK)

$(BUILD)/pair_test: $(TEST_DIR)/pair_test.c emu/seed.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Iemu -Ihal/emu -o $@ $(TEST_DIR)/pair_test.c emu/seed.c $(FW_LINK)

$(BUILD)/gps_test: $(TEST_DIR)/gps_test.c $(FW_LINK) $(HAL_BEEPER_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -Ihal/beeper -o $@ $(TEST_DIR)/gps_test.c $(FW_LINK) $(HAL_BEEPER_SRC)

$(BUILD)/heading_test: $(TEST_DIR)/heading_test.c $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/heading_test.c $(FW_LINK)

$(BUILD)/receiver_test: $(TEST_DIR)/receiver_test.c $(TEST_DIR)/locate_harness.h $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/receiver_test.c $(FW_LINK)

$(BUILD)/locate_test: $(TEST_DIR)/locate_test.c $(TEST_DIR)/locate_harness.h $(FW_LINK) | $(BUILD)
	$(CC) $(CFLAGS) -Ihal/emu -o $@ $(TEST_DIR)/locate_test.c $(FW_LINK)

test: $(BUILD)/page_test $(BUILD)/relay_test $(BUILD)/store_test $(BUILD)/mesh_test $(BUILD)/seed_test $(BUILD)/pair_test $(BUILD)/gps_test $(BUILD)/heading_test $(BUILD)/receiver_test $(BUILD)/locate_test
	$(BUILD)/page_test
	$(BUILD)/relay_test
	$(BUILD)/store_test
	$(BUILD)/mesh_test
	$(BUILD)/seed_test
	$(BUILD)/pair_test
	$(BUILD)/gps_test
	$(BUILD)/heading_test
	$(BUILD)/receiver_test
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

$(BUILD)/bringup.uf2: hal/beeper/start.S bringup/bringup.c hal/beeper/sx1262_setup.c bringup/feather.ld bringup/uf2.py | $(BUILD) arm-required
	mkdir -p $(BUILD)/bringup
	$(ARM_GCC) $(BRINGUP_CPU) $(BRINGUP_WARN) -c hal/beeper/start.S -o $(BUILD)/bringup/start.o
	$(ARM_GCC) $(BRINGUP_CPU) $(BRINGUP_WARN) -Ihal/beeper -c bringup/bringup.c -o $(BUILD)/bringup/bringup.o
	$(ARM_GCC) $(BRINGUP_CPU) $(BRINGUP_WARN) -Ihal/beeper -c hal/beeper/sx1262_setup.c -o $(BUILD)/bringup/sx1262_setup.o
	$(ARM_GCC) $(BRINGUP_CPU) -ffreestanding -nostdlib -nostartfiles \
	  -Wl,-T,bringup/feather.ld -Wl,-Map,$(BUILD)/bringup.map -Wl,--gc-sections \
	  -o $(BUILD)/bringup.elf $(BUILD)/bringup/start.o $(BUILD)/bringup/bringup.o $(BUILD)/bringup/sx1262_setup.o
	$(ARM_OBJCOPY) -O binary $(BUILD)/bringup.elf $(BUILD)/bringup.bin
	python3 bringup/uf2.py $(BUILD)/bringup.bin $(BUILD)/bringup.uf2 0x26000

bringup: $(BUILD)/bringup.uf2

$(BUILD)/beeper.uf2: hal/beeper/start.S hal/beeper/main.c hal/beeper/board.c hal/beeper/sx1262_setup.c $(HAL_BEEPER_SRC) $(FW_CORE) hal/beeper/feather.ld bringup/uf2.py | $(BUILD) arm-required
	mkdir -p $(BUILD)/beeper
	$(ARM_GCC) $(BEEPER_CFLAGS) -c hal/beeper/start.S -o $(BUILD)/beeper/start.o
	for f in $(BEEPER_OBJ); do \
	  $(ARM_GCC) $(BEEPER_CFLAGS) -c $$f -o $(BUILD)/beeper/$$(basename $$f .c).o || exit 1; \
	done
	$(ARM_GCC) $(BRINGUP_CPU) -nostartfiles \
	  -specs=nano.specs -specs=nosys.specs \
	  -Wl,-T,hal/beeper/feather.ld -Wl,-Map,$(BUILD)/beeper.map -Wl,--gc-sections \
	  -o $(BUILD)/beeper.elf $(BUILD)/beeper/*.o
	$(ARM_OBJCOPY) -O binary $(BUILD)/beeper.elf $(BUILD)/beeper.bin
	python3 bringup/uf2.py $(BUILD)/beeper.bin $(BUILD)/beeper.uf2 0x26000

beeper: $(BUILD)/beeper.uf2

flash-bringup: $(BUILD)/bringup.uf2
flash-beeper: $(BUILD)/beeper.uf2

flash-bringup flash-beeper:
	@if [ ! -d /Volumes/FTHR840BOOT ]; then \
	  echo "FTHR840BOOT is not mounted."; \
	  echo "Double-tap the Feather reset button until a drive named FTHR840BOOT appears."; \
	  echo "Then run make $@"; \
	  exit 1; \
	fi
	@if [ "$@" = flash-beeper ]; then \
	  src=$(BUILD)/beeper.uf2; file=beeper.uf2; where=pager; \
	else \
	  src=$(BUILD)/bringup.uf2; file=bringup.uf2; where="bring-up screen"; \
	fi; \
	cp -X $$src /Volumes/FTHR840BOOT/$$file; status=$$?; \
	sleep 1; \
	if [ ! -d /Volumes/FTHR840BOOT ]; then \
	  echo "Copied. The Feather reboots into the $$where."; \
	elif [ $$status -ne 0 ]; then \
	  echo "Copy failed and FTHR840BOOT is still mounted."; \
	  exit 1; \
	else \
	  echo "Copied. The Feather reboots into the $$where."; \
	fi

clean:
	rm -rf $(BUILD)
