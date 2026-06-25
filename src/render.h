#ifndef OPENSWEEPER_RENDER_H
#define OPENSWEEPER_RENDER_H

#include "game.h"
#include <stdbool.h>

// Fixed canvas sized to Expert (the largest difficulty).
// All difficulties render into this same canvas; smaller grids are centered.
#define CANVAS_W   992   // 30 cols * 32px + 2 * 16px padding
#define CANVAS_H   672   // 16 rows * 32px + 2 * 16px padding + 128px header

void render_init(void);
void render_cleanup(void);
bool render_window_should_close(void);
void render_toggle_fullscreen(void);

// Main render calls
void render_frame(const Game* g);
void render_menu(const char* title, const char** labels, int count, int selected, int gap_before);
void render_won(const Game* g);
void render_lost(const Game* g);

// Hit test: returns true and sets *cx/*cy if mouse is over a cell
bool render_cell_at(int mouse_x, int mouse_y, const Game* g, int* cx, int* cy);
// Hit test: returns true if mouse is over the face button
bool render_face_hit(int mouse_x, int mouse_y);

#endif
