# snake-wasm

Classic Snake, with the entire game core written in **C** and compiled to
**WebAssembly** via emscripten. The C code owns all game logic (movement,
direction queue, food, growth, collision, scoring) *and* renders every frame
into an RGBA pixel buffer; JavaScript only blits that buffer to the canvas
and forwards input.

- `snake.wasm` — ~2.4 KB of game logic
- 21×21 grid, 24 px cells (504×504 canvas)
- Direction queue with 180°-turn rejection — fast double-tap turns work
- Score + best score (persisted in `localStorage`)
- WASD / arrow keys + touch-swipe steering
- rAF-driven loop with a `setInterval` fallback so the game keeps ticking in
  background/starved tabs, with clean pause/resume on tab switch
- No dependencies, no build step to play

## Play

Prebuilt artifacts are committed (`snake.js`, `snake.wasm`). Any static file
server works:

```sh
python3 -m http.server 8321
# open http://127.0.0.1:8321/
```

Or just open `index.html` in a browser.

Controls: **WASD** / **arrow keys** to steer, **swipe** on touch devices.
180° turns are ignored; the input queue (depth 8) makes quick cornering
responsive.

## Rebuild

Requires [emsdk](https://github.com/emscripten-core/emsdk):

```sh
emcc src/snake.c -O2 \
  -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
  -s MODULARIZE=1 -s EXPORT_NAME="createSnake" \
  -s EXPORTED_RUNTIME_METHODS='["HEAPU8"]' \
  -s EXPORTED_FUNCTIONS='["_frame","_init_snake","_set_dir","_set_food","_get_score","_get_alive","_snake_buf","_snake_w","_snake_h"]' \
  -o snake.js
```

## Test

The core runs under Node (same WASM, same C code):

```sh
node test_node.js
```

Covers: initial render (color/layout), reverse-turn rejection, eating +
growth, wall death, self-collision, restart, and a 500-frame soak run.
A `_set_food(x, y)` export is exposed purely as a deterministic test hook.

## Layout

```
src/snake.c    C core: state machine, input queue, rendering (186 lines)
snake.js       emscripten glue — createSnake() factory
snake.wasm     compiled core (~2.4 KB)
index.html     browser shell: canvas blit, HUD, overlay, input
test_node.js   Node test harness (runs the WASM directly)
```
