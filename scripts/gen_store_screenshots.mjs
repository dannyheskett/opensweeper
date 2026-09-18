// Capture the Google Play and App Store screenshots from the web build.
//
// opensweeper has ONE adaptive layout, so the web build renders what a phone, a
// tablet and an iPad render -- a headless browser at the right viewport size
// produces pixel-equivalent frames without a device or a farm. Both stores want
// phone and tablet sets, and this game ships in both orientations, so each slot
// gets its own folder and you upload whichever orientation you want it to show.
//
// Regenerate whenever the portrait UI changes. The committed PNGs under
// android/play-assets/screenshots/ and ios/app-store-assets/screenshots/ are what
// scripts/play_release.py and scripts/asc_release.py push to the stores.
//
// Usage:
//   npm i playwright-core                    # not a repo dependency; dev-only
//   make web   (or: gh release download release-N -p '*-web-wasm.zip' && unzip it)
//   node scripts/gen_store_screenshots.mjs --src build/web
//
// Options:
//   --src <dir>      web bundle directory (contains opensweeper.html) [required]
//   --out <dir>      repo root to write screenshots under         [default: cwd]
//   --chrome <path>  Chromium binary  [default: $CHROME, else the Playwright cache]
//   --only <name>    capture a single target (see TARGETS below)
//
// Why a browser and not the real app: the maintainer has no Android device and no
// Mac, and Device Farm returns video, not clean full-resolution stills.

import { chromium } from 'playwright-core';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { existsSync, readdirSync } from 'node:fs';
import { extname, join, resolve } from 'node:path';
import { homedir } from 'node:os';

// ---------------------------------------------------------------------------
// Targets. Play's tablet slot takes the same 9:16 frames at 2x, which is why one
// capture pass fills both the 7-inch and 10-inch slots (see play-assets/LISTING.md).
// The App Store's 6.9" slot is the only iPhone size that covers every device.
// ---------------------------------------------------------------------------
const TARGETS = [
  { name: 'play-phone',            w: 1080, h: 1920, out: 'android/play-assets/screenshots/phone' },
  { name: 'play-phone-landscape',  w: 1920, h: 1080, out: 'android/play-assets/screenshots/phone-landscape' },
  { name: 'play-tablet',           w: 1600, h: 2560, out: 'android/play-assets/screenshots/tablet' },
  { name: 'play-tablet-landscape', w: 2560, h: 1600, out: 'android/play-assets/screenshots/tablet-landscape' },
  { name: 'ios-6.9',               w: 1290, h: 2796, out: 'ios/app-store-assets/screenshots/iphone-6.9' },
  { name: 'ios-6.9-landscape',     w: 2796, h: 1290, out: 'ios/app-store-assets/screenshots/iphone-6.9-landscape' },
  { name: 'ipad-13',               w: 2064, h: 2752, out: 'ios/app-store-assets/screenshots/ipad-13' },
  { name: 'ipad-13-landscape',     w: 2752, h: 2064, out: 'ios/app-store-assets/screenshots/ipad-13-landscape' },
];

// The board the shots are taken on: Beginner (9x9, 10 mines) fills a phone and
// reads at a glance in a store thumbnail. Three frames per target: the menu,
// the fresh board, and a game in progress with flags placed.
const MAX_GAMES = 40;        // restarts after a lost guess before giving up
const MAX_MOVES = 200;       // per game, a safety stop

// Colours from src/render.c, as read back from a screenshot.
const CELL_HIDDEN = [35, 35, 35];
const CELL_OPEN = [20, 20, 20];
const MINE_BG = [220, 60, 60];
const FLAG = [240, 200, 40];
const NUM_COLORS = [null,
  [100, 120, 240], [60, 180, 60], [220, 60, 60], [60, 60, 180],
  [160, 40, 40], [60, 180, 180], [220, 220, 220], [140, 140, 140]];
const GRID = 9;              // Beginner
const NOTICE_TITLE = [253, 249, 0];   // raylib YELLOW: the end-of-game notice's title

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
               '.data': 'application/octet-stream', '.png': 'image/png' };

function arg(flag, fallback) {
  const i = process.argv.indexOf(flag);
  return i > -1 && process.argv[i + 1] ? process.argv[i + 1] : fallback;
}

function findChrome() {
  const explicit = arg('--chrome', process.env.CHROME);
  if (explicit) return explicit;
  const cache = join(homedir(), '.cache/ms-playwright');
  if (!existsSync(cache)) return null;
  // Highest chromium-<rev> wins; Playwright keeps several revisions side by side.
  const dirs = readdirSync(cache)
    .filter(d => /^chromium-\d+$/.test(d))
    .sort((a, b) => +b.split('-')[1] - +a.split('-')[1]);
  for (const d of dirs) {
    for (const exe of ['chrome-linux64/chrome', 'chrome-linux/chrome', 'chrome-mac/Chromium.app/Contents/MacOS/Chromium']) {
      const p = join(cache, d, exe);
      if (existsSync(p)) return p;
    }
  }
  return null;
}

async function serve(dir) {
  const server = createServer(async (req, res) => {
    const rel = decodeURIComponent(req.url.split('?')[0]).replace(/^\/+/, '') || 'opensweeper.html';
    try {
      const body = await readFile(join(dir, rel));
      res.writeHead(200, { 'Content-Type': MIME[extname(rel)] || 'application/octet-stream' });
      res.end(body);
    } catch {
      res.writeHead(404).end('not found');
    }
  });
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  return { server, port: server.address().port };
}

// Store listings reject screenshots with an alpha channel; the browser writes
// RGBA. Re-encode as plain RGB PNG (colour type 2) with zlib, no dependencies.
// The same decoder reads the board back from screenshots while playing.
import { deflateSync, inflateSync } from 'node:zlib';
function crc32(buf) {
  let c, crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    c = (crc ^ buf[n]) & 0xff;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const td = Buffer.concat([Buffer.from(type), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
  return Buffer.concat([len, td, crc]);
}
// Decode an 8-bit RGB or RGBA PNG (as the browser writes) to RGB rows.
function decodePng(png) {
  let off = 8, w = 0, h = 0, ct = 0;
  const idat = [];
  while (off < png.length) {
    const len = png.readUInt32BE(off), type = png.toString('ascii', off + 4, off + 8);
    const data = png.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); ct = data[9]; }
    if (type === 'IDAT') idat.push(data);
    off += 12 + len;
  }
  if (ct !== 2 && ct !== 6) throw new Error(`unexpected PNG colour type ${ct}`);
  const bpp = ct === 6 ? 4 : 3, stride = w * bpp;
  const raw = inflateSync(Buffer.concat(idat));
  const rgb = Buffer.alloc(w * h * 3);
  const prev = Buffer.alloc(stride), cur = Buffer.alloc(stride);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)];
    raw.copy(cur, 0, y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let i = 0; i < stride; i++) {          // undo the PNG row filter
      const a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      let v = cur[i];
      if (f === 1) v += a; else if (f === 2) v += b; else if (f === 3) v += (a + b) >> 1;
      else if (f === 4) { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                          v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      cur[i] = v & 0xff;
    }
    for (let x = 0; x < w; x++) cur.copy(rgb, (y * w + x) * 3, x * bpp, x * bpp + 3);
    cur.copy(prev);
  }
  return { w, h, rgb };
}

function encodeRgbPng({ w, h, rgb }) {
  const out = Buffer.alloc(h * (1 + w * 3));
  for (let y = 0; y < h; y++) {
    out[y * (1 + w * 3)] = 0;
    rgb.copy(out, y * (1 + w * 3) + 1, y * w * 3, (y + 1) * w * 3);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr),
                        chunk('IDAT', deflateSync(out)), chunk('IEND', Buffer.alloc(0))]);
}

async function capture(target, url, chrome) {
  const { w, h, out } = target;
  const browser = await chromium.launch({
    executablePath: chrome,
    // SwiftShader: the runner has no GPU, and raylib needs a real WebGL context.
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  });
  // hasTouch makes matchMedia('(pointer: coarse)') match, which is what
  // src/main.c reads to describe taps ("Tap to continue") and hide the keyboard
  // cursor, as on a phone. No page-script shim is needed.
  const ctx = await browser.newContext({
    viewport: { width: w, height: h }, deviceScaleFactor: 1, hasTouch: true,
  });
  const page = await ctx.newPage();
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(w * h > 3000000 ? 9000 : 6000);   // WASM boot + font upload

  const wait = ms => page.waitForTimeout(ms);
  const { writeFile } = await import('node:fs/promises');
  const shot = async name => writeFile(join(out, `${name}.png`), encodeRgbPng(decodePng(await page.screenshot())));

  // The input layer samples touches once a frame and decides a tap on release,
  // so a press has to span more than a couple of frames; 60ms is silently
  // dropped, 250ms is reliable at phone sizes. SwiftShader
  // renders a 4K tablet frame slowly, so the canvas is only read back once the
  // frame after the tap has certainly been drawn.
  // Timings scale with the viewport: SwiftShader draws a 2064x2752 iPad frame
  // several times slower than a phone frame, and a 120ms press that spans two
  // frames on a phone can land inside ONE there -- the recognizer never sees a
  // press and then a release, so the tap is silently dropped and the board
  // never finishes.
  const slow = w * h > 3000000;
  // A tap must stay under the game's 0.4 s press-and-hold threshold
  // (src/input.c LONG_PRESS_S) at every size, or it flags instead of revealing.
  const T = { settle: slow ? 220 : 80, hold: 250, after: slow ? 900 : 500 };
  // Real touch events (the Chrome DevTools protocol), not a mouse: the game
  // reads taps and press-and-hold from its touch recognizer, exactly as on a
  // phone.
  const cdp = await ctx.newCDPSession(page);
  const touch = (type, x, y) => cdp.send('Input.dispatchTouchEvent',
      { type, touchPoints: type === 'touchEnd' ? [] : [{ x, y }] });
  async function press(x, y, ms) {
    await wait(T.settle);
    await touch('touchStart', x, y); await wait(ms); await touch('touchEnd'); await wait(T.after);
  }
  const tap = (x, y) => {
    if (process.env.OS_DEBUG) console.log(`  tap ${Math.round(x)},${Math.round(y)}`);
    return press(x, y, T.hold);
  };

  // Read pixels back from a screenshot of the page. (Reading the WebGL canvas
  // from page script returned the previous frame under SwiftShader; a screenshot
  // always waits for the current one.) Returns RGB triples at the given points.
  const pixels = async pts => {
    const img = decodePng(await page.screenshot());
    return pts.map(([px, py]) => {
      const i = (Math.round(py) * img.w + Math.round(px)) * 3;
      return [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
    });
  };
  const near = (p, q, tol = 26) => p.every((v, i) => Math.abs(v - q[i]) <= tol);

  // A press held in place long enough to flag (src/input.c LONG_PRESS_S is
  // 0.4 s; hold well past it so a slow frame cannot cut it short).
  const hold = (x, y) => press(x, y, slow ? 1400 : 800);
  // Keys, like taps, are held across a few frames: a press and release inside
  // one frame never shows up as IsKeyPressed.
  const key = async k => {
    await page.keyboard.down(k); await wait(T.hold); await page.keyboard.up(k); await wait(T.after);
  };

  // Find the grid by its hidden cells on a freshly started board. Reading the
  // geometry back out of the picture keeps this script honest: it never assumes
  // the layout formula, so a change in src/layout.c cannot silently produce
  // screenshots of thin air.
  async function readGrid() {
    const img = decodePng(await page.screenshot());
    const at = (x, y) => { const i = (y * img.w + x) * 3; return [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]]; };
    let x0 = w, x1 = -1, y0 = h, y1 = -1;
    for (let y = 0; y < h; y += 2)
      for (let x = 0; x < w; x += 2)
        // A solid patch, not a stray pixel: the wordmark's antialiased edges
        // pass through the same grey.
        if (x + 12 < w && y + 12 < h &&
            [[0, 0], [12, 0], [0, 12], [12, 12], [6, 6]].every(([dx, dy]) =>
              near(at(x + dx, y + dy), CELL_HIDDEN, 1))) {
          if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
        }
    if (x1 < 0) throw new Error(`${target.name}: no board found`);
    // x1 / y1 are the last patch origins; the patch reaches 12px further.
    const g = { x0, y0, pitch: ((x1 + 13 - x0) + (y1 + 13 - y0)) / 2 / GRID };
    if (process.env.OS_DEBUG) console.log(`  grid: ${JSON.stringify({ x0, x1, y0, y1, pitch: g.pitch })}`);
    return g;
  }

  // Classify every cell from one screenshot: 'h' hidden, 'f' flagged, 'm' the
  // detonated mine, or a number 0..8 for a revealed cell.
  async function readCells(g) {
    const img = decodePng(await page.screenshot());
    const at = (x, y) => { const i = (Math.round(y) * img.w + Math.round(x)) * 3; return [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]]; };
    const cells = [];
    for (let r = 0; r < GRID; r++) {
      const row = [];
      for (let c = 0; c < GRID; c++) {
        const cx = g.x0 + (c + 0.5) * g.pitch, cy = g.y0 + (r + 0.5) * g.pitch;
        const corner = at(cx - g.pitch * 0.38, cy - g.pitch * 0.38);
        if (near(corner, MINE_BG, 30)) { row.push('m'); continue; }
        // The most saturated / brightest pixel near the centre carries the
        // cell's content: a digit's colour, or the yellow of a flag.
        let best = null, bestScore = -1;
        for (let dy = -0.3; dy <= 0.3; dy += 0.05)
          for (let dx = -0.3; dx <= 0.3; dx += 0.05) {
            const p = at(cx + dx * g.pitch, cy + dy * g.pitch);
            const score = Math.max(...p) - Math.min(...p) + Math.max(...p) / 4;
            if (score > bestScore) { bestScore = score; best = p; }
          }
        if (near(best, FLAG, 40)) { row.push('f'); continue; }
        if (near(corner, CELL_HIDDEN, 6)) { row.push('h'); continue; }
        let num = 0, bestD = 1e9;
        if (bestScore > 60) {
          for (let k = 1; k <= 8; k++) {
            const d = NUM_COLORS[k].reduce((a, v, i) => a + (v - best[i]) ** 2, 0);
            if (d < bestD) { bestD = d; num = k; }
          }
        }
        row.push(num);
      }
      cells.push(row);
    }
    return cells;
  }

  // The end-of-game notice is up: its title is the only pure raylib-yellow text
  // on screen (flags are a warmer yellow). It dims the board under it, so the
  // cells can no longer be read once it shows.
  async function noticeShowing() {
    const img = decodePng(await page.screenshot());
    for (let y = Math.round(h * 0.35); y < h * 0.65; y += 3)
      for (let x = Math.round(w * 0.2); x < w * 0.8; x += 3) {
        const i = (y * img.w + x) * 3;
        if (near([img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]], NOTICE_TITLE, 20)) return true;
      }
    return false;
  }

  const centre = (g, r, c) => [g.x0 + (c + 0.5) * g.pitch, g.y0 + (r + 0.5) * g.pitch];
  const neighbours = (r, c) => {
    const out = [];
    for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
      const rr = r + dr, cc = c + dc;
      if ((dr || dc) && rr >= 0 && rr < GRID && cc >= 0 && cc < GRID) out.push([rr, cc]);
    }
    return out;
  };

  // Play one game: the two single-number deductions, then the subset rule
  // between two numbers, and a guess only when nothing is certain -- at the
  // hidden cell least likely to be a mine. Returns 'mid' once the mid-game
  // shot is taken, or 'over' if the game ends first.
  let midDone = false;   // the mid-game shot, taken once in whichever game gets there
  async function playOne(g) {
    await tap(...centre(g, 4, 4));             // the first reveal is always safe
    for (let move = 0; move < MAX_MOVES; move++) {
      if (await noticeShowing()) return 'over';
      const cells = await readCells(g);
      const hidden = [], flags = [];
      cells.forEach((row, r) => row.forEach((v, c) => { if (v === 'h') hidden.push([r, c]); if (v === 'f') flags.push([r, c]); }));

      // Every number's constraint: its hidden neighbours hold `need` mines.
      const cons = [];
      for (let r = 0; r < GRID; r++)
        for (let c = 0; c < GRID; c++) {
          const n = cells[r][c];
          if (typeof n !== 'number' || n === 0) continue;
          const nb = neighbours(r, c);
          const h = nb.filter(([rr, cc]) => cells[rr][cc] === 'h');
          const f = nb.filter(([rr, cc]) => cells[rr][cc] === 'f').length;
          if (h.length) cons.push({ r, c, need: n - f, h, key: new Set(h.map(([a, b]) => a * GRID + b)) });
        }

      let acted = false;
      for (const k of cons) {
        if (k.need === 0) { await tap(...centre(g, k.r, k.c)); acted = true; break; }      // chord
        if (k.need === k.h.length) {                                                    // all mines
          for (const [rr, cc] of k.h) await hold(...centre(g, rr, cc));
          acted = true; break;
        }
      }
      // Subset rule: if A's hidden cells are all among B's, the cells only B
      // sees hold exactly need(B) - need(A) mines -- none, or all of them.
      for (let i = 0; i < cons.length && !acted; i++)
        for (let j = 0; j < cons.length && !acted; j++) {
          const A = cons[i], B = cons[j];
          if (i === j || A.h.length >= B.h.length) continue;
          if (![...A.key].every(x => B.key.has(x))) continue;
          const rest = B.h.filter(([a, b]) => !A.key.has(a * GRID + b));
          const m = B.need - A.need;
          if (m === 0) { await tap(...centre(g, ...rest[0])); acted = true; }
          else if (m === rest.length) { for (const [a, b] of rest) await hold(...centre(g, a, b)); acted = true; }
        }

      if (!acted) {
        if (!hidden.length) return 'stuck';
        // Least likely mine: the worst local ratio from any number touching the
        // cell, or the board's leftover density for a cell no number touches.
        const left = Math.max(0, 10 - flags.length);
        const density = left / hidden.length;
        let best = hidden[0], bestP = 2;
        for (const [r, c] of hidden) {
          let p = density;
          for (const k of cons) if (k.key.has(r * GRID + c)) p = Math.max(p, k.need / k.h.length);
          if (p < bestP) { bestP = p; best = [r, c]; }
        }
        await tap(...centre(g, ...best));
      }
      if (flags.length >= 2 && hidden.length < 60) { await shot('03-play'); midDone = true; return 'mid'; }
    }
    return 'stuck';
  }

  await mkdir(out, { recursive: true });

  // 1. Title menu, untouched. Sound reads "Off" because it genuinely is off by
  //    default on every platform (src/sound.c) -- not a capture artifact.
  await shot('01-menu');

  // Options -> Difficulty one step left (Intermediate -> Beginner) -> back ->
  // New Game. The keyboard drives the menu here only because it is exact; the
  // board itself is played with taps and holds.
  await key('ArrowDown'); await key('Enter');     // Options
  await key('ArrowLeft');                          // Beginner
  await key('Escape');                             // back to the menu
  await key('Enter');                              // New Game
  await wait(800);

  // 2. The board as it starts: every cell hidden.
  await shot('02-board');
  const g = await readGrid();

  // 3. A game in progress, with flags placed. A game that ends before it gets
  //    there (a lost guess) is dismissed and started again.
  for (let game = 0; !midDone; game++) {
    if (game === MAX_GAMES) throw new Error(`${target.name}: no mid-game frame in ${MAX_GAMES} games`);
    const result = await playOne(g);
    if (result === 'mid') break;
    await wait(600);
    await tap(w / 2, h / 2);       // dismiss the notice -> menu
    await key('Enter');            // New Game
    await wait(600);
  }

  await browser.close();
  console.log(`${target.name}: 3 frames -> ${out}`);
}

const src = arg('--src');
if (!src) { console.error('--src <web bundle dir> is required'); process.exit(2); }
const root = resolve(arg('--out', process.cwd()));
const chrome = findChrome();
if (!chrome) { console.error('no Chromium found; pass --chrome or set $CHROME'); process.exit(2); }

const { server, port } = await serve(resolve(src));
const url = `http://127.0.0.1:${port}/opensweeper.html`;
const only = arg('--only');

// A run can stall if a tap lands during an animation, so a target is worth
// replaying rather than failing the batch.
for (const t of TARGETS) {
  if (only && t.name !== only) continue;
  const target = { ...t, out: join(root, t.out) };
  for (let attempt = 1; ; attempt++) {
    try { await capture(target, url, chrome); break; }
    catch (e) {
      if (attempt === 3) throw e;
      console.log(`${t.name}: attempt ${attempt} failed (${e.message}); retrying`);
    }
  }
}
server.close();
