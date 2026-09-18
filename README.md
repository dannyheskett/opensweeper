# opensweeper

A Minesweeper clone written in C with Raylib, following classic Windows 95/XP conventions.

## Building

### Linux/WSL2

```bash
./scripts/build_raylib_linux.sh
make
make run
```

### Release Build

```bash
make release
make run-release
```

### Windows Cross-Compile

Requires mingw-w64:

```bash
./scripts/build_raylib_windows.sh
make windows
```

Produces `build/opensweeper-x64.exe` and `build/opensweeper-x86.exe`.

### macOS

```bash
./scripts/build_raylib_mac.sh
make mac  # -> build/opensweeper-mac (universal arm64 + x86_64)
```

## Tests

```bash
make test
```

## Difficulty

| Mode         | Grid   | Mines |
|--------------|--------|-------|
| Beginner     | 9×9    | 10    |
| Intermediate | 16×16  | 40    |
| Expert       | 30×16  | 99    |

Set under **Options** in the menu, with **Marks (?)** (whether right-click
cycles through a question mark). A new difficulty applies from the next game.
Every grid is drawn centred on the same 992×672 board, which shrinks to fit a
smaller window and never enlarges.

## Controls

**Mouse:**
- Left click: reveal cell
- Right click: flag / question mark / clear
- Middle click (or left+right): chord reveal

**Keyboard:**
- Arrow keys / WASD: move cursor
- Space / Enter: reveal cell
- F: flag cell
- Escape: menu (the game stays resumable)
- Alt+Enter: toggle fullscreen

**Menu:** click a row to choose it, or Up / Down (or W / S) + Enter / Space;
Left / Right (or A / D) cycle a value on the Options screen.

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

## Recording

Toggle **Record: On/Off** from the menu to capture your session to an H.264 MP4 (`opensweeper-YYYYMMDD-HHMMSS.mp4`). No external tools required.

## License

MIT. See [LICENSE](LICENSE). Bundled third-party components (minih264,
minimp4, the Nunito typeface) are listed in [NOTICE](NOTICE).
