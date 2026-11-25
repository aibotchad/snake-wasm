// Snake WASM — Node smoke test (factory + HEAPU8 exports)
const assert = require("assert");

(async () => {
  const m = await require("./snake.js")({
    print: (c) => console.log(c),
    printErr: (c) => console.error("ERR:", c),
  });

  const W = m._snake_w(), H = m._snake_h();
  assert.strictEqual(W, 504, "canvas width");
  assert.strictEqual(H, 504, "canvas height");
  const buf = m._snake_buf();
  assert(buf > 0, "pixel buffer pointer");
  const px = () => new Uint8Array(m.HEAPU8.buffer, buf, W * H * 4);

  // --- 1. rendering: head, body and food pixels present ---
  m._init_snake();
  m._frame(performance.now());
  {
    const p = px();
    let head = 0, body = 0, food = 0, opaque = 0;
    for (let i = 0; i < W * H; i++) {
      const r = p[i * 4], g = p[i * 4 + 1], b = p[i * 4 + 2];
      if (p[i * 4 + 3] !== 255) continue;
      opaque++;
      if (r >= 150 && g >= 190 && b >= 150 && r < 240) head++;
      else if (g > 180 && r < 120 && b < 160) body++;
      else if (r > 200 && g < 140) food++;
    }
    console.log(`render — head px: ${head}, body px: ${body}, food px: ${food}, opaque: ${opaque}/${W * H}`);
    assert(opaque === W * H, "all pixels opaque");
    assert(head > 100 && body > 100 && food > 50, "snake+food missing from buffer");
  }

  // --- 2. reverse guard: facing right, left press ignored ---
  m._set_dir(3); // left (180°) must be ignored
  m._set_dir(3); // again
  let now = performance.now() + 200;
  m._frame(now);
  now += 200; m._frame(now);
  assert.strictEqual(m._get_alive(), 1, "still alive after reversal attempt");
  console.log("reverse guard OK");

  // --- 3. eating: food directly ahead, snake grows + score rises ---
  m._init_snake();
  m._frame(performance.now());
  // initial snake: head at (10,10) facing right; put food 1 ahead, then 2 ahead
  m._set_food(11, 10);
  let t = performance.now() + 200;
  m._frame(t); // moves right into food (11,10)
  assert.strictEqual(m._get_score(), 1, "score after first eat");
  const bodyAfter1 = () => {
    const p = px(); let n = 0;
    for (let i = 0; i < W * H; i++) {
      const r = p[i*4], g = p[i*4+1], b = p[i*4+2];
      if (g > 180 && r < 120 && b < 160) n++;
    }
    return n;
  };
  const len1 = bodyAfter1();
  m._set_food(13, 10); // another one ahead
  t += 200; m._frame(t);
  t += 200; m._frame(t); // into (13,10)
  assert.strictEqual(m._get_score(), 2, "score after second eat");
  const len2 = bodyAfter1();
  assert(len2 > len1, "snake visibly longer after eating");
  console.log(`eating OK — body px ${len1} -> ${len2}, score=2`);

  // --- 4. wall death ---
  for (let i = 0; i < 60 && m._get_alive(); i++) { t += 150; m._frame(t); }
  assert.strictEqual(m._get_alive(), 0, "wall should kill");
  console.log(`wall death OK — final score ${m._get_score()}`);

  // --- 5. self-collision death: steer into own body ---
  m._init_snake();
  t = performance.now();
  m._frame(t);
  // head (10,10) facing right, len 4: plant food 5 ahead, then loop up-left-down
  m._set_food(15, 10);
  for (let i = 0; i < 5 && m._get_alive(); i++) { t += 150; m._frame(t); }
  assert(m._get_score() >= 1, "ate before self-test");
  m._set_dir(0); m._set_dir(3); m._set_dir(2); // up, left, down → body at (14,10)
  for (let i = 0; i < 4 && m._get_alive(); i++) { t += 150; m._frame(t); }
  assert.strictEqual(m._get_alive(), 0, "self-collision should kill");
  console.log("self-collision OK");

  // --- 6. restart resets state ---
  m._init_snake();
  m._frame(performance.now());
  assert(m._get_alive() === 1 && m._get_score() === 0, "reset state");
  console.log("restart OK");

  // --- 7. soak: 500 frames, no crash, state stays sane ---
  let alive = 1, score = 0;
  for (let i = 0; i < 500; i++) {
    if (!m._get_alive()) { m._init_snake(); score += m._get_score(); }
    t += 131; m._frame(t);
  }
  console.log("500-frame soak OK");

  console.log("\nALL TESTS PASSED");
  process.exit(0);
})().catch((e) => {
  console.error("SMOKE FAIL:", e);
  process.exit(1);
});
