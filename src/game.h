#ifndef OPENSWEEPER_GAME_H
#define OPENSWEEPER_GAME_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_COLS 30
#define MAX_ROWS 16

typedef struct {
    uint8_t mine;
    uint8_t revealed;
    uint8_t flagged;
    uint8_t question;
    uint8_t adj;        // adjacent mine count (0-8)
} Cell;

typedef enum {
    DIFF_BEGINNER    = 0,  //  9x9,  10 mines
    DIFF_INTERMEDIATE = 1, // 16x16, 40 mines
    DIFF_EXPERT      = 2,  // 30x16, 99 mines
} Difficulty;

typedef enum {
    PHASE_IDLE,     // waiting for first click (mines not yet placed)
    PHASE_PLAY,
    PHASE_WON,
    PHASE_LOST,
} GamePhase;

// Per-frame event flags consumed by main.c to drive sound.
enum {
    EV_REVEAL    = 1 << 0,
    EV_FLAG      = 1 << 1,
    EV_UNFLAG    = 1 << 2,
    EV_QUESTION  = 1 << 3,
    EV_CHORD     = 1 << 4,
    EV_DETONATE  = 1 << 5,
    EV_WIN       = 1 << 6,
    EV_MENU_MOVE = 1 << 7,
    EV_MENU_SEL  = 1 << 8,
};

typedef struct {
    Cell cells[MAX_ROWS][MAX_COLS];
    int cols, rows, mines;
    Difficulty difficulty;
    GamePhase phase;
    int flags_placed;
    int cells_revealed;    // non-mine cells revealed so far
    int timer_frames;      // frames elapsed since first reveal (phase PLAY+)
    int cursor_x, cursor_y;
    bool marks_enabled;    // question mark cycling
    int detonated_x, detonated_y;  // the mine that was clicked (PHASE_LOST)
    unsigned events;       // EV_* flags set this frame (cleared each frame)
} Game;

Game*  game_create(Difficulty diff, bool marks_enabled);
void   game_destroy(Game* g);
void   game_frame_begin(Game* g);           // clear per-frame events
void   game_reveal(Game* g, int x, int y);  // left-click a cell
void   game_flag(Game* g, int x, int y);    // right-click a cell
void   game_chord(Game* g, int x, int y);   // chord on a revealed number
void   game_tick_timer(Game* g);            // call once per frame during PHASE_PLAY

#endif
