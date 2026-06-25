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

Selectable from the menu. The window resizes to fit.

## Controls

**Mouse:**
- Left click: reveal cell
- Right click: flag / question mark / clear
- Middle click (or left+right): chord reveal

**Keyboard:**
- Arrow keys / WASD: move cursor
- Space / Enter: reveal cell
- F: flag cell
- Escape: menu
- Alt+Enter: toggle fullscreen

## Recording

Toggle **Record: On/Off** from the menu to capture your session to an H.264 MP4 (`opensweeper-YYYYMMDD-HHMMSS.mp4`). No external tools required.

## License

MIT. See [LICENSE](LICENSE).
