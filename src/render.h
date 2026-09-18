#ifndef OPENSWEEPER_RENDER_H
#define OPENSWEEPER_RENDER_H

#include "game.h"
#include "platform.h"
#include "os_types.h"
#include <stdbool.h>

// One adaptive renderer for every platform: the board is laid out from the live
// view each frame (layout.c) and drawn through the gfx primitives, so the same
// code draws a desktop window, a browser tab, both phone orientations and an
// iPad.

// Window setup and teardown (window.c) plus the recorder's capture canvas.
void render_init(void);
void render_cleanup(void);

// Which input grammar the screens should describe: taps (phones, tablets,
// touch browsers) or a mouse and keyboard. Decides the keyboard cursor and the
// "Tap to continue" / "Press any key" wording, never the layout.
void render_set_touch_ui(bool touch);
bool render_touch_ui(void);

// The board. `pressing` shows the anxious face while a cell is held down.
void render_frame(const Game* g, bool pressing);
// The board with the end-of-game notice over it.
void render_won(const Game* g);
void render_lost(const Game* g);
// The family menu (menu.c). gap_before, if >= 0, inserts a blank line before
// that item index. Hit-test its rows with menu_hit_test().
void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before);

// Hit tests against the live window.
bool render_cell_at(const Game* g, int x, int y, int* col, int* row);
bool render_face_hit(const Game* g, int x, int y);
// Top edge of the grid. On touch, a tap above it (outside the face) opens the
// menu.
int  render_board_top(const Game* g);
// True when the grid is drawn turned sideways, so screen directions map to the
// other game axis (keyboard cursor movement follows the screen).
bool render_transposed(const Game* g);
// Cell side in px for the live window. The touch layer scales its tap slop
// from it.
int  render_cell_size(void);

#endif
