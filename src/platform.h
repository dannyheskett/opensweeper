#ifndef OPENSWEEPER_PLATFORM_H
#define OPENSWEEPER_PLATFORM_H

// The game's name: window title and recording file prefix.
#define GAME_NAME "opensweeper"

// Recording size (recorder.c): the board canvas (render.h CANVAS_W x
// CANVAS_H). Both are multiples of 16 for the H.264 encoder.
#define REC_W 992
#define REC_H 672

// opensweeper builds for the desktop and the web. raylib defines PLATFORM_WEB
// for its own sources; our build passes the matching -D for the game
// translation units. There is no touch frontend.

#endif // OPENSWEEPER_PLATFORM_H
