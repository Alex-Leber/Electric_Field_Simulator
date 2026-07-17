# ============================================================
#  Electric Field Simulator - Emscripten / WebAssembly build
# ============================================================
#  Run these from THIS folder (the one with main.c) in Git Bash:
#    make          compile main.c  ->  index.js + index.wasm
#    make serve    start a local web server on $(PORT)
#    make run      build, then serve
#    make clean    remove generated index.js / index.wasm
#    make raylib   rebuild libraylib.a from ../raylib/src
#                  (use if you upgrade emsdk and hit linker errors)
#
#  NOTE: the indented recipe lines below MUST start with a TAB,
#  not spaces. If you edit this file, keep the tabs.
# ============================================================

# --- Tools (require: source ../emsdk/emsdk_env.sh first) ---
EMCC   := emcc
EMAR   := emar
PYTHON := python3

# --- Project layout ---
SRC        := main.c
OUT        := index.js          # emcc emits index.js AND index.wasm
RAYLIB_LIB := libraylib.a
RAYLIB_SRC := ../raylib/src      # cloned raylib source tree
PORT       := 8000
ASSETS     := --preload-file Fonts

# --- Compiler / linker flags (must match your working build) ---
CFLAGS  := -I. -Os -Wall -DPLATFORM_WEB
LDFLAGS := -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1

# ------------------------------------------------------------
.PHONY: all build serve run clean raylib

all: build

build: $(OUT)

# Only recompiles when main.c or the library actually change.
$(OUT): $(SRC) $(RAYLIB_LIB)
	$(EMCC) $(SRC) -o $(OUT) $(RAYLIB_LIB) $(CFLAGS) $(LDFLAGS) $(ASSETS)
	@echo ""
	@echo "Built $(OUT) + index.wasm  ->  'make serve', then http://localhost:$(PORT)/"

# Serve this folder over HTTP (index.html loads automatically).
serve:
	@echo "Serving http://localhost:$(PORT)/   (Ctrl+C to stop)"
	$(PYTHON) -m http.server $(PORT)

# Convenience: build then serve in one command.
run: build serve

clean:
	rm -f index.js index.wasm

# Rebuild libraylib.a from source with the CURRENT emsdk, then copy it
# next to main.c. Run this once after any 'emsdk activate' / upgrade.
raylib:
	cd $(RAYLIB_SRC) && \
	$(EMCC) -c rcore.c     -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 && \
	$(EMCC) -c rshapes.c   -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 && \
	$(EMCC) -c rtextures.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 && \
	$(EMCC) -c rtext.c     -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 && \
	$(EMCC) -c rmodels.c   -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 && \
	$(EMCC) -c utils.c     -Os -Wall -DPLATFORM_WEB && \
	$(EMCC) -c raudio.c    -Os -Wall -DPLATFORM_WEB && \
	$(EMAR) rcs libraylib.a rcore.o rshapes.o rtextures.o rtext.o rmodels.o utils.o raudio.o && \
	rm -f *.o
	cp $(RAYLIB_SRC)/libraylib.a ./$(RAYLIB_LIB)
	@echo "Rebuilt $(RAYLIB_LIB) from source."