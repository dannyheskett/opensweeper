# ---------------------------------------------------------------------------
# opensweeper build
# ---------------------------------------------------------------------------
RAYLIB       := third_party/raylib-install
RAYLIB_WIN64 := third_party/raylib-install-win64
RAYLIB_WIN32 := third_party/raylib-install-win32

MINIH264_INC := third_party/minih264
MINIMP4_INC  := third_party/minimp4

SRC := src/main.c src/game.c src/input.c src/render.c src/sound.c \
       src/recorder.c src/encode_h264.c src/encode_mux.c

CFLAGS_COMMON := -std=c99 -Wall -Wextra -I$(MINIH264_INC) -I$(MINIMP4_INC) -Isrc

OPENSWEEPER_VERSION ?= $(shell git tag --list 'release-*' 2>/dev/null | sed -n 's/^release-\([1-9][0-9]*\)$$/\1/p' | sort -n | tail -1 | grep . || echo 0)
VERSION_SLUG        := build-$(OPENSWEEPER_VERSION)

# ---------------------------------------------------------------------------
# Linux
# ---------------------------------------------------------------------------
CFLAGS   := $(CFLAGS_COMMON) -O2 -I$(RAYLIB)/include
RELFLAGS := $(CFLAGS_COMMON) -O3 -I$(RAYLIB)/include
LDFLAGS  := -L$(RAYLIB)/lib -Wl,-Bstatic -lraylib -Wl,-Bdynamic -lm -lpthread -ldl -lrt -lX11

OBJ_DIR     := build/obj
REL_OBJ_DIR := build/obj-release
OBJ     := $(SRC:src/%.c=$(OBJ_DIR)/%.o)
REL_OBJ := $(SRC:src/%.c=$(REL_OBJ_DIR)/%.o)

OUT         := build/opensweeper
OUT_RELEASE := build/opensweeper-release

all: $(OUT)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	gcc $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT): $(OBJ)
	gcc $(OBJ) -o $@ $(LDFLAGS)

$(REL_OBJ_DIR)/%.o: src/%.c | $(REL_OBJ_DIR)
	gcc $(RELFLAGS) -MMD -MP -c $< -o $@

$(OUT_RELEASE): $(REL_OBJ)
	gcc $(REL_OBJ) -o $@ $(LDFLAGS)

release: $(OUT_RELEASE)

run: $(OUT)
	./$(OUT)

run-release: $(OUT_RELEASE)
	./$(OUT_RELEASE)

# ---------------------------------------------------------------------------
# Windows cross-compile
# ---------------------------------------------------------------------------
WIN_CFLAGS  := $(CFLAGS_COMMON) -O2
WIN_LDFLAGS := -Wl,-Bstatic -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -Wl,-Bdynamic -mwindows -static -static-libgcc

WIN64_CC := x86_64-w64-mingw32-gcc
WIN32_CC := i686-w64-mingw32-gcc

WIN64_OBJ_DIR := build/obj-win64
WIN32_OBJ_DIR := build/obj-win32
WIN64_OBJ := $(SRC:src/%.c=$(WIN64_OBJ_DIR)/%.o)
WIN32_OBJ := $(SRC:src/%.c=$(WIN32_OBJ_DIR)/%.o)

OUT_WIN64 := build/opensweeper-x64.exe
OUT_WIN32 := build/opensweeper-x86.exe

windows: $(OUT_WIN64) $(OUT_WIN32)

$(WIN64_OBJ_DIR)/%.o: src/%.c | $(WIN64_OBJ_DIR)
	$(WIN64_CC) $(WIN_CFLAGS) -I$(RAYLIB_WIN64)/include -MMD -MP -c $< -o $@

$(OUT_WIN64): $(WIN64_OBJ)
	$(WIN64_CC) $(WIN64_OBJ) -o $@ -L$(RAYLIB_WIN64)/lib $(WIN_LDFLAGS)

$(WIN32_OBJ_DIR)/%.o: src/%.c | $(WIN32_OBJ_DIR)
	$(WIN32_CC) $(WIN_CFLAGS) -I$(RAYLIB_WIN32)/include -MMD -MP -c $< -o $@

$(OUT_WIN32): $(WIN32_OBJ)
	$(WIN32_CC) $(WIN32_OBJ) -o $@ -L$(RAYLIB_WIN32)/lib $(WIN_LDFLAGS)

# ---------------------------------------------------------------------------
# macOS universal
# ---------------------------------------------------------------------------
RAYLIB_MAC  := third_party/raylib-install-mac
MAC_CC      := clang
MAC_ARCHES  := -arch arm64 -arch x86_64
MAC_CFLAGS  := $(CFLAGS_COMMON) -O2 $(MAC_ARCHES) -I$(RAYLIB_MAC)/include
MAC_LDFLAGS := $(MAC_ARCHES) -L$(RAYLIB_MAC)/lib -lraylib -lpthread \
               -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL

MAC_OBJ_DIR := build/obj-mac
MAC_OBJ := $(SRC:src/%.c=$(MAC_OBJ_DIR)/%.o)
OUT_MAC := build/opensweeper-mac

mac: $(OUT_MAC)

$(MAC_OBJ_DIR)/%.o: src/%.c | $(MAC_OBJ_DIR)
	$(MAC_CC) $(MAC_CFLAGS) -MMD -MP -c $< -o $@

$(OUT_MAC): $(MAC_OBJ)
	$(MAC_CC) $(MAC_OBJ) -o $@ $(MAC_LDFLAGS)

# ---------------------------------------------------------------------------
# Web build (WebAssembly via Emscripten). CI-only: needs emcc (emsdk) on PATH.
# Run scripts/build_raylib_web.sh first to produce the raylib web archive.
# ---------------------------------------------------------------------------
RAYLIB_WEB := third_party/raylib-install-web

WEB_SRC     := $(filter-out src/encode_h264.c src/encode_mux.c,$(SRC))
WEB_CFLAGS  := -std=c99 -Wall -Wextra -Isrc -DPLATFORM_WEB -Os \
               -I$(RAYLIB_WEB)/include
# Fixed memory (not ALLOW_MEMORY_GROWTH): a growable WASM heap yields resizable
# ArrayBuffers, which modern browsers reject in WebGL texImage2D. 64 MiB is ample
# for this game.
WEB_LDFLAGS := -Os -sUSE_GLFW=3 -sINITIAL_MEMORY=67108864 \
               --shell-file web/shell.html $(RAYLIB_WEB)/lib/libraylib.a

WEB_OUT_DIR := build/web
WEB_OUT     := $(WEB_OUT_DIR)/opensweeper.html

web: $(WEB_OUT)

$(WEB_OUT): $(WEB_SRC) $(wildcard src/*.h) web/shell.html | $(WEB_OUT_DIR)
	emcc $(WEB_CFLAGS) $(WEB_SRC) -o $@ $(WEB_LDFLAGS)
	@echo "[web] built $@"

# ---------------------------------------------------------------------------
# Unit tests (no Raylib)
# ---------------------------------------------------------------------------
TEST_BIN := build/test_game

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): tests/test_game.c src/game.c src/game.h | $(OBJ_DIR)
	gcc $(CFLAGS_COMMON) -O0 -g tests/test_game.c src/game.c -o $(TEST_BIN)

# ---------------------------------------------------------------------------
# Distribution archives
# ---------------------------------------------------------------------------
DIST    := dist
STAGING := build/staging
DOCS    := README.md LICENSE

dist: dist-linux dist-windows dist-mac

dist-linux: release
	@rm -rf $(STAGING)/linux && mkdir -p $(STAGING)/linux/opensweeper-$(VERSION_SLUG) $(DIST)
	cp $(OUT_RELEASE) $(STAGING)/linux/opensweeper-$(VERSION_SLUG)/opensweeper
	cp $(DOCS) $(STAGING)/linux/opensweeper-$(VERSION_SLUG)/
	(cd $(STAGING)/linux && tar -czf ../../../$(DIST)/opensweeper-$(VERSION_SLUG)-linux-x86_64.tar.gz opensweeper-$(VERSION_SLUG))

dist-windows: $(OUT_WIN64) $(OUT_WIN32)
	@rm -rf $(STAGING)/win && mkdir -p $(DIST) \
	    $(STAGING)/win/opensweeper-$(VERSION_SLUG)-x64 \
	    $(STAGING)/win/opensweeper-$(VERSION_SLUG)-x86
	cp $(OUT_WIN64) $(STAGING)/win/opensweeper-$(VERSION_SLUG)-x64/opensweeper.exe
	cp $(DOCS) $(STAGING)/win/opensweeper-$(VERSION_SLUG)-x64/
	cp $(OUT_WIN32) $(STAGING)/win/opensweeper-$(VERSION_SLUG)-x86/opensweeper.exe
	cp $(DOCS) $(STAGING)/win/opensweeper-$(VERSION_SLUG)-x86/
	(cd $(STAGING)/win && zip -qr ../../../$(DIST)/opensweeper-$(VERSION_SLUG)-windows-x64.zip opensweeper-$(VERSION_SLUG)-x64)
	(cd $(STAGING)/win && zip -qr ../../../$(DIST)/opensweeper-$(VERSION_SLUG)-windows-x86.zip opensweeper-$(VERSION_SLUG)-x86)

dist-mac: $(OUT_MAC)
	@rm -rf $(STAGING)/mac && mkdir -p $(STAGING)/mac/opensweeper-$(VERSION_SLUG) $(DIST)
	cp $(OUT_MAC) $(STAGING)/mac/opensweeper-$(VERSION_SLUG)/opensweeper
	codesign --force --sign - $(STAGING)/mac/opensweeper-$(VERSION_SLUG)/opensweeper
	cp $(DOCS) $(STAGING)/mac/opensweeper-$(VERSION_SLUG)/
	(cd $(STAGING)/mac && zip -qr ../../../$(DIST)/opensweeper-$(VERSION_SLUG)-macos-universal.zip opensweeper-$(VERSION_SLUG))

# The web build ships as a zip of the HTML/JS/WASM (serve over http to play).
dist-web: $(WEB_OUT)
	@rm -rf $(STAGING)/web && mkdir -p $(STAGING)/web/opensweeper-$(VERSION_SLUG)-web $(DIST)
	cp $(WEB_OUT_DIR)/opensweeper.html $(WEB_OUT_DIR)/opensweeper.js $(WEB_OUT_DIR)/opensweeper.wasm \
	    $(STAGING)/web/opensweeper-$(VERSION_SLUG)-web/
	cp $(DOCS) $(STAGING)/web/opensweeper-$(VERSION_SLUG)-web/
	(cd $(STAGING)/web && zip -qr ../../../$(DIST)/opensweeper-$(VERSION_SLUG)-web-wasm.zip opensweeper-$(VERSION_SLUG)-web)

# ---------------------------------------------------------------------------
$(OBJ_DIR) $(REL_OBJ_DIR) $(WIN64_OBJ_DIR) $(WIN32_OBJ_DIR) $(MAC_OBJ_DIR) $(WEB_OUT_DIR):
	mkdir -p $@

clean:
	rm -rf build dist

-include $(OBJ:.o=.d) $(REL_OBJ:.o=.d) $(WIN64_OBJ:.o=.d) $(WIN32_OBJ:.o=.d) $(MAC_OBJ:.o=.d)

.PHONY: all run release run-release windows mac web test dist dist-linux dist-windows dist-mac dist-web clean
