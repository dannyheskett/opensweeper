#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { int cols, rows, mines; } Preset;
static const Preset PRESETS[3] = {
    { 9,  9,  10},  // BEGINNER
    {16, 16,  40},  // INTERMEDIATE
    {30, 16,  99},  // EXPERT
};

Game* game_create(Difficulty diff, bool marks_enabled) {
    Game* g = calloc(1, sizeof(Game));
    if (!g) return NULL;
    g->difficulty   = diff;
    g->cols         = PRESETS[diff].cols;
    g->rows         = PRESETS[diff].rows;
    g->mines        = PRESETS[diff].mines;
    g->marks_enabled = marks_enabled;
    g->phase        = PHASE_IDLE;
    g->cursor_x     = g->cols / 2;
    g->cursor_y     = g->rows / 2;
    g->detonated_x  = -1;
    g->detonated_y  = -1;
    return g;
}

void game_destroy(Game* g) { free(g); }

void game_frame_begin(Game* g) { g->events = 0; }

// --------------------------------------------------------------------------
// Mine placement (deferred until first click — first-clicked cell is safe)
// --------------------------------------------------------------------------
static void place_mines(Game* g, int safe_x, int safe_y) {
    int total = g->cols * g->rows;
    int* positions = malloc(sizeof(int) * total);
    if (!positions) return;

    // Build list of positions excluding the safe cell
    int n = 0;
    for (int i = 0; i < total; i++) {
        int cx = i % g->cols;
        int cy = i / g->cols;
        if (cx == safe_x && cy == safe_y) continue;
        positions[n++] = i;
    }

    // Fisher-Yates shuffle, take first g->mines positions
    for (int i = 0; i < g->mines && i < n; i++) {
        int j = i + rand() % (n - i);
        int tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
        int x = positions[i] % g->cols;
        int y = positions[i] / g->cols;
        g->cells[y][x].mine = 1;
    }
    free(positions);
}

static void calc_adj(Game* g) {
    for (int y = 0; y < g->rows; y++) {
        for (int x = 0; x < g->cols; x++) {
            int count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= g->cols || ny < 0 || ny >= g->rows) continue;
                    if (g->cells[ny][nx].mine) count++;
                }
            }
            g->cells[y][x].adj = (uint8_t)count;
        }
    }
}

// --------------------------------------------------------------------------
// Flood-fill reveal (BFS) for cells with adj == 0
// --------------------------------------------------------------------------
static void flood_reveal(Game* g, int start_x, int start_y) {
    // Simple iterative BFS using a stack
    int stack[MAX_COLS * MAX_ROWS][2];
    int top = 0;
    stack[top][0] = start_x; stack[top][1] = start_y; top++;

    while (top > 0) {
        top--;
        int x = stack[top][0], y = stack[top][1];
        if (x < 0 || x >= g->cols || y < 0 || y >= g->rows) continue;
        Cell* c = &g->cells[y][x];
        if (c->revealed || c->mine) continue;
        if (c->flagged || c->question) {
            // Clear flag/question so flood can go through (matches Win95 behavior)
            if (c->flagged)  { c->flagged = 0; g->flags_placed--; }
            if (c->question) { c->question = 0; }
        }
        c->revealed = 1;
        g->cells_revealed++;
        if (c->adj == 0) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= g->cols || ny < 0 || ny >= g->rows) continue;
                    if (!g->cells[ny][nx].revealed) {
                        stack[top][0] = nx; stack[top][1] = ny; top++;
                    }
                }
            }
        }
    }
}

static void check_win(Game* g) {
    if (g->cells_revealed == g->rows * g->cols - g->mines) {
        g->phase = PHASE_WON;
        g->events |= EV_WIN;
        // Auto-flag all remaining unflagged mines on win
        for (int y = 0; y < g->rows; y++) {
            for (int x = 0; x < g->cols; x++) {
                if (g->cells[y][x].mine && !g->cells[y][x].flagged) {
                    g->cells[y][x].flagged = 1;
                    g->flags_placed++;
                }
            }
        }
    }
}

static void detonate(Game* g, int x, int y) {
    g->phase = PHASE_LOST;
    g->detonated_x = x;
    g->detonated_y = y;
    g->events |= EV_DETONATE;
    // Reveal all mines; mark incorrect flags
    for (int cy = 0; cy < g->rows; cy++) {
        for (int cx = 0; cx < g->cols; cx++) {
            Cell* c = &g->cells[cy][cx];
            if (c->mine && !c->flagged) c->revealed = 1;
            // Wrong flag (flagged but no mine): leave flagged, render will show X
        }
    }
}

void game_reveal(Game* g, int x, int y) {
    if (x < 0 || x >= g->cols || y < 0 || y >= g->rows) return;
    Cell* c = &g->cells[y][x];
    if (c->revealed || c->flagged) return;

    if (g->phase == PHASE_IDLE) {
        place_mines(g, x, y);
        calc_adj(g);
        g->phase = PHASE_PLAY;
    }
    if (g->phase != PHASE_PLAY) return;

    if (c->mine) {
        detonate(g, x, y);
        return;
    }

    if (c->question) { c->question = 0; }

    if (c->adj == 0) {
        flood_reveal(g, x, y);
    } else {
        c->revealed = 1;
        g->cells_revealed++;
    }
    g->events |= EV_REVEAL;
    check_win(g);
}

void game_flag(Game* g, int x, int y) {
    if (x < 0 || x >= g->cols || y < 0 || y >= g->rows) return;
    if (g->phase != PHASE_PLAY && g->phase != PHASE_IDLE) return;
    Cell* c = &g->cells[y][x];
    if (c->revealed) return;

    if (!c->flagged && !c->question) {
        // blank -> flag
        c->flagged = 1;
        g->flags_placed++;
        g->events |= EV_FLAG;
    } else if (c->flagged) {
        c->flagged = 0;
        g->flags_placed--;
        if (g->marks_enabled) {
            c->question = 1;
            g->events |= EV_QUESTION;
        } else {
            g->events |= EV_UNFLAG;
        }
    } else {
        // question -> blank
        c->question = 0;
        g->events |= EV_UNFLAG;
    }
}

void game_chord(Game* g, int x, int y) {
    if (x < 0 || x >= g->cols || y < 0 || y >= g->rows) return;
    if (g->phase != PHASE_PLAY) return;
    Cell* c = &g->cells[y][x];
    if (!c->revealed || c->adj == 0) return;

    // Count adjacent flags
    int adj_flags = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= g->cols || ny < 0 || ny >= g->rows) continue;
            if (g->cells[ny][nx].flagged) adj_flags++;
        }
    }
    if (adj_flags != c->adj) return;

    // Reveal all unflagged neighbors
    g->events |= EV_CHORD;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= g->cols || ny < 0 || ny >= g->rows) continue;
            if (!g->cells[ny][nx].flagged && !g->cells[ny][nx].revealed) {
                game_reveal(g, nx, ny);
                if (g->phase == PHASE_LOST) return; // detonated mid-chord
            }
        }
    }
}

void game_tick_timer(Game* g) {
    if (g->phase == PHASE_PLAY) g->timer_frames++;
}

