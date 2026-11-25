/* Snake — compiled to WebAssembly with emscripten.
 *
 * Game logic + rendering in C. Rendering writes an RGBA byte buffer;
 * a thin JS layer (index.html) pushes it to a <canvas> each frame.
 *
 * Exports: init_snake() set_dir(int) set_food(int,int) frame(double now_ms)
 *          get_score() get_alive() snake_buf() snake_w() snake_h()
 */
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define GRID 21
#define CELL 24
#define W (GRID * CELL)
#define H (GRID * CELL)

/* colors (24-bit; unpacked per pixel in render) */
#define BG1C    0x161927
#define BG2C    0x1b1f30
#define BODYC   0x3de07a
#define HEADC   0xb4ffc6
#define FOODC   0xffa492
#define FOODR   0xff4d6d

static int snake_x[GRID * GRID * 2];
static int snake_y[GRID * GRID * 2];
static int len = 4;                 /* current snake length */
static int head_dir = 1;            /* 0 up, 1 right, 2 down, 3 left */
static int iq_dir[8], iq_head = 0, iq_tail = 0;
static int food_x = GRID / 2, food_y = GRID / 2;
static int score = 0;
static int alive = 1;
static double last_tick = 0.0;
static double tick_ms = 130.0;
static uint32_t rng_state = 0x1234abcdu;

static uint8_t pixels[W * H * 4];
static uint8_t occ[GRID][GRID];     /* 0 empty 1 body 2 head 3 food */

static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int on_snake(int x, int y) {
    int i;
    for (i = 0; i < len; i++)
        if (snake_x[i] == x && snake_y[i] == y) return 1;
    return 0;
}

static void place_food(void) {
    int tries, i, x, y;
    for (tries = 0; tries < 2000; tries++) {
        x = (int)(rng() % GRID);
        y = (int)(rng() % GRID);
        if (x == food_x && y == food_y) continue;
        for (i = 0; i < len; i++)
            if (snake_x[i] == x && snake_y[i] == y) break;
        if (i >= len) { food_x = x; food_y = y; return; }
    }
    /* board nearly full fallback: spiral out from center (never actually hit) */
    food_x = (food_x + 1) % GRID;
    food_y = (food_y + 3) % GRID;
}

EMSCRIPTEN_KEEPALIVE
void init_snake(void) {
    len = 4;
    snake_x[0] = GRID / 2;     snake_y[0] = GRID / 2;
    snake_x[1] = GRID / 2 - 1; snake_y[1] = GRID / 2;
    snake_x[2] = GRID / 2 - 2; snake_y[2] = GRID / 2;
    snake_x[3] = GRID / 2 - 3; snake_y[3] = GRID / 2;
    head_dir = 1;
    iq_head = iq_tail = 0;
    score = 0;
    alive = 1;
    tick_ms = 130.0;
    last_tick = 0.0;
    place_food();
}

EMSCRIPTEN_KEEPALIVE
void set_dir(int d) {
    int ref;
    if (d < 0 || d > 3 || !alive) return;
    ref = (iq_head != iq_tail) ? iq_dir[((iq_tail - 1) % 8 + 8) % 8] : head_dir;
    if ((d ^ ref) == 2) return;          /* 180° reversal */
    if ((iq_tail - iq_head) >= 7) return;
    iq_dir[iq_tail % 8] = d;
    iq_tail++;
}

static void tick(void) {
    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};
    int i, nx, ny;

    if (iq_head != iq_tail) {
        int d = iq_dir[iq_head % 8];
        iq_head++;
        if (d != head_dir) head_dir = d;
    }
    nx = snake_x[0] + dx[head_dir];
    ny = snake_y[0] + dy[head_dir];

    if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) { alive = 0; return; }

    /* self collision: tail cell will move away this tick unless we grow */
    i = (nx == food_x && ny == food_y) ? len : len - 1;
    for (; i >= 0; i--)
        if (snake_x[i] == nx && snake_y[i] == ny) { alive = 0; return; }

    for (i = len - 1; i > 0; i--) { snake_x[i] = snake_x[i - 1]; snake_y[i] = snake_y[i - 1]; }
    snake_x[0] = nx;
    snake_y[0] = ny;

    if (nx == food_x && ny == food_y) {
        len++;
        score++;
        if (tick_ms > 70) tick_ms -= 2.5;
        place_food();
    }
}

static void build_occ(void) {
    int i;
    memset(occ, 0, sizeof occ);      /* clear stale cells so vacated tail/track doesn't ghost */
    for (i = 0; i < len; i++) occ[snake_x[i]][snake_y[i]] = 1;
    occ[snake_x[0]][snake_y[0]] = 2;
    occ[food_x][food_y] = 3;
}

static void render(void) {
    int x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            uint8_t *p = &pixels[((size_t)y * W + x) * 4];
            int cx, cy, v;
            int dr = (x % CELL) - CELL / 2, dc = (y % CELL) - CELL / 2;
            int col;
            cx = x / CELL; cy = y / CELL;
            if (x % CELL == CELL / 2) dc = -CELL / 2;   /* crisp circles */
            v = occ[cx][cy];
            if (v == 3) {
                int d2 = dr * dr + dc * dc;
                col = (d2 < 90) ? FOODC : (d2 < 121) ? FOODR : ((cx ^ cy) & 1) ? BG2C : BG1C;
            } else if (v == 2)      col = HEADC;
            else if (v == 1)        col = BODYC;
            else                    col = ((cx ^ cy) & 1) ? BG2C : BG1C;
            p[0] = (uint8_t)(col >> 16);
            p[1] = (uint8_t)(col >> 8);
            p[2] = (uint8_t)col;
            p[3] = 255;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void frame(double now_ms) {
    int guard = 0;
    if (last_tick == 0.0) { last_tick = now_ms; }        /* prime clock (init / tab resume) */
    else if (alive && now_ms - last_tick >= tick_ms) {
        while (alive && now_ms - last_tick >= tick_ms && guard++ < 4) {
            last_tick += tick_ms;
            tick();
        }
    }
    build_occ();
    render();
}

EMSCRIPTEN_KEEPALIVE
void set_food(int x, int y) {
    if (x < 0 || x >= GRID || y < 0 || y >= GRID) return;
    food_x = x; food_y = y;
}

EMSCRIPTEN_KEEPALIVE int get_score(void)  { return score; }
EMSCRIPTEN_KEEPALIVE int get_alive(void)  { return alive; }
EMSCRIPTEN_KEEPALIVE uint8_t *snake_buf(void) { return pixels; }
EMSCRIPTEN_KEEPALIVE int snake_w(void) { return W; }
EMSCRIPTEN_KEEPALIVE int snake_h(void) { return H; }
