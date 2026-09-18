#ifndef OPENSWEEPER_PLATFORM_H
#define OPENSWEEPER_PLATFORM_H

// The game's name: window title and recording file prefix.
#define GAME_NAME "opensweeper"

// Recording size (recorder.c): Expert's grid at 32px cells with its chrome.
// Both are multiples of 16 for the H.264 encoder.
#define REC_W 992
#define REC_H 672

// OS_TOUCH compiles the touch frontend: tap to reveal, press and hold to flag,
// and tap-driven menus. It is enabled on Android, iOS, and the WebAssembly
// build (which serves phones as well as desktop browsers; the pointer type
// picks which grammar is live). Desktop native builds leave it unset.
//
// raylib defines PLATFORM_ANDROID / PLATFORM_WEB for its own sources; our build
// passes the matching -D for the game translation units.
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB) || defined(PLATFORM_IOS)
#define OS_TOUCH 1
#endif

// There is one adaptive layout (layout.c) for every platform; the grid is
// fitted to the live view and turned sideways when that gives bigger cells.

#endif // OPENSWEEPER_PLATFORM_H
