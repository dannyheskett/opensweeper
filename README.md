# opensweeper

The classic mine-sweeping puzzle, written in C. Reveal every cell that is not a
mine; each number says how many mines touch that cell. It runs natively on
Windows, macOS, Linux, Android, and iOS, and in the browser via WebAssembly.

Rendering, input, and audio go through raylib 6.0 on every platform except iOS,
which uses a native Metal backend with no raylib (see
[Architecture](#architecture)). The game logic (`src/game.c`) and the board
layout (`src/layout.c`) are platform-independent and shared unchanged.

## Platforms

| Platform | Build | Orientation | Input |
|----------|-------|-------------|-------|
| Linux / Windows / macOS | native (raylib) | any window shape | mouse + keyboard |
| Web (WASM) | Emscripten (raylib) | any window shape | mouse + keyboard + touch |
| Android | NativeActivity (raylib) | portrait or landscape | touch |
| iOS | native Metal (no raylib) | portrait or landscape, iPhone + iPad | touch |

One adaptive layout serves every platform: the grid is fitted to the live view
each frame at the largest square cell that fits, so it grows with a desktop
window and re-fits when a phone is turned. When a grid fits better sideways it
is drawn turned: Expert's 30x16 stands upright as 16 columns of 30 on a phone
held upright, which nearly doubles its cells.

## Difficulty

| Mode         | Grid   | Mines |
|--------------|--------|-------|
| Beginner     | 9×9    | 10    |
| Intermediate | 16×16  | 40    |
| Expert       | 30×16  | 99    |

Set under **Options** in the menu, with **Marks (?)** (whether flagging cycles
through a question mark). A new difficulty applies from the next game. The
first cell revealed is never a mine.

## Controls

**Mouse and keyboard**, on desktop and desktop browsers:

| Input | Action |
|-------|--------|
| Left click | Reveal a cell |
| Right click | Flag / question mark / clear |
| Middle click (or left + right) | Chord: reveal the neighbours of a satisfied number |
| Click the face | New game |
| Arrow keys / WASD | Move the cursor |
| Space / Enter | Reveal the cell under the cursor |
| F | Flag the cell under the cursor |
| Escape | Back to the menu, game stays resumable |
| Alt+Enter | Toggle fullscreen |
| Click a menu row | Choose it; Up / Down + Enter also work, Left / Right cycle a value in Options |

**Touch**, on iOS, Android, and mobile browsers:

| Gesture | Action |
|---------|--------|
| Tap a hidden cell | Reveal it |
| Press and hold a cell | Flag / question mark / clear |
| Tap a revealed number | Chord: reveal its neighbours once its flags are placed |
| Tap the face | New game |
| Tap the title bar | Back to the menu, game stays resumable |
| Two-finger tap | The same, anywhere |
| Tap a menu row | Choose it; in Options, tapping cycles the value |
| Swipe up / down | Move the menu selection; left / right cycles a value |

## Menu and window

These behave identically in every game in this family (openblocks, openrackem,
openklondike, opencheckers, openpairs, opensweeper). The code for them
(`src/menu.c`, `src/window.c`, `src/present.c`, and the gfx, safe-area,
timing, audio and recorder layers) is the same file in every repo.

- **Menu**: Resume Game (when a game is in progress), New Game, Options (when
  the game has settings), Sound, Record (desktop only), Exit (desktop only, set
  apart by a blank line). Options holds the settings and Back.
- **Menu input**: Up / Down (or W / S) move, Enter / Space choose, Left / Right
  (or A / D) cycle an Options value, Escape backs out. A mouse click or a tap on
  a row chooses it. Swipes move the selection and cycle values.
- **Menu size**: derived from the long edge of the view, so it is the same size
  upright and sideways and grows with the window; it shrinks only when its rows
  would not otherwise fit.
- **Back to the menu**: Escape, Android Back, or a two-finger tap. Losing focus
  (app backgrounded, tab hidden, window deactivated) also returns to the menu;
  the game stays resumable.
- **Window**: desktop opens at 960×720, resizes freely down to 640×480, and
  Alt+Enter toggles borderless fullscreen and back to the previous window.
  Web fills the browser viewport. Android and iOS are fullscreen.

## Building

raylib is built once from source into a gitignored install directory (per
platform) before the game is built. Each `scripts/build_raylib_*.sh` clones
raylib (pinned via `RAYLIB_TAG`, default `6.0`) and installs its headers and
`libraylib.a`. CI runs these scripts before each build.

### Desktop

```bash
./scripts/build_raylib_linux.sh      # once, on a fresh clone
make                                 # -> build/opensweeper   (dev, -O2)
make run
make release                         # -> build/opensweeper-release (-O3)
```

Windows (mingw-w64 cross-compile) and macOS (universal arm64 + x86_64):

```bash
./scripts/build_raylib_windows.sh && make windows   # -> build/opensweeper-x64.exe, -x86.exe
./scripts/build_raylib_mac.sh     && make mac       # -> build/opensweeper-mac
```

### Android (needs the Android SDK + NDK)

```bash
./scripts/build_raylib_android.sh
make android        # -> build/opensweeper.apk   (debug-signed, sideloadable)
make android-play   # -> build/opensweeper.aab   (Play App Bundle; PLAY_* signing vars)
```

A `NativeActivity` with no Gradle; a small `OpensweeperActivity` Java class
(compiled with `javac` + `d8`) handles immersive full screen and hands the
window insets to the layout. arm64-v8a, `targetSdk` 36, 16 KB-page aligned.

### iOS (needs macOS + Xcode; no raylib)

```bash
make ios-sim   # -> build/ios-sim/Opensweeper.app   (Simulator, arm64)
make ios       # -> build/opensweeper.ipa           (device arm64, unsigned)
```

iPhone and iPad, both orientations, iOS 15+. The `.ipa` is unsigned unless
`IOS_SIGN_IDENTITY` / `IOS_PROFILE` / `IOS_TEAM_ID` are set; AWS Device Farm
re-signs an unsigned one on upload.

### Web (needs Emscripten)

```bash
./scripts/build_raylib_web.sh
make web        # -> build/web/opensweeper.{html,js,wasm}
make web-serve  # http://localhost:8080/opensweeper.html
```

## Tests

Unit tests with no raylib or window required:

```bash
make test
```

- `test_game` — mine placement (the first reveal is safe), neighbour counts,
  flood fill, flags and question marks, chords, winning and losing.
- `test_layout` — every grid fits every view shape, the sideways grid, chrome
  that keeps its size when the device is turned, hit testing, and clearing a
  display cutout.
- `test_input` — the touch recognizer: taps decided on release, press and hold
  firing once, drags, two-finger taps, and swipes.
- `test_menu` — the family menu: it fits every view, keeps its size on
  rotation, grows with the window, and a pointer picks the row under it.

## Continuous integration and releases

Every pull request to `main` builds all platforms via GitHub Actions
([`ci.yml`](.github/workflows/ci.yml)) and runs `make test`. Pushing to `main`
cuts the next `release-N` via [`release.yml`](.github/workflows/release.yml),
which attaches per-platform archives, the Android APK, the iOS `.ipa` and the
WASM bundle to the GitHub Release; when the store secrets are set it also
uploads the AAB to the Play internal track, uploads the `.ipa` to TestFlight and
submits it to App Review. Setup:
[`android/play-assets/KEYSTORE.md`](android/play-assets/KEYSTORE.md) and
[`ios/app-store-assets/TESTFLIGHT.md`](ios/app-store-assets/TESTFLIGHT.md).

## Recording (desktop only)

Toggle **Record: On/Off** from the menu to capture the session to an H.264 MP4
(`opensweeper-YYYYMMDD-HHMMSS.mp4`), one video frame per rendered frame, no
external tools. The board is re-rendered at a fixed 992×672 and supersampled
for capture. Mobile and web compile it out.

```bash
./build/opensweeper --record            # auto-named file
./build/opensweeper --record clip.mp4   # explicit path
```

## Architecture

- `src/game.c` — rules only: mine placement, reveal and flood fill, flags,
  chords, the clock, winning and losing. No drawing, no input, no platform.
- `src/layout.c` — the board solver: chrome sizes, the cell size, whether the
  grid is drawn sideways, and hit testing, as pure functions of the view size.
- `src/render.c` — one adaptive renderer: chrome, the grid, mines and flags,
  the end notice. Drawing goes through a small primitive layer (`src/gfx.h`):
  `src/gfx_raylib.c` wraps raylib, `ios/gfx_metal.mm` is a native Metal
  implementation. `src/os_types.h` supplies raylib-compatible types so the
  shared code compiles without raylib on iOS.
- Audio is a similar seam (`src/audio.h`): `src/audio_raylib.c` vs
  `ios/audio_ios.mm` (AVAudioEngine). Effects are synthesized at startup; sound
  is off by default.
- iOS backend: `ios/plat_ios.mm` (touch / screen / timing) and `ios/ios_main.mm`
  (UIKit app + `CAMetalLayer` view + `CADisplayLink` loop).
- `src/safe_area.c` carries all four window insets: sideways, the camera cutout
  and the gesture bar move to a side edge.
- All text is the bundled Nunito SemiBold (SIL OFL, see `NOTICE`), embedded so
  there is no runtime asset file. There are no asset files at all: mines and
  flags are drawn, sounds are synthesized, icons and store screenshots are
  generated (`scripts/gen_icons.py`, `scripts/gen_store_screenshots.mjs`).

## Dependencies

- A C99 compiler (GCC or Clang); a C++ / Objective-C++ compiler for the iOS
  backend.
- [raylib](https://github.com/raysan5/raylib) 6.0 (static) on all platforms
  except iOS, built by the `scripts/build_raylib_*.sh` helpers.
- The MP4 recorder uses two vendored public-domain (CC0) single-header
  libraries: [minih264](third_party/minih264) and [minimp4](third_party/minimp4).

## Project structure

```
opensweeper/
├── src/            # shared C sources + gfx/audio raylib backends
├── ios/            # native Metal / UIKit backend (Objective-C++) + App Store assets
├── android/        # NativeActivity manifest, resources, Java activity + Play assets
├── web/            # Emscripten HTML shell
├── scripts/        # raylib build scripts, asset/font generators, store tooling
├── third_party/    # vendored single-header libs + Nunito
├── tests/          # game, layout, input and menu unit tests
├── Makefile
├── LICENSE         # MIT (this project's own code)
└── NOTICE          # third-party attributions
```

## License

opensweeper's own code is released under the [MIT License](LICENSE). The
vendored `minih264` and `minimp4` libraries are public domain (CC0), and the
Nunito font is under the SIL Open Font License; see [NOTICE](NOTICE) for
attributions.
