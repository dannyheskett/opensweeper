#ifndef OPENSWEEPER_LAYOUT_H
#define OPENSWEEPER_LAYOUT_H

#include <stdbool.h>

// Board geometry, derived from the live view size every frame. Pure: no raylib,
// no globals, no caching -- which is what makes rotation free (the next frame
// simply fits the same grid to the new shape) and makes the whole thing
// unit-testable without a window (tests/test_layout.c).
//
// From the top: the wordmark bar, a band holding the mine counter, the face
// button and the timer, then the grid, fitted into what is left at the largest
// square cell. A grid that fits better turned sideways is drawn transposed:
// Expert's 30x16 stands upright as 16 columns of 30 on a portrait phone, so its
// cells are nearly twice the size. The game's own (col, row) never change;
// only the drawing does.
typedef struct {
    int view_w, view_h;

    int margin;        // breathing room on every edge
    int titlebar_h;    // wordmark bar, grown to clear a display cutout
    int title_fs;
    int hud_y, hud_h, hud_fs;       // counter / face / timer band
    int face_x, face_y, face_size;  // the face (new game) button

    bool transposed;   // game column c is drawn as screen row c
    int cols, rows;    // the grid as drawn (after any transpose)
    int cell;          // cell side in px
    int board_x, board_y, board_w, board_h;
} Layout;

// Fit a game grid of `game_cols` x `game_rows` into the view.
Layout layout_for(int view_w, int view_h, int game_cols, int game_rows);

// Game cell under a point. Returns true and sets *col / *row when the point is
// on the grid.
bool layout_cell_at(Layout l, int x, int y, int* col, int* row);

// Top-left corner, on screen, of game cell (col, row).
void layout_cell_pos(Layout l, int col, int row, int* x, int* y);

// True if the point is on the face button.
bool layout_face_hit(Layout l, int x, int y);

#endif
